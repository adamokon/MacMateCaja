#!/usr/bin/env python3
"""Read the local NetBird daemon; emit a GKeyFile for Caja (no API token)."""
import ipaddress
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def escape(value):
    return str(value).replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


def render(status):
    peers = status.get("peers", {}).get("details", [])
    if not isinstance(peers, list):
        raise ValueError("Nieprawidłowa lista komputerów NetBird")
    records = []
    for peer in peers:
        address = str(peer.get("netbirdIp", "")).split("/")[0]
        try:
            address = str(ipaddress.ip_address(address))
        except ValueError:
            continue
        name = peer.get("fqdn") or address
        records.append((str(name), address, str(peer.get("status", "Unknown"))))
    records.sort(key=lambda row: (row[2] != "Connected", row[0].casefold()))
    connected = bool(status.get("management", {}).get("connected", False))
    lines = ["[netbird]", "connected=" + str(connected).lower(), "count=" + str(len(records))]
    for index, (name, address, state) in enumerate(records):
        lines += ["", "[peer{}]".format(index), "name=" + escape(name),
                  "address=" + address, "status=" + escape(state)]
    return "\n".join(lines) + "\n"


def main():
    candidates = [shutil.which("netbird"), "/usr/local/bin/netbird", "/opt/homebrew/bin/netbird",
                  str(Path.home() / "homebrew/bin/netbird"),
                  "/Applications/NetBird.app/Contents/MacOS/netbird"]
    binary = next((p for p in candidates if p and os.path.isfile(p) and os.access(p, os.X_OK)), None)
    if not binary:
        raise RuntimeError("Nie znaleziono NetBird. Zainstaluj klienta i połącz go z siecią.")
    result = subprocess.run([binary, "status", "--json"], capture_output=True, text=True, timeout=12)
    if result.returncode:
        raise RuntimeError("NetBird nie odpowiada. Sprawdź, czy klient jest uruchomiony i zalogowany.")
    print(render(json.loads(result.stdout)), end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired, AttributeError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
