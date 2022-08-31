#!/usr/bin/env python

# Copyright 2022 Google
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

r"""Tool for reading & validating game-loop test log from Firebase Test Lab 
GitHub Action (FTL GHA).
  https://github.com/FirebaseExtended/github-actions

This tool will read files from GCS storage bucket. Requires Cloud SDK installed 
with gsutil. (Should be installed by FTL GHA already.)

Usage:

  python read_ftl_test_result.py --test_result ${JSON_format_output_from_FTL_GHA} \
    --output_path ${log_path}

"""

import os
import attr
import subprocess
import json

from absl import app
from absl import flags
from absl import logging

from integration_testing import test_validation
from integration_testing import ftl_gha_validator


FLAGS = flags.FLAGS
flags.DEFINE_string("test_result", None, "FTL test result in JSON format.")
flags.DEFINE_string("output_path", None, "Log will be write into this path.")

@attr.s(frozen=False, eq=False)
class Test(object):
  """Holds data related to the testing of one testapp."""
  testapp_path = attr.ib()
  logs = attr.ib()


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  test_result = json.loads(FLAGS.test_result)
  tests = []
  for app in test_result.get("apps"):
    app_path = app.get("testapp_path")
    return_code = app.get("return_code")
    logging.info("testapp: %s\nreturn code: %s" % (app_path, return_code))
    if return_code == 0:
      gcs_dir = app.get("raw_result_link").replace("https://console.developers.google.com/storage/browser/", "gs://")
      logging.info("gcs_dir: %s" % gcs_dir)
      logs = ftl_gha_validator._get_testapp_log_text_from_gcs(gcs_dir)
      logging.info("Test result: %s", logs)
      tests.append(Test(testapp_path=app_path, logs=logs))
    else:
      logging.error("Test failed: %s", app)
      tests.append(Test(testapp_path=app_path, logs=None))

  (output_dir, file_name) = os.path.split(os.path.abspath(FLAGS.output_path))
  return test_validation.summarize_test_results(
    tests, 
    "cpp", 
    output_dir, 
    file_name=file_name)


if __name__ == '__main__':
  flags.mark_flag_as_required("test_result")
  flags.mark_flag_as_required("output_path")
  app.run(main)
