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
import json
import os
import re
import subprocess
import time
import urllib.parse

def main():
  args = parse_cmdline_args()
  if args.repo is None:
      args.repo=subprocess.check_output(['git', 'config', '--get', 'remote.origin.url']).decode('utf-8').rstrip('\n').lower()
      print('autodetected repo: %s' % args.repo)
  if args.branch is None:
      args.branch=subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).decode('utf-8').rstrip('\n')
      print('autodetected branch: %s' % args.branch)
  if not args.repo.startswith('https://github.com/'):
      print('Error, only https://github.com/ repositories are allowed.')
      exit(2)
  (repo_owner, repo_name) = re.match(r'https://github\.com/([^/]+)/([^/.]+)', args.repo).groups()

  # POST /repos/{owner}/{repo}/actions/workflows/{workflow_id}/dispatches
  request_url = 'https://api.github.com/repos/%s/%s/actions/workflows/%s/dispatches' % (repo_owner, repo_name, args.workflow)
  json_params = {}
  for param in args.param:
      json_params[param[0]] = param[1]
  json_text = '{"ref":%s,"inputs":%s}' % (json.dumps(args.branch), json.dumps(json_params))
  if args.verbose or args.dryrun:
    print('request_url: %s' % request_url)
    print('request_body: %s' % json_text)
  if args.dryrun:
    return(0)

  print('Sending request to GitHub API...')
  run_output = subprocess.check_output([args.curl,
                                        '-s', '-o', '-', '-w', '\nHTTP status %{http_code}\n',
                                        '-X', 'POST',
                                        '-H', 'Accept: application/vnd.github.v3+json',
                                        '-H', 'Authorization: token %s' % args.token,
                                        request_url, '-d', json_text]
                                      + ([] if not args.verbose else ['-v'])).decode('utf-8').rstrip('\n')
  if args.verbose:
    print(run_output)
  if not re.search('HTTP status 2[0-9][0-9]$', run_output):
    if not args.verbose:
      print(run_output)
    # Super quick and dirty way to get the message text since the appended status code means that
    # the contents are not valid JSON.
    error_message = re.search(r'"message": "([^"]+)"', run_output).group(1)
    print('%sFailed to trigger workflow %s: %s' % (
      '::error ::' if args.in_github_action else '', args.workflow, error_message))
    return(-1)

  print('Success!')
  time.sleep(args.sleep)  # Give a few seconds for the job to become queued.
  # Unfortunately, the GitHub REST API doesn't return the new workflow's run ID.
  # Query the list of workflows to find the one we just added.
  request_url = 'https://api.github.com/repos/%s/%s/actions/workflows/%s/runs?event=workflow_dispatch&branch=%s' % (repo_owner, repo_name, args.workflow, args.branch)
  run_output = subprocess.check_output([args.curl,
                                          '-s', '-X', 'GET',
                                          '-H', 'Accept: application/vnd.github.v3+json',
                                          '-H', 'Authorization: token %s' % args.token,
                                          request_url]).decode('utf-8').rstrip('\n')
  run_id = 0
  workflows = json.loads(run_output)
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
    workflow_url = 'https://github.com/%s/%s/actions/workflows/%s?query=%s+%s' % (
      repo_owner, repo_name, args.workflow,
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


