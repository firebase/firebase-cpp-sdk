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
        default=10, # Default to 10 lines, 0 means full hunk.
        help="Number of context lines from the diff hunk. Use 0 for the full hunk. "
             "If > 0, shows the last N lines of the hunk. Default: 10."
    )
    parser.add_argument(
        "--since",
        type=str,
        default=None,
        help="Only show comments created at or after this ISO 8601 timestamp (e.g., YYYY-MM-DDTHH:MM:SSZ)."
    )
    parser.add_argument(
        "--skip-outdated",
        action="store_true",
        help="If set, outdated comments will not be printed."
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
    if args.skip_outdated:
        print("Skipping outdated comments.", file=sys.stderr)


    comments = firebase_github.get_pull_request_review_comments(
        args.token,
        args.pull_number,
        since=args.since
    )

    if not comments:
        print(f"No review comments found for PR #{args.pull_number} (or matching filters), or an error occurred.", file=sys.stderr)
        return

    print("\n--- Review Comments ---")
    for comment in comments:
        # Determine outdated status and effective line for display
        is_outdated = comment.get("position") is None

        if args.skip_outdated and is_outdated:
            continue

        line_to_display = comment.get("original_line") if is_outdated else comment.get("line")
        # Ensure line_to_display has a fallback if None from both
        if line_to_display is None: line_to_display = "N/A"


        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")

        body = comment.get("body", "").strip()
        if not body: # Skip comments with no actual text body
            continue

        diff_hunk = comment.get("diff_hunk")
        html_url = comment.get("html_url", "N/A")
        comment_id = comment.get("id")
        in_reply_to_id = comment.get("in_reply_to_id")
        created_at = comment.get("created_at")

        status_text = "[OUTDATED]" if is_outdated else "[CURRENT]"

        # Start printing comment details
        print(f"Comment by: {user} (ID: {comment_id}){f' (In Reply To: {in_reply_to_id})' if in_reply_to_id else ''}")
        if created_at:
            print(f"Timestamp: {created_at}")

        print(f"Status: {status_text}")
        print(f"File: {path}")
        print(f"Line in File Diff: {line_to_display}")
        print(f"URL: {html_url}")

        print("--- Diff Hunk Context ---")
        if diff_hunk and diff_hunk.strip():
            hunk_lines = diff_hunk.split('\n')
            if args.context_lines == 0: # User wants the full hunk
                print(diff_hunk)
            elif args.context_lines > 0: # User wants N lines of context (last N lines)
                lines_to_print_count = args.context_lines
                actual_lines_to_print = hunk_lines[-lines_to_print_count:]
                for line_content in actual_lines_to_print:
                    print(line_content)
            # If context_lines < 0, argparse should ideally prevent this or it's handled by default type int.
            # No explicit handling here means it might behave unexpectedly or error if not positive/zero.
        else:
            print("(No diff hunk available for this comment)")

        print("--- Comment ---")
        print(body)
        print("----------------------------------------\n")

if __name__ == "__main__":
    main()
