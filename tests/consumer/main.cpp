#include <cmath>
#include <fstream>
#include <grib/grib.hpp>
#include <iterator>
int main(int argc, char** argv) {
	if (argc != 2) {
		return 2;
	}
	std::ifstream file(argv[1], std::ios::binary);
	const std::string bytes((std::istreambuf_iterator<char>(file)),
							std::istreambuf_iterator<char>());
	const std::span<const std::byte> data(reinterpret_cast<const std::byte*>(bytes.data()),
										  bytes.size());
	const grib::Result<double> kelvin = grib::extract_point(data, 40.7128, -74.0060);
	return kelvin && std::isfinite(*kelvin) && *kelvin > 180 && *kelvin < 340 ? 0 : 1;
}
