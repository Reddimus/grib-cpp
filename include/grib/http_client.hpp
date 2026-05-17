// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

#include "grib/error.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace grib {

/// HTTP response. `body` is raw bytes (GRIB messages are binary) stored in a
/// std::string used as a byte buffer.
struct HttpResponse {
	std::int16_t status_code; // HTTP status codes fit in int16 (100-599)
	std::string body;
	std::vector<std::pair<std::string, std::string>> headers;
};

/// HTTP client configuration. `base_url` is empty by default — grib-cpp
/// always fetches fully-qualified NOMADS / public-S3 URLs, so paths passed
/// to get()/get_range() are expected to be absolute.
struct HttpClientConfig {
	std::string base_url;
	std::string user_agent{"grib-cpp/0.1.0"};
	std::chrono::seconds timeout{120};
	bool verify_ssl{true};
};

/// GET-only HTTP client for anonymous public GRIB buckets/NOMADS.
///
/// Anonymous S3 / NOMADS need no SigV4 — a plain HTTPS GET (optionally with
/// an HTTP Range header) is sufficient, so there is no aws-sdk-cpp dependency.
///
/// @note Thread Safety: NOT thread-safe. The underlying CURL handle is shared
/// across requests. Use one HttpClient per thread (the byte-range fan-out in
/// consuming services does exactly this).
class HttpClient {
public:
	explicit HttpClient(HttpClientConfig config);
	~HttpClient();

	HttpClient(HttpClient&&) noexcept;
	HttpClient& operator=(HttpClient&&) noexcept;

	// Non-copyable
	HttpClient(const HttpClient&) = delete;
	HttpClient& operator=(const HttpClient&) = delete;

	/// Perform a full GET request.
	/// @param path Absolute URL (or base_url-relative path if base_url is set).
	[[nodiscard]] Result<HttpResponse> get(std::string_view path) const;

	/// Perform a byte-range GET (HTTP Range: bytes=offset-end).
	/// This is the core of the `.idx`/`.index` sidecar workflow — pull just
	/// the one GRIB record (~1 MB) instead of the whole multi-hundred-MB file.
	/// @param offset First byte (inclusive).
	/// @param length Number of bytes; if 0 the range is open-ended ("offset-").
	[[nodiscard]] Result<HttpResponse> get_range(std::string_view path, std::uint64_t offset,
												 std::uint64_t length) const;

	/// Get the client configuration
	[[nodiscard]] const HttpClientConfig& config() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace grib
