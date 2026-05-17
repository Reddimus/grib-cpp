// Copyright (c) 2026 PredictionMarketsAI
// SPDX-License-Identifier: MIT
#pragma once

/// @file grib.hpp
/// @brief Umbrella header for grib-cpp.
///
/// Include this single header to get the whole SDK: error/Result, the
/// byte-range HTTP client, rate-limit/retry, the `.idx`/`.index` sidecar
/// parsers, the ecCodes RAII decode wrappers, and the station extract API.

#include "grib/error.hpp"
#include "grib/extract.hpp"
#include "grib/handle.hpp"
#include "grib/http_client.hpp"
#include "grib/rate_limit.hpp"
#include "grib/retry.hpp"
#include "grib/sidecar.hpp"
