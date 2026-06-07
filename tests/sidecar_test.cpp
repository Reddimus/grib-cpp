// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/sidecar.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using grib::GribIndexEntry;
using grib::GribSelector;

TEST(NomadsIdx, ParsesOffsetsLengthsAndFields) {
	const std::string idx = "1:0:d=2026051700:PRMSL:mean sea level:anl:\n"
							"2:1048576:d=2026051700:TMP:2 m above ground:anl:\n"
							"3:2097152:d=2026051700:UGRD:10 m above ground:anl:\n";

	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_nomads_idx(idx);
	ASSERT_TRUE(r.has_value()) << r.error().message;
	const std::vector<GribIndexEntry>& e = *r;
	ASSERT_EQ(e.size(), 3u);

	EXPECT_EQ(e[0].record, 1u);
	EXPECT_EQ(e[0].offset, 0u);
	EXPECT_EQ(e[0].length, 1048576u); // next.offset - this.offset
	EXPECT_EQ(e[0].param, "PRMSL");

	EXPECT_EQ(e[1].param, "TMP");
	EXPECT_EQ(e[1].level, "2 m above ground");
	EXPECT_EQ(e[1].offset, 1048576u);
	EXPECT_EQ(e[1].length, 1048576u);

	EXPECT_EQ(e[2].length, 0u); // final record is open-ended
	EXPECT_EQ(e[2].member, -1);
}

TEST(NomadsIdx, ParsesEnsembleMember) {
	const std::string idx = "1:0:d=2026051700:TMP:2 m above ground:6 hour fcst:ENS=low-res ctl:\n"
							"2:500:d=2026051700:TMP:2 m above ground:6 hour fcst:ENS=+5:\n";
	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_nomads_idx(idx);
	ASSERT_TRUE(r.has_value()) << r.error().message;
	EXPECT_EQ((*r)[0].member, 0); // control
	EXPECT_EQ((*r)[1].member, 5); // perturbed +5
}

TEST(NomadsIdx, RejectsBadOffset) {
	const std::string idx = "1:NOTANUMBER:d=2026:TMP:2 m above ground:anl:\n";
	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_nomads_idx(idx);
	EXPECT_FALSE(r.has_value());
}

TEST(EcmwfIndex, ParsesJsonLines) {
	const std::string index =
		R"({"type":"fc","stream":"oper","step":"0","levtype":"sfc","param":"2t","_offset":1234,"_length":5678})"
		"\n"
		R"({"type":"cf","number":"0","levtype":"sfc","param":"2t","step":"24","_offset":9000,"_length":1000})"
		"\n"
		R"({"type":"pf","number":"12","levtype":"sfc","param":"2t","step":"24","_offset":10000,"_length":1000})"
		"\n";

	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_ecmwf_index(index);
	ASSERT_TRUE(r.has_value()) << r.error().message;
	const std::vector<GribIndexEntry>& e = *r;
	ASSERT_EQ(e.size(), 3u);

	EXPECT_EQ(e[0].offset, 1234u);
	EXPECT_EQ(e[0].length, 5678u);
	EXPECT_EQ(e[0].param, "2t");
	EXPECT_EQ(e[0].level, "sfc");
	EXPECT_EQ(e[0].step, "0");
	EXPECT_EQ(e[0].member, -1); // deterministic fc

	EXPECT_EQ(e[1].member, 0);	// cf == control
	EXPECT_EQ(e[2].member, 12); // pf number 12
}

TEST(EcmwfIndex, RejectsBadJson) {
	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_ecmwf_index("{not json}\n");
	EXPECT_FALSE(r.has_value());
}

TEST(EcmwfIndex, RejectsOutOfRangeOffset) {
	// Regression: _offset/_length were cast double->uint64 with no validation;
	// a negative or absurdly large value is UB to cast and would drive a garbage
	// HTTP Range. Such lines must error, not silently parse.
	grib::Result<std::vector<GribIndexEntry>> neg =
		grib::parse_ecmwf_index(R"({"param":"2t","_offset":-5,"_length":1000})"
								"\n");
	EXPECT_FALSE(neg.has_value());
	grib::Result<std::vector<GribIndexEntry>> huge =
		grib::parse_ecmwf_index(R"({"param":"2t","_offset":1e30,"_length":1000})"
								"\n");
	EXPECT_FALSE(huge.has_value());
}

TEST(Select, FiltersByParamLevelMember) {
	const std::string idx = "1:0:d=2026051700:PRMSL:mean sea level:anl:\n"
							"2:100:d=2026051700:TMP:2 m above ground:anl:ENS=low-res ctl:\n"
							"3:200:d=2026051700:TMP:2 m above ground:anl:ENS=+3:\n";
	grib::Result<std::vector<GribIndexEntry>> r = grib::parse_nomads_idx(idx);
	ASSERT_TRUE(r.has_value());

	GribSelector by_param;
	by_param.param = "TMP";
	EXPECT_EQ(grib::select(*r, by_param).size(), 2u);

	GribSelector by_member;
	by_member.member = 3;
	std::vector<GribIndexEntry> m = grib::select(*r, by_member);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].offset, 200u);

	GribSelector empty;
	EXPECT_EQ(grib::select(*r, empty).size(), 3u);
}
