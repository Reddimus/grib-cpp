# Changelog

All notable changes to **grib-cpp** are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-05-17

### Added

- Initial public release. A C++23 SDK for fetching and decoding GRIB2
  weather-model output via HTTP byte-range reads — the cheap, low-latency
  way to pull one 2 m-temperature field (~1 MB) out of a multi-hundred-MB
  NOAA / ECMWF model file instead of downloading the whole thing.
- Layered static libraries: `grib_core` -> `grib_http` -> `grib_models`
  -> `grib_decode` -> `grib_api` -> `grib` (INTERFACE).
- `std::expected<T, Error>` for all returns; no exceptions.
- `HttpClient` (libcurl pimpl): GET and `get_range()` byte-range GET.
  Anonymous public S3 / NOMADS buckets need no SigV4, so there is **no
  aws-sdk-cpp dependency**.
- Byte-range sidecar parsers (`grib/sidecar.hpp`): `parse_nomads_idx`
  (plain-text `.idx`, length = next-offset minus offset) and
  `parse_ecmwf_index` (newline-delimited JSON `.index` with explicit
  `_offset`/`_length`, parsed with Glaze). The two formats are strictly
  separate — they share an entry shape but not their param vocabularies
  (NOMADS `TMP` vs ECMWF `2t`). `select()` filters by
  param/level/step/member.
- RAII ecCodes wrappers (`grib/handle.hpp`): `GribHandle`
  (`get_double/long/string`, `values()`, `nearest(lat,lon)`) and
  `GribFile` (iterates the messages in a multi-record blob).
- Station extraction (`grib/extract.hpp`): `extract_point`,
  `extract_points`, `extract_point_member`.
- Vendored, pinned, **static** ECMWF ecCodes 2.47.0 (Apache-2.0) built as
  an ExternalProject with `ENABLE_MEMFS=ON` (GRIB definitions embedded in
  the archive — no runtime filesystem dependency). libaec 1.1.6 is built
  from source as a pinned ExternalProject and fed to ecCodes (distro
  libaec is too old for ecCodes' `>= 1.1.4` config-mode check). JPEG2000
  (openjpeg) / PNG / CCSDS-AEC packings all enabled.
- House toolchain parity: `Makefile`
  (build/debug/test/lint/format/coverage/pre-commit/install-hooks),
  `tools/cpp_auto_audit.py` (bans bare local `auto`), `.clang-format`
  (LLVM, tabs, 100 col), GoogleTest via FetchContent, Glaze v7.6.0
  pinned to the estate-wide tag, CMake `install(EXPORT)` /
  `find_package(grib-cpp)`, auto-release-on-tag workflow.
- CI on `ubuntu-24.04` + `macos-latest` (build + test + lint) and
  markdown-lint. No Windows leg — ecCodes static + MEMFS is fragile
  under MSVC and the SDK is consumed only by Linux services.
- Public-domain NOAA GRIB2 2 m-temperature sample committed as a decode
  test fixture; deterministic offline sidecar tests.

[Unreleased]: https://github.com/Reddimus/grib-cpp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Reddimus/grib-cpp/releases/tag/v0.1.0
