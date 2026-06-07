// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/sidecar.hpp"

#include <charconv>
#include <cmath>
#include <glaze/glaze.hpp>
#include <glaze/json/generic.hpp>
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
	return r.ec == std::errc{};
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
	std::from_chars(tok.data() + i, tok.data() + j, n);
	return n;
}

std::string generic_to_string(const glz::generic& v) {
	if (v.is_string()) {
		return v.get<std::string>();
	}
	if (v.is_number()) {
		const double d = v.get<double>();
		const long long ll = static_cast<long long>(d);
		if (static_cast<double>(ll) == d) {
			return std::to_string(ll);
		}
		return std::to_string(d);
	}
	return {};
}

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
		if (to_u64(f[0], rec)) {
			e.record = static_cast<std::uint32_t>(rec);
		}
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
		if (entries[i + 1].offset > entries[i].offset) {
			entries[i].length = entries[i + 1].offset - entries[i].offset;
		}
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
		glz::generic root{};
		const glz::error_ctx ec = glz::read_json(root, std::string(line));
		if (ec || !root.is_object()) {
			return std::unexpected(
				Error::parse("ecmwf .index: invalid JSON line: " + std::string(line)));
		}
		const glz::generic::object_t& obj = root.get_object();

		GribIndexEntry e;
		e.raw = std::string(line);

		glz::generic::object_t::const_iterator it_off = obj.find("_offset");
		glz::generic::object_t::const_iterator it_len = obj.find("_length");
		if (it_off == obj.end() || it_len == obj.end() || !it_off->second.is_number() ||
			!it_len->second.is_number()) {
			return std::unexpected(
				Error::parse("ecmwf .index: missing _offset/_length in: " + std::string(line)));
		}
		// Validate before the double->uint64 narrowing cast: negative, NaN/inf,
		// or >= 2^64 values are UB to cast and would drive a garbage HTTP Range.
		// A malformed .index must error, not silently produce a bad offset/length.
		const double d_off = it_off->second.get<double>();
		const double d_len = it_len->second.get<double>();
		if (!std::isfinite(d_off) || d_off < 0.0 || d_off >= 18446744073709551616.0 ||
			!std::isfinite(d_len) || d_len < 0.0 || d_len >= 18446744073709551616.0) {
			return std::unexpected(Error::parse("ecmwf .index: out-of-range _offset/_length in: " +
												std::string(line)));
		}
		e.offset = static_cast<std::uint64_t>(d_off);
		e.length = static_cast<std::uint64_t>(d_len);

		glz::generic::object_t::const_iterator it_param = obj.find("param");
		if (it_param != obj.end()) {
			e.param = generic_to_string(it_param->second);
		}
		glz::generic::object_t::const_iterator it_lt = obj.find("levtype");
		glz::generic::object_t::const_iterator it_ll = obj.find("levelist");
		if (it_ll != obj.end()) {
			e.level = generic_to_string(it_ll->second);
		} else if (it_lt != obj.end()) {
			e.level = generic_to_string(it_lt->second);
		}
		glz::generic::object_t::const_iterator it_step = obj.find("step");
		if (it_step != obj.end()) {
			e.step = generic_to_string(it_step->second);
		}
		// type: "cf" (control) -> member 0; "pf" + number -> member N.
		glz::generic::object_t::const_iterator it_type = obj.find("type");
		glz::generic::object_t::const_iterator it_num = obj.find("number");
		if (it_type != obj.end() && it_type->second.is_string() &&
			it_type->second.get<std::string>() == "cf") {
			e.member = 0;
		} else if (it_num != obj.end()) {
			const std::string n = generic_to_string(it_num->second);
			int parsed = -1;
			if (!n.empty()) {
				std::from_chars(n.data(), n.data() + n.size(), parsed);
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
