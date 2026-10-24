# Family dashboard

C++26 hello world using the named module `dashboard.greeting`, cross-compiled
directly on macOS for the Tinkerboard S (ARMv7 hard-float). No container or VM.

## Build and upload

Install the native build tools with Homebrew:

```sh
brew install llvm lld cmake ninja python@3.14
bash scripts/build.sh
bash scripts/deploy.sh
```

The build script uses Homebrew Clang and LLD to produce a static ARM Linux
executable at `build/armv7-native/family-dashboard`. On first run it downloads
Debian trixie ARM headers and libraries into `build/sysroot`. Packages are
SHA256-checked against the Debian package index fetched over HTTPS; package
names, versions, URLs, and hashes are recorded in `build/sysroot/packages.json`.
These target packages are extracted locally, never installed on macOS.

The deploy script defaults to `linaro@192.168.1.183`, uploads the executable to
`~/.local/bin/family-dashboard-hello`, and runs it. Expected output:

```text
hello world
```

An alternative SSH destination can be supplied as the first argument:

```sh
bash scripts/deploy.sh linaro@192.168.1.183
```

SSH uses your normal keys or interactive password prompt. Passwords are not
stored in this repository.

## Toolchain and compatibility

CMake and Ninja track named-module dependencies using Clang's module scanner.
The toolchain sets `--target=arm-linux-gnueabihf`, the local Linux sysroot, and
LLD for ELF linking. Apple Clang and the macOS linker are not used.

For this smoke test, the executable is statically linked, avoiding a dependency
on the board's installed glibc or libstdc++ versions. ARMv7 hard-float and a
Linux kernel compatible with the toolchain are still required. Successful
execution on the board is the final compatibility check.

Before adding networking, graphics, and GPIO dependencies, inspect the board:

```sh
cat /etc/os-release
uname -m
getconf GNU_LIBC_VERSION
dpkg --print-architecture
```

Choose a target sysroot and runtime deployment strategy for those dependencies
once the OS is known. The current program only prints text to standard output.

`LLVM_ROOT`, `LLD_ROOT`, and `PYTHON` override the build script's Homebrew tool
locations. Downloads and generated outputs are contained in the ignored
`build/` directory. Delete that directory to refresh all target packages.

## Verification

The native macOS cross-build passed with Clang/LLD 23.1.4, CMake 4.4.4, and
Ninja 1.13.2. ELF inspection confirms ARMv7, hard-float ABI, static linking,
and a Linux 3.2 minimum kernel note. Script syntax checks passed.

Hardware execution is pending: the SSH upload attempt disconnected at login
with `System is going down.` No upload or remote execution was confirmed.
