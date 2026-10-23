// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/connection/web_socket_hub.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <memory>
#include <string>

#include "app/src/logger.h"
#include "gtest/gtest.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {
namespace {

TEST(GetHostFromWebSocketUri, ExtractsHostFromFirebaseConnectionUrl) {
  EXPECT_EQ("test.firebaseio.com",
            GetHostFromWebSocketUri(
                "wss://test.firebaseio.com/.ws?ns=test&v=5&ls=ABC"));
  EXPECT_EQ("s-usc1c-nss-123.firebaseio.com",
            GetHostFromWebSocketUri(
                "wss://s-usc1c-nss-123.firebaseio.com/.ws?ns=test&v=5"));
}

TEST(GetHostFromWebSocketUri, StripsPortPathQueryAndFragment) {
  EXPECT_EQ("localhost", GetHostFromWebSocketUri("ws://localhost:9000"));
  EXPECT_EQ("localhost", GetHostFromWebSocketUri("ws://localhost:9000/"));
  EXPECT_EQ("example.com",
            GetHostFromWebSocketUri("wss://example.com?query=1#frag"));
  EXPECT_EQ("example.com", GetHostFromWebSocketUri("wss://example.com#frag"));
}

TEST(GetHostFromWebSocketUri, StripsUserInfo) {
  EXPECT_EQ("example.com",
            GetHostFromWebSocketUri("wss://user:p%40ss@example.com:443/x"));
}

TEST(GetHostFromWebSocketUri, HandlesIpLiterals) {
  EXPECT_EQ("127.0.0.1", GetHostFromWebSocketUri("ws://127.0.0.1:9000/.ws"));
  EXPECT_EQ("::1", GetHostFromWebSocketUri("wss://[::1]:9000/.ws"));
  EXPECT_EQ("fe80::1", GetHostFromWebSocketUri("wss://[fe80::1]/"));
}

TEST(GetHostFromWebSocketUri, ReturnsEmptyWhenThereIsNoHost) {
  EXPECT_EQ("", GetHostFromWebSocketUri(""));
  EXPECT_EQ("", GetHostFromWebSocketUri("wss://"));
  EXPECT_EQ("", GetHostFromWebSocketUri("wss:///.ws?ns=test"));
  EXPECT_EQ("", GetHostFromWebSocketUri("wss://[::1/"));
}

// ---------------------------------------------------------------------------
// SSL_CTX configuration.

TEST(WebSocketHub, RequiresPeerVerification) {
  SystemLogger system_logger;
  Logger logger(&system_logger);
  WebSocketHub hub("test.firebaseio.com", &logger);
  SSL_CTX* context = hub.client_ssl_context();
  ASSERT_NE(nullptr, context);

  EXPECT_EQ(SSL_VERIFY_PEER, SSL_CTX_get_verify_mode(context));
  EXPECT_NE(nullptr, SSL_CTX_get_verify_callback(context));
  EXPECT_EQ(TLS1_2_VERSION, SSL_CTX_get_min_proto_version(context));
}

TEST(WebSocketHub, LoadsAtLeastOneTrustedRoot) {
  SystemLogger system_logger;
  Logger logger(&system_logger);
  WebSocketHub hub("test.firebaseio.com", &logger);
  X509_STORE* store = SSL_CTX_get_cert_store(hub.client_ssl_context());
  ASSERT_NE(nullptr, store);
  // Every supported desktop OS ships a non-empty root store; if this fails,
  // all wss:// connections would be rejected.
  STACK_OF(X509_OBJECT)* objects = X509_STORE_get0_objects(store);
  ASSERT_NE(nullptr, objects);
  EXPECT_GT(static_cast<size_t>(sk_X509_OBJECT_num(objects)), 0u);
}

TEST(WebSocketHub, SharesOneTrustStoreBetweenHubs) {
  // Loading the OS trust store is slow and a hub is created per connection,
  // so all hubs must share a single store rather than each loading its own.
  SystemLogger system_logger;
  Logger logger(&system_logger);
  WebSocketHub first("a.firebaseio.com", &logger);
  WebSocketHub second("b.firebaseio.com", &logger);
  EXPECT_EQ(SSL_CTX_get_cert_store(first.client_ssl_context()),
            SSL_CTX_get_cert_store(second.client_ssl_context()));
}

TEST(WebSocketHub, ToleratesEmptyHost) {
  // A hub with no host must still be constructible (plaintext ws:// clients
  // never touch the SSL_CTX); its secure connections fail closed instead,
  // which is covered by the handshake tests below.
  SystemLogger system_logger;
  Logger logger(&system_logger);
  WebSocketHub hub("", &logger);
  EXPECT_EQ(SSL_VERIFY_PEER, SSL_CTX_get_verify_mode(hub.client_ssl_context()));
}

// ---------------------------------------------------------------------------
// End-to-end TLS handshakes against a server using a certificate generated at
// test time, run in-process over a BIO pair so no network is involved. These
// are the tests that prove a bad certificate cannot complete a handshake.

struct EvpPkeyDeleter { void operator()(EVP_PKEY* p) const { EVP_PKEY_free(p); } };
struct X509Deleter { void operator()(X509* x) const { X509_free(x); } };
struct X509StoreDeleter {
  void operator()(X509_STORE* s) const { X509_STORE_free(s); }
};
struct SslCtxDeleter { void operator()(SSL_CTX* c) const { SSL_CTX_free(c); } };
struct SslDeleter { void operator()(SSL* s) const { SSL_free(s); } };

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using X509StorePtr = std::unique_ptr<X509_STORE, X509StoreDeleter>;
using SslCtxPtr = std::unique_ptr<SSL_CTX, SslCtxDeleter>;
using SslPtr = std::unique_ptr<SSL, SslDeleter>;

EvpPkeyPtr GenerateKey() {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
  EVP_PKEY* key = nullptr;
  if (ctx != nullptr && EVP_PKEY_keygen_init(ctx) == 1 &&
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) == 1) {
    EVP_PKEY_keygen(ctx, &key);
  }
  EVP_PKEY_CTX_free(ctx);
  return EvpPkeyPtr(key);
}

bool AddExtension(X509* certificate, int nid, const char* value) {
  X509_EXTENSION* extension =
      X509V3_EXT_nconf_nid(nullptr, nullptr, nid, const_cast<char*>(value));
  if (extension == nullptr) return false;
  const bool added = X509_add_ext(certificate, extension, -1) == 1;
  X509_EXTENSION_free(extension);
  return added;
}

// Creates a self-signed certificate (so it can serve as its own trust anchor)
// with the given subject common name and subjectAltName extension value.
X509Ptr MakeSelfSignedCertificate(EVP_PKEY* key, const char* common_name,
                                  const char* subject_alt_name) {
  X509Ptr certificate(X509_new());
  if (!certificate) return nullptr;
  X509* x = certificate.get();
  X509_set_version(x, 2);  // X.509 v3.
  ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
  X509_gmtime_adj(X509_getm_notBefore(x), -60);
  X509_gmtime_adj(X509_getm_notAfter(x), 60 * 60);
  X509_NAME* name = X509_get_subject_name(x);
  X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_ASC,
      reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0);
  X509_set_issuer_name(x, name);
  X509_set_pubkey(x, key);
  if (!AddExtension(x, NID_basic_constraints, "critical,CA:TRUE") ||
      !AddExtension(x, NID_key_usage,
                    "critical,digitalSignature,keyCertSign") ||
      !AddExtension(x, NID_subject_alt_name, subject_alt_name) ||
      X509_sign(x, key, EVP_sha256()) == 0) {
    return nullptr;
  }
  return certificate;
}

// Runs a TLS handshake between `client` and `server` over an in-memory BIO
// pair. Returns true if both sides completed the handshake.
bool Handshake(SSL* client, SSL* server) {
  BIO* client_bio = nullptr;
  BIO* server_bio = nullptr;
  if (BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1) return false;
  SSL_set_bio(client, client_bio, client_bio);  // SSL takes ownership.
  SSL_set_bio(server, server_bio, server_bio);
  SSL_set_connect_state(client);
  SSL_set_accept_state(server);

  bool client_done = false;
  bool server_done = false;
  for (int i = 0; i < 100 && !(client_done && server_done); ++i) {
    bool progressed = false;
    for (SSL* ssl : {client, server}) {
      bool& done = (ssl == client) ? client_done : server_done;
      if (done) continue;
      const int result = SSL_do_handshake(ssl);
      if (result == 1) {
        done = progressed = true;
        continue;
      }
      const int error = SSL_get_error(ssl, result);
      if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
        return false;  // Handshake failed on this side.
      }
      progressed = true;
    }
    if (!progressed) break;
  }
  return client_done && server_done;
}

class WebSocketHubHandshakeTest : public ::testing::Test {
 protected:
  static constexpr char kHost[] = "test.firebaseio.com";

  void SetUp() override {
    key_ = GenerateKey();
    ASSERT_TRUE(key_);
    certificate_ = MakeSelfSignedCertificate(key_.get(), kHost,
                                             "DNS:test.firebaseio.com");
    ASSERT_TRUE(certificate_);
    server_context_.reset(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(server_context_);
    ASSERT_EQ(1, SSL_CTX_use_certificate(server_context_.get(),
                                         certificate_.get()));
    ASSERT_EQ(1, SSL_CTX_use_PrivateKey(server_context_.get(), key_.get()));
  }

  // Connects a client created from `hub`'s SSL_CTX to the test server. If
  // `trust_server_certificate` is set, the test certificate is trusted for this
  // connection only (the hub's shared OS trust store is never modified).
  // Returns the client's certificate verification result, or -1 if the
  // handshake failed before verification produced one.
  long Connect(const WebSocketHub& hub, bool trust_server_certificate,
               bool* handshake_ok) {
    SslPtr client(SSL_new(hub.client_ssl_context()));
    SslPtr server(SSL_new(server_context_.get()));
    EXPECT_TRUE(client && server);
    if (trust_server_certificate) {
      X509StorePtr store(X509_STORE_new());
      X509_STORE_add_cert(store.get(), certificate_.get());
      EXPECT_EQ(1, SSL_set1_verify_cert_store(client.get(), store.get()));
    }
    *handshake_ok = Handshake(client.get(), server.get());
    return SSL_get_verify_result(client.get());
  }

  SystemLogger system_logger_;
  Logger logger_{&system_logger_};
  EvpPkeyPtr key_;
  X509Ptr certificate_;
  SslCtxPtr server_context_;
};

constexpr char WebSocketHubHandshakeTest::kHost[];

TEST_F(WebSocketHubHandshakeTest, AcceptsTrustedCertificateForMatchingHost) {
  WebSocketHub hub(kHost, &logger_);
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/true,
                              &handshake_ok);
  EXPECT_TRUE(handshake_ok);
  EXPECT_EQ(X509_V_OK, result);
}

TEST_F(WebSocketHubHandshakeTest, RejectsUntrustedCertificate) {
  WebSocketHub hub(kHost, &logger_);
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/false,
                              &handshake_ok);
  EXPECT_FALSE(handshake_ok);
  EXPECT_EQ(X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT, result);
}

TEST_F(WebSocketHubHandshakeTest, RejectsCertificateForDifferentHost) {
  WebSocketHub hub("other.firebaseio.com", &logger_);
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/true,
                              &handshake_ok);
  EXPECT_FALSE(handshake_ok);
  EXPECT_EQ(X509_V_ERR_HOSTNAME_MISMATCH, result);
}

TEST_F(WebSocketHubHandshakeTest, RejectsWildcardSpanningMultipleLabels) {
  // "*.firebaseio.com" must not match a two-level subdomain.
  WebSocketHub hub("a.b.firebaseio.com", &logger_);
  certificate_ = MakeSelfSignedCertificate(key_.get(), "ignored",
                                           "DNS:*.firebaseio.com");
  ASSERT_TRUE(certificate_);
  ASSERT_EQ(1, SSL_CTX_use_certificate(server_context_.get(),
                                       certificate_.get()));
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/true,
                              &handshake_ok);
  EXPECT_FALSE(handshake_ok);
  EXPECT_EQ(X509_V_ERR_HOSTNAME_MISMATCH, result);
}

TEST_F(WebSocketHubHandshakeTest, IgnoresSubjectCommonName) {
  // A certificate whose CN matches but whose subjectAltName does not must be
  // rejected: only subjectAltName identifies the host.
  WebSocketHub hub(kHost, &logger_);
  certificate_ = MakeSelfSignedCertificate(key_.get(), kHost,
                                           "DNS:other.firebaseio.com");
  ASSERT_TRUE(certificate_);
  ASSERT_EQ(1, SSL_CTX_use_certificate(server_context_.get(),
                                       certificate_.get()));
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/true,
                              &handshake_ok);
  EXPECT_FALSE(handshake_ok);
  EXPECT_EQ(X509_V_ERR_HOSTNAME_MISMATCH, result);
}

TEST_F(WebSocketHubHandshakeTest, AcceptsIpAddressInSubjectAltName) {
  WebSocketHub hub("127.0.0.1", &logger_);
  certificate_ = MakeSelfSignedCertificate(key_.get(), "localhost",
                                           "IP:127.0.0.1");
  ASSERT_TRUE(certificate_);
  ASSERT_EQ(1, SSL_CTX_use_certificate(server_context_.get(),
                                       certificate_.get()));
  bool handshake_ok = false;
  const long result = Connect(hub, /*trust_server_certificate=*/true,
                              &handshake_ok);
  EXPECT_TRUE(handshake_ok);
  EXPECT_EQ(X509_V_OK, result);
}

TEST_F(WebSocketHubHandshakeTest, RejectsEverythingWhenHostIsUnknown) {
  // Even a trusted, otherwise valid certificate is rejected when the hub does
  // not know which host it is talking to.
  WebSocketHub hub("", &logger_);
  bool handshake_ok = false;
  Connect(hub, /*trust_server_certificate=*/true, &handshake_ok);
  EXPECT_FALSE(handshake_ok);
}

}  // namespace
}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
