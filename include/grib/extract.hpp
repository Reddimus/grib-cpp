// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

/// @file extract.hpp
/// @brief High-level station extraction from a byte-range GRIB blob.
///
/// Typical flow: fetch the sidecar (.idx/.index), `select()` the record(s),
/// `HttpClient::get_range()` just those bytes, then pull the per-station
/// value(s) here. The blob usually holds exactly the selected record(s);
/// the *_member helper disambiguates when several members share one blob.

#include "grib/error.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace grib {

struct Station {
	std::string id;
	double lat{0.0};
	double lon{0.0};
};

/// Nearest-grid-point value from the FIRST GRIB message in `blob`.
[[nodiscard]] Result<double> extract_point(std::span<const std::byte> blob, double lat, double lon);

/// Nearest value per station from the FIRST GRIB message in `blob`.
[[nodiscard]] Result<std::vector<std::pair<std::string, double>>>
extract_points(std::span<const std::byte> blob, std::span<const Station> stations);

/// Nearest value from the message in `blob` whose ensemble member
/// (`perturbationNumber`, fallback `number`) equals `member` (0 == control).
[[nodiscard]] Result<double> extract_point_member(std::span<const std::byte> blob, double lat,
												  double lon, int member);

} // namespace grib
