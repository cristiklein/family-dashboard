#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
llvm_dir="${LLVM_ROOT:-$(brew --prefix llvm)}"
python_bin="${PYTHON:-$(brew --prefix python@3.14)/bin/python3.14}"
export CXX="$llvm_dir/bin/clang++"
exec "$python_bin" "$project_dir/scripts/preview.py" "$@"
