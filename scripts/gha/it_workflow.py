import datetime
import pytz
import shutil

from absl import app
from absl import flags
from absl import logging

import github
import summarize_test_results as summarize


LABEL_PROGRESS = "tests: in-progress"
LABEL_FAILED = "tests: failed"
LABEL_SUCCEED = "tests: succeeded"

COMMENT_TITLE_PROGESS = "### ⏳&nbsp; Integration test in progress...\n"
COMMENT_TITLE_PROGESS_FAIL = "### ❌&nbsp; Integration test FAILED (but still ⏳&nbsp; in progress)\n" 
COMMENT_TITLE_FAIL = "### ❌&nbsp; Integration test FAILED\n"
COMMENT_TITLE_SUCCEED = "### ✅&nbsp; Integration test succeeded!\n"

COMMENT_IDENTIFIER = "integration-test-status-comment"
COMMENT_SUFFIX = f'\n\n\n<hidden value="{COMMENT_IDENTIFIER}"></hidden>'

LOG_ARTIFACT_NAME = "log-artifact"
LOG_OUTPUT_DIR = "test_results"

_BUILD_STAGES_START = "start"
_BUILD_STAGES_PROGRESS = "progress"
_BUILD_STAGES_END = "end"
_BUILD_STAGES = [_BUILD_STAGES_START, _BUILD_STAGES_PROGRESS, _BUILD_STAGES_END]

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
    "Use a different token to remove the \"in-progress\" label,"
    "to allow the removal to trigger the \"Check Labels\" workflow.")                    

def test_start(token, issue_number, actor, commit, run_id):
  github.add_label(token, issue_number, LABEL_PROGRESS)
  for label in [LABEL_FAILED, LABEL_SUCCEED]:
    github.delete_label(token, issue_number, label)

  comment = (COMMENT_TITLE_PROGESS +
             get_description(actor, commit, run_id) +
             COMMENT_SUFFIX)
  update_comment(token, issue_number, comment)


def test_progress(token, issue_number, actor, commit, run_id):
  log_summary = get_summary_talbe(token, run_id)
  if log_summary == 0:
    return
  else:
    github.add_label(token, issue_number, LABEL_FAILED)
    comment = (COMMENT_TITLE_PROGESS_FAIL +
                get_description(actor, commit, run_id) +
                log_summary +
                COMMENT_SUFFIX)
    update_comment(token, issue_number, comment)


def test_end(token, issue_number, actor, commit, run_id, new_token):
  log_summary = get_summary_talbe(token, run_id)
  if log_summary == 0:
    github.add_label(token, issue_number, LABEL_SUCCEED)
    comment = (COMMENT_TITLE_SUCCEED +
                get_description(actor, commit, run_id) +
                COMMENT_SUFFIX)
    update_comment(token, issue_number, comment)
  else:
    github.add_label(token, issue_number, LABEL_FAILED)
    comment = (COMMENT_TITLE_FAIL +
                get_description(actor, commit, run_id) +
                log_summary +
                COMMENT_SUFFIX)
    update_comment(token, issue_number, comment)

  github.delete_label(new_token, issue_number, LABEL_PROGRESS)


def update_comment(token, issue_number, comment):
  comment_id = get_comment_id(issue_number, COMMENT_SUFFIX)
  if not comment_id:
    github.add_comment(token, issue_number, comment)
  else:
    github.update_comment(token, comment_id, comment)

  
def get_comment_id(issue_number, comment_identifier):
  comments = github.list_comments(issue_number)
  for comment in comments:
    if comment_identifier in comment['body']:
      return comment['id']
  return None


def get_description(actor, commit, run_id):
  return ("Requested by @%s on commit %s\n" % (actor, commit) +
          "Last updated: %s \n" % get_datetime() +
          "**[View integration test log & download artifacts](https://github.com/firebase/firebase-cpp-sdk/actions/runs/%s)**\n" % run_id)


def get_datetime():
  pst_now = datetime.datetime.utcnow().astimezone(pytz.timezone("America/Los_Angeles"))
  return pst_now.strftime("%a %b %e %H:%M %Z %G")


def get_summary_talbe(token, run_id):
  # artifact_id only exist after workflow finishs running
  # Thus, "down artifact" logic is in the workflow 
  # artifact_id = get_artifact_id(token, run_id, LOG_ARTIFACT_NAME)
  # artifact_path = LOG_ARTIFACT_NAME + ".zip"
  # github.download_artifact(token, artifact_id, artifact_path)
  # shutil.unpack_archive(artifact_path, LOG_OUTPUT_DIR)
  summary_talbe = summarize.summarize_logs(dir=LOG_OUTPUT_DIR, markdown=True)
  return summary_talbe


def get_artifact_id(token, run_id, name):
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
  else:
    print("Invalid stage value. Valid value: " + ",".join(_BUILD_STAGES))


if __name__ == "__main__":
  flags.mark_flag_as_required("stage")
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("actor")
  flags.mark_flag_as_required("commit")
  flags.mark_flag_as_required("run_id")
  app.run(main)
