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
python trigger_workflow.py -w workflow_name -t github_token [-c commit_id]
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

def main():
  args = parse_cmdline_args()
  if args.repo is None:
      args.repo=subprocess.check_output(['git', 'config', '--get', 'remote.origin.url']).decode('utf-8').rstrip("\n").lower()
      print('autodetect repo: %s' % args.repo)
  if args.commit is None:
      args.commit=subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode('utf-8').rstrip("\n").lower()
      print('autodetect commit: %s' % args.commit)
  if not args.repo.startswith('https://github.com/'):
      print('Error, only https://github.com/ repositories are allowed.')
      exit(2)
  (repo_owner, repo_name) = re.match(r'https://github\.com/([^/]+)/([^/.]+)', args.repo).groups()

  # POST /repos/{owner}/{repo}/actions/workflows/{workflow_id}/dispatches
  request_url = "https://api.github.com/repos/%s/%s/actions/workflows/%s/dispatches" % (repo_owner, repo_name, args.workflow)
  print('POST url: %s' % request_url)
  json_params = {}
  for param in args.param:
      json_params[param[0]] = param[1]
  json_text = '{ "ref": %s, "inputs": "%s" }' % (json.dumps(args.commit), json.dumps(json_params))
  print(json_text)
  subprocess.run([args.curl, '-X', 'POST', '-H', 'Accept: application/vnd.github.v3+json',
                  request_url, '-d', json_text])
  

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Query matrix and config parameters used in Github workflows.')
  parser.add_argument('-w', '--workflow', required=True, help='Workflow name to run, e.g. "integration_test"')
  parser.add_argument('-t', '--token', required=True, help='GitHub access token')
  parser.add_argument('-c', '--commit', help='Commit ID to trigger workflow on, default is current HEAD')
  parser.add_argument('-r', '--repo', help='GitHub repo to trigger workflow on, default is current repo')
  parser.add_argument('-C', '--curl', default='curl', help='Curl command to use for making request')
  parser.add_argument('-p', '--param', default=[], nargs=2, action='append',
                      help='Add a parameter to the workflow run: -p <input> <value>')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()


