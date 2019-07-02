#ifndef FIREBASE_TESTING_CPPSDK_REPORTER_H_
#define FIREBASE_TESTING_CPPSDK_REPORTER_H_

#include <initializer_list>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace firebase {
namespace testing {
namespace cppsdk {

enum Platform {
  kAny = 0,
  kAndroid,
  kIos,
};

class ReportRow {
 public:
  ReportRow();
  ReportRow(std::string fake, std::string result,
            std::vector<std::string> args);
  ReportRow(std::string fake, std::string result, Platform platform,
            std::initializer_list<std::string> args);

  std::string getFake() const;
  std::string getResult() const;
  Platform getPlatform() const;
  std::string getPlatformString() const;
  std::vector<std::string> getArgs() const;

  bool operator==(const ReportRow& other) const;
  bool operator!=(const ReportRow& other) const;
  bool operator<(const ReportRow& other) const;

  std::string toString() const;

 private:
  std::string fake_;
  std::string result_;
  Platform platform_;
  std::vector<std::string> args_;
};

::std::ostream& operator<<(::std::ostream& os, const ReportRow& r);

class Reporter {
 public:
  FRIEND_TEST(ReporterTest, TestGetAllFakesAndroid);
  FRIEND_TEST(ReporterTest, TestGetFakeArgsAndroid);
  FRIEND_TEST(ReporterTest, TestGetFakeResultAndroid);

  void reset();
  void addExpectation(const ReportRow& expectation);
  void addExpectation(std::string fake, std::string result,
                      Platform platform,
                      std::initializer_list<std::string> args);

  std::vector<ReportRow> getExpectations();
  std::vector<ReportRow> getFakeReports();

 private:
  std::vector<ReportRow> expectations_;

  std::vector<std::string> getAllFakes();
  std::vector<std::string> getFakeArgs(const std::string& fake);
  std::string getFakeResult(const std::string& fake);
};

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_REPORTER_H_
