# Security Policy

`grib-cpp` is a C++ client that fetches GRIB2 weather-model files over
HTTP (byte-range GETs against anonymous public buckets) and decodes them
with ECMWF ecCodes. The two relevant attack surfaces are: (1) request
construction / URL handling in the libcurl client, and (2) parsing
untrusted bytes — the `.idx`/`.index` sidecars and the GRIB2 payload fed
to ecCodes. This file is the canonical contact path for reporting an
issue in either.

## Supported Versions

Security fixes are made on the latest published `vX.Y.Z` tag. Older
tags are not back-patched — bump your `FetchContent_Declare(... GIT_TAG
...)` pin or your `find_package(grib-cpp X.Y.Z REQUIRED)` constraint to
the latest minor on the same major as part of the upgrade.

| Version    | Supported          |
| ---------- | ------------------ |
| latest tag | :white_check_mark: |
| older      | :x:                |

## Reporting a Vulnerability

**Do not open a public issue.** Use GitHub's [private vulnerability
reporting](https://github.com/Reddimus/grib-cpp/security/advisories/new)
flow, which delivers the report to the maintainer privately and tracks
coordinated disclosure.

When reporting, please include:

- Affected version (tag or commit SHA)
- A reproduction — minimal code or a sidecar/GRIB sample
- Impact (memory safety in a parser / forged request / DoS / other)
- Whether you've notified anyone else (e.g. ECMWF for an ecCodes issue)

You can expect:

- Acknowledgement within **3 business days**
- An initial assessment + severity rating within **7 business days**
- A fix on a new `vX.Y.Z+1` tag, or a clear timeline if the fix is
  larger

## Out of Scope

- Bugs in upstream data providers (NOAA NOMADS, AWS open-data buckets,
  ECMWF open data) — not this client library.
- Operational issues (a model cycle not posted yet → `FeedUnavailable`,
  network blips) — file a regular issue.
- Theoretical issues against dependencies — report them upstream
  (`ecCodes`, `libaec`, `openjpeg`, `libpng`, `libcurl`, `glaze`,
  `googletest`). We pin via FetchContent / ExternalProject and bump on
  credible advisories.
