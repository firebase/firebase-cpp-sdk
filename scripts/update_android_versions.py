import argparse
import logging
import requests
import os
import re
import sys
from xml.etree import ElementTree

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

def modify_dependency_file(dependency_filepath, version_map):
  """Modify a dependency file to reference the correct module versions.

  Looks for lines like: 'com.google.firebase:firebase-auth:1.2.3'
  for modules matching the ones in the version map, and modifies them in-place.

  Args:
    dependency_filename: Relative path to the dependency file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'com.google.firebase.firebase_auth': '15.0.0' }
  """
  logging.debug('Reading dependency file: {0}'.format(dependency_filepath))
  lines = None
  with open(dependency_filepath, "r") as dependency_file:
    lines = dependency_file.readlines()
  if not lines:
    logging.fatal('Could not read contents of file %s', dependency_filepath)

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

  for line in lines:
    output_lines.append(
        re.sub(RE_GENERIC_DEPENDENCY_MODULE, replace_dependency, line))

  if lines == output_lines:
    logging.debug('No changes to dependency file: %s', dependency_filepath)
    return

  with open(dependency_filepath, 'w') as dependency_file:
    if not dependency_file:
      logging.fatal('Could not open dependency file %s for writing',
                    dependency_filepath)
    dependency_file.writelines(output_lines)

RE_README_ANDROID_VERSION = re.compile(
    r"<br>(?P<pkg>[a-zA-Z0-9._-]+:[a-zA-Z0-9._-]+):([0-9.]+)<br>")


def modify_readme_file(readme_filename, version_map):
  """Modify a readme Markdown file to reference correct module versions.

  Looks for lines like:
  |                  | com.google.firebase:firebase-core:15.0.0    |
  for modules matching the ones in the version map, and modifies them in-place.

  Args:
    readme_filename: Relative path to readme file to edit.
    version_map: Dictionary of packages to version numbers, e.g. {
        'com.google.firebase.firebase_auth': '15.0.0' }
  """
  logging.debug('Reading readme file: {0}'.format(readme_filename))

  lines = None
  with open(readme_filename, "r") as readme_file:
    lines = readme_file.readlines()
  if not lines:
    logging.fatal('Could not read contents of file %s', readme_filename)

  output_lines = []

  # Replacement function, look up the version number of the given pkg.
  def replace_module_line(m):
    if not m.group('pkg'):
      return m.group(0)
    pkg = m.group('pkg').replace('-', '_').replace(':', '.')
    if pkg not in version_map:
      return m.group(0)
    repl = '%s:%s' % (m.group('pkg'), version_map[pkg])
    return '<br>{0}<br>'.format(repl)

  for line in lines:
    output_lines.append(
        re.sub(RE_README_ANDROID_VERSION, replace_module_line, line))

  if lines == output_lines:
    logging.debug('No changes to readme file: %s', readme_filename)
    return
  logging.debug('Writing readme file: %s', readme_filename)
  with open(readme_filename, 'w') as readme_file:
    if not readme_file:
      logging.fatal('Could not open readme file %s for writing', readme_filename)
    readme_file.writelines(output_lines)


def main():
  args = parse_cmdline_args()
  if not args.files_or_dirs:
    args.files_or_dirs = [os.getcwd()]
  #latest_versions_map = get_latest_maven_versions()
  latest_versions_map = {
    'com.google.firebase.firebase_auth': '6.6.6',
    'com.google.firebase.firebase_database': '9.9.9',

  }
  dep = "/Users/vimanyujain/dev/cpp/firebase/firebase-cpp-sdk/Android/firebase_dependencies.gradle"
  readme = "/Users/vimanyujain/dev/cpp/firebase/firebase-cpp-sdk/release_build_files/readme.md"
  # modify_dependency_file(dep, latest_versions_map)
  modify_readme_file(readme, latest_versions_map)
  # android_dependencies_files = get_android_dependencies_files(args.files_or_dirs)
  # update_android_dependencies_files(android_dependencies_files,
                                    # latest_versions_map, args.dryrun)

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
