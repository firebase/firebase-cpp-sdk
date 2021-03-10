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
python scripts/gha/print_matrix_configuration.py -k os

# Query value for matrix (default) parameter "os" for "android" workflow.
python scripts/gha/print_matrix_configuration.py -w android -k os

# Query value for expanded matrix (default) parameter "os" for "android" workflow.
python scripts/gha/print_matrix_configuration.py -w android -e 1 -k os

# Override the value for "os" for "integration_tests"
python scripts/gha/print_matrix_configuration.py -w integration_tests
        -o my_custom_os -k os

# Query value for config parameter "apis" for "integration_tests" workflow.
python scripts/gha/print_matrix_configuration.py -c -w integration_tests -k apis

# Override the value for config parameters "apis" for integration_tests
python scripts/gha/print_matrix_configuration.py -c -w integration_tests
        -o my_custom_api -k apis
"""

import json
import argparse

# Note that desktop is used for fallback,
# if there is no direct match for a key.
DEFAULT_WORKFLOW = "desktop"
EXPANDED_KEY = "expanded"

PARAMETERS = {
  "desktop": {
    "matrix": {
      "os": ["ubuntu-latest", "macos-latest"],
      "build_type": ["Release", "Debug"],
      "architecture": ["x64", "x86"],
      "msvc_runtime": ["static","dynamic"],
      "vcpkg_triplet_suffix": ["windows-static", "windows-static-md", "linux", "osx"],
      "xcode_version": ["11.7"],
      "python_version": ["3.7"],

      EXPANDED_KEY: {
        "xcode_version": ["11.7", "12.4"],
      }
    }
  },

  "android": {
    "matrix": {
      "os": ["ubuntu-latest", "macos-latest"],
      "architecture": ["x64"],
      "python_version": ["3.7"],

      EXPANDED_KEY: {
        "os": ["android-expanded-example-os"]
      }
    }
  },

  "integration_tests": {
    "matrix": {
        "os": ["ubuntu-latest", "macos-latest", "windows-latest"],
        "platform": ["Desktop", "Android", "iOS"],
        "ssl_lib": ["openssl", "boringssl"]
    },
    "config": {
        "apis": "admob,analytics,auth,database,dynamic_links,firestore,functions,installations,instance_id,messaging,remote_config,storage",
        "android_device": "flame",
        "android_api": "29",
        "ios_device": "iphone8",
        "ios_version": "11.4"
    },

    "ios": {
    "matrix": {
      "xcode_version": ["12"],

      EXPANDED_KEY: {
        "xcode_version": ["12", "12.4"]
      }
    }
  },


  }
}


def get_value(workflow, use_expanded, parm_key, config_parms_only=False):
  """ Fetch value from configuration

  Args:
      workflow (str): Key corresponding to the github workflow.
      use_expanded (bool): Use expanded configuration for the workflow?
      parm_key (str): Exact key name to fetch from configuration.
      config_parms_only (bool): Search in config blocks if True, else matrix
                                blocks.

  Raises:
      KeyError: Raised if given key is not found in configuration.

  Returns:
      (str|list): Matched value for the given key.
  """
  # Search for a given key happens in the following sequential order
  # Expanded block (if use_expanded) -> Standard block
  # -> Expanded default block (if_use_expanded) -> Default standard block
  search_blocks = []

  parm_type_key = "config" if config_parms_only else "matrix"
  default_workflow_block = PARAMETERS.get(DEFAULT_WORKFLOW)
  default_workflow_parm_block = default_workflow_block.get(parm_type_key)
  search_blocks.insert(0, default_workflow_parm_block)
  if use_expanded and EXPANDED_KEY in default_workflow_parm_block:
    search_blocks.insert(0, default_workflow_parm_block[EXPANDED_KEY])

  if workflow != DEFAULT_WORKFLOW:
    workflow_block = PARAMETERS.get(workflow)
    if workflow_block:
      workflow_parm_block = workflow_block.get(parm_type_key)
      if workflow_parm_block:
        search_blocks.insert(0, workflow_parm_block)
        if use_expanded and EXPANDED_KEY in workflow_parm_block:
          search_blocks.insert(0, workflow_parm_block[EXPANDED_KEY])

  for search_block in search_blocks:
    if parm_key in search_block:
      return search_block[parm_key]

  else:
    raise KeyError("Parameter key: '{0}' of type '{1}' not found "\
                   "for workflow '{2}' (expanded = {3}) .".format(parm_key,
                                                                parm_type_key,
                                                                workflow,
                                                                use_expanded))


def print_value(value):
  """ Print Json formatted string that can be consumed in Github workflow."""
  # Eg: for lists,
  # print(json.dumps) ->
  # ["ubuntu-latest", "macos-latest", "windows-latest"]
  # print(repr(json.dumps)) ->
  # '["ubuntu-latest", "macos-latest", "windows-latest"]'

  # Eg: for strings
  # print(json.dumps) -> "flame"
  # print(repr(json.dumps)) -> '"flame"'

  print(json.dumps(value))


def main():
  args = parse_cmdline_args()
  if args.override:
    # If it is matrix parm, convert CSV string into a list
    if not args.config:
      args.override = args.override.split(',')
    print_value(args.override)
    return

  value = get_value(args.workflow, args.expanded, args.parm_key, args.config)
  print_value(value)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Query matrix and config parameters used in Github workflows.')
  parser.add_argument('-c', '--config', action='store_true', help='Query parameter used for Github workflow/dispatch configurations.')
  parser.add_argument('-w', '--workflow', default=DEFAULT_WORKFLOW, help='Config key for Github workflow.')
  parser.add_argument('-e', '--expanded', type=bool, default=False, help='Use expanded matrix')
  parser.add_argument('-k', '--parm_key', required=True, help='Print the value of specified key from matrix or config maps.')
  parser.add_argument('-o', '--override', help='Override existing value with provided value')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()
