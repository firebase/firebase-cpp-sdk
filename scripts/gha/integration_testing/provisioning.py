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

"""Helper module for dealing with mobile provisioning profiles.

This module provides functionality to extract a UUID from a mobile provisioning
file and patch it into an exports plist. The motivation for this arose when it
was discovered that the UUID changes each time the profile is updated. Since the
profile was pulled from a source where it was updated daily, manually specifying
the UUID in the exports plist was not feasible.

The UUID is needed to tell xcodebuild which provisioning profile to use.

This should only be run on MacOS, as it uses a mac-specific
application.

"""

import plistlib
import subprocess


def patch_provisioning_profile(plist_path, bundle_id, uuid):
  """Patches the plist with a provisioning profile.

  Will add an entry to the provisioning profiles dictionary in the provided
  plist. The entry will be bundle_id:uuid, associating a project with a
  provisioning profile. This is needed for exporting an xcode archive using
  xcodebuild.

  If an entry with the same bundle id already exists, its value will be
  overwritten.

  Args:
    plist_path (str): Path to a valid plist with a provisioningProfiles
        dictionary in its root.
    bundle_id (str): Bundle id identifying this project.
    uuid (str): UUID from a mobile provisioning profile.

  Raises:
    ValueError: plist not in correct format.

  """
  # Need to explicitly specify binary format for load to work.
  with open(plist_path, "rb") as f_read:
    plist = plistlib.load(f_read)
  _patch_provisioning_profile(plist, bundle_id, uuid)
  with open(plist_path, "wb") as f_write:
    plistlib.dump(plist, f_write)


def _patch_provisioning_profile(plist, bundle_id, uuid):
  """Patches the plist dictionary with bundle_id:uuid."""
  try:
    profiles = plist["provisioningProfiles"]
    profiles[bundle_id] = uuid
  except (TypeError, KeyError):
    raise ValueError(
        "Plist not in correct format. 'provisioningProfiles' misplaced?")


def get_provision_id(provision_path):
  """Extracts the provisioning UUID in the provided provisioning profile.

  Can only be run on macOS.

  Args:
    provision_path (str): Path to a .mobileprovision file.

  Returns:
    str: UUID.

  Raises:
    ValueError: If provision_path is empty or None, or isn't a mobileprovision.
    RuntimeError: Called this script on anything but a Mac.

  """
  if not provision_path:
    raise ValueError("Must provide valid directory for provisions")

  if not provision_path.endswith(".mobileprovision"):
    raise ValueError("Path is not a .mobileprovision file: %s" % provision_path)

  plist = _extract_plist(provision_path)
  uuid = _extract_uuid(plist)
  if not uuid:
    raise RuntimeError("No UUID found in provision %s." % provision_path)
  return uuid


def _extract_plist(provision_path):
  """Extract a plist as a string from the specified mobile provision.

  Mobile provisions are encoded files with an embedded plist. This method
  will attempt to decode the file and return the plist as a string.

  Args:
    provision_path (str): Path to a mobileprovision profile.

  Returns:
    bytes: Contents of the plist file.

  Raises:
    subprocess.CalledProcessError: Failed to decode the given file.

  """
  # -D specifies decode, -i <path> specifies input.
  return subprocess.check_output(
      ["security", "cms", "-D", "-i", provision_path])


def _extract_uuid(plist):
  """Extract the UUID from the plist.

  Will extract the value of the UUID key from the mobile provisioning plist.

  Args:
    plist (bytes): Plist bytes read from a mobileprovisioning file.

  Returns:
    str: UUID.

  """
  # In Python 3, readPlistFromString is removed in favor of loads, which is not
  # present in Python 2.
  try:
    root = plistlib.readPlistFromString(plist)  # First try Python 2 method.
  except AttributeError:
    root = plistlib.loads(plist)  # If nonexistent, try Python 3 method.
  return root.get("UUID", None)
