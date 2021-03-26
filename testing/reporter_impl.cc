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

#include "testing/reporter_impl.h"
#include "testing/reporter.h"

#include <map>
#include <string>
#include <vector>

FakeReporterClass* FakeReporter = new FakeReporterClass();

void FakeReporterClass::Reset() { reports_.clear(); }

void FakeReporterClass::AddReport(const char* fake,
                                  std::initializer_list<const char*> args) {
  AddReport(fake, "", args);
}

std::vector<std::string> FakeReporterClass::GetAllFakes() {
  std::vector<std::string> fakes;
  for (const auto& keyvalue : reports_) {
    fakes.push_back(keyvalue.first);
  }
  return fakes;
}

std::vector<std::string> FakeReporterClass::GetFakeArgs(
    const std::string& fake) {
  if (reports_.find(fake) == reports_.end()) return std::vector<std::string>();
  return reports_[fake].getArgs();
}

std::string FakeReporterClass::GetFakeResult(const std::string& fake) {
  if (reports_.find(fake) == reports_.end()) return "";
  return reports_[fake].getResult();
}

// iOS and Desktop specific functions implementation.
namespace firebase {
namespace testing {
namespace cppsdk {

void Reporter::reset() { FakeReporter->Reset(); }

std::vector<std::string> Reporter::getAllFakes() {
  return FakeReporter->GetAllFakes();
}

std::vector<std::string> Reporter::getFakeArgs(const std::string& fake) {
  return FakeReporter->GetFakeArgs(fake);
}

std::string Reporter::getFakeResult(const std::string& fake) {
  return FakeReporter->GetFakeResult(fake);
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
