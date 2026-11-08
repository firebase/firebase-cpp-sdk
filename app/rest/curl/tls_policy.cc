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

#include "app/rest/curl/tls_policy.h"

#ifdef FIREBASE_SSL_CAPATH
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#endif  // FIREBASE_SSL_CAPATH

#include <string>

#include "app/rest/curl/curl_handles.h"

namespace firebase {
namespace rest {
namespace curl {

namespace {

#ifdef FIREBASE_SSL_CAPATH
// Reads an environment variable that influences which CAs are trusted. In
// AT_SECURE processes (setuid/setgid/file capabilities) the environment is
// attacker-controlled, so glibc's secure_getenv() returns nullptr there and the
// distribution defaults are used instead.
const char* TrustedGetEnv(const char* name) {
#if defined(__GLIBC__)
  return secure_getenv(name);
#else
  return std::getenv(name);
#endif  // defined(__GLIBC__)
}

bool IsReadableRegularFile(const char* path) {
  if (path == nullptr || path[0] == '\0') return false;
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode) && access(path, R_OK) == 0;
}

bool IsReadableDirectory(const char* path) {
  if (path == nullptr || path[0] == '\0') return false;
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode) &&
         access(path, R_OK | X_OK) == 0;
}
#endif  // FIREBASE_SSL_CAPATH

}  // namespace

CURLcode ApplyTlsPolicy(CURL* easy) {
  CURLcode first_error = CURLE_OK;
  // Verify the authenticity of the server's TLS certificate chain against the
  // trusted CA store (1L = enabled).
  ApplyOpt(easy, CURLOPT_SSL_VERIFYPEER, 1L, &first_error);
  // Verify that the server certificate's Subject Alternative Name (SAN) or
  // Common Name (CN) matches the target hostname (2L = strict hostname match;
  // 1L is deprecated in libcurl).
  ApplyOpt(easy, CURLOPT_SSL_VERIFYHOST, 2L, &first_error);
  // Require TLS 1.2 or newer (CURL_SSLVERSION_TLSv1_2 sets the minimum allowed
  // TLS protocol version in libcurl >= 7.54.0, permitting TLS 1.2 and TLS 1.3
  // while rejecting TLS 1.0 and 1.1).
  ApplyOpt(easy, CURLOPT_SSLVERSION,
           static_cast<long>(CURL_SSLVERSION_TLSv1_2),  // NOLINT
           &first_error);
  // The same policy applies to the TLS leg to an HTTPS proxy (https_proxy=
  // https://...). libcurl verifies that leg by default, but its CA location
  // and minimum version are configured separately.
  ApplyOpt(easy, CURLOPT_PROXY_SSL_VERIFYPEER, 1L, &first_error);
  ApplyOpt(easy, CURLOPT_PROXY_SSL_VERIFYHOST, 2L, &first_error);
  ApplyOpt(easy, CURLOPT_PROXY_SSLVERSION,
           static_cast<long>(CURL_SSLVERSION_TLSv1_2),  // NOLINT
           &first_error);
#ifdef FIREBASE_SSL_CAPATH
  // Explicitly setting CURLOPT_CAINFO (even to nullptr) marks
  // ssl.custom_cafile = TRUE inside libcurl, which stops it from substituting
  // the build machine's compile-time CURL_CA_BUNDLE path. See the comment on
  // LinuxCaBundlePath() for why that matters.
  ApplyOpt(easy, CURLOPT_CAINFO, LinuxCaBundlePath(), &first_error);
  ApplyOpt(easy, CURLOPT_PROXY_CAINFO, LinuxCaBundlePath(), &first_error);
  for (CURLoption option : {CURLOPT_CAPATH, CURLOPT_PROXY_CAPATH}) {
    CURLcode capath_code =
        curl_easy_setopt(easy, option, LinuxCaDirectoryPath());
    if (capath_code != CURLE_OK && capath_code != CURLE_NOT_BUILT_IN &&
        first_error == CURLE_OK) {
      first_error = capath_code;
    }
  }
#endif  // FIREBASE_SSL_CAPATH
  return first_error;
}

// Standard system CA bundle locations across major Linux distributions
// (matching Go's crypto/x509/root_linux.go and libcurl's CMakeLists.txt /
// acinclude.m4 CURL_CHECK_CA_BUNDLE detection):
// - /etc/ssl/certs/ca-certificates.crt: Debian, Ubuntu, Gentoo, Arch, Alpine
// - /etc/pki/tls/certs/ca-bundle.crt: Fedora, RHEL, CentOS, Rocky, Alma,
//   Amazon Linux
// - /etc/ssl/ca-bundle.pem: openSUSE, SLES
// - /etc/pki/tls/cacert.pem: OpenELEC
// - /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem: CentOS/RHEL/Fedora
//   p11-kit trust store
// - /etc/ssl/cert.pem: Alpine, Arch, Fedora, FreeBSD/OpenBSD symlink
//
// Why we probe at runtime instead of relying on libcurl's compile-time
// CURL_CA_BUNDLE:
// 1. At CMake configure time, libcurl detects the CA bundle path on the
//    *build* host (typically Ubuntu's /etc/ssl/certs/ca-certificates.crt) and
//    compiles that single path into CURL_CA_BUNDLE.
// 2. In libcurl's OpenSSL backend (ossl_populate_x509_store in
//    lib/vtls/openssl.c), ssl_cafile is loaded before ssl_capath (via
//    X509_STORE_load_file on OpenSSL 3.0+, or X509_STORE_load_locations on
//    OpenSSL < 3.0 and BoringSSL). If ssl_cafile points to a build-host
//    CURL_CA_BUNDLE file that does not exist on the runtime machine, libcurl
//    immediately aborts with CURLE_SSL_CACERT_BADFILE before ever loading
//    ssl_capath.
// 3. Conversely, some Linux distributions and container images only ship a
//    combined CA bundle file without populating OpenSSL c_rehash (<hash>.0)
//    symlinks in /etc/ssl/certs, so setting only CURLOPT_CAPATH is
//    insufficient.
// Explicitly setting CURLOPT_CAINFO to LinuxCaBundlePath() (even when it
// returns nullptr) marks ssl.custom_cafile = TRUE in libcurl
// (lib/vtls/vtls_config.c), preventing a missing build-host CURL_CA_BUNDLE
// path from breaking verification while loading both the runtime
// distribution's CA bundle and hashed CA directory when available.
const char* LinuxCaBundlePath() {
#ifdef FIREBASE_SSL_CAPATH
  // Probed once per process: the answer is a property of the host, this runs
  // on every REST request and WebSocket connect, and copying the getenv()
  // result avoids handing libcurl a pointer into the environment block.
  static const std::string* const kBundlePath = []() -> const std::string* {
    std::string path = internal::ResolveLinuxCaBundlePath();
    return path.empty() ? nullptr : new std::string(path);
  }();
  return kBundlePath != nullptr ? kBundlePath->c_str() : nullptr;
#else
  return nullptr;
#endif  // FIREBASE_SSL_CAPATH
}

const char* LinuxCaDirectoryPath() {
#ifdef FIREBASE_SSL_CAPATH
  static const std::string* const kDirectoryPath = []() -> const std::string* {
    std::string path = internal::ResolveLinuxCaDirectoryPath();
    return path.empty() ? nullptr : new std::string(path);
  }();
  return kDirectoryPath != nullptr ? kDirectoryPath->c_str() : nullptr;
#else
  return nullptr;
#endif  // FIREBASE_SSL_CAPATH
}

namespace internal {

std::string ResolveLinuxCaBundlePath() {
#ifdef FIREBASE_SSL_CAPATH
  static const char* const kCaBundleEnvVars[] = {
      "CURL_CA_BUNDLE",
      "SSL_CERT_FILE",
  };
  for (const char* env_var : kCaBundleEnvVars) {
    const char* env_path = TrustedGetEnv(env_var);
    if (IsReadableRegularFile(env_path)) {
      return env_path;
    }
  }
  static const char* const kCaBundlePaths[] = {
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/etc/ssl/ca-bundle.pem",
      "/etc/pki/tls/cacert.pem",
      "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
      "/etc/ssl/cert.pem",
  };
  for (const char* path : kCaBundlePaths) {
    if (IsReadableRegularFile(path)) {
      return path;
    }
  }
#endif  // FIREBASE_SSL_CAPATH
  return std::string();
}

std::string ResolveLinuxCaDirectoryPath() {
#ifdef FIREBASE_SSL_CAPATH
  const char* env_dir = TrustedGetEnv("SSL_CERT_DIR");
  if (IsReadableDirectory(env_dir)) {
    return env_dir;
  }
  if (IsReadableDirectory(FIREBASE_SSL_CAPATH)) {
    return FIREBASE_SSL_CAPATH;
  }
#endif  // FIREBASE_SSL_CAPATH
  return std::string();
}

}  // namespace internal

}  // namespace curl
}  // namespace rest
}  // namespace firebase
