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
line. This module provides templates for building, archiving and exporting. The
motivation was to simplify usage of xcodebuild, since it was non-trivial to
figure out which flags were needed to get it working in a CI environment.
The options required by the methods in this module were found to work both
locally and on CI, with both the Unity and C++ projects.

Note that instead of performing operations with xcodebuild directly, this module
returns arg sequences. These sequences can be passed to e.g. subprocess.run to
execute the operations.

The methods support either device or simulator builds. For simulator
builds, it suffices to use gets_args_for_build to create a .app that can be
used with simulators. For device builds, it's necessary to use both a
get_args_for_archive step and a get_args_for_export step. The archive step
generates an .xcarchive, and the export step 'exports' this archive to create
an .ipa that can be installed onto devices.

"""


def get_args_for_archive(
    path, scheme, uuid, output_dir, archive_path, ios_sdk, dev_team,
    configuration):
  """Subprocess args for an iOS archive. Necessary step for creating an .ipa.

  Args:
    path (str): Full path to the project or workspace to build. Must end in
        either .xcodeproj or .xcworkspace.
    scheme (str): Name of the scheme to build.
    uuid (str): The mobileprovision's uuid.
    output_dir (str): Directory for the resulting build artifacts. Will be
        created if it doesn't already exist.
    archive_path (str): Archive will be created at this path.
    ios_sdk (str): Where this build will be run: device or simulator.
    dev_team (str): Apple development team id.
    configuration (str): Value for the -configuration flag. If building for
        Unity, Unity will by default generate Debug, Release, ReleaseForRunning,
        and ReleaseForProfiling configurations.

  Returns:
    Sequence of strings, corresponding to valid args for a subprocess call.
  """
  xcode_args = get_args_for_build(
      path, scheme, output_dir, ios_sdk, dev_team, configuration)
  additional_device_args = [
      "PROVISIONING_PROFILE=" + uuid,
      "CODE_SIGN_STYLE=Manual",
      "-archivePath", archive_path,
      "archive"
  ]
  return xcode_args + additional_device_args


def get_args_for_build(
    path, scheme, output_dir, ios_sdk, dev_team, configuration):
  """Constructs subprocess args for an xcode build.

  Args:
    path (str): Full path to the project or workspace to build. Must end in
        either .xcodeproj or .xcworkspace.
    scheme (str): Name of the scheme to build.
    output_dir (str): Directory for the resulting build artifacts. Will be
        created if it doesn't already exist.
    ios_sdk (str): Where this build will be run: device or simulator.
    dev_team (str): Apple development team id.
    configuration (str): Value for the -configuration flag.

  Returns:
    Sequence of strings, corresponding to valid args for a subprocess call.

  """
  args = [
      "xcodebuild",
      "-sdk", _get_ios_env_from_target(ios_sdk),
      "-scheme", scheme,
      "-configuration", configuration,
      "-quiet",
      "DEVELOPMENT_TEAM=" + dev_team,
      "BUILD_DIR=" + output_dir
  ]

  if not path:
    raise ValueError("Must supply a path.")
  if path.endswith(".xcworkspace"):
    args.extend(("-workspace", path))
  elif path.endswith(".xcodeproj"):
    args.extend(("-project", path))
  else:
    raise ValueError("Path must end with .xcworkspace or .xcodeproj: %s" % path)
  return args


def get_args_for_export(output_dir, archive_path, plist_path):
  """Subprocess args to export an xcode archive, creating an ipa.

  Must have already performed an xcodebuild archive step before executing
  the command given by the returned args.

  Args:
    output_dir (str): Directory to contain the exported artifact.
    archive_path (str): Path to the xcode archive (.xcarchive) to be exported.
    plist_path (str): Path for the export plist. The export plist
        contains configuration for the export process, and must be present
        even if the file is vacuous (contains no configuration).

  Returns:
    Sequence of strings, corresponding to valid args for a subprocess call.

  """
  return [
      "xcodebuild",
      "-archivePath", archive_path,
      "-exportOptionsPlist", plist_path,
      "-exportPath", output_dir,
      "-exportArchive",
      "-quiet"
  ]


def _get_ios_env_from_target(ios_sdk):
  """Return a value for the -sdk flag based on the target (device/simulator)."""
  if ios_sdk == "device":
    return "iphoneos"
  elif ios_sdk == "simulator":
    return "iphonesimulator"
  else:
    raise ValueError("Unrecognized ios_sdk: %s" % ios_sdk)
