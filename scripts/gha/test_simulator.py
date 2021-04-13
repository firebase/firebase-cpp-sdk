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

r"""Tool for mobile testapps to Test on local simulators/emulaotrs.

Usage:

  python test_simulator.py --testapp_dir ~/testapps 

This will recursively search ~/testapps for apps,
test on local simulators/emulaotrs, and validate their results. The validation is 
specific to the structure of the Firebase Unity and C++ testapps.

----iOS only----
Requires simulators installed. iOS simulator can be installed via tool:
  https://github.com/xcpretty/xcode-install#simulators

If you wish to specify a particular iOS device to test on, you will need the model
id and version (OS version for iOS). These change over time. You can listing all 
available simulators (supported models and versions) with the following commands:

  xcrun simctl list

Note: you need to combine Name and Version with ";". Examples:
iPhone 11, OS 14.4:
  --ios_device "iPhone 11;14.4"

----Android only----
Environment Variables (on MacOS)
    JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk-8-latest/Contents/Home
    ANDROID_HOME=/Users/user_name/Library/Android/sdk
Environment Variables (on Linux)
    JAVA_HOME=/usr/local/buildtools/java/jdk/
    ANDROID_HOME=~/Android/Sdk

If you wish to specify a particular Android device to test on, you will need 
the sdk id and build tool version. These change over time. You can listing all 
available tools with the following commands:

  $ANDROID_HOME/tools/bin/sdkmanager --list

Note: you need to combine them with ";". Examples:

sdk id "system-images;android-29;google_apis", build tool "29.0.3":
  --android_device "system-images;android-29;google_apis;x86;29.0.3"

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

_GAMELOOP_PACKAGE = "com.google.firebase.gameloop"

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "Testapps in this directory will be tested.")
flags.DEFINE_string(
    "ios_device", "iPhone 11;14.4",
    "iOS device, which is a combination of device name and os version")
flags.DEFINE_string(
    "android_device", "system-images;android-29;google_apis;x86;29.0.3",
    "android device, which is a combination of sdk id and build tool version")
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
  android_testapps = []
  for file_dir, directories, file_names in os.walk(testapp_dir):
    # .app is treated as a directory, not a file in MacOS
    for directory in directories:
      full_path = os.path.join(file_dir, directory)
      if "simulator" in full_path and directory.endswith(".app"):
        ios_testapps.append(full_path)
    for file_name in file_names:
      full_path = os.path.join(file_dir, file_name)
      if file_name.endswith(".apk"):
        android_testapps.append(full_path)    

  if not ios_testapps and not android_testapps:
    logging.info("No testapps found")
    return 1

  tests = []
  if ios_testapps:
    logging.info("iOS Testapps found: %s", "\n".join(path for path in ios_testapps))
    
    ios_device = FLAGS.ios_device
    device_info = ios_device.split(";")
    device_name = device_info[0]
    device_os = device_info[1]

    device_id = _boot_simulator(device_name, device_os)
    if not device_id:
      logging.error("simulator created fail")
      return 1

    # A tool that enable game-loop test. This is a XCode project
    ios_gameloop_project = os.path.join(current_dir, "integration_testing", "gameloop")
    ios_gameloop_app = _build_ios_gameloop(ios_gameloop_project, device_name, device_os)
    if not ios_gameloop_app:
      logging.error("gameloop app not found")
      return 2

    config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
    with open(config_path, "r") as config:
      config = json.load(config)
    if not config:
      logging.error("No config found")
      return 3
  
    for app_path in ios_testapps:
      bundle_id = _get_bundle_id(app_path, config)
      tests.append(Test(
                      testapp_path=app_path, 
                      logs=_run_ios_gameloop_test(bundle_id, app_path, ios_gameloop_app, device_id)))

  if android_testapps:
    logging.info("Android Testapps found: %s", "\n".join(path for path in android_testapps))

    android_device = FLAGS.android_device
    device_info = android_device.rsplit(";", 1)
    sdk_id = device_info[0]
    platforms_version = sdk_id.split(";")[1]
    build_tools_version = device_info[1]

    if not _check_java_version():
      logging.error("Please set JAVA_HOME to java 8")
      return 1

    _set_android_env(platforms_version, build_tools_version)  

    _create_and_boot_emulator(sdk_id)

    android_gameloop_project = os.path.join(current_dir, "integration_testing", "gameloop_android")
    _install_android_gameloop_app(android_gameloop_project)

    for app_path in android_testapps:
      package_name = _get_package_name(app_path)
      tests.append(Test(
                      testapp_path=app_path, 
                      logs=_run_android_gameloop_test(package_name, app_path, android_gameloop_project)))

  return test_validation.summarize_test_results(
    tests, test_validation.CPP, testapp_dir)


def _build_ios_gameloop(gameloop_project, device_name, device_os):
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
  logging.info("Building game-loop test: %s", " ".join(args))
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


def _run_ios_gameloop_test(bundle_id, app_path, gameloop_app, device_id):
  """Run gameloop test and collect test result."""
  logging.info("Running iOS gameloop test: %s, %s, %s, %s", bundle_id, app_path, gameloop_app, device_id)
  _install_ios_app(app_path, device_id)
  _run_xctest(gameloop_app, device_id)
  logs = _get_ios_test_log(bundle_id, app_path, device_id)
  _uninstall_ios_app(bundle_id, device_id)
  return logs


def _install_ios_app(app_path, device_id):
  """Install integration_test app into the simulator."""
  args = ["xcrun", "simctl", "install", device_id, app_path]
  logging.info("Install testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _uninstall_ios_app(bundle_id, device_id):
  """Uninstall integration_test app from the simulator."""
  args = ["xcrun", "simctl", "uninstall", device_id, bundle_id]
  logging.info("Uninstall testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _get_ios_test_log(bundle_id, app_path, device_id):
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


def _check_java_version():
  command = "java -version 2>&1 | awk -F[\\\"_] 'NR==1{print $2}'"
  logging.info("Get java version: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  java_version = result.stdout.read().strip()
  return "1.8" in java_version


def _set_android_env(platforms_version, build_tools_version):
  android_home = os.environ["ANDROID_HOME"]
  pathlist = ["%s/emulator" % android_home, 
    "%s/tools" % android_home, 
    "%s/tools/bin" % android_home, 
    "%s/platform-tools" % android_home, 
    "%s/build-tools/%s" % (android_home, build_tools_version)]
  os.environ["PATH"] += os.pathsep + os.pathsep.join(pathlist)
  
  args = ["sdkmanager", 
    "emulator", "platform-tools", 
    "platforms;%s" % platforms_version, 
    "build-tools;%s" % build_tools_version]
  logging.info("Install packages: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  args = ["sdkmanager", "--update"]
  logging.info("Update all installed packages: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _create_and_boot_emulator(sdk_id):
  args = ["sdkmanager", sdk_id]
  logging.info("Download an emulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  args = ["avdmanager", "-s", 
    "create", "avd", 
    "-n", "test_emulator", 
    "-k", sdk_id, 
    "-f"]
  logging.info("Create an emulator: %s", args)
  p_no = subprocess.Popen(["echo", "no"], stdout=subprocess.PIPE)
  proc = subprocess.Popen(args, stdin=p_no.stdout, stdout=subprocess.PIPE)
  p_no.stdout.close()
  proc.communicate()

  args = ["adb", "start-server"]
  logging.info("Start adb server: %s", " ".join(args))
  subprocess.run(args=args, check=True)

  command = "adb devices | grep emulator | cut -f1 | while read line; do adb -s $line emu kill; done"
  logging.info("Kill all running emulator: %s", command)
  subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)

  if not FLAGS.ci: 
    command = "emulator -avd test_emulator &"
  else:
    command = "emulator -avd test_emulator -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect &"
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



def _get_package_name(app_path):
  command = "aapt dump badging %s | awk -v FS=\"'\" '/package: name=/{print $2}'" % app_path
  logging.info("Get package_name: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  package_name = result.stdout.read().strip()
  return package_name  


def _run_android_gameloop_test(package_name, app_path, gameloop_project): 
  logging.info("Running android gameloop test: %s, %s, %s", package_name, app_path, gameloop_project)
  _install_android_app(app_path)
  _run_instrumented_test()
  logs = _get_android_test_log(package_name)
  _uninstall_android_app(package_name)
  return logs


def _install_android_app(app_path):
  """Install integration_test app into the emulator."""
  args = ["adb", "install", app_path]
  logging.info("Install testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _uninstall_android_app(package_name):
  """Uninstall integration_test app from the emulator."""
  args = ["adb", "uninstall", package_name]
  logging.info("Uninstall testapp: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _install_android_gameloop_app(gameloop_project):
  os.chdir(gameloop_project)
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
  subprocess.run(args=args, check=False) 


def _get_android_test_log(test_package):
  """Read integration_test app testing result."""
  path = "$EXTERNAL_STORAGE/Android/data/%s/files/%s/results1.json" % (_GAMELOOP_PACKAGE, test_package)
  args = ["adb", "shell", "cat", path]
  logging.info("Running game-loop test: %s", " ".join(args))
  result = subprocess.run(args=args, check=False)
  return result.stdout


if __name__ == '__main__':
  flags.mark_flag_as_required("testapp_dir")
  app.run(main)
