// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

/// @file sidecar.hpp
/// @brief GRIB2 byte-range index ("sidecar") parsers.
///
/// Two strictly-separate formats — they share an entry shape but NOTHING
/// else (param vocabularies differ: NOMADS says `TMP`, ECMWF says `2t`):
///
///  - NOMADS `.idx`  : plain text, one record per line
///       `record:byteoffset:d=YYYYMMDDHH:VAR:LEVEL:FCST:[ENS=..]`
///    The record LENGTH is implicit = next record's offset - this offset
///    (the final record is open-ended).
///  - ECMWF `.index` : newline-delimited JSON, one object per line, with an
///    explicit `_offset` and `_length`.
///
/// The selector is applied per-source by the caller (it must use the right
/// vocabulary for the source).

#include "grib/error.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace grib {

/// One addressable GRIB record within a file.
struct GribIndexEntry {
	std::size_t record{0};	 ///< 1-based record number
	std::uint64_t offset{0}; ///< first byte (inclusive)
	std::uint64_t length{0}; ///< byte count; 0 == open-ended (last record)
	std::string param;		 ///< NOMADS: "TMP"; ECMWF: "2t"
	std::string level;		 ///< NOMADS: "2 m above ground"; ECMWF: "sfc"
	std::string step;		 ///< forecast step / fcst string
	int member{-1};			 ///< ensemble member; 0 == control, -1 == n/a
	std::string raw;		 ///< original line (advanced matching)
};

/// Filter criteria. Each field, if set, must equal (param/level/step are
/// substring-insensitive to the source vocabulary differences by exact
/// token here — the caller passes the source-correct token).
struct GribSelector {
	std::optional<std::string> param;
	std::optional<std::string> level;
	std::optional<std::string> step;
	std::optional<int> member;
};

/// Parse a NOMADS-style `.idx` text inventory.
[[nodiscard]] Result<std::vector<GribIndexEntry>> parse_nomads_idx(std::string_view text);

/// Parse an ECMWF newline-delimited JSON `.index`.
[[nodiscard]] Result<std::vector<GribIndexEntry>> parse_ecmwf_index(std::string_view text);

/// Return entries matching the selector (substring match on param/level/step,
/// exact on member). Empty selector returns everything.
[[nodiscard]] std::vector<GribIndexEntry> select(const std::vector<GribIndexEntry>& entries,
												 const GribSelector& sel);

} // namespace grib
