#!/usr/bin/env python

# Copyright 2020 Google
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
Note: Desktop workflow is treated as the fallback option if there is no match.
This also means that all workflows inherit the base set of keys and values from
Desktop and optionally override them.
Raises:
    ValueError: If the specific key is not found at all even after trying all
                the fallbacks.
    argparse.ArgumentError: If "--overrides" flag is incorrectly specified.
                            It MUST have an even number of items specified.
                            "--overrides key1 value1 key2 value2...".
Usage examples:
# Print the value for os for default (unspecified on command line) workflow
python scripts/gha/print_matrix_configuration.py -k os
# Print the value for os for android workflow
python scripts/gha/print_matrix_configuration.py -w android -k os
# Print the value for os for expanded android workflow
python scripts/gha/print_matrix_configuration.py -w android -e 1 -k os
# Override the value for os for integration_tests
python scripts/gha/print_matrix_configuration.py -w integration_tests
        --overrides os user_os1,user_os2 python_version 3.8 -k os
"""

import json
import argparse

# Note that desktop is used for fallback,
# if there is no direct match for a key.
DEFAULT_WORKFLOW = "desktop"
EXPANDED_KEY = "expanded"

configurations = {
  "desktop": {
    "os": ["ubuntu-latest", "macos-latest"],
    "build_type": ["Release", "Debug"],
    "architecture": ["x64", "x86"],
    "python_version": ["3.7"],

    EXPANDED_KEY: {
      "os": ["desktop-expanded-example-os"]
    }
  },

  "android": {
    "architecture": ["x64"],

    EXPANDED_KEY: {
      "os": ["android-expanded-example-os"]
    }
  },

  "integration_tests": {
      "os": ["ubuntu-latest", "macos-latest", "windows-latest"],
      "platform": ["Desktop", "Android", "iOS"],
      "apis": ["admob","analytics","auth","database","dynamic_links",
               "firestore","functions","installations","instance_id",
               "messaging","remote_config","storage"],
      "ssl_lib": ["openssl", "boringssl"],
      "android_device": ["flame"],
      "android_api": ["29"],
      "ios_device": ["iphone8"],
      "ios_version": ["11.4"]
  }
}


def get_value(workflow, use_expanded, config_key):
  """ Fetch value from configuration
  Args:
      workflow (str): Key corresponding to the github workflow.
      use_expanded (bool): Use expanded configuration for the workflow?
      config_key (str): Exact key name to fetch from configuration
  Raises:
      KeyError: Raised if given key is not found in configuration.
  Returns:
      (str|list): Matched value for the given key.
  """
  # Search for a given key happens in the following sequential order
  # Expanded block (if use_expanded) -> Standard block
  # -> Expanded default block (if_use_expanded) -> Default standard block
  search_blocks = []

  default_workflow_block = configurations.get(DEFAULT_WORKFLOW)
  search_blocks.insert(0, default_workflow_block)
  if use_expanded and EXPANDED_KEY in default_workflow_block:
    search_blocks.insert(0, default_workflow_block[EXPANDED_KEY])

  if workflow != DEFAULT_WORKFLOW:
    workflow_block = configurations.get(workflow)
    if workflow_block:
      search_blocks.insert(0, workflow_block)
      if use_expanded and EXPANDED_KEY in workflow_block:
        search_blocks.insert(0, workflow_block[EXPANDED_KEY])

  for search_block in search_blocks:
    if config_key in search_block:
      return search_block[config_key]

  else:
    raise KeyError("Config key: {0} not found.".format(config_key))


def process_overrides(overrides):
  """Build a dictionary of key,value pairs specified as overrides.
  Values specified as CSV are treated as lists and rest are strings.
  Args:
      overrides (list(str)): A list of overrides of keys and values.
                            Eg: ["os", "os1,os2", "platform", "platform1"]
  Returns:
      (dict): Map with keys and values converting CSV into lists.
  """
  overrides_map = {}
  for idx in range(0, len(overrides), 2):
    key = overrides[idx]
    value = overrides[idx+1]
    if not value:
      # We could receive empty strings as arguments. Ignoring them.
      # For example, the default values for workflow dispatch parameters could
      # be empty.
      continue
    # Comma separated values indicate a list
    if "," in value:
      value = value.split(",")
    overrides_map[key] = value
  return overrides_map


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
  
  if(isinstance(value, str) ):
    print(json.dumps([value]))
  else:
    print(json.dumps(value))


def main():
  args = parse_cmdline_args()
  overrides_map = None
  # Handle an empty overrides parameter list even if key is defined.
  # This helps us support the optional existence of work_dispatch parameters. 
  if args.overrides and len(args.overrides)!=1:
    if len(args.overrides)%2!=0:
      raise ValueError("--overrides flag should have an even number of items." +
                       " Every key should have a corresponding value.")
    overrides_map = process_overrides(args.overrides)
    # If user has provided a custom override just return that.
    if args.config_key in overrides_map:
      print_value(overrides_map[args.config_key])
      return

  value = get_value(args.workflow, args.expanded, args.config_key)
  print_value(value)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
  parser.add_argument('-w', '--workflow', default=DEFAULT_WORKFLOW, help='Config key for Github workflow.')
  parser.add_argument('-e', '--expanded', type=bool, default=False, help='Use expanded matrix')
  parser.add_argument('-k', '--config_key', required=True, help='Print the value of specified key in config.')
  parser.add_argument('--overrides', nargs='+', help='A list of keys and overridden values (comma separated if it is a list). Example: --override os ubuntu-latest,windows-latest')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main() 