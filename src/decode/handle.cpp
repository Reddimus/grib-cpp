// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/handle.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <eccodes.h>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace grib {

namespace {

codes_handle* as_handle(void* p) {
	return static_cast<codes_handle*>(p);
}

// ecCodes loads its MEMFS-embedded definition tree lazily on the first handle
// creation; that init mutates non-thread-safe global parser state (the flex
// scanner / grib_context), so concurrent first-decodes across worker threads
// race and abort the process with "Cannot create accessor ... action_class_noop
// / fatal flex scanner internal error". Serialize only the FIRST handle
// creation process-wide via double-checked locking — once the context + common
// definitions are warm, every subsequent decode runs fully concurrently (the
// fast path is a single relaxed-after-acquire atomic load, no lock).
std::mutex g_eccodes_init_mtx;
std::atomic<bool> g_eccodes_ready{false};

codes_handle* make_handle_serialized(const void* msg, std::size_t len) {
	if (!g_eccodes_ready.load(std::memory_order_acquire)) {
		const std::lock_guard<std::mutex> lock(g_eccodes_init_mtx);
		if (!g_eccodes_ready.load(std::memory_order_relaxed)) {
			codes_handle* const h = codes_handle_new_from_message_copy(nullptr, msg, len);
			if (h != nullptr) {
				g_eccodes_ready.store(true, std::memory_order_release);
			}
			return h;
		}
	}
	return codes_handle_new_from_message_copy(nullptr, msg, len);
}

// Distinguish clean EOF from invalid/truncated GRIB messages.
enum class ScanResult { Found, End, Invalid };
ScanResult find_message(std::string_view buf, std::size_t from, std::size_t& msg_start,
						std::size_t& msg_len) {
	const std::size_t n = buf.size();
	for (std::size_t i = from; i < n && n - i >= 4; ++i) {
		if (buf[i] == 'G' && buf[i + 1] == 'R' && buf[i + 2] == 'I' && buf[i + 3] == 'B') {
			if (n - i < 8) {
				return ScanResult::Invalid;
			}
			const unsigned char edition = static_cast<unsigned char>(buf[i + 7]);
			std::uint64_t len = 0;
			if (edition == 2) {
				if (n - i < 16) {
					return ScanResult::Invalid;
				}
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
			if (len < (edition == 2 ? 20U : 12U) || len > n - i) {
				return ScanResult::Invalid; // truncated range — caller treats as decode error
			}
			if (buf.compare(i + static_cast<std::size_t>(len) - 4, 4, "7777") != 0) {
				return ScanResult::Invalid;
			}
			msg_start = i;
			msg_len = static_cast<std::size_t>(len);
			return ScanResult::Found;
		}
	}
	return ScanResult::End;
}

} // namespace

// ===== GribHandle =====

GribHandle::GribHandle(void* h) noexcept : h_(h) {}

GribHandle::~GribHandle() {
	if (nearest_) {
		codes_grib_nearest_delete(static_cast<codes_nearest*>(nearest_));
	}
	if (h_) {
		codes_handle_delete(as_handle(h_));
	}
}

GribHandle::GribHandle(GribHandle&& o) noexcept
	: h_(std::exchange(o.h_, nullptr)), nearest_(std::exchange(o.nearest_, nullptr)) {}

GribHandle& GribHandle::operator=(GribHandle&& o) noexcept {
	if (this != &o) {
		if (nearest_) {
			codes_grib_nearest_delete(static_cast<codes_nearest*>(nearest_));
		}
		if (h_) {
			codes_handle_delete(as_handle(h_));
		}
		h_ = std::exchange(o.h_, nullptr);
		nearest_ = std::exchange(o.nearest_, nullptr);
	}
	return *this;
}

Result<GribHandle> GribHandle::from_bytes(std::span<const std::byte> bytes) {
	if (bytes.empty()) {
		return std::unexpected(Error::decode("Empty GRIB buffer"));
	}
	const std::string_view buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	std::size_t start = 0;
	std::size_t len = 0;
	if (find_message(buf, 0, start, len) != ScanResult::Found) {
		return std::unexpected(Error::decode("no complete GRIB message in buffer"));
	}
	codes_handle* h = make_handle_serialized(buf.data() + start, len);
	if (!h) {
		return std::unexpected(Error::decode("codes_handle_new_from_message_copy failed"));
	}
	return GribHandle(static_cast<void*>(h));
}

Result<double> GribHandle::get_double(std::string_view key) const {
	if (!h_ || key.find('\0') != std::string_view::npos) {
		return std::unexpected(Error::decode("Invalid handle or key"));
	}
	double v = 0.0;
	const int err = codes_get_double(as_handle(h_), std::string(key).c_str(), &v);
	if (err != CODES_SUCCESS) {
		return std::unexpected(Error::decode(std::string("get_double ") + std::string(key) + ": " +
											 codes_get_error_message(err)));
	}
	return v;
}

Result<long> GribHandle::get_long(std::string_view key) const {
	if (!h_ || key.find('\0') != std::string_view::npos) {
		return std::unexpected(Error::decode("Invalid handle or key"));
	}
	long v = 0;
	const int err = codes_get_long(as_handle(h_), std::string(key).c_str(), &v);
	if (err != CODES_SUCCESS) {
		return std::unexpected(Error::decode(std::string("get_long ") + std::string(key) + ": " +
											 codes_get_error_message(err)));
	}
	return v;
}

Result<std::string> GribHandle::get_string(std::string_view key) const {
	if (!h_ || key.find('\0') != std::string_view::npos) {
		return std::unexpected(Error::decode("Invalid handle or key"));
	}
	const std::string name(key);
	std::size_t size = 0;
	int error = codes_get_length(as_handle(h_), name.c_str(), &size);
	if (error != CODES_SUCCESS || size == std::numeric_limits<std::size_t>::max()) {
		return std::unexpected(Error::decode("Could not determine string length"));
	}
	std::string value(size + 1, '\0');
	size = value.size();
	error = codes_get_string(as_handle(h_), name.c_str(), value.data(), &size);
	if (error != CODES_SUCCESS) {
		return std::unexpected(Error::decode(codes_get_error_message(error)));
	}
	value.resize(std::strlen(value.c_str()));
	return value;
}

Result<std::vector<double>> GribHandle::values() const {
	if (!h_) {
		return std::unexpected(Error::decode("Handle not initialized"));
	}
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
	if (!h_ || !std::isfinite(lat) || lat < -90 || lat > 90 || !std::isfinite(lon)) {
		return std::unexpected(Error::decode("Invalid handle or coordinates"));
	}
	lon = std::fmod(lon, 360.0);
	if (lon < 0) {
		lon += 360.0;
	}
	int err = 0;
	if (!nearest_) {
		codes_nearest* created = codes_grib_nearest_new(as_handle(h_), &err);
		if (!created || err != CODES_SUCCESS) {
			if (created) {
				codes_grib_nearest_delete(created);
			}
			return std::unexpected(
				Error::decode(std::string("nearest_new: ") + codes_get_error_message(err)));
		}
		nearest_ = created;
	}
	codes_nearest* near = static_cast<codes_nearest*>(nearest_);

	double lats[4];
	double lons[4];
	double vals[4];
	double dist[4];
	int idx[4];
	std::size_t len = 4;
	err = codes_grib_nearest_find(near, as_handle(h_), lat, lon,
								  CODES_NEAREST_SAME_GRID | CODES_NEAREST_SAME_DATA, lats, lons,
								  vals, dist, idx, &len);
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
	if (!bytes.empty()) {
		f.buf_.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}
	f.pos_ = 0;
	return f;
}

Result<std::optional<GribHandle>> GribFile::next() {
	std::size_t start = 0;
	std::size_t len = 0;
	const ScanResult scan = find_message(buf_, pos_, start, len);
	if (scan == ScanResult::Invalid) {
		return std::unexpected(Error::decode("Incomplete or invalid GRIB message"));
	}
	if (scan == ScanResult::End) {
		return std::optional<GribHandle>{};
	}
	codes_handle* h = make_handle_serialized(buf_.data() + start, len);
	pos_ = start + len;
	if (!h) {
		return std::unexpected(Error::decode("codes_handle_new_from_message_copy failed"));
	}
	return std::optional<GribHandle>(GribHandle(static_cast<void*>(h)));
}

} // namespace grib
