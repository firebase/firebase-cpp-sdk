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
