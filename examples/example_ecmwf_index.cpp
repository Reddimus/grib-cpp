// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
//
// Same flow as example_nomads_idx but against ECMWF open data (CC-BY-4.0):
// the sidecar is a newline-delimited JSON `.index` with explicit
// _offset/_length, and the 2 m-temperature param is `2t` (not `TMP`).

#include <cstddef>
#include <ctime>
#include <grib/grib.hpp>
#include <iostream>
#include <span>
#include <string>

int main() {
	// Yesterday 00z IFS oper, step 0.
	std::time_t now = std::time(nullptr) - 24 * 3600;
	std::tm tm_utc{};
	gmtime_r(&now, &tm_utc);
	char ymd[16];
	std::strftime(ymd, sizeof(ymd), "%Y%m%d", &tm_utc);

	const std::string base = std::string("https://data.ecmwf.int/forecasts/") + ymd +
							 "/00z/ifs/0p25/oper/" + ymd + "000000-0h-oper-fc.grib2";
	const std::string idx_url = base + ".index";

	grib::HttpClient client(grib::HttpClientConfig{});

	grib::Result<grib::HttpResponse> idx = client.get(idx_url);
	if (!idx || idx->status_code != 200) {
		std::cerr << "index fetch failed (" << idx_url << ")\n";
		return 1;
	}

	grib::Result<std::vector<grib::GribIndexEntry>> entries = grib::parse_ecmwf_index(idx->body);
	if (!entries) {
		std::cerr << "parse failed: " << entries.error().message << "\n";
		return 1;
	}

	grib::GribSelector sel;
	sel.param = "2t";
	std::vector<grib::GribIndexEntry> hits = grib::select(*entries, sel);
	if (hits.empty()) {
		std::cerr << "no 2t record in index\n";
		return 1;
	}
	const grib::GribIndexEntry& rec = hits.front();
	std::cout << "offset=" << rec.offset << " length=" << rec.length << "\n";

	grib::Result<grib::HttpResponse> blob = client.get_range(base, rec.offset, rec.length);
	if (!blob) {
		std::cerr << "range fetch failed: " << blob.error().message << "\n";
		return 1;
	}
	std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(blob->body.data()),
									 blob->body.size());

	const double lat = 51.5074;
	const double lon = -0.1278; // London
	grib::Result<double> kelvin = grib::extract_point(bytes, lat, lon);
	if (!kelvin) {
		std::cerr << "extract failed: " << kelvin.error().message << "\n";
		return 1;
	}
	std::cout << "London 2 m temp: " << *kelvin << " K\n";
	return 0;
}
