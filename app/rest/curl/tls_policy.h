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

#ifndef FIREBASE_APP_REST_CURL_TLS_POLICY_H_
#define FIREBASE_APP_REST_CURL_TLS_POLICY_H_

#include <string>

#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace curl {

// Applies the SDK-wide TLS policy to a libcurl easy handle. This is the single
// place where transport security is configured for every HTTPS and WSS
// connection the SDK opens (REST transport and Realtime Database WebSockets).
//
// The policy is:
//  * Always verify the peer certificate chain (CURLOPT_SSL_VERIFYPEER=1) and
//    that the certificate matches the requested hostname/SANs
//    (CURLOPT_SSL_VERIFYHOST=2).
//  * Require TLS 1.2 or newer (CURLOPT_SSLVERSION=TLSv1_2). TLS 1.3 is still
//    negotiated when available; TLS 1.0/1.1 downgrades are refused.
//  * Take trusted CAs from the operating system:
//      - Windows (CURL_USE_SCHANNEL): the Windows certificate store.
//      - macOS (USE_APPLE_SECTRUST): the system keychain via
//      Security.framework.
//      - Linux (OpenSSL/BoringSSL): the distribution's ca-certificates store,
//        resolved at *runtime* by LinuxCaBundlePath() and
//        LinuxCaDirectoryPath() rather than the path libcurl detected on the
//        build machine.
//    Revocation checking is whatever the TLS backend does by default; the SDK
//    does not configure it.
//  * The same verification, version floor and CA location are applied to the
//    TLS leg to an HTTPS proxy (CURLOPT_PROXY_*), if one is configured through
//    libcurl's proxy environment variables.
//
// Returns the first non-CURLE_OK result from curl_easy_setopt(), or CURLE_OK.
CURLcode ApplyTlsPolicy(CURL* easy);

// Returns the CA bundle file to use on the current Linux host, or nullptr if
// none is found or on non-Linux platforms. In order of precedence: the
// CURL_CA_BUNDLE and SSL_CERT_FILE environment variables (the same variables
// the curl command-line tool honors; libcurl itself does not read them), then
// the standard Debian/Ubuntu, RHEL/Fedora, openSUSE and Alpine bundle paths.
// Only readable regular files are accepted. The environment variables are
// ignored in AT_SECURE (setuid/setgid) processes.
//
// The result is computed once per process on first use and cached, so the
// environment variables must be set before the first Firebase network request.
const char* LinuxCaBundlePath();

// Returns the CA directory to use on the current Linux host, or nullptr if none
// is found or on non-Linux platforms: the SSL_CERT_DIR environment variable if
// it names a readable directory, otherwise FIREBASE_SSL_CAPATH
// ("/etc/ssl/certs") if that exists. Cached like LinuxCaBundlePath().
const char* LinuxCaDirectoryPath();

namespace internal {

// Uncached probes behind LinuxCaBundlePath() / LinuxCaDirectoryPath(), exposed
// so tests can exercise the environment-variable precedence. Return an empty
// string when nothing suitable is found or on non-Linux platforms.
std::string ResolveLinuxCaBundlePath();
std::string ResolveLinuxCaDirectoryPath();

}  // namespace internal

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_TLS_POLICY_H_
