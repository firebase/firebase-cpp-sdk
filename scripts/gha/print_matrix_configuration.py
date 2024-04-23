#!/usr/bin/env python

# Copyright 2021 Google
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
""" Fetch and print Github workflow matrix values for a given configuration.

This script holds the configurations (standard, expanded) for all of our
Github worklows and prints a string in the format that can be easily parsed
in Github workflows.

Design Notes:
- Desktop workflow is treated as the fallback option if there is no match.
  This also means that all workflows inherit the base set of keys and values from
  Desktop and optionally override them.
- There are 2 types of parameters - matrix and config. Matrix parameters are
  lists and generate jobs dynamically in Github workflows. Config parameters
  represent the additonal fine tuning parameters that can be used in workflow
  dispatch. They are always treated as strings.

Raises:
    ValueError: If the specific key is not found at all even after trying all
                the fallbacks.

Usage examples:
# Query value for matrix (default) parameter "os" for "desktop" (default) workflow.
python3 scripts/gha/print_matrix_configuration.py -k os

# Query value for matrix (default) parameter "os" for "android" workflow.
python3 scripts/gha/print_matrix_configuration.py -w android -k os

# Query value for expanded matrix (default) parameter "os" for "android" workflow.
python3 scripts/gha/print_matrix_configuration.py -w android -e 1 -k os

# Override the value for "os" for "integration_tests"
python3 scripts/gha/print_matrix_configuration.py -w integration_tests
        -o my_custom_os -k os

# Query value for config parameter "apis" for "integration_tests" workflow.
python3 scripts/gha/print_matrix_configuration.py -c -w integration_tests -k apis

# Override the value for config parameters "apis" for integration_tests
python3 scripts/gha/print_matrix_configuration.py -c -w integration_tests
        -o my_custom_api -k apis
"""

import argparse
import json
import os
import random
import re
import subprocess
import sys

from integration_testing import config_reader

# Note that desktop is used for fallback,
# if there is no direct match for a key.
DEFAULT_WORKFLOW = "desktop"
EXPANDED_KEY = "expanded"
MINIMAL_KEY = "minimal"

PARAMETERS = {
  "desktop": {
    "matrix": {
      "os": ["ubuntu-20.04", "macos-13"],
      "build_type": ["Release", "Debug"],
      "architecture": ["x64", "x86", "arm64"],
      "msvc_runtime": ["static","dynamic"],
      "xcode_version": ["15.1"],
      "python_version": ["3.7"],

      EXPANDED_KEY: {
        "os": ["ubuntu-20.04", "macos-13", "windows-latest"],
        "xcode_version": ["15.1"],
      }
    }
  },

  "android": {
    "matrix": {
      "os": ["ubuntu-20.04", "macos-13", "windows-latest"],
      "architecture": ["x64"],
      "python_version": ["3.7"],

      EXPANDED_KEY: {
        "os": ["ubuntu-20.04", "macos-13", "windows-latest"]
      }
    }
  },

  "integration_tests": {
    "matrix": {
      "os": ["ubuntu-20.04", "macos-13", "windows-latest"],
      "platform": ["Desktop", "Android", "iOS", "tvOS"],
      "ssl_lib": ["openssl"],
      "android_device": ["android_target", "emulator_ftl_target"],
      "ios_device": ["ios_target", "simulator_target"],
      "tvos_device": ["tvos_simulator"],
      "build_type": ["Debug"],
      "architecture_windows_linux": ["x64"],
      "architecture_macos": ["x64"],
      "msvc_runtime": ["dynamic"],
      "cpp_compiler_windows": ["VisualStudio2019"],
      "cpp_compiler_linux": ["clang-11.0"],
      "xcode_version": ["15.1"],  # only the first one is used
      "ndk_version": ["r22b"],
      "platform_version": ["28"],
      "build_tools_version": ["28.0.3"],

      MINIMAL_KEY: {
        "os": ["ubuntu-20.04"],
        "platform": ["Desktop"],
        "apis": "firestore"
      },

      EXPANDED_KEY: {
        "ssl_lib": ["openssl", "boringssl"],
        "android_device": ["android_target", "android_latest", "emulator_ftl_target", "emulator_ftl_latest"],
        "ios_device": ["ios_min", "ios_target", "ios_latest", "simulator_min", "simulator_target", "simulator_latest"],
        "tvos_device": ["tvos_simulator"],
        "architecture_windows_linux": ["x64", "x86"],
        "architecture_macos": ["x64", "arm64"],
      }
    },
    "config": {
      "apis": "analytics,app_check,auth,database,dynamic_links,firestore,functions,gma,installations,messaging,remote_config,storage",
      "mobile_test_on": "real,virtual"
    }
  },

  "ios": {
    "matrix": {
      "xcode_version": ["15.1"],

      EXPANDED_KEY: {
        "xcode_version": ["15.1"]
      }
    }
  },
}

# key: supported platforms.
# value: supported build configurations for that platform
# It consists with workflow matrix. And Test Result Report will use it.
BUILD_CONFIGS = {
  "windows": ["ssl_lib", "build_type", "architecture_windows_linux", "msvc_runtime", "cpp_compiler_windows"],
  "ubuntu": ["ssl_lib", "build_type", "architecture_windows_linux", "cpp_compiler_linux"],
  "macos": ["ssl_lib", "architecture_macos", "xcode_version"],
  "android": ["os", "ndk_version", "build_tools", "platform_version", "android_device"],
  "ios": ["os", "xcode_version", "ios_device"],
  "tvos": ["os", "xcode_version", "tvos_device"]
}

# Check currently supported models and versions with the following commands:
#   gcloud firebase test android models list
#   gcloud firebase test ios models list

# For each device type, one entry from the list will be used for each
# run. If there is only one device type listed, it will be chosen by
# default.
#
# Note: All entries in a given list must have the same type, (ftl or virtual).
TEST_DEVICES = {
  "android_target": [
      {"type": "ftl", "device": "model=blueline,version=28"}, # Pixel 3
      {"type": "ftl", "device": "model=gts3lltevzw,version=28"},  # Galaxy Tab S3
      {"type": "ftl", "device": "model=SH-01L,version=28"},  # AQUOS sense2 SH-01L
  ],
  "android_latest": [
      {"type": "ftl", "device": "model=oriole,version=33"},  # Pixel 6
      {"type": "ftl", "device": "model=panther,version=33"},  # Pixel 7
      {"type": "ftl", "device": "model=lynx,version=33"},  # Pixel 7a
      {"type": "ftl", "device": "model=cheetah,version=33"},  # Pixel 7 Pro
      {"type": "ftl", "device": "model=felix,version=33"},  # Pixel Fold
      {"type": "ftl", "device": "model=tangorpro,version=33"},  # Pixel Tablet
      {"type": "ftl", "device": "model=gts8uwifi,version=33"},  # Galaxy Tab S8 Ultra
      {"type": "ftl", "device": "model=b0q,version=33"},  # Galaxy S22 Ultra
      {"type": "ftl", "device": "model=b4q,version=33"},  # Galaxy Z Flip4
  ],
  "emulator_ftl_target": [
      {"type": "ftl", "device": "model=Pixel2,version=28"},
      {"type": "ftl", "device": "model=Pixel2.arm,version=28"},
      {"type": "ftl", "device": "model=MediumPhone.arm,version=28"},
      {"type": "ftl", "device": "model=MediumTablet.arm,version=28"},
      {"type": "ftl", "device": "model=SmallPhone.arm,version=28"},
  ],
  "emulator_ftl_latest": [
      {"type": "ftl", "device": "model=Pixel2.arm,version=32"},
      {"type": "ftl", "device": "model=MediumPhone.arm,version=32"},
      {"type": "ftl", "device": "model=MediumTablet.arm,version=32"},
      {"type": "ftl", "device": "model=SmallPhone.arm,version=32"},
  ],
  "emulator_target": [ {"type": "virtual", "image":"system-images;android-30;google_apis;x86_64"} ],
  "emulator_latest": [ {"type": "virtual", "image":"system-images;android-32;google_apis;x86_64"} ],
  "emulator_32bit": [ {"type": "virtual", "image":"system-images;android-30;google_apis;x86"} ],
  "ios_min": [
      # Slightly different OS versions because of limited FTL selection.
      {"type": "ftl", "device": "model=iphone8,version=14.7"},
      {"type": "ftl", "device": "model=iphone11pro,version=14.7"},
      {"type": "ftl", "device": "model=iphone12pro,version=14.8"},
  ],
  "ios_target": [
      # Slightly different OS versions because of limited FTL selection.
      {"type": "ftl", "device": "model=iphone13pro,version=15.7"},
      {"type": "ftl", "device": "model=iphone8,version=15.7"},
  ],
  "ios_latest": [
      {"type": "ftl", "device": "model=iphone14pro,version=16.6"},
      {"type": "ftl", "device": "model=iphone11pro,version=16.6"},
      {"type": "ftl", "device": "model=iphone8,version=16.6"},
      {"type": "ftl", "device": "model=ipad10,version=16.6"},
  ],
  "simulator_min": [ {"type": "virtual", "name":"iPhone SE (3rd generation)", "version":"17.0.1"} ],
  "simulator_target": [ {"type": "virtual", "name":"iPhone 15", "version":"17.2"} ],
  "simulator_latest": [ {"type": "virtual", "name":"iPad Pro (12.9-inch) (6th generation)", "version":"17.4"} ],
  "tvos_simulator": [ {"type": "virtual", "name":"Apple TV", "version":"16.1"} ],
}

# Easy accesssor for getting a TEST_DEVICES entry. Note that once a device model
# is chosen on a specific machine, it will be maintained on that same machine.
def get_test_device(device_type):
  """Get a TEST_DEVICES entry for the given device type.

  If there is more than one entry listed for the given device type, one will be
  selected randomly. Once a selection is made for a given device_type, the same
  selection will be returned on subsequent calls.

  Args:
      device_type(str): Device type (key for TEST_DEVICES dictionary)

  Returns:
      A single entry from TEST_DEVICES (dictionary), or None if the key is not
      found.

  """
  device_entry = TEST_DEVICES.get(device_type)
  if not isinstance(device_entry, list):
    # If None or just a dictionary (not in an array)
    return device_entry
  return random.choice(device_entry)


def get_value(workflow, test_matrix, parm_key, config_parms_only=False):
  """ Fetch value from configuration

  Args:
      workflow (str): Key corresponding to the github workflow.
      test_matrix (str): Use EXPANDED_KEY or MINIMAL_KEY configuration for the workflow?
      parm_key (str): Exact key name to fetch from configuration.
      config_parms_only (bool): Search in config blocks if True, else matrix
                                blocks.

  Raises:
      KeyError: Raised if given key is not found in configuration.

  Returns:
      (str|list): Matched value for the given key.
  """
  # Search for a given key happens in the following sequential order
  # Minimal/Expanded block (if test_matrix) -> Standard block

  parm_type_key = "config" if config_parms_only else "matrix"
  workflow_block = PARAMETERS.get(workflow)
  if workflow_block:
    if test_matrix and test_matrix in workflow_block["matrix"]:
      if parm_key in workflow_block["matrix"][test_matrix]:
        return workflow_block["matrix"][test_matrix][parm_key]

    return workflow_block[parm_type_key][parm_key]

  else:
    raise KeyError("Parameter key: '{0}' of type '{1}' not found "\
                   "for workflow '{2}' (test_matrix = {3}) .".format(parm_key,
                                                                parm_type_key,
                                                                workflow,
                                                                test_matrix))


def filter_devices(devices, device_type):
  """ Filter device by device_type
  """
  device_type = device_type.replace("real","ftl")
  filtered_value = filter(lambda device: get_test_device(device).get("type") in device_type, devices)
  return list(filtered_value)


def print_value(value):
  """ Print Json formatted string that can be consumed in Github workflow."""
  # Eg: for lists,
  # print(json.dumps) ->
  # ["ubuntu-20.04", "macos-latest", "windows-latest"]
  # print(repr(json.dumps)) ->
  # '["ubuntu-20.04", "macos-latest", "windows-latest"]'

  # Eg: for strings
  # print(json.dumps) -> "flame"
  # print(repr(json.dumps)) -> '"flame"'

  print(json.dumps(value))

def scan_changes_in_file(parm_key, auto_diff, path, requested_api_list):
  """Scan for changes in the given file and return APIs that were modified."""
  change_lines = subprocess.check_output(['git', 'diff', '-U0', auto_diff, path]).decode('utf-8').rstrip('\n').split('\n')
  change_lines = [l for l in change_lines if l.startswith('-') or l.startswith('+')]
  change_lines = [l for l in change_lines if not l.startswith('---') and not l.startswith('+++')]
  change_lines = [l for l in change_lines if len(l) > 20]
  changed_apis = set()
  for line in change_lines:
    if ("Google-Mobile-Ads" in line or "GoogleUserMessagingPlatform" in line or
       "play-services-ads" in line or "user-messaging-platform" in line):
      changed_apis.add("gma")
    else:
      changed_apis.update(requested_api_list)
      break
  changed_apis.intersection_update(requested_api_list)
  return ",".join(sorted(changed_apis)) if changed_apis else None

def filter_values_on_diff(parm_key, value, auto_diff):
  """Filter the given key based on a branch diff.

  Remove entries from the list based on what we observe in the
  source tree, relative to the given base branch."""
  file_list = set(subprocess.check_output(['git', 'diff', '--name-only', auto_diff]).decode('utf-8').rstrip('\n').split('\n'))
  if parm_key == 'apis':
    file_redirects = {
      # Custom handling for specific files, to be treated as a different path,
      # ignored completely (set to None), or scan the changes more in-depth (set
      # to scan_changes function)
      "cmake/external/firestore.cmake": "firestore",
      "cmake/external/libuv.cmake": "database",
      "cmake/external/uWebSockets.cmake": "database",
      "ios_pod/Podfile":scan_changes_in_file,
      "Android/firebase_dependencies.gradle":scan_changes_in_file,
      "release_build_files/Android/firebase_dependencies.gradle":scan_changes_in_file,
    }
    custom_triggers = {
      # Special handling for several top-level directories.
      # Any top-level directories set to None are completely ignored.
      "external": None,
      "release_build_files": None,
      # These two handled by file_redirects above.
      "ios_pod": None,
      "Android": None,
      # Uncomment the two below lines when debugging this script, or GitHub
      # actions related to auto-diff mode.
      # ".github": None,
      # "scripts": None,
      # Top-level directories listed below trigger additional APIs being tested.
      # For example, if auth is touched by a PR, we also need to test functions,
      # database, firestore, and storage.
      "auth": "auth,functions,database,firestore,storage",
    }
    requested_api_list = set(value.split(','))
    filtered_api_list = set()
    file_list_array = list(file_list)
    for path in file_list_array:
      if len(path) == 0: continue
      if path in file_redirects:
        if file_redirects[path] is None:
          continue
        else:
          if callable(file_redirects[path]):
            add_paths = file_redirects[path](parm_key, auto_diff, path, requested_api_list)
            if add_paths: file_list_array.extend(add_paths.split(","))
            continue
          else:
            path = os.path.join(file_redirects[path], path)
      topdir = path.split(os.path.sep)[0]
      if topdir in custom_triggers:
        if not custom_triggers[topdir]: continue  # Skip ones set to None.
        for added_api in custom_triggers[topdir].split(','):
          filtered_api_list.add(added_api)
      if topdir in requested_api_list:
        filtered_api_list.add(topdir)
      else:
        # Something was modified that's not a known subdirectory.
        # Abort this whole process and just return the original api list.
        sys.stderr.write("Path '%s' is outside known directories, defaulting to all APIs: %s\n" % (path, value))
        return value
    sys.stderr.write("::warning::Autodetected APIs: %s\n" % ','.join(sorted(filtered_api_list)))
    return ','.join(sorted(filtered_api_list if filtered_api_list else requested_api_list))
  elif parm_key == 'platform':
    # Quick and dirty check:
    # For each file:
    #   If the filename matches "android" or ".java", add android to the list.
    #   If the filename matches "ios" or ".mm", add ios to the list.
    #   If the filename matches "desktop", add desktop to the list.
    #   If the filename matches anything else, return the full list.
    requested_platform_list = set(value)
    filtered_platform_list = set()
    for path in file_list:
      if len(path) == 0: continue
      matched = False
      if (re.search(r'^external/', path) or
          re.search(r'^release_build_files/', path) or
          re.search(r'readme', path, re.IGNORECASE)):
        matched = True
      if "Android" in requested_platform_list and (
          re.search(r'android', path, re.IGNORECASE) or
          re.search(r'\.java$', path) or
          re.search(r'gradle', path)):
        filtered_platform_list.add("Android")
        matched = True
      if "iOS" in requested_platform_list and (
          re.search(r'[_./]ios[_./]', path, re.IGNORECASE) or
          re.search(r'apple', path, re.IGNORECASE) or
          re.search(r'\.mm$', path) or
          re.search(r'xcode', path, re.IGNORECASE) or
          re.search(r'Pod', path)):
        filtered_platform_list.add("iOS")
        matched = True
      if "Desktop" in requested_platform_list and (
          re.search(r'desktop', path)):
        filtered_platform_list.add("Desktop")
        matched = True
      if not matched:
        # If the file didn't match any of these, trigger all requested platforms.
        sys.stderr.write("Defaulting to all platforms: %s\n" % ','.join(sorted(requested_platform_list)))
        return sorted(requested_platform_list)
    sys.stderr.write("::warning::Autodetected platforms: %s\n" % ','.join(sorted(filtered_platform_list)))
    return sorted(filtered_platform_list)
  else:
    return value


def filter_platforms_on_apis(platforms, apis):
  if "tvOS" in platforms:
    config = config_reader.read_config()
    supported_apis = [api for api in apis if config.get_api(api).tvos_target]
    if not supported_apis:
      platforms.remove("tvOS")

  return platforms


def main():
  args = parse_cmdline_args()
  if args.override:
    # If it is matrix parm, convert CSV string into a list
    if not args.config:
      args.override = args.override.split(',')
    if args.parm_key == "platform" and args.apis:
      # e.g. args.apis = "\"auth,analytics\""
      args.override = filter_platforms_on_apis(args.override, args.apis.strip('"').split(','))

    print_value(args.override)
    return

  if args.get_device_type:
    print(get_test_device(args.parm_key).get("type"))
    return
  if args.get_ftl_device:
    print(get_test_device(args.parm_key).get("device"))
    return
  if args.get_ftl_device_list:
    print(";".join([entry.get("device") for entry in TEST_DEVICES.get(args.parm_key)]))
    return

  if args.expanded:
    test_matrix = EXPANDED_KEY
  elif args.minimal:
    test_matrix = MINIMAL_KEY
  else:
    test_matrix = ""
  value = get_value(args.workflow, test_matrix, args.parm_key, args.config)
  if args.workflow == "integration_tests" and args.parm_key == "ios_device":
    value = filter_devices(value, args.device_type)
  if args.auto_diff:
    value = filter_values_on_diff(args.parm_key, value, args.auto_diff)
  if args.parm_key == "platform" and args.apis:
    # e.g. args.apis = "\"auth,analytics\""
    value = filter_platforms_on_apis(value, args.apis.strip('"').split(','))
  print_value(value)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Query matrix and config parameters used in Github workflows.')
  parser.add_argument('-c', '--config', action='store_true', help='Query parameter used for Github workflow/dispatch configurations.')
  parser.add_argument('-w', '--workflow', default=DEFAULT_WORKFLOW, help='Config key for Github workflow.')
  parser.add_argument('-m', '--minimal', type=bool, default=False, help='Use minimal matrix')
  parser.add_argument('-e', '--expanded', type=bool, default=False, help='Use expanded matrix')
  parser.add_argument('-k', '--parm_key', required=True, help='Print the value of specified key from matrix or config maps.')
  parser.add_argument('-a', '--auto_diff', metavar='BRANCH', help='Compare with specified base branch to automatically set matrix options')
  parser.add_argument('-o', '--override', help='Override existing value with provided value')
  parser.add_argument('-get_device_type', action='store_true', help='Get the device type, used with -k $device')
  parser.add_argument('-get_ftl_device', action='store_true', help='Get the ftl test device, used with -k $device')
  parser.add_argument('-get_ftl_device_list', action='store_true',
                      help='Get the FTL device list (semicolon-delimited) for the given -k $device')
  parser.add_argument('-t', '--device_type', default=['real', 'virtual'], help='Test on which type of mobile devices')
  parser.add_argument('--apis', default=PARAMETERS["integration_tests"]["config"]["apis"],
                      help='Exclude platform based on apis. Certain platform does not support all apis. e.g. tvOS does not support messaging')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()
