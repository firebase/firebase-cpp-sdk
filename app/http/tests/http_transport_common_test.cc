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

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "app/http/http_transport.h"
#include "gtest/gtest.h"

namespace firebase {
namespace http {
namespace {

TEST(HttpTransportCommonTest, ParseUrlOriginStandardAndDefaults) {
  auto https = ParseUrlOrigin("HTTPS://FireBaseStorage.GoogleAPIs.com/v0/b/x");
  ASSERT_TRUE(https.has_value());
  EXPECT_EQ(https->scheme, "https");
  EXPECT_EQ(https->host, "firebasestorage.googleapis.com");
  EXPECT_EQ(https->port, 443);

  auto http = ParseUrlOrigin("http://localhost/foo?bar=1");
  ASSERT_TRUE(http.has_value());
  EXPECT_EQ(http->scheme, "http");
  EXPECT_EQ(http->host, "localhost");
  EXPECT_EQ(http->port, 80);

  auto explicit_port = ParseUrlOrigin("http://127.0.0.1:5001/proj/us/fn");
  ASSERT_TRUE(explicit_port.has_value());
  EXPECT_EQ(explicit_port->scheme, "http");
  EXPECT_EQ(explicit_port->host, "127.0.0.1");
  EXPECT_EQ(explicit_port->port, 5001);

  auto ipv6 = ParseUrlOrigin("https://[::1]:8443/path");
  ASSERT_TRUE(ipv6.has_value());
  EXPECT_EQ(ipv6->host, "[::1]");
  EXPECT_EQ(ipv6->port, 8443);

  auto userinfo = ParseUrlOrigin("https://user:pass@Example.COM/p");
  ASSERT_TRUE(userinfo.has_value());
  EXPECT_EQ(userinfo->host, "example.com");
  EXPECT_EQ(userinfo->port, 443);
}

TEST(HttpTransportCommonTest, ParseUrlOriginRejectsUnsafeInputs) {
  EXPECT_FALSE(ParseUrlOrigin("").has_value());
  EXPECT_FALSE(ParseUrlOrigin("file:///etc/passwd").has_value());
  EXPECT_FALSE(ParseUrlOrigin("ftp://example.com/a").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://evil.com\\@trusted.com/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://bad host/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://example.com:0/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://example.com:70000/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://example.com:abc/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://[::1/p").has_value());
  EXPECT_FALSE(ParseUrlOrigin("https://[]/p").has_value());
}

TEST(HttpTransportCommonTest, IsSameOriginComparesNormalizedTuples) {
  EXPECT_TRUE(
      IsSameOrigin("https://Example.COM/a", "HTTPS://example.com:443/b"));
  EXPECT_TRUE(IsSameOrigin("http://localhost/a", "http://localhost:80/b"));
  EXPECT_FALSE(IsSameOrigin("https://example.com/a", "http://example.com/a"));
  EXPECT_FALSE(
      IsSameOrigin("http://127.0.0.1:8080/a", "http://127.0.0.1:8081/a"));
  EXPECT_FALSE(
      IsSameOrigin("https://a.example.com/", "https://b.example.com/"));
  EXPECT_FALSE(IsSameOrigin("not-a-url", "https://example.com/"));
}

TEST(HttpTransportCommonTest, SensitiveRedirectHeadersMatchCaseInsensitively) {
  EXPECT_TRUE(IsSensitiveRedirectHeader("Authorization"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("authorization"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("Proxy-Authorization"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("X-Firebase-AppCheck"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("x-firebase-appcheck"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("x-goog-api-key"));
  EXPECT_TRUE(IsSensitiveRedirectHeader("X-Goog-Api-Key"));
  EXPECT_FALSE(IsSensitiveRedirectHeader("Content-Type"));
  EXPECT_FALSE(IsSensitiveRedirectHeader("X-Custom-Header"));
}

TEST(HttpTransportCommonTest, ResolveRedirectUrlRfc3986Forms) {
  const std::string base =
      "https://storage.example.com/v0/b/my-bucket/o?a=1#frag";

  EXPECT_EQ(ResolveRedirectUrl(base, "https://cdn.example.com/file"),
            "https://cdn.example.com/file");
  EXPECT_EQ(ResolveRedirectUrl(base, "//cdn.example.com/file"),
            "https://cdn.example.com/file");
  EXPECT_EQ(ResolveRedirectUrl(base, "/new/path?x=2"),
            "https://storage.example.com/new/path?x=2");
  EXPECT_EQ(ResolveRedirectUrl(base, "?next=1"),
            "https://storage.example.com/v0/b/my-bucket/o?next=1");
  EXPECT_EQ(ResolveRedirectUrl(base, "#other"),
            "https://storage.example.com/v0/b/my-bucket/o?a=1#other");
  EXPECT_EQ(ResolveRedirectUrl(base, "sibling.bin"),
            "https://storage.example.com/v0/b/my-bucket/sibling.bin");
  EXPECT_EQ(ResolveRedirectUrl(base, "next?redir=http://ignored.example/x"),
            "https://storage.example.com/v0/b/my-bucket/next?redir=http://"
            "ignored.example/x");
}

TEST(HttpTransportCommonTest, ResolveRedirectUrlBlocksDowngradesAndConfusion) {
  const std::string https_base = "https://example.com/a/b";
  EXPECT_FALSE(
      ResolveRedirectUrl(https_base, "http://example.com/a/b").has_value());
  EXPECT_FALSE(
      ResolveRedirectUrl(https_base, "file:///etc/passwd").has_value());
  EXPECT_FALSE(
      ResolveRedirectUrl(https_base, "data:text/plain,hi").has_value());
  EXPECT_FALSE(
      ResolveRedirectUrl(https_base, "javascript:alert(1)").has_value());
  EXPECT_FALSE(ResolveRedirectUrl(https_base, "\\\\evil.com/x").has_value());
  EXPECT_FALSE(ResolveRedirectUrl(https_base, "/\\evil.com/x").has_value());
  EXPECT_FALSE(ResolveRedirectUrl(https_base, "/bad\r\nHeader: 1").has_value());
  EXPECT_FALSE(ResolveRedirectUrl(https_base, "").has_value());
  EXPECT_FALSE(ResolveRedirectUrl(https_base, ":no-scheme").has_value());

  // HTTP -> HTTP and HTTP -> HTTPS are both allowed.
  EXPECT_EQ(ResolveRedirectUrl("http://127.0.0.1:8080/a", "/b"),
            "http://127.0.0.1:8080/b");
  EXPECT_EQ(
      ResolveRedirectUrl("http://127.0.0.1:8080/a", "https://example.com/b"),
      "https://example.com/b");
}

TEST(HttpTransportCommonTest, ParseProxyHostPort) {
  auto p1 = ParseProxyHostPort("http://127.0.0.1:8888");
  ASSERT_TRUE(p1.has_value());
  EXPECT_EQ(p1->first, "127.0.0.1");
  EXPECT_EQ(p1->second, 8888);

  auto p2 = ParseProxyHostPort("proxy.local:3128");
  ASSERT_TRUE(p2.has_value());
  EXPECT_EQ(p2->first, "proxy.local");
  EXPECT_EQ(p2->second, 3128);

  auto p3 = ParseProxyHostPort("http://[::1]:8080/");
  ASSERT_TRUE(p3.has_value());
  EXPECT_EQ(p3->first, "::1");
  EXPECT_EQ(p3->second, 8080);

  EXPECT_FALSE(ParseProxyHostPort("").has_value());
  EXPECT_FALSE(ParseProxyHostPort("socks5://127.0.0.1:1080").has_value());
}

TEST(HttpTransportCommonTest, ParsePackedHeadersPreservesDuplicates) {
  auto headers = ParsePackedHeaders(
      "Authorization\tBearer tok\n"
      "X-Multi\tfirst\r\n"
      "X-Multi\tsecond\n"
      "Content-Type: application/json\n");
  ASSERT_EQ(headers.size(), 4u);
  EXPECT_EQ(headers[0], std::make_pair(std::string("Authorization"),
                                       std::string("Bearer tok")));
  EXPECT_EQ(headers[1],
            std::make_pair(std::string("X-Multi"), std::string("first")));
  EXPECT_EQ(headers[2],
            std::make_pair(std::string("X-Multi"), std::string("second")));
  EXPECT_EQ(headers[3], std::make_pair(std::string("Content-Type"),
                                       std::string("application/json")));
}

TEST(HttpTransportCommonTest, RequestAndResponsePipeWatermarksAndCancel) {
  int64_t handle = HttpCreateRequestInternal(
      "POST", "http://127.0.0.1:80/upload", "X-Test\t1\n", "",
      static_cast<int64_t>(kMaxRequestBufferBytes + 1024));
  ASSERT_GT(handle, 0);
  auto op = FindOperation(handle);
  ASSERT_NE(op, nullptr);

  // Fill upload buffer beyond kMaxRequestBufferBytes to test cap & arming.
  std::vector<uint8_t> upload_buf(kMaxRequestBufferBytes + 1024, 0xAB);
  int32_t accepted = HttpTryWriteRequestBodyInternal(
      handle, upload_buf.data(), static_cast<int32_t>(upload_buf.size()));
  EXPECT_EQ(static_cast<size_t>(accepted), kMaxRequestBufferBytes);
  EXPECT_EQ(HttpTryWriteRequestBodyInternal(handle, upload_buf.data(), 16), 0);

  // Drain below low-water mark so space-available event fires.
  std::vector<uint8_t> drain(kMaxRequestBufferBytes);
  bool eof = false;
  size_t popped =
      op->PopRequestBytes(drain.data(), kMaxRequestBufferBytes, &eof);
  EXPECT_EQ(popped, kMaxRequestBufferBytes);
  EXPECT_FALSE(eof);

  // Commit response headers and push response bytes over high-water mark.
  EXPECT_EQ(HttpPollResponseHeadersInternal(handle), 0);
  op->CommitResponseHeaders(200, "OK", {{"X-A", "1"}, {"X-A", "2"}});
  EXPECT_EQ(HttpPollResponseHeadersInternal(handle), 200);
  EXPECT_EQ(HttpGetResponseHeadersInternal(handle), "OK\nX-A\t1\nX-A\t2\n");

  std::atomic<bool> resumed{false};
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    op->on_response_space_available = [&resumed]() { resumed.store(true); };
  }
  std::vector<uint8_t> big(kMaxResponseBufferBytes, 0x5A);
  EXPECT_TRUE(op->PushResponseBytes(big.data(), big.size()));

  std::vector<uint8_t> out(kMaxResponseBufferBytes);
  int32_t read_n = HttpTryReadResponseBodyInternal(
      handle, out.data(), static_cast<int32_t>(out.size()));
  EXPECT_EQ(static_cast<size_t>(read_n), kMaxResponseBufferBytes);
  EXPECT_TRUE(resumed.load());

  // Next read when empty returns WouldBlock until canceled.
  EXPECT_EQ(HttpTryReadResponseBodyInternal(handle, out.data(), 64),
            kHttpReadWouldBlock);
  HttpCancelRequestInternal(handle);
  EXPECT_EQ(HttpTryReadResponseBodyInternal(handle, out.data(), 64),
            kHttpReadErrorOrCanceled);

  HttpDestroyRequestInternal(handle);
  EXPECT_EQ(FindOperation(handle), nullptr);
}

TEST(HttpTransportCommonTest, MarkResponseCompleteWakesBlockedUploader) {
  int64_t handle = HttpCreateRequestInternal(
      "PUT", "http://127.0.0.1:80/early-413", "", "",
      static_cast<int64_t>(kMaxRequestBufferBytes * 2));
  ASSERT_GT(handle, 0);
  auto op = FindOperation(handle);
  ASSERT_NE(op, nullptr);

  // Fill request buffer past high-water mark so req_space_armed is set.
  std::vector<uint8_t> chunk(kMaxRequestBufferBytes + 1024, 0xCC);
  int32_t accepted = HttpTryWriteRequestBodyInternal(
      handle, chunk.data(), static_cast<int32_t>(chunk.size()));
  EXPECT_EQ(static_cast<size_t>(accepted), kMaxRequestBufferBytes);
  EXPECT_EQ(HttpTryWriteRequestBodyInternal(handle, chunk.data(), 64), 0);

  // Server completes an early clean response without draining the upload.
  op->CommitResponseHeaders(413, "Payload Too Large", {});
  op->MarkResponseComplete();

  // Completion queue must wake the armed uploader and subsequent writes fail
  // immediately instead of blocking forever.
  bool saw_req_space_event = false;
  for (int i = 0; i < 8; ++i) {
    int64_t ev = HttpWaitNextEventInternal(0);
    if (ev == 0) {
      break;
    }
    if ((ev >> 8) == handle && static_cast<HttpEventKind>(ev & 0xFF) ==
                                   kHttpEventRequestSpaceAvailable) {
      saw_req_space_event = true;
    }
  }
  EXPECT_TRUE(saw_req_space_event);
  EXPECT_EQ(HttpTryWriteRequestBodyInternal(handle, chunk.data(), 64), -1);

  HttpDestroyRequestInternal(handle);
}

TEST(HttpTransportCommonTest, StartOperationSingleStartFlag) {
  int64_t handle = HttpCreateRequestInternal("GET", "http://127.0.0.1:80/once",
                                             "", "", kNoRequestContent);
  ASSERT_GT(handle, 0);
  auto op = FindOperation(handle);
  ASSERT_NE(op, nullptr);

  // Cancel before calling StartOperation twice; second StartOperation must be
  // a no-op via the atomic `started` guard.
  HttpCancelRequestInternal(handle);
  EXPECT_FALSE(op->started.load());
  StartOperation(handle);
  EXPECT_TRUE(op->started.load());
  StartOperation(handle);
  EXPECT_TRUE(op->started.load());

  HttpDestroyRequestInternal(handle);
}

}  // namespace
}  // namespace http
}  // namespace firebase
