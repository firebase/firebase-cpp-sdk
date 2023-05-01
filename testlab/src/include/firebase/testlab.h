/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_TESTLAB_SRC_INCLUDE_FIREBASE_TESTLAB_H_
#define FIREBASE_TESTLAB_SRC_INCLUDE_FIREBASE_TESTLAB_H_

#include <string>

#include "firebase/app.h"
#include "firebase/internal/common.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(test_lab)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

/// @brief Namespace that encompasses all Firebase APIs.
namespace firebase {

/// @brief Firebase Test Lab API.
///
/// See <a href="/docs/test-lab">the developer guides</a> for general
/// information on using Firebase Test Lab.
///
/// This library is experimental and is not currently officially supported.
namespace test_lab {
namespace game_loop {

/// @brief Indicate the outcome of a game loop scenario (e.g. success).
enum ScenarioOutcome { kScenarioOutcomeSuccess, kScenarioOutcomeFailure };

/// @brief Initialize the Test Lab Game Loop API.
///
/// This must be called prior to calling any other methods in the
/// firebase::test_lab::game_loop namespace.
///
/// @param[in] app Default firebase::App instance.
///
/// @see firebase::App::GetInstance().
void Initialize(const firebase::App& app);

/// @brief Terminate the Test Lab Game Loop API.
///
/// @note The application will continue to run after calling this method, but
/// any future calls to methods in the firebase::test_lab::game_loop namespace
/// will have no effect unless it is initialized again.
///
/// @note If this function is called during a game loop, any results logged as
/// part of that game loop scenario will not appear in the scenario's custom
/// results.
///
/// Cleans up resources associated with the Test Lab Game Loop API.
void Terminate();

/// @brief Retrieve the current scenario number of a game loop test.
///
/// @return A positive integer representing the current game loop scenario, or
/// 0 if not game loop is running.
int GetScenario();

/// @brief Record progress of a game loop to the test's custom results.
///
/// @note These messages also forwarded to the system log at the DEBUG level.
///
/// @param[in] format Format string of the message to include in the scenario's
/// custom results file.
void LogText(const char* format, ...);

/// @brief Complete the current game loop scenario and exit the application.
///
/// @note This method implicitly calls `game_loop::Terminate()` prior to
/// exiting. If no game loop is running, this method has no effect.
///
/// Finish the current game loop scenario by cleaning up its resources and
/// exiting the application.
void FinishScenario(ScenarioOutcome outcome);

/// @brief Set the scenario of the currently running test.
///
/// @note Calling this method and changing the scenario will clear any results
/// for the previous scenario.
void SetScenario(int scenario_number);

/// @brief Set the directory where custom results will be written to when
/// FinishScenario() is called.
void SetResultsDirectory(const char* path);

/// @brief The currently set directory where custom results will be written to
/// when FinishScenario() is called. If no directory has been set, this function
/// returns an empty string.
std::string GetResultsDirectory();

}  // namespace game_loop
}  // namespace test_lab
}  // namespace firebase

#endif  // FIREBASE_TESTLAB_SRC_INCLUDE_FIREBASE_TESTLAB_H_
