import requests
import json
import shutil

from absl import logging

OWNER = 'firebase'
REPO = 'firebase-cpp-sdk'
BASE_URL = 'https://api.github.com/repos/%s/%s' % (OWNER, REPO)
logging.set_verbosity(logging.INFO)


def list_comments(issue_number):
  url = f'{BASE_URL}/issues/{issue_number}/comments' 
  headers = {'Accept': 'application/vnd.github.v3+json'}
  with requests.get(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)
    return response.json()


def add_comment(token, issue_number, comment):
  url = f'{BASE_URL}/issues/{issue_number}/comments' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.post(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def update_comment(token, comment_id, comment):
  url = f'{BASE_URL}/issues/comments/{comment_id}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = {'body': comment}
  with requests.patch(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def delete_comment(token, comment_id):
  url = f'{BASE_URL}/issues/comments/{comment_id}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)


def add_label(token, issue_number, label):
  url = f'{BASE_URL}/issues/{issue_number}/labels' 
  headers={}
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  data = [label]
  with requests.post(url, headers=headers, data=json.dumps(data)) as response:
    logging.info("%s response: %s", url, response)


def delete_label(token, issue_number, label):
  url = f'{BASE_URL}/issues/{issue_number}/labels/{label}' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.delete(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)


def list_artifacts(token, run_id):
  url = f'{BASE_URL}/actions/runs/{run_id}/artifacts' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.get(url, headers=headers) as response:
    logging.info("%s response: %s", url, response)
    logging.info(response.json())
    return response.json()["artifacts"]


def download_artifact(token, artifact_id, output_path):
  url = f'{BASE_URL}/actions/artifacts/{artifact_id}/zip' 
  headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
  with requests.get(url, headers=headers, stream=True) as response:
    logging.info("%s response: %s", url, response)
    with open(output_path, 'wb') as file:
        shutil.copyfileobj(response.raw, file)