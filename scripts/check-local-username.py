#!/usr/bin/env python3
"""Reject checked files containing the current developer's username."""

import getpass
import os
from pathlib import Path
import sys


def main():
    username = os.fsencode(os.environ.get("USER") or getpass.getuser())
    found = False
    for filename in sys.argv[1:]:
        path = Path(filename)
        if path.is_symlink():
            contents = os.fsencode(os.readlink(path))
        else:
            contents = path.read_bytes()
        if username in contents:
            print(f"{filename} contains your local username.", file=sys.stderr)
            found = True
    return int(found)


if __name__ == "__main__":
    sys.exit(main())
