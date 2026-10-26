// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/retry.hpp"

namespace grib {

RetryingClient::RetryingClient(HttpClient& client, RetryPolicy policy)
	: client_(client), policy_(std::move(policy)) {}

Result<HttpResponse> RetryingClient::get(std::string_view path) {
	return with_retry([&]() { return client_.get(path); }, policy_);
}

Result<HttpResponse> RetryingClient::get_range(std::string_view path, std::uint64_t offset,
											   std::uint64_t length) {
	return with_retry([&]() { return client_.get_range(path, offset, length); }, policy_);
}

const RetryPolicy& RetryingClient::policy() const noexcept {
	return policy_;
}

void RetryingClient::set_policy(RetryPolicy policy) noexcept {
	policy_ = std::move(policy);
}

} // namespace grib
