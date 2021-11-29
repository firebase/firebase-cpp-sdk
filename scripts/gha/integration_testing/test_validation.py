# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Provides logic for determining results in a C++/Unity testapp.

The initial motivation for this was when sending testapps to Firebase Test Lab.
When using Game Loops, FTL will only report whether the app ran to completion,
i.e. didn't crash or timeout, but is unable to know whether the internal
test runners reported success or failure.

To validate this, the log output is parsed for the test summary generated by
each internal test runner. For C++ this was gTest, for Unity it was a custom
runner. In both cases, the logic works by searching for text indicating the end
of the internal tests, and then parsing the result summary.

In cases where the test summary does not exist (crash, or timeout) a tail of the
log is obtained instead, to help identify where the crash or timeout occurred.

"""

import datetime
import os
import re
import json

from absl import logging

import attr

UNITY = "unity"
CPP = "cpp"


def validate_results(log_text, platform):
  """Determines the results in the log output of a testapp.

  Args:
    log_text (str): Log output from a testapp. Can be None.
    platform (str): What type of testapp generated the log: 'unity' or 'cpp'.

  Returns:
    (TestResults): Structured results from the log.

  """
  if not log_text:
    return TestResults(complete=False)
  if platform == UNITY:
    return validate_results_unity(log_text)
  elif platform == CPP:
    return validate_results_cpp(log_text)
  else:
    raise ValueError("Invalid platform: %s." % platform)


def validate_results_unity(log_text):
  """Determines the results in the log output of a Unity testapp.

  Args:
    log_text (str): Log output from a Unity testapp.

  Returns:
    (TestResults): Structured results from the log.

  """
  results_match = re.search(
      r"PASS: (?P<pass>[0-9]+), FAIL: (?P<fail>[0-9]+)", log_text)
  match_dict = results_match.groupdict()
  if results_match:
    # After the result string comes a list of failing testapps.
    summary = log_text[results_match.start():]
  else:
    summary = _tail(log_text, 15)
  return TestResults(
      complete=bool(results_match),
      passes=0 if not results_match else int(match_dict["pass"]),
      fails=0 if not results_match else int(match_dict["fail"]),
      skips=0,
      summary=summary)


def validate_results_cpp(log_text):
  """Determines the results in the log output of a C++ testapp.

  Args:
    log_text (str): Log output from a C++ testapp.

  Returns:
    (TestResults): Structured results from the log.

  """
  # The gtest runner dumps a useful summary of tests after the tear down.
  end_marker = "Global test environment tear-down"
  complete = end_marker in log_text

  if complete:
    # rpartition splits a string into three components around the final
    # occurrence of the end marker, returning a triplet (before, marker, after)
    result_summary = log_text.rpartition(end_marker)[2].lstrip()

    passes = re.search(r"\[  PASSED  \] (?P<count>[0-9]+) test", result_summary)
    fails = re.search(r"\[  FAILED  \] (?P<count>[0-9]+) test", result_summary)
    skips = re.search(r"\[  SKIPPED \] (?P<count>[0-9]+) test", result_summary)
  else:
    result_summary = _tail(log_text, 15)
    passes = None
    fails = None
    skips = None

  return TestResults(
      complete=complete,
      passes=0 if not passes else int(passes.group("count")),
      fails=0 if not fails else int(fails.group("count")),
      skips=0 if not skips else int(skips.group("count")),
      summary=result_summary)


def summarize_test_results(tests, platform, summary_dir, file_name="summary.log", extra_info=""):
  """Summarizes and logs test results for multiple tests.

  Each 'test' should be an object with properties "testapp_path", which
  is a path to the binary, and "logs", which is a string containing the logs
  from that test.

  This will compute the results from each log's internal test summary. If
  the logs do not contain a test summary, that test's result will be "error".

  In addition to logging results, this will append a short summary to a file
  in summary_dir.

  Args:
    tests (Test): Objects containing str properties "testapp_path" and "logs".
    platform (str): What type of testapp generated the log: 'unity' or 'cpp'.
    summary_dir (str): Directory in which to append the summary file.

  Returns:
    (int): Return code. 0 for all success, 1 for any failures or errors.

  """
  successes = []
  failures = []
  errors = []
  success_testapp_paths = set()

  for test in tests:
    results = validate_results(test.logs, platform)
    test_result_pair = (test, results)
    if not results.complete:
      errors.append(test_result_pair)
    elif results.fails > 0:
      failures.append(test_result_pair)
    else:
      successes.append(test_result_pair)
      success_testapp_paths.add(test.testapp_path)

  # First log the successes, then the failures and errors, then the summary.
  # This way, debugging will involve checking the summary at the bottom,
  # then scrolling up for more information and seeing the failures first.
  #
  # For successes, just report the internal test summary, to reduce noise.
  # For failures, log the entire output for full diagnostic context.
  for test, results in successes:
    logging.info("%s:\n%s", test.testapp_path, results.summary)
  for test, _ in failures:
    logging.info("%s failed:\n%s", test.testapp_path, test.logs)
  for test, _ in errors:
    logging.info("%s didn't finish normally.\n%s", test.testapp_path, test.logs)

  # Testapps that failed first, but succeed after retry. (max 3 retry)
  flaky_testapps = []
  failures_exclude_flakiness = []
  errors_exclude_flakiness = []

  # The summary is much more terse, to minimize the time it takes to understand
  # what went wrong, without necessarily providing full debugging context.
  summary = []
  summary.append("TEST SUMMARY%s:" % extra_info)
  if successes:
    summary.append("%d TESTAPPS SUCCEEDED:" % len(successes))
    summary.extend((test.testapp_path for (test, _) in successes))
  if errors:
    summary.append("\n%d TESTAPPS EXPERIENCED ERRORS:" % len(errors))
    for test, results in errors:
      summary.append("\n%s:" % test.testapp_path)
      if test.testapp_path in success_testapp_paths:
        summary.append("THIS TESTAPP IS FLAKY")
        flaky_testapps.append((test, results))
      else:
        errors_exclude_flakiness.append((test, results))
      if hasattr(test, "ftl_link") and test.ftl_link:
        summary.append("ftl_link: %s" % test.ftl_link)
      if hasattr(test, "raw_result_link") and test.raw_result_link:
        summary.append("raw_result_link: %s" % test.raw_result_link)
      if results.summary:
        summary.append("log tail:")
        summary.append(results.summary)
      else:
        summary.append(
            "%s lacks logs (crashed, not found, etc)" % test.testapp_path)
  if failures:
    summary.append("\n%d TESTAPPS FAILED:" % len(failures))
    for test, results in failures:
      summary.append("\n%s:" % test.testapp_path)
      if test.testapp_path in success_testapp_paths:
        summary.append("THIS TESTAPP IS FLAKY")
        flaky_testapps.append((test, results))
      else:
        failures_exclude_flakiness.append((test, results))
      if hasattr(test, "ftl_link") and test.ftl_link:
        summary.append("ftl_link: %s" % test.ftl_link)
      if hasattr(test, "raw_result_link") and test.raw_result_link:
        summary.append("raw_result_link: %s" % test.raw_result_link)
      summary.append(results.summary)
  summary.append(
      "%d TESTAPPS TOTAL: %d PASSES, %d FAILURES, %d ERRORS"
      % (len(tests), len(successes), len(failures), len(errors)))

  if len(flaky_testapps) > 0 and len(flaky_testapps) == len(failures) + len(errors):
    logging.info("All failures and errors are due to flakiness.")
    summary.append("ALL THE FOLLOWING FAILURES AND ERRORS ARE DUE TO FLAKINESS:(")

  # summary_json format:
  #   { "type": "test",
  #     "testapps": [testapp],
  #     "errors": {testapp:{"logs": [error_log], "ftl_links": [ftl_link], "raw_result_links": [raw_result_link]}},
  #     "failures": {testapp:{"logs": [error_log], "ftl_links": [ftl_link], "raw_result_links": [raw_result_link],
  #                           "failed_tests": {falied_test: test_log}}},
  #     "flakiness": {testapp:{"logs": [error_log], "ftl_links": [ftl_link], "raw_result_links": [raw_result_link],
  #                           "flaky_tests": {flaky_test: test_log}}}}
  summary_json = {}
  summary_json["type"] = "test"
  summary_json["testapps"] = [get_name(test.testapp_path) for test in tests]
  summary_json["errors"] = {get_name(test.testapp_path):{"logs": [], "ftl_links": [], "raw_result_links": []} for (test, _) in errors_exclude_flakiness}
  for (test, results) in errors_exclude_flakiness:
    testapp = get_name(test.testapp_path)
    summary_json["errors"][testapp]["logs"].append(results.summary)
    if hasattr(test, "ftl_link") and test.ftl_link:
      summary_json["errors"][testapp]["ftl_links"].append(test.ftl_link)
    if hasattr(test, "raw_result_link") and test.raw_result_link:
      summary_json["errors"][testapp]["raw_result_links"].append(test.raw_result_link)
  summary_json["failures"] = {get_name(test.testapp_path):{"logs": [], "ftl_links": [], "raw_result_links": [], "failed_tests": dict()} for (test, _) in failures_exclude_flakiness}
  for (test, results) in failures_exclude_flakiness:
    testapp = get_name(test.testapp_path)
    summary_json["failures"][testapp]["logs"].append(results.summary)
    if hasattr(test, "ftl_link") and test.ftl_link:
      summary_json["failures"][testapp]["ftl_links"].append(test.ftl_link)
    if hasattr(test, "raw_result_link") and test.raw_result_link:
      summary_json["failures"][testapp]["raw_result_links"].append(test.raw_result_link)
    failed_tests = re.findall(r"\[  FAILED  \] (.+)[.](.+)", results.summary)
    for failed_test in failed_tests:
      failed_test = failed_test[0] + "." + failed_test[1]
      pattern = fr'\[ RUN      \] {failed_test}(.*?)\[  FAILED  \] {failed_test}'
      failure_log = re.search(pattern, test.logs, re.MULTILINE | re.DOTALL)
      summary_json["failures"][testapp]["failed_tests"][failed_test] = failure_log.group()
      summary.append("\n%s FAILED:\n%s\n" % (failed_test, failure_log.group()))
  summary_json["flakiness"] = {get_name(test.testapp_path):{"logs": [], "ftl_links": [], "raw_result_links": [], "flaky_tests": dict()} for (test, _) in flaky_testapps}
  for (test, results) in flaky_testapps:
    testapp = get_name(test.testapp_path)
    summary_json["flakiness"][testapp]["logs"].append(results.summary)
    if hasattr(test, "ftl_link") and test.ftl_link:
      summary_json["flakiness"][testapp]["ftl_links"].append(test.ftl_link)
    if hasattr(test, "raw_result_link") and test.raw_result_link:
      summary_json["flakiness"][testapp]["raw_result_links"].append(test.raw_result_link)
    flaky_tests = re.findall(r"\[  FAILED  \] (.+)[.](.+)", results.summary)
    for flaky_test in flaky_tests:
      flaky_test = flaky_test[0] + "." + flaky_test[1]
      pattern = fr'\[ RUN      \] {flaky_test}(.*?)\[  FAILED  \] {flaky_test}'
      failure_log = re.search(pattern, test.logs, re.MULTILINE | re.DOTALL)
      if failure_log:
        summary_json["flakiness"][testapp]["flaky_tests"][flaky_test] = failure_log.group()
        summary.append("\n%s FAILED:\n%s\n" % (flaky_test, failure_log.group()))

  with open(os.path.join(summary_dir, file_name+".json"), "a") as f:
    f.write(json.dumps(summary_json, indent=2))

  summary = "\n".join(summary)
  write_summary(summary_dir, summary, file_name)

  # success or only flakiness
  if len(tests) == len(successes) or len(flaky_testapps) == len(failures) + len(errors):
    return 0
  else:
    return 1


def write_summary(testapp_dir, summary, file_name="summary.log"):
  """Writes a summary of tests/builds to a file in the testapp directory.

  This will append the given summary to a file in the testapp directory,
  along with a timestamp. This summary's primary purpose is to aggregate
  results from separate steps on CI to be logged in a single summary step.

  Args:
    testapp_dir (str): Path to the directory of testapps being built or tested.
    summary (str): Short, readable multi-line summary of results.

  """
  # This method serves as the source of truth on where to put the summary.
  with open(os.path.join(testapp_dir, file_name), "a") as f:
    # The timestamp mainly helps when running locally: if running multiple
    # tests on the same directory, the results will accumulate, with a timestamp
    # to help keep track of when a given test was run.
    timestamp = datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
    f.write("\n%s\n%s\n" % (timestamp, summary))


def get_name(testapp_path):
  """Returns testapp api."""
  return testapp_path.split(os.sep)[-2]


def _tail(text, n):
  """Returns the last n lines in text, or all of text if too few lines."""
  return "\n".join(text.splitlines()[-n:])


@attr.s(frozen=True, eq=False)
class TestResults(object):
  complete = attr.ib()  # Did the testapp reach the end, or did it crash/timeout
  passes = attr.ib(default=0)
  fails = attr.ib(default=0)
  skips = attr.ib(default=0)
  summary = attr.ib(default="")  # Summary from internal runner OR tail of log
