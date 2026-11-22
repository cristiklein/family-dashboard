"""Keep GitHub Actions shell logic in repository scripts that can run locally."""

import os
import re
import subprocess
import sys
from pathlib import Path

import yaml

WORKFLOWS_DIR = Path(".github/workflows")
SCRIPT = r"\./scripts/(?:[\w-]+/)*[\w-][\w.-]*"
ARGUMENT = r"""(?:[\w./=:@+,%-]|'[^'\r\n$]*'|"[^"\\\r\n$`]*")+"""
COMMAND = re.compile(rf"(?:(?P<script>{SCRIPT})|pre-commit[ \t]+run)(?:[ \t]+{ARGUMENT})*")


def staged_files():
    output = subprocess.check_output(
        ["git", "ls-files", "--stage", "-z", "--", str(WORKFLOWS_DIR), "scripts"]
    )
    files = {}
    for entry in filter(None, output.split(b"\0")):
        metadata, filename = entry.split(b"\t", 1)
        mode, _, stage = metadata.split()
        if stage == b"0":
            files[Path(os.fsdecode(filename))] = mode
    return files


def check_workflow(path, files):
    try:
        workflow = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    except yaml.YAMLError as error:
        yield f"{path}: {error}"
        return

    for job_id, job in workflow.get("jobs", {}).items():
        for number, step in enumerate(job.get("steps", []), start=1):
            if "run" not in step:
                continue

            location = f"{path}: job {job_id}, step {number} ({step.get('name', 'run')})"
            command = step["run"]
            match = COMMAND.fullmatch(command.strip()) if isinstance(command, str) else None
            if match is None:
                yield (
                    f"{location}: Use a single './scripts/…' or 'pre-commit run' command. "
                    "Move inline shell logic into an executable script."
                )
                continue

            script = match.group("script")
            if script and files.get(Path(script)) != b"100755":
                yield f"{location}: {script} must be tracked and executable."


def main():
    files = staged_files()
    failed = False
    for path in sorted(files):
        if path.parent == WORKFLOWS_DIR and path.suffix in {".yml", ".yaml"}:
            for message in check_workflow(path, files):
                print(message, file=sys.stderr)
                failed = True
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
