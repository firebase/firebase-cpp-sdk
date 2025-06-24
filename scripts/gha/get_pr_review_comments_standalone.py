#!/usr/bin/env python3
# Copyright 2025 Google LLC
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

"""Fetches and formats review comments from a GitHub Pull Request."""

import argparse
import os
import sys
import datetime
from datetime import timezone, timedelta
import requests
import json
import re
import subprocess
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry
from absl import logging

# Constants from firebase_github.py
RETRIES = 3
BACKOFF = 5
RETRY_STATUS = (403, 500, 502, 504)
TIMEOUT = 5
TIMEOUT_LONG = 20 # Not used in the functions we are copying, but good to have if expanding.

OWNER = '' # Will be determined dynamically or from args
REPO = '' # Will be determined dynamically or from args
BASE_URL = 'https://api.github.com'
GITHUB_API_URL = '' # Will be set by set_repo_url_standalone

logging.set_verbosity(logging.INFO)


def set_repo_url_standalone(owner_name, repo_name):
  global OWNER, REPO, GITHUB_API_URL
  OWNER = owner_name
  REPO = repo_name
  GITHUB_API_URL = '%s/repos/%s/%s' % (BASE_URL, OWNER, REPO)
  return True


def requests_retry_session(retries=RETRIES,
                           backoff_factor=BACKOFF,
                           status_forcelist=RETRY_STATUS):
    session = requests.Session()
    retry = Retry(total=retries,
                  read=retries,
                  connect=retries,
                  backoff_factor=backoff_factor,
                  status_forcelist=status_forcelist)
    adapter = HTTPAdapter(max_retries=retry)
    session.mount('http://', adapter)
    session.mount('https://', adapter)
    return session


def get_pull_request_review_comments(token, pull_number, since=None):
  """https://docs.github.com/en/rest/pulls/comments#list-review-comments-on-a-pull-request"""
  url = f'{GITHUB_API_URL}/pulls/{pull_number}/comments'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}

  page = 1
  per_page = 100
  results = []

  # Base parameters for the API request
  base_params = {'per_page': per_page}
  if since:
    base_params['since'] = since

  while True: # Loop indefinitely until explicitly broken
    current_page_params = base_params.copy()
    current_page_params['page'] = page

    try:
      with requests_retry_session().get(url, headers=headers, params=current_page_params,
                        stream=True, timeout=TIMEOUT) as response:
        response.raise_for_status()
        # Log which page and if 'since' was used for clarity
        logging.info("get_pull_request_review_comments: %s params %s response: %s", url, current_page_params, response)

        current_page_results = response.json()
        if not current_page_results: # No more results on this page
            break # Exit loop, no more comments to fetch

        results.extend(current_page_results)

        # If fewer results than per_page were returned, it's the last page
        if len(current_page_results) < per_page:
            break # Exit loop, this was the last page

        page += 1 # Increment page for the next iteration

    except requests.exceptions.RequestException as e:
      logging.error(f"Error fetching review comments (page {page}, params: {current_page_params}) for PR {pull_number}: {e}")
      break # Stop trying if there's an error
  return results


def main():
    STATUS_IRRELEVANT = "[IRRELEVANT]"
    STATUS_OLD = "[OLD]"
    STATUS_CURRENT = "[CURRENT]"

    determined_owner = None
    determined_repo = None
    try:
        git_url_bytes = subprocess.check_output(["git", "remote", "get-url", "origin"], stderr=subprocess.PIPE)
        git_url = git_url_bytes.decode().strip()
        # Regex for https://github.com/owner/repo.git or git@github.com:owner/repo.git
        match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+)(?:\.git)?", git_url)
        if match:
            determined_owner = match.group(1)
            determined_repo = match.group(2)
            sys.stderr.write(f"Determined repository: {determined_owner}/{determined_repo} from git remote.\n")
    except (subprocess.CalledProcessError, FileNotFoundError, UnicodeDecodeError) as e:
        sys.stderr.write(f"Could not automatically determine repository from git remote: {e}\n")
    except Exception as e: # Catch any other unexpected error during git processing
        sys.stderr.write(f"An unexpected error occurred while determining repository: {e}\n")

    # Helper function to parse owner/repo from URL
    def parse_repo_url(url_string):
        # Regex for https://github.com/owner/repo.git or git@github.com:owner/repo.git
        # Also handles URLs without .git suffix
        url_match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+?)(?:\.git)?/?$", url_string)
        if url_match:
            return url_match.group(1), url_match.group(2)
        return None, None

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format into simple text output.\n"
                    "Repository can be specified via --url, or --owner AND --repo, or auto-detected from git remote 'origin'.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument( # This is always required
        "--pull_number",
        type=int,
        required=True,
        help="Pull request number."
    )
    repo_spec_group = parser.add_mutually_exclusive_group()
    repo_spec_group.add_argument(
        "--url",
        type=str,
        default=None,
        help="Full GitHub repository URL (e.g., https://github.com/owner/repo or git@github.com:owner/repo.git). Overrides --owner/--repo and git detection."
    )
    # Create a sub-group for owner/repo pair if not using URL
    owner_repo_group = repo_spec_group.add_argument_group('owner_repo_pair', 'Specify owner and repository name (used if --url is not provided)')
    owner_repo_group.add_argument(
        "--owner",
        type=str,
        default=determined_owner,
        help=f"Repository owner. {'Default: ' + determined_owner if determined_owner else 'Required if --url is not used and not determinable from git.'}"
    )
    owner_repo_group.add_argument(
        "--repo",
        type=str,
        default=determined_repo,
        help=f"Repository name. {'Default: ' + determined_repo if determined_repo else 'Required if --url is not used and not determinable from git.'}"
    )
    parser.add_argument(
        "--token",
        type=str,
        default=os.environ.get("GITHUB_TOKEN"),
        help="GitHub token. Can also be set via GITHUB_TOKEN env var."
    )
    parser.add_argument(
        "--context-lines",
        type=int,
        default=10,
        help="Number of context lines from the diff hunk. 0 for full hunk. If > 0, shows header (if any) and last N lines of the remaining hunk. Default: 10."
    )
    parser.add_argument(
        "--since",
        type=str,
        default=None,
        help="Only show comments updated at or after this ISO 8601 timestamp (e.g., YYYY-MM-DDTHH:MM:SSZ)."
    )
    parser.add_argument(
        "--exclude-old",
        action="store_true",
        default=False,
        help="Exclude comments marked [OLD] (where line number has changed due to code updates but position is still valid)."
    )
    parser.add_argument(
        "--include-irrelevant",
        action="store_true",
        default=False,
        help="Include comments marked [IRRELEVANT] (where GitHub can no longer anchor the comment to the diff, i.e., position is null)."
    )

    args = parser.parse_args()

    if not args.token:
        sys.stderr.write("Error: GitHub token not provided. Set GITHUB_TOKEN or use --token.\n")
        sys.exit(1)

    final_owner = None
    final_repo = None

    if args.url:
        parsed_owner, parsed_repo = parse_repo_url(args.url)
        if parsed_owner and parsed_repo:
            final_owner = parsed_owner
            final_repo = parsed_repo
            sys.stderr.write(f"Using repository from --url: {final_owner}/{final_repo}\n")
        else:
            sys.stderr.write(f"Error: Invalid URL format provided: {args.url}. Expected https://github.com/owner/repo or git@github.com:owner/repo.git\n")
            parser.print_help()
            sys.exit(1)
    # If URL is not provided, check owner/repo. They default to determined_owner/repo.
    elif args.owner and args.repo:
        final_owner = args.owner
        final_repo = args.repo
        # If these values are different from the auto-detected ones (i.e., user explicitly provided them),
        # or if auto-detection failed and these are the only source.
        if (args.owner != determined_owner or args.repo != determined_repo) and (determined_owner or determined_repo):
             sys.stderr.write(f"Using repository from --owner/--repo args: {final_owner}/{final_repo}\n")
        # If auto-detection worked and user didn't override, the initial "Determined repository..." message is sufficient.
    elif args.owner or args.repo: # Only one of owner/repo was specified (and not --url)
        sys.stderr.write("Error: Both --owner and --repo must be specified if one is provided and --url is not used.\n")
        parser.print_help()
        sys.exit(1)
    # If --url, --owner, --repo are all None, it means auto-detection failed AND user provided nothing.
    # This case is caught by the final check below.


    if not final_owner or not final_repo:
        sys.stderr.write("Error: Could not determine repository. Please specify --url, or both --owner and --repo, or ensure git remote 'origin' is configured correctly.\n")
        parser.print_help()
        sys.exit(1)

    if not set_repo_url_standalone(final_owner, final_repo):
        sys.stderr.write(f"Error: Could not set repository to {final_owner}/{final_repo}. Ensure owner/repo are correct.\n")
        sys.exit(1)

    sys.stderr.write(f"Fetching comments for PR #{args.pull_number} from {OWNER}/{REPO}...\n")
    if args.since:
        sys.stderr.write(f"Filtering comments updated since: {args.since}\n")

    comments = get_pull_request_review_comments(
        args.token,
        args.pull_number,
        since=args.since
    )

    if not comments:
        sys.stderr.write(f"No review comments found for PR #{args.pull_number} (or matching filters), or an error occurred.\n")
        return

    latest_activity_timestamp_obj = None
    processed_comments_count = 0
    print("# Review Comments\n\n")
    for comment in comments:
        created_at_str = comment.get("created_at")

        current_pos = comment.get("position")
        current_line = comment.get("line")
        original_line = comment.get("original_line")

        status_text = ""
        line_to_display = None

        if current_pos is None:
            status_text = STATUS_IRRELEVANT
            line_to_display = original_line
        elif original_line is not None and current_line != original_line:
            status_text = STATUS_OLD
            line_to_display = current_line
        else:
            status_text = STATUS_CURRENT
            line_to_display = current_line

        if line_to_display is None:
            line_to_display = "N/A"

        if status_text == STATUS_IRRELEVANT and not args.include_irrelevant:
            continue
        if status_text == STATUS_OLD and args.exclude_old:
            continue

        # Track latest 'updated_at' for '--since' suggestion; 'created_at' is for display.
        updated_at_str = comment.get("updated_at")
        if updated_at_str: # Check if updated_at_str is not None and not empty
            try:
                if sys.version_info < (3, 11):
                    dt_str_updated = updated_at_str.replace("Z", "+00:00")
                else:
                    dt_str_updated = updated_at_str
                current_comment_activity_dt = datetime.datetime.fromisoformat(dt_str_updated)
                if latest_activity_timestamp_obj is None or current_comment_activity_dt > latest_activity_timestamp_obj:
                    latest_activity_timestamp_obj = current_comment_activity_dt
            except ValueError:
                sys.stderr.write(f"Warning: Could not parse updated_at timestamp: {updated_at_str}\n")

        # Get other comment details
        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")
        body = comment.get("body", "").strip()

        if not body:
            continue

        processed_comments_count += 1

        diff_hunk = comment.get("diff_hunk")
        html_url = comment.get("html_url", "N/A")
        comment_id = comment.get("id")
        in_reply_to_id = comment.get("in_reply_to_id")

        print(f"## Comment by: **{user}** (ID: `{comment_id}`){f' (In Reply To: `{in_reply_to_id}`)' if in_reply_to_id else ''}\n")
        if created_at_str:
            print(f"*   **Timestamp**: `{created_at_str}`")
        print(f"*   **Status**: `{status_text}`")
        print(f"*   **File**: `{path}`")
        print(f"*   **Line**: `{line_to_display}`")
        print(f"*   **URL**: <{html_url}>\n")

        print("\n### Context:")
        print("```") # Start of Markdown code block
        if diff_hunk and diff_hunk.strip():
            if args.context_lines == 0: # User wants the full hunk
                print(diff_hunk)
            else: # User wants N lines of context (args.context_lines > 0)
                hunk_lines = diff_hunk.split('\n')
                if hunk_lines and hunk_lines[0].startswith("@@ "):
                    print(hunk_lines[0])
                    hunk_lines = hunk_lines[1:] # Modify list in place for remaining operations

                # Proceed with the (potentially modified) hunk_lines
                # If hunk_lines is empty here (e.g. original hunk was only a header that was removed),
                # hunk_lines[-args.context_lines:] will be [], and "\n".join([]) is "",
                # so print("") will effectively print a newline. This is acceptable.
                print("\n".join(hunk_lines[-args.context_lines:]))
        else: # diff_hunk was None or empty
            print("(No diff hunk available for this comment)")
        print("```") # End of Markdown code block

        print("\n### Comment:")
        print(body)
        print("\n---")

    sys.stderr.write(f"\nPrinted {processed_comments_count} comments to stdout.\n")

    if latest_activity_timestamp_obj:
        try:
            # Ensure it's UTC before adding timedelta, then format
            next_since_dt = latest_activity_timestamp_obj.astimezone(timezone.utc) + timedelta(seconds=2)
            next_since_str = next_since_dt.strftime('%Y-%m-%dT%H:%M:%SZ')

            new_cmd_args = [sys.executable, sys.argv[0]] # Start with interpreter and script path
            i = 1 # Start checking from actual arguments in sys.argv
            while i < len(sys.argv):
                if sys.argv[i] == "--since":
                    i += 2 # Skip --since and its value
                    continue
                new_cmd_args.append(sys.argv[i])
                i += 1

            new_cmd_args.extend(["--since", next_since_str])
            suggested_cmd = " ".join(new_cmd_args)
            sys.stderr.write(f"\nTo get comments created after the last one in this batch, try:\n{suggested_cmd}\n")
        except Exception as e:
            sys.stderr.write(f"\nWarning: Could not generate next command suggestion: {e}\n")

if __name__ == "__main__":
    main()
