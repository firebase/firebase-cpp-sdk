/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_UTIL_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_UTIL_H_

#include "firebase/app.h"
#include "firebase/future.h"

namespace firebase {

struct ModuleInitializerData;

/// @brief Utility class to help with initializing Firebase modules.
class ModuleInitializer {
 public:
  /// @brief Initialization function, which should initialize a single Firebase
  /// module and return the InitResult.
  typedef InitResult (*InitializerFn)(const App& app, void* context);

  ModuleInitializer();
  virtual ~ModuleInitializer();

  Future<void> Initialize(const App& app, void* context,
                          const InitializerFn* init_fns, size_t init_fns_count);

  Future<void> Initialize(const App& app, void* context, InitializerFn init_fn);

  /// @brief Get the result of the most recent call to @see Initialize().
  Future<void> InitializeLastResult();

 private:
  ModuleInitializerData* data_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_UTIL_H_
