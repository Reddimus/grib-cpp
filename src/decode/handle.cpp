// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/handle.hpp"

#include <cstdint>
#include <cstring>
#include <eccodes.h>
#include <string>
#include <utility>

namespace grib {

namespace {

codes_handle* as_handle(void* p) {
	return static_cast<codes_handle*>(p);
}

// Locate the next GRIB message at/after `from` in `buf`. Returns true and
// sets [msg_start, msg_len) on success. Reads the message length from
// Section 0 (GRIB2: 8-byte big-endian at offset+8; GRIB1: 3-byte at +4).
bool find_message(const std::string& buf, std::size_t from, std::size_t& msg_start,
				  std::size_t& msg_len) {
	const std::size_t n = buf.size();
	for (std::size_t i = from; i + 8 <= n; ++i) {
		if (buf[i] == 'G' && buf[i + 1] == 'R' && buf[i + 2] == 'I' && buf[i + 3] == 'B') {
			const unsigned char edition = static_cast<unsigned char>(buf[i + 7]);
			std::uint64_t len = 0;
			if (edition == 2) {
				for (int b = 0; b < 8; ++b) {
					len = (len << 8) |
						  static_cast<unsigned char>(buf[i + 8 + static_cast<std::size_t>(b)]);
				}
			} else if (edition == 1) {
				for (int b = 0; b < 3; ++b) {
					len = (len << 8) |
						  static_cast<unsigned char>(buf[i + 4 + static_cast<std::size_t>(b)]);
				}
			} else {
				continue;
			}
			if (len == 0 || i + len > n) {
				return false; // truncated range — caller treats as decode error
			}
			msg_start = i;
			msg_len = static_cast<std::size_t>(len);
			return true;
		}
	}
	return false;
}

} // namespace

// ===== GribHandle =====

GribHandle::GribHandle(void* h) noexcept : h_(h) {}

GribHandle::~GribHandle() {
	if (h_) {
		codes_handle_delete(as_handle(h_));
	}
}

GribHandle::GribHandle(GribHandle&& o) noexcept : h_(o.h_) {
	o.h_ = nullptr;
}

GribHandle& GribHandle::operator=(GribHandle&& o) noexcept {
	if (this != &o) {
		if (h_) {
			codes_handle_delete(as_handle(h_));
		}
		h_ = o.h_;
		o.h_ = nullptr;
	}
	return *this;
}

Result<GribHandle> GribHandle::from_bytes(std::span<const std::byte> bytes) {
	const std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	std::size_t start = 0;
	std::size_t len = 0;
	if (!find_message(buf, 0, start, len)) {
		return std::unexpected(Error::decode("no complete GRIB message in buffer"));
	}
	codes_handle* h = codes_handle_new_from_message_copy(nullptr, buf.data() + start, len);
	if (!h) {
		return std::unexpected(Error::decode("codes_handle_new_from_message_copy failed"));
	}
	return GribHandle(static_cast<void*>(h));
}

Result<double> GribHandle::get_double(std::string_view key) const {
	double v = 0.0;
	const int err = codes_get_double(as_handle(h_), std::string(key).c_str(), &v);
	if (err != CODES_SUCCESS) {
		return std::unexpected(Error::decode(std::string("get_double ") + std::string(key) + ": " +
											 codes_get_error_message(err)));
	}
	return v;
}

Result<long> GribHandle::get_long(std::string_view key) const {
	long v = 0;
	const int err = codes_get_long(as_handle(h_), std::string(key).c_str(), &v);
	if (err != CODES_SUCCESS) {
		return std::unexpected(Error::decode(std::string("get_long ") + std::string(key) + ": " +
											 codes_get_error_message(err)));
	}
	return v;
}

Result<std::string> GribHandle::get_string(std::string_view key) const {
	char buf[1024];
	std::size_t len = sizeof(buf);
	const int err = codes_get_string(as_handle(h_), std::string(key).c_str(), buf, &len);
	if (err != CODES_SUCCESS) {
		return std::unexpected(Error::decode(std::string("get_string ") + std::string(key) + ": " +
											 codes_get_error_message(err)));
	}
	return std::string(buf);
}

Result<std::vector<double>> GribHandle::values() const {
	std::size_t size = 0;
	int err = codes_get_size(as_handle(h_), "values", &size);
	if (err != CODES_SUCCESS) {
		return std::unexpected(
			Error::decode(std::string("get_size values: ") + codes_get_error_message(err)));
	}
	std::vector<double> v(size);
	err = codes_get_double_array(as_handle(h_), "values", v.data(), &size);
	if (err != CODES_SUCCESS) {
		return std::unexpected(
			Error::decode(std::string("get_double_array values: ") + codes_get_error_message(err)));
	}
	v.resize(size);
	return v;
}

Result<double> GribHandle::nearest(double lat, double lon) const {
	int err = 0;
	codes_nearest* near = codes_grib_nearest_new(as_handle(h_), &err);
	if (!near || err != CODES_SUCCESS) {
		if (near) {
			codes_grib_nearest_delete(near);
		}
		return std::unexpected(
			Error::decode(std::string("nearest_new: ") + codes_get_error_message(err)));
	}
	double lats[4];
	double lons[4];
	double vals[4];
	double dist[4];
	int idx[4];
	std::size_t len = 4;
	err = codes_grib_nearest_find(near, as_handle(h_), lat, lon, 0, lats, lons, vals, dist, idx,
								  &len);
	codes_grib_nearest_delete(near);
	if (err != CODES_SUCCESS || len == 0) {
		return std::unexpected(
			Error::decode(std::string("nearest_find: ") + codes_get_error_message(err)));
	}
	std::size_t best = 0;
	for (std::size_t i = 1; i < len; ++i) {
		if (dist[i] < dist[best]) {
			best = i;
		}
	}
	return vals[best];
}

// ===== GribFile =====

GribFile::~GribFile() = default;

GribFile::GribFile(GribFile&& o) noexcept : buf_(std::move(o.buf_)), pos_(o.pos_) {
	o.pos_ = 0;
}

GribFile& GribFile::operator=(GribFile&& o) noexcept {
	if (this != &o) {
		buf_ = std::move(o.buf_);
		pos_ = o.pos_;
		o.pos_ = 0;
	}
	return *this;
}

Result<GribFile> GribFile::open(std::span<const std::byte> bytes) {
	GribFile f;
	f.buf_.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	f.pos_ = 0;
	return f;
}

Result<std::optional<GribHandle>> GribFile::next() {
	std::size_t start = 0;
	std::size_t len = 0;
	if (!find_message(buf_, pos_, start, len)) {
		return std::optional<GribHandle>{};
	}
	codes_handle* h = codes_handle_new_from_message_copy(nullptr, buf_.data() + start, len);
	pos_ = start + len;
	if (!h) {
		return std::unexpected(Error::decode("codes_handle_new_from_message_copy failed"));
	}
	return std::optional<GribHandle>(GribHandle(static_cast<void*>(h)));
}

} // namespace grib
