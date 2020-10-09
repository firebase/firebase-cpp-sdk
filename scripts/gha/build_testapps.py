# Copyright 2018 Google LLC
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

r"""Build automation tool for Firebase C++ testapps for desktop and mobile.

USAGE:

This tool has a number of dependencies (listed below). Once those are taken
care of, here is an example of an execution of the tool (on MacOS):

python build_testapps.py --t auth,messaging --p iOS

Critical flags:
--t (full name: testapps, default: None)
--p (full name: platforms, default: None)

Under most circumstances the other flags don't need to be set, but can be
seen by running --help. Note that all path flags will forcefully expand
the user ~.

DEPENDENCIES:

----Firebase SDK----
The Firebase SDK (prebuilt) or repo must be locally present.
Path specified by the flag:

    --sdk_dir (default: current working directory)

----Python Dependencies----
The requirements.txt file has the required dependencies for this Python tool.

    pip install -r requirements.txt

----CMake (Desktop only)----
CMake must be installed and on the system path.

----Environment Variables (Android only)----
If building for Android, gradle requires several environment variables.
The following lists expected variables, and examples of what
a configured value may look like on MacOS:

    JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk-8-latest/Contents/Home
    ANDROID_HOME=/Users/user_name/Library/Android/sdk
    ANDROID_SDK_HOME=/Users/user_name/Library/Android/sdk
    ANDROID_NDK_HOME=/Users/user_name/Library/Android/sdk/ndk-bundle

Or on Linux:
    JAVA_HOME=/usr/local/buildtools/java/jdk/
    ANDROID_HOME=~/Android/Sdk
    ANDROID_SDK_HOME=~/Android/Sdk
    ANDROID_NDK_HOME=~/Android/Sdk/ndk

If using this tool frequently, you will likely find it convenient to
modify your bashrc file to automatically set these variables.

"""

import datetime
from distutils import dir_util
import os
import pathlib
import platform
import shutil
import subprocess

from absl import app
from absl import flags
from absl import logging

import attr

from integration_testing import config_reader
from integration_testing import provisioning
from integration_testing import xcodebuild

# Environment variables
_JAVA_HOME = "JAVA_HOME"
_ANDROID_HOME = "ANDROID_HOME"
_ANDROID_SDK_HOME = "ANDROID_SDK_HOME"
_NDK_ROOT = "NDK_ROOT"
_ANDROID_NDK_HOME = "ANDROID_NDK_HOME"

# Platforms
_ANDROID = "Android"
_IOS = "iOS"
_DESKTOP = "Desktop"
_SUPPORTED_PLATFORMS = (_ANDROID, _IOS, _DESKTOP)

# Values for iOS SDK flag (where the iOS app will run)
_IOS_SDK_DEVICE = "device"
_IOS_SDK_SIMULATOR = "simulator"
_IOS_SDK_BOTH = "both"
_SUPPORTED_IOS_SDK = (_IOS_SDK_DEVICE, _IOS_SDK_SIMULATOR, _IOS_SDK_BOTH)

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "sdk_dir", os.getcwd(), "Unzipped Firebase C++ sdk OR Github repo.")

flags.DEFINE_string(
    "output_directory", "~",
    "Build output will be placed in this directory.")

flags.DEFINE_string(
    "root_dir", os.getcwd(),
    "Directory with which to join the relative paths in the config."
    " Used to find e.g. the integration test projects. If using the SDK repo"
    " this will be the same as the sdk dir, but not if using prebuilts."
    " Defaults to the current directory.")

flags.DEFINE_list(
    "testapps", None, "Which testapps (Firebase APIs) to build, e.g."
    " 'analytics,auth'.",
    short_name="t")

flags.DEFINE_list(
    "platforms", None, "Which platforms to build. Can be Android, iOS and/or"
    " Desktop", short_name="p")

flags.DEFINE_bool(
    "add_timestamp", True,
    "Add a timestamp to the output directory for disambiguation."
    " Recommended when running locally, so each execution gets its own "
    " directory.")

flags.DEFINE_enum(
    "ios_sdk", _IOS_SDK_DEVICE, _SUPPORTED_IOS_SDK,
    "(iOS only) Build for device (ipa), simulator (app), or both."
    " Building for both will produce both an .app and an .ipa.")

flags.DEFINE_bool(
    "update_pod_repo", True,
    "(iOS only) Will run 'pod repo update' before building for iOS to update"
    " the local spec repos available on this machine. Must also include iOS"
    " in platforms flag.")

flags.DEFINE_bool(
    "execute_desktop_testapp", True,
    "(Desktop only) Run the testapp after building it. Will return non-zero"
    " code if any tests fail inside the testapp.")

flags.DEFINE_string(
    "compiler", None,
    "(Desktop only) Specify the compiler with CMake during the testapps build."
    " Check the config file to see valid choices for this flag."
    " If none, will invoke cmake without specifying a compiler.")

flags.DEFINE_bool(
    "use_vcpkg", False,
    "(Desktop only) Use the vcpkg repo inside the C++ repo. For"
    " this to work, sdk_dir must be set to the repo, not the prebuilt SDK."
    " Will install vcpkg, use it to install dependencies, and then configure"
    " CMake to use it.")


flags.register_validator(
    "platforms",
    lambda p: all(platform in _SUPPORTED_PLATFORMS for platform in p),
    message="Valid platforms: " + ",".join(_SUPPORTED_PLATFORMS),
    flag_values=FLAGS)


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  platforms = FLAGS.platforms
  testapps = FLAGS.testapps

  update_pod_repo = FLAGS.update_pod_repo
  if FLAGS.add_timestamp:
    timestamp = datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
  else:
    timestamp = ""

  config = config_reader.read_config()

  ios_framework_dir = os.path.join(os.path.expanduser(FLAGS.sdk_dir), "frameworks")
  ios_framework_exist = os.path.isdir(ios_framework_dir)
  if not ios_framework_exist and _IOS in platforms:
    _generate_makefiles_from_repo(FLAGS.sdk_dir)

  if update_pod_repo and _IOS in platforms:
    _run(["pod", "repo", "update"])

  cmake_flags = _get_desktop_compiler_flags(FLAGS.compiler, config.compilers)
  if _DESKTOP in platforms and FLAGS.use_vcpkg:
    _run(["git", "submodule", "update", "--init"])
    vcpkg = Vcpkg.generate(os.path.join(FLAGS.sdk_dir, config.vcpkg_dir))
    vcpkg.install_and_run()
    cmake_flags.extend(vcpkg.cmake_flags)

  failures = []
  for testapp in testapps:
    logging.info("BEGIN building for %s", testapp)
    failures += _build(
        testapp=testapp,
        platforms=platforms,
        api_config=config.get_api(testapp),
        output_dir=os.path.expanduser(FLAGS.output_directory),
        sdk_dir=os.path.expanduser(FLAGS.sdk_dir),
        ios_framework_exist=ios_framework_exist,
        timestamp=timestamp,
        root_dir=os.path.expanduser(FLAGS.root_dir),
        ios_sdk=FLAGS.ios_sdk,
        cmake_flags=cmake_flags,
        execute_desktop_testapp=FLAGS.execute_desktop_testapp)
    logging.info("END building for %s", testapp)

  _summarize_results(testapps, platforms, failures)
  return 1 if failures else 0


def _build(
    testapp, platforms, api_config, output_dir, sdk_dir, ios_framework_exist, 
    timestamp, root_dir, ios_sdk, cmake_flags, execute_desktop_testapp):
  """Builds one testapp on each of the specified platforms."""
  testapp_dir = os.path.join(root_dir, api_config.testapp_path)
  project_dir = os.path.join(
      output_dir, "testapps" + timestamp, api_config.full_name,
      os.path.basename(testapp_dir))

  logging.info("Copying testapp project to %s", project_dir)
  os.makedirs(project_dir)
  dir_util.copy_tree(testapp_dir, project_dir)

  logging.info("Changing directory to %s", project_dir)
  os.chdir(project_dir)

  _run_setup_script(root_dir, project_dir)

  failures = []

  if _DESKTOP in platforms:
    logging.info("BEGIN %s, %s", testapp, _DESKTOP)
    try:
      _build_desktop(sdk_dir, cmake_flags)
      if execute_desktop_testapp:
        _execute_desktop_testapp(project_dir)
    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_DESKTOP, error_message=str(e)))
    _rm_dir_safe(os.path.join(project_dir, "bin"))
    logging.info("END %s, %s", testapp, _DESKTOP)

  if _ANDROID in platforms:
    logging.info("BEGIN %s, %s", testapp, _ANDROID)
    try:
      _validate_android_environment_variables()
      _build_android(project_dir, sdk_dir)
    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_ANDROID, error_message=str(e)))
    _rm_dir_safe(os.path.join(project_dir, "build", "intermediates"))
    _rm_dir_safe(os.path.join(project_dir, ".externalNativeBuild"))
    logging.info("END %s, %s", testapp, _ANDROID)

  if _IOS in platforms:
    logging.info("BEGIN %s, %s", testapp, _IOS)
    try:
      _build_ios(
          sdk_dir=sdk_dir,
          ios_framework_exist=ios_framework_exist,
          project_dir=project_dir,
          root_dir=root_dir,
          api_config=api_config,
          ios_sdk=ios_sdk)
    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_IOS, error_message=str(e)))
    logging.info("END %s, %s", testapp, _IOS)

  return failures


def _summarize_results(testapps, platforms, failures):
  """Logs a readable summary of the results of the build."""
  logging.info(
      "FINISHED BUILDING TESTAPPS.\n\n\n"
      "Tried to build these testapps: %s\n"
      "On these platforms: %s",
      ", ".join(testapps), ", ".join(platforms))
  if not failures:
    logging.info("No failures occurred")
  else:
    # Collect lines, then log once, to reduce logging noise from timestamps etc.
    lines = ["Some failures occurred:"]
    for i, failure in enumerate(failures, start=1):
      lines.append("%d: %s" % (i, failure.describe()))
    logging.info("\n".join(lines))


def _build_desktop(sdk_dir, cmake_flags):
  _run(["cmake", ".", "-DFIREBASE_CPP_SDK_DIR=" + sdk_dir] + cmake_flags)
  _run(["cmake", "--build", "."])


def _execute_desktop_testapp(project_dir):
  if platform.system() == "Windows":
    testapp_path = os.path.join(project_dir, "Debug", "integration_test.exe")
  else:
    testapp_path = os.path.join(project_dir, "integration_test")
  _run([testapp_path], timeout=300)


def _get_desktop_compiler_flags(compiler, compiler_table):
  """Returns the command line flags for this compiler."""
  if not compiler:  # None is an acceptable default value
    return []
  try:
    return compiler_table[compiler]
  except KeyError:
    valid_keys = ", ".join(compiler_table.keys())
    raise ValueError(
        "Given compiler: %s. Valid compilers: %s" % (compiler, valid_keys))


def _build_android(project_dir, sdk_dir):
  """Builds an Android binary (apk)."""
  logging.info("Patching gradle properties with path to SDK")
  gradle_properties = os.path.join(project_dir, "gradle.properties")
  with open(gradle_properties, "a+") as f:
    f.write("systemProp.firebase_cpp_sdk.dir=" + sdk_dir + "\n")
  # This will log the versions of dependencies for debugging purposes.
  _run(
      ["./gradlew", "dependencies", "--configuration", "debugCompileClasspath"])
  # Building for Android has a known issue that can be worked around by
  # simply building again. Since building from source takes a while, we don't
  # want to retry the build if a different error occurred.
  build_args = ["./gradlew", "assembleDebug", "--stacktrace"]
  result = _run(args=build_args, capture_output=True, text=True, check=False)
  if result.returncode:
    if "Execution failed for task ':generateJsonModel" in result.stderr:
      logging.info("Task failed for ':generateJsonModel<target>'. Retrying.")
      _run(args=build_args)
    else:
      logging.error(result.stderr)
      raise subprocess.CalledProcessError(
          returncode=result.returncode,
          cmd=build_args)


def _validate_android_environment_variables():
  """Checks environment variables that may be required for Android."""
  # Ultimately we let the gradle build be the source of truth on what env vars
  # are required, but try to repair holes and log warnings if we can't.
  logging.info("Checking environment variables for the Android build")
  if not os.environ.get(_ANDROID_NDK_HOME):
    ndk_root = os.environ.get(_NDK_ROOT)
    if ndk_root:  # Use NDK_ROOT as a backup for ANDROID_NDK_HOME
      os.environ[_ANDROID_NDK_HOME] = ndk_root
      logging.info("%s not found, using %s", _ANDROID_NDK_HOME, _NDK_ROOT)
    else:
      logging.warning("Neither %s nor %s is set.", _ANDROID_NDK_HOME, _NDK_ROOT)
  if not os.environ.get(_JAVA_HOME):
    logging.warning("%s not set", _JAVA_HOME)
  if not os.environ.get(_ANDROID_SDK_HOME):
    android_home = os.environ.get(_ANDROID_HOME)
    if android_home:  # Use ANDROID_HOME as backup for ANDROID_SDK_HOME
      os.environ[_ANDROID_SDK_HOME] = android_home
      logging.info("%s not found, using %s", _ANDROID_SDK_HOME, _ANDROID_HOME)
    else:
      logging.warning(
          "Neither %s nor %s is set", _ANDROID_SDK_HOME, _ANDROID_HOME)


# If sdk_dir contains no framework, consider it is Github repo, then 
# generate makefiles for ios frameworks
def _generate_makefiles_from_repo(sdk_dir):
  ios_framework_builder = os.path.join(
    sdk_dir, "build_scripts", "ios", "build.sh")

  framework_builder_args = [
      ios_framework_builder,
      "-b", sdk_dir,
      "-s", sdk_dir,
      "-c", "false"
  ]
  _run(framework_builder_args)


# build required ios frameworks based on makefiles
def _build_ios_framework_from_repo(sdk_dir, api_config):
  sdk_dir = os.path.expanduser(sdk_dir)
  ios_framework_builder = os.path.join(
    sdk_dir, "build_scripts", "ios", "build.sh")
  
  # build only required targets to save time
  target = set()
  for framework in api_config.frameworks:
    target.add(os.path.splitext(framework)[0])
  # firebase is not a target in CMake, firebase_app is the target
  # firebase_app will be built by other target as well 
  target.remove("firebase") 

  framework_builder_args = [
      ios_framework_builder,
      "-b", sdk_dir,
      "-t", ",".join(target),
      "-g", "false"
  ]
  _run(framework_builder_args)


def _build_ios(
    sdk_dir, ios_framework_exist, project_dir, root_dir, api_config, ios_sdk):
  if not ios_framework_exist:
    _build_ios_framework_from_repo(sdk_dir, api_config)

  """Builds an iOS application (.app, .ipa or both)."""
  build_dir = os.path.join(project_dir, "ios_build")
  os.makedirs(build_dir)

  logging.info("Copying XCode frameworks")
  framework_src_dir = os.path.join(sdk_dir, "frameworks", "ios", "universal")
  framework_paths = []  # Paths to the copied frameworks.
  for framework in api_config.frameworks:
    framework_src_path = os.path.join(framework_src_dir, framework)
    framework_dest_path = os.path.join(project_dir, "Frameworks", framework)
    dir_util.copy_tree(framework_src_path, framework_dest_path)
    framework_paths.append(framework_dest_path)

  _run(["pod", "install"])

  entitlements_path = os.path.join(
      project_dir, api_config.ios_target + ".entitlements")
  xcode_tool_path = os.path.join(
      root_dir, "scripts", "gha", "integration_testing", "xcode_tool.rb")
  xcode_patcher_args = [
      "ruby", xcode_tool_path,
      "--XCodeCPP.xcodeProjectDir", project_dir,
      "--XCodeCPP.target", api_config.ios_target,
      "--XCodeCPP.frameworks", ",".join(framework_paths)
  ]
  if os.path.isfile(entitlements_path):  # Not all testapps require entitlements
    logging.info("Entitlements file detected.")
    xcode_patcher_args.extend(("--XCodeCPP.entitlement", entitlements_path))
  else:
    logging.info("No entitlements found at %s.", entitlements_path)
  _run(xcode_patcher_args)

  xcode_path = os.path.join(project_dir, api_config.ios_target + ".xcworkspace")
  if ios_sdk in [_IOS_SDK_SIMULATOR, _IOS_SDK_BOTH]:
    _run(
        xcodebuild.get_args_for_build(
            path=xcode_path,
            scheme=api_config.scheme,
            output_dir=build_dir,
            ios_sdk=_IOS_SDK_SIMULATOR,
            configuration="Debug"))

  if ios_sdk in [_IOS_SDK_DEVICE, _IOS_SDK_BOTH]:
    _run(
        xcodebuild.get_args_for_build(
            path=xcode_path,
            scheme=api_config.scheme,
            output_dir=build_dir,
            ios_sdk=_IOS_SDK_DEVICE,
            configuration="Debug"))

    xcodebuild.generate_unsigned_ipa(output_dir=build_dir, configuration="Debug")


# This script is responsible for copying shared files into the integration
# test projects. Should be executed before performing any builds.
def _run_setup_script(root_dir, testapp_dir):
  """Runs the setup_integration_tests.py script if needed."""
  script_path = os.path.join(root_dir, "setup_integration_tests.py")
  if os.path.isfile(script_path):
    _run(["python", script_path, testapp_dir])
  else:
    logging.info("setup_integration_tests.py not found")


def _run(args, timeout=2400, capture_output=False, text=None, check=True):
  """Executes a command in a subprocess."""
  logging.info("Running in subprocess: %s", " ".join(args))
  return subprocess.run(
      args=args,
      timeout=timeout,
      capture_output=capture_output,
      text=text,
      check=check)


def _rm_dir_safe(directory_path):
  """Removes directory at given path. No error if dir doesn't exist."""
  try:
    shutil.rmtree(directory_path)
    logging.info("Deleted %s", directory_path)
  except FileNotFoundError:
    logging.warning("Tried to delete %s, but it doesn't exist.", directory_path)


@attr.s(frozen=True, eq=False)
class Vcpkg(object):
  """Holds data related to the vcpkg tool used for managing dependent tools."""
  installer = attr.ib()
  binary = attr.ib()
  triplet = attr.ib()
  response_file = attr.ib()
  toolchain_file = attr.ib()

  @classmethod
  def generate(cls, vcpkg_dir):
    """Generates the vcpkg data based on the given vcpkg submodule path."""
    installer = os.path.join(vcpkg_dir, "bootstrap-vcpkg")
    binary = os.path.join(vcpkg_dir, "vcpkg")
    response_file_fmt = vcpkg_dir + "_%s_response_file.txt"
    if platform.system() == "Windows":
      triplet = "x64-windows-static"
      installer += ".bat"
      binary += ".exe"
    elif platform.system() == "Darwin":
      triplet = "x64-osx"
      installer += ".sh"
    elif platform.system() == "Linux":
      triplet = "x64-linux"
      installer += ".sh"
    else:
      raise ValueError("Unrecognized system: %s" % platform.system())
    return cls(
        installer=installer,
        binary=binary,
        triplet=triplet,
        response_file=response_file_fmt % triplet,
        toolchain_file=os.path.join(
            vcpkg_dir, "scripts", "buildsystems", "vcpkg.cmake"))

  def install_and_run(self):
    """Installs vcpkg (if needed) and runs it to install dependencies."""
    if not os.path.exists(self.binary):
      _run([self.installer])
    _run([
        self.binary, "install", "@" + self.response_file, "--disable-metrics"])

  @property
  def cmake_flags(self):
    return [
        "-DCMAKE_TOOLCHAIN_FILE=%s" % self.toolchain_file,
        "-DVCPKG_TARGET_TRIPLET=%s" % self.triplet]


@attr.s(frozen=True, eq=False)
class Failure(object):
  """Holds context for the failure of a testapp to build/run."""
  testapp = attr.ib()
  platform = attr.ib()
  error_message = attr.ib()

  def describe(self):
    return "%s, %s: %s" % (self.testapp, self.platform, self.error_message)


if __name__ == "__main__":
  flags.mark_flag_as_required("testapps")
  flags.mark_flag_as_required("platforms")
  app.run(main)
