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

// The platform trust-store headers must come after uWS.h (included by
// web_socket_hub.h) so that OpenSSL's headers are parsed before <wincrypt.h>
// defines macros that collide with OpenSSL type names.
#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#undef X509_NAME
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif  // defined(_WIN32)

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <string>

#include "app/src/assert.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

namespace {

// Index into the SSL_CTX's application data where the owning WebSocketHub is
// stored, so that VerifyCallback can reach the Hub's host and logger.
int HubExDataIndex() {
  static const int index =
      SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  return index;
}

WebSocketHub* GetHubFromStoreContext(X509_STORE_CTX* store_ctx) {
  SSL* ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(
      store_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
  if (ssl == nullptr) return nullptr;
  return static_cast<WebSocketHub*>(
      SSL_CTX_get_ex_data(SSL_get_SSL_CTX(ssl), HubExDataIndex()));
}

// Invoked by the TLS library for every certificate in the server's chain with
// the library's own verdict in `preverify_ok`. The verdict is never upgraded;
// this callback only adds diagnostics and enforces that a host is known.
int VerifyCallback(int preverify_ok, X509_STORE_CTX* store_ctx) {
  WebSocketHub* hub = GetHubFromStoreContext(store_ctx);
  const Logger* logger = hub != nullptr ? hub->logger() : nullptr;

  if (hub != nullptr && hub->host().empty()) {
    // Without a host to compare against there is no way to authenticate the
    // server, so fail closed rather than accept any valid-looking chain.
    if (logger != nullptr) {
      logger->LogError(
          "Rejected TLS certificate: connection URI has no host to verify");
    }
    return 0;
  }

  if (!preverify_ok && logger != nullptr) {
    const int error = X509_STORE_CTX_get_error(store_ctx);
    logger->LogError(
        "Rejected TLS certificate for %s: %s (certificate depth %d)",
        hub->host().c_str(), X509_verify_cert_error_string(error),
        X509_STORE_CTX_get_error_depth(store_ctx));
  }
  return preverify_ok;
}

// Adds a DER-encoded certificate to `store`. Duplicates are ignored.
void AddDerCertificate(X509_STORE* store, const unsigned char* der,
                       long length) {
  X509* certificate = d2i_X509(nullptr, &der, length);
  if (certificate == nullptr) return;
  X509_STORE_add_cert(store, certificate);
  X509_free(certificate);
}

#if defined(_WIN32)

// Copies the roots trusted by Windows into `store`. The "ROOT" logical store
// includes the machine, group-policy and enterprise root stores, so roots
// installed by an administrator (for example for a TLS-inspecting proxy) are
// honoured.
//
// Known limitation: Windows downloads some roots on demand (Automatic Root
// Certificates Update) only when its own chain builder first needs them, which
// this code does not trigger. Such roots are present only once another
// application has validated a chain through them. Google's serving chains
// cross-sign to the GlobalSign Root CA, which ships in every Windows image, so
// Firebase hosts verify regardless.
void AddSystemRoots(X509_STORE* store) {
  HCERTSTORE system_store = CertOpenSystemStoreA(0, "ROOT");
  if (system_store == nullptr) return;
  PCCERT_CONTEXT context = nullptr;
  while ((context = CertEnumCertificatesInStore(system_store, context)) !=
         nullptr) {
    AddDerCertificate(store, context->pbCertEncoded,
                      static_cast<long>(context->cbCertEncoded));
  }
  CertCloseStore(system_store, 0);
}

#elif defined(__APPLE__)

SecCertificateRef CertificateAt(CFArrayRef array, CFIndex index) {
  return static_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(array, index)));
}

void AddCertificate(X509_STORE* store, SecCertificateRef certificate) {
  CFDataRef der = SecCertificateCopyData(certificate);
  if (der == nullptr) return;
  AddDerCertificate(store, CFDataGetBytePtr(der),
                    static_cast<long>(CFDataGetLength(der)));
  CFRelease(der);
}

// Returns whether the trust settings for `certificate` in `domain` make it a
// trusted root. Follows the Security framework's documented semantics: an
// empty settings array means "trust as root"; otherwise at least one usage
// constraint must have a TrustRoot / TrustAsRoot result and none may Deny.
bool IsTrustedRootInDomain(SecCertificateRef certificate,
                           SecTrustSettingsDomain domain) {
  CFArrayRef settings = nullptr;
  if (SecTrustSettingsCopyTrustSettings(certificate, domain, &settings) !=
          errSecSuccess ||
      settings == nullptr) {
    return false;
  }
  const CFIndex count = CFArrayGetCount(settings);
  bool trusted = (count == 0);
  bool denied = false;
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef constraint = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(settings, i));
    if (CFGetTypeID(constraint) != CFDictionaryGetTypeID()) continue;
    CFNumberRef result_number = static_cast<CFNumberRef>(
        CFDictionaryGetValue(constraint, kSecTrustSettingsResult));
    // A constraint without a result uses the default, TrustRoot.
    SInt32 result = kSecTrustSettingsResultTrustRoot;
    if (result_number != nullptr &&
        (CFGetTypeID(result_number) != CFNumberGetTypeID() ||
         !CFNumberGetValue(result_number, kCFNumberSInt32Type, &result))) {
      continue;
    }
    if (result == kSecTrustSettingsResultTrustRoot ||
        result == kSecTrustSettingsResultTrustAsRoot) {
      trusted = true;
    } else if (result == kSecTrustSettingsResultDeny) {
      denied = true;
    }
  }
  CFRelease(settings);
  return trusted && !denied;
}

// Adds the certificates that have been explicitly trusted as roots in `domain`
// (for example by `security add-trusted-cert` or a configuration profile).
void AddTrustSettingsRoots(X509_STORE* store, SecTrustSettingsDomain domain) {
  CFArrayRef certificates = nullptr;
  // Returns errSecNoTrustSettings when the domain has none.
  if (SecTrustSettingsCopyCertificates(domain, &certificates) !=
          errSecSuccess ||
      certificates == nullptr) {
    return;
  }
  const CFIndex count = CFArrayGetCount(certificates);
  for (CFIndex i = 0; i < count; ++i) {
    SecCertificateRef certificate = CertificateAt(certificates, i);
    if (IsTrustedRootInDomain(certificate, domain)) {
      AddCertificate(store, certificate);
    }
  }
  CFRelease(certificates);
}

// Copies the roots trusted by macOS into `store`: Apple's built-in system
// roots plus any roots the administrator or user has explicitly trusted.
void AddSystemRoots(X509_STORE* store) {
  CFArrayRef anchors = nullptr;
  if (SecTrustCopyAnchorCertificates(&anchors) == errSecSuccess &&
      anchors != nullptr) {
    const CFIndex count = CFArrayGetCount(anchors);
    for (CFIndex i = 0; i < count; ++i) {
      AddCertificate(store, CertificateAt(anchors, i));
    }
    CFRelease(anchors);
  }
  AddTrustSettingsRoots(store, kSecTrustSettingsDomainAdmin);
  AddTrustSettingsRoots(store, kSecTrustSettingsDomainUser);
}

#else  // Linux and other POSIX systems.

// Loads the distribution's CA bundle into `store`. The first bundle that can be
// read wins; the hashed certificate directory is added as well so that
// locally installed roots are honoured.
void AddSystemRoots(X509_STORE* store) {
  static const char* const kBundlePaths[] = {
      "/etc/ssl/certs/ca-certificates.crt",  // Debian, Ubuntu, Arch, Alpine
      "/etc/pki/tls/certs/ca-bundle.crt",    // Fedora, RHEL, CentOS
      "/etc/ssl/ca-bundle.pem",              // openSUSE
      "/etc/pki/tls/cacert.pem",             // OpenELEC
      "/etc/ssl/cert.pem",                   // Alpine, FreeBSD, OpenBSD
  };
  for (const char* path : kBundlePaths) {
    if (X509_STORE_load_locations(store, path, nullptr) == 1) break;
  }
  X509_STORE_load_locations(store, nullptr, "/etc/ssl/certs");
}

#endif  // defined(_WIN32)

// Returns the process-wide store of trusted root certificates, built on first
// use. Reading the operating system's trust store costs tens of milliseconds,
// and a WebSocketHub is created for every (re)connection, so the roots are
// loaded once and shared: X509_STORE is reference counted and safe to share
// between SSL_CTX objects and threads. The store is intentionally never freed.
X509_STORE* SystemTrustStore() {
  static X509_STORE* const store = [] {
    X509_STORE* new_store = X509_STORE_new();
    // Default paths honour the SSL_CERT_FILE and SSL_CERT_DIR environment
    // variables and, when building against the system OpenSSL, its configured
    // certificate directory.
    X509_STORE_set_default_paths(new_store);
    AddSystemRoots(new_store);
    // Loading roots can leave benign errors (missing optional bundles,
    // duplicate certificates) on the thread's error queue; don't let them be
    // mistaken for handshake errors later.
    ERR_clear_error();
    return new_store;
  }();
  return store;
}

}  // namespace

std::string GetHostFromWebSocketUri(const std::string& uri) {
  // scheme://[userinfo@]host[:port][/path][?query][#fragment]
  // The scheme separator is only recognised if it precedes the first path,
  // query or fragment delimiter.
  std::string::size_type start = uri.find("://");
  if (start != std::string::npos &&
      uri.find_first_of("/?#") < start) {
    start = std::string::npos;
  }
  start = (start == std::string::npos) ? 0 : start + 3;
  const std::string::size_type end = uri.find_first_of("/?#", start);
  std::string authority = uri.substr(
      start, end == std::string::npos ? std::string::npos : end - start);

  const std::string::size_type at = authority.rfind('@');
  if (at != std::string::npos) authority.erase(0, at + 1);

  if (!authority.empty() && authority[0] == '[') {
    // IPv6 literal, e.g. "[::1]:9000".
    const std::string::size_type close = authority.find(']');
    return close == std::string::npos ? std::string()
                                      : authority.substr(1, close - 1);
  }

  const std::string::size_type colon = authority.find(':');
  return colon == std::string::npos ? authority : authority.substr(0, colon);
}

WebSocketHub::WebSocketHub(const std::string& host, Logger* logger)
    : uWS::Hub(), host_(host), logger_(logger) {
  // `nodeData` is the protected uS::Node member holding the client SSL_CTX
  // that uWS::Hub::connect() uses for every wss:// connection.
  SSL_CTX* context = nodeData->clientContext;

  // VerifyCallback relies on finding this hub through the context; without it
  // the fail-closed rule for an unknown host could not be enforced.
  FIREBASE_ASSERT(HubExDataIndex() >= 0);
  SSL_CTX_set_ex_data(context, HubExDataIndex(), this);

  // uWebSockets only disables SSLv3; require TLS 1.2 or newer.
  SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);

  // Require a chain that validates against the trusted roots.
  SSL_CTX_set_verify(context, SSL_VERIFY_PEER, VerifyCallback);
  X509_STORE* trust_store = SystemTrustStore();
  X509_STORE_up_ref(trust_store);  // SSL_CTX_set_cert_store takes ownership.
  SSL_CTX_set_cert_store(context, trust_store);

  // Require the leaf certificate to be issued for `host`. These parameters are
  // inherited by every SSL object created from the context. As in browsers,
  // only subjectAltName is consulted (never the subject CN) and wildcards may
  // only replace a whole label.
  X509_VERIFY_PARAM* param = SSL_CTX_get0_param(context);
  X509_VERIFY_PARAM_set_hostflags(param,
                                  X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS |
                                      X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
  if (!host_.empty()) {
    // An IP literal must be matched against the iPAddress subjectAltName;
    // set1_ip_asc fails (without side effects) for anything that is not an IP.
    if (!X509_VERIFY_PARAM_set1_ip_asc(param, host_.c_str())) {
      X509_VERIFY_PARAM_set1_host(param, host_.c_str(), host_.size());
    }
  } else if (logger_ != nullptr) {
    logger_->LogWarning(
        "Could not determine the host to verify TLS certificates against; "
        "secure connections from this client will be rejected");
  }
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
