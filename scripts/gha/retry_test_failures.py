# Copyright 2023 Google LLC
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

"""A utility to retry failed jobs in a workflow run.

USAGE:
  python3 scripts/gha/retry_test_failures.py \
    --token ${{github.token}} \
    --run_id <github_workflow_run_id>
"""

import datetime
import json
import re
import shutil

from absl import app
from absl import flags
from absl import logging

import firebase_github

FLAGS = flags.FLAGS
MAX_RETRIES=2

flags.DEFINE_string(
    "token", None,
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "run_id", None,
    "Github's workflow run ID.")


def get_log_group(log_text, group_name):
  group_log = []
  in_group = False
  for line in log_text.split("\n"):
    line_no_ts = line[29:]
    if line_no_ts.startswith('##[group]'):
      if group_name in line_no_ts:
        print("got group %s" % group_name)
        in_group = True
    if in_group:
      group_log.append(line_no_ts)
      if line_no_ts.startswith('##[error])'):
        print("end group %s" % group_name)
        in_group = False
        break
  return group_log

def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  # Get list of workflow jobs.
  workflow_jobs = firebase_github.list_jobs_for_workflow_run(
      FLAGS.token, FLAGS.run_id, attempt='all')
  if not workflow_jobs or len(workflow_jobs) == 0:
    logging.error("No jobs found for workflow run %s", FLAGS.run_id)
    exit(1)

  failed_jobs = {}
  all_jobs = {}
  for job in workflow_jobs:
    all_jobs[job['id']] = job
    if job['conclusion'] != 'success' and job['conclusion'] != 'skipped':
      if job['name'] in failed_jobs:
        other_run = failed_jobs[job['name']]
        if job['run_attempt'] > other_run['run_attempt']:
          # This is a later run than the one that's already there
          failed_jobs[job['name']] = job
      else:
        failed_jobs[job['name']] = job

  should_rerun_jobs = False
  for job_name in failed_jobs:
    job = failed_jobs[job_name]
    logging.info('Considering job %s attempt %d: %s (%s)',
                 job['conclusion'] if job['conclusion'] else job['status'],
                 job['run_attempt'], job['name'], job['id'])
    if job['status'] != 'completed':
      # Don't retry a job that is already in progress or queued
      logging.info("Not retrying, as %s is already %s",
                   job['name'], job['status'].replace("_", " "))
      should_rerun_jobs = False
      break
    if job['run_attempt'] > MAX_RETRIES:
      # Don't retry a job more than MAX_RETRIES times.
      logging.info("Not retrying, as %s has already been attempted %d times",
                   job['name'], job['run_attempt'])
      should_rerun_jobs = False
      break
    if job['conclusion'] == 'failure':
      job_logs = firebase_github.download_job_logs(FLAGS.token, job['id'])
      if job['name'].startswith('build-'):
        # Retry all build failures that don't match a compiler error.
        if not re.search(r'.*/.*:[0-9]+:[0-9]+: error:', job_logs):
          should_rerun_jobs = True
        # Also retry build jobs that timed out
        if re.search(r'timed? ?out|network error|maximum execution time',
                     job_logs, re.IGNORECASE):
          should_rerun_jobs = True
      elif job['name'].startswith('download-'):
        # If a download step failed, automatically retry.
        should_rerun_jobs = True
      elif job['name'].startswith('package-'):
        # Retry packaging jobs that timed out
        if re.search(r'timed? ?out|network error|maximum execution time',
                     job_logs, re.IGNORECASE):
          should_rerun_jobs = True
      elif job['name'].startswith('test-'):
        if '-android' in job['name'] or '-ios' in job['name']:
          # Mobile tests should always be retried.
          should_rerun_jobs = True
        else:
          # Desktop tests should only retry on a network error or timeout.
          if re.search(r'timed? ?out|network error|maximum execution time',
                       job_logs, re.IGNORECASE):
            should_rerun_jobs = True

  if should_rerun_jobs:
    logging.info("Re-running failed jobs in workflow run %s", FLAGS.run_id)
    if not firebase_github.rerun_failed_jobs_for_workflow_run(
        FLAGS.token, FLAGS.run_id):
      logging.error("Error submitting GitHub API request")
      exit(1)
  else:
    logging.info("Not re-running jobs.")


if __name__ == "__main__":
  flags.mark_flag_as_required("token")
  flags.mark_flag_as_required("run_id")
  app.run(main)
