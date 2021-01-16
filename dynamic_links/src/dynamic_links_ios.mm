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

#include <cstring>

#include "dynamic_links/src/include/firebase/dynamic_links.h"
#include "dynamic_links/src/include/firebase/dynamic_links/components.h"

#include "app/src/build_type_generated.h"

#include "FDLURLComponents.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/include/firebase/version.h"
#include "app/src/invites/ios/invites_receiver_internal_ios.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "dynamic_links/src/common.h"

namespace firebase {
namespace dynamic_links {

DEFINE_FIREBASE_VERSION_STRING(FirebaseDynamicLinks);

static invites::internal::InvitesReceiverInternalIos::StartupRegistration
    g_invites_startup_registration("ddl");
static bool g_initialized = false;
static const char* kUrlEncodingError = "Specified link could not be encoded as a URL.";

InitResult Initialize(const App& app, Listener* listener) {
  if (!g_initialized) {
    invites::internal::InvitesReceiverInternalIos::RegisterStartup(&g_invites_startup_registration);
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

// Encode a URL from a C string.  If this fails, nil is returned.
static NSURL* EncodeUrlFromString(const char* url_string) {
  // Try encoding without escaping as the URL may already be escaped correctly.
  NSURL*url = [NSURL URLWithString:@(url_string)];
  if (url) return url;
  // If encoding without escaping fails, try again with percent encoding.
  return [NSURL URLWithString:[@(url_string)
                                  stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
}

// Add a warning to the list of warnings regarding failure to encode the specified
// URL.
static void AddUrlEncodingWarning(
    std::vector<std::string>* warnings, const char* url, const char* field_description) {
  warnings->push_back(std::string("Failed to encode URL for ") +
                      std::string(field_description) + ", URL: " + url);
}

// Set a URL field of an iOS dynamic link components object from a C++ structure string field.
#define FIR_DYNAMIC_LINK_PARAMS_SET_URL(fir_params_object, field, cpp_params_object, cpp_field, \
                                        field_description, generated_link_warnings) \
  { \
    if ((cpp_params_object)->cpp_field) { \
      NSURL* ns_url = EncodeUrlFromString((cpp_params_object)->cpp_field); \
      if (ns_url) { \
        (fir_params_object).field = ns_url; \
      } else { \
        AddUrlEncodingWarning(generated_link_warnings, (cpp_params_object)->cpp_field, \
                              field_description); \
      } \
    } \
  }

// Set a string field of an iOS dynamic link components object from a C++ structure string field.
#define FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params_object, field, cpp_params_object, cpp_field) \
  { \
    if ((cpp_params_object)->cpp_field) { \
      (fir_params_object).field = @((cpp_params_object)->cpp_field); \
    } \
  }

// Construct FIRDynamicLinkComponents and populate GeneratedDynamicLink from DynamicLinkComponents.
static FIRDynamicLinkComponents* GetFIRComponentsAndGeneratedLink(
    const DynamicLinkComponents& components, GeneratedDynamicLink* generated_link) {
  static const char* kMissingFieldPostfix = "must be specified";
  if (!components.link) {
    generated_link->error = std::string("Link ") + kMissingFieldPostfix;
    return nil;
  }
  if (!components.domain_uri_prefix || !*components.domain_uri_prefix) {
    generated_link->error =
        std::string("Domain URI Prefix ") + kMissingFieldPostfix;
    return nil;
  }
  NSURL *link_url = EncodeUrlFromString(components.link);
  if (!link_url) {
    generated_link->error = kUrlEncodingError;
    return nil;
  }
  FIRDynamicLinkComponents* fir_components =
      [FIRDynamicLinkComponents componentsWithLink:link_url
                                   domainURIPrefix:@(components.domain_uri_prefix)];
  {
    auto* cpp_params = components.google_analytics_parameters;
    if (cpp_params) {
      FIRDynamicLinkGoogleAnalyticsParameters* fir_params =
          [[FIRDynamicLinkGoogleAnalyticsParameters alloc] init];
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, source, cpp_params, source);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, medium, cpp_params, medium);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, campaign, cpp_params, campaign);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, term, cpp_params, term);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, content, cpp_params, content);
      fir_components.analyticsParameters = fir_params;
    }
  }
  {
    auto* cpp_params = components.ios_parameters;
    if (cpp_params) {
      if (!cpp_params->bundle_id) {
        generated_link->error = std::string("iOS parameters bundle ID ") + kMissingFieldPostfix;
        return nil;
      }
      FIRDynamicLinkIOSParameters* fir_params =
          [FIRDynamicLinkIOSParameters parametersWithBundleID:@(cpp_params->bundle_id)];
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, appStoreID, cpp_params, app_store_id);
      FIR_DYNAMIC_LINK_PARAMS_SET_URL(fir_params, fallbackURL, cpp_params, fallback_url,
                                      "iOS fallback URL", &generated_link->warnings);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, customScheme, cpp_params, custom_scheme);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, iPadBundleID, cpp_params, ipad_bundle_id);
      FIR_DYNAMIC_LINK_PARAMS_SET_URL(fir_params, iPadFallbackURL, cpp_params, ipad_fallback_url,
                                      "iOS iPad fallback URL", &generated_link->warnings);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, minimumAppVersion, cpp_params,
                                         minimum_version);
      fir_components.iOSParameters = fir_params;
    }
  }
  {
    auto* cpp_params = components.itunes_connect_analytics_parameters;
    if (cpp_params) {
      FIRDynamicLinkItunesConnectAnalyticsParameters* fir_params =
          [[FIRDynamicLinkItunesConnectAnalyticsParameters alloc] init];
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, affiliateToken, cpp_params, affiliate_token);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, campaignToken, cpp_params, campaign_token);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, providerToken, cpp_params, provider_token);
      fir_components.iTunesConnectParameters = fir_params;
    }
  }
  {
    auto* cpp_params = components.android_parameters;
    if (cpp_params) {
      if (!cpp_params->package_name) {
        generated_link->error = std::string("Android parameters package name ") +
            kMissingFieldPostfix;
        return nil;
      }
      FIRDynamicLinkAndroidParameters* fir_params =
          [FIRDynamicLinkAndroidParameters parametersWithPackageName:@(cpp_params->package_name)];
      FIR_DYNAMIC_LINK_PARAMS_SET_URL(fir_params, fallbackURL, cpp_params, fallback_url,
                                      "Android fallback URL", &generated_link->warnings);
      fir_params.minimumVersion = cpp_params->minimum_version;
      fir_components.androidParameters = fir_params;
    }
  }
  {
    auto* cpp_params = components.social_meta_tag_parameters;
    if (cpp_params) {
      FIRDynamicLinkSocialMetaTagParameters* fir_params =
          [[FIRDynamicLinkSocialMetaTagParameters alloc] init];
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, title, cpp_params, title);
      FIR_DYNAMIC_LINK_PARAMS_SET_STRING(fir_params, descriptionText, cpp_params, description);
      FIR_DYNAMIC_LINK_PARAMS_SET_URL(fir_params, imageURL, cpp_params, image_url,
                                      "Social Image URL", &generated_link->warnings);
      fir_components.socialMetaTagParameters = fir_params;
    }
  }
  if (fir_components.url) {
    generated_link->url = util::NSStringToString(fir_components.url.absoluteString);
  } else {
    generated_link->error = "Failed to generated long link.";
  }
  return fir_components;
}

GeneratedDynamicLink GetLongLink(const DynamicLinkComponents& components) {
  FIREBASE_ASSERT_RETURN(GeneratedDynamicLink(), internal::IsInitialized());
  GeneratedDynamicLink generated_link;
  GetFIRComponentsAndGeneratedLink(components, &generated_link);
  return generated_link;
}

// Convert DynamicLinkOptions to FIRDynamicLinkComponentsOptions.
static FIRDynamicLinkComponentsOptions* ToFIRDynamicLinkComponentsOptions(
    const DynamicLinkOptions& dynamic_link_options) {
  FIRDynamicLinkComponentsOptions* fir_options = [[FIRDynamicLinkComponentsOptions alloc] init];
  switch (dynamic_link_options.path_length) {
    case kPathLengthShort:
      fir_options.pathLength = FIRShortDynamicLinkPathLengthShort;
      break;
    case kPathLengthUnguessable:
      fir_options.pathLength = FIRShortDynamicLinkPathLengthUnguessable;
      break;
    case kPathLengthDefault:
      // Drop through.
    default:
      fir_options.pathLength = FIRShortDynamicLinkPathLengthDefault;
      break;
  }
  return fir_options;
}

// Generate a short link from a long dynamic link.
static Future<GeneratedDynamicLink> GetShortLink(NSURL *long_link_url,
                                                 const GeneratedDynamicLink& generated_link,
                                                 const DynamicLinkOptions& dynamic_link_options) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  const SafeFutureHandle<GeneratedDynamicLink> handle =
      api->SafeAlloc<GeneratedDynamicLink>(kDynamicLinksFnGetShortLink);
  if (long_link_url) {
    GeneratedDynamicLink* output_generated_link = new GeneratedDynamicLink();
    *output_generated_link = generated_link;
    [FIRDynamicLinkComponents shortenURL:long_link_url
                                 options:ToFIRDynamicLinkComponentsOptions(dynamic_link_options)
                              completion:^(NSURL * _Nullable shortURL,
                                           NSArray<NSString *> * _Nullable warnings,
                                           NSError * _Nullable error) {
        int error_code = kErrorCodeSuccess;
        if (error) {
          error_code = error.code ? static_cast<int>(error.code) : kErrorCodeFailed;
          output_generated_link->error = util::NSStringToString(error.localizedDescription);
        } else {
          if (warnings) {
            for (NSString* warning in warnings) {
              output_generated_link->warnings.push_back(warning.UTF8String);
            }
          }
          if (shortURL) {
            output_generated_link->url = util::NSStringToString(shortURL.absoluteString);
          } else {
            error_code = kErrorCodeFailed;
            output_generated_link->error = std::string("Failed to shorten link ") +
                output_generated_link->url;
          }
        }
        FutureData::Get()->api()->CompleteWithResult(
            handle, error_code, output_generated_link->error.c_str(), *output_generated_link);
        delete output_generated_link;
      }];
  } else {
    api->CompleteWithResult(handle, kErrorCodeFailed, generated_link.error.c_str(),
                            generated_link);
  }
  return MakeFuture(api, handle);
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components,
    const DynamicLinkOptions& dynamic_link_options) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  GeneratedDynamicLink generated_link;
  FIRDynamicLinkComponents* fir_components = GetFIRComponentsAndGeneratedLink(components,
                                                                              &generated_link);
  return GetShortLink(fir_components ? fir_components.url : nil, generated_link,
                      dynamic_link_options);
}

Future<GeneratedDynamicLink> GetShortLink(
    const DynamicLinkComponents& components) {
  return GetShortLink(components, DynamicLinkOptions());
}

Future<GeneratedDynamicLink> GetShortLink(
    const char* long_dynamic_link,
    const DynamicLinkOptions& dynamic_link_options) {
  FIREBASE_ASSERT_RETURN(Future<GeneratedDynamicLink>(),
                         internal::IsInitialized());
  GeneratedDynamicLink generated_link;
  NSURL *link_url = EncodeUrlFromString(long_dynamic_link);
  if (!link_url) generated_link.error = kUrlEncodingError;
  return GetShortLink(link_url, generated_link, dynamic_link_options);
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
