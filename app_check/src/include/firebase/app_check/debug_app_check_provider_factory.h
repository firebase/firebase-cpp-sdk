// Copyright 2022 Google LLC
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

namespace firebase {
namespace app_check {

/**
 * Implementation of an {@link AppCheckProviderFactory} that builds {@link DebugAppCheckProvider}s.
 */
class DebugAppCheckProviderFactory implements AppCheckProviderFactory {

  public:
  /**
   * Gets an instance of this class for installation into a {@link
   * com.google.firebase.appcheck.FirebaseAppCheck} instance. If no debug secret is found in {@link
   * android.content.SharedPreferences}, a new debug secret will be generated and printed to the
   * logcat. The debug secret should then be added to the allow list in the Firebase Console.
   */
  static DebugAppCheckProviderFactory getInstance();

  AppCheckProvider createProvider(const App& app) override;
}

}  // namespace app_check
}  // namespace firebase
