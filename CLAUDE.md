# Development

Use make build/test/lint/format/consumers. Debug uses build-debug;
GRIB_ENABLE_SANITIZERS enables ASan/UBSan. Test fixtures are required.

C++23, namespace grib, tabs, 100 columns, explicit local types except
iterators, structured bindings, and lambda closures. Layers are core,
HTTP, sidecar models, decode, extraction; umbrella target grib::grib.

Keep NOMADS and ECMWF sidecar conventions separate (see README). Parse
complete bounded JSON with generic_u64 to preserve offsets. Validate all
range arithmetic and Content-Range. Never unwind exceptions through C
callbacks. A truncated message is not clean EOF.

Decoder headers stay private. Installed packages must include ecCodes,
MEMFS, and libaec archives plus upstream licenses. Keep static/MEMFS builds;
do not add NCEPLIBS-g2c or wgrib2. Python is build-only.

Run offline fixture tests, relocated installed/FetchContent consumers, and
live examples when feeds are available. CI covers Linux and macOS; Windows
is unsupported. Merge passing PRs before signed release tags.
