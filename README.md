# grib-cpp

C++23 library for NOAA/ECMWF GRIB sidecars, HTTP byte ranges, and point
extraction through pinned static ecCodes. Linux and macOS are supported.

## Build

Install CMake 3.31+, a C++23 compiler (GCC 13+, Clang 19+ with libstdc++,
or Clang 18+ with libc++), Python 3, Git, libcurl 7.85+, OpenJPEG, libpng,
and zlib development packages. Libcurl must support thread-safe global
initialization. CMake builds ecCodes 2.47.0 and libaec 1.1.6 from source.
Python is needed only while building embedded decoder definitions.

```sh
make build
make test
make lint
make consumers
```

`make debug` uses build-debug. Enable ASan/UBSan with
`-DGRIB_ENABLE_SANITIZERS=ON`; Debug alone does not enable them.

## Use

```cmake
include(FetchContent)
FetchContent_Declare(grib_cpp
    GIT_REPOSITORY https://github.com/Reddimus/grib-cpp.git
    GIT_TAG v0.2.0)
FetchContent_MakeAvailable(grib_cpp)
target_link_libraries(your_target PRIVATE grib::grib)
```

Tests, examples, and LTO default to OFF in embedded builds. Parent compiler
flags are preserved; native CPU tuning is opt-in. Installed consumers use
`find_package(grib CONFIG REQUIRED)` and the same target. Installation
includes the private decoder archives and their licenses, so consumers
need no ecCodes headers or original build directory. OpenJPEG, libpng,
zlib, and libcurl remain system dependencies.

Run `make run-nomads-idx` or `make run-ecmwf-index` for complete live
examples: fetch a sidecar, select a field, download its byte range, then
extract a temperature. Both use yesterday's 00z cycle, which must still
be available upstream. Offline tests decode the committed NOAA fixture.

## API behavior

- NOMADS `.idx` appends to the GRIB URL. ECMWF `.index` replaces `.grib2`.
- Sidecar parsers retain integer byte offsets exactly. Invalid numbers,
  fractional offsets, unordered NOMADS offsets, and unsupported NOMADS
  submessage record IDs fail explicitly.
- NOMADS uses names such as TMP and levels such as 2 m above ground.
  ECMWF uses names such as 2t and levtype sfc; keep those conventions separate.
- `get_range` checks arithmetic, HTTP 206, Content-Range, and body length.
  Zero length requests an open-ended range. A server ignoring Range fails.
- HTTP bodies default to a 64 MiB decompressed limit; set
  max_response_bytes to zero for larger full-file downloads.
- Truncated GRIB messages return errors rather than EOF. Moved-from handles
  fail safely. HTTP clients need external synchronization.
- Expected request/decode failures use Result. Allocation failures can throw.

## Dependencies and contributions

The SDK is MIT. ecCodes, libaec, and other dependencies retain their own
licenses; see [NOTICE](NOTICE). This build does not use NCEPLIBS-g2c or
wgrib2. See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
