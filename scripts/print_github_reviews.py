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
# Constants for GitHub API interaction
RETRIES = 3
BACKOFF = 5
RETRY_STATUS = (403, 500, 502, 504) # HTTP status codes to retry on
TIMEOUT = 5 # Default timeout for requests in seconds
TIMEOUT_LONG = 20 # Longer timeout, currently not used by functions in this script

# Global variables for the target repository, populated by set_repo_url_standalone()
OWNER = ''
REPO = ''
BASE_URL = 'https://api.github.com'
GITHUB_API_URL = ''


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
  per_page = 100 # GitHub API default and max is 100 for many paginated endpoints
  results = []

  base_params = {'per_page': per_page}
  if since:
    base_params['since'] = since

  while True:
    current_page_params = base_params.copy()
    current_page_params['page'] = page

    try:
      with requests_retry_session().get(url, headers=headers, params=current_page_params,
                        stream=True, timeout=TIMEOUT) as response:
        response.raise_for_status()

        current_page_results = response.json()
        if not current_page_results: # No more data
            break

        results.extend(current_page_results)

        if len(current_page_results) < per_page: # Reached last page
            break

        page += 1

    except requests.exceptions.RequestException as e:
      sys.stderr.write(f"Error: Failed to fetch review comments (page {page}, params: {current_page_params}) for PR {pull_number}: {e}\n")
      return None
  return results


def list_pull_requests(token, state, head, base):
  """https://docs.github.com/en/rest/reference/pulls#list-pull-requests"""
  url = f'{GITHUB_API_URL}/pulls'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  page = 1
  per_page = 100
  results = []
  keep_going = True
  while keep_going:
    params = {'per_page': per_page, 'page': page}
    if state: params.update({'state': state})
    if head: params.update({'head': head})
    if base: params.update({'base': base})
    page = page + 1
    keep_going = False
    try:
      with requests_retry_session().get(url, headers=headers, params=params,
                        stream=True, timeout=TIMEOUT) as response:
        response.raise_for_status()
        current_page_results = response.json()
        if not current_page_results:
            break
        results.extend(current_page_results)
        keep_going = (len(current_page_results) == per_page)
    except requests.exceptions.RequestException as e:
      sys.stderr.write(f"Error: Failed to list pull requests (page {params.get('page', 'N/A')}, params: {params}) for {OWNER}/{REPO}: {e}\n")
      return None
  return results


def get_pull_request_reviews(token, owner, repo, pull_number):
    """Fetches all reviews for a given pull request."""
    # Note: GitHub API for listing reviews does not support a 'since' parameter directly.
    # Filtering by 'since' must be done client-side after fetching all reviews.
    url = f'{GITHUB_API_URL}/pulls/{pull_number}/reviews'
    headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
    page = 1
    per_page = 100
    results = []
    keep_going = True
    while keep_going:
        params = {'per_page': per_page, 'page': page}
        page = page + 1
        keep_going = False
        try:
            with requests_retry_session().get(url, headers=headers, params=params,
                                stream=True, timeout=TIMEOUT) as response:
                response.raise_for_status()
                current_page_results = response.json()
                if not current_page_results:
                    break
                results.extend(current_page_results)
                keep_going = (len(current_page_results) == per_page)
        except requests.exceptions.RequestException as e:
            sys.stderr.write(f"Error: Failed to list pull request reviews (page {params.get('page', 'N/A')}, params: {params}) for PR {pull_number} in {owner}/{repo}: {e}\n")
            return None
    return results


def get_current_branch_name():
    """Gets the current git branch name."""
    try:
        branch_bytes = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], stderr=subprocess.PIPE)
        return branch_bytes.decode().strip()
    except (subprocess.CalledProcessError, FileNotFoundError, UnicodeDecodeError) as e:
        sys.stderr.write(f"Could not determine current git branch: {e}\n")
        return None

def get_latest_pr_for_branch(token, owner, repo, branch_name):
    """Fetches the most recent open pull request for a given branch."""
    if not owner or not repo:
        sys.stderr.write("Owner and repo must be set to find PR for branch.\n")
        return None

    head_branch_spec = f"{owner}:{branch_name}" # Format required by GitHub API for head branch
    prs = list_pull_requests(token=token, state="open", head=head_branch_spec, base=None)

    if not prs:
        return None

    # Sort PRs by creation date (most recent first) to find the latest.
    try:
        prs.sort(key=lambda pr: pr.get("created_at", ""), reverse=True)
    except Exception as e: # Broad exception for safety, though sort issues are rare with valid data.
        sys.stderr.write(f"Could not sort PRs by creation date: {e}\n")
        return None

    if prs:
        return prs[0].get("number")
    return None


def main():
    STATUS_IRRELEVANT = "[IRRELEVANT]"
    STATUS_OLD = "[OLD]"
    STATUS_CURRENT = "[CURRENT]"

    determined_owner = None
    determined_repo = None
    try:
        git_url_bytes = subprocess.check_output(["git", "remote", "get-url", "origin"], stderr=subprocess.PIPE)
        git_url = git_url_bytes.decode().strip()
        match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+)(?:\.git)?", git_url)
        if match:
            determined_owner = match.group(1)
            determined_repo = match.group(2)
            sys.stderr.write(f"Determined repository: {determined_owner}/{determined_repo} from git remote.\n")
    except (subprocess.CalledProcessError, FileNotFoundError, UnicodeDecodeError) as e:
        sys.stderr.write(f"Could not automatically determine repository from git remote: {e}\n")
    except Exception as e: # Catch any other unexpected error.
        sys.stderr.write(f"An unexpected error occurred while determining repository: {e}\n")

    def parse_repo_url(url_string):
        """Parses owner and repository name from various GitHub URL formats."""
        url_match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+?)(?:\.git)?/?$", url_string)
        if url_match:
            return url_match.group(1), url_match.group(2)
        return None, None

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format into simple text output.\n"
                    "Repository can be specified via --url, or --owner AND --repo, or auto-detected from git remote 'origin'.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "--pull_number",
        type=int,
        default=None,
        help="Pull request number. If not provided, script attempts to find the latest open PR for the current git branch."
    )
    parser.add_argument(
        "--url",
        type=str,
        default=None,
        help="Full GitHub repository URL (e.g., https://github.com/owner/repo or git@github.com:owner/repo.git). Takes precedence over --owner/--repo."
    )
    parser.add_argument(
        "--owner",
        type=str,
        default=determined_owner,
        help=f"Repository owner. Used if --url is not provided. {'Default: ' + determined_owner if determined_owner else 'Required if --url is not used and not determinable from git.'}"
    )
    parser.add_argument(
        "--repo",
        type=str,
        default=determined_repo,
        help=f"Repository name. Used if --url is not provided. {'Default: ' + determined_repo if determined_repo else 'Required if --url is not used and not determinable from git.'}"
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
    error_suffix = " (use --help for more details)"

    if not args.token:
        sys.stderr.write(f"Error: GitHub token not provided. Set GITHUB_TOKEN or use --token.{error_suffix}\n")
        sys.exit(1)

    final_owner = None
    final_repo = None

    # Determine repository owner and name
    if args.url:
        owner_explicitly_set = args.owner is not None and args.owner != determined_owner
        repo_explicitly_set = args.repo is not None and args.repo != determined_repo
        if owner_explicitly_set or repo_explicitly_set:
            sys.stderr.write(f"Error: Cannot use --owner or --repo when --url is specified.{error_suffix}\n")
            sys.exit(1)

        parsed_owner, parsed_repo = parse_repo_url(args.url)
        if parsed_owner and parsed_repo:
            final_owner = parsed_owner
            final_repo = parsed_repo
            sys.stderr.write(f"Using repository from --url: {final_owner}/{final_repo}\n")
        else:
            sys.stderr.write(f"Error: Invalid URL format: {args.url}. Expected https://github.com/owner/repo or git@github.com:owner/repo.git{error_suffix}\n")
            sys.exit(1)
    else:
        is_owner_from_user = args.owner is not None and args.owner != determined_owner
        is_repo_from_user = args.repo is not None and args.repo != determined_repo

        if (is_owner_from_user or is_repo_from_user): # User explicitly set at least one of owner/repo
            if args.owner and args.repo:
                final_owner = args.owner
                final_repo = args.repo
                sys.stderr.write(f"Using repository from --owner/--repo args: {final_owner}/{final_repo}\n")
            else:
                 sys.stderr.write(f"Error: Both --owner and --repo must be specified if one is provided explicitly (and --url is not used).{error_suffix}\n")
                 sys.exit(1)
        elif args.owner and args.repo: # Both args have values, from successful auto-detection
            final_owner = args.owner
            final_repo = args.repo
        elif args.owner or args.repo: # Only one has a value from auto-detection (e.g. git remote parsing failed partially)
            sys.stderr.write(f"Error: Both --owner and --repo are required if not using --url, and auto-detection was incomplete.{error_suffix}\n")
            sys.exit(1)
        # If final_owner/repo are still None here, it means auto-detection failed AND user provided nothing.

    if not final_owner or not final_repo:
        sys.stderr.write(f"Error: Could not determine repository. Please specify --url, OR both --owner and --repo, OR ensure git remote 'origin' is configured correctly.{error_suffix}\n")
        sys.exit(1)

    if not set_repo_url_standalone(final_owner, final_repo):
        sys.stderr.write(f"Error: Could not set repository to {final_owner}/{final_repo}. Ensure owner/repo are correct.{error_suffix}\n")
        sys.exit(1)

    pull_request_number = args.pull_number
    current_branch_for_pr_check = None # Store branch name if auto-detecting PR
    if not pull_request_number:
        sys.stderr.write("Pull number not specified, attempting to find PR for current branch...\n")
        current_branch_for_pr_check = get_current_branch_name()
        if current_branch_for_pr_check:
            sys.stderr.write(f"Current git branch is: {current_branch_for_pr_check}\n")
            pull_request_number = get_latest_pr_for_branch(args.token, OWNER, REPO, current_branch_for_pr_check)
            if pull_request_number:
                sys.stderr.write(f"Found PR #{pull_request_number} for branch {current_branch_for_pr_check}.\n")
            else:
                sys.stderr.write(f"No open PR found for branch {current_branch_for_pr_check} in {OWNER}/{REPO}.\n") # Informational
        else:
            sys.stderr.write(f"Error: Could not determine current git branch. Cannot find PR automatically.{error_suffix}\n")
            sys.exit(1)

    if not pull_request_number: # Final check for PR number
        error_message = "Error: Pull request number is required."
        if not args.pull_number and current_branch_for_pr_check: # Auto-detect branch ok, but no PR found
            error_message = f"Error: Pull request number not specified and no open PR found for branch {current_branch_for_pr_check}."
        # The case where current_branch_for_pr_check is None (git branch fail) is caught and exited above.
        sys.stderr.write(f"{error_message}{error_suffix}\n")
        sys.exit(1)

    sys.stderr.write(f"Fetching overall reviews for PR #{pull_request_number} from {OWNER}/{REPO}...\n")
    overall_reviews = get_pull_request_reviews(args.token, OWNER, REPO, pull_request_number)

    if overall_reviews is None:
        sys.stderr.write(f"Error: Failed to fetch overall reviews due to an API or network issue.{error_suffix}\nPlease check logs for details.\n")
        sys.exit(1)

    filtered_overall_reviews = []
    if overall_reviews: # If not None and not empty
        for review in overall_reviews:
            if review.get("state") == "DISMISSED":
                continue

            if args.since:
                submitted_at_str = review.get("submitted_at")
                if submitted_at_str:
                    try:
                        # Compatibility for Python < 3.11
                        if sys.version_info < (3, 11):
                            dt_str_submitted = submitted_at_str.replace("Z", "+00:00")
                        else:
                            dt_str_submitted = submitted_at_str
                        submitted_dt = datetime.datetime.fromisoformat(dt_str_submitted)

                        since_dt_str = args.since
                        if sys.version_info < (3, 11) and args.since.endswith("Z"):
                             since_dt_str = args.since.replace("Z", "+00:00")
                        since_dt = datetime.datetime.fromisoformat(since_dt_str)

                        # Ensure 'since_dt' is timezone-aware if 'submitted_dt' is.
                        # GitHub timestamps are UTC. fromisoformat on Z or +00:00 makes them aware.
                        if submitted_dt.tzinfo and not since_dt.tzinfo:
                             since_dt = since_dt.replace(tzinfo=timezone.utc) # Assume since is UTC if not specified

                        if submitted_dt < since_dt:
                            continue
                    except ValueError as ve:
                        sys.stderr.write(f"Warning: Could not parse review submitted_at timestamp '{submitted_at_str}' or --since timestamp '{args.since}': {ve}\n")
                        # If parsing fails, we might choose to include the review to be safe, or skip. Current: include.

            # New filter: Exclude "COMMENTED" reviews with an empty body
            if review.get("state") == "COMMENTED" and not review.get("body", "").strip():
                continue

            filtered_overall_reviews.append(review)

        # Sort by submission time, oldest first
        try:
            filtered_overall_reviews.sort(key=lambda r: r.get("submitted_at", ""))
        except Exception as e: # Broad exception for safety
             sys.stderr.write(f"Warning: Could not sort overall reviews: {e}\n")

    # Output overall reviews if any exist after filtering
    if filtered_overall_reviews:
        print("# Code Reviews\n\n") # Changed heading
        # Explicitly re-initialize before this loop to be absolutely sure for the update logic within it.
        # The main initialization at the top of main() handles the case where this block is skipped.
        temp_latest_overall_review_dt = None # Use a temporary variable for this loop's accumulation
        for review in filtered_overall_reviews:
            user = review.get("user", {}).get("login", "Unknown user")
            submitted_at_str = review.get("submitted_at", "N/A") # Keep original string for printing
            state = review.get("state", "N/A")
            body = review.get("body", "").strip()

            # Track latest overall review timestamp within this block
            if submitted_at_str and submitted_at_str != "N/A":
                try:
                    if sys.version_info < (3, 11):
                        dt_str_submitted = submitted_at_str.replace("Z", "+00:00")
                    else:
                        dt_str_submitted = submitted_at_str
                    current_review_submitted_dt = datetime.datetime.fromisoformat(dt_str_submitted)
                    if temp_latest_overall_review_dt is None or current_review_submitted_dt > temp_latest_overall_review_dt:
                        temp_latest_overall_review_dt = current_review_submitted_dt
                except ValueError:
                    sys.stderr.write(f"Warning: Could not parse overall review submitted_at for --since suggestion: {submitted_at_str}\n")

            html_url = review.get("html_url", "N/A")
            review_id = review.get("id", "N/A")

            print(f"## Review by: **{user}** (ID: `{review_id}`)\n")
            print(f"*   **Submitted At**: `{submitted_at_str}`") # Print original string
            print(f"*   **State**: `{state}`")
            print(f"*   **URL**: <{html_url}>\n")
            # Removed duplicated lines here

            if body:
                print("\n### Summary Comment:")
                print(body)
            print("\n---")

        # After processing all overall reviews in this block, update the main variable
        if temp_latest_overall_review_dt:
            latest_overall_review_activity_dt = temp_latest_overall_review_dt

        # Add an extra newline to separate from line comments if any overall reviews were printed
        print("\n")


    sys.stderr.write(f"Fetching line comments for PR #{pull_request_number} from {OWNER}/{REPO}...\n")
    if args.since:
        sys.stderr.write(f"Filtering line comments updated since: {args.since}\n") # Clarify this 'since' is for line comments

    comments = get_pull_request_review_comments(
        args.token,
        pull_request_number,
        since=args.since # This 'since' applies to line comment's 'updated_at'
    )

    if comments is None:
        sys.stderr.write(f"Error: Failed to fetch line comments due to an API or network issue.{error_suffix}\nPlease check logs for details.\n")
        sys.exit(1)
    # Note: The decision to exit if only line comments fail vs. if only overall reviews fail could be nuanced.
    # For now, failure to fetch either is treated as a critical error for the script's purpose.

    # Initialize tracking variables early - MOVED TO TOP OF MAIN
    # latest_overall_review_activity_dt = None
    # latest_line_comment_activity_dt = None
    # processed_comments_count = 0 # This is specifically for line comments

    # Handling for line comments
    if not comments: # comments is an empty list here (None case handled above)
        sys.stderr.write(f"No line comments found for PR #{pull_request_number} (or matching filters).\n")
        # If there were also no overall reviews, and no line comments, then nothing to show.
        # The 'next command' suggestion logic below will still run if overall_reviews had content.
        if not filtered_overall_reviews : # and not comments (implicitly true here)
             # Only return (and skip 'next command' suggestion) if NO content at all was printed.
             # If overall_reviews were printed, we still want the 'next command' suggestion.
             pass # Let it fall through to the 'next command' suggestion logic
    else:
        print("# Review Comments\n\n")

    for comment in comments: # if comments is empty, this loop is skipped.
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

        updated_at_str = comment.get("updated_at")
        if updated_at_str:
            try:
                # Compatibility for Python < 3.11 which doesn't handle 'Z' suffix in fromisoformat
                if sys.version_info < (3, 11):
                    dt_str_updated = updated_at_str.replace("Z", "+00:00")
                else:
                    dt_str_updated = updated_at_str
                current_comment_activity_dt = datetime.datetime.fromisoformat(dt_str_updated)
                if latest_line_comment_activity_dt is None or current_comment_activity_dt > latest_line_comment_activity_dt: # Corrected variable name
                    latest_line_comment_activity_dt = current_comment_activity_dt # Corrected variable name
            except ValueError:
                sys.stderr.write(f"Warning: Could not parse line comment updated_at for --since suggestion: {updated_at_str}\n")

        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")
        body = comment.get("body", "").strip()

        if not body: # Skip comments with no actual text content
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
        print("```")
        if diff_hunk and diff_hunk.strip():
            if args.context_lines == 0: # 0 means full hunk
                print(diff_hunk)
            else: # Display header (if any) and last N lines
                hunk_lines = diff_hunk.split('\n')
                if hunk_lines and hunk_lines[0].startswith("@@ "): # Diff hunk header
                    print(hunk_lines[0])
                    hunk_lines = hunk_lines[1:]
                print("\n".join(hunk_lines[-args.context_lines:]))
        else:
            print("(No diff hunk available for this comment)")
        print("```")

        print("\n### Comment:")
        print(body)
        print("\n---")

    sys.stderr.write(f"\nPrinted {processed_comments_count} comments to stdout.\n")

    # Determine the overall latest activity timestamp
    overall_latest_activity_dt = None
    if latest_overall_review_activity_dt and latest_line_comment_activity_dt:
        overall_latest_activity_dt = max(latest_overall_review_activity_dt, latest_line_comment_activity_dt)
    elif latest_overall_review_activity_dt:
        overall_latest_activity_dt = latest_overall_review_activity_dt
    elif latest_line_comment_activity_dt:
        overall_latest_activity_dt = latest_line_comment_activity_dt

    if overall_latest_activity_dt:
        try:
            # Suggest next command with '--since' pointing to just after the latest activity.
            next_since_dt = overall_latest_activity_dt.astimezone(timezone.utc) + timedelta(seconds=2)
            next_since_str = next_since_dt.strftime('%Y-%m-%dT%H:%M:%SZ')

            new_cmd_args = [sys.executable, sys.argv[0]]
            i = 1
            while i < len(sys.argv):
                if sys.argv[i] == "--since": # Skip existing --since argument and its value
                    i += 2
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
