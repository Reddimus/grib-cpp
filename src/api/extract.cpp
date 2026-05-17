// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/extract.hpp"

#include "grib/handle.hpp"

#include <optional>

namespace grib {

Result<double> extract_point(std::span<const std::byte> blob, double lat, double lon) {
	Result<GribHandle> h = GribHandle::from_bytes(blob);
	if (!h) {
		return std::unexpected(h.error());
	}
	return h->nearest(lat, lon);
}

Result<std::vector<std::pair<std::string, double>>>
extract_points(std::span<const std::byte> blob, std::span<const Station> stations) {
	Result<GribHandle> h = GribHandle::from_bytes(blob);
	if (!h) {
		return std::unexpected(h.error());
	}
	std::vector<std::pair<std::string, double>> out;
	out.reserve(stations.size());
	for (const Station& s : stations) {
		Result<double> v = h->nearest(s.lat, s.lon);
		if (!v) {
			return std::unexpected(v.error());
		}
		out.emplace_back(s.id, *v);
	}
	return out;
}

Result<double> extract_point_member(std::span<const std::byte> blob, double lat, double lon,
									int member) {
	Result<GribFile> f = GribFile::open(blob);
	if (!f) {
		return std::unexpected(f.error());
	}
	while (true) {
		Result<std::optional<GribHandle>> msg = f->next();
		if (!msg) {
			return std::unexpected(msg.error());
		}
		if (!msg->has_value()) {
			break;
		}
		const GribHandle& h = **msg;
		int m = -1;
		Result<long> pn = h.get_long("perturbationNumber");
		if (pn) {
			m = static_cast<int>(*pn);
		} else {
			Result<long> num = h.get_long("number");
			if (num) {
				m = static_cast<int>(*num);
			}
		}
		if (m == member) {
			return h.nearest(lat, lon);
		}
	}
	return std::unexpected(Error::decode("no GRIB message with member " + std::to_string(member)));
}

} // namespace grib
