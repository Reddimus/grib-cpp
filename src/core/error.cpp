// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/error.hpp"

#include <string>

namespace grib {

Error Error::from_response(int status, const std::string& body) {
	Error err;
	err.http_status = status;

	// GRIB/S3/NOMADS endpoints return no structured (JSON) error body — a
	// failed byte-range GET is either an HTTP status or an XML/text blob.
	// A 404 almost always means "the model cycle/file is not posted yet",
	// which callers treat as transient (FeedUnavailable), not a hard error.
	if (status == 404) {
		err.code = ErrorCode::FeedUnavailable;
	} else if (status == 400 || status == 416) {
		// 416 = Range Not Satisfiable (stale .idx vs truncated object).
		err.code = ErrorCode::InvalidRequest;
	} else if (status == 429) {
		err.code = ErrorCode::RateLimited;
	} else if (status >= 500) {
		err.code = ErrorCode::ServerError;
	} else {
		err.code = ErrorCode::Unknown;
	}

	if (!body.empty()) {
		err.detail = body.substr(0, 256);
	}
	err.message = "HTTP " + std::to_string(status);
	if (!err.detail.empty()) {
		err.message += ": " + err.detail;
	}

	return err;
}

} // namespace grib
