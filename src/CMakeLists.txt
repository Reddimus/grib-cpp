# Core library (error, rate limiting, retry)
add_library(grib_core STATIC
    core/error.cpp
    core/rate_limit.cpp
    core/retry.cpp
)
target_compile_features(grib_core PUBLIC cxx_std_23)
target_include_directories(grib_core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
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
    $<BUILD_INTERFACE:grib_eccodes>
    $<INSTALL_INTERFACE:grib::eccodes>
)
target_include_directories(grib_decode PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
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

add_library(grib::grib ALIAS grib)
target_include_directories(grib_models SYSTEM PRIVATE ${glaze_SOURCE_DIR}/include)
target_include_directories(grib_decode SYSTEM PRIVATE ${ECCODES_INCLUDE})

if(WIN32)
    target_compile_definitions(grib_http PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
