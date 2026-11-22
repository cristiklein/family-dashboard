#!/usr/bin/env python3
"""Require reasons for std:: qualifiers and check module using directives."""

import re
import sys
from pathlib import Path

NON_CODE = re.compile(
    r'(?<!\w)(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\s]{0,16})'
    r'\((?s:.*?)\)(?P=delimiter)"'
    r'|//[^\n]*|/\*(?s:.*?)\*/'
    r'|"(?:\\[\s\S]|[^"\\\n])*"'
    r"|(?<!\w)(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\\n])*'"
)
EXCEPTION = re.compile(r"//\s*std-qualified:\s*\S")
QUALIFIED_NAME = re.compile(r"\bstd\s*:{2}")
USING_STD = re.compile(r"\b(?P<export>export\s+)?using\s+namespace\s+std\s*;")
NAMED_MODULE = re.compile(r"(?m)^[ \t]*(?:export\s+)?module\s+[^\W\d][\w.:]*\s*;")
IMPORT = re.compile(r"(?m)^[ \t]*(?:export\s+)?import\b\s*(?:[\w.:]+|<[^>\n]+>)?\s*;")


def check(path):
    source = path.read_text(encoding="utf-8")
    exceptions = set()

    def line_at(offset):
        return source.count("\n", 0, offset) + 1

    def mask(match):
        text = match.group()
        if EXCEPTION.match(text):
            exceptions.add(line_at(match.start()))
        return re.sub(r"[^\n]", " ", text)

    code = NON_CODE.sub(mask, source)
    for match in QUALIFIED_NAME.finditer(code):
        line = line_at(match.start())
        if line not in exceptions:
            yield line, "Use an unqualified name or add // std-qualified: <reason>"

    if path.suffix == ".cppm":
        module = NAMED_MODULE.search(code)
        imports_end = max((match.end() for match in IMPORT.finditer(code)), default=0)
        for match in USING_STD.finditer(code):
            line = line_at(match.start())
            if match.group("export"):
                yield line, "Keep 'using namespace std;' unexported"
            if module is None or match.start() < max(module.end(), imports_end):
                yield line, "Place 'using namespace std;' after the module declaration and imports"


def main():
    failed = False
    for filename in sys.argv[1:]:
        for line, message in check(Path(filename)):
            print(f"{filename}:{line}: {message}", file=sys.stderr)
            failed = True
    if failed:
        print(
            "\nPrefer 'using namespace std;' and unqualified standard-library names.\n"
            "Fewer 'std::' prefixes make implementation code easier to scan.\n"
            "In modules, keep the directive unexported and place it after the module\n"
            "declaration and imports.\n"
            "When qualification resolves ambiguity, explain why on the same line\n"
            "with '// std-qualified: <reason>'.",
            file=sys.stderr,
        )
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
