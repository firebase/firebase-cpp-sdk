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

analytics:
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
from print_matrix_configuration import TEST_DEVICES

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

  success_or_only_flakiness = True
  log_data = {}
  # log_data format:
  #   { testapps: {"build": [configs]},
  #               {"test": {"errors": [configs]},
  #                        {"failures": {failed_test: [configs]}},
  #                        {"flakiness": {flaky_test: [configs]}}}}
  all_tested_configs = { "build_configs": [], "test_configs": []}
  for build_log_file in build_log_files:
    configs = get_configs_from_file_name(build_log_file, build_log_name_re)
    all_tested_configs["build_configs"].append(configs)
    with open(build_log_file, "r") as log_reader:
      log_text = log_reader.read()
      if "__SUMMARY_MISSING__" in log_text:
        success_or_only_flakiness = False
        log_data.setdefault(MISSING_LOG, {}).setdefault("build", []).append(configs)
      else:
        log_reader_data = json.loads(log_text)
        for (testapp, _) in log_reader_data["errors"].items():
          success_or_only_flakiness = False
          log_data.setdefault(testapp, {}).setdefault("build", []).append(configs)

  for test_log_file in test_log_files:
    configs = get_configs_from_file_name(test_log_file, test_log_name_re)
    all_tested_configs["test_configs"].append(configs)
    with open(test_log_file, "r") as log_reader:
      log_text = log_reader.read()
      if "__SUMMARY_MISSING__" in log_text:
        success_or_only_flakiness = False
        log_data.setdefault(MISSING_LOG, {}).setdefault("test", {}).setdefault("errors", []).append(configs)
      else:
        log_reader_data = json.loads(log_text)
        for (testapp, _) in log_reader_data["errors"].items():
          success_or_only_flakiness = False
          log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("errors", []).append(configs)
        for (testapp, failures) in log_reader_data["failures"].items():
          for (test, _) in failures["failed_tests"].items():
            success_or_only_flakiness = False
            log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("failures", {}).setdefault(test, []).append(configs)
        for (testapp, flakiness) in log_reader_data["flakiness"].items():
          if flakiness["flaky_tests"].items():
            for (test, _) in flakiness["flaky_tests"].items():
              log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("flakiness", {}).setdefault(test, []).append(configs)
          else:
            log_data.setdefault(testapp, {}).setdefault("test", {}).setdefault("flakiness", {}).setdefault("CRASH/TIMEOUT", []).append(configs)

  if success_or_only_flakiness and not log_data:
    # No failures and no flakiness occurred, nothing to log.
    return (success_or_only_flakiness, None)

  # if failures (include flakiness) exist:
  # log_results format:
  #   { testapps: {configs: [failed tests]} }
  all_tested_configs = reorganize_all_tested_configs(all_tested_configs)
  logging.info("all_tested_configs: %s", all_tested_configs)
  log_results = reorganize_log(log_data, all_tested_configs)
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
  return (success_or_only_flakiness, log_summary)


def get_configs_from_file_name(file_name, file_name_re):
  # Extract the matrix name for this log.
  configs = re.search(file_name_re, file_name).groups(1)[0]
  # Split the matrix name into components.
  configs = re.sub('-', ' ', configs).split()
  # Remove redundant components. e.g. "latest" in "windows-latest"
  del configs[2]
  if "desktop" in configs: configs.remove("desktop")
  if "gameloop" in configs: configs.remove("gameloop")
  if "uitest" in configs: configs.remove("uitest")
  return configs


def reorganize_all_tested_configs(tested_configs):
  build_configs = reorganize_configs(tested_configs["build_configs"])
  test_configs = reorganize_configs(tested_configs["test_configs"])
  return { "build_configs": build_configs, "test_configs": test_configs}


def reorganize_log(log_data, all_tested_configs):
  log_results = {}
  for (testapp, errors) in log_data.items():
    if errors.get("build"):
      reorganized_configs = reorganize_configs(errors.get("build"))
      combined_configs = combine_configs(reorganized_configs, all_tested_configs["build_configs"])
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["BUILD"], ["ERROR"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), [])
          
    if errors.get("test",{}).get("errors"):
      reorganized_configs = reorganize_configs(errors.get("test",{}).get("errors"))
      combined_configs = combine_configs(reorganized_configs, all_tested_configs["test_configs"])
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["TEST"], ["ERROR"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), [])
    for (test, configs) in errors.get("test",{}).get("failures",{}).items():
      reorganized_configs = reorganize_configs(configs)
      combined_configs = combine_configs(reorganized_configs, all_tested_configs["test_configs"])
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["TEST"], ["FAILURE"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), []).append(test)
    for (test, configs) in errors.get("test",{}).get("flakiness",{}).items():
      reorganized_configs = reorganize_configs(configs)
      combined_configs = combine_configs(reorganized_configs, all_tested_configs["test_configs"])
      for (platform, configs) in combined_configs.items():
        for config in configs:
          all_configs = [["TEST"], ["FLAKINESS"], [CAPITALIZATIONS[platform]]]
          all_configs.extend(config)
          log_results.setdefault(testapp, {}).setdefault(flat_config(all_configs), []).append(test)
  
  return log_results


# Reorganize Config Lists
# e.g.
#     [['macos', 'simulator_min'], ['macos', 'simulator_target']]
#     -> [[['macos'], ['simulator_min', 'simulator_target']]]
def reorganize_configs(configs):
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
        # configk = combine_config(configk, platform, k)
        configs_list[i][k] = configk
      for config in remove_configs:
        configs_list.remove(config)

  return platform_configs

# If possible, combine kth config to "All *"
# e.g.
#     ['ubuntu', 'windows', 'macos']
#     -> ['All 3 os']
#     ['ubuntu', 'windows']
#     -> ['2/3 os: ubuntu,windows': ]
def combine_configs(error_configs, all_configs):
  for (platform, configs_list) in error_configs.items():
    for i in range(len(configs_list)):
      for j in range(len(configs_list[i])):
        configs_list[i][j] = combine_config(platform, configs_list[i][j], all_configs[platform][0][j], j)
  return error_configs


def combine_config(platform, config, config_value, k):
  config_before_combination = config.copy()
  if k == 1 and platform in ("android", "ios", "tvos"):
    # config_name = test_device here
    k = -1
  config_name = BUILD_CONFIGS[platform][k]
  # if certain config failed for all values, add message "All *"
  if len(config_value) > 1 and len(config) == len(config_value):
    config = ["All %d %s" % (len(config_value), config_name)]
  elif config_name == "ios_device":
    ftl_devices = set(filter(lambda device: TEST_DEVICES.get(device).get("type") in "ftl", config_value))
    simulators = set(filter(lambda device: TEST_DEVICES.get(device).get("type") in "virtual", config_value))
    if len(ftl_devices) > 1 and ftl_devices.issubset(set(config)):
      config.insert(0, "All %d FTL Devices" % len(ftl_devices))
      config = [x for x in config if (x not in ftl_devices)]
    if len(simulators) > 1 and simulators.issubset(set(config)):
      config.insert(0, "All %d Local Simulators" % len(simulators))
      config = [x for x in config if (x not in simulators)]
  elif config_name == "android_device":
    ftl_devices = set(filter(lambda device: TEST_DEVICES.get(device).get("type") in "ftl", config_value))
    emulators = set(filter(lambda device: TEST_DEVICES.get(device).get("type") in "virtual", config_value))
    if len(ftl_devices) > 1 and ftl_devices.issubset(set(config)):
      config.insert(0, "All %d FTL Devices" % len(ftl_devices))
      config = [x for x in config if (x not in ftl_devices)]
    if len(emulators) > 1 and emulators.issubset(set(config)):
      config.insert(0, "All %d Local Emulators" % len(emulators))
      config = [x for x in config if (x not in emulators)]
  # if certain config failed for more than 1 value but not all, add message "x/y" which means "x" out of "y" configs has errors.
  if len(config_value) > 1 and config_before_combination == config:
    config = ["%d/%d %s: %s" % (len(config), len(config_value), config_name, flat_config(config))]

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
