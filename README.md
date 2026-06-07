# grib-cpp

[![CI](https://github.com/Reddimus/grib-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Reddimus/grib-cpp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Reddimus/grib-cpp)](https://github.com/Reddimus/grib-cpp/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

C++23 SDK for fetching and decoding **GRIB2** numerical-weather-model
output via HTTP **byte-range** reads.

A single NOAA/ECMWF model file is hundreds of MB; a trading model only
needs one field (e.g. 2 m temperature) per cycle. grib-cpp parses the
`.idx`/`.index` sidecar, pulls just that record (~1 MB) with an HTTP
`Range` request against the anonymous public bucket, and decodes it with
[ECMWF ecCodes](https://github.com/ecmwf/eccodes) — no whole-file
download, no AWS SDK, no credentials.

## Quick start

```cpp
#include <grib/grib.hpp>
#include <iostream>

int main() {
    grib::HttpClient client(grib::HttpClientConfig{});

    const std::string base =
        "https://noaa-gfs-bdp-pds.s3.amazonaws.com/"
        "gfs.20260516/00/atmos/gfs.t00z.pgrb2.0p25.f000";

    // 1. fetch + parse the sidecar
    grib::Result<grib::HttpResponse> idx = client.get(base + ".idx");
    grib::Result<std::vector<grib::GribIndexEntry>> entries =
        grib::parse_nomads_idx(idx->body);

    // 2. select the 2 m-temperature record
    grib::GribSelector sel;
    sel.param = "TMP";
    sel.level = "2 m above ground";
    std::vector<grib::GribIndexEntry> hit = grib::select(*entries, sel);

    // 3. byte-range GET just that record (~1 MB, not the whole file)
    grib::Result<grib::HttpResponse> blob =
        client.get_range(base, hit.front().offset, hit.front().length);

    // 4. decode + read the value at a point
    std::span<const std::byte> bytes(
        reinterpret_cast<const std::byte*>(blob->body.data()),
        blob->body.size());
    grib::Result<double> kelvin =
        grib::extract_point(bytes, 40.7128, -74.0060);

    std::cout << "NYC 2 m temp: " << *kelvin << " K\n";
}
```

See `examples/` (`make run-nomads-idx`, `make run-ecmwf-index`).

## Install (CMake FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
    grib_cpp
    GIT_REPOSITORY https://github.com/Reddimus/grib-cpp.git
    GIT_TAG v0.1.2
)
FetchContent_MakeAvailable(grib_cpp)
target_link_libraries(your_target PRIVATE grib)
```

The first configure builds a pinned, static ecCodes (+ libaec) as an
ExternalProject. Build hosts need `python3` (ecCodes `MEMFS` embeds the
GRIB definition tree into the archive — there is **no runtime python or
filesystem dependency**) plus `libopenjp2-7-dev libpng-dev libaec-dev
zlib1g-dev libcurl4-openssl-dev`.

## Architecture

- **Layered static libraries**: `grib_core` → `grib_http` →
  `grib_models` → `grib_decode` → `grib_api` → `grib` (INTERFACE).
- **C++23**: `std::expected<T, Error>` everywhere, no exceptions.
- **Sidecars**: NOMADS `.idx` (plain text, length = next-offset −
  offset, params like `TMP`) and ECMWF `.index` (newline JSON with
  explicit `_offset`/`_length`, params like `2t`) — two strictly
  separate parsers, never share vocabulary.
- **Decode**: RAII wrappers over the ecCodes C handle API; every GRIB2
  packing (simple / complex / spatial-diff / JPEG2000 / PNG / CCSDS)
  decoded transparently.

## Third-party components

grib-cpp is MIT. It statically links ECMWF ecCodes (Apache-2.0), libaec
(BSD-2), OpenJPEG (BSD-2), libpng, zlib, and Glaze (MIT). No copyleft is
present anywhere in the dependency chain — see [NOTICE](NOTICE).
NCEPLIBS-g2c (LGPL-3.0) is deliberately **not** used; see
[CONTRIBUTING.md](CONTRIBUTING.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Security reports:
[SECURITY.md](SECURITY.md).

## License

MIT — see [LICENSE.md](LICENSE.md).
