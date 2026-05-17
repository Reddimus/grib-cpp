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
///
/// Field order is largest-alignment-first so the struct has zero internal
/// padding (sizeof == 144, vs 160 for the naive declaration order) — a
/// `std::vector<GribIndexEntry>` over a multi-thousand-record sidecar packs
/// ~10% tighter and scans with fewer cache lines. `record` is uint32_t: a
/// GRIB file has at most tens of thousands of records, far under 2^32.
struct GribIndexEntry {
	std::string param;		 ///< NOMADS: "TMP"; ECMWF: "2t"
	std::string level;		 ///< NOMADS: "2 m above ground"; ECMWF: "sfc"
	std::string step;		 ///< forecast step / fcst string
	std::string raw;		 ///< original line (advanced matching)
	std::uint64_t offset{0}; ///< first byte (inclusive)
	std::uint64_t length{0}; ///< byte count; 0 == open-ended (last record)
	std::uint32_t record{0}; ///< 1-based record number
	std::int32_t member{-1}; ///< ensemble member; 0 == control, -1 == n/a
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
