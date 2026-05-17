# grib-cpp Development Guide

## Build & Test

```bash
make build          # Release build (CMake + make); first run also builds
                    # pinned static ecCodes + libaec (slow once, cached)
make debug          # Debug build
make test           # Run unit tests (ctest)
make lint           # clang-format --dry-run + cpp_auto_audit
make format         # Format in place
make coverage       # lcov coverage report
make clean          # Remove build/

make run-nomads-idx   # examples/example_nomads_idx.cpp  (NOAA GFS, live)
make run-ecmwf-index  # examples/example_ecmwf_index.cpp (ECMWF, live)
```

Build hosts need `python3` (ecCodes `ENABLE_MEMFS` embeds the GRIB
definition tree into `libeccodes.a` at build time — **no runtime python
or filesystem dependency**) plus `libopenjp2-7-dev libpng-dev
libaec-dev zlib1g-dev libcurl4-openssl-dev`.

## Architecture

- **Layered static libraries**: `grib_core` → `grib_http` →
  `grib_models` → `grib_decode` → `grib_api` → `grib` (INTERFACE)
- **C++23**: `std::expected<T, Error>` for all returns, no exceptions
- **Patterns**: Pimpl (`HttpClient`), RAII (`GribHandle`/`GribFile`),
  move-only, `[[nodiscard]]`
- **HTTP**: libcurl. `get()` + `get_range()` (HTTP `Range`). Anonymous
  public S3/NOMADS need no SigV4 — no aws-sdk-cpp.
- **Sidecars** (`grib/sidecar.hpp`): `parse_nomads_idx` (plain text,
  length = next-offset − offset, `TMP`) and `parse_ecmwf_index`
  (newline-JSON via Glaze, explicit `_offset`/`_length`, `2t`). Two
  strictly separate parsers; never share param vocabulary. `select()`
  filters param/level/step/member.
- **Decode** (`grib/handle.hpp`): RAII over the ecCodes C handle API.
  `GribHandle` (get_double/long/string, values, nearest) + `GribFile`
  (multi-message blob iterator). ecCodes decodes every packing
  (simple/complex/spatial-diff/JPEG2000/PNG/CCSDS) transparently.
- **Extract** (`grib/extract.hpp`): `extract_point`, `extract_points`,
  `extract_point_member`.
- **ecCodes** (Apache-2.0) is a pinned static ExternalProject; libaec
  1.1.6 is built from source and fed to it (distro libaec is too old for
  ecCodes' `>= 1.1.4` config check). Public headers do NOT include
  `<eccodes.h>` — it is a private build dependency.

## Conventions

- Code style: `.clang-format` (LLVM base, tabs, 100 cols)
- Namespace: `grib`
- **No `auto`** for local declarations — explicit types. Carve-outs:
  structured bindings, lambda closures, iterator-like (`.find()`).
  Enforced by `tools/cpp_auto_audit.py` (prints to **stderr**; check the
  exit code).
- **JSON**: Glaze v7.6.0 (`glz::generic`) for the ECMWF `.index` only —
  pinned to the estate-wide tag so a service FetchContent'ing grib-cpp +
  open-meteo-cpp + nws-cpp resolves a single `glaze::glaze`.
- **Dependency policy**: NCEPLIBS-g2c (LGPL-3) and wgrib2 (mixed/GNU)
  are forbidden — they would poison the MIT static-link model. See
  CONTRIBUTING.md.

## CI

`.github/workflows/ci.yml`: build + test + lint on Ubuntu 24.04 and
macOS, plus markdown-lint. **No Windows leg** (ecCodes static + MEMFS is
fragile under MSVC; the SDK is consumed only by Linux services).
`release.yml` auto-creates a GitHub Release on `vX.Y.Z` tag push and
hard-fails if the tag ≠ the CMakeLists `project(grib-cpp VERSION ...)`.
