# Copyright 2019 Google LLC
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

"""A utility for working with testapp builder JSON files.

This module handles loading the central configuration file for a testapp
builder, as well as offering simplified access to the loaded in-memory
object.

The main motivation for this module is to provide descriptive error messages,
rather than an unhelpful KeyError that one would get from working with the
loaded JSON (a dictionary) directly. Instead, errors will dump a readable form
of the section of JSON being read, alongside the error.

Example of such a configuration file:

{
  "apis": [
    {
      "name": "analytics",
      "full_name": "FirebaseAnalytics",
      "bundle_id": "com.google.ios.analytics.testapp",
      "ios_target": "integration_test",
      "testapp_path": "firebase/analytics/client/cpp/integration_test",
      "frameworks": [
        "firebase_analytics.framework",
        "firebase.framework"
      ],
      "provision": "Google_Development.mobileprovision"
    },
    {
      "name": "admob",
      "full_name": "FirebaseAdmob",
      "bundle_id": "com.google.ios.admob.testapp",
      "ios_target": "integration_test",
      "testapp_path": "firebase/admob/client/cpp/integration_test",
      "frameworks": [
        "firebase_admob.framework",
        "firebase.framework"
      ],
      "provision": "Google_Development.mobileprovision"
    }
  ],
  "dev_team": "ABCDEFGHIJK"
}

Available methods:

    read_config reads the config into memory.
    read_general_config provides access to the root level config
    read_api_config provides access to an API's config, and requires its name.
        This must occur in a root level "apis" list of objects.
    get_api_names returns a sequence of the names of configured APIs, for use
        with read_api_config.

"""

import json
import os
import pathlib

# List of api-specific configurations.
_APIS_KEY = "apis"

_NAME_KEY = "name"

# These are used to contextualize where a value is being read within the config
# to provide better error messages.
_ROOT_CONFIG = "root config "
_API_CONFIG = "config in 'apis' list"


def read_config(path=None):
  """Creates an in-memory config object out of a testapp config file.

  Args:
    path: Path to a testapp builder config file. If not specified, will look
        for 'build_testapps.json' in the same directory as this file.

  Returns:
    An in-memory config object that can be used with the other methods in
    this module to extract data from the config passed in.

  """
  if not path:
    directory = pathlib.Path(__file__).parent.absolute()
    path = os.path.join(directory, "build_testapps.json")
  with open(path, "r") as config:
    return json.load(config)


def read_general_config(config, key, optional=False):
  """Reads a configuration value not tied to a particular API.

  Args:
    config: Configuration created by read_config.
    key: Key whose value is being read.
    optional: Return a None if the key is not found.

  Returns:
    Value associated to the given key. Could be any type supported by
    Python's JSON module.

  """
  return _get_from_json(config, key, _ROOT_CONFIG, optional)


def read_api_config(config, api_name, key, optional=False):
  """Reads a configuration value for a particular API.

  Args:
    config: Configuration created by read_config.
    api_name: Name of the API whose configuration is being read.
    key: Key whose value is being read.
    optional: Return a None if the key is not found.

  Returns:
    Value associated to the given key. Could be any type supported by
    Python's JSON module.

  """
  api_dicts = _get_from_json(config, _APIS_KEY, _ROOT_CONFIG)
  api_dict = [api for api in api_dicts if api[_NAME_KEY] == api_name][0]
  return _get_from_json(api_dict, key, _API_CONFIG, optional)


def get_api_names(config):
  """Returns a list of all the APIs present in this config."""
  apis = _get_from_json(config, _APIS_KEY, _ROOT_CONFIG)
  return [_get_from_json(api, _NAME_KEY, _API_CONFIG) for api in apis]


def _get_from_json(json_dict, key, json_description, optional=False):
  """Attempts to retrieve key from the json dictionary.

  If the key is not found and is not marked as optional, an error
  will be raised with detailed information including the key, a
  pretty-printed dump of the given json, and a description of the json
  for context.

  Args:
    json_dict: A dictionary representation of a JSON object.
    key: The key whose value is being extracted from the JSON dictionary.
    json_description: A description of the json object to clarify where
        the object came from. Will be included in an error message, if the key
        is not found in the dict. Especially important for nested objects.
        e.g. "API object from 'APIS' list in main config".
    optional: Return a None if the key is not found.

  Raises:
    ValueError: Key is None or empty, json_dict is not a dictionary.
    RuntimeError: Non-optional Key not found in the dictionary.

  Returns:
    The value of the key in the dictionary, equivalent to json_dict[key].

  """
  if not key:
    raise ValueError("Must supply a valid, non-empty key.")
  try:
    return json_dict[key]
  except TypeError:  # json_dict is not a dict
    raise ValueError(
        "Expected dictionary (JSON object) from %s, received %s: %s instead." %
        (json_description, type(json_dict), json_dict))
  except KeyError:  # key not found
    if optional:
      return None
    formatted_json = json.dumps(
        json_dict, sort_keys=True, indent=4, separators=(",", ":"))
    raise RuntimeError(
        "%s not found in %s:\n%s" % (key, json_description, formatted_json))
