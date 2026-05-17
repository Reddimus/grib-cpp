// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT

#include "grib/retry.hpp"

namespace grib {

RetryingClient::RetryingClient(HttpClient& client, RetryPolicy policy)
	: client_(client), policy_(std::move(policy)) {}

Result<HttpResponse> RetryingClient::get(std::string_view path) {
	return with_retry([&]() { return client_.get(path); }, policy_);
}

const RetryPolicy& RetryingClient::policy() const noexcept {
	return policy_;
}

void RetryingClient::set_policy(RetryPolicy policy) noexcept {
	policy_ = std::move(policy);
}

} // namespace grib
