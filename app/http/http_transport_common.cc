/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstring>
#include <unordered_map>

#include "app/http/http_transport.h"

namespace firebase {
namespace http {
namespace {

struct CompletionQueue {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<int64_t> events;
  bool wake_requested = false;
};

CompletionQueue& GetCompletionQueue() {
  static auto* queue = new CompletionQueue();
  return *queue;
}

struct OperationRegistry {
  std::mutex mutex;
  int64_t next_id = 1;
  std::unordered_map<int64_t, std::shared_ptr<HttpOperation>> operations;
};

OperationRegistry& GetRegistry() {
  static auto* registry = new OperationRegistry();
  return *registry;
}

bool HasControlOrWhitespace(std::string_view s) {
  for (unsigned char ch : s) {
    if (ch <= 0x20 || ch == 0x7F) {
      return true;
    }
  }
  return false;
}

bool HasControlChars(std::string_view s) {
  for (unsigned char ch : s) {
    if (ch < 0x20 || ch == 0x7F) {
      return true;
    }
  }
  return false;
}

// Trims only HTTP OWS (SP and HTAB) so control characters like CR/LF/VT/FF
// remain visible to header validation instead of being silently stripped.
std::string_view TrimHttpOws(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

bool IsHttpTchar(unsigned char ch) {
  if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
      (ch >= '0' && ch <= '9')) {
    return true;
  }
  switch (ch) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
      return true;
    default:
      return false;
  }
}

void CancelOperationObject(const std::shared_ptr<HttpOperation>& op) {
  if (!op) {
    return;
  }
  bool expected = false;
  if (!op->canceled.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
    return;
  }

  int64_t handle = op->id;
  std::function<void()> cancel_hook;
  bool notify_req = false;
  bool notify_headers = false;
  bool notify_body = false;
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (!op->failed && op->error_class == kHttpErrorNone) {
      op->error_class = kHttpErrorCanceled;
    }
    cancel_hook = std::move(op->on_cancel);
    op->on_cancel = nullptr;
    op->on_request_data_changed = nullptr;
    op->on_response_space_available = nullptr;
    if (op->req_space_armed) {
      op->req_space_armed = false;
      notify_req = true;
    }
    if (op->resp_headers_armed) {
      op->resp_headers_armed = false;
      notify_headers = true;
    }
    if (op->resp_read_armed) {
      op->resp_read_armed = false;
      notify_body = true;
    }
  }
  op->req_cv.notify_all();
  op->resp_cv.notify_all();
  if (cancel_hook) {
    cancel_hook();
  }
  if (notify_req) {
    PostCompletionEvent(handle, kHttpEventRequestSpaceAvailable);
  }
  if (notify_headers) {
    PostCompletionEvent(handle, kHttpEventResponseHeadersReady);
  }
  if (notify_body) {
    PostCompletionEvent(handle, kHttpEventResponseBodyAvailable);
  }
}

}  // namespace

bool IsValidHeaderName(std::string_view name) {
  if (name.empty()) {
    return false;
  }
  for (unsigned char ch : name) {
    if (!IsHttpTchar(ch)) {
      return false;
    }
  }
  return true;
}

bool IsValidHeaderValue(std::string_view value) {
  for (unsigned char ch : value) {
    if (ch == '\0' || ch == '\r' || ch == '\n' ||
        (ch < 0x20 && ch != '\t') || ch == 0x7F) {
      return false;
    }
  }
  return true;
}

bool TryParsePackedHeaders(
    std::string_view packed,
    std::vector<std::pair<std::string, std::string>>* out) {
  if (out == nullptr) {
    return false;
  }
  out->clear();
  if (packed.find('\0') != std::string_view::npos) {
    return false;
  }

  size_t pos = 0;
  while (pos < packed.size()) {
    size_t nl = packed.find('\n', pos);
    std::string_view line = nl == std::string_view::npos
                                ? packed.substr(pos)
                                : packed.substr(pos, nl - pos);
    pos = nl == std::string_view::npos ? packed.size() : nl + 1;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      continue;
    }
    // Reject any interior CR (header injection).
    if (line.find('\r') != std::string_view::npos) {
      out->clear();
      return false;
    }
    size_t sep = line.find('\t');
    if (sep == std::string_view::npos) {
      sep = line.find(':');
    }
    if (sep == std::string_view::npos || sep == 0) {
      out->clear();
      return false;
    }
    std::string_view name = line.substr(0, sep);
    std::string_view value = TrimHttpOws(line.substr(sep + 1));
    if (!IsValidHeaderName(name) || !IsValidHeaderValue(value)) {
      out->clear();
      return false;
    }
    out->emplace_back(std::string(name), std::string(value));
  }
  return true;
}

std::vector<std::pair<std::string, std::string>> ParsePackedHeaders(
    std::string_view packed) {
  std::vector<std::pair<std::string, std::string>> result;
  if (!TryParsePackedHeaders(packed, &result)) {
    result.clear();
  }
  return result;
}

bool AsciiEqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

std::string_view TrimAsciiWhitespace(std::string_view s) {
  while (!s.empty() &&
         std::isspace(static_cast<unsigned char>(s.front())) != 0) {
    s.remove_prefix(1);
  }
  while (!s.empty() &&
         std::isspace(static_cast<unsigned char>(s.back())) != 0) {
    s.remove_suffix(1);
  }
  return s;
}

std::optional<UrlOrigin> ParseUrlOrigin(std::string_view url) {
  url = TrimAsciiWhitespace(url);
  size_t scheme_end = url.find("://");
  if (scheme_end == std::string_view::npos || scheme_end == 0) {
    return std::nullopt;
  }
  std::string_view raw_scheme = url.substr(0, scheme_end);
  int default_port = 0;
  std::string norm_scheme;
  if (AsciiEqualsIgnoreCase(raw_scheme, "http")) {
    norm_scheme = "http";
    default_port = 80;
  } else if (AsciiEqualsIgnoreCase(raw_scheme, "https")) {
    norm_scheme = "https";
    default_port = 443;
  } else {
    return std::nullopt;
  }

  std::string_view rest = url.substr(scheme_end + 3);
  size_t path_pos = rest.find_first_of("/?#");
  std::string_view authority =
      path_pos == std::string_view::npos ? rest : rest.substr(0, path_pos);
  // Reject userinfo (`@`, where parsers disagree on first vs. last `@`),
  // backslashes (which WHATWG/OkHttp treat as path delimiters, enabling
  // `https://evil.com\@trusted.com` authority confusion), and control or
  // whitespace characters inside authority.
  if (authority.find('@') != std::string_view::npos ||
      authority.find('\\') != std::string_view::npos ||
      HasControlOrWhitespace(authority)) {
    return std::nullopt;
  }
  if (authority.empty()) {
    return std::nullopt;
  }

  std::string_view host_sv;
  std::string_view port_sv;
  if (authority.front() == '[') {
    size_t close = authority.find(']');
    if (close == std::string_view::npos || close <= 1) {
      return std::nullopt;
    }
    host_sv = authority.substr(0, close + 1);
    if (close + 1 < authority.size()) {
      if (authority[close + 1] != ':') {
        return std::nullopt;
      }
      port_sv = authority.substr(close + 2);
    }
  } else {
    size_t colon = authority.rfind(':');
    if (colon != std::string_view::npos) {
      host_sv = authority.substr(0, colon);
      port_sv = authority.substr(colon + 1);
    } else {
      host_sv = authority;
    }
    if (host_sv.find_first_of(":[]") != std::string_view::npos) {
      return std::nullopt;
    }
  }
  if (host_sv.empty()) {
    return std::nullopt;
  }

  int port = default_port;
  if (!port_sv.empty()) {
    int parsed = 0;
    auto [ptr, ec] = std::from_chars(port_sv.data(),
                                     port_sv.data() + port_sv.size(), parsed);
    if (ec != std::errc() || ptr != port_sv.data() + port_sv.size() ||
        parsed <= 0 || parsed > 65535) {
      return std::nullopt;
    }
    port = parsed;
  }

  std::string norm_host(host_sv.size(), '\0');
  for (size_t i = 0; i < host_sv.size(); ++i) {
    norm_host[i] =
        static_cast<char>(std::tolower(static_cast<unsigned char>(host_sv[i])));
  }

  UrlOrigin out;
  out.scheme = std::move(norm_scheme);
  out.host = std::move(norm_host);
  out.port = port;
  return out;
}

bool IsHttpOrHttpsUrl(std::string_view url) {
  return ParseUrlOrigin(url).has_value();
}

bool IsHttpsUrl(std::string_view url) {
  auto origin = ParseUrlOrigin(url);
  return origin.has_value() && origin->scheme == "https";
}

bool IsSameOrigin(std::string_view url_a, std::string_view url_b) {
  auto a = ParseUrlOrigin(url_a);
  auto b = ParseUrlOrigin(url_b);
  if (!a || !b) {
    return false;
  }
  return a->scheme == b->scheme && a->host == b->host && a->port == b->port;
}

bool IsSensitiveRedirectHeader(std::string_view header_name) {
  return AsciiEqualsIgnoreCase(header_name, "Authorization") ||
         AsciiEqualsIgnoreCase(header_name, "Proxy-Authorization") ||
         AsciiEqualsIgnoreCase(header_name, "Cookie") ||
         AsciiEqualsIgnoreCase(header_name, "X-Firebase-AppCheck") ||
         AsciiEqualsIgnoreCase(header_name, "x-goog-api-key");
}

std::optional<std::string> ResolveRedirectUrl(std::string_view current_url,
                                              std::string_view location) {
  current_url = TrimAsciiWhitespace(current_url);
  location = TrimAsciiWhitespace(location);
  if (location.empty() || HasControlChars(location)) {
    return std::nullopt;
  }
  // Reject leading backslashes or `/\` prefixes to prevent WHATWG/OkHttp
  // scheme-relative authority confusion (`\/evil.com`, `/\evil.com`).
  if (location.front() == '\\' ||
      (location.size() >= 2 && location[0] == '/' && location[1] == '\\')) {
    return std::nullopt;
  }
  auto base_origin = ParseUrlOrigin(current_url);
  if (!base_origin) {
    return std::nullopt;
  }

  // Detect an RFC 3986 leading URI scheme (`:` occurring before `/`, `\`, `?`,
  // or `#`) so relative URLs containing `://` in query/fragment stay relative
  // while non-HTTP(S) schemes like `data:`, `file:/...`, or `mailto:` are
  // passed to `ParseUrlOrigin` and rejected.
  size_t first_delim = location.find_first_of(":/\\?#");
  if (first_delim == 0 && location[0] == ':') {
    return std::nullopt;
  }
  bool has_scheme_prefix = first_delim != std::string_view::npos &&
                           first_delim > 0 && location[first_delim] == ':';

  std::string candidate;
  if (has_scheme_prefix) {
    candidate.assign(location.data(), location.size());
  } else if (location.size() >= 2 && location[0] == '/' && location[1] == '/') {
    candidate = base_origin->scheme + ":" + std::string(location);
  } else {
    size_t scheme_end = current_url.find("://");
    size_t auth_start = scheme_end + 3;
    size_t path_pos = current_url.find_first_of("/?#", auth_start);
    std::string_view origin_prefix = path_pos == std::string_view::npos
                                         ? current_url
                                         : current_url.substr(0, path_pos);
    std::string_view base_path = "/";
    if (path_pos != std::string_view::npos && current_url[path_pos] == '/') {
      size_t q_pos = current_url.find_first_of("?#", path_pos);
      base_path = q_pos == std::string_view::npos
                      ? current_url.substr(path_pos)
                      : current_url.substr(path_pos, q_pos - path_pos);
    }

    if (location.front() == '/') {
      candidate = std::string(origin_prefix) + std::string(location);
    } else if (location.front() == '?') {
      candidate = std::string(origin_prefix) + std::string(base_path) +
                  std::string(location);
    } else if (location.front() == '#') {
      size_t frag_pos = current_url.find('#');
      std::string_view without_frag = frag_pos == std::string_view::npos
                                          ? current_url
                                          : current_url.substr(0, frag_pos);
      candidate = std::string(without_frag) + std::string(location);
    } else {
      size_t last_slash = base_path.rfind('/');
      std::string_view dir = last_slash == std::string_view::npos
                                 ? std::string_view("/")
                                 : base_path.substr(0, last_slash + 1);
      candidate =
          std::string(origin_prefix) + std::string(dir) + std::string(location);
    }
  }

  auto next_origin = ParseUrlOrigin(candidate);
  if (!next_origin) {
    return std::nullopt;
  }
  if (base_origin->scheme == "https" && next_origin->scheme != "https") {
    return std::nullopt;
  }

  // Percent-encode ASCII spaces in the path/query/fragment so libcurl
  // (`CURLOPT_URL`), NSURL, and java.net.URL all handle redirect targets
  // containing spaces identically (spaces in the authority were already
  // rejected by `ParseUrlOrigin` above).
  if (candidate.find(' ') != std::string::npos) {
    std::string encoded;
    encoded.reserve(candidate.size() + 8);
    for (char ch : candidate) {
      if (ch == ' ') {
        encoded.append("%20");
      } else {
        encoded.push_back(ch);
      }
    }
    candidate = std::move(encoded);
  }
  return candidate;
}

std::optional<std::pair<std::string, int>> ParseProxyHostPort(
    std::string_view proxy_url) {
  proxy_url = TrimAsciiWhitespace(proxy_url);
  if (proxy_url.empty()) {
    return std::nullopt;
  }
  std::string normalized;
  if (proxy_url.find("://") == std::string_view::npos) {
    normalized = "http://" + std::string(proxy_url);
    proxy_url = normalized;
  }
  auto origin = ParseUrlOrigin(proxy_url);
  if (!origin) {
    return std::nullopt;
  }
  std::string host = origin->host;
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }
  if (host.empty()) {
    return std::nullopt;
  }
  return std::make_pair(std::move(host), origin->port);
}

void PostCompletionEvent(int64_t handle, HttpEventKind kind) {
  if (handle <= 0 || kind == kHttpEventNone) {
    return;
  }
  int64_t packed = (handle << 8) | (static_cast<int64_t>(kind) & 0xFF);
  auto& q = GetCompletionQueue();
  {
    std::lock_guard<std::mutex> lock(q.mutex);
    q.events.push_back(packed);
  }
  q.cv.notify_one();
}

int64_t CreateOperation(HttpRequestOptions options) {
  auto op = std::make_shared<HttpOperation>();
  op->method = options.method.empty() ? "GET" : std::move(options.method);
  op->url = std::move(options.url);
  op->request_headers = std::move(options.headers);
  op->proxy_url = std::move(options.proxy_url);
  op->content_length = options.content_length;
  if (op->content_length == kNoRequestContent || op->content_length == 0) {
    op->req_eof = true;
  }

  std::string validation_error;
  if (op->content_length < kNoRequestContent) {
    validation_error = "Invalid request content_length.";
  } else {
    for (const auto& kv : op->request_headers) {
      if (!IsValidHeaderName(kv.first) || !IsValidHeaderValue(kv.second)) {
        validation_error = "Invalid HTTP request header.";
        break;
      }
    }
  }

  auto& reg = GetRegistry();
  int64_t id = 0;
  {
    std::lock_guard<std::mutex> lock(reg.mutex);
    id = reg.next_id++;
    op->id = id;
    reg.operations.emplace(id, op);
  }
  if (!validation_error.empty()) {
    op->MarkFailed(validation_error, kHttpErrorOther);
  }
  return id;
}

std::shared_ptr<HttpOperation> FindOperation(int64_t handle) {
  auto& reg = GetRegistry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  auto it = reg.operations.find(handle);
  return it != reg.operations.end() ? it->second : nullptr;
}

void StartOperation(int64_t handle) {
  auto op = FindOperation(handle);
  if (!op) {
    return;
  }
  if (op->started.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (op->failed || op->IsCanceled()) {
      return;
    }
  }
  StartPlatformHttpOperation(op);
}

void CancelOperation(int64_t handle) {
  CancelOperationObject(FindOperation(handle));
}

void DestroyOperation(int64_t handle) {
  CancelOperation(handle);
  auto& reg = GetRegistry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  reg.operations.erase(handle);
}

void CancelAllOperations() {
  std::vector<std::shared_ptr<HttpOperation>> snapshot;
  {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    snapshot.reserve(reg.operations.size());
    for (auto& kv : reg.operations) {
      snapshot.push_back(std::move(kv.second));
    }
    reg.operations.clear();
  }
  for (const auto& op : snapshot) {
    CancelOperationObject(op);
  }
  HttpWakeEventPumpInternal();
}

void HttpOperation::CommitResponseHeaders(
    int32_t status, std::string_view reason,
    std::vector<std::pair<std::string, std::string>> headers) {
  bool notify = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (headers_ready || failed || IsCanceled()) {
      return;
    }
    status_code = status;
    reason_phrase.assign(reason.data(), reason.size());
    response_headers = std::move(headers);
    headers_ready = true;
    if (resp_headers_armed) {
      resp_headers_armed = false;
      notify = true;
    }
  }
  if (notify) {
    PostCompletionEvent(id, kHttpEventResponseHeadersReady);
  }
}

void HttpOperation::MarkRequestWriteClosed() {
  bool notify_req = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    req_write_closed = true;
    req_chunks.clear();
    req_buffered_bytes = 0;
    on_request_data_changed = nullptr;
    if (req_space_armed) {
      req_space_armed = false;
      notify_req = true;
    }
  }
  req_cv.notify_all();
  if (notify_req) {
    PostCompletionEvent(id, kHttpEventRequestSpaceAvailable);
  }
}

bool HttpOperation::PushResponseBytes(const uint8_t* bytes, size_t length) {
  if (length == 0) {
    std::lock_guard<std::mutex> lock(mutex);
    return resp_buffered_bytes >= kMaxResponseBufferBytes;
  }
  ByteChunk chunk;
  chunk.data.assign(bytes, bytes + length);

  bool notify = false;
  bool over_high_water = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (failed || IsCanceled()) {
      return false;
    }
    resp_buffered_bytes += length;
    resp_chunks.push_back(std::move(chunk));
    over_high_water = resp_buffered_bytes >= kMaxResponseBufferBytes;
    if (resp_read_armed) {
      resp_read_armed = false;
      notify = true;
    }
  }
  if (notify) {
    PostCompletionEvent(id, kHttpEventResponseBodyAvailable);
  }
  return over_high_water;
}

void HttpOperation::MarkResponseComplete() {
  bool notify_req = false;
  bool notify_headers = false;
  bool notify_body = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    resp_eof = true;
    req_write_closed = true;
    req_chunks.clear();
    req_buffered_bytes = 0;
    on_request_data_changed = nullptr;
    on_response_space_available = nullptr;
    on_cancel = nullptr;
    if (req_space_armed) {
      req_space_armed = false;
      notify_req = true;
    }
    if (resp_headers_armed) {
      resp_headers_armed = false;
      notify_headers = true;
    }
    if (resp_read_armed) {
      resp_read_armed = false;
      notify_body = true;
    }
  }
  req_cv.notify_all();
  if (notify_req) {
    PostCompletionEvent(id, kHttpEventRequestSpaceAvailable);
  }
  if (notify_headers) {
    PostCompletionEvent(id, kHttpEventResponseHeadersReady);
  }
  if (notify_body) {
    PostCompletionEvent(id, kHttpEventResponseBodyAvailable);
  }
}

void HttpOperation::MarkFailed(std::string_view message,
                               HttpErrorClass err_class) {
  bool notify_req = false;
  bool notify_headers = false;
  bool notify_body = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!failed) {
      failed = true;
      error_class = err_class == kHttpErrorNone ? kHttpErrorOther : err_class;
      error_message.assign(message.data(), message.size());
    }
    req_write_closed = true;
    on_request_data_changed = nullptr;
    on_response_space_available = nullptr;
    on_cancel = nullptr;
    if (req_space_armed) {
      req_space_armed = false;
      notify_req = true;
    }
    if (resp_headers_armed) {
      resp_headers_armed = false;
      notify_headers = true;
    }
    if (resp_read_armed) {
      resp_read_armed = false;
      notify_body = true;
    }
  }
  req_cv.notify_all();
  resp_cv.notify_all();
  if (notify_req) {
    PostCompletionEvent(id, kHttpEventRequestSpaceAvailable);
  }
  if (notify_headers) {
    PostCompletionEvent(id, kHttpEventResponseHeadersReady);
  }
  if (notify_body) {
    PostCompletionEvent(id, kHttpEventResponseBodyAvailable);
  }
}

size_t HttpOperation::PopRequestBytes(uint8_t* dest, size_t max_bytes,
                                      bool* out_eof) {
  size_t copied = 0;
  bool notify_space = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    while (copied < max_bytes && !req_chunks.empty()) {
      ByteChunk& front = req_chunks.front();
      size_t take = (std::min)(max_bytes - copied, front.remaining());
      std::memcpy(dest + copied, front.data.data() + front.offset, take);
      front.offset += take;
      copied += take;
      req_buffered_bytes -= take;
      if (front.remaining() == 0) {
        req_chunks.pop_front();
      }
    }
    if (out_eof != nullptr) {
      *out_eof = req_chunks.empty() && req_eof;
    }
    if (req_space_armed && req_buffered_bytes <= kLowWaterRequestBytes) {
      req_space_armed = false;
      notify_space = true;
    }
  }
  if (notify_space) {
    PostCompletionEvent(id, kHttpEventRequestSpaceAvailable);
  }
  return copied;
}

}  // namespace http

int64_t HttpCreateRequestInternal(const char* method, const char* url,
                                  const char* headers_packed,
                                  const char* proxy_url,
                                  int64_t content_length) {
  http::HttpRequestOptions options;
  options.method = method != nullptr ? method : "GET";
  options.url = url != nullptr ? url : "";
  bool invalid_headers = false;
  if (headers_packed != nullptr && headers_packed[0] != '\0') {
    if (!http::TryParsePackedHeaders(headers_packed, &options.headers)) {
      invalid_headers = true;
    }
  }
  if (proxy_url != nullptr && proxy_url[0] != '\0') {
    options.proxy_url = proxy_url;
  }
  options.content_length = content_length;
  int64_t handle = http::CreateOperation(std::move(options));
  if (invalid_headers) {
    if (auto op = http::FindOperation(handle)) {
      op->MarkFailed("Invalid HTTP request header.", http::kHttpErrorOther);
    }
  }
  return handle;
}

void HttpStartRequestInternal(int64_t handle) { http::StartOperation(handle); }

int32_t HttpTryWriteRequestBodyInternal(int64_t handle, void* data,
                                        int32_t length) {
  if (length < 0 || data == nullptr) {
    return -1;
  }
  auto op = http::FindOperation(handle);
  if (!op) {
    return -1;
  }
  if (length == 0) {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (op->failed || op->resp_eof || op->req_write_closed ||
        op->IsCanceled()) {
      return -1;
    }
    return 0;
  }

  std::function<void()> notify_backend;
  int32_t to_write = 0;
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (op->failed || op->resp_eof || op->req_write_closed ||
        op->IsCanceled()) {
      return -1;
    }
    size_t available =
        op->req_buffered_bytes < http::kMaxRequestBufferBytes
            ? http::kMaxRequestBufferBytes - op->req_buffered_bytes
            : 0;
    size_t accepted = (std::min)(static_cast<size_t>(length), available);
    if (accepted > 0) {
      http::ByteChunk chunk;
      const auto* src = static_cast<const uint8_t*>(data);
      chunk.data.assign(src, src + accepted);
      op->req_chunks.push_back(std::move(chunk));
      op->req_buffered_bytes += accepted;
      to_write = static_cast<int32_t>(accepted);
      notify_backend = op->on_request_data_changed;
    }
    if (accepted < static_cast<size_t>(length)) {
      op->req_space_armed = true;
    }
  }
  if (to_write > 0) {
    op->req_cv.notify_all();
    if (notify_backend) {
      notify_backend();
    }
  }
  return to_write;
}

void HttpFinishRequestBodyInternal(int64_t handle) {
  auto op = http::FindOperation(handle);
  if (!op) {
    return;
  }
  std::function<void()> notify_backend;
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    op->req_eof = true;
    notify_backend = op->on_request_data_changed;
  }
  op->req_cv.notify_all();
  if (notify_backend) {
    notify_backend();
  }
}

int32_t HttpPollResponseHeadersInternal(int64_t handle) {
  auto op = http::FindOperation(handle);
  if (!op) {
    return -1;
  }
  std::lock_guard<std::mutex> lock(op->mutex);
  if (op->IsCanceled() || (op->failed && !op->headers_ready)) {
    return -1;
  }
  if (op->headers_ready) {
    return op->status_code;
  }
  if (op->resp_eof) {
    return -1;
  }
  op->resp_headers_armed = true;
  return 0;
}

std::string HttpGetResponseHeadersInternal(int64_t handle) {
  auto op = http::FindOperation(handle);
  if (!op) {
    return std::string();
  }
  std::lock_guard<std::mutex> lock(op->mutex);
  std::string out;
  out.append(op->reason_phrase);
  out.push_back('\n');
  for (const auto& kv : op->response_headers) {
    out.append(kv.first);
    out.push_back('\t');
    out.append(kv.second);
    out.push_back('\n');
  }
  return out;
}

int32_t HttpTryReadResponseBodyInternal(int64_t handle, void* dest,
                                        int32_t max_length) {
  if (max_length <= 0 || dest == nullptr) {
    return http::kHttpReadErrorOrCanceled;
  }
  auto op = http::FindOperation(handle);
  if (!op) {
    return http::kHttpReadErrorOrCanceled;
  }

  size_t copied = 0;
  bool crossed_low_water = false;
  std::function<void()> resume_backend;
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (op->IsCanceled()) {
      return http::kHttpReadErrorOrCanceled;
    }
    size_t before = op->resp_buffered_bytes;
    auto* out = static_cast<uint8_t*>(dest);
    size_t want = static_cast<size_t>(max_length);
    while (copied < want && !op->resp_chunks.empty()) {
      http::ByteChunk& front = op->resp_chunks.front();
      size_t take = (std::min)(want - copied, front.remaining());
      std::memcpy(out + copied, front.data.data() + front.offset, take);
      front.offset += take;
      copied += take;
      op->resp_buffered_bytes -= take;
      if (front.remaining() == 0) {
        op->resp_chunks.pop_front();
      }
    }
    if (copied > 0) {
      if (before >= http::kLowWaterResponseBytes &&
          op->resp_buffered_bytes <= http::kLowWaterResponseBytes) {
        crossed_low_water = true;
        resume_backend = op->on_response_space_available;
      }
    } else {
      if (op->failed) {
        return http::kHttpReadErrorOrCanceled;
      }
      if (op->resp_eof) {
        return 0;
      }
      op->resp_read_armed = true;
      return http::kHttpReadWouldBlock;
    }
  }

  if (crossed_low_water) {
    op->resp_cv.notify_all();
    if (resume_backend) {
      resume_backend();
    }
  } else if (copied > 0) {
    op->resp_cv.notify_all();
  }
  return static_cast<int32_t>(copied);
}

std::string HttpGetErrorInternal(int64_t handle) {
  auto op = http::FindOperation(handle);
  if (!op) {
    return std::string();
  }
  std::lock_guard<std::mutex> lock(op->mutex);
  return op->error_message;
}

int32_t HttpGetErrorClassInternal(int64_t handle) {
  auto op = http::FindOperation(handle);
  if (!op) {
    return http::kHttpErrorOther;
  }
  std::lock_guard<std::mutex> lock(op->mutex);
  if (op->IsCanceled() && !op->failed) {
    return http::kHttpErrorCanceled;
  }
  return static_cast<int32_t>(op->error_class);
}

void HttpCancelRequestInternal(int64_t handle) {
  http::CancelOperation(handle);
}

void HttpDestroyRequestInternal(int64_t handle) {
  http::DestroyOperation(handle);
}

void HttpCancelAllRequestsInternal() { http::CancelAllOperations(); }

int64_t HttpWaitNextEventInternal(int32_t timeout_ms) {
  auto& q = http::GetCompletionQueue();
  std::unique_lock<std::mutex> lock(q.mutex);
  if (q.events.empty() && !q.wake_requested && timeout_ms != 0) {
    if (timeout_ms < 0) {
      q.cv.wait(lock, [&q] { return !q.events.empty() || q.wake_requested; });
    } else {
      q.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [&q] { return !q.events.empty() || q.wake_requested; });
    }
  }
  if (q.wake_requested) {
    q.wake_requested = false;
    if (q.events.empty()) {
      return 0;
    }
  }
  if (q.events.empty()) {
    return 0;
  }
  int64_t ev = q.events.front();
  q.events.pop_front();
  return ev;
}

void HttpWakeEventPumpInternal() {
  auto& q = http::GetCompletionQueue();
  {
    std::lock_guard<std::mutex> lock(q.mutex);
    q.wake_requested = true;
  }
  q.cv.notify_all();
}

}  // namespace firebase
