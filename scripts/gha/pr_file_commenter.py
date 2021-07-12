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
"""Take warnings/error lines from stdin and turn them into PR file comments.

Usage:
python pr_file_commenter.py -t github_token -p pr_number
       -T comment_tag [-r git_repo_url] [-C curl_command]
       [-P comment_prefix] [-S comment_suffix] [-d base_directory]
       < COMMENT_LINES

COMMENT_LINES should be a series of lines with the following format:
path/to/first_filename:line_number: comment text
 optional comment text continuation
path/to/second_filename:line_number: comment text

This script will scan through the comments and post any that fall into the diff
range of the given PR as file comments on that PR.

If -r is unspecified, uses the current repo.
"""

import argparse
from html import escape
import json
import os
import re
import subprocess
import sys
import time
from unidiff import PatchSet
import urllib.parse

def main():
  # This script performs a number of steps:
  #
  # 1. Get the PR's diff to find the list of affected files and lines in the PR.
  #
  # 2. Get the list of comments to post. Remove duplicates, and then
  #    omit any comment that doesn't fall in the affected lines.
  #
  # 3. Delete any prior comments posted by previous runs.
  #
  # 4. Post any comments that fall within the range of the PR's diff.

  args = parse_cmdline_args()
  HIDDEN_COMMENT_TAG = '<hidden value="%s"></hidden>' % args.comment_tag
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
  # Only comment on lines that refer to parts of files that are diffed will be shown.
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
  # Get the comments from stdin.
  comment_data = sys.stdin.readlines()

  all_comments = []
  in_comment = False
  for line in comment_data:
    # Match an line in this format:
    # path/to/file:line#: Message goes here
    m = re.match(r'([^:]+):([0-9]+): *(.*)$', line)
    if m:
      in_comment = True
      relative_filename = os.path.relpath(m.group(1), args.base_directory)
      all_comments.append({
          'filename': relative_filename,
          'line': int(m.group(2)),
          'text': '`%s`' % m.group(3)})
    elif in_comment and line.startswith(' '):
      # Capture subsequent lines starting with space
      last_comment = all_comments.pop()
      last_comment['text'] += '\n`%s`' % line.rstrip('\n')
      all_comments.append(last_comment)
    else:
      # If any line begins with anything other than "path:#: " or a space,
      # we are no longer inside a comment.
      in_comment = False

  pr_comments = []
  seen_comments = set()
  for comment in all_comments:
    if ('%s:%d:%s' % (comment['filename'], comment['line'], comment['text'])
        in seen_comments):
      # Don't add any comments already present.
      continue
    else:
      seen_comments.add('%s:%d:%s' %
                           (comment['filename'], comment['line'], comment['text']))

    if comment['filename'] in valid_lines:
      if comment['line'] in valid_lines[comment['filename']]:
        pr_comments.append(comment)
      elif args.fuzzy_lines != 0:
        # Search within +/- lines for a valid line. Prefer +.
        for try_line in range(1, args.fuzzy_lines+1):
          if comment['line'] + try_line in valid_lines[comment['filename']]:
            comment['adjust'] = try_line
            pr_comments.append(comment)
            break
          elif comment['line'] -try_line in valid_lines[comment['filename']]:
            comment['adjust'] = -try_line
            pr_comments.append(comment)
            break

  if args.verbose:
    print('Got %d relevant comments' % len(pr_comments))

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
    print('Delete previous comments:', comments_to_delete)
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
      if 'adjust' in pr_comment:
        pr_comment['text'] = '`[%d line%s %s] %s' % (
            abs(pr_comment['adjust']),
            '' if abs(pr_comment['adjust']) == 1 else 's',
            'up' if pr_comment['adjust'] > 0 else 'down',
            pr_comment['text'].lstrip('`'))
        pr_comment['line'] += pr_comment['adjust']
      comments_to_send.append({
          'body': (args.comment_prefix +
                   pr_comment['text'] +
                   args.comment_suffix +
                   HIDDEN_COMMENT_TAG),
          'path': pr_comment['filename'],
          'line': pr_comment['line'],
      })

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
      exit(1)
    else:
      print('Posted %d PR file comments successfully' % len(pr_comments))

  else:
    print('No PR file comments to post.')
    exit(0)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Add log warnings/errors as PR comments.')
  parser.add_argument('-t', '--token', required=True, help='GitHub access token')
  parser.add_argument('-p', '--pr_number', required=True, help='Pull request number')
  parser.add_argument('-r', '--repo', metavar='URL', help='GitHub repo of the pull request, default is current repo')
  parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose mode')
  parser.add_argument('-C', '--curl', default='curl', metavar='COMMAND', help='Curl command to use for making request')
  parser.add_argument('-P', '--comment_prefix', default='', metavar='TEXT', help='Prefix for comment')
  parser.add_argument('-S', '--comment_suffix', default='', metavar='TEXT', help='Suffix for comment')
  parser.add_argument('-T', '--comment_tag', required=True, metavar='TAG', help='Hidden text, used to identify and delete old comments')
  parser.add_argument('-f', '--fuzzy_lines', default='0', metavar='COUNT', type=int, help='If comment lines are outside the diff, adjust them by up to this amount')
  parser.add_argument('-d', '--base_directory', default=os.curdir, metavar='DIRECTORY', help='Base directory to use for file relative paths')
  args = parser.parse_args()
  return args


if __name__ == '__main__':
  main()
