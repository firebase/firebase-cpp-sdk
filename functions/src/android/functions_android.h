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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_FUNCTIONS_ANDROID_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_FUNCTIONS_ANDROID_H_

#include <jni.h>
#include <map>
#include <set>
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase {
namespace functions {
namespace internal {

// Used for registering global callbacks. See
// firebase::util::RegisterCallbackOnTask for context.
extern const char kApiIdentifier[];

class FunctionsInternal {
 public:
  // Build a Functions.
  FunctionsInternal(App* app, const char* region);
  ~FunctionsInternal();

  // Return the App we were created with.
  App* app() const;

  // Return the region we were created with.
  const char* region() const;

  HttpsCallableReferenceInternal* GetHttpsCallable(const char* name) const;

  void UseFunctionsEmulator(const char* origin);

  // Convert an error code obtained from a Java FunctionsException into a C++
  // Error enum.
  Error ErrorFromJavaErrorCode(jint java_error_code) const;

  // Convert a Java FunctionsExcetion instance into a C++ Error enum.
  // If error_message is not null, it will be set to the error message string
  // from the exception.
  Error ErrorFromJavaFunctionsException(jobject java_error,
                                        std::string* error_message) const;

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // When this is deleted, it will clean up all FunctionsReferences and other
  // objects.
  CleanupNotifier& cleanup() { return cleanup_; }

 private:
  // Initialize JNI for all classes.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Initialize classes loaded from embedded files.
  static bool InitializeEmbeddedClasses(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jint, Error> java_error_to_cpp_;

  App* app_;
  std::string region_;
  // Java FirebaseFunctions global ref.
  jobject obj_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_FUNCTIONS_ANDROID_H_
