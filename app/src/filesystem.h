/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_FILESYSTEM_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_FILESYSTEM_H_

#include <string>

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

/**
 * Returns a system-defined best directory in which to create application
 * data. Values vary wildly across platforms. They include:
 *
 *   * iOS: $container/Library/Application Support/$app_name
 *   * Linux: $HOME/.local/share/$app_name
 *   * macOS: $container/Library/Application Support/$app_name
 *   * Other UNIX: $HOME/.$app_name. Note: Android is not implemented yet.
 *   * tvOS: $container/Library/Caches/$app_name
 *   * Windows: %USERPROFILE%/AppData/Local
 *
 * On iOS, tvOS, and macOS (when running sandboxed), these locations are
 * relative to the data container for the current application. On macOS when
 * the application is not sandboxed, the returned value will be relative to
 * $HOME instead. See "About the iOS File System" in the Apple "File System
 * Programming Guide" at https://apple.co/2Nn7Bsb.
 *
 * Note: the returned path is just where the system thinks the application
 * data should be stored, but `AppDataDir` does not actually guarantee that this
 * path exists (unless `should_create` is `true`, in which case it will attempt
 * to create a directory at the given path).
 *
 * @param app_name The name of the application.
 * @return The path to the application data, or an empty string if an error
 *     occurred.
 */
// TODO(varconst): Android is currently not supported.
//
// TODO(b/171738655): use a separate function instead of the `should_create`
// flag. Use `StatusOr` for returning errors.
std::string AppDataDir(const char* app_name, bool should_create = true,
                       std::string* out_error = nullptr);

}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_FILESYSTEM_H_
