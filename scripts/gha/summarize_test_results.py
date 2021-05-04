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

python summarize_test_results.py --dir <directory> [--markdown]

Example table mode output (will be slightly different with --markdown):

| Platform                  | Build failures | Test failures   |
|---------------------------|----------------|-----------------|
| iOS (build on iOS)        |                | auth, firestore |
| Desktop Windows (OpenSSL) | analytics      | database        |

python summarize_test_results.py --dir <directory> <--text_log | --github_log>

Example log mode output (will be slightly different with --github_log):

INTEGRATION TEST FAILURES

iOS (built on MacOS):
  Test failures (2):
  - auth
  - firestore
Desktop Windows (OpenSSL):
  Build failures (1):
  - analytics
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
    "File pattern (glob) for test results."
    "The '*' part is used to determine the platform.")

flags.DEFINE_bool(
    "include_successful", False,
    "Print all logs including successful tests.")

flags.DEFINE_bool(
    "markdown", False,
    "Display a Markdown-formatted table.")

flags.DEFINE_bool(
    "github_log", False,
    "Display a GitHub log list.")

flags.DEFINE_bool(
    "text_log", False,
    "Display a text log list.")

flags.DEFINE_integer(
    "list_max", 5,
    "In Markdown mode, collapse lists larger than this size. 0 to disable.")

CAPITALIZATIONS = {
    "macos": "MacOS",
    "ubuntu": "Linux",
    "windows": "Windows",
    "openssl": "(OpenSSL)",
    "boringssl": "(BoringSSL)",
    "ios": "iOS",
    "android": "Android",
    "desktop": "Desktop",
}

SIMULATOR = "simulator"
HARDWARE = "hardware"

PLATFORM_HEADER = "Platform"
BUILD_FAILURES_HEADER = "Build failures"
TEST_FAILURES_HEADER = "Test failures"
SUCCESSFUL_TESTS_HEADER = "Successful tests"

LOG_HEADER = "INTEGRATION TEST FAILURES"

# Default list separator for printing in text format.
DEFAULT_LIST_SEPARATOR=", "

def print_table(log_results,
            platform_width = 0,
            build_failures_width = 0,
            test_failures_width = 0,
            successful_width = 0,
            space_char = " ",
            list_separator = DEFAULT_LIST_SEPARATOR):
  """Print out a table in the requested format (text or markdown)."""
  # Print table header
  output_lines = list()
  headers = [
    re.sub(r'\b \b', space_char, PLATFORM_HEADER.ljust(platform_width)),
    re.sub(r'\b \b', space_char,BUILD_FAILURES_HEADER.ljust(build_failures_width)),
    re.sub(r'\b \b', space_char,TEST_FAILURES_HEADER.ljust(test_failures_width))
  ] + (
    [re.sub(r'\b \b', space_char,SUCCESSFUL_TESTS_HEADER.ljust(successful_width))]
    if FLAGS.include_successful else []
  )
  # Print header line.
  output_lines.append(("|" + " %s |" * len(headers)) % tuple(headers))
  # Print a |-------|-------|---------| line.
  output_lines.append(("|" + "-%s-|" * len(headers)) %
                      tuple([ re.sub("[^|]","-", header) for header in headers ]))

  # Iterate through platforms and print out table lines.
  for platform in sorted(log_results.keys()):
    if log_results[platform]["build_failures"] or log_results[platform]["test_failures"] or FLAGS.include_successful:
      columns = [
        re.sub(r'\b \b', space_char, platform.ljust(platform_width)),
        format_result(log_results[platform]["build_failures"], justify=build_failures_width, list_separator=list_separator),
        format_result(log_results[platform]["test_failures"], justify=test_failures_width, list_separator=list_separator),
      ] + (
        [format_result(log_results[platform]["successful"], justify=successful_width, list_separator=list_separator)]
        if FLAGS.include_successful else []
      )
      output_lines.append(("|" + " %s |" * len(headers)) % tuple(columns))

  return output_lines


def format_result(test_set, list_separator=DEFAULT_LIST_SEPARATOR, justify=0):
  """Format a list of test names.
  In Markdown mode, this can collapse a large list into a dropdown."""
  list_output = list_separator.join(sorted(test_set))
  if FLAGS.markdown and FLAGS.list_max > 0 and len(test_set) > FLAGS.list_max:
      return "<details><summary>_(%s items)_</summary>%s</details>" % (
        len(test_set), list_output)
  else:
    return list_output.ljust(justify)


def print_text_table(log_results):
  """Print out a nicely-formatted text table."""
  # For text formatting, see how wide the strings are so we can
  # left-justify each column of the text table.
  max_platform = len(PLATFORM_HEADER)
  max_build_failures = len(BUILD_FAILURES_HEADER)
  max_test_failures = len(TEST_FAILURES_HEADER)
  max_sucessful = len(SUCCESSFUL_TESTS_HEADER)
  for (platform, results) in log_results.items():
    max_platform = max(max_platform, len(platform))
    max_build_failures = max(max_build_failures,
                             len(format_result(log_results[platform]["build_failures"])))
    max_test_failures = max(max_test_failures,
                            len(format_result(log_results[platform]["test_failures"])))
    max_sucessful = max(max_sucessful,
                        len(format_result(log_results[platform]["successful"])))
  return print_table(log_results,
                     platform_width=max_platform,
                     build_failures_width=max_build_failures,
                     test_failures_width=max_test_failures,
                     successful_width=max_sucessful)


def print_log(log_results):
  """Print the results in a text-only log format."""
  output_lines = []
  for platform in sorted(log_results.keys()):
    if log_results[platform]["build_failures"] or log_results[platform]["test_failures"] or FLAGS.include_successful:
      output_lines.append("")
      output_lines.append("%s:" % platform)
      if (FLAGS.include_successful and len(log_results[platform]["successful"]) > 0):
        output_lines.append("  Successful tests (%d):" %
                            len(log_results[platform]["successful"]))
        for test_name in sorted(log_results[platform]["successful"]):
          output_lines.append("  - %s" % test_name)
      if (len(log_results[platform]["build_failures"]) > 0):
        output_lines.append("  Build failures (%d):" %
                            len(log_results[platform]["build_failures"]))
        for test_name in sorted(log_results[platform]["build_failures"]):
          output_lines.append("  - %s" % test_name)
      if (len(log_results[platform]["test_failures"]) > 0):
        output_lines.append("  Test failures (%d):" %
                            len(log_results[platform]["test_failures"]))
        for test_name in sorted(log_results[platform]["test_failures"]):
          output_lines.append("  - %s" % test_name)
  return output_lines[1:]  # skip first blank line


def print_github_log(log_results):
  """Print a text log, but replace newlines with %0A and add
  the GitHub ::error text."""
  output_lines = [LOG_HEADER,
                  "".ljust(len(LOG_HEADER), "—"),
                  ""] + print_log(log_results)
  # "%0A" produces a newline in GitHub workflow logs.
  return ["::error ::%s" % "%0A".join(output_lines)]


def print_markdown_table(log_results):
  """Print a normal table, but with a few changes:
  Separate test names by newlines, and replace certain spaces
  with HTML non-breaking spaces to prevent aggressive word-wrapping."""
  return print_table(log_results, space_char = "&nbsp;", list_separator = "<br/>")


def main(argv):
  if len(argv) > 1:
      raise app.UsageError("Too many command-line arguments.")

  log_files = glob.glob(os.path.join(FLAGS.dir, FLAGS.pattern))

  # Replace the "*" in the file glob with a regex capture group,
  # so we can report the test name.
  log_name_re = re.escape(
      os.path.join(FLAGS.dir,FLAGS.pattern)).replace("\\*", "(.*)")

  any_failures = False
  log_data = {}

  for log_file in log_files:
      # Extract the matrix name for this log.
      log_name = re.search(log_name_re, log_file).groups(1)[0]
      # Split the matrix name into components.
      log_name = re.sub(r'[-_.]+', ' ', log_name).split()
      # Remove redundant components.
      if "latest" in log_name: log_name.remove("latest")
      if "Android" in log_name or "iOS" in log_name:
          log_name.remove('openssl')
      # Capitalize components in a nice way.
      log_name = [
          CAPITALIZATIONS[name.lower()]
          if name.lower() in CAPITALIZATIONS
          else name
          for name in log_name]
      if "Android" in log_name or "iOS" in log_name:
        # For Android and iOS, highlight the target OS.
        log_name[0] = "(built on %s)" % log_name[0]
        if FLAGS.markdown:
          log_name[1] = "**%s**" % log_name[1]
      elif FLAGS.markdown:
        # For desktop, highlight the entire platform string.
        log_name[0] = "%s**" % log_name[0]
        log_name[1] = "**%s" % log_name[1]
      with open(log_file, "r") as log_reader:
        log_reader_data = log_reader.read()
        if "Android" in log_name[1] or "iOS" in log_name[1]:
          # Rejoin matrix name with spaces.
          log_name_str = ' '.join([log_name[1], SIMULATOR, log_name[0]]+log_name[2:])
          log_data[log_name_str] = log_reader_data
          # iOS and Android repeat the list for simulator and device
          log_name_str = ' '.join([log_name[1], HARDWARE, log_name[0]]+log_name[2:])
          log_data[log_name_str] = log_reader_data
        else:
          # Rejoin matrix name with spaces.
          log_name_str = ' '.join([log_name[1], log_name[0]]+log_name[2:])
          log_data[log_name_str] = log_reader_data

  log_results = {}
  # Go through each log and extract out the build and test failures.
  for (platform, log_text) in log_data.items():
    if platform not in log_results:
      log_results[platform] = { "build_failures": set(), "test_failures": set(),
                                "attempted": set(), "successful": set() }
    if "__SUMMARY_MISSING__" in log_text:
      log_results[platform]["build_failures"].add('[log file missing]')
    # Get a full list of the products built.
    m = re.search(r'TRIED TO BUILD: ([^\n]*)', log_text)
    if m:
      log_results[platform]["attempted"].update(m.group(1).split(","))
    # Extract build failure lines, which follow "SOME FAILURES OCCURRED:"
    m = re.search(r'SOME FAILURES OCCURRED:\n(([\d]+:[^\n]*\n)+)', log_text, re.MULTILINE)
    if m:
      for build_failure_line in m.group(1).strip("\n").split("\n"):
        m2 = re.match(r'[\d]+: ([^,]+)', build_failure_line)
        if m2:
          product_name = m2.group(1).lower()
          if product_name:
            log_results[platform]["build_failures"].add(product_name)
            any_failures = True

    test_summary_title = "TEST SUMMARY:"
    # If the log reports "(ON SIMULATOR/EMULATOR)" or "(ON REAL DEVICE VIA FTL)", make sure it matches the platform.
    if SIMULATOR in platform:
      test_summary_title = r"TEST SUMMARY \(ON SIMULATOR/EMULATOR\):"
    elif HARDWARE in platform:
      test_summary_title = r"TEST SUMMARY \(ON REAL DEVICE VIA FTL\):"
    # Extract test failures.
    # (.*) Greedy match, which follows "TESTAPPS FAILED:" and skips "TESTAPPS EXPERIENCED ERRORS:"
    # (.*?) Nongreedy match, which follows "TESTAPPS EXPERIENCED ERRORS:"
    pattern = r'^%s(.*?)TESTAPPS (EXPERIENCED ERRORS|FAILED):\n(([^\n]*\n)+)' % test_summary_title
    m = re.search(pattern, log_text, re.MULTILINE | re.DOTALL)
    if m:
      for test_failure_line in m.group(3).strip("\n").split("\n"):
        # Don't process this if meet another TEST SUMMARY
        if "TEST SUMMARY" in test_failure_line: break
        # Only get the lines showing paths.
        if "/firebase-cpp-sdk/" not in test_failure_line: continue
        test_filename = "";
        if "log tail" in test_failure_line:
          test_filename = re.match(r'^(.*) log tail', test_failure_line).group(1)
        elif "lacks logs" in test_failure_line:
          test_filename = re.match(r'^(.*) lacks logs', test_failure_line).group(1)
        elif "it-debug.apk" in test_failure_line:
          test_filename = re.match(r'^(.*it-debug\.apk)', test_failure_line).group(1)
        elif "integration_test.ipa" in test_failure_line:
          test_filename = re.match(r'^(.*integration_test\.ipa)', test_failure_line).group(1)
        elif "integration_test.app" in test_failure_line:
          test_filename = re.match(r'^(.*integration_test\.app)', test_failure_line).group(1)

        if test_filename:
          m2 = re.search(r'/ta/(firebase)?([^/]+)/iti?/', test_filename, re.IGNORECASE)
          if not m2: m2 = re.search(r'/testapps/(firebase)?([^/]+)/integration_test', test_filename, re.IGNORECASE)
          if m2:
            product_name = m2.group(2).lower()
            if product_name:
              log_results[platform]["test_failures"].add(product_name)
              any_failures = True

  # After processing all the logs, we can determine the successful builds for each platform.
  for platform in log_results.keys():
    log_results[platform]["successful"] = log_results[platform]["attempted"].difference(
      log_results[platform]["test_failures"].union(
        log_results[platform]["build_failures"]))

  # Also, if any simulator and hardware targets are identical, filter them.
  to_del = set()
  to_add = dict()
  for platform in log_results.keys():
    simulator_str = (" %s " % SIMULATOR)
    if simulator_str in platform:
      other_platform = platform.replace(simulator_str, " %s " % HARDWARE)
      if log_results[platform] == log_results[other_platform]:
        targetless = platform.replace(simulator_str, " ")
        to_add[targetless] = log_results[platform]
        to_del.add(platform)
        to_del.add(other_platform)
  for platform_to_del in to_del:
    del log_results[platform_to_del]
  log_results.update(to_add)

  if not any_failures and not FLAGS.include_successful:
    # No failures occurred, nothing to log.
    return(0)

  log_lines = []
  if FLAGS.markdown:
    log_lines = print_markdown_table(log_results)
    # If outputting Markdown, don't bother justifying the table.
  elif FLAGS.github_log:
    log_lines = print_github_log(log_results)
  elif FLAGS.text_log:
    log_lines = print_log(log_results)
  else:
    log_lines = print_text_table(log_results)

  print("\n".join(log_lines))

if __name__ == "__main__":
  flags.mark_flag_as_required("dir")
  app.run(main)
