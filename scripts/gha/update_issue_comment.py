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

# Installing prerequisites:
# 
# sudo python3 -m pip install python-dateutil progress attrs

"""A utility to add modify part of an issue comment, which must already exist.
The comment text should be entered in stdin. The script preserves the previous
text before and affer the hidden tags.

USAGE:
  python scripts/gha/update_issue_comment.py \
    --token GITHUB_TOKEN \
    --issue_title "Issue title goes here" \
    --issue_label "Issue label goes here" \
    --start_tag "hidden-tag-start" \
    --end_tag "hidden-tag-end" < updated-comment-section.md
"""

import sys

from absl import app
from absl import flags
from absl import logging

import firebase_github

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "token", None,
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "start_tag", "comment-start",
    "Starting tag (inside a <hidden value=\"tag-goes-here\"></hidden> element).")

flags.DEFINE_string(
    "end_tag", "comment-end",
    "Ending tag (inside a <hidden value=\"tag-goes-here\"></hidden> element).")

flags.DEFINE_string(
    "issue_title", None,
    "Title of the issue to modify. Will fail if it doesn't exist.")

flags.DEFINE_string(
    "issue_label", 'nightly-testing',
    "Label to search for.")


def get_issue_number(token, title, label):
  """Get the GitHub isssue number for a given issue title and label"""
  issues = firebase_github.search_issues_by_label(label)
  for issue in issues:
    if issue["title"] == title:
      return issue["number"]
  return None


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  comment_start = "\r\n<hidden value=\"%s\"></hidden>\r\n" % FLAGS.start_tag
  comment_end = "\r\n<hidden value=\"%s\"></hidden>\r\n" % FLAGS.end_tag

  issue_number = get_issue_number(FLAGS.token, FLAGS.issue_title, FLAGS.issue_label)
  if not issue_number:
    logging.fatal("Couldn't find a '%s' issue matching '%s'",
                  FLAGS.issue_label,
                  FLAGS.issue_title)
  logging.info("Found issue number: %d", issue_number)
  
  previous_comment = firebase_github.get_issue_body(FLAGS.token, issue_number)
  if comment_start not in previous_comment:
    logging.fatal("Couldn't find start tag '%s' in previous comment", comment_start)
  if comment_end not in previous_comment:
    logging.fatal("Couldn't find end tag '%s' in previous comment", comment_end)

  if comment_start == comment_end:
    [prefix, _, suffix] = previous_comment.split(comment_start)
  else:
    [prefix, remainder] = previous_comment.split(comment_start)
    [_, suffix] = remainder.split(comment_end)

  new_text = sys.stdin.read()
  comment = prefix + comment_start + new_text + comment_end + suffix

  firebase_github.update_issue_comment(FLAGS.token, issue_number, comment)



if __name__ == "__main__":
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("issue_title")
  app.run(main)
