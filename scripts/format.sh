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
while IFS= read -r -d '' source_file; do
  sources+=("$source_file")
done < <(rg --files -0 src -g '*.cpp' -g '*.cppm' -g '*.h' -g '*.hpp')

if [[ "$mode" == --check ]]; then
  "$formatter" --style=file --dry-run --Werror "${sources[@]}"
else
  "$formatter" --style=file -i "${sources[@]}"
fi
