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

"""Fetches and formats review comments from a GitHub Pull Request,
displaying top-level review summaries with their associated line comments
nested underneath."""

import argparse
import os
import sys
import firebase_github
import datetime
from datetime import timezone, timedelta

def parse_iso_timestamp(timestamp_str):
    """Parses an ISO 8601 timestamp string, handling 'Z' for UTC."""
    if not timestamp_str:
        return None
    try:
        if sys.version_info < (3, 11) and timestamp_str.endswith("Z"):
            timestamp_str = timestamp_str[:-1] + "+00:00"
        return datetime.datetime.fromisoformat(timestamp_str)
    except ValueError:
        sys.stderr.write(f"Warning: Could not parse timestamp: {timestamp_str}\n")
        return None

def main():
    STATUS_IRRELEVANT = "[IRRELEVANT]"
    STATUS_OLD = "[OLD]"
    STATUS_CURRENT = "[CURRENT]"

    default_owner = firebase_github.OWNER
    default_repo = firebase_github.REPO

    parser = argparse.ArgumentParser(
        description="Fetch review comments from a GitHub PR and format into simple text output, nesting line comments under review summaries.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("--pull_number", type=int, required=True, help="Pull request number.")
    parser.add_argument("--owner", type=str, default=default_owner, help=f"Repository owner. Defaults to '{default_owner}'.")
    parser.add_argument("--repo", type=str, default=default_repo, help=f"Repository name. Defaults to '{default_repo}'.")
    parser.add_argument("--token", type=str, default=os.environ.get("GITHUB_TOKEN"), help="GitHub token. Can also be set via GITHUB_TOKEN env var.")
    parser.add_argument("--context-lines", type=int, default=10, help="Number of context lines from the diff hunk. 0 for full hunk. If > 0, shows header (if any) and last N lines of the remaining hunk. Default: 10.")
    parser.add_argument("--since", type=str, default=None, help="Only show reviews and comments updated/submitted at or after this ISO 8601 timestamp (e.g., YYYY-MM-DDTHH:MM:SSZ). Applies to review submission time and line comment update time.")
    parser.add_argument("--exclude-old", action="store_true", default=False, help="Exclude line comments marked [OLD] (where line number has changed due to code updates but position is still valid).")
    parser.add_argument("--include-irrelevant", action="store_true", default=False, help="Include line comments marked [IRRELEVANT] (where GitHub can no longer anchor the comment to the diff, i.e., position is null).")

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

    sys.stderr.write(f"Fetching reviews and comments for PR #{args.pull_number} from {firebase_github.OWNER}/{firebase_github.REPO}...\n")

    reviews_all = firebase_github.get_reviews(args.token, args.pull_number)
    line_comments_all = firebase_github.get_pull_request_review_comments(args.token, args.pull_number) # Fetches all, not using 'since' here

    if not reviews_all and not line_comments_all:
        sys.stderr.write(f"No reviews or line comments found for PR #{args.pull_number}.\n")
        return

    # Sort reviews by submission time
    reviews_all.sort(key=lambda r: r.get("submitted_at", ""))

    since_dt = None
    if args.since:
        since_dt = parse_iso_timestamp(args.since)
        if since_dt:
            sys.stderr.write(f"Filtering reviews and comments submitted/updated since: {args.since}\n")
        else:
            sys.stderr.write(f"Warning: Could not parse --since timestamp: {args.since}. Fetching all.\n")


    latest_overall_timestamp_obj = None
    processed_comments_count = 0
    print("# Review Comments\n\n")

    for review in reviews_all:
        review_id = review.get("id")
        review_user = review.get("user", {}).get("login", "Unknown user")
        review_state = review.get("state", "N/A")
        review_body = review.get("body", "").strip()
        review_html_url = review.get("html_url", "N/A")
        review_submitted_at_str = review.get("submitted_at")
        review_submitted_dt = parse_iso_timestamp(review_submitted_at_str)

        # Filter review itself by --since (based on submission time)
        if since_dt and review_submitted_dt and review_submitted_dt < since_dt:
            # If review is too old, also skip its line comments for this primary filter pass
            # (individual line comments might still be filtered if they are older than review's submission)
            continue

        review_content_printed_for_this_block = False

        # Print Review Summary (if body exists)
        if review_body:
            print(f"## Review by: **{review_user}** (State: `{review_state}`, ID: `{review_id}`)\n")
            if review_submitted_at_str:
                print(f"*   **Submitted At**: `{review_submitted_at_str}`")
            print(f"*   **URL**: <{review_html_url}>\n")
            print("\n### Summary Comment:")
            print(review_body)

            if review_submitted_dt and (latest_overall_timestamp_obj is None or review_submitted_dt > latest_overall_timestamp_obj):
                latest_overall_timestamp_obj = review_submitted_dt
            processed_comments_count += 1
            review_content_printed_for_this_block = True

        # Process Nested Line Comments for this review
        current_review_line_comments = [lc for lc in line_comments_all if lc.get("pull_request_review_id") == review_id]
        current_review_line_comments.sort(key=lambda lc: lc.get("created_at", "")) # or 'id'

        for line_comment in current_review_line_comments:
            line_comment_created_at_str = line_comment.get("created_at")
            # Individual line comment filtering by --since (based on its own created_at or updated_at)
            # For simplicity with GitHub's `since` behavior, we'll use updated_at for filtering line comments.
            line_comment_updated_at_str = line_comment.get("updated_at")
            line_comment_updated_dt = parse_iso_timestamp(line_comment_updated_at_str)

            if since_dt and line_comment_updated_dt and line_comment_updated_dt < since_dt:
                continue

            current_pos = line_comment.get("position")
            current_line = line_comment.get("line")
            original_line = line_comment.get("original_line")
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

            line_comment_user = line_comment.get("user", {}).get("login", "Unknown user")
            path = line_comment.get("path", "N/A")
            body = line_comment.get("body", "").strip()
            diff_hunk = line_comment.get("diff_hunk")
            line_html_url = line_comment.get("html_url", "N/A")
            line_comment_id = line_comment.get("id")

            if not body:
                continue

            if not review_content_printed_for_this_block: # First visible item for this review block
                print(f"## Comments from Review by: **{review_user}** (State: `{review_state}`, ID: `{review_id}`)\n")
                if review_submitted_at_str:
                     print(f"*   **Review Submitted At**: `{review_submitted_at_str}`")
                print(f"*   **Review URL**: <{review_html_url}>\n")

            print(f"\n### Comment by: **{line_comment_user}** (ID: `{line_comment_id}`, Part of Review ID: `{review_id}`)\n")
            if line_comment_created_at_str: # Display created_at for line comment itself
                print(f"*   **Timestamp**: `{line_comment_created_at_str}`")
            print(f"*   **Status**: `{status_text}`")
            print(f"*   **File**: `{path}`")
            print(f"*   **Line**: `{line_to_display}`")
            print(f"*   **URL**: <{line_html_url}>\n")

            print("\n### Context:")
            print("```")
            if diff_hunk and diff_hunk.strip():
                if args.context_lines == 0:
                    print(diff_hunk)
                else:
                    hunk_lines = diff_hunk.split('\n')
                    if hunk_lines and hunk_lines[0].startswith("@@ "):
                        print(hunk_lines[0])
                        hunk_lines = hunk_lines[1:]
                    print("\n".join(hunk_lines[-args.context_lines:]))
            else:
                print("(No diff hunk available for this comment)")
            print("```")

            print("\n### Comment:")
            print(body)

            if line_comment_updated_dt and (latest_overall_timestamp_obj is None or line_comment_updated_dt > latest_overall_timestamp_obj):
                latest_overall_timestamp_obj = line_comment_updated_dt
            processed_comments_count += 1
            review_content_printed_for_this_block = True

        if review_content_printed_for_this_block:
            print("\n---") # Separator after each review block that had content

    # Handle orphaned line comments (not associated with any fetched review - rare, but possible if reviews were deleted)
    # For now, this simplified model assumes line comments are primarily viewed in context of their review.
    # A separate loop here could process line_comments_all where pull_request_review_id is None or not in any review_id from reviews_all.

    sys.stderr.write(f"\nPrinted {processed_comments_count} review summaries/comments to stdout.\n")

    if latest_overall_timestamp_obj:
        try:
            next_since_dt = latest_overall_timestamp_obj.astimezone(timezone.utc) + timedelta(seconds=2) # Use 2 seconds as previously established
            next_since_str = next_since_dt.strftime('%Y-%m-%dT%H:%M:%SZ')
            new_cmd_args = [sys.executable, sys.argv[0]]
            i = 1
            while i < len(sys.argv):
                if sys.argv[i] == "--since":
                    i += 2
                    continue
                new_cmd_args.append(sys.argv[i])
                i += 1
            new_cmd_args.extend(["--since", next_since_str])
            suggested_cmd = " ".join(new_cmd_args)
            sys.stderr.write(f"\nTo get comments created/updated after the last one in this batch, try:\n{suggested_cmd}\n")
        except Exception as e:
            sys.stderr.write(f"\nWarning: Could not generate next command suggestion: {e}\n")

if __name__ == "__main__":
    main()
