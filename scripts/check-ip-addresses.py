#!/usr/bin/env python3
"""Reject IPv4 and IPv6 literals in source and other text files."""

import re
import sys
from ipaddress import ip_address
from pathlib import Path

IP_CANDIDATE = re.compile(
    r"(?<![\w.])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?!\w|\.[0-9])"
    r"|(?<![\w:])(?:[0-9a-fA-F.]*:){2,}[0-9a-fA-F.]*(?![\w:])"
)


def contains_ip_address(line):
    for match in IP_CANDIDATE.finditer(line):
        try:
            ip_address(match.group().strip("."))
        except ValueError:
            continue
        return True
    return False


def main():
    found = False
    for filename in sys.argv[1:]:
        with Path(filename).open(encoding="utf-8", errors="replace") as source:
            for line_number, line in enumerate(source, start=1):
                if contains_ip_address(line):
                    print(f"{filename}:{line_number}: IP address literal", file=sys.stderr)
                    found = True
    return int(found)


if __name__ == "__main__":
    sys.exit(main())
