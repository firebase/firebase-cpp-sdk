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


def download_artifact(token, artifact_id, output_path):
  """https://docs.github.com/en/rest/reference/actions#download-an-artifact"""
  url = f'{GITHUB_API_URL}/actions/artifacts/{artifact_id}/zip'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.get(url, headers=headers, stream=True, timeout=TIMEOUT) as response:
    logging.info("download_artifact: %s response: %s", url, response)
    with open(output_path, 'wb') as file:
        shutil.copyfileobj(response.raw, file)


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
      logging.info("get_reviews: %s response: %s", url, response)
      results = results + response.json()
      # If exactly per_page results were retrieved, read the next page.
      keep_going = (len(response.json()) == per_page)
  return results

