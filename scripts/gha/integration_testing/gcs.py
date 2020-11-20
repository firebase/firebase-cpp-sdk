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

"""A library of functionality related to the Google Cloud SDK."""

import datetime
import random
import shutil
import string
import subprocess

from absl import logging

# Full paths to the gCloud SDK tools. On Windows, subprocess.run does not check
# the PATH, so we need to find and supply the full paths.
# shutil.which returns None if it doesn't find a tool.
# Note: this kind of thing (among others) could likely be simplified by using
# the gCloud Python API instead of the command line tools.
GCLOUD = shutil.which("gcloud")
GSUTIL = shutil.which("gsutil")

PROJECT_ID = "games-auto-release-testing"


# This does not include the prefix "gs://<project_id>" because the gcloud
# command for using Firebase Test Lab requires the results dir to be a relative
# path within the bucket, not the full gs object URI.
def get_unique_gcs_id():
  """Defines an id usable for a unique object on GCS.

  To avoid artifacts from parallel runs overwriting each other, this creates
  a unique id. It's prefixed by a timestamp so that the artifact directories
  end up sorted, and to make it easier to locate the artifacts for a particular
  run.

  Returns:
      (str) A string of the form <timestamp>_<random_chars>.
  """
  # We generate a unique directory to store the results by appending 4
  # random letters to a timestamp. Timestamps are useful so that the
  # directories for different runs get sorted based on when they were run.
  timestamp = datetime.datetime.now().strftime("%y%m%d-%H%M%S")
  suffix = "".join(random.choice(string.ascii_letters) for _ in range(4))
  return "%s_%s" % (timestamp, suffix)


def relative_path_to_gs_uri(path):
  """Converts a relative GCS path to a GS URI understood by gsutil.

  This will prepend the gs prefix and project id to the path, i.e.
  path -> gs://<project_id>/results_dir

  Returns path unmodified if it already starts with the gs prefix.

  Args:
    path (str): A relative path.

  Returns:
    (str): The full GS URI for the given relative path.
  """
  if path.startswith("gs://"):
    return path
  else:
    return "gs://%s/%s" % (PROJECT_ID, path)


def authorize_gcs(key_file):
  """Activates the service account on GCS and specifies the project to use."""
  _verify_gcloud_sdk_command_line_tools()
  subprocess.run(
      args=[
          GCLOUD, "auth", "activate-service-account", "--key-file", key_file
      ],
      check=True)
  # Keep using this project for subsequent gcloud commands.
  subprocess.run(
      args=[GCLOUD, "config", "set", "project", PROJECT_ID],
      check=True)


# This is intended to be logged when a tool stores artifacts on GCS.
def get_gsutil_tips():
  """Returns a human readable string with tips on accessing a GCS bucket."""
  return "\n".join((
      "GCS Advice: Install the Google Cloud SDK to access the gsutil tool.",
      "Use 'gsutil ls <gs_uri>' to list contents of a directory on GCS.",
      "Use 'gsutil cp <gs_uri> <local_path>' to copy an artifact.",
      "Use 'gsutil -m cp -r <gs_uri> <local_path>' to copy a directory."
  ))


def _verify_gcloud_sdk_command_line_tools():
  """Verifies the presence of the gCloud SDK's command line tools."""
  logging.info("Looking for gcloud and gsutil tools...")
  if not GCLOUD:
    logging.error("gcloud not on path")
  if not GSUTIL:
    logging.error("gsutil not on path")
  if not GCLOUD or not GSUTIL:
    raise RuntimeError("Could not find required gCloud SDK tool(s)")
  subprocess.run([GCLOUD, "version"], check=True)
  subprocess.run([GSUTIL, "version"], check=True)
