import argparse
import logging
import os
import pprint
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pkg_resources import packaging

PODSPEC_REPOSITORY = 'https://github.com/CocoaPods/Specs.git'

PODS = (
  'FirebaseCore',
  'FirebaseAdMob',
  'FirebaseAnalytics',
  'FirebaseAuth',
  'FirebaseCrashlytics',
  'FirebaseDatabase',
  'FirebaseDynamicLinks',
  'FirebaseFirestore',
  'FirebaseFunctions',
  'FirebaseInstallations',
  'FirebaseInstanceID',
  'FirebaseMessaging',
  'FirebaseRemoteConfig',
  'FirebaseStorage',
)

PODS2 = (
  'FirebaseAuth',
)

def scan_pod_versions(local_repo_dir, pods=PODS):
  all_versions = defaultdict(list)
  logging.info('Scanning podspecs in Specs repo...')
  specs_dir = os.path.join(local_repo_dir, 'Specs')
  podspec_extension = ".podspec.json"
  for dirpath, _, filenames in os.walk(local_repo_dir):
    for f in filenames:
      if not f.endswith(podspec_extension):
        continue
      # Example: FirebaseAuth.podspec.json
      podname = f.split('.')[0]
      if not podname in pods:
        continue
      logging.debug('Found matching pod {0} in "Specs" at {1}'.format(podname,
                                                                     dirpath))
      version = os.path.basename(dirpath)
      all_versions[podname].append(version)

  return all_versions


def get_latest_pod_versions(pods=PODS):
  logging.info('Cloning podspecs git repo...')
  # Create a temporary directory context to clone pod spec repo.
  # Automatically cleaned up on completion of context.
  with tempfile.TemporaryDirectory(suffix='pods') as local_specs_repo_dir:
    # git_clone_cmd = ['git', 'clone', '-q', '--depth', '1',
                      # PODSPEC_REPOSITORY, local_specs_repo_dir]
    # subprocess.run(git_clone_cmd)
    local_specs_repo_dir = '/tmp/foo/Specs'
    all_versions = scan_pod_versions(local_specs_repo_dir, pods)
    latest_versions = {}
    for pod in all_versions:
      # all_versions map is in the following format:
      #  { 'PodnameA' : ['1.0.1', '2.0.4'], 'PodnameB': ['3.0.4', '1.0.2'] }
      # Convert string version numbers to semantic version objects and get
      # latest version.
      latest_version = max([packaging.version.parse(v)
                          for v in all_versions[pod]])
      # Replace the list of versions with just the latest version
      latest_versions[pod] = latest_version.base_version
    print("Latest pod versions retreived from cocoapods specs repo: \n")
    pprint.pprint(latest_versions)
    print()
    return latest_versions


def get_pod_files(dirs_and_files):
  pod_files = []
  for entry in dirs_and_files:
    abspath = os.path.abspath(entry)
    if not os.path.exists(abspath):
      continue
    if os.path.isdir(abspath):
      for dirpath, _, filenames in os.walk(abspath):
        for f in filenames:
          if f == 'Podfile':
            pod_files.append(os.path.join(dirpath, f))
    elif os.path.isfile(abspath):
      pod_files.append(abspath)

    return pod_files


def update_pod_files(pod_files, pod_version_map, dryrun=True):
  pattern = re.compile("pod '(?P<pod_name>.+)', '(?P<version>.+)'\n")
  for pod_file in pod_files:
    to_update = False
    existing_lines = []
    with open(pod_file, "r") as podfile:
      existing_lines = podfile.readlines()
    if not existing_lines:
      continue
    if dryrun:
      print('Checking if update is required for {0}'.format(pod_file))
    else:
      logging.debug('Checking if update is required for {0}'.format(pod_file))

    for idx, line in enumerate(existing_lines):
      match = re.match(pattern, line.lstrip())
      if match:
        pod_name = match['pod_name']
        pod_name_key = pod_name.replace('/', '')
        if pod_name_key in pod_version_map:
          latest_version = pod_version_map[pod_name_key]
          substituted_line = line.replace(match['version'], latest_version)
          if substituted_line != line:
            if dryrun:
              print('Replacing\n{0}with\n{1}'.format(line, substituted_line))
            else:
              logging.info('Replacing\n{0}with\n{1}'.format(line, substituted_line))
            existing_lines[idx] = substituted_line
            to_update = True

    if not dryrun and to_update:
      print('Updating contents of {0}'.format(pod_file))
      with open(pod_file, "w") as podfile:
        podfile.writelines(existing_lines)
      print()


def main():
  args = parse_cmdline_args()
  if not args.files_or_dirs:
    args.files_or_dirs = [os.getcwd()]
  #latest_versions_map = get_latest_pod_versions(PODS)
  latest_versions_map = {'FirebaseAuth': '8.0.0', 'FirebaseRemoteConfig':'9.9.9'}
  pod_files = get_pod_files(args.files_or_dirs)
  update_pod_files(pod_files, latest_versions_map, args.dryrun)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Update pod files with '
                                                'latest pod versions')
  parser.add_argument('--dryrun', action='store_true',
                      help='Just print the replaced lines, '
                      'DO NOT overwrite any files')
  parser.add_argument('files_or_dirs', nargs='*', metavar='file',
             help= 'List of pod files or directories containing podfiles')
  parser.add_argument( "--log_level", default="info",
                    help="Logging level (debug, warning, info)")
  args = parser.parse_args()

  # Special handling for log level argument
  log_levels = {
    'critical': logging.CRITICAL,
    'error': logging.ERROR,
    'warning': logging.WARNING,
    'info': logging.INFO,
    'debug': logging.DEBUG
  }

  level = log_levels.get(args.log_level.lower())
  if level is None:
    raise ValueError('Please use one of the following as'
                      'log levels:\n{0}'.format(','.join(log_levels.keys())))
  logging.basicConfig(level=level)
  logger = logging.getLogger(__name__)
  return args

if __name__ == '__main__':
  main()
  # from IPython import embed
  # embed()

