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
Assuming pre-requisites are in place (from running
`build_scripts/ios/install_prereqs.sh`), this builds the firebase cpp sdk for
ios and tvos.

It does the following,
- Build ios and tvos libraries via cmake (after downloading corresponding
cocoapods).
- Create a "universal" framework if all platforms and architectures were built.
- Create "xcframeworks" combining both ios and tvos frameworks.

Usage examples:
# Build all supported targets and architectures on all platforms.
python3 scripts/gha/build_ios_tvos.py

# Build specific targets
python3 scripts/gha/build_ios_tvos.py -t firebase_auth firebase_database

# Build for specific architectures
python3 scripts/gha/build_ios_tvos.py -a arm64 -t firebase_remote_config
"""

import argparse
from collections import defaultdict
import logging
import multiprocessing
import os
import shutil
import subprocess
import utils


# Configuration for supported os's, and platforms.
CONFIG = {
  'ios': {
    'supported_targets' : ('firebase_analytics', 'firebase_app_check',
                           'firebase_auth', 'firebase_database',
                           'firebase_dynamic_links', 'firebase_firestore',
                           'firebase_functions', 'firebase_gma',
                           'firebase_installations', 'firebase_messaging',
                           'firebase_remote_config', 'firebase_storage'),
    'device': {
      'architectures' : ('arm64', 'armv7'),
      'toolchain' : 'cmake/toolchains/ios.cmake',
    },
    'simulator': {
      'architectures' : ('arm64', 'x86_64', 'i386'),
      'toolchain': 'cmake/toolchains/ios_simulator.cmake',
    }
  },

  'tvos': {
    'supported_targets' : ('firebase_auth', 'firebase_analytics', 'firebase_app_check',
                           'firebase_database', 'firebase_firestore',
                           'firebase_functions', 'firebase_installations',
                           'firebase_messaging', 'firebase_remote_config',
                           'firebase_storage'),
    'device': {
      'architectures' : ('arm64',),
      'toolchain' : 'cmake/toolchains/apple.toolchain.cmake',
      'toolchain_platform': 'TVOS',
    },
    'simulator': {
      'architectures' : ('x86_64',),
      'toolchain' : 'cmake/toolchains/apple.toolchain.cmake',
      'toolchain_platform': 'SIMULATOR_TVOS'
    }
  },
}


def arrange_frameworks(archive_output_path):
  """Rename frameworks and remove unnecessary files.

  Args:
      archive_output_path (str): Output path containing frameworks.
        Subdirectories should be the various target frameworks.
  """
  archive_output_dir_entries = os.listdir(archive_output_path)
  if not 'firebase.framework' in archive_output_dir_entries:
    # Rename firebase_app path to firebase path
    old_dir = os.path.join(archive_output_path, 'firebase_app.framework')
    new_dir = os.path.join(archive_output_path, 'firebase.framework')
    logging.info('Renaming {0} to {1}'.format(old_dir, new_dir))
    os.rename(old_dir, new_dir)

    # Rename firebase_app library to firebase library
    logging.info('Renaming firebase_app library to firebase')
    os.rename(os.path.join(new_dir, 'firebase_app'),
              os.path.join(new_dir, 'firebase'),)

  # Delete all non framework directories
  for entry in archive_output_dir_entries:
    if not entry.endswith('.framework'):
      entry_absolute_path = os.path.join(archive_output_path, entry)
      if os.path.isdir(entry_absolute_path):
        logging.info('Deleting unnecessary path ' + entry_absolute_path)
        shutil.rmtree(entry_absolute_path)
      else:
        logging.info('Deleting unnecessary file ' + entry_absolute_path)
        os.remove(entry_absolute_path)

  # Delete all useless Info.plist
  for root, dirs, files in os.walk(archive_output_path):
    for f in files:
      if f == 'Info.plist':
        logging.info('Deleting unnecessary Info.plist file ' +
                     os.path.join(root, f))
        os.remove(os.path.join(root, f))


def build_universal_framework(frameworks_path, targets):
  """Create universal frameworks if possible.

  If all architectures (eg: arm64, armv7 etc) and platforms (device, simulator)
  were built, combine all of the libraries into a single universal framework.
  Args:
      frameworks_path (str): Root path containing subdirectories for each
        operating system and its frameworks.
      targets iterable(str): List of firebase libraries to process.
        (eg: [firebase_auth, firebase_remote_config])
        Eg: <build_dir>/frameworks              <------------- <frameworks_path>
                - ios
                  - device-arm64
                    - firebase.framework
                    - firebase_analytics.framework
                    ...
                  - simulator-i386
                  ...
                - tvos
                  - device-arm64
                    - firebase.framework
                    - firebase_analytics.framework
                    ...
                  - simulator-x86_64
                  ...

        Output: <build_dir>/frameworks
                - ios
                  - device-arm64
                    ...
                  - simulator-i386
                    ...
                  ...
                  - universal                     <-------------- Newly created
                    - firebase.framework
                    - firebase_analytics.framework
                    ...
                - tvos
                  - device-arm64
                    ...
                  - simulator-x86_64
                    ...
                  ...
                  - universal                     <-------------- Newly created
                    - firebase.framework
                    - firebase_analytics.framework
                    ...

  """
  for apple_os in os.listdir(frameworks_path):
    logging.info('Building universal framework for {0}'.format(apple_os))
    framework_os_path = os.path.join(frameworks_path, apple_os)
    # Extract list of all built platform-architecture combinations into a map.
    # Map looks like this,
    # {'device': ['arm64', 'armv7'], 'simulator': ['x86_64']}
    platform_variant_architecture_dirs = os.listdir(framework_os_path)
    platform_variant_arch_map = defaultdict(list)
    for variant_architecture in platform_variant_architecture_dirs:
      logging.debug('Inspecting ' + variant_architecture)
      platform_variant, architecture = variant_architecture.split('-')
      platform_variant_arch_map[platform_variant].append(architecture)

    build_universal = True
    for platform in platform_variant_arch_map:
      logging.debug('Found architectures for platform '
        '{0}: {1}'.format(platform,
                          ' '.join(platform_variant_arch_map[platform])))
      missing_architectures = set(CONFIG[apple_os][platform]['architectures']) \
                              - set(platform_variant_arch_map[platform])
      if missing_architectures:
        logging.error('Following architectures are missing for platform variant'
          '{0}: {1}'.format(platform_variant, ' '.join(missing_architectures)))
        build_universal = False
        break

    if not build_universal:
      logging.error('Missing some supported architectures. Skipping universal '
                    'framework creation')
      return

    # Pick any of the platform-arch directories as a reference candidate mainly
    # for obtaining a list of contained targets.
    reference_dir_path = os.path.join(framework_os_path,
                                      platform_variant_architecture_dirs[0])
    logging.debug('Using {0} as reference path for scanning '
                  'targets'.format(reference_dir_path))
    # Filter only .framework directories and make sure the framework is
    # in list of supported targets.
    target_frameworks = [x for x in os.listdir(reference_dir_path)
                        if x.endswith('.framework') and
                        x.split('.')[0] in targets]
    logging.debug('Targets found: {0}'.format(' '.join(target_frameworks)))

    # Collect a list of libraries from various platform-arch combinations for
    # each target and build a universal framework using lipo.
    for target_framework in target_frameworks:
      target_libraries = []
      # Eg: split firebase_auth.framework -> firebase_auth, .framework
      target, _ = os.path.splitext(target_framework)
      for variant_architecture_dir in platform_variant_architecture_dirs:
        # Since we have arm64 for both device and simulator, lipo cannot combine
        # them in the same fat file. We ignore simulator-arm64.
        if variant_architecture_dir == 'simulator-arm64':
          continue
        # <build_dir>/<apple_os>/frameworks/<platform-arch>/
        # <target>.framework/target
        library_path = os.path.join(framework_os_path,
                                    variant_architecture_dir,
                                    target_framework, target)
        target_libraries.append(library_path)

      # <build_dir>/<apple_os>/frameworks/universal/<target>.framework
      universal_target_path = os.path.join(framework_os_path, 'universal',
                                          target_framework)
      logging.debug('Ensuring all directories exist: ' + universal_target_path)
      os.makedirs(universal_target_path)
      # <build_dir>/<apple_os>/frameworks/universal/<target>.framework/<target>
      universal_target_library_path = os.path.join(universal_target_path,
                                                   target)
      # lipo -create <lib1> <lib2> <lib3> .. -output <universal_lib>
      cmd = ['lipo', '-create']
      cmd.extend(target_libraries)
      cmd.append('-output')
      cmd.append(universal_target_library_path)

      logging.info('Creating universal framework at' +
                   universal_target_library_path)
      utils.run_command(cmd)

    # Copy headers from platform specific firebase.framework to newly created
    # universal firebase.framework.
    firebase_framework_headers_path = os.path.join(reference_dir_path,
                                                  'firebase.framework',
                                                  'Headers')
    universal_firebase_framework_headers_path = os.path.join(
                                                  framework_os_path,
                                                  'universal',
                                                  'firebase.framework',
                                                  'Headers')

    shutil.copytree(firebase_framework_headers_path,
                    universal_firebase_framework_headers_path)


def build_xcframeworks(frameworks_path, xcframeworks_path, template_info_plist,
                       targets):
  """Build xcframeworks combining libraries for different operating systems.

  Combine frameworks for different operating systems (ios, tvos), architectures
  (arm64, armv7, x86_64 etc) per platform variant (device, simulator).
  This makes it super convenient for developers to use a single deliverable in
  XCode and develop for multiple platforms/operating systems in one project.

  Args:
      frameworks_path (str): Absolute path to path containing frameworks.
      xcframeworks_path (str): Absolute path to create xcframeworks in.
      template_info_plist (str): Absolute path to a template Info.plist that
        will be copied over to each xcframework and provides metadata to XCode.
      targets iterable(str): List of firebase target libraries.
        (eg: [firebase_auth, firebase_remote_config])

        Eg: <build_dir>/frameworks              <------------- <frameworks_path>
                - ios                           <---------- <frameworks_os_path>
                  - device-arm64
                    - firebase.framework
                    - firebase_analytics.framework
                    ...
                  - simulator-i386
                  ...
                - tvos
                  - device-arm64
                    - firebase.framework
                    - firebase_analytics.framework
                    ...
                  - simulator-x86_64
                  ...

        Output: <build_dir>/xcframeworks        <----------- <xcframeworks_path>
                - firebase.xcframework
                  - Info.plist                  <----------- <Info.plist file>
                  - ios-arm64_armv7           <-- <all_libraries_for_ios_device>
                    - firebase.framework
                      - firebase                <---- <library>
                      - Headers                 <---- <all_include_headers>
                  - ios-arm64_i386_x86_64-simulator <--- <for_ios_simulator>
                    - firebase.framework
                      - firebase
                      - Headers
                  - tvos-arm64                <- <all_libraries_for_tvos_device>
                    - firebase.framework
                      - firebase
                      - Headers
                    ...
                  ...
                - firebase_auth.xcframework   <-- <firebase_auth target>
                  - Info.plist
                  - ios-arm64_armv7
                    - firebase_auth.framework
                      - firebase_auth
                  - ios-arm64_i386_x86_64-simulator
                    - firebase_auth.framework
                      - firebase_auth
                  - tvos-arm64
                    - firebase.framework
                      - firebase
                      - Headers
                    ...
                  ...
                ...

  """
  for apple_os in os.listdir(frameworks_path):
    framework_os_path = os.path.join(frameworks_path, apple_os)
    platform_variant_architecture_dirs = os.listdir(framework_os_path)
    # Extract list of all built platform-architecture combinations into a map.
    # Map looks like this,
    # {'device': ['arm64', 'armv7'], 'simulator': ['x86_64']}
    platform_variant_arch_map = defaultdict(list)
    for variant_architecture in platform_variant_architecture_dirs:
      # Skip directories not of the format platform-arch (eg: universal)
      if not '-' in variant_architecture:
        continue
      platform_variant, architecture = variant_architecture.split('-')
      platform_variant_arch_map[platform_variant].append(architecture)

    reference_dir_path = os.path.join(framework_os_path,
                                      platform_variant_architecture_dirs[0])
    logging.debug('Using {0} as reference path for scanning '
                  'targets'.format(reference_dir_path))

    # Filter only .framework directories and make sure the framework is
    # in list of supported targets.
    target_frameworks = [x for x in os.listdir(reference_dir_path)
                        if x.endswith('.framework') and
                        x.split('.')[0] in targets]
    logging.debug('Targets found: {0}'.format(' '.join(target_frameworks)))

    # For each target, we collect all libraries for a specific platform variants
    # (device or simulator) and os (ios or tvos).
    for target_framework in target_frameworks:
      target_libraries = []
      # Eg: split firebase_auth.framework -> firebase_auth, .framework
      target, _ = os.path.splitext(target_framework)
      xcframework_target_map = {}
      for platform_variant in platform_variant_arch_map:
        architectures = platform_variant_arch_map[platform_variant]
        xcframework_libraries = []
        for architecture in architectures:
          # <build_dir>/<apple_os>/frameworks/<platform-arch>/
          # <target>.framework/target
          library_path = os.path.join(framework_os_path,
                                      '{0}-{1}'.format(platform_variant,
                                      architecture), target_framework, target)
          xcframework_libraries.append(library_path)

        xcframework_key_parts = [apple_os]
        xcframework_key_parts.append('_'.join(sorted(architectures)))
        if platform_variant != 'device':
          # device is treated as default platform variant and we do not add any
          # suffix at the end. For all other variants, add them as suffix.
          xcframework_key_parts.append(platform_variant)
        # Eg: ios-arm64_armv7, tvos-x86_64-simulator
        xcframework_key = '-'.join(xcframework_key_parts)

        # <build_dir>/xcframeworks/<target>.xcframework/<os>-<list_of_archs>/
        # <target>.framework
        library_output_dir = os.path.join(xcframeworks_path,
                                          '{0}.xcframework'.format(target),
                                          xcframework_key,
                                          '{0}.framework'.format(target))
        logging.debug('Ensuring all directories exist: ' + library_output_dir)
        os.makedirs(library_output_dir)
        cmd = ['lipo', '-create']
        cmd.extend(xcframework_libraries)
        cmd.append('-output')
        cmd.append(os.path.join(library_output_dir, target))

        logging.info('Creating xcframework at' +
                     os.path.join(library_output_dir, target))
        utils.run_command(cmd)

        # <build_dir>/xcframeworks/<target>.xcframework
        target_xcframeworks_path = os.path.join(xcframeworks_path,
          '{0}.xcframework'.format(target))
        # Create Info.plist for xcframework
        dest_path = os.path.join(target_xcframeworks_path, 'Info.plist')
        logging.info('Copying template {0}'.format(template_info_plist))
        shutil.copy(template_info_plist, dest_path)
        contents = None
        # Replace token LIBRARY_PATH with current target framework.
        with open(dest_path, 'r') as info_plist_file:
          contents = info_plist_file.read()
        if contents:
          logging.debug('Updating LIBRARY_PATH with '
                        '{0}.framework'.format(target))
          contents = contents.replace('LIBRARY_PATH',
                                      '{0}.framework'.format(target))
          with open(dest_path, 'w') as info_plist_file:
            info_plist_file.write(contents)

    # Copy Headers for firebase.xcframework from firebase.framework.
    # Using a random platform specific firebase.framework.
    firebase_framework_headers_path = os.path.join(reference_dir_path,
                                                   'firebase.framework',
                                                   'Headers')
    firebase_xcframework_path = os.path.join(xcframeworks_path,
                                             'firebase.xcframework')

    for xcframework_key in os.listdir(firebase_xcframework_path):
      if os.path.isfile(os.path.join(firebase_xcframework_path,
                                     xcframework_key)):
        continue
      dest_headers_path = os.path.join(firebase_xcframework_path,
                                       xcframework_key,
                                       'firebase.framework', 'Headers')
      if os.path.exists(dest_headers_path):
        continue
      logging.info('Copying {0} to {1}'.format(firebase_framework_headers_path,
                                               dest_headers_path))
      shutil.copytree(firebase_framework_headers_path,
                     dest_headers_path)


def cmake_configure(source_path, build_path, toolchain, archive_output_path,
                    architecture=None, toolchain_platform=None):
  """CMake configure which sets up the build project.

  Args:
      source_path (str): Source directory containing top level CMakeLists.txt.
      build_path (str): CMake build path (where project is built).
      toolchain (str): Path to CMake toolchain file. Differs based on os and/or
        platform.
      archive_output_path (str): Path to build and save libraries/frameworks to.
      targets (list(str)): CMake build targets. (eg: firebase_auth, etc)
      architecture (str, optional): Architecture passed onto the cmake build
        system. Used when building for ios only. (eg:'arm64', 'x86_64')
      toolchain_platform (str, optional): Platform cmake option passed for tvos
        builds only. Accepts all platforms supported by the tvos toolchain.
        (eg: 'TVOS', 'SIMULATOR_TVOS' etc)
  """
  cmd = ['cmake', '-S', source_path, '-B', build_path]
  cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(toolchain))
  cmd.append('-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY={0}'.format(archive_output_path))
  if architecture:
    cmd.append('-DCMAKE_OSX_ARCHITECTURES={0}'.format(architecture))
  elif toolchain_platform:
    cmd.append('-DPLATFORM={0}'.format(toolchain_platform))
  utils.run_command(cmd)


def cmake_build(build_path, targets):
  """CMake build which builds all libraries.

  Args:
      build_path (str): CMake build path (where project is built).
      targets (list(str)): CMake build targets. (eg: firebase_auth, etc)
  """
  # CMake build for os-platform-architecture
  cmd = ['cmake', '--build', build_path]
  cmd.append('--target')
  cmd.extend(targets)
  utils.run_command(cmd)


def main():
  args = parse_cmdline_args()
  if not os.path.isabs(args.build_dir):
    args.build_dir = os.path.abspath(args.build_dir)

  if not os.path.isabs(args.source_dir):
    args.source_dir = os.path.abspath(args.source_dir)

  frameworks_path = os.path.join(args.build_dir, 'frameworks')
  # List of cmake build process that we will be launched in parallel.
  processes = []
  for apple_os in args.os:
    logging.info("Building for {0}".format(apple_os))
    os_config = CONFIG.get(apple_os)
    if not os_config:
      raise ValueError('OS {0} not supported for building.'.format(apple_os))

    targets_from_config = set(os_config['supported_targets'])
    supported_targets = targets_from_config.intersection(args.target)
    if not supported_targets:
      raise ValueError('No supported targets found for {0}'.format(apple_os))

    frameworks_os_path = os.path.join(frameworks_path, apple_os)
    for platform_variant in args.platform_variant:
      os_platform_variant_config = os_config.get(platform_variant)
      if not os_platform_variant_config:
        raise ValueError('Could not find configuration for platform '
                         '{0} for os {1}'.format(platform_variant, apple_os))

      archs_from_config = set(os_platform_variant_config['architectures'])
      supported_archs = archs_from_config.intersection(args.architecture)
      if not supported_archs:
        raise ValueError('Could not find valid architectures for platform '
                         '{0} for os {1}'.format(platform_variant, apple_os))

      for architecture in supported_archs:
        platform_architecture_token = '{0}-{1}'.format(platform_variant,
                                                       architecture)
        archive_output_path = os.path.join(frameworks_os_path,
                                                platform_architecture_token)
        # Eg: <build_dir>/tvos_cmake_build/device-arm64
        build_path = os.path.join(args.build_dir,
                                  '{0}_cmake_build'.format(apple_os),
                                  platform_architecture_token)
        # For ios builds, we specify architecture to cmake configure.
        architecture = architecture if apple_os == 'ios' else None
        # For tvos builds, we pass a special cmake option PLATFORM to toolchain.
        toolchain_platform = os_platform_variant_config['toolchain_platform'] \
                             if apple_os == 'tvos' else None
        # CMake configure was having all sorts of issues when run in parallel.
        # It might be the Cocoapods that are downloaded in parallel into a
        # single cache directory.
        cmake_configure(args.source_dir, build_path,
                        os_platform_variant_config['toolchain'],
                        archive_output_path, architecture,
                        toolchain_platform)
        process = multiprocessing.Process(target=cmake_build,
                                          args=(build_path, supported_targets))
        processes.append((process, archive_output_path))

  # Launch all cmake build processes in parallel.
  for process, _ in processes:
    process.start()

  for process, archive_output_path in processes:
    process.join()
    # Reorganize frameworks (renaming, copying over headers etc)
    arrange_frameworks(archive_output_path)

  # Since we renamed firebase_app.framework to firebase.framework we add that
  # to our list of targets.
  targets = set(args.target)
  targets.add('firebase')

  # if we built for all architectures build universal framework as well.
  build_universal_framework(frameworks_path, targets)

  # Build xcframeworks
  xcframeworks_path = os.path.join(args.build_dir, 'xcframeworks')
  template_info_plist_path = os.path.join(args.source_dir, 'build_scripts',
                                          'tvos', 'Info_ios_and_tvos.plist')
  build_xcframeworks(frameworks_path, xcframeworks_path,
                     template_info_plist_path, targets)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Build for iOS and tvOS.')
  parser.add_argument('-b', '--build_dir',
    default='ios_tvos_build', help='Name of build directory.')
  parser.add_argument('-s', '--source_dir',
    default=os.getcwd(),
    help='Directory containing source code (top level CMakeLists.txt)')
  parser.add_argument('-v', '--platform_variant', nargs='+',
    default=('device', 'simulator'),
    help='List of platforms to build for.')
  parser.add_argument('-a', '--architecture', nargs='+',
    default=('arm64', 'armv7', 'x86_64', 'i386'),
    help='List of architectures to build for.')
  parser.add_argument('-t', '--target', nargs='+',
    default=( 'firebase_analytics', 'firebase_app_check',
              'firebase_auth', 'firebase_database',
              'firebase_dynamic_links', 'firebase_firestore',
              'firebase_functions', 'firebase_gma',
              'firebase_installations', 'firebase_messaging',
              'firebase_remote_config', 'firebase_storage'),
    help='List of CMake build targets')
  parser.add_argument('-o', '--os', nargs='+', default=('ios', 'tvos'),
    help='List of operating systems to build for.')
  parser.add_argument('--log_level', default='info',
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
  logging.getLogger(__name__)
  return args


if __name__ == '__main__':
  main()
