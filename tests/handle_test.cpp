// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
//
// Decodes the committed public-domain NOAA GRIB2 2 m-temperature sample
// (tests/fixtures/sample_2t.grib2) end-to-end through ecCodes. Skips if the
// fixture is absent so a checkout without it still goes green.

#include "grib/extract.hpp"
#include "grib/handle.hpp"

#include <atomic>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string read_fixture() {
	const std::string path = std::string(GRIB_FIXTURES_DIR) + "/sample_2t.grib2";
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		return {};
	}
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::span<const std::byte> as_bytes(const std::string& s) {
	return std::span<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
}

} // namespace

TEST(GribHandle, DecodesSample2t) {
	const std::string buf = read_fixture();
	if (buf.empty()) {
		GTEST_SKIP() << "sample_2t.grib2 fixture absent";
	}
	grib::Result<grib::GribHandle> h = grib::GribHandle::from_bytes(as_bytes(buf));
	ASSERT_TRUE(h.has_value()) << h.error().message;

	grib::Result<long> edition = h->get_long("editionNumber");
	ASSERT_TRUE(edition.has_value()) << edition.error().message;
	EXPECT_EQ(*edition, 2);

	grib::Result<std::string> sn = h->get_string("shortName");
	ASSERT_TRUE(sn.has_value()) << sn.error().message;
	EXPECT_FALSE(sn->empty());

	grib::Result<std::vector<double>> vals = h->values();
	ASSERT_TRUE(vals.has_value()) << vals.error().message;
	EXPECT_GT(vals->size(), 0u);

	// NYC; 2 m air temperature in Kelvin must be physically sane.
	grib::Result<double> k = h->nearest(40.7128, -74.0060);
	ASSERT_TRUE(k.has_value()) << k.error().message;
	EXPECT_GT(*k, 180.0);
	EXPECT_LT(*k, 340.0);
}

TEST(GribFile, IteratesMessagesThenEnds) {
	const std::string buf = read_fixture();
	if (buf.empty()) {
		GTEST_SKIP() << "sample_2t.grib2 fixture absent";
	}
	grib::Result<grib::GribFile> f = grib::GribFile::open(as_bytes(buf));
	ASSERT_TRUE(f.has_value());

	int count = 0;
	while (true) {
		grib::Result<std::optional<grib::GribHandle>> m = f->next();
		ASSERT_TRUE(m.has_value()) << m.error().message;
		if (!m->has_value()) {
			break;
		}
		++count;
	}
	EXPECT_GE(count, 1);
}

TEST(Extract, PointFromSample) {
	const std::string buf = read_fixture();
	if (buf.empty()) {
		GTEST_SKIP() << "sample_2t.grib2 fixture absent";
	}
	grib::Result<double> v = grib::extract_point(as_bytes(buf), 40.7128, -74.0060);
	ASSERT_TRUE(v.has_value()) << v.error().message;
	EXPECT_GT(*v, 180.0);
	EXPECT_LT(*v, 340.0);
}

// ecCodes loads its MEMFS definition tree lazily on the first handle creation;
// that init is not thread-safe. Before the serialized-init guard in handle.cpp,
// many threads decoding concurrently on a cold process aborted with a flex
// scanner internal error. Hammer the path; every concurrent decode must succeed.
TEST(GribHandle, ConcurrentDecodeIsRaceFree) {
	const std::string buf = read_fixture();
	if (buf.empty()) {
		GTEST_SKIP() << "sample_2t.grib2 fixture absent";
	}
	constexpr int kThreads = 16;
	std::atomic<int> ok{0};
	std::vector<std::thread> threads;
	threads.reserve(kThreads);
	for (int i = 0; i < kThreads; ++i) {
		threads.emplace_back([&] {
			const grib::Result<grib::GribHandle> h = grib::GribHandle::from_bytes(as_bytes(buf));
			if (h.has_value()) {
				ok.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}
	for (std::thread& t : threads) {
		t.join();
	}
	EXPECT_EQ(ok.load(), kThreads);
}
