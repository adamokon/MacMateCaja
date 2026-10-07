#!/usr/bin/env python3
"""Exercise the launcher without starting Caja or touching user data."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.fixture = tempfile.TemporaryDirectory(prefix='macmatecaja-launch-test-')
        self.root = Path(self.fixture.name) / 'project & żółty'
        self.contents = self.root / 'app/MacMateCaja.app/Contents'
        self.launcher = self.contents / 'MacOS/MacMateCaja'
        self.launcher.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / 'app/MacMateCaja.app/Contents/MacOS/MacMateCaja', self.launcher)
        self.launcher.chmod(0o755)
        runtime = self.root / 'prefix/quartz/bin'
        runtime.mkdir(parents=True)
        (self.root / 'runtime/gvfs').mkdir(parents=True)
        self.record = Path(self.fixture.name) / 'result.json'
        self.bus = Path(self.fixture.name) / 'bus.json'
        program = runtime / 'caja'
        program.write_text(f'#!{sys.executable}\n' + '''import json, os, sys
with open(os.environ['TEST_RECORD'], 'w') as output:
    json.dump({'args': sys.argv[1:], 'cache': os.environ['XDG_CACHE_HOME'],
               'backend': os.environ['GDK_BACKEND'], 'path': os.environ['PATH']}, output)
sys.exit(int(os.environ.get('TEST_EXIT', '0')))
''')
        program.chmod(0o755)
        dbus = runtime / 'dbus-run-session'
        dbus.write_text(f'#!{sys.executable}\n' + '''import json, os, subprocess, sys
import xml.etree.ElementTree as ET
filename = sys.argv[1].split('=', 1)[1]
tree = ET.parse(filename)
with open(os.environ['TEST_BUS'], 'w') as output:
    json.dump({'file': filename, 'dirs': [e.text for e in tree.findall('servicedir')]}, output)
environment = dict(os.environ, DBUS_SESSION_BUS_ADDRESS='test:synthetic')
sys.exit(subprocess.call(sys.argv[3:], env=environment))
''')
        dbus.chmod(0o755)
        self.env = dict(os.environ, TEST_RECORD=str(self.record), TEST_BUS=str(self.bus),
                        TMPDIR=self.fixture.name)
        self.env.pop('DBUS_SESSION_BUS_ADDRESS', None)
        for name in ('MACMATECAJA_QUARTZ_PREFIX', 'MACMATECAJA_GVFS_PREFIX'):
            self.env.pop(name, None)

    def tearDown(self):
        self.fixture.cleanup()

    def run_launch(self, args=(), exit_code=0):
        self.env['TEST_EXIT'] = str(exit_code)
        result = subprocess.run([str(self.launcher), *args], env=self.env,
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, exit_code, result.stderr)
        return json.loads(self.record.read_text())

    def test_default_and_cleanup(self):
        data = self.run_launch()
        self.assertEqual(data['args'], ['--browser', '--no-desktop'])
        self.assertEqual(data['backend'], 'quartz')
        self.assertTrue(data['cache'].startswith('/tmp/MacMateCaja-gvfs-'))
        self.assertIn(str(self.contents / 'Resources/libexec'), data['path'])
        bus = json.loads(self.bus.read_text())
        self.assertEqual(bus['dirs'][0], str(self.root / 'prefix/quartz/share/dbus-1/services'))
        self.assertFalse(Path(bus['file']).exists())
        self.assertFalse(Path(bus['file']).parent.exists())

    def test_explicit_args_and_finder(self):
        data = self.run_launch(['-psn_123', '--tabs', '/tmp/a file', 'sftp://user@example.invalid/docs'])
        self.assertEqual(data['args'], ['--browser', '--no-desktop', '--tabs',
                                       '/tmp/a file', 'sftp://user@example.invalid/docs'])

    def test_existing_bus(self):
        self.env['DBUS_SESSION_BUS_ADDRESS'] = 'test:already-running'
        data = self.run_launch(['--no-default-window'])
        self.assertEqual(data['args'][-1], '--no-default-window')
        self.assertFalse(self.bus.exists())

    def test_cleanup_on_failure(self):
        self.run_launch(exit_code=7)
        bus = json.loads(self.bus.read_text())
        self.assertFalse(Path(bus['file']).parent.exists())


if __name__ == '__main__':
    unittest.main()
