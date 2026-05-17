// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace grib {

/// Error codes for grib-cpp SDK operations
enum class ErrorCode : std::uint8_t {
	Ok = 0,
	NetworkError,
	RateLimited,
	ServerError,
	NotFound,
	InvalidRequest,
	ParseError,
	Timeout,
	DecodeError,	 ///< ecCodes failed to decode a GRIB message
	FeedUnavailable, ///< 404 — model cycle/file not posted yet (not an error)
	Unknown
};

/// Convert ErrorCode to string
[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept {
	switch (code) {
		case ErrorCode::Ok:
			return "Ok";
		case ErrorCode::NetworkError:
			return "NetworkError";
		case ErrorCode::RateLimited:
			return "RateLimited";
		case ErrorCode::ServerError:
			return "ServerError";
		case ErrorCode::NotFound:
			return "NotFound";
		case ErrorCode::InvalidRequest:
			return "InvalidRequest";
		case ErrorCode::ParseError:
			return "ParseError";
		case ErrorCode::Timeout:
			return "Timeout";
		case ErrorCode::DecodeError:
			return "DecodeError";
		case ErrorCode::FeedUnavailable:
			return "FeedUnavailable";
		case ErrorCode::Unknown:
			return "Unknown";
	}
	return "Unknown";
}

/// Error information returned by SDK operations
struct Error {
	ErrorCode code;
	std::string message;
	int http_status{0};
	std::string detail; // optional upstream error detail (raw body excerpt)

	[[nodiscard]] constexpr bool is_ok() const noexcept { return code == ErrorCode::Ok; }

	[[nodiscard]] static Error ok() noexcept { return {ErrorCode::Ok, "", 0, {}}; }

	[[nodiscard]] static Error network(std::string msg) {
		return {ErrorCode::NetworkError, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error parse(std::string msg) {
		return {ErrorCode::ParseError, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error not_found(std::string msg) {
		return {ErrorCode::NotFound, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error rate_limited(std::string msg) {
		return {ErrorCode::RateLimited, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error server(std::string msg) {
		return {ErrorCode::ServerError, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error invalid_request(std::string msg) {
		return {ErrorCode::InvalidRequest, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error timeout(std::string msg) {
		return {ErrorCode::Timeout, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error decode(std::string msg) {
		return {ErrorCode::DecodeError, std::move(msg), 0, {}};
	}

	[[nodiscard]] static Error feed_unavailable(std::string msg) {
		return {ErrorCode::FeedUnavailable, std::move(msg), 404, {}};
	}

	/// Create an Error from an HTTP response status code and body
	[[nodiscard]] static Error from_response(int status, const std::string& body);
};

/// Result type for SDK operations
template <typename T>
using Result = std::expected<T, Error>;

} // namespace grib
