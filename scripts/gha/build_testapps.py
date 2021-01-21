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

By default, this tool will build integration tests from source, which involves
building the underlying SDK libraries. To build from a packaged/released SDK,
supply the path to the SDK to --packaged_sdk:

python build_testapps.py --t auth --p iOS --packaged_sdk ~/firebase_cpp_sdk

Under most circumstances the other flags don't need to be set, but can be
seen by running --help. Note that all path flags will forcefully expand
the user ~.


DEPENDENCIES:

----Firebase Repo----
The Firebase C++ SDK repo must be locally present.
Path specified by the flag:

    --repo_dir (default: current working directory)

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
import platform
import shutil
import subprocess
import sys

from absl import app
from absl import flags
from absl import logging

import attr

from integration_testing import config_reader
from integration_testing import test_validation
from integration_testing import xcodebuild
import utils

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
    "packaged_sdk", None, "(Optional) Firebase SDK directory. If not"
    " supplied, will build from source.")

flags.DEFINE_string(
    "output_directory", "~",
    "Build output will be placed in this directory.")

flags.DEFINE_string(
    "repo_dir", os.getcwd(),
    "Firebase C++ SDK Git repository. Current directory by default.")

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

flags.DEFINE_string(
    "compiler", None,
    "(Desktop only) Specify the compiler with CMake during the testapps build."
    " Check the config file to see valid choices for this flag."
    " If none, will invoke cmake without specifying a compiler.")

flags.DEFINE_multi_string(
    "cmake_flag", None,
    "Pass an additional flag to the CMake configure step."
    " This option can be specified multiple times.")

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

  sdk_dir = _fix_path(FLAGS.packaged_sdk or FLAGS.repo_dir)
  output_dir = _fix_path(FLAGS.output_directory)
  repo_dir = _fix_path(FLAGS.repo_dir)

  update_pod_repo = FLAGS.update_pod_repo
  if FLAGS.add_timestamp:
    timestamp = datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
  else:
    timestamp = ""
  output_dir = os.path.join(output_dir, "testapps" + timestamp)

  ios_framework_dir = os.path.join(sdk_dir, "frameworks")
  ios_framework_exist = os.path.isdir(ios_framework_dir)
  if not ios_framework_exist and _IOS in platforms:
    _generate_makefiles_from_repo(sdk_dir)

  if update_pod_repo and _IOS in platforms:
    _run(["pod", "repo", "update"])

  config = config_reader.read_config()
  cmake_flags = _get_desktop_compiler_flags(FLAGS.compiler, config.compilers)
  # VCPKG is used to install dependencies for the desktop SDK.
  # Building from source requires building the underlying SDK libraries,
  # so we need to use VCPKG as well.
  if _DESKTOP in platforms and not FLAGS.packaged_sdk:
    installer = os.path.join(repo_dir, "scripts", "gha", "build_desktop.py")
    _run([sys.executable, installer, "--vcpkg_step_only"])
    toolchain_file = os.path.join(
        repo_dir, "external", "vcpkg", "scripts", "buildsystems", "vcpkg.cmake")
    cmake_flags.extend((
        "-DCMAKE_TOOLCHAIN_FILE=%s" % toolchain_file,
        "-DVCPKG_TARGET_TRIPLET=%s" % utils.get_vcpkg_triplet(arch="x64")
    ))

  if FLAGS.cmake_flag:
    cmake_flags.extend(FLAGS.cmake_flag)

  failures = []
  for testapp in testapps:
    logging.info("BEGIN building for %s", testapp)
    failures += _build(
        testapp=testapp,
        platforms=platforms,
        api_config=config.get_api(testapp),
        output_dir=output_dir,
        sdk_dir=sdk_dir,
        ios_framework_exist=ios_framework_exist,
        repo_dir=repo_dir,
        ios_sdk=FLAGS.ios_sdk,
        cmake_flags=cmake_flags)
    logging.info("END building for %s", testapp)

  _summarize_results(testapps, platforms, failures, output_dir)
  return 1 if failures else 0


def _build(
    testapp, platforms, api_config, output_dir, sdk_dir, ios_framework_exist,
    repo_dir, ios_sdk, cmake_flags):
  """Builds one testapp on each of the specified platforms."""
  testapp_dir = os.path.join(repo_dir, api_config.testapp_path)
  project_dir = os.path.join(
      output_dir, api_config.full_name, os.path.basename(testapp_dir))

  logging.info("Copying testapp project to %s", project_dir)
  os.makedirs(project_dir)
  dir_util.copy_tree(testapp_dir, project_dir)

  logging.info("Changing directory to %s", project_dir)
  os.chdir(project_dir)

  _run_setup_script(repo_dir, project_dir)

  failures = []

  if _DESKTOP in platforms:
    logging.info("BEGIN %s, %s", testapp, _DESKTOP)
    try:
      _build_desktop(sdk_dir, cmake_flags)
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
          repo_dir=repo_dir,
          api_config=api_config,
          ios_sdk=ios_sdk)
    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_IOS, error_message=str(e)))
    logging.info("END %s, %s", testapp, _IOS)

  return failures


def _summarize_results(testapps, platforms, failures, output_dir):
  """Logs a readable summary of the results of the build."""
  summary = []
  summary.append("BUILD SUMMARY:")
  summary.append("TRIED TO BUILD: " + ",".join(testapps))
  summary.append("ON PLATFORMS: " + ",".join(platforms))

  if not failures:
    summary.append("ALL BUILDS SUCCEEDED")
  else:
    summary.append("SOME FAILURES OCCURRED:")
    for i, failure in enumerate(failures, start=1):
      summary.append("%d: %s" % (i, failure.describe()))
  summary = "\n".join(summary)

  logging.info(summary)
  test_validation.write_summary(output_dir, summary)


def _build_desktop(sdk_dir, cmake_flags):
  cmake_configure_cmd = ["cmake", ".", "-DCMAKE_BUILD_TYPE=Debug",
                                       "-DFIREBASE_CPP_SDK_DIR=" + sdk_dir]
  if utils.is_windows_os():
    cmake_configure_cmd += ["-A", "x64"]
  _run(cmake_configure_cmd + cmake_flags)
  _run(["cmake", "--build", ".", "--config", "Debug"])


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
  if platform.system() == "Windows":
    gradlew = "gradlew.bat"
    sdk_dir = sdk_dir.replace("\\", "/")  # Gradle misinterprets backslashes.
  else:
    gradlew = "./gradlew"
  logging.info("Patching gradle properties with path to SDK")
  gradle_properties = os.path.join(project_dir, "gradle.properties")
  with open(gradle_properties, "a+") as f:
    f.write("systemProp.firebase_cpp_sdk.dir=" + sdk_dir + "\n")
  # This will log the versions of dependencies for debugging purposes.
  _run([gradlew, "dependencies", "--configuration", "debugCompileClasspath"])
  _run([gradlew, "assembleDebug", "--stacktrace"])


def _validate_android_environment_variables():
  """Checks environment variables that may be required for Android."""
  # Ultimately we let the gradle build be the source of truth on what env vars
  # are required, but try to repair holes and log warnings if we can't.
  android_home = os.environ.get(_ANDROID_HOME)
  if not os.environ.get(_JAVA_HOME):
    logging.warning("%s not set", _JAVA_HOME)
  if not os.environ.get(_ANDROID_SDK_HOME):
    if android_home:  # Use ANDROID_HOME as backup for ANDROID_SDK_HOME
      os.environ[_ANDROID_SDK_HOME] = android_home
      logging.info("%s not found, using %s", _ANDROID_SDK_HOME, _ANDROID_HOME)
    else:
      logging.warning("Missing: %s and %s", _ANDROID_SDK_HOME, _ANDROID_HOME)
  # Different environments may have different NDK env vars specified. We look
  # for these, in this order, and set the others to the first found.
  # If none are set, we check the default location for the ndk.
  ndk_path = None
  ndk_vars = [_NDK_ROOT, _ANDROID_NDK_HOME]
  for env_var in ndk_vars:
    val = os.environ.get(env_var)
    if val:
      ndk_path = val
      break
  if not ndk_path:
    if android_home:
      default_ndk_path = os.path.join(android_home, "ndk-bundle")
      if os.path.isdir(default_ndk_path):
        ndk_path = default_ndk_path
  if ndk_path:
    logging.info("Found ndk: %s", ndk_path)
    for env_var in ndk_vars:
      if os.environ.get(env_var) != ndk_path:
        logging.info("Setting %s to %s", env_var, ndk_path)
        os.environ[env_var] = ndk_path
  else:
    logging.warning("No NDK env var set. Set one of %s", ", ".join(ndk_vars))


# If sdk_dir contains no framework, consider it is Github repo, then
# generate makefiles for ios frameworks
def _generate_makefiles_from_repo(sdk_dir):
  """Generates cmake makefiles for building iOS frameworks from SDK source."""
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
  """Builds iOS framework from SDK source."""
  ios_framework_builder = os.path.join(
      sdk_dir, "build_scripts", "ios", "build.sh")

  # build only required targets to save time
  target = set()

  for framework in api_config.frameworks:
    # firebase_analytics.framework -> firebase_analytics
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
    sdk_dir, ios_framework_exist, project_dir, repo_dir, api_config, ios_sdk):
  """Builds an iOS application (.app, .ipa or both)."""
  if not ios_framework_exist:
    _build_ios_framework_from_repo(sdk_dir, api_config)

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

  podfile_tool_path = os.path.join(
      repo_dir, "scripts", "gha", "integration_testing", "update_podfile.py")
  podfile_patcher_args = [
      sys.executable, podfile_tool_path,
      "--sdk_podfile", os.path.join(repo_dir, "ios_pod", "Podfile"),
      "--app_podfile", os.path.join(project_dir, "Podfile")
  ]
  _run(podfile_patcher_args)
  _run(["pod", "install"])

  entitlements_path = os.path.join(
      project_dir, api_config.ios_target + ".entitlements")
  xcode_tool_path = os.path.join(
      repo_dir, "scripts", "gha", "integration_testing", "xcode_tool.rb")
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

    xcodebuild.generate_unsigned_ipa(
        output_dir=build_dir, configuration="Debug")


# This should be executed before performing any builds.
def _run_setup_script(root_dir, testapp_dir):
  """Runs the setup_integration_tests.py script."""
  # This script will download gtest to its own directory.
  # The CMake projects were configured to download gtest, but this was
  # found to be flaky and errors didn't propagate up the build system
  # layers. The workaround is to download gtest with this script and copy it.
  downloader_dir = os.path.join(root_dir, "testing", "test_framework")
  _run([sys.executable, os.path.join(downloader_dir, "download_googletest.py")])
  # Copies shared test framework files into the project, including gtest.
  script_path = os.path.join(root_dir, "setup_integration_tests.py")
  _run([sys.executable, script_path, testapp_dir])


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
  logging.info("Deleting %s...", directory_path)
  try:
    shutil.rmtree(directory_path)
  except OSError as e:
    # There are two known cases where this can happen:
    # The directory doesn't exist (FileNotFoundError)
    # A file in the directory is open in another process (PermissionError)
    logging.warning("Failed to remove directory:\n%s", e)


def _fix_path(path):
  """Expands ~, normalizes slashes, and converts relative paths to absolute."""
  return os.path.abspath(os.path.expanduser(path))


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
