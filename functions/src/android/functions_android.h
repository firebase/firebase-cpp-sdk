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

#ifndef FIREBASE_FUNCTIONS_SRC_ANDROID_FUNCTIONS_ANDROID_H_
#define FIREBASE_FUNCTIONS_SRC_ANDROID_FUNCTIONS_ANDROID_H_

#include <jni.h>

#include <map>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase {
namespace functions {
namespace internal {

class FunctionsInternal {
 public:
  FunctionsInternal(App* app, const char* region);
  ~FunctionsInternal();

  FunctionsInternal(const FunctionsInternal&) = delete;
  FunctionsInternal& operator=(const FunctionsInternal&) = delete;

  // Return the App we were created with.
  App* app() const { return app_; }

  // Return the region or custom domain we were created with.
  const char* region() const { return region_.c_str(); }

  // Return the Java object we are wrapping.
  jobject obj() const { return obj_; }

  // Manage Future implementations.
  FutureManager& future_manager() { return future_manager_; }

  CleanupNotifier& cleanup() { return cleanup_; }

  // Whether this object was successfully initialized by the constructor.
  bool is_valid() const { return app_ != nullptr && obj_ != nullptr; }

  HttpsCallableReferenceInternal* GetHttpsCallable(const char* name) const;
  HttpsCallableReferenceInternal* GetHttpsCallableWithOptions(
      const char* name, const HttpsCallableOptions& options) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      const char* url) const;
  HttpsCallableReferenceInternal* GetHttpsCallableFromURLWithOptions(
      const char* url, const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);

  Error ErrorFromJavaFunctionsException(JNIEnv* env, jobject java_exception,
                                        std::string* error_message,
                                        Variant* details_out = nullptr);

 private:
  // Initialize JNI classes and method IDs.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jobject, Error>* code_map_;

  FutureManager future_manager_;
  CleanupNotifier cleanup_;
  App* app_ = nullptr;
  std::string region_;
  jobject obj_ = nullptr;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_ANDROID_FUNCTIONS_ANDROID_H_
