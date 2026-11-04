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

#include "firebase/app.h"
#include "firebase/functions.h"
#include "firebase/future.h"
#include "firebase/variant.h"

namespace firebase {
namespace functions {

// [START functions_get_instance]
Functions* get_functions_instance(App* app) {
  return Functions::GetInstance(app);
}
// [END functions_get_instance]

// [START functions_add_numbers]
Future<HttpsCallableResult> add_numbers(Functions* functions, int a, int b) {
  // Create the arguments to the callable function.
  Variant data = Variant::EmptyMap();
  data.map()["firstNumber"] = a;
  data.map()["secondNumber"] = b;

  // Call the function and extract the operation from the result.
  HttpsCallableReference add_numbers_callable =
      functions->GetHttpsCallable("addNumbers");
  Future<HttpsCallableResult> future = add_numbers_callable.Call(data);
  future.OnCompletion(
      [](const Future<HttpsCallableResult>& completed_future) {
        if (completed_future.error() != kErrorNone ||
            completed_future.result() == nullptr) {
          // Handle error...
        } else {
          Variant result = completed_future.result()->data();
          (void)result;
        }
      });
  return future;
}
// [END functions_add_numbers]

}  // namespace functions
}  // namespace firebase
