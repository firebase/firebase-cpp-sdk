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
builder, returning a 'Config' object that exposes all the data.

The motivation for loading the config into a class as opposed to returning
the loaded JSON directly is to validate the data upfront, to fail fast if
anything is missing or formatted incorrectly.

Example of such a configuration file:

{
  "apis": [
    {
      "name": "analytics",
      "full_name": "FirebaseAnalytics",
      "bundle_id": "com.google.ios.analytics.testapp",
      "ios_target": "integration_test",
      "tvos_target": "integration_test_tvos",
      "testapp_path": "firebase/analytics/client/cpp/integration_test",
      "frameworks": [
        "firebase_analytics.framework",
        "firebase.framework"
      ],
      "provision": "Google_Development.mobileprovision"
    }
  ],
  "dev_team": "ABCDEFGHIJK"
}

"""

import json
import os
import pathlib

import attr

_DEFAULT_CONFIG_NAME = "build_testapps.json"


def read_config(path=None):
  """Creates an in-memory 'Config' object out of a testapp config file.

  Args:
    path (str): Path to a testapp builder config file. If not specified, will
        look for 'build_testapps.json' in the same directory as this file.

  Returns:
    Config: All of the testapp builder's configuration.

  """
  if not path:
    directory = pathlib.Path(__file__).parent.absolute()
    path = os.path.join(directory, _DEFAULT_CONFIG_NAME)
  with open(path, "r") as config:
    config = json.load(config)
  api_configs = dict()
  try:
    for api in config["apis"]:
      api_name = api["name"]
      api_configs[api_name] = APIConfig(
          name=api_name,
          full_name=api["full_name"],
          bundle_id=api["bundle_id"],
          ios_target=api["ios_target"],
          tvos_target=api["tvos_target"],
          ios_scheme=api["ios_target"],  # Scheme assumed to be same as target.
          tvos_scheme=api["tvos_target"], 
          testapp_path=api["testapp_path"],
          internal_testapp_path=api.get("internal_testapp_path", None),
          frameworks=api["frameworks"],
          provision=api["provision"],
          minify=api.get("minify", None))
    return Config(
        apis=api_configs,
        apple_team_id=config["apple_team_id"],
        compilers=config["compiler_dict"])
  except (KeyError, TypeError, IndexError):
    # The error will be cryptic on its own, so we dump the JSON to
    # offer context, then reraise the error.
    print(
        "Error occurred while parsing config. Full config dump:\n"
        + json.dumps(config, sort_keys=True, indent=4, separators=(",", ":")))
    raise


@attr.s(frozen=True, eq=False)
class Config(object):
  apis = attr.ib()  # Mapping of str: APIConfig
  apple_team_id = attr.ib()
  compilers = attr.ib()

  def get_api(self, api):
    """Returns the APIConfig object for the given api, e.g. 'analytics'."""
    return self.apis[api]


@attr.s(frozen=True, eq=False)
class APIConfig(object):
  """Holds all the configuration for a single testapp project."""
  name = attr.ib()
  full_name = attr.ib()
  bundle_id = attr.ib()
  ios_target = attr.ib()
  tvos_target = attr.ib()
  ios_scheme = attr.ib()
  tvos_scheme = attr.ib()
  testapp_path = attr.ib()  # Integration test dir relative to sdk root
  internal_testapp_path = attr.ib()  # Internal integration test dir relative to sdk root
  frameworks = attr.ib()  # Required custom xcode frameworks
  provision = attr.ib()  # Path to the local mobile provision
  minify = attr.ib()  # (Optional) Android minification.

