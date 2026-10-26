// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/sidecar.hpp"

#include <charconv>
#include <cmath>
#include <glaze/glaze.hpp>
#include <glaze/json/generic.hpp>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace grib {

namespace {

std::vector<std::string_view> split(std::string_view s, char delim) {
	std::vector<std::string_view> out;
	std::size_t start = 0;
	while (start <= s.size()) {
		std::size_t pos = s.find(delim, start);
		if (pos == std::string_view::npos) {
			out.push_back(s.substr(start));
			break;
		}
		out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	return out;
}

bool to_u64(std::string_view s, std::uint64_t& out) {
	const std::from_chars_result r = std::from_chars(s.data(), s.data() + s.size(), out);
	return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

// GEFS/SREF .idx records carry an ensemble tag in the trailing fields, e.g.
// "ENS=+12" (perturbed member 12), "ENS=low-res ctl" / "ENS=hi-res ctl"
// (control == member 0). Absent => -1 (deterministic / n/a).
int parse_nomads_member(std::string_view rest) {
	const std::size_t p = rest.find("ENS=");
	if (p == std::string_view::npos) {
		return -1;
	}
	std::string_view tok = rest.substr(p + 4);
	if (tok.starts_with("low-res ctl") || tok.starts_with("hi-res ctl") || tok.starts_with("ctl")) {
		return 0;
	}
	std::size_t i = 0;
	if (i < tok.size() && (tok[i] == '+' || tok[i] == '-')) {
		++i;
	}
	std::size_t j = i;
	while (j < tok.size() && tok[j] >= '0' && tok[j] <= '9') {
		++j;
	}
	if (j == i) {
		return -1;
	}
	int n = 0;
	const std::from_chars_result result = std::from_chars(tok.data() + i, tok.data() + j, n);
	return result.ec == std::errc{} && !tok.starts_with("-") ? n : -1;
}

std::string generic_to_string(const glz::generic_u64& value) {
	if (value.is_string()) {
		return value.get<std::string>();
	}
	if (const std::uint64_t* number = value.get_if<std::uint64_t>()) {
		return std::to_string(*number);
	}
	if (const std::int64_t* number = value.get_if<std::int64_t>()) {
		return std::to_string(*number);
	}
	if (const double* number = value.get_if<double>(); number && std::isfinite(*number)) {
		char buffer[64];
		const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), *number);
		if (result.ec == std::errc{}) {
			return std::string(buffer, result.ptr);
		}
	}
	return {};
}
bool exact_u64(const glz::generic_u64& value, std::uint64_t& out) {
	if (const std::uint64_t* number = value.get_if<std::uint64_t>()) {
		out = *number;
		return true;
	}
	if (const std::int64_t* number = value.get_if<std::int64_t>(); number && *number >= 0) {
		out = static_cast<std::uint64_t>(*number);
		return true;
	}
	if (const double* number = value.get_if<double>();
		number && std::isfinite(*number) && *number >= 0 && *number < 9007199254740992.0 &&
		std::trunc(*number) == *number) {
		out = static_cast<std::uint64_t>(*number);
		return true;
	}
	return false;
}
struct ReadOptions : glz::opts {
	bool validate_trailing_whitespace = true;
};
inline constexpr ReadOptions kReadOptions{{.null_terminated = false}};

} // namespace

Result<std::vector<GribIndexEntry>> parse_nomads_idx(std::string_view text) {
	std::vector<GribIndexEntry> entries;
	const std::vector<std::string_view> lines = split(text, '\n');

	for (std::string_view line : lines) {
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		if (line.empty()) {
			continue;
		}
		// record:offset:d=DATE:VAR:LEVEL:FCST:[trailing...]
		const std::vector<std::string_view> f = split(line, ':');
		if (f.size() < 6) {
			continue; // not an inventory line
		}
		GribIndexEntry e;
		e.raw = std::string(line);
		std::uint64_t rec = 0;
		if (!to_u64(f[0], rec) || rec == 0 || rec > std::numeric_limits<std::uint32_t>::max()) {
			return std::unexpected(
				Error::parse("nomads .idx: invalid or unsupported submessage record"));
		}
		e.record = static_cast<std::uint32_t>(rec);
		if (!to_u64(f[1], e.offset)) {
			return std::unexpected(
				Error::parse("nomads .idx: bad byte offset in: " + std::string(line)));
		}
		e.param = std::string(f[3]);
		e.level = std::string(f[4]);
		e.step = std::string(f[5]);
		// Anything after the 6th field carries the ENS= tag.
		std::string trailing;
		for (std::size_t i = 6; i < f.size(); ++i) {
			trailing += std::string(f[i]);
			trailing += ':';
		}
		e.member = parse_nomads_member(trailing);
		entries.push_back(std::move(e));
	}

	// Length = next record's offset - this offset (final = open-ended 0).
	for (std::size_t i = 0; i + 1 < entries.size(); ++i) {
		if (entries[i + 1].offset <= entries[i].offset) {
			return std::unexpected(Error::parse("nomads .idx: offsets must increase"));
		}
		entries[i].length = entries[i + 1].offset - entries[i].offset;
	}
	return entries;
}

Result<std::vector<GribIndexEntry>> parse_ecmwf_index(std::string_view text) {
	std::vector<GribIndexEntry> entries;
	const std::vector<std::string_view> lines = split(text, '\n');

	for (std::string_view line : lines) {
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		if (line.empty()) {
			continue;
		}
		glz::generic_u64 root{};
		const glz::error_ctx ec = glz::read<kReadOptions>(root, line);
		if (ec || !root.is_object()) {
			return std::unexpected(
				Error::parse("ecmwf .index: invalid JSON line: " + std::string(line)));
		}
		const glz::generic_u64::object_t& obj = root.get_object();

		GribIndexEntry e;
		e.raw = std::string(line);

		glz::generic_u64::object_t::const_iterator it_off = obj.find("_offset");
		glz::generic_u64::object_t::const_iterator it_len = obj.find("_length");
		if (it_off == obj.end() || it_len == obj.end() || !it_off->second.is_number() ||
			!it_len->second.is_number()) {
			return std::unexpected(
				Error::parse("ecmwf .index: missing _offset/_length in: " + std::string(line)));
		}
		if (!exact_u64(it_off->second, e.offset) || !exact_u64(it_len->second, e.length) ||
			(e.length > 0 && e.length - 1 > std::numeric_limits<std::uint64_t>::max() - e.offset)) {
			return std::unexpected(Error::parse("ecmwf .index: invalid byte range"));
		}

		glz::generic_u64::object_t::const_iterator it_param = obj.find("param");
		if (it_param != obj.end()) {
			e.param = generic_to_string(it_param->second);
		}
		glz::generic_u64::object_t::const_iterator it_lt = obj.find("levtype");
		glz::generic_u64::object_t::const_iterator it_ll = obj.find("levelist");
		if (it_ll != obj.end()) {
			e.level = generic_to_string(it_ll->second);
		} else if (it_lt != obj.end()) {
			e.level = generic_to_string(it_lt->second);
		}
		glz::generic_u64::object_t::const_iterator it_step = obj.find("step");
		if (it_step != obj.end()) {
			e.step = generic_to_string(it_step->second);
		}
		// type: "cf" (control) -> member 0; "pf" + number -> member N.
		glz::generic_u64::object_t::const_iterator it_type = obj.find("type");
		glz::generic_u64::object_t::const_iterator it_num = obj.find("number");
		if (it_type != obj.end() && it_type->second.is_string() &&
			it_type->second.get<std::string>() == "cf") {
			e.member = 0;
		} else if (it_num != obj.end()) {
			const std::string n = generic_to_string(it_num->second);
			int parsed = -1;
			if (!n.empty()) {
				const std::from_chars_result result =
					std::from_chars(n.data(), n.data() + n.size(), parsed);
				if (result.ec != std::errc{} || result.ptr != n.data() + n.size()) {
					parsed = -1;
				}
			}
			e.member = parsed;
		}
		entries.push_back(std::move(e));
	}
	return entries;
}

std::vector<GribIndexEntry> select(const std::vector<GribIndexEntry>& entries,
								   const GribSelector& sel) {
	std::vector<GribIndexEntry> out;
	for (const GribIndexEntry& e : entries) {
		if (sel.param && e.param.find(*sel.param) == std::string::npos) {
			continue;
		}
		if (sel.level && e.level.find(*sel.level) == std::string::npos) {
			continue;
		}
		if (sel.step && e.step.find(*sel.step) == std::string::npos) {
			continue;
		}
		if (sel.member && e.member != *sel.member) {
			continue;
		}
		out.push_back(e);
	}
	return out;
}

} // namespace grib
