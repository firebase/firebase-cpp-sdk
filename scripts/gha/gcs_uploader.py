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

"""Command line tool for submitting artifacts to Google Cloud Storage.

Example usage:
  python gcs_uploader.py --testapp_dir <dir> --key_file <path>

This will find desktop integration test artifacts in the testapp directory
and upload them to a suitable location on GCS.

The optional flag --extra_artifacts can be used to supply additional filenames
to upload. e.g. "--extra_artifacts GoogleService-Info.plist,Info.plist"

"""

import os
import subprocess

from absl import app
from absl import flags
from absl import logging

from integration_testing import gcs

_DEFAULT_ARTIFACTS = (
    "integration_test",
    "integration_test.exe",
    "google-services.json"
)

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "testapp_dir", None,
    "This directory will be searched for artifacts to upload.")
flags.DEFINE_string(
    "key_file", None, "Path to key file authorizing use of the GCS bucket.")
flags.DEFINE_list(
    "extra_artifacts", "",
    "Additional file names to upload. The following are always uploaded: "
    + ",".join(_DEFAULT_ARTIFACTS))


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  key_file_path = _fix_path(FLAGS.key_file)
  testapp_dir = _fix_path(FLAGS.testapp_dir)

  artifacts_to_upload = set(_DEFAULT_ARTIFACTS).union(FLAGS.extra_artifacts)

  logging.info(
      "Searching for the following artifacts:\n%s",
      "\n".join(artifacts_to_upload))

  artifacts = []
  for file_dir, _, file_names in os.walk(testapp_dir):
    for file_name in file_names:
      if file_name in artifacts_to_upload:
        artifacts.append(os.path.join(file_dir, file_name))

  if artifacts:
    logging.info("Found the following artifacts:\n%s", "\n".join(artifacts))
  else:
    logging.info("No artifacts found to upload")
    return

  gcs.authorize_gcs(key_file_path)

  gcs_prefix = gcs.relative_path_to_gs_uri(gcs.get_unique_gcs_id())
  logging.info("Uploading to %s", gcs_prefix)
  for artifact in artifacts:
    dest = _local_path_to_gcs_uri(gcs_prefix, artifact, testapp_dir)
    logging.info("Creating %s", dest)
    subprocess.run([gcs.GSUTIL, "cp", artifact, dest], check=True)
  logging.info("Finished uploading to %s", gcs_prefix)
  logging.info(
      "Use 'gsutil cp <gs_uri> <local_path>' to copy an artifact locally.\n"
      "Use 'gsutil -m cp -r %s <local_path>' to copy everything.", gcs_prefix)


def _local_path_to_gcs_uri(gcs_prefix, path, testapp_dir):
  """Converts full local path to a GCS URI for gsutil.

  Replaces backslashes with forward slashes, since GCS expects the latter.

  Example:
    gcs_prefix: gs://<id>/<timestamp>
    path: /Users/username/testapps/FirebaseAuth/integration_test
    testapp_dir: /Users/username/testapps

    Return value: gs://<id>/<timestamp>/FirebaseAuth/integration_test

  Args:
    gcs_prefix (str): Directory on GCS, of the form "gs://<id>/<etc>"
    path (str): Full local path to artifact.
    testapp_dir (str): Prefix to strip from all paths.

  Returns:
    (str): GCS URI for this artifact.

  """
  subdirectory = os.path.relpath(path, testapp_dir).replace("\\", "/")
  return gcs_prefix + "/" + subdirectory


def _fix_path(path):
  """Expands ~, normalizes slashes, and converts relative paths to absolute."""
  return os.path.abspath(os.path.expanduser(path))


if __name__ == "__main__":
  app.run(main)
