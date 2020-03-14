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

#ifndef FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_H_
#define FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "testing/reporter.h"

// FakeReporter implementation for iOS and Desktop. Will be used for testing iOS
// and Desktop wrappers or fakes.
//
// Objective C++ can include C++ libraries, that's why we can use one
// implementation for iOS and Dekstop.
class FakeReporterClass {
 public:
  void Reset();

  void AddReport(const char* fake, std::initializer_list<const char*> args);

  template<typename IterableType>
  void AddReport(
      const char* fake, const char* result, const IterableType& args) {
    std::vector<std::string> args_vector;
    for (const auto& arg : args) {
      args_vector.push_back(arg);
    }
    reports_[fake] =
        firebase::testing::cppsdk::ReportRow(fake, result, args_vector);
  }

  std::vector<std::string> GetAllFakes();

  std::vector<std::string> GetFakeArgs(const std::string& fake);

  std::string GetFakeResult(const std::string& fake);

 private:
  std::map<std::string, firebase::testing::cppsdk::ReportRow> reports_;
};

extern FakeReporterClass* FakeReporter;

#endif  // FIREBASE_TESTING_CPPSDK_REPORTER_IMPL_H_
