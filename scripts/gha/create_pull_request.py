# Copyright 2021 Google LLC
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

"""A utility to create pull requests.

USAGE:
  python scripts/gha/create_pull_request.py \
    --token ${{github.token}} \
    --head pr_branch \
    --base main \
    --title 'Title of the pull request' \
    [--body 'Body text for the pull request']

  Creates the pull request, and outputs the new PR number to stdout.
"""

import datetime
import shutil

from absl import app
from absl import flags
from absl import logging

import github

FLAGS = flags.FLAGS
_DEFAULT_MESSAGE = "Creating pull request."

flags.DEFINE_string(
    "token", None,
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "head", None,
    "Head branch name.")

flags.DEFINE_string(
    "base", "main",
    "Base branch name.")

flags.DEFINE_string(
    "title", None,
    "Title for the pull request.")

flags.DEFINE_string(
    "body", "",
    "Body text for the pull request.")

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  if github.create_pull_request(FLAGS.token, FLAGS.head, FLAGS.base, FLAGS.title, FLAGS.body, True):
    # Find the most recent pull_request with the given base and head, that's ours.
    pull_requests = github.list_pull_requests(FLAGS.token, "open", FLAGS.head, FLAGS.base)
    print(pull_requests[0]['number'])
  else:
    exit(1)


if __name__ == "__main__":
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("head")
  flags.mark_flag_as_required("title")
  app.run(main)
