#!/usr/bin/env python

# Copyright 2021 Google
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
"""Trigger a GitHub workflow_dispatch workflow using the GitHub REST API.

This script allows you to easily trigger a workflow on GitHub using an access
token. It uses the GitHub REST API documented here:
https://docs.github.com/en/rest/reference/actions#create-a-workflow-dispatch-event

Usage:
python trigger_workflow.py -w workflow_filename -t github_token [-b branch_name]
       [-r git_repo_url] [-p <input1> <value1> -p <input2> <value2> ...]'
       [-C curl_command]

If -r is unspecified, uses the current repo.
If -c is unspecified, uses the current HEAD.
"""

import argparse
import subprocess
import time
import urllib.parse
import github

def main():
  args = parse_cmdline_args()
  if args.branch is None:
    args.branch=subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).decode('utf-8').rstrip('\n')
    print('autodetected branch: %s' % args.branch)
  if args.repo: # else use default firebase/firebase-cpp-sdk repo
    if not github.set_repo_url(args.repo):
      exit(2)
    else:
      print('set repo url to: %s' % github.GITHUB_API_URL)

  json_params = {}
  for param in args.param:
    json_params[param[0]] = param[1]
  if args.verbose or args.dryrun:
    print(f'request_url: {github.GITHUB_API_URL}/actions/workflows/{args.workflow}/dispatches')
    print(f'request_body: ref: {args.branch}, inputs: {json_params}')
  if args.dryrun:
    return(0)

  print('Sending request to GitHub API...')
  if not github.create_workflow_dispatch(args.token, args.workflow, args.branch, json_params):
    print('%sFailed to trigger workflow %s' % (
      '::error ::' if args.in_github_action else '', args.workflow))
    return(-1)

  print('Success!')
  time.sleep(args.sleep)  # Give a few seconds for the job to become queued.
  # Unfortunately, the GitHub REST API doesn't return the new workflow's run ID.
  # Query the list of workflows to find the one we just added.
  workflows = github.list_workflows(args.token, args.workflow, args.branch)
  run_id = 0
  if "workflow_runs" in workflows:
    branch_sha = subprocess.check_output(['git', 'rev-parse', args.branch]).decode('utf-8').rstrip('\n')
    for workflow in workflows['workflow_runs']:
      # Use a heuristic to get the new workflow's run ID.
      # Must match the branch name and commit sha, and be queued/in progress.
      if (workflow['status'] in ('queued', 'in_progress') and
          workflow['head_sha'] == branch_sha and
          workflow['head_branch'] == args.branch):
        run_id = workflow['id']
        break

  if run_id:
    workflow_url = 'https://github.com/firebase/firebase-cpp-sdk/actions/runs/%s' % (run_id)
  else:
    # Couldn't get a run ID, use a generic URL.
    workflow_url = '%s/actions/workflows/%s?query=%s+%s' % (
      github.GITHUB_API_URL, args.workflow,
      urllib.parse.quote('event:workflow_dispatch', safe=''),
      urllib.parse.quote('branch:'+args.branch, safe=''))
  print('%sStarted workflow %s: %s' % ('::warning ::' if args.in_github_action else '',
                                       args.workflow, workflow_url))


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Query matrix and config parameters used in Github workflows.')
  parser.add_argument('-w', '--workflow', required=True, help='Workflow filename to run, e.g. "integration_tests.yml"')
  parser.add_argument('-t', '--token', required=True, help='GitHub access token')
  parser.add_argument('-b', '--branch', help='Branch name to trigger workflow on, default is current branch')
  parser.add_argument('-r', '--repo', metavar='URL', help='GitHub repo to trigger workflow on, default is current repo')
  parser.add_argument('-p', '--param', default=[], nargs=2, action='append', metavar=('NAME', 'VALUE'),
                      help='Pass an input parameter to the workflow run. Can be used multiple times.')
  parser.add_argument('-d', '--dryrun', action='store_true', help='Just print the URL and JSON and exit.')
  parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose mode')
  parser.add_argument('-C', '--curl', default='curl', metavar='COMMAND', help='Curl command to use for making request')
  parser.add_argument('-A', '--in_github_action', action='store_true', help='Enable special logging for GitHub actions')
  parser.add_argument('-s', '--sleep', type=int, default=5, metavar='SECONDS',
                      help='How long to sleep before querying for the run ID, default 5')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()


