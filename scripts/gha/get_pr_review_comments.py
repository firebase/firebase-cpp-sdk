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
import firebase_github # Assumes firebase_github.py is in the same directory or python path

# Attempt to configure logging for firebase_github if absl is available
try:
    from absl import logging as absl_logging
    # Set verbosity for absl logging if you want to see logs from firebase_github
    # absl_logging.set_verbosity(absl_logging.INFO)
except ImportError:
    pass # firebase_github.py uses absl.logging.info, so this won't redirect.


def print_contextual_diff_hunk(diff_hunk, comment_position, context_lines_count):
    if not diff_hunk or not diff_hunk.strip(): # Handle empty or whitespace-only diff_hunk
        print("(No diff hunk available or content is empty)")
        return

    hunk_lines = diff_hunk.split('\n')

    # comment_position is 1-indexed from GitHub API. If None, or context is 0, print full hunk.
    if context_lines_count == 0 or comment_position is None or comment_position < 1 or comment_position > len(hunk_lines):
        print(diff_hunk)
        return

    comment_line_index = comment_position - 1 # Convert to 0-indexed for list access

    start_index = max(0, comment_line_index - context_lines_count)
    end_index = min(len(hunk_lines), comment_line_index + context_lines_count + 1)

    # Ensure start_index is not greater than comment_line_index, in case of small hunks
    # This also means that if comment_line_index is valid, start_index will be <= comment_line_index
    start_index = min(start_index, comment_line_index if comment_line_index >=0 else 0)


    for i in range(start_index, end_index):
        # Basic safety for i, though start/end logic should make this robust
        if i >= 0 and i < len(hunk_lines):
            prefix = "> " if i == comment_line_index else "  "
            print(f"{prefix}{hunk_lines[i]}")
        # else: # This case should ideally not be reached with correct boundary conditions
            # print(f"  Error: Skipped line index {i} in hunk processing due to boundary issue.")


def main():
    default_owner = firebase_github.OWNER
    default_repo = firebase_github.REPO

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format for use with Jules.",
        formatter_class=argparse.RawTextHelpFormatter
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
        help=f"Repository owner. Defaults to '{default_owner}'."
    )
    parser.add_argument(
        "--repo",
        type=str,
        default=default_repo,
        help=f"Repository name. Defaults to '{default_repo}'."
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
        help="Number of context lines around the commented line from the diff hunk. Use 0 for the full hunk. Default: 10."
    )
    parser.add_argument(
        "--since",
        type=str,
        default=None,
        help="Only show comments created at or after this ISO 8601 timestamp (e.g., YYYY-MM-DDTHH:MM:SSZ)."
    )

    args = parser.parse_args()

    if not args.token:
        sys.stderr.write("Error: GitHub token not provided. Set GITHUB_TOKEN or use --token.\n")
        sys.exit(1)

    if args.owner != firebase_github.OWNER or args.repo != firebase_github.REPO:
        repo_url = f"https://github.com/{args.owner}/{args.repo}"
        if not firebase_github.set_repo_url(repo_url):
            sys.stderr.write(f"Error: Invalid repo URL: {args.owner}/{args.repo}. Expected https://github.com/owner/repo\n")
            sys.exit(1)
        print(f"Targeting repository: {firebase_github.OWNER}/{firebase_github.REPO}", file=sys.stderr)

    print(f"Fetching comments for PR #{args.pull_number} from {firebase_github.OWNER}/{firebase_github.REPO}...", file=sys.stderr)
    if args.since:
        print(f"Filtering comments created since: {args.since}", file=sys.stderr)

    comments = firebase_github.get_pull_request_review_comments(
        args.token,
        args.pull_number,
        since=args.since # Pass the 'since' argument
    )

    if not comments:
        print(f"No review comments found for PR #{args.pull_number} (or matching filters), or an error occurred.", file=sys.stderr)
        return

    print("\n--- Review Comments ---")
    for comment in comments:
        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")
        file_line = comment.get("line", "N/A")
        hunk_position = comment.get("position") # This is the 1-indexed position in the hunk

        body = comment.get("body", "").strip()
        diff_hunk = comment.get("diff_hunk") # Can be None or empty
        html_url = comment.get("html_url", "N/A")

        comment_id = comment.get("id")
        in_reply_to_id = comment.get("in_reply_to_id")
        created_at = comment.get("created_at")

        if not body:
            continue

        print(f"Comment by: {user} (ID: {comment_id}){f' (In Reply To: {in_reply_to_id})' if in_reply_to_id else ''}")
        if created_at:
            print(f"Timestamp: {created_at}")

        if diff_hunk: # Only show status if it's a diff-related comment
            status_text = "[OUTDATED]" if hunk_position is None else "[CURRENT]"
            print(f"Status: {status_text}")

        print(f"File: {path}")
        print(f"Line in File Diff: {file_line}")
        print(f"URL: {html_url}")

        print("--- Diff Hunk Context ---")
        if diff_hunk:
            print_contextual_diff_hunk(diff_hunk, hunk_position, args.context_lines)
        else:
            print("(Comment not associated with a specific diff hunk)")

        print("--- Comment ---")
        print(body)
        print("----------------------------------------\n") # Use
 for newline

if __name__ == "__main__":
    main()
