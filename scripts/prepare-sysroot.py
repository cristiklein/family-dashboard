#!/usr/bin/env python3
"""Extract Debian ARM libraries locally; no installation or Linux host required."""

import hashlib
import io
import json
import lzma
from pathlib import Path
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1] / "build"
SYSROOT = ROOT / "sysroot"
CACHE = ROOT / "downloads"
MIRROR = "https://deb.debian.org/debian"
PACKAGES = {
    "libc6", "libc6-dev", "linux-libc-dev", "libgcc-14-dev",
    "libstdc++-14-dev", "libgcc-s1", "libstdc++6", "gcc-14-base",
}


def fetch(url):
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def main():
    SYSROOT.mkdir(parents=True, exist_ok=True)
    # Debian's merged-/usr layout is normally established by base-files.
    # The library packages alone do not supply this root-level symlink.
    if not (SYSROOT / "lib").exists():
        (SYSROOT / "lib").symlink_to("usr/lib", target_is_directory=True)
    marker = SYSROOT / "packages.json"
    if marker.exists():
        print(f"Using ARM sysroot: {SYSROOT}")
        return
    CACHE.mkdir(parents=True, exist_ok=True)
    SYSROOT.mkdir(parents=True, exist_ok=True)
    index = CACHE / "trixie-armhf-Packages.xz"
    if not index.exists():
        index.write_bytes(fetch(f"{MIRROR}/dists/trixie/main/binary-armhf/Packages.xz"))
    records = {}
    for paragraph in lzma.decompress(index.read_bytes()).decode().split("\n\n"):
        fields = dict(line.split(": ", 1) for line in paragraph.splitlines()
                      if line and not line[0].isspace() and ": " in line)
        if fields.get("Package") in PACKAGES:
            records[fields["Package"]] = fields
    missing = PACKAGES - records.keys()
    if missing:
        raise RuntimeError(f"Packages absent from Debian index: {missing}")
    manifest = []
    for name in sorted(records):
        record = records[name]
        package = CACHE / Path(record["Filename"]).name
        if not package.exists():
            package.write_bytes(fetch(f'{MIRROR}/{record["Filename"]}'))
        if hashlib.sha256(package.read_bytes()).hexdigest() != record["SHA256"]:
            raise RuntimeError(f"SHA256 mismatch: {package}; remove it and retry")
        members = subprocess.check_output(["ar", "t", str(package)], text=True)
        member = next(m for m in members.splitlines() if m.startswith("data.tar"))
        archive = subprocess.check_output(["ar", "p", str(package), member])
        with tarfile.open(fileobj=io.BytesIO(archive)) as data:
            data.extractall(SYSROOT, filter="data")
        manifest.append({key: record[key] for key in
                         ("Package", "Version", "Filename", "SHA256")})
    marker.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared ARM sysroot: {SYSROOT}")


if __name__ == "__main__":
    main()
