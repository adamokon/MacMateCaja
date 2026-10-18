#!/usr/bin/env python3
"""
test-packaging.py - Rigorous verification of the standalone MacMateCaja package.

Verifies:
1. Every bundled Mach-O dependency is a real system library or resolves to an
   existing app-relative @rpath library inside Contents/Frameworks. Zero lingering
   Homebrew or developer paths.
2. Code signature passes deep and strict verification.
3. Info.plist passes plutil and contains honest OS/architecture declarations (minos 26.0).
4. Component PKG expands properly targeting /Applications with no postinstall scripts.
5. Relocation verification by copying to two distinct temp dirs (one with spaces)
   and launching --version without developer runtime or Homebrew in PATH.
   Uses TemporaryDirectory unique roots exclusively (no fixed shared names).
6. Non-network local synthetic file read with bundled GIO ('gio cat' and 'gio info')
   and Caja smoke execution.
"""

import os
import plistlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from xml.sax.saxutils import escape
from pathlib import Path


def is_macho(path):
    if not os.path.isfile(path) or os.path.islink(path):
        return False
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
            return magic in (
                b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe',
                b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe',
                b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'
            )
    except Exception:
        return False


class PackagingVerificationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace_root = Path(__file__).resolve().parents[1]
        cls.dist_dir = Path(os.environ.get("MACMATECAJA_TEST_DIST_DIR", str(cls.workspace_root / "dist"))).resolve()
        cls.app_path = cls.dist_dir / "MacMateCaja.app"
        cls.pkg_path = cls.dist_dir / "MacMateCaja-1.28.0-arm64.pkg"
        cls.zip_path = cls.dist_dir / "MacMateCaja-1.28.0-arm64.zip"

        if not cls.app_path.exists():
            raise unittest.SkipTest(f"App bundle not found at {cls.app_path}. Run build-package first.")

    def test_01_macho_dependencies_fully_resolved_and_relocatable(self):
        """
        Verify that every Mach-O file depends ONLY on:
        - Apple system libraries (/System/Library/ or /usr/lib/)
        - App-relative @rpath libraries that actually exist in Contents/Frameworks
        Zero lingering dependencies on /opt/homebrew, /Users, /opt/MacMateCaja, or /usr/local.
        """
        violations = []
        missing_targets = []
        checked_count = 0
        frameworks_dir = self.app_path / "Contents/Frameworks"

        for root, dirs, files in os.walk(self.app_path):
            for f in files:
                p = Path(root) / f
                if is_macho(p):
                    checked_count += 1
                    res = subprocess.run(["otool", "-L", str(p)], capture_output=True, text=True, check=True)
                    lines = res.stdout.strip().splitlines()[1:]
                    for line in lines:
                        line = line.strip()
                        if not line:
                            continue
                        dep = line.split(" (compatibility version")[0].strip()

                        # Apple macOS system libraries
                        if dep.startswith("/System/Library/") or dep.startswith("/usr/lib/"):
                            continue

                        # Relocatable @rpath libraries
                        if dep.startswith("@rpath/"):
                            dep_name = dep.replace("@rpath/", "")
                            target = frameworks_dir / dep_name
                            if not target.exists():
                                missing_targets.append((str(p.relative_to(self.app_path)), dep))
                            continue

                        # Any other path is non-relocatable and forbidden
                        violations.append((str(p.relative_to(self.app_path)), dep))

        self.assertGreater(checked_count, 15, "Should have verified multiple Mach-O binaries")
        self.assertEqual(violations, [], f"Found non-relocatable hardcoded library dependencies: {violations}")
        self.assertEqual(missing_targets, [], f"Found unresolved @rpath targets in Frameworks: {missing_targets}")

    def test_02_codesign_verification(self):
        """Verify ad-hoc code signature deep and strict."""
        res = subprocess.run(
            ["codesign", "--verify", "--deep", "--strict", str(self.app_path)],
            capture_output=True, text=True
        )
        self.assertEqual(res.returncode, 0, f"codesign verification failed:\n{res.stderr}")

    def test_03_plist_validation(self):
        """Validate Info.plist structure, honest minos (26.0), and executable."""
        plist_file = self.app_path / "Contents/Info.plist"
        self.assertTrue(plist_file.exists())

        res = subprocess.run(["plutil", "-lint", str(plist_file)], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"plutil lint failed: {res.stderr}")

        with open(plist_file, 'rb') as f:
            data = plistlib.load(f)

        self.assertEqual(data.get("CFBundlePackageType"), "APPL")
        self.assertEqual(data.get("CFBundleExecutable"), "MacMateCaja")
        self.assertEqual(data.get("LSMinimumSystemVersion"), "26.0")

    def test_04_pkg_inspection(self):
        """Verify that the PKG is an unsigned component targeting /Applications with no postinstall scripts."""
        if not self.pkg_path.exists():
            self.skipTest(f"PKG artifact not found at {self.pkg_path}")

        with tempfile.TemporaryDirectory(prefix="macmatecaja-pkg-expand-") as tmp:
            expand_dir = Path(tmp) / "expanded"
            res = subprocess.run(["pkgutil", "--expand-full", str(self.pkg_path), str(expand_dir)], capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, f"pkgutil expand failed: {res.stderr}")

            pkg_info = expand_dir / "PackageInfo"
            self.assertTrue(pkg_info.exists(), "PackageInfo missing from PKG")
            content = pkg_info.read_text(encoding="utf-8", errors="ignore")
            self.assertIn('install-location="/Applications"', content)

            scripts_dir = expand_dir / "Scripts"
            if scripts_dir.exists():
                postinstall = scripts_dir / "postinstall"
                self.assertFalse(postinstall.exists(), "PKG must not have postinstall script")

            payload_app = expand_dir / "Payload/MacMateCaja.app"
            self.assertTrue(payload_app.exists(), "Payload does not contain MacMateCaja.app")

    def test_05_relocation_two_temp_dirs(self):
        """
        Test launching MacMateCaja --version from two differently named temp dirs
        (one standard alphanumeric, one containing spaces).
        Uses unique TemporaryDirectory roots to prevent clashes.
        """
        dir_configs = [
            ("macmatecaja-alpha-", "AlphaRelocDir"),
            ("macmatecaja beta ", "Beta Reloc Dir With Spaces")
        ]

        for prefix, sub_name in dir_configs:
            with tempfile.TemporaryDirectory(prefix=prefix) as tmp_root:
                test_dir = Path(tmp_root) / sub_name
                test_dir.mkdir(parents=True)

                dest_app = test_dir / "MacMateCaja.app"
                shutil.copytree(self.app_path, dest_app, symlinks=True)

                launcher = dest_app / "Contents/MacOS/MacMateCaja"
                self.assertTrue(launcher.exists())

                clean_env = {
                    "PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
                    "TMPDIR": str(test_dir),
                    "HOME": str(test_dir),
                    "USER": os.environ.get("USER", "nobody"),
                    "TERM": "xterm-256color"
                }

                res = subprocess.run(
                    [str(launcher), "--version"],
                    env=clean_env,
                    capture_output=True,
                    text=True,
                    timeout=25
                )
                self.assertEqual(
                    res.returncode, 0,
                    f"Failed launching in relocated dir {test_dir}:\nSTDOUT: {res.stdout}\nSTDERR: {res.stderr}"
                )
                self.assertIn("caja", res.stdout.lower() + res.stderr.lower())

    def test_07_synthetic_window_start(self):
        """Start only a fresh synthetic directory; terminate our isolated process group."""
        with tempfile.TemporaryDirectory(prefix="macmatecaja-window-") as tmp:
            root = Path(tmp)
            services = root / "services"
            services.mkdir()
            contents = self.app_path / "Contents"
            for service in (contents / "Resources/share/dbus-1/services").glob("*.service"):
                template = service.read_text()
                template = template.replace("@APP_LIBEXEC@", str(contents / "Resources/libexec"))
                template = template.replace("@APP_BIN@", str(contents / "MacOS"))
                (services / service.name).write_text(template)
            config = root / "bus.conf"
            config.write_text('<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen>'
                              '<auth>EXTERNAL</auth><servicedir>' + escape(str(services)) +
                              '</servicedir><policy context="default"><allow send_destination="*"/>'
                              '<allow own="*"/></policy></busconfig>')
            env = {"PATH": str(contents / "MacOS") + ":/usr/bin:/bin:/usr/sbin:/sbin", "HOME": tmp, "TMPDIR": tmp,
                   "XDG_CONFIG_HOME": str(root / "config"),
                   "MACMATECAJA_CONFIG_FILE": str(root / "missing-profile.ini")}
            shell = 'exec "$1" "$2" >"$3/window.stdout" 2>"$3/window.stderr"'
            process = subprocess.Popen([str(contents / "MacOS/dbus-run-session"),
                                     "--config-file=" + str(config), "--", "/bin/bash", "-c", shell,
                                     "synthetic-window", str(contents / "MacOS/MacMateCaja"),
                                     root.as_uri(), tmp], env=env, stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL, start_new_session=True)
            try:
                time.sleep(3)
                errors = (root / "window.stderr").read_text() if (root / "window.stderr").exists() else ""
                self.assertIsNone(process.poll(), errors)
                self.assertNotIn("GLib-GIO-ERROR", errors)
            finally:
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                process.wait(timeout=10)

    def test_06_non_network_local_gio_read(self):
        """
        Verify non-network local synthetic file read using bundled GIO runtime ('gio cat' and 'gio info').
        Tests without Homebrew or developer prefixes.
        """
        with tempfile.TemporaryDirectory(prefix="macmatecaja-gio-test-") as tmp:
            test_file = Path(tmp) / "synthetic-test.txt"
            content = "Hello MacMateCaja synthetic GIO read test payload\n"
            test_file.write_text(content)

            clean_env = {
                "PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
                "TMPDIR": tmp,
                "HOME": tmp,
                "USER": os.environ.get("USER", "nobody"),
                "TERM": "xterm-256color"
            }

            gio_bin = self.app_path / "Contents/MacOS/gio"
            self.assertTrue(gio_bin.exists(), "Bundled gio executable missing from MacOS/")

            # 1. gio cat
            res_cat = subprocess.run(
                [str(gio_bin), "cat", str(test_file)],
                env=clean_env,
                capture_output=True,
                text=True,
                timeout=15
            )
            self.assertEqual(res_cat.returncode, 0, f"gio cat failed:\n{res_cat.stderr}")
            self.assertEqual(res_cat.stdout, content)

            # 2. gio info
            res_info = subprocess.run(
                [str(gio_bin), "info", str(test_file)],
                env=clean_env,
                capture_output=True,
                text=True,
                timeout=15
            )
            self.assertEqual(res_info.returncode, 0, f"gio info failed:\n{res_info.stderr}")
            self.assertIn("synthetic-test.txt", res_info.stdout)


if __name__ == "__main__":
    unittest.main()
