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
Helper functions that are shared amongst prereqs and build scripts across various
platforms.
"""

import distutils.spawn
import platform
import shutil
import subprocess
import os
import urllib.request

def run_command(cmd, capture_output=False, cwd=None, check=False, as_root=False,
                print_cmd=True):
 """Run a command.

 Args:
  cmd (list(str)): Command to run as a list object.
          Eg: ['ls', '-l'].
  capture_output (bool): Capture the output of this command.
                         Output can be accessed as <return_object>.stdout
  cwd (str): Directory to execute the command from.
  check (bool): Raises a CalledProcessError if True and the command errored out
  as_root (bool): Run command as root user with admin priveleges (supported on mac and linux).
  print_cmd (bool): Print the command we are running to stdout.

 Raises:
  (subprocess.CalledProcessError): If command errored out and `text=True`

 Returns:
  (`subprocess.CompletedProcess`): object containing information from
                                   command execution
 """

 if as_root and (is_mac_os() or is_linux_os()):
   cmd.insert(0, 'sudo')

 cmd_string = ' '.join(cmd)
 if print_cmd:
  print('Running cmd: {0}\n'.format(cmd_string))
 # If capture_output is requested, we also set text=True to store the returned value of the
 # command as a string instead of bytes object
 return subprocess.run(cmd, capture_output=capture_output, cwd=cwd,
                       check=check, text=capture_output)


def is_command_installed(tool):
  """Check if a command is installed on the system."""
  return distutils.spawn.find_executable(tool)


def delete_directory(dir_path):
  """Recursively delete a valid directory"""
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def download_file(url, file_path):
  """Download from url and save to specified file path."""
  with urllib.request.urlopen(url) as response, open(file_path, 'wb') as out_file:
    shutil.copyfileobj(response, out_file)


def unpack_files(archive_file_path, output_dir=None):
  """Unpack/extract an archive to specified output_directory"""
  shutil.unpack_archive(archive_file_path, output_dir)


def is_windows_os():
 return platform.system() == 'Windows'


def is_mac_os():
 return platform.system() == 'Darwin'


def is_linux_os():
 return platform.system() == 'Linux'


def copy_vcpkg_custom_data():
  """Copy custom files for vcpkg to vcpkg directory."""
  # Since vcpkg is a submodule in the cpp sdk repo, we cannot just keep our custom
  # files in the external/vcpkg directory. That would require committing to the
  # vcpkg submodule. Instead we keep the data in a separate directory and copy it
  # over after vcpkg submodule is initialized.
  custom_data_root_path = os.path.join(os.getcwd(), 'external', 'vcpkg_custom_data')
  destination_dirs_map = {
    'triplets': os.path.join(get_vcpkg_root_path(), 'triplets'),
    'toolchains': os.path.join(get_vcpkg_root_path(), 'scripts', 'buildsystems')
  }
  for custom_data_subdir in destination_dirs_map:
    custom_data_subdir_abspath = os.path.join(custom_data_root_path, custom_data_subdir)
    destination_dir = destination_dirs_map[custom_data_subdir]
    for custom_file in os.listdir(custom_data_subdir_abspath):
      abspath = os.path.join(custom_data_subdir_abspath, custom_file)
      shutil.copy(abspath, destination_dir)


def get_vcpkg_triplet(arch, msvc_runtime_library='static'):
  """ Get vcpkg target triplet (platform definition).

  Args:
    arch (str): Architecture (eg: 'x86', 'x64', 'arm64').
    msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').

  Raises:
    ValueError: If current OS is not win,mac or linux.

  Returns:
    (str): Triplet name.
       Eg: "x64-windows-static".
  """
  triplet_name = [arch]
  if is_windows_os():
    triplet_name.append('windows')
    triplet_name.append('static')
    if msvc_runtime_library == 'dynamic':
      triplet_name.append('md')
  elif is_mac_os():
    triplet_name.append('osx')
  elif is_linux_os():
    triplet_name.append('linux')

  triplet_name = '-'.join(triplet_name)
  print("Using vcpkg triplet: {0}".format(triplet_name))
  return triplet_name


def get_vcpkg_root_path():
  """Get absolute path to vcpkg root directory in repo."""
  return os.path.join(os.getcwd(), 'external', 'vcpkg')


def get_vcpkg_executable_file_path():
  """Get absolute path to vcpkg executable."""
  vcpkg_root_dir = get_vcpkg_root_path()
  if is_windows_os():
    vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg.exe')
  elif is_linux_os() or is_mac_os():
    vcpkg_executable_file_path = os.path.join(vcpkg_root_dir, 'vcpkg')
  return vcpkg_executable_file_path


def get_vcpkg_installation_script_path():
  """Get absolute path to the script used to build and install vcpkg."""
  vcpkg_root_dir = get_vcpkg_root_path()
  if is_windows_os():
    script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.bat')
  elif is_linux_os() or is_mac_os():
    script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.sh')

  return script_absolute_path


def verify_vcpkg_build(vcpkg_triplet, attempt_auto_fix=False):
  """Check if vcpkg installation finished successfully.

  Args:
    vcpkg_triplet (str): Triplet name for vcpkg. Eg: 'x64-linux'
    attempt_auto_fix (bool): If installation failed, try fixing some errors.

  Returns:
    (bool) True if everything looks good
           False if installation failed but auto fix was attempted.
                 Caller should retry installation in this case.
  Raises:
    (ValueError) Installation failed and auto fix was not attempted
  """
  # At the very least, we should have an "installed" directory under vcpkg triplet.
  vcpkg_root_dir_path = get_vcpkg_root_path()
  installed_triplets_dir_path = os.path.join(vcpkg_root_dir_path, 'installed', vcpkg_triplet)
  if not os.path.exists(installed_triplets_dir_path):
    if is_windows_os() and attempt_auto_fix:
      # On some Windows machines with NFS drives, we have seen errors
      # installing vcpkg due to permission issues while renaming temp directories.
      # Manually renaming and re-running script makes it go through.
      tools_dir_path = os.path.join(vcpkg_root_dir_path, 'downloads', 'tools')
      for name in os.listdir(tools_dir_path):
        # In the specific windows error that we noticed, the error occurs while
        # trying to rename intermediate directories for donwloaded tools
        # like "powershell.partial.<pid>" to "powershell". Renaming via python
        # also runs into the same error. Workaround is to copy instead of rename.
        if '.partial.' in name and os.path.isdir(os.path.join(tools_dir_path, name)):
          expected_name = name.split('.partial.')[0]
          shutil.copytree(os.path.join(tools_dir_path, name),
                          os.path.join(tools_dir_path, expected_name))
      return False

    raise ValueError("Could not find directory containing installed packages by vcpkg: {0}\n"
                     "Please check if there were errors during "
                     "vcpkg installation.".format(installed_triplets_dir_path))
  return True


def clean_vcpkg_temp_data():
  """Delete files/directories that vcpkg uses during its build"""
  # Clear temporary directories and files created by vcpkg buildtrees
  # could be several GBs and cause github runners to run out of space
  directories_to_remove = ['buildtrees', 'downloads', 'packages']
  vcpkg_root_dir_path = get_vcpkg_root_path()
  for directory_to_remove in directories_to_remove:
    abspath = os.path.join(vcpkg_root_dir_path, directory_to_remove)
    delete_directory(abspath)


def install_x86_support_libraries(gha_build=False):
  """Install support libraries needed to build x86 on x86_64 hosts.

  Args:
    gha_build: Pass in True if running on a GitHub runner; this will activate
               workarounds that might be undesirable on a personal system (e.g.
               downgrading Ubuntu packages).
  """
  if is_linux_os():
    packages = ['gcc-multilib', 'g++-multilib', 'libglib2.0-dev:i386',
                'libsecret-1-dev:i386', 'libpthread-stubs0-dev:i386',
                'libssl-dev:i386', 'libsecret-1-0:i386']
    remove_packages = []

    # First check if these packages exist on the machine already
    with open(os.devnull, "w") as devnull:
      process = subprocess.run(["dpkg", "-s"] + packages, stdout=devnull,
                               stderr=subprocess.STDOUT)

    if process.returncode != 0:
      # This implies not all of the required packages are already installed on
      # user's machine. Install them.
      run_command(['dpkg', '--add-architecture', 'i386'], as_root=True,
                  check=True)
      run_command(['apt', 'update'], as_root=True, check=True)
      run_command(['apt', 'install', 'aptitude'], as_root=True, check=True)
      if gha_build:
        # Remove libpcre to prevent package conflicts.
        # Only remove packages on GitHub runners.
        remove_packages = ['libpcre2-dev:amd64', 'libpcre2-32-0:amd64',
                           'libpcre2-8-0:amd64', 'libpcre2-16-0:amd64']
      # Note: With aptitude, you can remove package 'xyz' by specifying 'xyz-'
      # in the package list.
      run_command(['aptitude', 'install', '-V', '-y'] + packages +
                  ['%s-' % pkg for pkg in remove_packages], as_root=True, check=True)

      # Check if the packages were installed
      with open(os.devnull, "w") as devnull:
        subprocess.run(["dpkg", "-s"] + packages, stdout=devnull, stderr=subprocess.STDOUT,
                       check=True)

