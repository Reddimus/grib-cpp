# BuildEccodes.cmake — vendored, pinned, static ECMWF ecCodes (+ libaec).
#
# ecCodes (Apache-2.0) is the only license-compatible C/C++ GRIB2 decoder.
# NCEPLIBS-g2c is LGPL-3.0 and would impose relink obligations on every
# downstream consumer of this MIT, statically-linked SDK, so it is
# deliberately NOT used (see CONTRIBUTING.md).
#
# Built as an ExternalProject so its ecbuild-based CMake never enters our
# target/export graph. ecCodes self-bootstraps ecbuild 3.12.0 via
# FetchContent at its own configure step (no submodules); the only
# build-host requirements are network + python3 (MEMFS embeds the GRIB
# definition tree into libeccodes.a — no runtime filesystem dependency,
# which keeps consuming containers slim).
#
# ecCodes 2.47 requires libaec >= 1.1.4 via a CMake package config. Distro
# libaec (Ubuntu 24.04 ships 1.1.2, no config) is too old, so libaec is
# itself built from source as a pinned ExternalProject and fed to ecCodes
# via CMAKE_PREFIX_PATH. This keeps the build identical on the dev host,
# CI, and the ubuntu:24.04 service Dockerfiles.

include(ExternalProject)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

set(ECCODES_VERSION 2.47.0)
set(ECCODES_PREFIX   ${CMAKE_BINARY_DIR}/eccodes-prefix)
set(ECCODES_INCLUDE  ${ECCODES_PREFIX}/include)
set(ECCODES_LIB      ${ECCODES_PREFIX}/lib/libeccodes.a)
# ENABLE_MEMFS=ON + static emits a SECOND archive holding the embedded
# GRIB definition tree; libeccodes.a has `U codes_memfs_exists` resolved
# only by this one. It must be on the link line too.
set(ECCODES_MEMFS_LIB ${ECCODES_PREFIX}/lib/libeccodes_memfs.a)

set(LIBAEC_VERSION 1.1.6)
set(LIBAEC_PREFIX  ${CMAKE_BINARY_DIR}/libaec-prefix)
set(LIBAEC_LIB     ${LIBAEC_PREFIX}/lib/libaec.a)
set(LIBAEC_SZ      ${LIBAEC_PREFIX}/lib/libsz.a)

# JPEG2000 (DRT 5.40) and PNG (DRT 5.41) decompressors — distro dev libs
# are recent enough for ecCodes' detection (verified at configure).
find_library(OPENJP2_LIB NAMES openjp2 libopenjp2)
find_library(PNG_LIB NAMES png png16 libpng16)
if(NOT OPENJP2_LIB OR NOT PNG_LIB)
    message(FATAL_ERROR
        "grib-cpp: missing GRIB2 decompressor dev libs. Install "
        "libopenjp2-7-dev libpng-dev (zlib1g-dev). "
        "openjp2=${OPENJP2_LIB} png=${PNG_LIB}")
endif()

find_program(PYTHON3_FOR_MEMFS NAMES python3 python REQUIRED)

# --- libaec (CCSDS/AEC, DRT 5.42) — pinned, static, with CMake config ---
ExternalProject_Add(libaec_ep
    GIT_REPOSITORY https://github.com/MathisRosenhauer/libaec.git
    GIT_TAG        v${LIBAEC_VERSION}
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${LIBAEC_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=OFF
    BUILD_BYPRODUCTS ${LIBAEC_LIB} ${LIBAEC_SZ}
)

# --- ecCodes ---
ExternalProject_Add(eccodes_ep
    GIT_REPOSITORY https://github.com/ecmwf/eccodes.git
    GIT_TAG        ${ECCODES_VERSION}
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
    DEPENDS        libaec_ep
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${ECCODES_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_PREFIX_PATH=${LIBAEC_PREFIX}
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_MEMFS=ON
        -DENABLE_INSTALL_ECCODES_DEFINITIONS=OFF
        -DENABLE_INSTALL_ECCODES_SAMPLES=OFF
        -DENABLE_FORTRAN=OFF
        -DENABLE_NETCDF=OFF
        -DENABLE_PYTHON=OFF
        -DENABLE_BUILD_TOOLS=OFF
        -DENABLE_EXAMPLES=OFF
        -DENABLE_TESTS=OFF
        -DENABLE_PRODUCT_BUFR=OFF
        -DENABLE_JPG=ON
        -DENABLE_JPG_LIBOPENJPEG=ON
        -DENABLE_JPG_LIBJASPER=OFF
        -DENABLE_PNG=ON
        -DENABLE_AEC=ON
        -DENABLE_USE_SHARED_LIB_AEC=OFF
        -DPython3_EXECUTABLE=${PYTHON3_FOR_MEMFS}
        -DPYTHON_EXECUTABLE=${PYTHON3_FOR_MEMFS}
    BUILD_BYPRODUCTS ${ECCODES_LIB} ${ECCODES_MEMFS_LIB}
)

# INTERFACE_INCLUDE_DIRECTORIES must exist at configure time even though
# ExternalProject populates it at build time.
file(MAKE_DIRECTORY ${ECCODES_INCLUDE})

# INTERFACE imported lib (no IMPORTED_LOCATION) so libeccodes.a appears
# exactly once, bracketed with libeccodes_memfs.a inside a linker group:
# libeccodes.a needs codes_memfs_exists (in memfs.a) and memfs.a's
# per-chunk objects cross-reference each other, so a group makes the
# cyclic static resolution order-independent. GNU ld only; Apple ld
# re-scans archives so the group is omitted there.
add_library(eccodes::eccodes INTERFACE IMPORTED GLOBAL)
set_target_properties(eccodes::eccodes PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${ECCODES_INCLUDE}
    INTERFACE_LINK_LIBRARIES
        "$<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--start-group>;${ECCODES_LIB};${ECCODES_MEMFS_LIB};$<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--end-group>;${OPENJP2_LIB};${PNG_LIB};${LIBAEC_LIB};${LIBAEC_SZ};ZLIB::ZLIB;Threads::Threads;m;${CMAKE_DL_LIBS}"
)
add_dependencies(eccodes::eccodes eccodes_ep)
