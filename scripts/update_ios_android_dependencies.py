#!/usr/bin/env python

# Copyright 2021 Google
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
This script fetches the latest cocoapod and android package versions from their
respective public repositories and updates these versions in various files
across the C++ repository.

There are 3 types of files being updated by this script,
- Podfile : Files containins lists of cocoapods along with their versions.
            Eg: `ios_pods/Podfile` and any integration tests podfiles.

- Android dependencies gradle file: Gradle files containing list of android
                                    libraries and their versions that is
                                    referenced by gradle files from sub projects
                                    Eg: `Android/firebase_dependencies.gradle`

- Readme file: Readme file containing all dependencies (ios and android) and
               and their versions. Eg: 'release_build_files/readme.md`

Usage:
# Update versions in default set of files in the repository.
python3 scripts/update_ios_android_dependencies.py

# Update specific pod files (or directories containing pod files)
python3 scripts/update_ios_android_dependencies.py --podfiles foo/Podfile
                                                              dir_with_podfiles

Other similar flags:
--depfiles
--readmefiles

These "files" flags can take a list of paths (files and directories).
If directories are provided, they are scanned for known file types.
"""

import argparse
import logging
import os
import pprint
import re
import requests
import shutil
import subprocess
import sys
import tempfile

from collections import defaultdict
from pkg_resources import packaging
from xml.etree import ElementTree


def get_files_from_directory(dirpath, file_extension, file_name=None,
                             absolute_paths=True):
  """ Helper function to filter files in directories.

  Args:
      dirpath (str): Root directory to search in.
      file_extension (str): File extension to search for.
                            Eg: '.gradle'
      file_name (str, optional): Exact file name to search for.
                                 Defaults to None.
      absolute_paths (bool, optional): Return absolute paths to files.
                                       Defaults to True.
                                       If False, just filenames are returned.

  Returns:
      list(str): List of files matching the specified criteria.
                 List of filenames (if absolute_paths=False), or
                 a list of absolute paths (if absolute_paths=True)
  """
  files = []
  for dirpath, _, filenames in os.walk(dirpath):
    for filename in filenames:
      if not filename.endswith(file_extension):
        continue
      if file_name and not file_name == filename:
        continue
      if absolute_paths:
        files.append(os.path.join(dirpath, filename))
      else:
        files.append(filename)
  return files


# Cocoapods github repo from where we scan available pods and their versions.
PODSPEC_REPOSITORY = 'https://github.com/CocoaPods/Specs.git'

# Android gMaven repostiory from where we scan available android packages
# and their versions
GMAVEN_MASTER_INDEX = "https://dl.google.com/dl/android/maven2/master-index.xml"
GMAVEN_GROUP_INDEX = "https://dl.google.com/dl/android/maven2/{0}/group-index.xml"

# List of Pods that we are interested in.
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


def get_pod_versions(specs_repo, pods=PODS):
  """ Get available pods and their versions from the specs repo

  Args:
      local_repo_dir (str): Directory mirroring Cocoapods specs repo
      pods (iterable(str), optional): List of pods whose versions we need.
                                      Defaults to PODS.

  Returns:
      dict: Map of the form {<str>:list(str)}
            Containing a mapping of podnames to available versions.
  """
  all_versions = defaultdict(list)
  logging.info('Fetching pod versions from Specs repo...')
  podspec_files = get_files_from_directory(specs_repo,
                                           file_extension='.podspec.json')
  for podspec_file in podspec_files:
    filename = os.path.basename(podspec_file)
    # Example: FirebaseAuth.podspec.json
    podname = filename.split('.')[0]
    if not podname in pods:
      continue
    parent_dir = os.path.dirname(podspec_file)
    version = os.path.basename(parent_dir)
    all_versions[podname].append(version)

  return all_versions


def get_latest_pod_versions(specs_repo=None, pods=PODS):
  """Get latest versions for specified pods.

  Args:
      pods (iterable(str) optional): Pods for which we need latest version.
                                     Defaults to PODS.
      specs_repo (str optional): Local checkout of Cocoapods specs repo.

  Returns:
      dict: Map of the form {<str>:<str>} containing a mapping of podnames to
            latest version.
  """
  cleanup_required = False
  if specs_repo is None:
    specs_repo = tempfile.mkdtemp(suffix='pods')
    logging.info('Cloning podspecs git repo...')
    git_clone_cmd = ['git', 'clone', '-q', '--depth', '1',
                        PODSPEC_REPOSITORY, specs_repo]
    subprocess.run(git_clone_cmd)
    # Temporary directory should be cleaned up after use.
    cleanup_required = True

  all_versions = get_pod_versions(specs_repo, pods)
  if cleanup_required:
    shutil.rmtree(specs_repo)

  latest_versions = {}
  for pod in all_versions:
    # all_versions map is in the following format:
    #  { 'PodnameA' : ['1.0.1', '2.0.4'], 'PodnameB': ['3.0.4', '1.0.2'] }
    # Convert string version numbers to semantic version objects
    # for easier comparison and get the latest version.
    latest_version = max([packaging.version.parse(v)
                        for v in all_versions[pod]])
    # Replace the list of versions with just the latest version
    latest_versions[pod] = latest_version.base_version
  print("Latest pod versions retreived from cocoapods specs repo: \n")
  pprint.pprint(latest_versions)
  print()
  return latest_versions


def get_pod_files(dirs_and_files):
  """ Get final list of podfiles to update.
  If a directory is passed, it is searched recursively.

  Args:
      dirs_and_files (iterable(str)): List of paths which could be files or
                                      directories.

  Returns:
      iterable(str): Final list of podfiles after recursively searching dirs.
  """
  pod_files = []
  for entry in dirs_and_files:
    abspath = os.path.abspath(entry)
    if not os.path.exists(abspath):
      continue
    if os.path.isdir(abspath):
      pod_files = pod_files + get_files_from_directory(abspath,
                                                       file_extension='',
                                                       file_name='Podfile')
    elif os.path.isfile(abspath):
      pod_files.append(abspath)

    return pod_files

# Look for lines like,  pod 'Firebase/Core', '7.11.0'
RE_PODFILE_VERSION = re.compile("\s+pod '(?P<pod_name>.+)', '(?P<version>.+)'\n")

def modify_pod_file(pod_file, pod_version_map, dryrun=True):
  """ Update pod versions in specified podfile.

  Args:
      pod_file (str): Absolute path to a podfile.
      pod_version_map (dict): Map of podnames to their respective version.
      dryrun (bool, optional): Just print the substitutions.
                               Do not write to file. Defaults to True.
  """
  to_update = False
  existing_lines = []
  with open(pod_file, "r") as podfile:
    existing_lines = podfile.readlines()
  if not existing_lines:
    logging.debug('Update failed. ' +
                  'Could not read contents from pod file {0}.'.format(podfile))
    return
  logging.debug('Checking if update is required for {0}'.format(pod_file))

  substituted_pairs = []
  for idx, line in enumerate(existing_lines):
    match = re.match(RE_PODFILE_VERSION, line)
    if match:
      pod_name = match['pod_name']
      # Firebase/Auth -> FirebaseAuth
      pod_name_key = pod_name.replace('/', '')
      if pod_name_key in pod_version_map:
        latest_version = pod_version_map[pod_name_key]
        substituted_line = line.replace(match['version'], latest_version)
        if substituted_line != line:
          substituted_pairs.append((line, substituted_line))
          existing_lines[idx] = substituted_line
          to_update = True

  if to_update:
    print('Updating contents of {0}'.format(pod_file))
    for original, substituted in substituted_pairs:
      print('(-) ' + original + '(+) ' + substituted)

    if not dryrun:
      with open(pod_file, "w") as podfile:
        podfile.writelines(existing_lines)
    print()


def main():
  args = parse_cmdline_args()
  latest_versions_map = get_latest_pod_versions(args.specs_repo, PODS)
  #latest_versions_map = {'FirebaseAuth': '8.0.0', 'FirebaseRemoteConfig':'9.9.9'}
  pod_files = get_pod_files(args.podfiles)
  for pod_file in pod_files:
    modify_pod_file(pod_file, latest_versions_map, args.dryrun)

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Update pod files with '
                                                'latest pod versions')
  parser.add_argument('--dryrun', action='store_true',
            help='Just print the replaced lines, DO NOT overwrite any files')
  parser.add_argument( "--log_level", default="info",
            help="Logging level (debug, warning, info)")
  # iOS options
  parser.add_argument('--podfiles', nargs='+', default=(os.getcwd(),),
            help= 'List of pod files or directories containing podfiles')
  parser.add_argument('--specs_repo',
            help= 'Local checkout of github Cocoapods Specs repository')
  # Android options
  parser.add_argument('--depfiles', nargs='+',
            default=('Android/firebase_dependencies.gradle',),
            help= 'List of android dependency files or directories'
                  'containing them.')
  parser.add_argument('--readmefiles', nargs='+',
            default=('release_build_files/readme.md',),
            help= 'List of release readme markdown files or directories'
                  'containing them.')

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
