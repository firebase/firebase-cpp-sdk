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

r"""Tool for sending mobile testapps to Firebase Test Lab for testing.

Requires Cloud SDK installed with gsutil. Can be checked as follows:
  gcloud --version

This tool will use the games-auto-release-testing GCS storage bucket. 
To be authorized, it's necessary have the key file (JSON) and
supply it with the --key_file flag.
1) key file Valentine ID: 1561166657633900
2) Alternatively, decrypt the key file with 
  python scripts/gha/restore_secrets.py --passphrase SECRET
  SECRET Valentine ID: 1592951125596776


Usage:

  python test_lab.py --testapp_dir ~/testapps --code_platform unity \
    --key_file scripts/gha-encrypted/gcs_key_file.json

This will recursively search ~/testapps for apks and ipas,
send them to FTL, and validate their results. The validation is specific to
the structure of the Firebase Unity and C++ testapps.


If you wish to specify a particular device to test on, you will need the model
id and version (api level for Android, OS version for iOS). These change over
time. You can find the currently supported models and versions with the
following commands:

  gcloud firebase test android models list
  gcloud firebase test ios models list

Note: you need the value in the MODEL_ID column, not MODEL_NAME. 
Examples:
Pixel2, API level 28:
  --android_model Pixel2 --android_version 28

iphone6s, OS 12.0:
  --ios_model iphone6s --ios_version 12.0

Alternatively, to set a device, use the one of the values below:
[android_min, android_target, android_latest]
[ios_min, ios_target, ios_latest]
These Device Information stored in TEST_DEVICES in print_matrix_configuration.py 
Examples:
Pixel2, API level 28:
  --android_device android_target

iphone6s, OS 12.0:
  --ios_device ios_target

"""

import os
import subprocess
import threading
import re

from absl import app
from absl import flags
from absl import logging
import attr

from integration_testing import gcs
from integration_testing import test_validation
from print_matrix_configuration import TEST_DEVICES

_ANDROID = "android"
_IOS = "ios"

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "Testapps (apks and ipas) in this directory will be tested.")
flags.DEFINE_enum(
    "code_platform", None, [test_validation.UNITY, test_validation.CPP],
    "Whether the these testapps were built using C++ (cpp) or Unity (unity)."
    " Used to choose the validation logic to process the log output.")
flags.DEFINE_string(
    "key_file", None, "Path to key file authorizing use of the GCS bucket.")
flags.DEFINE_string(
    "android_device", None,
    "Model_id and API_level for desired device. See module docstring for details "
    "on how to set the value. If none, will use android_model and android_version.")
flags.DEFINE_string(
    "android_model", None,
    "Model id for desired device. See module docstring for details on how"
    " to get this id. If none, will use FTL's default.")
flags.DEFINE_string(
    "android_version", None,
    "API level for desired device. See module docstring for details on how"
    " to find available values. If none, will use FTL's default.")
flags.DEFINE_string(
    "ios_device", None,
    "Model_id and IOS_version for desired device. See module docstring for details "
    "on how to set the value. If none, will use ios_model and ios_version.")
flags.DEFINE_string(
    "ios_model", None,
    "Model id for desired device. See module docstring for details on how"
    " to get this id. If none, will use FTL's default.")
flags.DEFINE_string(
    "ios_version", None,
    "iOS version for desired device. See module docstring for details on how"
    " to find available values. If none, will use FTL's default.")
flags.DEFINE_string(
    "logfile_name", "ftl-test",
    "Create test log artifact test-results-$logfile_name.log."
    " logfile will be created and placed in testapp_dir.")   

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  testapp_dir = _fix_path(FLAGS.testapp_dir)
  key_file_path = _fix_path(FLAGS.key_file)
  code_platform = FLAGS.code_platform

  if not os.path.exists(key_file_path):
    raise ValueError("Key file path does not exist: %s" % key_file_path)

  if FLAGS.android_device:
    android_device_info = TEST_DEVICES.get(FLAGS.android_device)
    if android_device_info:
      android_device = Device(model=android_device_info.get("model"), version=android_device_info.get("version"))
    else:
      raise ValueError("Not a valid android device: %s" % FLAGS.android_device)
  else:
    android_device = Device(model=FLAGS.android_model, version=FLAGS.android_version)
  
  if FLAGS.ios_device:
    ios_device_info = TEST_DEVICES.get(FLAGS.ios_device)
    if ios_device_info:
      ios_device = Device(model=ios_device_info.get("model"), version=ios_device_info.get("version"))
    else:
      raise ValueError("Not a valid android device: %s" % FLAGS.ios_device)
  else:
    ios_device = Device(model=FLAGS.ios_model, version=FLAGS.ios_version)

  has_ios = False
  testapps = []
  for file_dir, _, file_names in os.walk(testapp_dir):
    for file_name in file_names:
      full_path = os.path.join(file_dir, file_name)
      if file_name.endswith(".apk"):
        testapps.append((android_device, _ANDROID, full_path))
      elif file_name.endswith(".ipa"):
        has_ios = True
        testapps.append((ios_device, _IOS, full_path))

  if not testapps:
    logging.error("No testapps found.")
    return 1

  logging.info("Testapps found: %s", "\n".join(path for _, _, path in testapps))

  gcs.authorize_gcs(key_file_path)

  gcs_base_dir = gcs.get_unique_gcs_id()
  logging.info("Store results in %s", gcs.relative_path_to_gs_uri(gcs_base_dir))

  if has_ios:
    _install_gcloud_beta()

  tests = []
  for device, platform, path in testapps:
    # e.g. /testapps/unity/firebase_auth/app.apk -> unity_firebase_auth_app_apk
    rel_path = os.path.relpath(path, testapp_dir)
    name = rel_path.replace("\\", "_").replace("/", "_").replace(".", "_")
    tests.append(
        Test(
            device=device,
            platform=platform,
            testapp_path=path,
            results_dir=gcs_base_dir + "/" + name))

  logging.info("Sending testapps to FTL")
  threads = []
  for test in tests:
    thread = threading.Thread(target=test.run)
    threads.append(thread)
    thread.start()
  for thread in threads:
    thread.join()

  # Useful diagnostic information to debug unexpected errors involving things
  # not being where they're supposed to be. This will show everything generated
  # in the GCS bucket for this run. Includes video links, binaries, and results.
  logging.info(
      "Results directory:\n%s", "\n".join(_gcs_list_dir(gcs_base_dir))
  )

  return test_validation.summarize_test_results(
      tests, 
      code_platform, 
      testapp_dir, 
      file_name="test-results-" + FLAGS.logfile_name + ".log",
      extra_info=" (ON REAL DEVICE VIA FTL)")


def _install_gcloud_beta():
  """Install Google Cloud beta components for iOS integration tests."""
  subprocess.run(
      args=[gcs.GCLOUD, "--quiet", "components", "install", "beta"],
      check=True)


def _gcs_list_dir(gcs_path):
  """Recursively returns a list of contents for a directory on GCS."""
  gcs_path = gcs.relative_path_to_gs_uri(gcs_path)
  args = [gcs.GSUTIL, "ls", "-r", gcs_path]
  logging.info("Listing GCS contents: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=True)
  return result.stdout.splitlines()


def _gcs_read_file(gcs_path):
  """Extracts the contents of a file on GCS."""
  gcs_path = gcs.relative_path_to_gs_uri(gcs_path)
  args = [gcs.GSUTIL, "cat", gcs_path]
  logging.info("Reading GCS file: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=True)
  return result.stdout


def _fix_path(path):
  """Expands ~, normalizes slashes, and converts relative paths to absolute."""
  return os.path.abspath(os.path.expanduser(path))


@attr.s(frozen=False, eq=False)
class Test(object):
  """Holds data related to the testing of one testapp."""
  device = attr.ib()
  platform = attr.ib()  # Android or iOS
  testapp_path = attr.ib()
  results_dir = attr.ib()  # Subdirectory on Cloud storage for this testapp
  # This will be populated after the test completes, instead of initialization.
  logs = attr.ib(init=False, default=None)
  ftl_link = attr.ib(init=False, default=None)
  raw_result_link = attr.ib(init=False, default=None)

  # This runs in a separate thread, so instead of returning values we store
  # them as fields so they can be accessed from the main thread.
  def run(self):
    """Send the testapp to FTL for testing and wait for it to finish."""
    # These execute in parallel, so we collect the output then log it at once.
    args = self._gcloud_command
    logging.info("Testapp sent: %s", " ".join(args))
    result = subprocess.run(
        args=args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False)
    logging.info("Finished: %s\n%s", " ".join(args), result.stdout)
    if result.returncode:
      logging.error("gCloud returned non-zero error code")
    ftl_link = re.search(r'Test results will be streamed to \[(.*?)\]', result.stdout, re.DOTALL)
    if ftl_link:
      self.ftl_link = ftl_link.group(1)
    raw_result_link = re.search(r'Raw results will be stored in your GCS bucket at \[(.*?)\]', result.stdout, re.DOTALL)
    if raw_result_link:
      self.raw_result_link = raw_result_link.group(1)

    self.logs = self._get_testapp_log_text_from_gcs()
    logging.info("Test result: %s", self.logs)

  @property
  def _gcloud_command(self):
    """Returns the args to send this testapp to FTL on the command line."""
    if self.platform == _ANDROID:
      cmd = [gcs.GCLOUD, "firebase", "test", "android", "run"]
    elif self.platform == _IOS:
      cmd = [gcs.GCLOUD, "beta", "firebase", "test", "ios", "run"]
    else:
      raise ValueError("Invalid platform, must be 'Android' or 'iOS'")
    return cmd + self.device.get_gcloud_flags() + [
        "--type", "game-loop",
        "--app", self.testapp_path,
        "--results-bucket", gcs.PROJECT_ID,
        "--results-dir", self.results_dir,
        "--timeout", "900s"
    ]

  def _get_testapp_log_text_from_gcs(self):
    """Gets the testapp log text generated by game loops."""
    try:
      gcs_contents = _gcs_list_dir(self.results_dir)
    except subprocess.CalledProcessError as e:
      logging.error("Unexpected error searching GCS logs:\n%s", e)
      return None
    # The path to the testapp log depends on the platform, device, and scenario
    # being tested. Search for a json file with 'results' in the name to avoid
    # hard-coding too many assumptions about the path. The testapp log should be
    # the only json, but 'results' adds some redundancy in case this changes.
    matching_gcs_logs = [
        line for line in gcs_contents
        if line.endswith(".json") and "results" in line.lower()
    ]
    if not matching_gcs_logs:
      logging.error("Failed to find results log on GCS.")
      return None
    # This assumes testapps only have one scenario. Could change in the future.
    if len(matching_gcs_logs) > 1:
      logging.warning("Multiple scenario logs found.")
    gcs_log = matching_gcs_logs[0]
    try:
      logging.info("Found results log: %s", gcs_log)
      log_text = _gcs_read_file(gcs_log)
      if not log_text:
        logging.warning("Testapp log is empty. App may have crashed.")
      return log_text
    except subprocess.CalledProcessError as e:
      logging.error("Unexpected error reading GCS log:\n%s", e)
      return None


# All device dimensions are optional: FTL will use default options when
# a dimension isn't specified.
@attr.s(frozen=True, eq=True)
class Device(object):
  """Specifies a device on Firebase Test Lab. All fields are optional."""
  model = attr.ib(default=None)
  version = attr.ib(default=None)

  def get_gcloud_flags(self):
    """Returns flags for gCloud command to use this device on FTL."""
    # e.g. ["--device", "model=shamu,version=23"]
    # FTL supports four device 'dimensions'. model, orientation, lang, and
    # orientation. We leave orientation and lang as the defaults.
    if not self.model and not self.version:
      return []
    gcloud_flags = ["--device"]
    dimensions = []
    if self.model:
      dimensions.append("model=" + self.model)
    if self.version:
      dimensions.append("version=" + self.version)
    gcloud_flags.append(",".join(dimensions))
    return gcloud_flags


if __name__ == "__main__":
  flags.mark_flag_as_required("testapp_dir")
  flags.mark_flag_as_required("code_platform")
  flags.mark_flag_as_required("key_file")
  app.run(main)
