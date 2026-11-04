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
//  * Trust the operating system's certificate store so that root and
//    intermediate CAs revoked or distrusted by the OS vendor are rejected:
//      - Windows (CURL_USE_SCHANNEL): Windows Certificate Store via Crypt32,
//        with native OCSP/CRL and Microsoft CTL revocation checks.
//      - macOS (USE_APPLE_SECTRUST): System Keychain via Security.framework
//        (SecTrustEvaluateWithError), with Apple OCSP/CRL and Valid/trustd
//        revocation checks.
//      - Linux (OpenSSL/BoringSSL): the distribution's ca-certificates trust
//        store, resolved at *runtime* by LinuxCaBundlePath() and
//        LinuxCaDirectoryPath() rather than relying on the path libcurl
//        detected on the build machine.
//
// Returns the first non-CURLE_OK result from curl_easy_setopt(), or CURLE_OK.
CURLcode ApplyTlsPolicy(CURL* easy);

// Returns the first readable system CA bundle file on the current Linux host
// (probing standard Debian/Ubuntu, RHEL/Fedora, openSUSE, and Alpine paths),
// or nullptr if no bundle file is found or on non-Linux platforms.
const char* LinuxCaBundlePath();

// Returns FIREBASE_SSL_CAPATH ("/etc/ssl/certs") if it exists as a readable
// directory on the current Linux host, or nullptr otherwise.
const char* LinuxCaDirectoryPath();

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_TLS_POLICY_H_
