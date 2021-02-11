// Copyright 2017 Google LLC
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

#include <assert.h>

#include <cstddef>
#include <map>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/include/firebase/version.h"
#include "app/src/reference_counted_future_impl.h"
#include "dynamic_links/src/common.h"
#include "dynamic_links/src/include/firebase/dynamic_links.h"
#include "dynamic_links/src/include/firebase/dynamic_links/components.h"

namespace firebase {
namespace dynamic_links {

DEFINE_FIREBASE_VERSION_STRING(FirebaseDynamicLinks);

static const char* kLinkShorteningNotSupported =
    "Link shortening is not supported on desktop.";

bool g_initialized = false;

InitResult Initialize(const App& app, Listener* listener) {
  if (!g_initialized) {
    g_initialized = CreateReceiver(app);
    assert(g_initialized);
    FutureData::Create();
    SetListener(listener);
  }
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_initialized; }

}  // namespace internal

void Terminate() {
  if (!g_initialized) return;
  DestroyReceiver();
  FutureData::Destroy();
  g_initialized = false;
}

// Store a value (as a string) associated with the string key in the specified
// output_map.
template <typename T>
void StoreKeyValuePairAsStringInMap(
    std::map<std::string, std::string>* output_map, const char* key,
    const T value) {
  (*output_map)[key] = Variant(value).AsString().mutable_string();
}

// Specialization of StoreKeyValuePairAsStringInMap() that ignores null strings.
template <>
void StoreKeyValuePairAsStringInMap<const char*>(
    std::map<std::string, std::string>* output_map, const char* key,
    const char* value) {
  if (value) (*output_map)[key] = value;
}

// Generate a query string from map of strings.
std::string QueryStringFromMap(
    const std::map<std::string, std::string>& parameters) {
  std::string query_string;
  int index = 0;
  for (auto it = parameters.begin(); it != parameters.end(); ++it, ++index) {
    const char* prefix = index == 0 ? "?" : "&";
    // TODO(smiles): URLs really need to be percent encoded.
    query_string += prefix;
    query_string += it->first;
    query_string += "=";
    query_string += it->second;
  }
  return query_string;
}

// Generate a long link from dynamic links components.
static GeneratedDynamicLink LongLinkFromComponents(
    const DynamicLinkComponents& components) {
  GeneratedDynamicLink generated_link;
  if (components.link == nullptr || strlen(components.link) == 0) {
    generated_link.error = "No target link specified.";
    return generated_link;
  }
  if (components.domain_uri_prefix == nullptr ||
       strlen(components.domain_uri_prefix) == 0) {
    generated_link.error = "No domain specified.";
    return generated_link;
  }
  std::map<std::string, std::string> query_parameters;
  StoreKeyValuePairAsStringInMap(&query_parameters, "link", components.link);
  {
    const auto* params = components.google_analytics_parameters;
    if (params) {
      StoreKeyValuePairAsStringInMap(&query_parameters, "utm_source",
                                     params->source);
      StoreKeyValuePairAsStringInMap(&query_parameters, "utm_medium",
                                     params->medium);
      StoreKeyValuePairAsStringInMap(&query_parameters, "utm_campaign",
                                     params->campaign);
      StoreKeyValuePairAsStringInMap(&query_parameters, "utm_term",
                                     params->term);
      StoreKeyValuePairAsStringInMap(&query_parameters, "utm_content",
                                     params->content);
    }
  }
  {
    const auto* params = components.ios_parameters;
    if (params) {
      StoreKeyValuePairAsStringInMap(&query_parameters, "ibi",
                                     params->bundle_id);
      StoreKeyValuePairAsStringInMap(&query_parameters, "isi",
                                     params->app_store_id);
      StoreKeyValuePairAsStringInMap(&query_parameters, "ifl",
                                     params->fallback_url);
      StoreKeyValuePairAsStringInMap(&query_parameters, "ius",
                                     params->custom_scheme);
      StoreKeyValuePairAsStringInMap(&query_parameters, "imv",
                                     params->minimum_version);
      StoreKeyValuePairAsStringInMap(&query_parameters, "ipbi",
                                     params->ipad_bundle_id);
      StoreKeyValuePairAsStringInMap(&query_parameters, "ipfl",
                                     params->ipad_fallback_url);
    }
  }
  {
    const auto* params = components.itunes_connect_analytics_parameters;
    if (params) {
      StoreKeyValuePairAsStringInMap(&query_parameters, "pt",
                                     params->provider_token);
      StoreKeyValuePairAsStringInMap(&query_parameters, "ct",
                                     params->campaign_token);
      StoreKeyValuePairAsStringInMap(&query_parameters, "at",
                                     params->affiliate_token);
    }
  }
  {
    const auto* params = components.android_parameters;
    if (params) {
      StoreKeyValuePairAsStringInMap(&query_parameters, "apn",
                                     params->package_name);
      StoreKeyValuePairAsStringInMap(&query_parameters, "afl",
                                     params->fallback_url);
      StoreKeyValuePairAsStringInMap(&query_parameters, "amv",
                                     params->minimum_version);
    }
  }
  {
    const auto* params = components.social_meta_tag_parameters;
    if (params) {
      StoreKeyValuePairAsStringInMap(&query_parameters, "st", params->title);
      StoreKeyValuePairAsStringInMap(&query_parameters, "sd",
                                     params->description);
      StoreKeyValuePairAsStringInMap(&query_parameters, "si",
                                     params->image_url);
    }
  }
  generated_link.url = std::string(components.domain_uri_prefix) + "/" +
      QueryStringFromMap(query_parameters);
  return generated_link;
}

GeneratedDynamicLink GetLongLink(const DynamicLinkComponents& components) {
  FIREBASE_ASSERT_RETURN(GeneratedDynamicLink(), internal::IsInitialized());
  return LongLinkFromComponents(components);
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components,
    const DynamicLinkOptions& /*dynamic_link_options*/) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  auto long_link = GetLongLink(components);
  long_link.warnings.push_back(kLinkShorteningNotSupported);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  const auto handle =
      api->SafeAlloc<GeneratedDynamicLink>(kDynamicLinksFnGetShortLink);
  api->CompleteWithResult(handle, 0, "", long_link);
  return GetShortLinkLastResult();
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components) {
  return GetShortLink(components, DynamicLinkOptions());
}

Future<GeneratedDynamicLink> GetShortLink(
    const char* long_dynamic_link,
    const DynamicLinkOptions& /*dynamic_link_options*/) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  GeneratedDynamicLink long_link;
  long_link.url = long_dynamic_link;
  long_link.warnings.push_back(kLinkShorteningNotSupported);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  const auto handle =
      api->SafeAlloc<GeneratedDynamicLink>(kDynamicLinksFnGetShortLink);
  api->CompleteWithResult(handle, 0, "", long_link);
  return GetShortLinkLastResult();
}

Future<GeneratedDynamicLink> GetShortLink(const char* long_dynamic_link) {
  return GetShortLink(long_dynamic_link, DynamicLinkOptions());
}

Future<GeneratedDynamicLink> GetShortLinkLastResult() {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<GeneratedDynamicLink>&>(
      api->LastResult(kDynamicLinksFnGetShortLink));
}

}  // namespace dynamic_links
}  // namespace firebase
