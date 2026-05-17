# Deterministic offline tests (ctest). The sidecar tests need no network;
# the handle test decodes a committed public-domain NOAA GRIB2 sample.

set(GRIB_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(grib_sidecar_test sidecar_test.cpp)
target_link_libraries(grib_sidecar_test PRIVATE gtest_main grib)
if(NOT MSVC)
	target_compile_options(grib_sidecar_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME grib_sidecar_test COMMAND grib_sidecar_test)

add_executable(grib_handle_test handle_test.cpp)
target_link_libraries(grib_handle_test PRIVATE gtest_main grib)
target_compile_definitions(grib_handle_test PRIVATE
	GRIB_FIXTURES_DIR="${GRIB_FIXTURES_DIR}")
if(NOT MSVC)
	target_compile_options(grib_handle_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME grib_handle_test COMMAND grib_handle_test)
set_tests_properties(grib_handle_test PROPERTIES TIMEOUT 30)
