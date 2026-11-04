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
#endif  // FIREBASE_SSL_CAPATH

namespace firebase {
namespace rest {
namespace curl {

namespace {

// Applies a single option and folds the result into *first_error, keeping only
// the first failure so the caller sees the root cause.
template <typename T>
void ApplyOpt(CURL* easy, CURLoption option, T value, CURLcode* first_error) {
  CURLcode code = curl_easy_setopt(easy, option, value);
  if (code != CURLE_OK && *first_error == CURLE_OK) *first_error = code;
}

}  // namespace

CURLcode ApplyTlsPolicy(CURL* easy) {
  CURLcode first_error = CURLE_OK;
  ApplyOpt(easy, CURLOPT_SSL_VERIFYPEER, 1L, &first_error);
  ApplyOpt(easy, CURLOPT_SSL_VERIFYHOST, 2L, &first_error);
  ApplyOpt(easy, CURLOPT_SSLVERSION,
           static_cast<long>(CURL_SSLVERSION_TLSv1_2),  // NOLINT
           &first_error);
#ifdef FIREBASE_SSL_CAPATH
  // Explicitly setting CURLOPT_CAINFO (even to nullptr) marks
  // ssl.custom_cafile = TRUE inside libcurl, which stops it from substituting
  // the build machine's compile-time CURL_CA_BUNDLE path. See the comment on
  // LinuxCaBundlePath() for why that matters.
  ApplyOpt(easy, CURLOPT_CAINFO, LinuxCaBundlePath(), &first_error);
  ApplyOpt(easy, CURLOPT_CAPATH, LinuxCaDirectoryPath(), &first_error);
#endif  // FIREBASE_SSL_CAPATH
  return first_error;
}

// Standard system CA bundle locations across major Linux distributions:
// - /etc/ssl/certs/ca-certificates.crt: Debian, Ubuntu, Gentoo, Arch, Alpine
// - /etc/pki/tls/certs/ca-bundle.crt: Fedora, RHEL, CentOS, Rocky, Alma,
//   Amazon Linux
// - /etc/ssl/ca-bundle.pem: openSUSE, SLES
// - /etc/pki/tls/cacert.pem: OpenELEC
// - /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem: CentOS/RHEL p11-kit
//   trust store
// - /etc/ssl/cert.pem: Alpine, Arch, Fedora/FreeBSD-style symlink
//
// Why we probe at runtime instead of relying on libcurl's compile-time
// CURL_CA_BUNDLE:
// 1. At CMake configure time, libcurl detects the CA bundle path on the
//    *build* host (typically Ubuntu's /etc/ssl/certs/ca-certificates.crt) and
//    compiles that single path into CURL_CA_BUNDLE.
// 2. When linked against BoringSSL (or OpenSSL < 3.0), libcurl calls
//    X509_STORE_load_locations(store, ssl_cafile, ssl_capath). BoringSSL's
//    X509_STORE_load_locations checks ssl_cafile first and immediately returns
//    0 (CURLE_SSL_CACERT_BADFILE) if that file does not exist on the runtime
//    machine, aborting before it ever loads ssl_capath.
// 3. Conversely, some Linux distributions and container images only ship a
//    combined CA bundle file without populating OpenSSL c_rehash (<hash>.0)
//    symlinks in /etc/ssl/certs, so setting only CURLOPT_CAPATH is
//    insufficient.
// Explicitly setting CURLOPT_CAINFO to LinuxCaBundlePath() (even when it
// returns nullptr) marks ssl.custom_cafile = TRUE in libcurl, preventing a
// missing build-host CURL_CA_BUNDLE path from breaking verification while
// loading both the runtime distribution's CA bundle and hashed CA directory
// when available.
const char* LinuxCaBundlePath() {
#ifdef FIREBASE_SSL_CAPATH
  static const char* const kCaBundlePaths[] = {
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/etc/ssl/ca-bundle.pem",
      "/etc/pki/tls/cacert.pem",
      "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
      "/etc/ssl/cert.pem",
  };
  for (const char* path : kCaBundlePaths) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
        access(path, R_OK) == 0) {
      return path;
    }
  }
#endif  // FIREBASE_SSL_CAPATH
  return nullptr;
}

const char* LinuxCaDirectoryPath() {
#ifdef FIREBASE_SSL_CAPATH
  struct stat st;
  if (stat(FIREBASE_SSL_CAPATH, &st) == 0 && S_ISDIR(st.st_mode) &&
      access(FIREBASE_SSL_CAPATH, R_OK | X_OK) == 0) {
    return FIREBASE_SSL_CAPATH;
  }
#endif  // FIREBASE_SSL_CAPATH
  return nullptr;
}

}  // namespace curl
}  // namespace rest
}  // namespace firebase
