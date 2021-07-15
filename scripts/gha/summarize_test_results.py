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

python summarize_test_results.py --dir <directory> --markdown

Example table mode output (with --markdown):

| Failures   |                    Configs                         |
|------------|----------------------------------------------------|
| missing_log|[BUILD] [ERROR] [Windows] [boringssl]               |
| messaging  |[BUILD] [ERROR] [Windows] [openssl]                 |
|            |[TEST] [ERROR] [Android] [All os] [emulator_min]    |
|            |[TEST] [FAILURE] [Android] [macos] [emulator_target]|
|            |▼(1 failed tests)                                   |

python summarize_test_results.py --dir <directory> [--github_log]

Example log mode output (will be slightly different with --github_log):

INTEGRATION TEST FAILURES

admob:
  Errors and Failures (1):
  - [BUILD] [ERROR] [Windows] [boringssl]
functions:
  Errors and Failures (2):
  - [BUILD] [ERROR] [Windows] [boringssl]
  - [TEST] [FAILURE] [iOS] [macos] [simulator_min, simulator_target]
    - failed tests: ['TestSignIn', 'TestFunction', 'TestFunctionWithData']
"""

from absl import app
from absl import flags
from absl import logging
from print_matrix_configuration import PARAMETERS
from print_matrix_configuration import BUILD_CONFIGS

import glob
import re
import os
import json

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "dir", None,
    "Directory containing test results.",
    short_name="d")

flags.DEFINE_bool(
    "markdown", False,
    "Display a Markdown-formatted table.")

flags.DEFINE_bool(
    "github_log", False,
    "Display a GitHub log list.")

LIST_MAX = 3

CAPITALIZATIONS = {
    "macos": "MacOS",
    "ubuntu": "Linux",
    "windows": "Windows",
    "ios": "iOS",
    "tvos": "tvOS",
    "android": "Android",
    "desktop": "Desktop",
}

BUILD_FILE_PATTERN = "build-results-*.log.json"
TEST_FILE_PATTERN = "test-results-*.log.json"

TESTAPPS_HEADER = "Failures"
CONFIGS_HEADER = "Configs"
MISSING_LOG = "missing_log"

LOG_HEADER = "INTEGRATION TEST FAILURES"

# Default list separator for printing in text format.
DEFAULT_LIST_SEPARATOR=", "

def print_table(log_results,
            testapps_width = 0,
            configs_width = 0,
            space_char = " ",
            list_separator = DEFAULT_LIST_SEPARATOR):
  """Print out a table in the requested format (text or markdown)."""
  # Print table header
  output_lines = list()
  headers = [
    re.sub(r'\b \b', space_char, TESTAPPS_HEADER.ljust(testapps_width)),
    re.sub(r'\b \b', space_char,CONFIGS_HEADER.ljust(configs_width))
  ]
  # Print header line.
  output_lines.append(("|" + " %s |" * len(headers)) % tuple(headers))
  # Print a |-------|---------| line.
  output_lines.append(("|" + "-%s-|" * len(headers)) %
                      tuple([ re.sub("[^|]","-", header) for header in headers ]))

  if MISSING_LOG in log_results.keys():
    columns = [
      re.sub(r'\b \b', space_char, MISSING_LOG.ljust(testapps_width)),
      format_result(log_results.pop(MISSING_LOG), space_char=space_char, list_separator=list_separator, justify=configs_width)
    ]
    output_lines.append(("|" + " %s |" * len(headers)) % tuple(columns))
  
  # Iterate through platforms and print out table lines.
  for testapp in sorted(log_results.keys()):
    columns = [
      re.sub(r'\b \b', space_char, testapp.ljust(testapps_width)),
      format_result(log_results[testapp], space_char=space_char, list_separator=list_separator, justify=configs_width)
    ]
    output_lines.append(("|" + " %s |" * len(headers)) % tuple(columns))

  return output_lines


def format_result(config_tests, space_char = " ", list_separator=DEFAULT_LIST_SEPARATOR, justify=0):
  """Format a list of test names.
  In Markdown mode, this can collapse a large list into a dropdown."""
  config_set = set()
  for config, tests in config_tests.items():
    if len(tests) > 0:
      count = len(tests)
      tests = [space_char+space_char+test for test in tests]
      tests = list_separator.join(sorted(tests))
      tests = "<details><summary>(%s failed tests)</summary>%s</details>" % (count, tests)
      config_set.add(config+tests)
    else:
      config_set.add(config+list_separator)

  if len(config_set) > LIST_MAX:
      return "<details><summary>(%s items)</summary>%s</details>" % (
        len(config_set), "".join(sorted(config_set)))
  else:
    return  "".join(sorted(config_set)).ljust(justify)


def print_log(log_results):
  """Print the results in a text-only log format."""
  output_lines = []
  for (testapps, configs) in log_results.items():
      output_lines.append("")
      output_lines.append("%s:" % testapps)
      if len(configs) > 0:
        output_lines.append("  Errors and Failures (%d):" %
                            len(configs))
      for config, extra in configs.items():
        output_lines.append("  - %s" % config)
        if extra:
          output_lines.append("    - failed tests: %s" % extra)
  return output_lines[1:]  # skip first blank line


def print_github_log(log_results):
  """Print a text log, but replace newlines with %0A and add
  the GitHub ::error text."""
  output_lines = ["::error ::%s" % LOG_HEADER,
                  "".ljust(len(LOG_HEADER), "—"),
                  ""] + print_log(log_results)
  # "%0A" produces a newline in GitHub workflow logs.
  return ["%0A".join(output_lines)]


def print_markdown_table(log_results):
  """Print a normal table, but with a few changes:
  Separate test names by newlines, and replace certain spaces
  with HTML non-breaking spaces to prevent aggressive word-wrapping."""
  return print_table(log_results, space_char = "&nbsp;", list_separator = "<br/>")


def main(argv):
  if len(argv) > 1:
      raise app.UsageError("Too many command-line arguments.")
  summarize_logs(FLAGS.dir, FLAGS.markdown, FLAGS.github_log)


def summarize_logs(dir, markdown=False, github_log=False):
  build_log_files = glob.glob(os.path.join(dir, BUILD_FILE_PATTERN))
  test_log_files = glob.glob(os.path.join(dir, TEST_FILE_PATTERN))
  # Replace the "*" in the file glob with a regex capture group,
  # so we can report the test name.
  build_log_name_re = re.escape(
      os.path.join(dir,BUILD_FILE_PATTERN)).replace("\\*", "(.*)")
  test_log_name_re = re.escape(
      os.path.join(dir,TEST_FILE_PATTERN)).replace("\\*", "(.*)")

  any_failures = False
  log_data = {}
  # log_data format:
  #   { testapps: {"build": [configs]},
  #               {"test": {"errors": [configs]},
  #                        {"failures": {failed_test: [configs]}}}}
  for build_log_file in build_log_files:
    configs = get_configs_from_file_name(build_log_file, build_log_name_re)
    with open(build_log_file, "r") as log_reader:
      log_text = log_reader.read()
      if "__SUMMARY_MISSING__" in log_text:
        any_failures = True
        log_data.setdefault(MISSING_LOG, {}).setdefault("build", []).append(configs)
      else:
        log_reader_data = json.loads(log_text)
        for (testapp, error) in log_reader_data["errors"].items():
          any_failures = True
          log_data.setdefault(testapp, {}).setdefault("build", []).append(configs)

  for test_log_file in test_log_files:
    configs = get_configs_from_file_name(test_log_file, test_log_name_re)
    with open(test_log_file, "r") as log_reader:
      log_text = log_reader.read()
      if "__SUMMARY_MISSING__" in log_text:
        any_failures = True
        log_data.setdefault(MISSING_LOG, {}).setdefault("test", {}).setdefault("errors", []).append(configs)
      else:
        log_reader_data = json.loads(log_text)
        for (testapp, error) in log_reader_data["errors"].items():
          any_failures = True
          log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("errors", []).append(configs)
        for (testapp, failures) in log_reader_data["failures"].items():
          for (test, failure) in failures["failed_tests"].items():
            any_failures = True
            log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("failures", {}).setdefault(test, []).append(configs)

  # log_results format:
  #   { testapps: {configs: [failed tests]} }
  log_results = reorganize_log(log_data)

  if not any_failures:
    # No failures occurred, nothing to log.
    return(0)

  log_lines = []
  if markdown:
    log_lines = print_markdown_table(log_results)
    # If outputting Markdown, don't bother justifying the table.
  elif github_log:
    log_lines = print_github_log(log_results)
  else:
    log_lines = print_log(log_results)

  log_summary = "\n".join(log_lines)
  print(log_summary)
  return log_summary


def get_configs_from_file_name(file_name, file_name_re):
  # Extract the matrix name for this log.
  configs = re.search(file_name_re, file_name).groups(1)[0]
  # Split the matrix name into components.
  configs = re.sub('-', ' ', configs).split()
  # Remove redundant components. e.g. "latest" in "windows-latest"
  del configs[2]
  if "desktop" in configs: configs.remove("desktop")
  return configs

def reorganize_log(log_data):
  log_results = {}
  for (testapp, errors) in log_data.items():
    if errors.get("build"):
      combined_configs = combine_configs(errors.get("build"))
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["BUILD"], ["ERROR"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), [])
          
    if errors.get("test",{}).get("errors"):
      combined_configs = combine_configs(errors.get("test",{}).get("errors"))
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["TEST"], ["ERROR"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), [])
    for (test, configs) in errors.get("test",{}).get("failures",{}).items():
      combined_configs = combine_configs(configs)
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["TEST"], ["FAILURE"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), []).append(test)
  
  return log_results


# Combine Config Lists
# e.g.
#     [['macos', 'simulator_min'], ['macos', 'simulator_target']]
#     -> [[['macos'], ['simulator_min', 'simulator_target']]]
def combine_configs(configs):
  platform_configs = {}
  for config in configs:
    platform = config[0]
    config = config[1:]
    platform_configs.setdefault(platform, []).append(config)

  for (platform, configs_list) in platform_configs.items():
    # combine kth config in configs
    for k in range(len(configs_list[0])):
      # once configs combined, keep the first config and remove the rest
      remove_configs = []
      for i in range(len(configs_list)):
        # once configs combined, update the kth config
        configk = []
        configk.append(configs_list[i][k])
        configs_i = configs_list[i][:k] + configs_list[i][k+1:]
        for j in range(i+1, len(configs_list)):
          if not contains(remove_configs, configs_list[j]):
            configs_j = configs_list[j][:k] + configs_list[j][k+1:]
            if equals(configs_i, configs_j):
              remove_configs.append(configs_list[j])
              configk.append(configs_list[j][k])
        configk = combine_config(configk, platform, k)
        configs_list[i][k] = configk
      for config in remove_configs:
        configs_list.remove(config)

  return platform_configs


# If possible, combine kth config to "All *"
# e.g.
#     ['ubuntu', 'windows', 'macos']
#     -> ['All os']
def combine_config(config, platform, k):
  if k == 1 and platform in ("android", "ios", "tvos"):
    # config_name = test_device here
    k = -1
  config_name = BUILD_CONFIGS[platform][k]
  config_value = PARAMETERS["integration_tests"]["matrix"][config_name]
  if len(config_value) > 1 and len(config) == len(config_value):
    config = ["All %s" % config_name]
  elif config_name == "ios_device":
    ftl_devices = {"ios_min", "ios_target", "ios_latest"}
    simulators = {"simulator_min", "simulator_target", "simulator_latest"}
    if ftl_devices.issubset(set(config)):
      config.insert(0, "All FTL Devices")
      config = [x for x in config if (x not in ftl_devices)]
    elif simulators.issubset(set(config)):
      config.insert(0, "All Simulators")
      config = [x for x in config if (x not in simulators)]
  elif config_name == "android_device":
    ftl_devices = {"android_min", "android_target", "android_latest"}
    emulators = {"emulator_min", "emulator_target", "emulator_latest"}
    if ftl_devices.issubset(set(config)):
      config.insert(0, "All FTL Devices")
      config = [x for x in config if (x not in ftl_devices)]
    elif emulators.issubset(set(config)):
      config.insert(0, "All Emulators")
      config = [x for x in config if (x not in emulators)]

  return config


# Flat Config List and return it as a string
# e.g.
#     [['TEST'], ['FAILURE'], ['iOS'], ['macos'], ['simulator_min', 'simulator_target']]
#     -> '[TEST] [FAILURE] [iOS] [macos] [simulator_min, simulator_target]'
def flat_config(configs):
  configs = [str(conf).replace('\'','') for conf in configs]
  return " ".join(configs)


def contains(configs_list, configs):
  for x in range(len(configs_list)):
    if equals(configs_list[x], configs):
      return True
  return False


def equals(configs_i, configs_j):
  for x in range(len(configs_i)):
    if set(configs_i[x]) != set(configs_j[x]):
      return False
  return True


if __name__ == "__main__":
  flags.mark_flag_as_required("dir")
  app.run(main)
