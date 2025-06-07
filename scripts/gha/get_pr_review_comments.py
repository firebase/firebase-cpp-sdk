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
import datetime
from datetime import timezone, timedelta


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
        description="Fetch review comments from a GitHub PR and format into a simple text output.",
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
        help="If set, comments marked [OUTDATED] or [FULLY_OUTDATED] will not be printed."
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
        sys.stderr.write(f"Targeting repository: {firebase_github.OWNER}/{firebase_github.REPO}\n")

    sys.stderr.write(f"Fetching comments for PR #{args.pull_number} from {firebase_github.OWNER}/{firebase_github.REPO}...\n")
    if args.since:
        sys.stderr.write(f"Filtering comments created since: {args.since}\n")
    if args.skip_outdated:
        sys.stderr.write("Skipping outdated comments based on status.\n")


    comments = firebase_github.get_pull_request_review_comments(
        args.token,
        args.pull_number,
        since=args.since
    )

    if not comments:
        sys.stderr.write(f"No review comments found for PR #{args.pull_number} (or matching filters), or an error occurred.\n")
        return

    latest_created_at_obj = None
    print("\n--- Review Comments ---")
    for comment in comments:
        created_at_str = comment.get("created_at")

        current_pos = comment.get("position")
        current_line = comment.get("line")
        original_line = comment.get("original_line")

        status_text = ""
        line_to_display = None
        is_effectively_outdated = False

        if current_pos is None: # Comment's specific diff context is gone
            status_text = "[FULLY_OUTDATED]"
            line_to_display = original_line # Show original line if available
            is_effectively_outdated = True
        elif original_line is not None and current_line != original_line: # Comment on a line that changed
            status_text = "[OUTDATED]"
            line_to_display = current_line # Show where the comment is now in the diff
            is_effectively_outdated = True
        else: # Comment is current or a file-level comment (original_line is None but current_pos exists)
            status_text = "[CURRENT]"
            line_to_display = current_line # For line comments, or None for file comments (handled by fallback)
            is_effectively_outdated = False

        if line_to_display is None:
            line_to_display = "N/A"

        if args.skip_outdated and is_effectively_outdated:
            continue

        # Update latest timestamp (only for comments that will be printed)
        if created_at_str:
            try:
                # GitHub ISO format "YYYY-MM-DDTHH:MM:SSZ"
                # Python <3.11 fromisoformat needs "+00:00" not "Z"
                if sys.version_info < (3, 11):
                    dt_str = created_at_str.replace("Z", "+00:00")
                else:
                    dt_str = created_at_str
                current_comment_dt = datetime.datetime.fromisoformat(dt_str)
                if latest_created_at_obj is None or current_comment_dt > latest_created_at_obj:
                    latest_created_at_obj = current_comment_dt
            except ValueError:
                sys.stderr.write(f"Warning: Could not parse timestamp: {created_at_str}\n")

        user = comment.get("user", {}).get("login", "Unknown user")
        path = comment.get("path", "N/A")
        body = comment.get("body", "").strip()

        if not body:
            continue

        diff_hunk = comment.get("diff_hunk")
        html_url = comment.get("html_url", "N/A")
        comment_id = comment.get("id")
        in_reply_to_id = comment.get("in_reply_to_id")

        print(f"Comment by: {user} (ID: {comment_id}){f' (In Reply To: {in_reply_to_id})' if in_reply_to_id else ''}")
        if created_at_str:
            print(f"Timestamp: {created_at_str}")

        print(f"Status: {status_text}")
        print(f"File: {path}")
        print(f"Line in File Diff: {line_to_display}")
        print(f"URL: {html_url}")

        print("--- Diff Hunk Context ---")
        print("```") # Start of Markdown code block
        if diff_hunk and diff_hunk.strip():
            hunk_lines = diff_hunk.split('\n')
            if args.context_lines == 0: # User wants the full hunk
                print(diff_hunk)
            elif args.context_lines > 0: # User wants N lines of context (last N lines)
                lines_to_print_count = args.context_lines
                actual_lines_to_print = hunk_lines[-lines_to_print_count:]
                for line_content in actual_lines_to_print:
                    print(line_content)
        else:
            print("(No diff hunk available for this comment)")
        print("```") # End of Markdown code block

        print("--- Comment ---")
        print(body)
        print("----------------------------------------\n")

    if latest_created_at_obj:
        try:
            # Ensure it's UTC before adding timedelta, then format
            next_since_dt = latest_created_at_obj.astimezone(timezone.utc) + timedelta(seconds=1)
            next_since_str = next_since_dt.strftime('%Y-%m-%dT%H:%M:%SZ')

            new_cmd_args = [sys.argv[0]]
            skip_next_arg = False
            for i in range(1, len(sys.argv)):
                if skip_next_arg:
                    skip_next_arg = False
                    continue
                if sys.argv[i] == "--since":
                    skip_next_arg = True
                    continue
                new_cmd_args.append(sys.argv[i])

            new_cmd_args.extend(["--since", next_since_str])
            suggested_cmd = " ".join(new_cmd_args)
            sys.stderr.write(f"\nTo get comments created after the last one in this batch, try:\n{suggested_cmd}\n")
        except Exception as e:
            sys.stderr.write(f"\nWarning: Could not generate next command suggestion: {e}\n")

if __name__ == "__main__":
    main()
