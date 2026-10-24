#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:---write}"
case "$mode" in
  --write|--check) ;;
  *) echo 'Usage: bash scripts/format.sh [--write|--check]' >&2; exit 2 ;;
esac

if [[ -n "${CLANG_FORMAT:-}" ]]; then
  formatter="$CLANG_FORMAT"
elif command -v clang-format >/dev/null 2>&1; then
  formatter="$(command -v clang-format)"
elif command -v brew >/dev/null 2>&1; then
  formatter="$(brew --prefix llvm)/bin/clang-format"
else
  echo 'Install clang-format (brew install llvm) or set CLANG_FORMAT.' >&2
  exit 1
fi

cd "$project_dir"
sources=()
source_dirs=(src)
if [[ -d tests ]]; then
  source_dirs+=(tests)
fi
while IFS= read -r -d '' source_file; do
  sources+=("$source_file")
done < <(find "${source_dirs[@]}" -type f \( -name '*.cpp' -o -name '*.cppm' -o -name '*.h' -o -name '*.hpp' \) -print0)

# Bash 3.2 (the macOS default) treats an empty array as unset with `set -u`.
if [[ ${#sources[@]} -eq 0 ]]; then
  echo 'No C++ sources found under src/.' >&2
  exit 1
fi

if [[ "$mode" == --check ]]; then
  "$formatter" --style=file --dry-run --Werror "${sources[@]}"
else
  "$formatter" --style=file -i "${sources[@]}"
fi
