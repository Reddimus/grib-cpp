# Layered static libraries:
#   grib_core -> grib_http -> grib_models -> grib_decode -> grib_api -> grib
#
# Glaze and ecCodes are pulled in via BUILD_INTERFACE only (Glaze through its
# include path, ecCodes through the IMPORTED eccodes::eccodes archive). This
# sidesteps CMake's "INTERFACE/IMPORTED target not in export set" failure that
# fires if they are attached to installable static libs. The estate consumes
# this SDK via FetchContent (build-from-source), so the BUILD_INTERFACE link
# is the real path; the find_package/install(EXPORT) path is best-effort.

# Core library (error, rate limiting, retry)
add_library(grib_core STATIC
    core/error.cpp
    core/rate_limit.cpp
    core/retry.cpp
)
target_compile_features(grib_core PUBLIC cxx_std_23)
target_include_directories(grib_core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${glaze_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# HTTP client library (libcurl; GET + byte-range GET for .idx/.index sidecars)
add_library(grib_http STATIC
    http/client.cpp
)
target_link_libraries(grib_http PUBLIC grib_core CURL::libcurl)
target_include_directories(grib_http PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Models library — byte-range sidecar parsers:
#   NOMADS `.idx`  (plain text, length = next-offset - offset)
#   ECMWF  `.index` (newline-delimited JSON, explicit _offset/_length) via Glaze
add_library(grib_models STATIC
    models/sidecar.cpp
)
target_link_libraries(grib_models PUBLIC grib_core)
target_include_directories(grib_models PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${glaze_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Decode library — RAII C++23 wrapper over the ecCodes C handle API.
# eccodes_ep (ExternalProject) produces libeccodes.a; the BUILD_INTERFACE
# wrap keeps the IMPORTED target out of the export set (Glaze precedent).
add_library(grib_decode STATIC
    decode/handle.cpp
)
add_dependencies(grib_decode eccodes_ep)
target_link_libraries(grib_decode PUBLIC
    grib_core
    $<BUILD_INTERFACE:eccodes::eccodes>
)
target_include_directories(grib_decode PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${ECCODES_INCLUDE}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# API library — high-level station point/per-member extraction
add_library(grib_api STATIC
    api/extract.cpp
)
target_link_libraries(grib_api PUBLIC grib_core grib_http grib_models grib_decode)
target_include_directories(grib_api PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Main SDK library (combines all)
add_library(grib INTERFACE)
target_link_libraries(grib INTERFACE grib_core grib_http grib_models grib_decode grib_api)
