#!/usr/bin/env python3
# Copyright 2024 Google LLC
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

"""Fetches and prints command lines for failed GitHub Actions check runs for a PR."""

import argparse
import os
import sys
import requests
import json
import re
import subprocess
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry

# Constants for GitHub API interaction
RETRIES = 3
BACKOFF = 5
RETRY_STATUS = (403, 500, 502, 504)  # HTTP status codes to retry on
TIMEOUT = 10  # Default timeout for requests in seconds

# Global variables for the target repository, populated by set_repo_info()
OWNER = ''
REPO = ''
BASE_URL = 'https://api.github.com'
GITHUB_API_URL = ''


def set_repo_info(owner_name, repo_name):
  """Sets the global repository owner, name, and API URL."""
  global OWNER, REPO, GITHUB_API_URL
  OWNER = owner_name
  REPO = repo_name
  GITHUB_API_URL = f'{BASE_URL}/repos/{OWNER}/{REPO}'
  return True


def requests_retry_session(retries=RETRIES,
                           backoff_factor=BACKOFF,
                           status_forcelist=RETRY_STATUS):
  """Creates a requests session with retry logic."""
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


def get_current_branch_name():
    """Gets the current git branch name."""
    try:
        branch_bytes = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], stderr=subprocess.PIPE)
        return branch_bytes.decode().strip()
    except (subprocess.CalledProcessError, FileNotFoundError, UnicodeDecodeError) as e:
        sys.stderr.write(f"Info: Could not determine current git branch via 'git rev-parse --abbrev-ref HEAD': {e}. Branch will need to be specified.\n")
        return None
    except Exception as e: # Catch any other unexpected error.
        sys.stderr.write(f"Info: An unexpected error occurred while determining current git branch: {e}. Branch will need to be specified.\n")
        return None


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


def get_latest_pr_for_branch(token, owner, repo, branch_name):
    """Fetches the most recent open pull request for a given branch."""
    if not owner or not repo:
        sys.stderr.write("Owner and repo must be set to find PR for branch.\n")
        return None

    head_branch_spec = f"{owner}:{branch_name}" # Format required by GitHub API for head branch
    prs = list_pull_requests(token=token, state="open", head=head_branch_spec, base=None)

    if prs is None: # Error occurred in list_pull_requests
        return None
    if not prs: # No PRs found
        return None

    # Sort PRs by creation date (most recent first) to find the latest.
    try:
        prs.sort(key=lambda pr: pr.get("created_at", ""), reverse=True)
    except Exception as e:
        sys.stderr.write(f"Could not sort PRs by creation date: {e}\n")
        return None # Or handle differently, maybe return the unsorted list's first?

    if prs:
        return prs[0] # Return the full PR object
    return None


def get_check_runs_for_commit(token, commit_sha):
  """Fetches all check runs for a specific commit SHA."""
  # https://docs.github.com/en/rest/checks/runs#list-check-runs-for-a-git-reference
  url = f'{GITHUB_API_URL}/commits/{commit_sha}/check-runs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}

  page = 1
  per_page = 100 # Max allowed by GitHub API
  all_check_runs = []

  while True:
    params = {'per_page': per_page, 'page': page}
    sys.stderr.write(f"INFO: Fetching check runs page {page} for commit {commit_sha}...\n")
    try:
      with requests_retry_session().get(url, headers=headers, params=params, timeout=TIMEOUT) as response:
        response.raise_for_status()
        data = response.json()
        # The API returns an object with a `check_runs` array and a `total_count`
        current_page_check_runs = data.get('check_runs', [])
        if not current_page_check_runs:
          break
        all_check_runs.extend(current_page_check_runs)
        if len(all_check_runs) >= data.get('total_count', 0) or len(current_page_check_runs) < per_page : # Check if we have fetched all
             break
        page += 1
    except requests.exceptions.HTTPError as e:
      sys.stderr.write(f"ERROR: HTTP error fetching check runs for commit {commit_sha} (page {page}): {e}\n")
      if e.response is not None:
          try:
              error_detail = e.response.json()
              sys.stderr.write(f"Response JSON: {json.dumps(error_detail, indent=2)}\n")
          except json.JSONDecodeError:
              sys.stderr.write(f"Response Text (first 500 chars): {e.response.text[:500]}...\n")
      return None # Indicate failure
    except requests.exceptions.RequestException as e:
      sys.stderr.write(f"ERROR: Request failed while fetching check runs for commit {commit_sha} (page {page}): {e}\n")
      return None # Indicate failure
    except json.JSONDecodeError as e:
      sys.stderr.write(f"ERROR: Failed to parse JSON response for check runs (commit {commit_sha}, page {page}): {e}\n")
      return None # Indicate failure

  sys.stderr.write(f"INFO: Successfully fetched {len(all_check_runs)} check runs for commit {commit_sha}.\n")
  return all_check_runs


def main():
  """Main function to parse arguments and orchestrate the script."""
  determined_owner = None
  determined_repo = None
  try:
    git_url_bytes = subprocess.check_output(["git", "remote", "get-url", "origin"], stderr=subprocess.PIPE)
    git_url = git_url_bytes.decode().strip()
    match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+)(?:\.git)?", git_url)
    if match:
      determined_owner = match.group(1)
      determined_repo = match.group(2)
      sys.stderr.write(f"Determined repository: {determined_owner}/{determined_repo} from git remote 'origin'.\n")
  except (subprocess.CalledProcessError, FileNotFoundError, UnicodeDecodeError) as e:
    sys.stderr.write(f"Could not automatically determine repository from git remote 'origin': {e}\n")
  except Exception as e:
    sys.stderr.write(f"An unexpected error occurred while determining repository: {e}\n")

  def parse_repo_url_arg(url_string):
    """Parses owner and repository name from various GitHub URL formats."""
    url_match = re.search(r"(?:(?:https?://github\.com/)|(?:git@github\.com:))([^/]+)/([^/.]+?)(?:\.git)?/?$", url_string)
    if url_match:
        return url_match.group(1), url_match.group(2)
    return None, None

  current_branch = get_current_branch_name()

  parser = argparse.ArgumentParser(
      description="Fetches failed GitHub Actions check runs for a PR and prints scripts/print_workflow_run_errors.py commands.",
      formatter_class=argparse.RawTextHelpFormatter
  )
  parser.add_argument(
      "--token",
      type=str,
      default=os.environ.get("GITHUB_TOKEN"),
      help="GitHub token. Can also be set via GITHUB_TOKEN env var or from ~/.github_token."
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
      "--branch",
      type=str,
      default=current_branch,
      help=f"GitHub branch name to find the PR for. {'Default: ' + current_branch if current_branch else 'Required if not determinable from current git branch.'}"
  )
  parser.add_argument(
      "--pull-number",
      type=int,
      default=None,
      help="Pull request number. If provided, --branch is ignored."
  )
  # Add other arguments here in subsequent steps

  args = parser.parse_args()
  error_suffix = " (use --help for more details)"

  token = args.token
  if not token:
    try:
      with open(os.path.expanduser("~/.github_token"), "r") as f:
        token = f.read().strip()
      if token:
        sys.stderr.write("Using token from ~/.github_token\n")
    except FileNotFoundError:
      pass
    except Exception as e:
      sys.stderr.write(f"Warning: Could not read ~/.github_token: {e}\n")

  if not token:
    sys.stderr.write(f"Error: GitHub token not provided. Set GITHUB_TOKEN, use --token, or place it in ~/.github_token.{error_suffix}\n")
    sys.exit(1)
  args.token = token # Ensure args.token is populated

  final_owner = None
  final_repo = None

  if args.url:
    owner_explicitly_set_via_arg = args.owner is not None and args.owner != determined_owner
    repo_explicitly_set_via_arg = args.repo is not None and args.repo != determined_repo
    if owner_explicitly_set_via_arg or repo_explicitly_set_via_arg:
        sys.stderr.write(f"Error: Cannot use --owner or --repo when --url is specified.{error_suffix}\n")
        sys.exit(1)

    parsed_owner, parsed_repo = parse_repo_url_arg(args.url)
    if parsed_owner and parsed_repo:
        final_owner = parsed_owner
        final_repo = parsed_repo
        sys.stderr.write(f"Using repository from --url: {final_owner}/{final_repo}\n")
    else:
        sys.stderr.write(f"Error: Invalid URL format: {args.url}. Expected https://github.com/owner/repo or git@github.com:owner/repo.git{error_suffix}\n")
        sys.exit(1)
  else:
    is_owner_from_user_arg = args.owner is not None and args.owner != determined_owner
    is_repo_from_user_arg = args.repo is not None and args.repo != determined_repo

    if is_owner_from_user_arg or is_repo_from_user_arg:
        if args.owner and args.repo:
            final_owner = args.owner
            final_repo = args.repo
            sys.stderr.write(f"Using repository from --owner/--repo args: {final_owner}/{final_repo}\n")
        else:
            sys.stderr.write(f"Error: Both --owner and --repo must be specified if one is provided explicitly (and --url is not used).{error_suffix}\n")
            sys.exit(1)
    elif args.owner and args.repo: # From auto-detection or if user supplied args matching auto-detected
        final_owner = args.owner
        final_repo = args.repo

  if not final_owner or not final_repo:
    missing_parts = []
    if not final_owner: missing_parts.append("--owner")
    if not final_repo: missing_parts.append("--repo")
    error_msg = "Error: Could not determine repository."
    if missing_parts: error_msg += f" Missing { ' and '.join(missing_parts) }."
    error_msg += f" Please specify --url, OR both --owner and --repo, OR ensure git remote 'origin' is configured correctly.{error_suffix}"
    sys.stderr.write(error_msg + "\n")
    sys.exit(1)

  if not set_repo_info(final_owner, final_repo): # set global OWNER and REPO
    sys.stderr.write(f"Error: Could not set repository info to {final_owner}/{final_repo}. Ensure owner/repo are correct.{error_suffix}\n")
    sys.exit(1)

  pull_request = None
  if args.pull_number:
    sys.stderr.write(f"INFO: Fetching PR details for specified PR number: {args.pull_number}\n")
    # We need a function to get PR by number, or adapt get_latest_pr_for_branch if it can take a number
    # For now, let's assume we'll add a get_pr_by_number function later.
    # This part will be fleshed out when get_pr_by_number is added.
    url = f'{GITHUB_API_URL}/pulls/{args.pull_number}'
    headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {args.token}'}
    try:
        with requests_retry_session().get(url, headers=headers, timeout=TIMEOUT) as response:
            response.raise_for_status()
            pull_request = response.json()
            sys.stderr.write(f"Successfully fetched PR: {pull_request.get('html_url')}\n")
    except requests.exceptions.HTTPError as e:
        sys.stderr.write(f"ERROR: HTTP error fetching PR {args.pull_number}: {e}\n")
        if e.response.status_code == 404:
             sys.stderr.write(f"PR #{args.pull_number} not found in {OWNER}/{REPO}.\n")
        sys.exit(1)
    except requests.exceptions.RequestException as e:
        sys.stderr.write(f"ERROR: Request failed fetching PR {args.pull_number}: {e}\n")
        sys.exit(1)

  else:
    if not args.branch:
      sys.stderr.write(f"Error: Branch name is required if --pull-number is not specified. Please specify --branch or ensure it can be detected from your current git repository.{error_suffix}\n")
      sys.exit(1)
    sys.stderr.write(f"INFO: Attempting to find latest PR for branch: {args.branch} in {OWNER}/{REPO}...\n")
    pull_request = get_latest_pr_for_branch(args.token, OWNER, REPO, args.branch)
    if not pull_request:
      sys.stderr.write(f"INFO: No open PR found for branch '{args.branch}' in repo {OWNER}/{REPO}.\n")
      sys.exit(0) # Exit gracefully if no PR found for the branch
    sys.stderr.write(f"INFO: Found PR #{pull_request['number']} for branch '{args.branch}': {pull_request.get('html_url')}\n")

  if not pull_request:
      sys.stderr.write(f"Error: Could not determine Pull Request to process.{error_suffix}\n")
      sys.exit(1)

  # PR object is now in pull_request
   # print(f"PR Found: {pull_request.get('html_url')}. Further implementation to follow.")

  pr_head_sha = pull_request.get('head', {}).get('sha')
  if not pr_head_sha:
    sys.stderr.write(f"Error: Could not determine the head SHA for PR #{pull_request.get('number')}. Cannot fetch check runs.\n")
    sys.exit(1)

  sys.stderr.write(f"INFO: Head SHA for PR #{pull_request.get('number')} is {pr_head_sha}.\n")

  check_runs = get_check_runs_for_commit(args.token, pr_head_sha)

  if check_runs is None:
    sys.stderr.write(f"Error: Failed to fetch check runs for PR #{pull_request.get('number')} (commit {pr_head_sha}).\n")
    sys.exit(1)

  if not check_runs:
    sys.stderr.write(f"INFO: No check runs found for PR #{pull_request.get('number')} (commit {pr_head_sha}).\n")
    sys.exit(0)

  failed_check_runs = []
  for run in check_runs:
    # Possible conclusions: action_required, cancelled, failure, neutral, success, skipped, stale, timed_out
    # We are primarily interested in 'failure'. Others like 'timed_out' or 'cancelled' might also be relevant
    # depending on exact needs, but the request specifies 'failed'.
    if run.get('conclusion') == 'failure':
      failed_check_runs.append(run)
      sys.stderr.write(f"INFO: Identified failed check run: '{run.get('name')}' (ID: {run.get('id')}, Status: {run.get('status')}, Conclusion: {run.get('conclusion')})\n")

  if not failed_check_runs:
    sys.stderr.write(f"INFO: No failed check runs found for PR #{pull_request.get('number')} (commit {pr_head_sha}).\n")
    # Check if there were any non-successful runs at all to provide more context
    non_successful_conclusions = [r.get('conclusion') for r in check_runs if r.get('conclusion') not in ['success', 'neutral', 'skipped']]
    if non_successful_conclusions:
        sys.stderr.write(f"INFO: Other non-successful conclusions found: {list(set(non_successful_conclusions))}\n")
    else:
        sys.stderr.write(f"INFO: All check runs completed successfully or were neutral/skipped.\n")
    sys.exit(0)

  print(f"\n# Commands to get logs for {len(failed_check_runs)} failed check run(s) for PR #{pull_request.get('number')} (commit {pr_head_sha}):\n")
  for run in failed_check_runs:
    # The 'id' of a check run is the correct `run_id` for `print_workflow_run_errors.py`
    # when that script is used to fetch logs for a specific check run (job).
    # The print_workflow_run_errors.py script uses job ID as run_id when fetching specific job logs.
    # A "check run" in the context of the Checks API often corresponds to a "job" in GitHub Actions workflows.
    check_run_id = run.get('id')
    check_run_name = run.get('name')

    # Construct the command
    # We use final_owner and final_repo which were resolved from args or git remote
    command = [
        "scripts/print_workflow_run_errors.py",
        "--owner", final_owner,
        "--repo", final_repo,
        "--token", "\"<YOUR_GITHUB_TOKEN_OR_USE_ENV_VAR>\"", # Using a placeholder
        "--run-id", str(check_run_id)
    ]

    # Add some optional parameters if they are set in the current script's args,
    # assuming print_workflow_run_errors.py supports them or similar ones.
    # This part is speculative based on common args in print_workflow_run_errors.py.
    # You might need to adjust these based on actual print_workflow_run_errors.py capabilities.
    # For now, we'll keep it simple and only pass the essentials.
    # if args.grep_pattern:
    #   command.extend(["--grep-pattern", args.grep_pattern])
    # if args.log_lines:
    #   command.extend(["--log-lines", str(args.log_lines)])

    print(f"# For failed check run: '{check_run_name}' (ID: {check_run_id})")
    print(" \\\n  ".join(command))
    print("\n")

  sys.stderr.write(f"\nINFO: Printed {len(failed_check_runs)} command(s) to fetch logs for failed check runs.\n")


if __name__ == "__main__":
  main()
