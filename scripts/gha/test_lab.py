r"""Tool for sending mobile testapps to Firebase Test Lab for testing.

Requires Cloud SDK installed with gsutil. Can be checked as follows:
  gcloud --version

This tool will use the games-auto-release-testing GCS storage bucket. To
be authorized, it's necessary have the key file (JSON) (from valentine) and
supply it with the --key_file flag.

Valentine ID: 1561166657633900


Usage:

  python test_lab.py --testapp_dir ~/testapps --code_platform unity \
    --key_file ~/Downloads/key_file.json

This will recursively search ~/testapps for apks and ipas,
send them to FTL, and validate their results. The validation is specific to
the structure of the Firebase Unity and C++ testapps.


If you wish to specify a particular device to test on, you will need the model
id and version (api level for Android, OS version for iOS). These change over
time. You can find the currently supported models and versions with the
following commands:

  gcloud firebase test android models list
  gcloud firebase test ios models list

Note: you need the value in the MODEL_ID column, not MODEL_NAME. Examples:

Pixel 2, API level 28:
  --android_model walleye --android_api 28

iPhone 8 Plus, OS 11.4:
  --ios_model iphone8plus --ios_version 11.4

"""

import datetime
import os
import random
import string
import subprocess
import threading

from absl import app
from absl import flags
from absl import logging
import attr

from integration_testing import test_validation

_ANDROID = "android"
_IOS = "ios"

_PROJECT_ID = "games-auto-release-testing"

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
    "android_model", None,
    "Model id for desired device. See module docstring for details on how"
    " to get this id. If none, will use FTL's default.")
flags.DEFINE_string(
    "android_api", None,
    "API level for desired device. See module docstring for details on how"
    " to find available values. If none, will use FTL's default.")
flags.DEFINE_string(
    "ios_model", None,
    "Model id for desired device. See module docstring for details on how"
    " to get this id. If none, will use FTL's default.")
flags.DEFINE_string(
    "ios_version", None,
    "iOS version for desired device. See module docstring for details on how"
    " to find available values. If none, will use FTL's default.")


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  testapp_dir = FLAGS.testapp_dir
  code_platform = FLAGS.code_platform

  android_device = Device(model=FLAGS.android_model, version=FLAGS.android_api)
  ios_device = Device(model=FLAGS.ios_model, version=FLAGS.ios_version)

  testapps = []
  for file_dir, _, file_names in os.walk(testapp_dir):
    for file_name in file_names:
      full_path = os.path.join(file_dir, file_name)
      if file_name.endswith(".apk"):
        testapps.append((android_device, _ANDROID, full_path))
      elif file_name.endswith(".ipa"):
        testapps.append((ios_device, _IOS, full_path))

  if not testapps:
    logging.error("No testapps found.")
    return 1

  logging.info("Testapps found: %s", "\n".join(path for _, _, path in testapps))

  _authorize_gcs(FLAGS.key_file)

  gcs_base_dir = _get_base_results_dir()
  logging.info("Storing results in %s", _relative_path_to_gs_uri(gcs_base_dir))

  _gcloud_beta_install()

  tests = []
  for device, platform, path in testapps:
    # e.g. /testapps/unity/firebase_auth/app.apk -> unity_firebase_auth_app_apk
    name = path[len(testapp_dir) + 1:].replace("/", "_").replace(".", "_")
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
      "Results directory:\n%s",
      "\n".join(_gcs_list_dir(_relative_path_to_gs_uri(gcs_base_dir)))
  )

  all_success = _report_results(tests, code_platform)
  return 0 if all_success else 1


def _get_base_results_dir():
  """Defines the object used on GCS for all tests in this run."""
  # We generate a unique directory to store the results by appending 4
  # random letters to a timestamp. Timestamps are useful so that the
  # directories for different runs get sorted based on when they were run.
  timestamp = datetime.datetime.now().strftime("%y%m%d-%H%M%S")
  suffix = "".join(random.choice(string.ascii_letters) for _ in range(4))
  return "%s_%s" % (timestamp, suffix)


def _authorize_gcs(key_file):
  """Activates the service account on GCS and specifies the project."""
  subprocess.run(
      args=[
          "gcloud", "auth", "activate-service-account", "--key-file", key_file
      ],
      check=True)
  # Keep using this project for subsequent gcloud commands.
  subprocess.run(
      args=["gcloud", "config", "set", "project", _PROJECT_ID],
      check=True)


def _report_results(tests, code_platform):
  """Reports a summary of the test results."""
  # Distinguish between a failure and an error: a failure means the testapp
  # ran to completion, but 1 or more test cases failed. An error means the
  # testapp did not run to completion: it timed out, crashed,
  # or we couldn't find its result log on FTL for any reason.
  testapp_failures = []
  testapp_errors = []
  for test in tests:
    logging.info("Determining results for %s", test.testapp_path)
    log_text = _get_testapp_log_text_from_gcs(test.results_dir)
    if log_text:
      results = test_validation.validate_results(log_text, code_platform)
      logging.info("Test summary or log tail:\n%s", results.summary)
      if not results.complete:
        testapp_errors.append(test.testapp_path)
      if results.fails > 0:
        testapp_failures.append(test.testapp_path)
    else:
      testapp_errors.append(test.testapp_path)

  num_tests = len(tests)
  num_fails = len(testapp_failures)
  num_errors = len(testapp_errors)
  num_successes = num_tests - num_fails - num_errors

  logging.info("OVERALL TEST SUMMARY:")
  logging.info(
      "%d TESTAPPS QUEUED, %d SUCCESSES, %d FAILURES, %d ERRORS",
      num_tests, num_successes, num_fails, num_errors)
  if testapp_failures:
    logging.info("TEST FAILURES:\n%s", "\n".join(testapp_failures))
  if testapp_errors:
    logging.info("TEST ERRORS:\n%s", "\n".join(testapp_errors))
  return num_successes == num_tests


def _get_testapp_log_text_from_gcs(results_dir):
  """Gets the testapp log text generated by game loops."""
  gcs_dir = _relative_path_to_gs_uri(results_dir)
  try:
    gcs_contents = _gcs_list_dir(gcs_dir)
  except subprocess.CalledProcessError as e:
    logging.error("Unexpected error searching GCS logs:\n%s", e)
    return None
  # The path to the testapp log depends on the platform, device, and scenario
  # being tested. We search for a json file with 'results' in the name to avoid
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


def _relative_path_to_gs_uri(path):
  """Converts a relative GCS path to a GS URI understood by gsutil."""
  # e.g. <results_dir> -> gs://<project_id>/<results_dir>
  return "gs://%s/%s" % (_PROJECT_ID, path)


def _gcs_list_dir(gcs_path):
  """Recursively returns a list of contents for a directory on GCS."""
  args = ["gsutil", "ls", "-r", gcs_path]
  logging.info("Listing GCS contents: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=True)
  return result.stdout.splitlines()


def _gcs_read_file(gcs_path):
  """Extracts the contents of a file on GCS."""
  args = ["gsutil", "cat", gcs_path]
  logging.info("Reading GCS file: %s", " ".join(args))
  result = subprocess.run(args=args, capture_output=True, text=True, check=True)
  return result.stdout

def _gcloud_beta_install():
  """Install gcloud Beta Commands components for iOS Test"""
  subprocess.run(
    args=["gcloud", "--quiet", "components", "install", "beta"]
    ,check=True)


@attr.s(frozen=False, eq=False)
class Test(object):
  """Holds data related to the testing of one testapp."""
  device = attr.ib()
  platform = attr.ib()  # Android or iOS
  testapp_path = attr.ib()
  results_dir = attr.ib()  # Subdirectory on Cloud storage for this testapp

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

  @property
  def _gcloud_command(self):
    """Returns the args to send this testapp to FTL on the command line."""
    if self.platform == _ANDROID:
      cmd = ["gcloud", "firebase", "test", "android", "run"]
    elif self.platform == _IOS:
      cmd = ["gcloud", "beta", "firebase", "test", "ios", "run"]
    else:
      raise ValueError("Invalid platform, must be 'Android' or 'iOS'")
    return cmd + self.device.get_gcloud_flags() + [
        "--type", "game-loop",
        "--app", self.testapp_path,
        "--results-bucket", _PROJECT_ID,
        "--results-dir", self.results_dir,
        "--timeout", "300s"
    ]


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
