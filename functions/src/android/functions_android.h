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

#include <memory>
#include <string>

#include "app/src/app_state.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase {
namespace functions {
namespace internal {

class FunctionsInternal : public ::firebase::internal::Service {
 public:
  // Build a Functions.
  FunctionsInternal(::firebase::internal::AppState& app_state, const App& app,
                    const char* region);
  ~FunctionsInternal() override;

  // Return the App we were created with.
  App app() const;

  ::firebase::internal::AppState& app_state() const { return app_state_; }

  JNIEnv* GetJNIEnv() const;

  // Return the region we were created with.
  const char* region() const;

  HttpsCallableReferenceInternal* GetHttpsCallable(
      std::shared_ptr<FunctionsInternal> self, const char* name) const;

  HttpsCallableReferenceInternal* GetHttpsCallable(
      std::shared_ptr<FunctionsInternal> self, const char* name,
      const HttpsCallableOptions& options) const;

  // Get a FunctionsReference for the specified URL.
  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      std::shared_ptr<FunctionsInternal> self, const char* url) const;

  HttpsCallableReferenceInternal* GetHttpsCallableFromURL(
      std::shared_ptr<FunctionsInternal> self, const char* url,
      const HttpsCallableOptions& options) const;

  void UseFunctionsEmulator(const char* origin);

  // Convert an error code obtained from a Java FunctionsException into a C++
  // Error enum.
  Error ErrorFromJavaErrorCode(jint java_error_code) const;

  // Convert a Java FunctionsException instance into a C++ Error enum.
  // If error_message is not null, it will be set to the error message string
  // from the exception.
  Error ErrorFromJavaFunctionsException(jobject java_error,
                                        std::string* error_message) const;

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return initialized_; }

  // Used for registering global callbacks. See
  // firebase::util::RegisterCallbackOnTask for context.
  const char* jni_task_id() { return jni_task_id_.c_str(); }

 private:
  // Initialize JNI for all classes.
  static bool Initialize(const App& app);
  static void Terminate(JNIEnv* env);

  static int initialize_count_;

  ::firebase::internal::AppState& app_state_;
  bool initialized_ = false;
  std::string region_;
  // Java FirebaseFunctions global ref.
  jobject obj_ = nullptr;

  FutureManager future_manager_;

  // String to be used when registering for JNI task callbacks.
  std::string jni_task_id_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_ANDROID_FUNCTIONS_ANDROID_H_
