# Examples — small standalone binaries that demonstrate SDK usage.
#
# Add a new example by:
#   1. Dropping example_<name>.cpp in this directory.
#   2. Calling `add_example(<name>)` here.
# Each example fetches a real public GRIB sidecar + one byte-range record;
# keep them minimal so they double as documentation.

function(add_example name)
	add_executable(example_${name} example_${name}.cpp)
	target_link_libraries(example_${name} PRIVATE grib)
	target_compile_features(example_${name} PRIVATE cxx_std_23)
endfunction()

add_example(nomads_idx)
add_example(ecmwf_index)
