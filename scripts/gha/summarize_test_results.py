# Copyright 2021 Google LLC
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

"""Summarize integration test results.

USAGE:

python summarize_test_results.py -d <directory>

Example output:

| Platform                | Build failures | Test failures   |
| ----------------------- | -------------- | --------------- |
| MacOS iOS               |                | auth, firestore |
| Windows Desktop OpenSSL | analytics      | database        |
"""

from absl import app
from absl import flags
from absl import logging
import glob
import re
import os

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "dir", None,
    "Directory containing test results.",
    short_name="d")

flags.DEFINE_string(
    "pattern", "test-results-*.txt",
    "File pattern (glob) for test results.")

flags.DEFINE_bool(
    "full", False,
    "Print a full table, including successful tests.")

flags.DEFINE_bool(
    "singleline", False,
    "Output a single line, with \n for newlines.")

flags.DEFINE_bool(
    "markdown", False,
    "Display a Markdown-formatted table.")

CAPITALIZATIONS = {
    "macos": "MacOS",
    "ubuntu": "Ubuntu",
    "windows": "Windows",
    "openssl": "OpenSSL",
    "boringssl": "BoringSSL",
    "ios": "iOS",
    "android": "Android",
}

PLATFORM_HEADER = "Platform"
BUILD_FAILURES_HEADER = "Build failures"
TEST_FAILURES_HEADER = "Test failures"

def main(argv):
  if len(argv) > 1:
      raise app.UsageError("Too many command-line arguments.")

  log_files = glob.glob(os.path.join(FLAGS.dir, FLAGS.pattern))

  # Replace the "*" in the file glob with a regex capture group,
  # so we can report the test name.
  log_name_re = re.escape(
      os.path.join(FLAGS.dir,FLAGS.pattern)).replace("\\*", "(.*)")

  log_data = {}

  for log_file in log_files:
      # Extract the matrix name for this log.
      log_name = re.search(log_name_re, log_file).groups(1)[0]
      # Split the matrix name into components.
      log_name = re.sub(r'[-_.]+', ' ', log_name).split()
      # Remove redundant components.
      if "Android" in log_name or "iOS" in log_name:
          log_name.remove('openssl')
      log_name.remove('latest')
      # Capitalize components in a nice way.
      log_name = [
          CAPITALIZATIONS[name.lower()]
          if name.lower() in CAPITALIZATIONS
          else name
          for name in log_name]
      # Rejoin matrix name with spaces.
      log_name = ' '.join(log_name)
      with open(log_file, "r") as log_reader:
          log_data[log_name] = log_reader.read()

  log_results = {}
  # Go through each log and extract out the build and test failures.
  for (platform, log_text) in log_data.items():
      log_results[platform] = { "build_failures": set(), "test_failures": set() }
      # Extract build failure lines, which follow "SOME FAILURES OCCURRED:"
      m = re.search(r'SOME FAILURES OCCURRED:\n(([\d+]:[^\n]*\n)+)', log_text, re.MULTILINE)
      if m:
        for build_failure_line in m.group(1).strip("\n").split("\n"):
          m2 = re.match(r'[\d]+: ([^,]+)', build_failure_line)
          if m2:
            product_name = m2.group(1).lower()
            if product_name:
              log_results[platform]["build_failures"].add(product_name)
      # Extract test failures, which follow "TESTAPPS EXPERIENCED ERRORS:"
      m = re.search(r'TESTAPPS EXPERIENCED ERRORS:\n(([^\n]*\n)+)', log_text, re.MULTILINE)
      if m:
        for test_failure_line in m.group(1).strip("\n").split("\n"):
          # Only get the lines showing paths.
          if "/firebase-cpp-sdk/" not in test_failure_line: continue
          test_filename = "";
          if "log tail" in test_failure_line:
            test_filename = re.match(r'^(.*) log tail', test_failure_line).group(1)
          if "lacks logs" in test_failure_line:
            test_filename = re.match(r'^(.*) lacks logs', test_failure_line).group(1)
          if test_filename:
            m2 = re.search(r'/ta/(firebase)?([^/]+)/iti?/', test_filename, re.IGNORECASE)
            if not m2: m2 = re.search(r'/testapps/(firebase)?([^/]+)/integration_test', test_filename, re.IGNORECASE)
            if m2:
              product_name = m2.group(2).lower()
              if product_name:
                log_results[platform]["test_failures"].add(product_name)
          

  
  # For text formatting, see how wide the strings are.
  max_platform = len(PLATFORM_HEADER)
  max_build_failures = len(BUILD_FAILURES_HEADER)
  max_test_failures = len(TEST_FAILURES_HEADER)
  for (platform, results) in log_results.items():
    build_failures = ", ".join(sorted(log_results[platform]["build_failures"]))
    test_failures = ", ".join(sorted(log_results[platform]["test_failures"]))
    max_platform = max(max_platform, len(platform))
    max_build_failures = max(max_build_failures, len(build_failures))
    max_test_failures = max(max_test_failures, len(test_failures))

  space_char = "&nbsp;" if FLAGS.markdown else " ";
  output_lines = list()
  output_lines.append("| %s | %s | %s |" % (
    re.sub(r'\b \b', space_char, PLATFORM_HEADER.ljust(max_platform)),
    re.sub(r'\b \b', space_char,BUILD_FAILURES_HEADER.ljust(max_build_failures)),
    re.sub(r'\b \b', space_char,TEST_FAILURES_HEADER.ljust(max_test_failures))))
  output_lines.append("|-%s-|-%s-|-%s-|" % (
    "".ljust(max_platform, "-"),
    "".ljust(max_build_failures, "-"),
    "".ljust(max_test_failures, "-")))

  for platform in sorted(log_results.keys()):
    if log_results[platform]["build_failures"] or log_results[platform]["test_failures"]:
      platform_str = re.sub(r'\b \b', space_char, platform.ljust(max_platform))
      build_failures = ", ".join(sorted(log_results[platform]["build_failures"])).ljust(max_build_failures)
      test_failures = ", ".join(sorted(log_results[platform]["test_failures"])).ljust(max_test_failures)
      output_lines.append("| %s | %s | %s |" % (platform_str, build_failures, test_failures))

  output_delim = "\\n" if FLAGS.singleline else "\n"
  print(output_delim.join(output_lines))

if __name__ == "__main__":
  flags.mark_flag_as_required("dir")
  app.run(main)
