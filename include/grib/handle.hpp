// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

/// @file handle.hpp
/// @brief RAII C++23 wrapper over the ecCodes C handle API.
///
/// ecCodes (Apache-2.0) decodes every GRIB2 packing template transparently
/// (simple / complex / spatial-diff / JPEG2000 / PNG / CCSDS). We never touch
/// packing — these wrappers just own the C handle and expose typed,
/// std::expected-returning accessors.

#include "grib/error.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace grib {

/// Owns a single decoded GRIB message (codes_handle*). Move-only.
class GribHandle {
public:
	/// Decode the first message in `bytes` (a copy is taken — `bytes` need
	/// not outlive the handle).
	[[nodiscard]] static Result<GribHandle> from_bytes(std::span<const std::byte> bytes);

	~GribHandle();
	GribHandle(GribHandle&&) noexcept;
	GribHandle& operator=(GribHandle&&) noexcept;
	GribHandle(const GribHandle&) = delete;
	GribHandle& operator=(const GribHandle&) = delete;

	[[nodiscard]] Result<double> get_double(std::string_view key) const;
	[[nodiscard]] Result<long> get_long(std::string_view key) const;
	[[nodiscard]] Result<std::string> get_string(std::string_view key) const;

	/// Full decoded grid ("values").
	[[nodiscard]] Result<std::vector<double>> values() const;

	/// Value at the grid point nearest to (lat, lon) — the station extract.
	[[nodiscard]] Result<double> nearest(double lat, double lon) const;

private:
	explicit GribHandle(void* h) noexcept;
	void* h_{nullptr}; // codes_handle*

	friend class GribFile;
};

/// Iterates the (possibly multiple) GRIB messages in one byte-range blob.
/// Move-only; owns its own copy of the buffer.
class GribFile {
public:
	[[nodiscard]] static Result<GribFile> open(std::span<const std::byte> bytes);

	~GribFile();
	GribFile(GribFile&&) noexcept;
	GribFile& operator=(GribFile&&) noexcept;
	GribFile(const GribFile&) = delete;
	GribFile& operator=(const GribFile&) = delete;

	/// Next message, or std::nullopt at end of buffer.
	[[nodiscard]] Result<std::optional<GribHandle>> next();

private:
	GribFile() = default;
	std::string buf_;
	std::size_t pos_{0};
};

} // namespace grib
