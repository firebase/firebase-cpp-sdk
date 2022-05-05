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
"""Run a code linter and post the results as GitHub comments.

This script allows you to easily trigger a workflow on GitHub using an access
token. It uses the GitHub REST API documented here:
https://docs.github.com/en/rest/reference/actions#create-a-workflow-dispatch-event

Usage:
python lint_commenter.py -t github_token -p pr_number
       [-r git_repo_url] [-C curl_command] [-l lint_command]

If -r is unspecified, uses the current repo.
"""

import argparse
from html import escape
import json
import os
import re
import subprocess
import time
from unidiff import PatchSet
import urllib.parse

# Put any lint warnings you want to fully ignore into this list.
# (Include a short explanation of why.)
IGNORE_LINT_WARNINGS = [
  'build/include_subdir',   # doesn't know about our include paths
  'build/c++11',            # ignore "unapproved c++11 header" warning
  'readability/casting',    # allow non-C++ casts in rare occasions
  'runtime/references',     # allow non-const references
  'whitespace/indent',      # we rely on our code formatter for this...
  'whitespace/line_length'  # ...and for this
]
# Exclude files within the following paths (specified as regexes).
EXCLUDE_PATH_REGEX = [
  # These files are copied from an external repo and are outside our control.
  r'^analytics/ios_headers/',
  r'^ios_pod/swift_headers/'
]
# The linter gives every error a confidence score.
# 1 = It's most likely not really an issue.
# 5 = It's definitely an issue.
# Set this to 1 to show all warnings, or higher to show fewer warnings.
MINIMUM_CONFIDENCE = 1
LABEL_TO_SKIP_LINT = 'no-lint'
CPPLINT_FILTER = '-'+',-'.join(IGNORE_LINT_WARNINGS)
LINT_COMMENT_HEADER = '⚠️ Lint warning: `'
LINT_COMMENT_FOOTER = '`'
HIDDEN_COMMENT_TAG = '<hidden value="cpplint-file-comment"></hidden>'

def main():
  # This script performs a number of steps:
  #
  # 1. Get the PR's diff to find the list of affected files and lines in the PR.
  #
  # 2. Run lint on all files in the PR. For each lint warning, check if it falls
  #    within the range of lines affected by the diff. Omit that warning if it
  #    doesn't fall in the affected lines.
  #
  # 3. Delete any prior lint warning comments posted by previous runs.
  #
  # 4. Post any lint warnings that fall within the range of the PR's diff.

  args = parse_cmdline_args()
  if args.repo is None:
      args.repo=subprocess.check_output(['git', 'config', '--get', 'remote.origin.url']).decode('utf-8').rstrip('\n').lower()
      if args.verbose:
        print('autodetected repo: %s' % args.repo)
  if not args.repo.startswith('https://github.com/'):
      print('Error, only https://github.com/ repositories are allowed.')
      exit(2)
  (repo_owner, repo_name) = re.match(r'https://github\.com/([^/]+)/([^/.]+)', args.repo).groups()

  # Get the head commit for the pull request.
  # GET /repos/{owner}/{repo}/pulls/{pull_number}
  request_url = 'https://api.github.com/repos/%s/%s/pulls/%s' % (repo_owner, repo_name, args.pr_number)
  header = 'Accept: application/vnd.github.VERSION.json'
  pr_data = json.loads(subprocess.check_output(
      [args.curl,
       '-s', '-X', 'GET',
       '-H', 'Accept: application/vnd.github.v3+json',
       '-H', 'Authorization: token %s' % args.token,
       request_url
      ] + ([] if not args.verbose else ['-v'])).decode('utf-8').rstrip('\n'))

  commit_sha = pr_data['head']['sha']
  if args.verbose:
    print('Commit sha:', commit_sha)

  skip_lint = False
  if 'labels' in pr_data:
    for label in pr_data['labels']:
      if label['name'] == LABEL_TO_SKIP_LINT:
        skip_lint = True
        break
  if skip_lint:
    print('PR #%s has "%s" label, skipping lint checks' % (args.pr_number, LABEL_TO_SKIP_LINT))
    exit(0)

  # Get the diff for the pull request.
  # GET /repos/{owner}/{repo}/pulls/{pull_number}
  request_url = 'https://api.github.com/repos/%s/%s/pulls/%s' % (repo_owner, repo_name, args.pr_number)
  header = 'Accept: application/vnd.github.VERSION.diff'

  if args.verbose:
    print('request_url: %s' % request_url)

  pr_diff = subprocess.check_output(
      [args.curl,
       '-s', '-o', '-', '-w', '\nHTTP status %{http_code}\n',
       '-X', 'GET',
       '-H', header,
       '-H', 'Authorization: token %s' % args.token,
       request_url
      ] + ([] if not args.verbose else ['-v'])).decode('utf-8')
  # Parse the diff to determine the whether each source line is touched.
  # Only lint lines that refer to parts of files that are diffed will be shown.
  # Information on what this means here:
  # https://docs.github.com/en/rest/reference/pulls#create-a-review-comment-for-a-pull-request
  valid_lines = {}
  file_list = []
  pr_patch = PatchSet(pr_diff)
  for pr_patch_file in pr_patch:
    # Skip files that only remove code.
    if pr_patch_file.removed and not pr_patch_file.added:
      continue
    # Skip files that match an EXCLUDE_PATH_REGEX
    excluded = False
    for exclude_regex in EXCLUDE_PATH_REGEX:
      if re.search(exclude_regex, pr_patch_file.path):
        excluded = True
        break
    if excluded: continue
    file_list.append(pr_patch_file.path)
    valid_lines[pr_patch_file.path] = set()
    for hunk in pr_patch_file:
      if hunk.target_length > 0:
        for line_number in range(
            hunk.target_start,
            hunk.target_start + hunk.target_length):
          # This line is modified by the diff, add it to the valid set of lines.
          valid_lines[pr_patch_file.path].add(line_number)

  # Now we also have a list of files in repo.
  try:
    lint_results=subprocess.check_output([
        args.lint_command,
        '--output=emacs',
        ('--filter=%s' % CPPLINT_FILTER),
        ('--repository=..')
    ] + file_list, stderr=subprocess.STDOUT).decode('utf-8').split('\n')
  except subprocess.CalledProcessError as e:
    # Nothing to do if there is an exception.
    lint_results=e.output.decode('utf-8').split('\n')

  all_comments = []
  for line in lint_results:
    # Match an output line from the linter, in this format:
    # path/to/file:line#: Lint message goes here [lint type] [confidence#]
    m = re.match(r'([^:]+):([0-9]+): *(.*[^ ]) +\[([^]]+)\] \[(\d+)\]$', line)
    if m:
      all_comments.append({
          'filename': m.group(1),
          'line': int(m.group(2)),
          'text': m.group(3),
          'type': m.group(4),
          'confidence': int(m.group(5)),
          'original_line': line})

  pr_comments = []
  for comment in all_comments:
    if comment['filename'] in valid_lines:
      if (comment['line'] in valid_lines[comment['filename']] and
          comment['confidence'] >= MINIMUM_CONFIDENCE):
        pr_comments.append(comment)
  if args.verbose:
    print('Got %d relevant lint comments' % len(pr_comments))

  # Next, get all existing review comments that we posted on the PR and delete them.
  comments_to_delete = []
  page = 1
  per_page=100
  keep_reading = True
  while keep_reading:
    if args.verbose:
      print('Read page %d of comments' % page)
    request_url = 'https://api.github.com/repos/%s/%s/pulls/%s/comments?per_page=%d&page=%d' % (repo_owner, repo_name, args.pr_number, per_page, page)
    comments = json.loads(subprocess.check_output([args.curl,
                                                   '-s', '-X', 'GET',
                                                   '-H', 'Accept: application/vnd.github.v3+json',
                                                   '-H', 'Authorization: token %s' % args.token,
                                                   request_url]).decode('utf-8').rstrip('\n'))
    for comment in comments:
      if HIDDEN_COMMENT_TAG in comment['body']:
        comments_to_delete.append(comment['id'])
    page = page + 1
    if len(comments) < per_page:
      # Stop once we're read less than a full page of comments.
      keep_reading = False
  if comments_to_delete:
    print('Delete previous lint comments:', comments_to_delete)
  for comment_id in comments_to_delete:
    # Delete all of these comments.
    # DELETE /repos/{owner}/{repo}/pulls/{pull_number}/comments
    request_url = 'https://api.github.com/repos/%s/%s/pulls/comments/%d' % (repo_owner, repo_name, comment_id)
    delete_output = subprocess.check_output([args.curl,
                                             '-s', '-X', 'DELETE',
                                             '-H', 'Accept: application/vnd.github.v3+json',
                                             '-H', 'Authorization: token %s' % args.token,
                                             request_url]).decode('utf-8').rstrip('\n')
  if len(pr_comments) > 0:
    comments_to_send = []
    for pr_comment in pr_comments:
      # Post each comment.
      # POST /repos/{owner}/{repo}/pulls/{pull_number}/comments
      request_url = 'https://api.github.com/repos/%s/%s/pulls/%s/reviews' % (repo_owner, repo_name, args.pr_number)
      comments_to_send.append({
          'body': (
              LINT_COMMENT_HEADER +
              pr_comment['text'] +
              LINT_COMMENT_FOOTER +
              HIDDEN_COMMENT_TAG +
              '<hidden value=%s></hidden>' % json.dumps(pr_comment['original_line'].replace('"','&quot;'))
                   ),
          'path': pr_comment['filename'],
          'line': pr_comment['line'],
      })
      print(pr_comment['original_line'])

    request_body = {
        'commit_id': commit_sha,
        'event': 'COMMENT',
        'comments': comments_to_send
    }
    json_text = json.dumps(request_body)
    run_output = json.loads(subprocess.check_output([args.curl,
                                                     '-s', '-X', 'POST',
                                                     '-H', 'Accept: application/vnd.github.v3+json',
                                                     '-H', 'Authorization: token %s' % args.token,
                                                     request_url, '-d', json_text]
                                                    + ([] if not args.verbose else ['-v'])).decode('utf-8').rstrip('\n'))
    if 'message' in run_output and 'errors' in run_output:
      print('%s error when posting comments:\n%s' %
            (run_output['message'], '\n'.join(run_output['errors'])))
      if args.in_github_action:
        print('::error ::%s error when posting comments:%0A%s' %
              (run_output['message'], '%0A'.join(run_output['errors'])))
      exit(1)
    else:
      print('Posted %d lint warnings successfully' % len(pr_comments))

    if args.in_github_action:
      # Also post a GitHub log comment.
      lines = ['Found %d lint warnings:' % len(pr_comments)]
      for comment in pr_comments:
        lines.append(comment['original_line'])
      print('::warning ::%s' % '%0A'.join(lines))
  else:
    print('No lint warnings found.')
    exit(0)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Run cpplint on code and add results as PR comments.')
  parser.add_argument('-t', '--token', required=True, help='GitHub access token')
  parser.add_argument('-p', '--pr_number', required=True, help='Pull request number')
  parser.add_argument('-l', '--lint_command', help='Lint command to run', default='cpplint.py')
  parser.add_argument('-r', '--repo', metavar='URL', help='GitHub repo of the pull request, default is current repo')
  parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose mode')
  parser.add_argument('-C', '--curl', default='curl', metavar='COMMAND', help='Curl command to use for making request')
  parser.add_argument('-A', '--in_github_action', action='store_true', help='Enable special logging for GitHub actions')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()
