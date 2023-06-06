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

"""A utility for integration test workflow.

This script helps to update PR/Issue comments and labels during testing process. 

For PR comment, this script will update (create if not exist) the "Test Result" in comment.
stage value: [start, progress, end]
USAGE:
  python scripts/gha/it_workflow.py --stage <stage> \
    --token ${{github.token}} \
    --issue_number ${{needs.check_trigger.outputs.pr_number}}\
    --actor ${{github.actor}} \
    --commit ${{needs.prepare_matrix.outputs.github_ref}} \
    --run_id ${{github.run_id}} \
    [--new_token ${{steps.generate-token.outputs.token}}]

For Daily Report, this script will update (create if not exist) the "Test Result" in Issue 
with title "Nightly Integration Testing Report" and label "nightly-testing".
stage value: [report]
USAGE:
  python scripts/gha/it_workflow.py --stage report \
    --token ${{github.token}} \
    --actor ${{github.actor}} \
    --commit ${{needs.prepare_matrix.outputs.github_ref}} \
    --run_id ${{github.run_id}}

"""

import datetime
import pytz
import shutil

from absl import app
from absl import flags
from absl import logging

import github
import summarize_test_results as summarize

_REPORT_LABEL = "nightly-testing"
_REPORT_TITLE = "[C++] Nightly Integration Testing Report"
_REPORT_TITLE_FIRESTORE = "[C++] Nightly Integration Testing Report for Firestore"

_LABEL_TRIGGER_FULL = "tests-requested: full"
_LABEL_TRIGGER_QUICK = "tests-requested: quick"
_LABEL_PROGRESS = "tests: in-progress"
_LABEL_FAILED = "tests: failed"
_LABEL_SUCCEED = "tests: succeeded"

_COMMENT_TITLE_PROGESS = "### ⏳&nbsp; Integration test in progress...\n"
_COMMENT_TITLE_PROGESS_FLAKY = "### Integration test with FLAKINESS (but still ⏳&nbsp; in progress)\n" 
_COMMENT_TITLE_PROGESS_FAIL = "### ❌&nbsp; Integration test FAILED (but still ⏳&nbsp; in progress)\n" 
_COMMENT_TITLE_FLAKY = "### Integration test with FLAKINESS (succeeded after retry)\n"
_COMMENT_TITLE_FAIL = "### ❌&nbsp; Integration test FAILED\n"
_COMMENT_TITLE_SUCCEED = "### ✅&nbsp; Integration test succeeded!\n"
_COMMENT_TITLE_FLAKY_REPO = "### [build against repo] Integration test with FLAKINESS (succeeded after retry)\n"
_COMMENT_TITLE_FAIL_REPO = "### ❌&nbsp; [build against repo] Integration test FAILED\n"
_COMMENT_TITLE_SUCCEED_REPO = "### ✅&nbsp; [build against repo] Integration test succeeded!\n"
_COMMENT_TITLE_FLAKY_SDK = "\n***\n### [build against SDK] Integration test with FLAKINESS (succeeded after retry)\n"
_COMMENT_TITLE_FAIL_SDK = "\n***\n### ❌&nbsp; [build against SDK] Integration test FAILED\n"
_COMMENT_TITLE_SUCCEED_SDK = "\n***\n### ✅&nbsp; [build against SDK] Integration test succeeded!\n"
_COMMENT_TITLE_FLAKY_TIP = "\n***\n### [build against tip] Integration test with FLAKINESS (succeeded after retry)\n"
_COMMENT_TITLE_FAIL_TIP = "\n***\n### ❌&nbsp; [build against tip] Integration test FAILED\n"
_COMMENT_TITLE_SUCCEED_TIP = "\n***\n### ✅&nbsp; [build against tip] Integration test succeeded!\n"

_COMMENT_FLAKY_TRACKER = "\n\nAdd flaky tests to **[go/fpl-cpp-flake-tracker](http://go/fpl-cpp-flake-tracker)**\n"

_COMMENT_IDENTIFIER = "integration-test-status-comment"
_COMMENT_HIDDEN_DIVIDER = f'\r\n<hidden value="{_COMMENT_IDENTIFIER}"></hidden>\r\n'

_LOG_ARTIFACT_NAME = "log-artifact"
_LOG_OUTPUT_DIR = "test_results"

_BUILD_STAGES_START = "start"
_BUILD_STAGES_PROGRESS = "progress"
_BUILD_STAGES_END = "end"
_BUILD_STAGES_REPORT = "report"
_BUILD_STAGES = [_BUILD_STAGES_START, _BUILD_STAGES_PROGRESS, _BUILD_STAGES_END, _BUILD_STAGES_REPORT]

_BUILD_AGAINST_SDK = "sdk"
_BUILD_AGAINST_REPO = "repo"
_BUILD_AGAINST_TIP = "tip"

_BUILD_API_ALL = "all"
_BUILD_API_FIRESTORE = "firestore"
_BUILD_API_ALL_EXCEPT_FIRESTORE = "all_except_firestore"

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "stage", None,
    "Different stage while running the workflow. Valid values in _BUILD_STAGES.")

flags.DEFINE_string(
    "token", None, 
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "issue_number", None,
    "Github's issue # or pull request #.")

flags.DEFINE_string(
    "actor", None,
    "github.actor: The login of the user that initiated the workflow run.")

flags.DEFINE_string(
    "commit", None, "GitHub commit hash")

flags.DEFINE_string(
    "run_id", None,
    "github.run_id: A unique number for each workflow run within a repository.")

flags.DEFINE_string(
    "new_token", None,
    "Only used with --stage end"
    "Use a different token to remove the \"in-progress\" label,"
    "to allow the removal to trigger the \"Check Labels\" workflow.")   

flags.DEFINE_string(
    "build_against", None,
    "Integration testapps could either build against packaged SDK or repo")

flags.DEFINE_string(
    "build_apis", None,
    "Generate spearate report for different apis.")


def test_start(token, issue_number, actor, commit, run_id):
  """In PR, when start testing, add comment and label \"tests: in-progress\""""
  github.add_label(token, issue_number, _LABEL_PROGRESS)
  for label in [_LABEL_TRIGGER_FULL, _LABEL_TRIGGER_QUICK, _LABEL_FAILED, _LABEL_SUCCEED]:
    github.delete_label(token, issue_number, label)

  comment = (_COMMENT_TITLE_PROGESS +
             _get_description(actor, commit, run_id) +
             _COMMENT_HIDDEN_DIVIDER)
  _update_comment(token, issue_number, comment)


def test_progress(token, issue_number, actor, commit, run_id):
  """In PR, when some test failed, update failure info and 
  add label \"tests: failed\""""
  success_or_only_flakiness, log_summary = _get_summary_table(token, run_id)
  if success_or_only_flakiness and not log_summary:
    # succeeded (without flakiness)
    return
  else:
    if success_or_only_flakiness:
      # all failures/errors are due to flakiness (succeeded after retry)
      title = _COMMENT_TITLE_PROGESS_FLAKY
    else:
      # failures/errors still exist after retry
      title = _COMMENT_TITLE_PROGESS_FAIL
      github.add_label(token, issue_number, _LABEL_FAILED)
    comment = (title +
               _get_description(actor, commit, run_id) +
               log_summary +
               _COMMENT_FLAKY_TRACKER +
               _COMMENT_HIDDEN_DIVIDER)
    _update_comment(token, issue_number, comment)


def test_end(token, issue_number, actor, commit, run_id, new_token):
  """In PR, when some test end, update Test Result Report and 
  update label: add \"tests: failed\" if test failed, add label
  \"tests: succeeded\" if test succeed"""
  success_or_only_flakiness, log_summary = _get_summary_table(token, run_id)
  if success_or_only_flakiness and not log_summary:
    # succeeded (without flakiness)
    github.add_label(token, issue_number, _LABEL_SUCCEED)
    comment = (_COMMENT_TITLE_SUCCEED +
               _get_description(actor, commit, run_id) +
               _COMMENT_HIDDEN_DIVIDER)
    _update_comment(token, issue_number, comment)
  else:
    if success_or_only_flakiness:
      # all failures/errors are due to flakiness (succeeded after retry)
      title = _COMMENT_TITLE_FLAKY
      github.add_label(token, issue_number, _LABEL_SUCCEED)
    else:
      # failures/errors still exist after retry
      title = _COMMENT_TITLE_FAIL
      github.add_label(token, issue_number, _LABEL_FAILED)
    comment = (title +
               _get_description(actor, commit, run_id) +
               log_summary +
               _COMMENT_FLAKY_TRACKER +
               _COMMENT_HIDDEN_DIVIDER)
    _update_comment(token, issue_number, comment)

  github.delete_label(new_token, issue_number, _LABEL_PROGRESS)


def test_report(token, actor, commit, run_id, build_against, build_apis):
  """Update (create if not exist) a Daily/Nightly Report in Issue. 
  The Issue with title _REPORT_TITLE and label _REPORT_LABEL:
  https://github.com/firebase/firebase-cpp-sdk/issues?q=is%3Aissue+label%3Anightly-testing
  The report is with the format below:
    PREFIX
    HIDDEN DIVIDER
    REPORT (TEST AGAINST REPO)
    HIDDEN DIVIDER
    REPORT (TEST AGAINST SDK)
  """
  if build_apis == _BUILD_API_FIRESTORE:
    report_title = _REPORT_TITLE_FIRESTORE
    prefix = ""
  else:
    report_title = _REPORT_TITLE
    firestore_issue_number = _get_issue_number(token, _REPORT_TITLE_FIRESTORE, _REPORT_LABEL)
    firestore_issue_url = "https://github.com/firebase/firebase-cpp-sdk/issues/%s" % firestore_issue_number
    prefix = "Note: This report excludes firestore. Please also check **[the report for firestore](%s)**\n***\n" % firestore_issue_url

  issue_number = _get_issue_number(token, report_title, _REPORT_LABEL)
  previous_comment = github.get_issue_body(token, issue_number)
  [_, previous_comment_repo, previous_comment_sdk, previous_comment_tip] = previous_comment.split(_COMMENT_HIDDEN_DIVIDER)
  success_or_only_flakiness, log_summary = _get_summary_table(token, run_id)
  if success_or_only_flakiness and not log_summary:
    # succeeded (without flakiness)
    if build_against==_BUILD_AGAINST_REPO:
      title = _COMMENT_TITLE_SUCCEED_REPO  
    elif build_against==_BUILD_AGAINST_SDK:
      title = _COMMENT_TITLE_SUCCEED_SDK
    else:
      title = _COMMENT_TITLE_SUCCEED_TIP
    comment = title + _get_description(actor, commit, run_id)
  else:
    if success_or_only_flakiness:
      # all failures/errors are due to flakiness (succeeded after retry)
      if build_against==_BUILD_AGAINST_REPO:
        title = _COMMENT_TITLE_FLAKY_REPO  
      elif build_against==_BUILD_AGAINST_SDK:
        title = _COMMENT_TITLE_FLAKY_SDK
      else:
        title = _COMMENT_TITLE_FLAKY_TIP
    else:
      # failures/errors still exist after retry
      if build_against==_BUILD_AGAINST_REPO:
        title = _COMMENT_TITLE_FAIL_REPO  
      elif build_against==_BUILD_AGAINST_SDK:
        title = _COMMENT_TITLE_FAIL_SDK
      else:
        title = _COMMENT_TITLE_FAIL_TIP
    comment = title + _get_description(actor, commit, run_id) + log_summary + _COMMENT_FLAKY_TRACKER
  
  if build_against==_BUILD_AGAINST_REPO:
    comment = prefix + _COMMENT_HIDDEN_DIVIDER + comment + _COMMENT_HIDDEN_DIVIDER + previous_comment_sdk + _COMMENT_HIDDEN_DIVIDER + previous_comment_tip
  elif build_against==_BUILD_AGAINST_SDK:
    comment = prefix + _COMMENT_HIDDEN_DIVIDER + previous_comment_repo + _COMMENT_HIDDEN_DIVIDER + comment + _COMMENT_HIDDEN_DIVIDER + previous_comment_tip
  else:
    comment = prefix + _COMMENT_HIDDEN_DIVIDER + previous_comment_repo + _COMMENT_HIDDEN_DIVIDER + previous_comment_sdk + _COMMENT_HIDDEN_DIVIDER + comment

  if (_COMMENT_TITLE_SUCCEED_REPO in comment) and (_COMMENT_TITLE_SUCCEED_SDK in comment) and (build_apis != _BUILD_API_FIRESTORE or _COMMENT_TITLE_SUCCEED_TIP in comment):
    github.close_issue(token, issue_number)
  else:
    github.open_issue(token, issue_number)
    
  github.update_issue_comment(token, issue_number, comment)


def _get_issue_number(token, title, label):
  issues = github.search_issues_by_label(label)
  for issue in issues:
    if issue["title"] == title:
      return issue["number"]
  empty_comment = _COMMENT_HIDDEN_DIVIDER + " " + _COMMENT_HIDDEN_DIVIDER + " " + _COMMENT_HIDDEN_DIVIDER 
  return github.create_issue(token, title, label, empty_comment)["number"]


def _update_comment(token, issue_number, comment):
  comment_id = _get_comment_id(token, issue_number, _COMMENT_HIDDEN_DIVIDER)
  if not comment_id:
    github.add_comment(token, issue_number, comment)
  else:
    github.update_comment(token, comment_id, comment)

  
def _get_comment_id(token, issue_number, comment_identifier):
  comments = github.list_comments(token, issue_number)
  for comment in comments:
    if comment_identifier in comment['body']:
      return comment['id']
  return None


def _get_description(actor, commit, run_id):
  """Test Result Report Title and description"""
  return ("Requested by @%s on commit %s\n" % (actor, commit) +
          "Last updated: %s \n" % _get_datetime() +
          "**[View integration test log & download artifacts](https://github.com/firebase/firebase-cpp-sdk/actions/runs/%s)**\n" % run_id)


def _get_datetime():
  """Date time when Test Result Report updated"""
  pst_now = datetime.datetime.utcnow().astimezone(pytz.timezone("America/Los_Angeles"))
  return pst_now.strftime("%a %b %e %H:%M %Z %G")


def _get_summary_table(token, run_id):
  """Test Result Report Body, which is failed test table with markdown format"""
  # return (success_or_only_flakiness, failed_test_summary_table)
  return summarize.summarize_logs(dir=_LOG_OUTPUT_DIR, markdown=True)


def _get_artifact_id(token, run_id, name):
  artifacts = github.list_artifacts(token, run_id)
  for artifact in artifacts:
    if artifact["name"] == name:
      return artifact["id"]


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  if FLAGS.stage == _BUILD_STAGES_START:
    test_start(FLAGS.token, FLAGS.issue_number, FLAGS.actor, FLAGS.commit, FLAGS.run_id)
  elif FLAGS.stage == _BUILD_STAGES_PROGRESS:
    test_progress(FLAGS.token, FLAGS.issue_number, FLAGS.actor, FLAGS.commit, FLAGS.run_id)
  elif FLAGS.stage == _BUILD_STAGES_END:
    test_end(FLAGS.token, FLAGS.issue_number, FLAGS.actor, FLAGS.commit, FLAGS.run_id, FLAGS.new_token)
  elif FLAGS.stage == _BUILD_STAGES_REPORT:
    test_report(FLAGS.token, FLAGS.actor, FLAGS.commit, FLAGS.run_id, FLAGS.build_against, FLAGS.build_apis)
  else:
    print("Invalid stage value. Valid value: " + ",".join(_BUILD_STAGES))


if __name__ == "__main__":
  flags.mark_flag_as_required("stage")
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("actor")
  flags.mark_flag_as_required("commit")
  flags.mark_flag_as_required("run_id")
  app.run(main)
