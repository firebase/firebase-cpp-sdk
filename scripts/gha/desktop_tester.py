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

"""Runs and validates desktop C++ testapps.

Usage:
  python desktop_tester.py --testapp_dir ~/Downloads/testapps

This will search --testapp_dir for files whose name is given by --testapp_name
(default: "integration_test", or "integration_test.exe" on Windows), execute
them, parse their log output according to the C++ test summary format, and
report a summary of results.

"""

import os
import platform
import shlex
import subprocess
import time
import threading

from absl import app
from absl import flags
from absl import logging
import attr

from integration_testing import test_validation

FLAGS = flags.FLAGS

flags.DEFINE_string("testapp_dir", None, "Look for testapps in this directory.")
flags.DEFINE_string("testapp_name", "integration_test", "Name of the testapps.")
flags.DEFINE_string(
    "logfile_name", "",
    "Create test log artifact test-results-$logfile_name.log."
    " logfile will be created and placed in testapp_dir.")  
flags.DEFINE_string(
  "cmd_prefix", "",
  "Prefix to include before the command when running each test")

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  testapp_dir = _fix_path(FLAGS.testapp_dir)

  testapp_name = FLAGS.testapp_name
  if platform.system() == "Windows":
    testapp_name += ".exe"

  testapps = []
  for file_dir, _, file_names in os.walk(testapp_dir):
    for file_name in file_names:
      # ios build create intermediates file with same name, filter them out 
      if file_name == testapp_name and "ios_build" not in file_dir:
        testapps.append(os.path.join(file_dir, file_name))
  if not testapps:
    logging.error("No testapps found.")
    test_validation.write_summary(testapp_dir, "Desktop tests: none found.")
    return 1
  logging.info("Testapps found: %s\n", "\n".join(testapps))

  tests = [Test(testapp_path=testapp) for testapp in testapps]

  logging.info("Running tests...")
  threads = []
  for test in tests:
    thread = threading.Thread(target=test.run)
    threads.append(thread)
    thread.start()
  for thread in threads:
    thread.join()

  return test_validation.summarize_test_results(
      tests, 
      test_validation.CPP, 
      testapp_dir, 
      file_name="test-results-" + FLAGS.logfile_name + ".log")


def _fix_path(path):
  """Expands ~, normalizes slashes, and converts relative paths to absolute."""
  return os.path.abspath(os.path.expanduser(path))


@attr.s(frozen=False, eq=False)
class Test(object):
  """Holds data related to the testing of one testapp."""
  testapp_path = attr.ib()
  # This will be populated after the test completes, instead of initialization.
  logs = attr.ib(init=False, default=None)

  # This runs in a separate thread, so instead of returning values we store
  # them as fields so they can be accessed from the main thread.
  def run(self):
    """Executes this testapp."""
    os.chmod(self.testapp_path, 0o777)
    args = list(shlex.split(FLAGS.cmd_prefix)) + [self.testapp_path]
    args_str = subprocess.list2cmdline(args)
    logging.info("Test starting: %s", args_str)
    start_time_secs = time.monotonic()
    try:
      result = subprocess.run(
          args=args,
          cwd=os.path.dirname(self.testapp_path),  # Testapp uses CWD for config
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          text=True,
          errors="replace",
          check=False,
          timeout=900)
    except subprocess.TimeoutExpired as e:
      logging.error("Testapp timed out: %s", args_str)
      # e.output will sometimes be bytes, sometimes string. Decode if needed.
      try:
        self.logs = e.output.decode()
      except AttributeError:  # This will happen if it's already a string.
        self.logs = e.output
    else:
      self.logs = result.stdout
      logging.info(
        "Test result of %s (exit code: %s): %s",
        args_str, result.returncode, self.logs)

    end_time_secs = time.monotonic()
    elapsed_time_secs = end_time_secs - start_time_secs
    elapsed_time_str = f"{elapsed_time_secs/60:.2f} minutes"
    logging.info(
      "Test completed (elapsed time: %s): %s",
      elapsed_time_str, args_str)


if __name__ == "__main__":
  app.run(main)
