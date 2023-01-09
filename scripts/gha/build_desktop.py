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
"""
Assuming pre-requisites are in place (from running
`scripts/gha/install_prereqs_desktop.py`), this builds the firebase cpp sdk.

It does the following,
- install desktop platform specific cpp dependencies via vcpkg
- cmake configure
- cmake build

Usage examples:
# Build all targets with x64 architecture, Release config and 8 cpu cores
python scripts/gha/build_desktop.py --arch x64 --config Release -j 8

# Build all targets with default options and also build unit tests
python scripts/gha/build_desktop.py --build_tests --arch x64

# Build only firebase_app and firebase_auth
python scripts/gha/build_desktop.py --target firebase_app firebase_auth

"""

import argparse
import os
import utils
import shutil
import subprocess
import sys

def append_line_to_file(path, line):
  """Append the given line to the end of the file if it's not already in the file.

  Used to disable specific errors in CMakeLists files if needed.

  Args:
    path: File to edit.
    line: String to add to the end of the file.
  """
  with open(path, "r") as file:
    lines = file.readlines()
  if line not in lines:
    with open(path, "a") as file:
      file.write("\n" + line + "\n")


def _install_cpp_dependencies_with_vcpkg(arch, msvc_runtime_library, use_openssl=False):
  """Install packages with vcpkg.

  This does the following,
    - initialize/update the vcpkg git submodule
    - build vcpkg executable
    - install packages via vcpkg.
  Args:
    arch (str): Architecture (eg: 'x86', 'x64', 'arm64').
    msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').
    use_openssl (bool): Use OpenSSL based vcpkg response files.
  """

  # Install vcpkg executable if its not installed already
  vcpkg_executable_file_path = utils.get_vcpkg_executable_file_path()
  found_vcpkg_executable = os.path.exists(vcpkg_executable_file_path)
  if not found_vcpkg_executable:
    script_absolute_path = utils.get_vcpkg_installation_script_path()
    # Example: ./external/vcpkg/bootstrap-sh
    utils.run_command([script_absolute_path], check=True)

  # Copy any of our custom defined vcpkg data to vcpkg submodule directory
  utils.copy_vcpkg_custom_data()

  # for each desktop platform, there exists a vcpkg response file in the repo
  # (external/vcpkg_<triplet>_response_file.txt) defined for each target triplet
  vcpkg_triplet = utils.get_vcpkg_triplet(arch, msvc_runtime_library)
  vcpkg_response_files_dir_path = os.path.join(os.getcwd(), 'external', 'vcpkg_custom_data',
                      'response_files')
  if use_openssl:
    vcpkg_response_files_dir_path = os.path.join(vcpkg_response_files_dir_path, 'openssl')

  vcpkg_response_file_path = os.path.join(vcpkg_response_files_dir_path,
                                          '{0}.txt'.format(vcpkg_triplet))

  # Eg: ./external/vcpkg/vcpkg install @external/vcpkg_x64-osx_response_file.txt
  # --disable-metrics
  utils.run_command([vcpkg_executable_file_path, 'install',
                     '@' + vcpkg_response_file_path, '--disable-metrics'],
                    check=True)


def install_cpp_dependencies_with_vcpkg(arch, msvc_runtime_library, cleanup=True,
                                        use_openssl=False):
  """Install packages with vcpkg and optionally cleanup any intermediates.

  This is a wrapper over a low level installation function and attempts the
  installation twice, a second time after attempting to auto fix known issues.

  Args:
    arch (str): Architecture (eg: 'x86', 'x64', 'arm64').
    msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').
    cleanup (bool): Clean up intermediate files used during installation.
    use_openssl (bool): Use OpenSSL based vcpkg response files.

  Raises:
    (ValueError) If installation wasn't successful.
  """
  _install_cpp_dependencies_with_vcpkg(arch, msvc_runtime_library, use_openssl)
  vcpkg_triplet = utils.get_vcpkg_triplet(arch, msvc_runtime_library)
  # Verify the installation with an attempt to auto fix any issues.
  success = utils.verify_vcpkg_build(vcpkg_triplet, attempt_auto_fix=True)
  if not success:
    print("Installation was not successful but auto fix was attempted. "
          "Retrying installation...")
    # Retry once more after attempted auto fix.
    _install_cpp_dependencies_with_vcpkg(arch, msvc_runtime_library)
    # Check for success again. If installation failed, this call will raise a ValueError.
    success = utils.verify_vcpkg_build(vcpkg_triplet, attempt_auto_fix=False)

  if cleanup:
    # Clear temporary directories and files created by vcpkg buildtrees
    # could be several GBs and cause github runners to run out of space
    utils.clean_vcpkg_temp_data()

def cmake_configure(build_dir, arch, msvc_runtime_library='static', linux_abi='legacy',
                    build_tests=True, config=None, target_format=None,
                    use_openssl=False, disable_vcpkg=False, firestore_dep_source=None,
                    gha_build=False, verbose=False):
  """ CMake configure.

  If you are seeing problems when running this multiple times,
  make sure to clean/delete previous build directory.

  Args:
   build_dir (str): Output build directory.
   arch (str): Platform Architecture (example: 'x64', 'x86', 'arm64').
   msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').
   linux_abi (str): Linux ABI (eg: 'legacy', 'c++11').
   build_tests (bool): Build cpp unit tests.
   config (str): Release/Debug config.
          If its not specified, cmake's default is used (most likely Debug).
   target_format (str): If specified, build for this targetformat ('frameworks' or 'libraries').
   use_openssl (bool) : Use prebuilt OpenSSL library instead of using boringssl
                        downloaded and built during the cmake configure step.
   disable_vcpkg (bool): If True, skip vcpkg and just use CMake for deps.
   firestore_dep_source (str): Where to get Firestore C++ Core source code.
   gha_build (bool): If True, this build will be marked as having been built
                     from GitHub, which is useful for metrics tracking.
   verbose (bool): If True, enable verbose mode in the CMake file.
  """
  cmd = ['cmake', '-S', '.', '-B', build_dir]

  # If generator is not specifed, default for platform is used by cmake, else
  # use the specified value
  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))
  if build_tests:
    cmd.append('-DFIREBASE_CPP_BUILD_TESTS=ON')
    cmd.append('-DFIREBASE_FORCE_FAKE_SECURE_STORAGE=ON')
  else:
    # workaround, absl doesn't build without tests enabled
    cmd.append('-DBUILD_TESTING=off')

  if disable_vcpkg:
    if utils.is_linux_os() and arch == 'x86':
      toolchain_file_path = os.path.join(os.getcwd(), 'cmake', 'toolchains', 'linux_32.cmake')
      cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(toolchain_file_path))
  else: # not disable_vcpkg - therefore vcpkg is enabled
    if utils.is_linux_os() and arch == 'x86':
      # Use a separate cmake toolchain for cross compiling linux x86 builds
      vcpkg_toolchain_file_path = os.path.join(os.getcwd(), 'external', 'vcpkg',
                                               'scripts', 'buildsystems', 'linux_32.cmake')
    elif utils.is_mac_os() and arch == 'arm64':
      vcpkg_toolchain_file_path = os.path.join(os.getcwd(), 'external', 'vcpkg',
                                               'scripts', 'buildsystems', 'macos_arm64.cmake')
    else:
      vcpkg_toolchain_file_path = os.path.join(os.getcwd(), 'external',
                                               'vcpkg', 'scripts',
                                               'buildsystems', 'vcpkg.cmake')
    cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))
    vcpkg_triplet = utils.get_vcpkg_triplet(arch, msvc_runtime_library)
    cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(vcpkg_triplet))

  if utils.is_windows_os():
    # If building for x86, we should supply -A Win32 to cmake configure
    # Its a good habit to specify for x64 too as the default might be different
    # on different windows machines.
    cmd.append('-A')
    cmd.append('Win32') if arch == 'x86' else cmd.append('x64')
    # Also, for Windows, specify the path to Python.
    cmd.append('-DFIREBASE_PYTHON_HOST_EXECUTABLE:FILEPATH=%s' % sys.executable)

    # Use our special cmake flag to specify /MD vs /MT
    if msvc_runtime_library == "static":
      cmd.append('-DMSVC_RUNTIME_LIBRARY_STATIC=ON')

  if utils.is_mac_os():
    if (arch == 'arm64'):
      cmd.append('-DCMAKE_OSX_ARCHITECTURES=arm64')
    else:
      cmd.append('-DCMAKE_OSX_ARCHITECTURES=x86_64')

  if utils.is_linux_os() and linux_abi == 'c++11':
      cmd.append('-DFIREBASE_LINUX_USE_CXX11_ABI=TRUE')

  if (target_format):
    cmd.append('-DFIREBASE_XCODE_TARGET_FORMAT={0}'.format(target_format))

  if not use_openssl:
    cmd.append('-DFIREBASE_USE_BORINGSSL=ON')

  if firestore_dep_source:
    cmd.append('-DFIRESTORE_DEP_SOURCE={0}'.format(firestore_dep_source))

  # When building from GitHub Actions, this should always be set.
  if gha_build:
    cmd.append('-DFIREBASE_GITHUB_ACTION_BUILD=ON')

  # Print out every command while building.
  if verbose:
    cmd.append('-DCMAKE_VERBOSE_MAKEFILE=1')

  utils.run_command(cmd, check=True)

def main():
  args = parse_cmdline_args()

  # Ensure that the submodules are initialized and updated
  # Example: vcpkg is a submodule (external/vcpkg)
  if not args.disable_vcpkg:
    utils.run_command(['git', 'submodule', 'init'], check=True)
    utils.run_command(['git', 'submodule', 'update'], check=True)

  # To build x86 on x86_64 linux hosts, we also need x86 support libraries
  if args.arch == 'x86' and utils.is_linux_os():
    utils.install_x86_support_libraries(args.gha_build)

  # Install C++ dependencies using vcpkg
  if not args.disable_vcpkg:
    # Install C++ dependencies using vcpkg
    install_cpp_dependencies_with_vcpkg(args.arch, args.msvc_runtime_library,
                                        cleanup=True, use_openssl=args.use_openssl)

  if args.vcpkg_step_only:
    print("Exiting without building the Firebase C++ SDK as just vcpkg step was requested.")
    return

  # CMake configure
  cmake_configure(args.build_dir, args.arch, args.msvc_runtime_library, args.linux_abi,
                  args.build_tests, args.config, args.target_format,
                  args.use_openssl, args.disable_vcpkg, args.firestore_dep_source, args.gha_build, args.verbose)

  # CMake build
  # cmake --build build -j 8
  cmd = ['cmake', '--build', args.build_dir, '-j', str(os.cpu_count()),
         '--config', args.config]

  if args.target:
    # Example:  cmake --build build -j 8 --target firebase_app firebase_auth
    cmd.append('--target')
    cmd.extend(args.target)
  utils.run_command(cmd, check=True)


def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
  parser.add_argument('-a', '--arch', default='x64', help='Platform architecture (x64, x86, arm64)')
  parser.add_argument('--msvc_runtime_library', default='static',
                      help='Runtime library for MSVC (static(/MT) or dynamic(/MD)')
  parser.add_argument('--linux_abi', default='legacy',
                      help='C++ ABI for Linux (legacy or c++11)')
  parser.add_argument('--build_dir', default='build', help='Output build directory')
  parser.add_argument('--build_tests', action='store_true', help='Build unit tests too')
  parser.add_argument('--verbose', action='store_true', help='Enable verbose CMake builds.')
  parser.add_argument('--disable_vcpkg', default='True', action='store_true', help='Disable vcpkg and just use CMake.')
  parser.add_argument('--vcpkg_step_only', action='store_true', help='Just install cpp packages using vcpkg and exit.')
  parser.add_argument('--config', default='Release', help='Release/Debug config')
  parser.add_argument('--target', nargs='+', help='A list of CMake build targets (eg: firebase_app firebase_auth)')
  parser.add_argument('--target_format', default=None, help='(Mac only) whether to output frameworks (default) or libraries.')
  parser.add_argument('--use_openssl', action='store_true', default=None, help='Use openssl for build instead of boringssl')
  parser.add_argument('--firestore_dep_source', default='RELEASED', help='Where to get Firestore C++ Core source code. "RELEASED"/"TIP"/(Git tag/branch/commit)')
  parser.add_argument('--gha_build', action='store_true', default=None, help='Set to true when building on GitHub, for metric tracking purposes (also changes some prerequisite installation behavior).')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()
