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

import attr
import datetime
import json
import os
import platform
import shutil
import stat
import subprocess
import sys
import tempfile

from absl import app
from absl import flags
from absl import logging
from distutils import dir_util

import utils
from integration_testing import config_reader
from integration_testing import test_validation
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
_TVOS = "tvOS"
_DESKTOP = "Desktop"
_SUPPORTED_PLATFORMS = (_ANDROID, _IOS, _TVOS, _DESKTOP)

# Architecture
_SUPPORTED_ARCHITECTURES = ("x64", "x86", "arm64")

# Values for iOS SDK flag (where the iOS app will run)
_APPLE_SDK_DEVICE = "real"
_APPLE_SDK_SIMULATOR = "virtual"
_SUPPORTED_APPLE_SDK = (_APPLE_SDK_DEVICE, _APPLE_SDK_SIMULATOR)

_DEFAULT_RUN_TIMEOUT_SECONDS = 4800  # 1 hour 20 min

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "packaged_sdk", None, "(Optional) Firebase SDK directory. If not"
    " supplied, will build from source.")

flags.DEFINE_string(
    "output_directory", "~",
    "Build output will be placed in this directory.")

flags.DEFINE_string(
    "artifact_name", "local-build",
    "artifacts will be created and placed in output_directory."
    " testapps artifact is testapps-$artifact_name;"
    " build log artifact is build-results-$artifact_name.log.")    

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

flags.DEFINE_list(
    "ios_sdk", _APPLE_SDK_DEVICE, 
    "(iOS only) Build for real device (.ipa), virtual device / simulator (.app), "
    "or both. Building for both will produce both an .app and an .ipa.")

flags.DEFINE_list(
    "tvos_sdk", _APPLE_SDK_SIMULATOR, 
    "(tvOS only) Build for real device (.ipa), virtual device / simulator (.app), "
    "or both. Building for both will produce both an .app and an .ipa.")

flags.DEFINE_bool(
    "update_pod_repo", True,
    "(iOS/tvOS only) Will run 'pod repo update' before building for iOS/tvOS to update"
    " the local spec repos available on this machine. Must also include iOS/tvOS"
    " in platforms flag.")

flags.DEFINE_string(
    "compiler", None,
    "(Desktop only) Specify the compiler with CMake during the testapps build."
    " Check the config file to see valid choices for this flag."
    " If none, will invoke cmake without specifying a compiler.")

flags.DEFINE_string(
    "arch", "x64",
    "(Desktop only) Which architecture to build: x64 (all), x86 (Windows/Linux), "
    "or arm64 (Mac only).")

# Get the number of CPUs for the default value of FLAGS.jobs
CPU_COUNT = os.cpu_count();
# If CPU count couldn't be determined, default to 2.
DEFAULT_CPU_COUNT = 2
if CPU_COUNT is None: CPU_COUNT = DEFAULT_CPU_COUNT
# Cap at 4 CPUs.
MAX_CPU_COUNT = 4
if CPU_COUNT > MAX_CPU_COUNT: CPU_COUNT = MAX_CPU_COUNT

flags.DEFINE_integer(
    "jobs", CPU_COUNT,
    "(Desktop only) If > 0, pass in -j <number> to make CMake parallelize the"
    " build. Defaults to the system's CPU count (max %s)." % MAX_CPU_COUNT)

flags.DEFINE_multi_string(
    "cmake_flag", None,
    "Pass an additional flag to the CMake configure step."
    " This option can be specified multiple times.")

flags.register_validator(
    "platforms",
    lambda p: all(platform in _SUPPORTED_PLATFORMS for platform in p),
    message="Valid platforms: " + ",".join(_SUPPORTED_PLATFORMS),
    flag_values=FLAGS)

flags.register_validator(
    "ios_sdk",
    lambda s: all(ios_sdk in _SUPPORTED_APPLE_SDK for ios_sdk in s),
    message="Valid platforms: " + ",".join(_SUPPORTED_APPLE_SDK),
    flag_values=FLAGS)

flags.register_validator(
    "tvos_sdk",
    lambda s: all(tvos_sdk in _SUPPORTED_APPLE_SDK for tvos_sdk in s),
    message="Valid platforms: " + ",".join(_SUPPORTED_APPLE_SDK),
    flag_values=FLAGS)

flags.DEFINE_bool(
    "short_output_paths", False,
    "Use short directory names for output paths. Useful to avoid hitting file "
    "path limits on Windows.")

flags.DEFINE_bool(
    "gha_build", False,
    "Set to true if this is a GitHub Actions build.")

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  platforms = FLAGS.platforms
  testapps = FLAGS.testapps

  sdk_dir = _fix_path(FLAGS.packaged_sdk or FLAGS.repo_dir)
  root_output_dir = _fix_path(FLAGS.output_directory)
  repo_dir = _fix_path(FLAGS.repo_dir)

  update_pod_repo = FLAGS.update_pod_repo
  if FLAGS.add_timestamp:
    timestamp = datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
  else:
    timestamp = ""

  if FLAGS.short_output_paths:
    output_dir = os.path.join(root_output_dir, "ta")
  else:
    output_dir = os.path.join(root_output_dir, "testapps" + timestamp)

  config = config_reader.read_config()

  xcframework_dir = os.path.join(sdk_dir, "xcframeworks")
  xcframework_exist = os.path.isdir(xcframework_dir)
  if not xcframework_exist:
    if _IOS in platforms:
      _build_xcframework_from_repo(repo_dir, "ios", testapps, config)
    if _TVOS in platforms:
      _build_xcframework_from_repo(repo_dir, "tvos", testapps, config)

  if update_pod_repo and (_IOS in platforms or _TVOS in platforms):
    _run(["pod", "repo", "update"])

  cmake_flags = _get_desktop_compiler_flags(FLAGS.compiler, config.compilers)
  if _DESKTOP in platforms and not FLAGS.packaged_sdk:
    cmake_flags.extend((
        "-DFIREBASE_PYTHON_HOST_EXECUTABLE:FILEPATH=%s" % sys.executable,
    ))

  if (_DESKTOP in platforms and utils.is_linux_os() and FLAGS.arch == "x86"):
      # Write out a temporary toolchain file to force 32-bit Linux builds, as
      # the SDK-included toolchain file may not be present when building against
      # the packaged SDK.
      temp_toolchain_file = tempfile.NamedTemporaryFile("w+", suffix=".cmake")
      temp_toolchain_file.writelines([
        'set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")\n',
        'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")\n',
        'set(CMAKE_LIBRARY_PATH "/usr/lib/i386-linux-gnu")\n',
        'set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} "/usr/include/i386-linux-gnu")\n',
        'set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")'])
      temp_toolchain_file.flush()
      # Leave the file open, as it will be deleted on close, i.e. when this script exits.
      # (On Linux, the file can be opened a second time by cmake while still open by
      # this script)
      cmake_flags.extend(["-DCMAKE_TOOLCHAIN_FILE=%s" % temp_toolchain_file.name])

  if FLAGS.cmake_flag:
    cmake_flags.extend(FLAGS.cmake_flag)

  failures = []
  for testapp in testapps:
    api_config = config.get_api(testapp)
    if FLAGS.repo_dir and not FLAGS.packaged_sdk and api_config.internal_testapp_path:
      testapp_dirs = [api_config.internal_testapp_path]
    else:
      testapp_dirs = [api_config.testapp_path]
    for testapp_dir in testapp_dirs:
      logging.info("BEGIN building for %s: %s", testapp, testapp_dir)
      failures += _build(
          testapp=testapp,
          platforms=platforms,
          api_config=config.get_api(testapp),
          testapp_dir=testapp_dir,
          output_dir=output_dir,
          sdk_dir=sdk_dir,
          xcframework_exist=xcframework_exist,
          repo_dir=repo_dir,
          ios_sdk=FLAGS.ios_sdk,
          tvos_sdk=FLAGS.tvos_sdk,
          cmake_flags=cmake_flags,
          short_output_paths=FLAGS.short_output_paths)
      logging.info("END building for %s", testapp)
  
  _collect_integration_tests(testapps, root_output_dir, output_dir, FLAGS.artifact_name)

  _summarize_results(testapps, platforms, failures, root_output_dir, FLAGS.artifact_name)
  return 1 if failures else 0


def _build(
    testapp, platforms, api_config, testapp_dir, output_dir, sdk_dir, xcframework_exist,
    repo_dir, ios_sdk, tvos_sdk, cmake_flags, short_output_paths):
  """Builds one testapp on each of the specified platforms."""
  os.chdir(repo_dir)
  project_dir = os.path.join(output_dir, api_config.name)
  if short_output_paths:
    # Combining the first letter of every part separated by underscore for
    # testapp paths. This is a trick to reduce file path length as we were
    # exceeding the limit on Windows.
    testapp_dir_parts = os.path.basename(testapp_dir).split('_')
    output_testapp_dir = ''.join([x[0] for x in testapp_dir_parts])
  else:
    output_testapp_dir = os.path.basename(testapp_dir)

  project_dir = os.path.join(project_dir, output_testapp_dir)

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
      _build_apple(
          sdk_dir=sdk_dir,
          xcframework_exist=xcframework_exist,
          project_dir=project_dir,
          repo_dir=repo_dir,
          api_config=api_config,
          target=api_config.ios_target,
          scheme=api_config.ios_scheme,
          apple_platfrom=_IOS,
          apple_sdk=ios_sdk)

    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_IOS, error_message=str(e)))
    logging.info("END %s, %s", testapp, _IOS)

  if _TVOS in platforms and api_config.tvos_target:
    logging.info("BEGIN %s, %s", testapp, _TVOS)
    try:
      _build_apple(
          sdk_dir=sdk_dir,
          xcframework_exist=xcframework_exist,
          project_dir=project_dir,
          repo_dir=repo_dir,
          api_config=api_config,
          target=api_config.tvos_target,
          scheme=api_config.tvos_scheme,
          apple_platfrom=_TVOS,
          apple_sdk=tvos_sdk)
    except subprocess.SubprocessError as e:
      failures.append(
          Failure(testapp=testapp, platform=_TVOS, error_message=str(e)))
    logging.info("END %s, %s", testapp, _TVOS)

  return failures


def _collect_integration_tests(testapps, root_output_dir, output_dir, artifact_name):
  testapps_artifact_dir = "testapps-" + artifact_name
  android_testapp_extension = ".apk"
  ios_testapp_extension = ".ipa"
  ios_simualtor_testapp_extension = ".app"
  desktop_testapp_name = "integration_test" 
  if platform.system() == "Windows":
    desktop_testapp_name += ".exe"

  testapp_paths = []
  testapp_google_services = {}
  for file_dir, directories, file_names in os.walk(output_dir):
    for directory in directories:
      if directory.endswith(ios_simualtor_testapp_extension):
        testapp_paths.append(os.path.join(file_dir, directory))
    for file_name in file_names:
      if ((file_name == desktop_testapp_name and "ios_build" not in file_dir) 
          or file_name.endswith(android_testapp_extension) 
          or file_name.endswith(ios_testapp_extension)):
        testapp_paths.append(os.path.join(file_dir, file_name))
      if (file_name == "google-services.json"):
        testapp_google_services[file_dir.split(os.path.sep)[-2]] = os.path.join(file_dir, file_name)

  artifact_path = os.path.join(root_output_dir, testapps_artifact_dir)
  _rm_dir_safe(artifact_path)
  for testapp in testapps:
    os.makedirs(os.path.join(artifact_path, testapp))
  for path in testapp_paths:
    for testapp in testapps:
      if testapp in path:
        if os.path.isfile(path):
          shutil.copy(path, os.path.join(artifact_path, testapp))
          if path.endswith(desktop_testapp_name) and testapp_google_services.get(testapp):
            shutil.copy(testapp_google_services[testapp], os.path.join(artifact_path, testapp))
        else:
          dir_util.copy_tree(path, os.path.join(artifact_path, testapp, os.path.basename(path)))
        break


def _summarize_results(testapps, platforms, failures, root_output_dir, artifact_name):
  """Logs a readable summary of the results of the build."""
  file_name = "build-results-" + artifact_name + ".log"

  summary = []
  summary.append("BUILD SUMMARY:")
  summary.append("TRIED TO BUILD: " + ",".join(testapps))
  summary.append("ON PLATFORMS: " + ",".join(platforms))

  if not failures:
    summary.append("ALL BUILDS SUCCEEDED")
  else:
    summary.append("SOME ERRORS OCCURRED:")
    for i, failure in enumerate(failures, start=1):
      summary.append("%d: %s" % (i, failure.describe()))
  summary = "\n".join(summary)

  logging.info(summary)
  test_validation.write_summary(root_output_dir, summary, file_name=file_name)

  summary_json = {}
  summary_json["type"] = "build"
  summary_json["testapps"] = testapps
  summary_json["errors"] = {failure.testapp:failure.error_message for failure in failures}
  with open(os.path.join(root_output_dir, file_name+".json"), "a") as f:
    f.write(json.dumps(summary_json, indent=2))


def _build_desktop(sdk_dir, cmake_flags):
  cmake_configure_cmd = ["cmake", ".", "-DCMAKE_BUILD_TYPE=Debug",
                                       "-DFIREBASE_CPP_SDK_DIR=" + sdk_dir]
  if utils.is_windows_os():
    cmake_configure_cmd += ["-A",
                            "Win32" if FLAGS.arch == "x86" else FLAGS.arch]
  elif utils.is_mac_os():
    # Ensure that correct Mac architecture is built.
    cmake_configure_cmd += ["-DCMAKE_OSX_ARCHITECTURES=%s" %
                            ("arm64" if FLAGS.arch == "arm64" else "x86_64")]

  _run(cmake_configure_cmd + cmake_flags)
  _run(["cmake", "--build", ".", "--config", "Debug"] +
       ["-j", str(FLAGS.jobs)] if FLAGS.jobs > 0 else [])


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
    f.write("http.keepAlive=false\n")
    f.write("maven.wagon.http.pool=false\n")
    f.write("maven.wagon.httpconnectionManager.ttlSeconds=120")
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

# build required ios xcframeworks based on makefiles
# the xcframeworks locates at repo_dir/ios_build
def _build_xcframework_from_repo(repo_dir, apple_platform, testapps, config):
  """Builds xcframework from SDK source."""
  output_path = os.path.join(repo_dir, apple_platform + "_build")
  _rm_dir_safe(output_path)
  xcframework_builder = os.path.join(
    repo_dir, "scripts", "gha", "build_ios_tvos.py")

  # build only required targets to save time
  target = set()
  for testapp in testapps:
    api_config = config.get_api(testapp)
    if apple_platform == "ios" or (apple_platform == "tvos" and api_config.tvos_target):
      for framework in api_config.frameworks:
        # firebase_analytics.framework -> firebase_analytics
        target.add(os.path.splitext(framework)[0])

  # firebase is not a target in CMake, firebase_app is the target
  # firebase_app will be built by other target as well
  target.remove("firebase")

  framework_builder_args = [
      sys.executable, xcframework_builder,
      "-b", output_path,
      "-s", repo_dir,
      "-o", apple_platform,
      "-t"
  ]
  framework_builder_args.extend(target)
  _run(framework_builder_args)


def _build_apple(
    sdk_dir, xcframework_exist, project_dir, repo_dir, api_config, 
    target, scheme, apple_platfrom, apple_sdk):
  """Builds an iOS application (.app, .ipa or both)."""
  build_dir = apple_platfrom.lower() + "_build"
  if not xcframework_exist:
    sdk_dir = os.path.join(repo_dir, build_dir)

  build_dir = os.path.join(project_dir, build_dir)
  os.makedirs(build_dir)

  logging.info("Copying XCFrameworks")
  framework_src_dir = os.path.join(sdk_dir, "xcframeworks")
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
      repo_dir, "scripts", "gha", "integration_testing", "xcode_tool.rb")
  xcode_patcher_args = [
      "ruby", xcode_tool_path,
      "--XCodeCPP.xcodeProjectDir", project_dir,
      "--XCodeCPP.target", target,
      "--XCodeCPP.frameworks", ",".join(framework_paths)
  ]
  # Internal integration tests require the SDK root as an include path.
  if repo_dir and api_config.internal_testapp_path:
    xcode_patcher_args.extend(("--XCodeCPP.include", repo_dir))
  if os.path.isfile(entitlements_path):  # Not all testapps require entitlements
    logging.info("Entitlements file detected.")
    xcode_patcher_args.extend(("--XCodeCPP.entitlement", entitlements_path))
  else:
    logging.info("No entitlements found at %s.", entitlements_path)
  _run(xcode_patcher_args)

  xcode_path = os.path.join(project_dir, "integration_test.xcworkspace")
  if _APPLE_SDK_SIMULATOR in apple_sdk:
    _run(
        xcodebuild.get_args_for_build(
            path=xcode_path,
            scheme=scheme,
            output_dir=build_dir,
            apple_platfrom=apple_platfrom,
            apple_sdk=_APPLE_SDK_SIMULATOR,
            configuration="Debug"))

  if _APPLE_SDK_DEVICE in apple_sdk:
    _run(
        xcodebuild.get_args_for_build(
            path=xcode_path,
            scheme=scheme,
            output_dir=build_dir,
            apple_platfrom=apple_platfrom,
            apple_sdk=_APPLE_SDK_DEVICE,
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


def _run(args, timeout=_DEFAULT_RUN_TIMEOUT_SECONDS, capture_output=False, text=None, check=True):
  """Executes a command in a subprocess."""
  logging.info("Running in subprocess: %s", " ".join(args))
  return subprocess.run(
      args=args,
      timeout=timeout,
      capture_output=capture_output,
      text=text,
      check=check)


def _handle_readonly_file(func, path, excinfo):
  """Function passed into shutil.rmtree to handle Access Denied error"""
  os.chmod(path, stat.S_IWRITE)
  func(path)  # will re-throw if a different error occurrs


def _rm_dir_safe(directory_path):
  """Removes directory at given path. No error if dir doesn't exist."""
  logging.info("Deleting %s...", directory_path)
  try:
    shutil.rmtree(directory_path, onerror=_handle_readonly_file)
  except OSError as e:
    # There are two known cases where this can happen:
    # The directory doesn't exist (FileNotFoundError)
    # A file in the directory is open in another process (PermissionError)
    logging.warning("Failed to remove directory:\n%s", e.strerror)


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
