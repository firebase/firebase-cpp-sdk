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

"""A utility to dismiss PR reviews.

USAGE:
  python scripts/gha/dismiss_reviews.py \
    --token ${{github.token}} \
    --pull_number ${{needs.check_trigger.outputs.pr_number}} \
    [--message 'Message to be posted on the dismissal.'] \
    [--reviewer github_username] \
    [--review_state ANY|APPROVED|CHANGES_REQUESTED|COMMENTED|PENDING]
"""

import datetime
import shutil

from absl import app
from absl import flags
from absl import logging

import github

FLAGS = flags.FLAGS
_DEFAULT_MESSAGE = "Dismissing stale review."

flags.DEFINE_string(
    "token", None,
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "pull_number", None,
    "Github's pull request #.")

flags.DEFINE_string(
    "message", _DEFAULT_MESSAGE,
    "Message to post on dismissed reviews")

flags.DEFINE_string(
    "reviewer", None,
    "Reviewer to dismiss (by username). If unspecified, dismiss all reviews.")

flags.DEFINE_enum(
    "review_state", "ANY", ["ANY", "APPROVED", "CHANGES_REQUESTED", "COMMENTED",
                            "PENDING"],
    "Only dismiss reviews in this state. Specify ANY for any state.")

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  # Get list of reviews from PR.
  reviews = github.get_reviews(FLAGS.token, FLAGS.pull_number)
  logging.debug("Found %d reviews", len(reviews))

  # Filter out already-dismissed reviews.
  reviews = [r for r in reviews if r['state'] != 'DISMISSED']
  # Filter by review_state if specified.
  if FLAGS.review_state != 'ANY':
    reviews = [r for r in reviews if r['state'] == FLAGS.review_state]
  # Filter by reviewer's username, if specified.
  if FLAGS.reviewer:
    reviews = [r for r in reviews if r['user']['login'] == FLAGS.reviewer]

  if reviews:
    review_ids = [r['id'] for r in reviews]
    logging.debug("Dismissing reviews: %s", review_ids)
    for review_id in review_ids:
      github.dismiss_review(FLAGS.token, FLAGS.pull_number,
                            review_id, FLAGS.message)

if __name__ == "__main__":
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("pull_number")
  app.run(main)
