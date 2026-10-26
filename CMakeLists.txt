cmake_minimum_required(VERSION 3.31)
project(grib-cpp VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Options
option(GRIB_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(GRIB_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(GRIB_ENABLE_LTO "Enable Link Time Optimization" ${PROJECT_IS_TOP_LEVEL})
option(GRIB_NATIVE_ARCH "Use -march=native for CPU-specific tuning" OFF)

# Compiler warnings. -Wpedantic / -Wextra are GCC/Clang only;
# MSVC's analogous switch is /W4 but it's noisy so we leave it off
# for the Windows path.
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Enable LTO if requested and supported
if(GRIB_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
    if(LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "LTO enabled for Release builds")
    else()
        message(WARNING "LTO not supported: ${LTO_ERROR}")
    endif()
endif()

# Native CPU tuning is explicit; portable defaults preserve parent flags.
if(GRIB_NATIVE_ARCH AND NOT MSVC)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()

# Sanitizer support
option(GRIB_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSanitizer" OFF)
if(GRIB_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "Sanitizers enabled (ASan + UBSan)")
endif()

# Code coverage support
option(GRIB_ENABLE_COVERAGE "Enable code coverage instrumentation (gcov)" OFF)
if(GRIB_ENABLE_COVERAGE)
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
    message(STATUS "Code coverage enabled")
endif()

# Find dependencies
find_package(CURL 7.85 REQUIRED)

include(FetchContent)

# Glaze — JSON library (compile-time reflection, 3-4x parse speedup over
# nlohmann on dense payloads). Used here to parse the ECMWF newline-JSON
# `.index` byte-range sidecars. License: MIT. Pinned to the estate-wide
# tag so a service that FetchContents grib-cpp + open-meteo-cpp + nws-cpp
# resolves a single glaze::glaze target (first-declaration-wins).
FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG v9.0.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glaze)

# ECMWF ecCodes (Apache-2.0) — the only license-compatible C/C++ GRIB2
# decoder. Built as a pinned, static, self-contained ExternalProject
# (MEMFS bakes the GRIB definitions into the .a so there is no runtime
# filesystem dependency — slim-container safe). NCEPLIBS-g2c is LGPL-3.0
# and is deliberately NOT used; see CONTRIBUTING.md.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildEccodes.cmake)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src)

if(GRIB_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GRIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install targets
set(GRIB_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/grib")

install(TARGETS grib_core grib_http grib_models grib_decode grib_api grib
    EXPORT gribTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/grib DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gribConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/gribConfig.cmake
    INSTALL_DESTINATION ${GRIB_INSTALL_CMAKEDIR}
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/gribConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
export(EXPORT gribTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/gribTargets.cmake
    NAMESPACE grib::
)
install(EXPORT gribTargets
    FILE gribTargets.cmake
    NAMESPACE grib::
    DESTINATION ${GRIB_INSTALL_CMAKEDIR}
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/gribConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/gribConfigVersion.cmake
    DESTINATION ${GRIB_INSTALL_CMAKEDIR}
)

# Private decoder archives are required by installed static consumers.
install(FILES ${ECCODES_LIB} ${ECCODES_MEMFS_LIB} ${LIBAEC_LIB} ${LIBAEC_SZ}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/grib/vendor)
install(FILES LICENSE.md NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/grib-cpp)
ExternalProject_Get_Property(eccodes_ep SOURCE_DIR)
install(FILES "${SOURCE_DIR}/LICENSE" "${SOURCE_DIR}/NOTICE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/grib-cpp/eccodes)
ExternalProject_Get_Property(libaec_ep SOURCE_DIR)
install(FILES "${SOURCE_DIR}/LICENSE.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/grib-cpp/libaec)
