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
#include <charconv>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/http/http_transport.h"
#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/tls_policy.h"
#include "curl/curl.h"

namespace firebase {
namespace http {
namespace {

using ::firebase::rest::curl::ApplyOpt;

bool HasHeaderIgnoreCase(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view target) {
  for (const auto& kv : headers) {
    if (AsciiEqualsIgnoreCase(kv.first, target)) {
      return true;
    }
  }
  return false;
}

std::string_view FindHeaderValueIgnoreCase(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view target) {
  for (const auto& kv : headers) {
    if (AsciiEqualsIgnoreCase(kv.first, target)) {
      return kv.second;
    }
  }
  return {};
}

bool IsRedirectStatus(int32_t status) {
  return status == 301 || status == 302 || status == 303 || status == 307 ||
         status == 308;
}

bool AppendSList(::firebase::rest::curl::UniqueSList* list, const char* line) {
  curl_slist* next = curl_slist_append(list->get(), line);
  if (next == nullptr) {
    return false;
  }
  list->release();
  list->reset(next);
  return true;
}

HttpErrorClass ClassifyCurlError(CURLcode code, bool canceled) {
  if (canceled) {
    return kHttpErrorCanceled;
  }
  switch (code) {
    case CURLE_OPERATION_TIMEDOUT:
      return kHttpErrorTimeout;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_SSL_CACERT_BADFILE:
    case CURLE_SSL_SHUTDOWN_FAILED:
    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    case CURLE_SSL_INVALIDCERTSTATUS:
    case CURLE_USE_SSL_FAILED:
    case CURLE_SSL_ENGINE_INITFAILED:
    case CURLE_SSL_CLIENTCERT:
      return kHttpErrorTls;
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
    case CURLE_GOT_NOTHING:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
    case CURLE_PARTIAL_FILE:
    case CURLE_INTERFACE_FAILED:
      return kHttpErrorConnect;
    default:
      return kHttpErrorOther;
  }
}

struct CurlTransfer {
  std::shared_ptr<HttpOperation> op;
  ::firebase::rest::curl::UniqueEasy easy;
  ::firebase::rest::curl::UniqueSList headers;
  char error_buf[CURL_ERROR_SIZE] = {};
  bool upload_paused = false;
  bool download_paused = false;
  bool headers_committed = false;

  std::string current_url;
  std::string active_method;
  std::vector<std::pair<std::string, std::string>> active_headers;
  int64_t active_content_length = kNoRequestContent;
  int redirect_count = 0;

  std::string redirect_url;
  int32_t redirect_status = 0;
  size_t redirect_body_drained = 0;
  std::string redirect_blocked_error;

  int32_t staged_status = 0;
  std::string staged_reason;
  std::vector<std::pair<std::string, std::string>> staged_headers;

  void CommitStagedHeadersIfNeeded() {
    if (headers_committed || staged_status <= 0) {
      return;
    }
    headers_committed = true;
    op->CommitResponseHeaders(staged_status, staged_reason,
                              std::move(staged_headers));
    staged_headers.clear();
  }
};

size_t OnCurlHeader(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* transfer = static_cast<CurlTransfer*>(userdata);
  size_t total = size * nitems;
  if (transfer == nullptr || transfer->op->IsCanceled()) {
    return 0;
  }
  if (!transfer->redirect_url.empty()) {
    // Ignore any trailing headers (e.g. chunked trailers) on a 3xx redirect
    // response whose Location has already been captured.
    return total;
  }

  std::string_view line(buffer, total);
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.remove_suffix(1);
  }

  if (line.empty()) {
    if (transfer->staged_status >= 200) {
      std::string_view location =
          FindHeaderValueIgnoreCase(transfer->staged_headers, "Location");
      if (IsRedirectStatus(transfer->staged_status) && !location.empty()) {
        auto resolved = ResolveRedirectUrl(transfer->current_url, location);
        if (!resolved ||
            (IsHttpsUrl(transfer->op->url) && !IsHttpsUrl(*resolved))) {
          transfer->redirect_blocked_error =
              "Blocked unsafe or scheme-downgraded HTTP redirect.";
          return 0;
        }
        // Route all HTTP redirects manually through HandleRedirect so
        // 301/302/303 -> GET rewrites (clearing CURLOPT_UPLOAD and
        // CURLOPT_CUSTOMREQUEST), kMaxRedirects, and cross-origin sensitive
        // header stripping match Android and Apple identically.
        transfer->redirect_url = std::move(*resolved);
        transfer->redirect_status = transfer->staged_status;
        if (transfer->active_content_length != kNoRequestContent &&
            transfer->active_content_length != 0) {
          return 0;
        }
        return total;
      }
      transfer->CommitStagedHeadersIfNeeded();
    }
    return total;
  }

  if (line.size() >= 5 && line.substr(0, 5) == "HTTP/") {
    transfer->staged_headers.clear();
    transfer->staged_reason.clear();
    transfer->staged_status = 0;

    size_t sp1 = line.find(' ');
    if (sp1 != std::string_view::npos) {
      std::string_view rest = TrimAsciiWhitespace(line.substr(sp1 + 1));
      size_t sp2 = rest.find(' ');
      std::string_view code_str =
          sp2 == std::string_view::npos ? rest : rest.substr(0, sp2);
      int32_t parsed = 0;
      auto [ptr, ec] = std::from_chars(
          code_str.data(), code_str.data() + code_str.size(), parsed);
      if (ec == std::errc() && parsed > 0) {
        transfer->staged_status = parsed;
      }
      if (sp2 != std::string_view::npos) {
        std::string_view reason = TrimAsciiWhitespace(rest.substr(sp2 + 1));
        transfer->staged_reason.assign(reason.data(), reason.size());
      }
    }
    return total;
  }

  size_t colon = line.find(':');
  if (colon != std::string_view::npos && colon > 0) {
    std::string_view name = TrimAsciiWhitespace(line.substr(0, colon));
    std::string_view value = TrimAsciiWhitespace(line.substr(colon + 1));
    if (!name.empty()) {
      transfer->staged_headers.emplace_back(std::string(name),
                                            std::string(value));
    }
  }
  return total;
}

size_t OnCurlWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* transfer = static_cast<CurlTransfer*>(userdata);
  size_t total = size * nmemb;
  if (transfer == nullptr || transfer->op->IsCanceled()) {
    return 0;
  }
  if (!transfer->redirect_url.empty()) {
    // Drain small 3xx bodies so keep-alive connections remain pooled in
    // multi_, capping at 64 KiB so oversized redirect bodies abort promptly.
    constexpr size_t kMaxRedirectBodyDrainBytes = 64 * 1024;
    transfer->redirect_body_drained += total;
    if (transfer->redirect_body_drained > kMaxRedirectBodyDrainBytes) {
      return 0;
    }
    return total;
  }
  transfer->CommitStagedHeadersIfNeeded();
  if (total == 0) {
    return 0;
  }

  {
    std::lock_guard<std::mutex> lock(transfer->op->mutex);
    if (transfer->op->resp_buffered_bytes >= kMaxResponseBufferBytes) {
      transfer->download_paused = true;
      return CURL_WRITEFUNC_PAUSE;
    }
  }

  transfer->op->PushResponseBytes(reinterpret_cast<const uint8_t*>(ptr), total);
  return total;
}

size_t OnCurlRead(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* transfer = static_cast<CurlTransfer*>(userdata);
  size_t cap = size * nitems;
  if (transfer == nullptr || transfer->op->IsCanceled()) {
    return CURL_READFUNC_ABORT;
  }
  if (cap == 0) {
    return 0;
  }

  bool eof = false;
  size_t n = transfer->op->PopRequestBytes(reinterpret_cast<uint8_t*>(buffer),
                                           cap, &eof);
  if (n > 0) {
    return n;
  }
  if (eof) {
    return 0;
  }
  transfer->upload_paused = true;
  return CURL_READFUNC_PAUSE;
}

class CurlHttpEngine {
 public:
  static CurlHttpEngine& Instance() {
    static auto* engine = new CurlHttpEngine();
    return *engine;
  }

  void StartOperation(const std::shared_ptr<HttpOperation>& op) {
    int64_t id = op->id;
    {
      std::lock_guard<std::mutex> lock(op->mutex);
      if (op->IsCanceled()) {
        return;
      }
      op->on_request_data_changed = [this, id]() { EnqueueResumeUpload(id); };
      op->on_response_space_available = [this, id]() {
        EnqueueResumeDownload(id);
      };
      op->on_cancel = [this, id]() { EnqueueCancel(id); };
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_starts_.push_back(op);
    }
    WakeLoop();
  }

 private:
  CurlHttpEngine() {
    // Share reference-counted curl_global_init lifetime with firebase_rest_lib
    // without spawning an unused CurlThread, ensuring App::Dispose() never
    // tears down libcurl globals underneath active HTTP operations.
    ::firebase::rest::curl::AcquireCurlGlobal();

    multi_.reset(curl_multi_init());
    if (multi_ != nullptr) {
      curl_multi_setopt(multi_.get(), CURLMOPT_MAX_HOST_CONNECTIONS, 16L);
      curl_multi_setopt(multi_.get(), CURLMOPT_MAX_TOTAL_CONNECTIONS, 64L);
    }
    worker_ = std::thread([this]() { RunLoop(); });
    worker_.detach();
  }

  void EnqueueResumeUpload(int64_t id) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_upload_resumes_.push_back(id);
    }
    WakeLoop();
  }

  void EnqueueResumeDownload(int64_t id) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_download_resumes_.push_back(id);
    }
    WakeLoop();
  }

  void EnqueueCancel(int64_t id) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_cancels_.push_back(id);
    }
    WakeLoop();
  }

  void WakeLoop() {
    if (multi_ != nullptr) {
      curl_multi_wakeup(multi_.get());
    }
    cv_.notify_one();
  }

  std::unique_ptr<CurlTransfer> BuildTransfer(
      const std::shared_ptr<HttpOperation>& op, std::string target_url,
      std::string method,
      std::vector<std::pair<std::string, std::string>> request_headers,
      int64_t content_length, int redirect_count) {
    if (!IsHttpOrHttpsUrl(target_url)) {
      op->MarkFailed("Unsupported URI scheme; only http and https are allowed.",
                     kHttpErrorOther);
      return nullptr;
    }

    if (!op->proxy_url.empty() && !ParseProxyHostPort(op->proxy_url)) {
      op->MarkFailed("Invalid or unsupported proxy URL.", kHttpErrorOther);
      return nullptr;
    }

    bool is_get = AsciiEqualsIgnoreCase(method, "GET");
    bool is_head = AsciiEqualsIgnoreCase(method, "HEAD");
    if ((is_get || is_head) &&
        (content_length > 0 || content_length == kChunkedRequestContent)) {
      op->MarkFailed("GET and HEAD requests cannot send a request body.",
                     kHttpErrorOther);
      return nullptr;
    }

    ::firebase::rest::curl::UniqueEasy easy(curl_easy_init());
    if (easy == nullptr) {
      op->MarkFailed("Failed to initialize native libcurl handle.",
                     kHttpErrorOther);
      return nullptr;
    }

    CURLcode tls_rc = ::firebase::rest::curl::ApplyTlsPolicy(easy.get());
    if (tls_rc != CURLE_OK) {
      op->MarkFailed(curl_easy_strerror(tls_rc), kHttpErrorTls);
      return nullptr;
    }

    auto transfer = std::make_unique<CurlTransfer>();
    transfer->op = op;
    transfer->current_url = std::move(target_url);
    transfer->active_method = std::move(method);
    transfer->active_headers = std::move(request_headers);
    transfer->active_content_length = content_length;
    transfer->redirect_count = redirect_count;
    transfer->error_buf[0] = '\0';

    CURL* raw_easy = easy.get();
    CURLcode opt_err = CURLE_OK;
    ApplyOpt(raw_easy, CURLOPT_ERRORBUFFER, transfer->error_buf, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_NOSIGNAL, 1L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_PROTOCOLS_STR, "http,https", &opt_err);
    ApplyOpt(raw_easy, CURLOPT_UNRESTRICTED_AUTH, 0L, &opt_err);
    // Redirects are followed manually via HandleRedirect so method rewrites,
    // redirect limits, HTTPS downgrade blocking, and cross-origin header
    // stripping stay consistent with Android and Apple.
    ApplyOpt(raw_easy, CURLOPT_FOLLOWLOCATION, 0L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_TCP_NODELAY, 1L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_BUFFERSIZE, 256L * 1024L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_UPLOAD_BUFFERSIZE, 256L * 1024L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_CONNECTTIMEOUT_MS, 60000L, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_TIMEOUT_MS, 0L, &opt_err);

    ApplyOpt(raw_easy, CURLOPT_URL, transfer->current_url.c_str(), &opt_err);
    ApplyOpt(raw_easy, CURLOPT_PRIVATE, transfer.get(), &opt_err);
    ApplyOpt(raw_easy, CURLOPT_HEADERFUNCTION, OnCurlHeader, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_HEADERDATA, transfer.get(), &opt_err);
    ApplyOpt(raw_easy, CURLOPT_WRITEFUNCTION, OnCurlWrite, &opt_err);
    ApplyOpt(raw_easy, CURLOPT_WRITEDATA, transfer.get(), &opt_err);

    if (!op->proxy_url.empty()) {
      ApplyOpt(raw_easy, CURLOPT_PROXY, op->proxy_url.c_str(), &opt_err);
    }

    bool has_expect = HasHeaderIgnoreCase(transfer->active_headers, "Expect");
    bool has_transfer_encoding =
        HasHeaderIgnoreCase(transfer->active_headers, "Transfer-Encoding");
    bool has_content_length =
        HasHeaderIgnoreCase(transfer->active_headers, "Content-Length");

    ::firebase::rest::curl::UniqueSList header_list;
    for (const auto& kv : transfer->active_headers) {
      std::string line;
      line.reserve(kv.first.size() + kv.second.size() + 2);
      line.append(kv.first);
      line.append(": ");
      line.append(kv.second);
      if (!AppendSList(&header_list, line.c_str())) {
        op->MarkFailed("Out of memory building request headers.",
                       kHttpErrorOther);
        return nullptr;
      }
    }
    if (!has_expect) {
      // Disable libcurl's automatic "Expect: 100-continue" wait on uploads.
      if (!AppendSList(&header_list, "Expect:")) {
        op->MarkFailed("Out of memory building request headers.",
                       kHttpErrorOther);
        return nullptr;
      }
    }

    bool no_body = content_length == kNoRequestContent ||
                   ((is_get || is_head) && content_length == 0);
    if (no_body) {
      if (is_head) {
        ApplyOpt(raw_easy, CURLOPT_NOBODY, 1L, &opt_err);
      } else if (is_get) {
        ApplyOpt(raw_easy, CURLOPT_HTTPGET, 1L, &opt_err);
      } else {
        ApplyOpt(raw_easy, CURLOPT_CUSTOMREQUEST,
                 transfer->active_method.c_str(), &opt_err);
        if (!has_content_length &&
            (AsciiEqualsIgnoreCase(transfer->active_method, "POST") ||
             AsciiEqualsIgnoreCase(transfer->active_method, "PUT") ||
             AsciiEqualsIgnoreCase(transfer->active_method, "PATCH"))) {
          if (!AppendSList(&header_list, "Content-Length: 0")) {
            op->MarkFailed("Out of memory building request headers.",
                           kHttpErrorOther);
            return nullptr;
          }
        }
      }
    } else {
      ApplyOpt(raw_easy, CURLOPT_UPLOAD, 1L, &opt_err);
      ApplyOpt(raw_easy, CURLOPT_CUSTOMREQUEST, transfer->active_method.c_str(),
               &opt_err);
      ApplyOpt(raw_easy, CURLOPT_READFUNCTION, OnCurlRead, &opt_err);
      ApplyOpt(raw_easy, CURLOPT_READDATA, transfer.get(), &opt_err);
      if (content_length >= 0) {
        ApplyOpt(raw_easy, CURLOPT_INFILESIZE_LARGE,
                 static_cast<curl_off_t>(content_length), &opt_err);
      } else if (!has_transfer_encoding) {
        if (!AppendSList(&header_list, "Transfer-Encoding: chunked")) {
          op->MarkFailed("Out of memory building request headers.",
                         kHttpErrorOther);
          return nullptr;
        }
      }
    }

    transfer->headers = std::move(header_list);
    if (transfer->headers != nullptr) {
      ApplyOpt(raw_easy, CURLOPT_HTTPHEADER, transfer->headers.get(), &opt_err);
    }

    if (opt_err != CURLE_OK) {
      op->MarkFailed(curl_easy_strerror(opt_err),
                     ClassifyCurlError(opt_err, op->IsCanceled()));
      return nullptr;
    }

    transfer->easy = std::move(easy);
    return transfer;
  }

  void FailAllActive(CURLMcode mc) {
    std::string err = curl_multi_strerror(mc);
    std::unordered_map<int64_t, std::unique_ptr<CurlTransfer>> failed;
    failed.swap(active_);
    for (auto& kv : failed) {
      if (kv.second) {
        curl_multi_remove_handle(multi_.get(), kv.second->easy.get());
        if (!kv.second->op->IsCanceled()) {
          kv.second->op->MarkFailed(err, kHttpErrorOther);
        }
      }
    }
  }

  void RunLoop() {
    while (true) {
      std::deque<std::shared_ptr<HttpOperation>> starts;
      std::vector<int64_t> upload_resumes;
      std::vector<int64_t> download_resumes;
      std::vector<int64_t> cancels;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (active_.empty() && pending_starts_.empty() &&
            pending_cancels_.empty()) {
          cv_.wait(lock, [this]() {
            return !pending_starts_.empty() || !pending_cancels_.empty();
          });
        }
        starts.swap(pending_starts_);
        upload_resumes.swap(pending_upload_resumes_);
        download_resumes.swap(pending_download_resumes_);
        cancels.swap(pending_cancels_);
      }

      for (int64_t id : cancels) {
        auto it = active_.find(id);
        if (it != active_.end()) {
          curl_multi_remove_handle(multi_.get(), it->second->easy.get());
          active_.erase(it);
        }
      }

      for (const auto& op : starts) {
        if (op->IsCanceled() || active_.find(op->id) != active_.end()) {
          continue;
        }
        if (multi_ == nullptr) {
          op->MarkFailed("Native libcurl multi engine failed to initialize.",
                         kHttpErrorOther);
          continue;
        }
        auto transfer =
            BuildTransfer(op, op->url, op->method, op->request_headers,
                          op->content_length, 0);
        if (!transfer) {
          continue;
        }
        CURLMcode mc =
            curl_multi_add_handle(multi_.get(), transfer->easy.get());
        if (mc != CURLM_OK) {
          op->MarkFailed(curl_multi_strerror(mc), kHttpErrorOther);
          continue;
        }
        active_.emplace(op->id, std::move(transfer));
      }

      auto apply_pause_mask = [](CurlTransfer& t) {
        int mask = (t.upload_paused ? CURLPAUSE_SEND : CURLPAUSE_SEND_CONT) |
                   (t.download_paused ? CURLPAUSE_RECV : CURLPAUSE_RECV_CONT);
        curl_easy_pause(t.easy.get(), mask);
      };

      for (int64_t id : upload_resumes) {
        auto it = active_.find(id);
        if (it != active_.end() && it->second->upload_paused) {
          it->second->upload_paused = false;
          apply_pause_mask(*it->second);
        }
      }

      for (int64_t id : download_resumes) {
        auto it = active_.find(id);
        if (it != active_.end() && it->second->download_paused) {
          it->second->download_paused = false;
          apply_pause_mask(*it->second);
        }
      }

      if (active_.empty()) {
        continue;
      }

      int still_running = 0;
      CURLMcode perform_rc = curl_multi_perform(multi_.get(), &still_running);
      if (perform_rc != CURLM_OK && perform_rc != CURLM_CALL_MULTI_PERFORM) {
        FailAllActive(perform_rc);
        continue;
      }

      bool added_redirect = false;
      int msgs_left = 0;
      while (CURLMsg* msg = curl_multi_info_read(multi_.get(), &msgs_left)) {
        if (msg->msg != CURLMSG_DONE) {
          continue;
        }
        // Extract fields from `msg` before `curl_multi_remove_handle()`, which
        // invalidates the CURLMsg pointer returned by `curl_multi_info_read()`.
        CURL* easy_handle = msg->easy_handle;
        CURLcode result = msg->data.result;
        CurlTransfer* raw_transfer = nullptr;
        curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &raw_transfer);
        if (raw_transfer == nullptr) {
          curl_multi_remove_handle(multi_.get(), easy_handle);
          continue;
        }

        int64_t id = raw_transfer->op->id;
        auto it = active_.find(id);
        std::unique_ptr<CurlTransfer> owned_transfer;
        curl_multi_remove_handle(multi_.get(), easy_handle);
        if (it != active_.end()) {
          owned_transfer = std::move(it->second);
          active_.erase(it);
        }
        if (!owned_transfer) {
          continue;
        }

        if (!owned_transfer->op->IsCanceled()) {
          if (!owned_transfer->redirect_blocked_error.empty()) {
            owned_transfer->op->MarkFailed(
                owned_transfer->redirect_blocked_error, kHttpErrorOther);
          } else if (!owned_transfer->redirect_url.empty()) {
            if (HandleRedirect(std::move(owned_transfer))) {
              added_redirect = true;
            }
          } else if (result == CURLE_OK) {
            if (!owned_transfer->headers_committed) {
              long code = 0;  // NOLINT
              curl_easy_getinfo(owned_transfer->easy.get(),
                                CURLINFO_RESPONSE_CODE, &code);
              if (owned_transfer->staged_status <= 0 && code > 0) {
                owned_transfer->staged_status = static_cast<int32_t>(code);
              }
              owned_transfer->CommitStagedHeadersIfNeeded();
            }
            owned_transfer->op->MarkResponseComplete();
          } else {
            std::string err = owned_transfer->error_buf[0] != '\0'
                                  ? std::string(owned_transfer->error_buf)
                                  : std::string(curl_easy_strerror(result));
            owned_transfer->op->MarkFailed(
                err, ClassifyCurlError(result, owned_transfer->op->IsCanceled()));
          }
        }
      }

      if (added_redirect) {
        // Drive newly added redirect handles immediately with
        // curl_multi_perform rather than sleeping up to 1000 ms in
        // curl_multi_poll first.
        continue;
      }

      if (!active_.empty()) {
        int numfds = 0;
        CURLMcode poll_rc =
            curl_multi_poll(multi_.get(), nullptr, 0, 1000, &numfds);
        if (poll_rc != CURLM_OK) {
          FailAllActive(poll_rc);
        }
      }
    }
  }

  bool HandleRedirect(std::unique_ptr<CurlTransfer> prev) {
    if (!prev) {
      return false;
    }
    auto op = prev->op;
    if (prev->redirect_count >= kMaxRedirects) {
      op->MarkFailed("Maximum HTTP redirects exceeded.", kHttpErrorOther);
      return false;
    }

    int32_t status = prev->redirect_status;
    std::string next_method = prev->active_method;
    int64_t next_content_len = prev->active_content_length;
    if ((status == 301 || status == 302 || status == 303) &&
        !AsciiEqualsIgnoreCase(next_method, "GET") &&
        !AsciiEqualsIgnoreCase(next_method, "HEAD")) {
      next_method = "GET";
      next_content_len = kNoRequestContent;
      op->MarkRequestWriteClosed();
    } else if (status == 303 && AsciiEqualsIgnoreCase(next_method, "HEAD")) {
      next_content_len = kNoRequestContent;
    } else if (next_content_len != kNoRequestContent && next_content_len != 0) {
      op->MarkFailed("Cannot replay streamed request body across redirect.",
                     kHttpErrorOther);
      return false;
    }

    bool same_origin = IsSameOrigin(op->url, prev->redirect_url) &&
                       IsSameOrigin(prev->current_url, prev->redirect_url);
    std::vector<std::pair<std::string, std::string>> filtered;
    filtered.reserve(prev->active_headers.size());
    for (const auto& kv : prev->active_headers) {
      if (!same_origin && (IsSensitiveRedirectHeader(kv.first) ||
                           AsciiEqualsIgnoreCase(kv.first, "Host"))) {
        continue;
      }
      if (next_content_len == kNoRequestContent &&
          (AsciiEqualsIgnoreCase(kv.first, "Content-Type") ||
           AsciiEqualsIgnoreCase(kv.first, "Content-Length") ||
           AsciiEqualsIgnoreCase(kv.first, "Transfer-Encoding"))) {
        continue;
      }
      filtered.push_back(kv);
    }

    auto next_transfer = BuildTransfer(
        op, std::move(prev->redirect_url), std::move(next_method),
        std::move(filtered), next_content_len, prev->redirect_count + 1);
    if (!next_transfer) {
      return false;
    }
    CURLMcode mc =
        curl_multi_add_handle(multi_.get(), next_transfer->easy.get());
    if (mc != CURLM_OK) {
      op->MarkFailed(curl_multi_strerror(mc), kHttpErrorOther);
      return false;
    }
    active_.emplace(op->id, std::move(next_transfer));
    return true;
  }

  ::firebase::rest::curl::UniqueMulti multi_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<HttpOperation>> pending_starts_;
  std::vector<int64_t> pending_upload_resumes_;
  std::vector<int64_t> pending_download_resumes_;
  std::vector<int64_t> pending_cancels_;
  std::unordered_map<int64_t, std::unique_ptr<CurlTransfer>> active_;
  std::thread worker_;
};

}  // namespace

void StartPlatformHttpOperation(const std::shared_ptr<HttpOperation>& op) {
  CurlHttpEngine::Instance().StartOperation(op);
}

}  // namespace http
}  // namespace firebase
