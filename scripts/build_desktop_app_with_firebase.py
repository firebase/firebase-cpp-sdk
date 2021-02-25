#!/usr/bin/env python

# Copyright 2020 Google
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
"""Standalone script to build desktops apps with Firebase.

Standalone self-sufficient (no external dependencies) python script that can
ease building desktop apps with Firebase, by either using the C++ source
(firebase-cpp-sdk github repo) or the prebuilt release Firebase libraries.

Note that this script works with only Python3 (3.6+).
Also note, that since this script runs a cmake configure step, it is advised to
run it with a fresh clean build directory.

Known side effects:
If building against Firebase cpp source, this script might checkout a specific
branch on github containing vcpkg. This will not be required once vcpkg is in
the main branch.

Example usage:
Let's say we want to build the quickstart cpp example for Firebase database.
As specified above, there are 2 options - build against the Firebase source or
prebuilt Firebase libraries.

# Build against the Firebase cpp sdk source
python3 scripts/build_desktop_app_with_firebase.py
                      --app_dir ~/quickstart-cpp/database/testapp
                      --sdk_dir .
                      --build_dir build_source

(or)

# Build against the prebuilt released Firebase libraries
python3 scripts/build_desktop_app_with_firebase.py
                      --app_dir ~/quickstart-cpp/database/testapp
                      --sdk_dir ~/prebuilt/firebase_cpp_sdk_6.15.1/
                      --build_dir build_prebuilt

# If the script ran successfully, it will print the path to the build directory.
# The output looks like the following,
Build successful!
Please find your executables in build directory:
/Users/<user>/quickstart-cpp/database/testapp/build_source

# Running the built example
$ ./Users/<user>/quickstart-cpp/database/testapp/build_source/desktop_testapp
"""
import argparse
import os
import platform
import subprocess
import sys


def is_path_valid_for_cmake(path):
  """Check if specified path is setup for cmake."""
  return os.path.exists(os.path.join(path, 'CMakeLists.txt'))


def is_sdk_path_source(sdk_dir):
  """Validate if Firebase sdk dir is Firebase cpp source dir."""
  # Not the most reliable way to detect if the sdk path is source or prebuilt
  # but should work for our purpose.
  return os.path.exists(os.path.join(sdk_dir, 'build_tools'))


def validate_prebuilt_args(arch, config):
  """Validate cmd line args for build with prebuilt libraries."""
  # Some options are not available when using prebuilt libraries.
  if platform.system() == 'Darwin':
    if arch == 'x86' or config == 'Debug':
      raise ValueError('Prebuilt mac Firebase libraries are built for x64 and '
                       'Release mode only. '
                       'Please fix the command line arguments and try again')

  if platform.system() == 'Linux':
    if config == 'Debug':
      raise ValueError('Prebuilt linux Firebase libraries are built with '
                       'Release mode only. Please fix the --config command '
                       'line argument and try again.')


def get_vcpkg_triplet(arch, msvc_runtime_library='static'):
  """Get vcpkg target triplet (platform definition).

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
    msvc_runtime_library (str): Runtime library for MSVC.
                                (eg: 'static', 'dynamic').

  Raises:
    ValueError: If current OS is not win, mac or linux.

  Returns:
    (str): Triplet name.
       Eg: "x64-windows-static".
  """
  triplet_name = [arch]
  if platform.system() == 'Windows':
    triplet_name.append('windows')
    triplet_name.append('static')
    if msvc_runtime_library == 'dynamic':
      triplet_name.append('md')
  elif platform.system() == 'Darwin':
    triplet_name.append('osx')
  elif platform.system() == 'Linux':
    triplet_name.append('linux')
  else:
    raise ValueError('Unsupported platform. This function works only on '
                     'Windows, Linux or Mac platforms.')

  triplet_name = '-'.join(triplet_name)
  print('Using vcpkg triplet: {0}'.format(triplet_name))
  return triplet_name


def build_source_vcpkg_dependencies(sdk_source_dir, arch, msvc_runtime_library):
  """Build C++ dependencies for Firebase source SDK using vcpkg.

  Args:
   sdk_source_dir (str): Path to Firebase C++ source directory.
   arch (str): Platform Architecture (eg: 'x64').
   msvc_runtime_library (str): Runtime library for MSVC.
                               (eg: 'static', 'dynamic').
  """
  # TODO(b/174141707): Remove this once dev branch of firebase-cpp-sdk repo has
  # been merged onto main branch. This is required because vcpkg lives only in
  # dev branch currently.
  subprocess.run(['git', 'checkout', 'dev'],
                 cwd=sdk_source_dir, check=True)
  subprocess.run(['git', 'pull'], cwd=sdk_source_dir, check=True)

  # sys.executable should point to the python bin running this script. We use
  # the same executable to execute these subprocess python scripts.
  subprocess.run([sys.executable, 'scripts/gha/install_prereqs_desktop.py'],
                 cwd=sdk_source_dir, check=True)
  subprocess.run([sys.executable, 'scripts/gha/build_desktop.py',
                  '--arch', arch,
                  '--msvc_runtime_library', msvc_runtime_library,
                  '--vcpkg_step_only'], cwd=sdk_source_dir, check=True)


def build_app_with_source(app_dir, sdk_source_dir, build_dir, arch,
                          msvc_runtime_library='static', config=None,
                          target_format=None):
  """Build desktop app directly against the Firebase C++ SDK source.

  Since this involves a cmake configure, it is advised to run this on a clean
  build directory.

  Args:
   app_dir (str): Path to directory containing application's CMakeLists.txt.
   sdk_source_dir (str): Path to Firebase C++ SDK source directory.
                         (root of firebase-cpp-sdk github repo).
   build_dir (str): Output build directory.
   arch (str): Platform Architecture (example: 'x64').
   msvc_runtime_library (str): Runtime library for MSVC.
                               (eg: 'static', 'dynamic').
   config (str): Release/Debug config.
          If it's not specified, cmake's default is used (most likely Debug).
   target_format (str): If specified, build for this target format.
                       ('frameworks' or 'libraries').
  """
  # Cmake configure.
  cmd = ['cmake', '-S', '.', '-B', build_dir]
  cmd.append('-DFIREBASE_CPP_SDK_DIR={0}'.format(sdk_source_dir))

  # If generator is not specified, default for platform is used by cmake, else
  # use the specified value.
  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))
    # workaround, absl doesn't build without tests enabled.
    cmd.append('-DBUILD_TESTING=off')

  if platform.system() == 'Linux' and arch == 'x86':
    # Use a separate cmake toolchain for cross compiling linux x86 builds.
    vcpkg_toolchain_file_path = os.path.join(sdk_source_dir, 'external',
                                             'vcpkg', 'scripts',
                                             'buildsystems', 'linux_32.cmake')
  else:
    vcpkg_toolchain_file_path = os.path.join(sdk_source_dir, 'external',
                                             'vcpkg', 'scripts',
                                             'buildsystems', 'vcpkg.cmake')

  cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))

  vcpkg_triplet = get_vcpkg_triplet(arch, msvc_runtime_library)
  cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(vcpkg_triplet))

  if platform.system() == 'Windows':
    # If building for x86, we should supply -A Win32 to cmake configure.
    # Since the default architecture for cmake varies from machine to machine,
    # it is a good practice to specify it all the time (even for x64).
    cmd.append('-A')
    windows_cmake_arch_flag_value = 'Win32' if arch == 'x86' else 'x64'
    cmd.append(windows_cmake_arch_flag_value)

    # Use our special cmake option to specify /MD (dynamic) vs /MT (static).
    if msvc_runtime_library == 'static':
      cmd.append('-DMSVC_RUNTIME_LIBRARY_STATIC=ON')

  if target_format:
    cmd.append('-DFIREBASE_XCODE_TARGET_FORMAT={0}'.format(target_format))
  print('Running {0}'.format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

  # CMake build.
  num_cpus = str(os.cpu_count())
  cmd = ['cmake', '--build', build_dir, '-j', num_cpus, '--config', config]
  print('Running {0}'.format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)


def build_app_with_prebuilt(app_dir, sdk_prebuilt_dir, build_dir, arch,
                            msvc_runtime_library='static', config=None):
  """Build desktop app directly against the prebuilt Firebase C++ libraries.

  Since this involves a cmake configure, it is advised to run this on a clean
  build directory.

  Args:
   app_dir (str): Path to directory containing application's CMakeLists.txt.
   sdk_prebuilt_dir (str): Path to prebuilt Firebase C++ libraries.
   build_dir (str): Output build directory.
   arch (str): Platform Architecture (eg: 'x64').
   msvc_runtime_library (str): Runtime library for MSVC.
                               (eg: 'static', 'dynamic').
   config (str): Release/Debug config (eg: 'Release', 'Debug')
             If it's not specified, cmake's default is used (most likely Debug).
  """

  cmd = ['cmake', '-S', '.', '-B', build_dir]
  cmd.append('-DFIREBASE_CPP_SDK_DIR={0}'.format(sdk_prebuilt_dir))

  if platform.system() == 'Windows':
    if arch == 'x64':
      cmd.append('-DCMAKE_CL_64=ON')
    if msvc_runtime_library == 'dynamic':
      cmd.append('-DMSVC_RUNTIME_MODE=MD')
    else:
      cmd.append('-DMSVC_RUNTIME_MODE=MT')

  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))

  print('Running {0}'.format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

  # CMake build.
  num_cpus = str(os.cpu_count())
  cmd = ['cmake', '--build', build_dir, '-j', num_cpus, '--config', config]
  print('Running {0}'.format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)


def main():
  args = parse_cmdline_args()

  if not is_path_valid_for_cmake(args.sdk_dir):
    print('SDK path provided is not valid. '
          'Could not find a CMakeLists.txt at the root level.\n'
          'Please check the argument to "--sdk_dir".')
    sys.exit(1)

  if not is_path_valid_for_cmake(args.app_dir):
    print('App path provided is not valid. '
          'Could not find a CMakeLists.txt at the root level.\n'
          'Please check the argument to "--app_dir"')
    sys.exit(1)

  if is_sdk_path_source(args.sdk_dir):
    print('SDK path provided is a Firebase C++ source directory. Building...')
    build_source_vcpkg_dependencies(args.sdk_dir, args.arch,
                                    args.msvc_runtime_library)
    build_app_with_source(args.app_dir, args.sdk_dir, args.build_dir, args.arch,
                          args.msvc_runtime_library, args.config,
                          args.target_format)
  else:
    validate_prebuilt_args(args.arch, args.config)
    print('SDK path provided is Firebase C++ prebuilt libraries. Building...')
    build_app_with_prebuilt(args.app_dir, args.sdk_dir, args.build_dir,
                            args.arch, args.msvc_runtime_library, args.config)

  print('Build successful!\n'
        'Please find your executables in the build directory: {0}'.format(
            os.path.join(args.app_dir, args.build_dir)))


def parse_cmdline_args():
  """Parse command line arguments."""
  parser = argparse.ArgumentParser(description='Install Prerequisites for '
                                               'building cpp sdk.')
  parser.add_argument('--sdk_dir', help='Path to Firebase SDK - source or '
                                        'prebuilt libraries.',
                      type=os.path.abspath)
  parser.add_argument('--app_dir', help="Path to application to build "
                      "(directory containing application's CMakeLists.txt",
                      type=os.path.abspath)
  parser.add_argument('-a', '--arch', default='x64',
                      help='Platform architecture (x64, x86)')
  parser.add_argument('--msvc_runtime_library', default='static',
                      help='Runtime library for MSVC '
                           '(static(/MT) or dynamic(/MD)')
  parser.add_argument('--build_dir', default='build',
                      help='Output build directory')
  parser.add_argument('--config', default='Release',
                      help='Release/Debug config')
  parser.add_argument('--target_format', default=None,
                      help='(Mac only) whether to output frameworks (default)'
                           'or libraries.')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
