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
import firebase_github
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
    STATUS_IRRELEVANT = "[IRRELEVANT]"
    STATUS_OLD = "[OLD]"
    STATUS_CURRENT = "[CURRENT]"

    default_owner = firebase_github.OWNER
    default_repo = firebase_github.REPO

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format into simple text output.",
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
        help="Number of context lines from the diff hunk. 0 for full hunk. If > 0, shows header (if any) and last N lines of the remaining hunk. Default: 10."
    )
    parser.add_argument(
        "--since",
        type=str,
        default=None,
        help="Only show comments created at or after this ISO 8601 timestamp (e.g., YYYY-MM-DDTHH:MM:SSZ)."
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

    if args.owner != firebase_github.OWNER or args.repo != firebase_github.REPO:
        repo_url = f"https://github.com/{args.owner}/{args.repo}"
        if not firebase_github.set_repo_url(repo_url):
            sys.stderr.write(f"Error: Invalid repo URL: {args.owner}/{args.repo}. Expected https://github.com/owner/repo\n")
            sys.exit(1)
        sys.stderr.write(f"Targeting repository: {firebase_github.OWNER}/{firebase_github.REPO}\n")

    sys.stderr.write(f"Fetching comments for PR #{args.pull_number} from {firebase_github.OWNER}/{firebase_github.REPO}...\n")
    if args.since:
        sys.stderr.write(f"Filtering comments created since: {args.since}\n")
    # Removed skip_outdated message block


    comments = firebase_github.get_pull_request_review_comments(
        args.token,
        args.pull_number,
        since=args.since
    )

    if not comments:
        sys.stderr.write(f"No review comments found for PR #{args.pull_number} (or matching filters), or an error occurred.\n")
        return

    latest_created_at_obj = None
    processed_comments_count = 0
    print("# Review Comments\n\n")
    for comment in comments:
        # This replaces the previous status/skip logic for each comment
        created_at_str = comment.get("created_at")

        current_pos = comment.get("position")
        current_line = comment.get("line")
        original_line = comment.get("original_line")

        status_text = ""
        line_to_display = None
        # is_effectively_outdated is no longer needed with the new distinct flags

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
        print(f"*   **Line**: `{line_to_display}`") # Label changed from "Line in File Diff"
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
                if hunk_lines: # Check if there's anything left to print
                    # args.context_lines is > 0 here
                    actual_trailing_lines = hunk_lines[-args.context_lines:]
                    print("\n".join(actual_trailing_lines))
                # If hunk_lines is empty here (e.g. only contained a header that was removed), nothing more is printed.
        else: # diff_hunk was None or empty
            print("(No diff hunk available for this comment)")
        print("```") # End of Markdown code block

        print("\n### Comment:")
        print(body)
        print("\n---")

    sys.stderr.write(f"\nPrinted {processed_comments_count} comments to stdout.\n")

    if latest_created_at_obj:
        try:
            # Ensure it's UTC before adding timedelta, then format
            next_since_dt = latest_created_at_obj.astimezone(timezone.utc) + timedelta(seconds=2)
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
