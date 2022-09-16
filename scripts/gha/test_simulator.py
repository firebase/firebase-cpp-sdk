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

  python test_simulator.py --testapp_dir ~/testapps --test_type gameloop

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
  --android_sdk "system-images;android-29;google_apis;x86" --build_tools_version "29.0.2"

Alternatively, to set an Android device, use the one of the values below:
[emulator_min, emulator_target, emulator_latest]
Example:
  --android_device "emulator_target" --build_tools_version "29.0.2"

Returns:
   1: No iOS/Android integration_test apps found
   20: Invalid ios_device flag  
   21: iOS Simulator created fail  
   22: iOS helper app not found
   23: build_testapps.json file not found
   30: Invalid android_device flag  
   31: For android test, JAVA_HOME is not set to java 8
"""

import json
import os
import pathlib
import platform
import shutil
import signal
import subprocess
import tempfile
import time

from absl import app
from absl import flags
from absl import logging
import attr
from integration_testing import test_validation
from print_matrix_configuration import TEST_DEVICES


# Gameloop test will skip UI Tests
_TEST_TYPE_GAMELOOP = "gameloop"
# UITest only triggers UI Tests, and skip non-UI Tests
_TEST_TYPE_UITEST = "uitest"

CONSTANTS = {
  _TEST_TYPE_GAMELOOP: {
    "apple_path": "integration_testing/gameloop_apple",
    "apple_scheme": "gameloop",
    "android_path": "integration_testing/gameloop_android",
    "android_package": "com.google.firebase.gameloop",
  },
  _TEST_TYPE_UITEST: {
    "apple_path": "ui_testing/uitest_apple",
    "apple_scheme": "FirebaseCppUITestApp",
    "android_path": "ui_testing/uitest_android",
    "android_package": "com.google.firebase.uitest",
  }
}

_RESULT_FILE = "Results1.json"
_MAX_ATTEMPTS = 3
_CMD_TIMEOUT = 300

_DEVICE_NONE = "None"
_DEVICE_ANDROID = "Android"
_DEVICE_APPLE = "Apple"

_RESET_TYPE_REBOOT = 1
_RESET_TYPE_WIPE_REBOOT = 2

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "Testapps in this directory will be tested.")
flags.DEFINE_string(
    "test_type", _TEST_TYPE_GAMELOOP,
    "Gameloop Test or UI Test")
flags.DEFINE_string(
    "ios_device", None,
    "iOS device, which is a combination of device name and os version"
    "See module docstring for details on how to set and get this id. "
    "If none, will use ios_name and ios_version.")
flags.DEFINE_string(
    "ios_name", "iPhone 8",
    "See module docstring for details on how to set and get this name.")
flags.DEFINE_string(
    "ios_version", "14.5",
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
    "tvos_version", "14.5",
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
    "build_tools_version", "29.0.2",
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
  
  config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
  with open(config_path, "r") as configFile:
    config = json.load(configFile)
  if not config:
    logging.error("No config file found")
    return 23

  ios_testapps = []
  tvos_testapps = []
  android_testapps = []
  for file_dir, directories, file_names in os.walk(testapp_dir):
    # .app is treated as a directory, not a file in MacOS
    for directory in directories:
      full_path = os.path.join(file_dir, directory)
      if directory.endswith("integration_test.app"):
        if FLAGS.test_type == _TEST_TYPE_UITEST and not _has_uitests(full_path, config):
          logging.info("Skip %s, as it has no uitest", full_path)
        else:
          ios_testapps.append(full_path)
      elif directory.endswith("integration_test_tvos.app"):
        if FLAGS.test_type == _TEST_TYPE_UITEST and not _has_uitests(full_path, config):
          logging.info("Skip %s, as it has no uitest", full_path)
        else:
          tvos_testapps.append(full_path)
    for file_name in file_names:
      full_path = os.path.join(file_dir, file_name)
      if file_name.endswith(".apk"):
        if FLAGS.test_type == _TEST_TYPE_UITEST and not _has_uitests(full_path, config):
          logging.info("Skip %s, as it has no uitest", full_path)
        else:
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
    ios_helper_project = os.path.join(current_dir, CONSTANTS[FLAGS.test_type]["apple_path"])
    ios_helper_app = _build_ios_helper(ios_helper_project, device_name, device_os)
    if not ios_helper_app:
      logging.error("helper app not found")
      return 22
  
    for app_path in ios_testapps:
      bundle_id = _get_bundle_id(app_path, config)
      logs=_run_apple_test(testapp_dir, bundle_id, app_path, ios_helper_app, device_id)
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
    tvos_helper_project = os.path.join(current_dir, CONSTANTS[FLAGS.test_type]["apple_path"])
    tvos_helper_app = _build_tvos_helper(tvos_helper_project, device_name, device_os)
    if not tvos_helper_app:
      logging.error("helper app not found")
      return 22

    config_path = os.path.join(current_dir, "integration_testing", "build_testapps.json")
    with open(config_path, "r") as configFile:
      config = json.load(configFile)
    if not config:
      logging.error("No config file found")
      return 23
  
    for app_path in tvos_testapps:
      bundle_id = _get_bundle_id(app_path, config)
      logs=_run_apple_test(testapp_dir, bundle_id, app_path, tvos_helper_app, device_id, record_video_display="external")
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

    android_helper_project = os.path.join(current_dir, CONSTANTS[FLAGS.test_type]["android_path"])
    _install_android_helper_app(android_helper_project)

    for app_path in android_testapps:
      package_name = _get_package_name(app_path)
      logs=_run_android_test(testapp_dir, package_name, app_path, android_helper_project)
      tests.append(Test(testapp_path=app_path, logs=logs))

    _shutdown_emulator()

  return test_validation.summarize_test_results(
    tests, 
    test_validation.CPP, 
    testapp_dir, 
    test_type=FLAGS.test_type,
    file_name="test-results-" + FLAGS.logfile_name + ".log", 
    extra_info=" (ON SIMULATOR/EMULATOR)")


# -------------------Apple Only-------------------
def _build_ios_helper(helper_project, device_name, device_os):
  """Build helper UI Test app. 

  This helper app can run integration_test app automatically.
  """
  project_path = os.path.join(helper_project, "%s.xcodeproj" % CONSTANTS[FLAGS.test_type]["apple_scheme"])
  output_path = os.path.join(helper_project, "Build")

  """Build the helper app for test."""
  args = ["xcodebuild", "-project", project_path,
    "-scheme", CONSTANTS[FLAGS.test_type]["apple_scheme"],
    "build-for-testing", 
    "-destination", "platform=iOS Simulator,name=%s,OS=%s" % (device_name, device_os), 
    "SYMROOT=%s" % output_path]
  logging.info("Building game-loop test: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  
  for file_dir, _, file_names in os.walk(output_path):
    for file_name in file_names:
      if file_name.endswith(".xctestrun") and "iphonesimulator" in file_name: 
        return os.path.join(file_dir, file_name)


def _build_tvos_helper(helper_project, device_name, device_os):
  """Build helper UI Test app. 

  This helper app can run integration_test app automatically.
  """
  project_path = os.path.join(helper_project, "%s.xcodeproj" % CONSTANTS[FLAGS.test_type]["apple_scheme"])
  output_path = os.path.join(helper_project, "Build")

  """Build the helper app for test."""
  args = ["xcodebuild", "-project", project_path,
    "-scheme", "%s_tvos" % CONSTANTS[FLAGS.test_type]["apple_scheme"],
    "build-for-testing", 
    "-destination", "platform=tvOS Simulator,name=%s,OS=%s" % (device_name, device_os), 
    "SYMROOT=%s" % output_path]
  logging.info("Building game-loop test: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  
  for file_dir, _, file_names in os.walk(output_path):
    for file_name in file_names:
      if file_name.endswith(".xctestrun") and "appletvsimulator" in file_name: 
        return os.path.join(file_dir, file_name)


def _record_apple_tests(video_name, display):
  args = [
    "xcrun",
    "simctl",
    "io",
    "booted",
    "recordVideo",
    "-f",
    "--codec=h264",
  ]
  if display is not None:
    args.append("--display=" + display)
  args.append(video_name)
  return _start_recording(args)


def _start_recording(args):
  logging.info("Starting screen recording: %s", subprocess.list2cmdline(args))

  output_file = tempfile.TemporaryFile()

  # Specify CREATE_NEW_PROCESS_GROUP on Windows because it is required for
  # sending CTRL_C_SIGNAL, which is done by _stop_recording().
  proc = subprocess.Popen(
    args,
    stdout=output_file,
    stderr=subprocess.STDOUT,
    creationflags=
        subprocess.CREATE_NEW_PROCESS_GROUP
        if platform.system() == "Windows"
        else 0,
  )
  logging.info("Started screen recording with PID %s", proc.pid)
  return (proc, output_file)


def _stop_recording(start_recording_retval):
  (proc, output_file) = start_recording_retval
  logging.info("Stopping screen recording with PID %s by sending CTRL+C",
    proc.pid)

  # Make sure that `returncode` is set on the `Popen` object.
  proc.poll()

  if proc.returncode is not None:
    logging.warning(
      "WARNING: Screen recording process ended prematurely with exit code %s",
      proc.returncode
    )
    output_file.seek(0)
    logging.warning("==== BEGIN screen recording output ====")
    for line in output_file.read().decode("utf8", errors="replace").splitlines():
      logging.warning("%s", line.rstrip())
    logging.warning("==== END screen recording output ====")

  output_file.close()

  proc.send_signal(
    signal.CTRL_C_SIGNAL
    if platform.system() == "Windows"
    else signal.SIGINT
  )
  proc.wait()
  time.sleep(5)


def _save_recorded_apple_video(video_name, summary_dir):
  logging.info("Save test video %s to: %s", video_name, summary_dir)
  if not os.path.exists(video_name):
    logging.warning("Save test video failed: file not found: %s", video_name)
    return
  shutil.move(video_name, os.path.join(summary_dir, video_name))


def _run_xctest(helper_app, device_id):
  """Run the helper app.
    This helper app can run integration_test app automatically.
  """
  args = ["xcodebuild", "test-without-building", 
    "-xctestrun", helper_app, 
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


def _reset_simulator_on_error(device_id, type=_RESET_TYPE_REBOOT):
  _shutdown_simulator()

  if type == _RESET_TYPE_WIPE_REBOOT:
    args = ["xcrun", "simctl", "erase", device_id]
    logging.info("Erase my simulator: %s", " ".join(args))
    subprocess.run(args=args, check=True)

  # reboot simulator: _RESET_TYPE_WIPE_REBOOT, _RESET_TYPE_REBOOT
  args = ["xcrun", "simctl", "boot", device_id]
  logging.info("Reboot my simulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)


def _get_bundle_id(app_path, config):
  """Get app bundle id from build_testapps.json file."""
  for api in config["apis"]:
    if api["name"] != "app" and (api["name"] in app_path or api["full_name"] in app_path):
      return api["bundle_id"]


def _has_uitests(app_path, config):
  """Get app bundle id from build_testapps.json file."""
  for api in config["apis"]:
    if api["name"] != "app" and (api["name"] in app_path or api["full_name"] in app_path):
      return api.get("has_uitests", False)


def _run_apple_test(testapp_dir, bundle_id, app_path, helper_app, device_id, max_attempts=_MAX_ATTEMPTS, record_video_display=None):
  """Run helper test and collect test result."""
  attempt_num = 1
  while attempt_num <= max_attempts:
    logging.info("Running apple helper test (attempt %s of %s): %s, %s, %s, %s", attempt_num, max_attempts, bundle_id, app_path, helper_app, device_id)
    _install_apple_app(app_path, device_id)
    video_name = "video-%s-%s-%s.mp4" % (bundle_id, attempt_num, FLAGS.logfile_name)
    record_process = _record_apple_tests(video_name, display=record_video_display)
    _run_xctest(helper_app, device_id)
    _stop_recording(record_process)
    log = _get_apple_test_log(bundle_id, app_path, device_id)
    _uninstall_apple_app(bundle_id, device_id)
    result = test_validation.validate_results(log, test_validation.CPP)
    if result.complete and (FLAGS.test_type != "uitest" or result.fails == 0):
      break
    _save_recorded_apple_video(video_name, testapp_dir)
    attempt_num += 1

  return log
  

def _install_apple_app(app_path, device_id):
  """Install integration_test app into the simulator."""
  args = ["xcrun", "simctl", "install", device_id, app_path]
  logging.info("Install testapp: %s", " ".join(args))
  _run_with_retry(args, device=device_id, type=_RESET_TYPE_WIPE_REBOOT)


def _uninstall_apple_app(bundle_id, device_id):
  """Uninstall integration_test app from the simulator."""
  args = ["xcrun", "simctl", "uninstall", device_id, bundle_id]
  logging.info("Uninstall testapp: %s", " ".join(args))
  _run_with_retry(args, device=device_id, type=_RESET_TYPE_REBOOT)


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
  _run_with_retry(args)

  command = "yes | sdkmanager --licenses"
  logging.info("Accept all licenses: %s", command)
  _run_with_retry(command, shell=True, check=False)

  args = ["sdkmanager", sdk_id]
  logging.info("Download an emulator: %s", " ".join(args))
  _run_with_retry(args)

  args = ["sdkmanager", "--update"]
  logging.info("Update all installed packages: %s", " ".join(args))
  _run_with_retry(args, check=False)


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
    command = "$ANDROID_HOME/emulator/emulator -avd test_emulator -no-window -no-audio -no-boot-anim -gpu auto &"
  logging.info("Boot test emulator: %s", command)
  subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)

  args = ["adb", "wait-for-device"]
  logging.info("Wait for emulator to boot: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  if FLAGS.ci: 
    # wait extra 210 seconds to ensure emulator fully booted.
    time.sleep(210)
  else:
    time.sleep(45)


def _reset_emulator_on_error(type=_RESET_TYPE_REBOOT):
  if type == _RESET_TYPE_WIPE_REBOOT:
    # wipe emulator data
    args = ["adb", "shell", "recovery", "--wipe_data"]
    logging.info("Erase my Emulator: %s", " ".join(args))
    subprocess.run(args=args, check=True)

  # reboot emulator: _RESET_TYPE_WIPE_REBOOT, _RESET_TYPE_REBOOT
  logging.info("game-loop test error!!! reboot emualtor...")
  args = ["adb", "-e", "reboot"]
  logging.info("Reboot android emulator: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  args = ["adb", "wait-for-device"]
  logging.info("Wait for emulator to boot: %s", " ".join(args))
  subprocess.run(args=args, check=True)
  if FLAGS.ci: 
    # wait extra 210 seconds to ensure emulator booted.
    time.sleep(210)
  else:
    time.sleep(45)


def _get_package_name(app_path):
  command = "aapt dump badging %s | awk -v FS=\"'\" '/package: name=/{print $2}'" % app_path
  logging.info("Get package_name: %s", command)
  result = subprocess.Popen(command, universal_newlines=True, shell=True, stdout=subprocess.PIPE)
  package_name = result.stdout.read().strip()
  return package_name  


def _run_android_test(testapp_dir, package_name, app_path, helper_project, max_attempts=_MAX_ATTEMPTS): 
  attempt_num = 1
  while attempt_num <= max_attempts:
    logging.info("Running android helper test (attempt %s of %s): %s, %s, %s", attempt_num, max_attempts, package_name, app_path, helper_project)
    _install_android_app(app_path)
    video_name = "video-%s-%s-%s.mp4" % (package_name, attempt_num, FLAGS.logfile_name)
    logcat_name = "logcat-%s-%s-%s.txt" % (package_name, attempt_num, FLAGS.logfile_name)
    record_process = _record_android_tests(video_name)
    _clear_android_logcat()
    _run_instrumented_test()
    _stop_recording(record_process)
    log = _get_android_test_log(package_name)
    _uninstall_android_app(package_name)

    result = test_validation.validate_results(log, test_validation.CPP)
    if result.complete and result.fails == 0:
      break

    _save_recorded_android_video(video_name, testapp_dir)
    _save_android_logcat(logcat_name, testapp_dir)
    attempt_num += 1
  
  return log


def _install_android_app(app_path):
  """Install integration_test app into the emulator."""
  args = ["adb", "install", app_path]
  logging.info("Install testapp: %s", " ".join(args))
  _run_with_retry(args, device=_DEVICE_ANDROID, type=_RESET_TYPE_WIPE_REBOOT)


def _uninstall_android_app(package_name):
  """Uninstall integration_test app from the emulator."""
  args = ["adb", "uninstall", package_name]
  logging.info("Uninstall testapp: %s", " ".join(args))
  _run_with_retry(args, device=_DEVICE_ANDROID, type=_RESET_TYPE_REBOOT)


def _install_android_helper_app(helper_project):
  os.chdir(helper_project)
  logging.info("cd to helper_project: %s", helper_project)
  args = ["adb", "uninstall", CONSTANTS[FLAGS.test_type]["android_package"]]
  _run_with_retry(args, check=False, device=_DEVICE_ANDROID, type=_RESET_TYPE_REBOOT)

  args = ["./gradlew", "clean"]
  logging.info("Clean game-loop cache: %s", " ".join(args))
  _run_with_retry(args, check=False, device=_DEVICE_ANDROID, type=_RESET_TYPE_REBOOT)

  args = ["./gradlew", "installDebug", "installDebugAndroidTest"]
  logging.info("Installing game-loop app and test: %s", " ".join(args))
  _run_with_retry(args, device=_DEVICE_ANDROID, type=_RESET_TYPE_REBOOT)


def _record_android_tests(video_name):
  return _start_recording([
    "adb",
    "shell",
    "screenrecord",
    "--bugreport",
    "/sdcard/%s" % video_name,
  ])


def _save_recorded_android_video(video_name, summary_dir):
  args = ["adb", "pull", "/sdcard/%s" % video_name, summary_dir]
  logging.info("Save test video: %s", " ".join(args))
  subprocess.run(args=args, capture_output=True, text=True, check=False) 


def _save_android_logcat(logcat_name, summary_dir):
  logcat_file = os.path.join(summary_dir, logcat_name)
  args = ["adb", "logcat", "-d"]
  logging.info("Save logcat to %s: %s", logcat_file, " ".join(args))
  with open(logcat_file, "wb") as f:
    subprocess.run(args=args, stdout=f, check=False)


def _clear_android_logcat():
  args = ["adb", "logcat", "-c"]
  logging.info("Clear logcat: %s", " ".join(args))
  subprocess.run(args=args, check=False)


def _run_instrumented_test():
  """Run the helper app.
    This helper app can run integration_test app automatically.
  """
  args = ["adb", "shell", "am", "instrument",
    "-w", "%s.test/androidx.test.runner.AndroidJUnitRunner" % CONSTANTS[FLAGS.test_type]["android_package"]] 
  logging.info("Running game-loop test: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=False) 
  # if "FAILURES!!!" in result.stdout:
  #   _reset_emulator_on_error(_RESET_TYPE_REBOOT)


def _get_android_test_log(test_package):
  """Read integration_test app testing result."""
  # getFilesDir()	-> /data/data/package/files
  path = "/data/data/%s/files/%s/%s" % (CONSTANTS[FLAGS.test_type]["android_package"], test_package, _RESULT_FILE)
  args = ["adb", "shell", "su", "0", "cat", path]
  logging.info("Get android test result: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=False)
  logging.info("Android test result: %s", result.stdout)
  return result.stdout


def _run_with_retry(args, shell=False, check=True, timeout=_CMD_TIMEOUT, max_attempts=_MAX_ATTEMPTS, device=_DEVICE_NONE, type=_RESET_TYPE_REBOOT):
  attempt_num = 1
  while attempt_num <= max_attempts:
    logging.info("run_with_retry: %s (attempt %s of %s)", args, attempt_num, max_attempts)
    try:
      subprocess.run(args, shell=shell, check=check, timeout=timeout)
    except subprocess.SubprocessError:
      logging.exception("run_with_retry: %s (attempt %s of %s) FAILED", args, attempt_num, max_attempts)

      # If retries have been exhausted, just raise the exception
      if attempt_num >= max_attempts:
        raise

      # Otherwise, reset the emulator/simulator and try again.
      if device == _DEVICE_NONE:
        pass
      elif device == _DEVICE_ANDROID:
        # Android
        _reset_emulator_on_error(type)
      else:
        # Apple
        _reset_simulator_on_error(device, type)
    else:
      break

    attempt_num += 1


if __name__ == '__main__':
  flags.mark_flag_as_required("testapp_dir")
  app.run(main)
