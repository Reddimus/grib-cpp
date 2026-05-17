// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
//
// Fetch a public NOAA GFS `.idx`, select the 2 m-temperature record, pull
// just that record via an HTTP byte-range GET, and read the value at a
// point. Demonstrates the whole grib-cpp pipeline against an anonymous
// public S3 bucket (no credentials).

#include <cstddef>
#include <ctime>
#include <grib/grib.hpp>
#include <iostream>
#include <span>
#include <string>

int main() {
	// Yesterday 00z f000 — recent enough to exist on the rolling bucket.
	std::time_t now = std::time(nullptr) - 24 * 3600;
	std::tm tm_utc{};
	gmtime_r(&now, &tm_utc);
	char ymd[16];
	std::strftime(ymd, sizeof(ymd), "%Y%m%d", &tm_utc);

	const std::string base = std::string("https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs.") + ymd +
							 "/00/atmos/gfs.t00z.pgrb2.0p25.f000";
	const std::string idx_url = base + ".idx";

	grib::HttpClient client(grib::HttpClientConfig{});

	grib::Result<grib::HttpResponse> idx = client.get(idx_url);
	if (!idx || idx->status_code != 200) {
		std::cerr << "idx fetch failed (" << idx_url << ")\n";
		return 1;
	}

	grib::Result<std::vector<grib::GribIndexEntry>> entries = grib::parse_nomads_idx(idx->body);
	if (!entries) {
		std::cerr << "parse failed: " << entries.error().message << "\n";
		return 1;
	}

	grib::GribSelector sel;
	sel.param = "TMP";
	sel.level = "2 m above ground";
	std::vector<grib::GribIndexEntry> hits = grib::select(*entries, sel);
	if (hits.empty()) {
		std::cerr << "no TMP:2 m above ground record in idx\n";
		return 1;
	}
	const grib::GribIndexEntry& rec = hits.front();
	std::cout << "record " << rec.record << " offset=" << rec.offset << " length=" << rec.length
			  << "\n";

	grib::Result<grib::HttpResponse> blob = client.get_range(base, rec.offset, rec.length);
	if (!blob) {
		std::cerr << "range fetch failed: " << blob.error().message << "\n";
		return 1;
	}
	std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(blob->body.data()),
									 blob->body.size());

	const double lat = 40.7128;
	const double lon = -74.0060; // New York City
	grib::Result<double> kelvin = grib::extract_point(bytes, lat, lon);
	if (!kelvin) {
		std::cerr << "extract failed: " << kelvin.error().message << "\n";
		return 1;
	}
	const double fahrenheit = (*kelvin - 273.15) * 9.0 / 5.0 + 32.0;
	std::cout << "NYC 2 m temp: " << *kelvin << " K (" << fahrenheit << " F)\n";
	return 0;
}
