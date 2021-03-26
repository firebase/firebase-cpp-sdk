// Copyright 2020 Google
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

#include "testing/reporter_impl_fake.h"

#include "testing/reporter_impl.h"

namespace firebase {
namespace testing {
namespace cppsdk {
namespace fake {

void TestFunction() {
  FakeReporter->AddReport(
      "fake_function_name", "fake_function_result",
      std::initializer_list<const char *>({
         "fake_argument0", "fake_argument1", "fake_argument2"}));
}

}  // namespace fake
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
