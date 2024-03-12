# Copyright 2020 Google LLC
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

"""Helper module for working with xcode projects.

The tool xcodebuild provides support to build xcode projects from the command
line. The motivation was to simplify usage of xcodebuild, since it was non-trivial 
to figure out which flags were needed to get it working in a CI environment.
The options required by the methods in this module were found to work both
locally and on CI, with both the Unity and C++ projects.

get_args_for_build() method doesn't performing operations with xcodebuild directly, 
this module returns arg sequences. These sequences can be passed to e.g. 
subprocess.run to execute the operations.

get_args_for_build() support either device or simulator builds. For simulator
builds, it suffices to use get_args_for_build() to create a .app that can be
used with simulators. For unsigned device builds, generate .app via 
get_args_for_build() step and then use generate_unsigned_ipa() to package 
the .app to .ipa. 

"""

import os
import shutil

def get_args_for_build(
    path, scheme, output_dir, apple_platfrom, apple_sdk, configuration):
  """Constructs subprocess args for an unsigned xcode build.

  Args:
    path (str): Full path to the project or workspace to build. Must end in
        either .xcodeproj or .xcworkspace.
    scheme (str): Name of the scheme to build.
    output_dir (str): Directory for the resulting build artifacts. Will be
        created if it doesn't already exist.
    apple_platfrom (str): iOS or tvOS.
    apple_sdk (str): Where this build will be run: real device or virtual device (simulator).
    configuration (str): Value for the -configuration flag.

  Returns:
    Sequence of strings, corresponding to valid args for a subprocess call.

  """
  args = [
      "xcodebuild",
      "-sdk", _get_apple_env_from_target(apple_platfrom, apple_sdk),
      "-scheme", scheme,
      "-configuration", configuration,
      "-quiet",
      "BUILD_DIR=" + output_dir
  ]

  if apple_sdk == "real":
    args.extend(['CODE_SIGN_IDENTITY=""',
      "CODE_SIGNING_REQUIRED=NO",
      "CODE_SIGNING_ALLOWED=NO"])
  elif apple_sdk == "virtual" and apple_platfrom == "tvOS":
    args.extend(['-arch', "x86_64"])

  if not path:
    raise ValueError("Must supply a path.")
  if path.endswith(".xcworkspace"):
    args.extend(("-workspace", path))
  elif path.endswith(".xcodeproj"):
    args.extend(("-project", path))
  else:
    raise ValueError("Path must end with .xcworkspace or .xcodeproj: %s" % path)
  return args


def _get_apple_env_from_target(apple_platfrom, apple_sdk):
  """Return a value for the -sdk flag based on the target (device/simulator)."""
  if apple_platfrom == "iOS":
    if apple_sdk == "real":
      return "iphoneos"
    elif apple_sdk == "virtual":
      return "iphonesimulator"
    else:
      raise ValueError("Unrecognized apple_sdk: %s" % apple_sdk)
  elif apple_platfrom == "tvOS":
    if apple_sdk == "real":
      return "appletvos"
    elif apple_sdk == "virtual":
      return "appletvsimulator"
    else:
      raise ValueError("Unrecognized apple_sdk: %s" % apple_sdk)
  else:
    raise ValueError("Unrecognized apple_sdk: %s" % apple_sdk)


def generate_unsigned_ipa(output_dir, configuration):
  """create unsigned .ipa from .app, then remove .app afterwards

  Args:
    output_dir (str): Same value as get_args_for_build. generated unsigned .ipa 
      will be placed within the subdirectory "Debug-iphoneos" or "Release-iphoneos".
    configuration (str): Same value as get_args_for_build.
  """
  iphone_build_dir = os.path.join(output_dir, configuration + "-iphoneos")
  payload_path = os.path.join(iphone_build_dir, "Payload")
  app_path = os.path.join(iphone_build_dir, "integration_test.app")
  ipa_path = os.path.join(iphone_build_dir, "integration_test.ipa")
  os.mkdir(payload_path)
  shutil.move(app_path, payload_path)
  shutil.make_archive(payload_path, 'zip', root_dir=iphone_build_dir, base_dir='Payload')
  shutil.move('%s.%s'%(payload_path, 'zip'), ipa_path)
  shutil.rmtree(payload_path)
