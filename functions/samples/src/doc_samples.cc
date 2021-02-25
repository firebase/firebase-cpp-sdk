// Copyright 2018 Google LLC
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

// WARNING: Code from this file is included verbatim in the Functions C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

// [START functions_includes]
#include "firebase/app.h"
#include "firebase/functions.h"
// [END functions_includes]

// Stub functions to allow sample functions to compile.
void DisplayError(firebase::functions::Error code, const char *message) {}
void DisplayResult(int result) {}
void DisplayResult(const std::string& result) {}

// [START define_functions_instance]
firebase::functions::Functions* functions;
// [END define_functions_instance]

firebase::functions::Functions* CodeSnippets(firebase::App* app) {
  // [START initialize_functions_instance]
  functions = firebase::functions::Functions::GetInstance(app);
  // [END initialize_functions_instance]
  return functions;
}

// [START function_add_numbers]
firebase::Future<firebase::functions::HttpsCallableResult> AddNumbers(int a,
                                                                      int b) {
  // Create the arguments to the callable function, which are two integers.
  firebase::Variant data = firebase::Variant::EmptyMap();
  data.map()["firstNumber"] = a;
  data.map()["secondNumber"] = b;

  // Call the function and add a callback for the result.
  firebase::functions::HttpsCallableReference doSomething =
      functions->GetHttpsCallable("addNumbers");
  return doSomething.Call(data);
}
// [END function_add_numbers]

// [START function_add_message]
firebase::Future<firebase::functions::HttpsCallableResult> AddMessage(
    const std::string& text) {
  // Create the arguments to the callable function.
  firebase::Variant data = firebase::Variant::EmptyMap();
  data.map()["text"] = firebase::Variant(text);
  data.map()["push"] = true;

  // Call the function and add a callback for the result.
  firebase::functions::HttpsCallableReference doSomething =
      functions->GetHttpsCallable("addMessage");
  return doSomething.Call(data);
}
// [END function_add_message]

// [START call_add_numbers]
void OnAddNumbersCallback(
    const firebase::Future<firebase::functions::HttpsCallableResult>& future) {
  if (future.error() != firebase::functions::kErrorNone) {
    // Function error code, will be kErrorInternal if the failure was not
    // handled properly in the function call.
    auto code = static_cast<firebase::functions::Error>(future.error());

    // Display the error in the UI.
    DisplayError(code, future.error_message());
    return;
  }

  const firebase::functions::HttpsCallableResult *result = future.result();
  firebase::Variant data = result->data();
  // This will assert if the result returned from the function wasn't a map with
  // a number for the "operationResult" result key.
  int opResult = data.map()["operationResult"].int64_value();
  // Display the result in the UI.
  DisplayResult(opResult);
}

// ...

// [START_EXCLUDE]
void AddNumbersAndDisplay(int firstNumber, int secondNumber) {
// [END_EXCLUDE]
  auto future = AddNumbers(firstNumber, secondNumber);
  future.OnCompletion(OnAddNumbersCallback);
// [START_EXCLUDE]
}
// [END_EXCLUDE]
// [END call_add_numbers]

// [START call_add_message]
void OnAddMessageCallback(
    const firebase::Future<firebase::functions::HttpsCallableResult>& future) {
  if (future.error() != firebase::functions::kErrorNone) {
    // Function error code, will be kErrorInternal if the failure was not
    // handled properly in the function call.
    auto code = static_cast<firebase::functions::Error>(future.error());

    // Display the error in the UI.
    DisplayError(code, future.error_message());
    return;
  }

  const firebase::functions::HttpsCallableResult *result = future.result();
  firebase::Variant data = result->data();
  // This will assert if the result returned from the function wasn't a string.
  std::string message = data.string_value();
  // Display the result in the UI.
  DisplayResult(message);
}

// ...

// [START_EXCLUDE]
void AddMessageAndDisplay(const std::string& message) {
// [END_EXCLUDE]
  auto future = AddMessage(message);
  future.OnCompletion(OnAddMessageCallback);
// [START_EXCLUDE]
}
// [END_EXCLUDE]
// [END call_add_message]
