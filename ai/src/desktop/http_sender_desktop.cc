/*
 * Copyright 2025 Google LLC
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

#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include "ai/src/common/http_sender.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "curl/curl.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

bool FileExists(const char* path) {
  if (!path || path[0] == '\0') return false;
  std::ifstream ifs(path, std::ios::binary);
  return ifs.good();
}

const char* ResolveCaBundlePath() {
  const char* env_bundle = std::getenv("CURL_CA_BUNDLE");
  if (FileExists(env_bundle)) return env_bundle;
  const char* env_ssl = std::getenv("SSL_CERT_FILE");
  if (FileExists(env_ssl)) return env_ssl;
  static const char* const kCandidates[] = {
      "/etc/ssl/cert.pem",                   // macOS & FreeBSD
      "/etc/ssl/certs/ca-certificates.crt",  // Debian/Ubuntu
      "/etc/pki/tls/certs/ca-bundle.crt",    // RHEL/Fedora
  };
  for (const char* candidate : kCandidates) {
    if (FileExists(candidate)) return candidate;
  }
  return nullptr;
}

struct CurlWriteContext {
  CURL* curl = nullptr;
  bool is_stream = false;
  HttpStreamChunkCallback on_chunk;
  std::string body;
};

size_t OnCurlWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
  size_t total = size * nmemb;
  if (total == 0 || !userdata) return 0;
  CurlWriteContext* ctx = static_cast<CurlWriteContext*>(userdata);
  long http_code = 0;
  if (ctx->curl) {
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);
  }
  if (ctx->is_stream && http_code >= 200 && http_code < 300 && ctx->on_chunk) {
    if (!ctx->on_chunk(ptr, total)) {
      return 0;
    }
    return total;
  }
  ctx->body.append(ptr, total);
  return total;
}

void PerformCurlRequestAsync(const HttpRequest& req,
                             const HttpStreamChunkCallback& on_chunk,
                             const HttpCompletionCallback& on_complete) {
  std::thread([req, on_chunk, on_complete]() {
    CURL* curl = curl_easy_init();
    if (!curl) {
      if (on_complete) {
        on_complete(0, "", "Failed to initialize libcurl handle.");
      }
      return;
    }

    char err_buf[CURL_ERROR_SIZE];
    err_buf[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buf);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    const char* ca_bundle = ResolveCaBundlePath();
    if (ca_bundle) {
      curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);
    }

    if (req.timeout_ms > 0) {
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                       static_cast<long>(req.timeout_ms));
    }

    struct curl_slist* headers = nullptr;
    for (const auto& kv : req.headers) {
      std::string header_line = kv.first + ": " + kv.second;
      headers = curl_slist_append(headers, header_line.c_str());
    }
    if (headers) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (req.method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                       static_cast<long>(req.body.size()));
    } else if (req.method != "GET" && !req.method.empty()) {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    }

    CurlWriteContext write_ctx;
    write_ctx.curl = curl;
    write_ctx.is_stream = static_cast<bool>(on_chunk);
    write_ctx.on_chunk = on_chunk;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnCurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_ctx);

    CURLcode res = curl_easy_perform(curl);
    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

    if (headers) {
      curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);

    std::string transport_err;
    if (res != CURLE_OK && !(write_ctx.is_stream && res == CURLE_WRITE_ERROR &&
                             http_status >= 200 && http_status < 300)) {
      transport_err =
          std::string("libcurl error (") + curl_easy_strerror(res) + ")";
      if (err_buf[0] != '\0') {
        transport_err += std::string(": ") + err_buf;
      }
    }

    if (on_complete) {
      on_complete(static_cast<int>(http_status), write_ctx.body, transport_err);
    }
  }).detach();
}

}  // namespace

void HttpSender::Initialize() {
  ::firebase::rest::InitTransportCurl();
  ::firebase::rest::util::Initialize();
}

void HttpSender::Cleanup() {
  ::firebase::rest::util::Terminate();
  ::firebase::rest::CleanupTransportCurl();
}

void HttpSender::SendUnary(::firebase::App* /*app*/, const HttpRequest& request,
                           const HttpCompletionCallback& on_complete) {
  PerformCurlRequestAsync(request, HttpStreamChunkCallback(), on_complete);
}

void HttpSender::SendStream(::firebase::App* /*app*/,
                            const HttpRequest& request,
                            const HttpStreamChunkCallback& on_chunk,
                            const HttpCompletionCallback& on_complete) {
  PerformCurlRequestAsync(request, on_chunk, on_complete);
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
