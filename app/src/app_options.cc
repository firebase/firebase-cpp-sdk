/*
 * Copyright 2017 Google LLC
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

#include <string.h>

#include <cassert>
#include <cstdint>
#include <string>

#include "app/google_services_generated.h"
#include "app/google_services_resource.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

namespace firebase {
// static
AppOptions* AppOptions::LoadFromJsonConfig(const char* config,  // NOLINT
                                           AppOptions* options) {
  if (!config) {
    LogError("Config string passed to AppOptions::LoadFromJsonConfig() cannot "
             "be null.");
    return nullptr;
  }

  // Initialize flatbuffer parser.
  flatbuffers::IDLOptions fbs_options;
  // Skip irrelevant and unsupported fields.
  fbs_options.skip_unexpected_fields_in_json = true;
  flatbuffers::Parser parser(fbs_options);

  // Parse schema, which should always be valid in a released SDK.
  const char* schema =
      reinterpret_cast<const char*>(fbs::google_services_resource_data);
  // Evaluate beforehand due to b/63396663.
  bool parse_schema_ok = parser.Parse(schema);
  FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, parse_schema_ok,
                                 "Failed to load Firebase resource schema: %s.",
                                 parser.error_.c_str());

  // Parse resource file.
  if (!parser.Parse(config)) {
    LogError(
        "Failed to parse Firebase config: %s. Check the config string "
        "passed to AppOptions::LoadFromJsonConfig()",
        parser.error_.c_str());
    return nullptr;
  }
  uint8_t* buffer_pointer = parser.builder_.GetBufferPointer();
  flatbuffers::Verifier verifier(buffer_pointer, parser.builder_.GetSize());
  if (!firebase::fbs::VerifyGoogleServicesBuffer(verifier)) {
    LogError(
        "Failed to parse Firebase config: integrity check failed. Check "
        "the config string passed to AppOptions::LoadFromJsonConfig()");
    return nullptr;
  }

  AppOptions temp_options;
  if (options != nullptr) {
    temp_options = *options;
  }

  // Read project information.
  const firebase::fbs::GoogleServices* google_services =
      firebase::fbs::GetGoogleServices(buffer_pointer);
  const firebase::fbs::ProjectInfo* project_info =
      google_services ? google_services->project_info() : nullptr;
  bool parse_failed = false;
  if (project_info) {
    if (project_info->firebase_url()) {
      temp_options.set_database_url(project_info->firebase_url()->c_str());
    }
    if (project_info->project_number()) {
      temp_options.set_messaging_sender_id(
          project_info->project_number()->c_str());
    }
    if (project_info->storage_bucket()) {
      temp_options.set_storage_bucket(project_info->storage_bucket()->c_str());
    }
    if (project_info->project_id()) {
      temp_options.set_project_id(project_info->project_id()->c_str());
    }

    // Read client information.
    const firebase::fbs::Client* selected_client = nullptr;
    const char* requested_package =
        (options && strlen(options->package_name()) > 0)
            ? options->package_name()
            : nullptr;
    if (google_services->client()) {
      if (requested_package) {
        for (const firebase::fbs::Client* client : *google_services->client()) {
          if (client && client->client_info() &&
              client->client_info()->android_client_info() &&
              client->client_info()->android_client_info()->package_name() &&
              strcmp(client->client_info()
                         ->android_client_info()
                         ->package_name()
                         ->c_str(),
                     requested_package) == 0) {
            selected_client = client;
            break;
          }
        }
      }
      if (!selected_client) {
        for (const firebase::fbs::Client* client : *google_services->client()) {
          if (client && client->client_info() &&
              client->client_info()->android_client_info() &&
              client->client_info()->android_client_info()->package_name()) {
            selected_client = client;
            break;
          }
        }
      }
    }
    if (selected_client) {
      temp_options.set_package_name(selected_client->client_info()
                                        ->android_client_info()
                                        ->package_name()
                                        ->c_str());
      if (selected_client->api_key()) {
        for (const firebase::fbs::ApiKey* api_key :
             *selected_client->api_key()) {
          if (api_key && api_key->current_key()) {
            temp_options.set_api_key(api_key->current_key()->c_str());
            break;
          }
        }
      }
      if (selected_client->client_info() &&
          selected_client->client_info()->mobilesdk_app_id()) {
        temp_options.set_app_id(
            selected_client->client_info()->mobilesdk_app_id()->c_str());
      }
      if (selected_client->services()) {
        const firebase::fbs::Services* services = selected_client->services();
        if (services->analytics_service() &&
            services->analytics_service()->analytics_property() &&
            services->analytics_service()
                ->analytics_property()
                ->tracking_id()) {
          temp_options.set_ga_tracking_id(services->analytics_service()
                                              ->analytics_property()
                                              ->tracking_id()
                                              ->c_str());
        }
      }
    } else {
      LogError(
          "'client' data (oauth client ID, API key etc.) not found in "
          "Firebase config.");
      parse_failed = true;
    }
  } else {
    LogError("'project_info' not found in Firebase config.");
    parse_failed = true;
  }

  if (parse_failed) {
    return nullptr;
  }

  // Make sure required options were read.
  struct ValidateOption {
    const char* option_value;
    const char* option_name;
  } options_to_validate[] = {
      {temp_options.database_url(), "Database URL"},
      {temp_options.storage_bucket(), "Storage bucket"},
      {temp_options.project_id(), "Project ID"},
      {temp_options.api_key(), "API key"},
      {temp_options.app_id(), "App ID"},
  };
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(options_to_validate); ++i) {
    const auto& validate_option = options_to_validate[i];
    if (strlen(validate_option.option_value) == 0) {
      LogWarning("%s not set in the Firebase config.",
                 validate_option.option_name);
    }
  }

  AppOptions* return_options = options ? options : new AppOptions();
  *return_options = temp_options;
  return return_options;
}

// Attempt to populate required options with default values if not specified.
bool AppOptions::PopulateRequiredWithDefaults(
#if FIREBASE_PLATFORM_ANDROID
    JNIEnv* jni_env, jobject activity
#endif  // FIREBASE_PLATFORM_ANDROID
) {
  // Populate App ID, API key, and Project ID from the default options if
  // they're not specified.
  if (app_id_.empty() || api_key_.empty() || project_id_.empty()) {
    AppOptions default_options;
    if (AppOptions::LoadDefault(&default_options
#if FIREBASE_PLATFORM_ANDROID
                                ,
                                jni_env, activity
#endif  // FIREBASE_PLATFORM_ANDROID
                                )) {
      if (app_id_.empty()) app_id_ = default_options.app_id_;
      if (api_key_.empty()) api_key_ = default_options.api_key_;
      if (project_id_.empty()) project_id_ = default_options.project_id_;
    } else {
      LogError(
          "Failed to load default options when attempting to populate "
          "missing fields");
    }
  }
  if (app_id_.empty() || api_key_.empty() || project_id_.empty()) {
    LogError(
        "App ID, API key, and Project ID must be specified in App options.");
    return false;
  }
  return true;
}

}  // namespace firebase
