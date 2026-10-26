#include "grib/http_client.hpp"

#include <algorithm>
#include <charconv>
#include <curl/curl.h>
#include <limits>
#include <memory>

namespace grib {
namespace {

bool is_absolute_url(std::string_view value) {
	const std::size_t end = value.find("://");
	if (end == std::string_view::npos || end == 0) {
		return false;
	}
	const char first = value.front();
	if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))) {
		return false;
	}
	return value.substr(0, end).find_first_not_of(
			   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+.-") ==
		   std::string_view::npos;
}

class CurlRuntime {
public:
	CurlRuntime() : status_(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
	~CurlRuntime() {
		if (status_ == CURLE_OK) {
			curl_global_cleanup();
		}
	}
	CurlRuntime(const CurlRuntime&) = delete;
	CurlRuntime& operator=(const CurlRuntime&) = delete;
	[[nodiscard]] CURLcode status() const noexcept { return status_; }

private:
	CURLcode status_;
};

std::shared_ptr<const CurlRuntime> runtime() {
	static const std::shared_ptr<const CurlRuntime> value = std::make_shared<const CurlRuntime>();
	return value;
}

struct ResponseSink {
	HttpResponse response{};
	std::size_t limit;
	bool overflowed{false};
};

// Libcurl's C stack must never unwind through a C++ allocation failure.
std::size_t write_callback(char* data, std::size_t size, std::size_t count,
						   void* userdata) noexcept {
	ResponseSink& sink = *static_cast<ResponseSink*>(userdata);
	if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
		return 0;
	}
	const std::size_t bytes = size * count;
	if (sink.limit > 0 && bytes > sink.limit - sink.response.body.size()) {
		sink.overflowed = true;
		return 0;
	}
	try {
		sink.response.body.append(data, bytes);
	} catch (...) {
		return 0;
	}
	return bytes;
}

std::size_t header_callback(char* data, std::size_t size, std::size_t count,
							void* userdata) noexcept {
	ResponseSink& sink = *static_cast<ResponseSink*>(userdata);
	if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
		return 0;
	}
	const std::size_t bytes = size * count;
	try {
		const std::string_view line(data, bytes);
		if (line.starts_with("HTTP/")) {
			sink.response.headers.clear();
		}
		const std::size_t colon = line.find(':');
		if (colon != std::string_view::npos) {
			const std::string_view raw = line.substr(colon + 1);
			const std::size_t start = raw.find_first_not_of(" \t\r\n");
			const std::size_t end = raw.find_last_not_of(" \t\r\n");
			const std::string_view value = start == std::string_view::npos
											   ? std::string_view{}
											   : raw.substr(start, end - start + 1);
			sink.response.headers.emplace_back(line.substr(0, colon), value);
		}
	} catch (...) {
		return 0;
	}
	return bytes;
}

bool invalid_header(std::string_view value) {
	return value.find_first_of(std::string_view{"\r\n\0", 3}) != std::string_view::npos;
}
} // namespace

struct HttpClient::Impl {
	// Kept alive until after this instance's easy handle has been closed.
	std::shared_ptr<const CurlRuntime> global{runtime()};
	HttpClientConfig config;
	CURL* curl{global->status() == CURLE_OK ? curl_easy_init() : nullptr};
	explicit Impl(HttpClientConfig value) : config(std::move(value)) {}
	~Impl() {
		if (curl != nullptr) {
			curl_easy_cleanup(curl);
		}
	}
	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;
};

HttpClient::HttpClient(HttpClientConfig config)
	: impl_(std::make_unique<Impl>(std::move(config))) {}
HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

Result<HttpResponse> HttpClient::perform(std::string_view path, const char* range) const {
	if (!impl_ || impl_->curl == nullptr) {
		return std::unexpected(Error::network("CURL not initialized"));
	}
	const HttpClientConfig& config = impl_->config;
	const std::string url =
		is_absolute_url(path) ? std::string(path) : config.base_url + std::string(path);
	if (url.find('\0') != std::string::npos || invalid_header(config.user_agent) ||
		config.timeout < std::chrono::seconds::zero()) {
		return std::unexpected(Error::invalid_request("Invalid URL, header, or timeout"));
	}
	using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
	const std::string accept = "Accept: application/octet-stream";
	const Headers headers(curl_slist_append(nullptr, accept.c_str()), &curl_slist_free_all);
	if (!headers) {
		return std::unexpected(Error::network("Could not allocate HTTP headers"));
	}
	ResponseSink sink{{}, config.max_response_bytes};
	CURL* curl = impl_->curl;
	curl_easy_reset(curl);
	const long timeout = static_cast<long>(
		std::min(config.timeout.count(),
				 static_cast<std::chrono::seconds::rep>(std::numeric_limits<long>::max())));
	const long verify = config.verify_ssl ? 1L : 0L;
	const bool configured =
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https") == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https") == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &header_callback) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sink) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get()) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_USERAGENT, config.user_agent.c_str()) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_RANGE, range) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, range ? "identity" : "") == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify) == CURLE_OK &&
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify * 2L) == CURLE_OK;
	if (!configured) {
		return std::unexpected(Error::network("libcurl rejected a required option"));
	}
	const CURLcode result = curl_easy_perform(curl);
	if (sink.overflowed) {
		return std::unexpected(
			Error::invalid_request("Response exceeded HttpClientConfig::max_response_bytes"));
	}
	if (result == CURLE_OPERATION_TIMEDOUT) {
		return std::unexpected(Error::timeout(curl_easy_strerror(result)));
	}
	if (result != CURLE_OK) {
		return std::unexpected(Error::network(curl_easy_strerror(result)));
	}
	long status = 0;
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK) {
		return std::unexpected(Error::network("libcurl did not return HTTP status"));
	}
	sink.response.status_code = static_cast<std::int16_t>(status);
	return std::move(sink.response);
}

Result<HttpResponse> HttpClient::get(std::string_view path) const {
	return perform(path, nullptr);
}
Result<HttpResponse> HttpClient::get_range(std::string_view path, std::uint64_t offset,
										   std::uint64_t length) const {
	if (length > 0 && length - 1 > std::numeric_limits<std::uint64_t>::max() - offset) {
		return std::unexpected(Error::invalid_request("Byte range overflow"));
	}
	std::string range = std::to_string(offset) + "-";
	if (length > 0) {
		range += std::to_string(offset + length - 1);
	}
	Result<HttpResponse> response = perform(path, range.c_str());
	if (response && response->status_code >= 200 && response->status_code < 300 &&
		response->status_code != 206) {
		return std::unexpected(Error::network("Server ignored the requested byte range"));
	}
	if (response && response->status_code == 206) {
		std::string_view content_range;
		for (const auto& [name, value] : response->headers) {
			std::string lower = name;
			for (char& c : lower) {
				if (c >= 'A' && c <= 'Z') {
					c = static_cast<char>(c + ('a' - 'A'));
				}
			}
			if (lower == "content-range") {
				content_range = value;
			}
		}
		const auto invalid = []() -> Result<HttpResponse> {
			return std::unexpected(Error::network("Invalid Content-Range response"));
		};
		if (!content_range.starts_with("bytes ") || response->body.empty()) {
			return invalid();
		}
		content_range.remove_prefix(6);
		std::uint64_t first = 0, last = 0;
		const char* end = content_range.data() + content_range.size();
		const std::from_chars_result begin_result =
			std::from_chars(content_range.data(), end, first);
		if (begin_result.ec != std::errc{} || begin_result.ptr == end || *begin_result.ptr != '-') {
			return invalid();
		}
		const std::from_chars_result end_result = std::from_chars(begin_result.ptr + 1, end, last);
		if (end_result.ec != std::errc{} || end_result.ptr == end || *end_result.ptr != '/' ||
			first != offset || last < first || last - first != response->body.size() - 1 ||
			(length > 0 && response->body.size() > length)) {
			return invalid();
		}
		const std::string_view total(end_result.ptr + 1,
									 static_cast<std::size_t>(end - end_result.ptr - 1));
		if (total == "*" && length > 0 && response->body.size() != length) {
			return invalid();
		}
		if (total != "*") {
			std::uint64_t size = 0;
			const std::from_chars_result total_result = std::from_chars(total.data(), end, size);
			if (total_result.ec != std::errc{} || total_result.ptr != end || size <= last) {
				return invalid();
			}
			if (length > 0 && last != std::min(offset + length - 1, size - 1)) {
				return invalid();
			}
		}
	}
	return response;
}

const HttpClientConfig& HttpClient::config() const noexcept {
	if (impl_) {
		return impl_->config;
	}
	static const HttpClientConfig moved_from{.base_url = {}, .user_agent = {}};
	return moved_from;
}
} // namespace grib
