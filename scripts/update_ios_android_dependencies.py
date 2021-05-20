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
This script fetches the latest Cocoapod and Android package versions from their
respective public repositories and updates these versions in various files
across the C++ repository.

There are 3 types of files being updated by this script,
- Podfile : Files containing lists of cocoapods along with their versions.
            Eg: `ios_pods/Podfile` and any integration tests podfiles.

- Android dependencies gradle file: Gradle files containing list of Android
                                    libraries and their versions that is
                                    referenced by gradle files from sub projects
                                    Eg: `Android/firebase_dependencies.gradle`

- Readme file: Readme file containing all dependencies (iOS and Android) and
               and their versions. Eg: 'release_build_files/readme.md`

Usage:
# Dryrun (does not update any files on disk but prints out all replacements for
# preview) - Update versions in default set of files in the repository.
python3 scripts/update_ios_android_dependencies.py --dryrun

# Update versions in default set of files in the repository
python3 scripts/update_ios_android_dependencies.py

# Update only Android packages
python3 scripts/update_ios_android_dependencies.py --skip_ios

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
  """Helper function to filter files in directories.

  Args:
      dirpath (str): Root directory to search in.
      file_extension (str): File extension to search for.
                            Eg: '.gradle'
      file_name (str, optional): Exact file name to search for.
                                 Defaults to None. Eg: 'foo.gradle'
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


def get_files(dirs_and_files, file_extension, file_name=None):
  """Get final list of files after searching directories.
  If a directory is passed, it is searched recursively.

  Args:
      dirs_and_files (iterable(str)): List of paths which could be files or
                                      directories.
      file_extension (str): File extension to search for.
                            Eg: '.gradle'
      file_name (str, optional): Exact file name to search for.
                                 Defaults to None. Eg: 'foo.gradle'

  Returns:
      iterable(str): Final list of files after recursively searching dirs.
  """
  files = []
  for entry in dirs_and_files:
    abspath = os.path.abspath(entry)
    if not os.path.exists(abspath):
      continue
    if os.path.isdir(abspath):
      files = files + get_files_from_directory(abspath,
                                                  file_extension=file_extension,
                                                  file_name=file_name)
    elif os.path.isfile(abspath):
      files.append(abspath)
  return files


##########  iOS pods versions update #######################################

# Cocoapods github repo from where we scan available pods and their versions.
PODSPEC_REPOSITORY = 'https://github.com/CocoaPods/Specs.git'

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
  'Google-Mobile-Ads-SDK'
)


def get_pod_versions(specs_repo, pods=PODS):
  """Get available pods and their versions from the specs repo

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
  """Get final list of podfiles to update.
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
  """Update pod versions in specified podfile.

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
    logging.fatal('Update failed. ' +
                  'Could not read contents from pod file {0}.'.format(podfile))

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


RE_README_POD_VERSION = re.compile(
  r"\|(?P<spaces>\s+)\| (?P<pod_name>.*) Cocoapod \((?P<version>([0-9.]+))\)")

def modify_readme_file_pods(readme_filepath, version_map, dryrun=True):
  """Modify a readme Markdown file to reference correct cocoapods versions.

  Looks for lines like:
  <br>Firebase/AdMob Cocoapod (7.11.0)<br>Firebase/Auth Cocoapod (8.1.0)
  for pods matching the ones in the version map, and modifies them in-place.

  Args:
    readme_filepath: Path to readme file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'FirebaseAuth': '15.0.0', 'FirebaseDatabase': '14.0.0' }
    dryrun (bool, optional): Just print the substitutions.
                             Do not write to file. Defaults to True.
  """
  logging.debug('Reading readme file: {0}'.format(readme_filepath))

  lines = None
  with open(readme_filepath, "r") as readme_file:
    lines = readme_file.readlines()
  if not lines:
    logging.fatal('Could not read contents of file %s', readme_filepath)

  output_lines = []

  # Replacement function, look up the version number of the given pkg.
  def replace_pod_line(m):
    if not m.group('pod_name'):
      return m.group(0)
    pod_key = m.group('pod_name').replace('/', '')
    if pod_key not in version_map:
      return m.group(0)
    repl = '|%s| %s Cocoapod (%s)' % (m.group('spaces'),
                                    m.group('pod_name'),
                                    version_map[pod_key])
    return repl

  substituted_pairs = []
  to_update = False
  for line in lines:
    substituted_line = re.sub(RE_README_POD_VERSION, replace_pod_line,
                              line)
    output_lines.append(substituted_line)
    if substituted_line != line:
      substituted_pairs.append((line, substituted_line))
      to_update = True

  if to_update:
    print('Updating contents of {0}'.format(readme_filepath))
    for original, substituted in substituted_pairs:
      print('(-) ' + original + '(+) ' + substituted)

    if not dryrun:
      with open(readme_filepath, 'w') as readme_file:
        readme_file.writelines(output_lines)
    print()

########## END: iOS pods versions update #####################################

########## Android versions update #############################

# Android gMaven repostiory from where we scan available Android packages
# and their versions
GMAVEN_MASTER_INDEX = "https://dl.google.com/dl/android/maven2/master-index.xml"
GMAVEN_GROUP_INDEX = "https://dl.google.com/dl/android/maven2/{0}/group-index.xml"


def get_latest_maven_versions():
  latest_versions = {}
  response = requests.get(GMAVEN_MASTER_INDEX)
  index_xml = ElementTree.fromstring(response.content)
  for index_child in index_xml:
    group_name = index_child.tag
    group_path = group_name.replace('.', '/')
    response = requests.get(GMAVEN_GROUP_INDEX.format(group_path))
    group_xml = ElementTree.fromstring(response.content)
    for group_child in group_xml:
      package_name = group_child.tag.replace('-', '_')
      package_full_name = group_name + "." + package_name
      package_version = group_child.attrib['versions'].split(',')[-1]
      latest_versions[package_full_name] = package_version
  return latest_versions


# Regex to match lines like:
# 'com.google.firebase:firebase-auth:1.2.3'
RE_GENERIC_DEPENDENCY_MODULE = re.compile(
    r"(?P<quote>[\'\"])(?P<pkg>[a-zA-Z0-9._-]+:[a-zA-Z0-9._-]+):([0-9.]+)[\'\"]"
)

def modify_dependency_file(dependency_filepath, version_map, dryrun=True):
  """Modify a dependency file to reference the correct module versions.

  Looks for lines like: 'com.google.firebase:firebase-auth:1.2.3'
  for modules matching the ones in the version map, and modifies them in-place.

  Args:
    dependency_filename: Relative path to the dependency file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'com.google.firebase.firebase_auth': '15.0.0' }
    dryrun (bool, optional): Just print the substitutions.
                             Do not write to file. Defaults to True.
  """
  logging.debug('Reading dependency file: {0}'.format(dependency_filepath))
  lines = None
  with open(dependency_filepath, "r") as dependency_file:
    lines = dependency_file.readlines()
  if not lines:
    logging.fatal('Update failed. ' +
          'Could not read contents from file {0}.'.format(dependency_filepath))

  output_lines = []

  # Replacement function, look up the version number of the given pkg.
  def replace_dependency(m):
    if not m.group('pkg'):
      return m.group(0)
    pkg = m.group('pkg').replace('-', '_').replace(':', '.')
    if pkg not in version_map:
      return m.group(0)
    quote_type = m.group('quote')
    if not quote_type:
      quote_type = "'"
    return '%s%s:%s%s' % (quote_type, m.group('pkg'), version_map[pkg],
                          quote_type)

  substituted_pairs = []
  to_update = False
  for line in lines:
    substituted_line = re.sub(RE_GENERIC_DEPENDENCY_MODULE, replace_dependency,
                             line)
    output_lines.append(substituted_line)
    if substituted_line != line:
      substituted_pairs.append((line, substituted_line))
      to_update = True

  if to_update:
    print('Updating contents of {0}'.format(dependency_filepath))
    for original, substituted in substituted_pairs:
      print('(-) ' + original + '(+) ' + substituted)

    if not dryrun:
      with open(dependency_filepath, 'w') as dependency_file:
        dependency_file.writelines(output_lines)
    print()


RE_README_ANDROID_VERSION = re.compile(
    r"\|(?P<spaces>\s+)\| (?P<pkg>[a-zA-Z0-9._-]+:[a-zA-Z0-9._-]+):([0-9.]+)")


def modify_readme_file_android(readme_filepath, version_map, dryrun=True):
  """Modify a readme Markdown file to reference correct module versions.

  Looks for lines like:
  <br>com.google.firebase:firebase-core:15.0.0<br>
  for modules matching the ones in the version map, and modifies them in-place.

  Args:
    readme_filepath: Path to readme file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'com.google.firebase.firebase_auth': '15.0.0' }
    dryrun (bool, optional): Just print the substitutions.
                             Do not write to file. Defaults to True.
  """
  logging.debug('Reading readme file: {0}'.format(readme_filepath))

  lines = None
  with open(readme_filepath, "r") as readme_file:
    lines = readme_file.readlines()
  if not lines:
    logging.fatal('Update failed. ' +
          'Could not read contents from file {0}.'.format(readme_filepath))

  output_lines = []

  # Replacement function, look up the version number of the given pkg.
  def replace_module_line(m):
    if not m.group('pkg'):
      return m.group(0)
    pkg = m.group('pkg').replace('-', '_').replace(':', '.')
    if pkg not in version_map:
      return m.group(0)
    repl = '|%s| %s:%s ' % (m.group('spaces'), m.group('pkg'), version_map[pkg])
    return repl

  substituted_pairs = []
  to_update = False
  for line in lines:
    substituted_line = re.sub(RE_README_ANDROID_VERSION, replace_module_line,
                              line)
    output_lines.append(substituted_line)
    if substituted_line != line:
      substituted_pairs.append((line, substituted_line))
      to_update = True

  if to_update:
    print('Updating contents of {0}'.format(readme_filepath))
    for original, substituted in substituted_pairs:
      print('(-) ' + original + '(+) ' + substituted)

    if not dryrun:
      with open(readme_filepath, 'w') as readme_file:
        readme_file.writelines(output_lines)
    print()


# Regex to match lines like:
# implementation 'com.google.firebase:firebase-auth:1.2.3'
RE_GRADLE_COMPILE_MODULE = re.compile(
    r"implementation\s*\'(?P<pkg>[a-zA-Z0-9._-]+:[a-zA-Z0-9._-]+):([0-9.]+)\'")


def modify_gradle_file(gradle_filepath, version_map, dryrun=True):
  """Modify a build.gradle file to reference the correct module versions.

  Looks for lines like: implementation 'com.google.firebase:firebase-auth:1.2.3'
  for modules matching the ones in the version map, and modifies them in-place
  (with a g4 edit) to reference the released version.

  Args:
    gradle_filename: Relative path to build.gradle file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'com.google.firebase.firebase_auth': '15.0.0' }
    dryrun (bool, optional): Just print the substitutions.
                             Do not write to file. Defaults to True.
  """
  logging.debug("Reading gradle file: %s", gradle_filepath)

  lines = None
  with open(gradle_filepath, "r") as gradle_file:
    lines = gradle_file.readlines()
  if not lines:
    logging.fatal('Update failed. ' +
          'Could not read contents from file {0}.'.format(gradle_filepath))
  output_lines = []

  # Replacement function, look up the version number of the given pkg.
  def replace_module_line(m):
    if not m.group("pkg"):
      return m.group(0)
    pkg = m.group("pkg").replace("-", "_").replace(":", ".")
    if pkg not in version_map:
      return m.group(0)
    return "implementation '%s:%s'" % (m.group("pkg"), version_map[pkg])

  substituted_pairs = []
  to_update = False
  for line in lines:
    substituted_line = re.sub(RE_GRADLE_COMPILE_MODULE, replace_module_line,
                              line)
    output_lines.append(substituted_line)
    if substituted_line != line:
      substituted_pairs.append((line, substituted_line))
      to_update = True

  if to_update:
    print('Updating contents of {0}'.format(gradle_filepath))
    for original, substituted in substituted_pairs:
      print('(-) ' + original + '(+) ' + substituted)

    if not dryrun:
      with open(gradle_filepath, 'w') as gradle_file:
        gradle_file.writelines(output_lines)
    print()


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Update pod files with '
                                                'latest pod versions')
  parser.add_argument('--dryrun', action='store_true',
            help='Just print the replaced lines, DO NOT overwrite any files')
  parser.add_argument( "--log_level", default="info",
            help="Logging level (debug, warning, info)")
  # iOS options
  parser.add_argument('--skip_ios', action='store_true',
            help='Skip iOS pod version update.')
  parser.add_argument('--podfiles', nargs='+', default=(os.getcwd(),),
            help= 'List of pod files or directories containing podfiles')
  parser.add_argument('--specs_repo',
            help= 'Local checkout of github Cocoapods Specs repository')
  # Android options
  parser.add_argument('--skip_android', action='store_true',
            help='Skip Android libraries version update.')
  parser.add_argument('--depfiles', nargs='+',
            default=('Android/firebase_dependencies.gradle',
                    'release_build_files/Android/firebase_dependencies.gradle'),
            help= 'List of android dependency files or directories'
                  'containing them.')
  parser.add_argument('--gradlefiles', nargs='+',
            default=(os.getcwd(),),
            help= 'List of android build.gradle files to update.')
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


def main():
  args = parse_cmdline_args()
  # Readme files have to be updated for both Android and iOS dependencies.
  readme_files = get_files(args.readmefiles, file_extension='.md',
                           file_name='readme')

  if not args.skip_ios:
    latest_pod_versions_map = get_latest_pod_versions(args.specs_repo, PODS)
    pod_files = get_files(args.podfiles, file_extension='', file_name='Podfile')
    for pod_file in pod_files:
      modify_pod_file(pod_file, latest_pod_versions_map, args.dryrun)
    for readme_file in readme_files:
      modify_readme_file_pods(readme_file, latest_pod_versions_map, args.dryrun)

  if not args.skip_android:
    latest_android_versions_map = get_latest_maven_versions()
    dep_files = get_files(args.depfiles, file_extension='.gradle',
                          file_name='firebase_dependencies.gradle')
    for dep_file in dep_files:
      modify_dependency_file(dep_file, latest_android_versions_map, args.dryrun)

    for readme_file in readme_files:
      modify_readme_file_android(readme_file, latest_android_versions_map,
                                 args.dryrun)

    gradle_files = get_files(args.gradlefiles, file_extension='.gradle',
                             file_name='build.gradle')
    for gradle_file in gradle_files:
      modify_gradle_file(gradle_file, latest_android_versions_map, args.dryrun)

if __name__ == '__main__':
  main()
