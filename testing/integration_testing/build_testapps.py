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

python build_testapps.py --t auth,invites --p iOS

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

    --sdk_dir (default: current working directory),

----Python Dependencies----
The requirements.txt file has the required dependencies for this Python tool.

    pip install -r requirements.txt

----CMake (Desktop only)----
CMake must be installed and on the system path.

----Environment Variables (Android only)----
If building for Android, this tool requires several environment variables.
If any are missing, the tool will terminate and report the missing environment
variables. The following lists the required variables, and examples of what
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

----Mobile Provisions (iOS only)----
If building for iOS, the required mobile provisions must be installed and
locally present.
Path specified by the flag:

    --provision_dir (default: ~/Downloads/export)

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

import config_reader
import provisioning
import xcodebuild

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

_REQUIRED_ANDROID_ENV_VARS = [
    "JAVA_HOME", "ANDROID_HOME", "ANDROID_SDK_HOME", "ANDROID_NDK_HOME"
]

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

flags.DEFINE_string(
    "provisions_dir", "~/Downloads/export",
    "(iOS only) Directory containing the mobileprovision.")

flags.DEFINE_bool(
    "execute_desktop_testapp", True,
    "(Desktop only) Run the testapp after building it. Will return non-zero"
    " code if any tests fail inside the testapp.")

flags.DEFINE_string(
    "compiler", None,
    "(Desktop only) Specify the compiler with CMake during the testapps build."
    " Check the config file to see valid choices for this flag."
    " If none, will invoke cmake without specifying a compiler.")

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

  if update_pod_repo and _IOS in platforms:
    _run(["pod", "repo", "update"])

  failures = []
  for testapp in testapps:
    logging.info("BEGIN building for %s", testapp)
    failures += _build(
        testapp=testapp,
        platforms=platforms,
        config=config_reader.read_config(),
        output_dir=os.path.expanduser(FLAGS.output_directory),
        sdk_dir=os.path.expanduser(FLAGS.sdk_dir),
        timestamp=timestamp,
        builder_dir=pathlib.Path(__file__).parent.absolute(),
        root_dir=os.path.expanduser(FLAGS.root_dir),
        provisions_dir=os.path.expanduser(FLAGS.provisions_dir),
        ios_sdk=FLAGS.ios_sdk,
        desktop_compiler=FLAGS.compiler,
        execute_desktop_testapp=FLAGS.execute_desktop_testapp)
    logging.info("END building for %s", testapp)

  _summarize_results(testapps, platforms, failures)
  return 1 if failures else 0


def _build(
    testapp, platforms, config, output_dir, sdk_dir, timestamp, builder_dir,
    root_dir, provisions_dir, ios_sdk, desktop_compiler,
    execute_desktop_testapp):
  """Builds one testapp on each of the specified platforms."""
  api_config = APIConfig.from_config(config, testapp)
  testapp_dir = os.path.join(root_dir, api_config.api_dir)
  project_dir = os.path.join(
      output_dir, "testapps" + timestamp, api_config.full_name,
      os.path.basename(testapp_dir))

  logging.info("Copying testapp project to %s", project_dir)
  os.makedirs(project_dir)
  dir_util.copy_tree(testapp_dir, project_dir)

  logging.info("Changing directory to %s", project_dir)
  os.chdir(project_dir)

  _run_setup_script(sdk_dir, project_dir)

  failures = []

  if _DESKTOP in platforms:
    logging.info("BEGIN %s, %s", testapp, _DESKTOP)
    try:
      compiler_flags = _get_desktop_compiler_flags(desktop_compiler, config)
      _build_desktop(sdk_dir, compiler_flags)
      if execute_desktop_testapp:
        _execute_desktop_testapp(project_dir)
    except subprocess.CalledProcessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_DESKTOP, error_message=str(e)))
    logging.info("END %s, %s", testapp, _DESKTOP)

  if _ANDROID in platforms:
    logging.info("BEGIN %s, %s", testapp, _ANDROID)
    try:
      _validate_android_environment_variables()
      _build_android(project_dir, sdk_dir)
    except subprocess.CalledProcessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_ANDROID, error_message=str(e)))
    logging.info("END %s, %s", testapp, _ANDROID)

  if _IOS in platforms:
    logging.info("BEGIN %s, %s", testapp, _IOS)
    try:
      _build_ios(
          sdk_dir=sdk_dir,
          project_dir=project_dir,
          testapp_builder_dir=builder_dir,
          provisions_dir=provisions_dir,
          api_config=api_config,
          ios_sdk=ios_sdk,
          dev_team=config_reader.read_general_config(config, "apple_team_id"))
    except subprocess.CalledProcessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_IOS, error_message=str(e)))
    logging.info("END %s, %s", testapp, _IOS)

  logging.info("END building for API: %s", testapp)
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


def _build_desktop(sdk_dir, compiler_flags):
  _run(["cmake", ".", "-DFIREBASE_CPP_SDK_DIR=" + sdk_dir] + compiler_flags)
  _run(["cmake", "--build", "."])


def _execute_desktop_testapp(project_dir):
  if platform.system() == "Windows":
    testapp_path = os.path.join(project_dir, "Debug", "integration_test.exe")
  else:
    testapp_path = os.path.join(project_dir, "integration_test")
  _run([testapp_path])


def _get_desktop_compiler_flags(compiler, config):
  """Returns the command line flags for this compiler."""
  if not compiler:  # None is an acceptable default value
    return []
  compilers = config_reader.read_general_config(config, "compiler_dict")
  try:
    return compilers[compiler]
  except KeyError:
    valid_keys = ", ".join(compilers.keys())
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
  _run(["./gradlew", "assembleDebug", "--stacktrace"])


def _validate_android_environment_variables():
  """Raises an error if any environment variables for Android are missing."""
  # These are environment variables that must be set for gradle.
  missing_env_vars = [
      var for var in _REQUIRED_ANDROID_ENV_VARS if not os.environ.get(var)
  ]
  if missing_env_vars:
    raise ValueError(
        "Missing required environment variable(s): "
        + ", ".join(missing_env_vars))


def _build_ios(
    sdk_dir, project_dir, testapp_builder_dir, provisions_dir, api_config,
    ios_sdk, dev_team):
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
  xcode_patcher_args = [
      "ruby", os.path.join(testapp_builder_dir, "xcode_tool.rb"),
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
            dev_team=dev_team,
            configuration="Debug"))

  if ios_sdk in [_IOS_SDK_DEVICE, _IOS_SDK_BOTH]:
    logging.info("Creating 'export.plist' export options")
    provision_id = provisioning.get_provision_id(
        os.path.join(provisions_dir, api_config.provision))
    export_src = os.path.join(testapp_builder_dir, "export.plist")
    export_dest = os.path.join(build_dir, "export.plist")
    shutil.copy(export_src, export_dest)
    provisioning.patch_provisioning_profile(
        export_dest, api_config.bundle_id, provision_id)

    archive_path = os.path.join(build_dir, "app.xcarchive")
    _run(
        xcodebuild.get_args_for_archive(
            path=xcode_path,
            scheme=api_config.ios_target,
            uuid=provision_id,
            output_dir=build_dir,
            archive_path=archive_path,
            ios_sdk=_IOS_SDK_DEVICE,
            dev_team=dev_team,
            configuration="Debug"))
    _run(
        xcodebuild.get_args_for_export(
            output_dir=build_dir,
            archive_path=archive_path,
            plist_path=export_dest))


# This script is responsible for copying shared files into the integration
# test projects. Should be executed before performing any builds.
def _run_setup_script(sdk_dir, testapp_dir):
  """Runs the setup_integration_tests.py script if needed."""
  script_path = os.path.join(sdk_dir, "setup_integration_tests.py")
  if os.path.isfile(script_path):
    _run(["python", script_path, testapp_dir])
  else:
    logging.info("setup_integration_tests.py not found")


def _run(args, timeout=1200):
  """Executes a command in a subprocess."""
  logging.info("Running in subprocess: %s", " ".join(args))
  return subprocess.run(args=args, timeout=timeout, check=True)


@attr.s(frozen=True, eq=False)
class Failure(object):
  """Holds context for the failure of a testapp to build/run."""
  testapp = attr.ib()
  platform = attr.ib()
  error_message = attr.ib()

  def describe(self):
    return "%s, %s: %s" % (self.testapp, self.platform, self.error_message)


@attr.s(frozen=True, eq=False)
class APIConfig(object):
  """Holds all the configuration for a single testapp project."""
  name = attr.ib()
  full_name = attr.ib()
  bundle_id = attr.ib()
  ios_target = attr.ib()
  api_dir = attr.ib()  # Integration test dir relative to sdk root
  frameworks = attr.ib()  # Required custom xcode frameworks
  provision = attr.ib()  # Path to the local mobile provision

  @classmethod
  def from_config(cls, config, testapp):
    """Builds the APIConfig for this testapp.

    Args:
      config: The full configuration dictionary for the testapp builder.
      testapp: Short name for the testapp, e.g. 'analytics'.

    Returns:
      API-specific configuration for this testapp.

    """

    if testapp not in config_reader.get_api_names(config):
      raise ValueError("Invalid api name (not found in config): " + testapp)

    return cls(
        name=testapp,
        full_name=config_reader.read_api_config(config, testapp, "full_name"),
        bundle_id=config_reader.read_api_config(config, testapp, "bundle_id"),
        ios_target=config_reader.read_api_config(config, testapp, "ios_target"),
        api_dir=config_reader.read_api_config(config, testapp, "testapp_path"),
        frameworks=config_reader.read_api_config(config, testapp, "frameworks"),
        provision=config_reader.read_api_config(config, testapp, "provision"))


if __name__ == "__main__":
  app.run(main)
