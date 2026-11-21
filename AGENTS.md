# Project conventions

- Use Allman braces for all C++ code, including module files (`.cppm`). Put
  opening braces on their own lines and use braces for control-flow bodies.
- Keep the two-space indentation defined in `.clang-format`.
- Prefer `using namespace std;` and unqualified standard-library names in C++
  source files. In module units, place the directive after the module declaration
  and imports; do not export it. Qualify individual names if needed to resolve
  an ambiguity.
- After editing C++ files, run
  `pre-commit run clang-format-fix --hook-stage manual --files <paths>`.
  Verify formatting with `pre-commit run clang-format --files <paths>` before
  completing the change.
- Preserve C++26 named modules and the native macOS-to-ARM Linux build.
- Keep personal IP addresses out of the README.
