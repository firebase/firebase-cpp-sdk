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

// WARNING: Code from this file is included verbatim in the Auth C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

// [START dynamic_link_includes]
#include "firebase/app.h"
#include "firebase/dynamic_links.h"

// Needed for creating links only.
#include "firebase/dynamic_links/components.h"
// [END dynamic_link_includes]

#if defined(__ANDROID__)
JNIEnv* my_jni_env = nullptr;
jobject my_activity = nullptr;
#endif  // defined(__ANDROID__)


void CreateLinks() {
  {
    // [START dlink_create_longlink_minimal]
    firebase::dynamic_links::IOSParameters ios_parameters("com.example.ios");

    firebase::dynamic_links::AndroidParameters android_parameters(
        "com.example.android.package_name");

    firebase::dynamic_links::DynamicLinkComponents components(
        "https://www.example.com/", "example.page.link");
    components.android_parameters = &android_parameters;
    components.ios_parameters = &ios_parameters;

    firebase::dynamic_links::GeneratedDynamicLink long_link =
        firebase::dynamic_links::GetLongLink(components);
    // [END dlink_create_longlink_minimal]

    // [START dlink_create_shortlink_minimal]
    firebase::dynamic_links::DynamicLinkOptions short_link_options;
    short_link_options.path_length = firebase::dynamic_links::kPathLengthShort;

    firebase::Future<firebase::dynamic_links::GeneratedDynamicLink> result =
        firebase::dynamic_links::GetShortLink(components, short_link_options);
    // [END dlink_create_shortlink_minimal]

    // [START poll_dlink_future]
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::dynamic_links::kErrorCodeSuccess) {
        firebase::dynamic_links::GeneratedDynamicLink link = *result.result();
        printf("Create short link succeeded: %s\n", link.url.c_str());
      } else {
        printf("Created short link failed with error '%s'\n",
               result.error_message());
      }
    }
    // [END poll_dlink_future]
  }
}

// [START dlink_listener]
class Listener : public firebase::dynamic_links::Listener {
 public:
  // Called on the client when a dynamic link arrives.
  void OnDynamicLinkReceived(
      const firebase::dynamic_links::DynamicLink* dynamic_link) override {
    printf("Received link: %s", dynamic_link->url.c_str());
  }
};
// [END dlink_listener]

