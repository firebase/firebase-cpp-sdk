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

"""Fetches and formats review comments from a GitHub Pull Request."""

import argparse
import os
import sys
import firebase_github # Assumes firebase_github.py is in the same directory or python path

# Attempt to configure logging for firebase_github if absl is available
try:
    from absl import logging as absl_logging
    # Set verbosity for absl logging if you want to see logs from firebase_github
    # absl_logging.set_verbosity(absl_logging.INFO)
except ImportError:
    # If absl is not used, standard logging can be configured if needed
    # import logging as std_logging
    # std_logging.basicConfig(level=std_logging.INFO)
    pass # firebase_github.py uses absl.logging.info, so this won't redirect.


def main():
    # Default owner and repo from firebase_github, ensuring it's loaded.
    default_owner = firebase_github.OWNER
    default_repo = firebase_github.REPO

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format for use with Jules.",
        formatter_class=argparse.RawTextHelpFormatter # To preserve formatting in help text
    )
    parser.add_argument(
        "--pull_number",
        type=int,
        required=True,
        help="Pull request number."
    )
    parser.add_argument(
        "--owner",
        type=str,
        default=default_owner,
        help=f"Repository owner. Defaults to '{default_owner}' (from firebase_github.py)."
    )
    parser.add_argument(
        "--repo",
        type=str,
        default=default_repo,
        help=f"Repository name. Defaults to '{default_repo}' (from firebase_github.py)."
    )
    parser.add_argument(
        "--token",
        type=str,
        default=os.environ.get("GITHUB_TOKEN"),
        help="GitHub token. Can also be set via GITHUB_TOKEN environment variable."
    )

    args = parser.parse_args()

    if not args.token:
        sys.stderr.write("Error: GitHub token not provided. Set GITHUB_TOKEN environment variable or use --token argument.\n")
        sys.exit(1)

    # Update the repository details in firebase_github module if different from default
    if args.owner != firebase_github.OWNER or args.repo != firebase_github.REPO:
        repo_url = f"https://github.com/{args.owner}/{args.repo}"
        if not firebase_github.set_repo_url(repo_url):
            sys.stderr.write(f"Error: Invalid repository URL format for {args.owner}/{args.repo}. Expected format: https://github.com/owner/repo\n")
            sys.exit(1)
        # Using print to stderr for info, as absl logging might not be configured here for this script's own messages.
        print(f"Targeting repository: {firebase_github.OWNER}/{firebase_github.REPO}", file=sys.stderr)


    print(f"Fetching review comments for PR #{args.pull_number} from {firebase_github.OWNER}/{firebase_github.REPO}...", file=sys.stderr)

    comments = firebase_github.get_pull_request_review_comments(args.token, args.pull_number)

    if not comments: # This will be true if list is empty (no comments or error in fetching first page)
        print(f"No review comments found for PR #{args.pull_number}, or an error occurred during fetching.", file=sys.stderr)
        # If firebase_github.py's get_pull_request_review_comments logs errors, those might provide more details.
        return # Exit gracefully if no comments

    # Output actual data to stdout
    print("\n--- Review Comments ---")
    for comment in comments:
        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")
        line = comment.get("line", "N/A")
        body = comment.get("body", "").strip() # Strip whitespace from comment body
        diff_hunk = comment.get("diff_hunk", "N/A")
        html_url = comment.get("html_url", "N/A")

        # Only print comments that have a body
        if not body:
            continue

        print(f"Comment by: {user}")
        print(f"File: {path}")
        # The 'line' field in GitHub's API for PR review comments refers to the line number in the diff.
        # 'original_line' refers to the line number in the file at the time the comment was made.
        # 'start_line' and 'original_start_line' for multi-line comments.
        # For simplicity, we use 'line'.
        print(f"Line in diff: {line}")
        print(f"URL: {html_url}")
        print("--- Diff Hunk ---")
        print(diff_hunk)
        print("--- Comment ---")
        print(body)
        print("----------------------------------------\n")

if __name__ == "__main__":
    main()
