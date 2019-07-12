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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_FUNCTION_REGISTRY_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_FUNCTION_REGISTRY_H_

#include <map>
#include "app/src/mutex.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
class App;

namespace internal {

// Identifiers for the function registry.
enum FunctionId {
  FnAuthGetCurrentToken,
  FnAuthStartTokenListener,
  FnAuthStopTokenListener,
  FnAuthGetTokenAsync
};

// Class for providing a generic way for firebase libraries to expose their
// methods to each other, without requiring a link dependency.
class FunctionRegistry {
 public:
  // Template for the functions we pass around.  They will always accept a
  // pointer to the current app, as well as a pointer that can be used for
  // arbitrary structs, and a pointer indicating where to place the output
  // data.  The function should return true if it completed successfully,
  // and false otherwise.
  typedef bool (*RegisteredFunction)(::FIREBASE_NAMESPACE::App* app, void* args,
                                     void* out);

  // Add a function to the registry, bound to a unique identifier.  Asserts
  // if a function is already bound to that identifier.
  bool RegisterFunction(FunctionId id, RegisteredFunction registered_function);
  // Remove a function from the registry, or asserts if nothing is bound to that
  // identifier.
  bool UnregisterFunction(FunctionId id);
  // Checks if an identifier has a function bound to it.
  bool FunctionExists(FunctionId id);
  // Executes a function if possible.  Returns false if the identifier is
  // is unbound, or if the function fails.  Results are returned via the "out"
  // pointer.
  bool CallFunction(FunctionId id, App* app, void* args, void* out);

 private:
  std::map<FunctionId, RegisteredFunction> registered_functions_;
  Mutex map_mutex_;
};

}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_FUNCTION_REGISTRY_H_
