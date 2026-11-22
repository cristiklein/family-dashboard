# Project conventions

- Prefer conventions enforced through pre-commit over instructions in
  `AGENTS.md`. This ensures both humans and agents follow the same rules.
- Prefer `using namespace std;` and unqualified standard-library names in C++
  source files. In module units, place the directive after the module declaration
  and imports; do not export it. Qualify individual names if needed to resolve
  an ambiguity.
- Preserve C++26 named modules and the native macOS-to-ARM Linux build.
