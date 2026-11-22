#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
llvm_dir="${LLVM_ROOT:-$(brew --prefix llvm)}"
lld_dir="${LLD_ROOT:-$(brew --prefix lld)}"
binary="$project_dir/build/arm64-native/family-dashboard"
export PATH="$llvm_dir/bin:$lld_dir/bin:$PATH"

"${PYTHON:-$(brew --prefix python@3.14)/bin/python3.14}" "$project_dir/scripts/prepare-sysroot.py"
cmake -S "$project_dir" -B "$project_dir/build/arm64-native" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$project_dir/cmake/zero.cmake" \
  -DCMAKE_CXX_COMPILER="$llvm_dir/bin/clang++" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_dir/build/arm64-native"

artifact="$(LC_ALL=C file -b "$binary")"
printf '%s: %s\n' "$binary" "$artifact"
if [[ "$artifact" != "ELF 64-bit LSB executable, ARM aarch64,"* ]]; then
  echo 'Expected a 64-bit AArch64 ELF executable.' >&2
  exit 1
fi

sha256="$(shasum -a 256 < "$binary" | cut -d ' ' -f 1)"
printf 'Built executable SHA-256: %s\n' "$sha256"
