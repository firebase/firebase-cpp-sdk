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

r"""Tool for mobile testapps to Test on local simulators.

Requires simulators installed. iOS simulator can be installed via tool:
  https://github.com/xcpretty/xcode-install#simulators


Usage:

  python test_simulator.py --testapp_dir ~/testapps 

This will recursively search ~/testapps for apps,
test on local simulator, and validate their results. The validation is specific to
the structure of the Firebase Unity and C++ testapps.


If you wish to specify a particular device to test on, you will need the model
id and version (OS version for iOS). These change over time. You can listing all 
available simulators (supported models and versions) with the following commands:

  xcrun simctl list

Note: you need to combine Name and Version with "-". Examples:

iPhone 11, OS 14.4:
  --ios_device "iPhone 11-14.4"

"""

import json
import os
import pathlib
import subprocess
import zipfile

from absl import app
from absl import flags
from absl import logging
import attr
from integration_testing import test_validation


FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "Testapps in this directory will be tested.")
flags.DEFINE_string(
    "gameloop_project", "integration_testing/gameloop",
    "An zipped UI Test app that helps doing game-loop test."
    " The source code can be found here: integration_testing/gameloop")
flags.DEFINE_string(
    "ios_device", "iPhone 11-14.4",
    "iOS device, which is a combination of device name and os version")

@attr.s(frozen=False, eq=False)
class Test(object):
  """Holds data related to the testing of one testapp."""
  testapp_path = attr.ib()
  logs = attr.ib()

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  current_dir = pathlib.Path(__file__).parent.absolute()
  testapp_dir = os.path.abspath(os.path.expanduser(FLAGS.testapp_dir))
  gameloop_project = os.path.join(current_dir, FLAGS.gameloop_project)
  ios_device = FLAGS.ios_device
  device_info = ios_device.split("-")
  device_name = device_info[0]
  device_os = device_info[1]

  config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
  with open(config_path, "r") as config:
    config = json.load(config)

  if not config:
    logging.info("No config found")
    return 1

  logging.info("Config found: %s", config)

  testapps = []
  for file_dir, directories, _ in os.walk(testapp_dir):
    # .app is treated as a directory, not a file in MacOS
    for directory in directories:
      full_path = os.path.join(file_dir, directory)
      if "simulator" in full_path and directory.endswith(".app"):
        testapps.append((_get_bundle_id(full_path, config), full_path))

  if not testapps:
    logging.info("No testapps found")
    return 1

  logging.info("Testapps found: %s", "\n".join(path for _, path in testapps))
  
  gameloop_app = _build_gameloop(gameloop_project, device_name, device_os)
  if not gameloop_app:
    logging.info("gameloop app not found")
    return 2

  device_id = _boot_simulator(device_name, device_os)
  if not device_id:
    logging.info("simulator created fail")
    return 3

  tests = []
  for bundle_id, app_path in testapps:
    tests.append(Test(
                    testapp_path=app_path, 
                    logs=_run_gameloop_test(bundle_id, app_path, gameloop_app, device_id)))

  return test_validation.summarize_test_results(
    tests, test_validation.CPP, testapp_dir)


def _get_bundle_id(app_path, config):
  """Get app bundle id from build_testapps.json file."""
  for api in config["apis"]:
    if api["name"] != "app" and (api["name"] in app_path or api["full_name"] in app_path):
      return api["bundle_id"]


def _build_gameloop(gameloop_project, device_name, device_os):
  """Build gameloop UI Test app. 

  This gameloop app can run integration_test app automatically.
  """
  project_path = os.path.join(gameloop_project, "gameloop.xcodeproj")
  output_path = os.path.join(gameloop_project, "Build")

  """Build the gameloop app for test."""
  args = ["xcodebuild", "-project", project_path,
    "-scheme", "gameloop",
    "-sdk", "iphonesimulator",
    "build-for-testing", 
    "-destination", "platform=iOS Simulator,name=%s,OS=%s" % (device_name, device_os), 
    "SYMROOT=%s" % output_path]
  logging.info("Running game-loop test: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  
  for file_dir, _, file_names in os.walk(output_path):
    for file_name in file_names:
      if file_name.endswith(".xctestrun"): 
        return os.path.join(file_dir, file_name)


def _run_xctest(gameloop_app, device_id):
  """Run the gameloop UI Test app.
    This gameloop app can run integration_test app automatically.
  """
  args = ["xcodebuild", "test-without-building", 
    "-xctestrun", gameloop_app, 
    "-destination", "id=%s" % device_id]
  logging.info("Running game-loop test: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=False)

  if not result.stdout:
    logging.info("No xctest result")
    return

  result = result.stdout.splitlines()
  log_path = next((line for line in result if ".xcresult" in line), None)
  logging.info("game-loop xctest result: %s", log_path)
  return log_path


def _boot_simulator(device_name, device_os):
  """Create a simulator locally. Will wait until this simulator botted."""
  args = ["xcrun", "simctl", "shutdown", "all"]
  logging.info("Shutdown all simulators: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  command = "xcrun xctrace list devices 2>&1 | grep \"%s (%s)\" | awk -F'[()]' '{print $4}'" % (device_name, device_os)
  logging.info("Get my simulator: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  device_id = result.stdout.read().strip()

  args = ["xcrun", "simctl", "boot", device_id]
  logging.info("Boot my simulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  args = ["xcrun", "simctl", "bootstatus", device_id]
  subprocess.run(args=args, check=True)
  return device_id


def _delete_simulator(device_id):
  """Delete the created simulator."""
  args = ["xcrun", "simctl", "delete", device_id]
  logging.info("Delete created simulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _run_gameloop_test(bundle_id, app_path, gameloop_app, device_id):
  """Run gameloop test and collect test result."""
  logging.info("Running test: %s, %s, %s, %s", bundle_id, app_path, gameloop_app, device_id)
  _install_app(app_path, device_id)
  _run_xctest(gameloop_app, device_id)
  logs = _get_test_log(bundle_id, app_path, device_id)
  _uninstall_app(bundle_id, device_id)
  return logs


def _install_app(app_path, device_id):
  """Install integration_test app into the simulator."""
  args = ["xcrun", "simctl", "install", device_id, app_path]
  logging.info("Install testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _uninstall_app(bundle_id, device_id):
  """Uninstall integration_test app from the simulator."""
  args = ["xcrun", "simctl", "uninstall", device_id, bundle_id]
  logging.info("Uninstall testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _get_test_log(bundle_id, app_path, device_id):
  """Read integration_test app testing result."""
  args=["xcrun", "simctl", "get_app_container", device_id, bundle_id, "data"]
  logging.info("Get test result: %s", " ".join(args))
  result = subprocess.run(
      args=args,
      capture_output=True, text=True, check=False)
  
  if not result.stdout:
    logging.info("No test Result")
    return None

  log_path = os.path.join(result.stdout.strip(), "Documents", "GameLoopResults", "Results1.json") 
  return _read_file(log_path) 


def _read_file(path):
  """Extracts the contents of a file."""
  with open(path, "r") as f:
    test_result = f.read()

  logging.info("Reading file: %s", path)
  logging.info("File contant: %s", test_result)
  return test_result


if __name__ == '__main__':
  flags.mark_flag_as_required("testapp_dir")
  app.run(main)
