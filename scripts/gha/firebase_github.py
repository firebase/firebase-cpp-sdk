# Copyright 2021 Google LLC
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

"""A utility for GitHub REST API.

This script handles GitHub Issue, Pull Request, Comment, Label and Artifact

"""

import requests
import json
import shutil
import re

from absl import logging
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry

RETRIES = 3
BACKOFF = 5
RETRY_STATUS = (403, 500, 502, 504)
TIMEOUT = 5
TIMEOUT_LONG = 20

OWNER = 'firebase'
REPO = 'firebase-cpp-sdk'

BASE_URL = 'https://api.github.com'
GITHUB_API_URL = '%s/repos/%s/%s' % (BASE_URL, OWNER, REPO)
logging.set_verbosity(logging.INFO)


def set_repo_url(repo):
  match = re.match(r'https://github\.com/([^/]+)/([^/.]+)', repo)
  if not match:
    logging.info('Error, only pattern https://github.com/\{repo_owner\}/\{repo_name\} are allowed.')
    return False

  (repo_owner, repo_name) = match.groups()
  global OWNER, REPO, GITHUB_API_URL
  OWNER = repo_owner
  REPO = repo_name
  GITHUB_API_URL = '%s/repos/%s/%s' % (BASE_URL, OWNER, REPO)
  return True


def requests_retry_session(retries=RETRIES,
                           backoff_factor=BACKOFF,
                           status_forcelist=RETRY_STATUS,
                           allowed_methods=frozenset(['GET', 'POST', 'PUT', 'DELETE', 'PATCH'])):  # Added allowed_methods
    session = requests.Session()
    retry = Retry(total=retries,
                  read=retries,
                  connect=retries,
                  backoff_factor=backoff_factor,
                  status_forcelist=status_forcelist,
                  allowed_methods=allowed_methods)  # Added allowed_methods
    adapter = HTTPAdapter(max_retries=retry)
    session.mount('http://', adapter)
    session.mount('https://', adapter)
    return session

def create_issue(token, title, label, body):
  """Create an issue: https://docs.github.com/en/rest/reference/issues#create-an-issue"""
  url = f'{GITHUB_API_URL}/issues'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'title': title, 'labels': [label], 'body': body}
  with requests.post(url, headers=headers, data=json.dumps(data), timeout=TIMEOUT) as response:
    logging.info("create_issue: %s response: %s", url, response)
    return response.json()


def get_issue_body(token, issue_number):
  """https://docs.github.com/en/rest/reference/issues#get-an-issue-comment"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests_retry_session().get(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("get_issue_body: %s response: %s", url, response)
    return response.json()["body"]


def update_issue(token, issue_number, data):
  """Update an issue: https://docs.github.com/en/rest/reference/issues#update-an-issue"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests_retry_session().patch(url, headers=headers, data=json.dumps(data), timeout=TIMEOUT) as response:
    logging.info("update_issue: %s response: %s", url, response)


def open_issue(token, issue_number):
  update_issue(token, issue_number, data={'state': 'open'})


def close_issue(token, issue_number):
  update_issue(token, issue_number, data={'state': 'closed'})


def update_issue_comment(token, issue_number, comment):
  update_issue(token, issue_number, data={'body': comment})


def search_issues_by_label(label):
  """https://docs.github.com/en/rest/reference/search#search-issues-and-pull-requests"""
  url = f'{BASE_URL}/search/issues?q=repo:{OWNER}/{REPO}+label:"{label}"+is:issue'
  headers = {'Accept': 'application/vnd.github.v3+json'}
  with requests_retry_session().get(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("search_issues_by_label: %s response: %s", url, response)
    return response.json()["items"]


def list_comments(token, issue_number):
  """https://docs.github.com/en/rest/reference/issues#list-issue-comments"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}/comments'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests_retry_session().get(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("list_comments: %s response: %s", url, response)
    return response.json()


def add_comment(token, issue_number, comment):
  """https://docs.github.com/en/rest/reference/issues#create-an-issue-comment"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}/comments'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.post(url, headers=headers, data=json.dumps(data), timeout=TIMEOUT) as response:
    logging.info("add_comment: %s response: %s", url, response)


def update_comment(token, comment_id, comment):
  """https://docs.github.com/en/rest/reference/issues#update-an-issue-comment"""
  url = f'{GITHUB_API_URL}/issues/comments/{comment_id}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests_retry_session().patch(url, headers=headers, data=json.dumps(data), timeout=TIMEOUT) as response:
    logging.info("update_comment: %s response: %s", url, response)


def delete_comment(token, comment_id):
  """https://docs.github.com/en/rest/reference/issues#delete-an-issue-comment"""
  url = f'{GITHUB_API_URL}/issues/comments/{comment_id}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("delete_comment: %s response: %s", url, response)


def add_label(token, issue_number, label):
  """https://docs.github.com/en/rest/reference/issues#add-labels-to-an-issue"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}/labels'
  headers={}
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = [label]
  with requests.post(url, headers=headers, data=json.dumps(data), timeout=TIMEOUT) as response:
    logging.info("add_label: %s response: %s", url, response)


def delete_label(token, issue_number, label):
  """https://docs.github.com/en/rest/reference/issues#delete-a-label"""
  url = f'{GITHUB_API_URL}/issues/{issue_number}/labels/{label}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("delete_label: %s response: %s", url, response)


def list_artifacts(token, run_id):
  """https://docs.github.com/en/rest/reference/actions#list-workflow-run-artifacts"""
  url = f'{GITHUB_API_URL}/actions/runs/{run_id}/artifacts'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests_retry_session().get(url, headers=headers, timeout=TIMEOUT) as response:
    logging.info("list_artifacts: %s response: %s", url, response)
    return response.json()["artifacts"]


def download_artifact(token, artifact_id, output_path=None):
  """https://docs.github.com/en/rest/reference/actions#download-an-artifact"""
  url = f'{GITHUB_API_URL}/actions/artifacts/{artifact_id}/zip'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  # Custom retry for artifact download due to potential for 410 errors (artifact expired)
  # which shouldn't be retried indefinitely like other server errors.
  artifact_retry = Retry(total=5,  # Increased retries
                         read=5,
                         connect=5,
                         backoff_factor=1,  # More aggressive backoff for artifacts
                         status_forcelist=(500, 502, 503, 504), # Only retry on these server errors
                         allowed_methods=frozenset(['GET']))
  session = requests.Session()
  adapter = HTTPAdapter(max_retries=artifact_retry)
  session.mount('https://', adapter)

  try:
    with session.get(url, headers=headers, stream=True, timeout=TIMEOUT_LONG) as response:
      logging.info("download_artifact: %s response: %s", url, response)
      response.raise_for_status()  # Raise an exception for bad status codes
      if output_path:
        with open(output_path, 'wb') as file:
            shutil.copyfileobj(response.raw, file)
        return True # Indicate success
      else:
        return response.content
  except requests.exceptions.HTTPError as e:
    logging.error(f"HTTP error downloading artifact {artifact_id}: {e.response.status_code} - {e.response.reason}")
    if e.response.status_code == 410:
        logging.warning(f"Artifact {artifact_id} has expired and cannot be downloaded.")
    return None # Indicate failure
  except requests.exceptions.RequestException as e:
    logging.error(f"Error downloading artifact {artifact_id}: {e}")
    return None # Indicate failure


def dismiss_review(token, pull_number, review_id, message):
  """https://docs.github.com/en/rest/reference/pulls#dismiss-a-review-for-a-pull-request"""
  url = f'{GITHUB_API_URL}/pulls/{pull_number}/reviews/{review_id}/dismissals'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'message': message}
  with requests_retry_session().put(url, headers=headers, data=json.dumps(data),
                    stream=True, timeout=TIMEOUT) as response:
    logging.info("dismiss_review: %s response: %s", url, response)
    return response.json()


def get_reviews(token, pull_number):
  """https://docs.github.com/en/rest/reference/pulls#list-reviews-for-a-pull-request"""
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
    with requests_retry_session().get(url, headers=headers, params=params,
                      stream=True, timeout=TIMEOUT) as response:
      logging.info("get_reviews: %s response: %s", url, response)
      results = results + response.json()
      # If exactly per_page results were retrieved, read the next page.
      keep_going = (len(response.json()) == per_page)
  return results


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


def create_workflow_dispatch(token, workflow_id, ref, inputs):
  """https://docs.github.com/en/rest/reference/actions#create-a-workflow-dispatch-event"""
  url = f'{GITHUB_API_URL}/actions/workflows/{workflow_id}/dispatches'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'ref': ref, 'inputs': inputs}
  with requests.post(url, headers=headers, data=json.dumps(data),
                    stream=True, timeout=TIMEOUT) as response:
    logging.info("create_workflow_dispatch: %s response: %s", url, response)
    # Response Status: 204 No Content
    return True if response.status_code == 204 else False


def list_workflows(token, workflow_id, branch):
  """https://docs.github.com/en/rest/reference/actions#list-workflow-runs-for-a-repository"""
  url = f'{GITHUB_API_URL}/actions/workflows/{workflow_id}/runs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'event': 'workflow_dispatch', 'branch': branch, 'per_page': 5}
  with requests.get(url, headers=headers, data=json.dumps(data),
                    stream=True, timeout=TIMEOUT_LONG) as response:
    logging.info("list_workflows: %s response: %s", url, response)
    return response.json()


def create_pull_request(token, head, base, title, body, maintainer_can_modify):
  """https://docs.github.com/en/rest/reference/pulls#create-a-pull-request"""
  url = f'{GITHUB_API_URL}/pulls'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'head': head, 'base': base, 'title': title, 'body': body,
          'maintainer_can_modify': maintainer_can_modify}
  with requests.post(url, headers=headers, data=json.dumps(data),
                    stream=True, timeout=TIMEOUT) as response:
    logging.info("create_pull_request: %s response: %s", head, response)
    return True if response.status_code == 201 else False


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
    with requests_retry_session().get(url, headers=headers, params=params,
                      stream=True, timeout=TIMEOUT) as response:
      logging.info("list_pull_requests: %s response: %s", url, response)
      results = results + response.json()
      # If exactly per_page results were retrieved, read the next page.
      keep_going = (len(response.json()) == per_page)
  return results


def list_workflow_runs(token, workflow_id, branch=None, event=None, limit=200):
  """https://docs.github.com/en/rest/actions/workflow-runs?list-workflow-runs-for-a-required-workflow"""
  url = f'{GITHUB_API_URL}/actions/workflows/{workflow_id}/runs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  page = 1
  per_page = 100
  results = []
  keep_going = True
  while keep_going:
    params = {'per_page': per_page, 'page': page}
    if branch: params.update({'branch': branch})
    if event: params.update({'event': event})
    page = page + 1
    keep_going = False
    with requests_retry_session().get(url, headers=headers, params=params,
                      stream=True, timeout=TIMEOUT) as response:
      logging.info("list_workflow_runs: %s page %d, response: %s", url, params['page'], response)
      if 'workflow_runs' not in response.json():
        break
      run_list_results = response.json()['workflow_runs']
      results = results + run_list_results
      # If exactly per_page results were retrieved, read the next page.
      keep_going = (len(run_list_results) == per_page)
      if limit > 0 and len(results) >= limit:
        keep_going = False
        results = results[:limit]
  return results


def list_jobs_for_workflow_run(token, run_id, attempt=None, limit=200):
  """https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run
  https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt

  Args:
    attempt: Which attempt to fetch. Should be a number >0, 'latest', or 'all'.
    If unspecified, returns 'latest'.
  """
  if attempt == 'latest' or attempt== 'all' or attempt == None:
    url = f'{GITHUB_API_URL}/actions/runs/{run_id}/jobs'
  else:
    url = f'{GITHUB_API_URL}/actions/runs/{run_id}/attempts/{attempt}/jobs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  page = 1
  per_page = 100
  results = []
  keep_going = True
  while keep_going:
    params = {'per_page': per_page, 'page': page}
    if attempt == 'latest' or attempt == 'all':
      params.update({'filter': attempt})
    page = page + 1
    keep_going = False
    with requests_retry_session().get(url, headers=headers, params=params,
                      stream=True, timeout=TIMEOUT) as response:
      logging.info("list_jobs_for_workflow_run: %s page %d, response: %s",
                   url, params['page'], response)
      if 'jobs' not in response.json():
        break
      job_results = response.json()['jobs']
      results = results + job_results
      # If exactly per_page results were retrieved, read the next page.
      keep_going = (len(job_results) == per_page)
      if limit > 0 and len(results) >= limit:
        keep_going = False
        results = results[:limit]
  return results

def download_job_logs(token, job_id):
  """https://docs.github.com/en/rest/actions/workflow-jobs#download-job-logs-for-a-workflow-run"""
  url = f'{GITHUB_API_URL}/actions/jobs/{job_id}/logs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests_retry_session().get(url, headers=headers, stream=True, timeout=TIMEOUT) as response:
    logging.info("download_job_logs: %s response: %s", url, response)
    return response.content.decode('utf-8')


def rerun_failed_jobs_for_workflow_run(token, run_id):
  """https://docs.github.com/en/rest/actions/workflow-runs#re-run-failed-jobs-from-a-workflow-run"""
  url = f'{GITHUB_API_URL}/actions/runs/{run_id}/rerun-failed-jobs'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.post(url, headers=headers,
                    stream=True, timeout=TIMEOUT) as response:
    logging.info("rerun_failed_jobs_for_workflow_run: %s response: %s", url, response)
    return True if response.status_code == 201 else False
