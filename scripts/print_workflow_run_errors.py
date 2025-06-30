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

"""Fetches and prints errors from a GitHub Workflow run."""

import argparse
import os
import sys
import datetime
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
LONG_TIMEOUT = 30 # Timeout for potentially longer requests like log downloads

# Global variables for the target repository, populated by set_repo_info()
OWNER = ''
REPO = ''
BASE_URL = 'https://api.github.com'
GITHUB_API_URL = ''

DEFAULT_JOB_PATTERNS = ['^build.*', '^test.*', '.*']

# Regex to match ISO 8601 timestamps like "2023-10-27T18:30:59.1234567Z " or "2023-10-27T18:30:59Z "
TIMESTAMP_REGEX = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?Z\s*")

def strip_initial_timestamp(line: str) -> str:
    """Removes an ISO 8601-like timestamp from the beginning of a line if present."""
    return TIMESTAMP_REGEX.sub("", line)


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

def _process_and_display_logs_for_failed_jobs(args, list_of_failed_jobs, workflow_run_html_url, current_pattern_str):
  """
  Helper function to process a list of already identified failed jobs for a specific pattern.
  It handles fetching logs, stripping timestamps, grepping, and printing Markdown output.
  """
  print(f"\n# Detailed Logs for Failed Jobs (matching pattern '{current_pattern_str}') for Workflow Run ([Run Link]({workflow_run_html_url}))\n")

  total_failed_jobs_to_process = len(list_of_failed_jobs)
  successful_log_fetches = 0

  # Print summary of these specific failed jobs to stderr
  sys.stderr.write(f"INFO: Summary of failed jobs for pattern '{current_pattern_str}':\n")
  for job in list_of_failed_jobs:
    sys.stderr.write(f"  - {job['name']} (ID: {job['id']})\n")
  sys.stderr.write("\n")

  for idx, job in enumerate(list_of_failed_jobs):
    sys.stderr.write(f"INFO: Downloading log {idx+1}/{total_failed_jobs_to_process} for job '{job['name']}' (ID: {job['id']})...\n")
    job_logs_raw = get_job_logs(args.token, job['id']) # Renamed to avoid conflict with global

    print(f"\n## Job: {job['name']} (ID: {job['id']}) - FAILED")
    print(f"[Job URL]({job.get('html_url', 'N/A')})\n")

    if not job_logs_raw:
      print("**Could not retrieve logs for this job.**")
      sys.stderr.write(f"WARNING: Failed to retrieve logs for job '{job['name']}' (ID: {job['id']}).\n")
      continue

    successful_log_fetches += 1

    failed_steps_details = []
    if job.get('steps'):
        for step in job['steps']:
            if step.get('conclusion') == 'failure':
                failed_steps_details.append(step)

    if not failed_steps_details:
        print("\n**Note: No specific failed steps were identified in the job's metadata, but the job itself is marked as failed.**")
        stripped_log_lines_fallback = [strip_initial_timestamp(line) for line in job_logs_raw.splitlines()]
        if args.grep_pattern:
            print(f"Displaying grep results for pattern '{args.grep_pattern}' with context {args.grep_context} from **entire job log**:")
            print("\n```log")
            try:
                process = subprocess.run(
                    ['grep', '-E', f"-C{args.grep_context}", args.grep_pattern],
                    input="\n".join(stripped_log_lines_fallback), text=True, capture_output=True, check=False
                )
                if process.returncode == 0: print(process.stdout.strip())
                elif process.returncode == 1: print(f"No matches found for pattern '{args.grep_pattern}' in entire job log.")
                else: sys.stderr.write(f"Grep command failed on full job log: {process.stderr}\n") # Should this be in log block?
            except FileNotFoundError: sys.stderr.write("Error: 'grep' not found, cannot process full job log with grep.\n")
            except Exception as e: sys.stderr.write(f"Grep error on full job log: {e}\n")
            print("```")
        else:
            print(f"Displaying last {args.log_lines} lines from **entire job log** as fallback:")
            print("\n```log")
            for line in stripped_log_lines_fallback[-args.log_lines:]:
                print(line)
            print("```")
        print("\n---")
        continue

    print(f"\n### Failed Steps in Job: {job['name']}")
    first_failed_step_logged = False
    for step in failed_steps_details:
      if not args.all_failed_steps and first_failed_step_logged:
        print(f"\n--- Skipping subsequent failed step: {step.get('name', 'Unnamed step')} (use --all-failed-steps to see all) ---")
        break

      step_name = step.get('name', 'Unnamed step')
      print(f"\n#### Step: {step_name}")

      escaped_step_name = re.escape(step_name)
      step_start_pattern = re.compile(r"^##\[group\](?:Run\s+|Setup\s+|Complete\s+)?.*?" + escaped_step_name, re.IGNORECASE)
      step_end_pattern = re.compile(r"^##\[endgroup\]")

      raw_log_lines_for_job_step_search = job_logs_raw.splitlines()
      current_step_raw_log_segment_lines = []
      capturing_for_failed_step = False
      for line in raw_log_lines_for_job_step_search:
          if step_start_pattern.search(line):
              capturing_for_failed_step = True
              current_step_raw_log_segment_lines = [line]
              continue
          if capturing_for_failed_step:
              current_step_raw_log_segment_lines.append(line)
              if step_end_pattern.search(line):
                  capturing_for_failed_step = False
                  break

      lines_to_process_stripped = []
      log_source_message = ""

      if current_step_raw_log_segment_lines:
          lines_to_process_stripped = [strip_initial_timestamp(line) for line in current_step_raw_log_segment_lines]
          log_source_message = f"Log for failed step '{step_name}'"
      else:
          lines_to_process_stripped = [strip_initial_timestamp(line) for line in raw_log_lines_for_job_step_search] # Use full job log if segment not found
          log_source_message = f"Could not isolate log for step '{step_name}'. Using entire job log"

      log_content_for_processing = "\n".join(lines_to_process_stripped)

      if args.grep_pattern:
          print(f"\n{log_source_message} (grep results for pattern `{args.grep_pattern}` with context {args.grep_context}):\n")
          print("```log")
          try:
              process = subprocess.run(
                  ['grep', '-E', f"-C{args.grep_context}", args.grep_pattern],
                  input=log_content_for_processing, text=True, capture_output=True, check=False
              )
              if process.returncode == 0:
                  print(process.stdout.strip())
              elif process.returncode == 1:
                  print(f"No matches found for pattern '{args.grep_pattern}' in this log segment.")
              else:
                  print(f"Grep command failed with error code {process.returncode}. Stderr:\n{process.stderr}")
          except FileNotFoundError:
              sys.stderr.write("Error: 'grep' command not found. Please ensure it is installed and in your PATH to use --grep-pattern.\n")
              print("Skipping log display for this step as grep is unavailable.")
          except Exception as e:
              sys.stderr.write(f"An unexpected error occurred while running grep: {e}\n")
              print("Skipping log display due to an error with grep.")
          print("```")
      else:
          print(f"\n{log_source_message} (last {args.log_lines} lines):\n")
          print("```log")
          for log_line in lines_to_process_stripped[-args.log_lines:]:
              print(log_line)
          print("```")

      print(f"\n---")
      first_failed_step_logged = True
    print(f"\n---")

  sys.stderr.write(f"INFO: Log processing complete for this batch. Successfully fetched and processed logs for {successful_log_fetches}/{total_failed_jobs_to_process} job(s) from pattern '{current_pattern_str}'.\n")


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
      description="Fetch and display failed steps and their logs from a GitHub workflow run.",
      formatter_class=argparse.RawTextHelpFormatter
  )
  parser.add_argument(
      "--workflow", "--workflow-name",
      type=str,
      default="integration_tests.yml",
      help="Name of the workflow file (e.g., 'main.yml' or 'build-test.yml'). Default: 'integration_tests.yml'."
  )
  parser.add_argument(
      "--branch",
      type=str,
      default=current_branch,
      help=f"GitHub branch name to check for the workflow run. {'Default: ' + current_branch if current_branch else 'Required if not determinable from current git branch.'}"
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
      help="GitHub token. Can also be set via GITHUB_TOKEN env var or from ~/.github_token."
  )
  parser.add_argument(
      "--log-lines",
      type=int,
      default=100,
      help="Number of lines to print from the end of each failed step's log (if not using grep). Default: 100."
  )
  parser.add_argument(
      "--all-failed-steps",
      action="store_true",
      default=False,
      help="If set, print logs for all failed steps in a job. Default is to print logs only for the first failed step."
  )
  parser.add_argument(
      "--grep-pattern", "-g",
      type=str,
      default="[Ee][Rr][Rr][Oo][Rr][: ]",
      help="Extended Regular Expression (ERE) to search for in logs. Default: \"[Ee][Rr][Rr][Oo][Rr][: ]\". If an empty string is passed, grep is disabled."
  )
  parser.add_argument(
      "--grep-context", "-C",
      type=int,
      default=10,
      help="Number of lines of leading and trailing context to print for grep matches. Default: 10."
  )
  parser.add_argument(
      "--job-pattern",
      action='append',
      type=str,
      help="Regular expression to filter job names. Can be specified multiple times to check patterns in order. "
           "If no patterns are specified, defaults to checking: '^build.*', then '^test.*', then '.*'."
  )

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
  args.token = token

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
    elif args.owner and args.repo:
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

  if not set_repo_info(final_owner, final_repo):
    sys.stderr.write(f"Error: Could not set repository info to {final_owner}/{final_repo}. Ensure owner/repo are correct.{error_suffix}\n")
    sys.exit(1)

  if not args.branch:
      sys.stderr.write(f"Error: Branch name is required. Please specify --branch or ensure it can be detected from your current git repository.{error_suffix}\n")
      sys.exit(1)

  sys.stderr.write(f"Processing workflow '{args.workflow}' on branch '{args.branch}' for repo {OWNER}/{REPO}\n")

  run = get_latest_workflow_run(args.token, args.workflow, args.branch)
  if not run:
    sys.stderr.write(f"No workflow run found for workflow '{args.workflow}' on branch '{args.branch}'.\n")
    sys.exit(0)

  sys.stderr.write(f"Found workflow run ID: {run['id']} (Status: {run.get('status')}, Conclusion: {run.get('conclusion')})\n")

  patterns_to_check = args.job_pattern if args.job_pattern else DEFAULT_JOB_PATTERNS

  all_jobs_api_response = get_all_jobs_for_run(args.token, run['id'])
  if all_jobs_api_response is None:
    sys.stderr.write(f"Could not retrieve jobs for workflow run ID: {run['id']}. Exiting.\n")
    sys.exit(1)

  found_failures_and_processed = False
  for current_pattern_str in patterns_to_check:
    sys.stderr.write(f"\nINFO: Checking with job pattern: '{current_pattern_str}'...\n")
    try:
      current_job_name_regex = re.compile(current_pattern_str)
    except re.error as e:
      sys.stderr.write(f"WARNING: Invalid regex for job pattern '{current_pattern_str}': {e}. Skipping this pattern.\n")
      continue

    name_matching_jobs = [j for j in all_jobs_api_response if current_job_name_regex.search(j['name'])]

    if not name_matching_jobs:
      sys.stderr.write(f"INFO: No jobs found matching pattern '{current_pattern_str}'.\n")
      continue

    sys.stderr.write(f"INFO: Found {len(name_matching_jobs)} job(s) matching pattern '{current_pattern_str}'. Checking for failures...\n")
    failed_jobs_this_pattern = [j for j in name_matching_jobs if j.get('conclusion') == 'failure']

    if failed_jobs_this_pattern:
      sys.stderr.write(f"INFO: Found {len(failed_jobs_this_pattern)} failed job(s) for pattern '{current_pattern_str}'.\n")

      # Call the refactored processing function
      _process_and_display_logs_for_failed_jobs(args, failed_jobs_this_pattern, run.get('html_url'), current_pattern_str)

      found_failures_and_processed = True
      sys.stderr.write(f"INFO: Failures processed for pattern '{current_pattern_str}'. Subsequent patterns will not be checked.\n")
      break
    else:
      sys.stderr.write(f"INFO: All {len(name_matching_jobs)} job(s) matching pattern '{current_pattern_str}' succeeded or are not yet concluded.\n")

  if not found_failures_and_processed:
    sys.stderr.write(f"\nINFO: No failed jobs found for any of the specified/default patterns ('{', '.join(patterns_to_check)}') after checking the workflow run.\n")
    # Optionally print overall run status if nothing specific was found
    overall_status = run.get('status')
    overall_conclusion = run.get('conclusion')
    if overall_status and overall_conclusion:
        sys.stderr.write(f"INFO: Overall workflow run status: {overall_status}, conclusion: {overall_conclusion}.\n")
    elif overall_status:
        sys.stderr.write(f"INFO: Overall workflow run status: {overall_status}.\n")


def get_latest_workflow_run(token, workflow_name, branch_name):
  """Fetches the most recent workflow run for a given workflow name and branch."""
  url = f'{GITHUB_API_URL}/actions/workflows/{workflow_name}/runs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  params = {'branch': branch_name, 'per_page': 1, 'page': 1}

  try:
    with requests_retry_session().get(url, headers=headers, params=params, timeout=TIMEOUT) as response:
      response.raise_for_status()
      data = response.json()
      if data['workflow_runs'] and len(data['workflow_runs']) > 0:
        return data['workflow_runs'][0]
      else:
        return None
  except requests.exceptions.RequestException as e:
    sys.stderr.write(f"Error: Failed to fetch workflow runs for '{workflow_name}' on branch '{branch_name}': {e}\n")
    if e.response is not None:
        sys.stderr.write(f"Response content: {e.response.text}\n")
    return None
  except json.JSONDecodeError as e:
    sys.stderr.write(f"Error: Failed to parse JSON response for workflow runs: {e}\n")
    return None


def get_all_jobs_for_run(token, run_id):
  """Fetches all jobs for a given workflow run."""
  url = f'{GITHUB_API_URL}/actions/runs/{run_id}/jobs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}

  page = 1
  per_page = 100
  all_jobs = []

  while True:
    params = {'per_page': per_page, 'page': page, 'filter': 'latest'}
    try:
      with requests_retry_session().get(url, headers=headers, params=params, timeout=TIMEOUT) as response:
        response.raise_for_status()
        data = response.json()
        current_page_jobs = data.get('jobs', [])
        if not current_page_jobs:
          break
        all_jobs.extend(current_page_jobs)
        if len(current_page_jobs) < per_page:
          break
        page += 1
    except requests.exceptions.RequestException as e:
      sys.stderr.write(f"Error: Failed to fetch jobs for run ID {run_id} (page {page}): {e}\n")
      if e.response is not None:
        sys.stderr.write(f"Response content: {e.response.text}\n")
      return None
    except json.JSONDecodeError as e:
      sys.stderr.write(f"Error: Failed to parse JSON response for jobs: {e}\n")
      return None

  # This was an error in previous version, failed_jobs was defined but all_jobs returned
  # Now it correctly returns all_jobs as intended by the function name.
  return all_jobs


def get_job_logs(token, job_id):
  """Downloads the logs for a specific job."""
  url = f'{GITHUB_API_URL}/actions/jobs/{job_id}/logs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}

  try:
    with requests_retry_session().get(url, headers=headers, timeout=LONG_TIMEOUT, stream=False) as response:
      response.raise_for_status()
      return response.text
  except requests.exceptions.RequestException as e:
    sys.stderr.write(f"Error: Failed to download logs for job ID {job_id}: {e}\n")
    if e.response is not None:
        sys.stderr.write(f"Response status: {e.response.status_code}\n")
    return None


if __name__ == "__main__":
  main()
