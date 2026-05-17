# Contributing to grib-cpp

Thanks for your interest in contributing. This guide covers the local
build flow, code style expectations, and PR conventions used in this
repo. For security-vulnerability reports, see [SECURITY.md](SECURITY.md)
— do **not** open a public issue for those.

## Dependency policy (read this first)

GRIB2 has exactly one license-compatible C/C++ decoder: **ECMWF ecCodes**
(Apache-2.0). It is vendored as a pinned static ExternalProject.

**NCEPLIBS-g2c is LGPL-3.0 and MUST NOT be added.** It would impose
relink / corresponding-source obligations on every downstream consumer of
this MIT, statically-linked SDK, which defeats the point of the house
pattern. `wgrib2` is also off-limits (mixed/GNU licensing, CLI-first). Do
not introduce either, and do not add `aws-sdk-cpp` — anonymous public S3
buckets need no SigV4, so plain libcurl `CURLOPT_RANGE` is sufficient.

## Getting started

```bash
git clone https://github.com/Reddimus/grib-cpp.git
cd grib-cpp

# One-time: install build deps (Ubuntu 24.04 example).
# python3 is a BUILD-time dep (ecCodes ENABLE_MEMFS embeds the GRIB
# definition tree into the static archive); there is no runtime python.
sudo apt install -y build-essential cmake clang-format git python3 \
    libcurl4-openssl-dev libopenjp2-7-dev libpng-dev libaec-dev zlib1g-dev

make build      # CMake configure + Release build (also builds ecCodes + libaec)
make test       # Run unit tests (ctest)
```

The first build clones + compiles ecCodes (and libaec, and bootstraps
ecbuild) — it is slow once, then cached under `build/`. macOS uses
Homebrew (`brew install curl openjpeg libpng libaec`); see the CI
workflow for exact invocations. There is no Windows build.

## Development workflow

```bash
make debug          # Debug build with symbols + sanitizers
make test           # Run unit tests (ctest)
make lint           # clang-format --dry-run + cpp_auto_audit
make format         # Apply clang-format in place
make coverage       # lcov coverage report (Debug build)
make clean          # Remove build/
```

Always run `make lint` (or `make pre-commit`) before pushing — CI gates
on both `clang-format --dry-run` AND `cpp_auto_audit.py`.

## Code style

- **C++23**: `std::expected<T, Error>` for all error-returning
  operations, no exceptions in the public API.
- **No `auto`** for local variable declarations. Carve-outs: structured
  bindings, lambda closures, iterator-like results (`.find()` etc).
- **Formatting**: `.clang-format` (LLVM base, tabs, 100-col). `make
  format` applies it.
- **Includes**: project headers first, then system (clang-format
  `SortIncludes`).
- **JSON**: the ECMWF `.index` is parsed with **Glaze** (`glz::generic`).
  The NOMADS `.idx` is plain text — no JSON. Keep the two sidecar parsers
  strictly separate; never share param-vocabulary logic between them.
- **ecCodes** is wrapped behind `grib/handle.hpp` only. Public headers do
  NOT include `<eccodes.h>` (it stays a private build dependency).

## PR conventions

- Branch names: `feat/...`, `fix/...`, `docs/...`, `chore/...`,
  `test/...`, `build/...`, `ci/...`, `refactor/...`.
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/).
- Squash + delete branch on merge. PR titles become the squash subject.
- Update `CHANGELOG.md` under `## [Unreleased]` for any user-visible
  change. Keep-a-Changelog sub-headers: Added / Changed / Fixed / Removed.
- CI must pass on Ubuntu 24.04 + macOS + markdown-lint before merge.

## Release process

```bash
# 1. Update CMakeLists.txt VERSION and CHANGELOG.md [Unreleased] -> [vX.Y.Z]
# 2. git commit -am "chore(release): cut vX.Y.Z" && git push origin main
# 3. git tag vX.Y.Z && git push origin vX.Y.Z   # release.yml auto-publishes
```

`release.yml` hard-fails if the tag does not equal the CMakeLists
`project(grib-cpp VERSION ...)` field. Semver: MINOR for new public API,
PATCH for fixes/docs/CI.

## License

By contributing, you agree your changes are licensed under the MIT
license that covers this repository.
