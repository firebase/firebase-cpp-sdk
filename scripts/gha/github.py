import requests
import json
import shutil

from absl import logging

OWNER = 'firebase'
REPO = 'firebase-cpp-sdk'

BASE_URL = 'https://api.github.com'
FIREBASE_URL = '%s/repos/%s/%s' % (BASE_URL, OWNER, REPO)
logging.set_verbosity(logging.INFO)


def create_issue(token, title, label):
  url = f'{FIREBASE_URL}/issues'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'title': title, 'labels': [label]}
  with requests.post(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def open_issue(token, issue_number):
  url = f'{FIREBASE_URL}/issues/{issue_number}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'state': 'open'}
  with requests.patch(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def close_issue(token, issue_number):
  url = f'{FIREBASE_URL}/issues/{issue_number}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'state': 'closed'}
  with requests.patch(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def update_issue_comment(token, issue_number, comment):
  url = f'{FIREBASE_URL}/issues/{issue_number}'
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.patch(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def search_issues_by_label(label):
  url = f'{BASE_URL}/search/issues?q=repo:{OWNER}/{REPO}+label:"{label}"+is:issue' 
  headers = {'Accept': 'application/vnd.github.v3+json'}
  with requests.get(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)
    return response.json()["items"]


def list_comments(issue_number):
  url = f'{FIREBASE_URL}/issues/{issue_number}/comments' 
  headers = {'Accept': 'application/vnd.github.v3+json'}
  with requests.get(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def add_comment(token, issue_number, comment):
  url = f'{FIREBASE_URL}/issues/{issue_number}/comments' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.post(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def update_comment(token, comment_id, comment):
  url = f'{FIREBASE_URL}/issues/comments/{comment_id}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.patch(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def delete_comment(token, comment_id):
  url = f'{FIREBASE_URL}/issues/comments/{comment_id}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)


def add_label(token, issue_number, label):
  url = f'{FIREBASE_URL}/issues/{issue_number}/labels' 
  headers={}
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = [label]
  with requests.post(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def delete_label(token, issue_number, label):
  url = f'{FIREBASE_URL}/issues/{issue_number}/labels/{label}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)


def list_artifacts(token, run_id):
  url = f'{FIREBASE_URL}/actions/runs/{run_id}/artifacts' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.get(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)
    return response.json()["artifacts"]


def download_artifact(token, artifact_id, output_path):
  url = f'{FIREBASE_URL}/actions/artifacts/{artifact_id}/zip' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.get(url, headers=headers, stream=True) as response:
    logging.info("%s response: %s", url, response)
    with open(output_path, 'wb') as file:
        shutil.copyfileobj(response.raw, file)
