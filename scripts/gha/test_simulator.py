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

r"""Tool for mobile testapps to Test on iOS Simulator / Android Emulator locally.

Usage:

  python test_simulator.py --testapp_dir ~/testapps 

This will recursively search ~/testapps for apps,
test on local simulators/emulators, and validate their results. The validation is 
specific to the structure of the Firebase Unity and C++ testapps.

----iOS only----
Requires simulators installed. iOS simulator can be installed via tool:
  https://github.com/xcpretty/xcode-install#simulators

If you wish to specify a particular iOS device to test on, you will need the model
id and version (OS version for iOS). These change over time. You can listing all 
available simulators (supported models and versions) with the following commands:

  xcrun simctl list

Device Information is stored in TEST_DEVICES in print_matrix_configuration.py
Example:
iPhone 8, OS 12.0:
  --ios_name "iPhone 8" --ios_version "12.0"
Alternatively, to set an iOS device, use the one of the values below:
[simulator_min, simulator_target, simulator_latest]
Example:
  --ios_device "simulator_target"

----Android only----
Java 8 is required
Environment Variables (on MacOS)
    JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk-8-latest/Contents/Home
    ANDROID_HOME=/Users/user_name/Library/Android/sdk
Environment Variables (on Linux)
    JAVA_HOME=/usr/local/buildtools/java/jdk8/
    ANDROID_HOME=~/Android/Sdk

If you wish to specify a particular Android device to test on, you will need 
the sdk id and build tool version. These change over time. You can listing all 
available tools with the following commands:

  $ANDROID_HOME/tools/bin/sdkmanager --list

Device Information is stored in TEST_DEVICES in print_matrix_configuration.py
Example:
sdk id "system-images;android-29;google_apis;x86":
  --android_sdk "system-images;android-29;google_apis;x86" --build_tools_version "28.0.3"

Alternatively, to set an Android device, use the one of the values below:
[emulator_min, emulator_target, emulator_latest]
Example:
  --android_device "emulator_target" --build_tools_version "28.0.3"

Returns:
   1: No iOS/Android integration_test apps found
   20: Invalid ios_device flag  
   21: iOS Simulator created fail  
   22: iOS gameloop app not found
   23: build_testapps.json file not found
   30: Invalid android_device flag  
   31: For android test, JAVA_HOME is not set to java 8
"""

import json
import os
import pathlib
import subprocess
import time

from absl import app
from absl import flags
from absl import logging
import attr
from integration_testing import test_validation
from print_matrix_configuration import TEST_DEVICES

_GAMELOOP_PACKAGE = "com.google.firebase.gameloop"
_RESULT_FILE = "Results1.json"
_TEST_RETRY = 3

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "Testapps in this directory will be tested.")
flags.DEFINE_string(
    "ios_device", None,
    "iOS device, which is a combination of device name and os version"
    "See module docstring for details on how to set and get this id. "
    "If none, will use ios_name and ios_version.")
flags.DEFINE_string(
    "ios_name", "iPhone 8",
    "See module docstring for details on how to set and get this name.")
flags.DEFINE_string(
    "ios_version", "12.0",
    "See module docstring for details on how to set and get this version.")
flags.DEFINE_string(
    "tvos_device", None,
    "tvOS device, which is a combination of device name and os version"
    "See module docstring for details on how to set and get this id. "
    "If none, will use ios_name and ios_version.")
flags.DEFINE_string(
    "tvos_name", "Apple TV",
    "See module docstring for details on how to set and get this name.")
flags.DEFINE_string(
    "tvos_version", "14.0",
    "See module docstring for details on how to set and get this version.")
flags.DEFINE_string(
    "android_device", None,
    "Android device, which is the sdk id of an emulator image"
    "See module docstring for details on how to set and get this id."
    "If none, will use android_sdk.")
flags.DEFINE_string(
    "android_sdk", "system-images;android-29;google_apis;x86",
    "See module docstring for details on how to set and get this id.")
flags.DEFINE_string(
    "build_tools_version", "28.0.3",
    "android build_tools_version")
flags.DEFINE_string(
    "logfile_name", "simulator-test",
    "Create test log artifact test-results-$logfile_name.log."
    " logfile will be created and placed in testapp_dir.")   
flags.DEFINE_boolean(
    "ci", False,
    "If this script used in a CI system, set True.")


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
  
  ios_testapps = []
  tvos_testapps = []
  android_testapps = []
  for file_dir, directories, file_names in os.walk(testapp_dir):
    # .app is treated as a directory, not a file in MacOS
    for directory in directories:
      full_path = os.path.join(file_dir, directory)
      if directory.endswith("integration_test.app"):
        ios_testapps.append(full_path)
      elif directory.endswith("integration_test_tvos.app"):
        tvos_testapps.append(full_path)
    for file_name in file_names:
      full_path = os.path.join(file_dir, file_name)
      if file_name.endswith(".apk"):
        android_testapps.append(full_path)    

  if not ios_testapps and not tvos_testapps and not android_testapps:
    logging.info("No testapps found")
    return 1

  tests = []
  if ios_testapps:
    logging.info("iOS Testapps found: %s", "\n".join(path for path in ios_testapps))
    
    if FLAGS.ios_device:
      device_info = TEST_DEVICES.get(FLAGS.ios_device)
      if not device_info:
        logging.error("Not a valid ios device: %s" % FLAGS.ios_device)
        return 20
      device_name = device_info.get("name")
      device_os = device_info.get("version")
    else:
      device_name = FLAGS.ios_name
      device_os = FLAGS.ios_version

    device_id = _create_and_boot_simulator("iOS", device_name, device_os)
    if not device_id:
      logging.error("simulator created fail")
      return 21

    # A tool that enable game-loop test. This is a XCode project
    ios_gameloop_project = os.path.join(current_dir, "integration_testing", "gameloop_apple")
    ios_gameloop_app = _build_ios_gameloop(ios_gameloop_project, device_name, device_os)
    if not ios_gameloop_app:
      logging.error("gameloop app not found")
      return 22

    config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
    with open(config_path, "r") as configFile:
      config = json.load(configFile)
    if not config:
      logging.error("No config file found")
      return 23
  
    for app_path in ios_testapps:
      bundle_id = _get_bundle_id(app_path, config)
      logs=_run_apple_gameloop_test(bundle_id, app_path, ios_gameloop_app, device_id, _TEST_RETRY)
      tests.append(Test(testapp_path=app_path, logs=logs))

    _shutdown_simulator()

  if tvos_testapps:
    logging.info("tvOS Testapps found: %s", "\n".join(path for path in tvos_testapps))
    
    if FLAGS.tvos_device:
      device_info = TEST_DEVICES.get(FLAGS.tvos_device)
      if not device_info:
        logging.error("Not a valid tvos device: %s" % FLAGS.tvos_device)
        return 20
      device_name = device_info.get("name")
      device_os = device_info.get("version")
    else:
      device_name = FLAGS.tvos_name
      device_os = FLAGS.tvos_version

    device_id = _create_and_boot_simulator("tvOS", device_name, device_os)
    if not device_id:
      logging.error("simulator created fail")
      return 21

    # A tool that enable game-loop test. This is a XCode project
    tvos_gameloop_project = os.path.join(current_dir, "integration_testing", "gameloop_apple")
    tvos_gameloop_app = _build_tvos_gameloop(tvos_gameloop_project, device_name, device_os)
    if not tvos_gameloop_app:
      logging.error("gameloop app not found")
      return 22

    config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
    with open(config_path, "r") as configFile:
      config = json.load(configFile)
    if not config:
      logging.error("No config file found")
      return 23
  
    for app_path in tvos_testapps:
      bundle_id = _get_bundle_id(app_path, config)
      logs=_run_apple_gameloop_test(bundle_id, app_path, tvos_gameloop_app, device_id, _TEST_RETRY)
      tests.append(Test(testapp_path=app_path, logs=logs))

    _shutdown_simulator()


  if android_testapps:
    logging.info("Android Testapps found: %s", "\n".join(path for path in android_testapps))

    if FLAGS.android_device:
      device_info = TEST_DEVICES.get(FLAGS.android_device)
      if not device_info:
        logging.error("Not a valid android device: %s" % FLAGS.android_device)
        return 30
      sdk_id = device_info.get("image")
    else:
      sdk_id = FLAGS.android_sdk

    
    platform_version = sdk_id.split(";")[1]
    build_tool_version = FLAGS.build_tools_version

    if not _check_java_version():
      logging.error("Please set JAVA_HOME to java 8")
      return 31

    _setup_android(platform_version, build_tool_version, sdk_id)  

    _create_and_boot_emulator(sdk_id)

    android_gameloop_project = os.path.join(current_dir, "integration_testing", "gameloop_android")
    _install_android_gameloop_app(android_gameloop_project)

    for app_path in android_testapps:
      package_name = _get_package_name(app_path)
      logs=_run_android_gameloop_test(package_name, app_path, android_gameloop_project, _TEST_RETRY)
      tests.append(Test(testapp_path=app_path, logs=logs))

    _shutdown_emulator()

  return test_validation.summarize_test_results(
    tests, 
    test_validation.CPP, 
    testapp_dir, 
    file_name="test-results-" + FLAGS.logfile_name + ".log", 
    extra_info=" (ON SIMULATOR/EMULATOR)")


# -------------------Apple Only-------------------
def _build_ios_gameloop(gameloop_project, device_name, device_os):
  """Build gameloop UI Test app. 

  This gameloop app can run integration_test app automatically.
  """
  project_path = os.path.join(gameloop_project, "gameloop.xcodeproj")
  output_path = os.path.join(gameloop_project, "Build")

  """Build the gameloop app for test."""
  args = ["xcodebuild", "-project", project_path,
    "-scheme", "gameloop",
    "build-for-testing", 
    "-destination", "platform=iOS Simulator,name=%s,OS=%s" % (device_name, device_os), 
    "SYMROOT=%s" % output_path]
  logging.info("Building game-loop test: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  
  for file_dir, _, file_names in os.walk(output_path):
    for file_name in file_names:
      if file_name.endswith(".xctestrun") and "iphonesimulator" in file_name: 
        return os.path.join(file_dir, file_name)


def _build_tvos_gameloop(gameloop_project, device_name, device_os):
  """Build gameloop UI Test app. 

  This gameloop app can run integration_test app automatically.
  """
  project_path = os.path.join(gameloop_project, "gameloop.xcodeproj")
  output_path = os.path.join(gameloop_project, "Build")

  """Build the gameloop app for test."""
  args = ["xcodebuild", "-project", project_path,
    "-scheme", "gameloop_tvos",
    "build-for-testing", 
    "-destination", "platform=tvOS Simulator,name=%s,OS=%s" % (device_name, device_os), 
    "SYMROOT=%s" % output_path]
  logging.info("Building game-loop test: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  
  for file_dir, _, file_names in os.walk(output_path):
    for file_name in file_names:
      if file_name.endswith(".xctestrun") and "appletvsimulator" in file_name: 
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


def _shutdown_simulator():
  args = ["xcrun", "simctl", "shutdown", "all"]
  logging.info("Shutdown all simulators: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _create_and_boot_simulator(apple_platform, device_name, device_os):
  """Create a simulator locally. Will wait until this simulator booted."""
  _shutdown_simulator()
  command = "xcrun xctrace list devices 2>&1 | grep \"%s (%s)\" | awk -F'[()]' '{print $4}'" % (device_name, device_os)
  logging.info("Get test simulator: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  device_id = result.stdout.read().strip()

  if not device_id:
    # download and create device
    os.environ["GEM_HOME"] = "$HOME/.gem"
    args = ["gem", "install", "xcode-install"]
    logging.info("Download xcode-install: %s", " ".join(args))
    subprocess.run(args=args, check=True)

    args = ["xcversion", "simulators", "--install=%s %s" % (apple_platform, device_os)]
    logging.info("Download simulator: %s", " ".join(args))
    subprocess.run(args=args, check=False)
    
    args = ["xcrun", "simctl", "create", "test_simulator", device_name, "%s%s" % (apple_platform, device_os)]
    logging.info("Create test simulator: %s", " ".join(args))
    result = subprocess.run(args=args, capture_output=True, text=True, check=True)
    device_id = result.stdout.strip()

  args = ["xcrun", "simctl", "boot", device_id]
  logging.info("Boot my simulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  args = ["xcrun", "simctl", "bootstatus", device_id]
  logging.info("Wait for simulator to boot: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  return device_id


def _delete_simulator(device_id):
  """Delete the created simulator."""
  args = ["xcrun", "simctl", "delete", device_id]
  logging.info("Delete created simulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _get_bundle_id(app_path, config):
  """Get app bundle id from build_testapps.json file."""
  for api in config["apis"]:
    if api["name"] != "app" and (api["name"] in app_path or api["full_name"] in app_path):
      return api["bundle_id"]


def _run_apple_gameloop_test(bundle_id, app_path, gameloop_app, device_id, retry=1):
  """Run gameloop test and collect test result."""
  logging.info("Running apple gameloop test: %s, %s, %s, %s", bundle_id, app_path, gameloop_app, device_id)
  _install_apple_app(app_path, device_id)
  _run_xctest(gameloop_app, device_id)
  log = _get_apple_test_log(bundle_id, app_path, device_id)
  _uninstall_apple_app(bundle_id, device_id)
  if retry > 1:
    result = test_validation.validate_results(log, test_validation.CPP)
    if not result.complete:
      logging.info("Retry _run_apple_gameloop_test. Remaining retry: %s", retry-1)
      return _run_apple_gameloop_test(bundle_id, app_path, gameloop_app, device_id, retry=retry-1)
  
  return log
  

def _install_apple_app(app_path, device_id):
  """Install integration_test app into the simulator."""
  args = ["xcrun", "simctl", "install", device_id, app_path]
  logging.info("Install testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _uninstall_apple_app(bundle_id, device_id):
  """Uninstall integration_test app from the simulator."""
  args = ["xcrun", "simctl", "uninstall", device_id, bundle_id]
  logging.info("Uninstall testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _get_apple_test_log(bundle_id, app_path, device_id):
  """Read integration_test app testing result."""
  args=["xcrun", "simctl", "get_app_container", device_id, bundle_id, "data"]
  logging.info("Get test result: %s", " ".join(args))
  result = subprocess.run(
      args=args,
      capture_output=True, text=True, check=False)
  
  if not result.stdout:
    logging.info("No test Result")
    return None

  log_path = os.path.join(result.stdout.strip(), "Documents", "GameLoopResults", _RESULT_FILE) 
  log = _read_file(log_path) 
  logging.info("Apple test result: %s", log)
  return log


def _read_file(path):
  """Extracts the contents of a file."""
  if os.path.isfile(path):
    with open(path, "r") as f:
      test_result = f.read()

    logging.info("Reading file: %s", path)
    logging.info("File content: %s", test_result)
    return test_result


# -------------------Android Only-------------------
def _check_java_version():
  command = "java -version 2>&1 | awk -F[\\\"_] 'NR==1{print $2}'"
  logging.info("Get java version: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  java_version = result.stdout.read().strip()
  logging.info("Java version: %s", java_version)
  return "1.8" in java_version


def _setup_android(platform_version, build_tool_version, sdk_id):
  android_home = os.environ["ANDROID_HOME"]
  pathlist = [os.path.join(android_home, "emulator"), 
    os.path.join(android_home, "tools"), 
    os.path.join(android_home, "tools", "bin"), 
    os.path.join(android_home, "platform-tools"), 
    os.path.join(android_home, "build-tools", build_tool_version)]
  os.environ["PATH"] += os.pathsep + os.pathsep.join(pathlist)

  args = ["sdkmanager", 
    "emulator", "platform-tools", 
    "platforms;%s" % platform_version, 
    "build-tools;%s" % build_tool_version]
  logging.info("Install packages: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  command = "yes | sdkmanager --licenses"
  logging.info("Accept all licenses: %s", command)
  subprocess.run(command, shell=True, check=False)

  args = ["sdkmanager", sdk_id]
  logging.info("Download an emulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  args = ["sdkmanager", "--update"]
  logging.info("Update all installed packages: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _shutdown_emulator():
  command = "adb devices | grep emulator | cut -f1 | while read line; do adb -s $line emu kill; done"
  logging.info("Kill all running emulator: %s", command)
  subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  time.sleep(5)
  args = ["adb", "kill-server"]
  logging.info("Kill adb server: %s", " ".join(args))
  subprocess.run(args=args, check=False)
  time.sleep(5)


def _create_and_boot_emulator(sdk_id):
  _shutdown_emulator()

  command = "echo no | avdmanager -s create avd -n test_emulator -k '%s' -f" % sdk_id
  logging.info("Create an emulator: %s", command)
  subprocess.run(command, shell=True, check=True)

  args = ["adb", "start-server"]
  logging.info("Start adb server: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  if not FLAGS.ci: 
    command = "$ANDROID_HOME/emulator/emulator -avd test_emulator &"
  else:
    command = "$ANDROID_HOME/emulator/emulator -avd test_emulator -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect &"
  logging.info("Boot test emulator: %s", command)
  subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)

  args = ["adb", "wait-for-device"]
  logging.info("Wait for emulator to boot: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  if FLAGS.ci: 
    # wait extra 90 seconds to ensure emulator booted.
    time.sleep(90)
  else:
    time.sleep(45)


def _reset_emulator_on_error(instrumented_test_result):
  logging.info("game-loop test result: %s", instrumented_test_result)
  if "FAILURES!!!" in instrumented_test_result:
    logging.info("game-loop test error!!! reboot emualtor...")
    args = ["adb", "-e", "reboot"]
    logging.info("Reboot android emulator: %s", " ".join(args))
    subprocess.run(args=args, check=True)
    args = ["adb", "wait-for-device"]
    logging.info("Wait for emulator to boot: %s", " ".join(args))
    subprocess.run(args=args, check=True)
    if FLAGS.ci: 
      # wait extra 90 seconds to ensure emulator booted.
      time.sleep(90)
    else:
      time.sleep(45)


def _get_package_name(app_path):
  command = "aapt dump badging %s | awk -v FS=\"'\" '/package: name=/{print $2}'" % app_path
  logging.info("Get package_name: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  package_name = result.stdout.read().strip()
  return package_name  


def _run_android_gameloop_test(package_name, app_path, gameloop_project, retry=1): 
  logging.info("Running android gameloop test: %s, %s, %s", package_name, app_path, gameloop_project)
  _install_android_app(app_path)
  _run_instrumented_test()
  log = _get_android_test_log(package_name)
  _uninstall_android_app(package_name)
  if retry > 1:
    result = test_validation.validate_results(log, test_validation.CPP)
    if not result.complete:
      logging.info("Retry _run_android_gameloop_test. Remaining retry: %s", retry-1)
      return _run_android_gameloop_test(package_name, app_path, gameloop_project, retry=retry-1)
  
  return log


def _install_android_app(app_path):
  """Install integration_test app into the emulator."""
  args = ["adb", "install", app_path]
  logging.info("Install testapp: %s", " ".join(args))
  subprocess.run(args=args, check=False)


def _uninstall_android_app(package_name):
  """Uninstall integration_test app from the emulator."""
  args = ["adb", "uninstall", package_name]
  logging.info("Uninstall testapp: %s", " ".join(args))
  subprocess.run(args=args, check=False)


def _install_android_gameloop_app(gameloop_project):
  os.chdir(gameloop_project)
  logging.info("CD to gameloop_project: %s", gameloop_project)
  _uninstall_android_app("com.google.firebase.gameloop")
  args = ["./gradlew", "clean"]
  logging.info("Clean game-loop cache: %s", " ".join(args))
  subprocess.run(args=args, check=False)
  args = ["./gradlew", "installDebug", "installDebugAndroidTest"]
  logging.info("Installing game-loop app and test: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _run_instrumented_test():
  """Run the gameloop UI Test app.
    This gameloop app can run integration_test app automatically.
  """
  args = ["adb", "shell", "am", "instrument",
    "-w", "%s.test/androidx.test.runner.AndroidJUnitRunner" % _GAMELOOP_PACKAGE] 
  logging.info("Running game-loop test: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=False) 
  _reset_emulator_on_error(result.stdout)


def _get_android_test_log(test_package):
  """Read integration_test app testing result."""
  # getFilesDir()	-> /data/data/package/files
  path = "/data/data/%s/files/%s/%s" % (_GAMELOOP_PACKAGE, test_package, _RESULT_FILE)
  args = ["adb", "shell", "su", "0", "cat", path]
  logging.info("Get android test result: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=False)
  logging.info("Android test result: %s", result.stdout)
  return result.stdout


if __name__ == '__main__':
  flags.mark_flag_as_required("testapp_dir")
  app.run(main)
