#!/usr/bin/env python

# Copyright 2019 Google
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
This script will take care of installing Python and C++ dependencies required for the build
It detects if an installation step is necessary and runs it only when required.

The goal is to avoid any major changes in system configurations of users. 
Almost all dependencies (except ninja/ccache) can be installed with vcpkg and are local to
the repo.

Requirements before running this script,

- Python and pip
- System package manager (if you plan to install ninja or ccache)
    Windows: choco (Chocolatey)
    Mac: brew (homebrew)
    Linux: apt (ubuntu's default package manager)

Usage examples:
# Standard usage
python scripts/install_prereqs.py

# If you want to install ccache via system package manager (cacche works only on mac and linux)
python scripts/install_prereqs.py --ccache

# If you want to install ninja via system package manager and an x86 build
# Could have used a boolean flag but this makes it easier to call from github workflow
python scripts/install_prereqs.py --generator "Ninja" --arch x86

# If you do not wish to use vcpkg (this will prompt you to take care of cpp dependencies yourself)
python scripts/install_prereqs.py --no_vcpkg
"""

import os
import sys
import argparse
import subprocess
import distutils.spawn
import pkg_resources


class PackageManager(object):
    WINDOWS = 'choco'
    LINUX = 'apt'
    MAC = 'brew'


def run_command(cmd, as_root_user=False, collect_output=False):
    """Run a command

    Args:
        cmd (:obj:`list` of :obj:`str`): Command to run as a list object
                                         Eg: ['ls', '-l']
        as_root_user (bool): Run command as root user with admin priveleges (supported on mac and linux)
        collect_output (bool): Get output from the command. The string returned from this function
                               call will have the output from command execution.
    Raises:
        (IOError): If command errored out
    Returns:
        (str|None): If collect_output is provided, a string is returned
                    None if output is not requested
    """
    if as_root_user and (is_mac_os() or is_linux_os()):
        cmd.insert(0, 'sudo')

    cmd_string = ' '.join(cmd)
    print('Running cmd: {0}\n'.format(cmd_string))
    try:
        if collect_output:
            output = subprocess.check_output(cmd)
            return output.decode('utf-8').strip()
        else:
            subprocess.call(cmd)
    except subprocess.CalledProcessError:
        raise IOError('Error executing {0}'.format(cmd_string))


def get_repo_root_dir():
    """ Get root directory of current git repo

    This function takes the guesswork out when executing commands
    or locating files relative to root of the repo

    Raises:
        IOError: Current directory is not in a git repo

    Returns:
        (str): Root directory of git repo
    """
    return run_command(['git', 'rev-parse', '--show-toplevel'], collect_output=True)


# Global constant (would have preferred lru_cache on functions but that breaks Python2 scripts and custom decorator
# breaks the simplicity of this script)
# Resorted to using these in order to cache root dir evaluation which is needed multiple times in script
ROOT_REPO_DIR = get_repo_root_dir()


def get_pip_requirements_file_path():
    """ Get absolute path to pip requirements file containing list of python packages to install

    Returns:
        (str): Absolute path to pip requirements file
    """
    return os.path.join(ROOT_REPO_DIR, 'external', 'pip_requirements.txt')


def get_vcpkg_root_dir():
    """ Get absolute path to vcpkg submodule in the repository

    Returns:
        (str): Absolute path to vcpkg submodule root directory
    """
    return os.path.join(ROOT_REPO_DIR, 'external', 'vcpkg')


def get_vcpkg_executable_file_path():
    """ Get absolute path to vcpkg executable
    
    Note: On Windows, exectuable file has a '.exe' extension
    Raises:
        ValueError: Unsupported operating system

    Returns:
        (str): Absolute path to vcpkg executable
    """
    if is_windows_os():
        vcpkg_executable_file_path = os.path.join(get_vcpkg_root_dir(), 'vcpkg.exe')
    elif is_linux_os() or is_mac_os():
        vcpkg_executable_file_path = os.path.join(get_vcpkg_root_dir(), 'vcpkg')
    else:
        raise ValueError('Unsupported operating system (supported os: windows, mac, linux')

    return vcpkg_executable_file_path


def get_vcpkg_response_file_path(vcpkg_triplet):
    """ Get absolute path to vcpkg response file
    Response file stores the list of packages to install with vcpkg

    Args:
        vcpkg_triplet (str): Name of the vcpkg triplet (eg: 'x64-windows-static', 'x64-osx')

    Returns:
        (str): Absoulte path to vcpkg response file
    """
    vcpkg_response_file_path = os.path.join(ROOT_REPO_DIR, 'external', 
                                            'vcpkg_' + vcpkg_triplet + '_response_file.txt')
    return vcpkg_response_file_path


def get_vcpkg_installation_script_path():
    """ Get absolute path to the script used to build and install vcpkg

    Raises:
        ValueError: Unsupported operating system

    Returns:
        (str): Absolute path to vcpkg installation script
    """
    if is_windows_os():
        script_absolute_path = os.path.join(get_vcpkg_root_dir(), 'bootstrap-vcpkg.bat')
    elif is_linux_os() or is_mac_os():
        script_absolute_path = os.path.join(get_vcpkg_root_dir(), 'bootstrap-vcpkg.sh')
    else:
        raise ValueError('Unsupported operating system (supported os: windows, mac, linux')

    return script_absolute_path


def is_tool_installed(tool):
    """Check if a tool is installed on the system

    Args:
        tool (str): Name of the tool

    Returns:
        (bool) : True if installed on the system, False otherwise
    """
    return distutils.spawn.find_executable(tool)


def is_windows_os():
    """Check if the current OS is Windows

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('win')


def is_linux_os():
    """Check if the current OS is Linux

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('linux')


def is_mac_os():
    """Check if the current OS is mac

    Returns:
        (bool): True if windows, False otherwise
    """
    return sys.platform.startswith('darwin')


def get_system_package_manager():
    """ Get package manger based on the platform
    brew for mac
    apt for linux
    choco for windows

    Raises:
        ValueError: If the current OS could not be identified

    Returns:
        (str): Package manager for current platform
    """
    if is_windows_os():
        if not is_tool_installed(PackageManager.WINDOWS):
            raise ValueError('Did not find {0} on the system. \
                                Please install it or \
                                try setting install_if_not_found=True'.format(PackageManager.WINDOWS))
        return PackageManager.WINDOWS

    elif is_mac_os():
        if not is_tool_installed(PackageManager.MAC):
            raise ValueError('Did not find {0} on the system. \
                            Please install it or \
                            try setting install_if_not_found=True'.format(PackageManager.MAC))
        return PackageManager.MAC

    elif is_linux_os():
        if not is_tool_installed(PackageManager.LINUX):
            raise ValueError('Did not find {0} on the system. \
                                Please install it or \
                                try setting install_if_not_found=True'.format(PackageManager.LINUX))
        return PackageManager.LINUX

    else:
        raise ValueError('Could not recognize OS')


def install_system_packages(package_names):
    """ Install packages using system package manager
    brew for mac
    apt for linux
    choco for windows

    Args:
        package_names (:obj:`list` of :obj:`str`): List of packages to install
                                                    Eg: ['ninja', 'ccache']
    Raises:
        (ValueError): When either package manager is not found or packages list is empty
    """
    if not package_names:
        print('No packages specified to install via system pacakge manager')
        return

    # Get current platform's system package manager
    package_manager = get_system_package_manager()
    if not package_manager:
        raise ValueError('No package manager found on the system. \
                         Could not install packages {0}'.format(" ".join(package_names)))
    cmd = [package_manager, 'install'] + package_names
    if is_linux_os():
        run_command(cmd, as_root_user=True)
    else:
        run_command(cmd)


def install_python_packages():
    """ Install python packages needed as dependencies for cpp sdk build
    """
    requirements_file_path = get_pip_requirements_file_path()
    cmd = ['python', '-m', 'pip', 'install', '-r', requirements_file_path, '--user']
    run_command(cmd)


def get_vcpkg_triplet(arch):
    """ Get vcpkg target triplet (platform definition)

    Args:
        arch (str): Architecture (eg: 'x86', 'x64')

    Raises:
        ValueError: If current OS is not win,mac or linux
    
    Returns:
        (str): Triplet name
               Eg: "x64-windows-static"
    """
    triplet_name = [arch]
    if is_windows_os():
        triplet_name.append('windows')
        # For windows, default is to build dynamic. Hence we specify static.
        # For mac/linux, default is to build static libraries
        triplet_name.append('static')
    elif is_mac_os():
        triplet_name.append('osx')
    elif is_linux_os():
        triplet_name.append('linux')
    else:
        raise ValueError('Unsupported operating system \
                          We currently support windows, mac, linux')

    return '-'.join(triplet_name)


def install_packages_with_vcpkg(arch, build_vcpkg=True):
    """Install packages with vcpkg

    This does the following,
        - initialize/update the vcpkg git submodule
        - build vcpkg executable
        - install packages via vcpkg
    Args:
        arch (str): Architecture (eg: 'x86', 'x64')
        install_vcpkg (bool): Build/Install vcpkg before building packages?
    """
    # vcpkg is a submodule of the main cpp repo
    # Ensure that the submodule is initialized and updated
    run_command(['git', 'submodule', 'init'])
    run_command(['git', 'submodule', 'update'])

    if build_vcpkg:
        # build and install vcpkg
        script_absolute_path = get_vcpkg_installation_script_path()
        run_command([script_absolute_path])
    else:
        print("Skipping building/installing vcpkg executable")

    # for each desktop platform, there is an existing vcpkg response file 
    # in the repo (external/vcpkg_<triplet>_response_file.txt) defined for each target triplet
    vcpkg_response_file_path = get_vcpkg_response_file_path(get_vcpkg_triplet(arch))
    vcpkg_executable_file_path = get_vcpkg_executable_file_path()

    run_command([vcpkg_executable_file_path, 'install', '@' + vcpkg_response_file_path, '--disable-metrics'])


def step_install_system_packages(args):
    """ Install packages (dependencies) with system package manager 

    Args:
        args (`argparse.Namespace`): Parsed command line arguments
    """
    system_packages_to_install = []
    # Install ninja and/or ccache if needed
    # check if ninja is already installed before attempting to install it
    if args.generator == 'Ninja':
        if is_tool_installed('ninja'):
            print('Ninja requested but is already installed. Skipping installation...')
        elif is_linux_os():
            system_packages_to_install.append('ninja-build')
        elif is_mac_os:
            system_packages_to_install.append('ninja')   

    # ccache is supported only on mac and linux and check if it is already installed
    if args.ccache and (is_linux_os() | is_mac_os()) and not is_tool_installed('ccache'):
        if is_tool_installed('ccache'):
            print('ccache requested but is already installed. Skipping installation...')
        elif is_linux_os() or is_mac_os():
            system_packages_to_install.append('ccache')

    if system_packages_to_install:
        install_system_packages(system_packages_to_install)


def step_install_python_packages(args):
    """ Install python packages (dependencies)

    Args:
        args (`argparse.Namespace`): Parsed command line arguments
    """
    # Check for packages installed in this python environment
    installed_packages = {pkg.key for pkg in pkg_resources.working_set}

    # Get the list of packages we intend to install
    requirements_file_path = get_pip_requirements_file_path()
    packages_to_install = None
    with open(requirements_file_path, 'r') as requirements_file:
        packages_to_install = {line.strip('\n') for line in requirements_file.readlines()}

    # Check if there is any work to do by comparing these lists
    if packages_to_install and not (packages_to_install-installed_packages):
        print('Skipping python packages installation because following '\
               'python packages are already installed: \n {0}'.format(' '.join(packages_to_install)))
        return

    install_python_packages()


def step_install_cpp_packages(args):
    """ Install C++ packages (dependencies)

    Installation supported only via vcpkg as system package manager can affect
    system wide packages. User should handle those themselves

    Args:
        args (`argparse.Namespace`): Parsed command line arguments
    """
    if not args.no_vcpkg:
        # Check if vcpkg is already installed
        found_vcpkg_executable = os.path.exists(get_vcpkg_executable_file_path())

        # Install packages and only install vcpkg if its not built already
        install_packages_with_vcpkg(args.arch, build_vcpkg=not found_vcpkg_executable)

    else:
        # Do not install dependencies automatically as we want users to have control
        # on their system level package installations
        print('Please install packages listed in {0}'\
              'with your system package manager'.format(get_vcpkg_response_file_path(get_vcpkg_triplet(args.arch))))


def main():
    args = parse_cmdline_args()

    step_install_system_packages(args)
    step_install_python_packages(args)
    step_install_cpp_packages(args)


def parse_cmdline_args():
    parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
    parser.add_argument('-A', '--arch', default='x64', help='Platform architecture (x64, x86)')
    parser.add_argument('-G', '--generator', help='Build generator to use (install ninja if needed)')
    parser.add_argument('-c', '--ccache', action='store_true', help='Install ccache using system package manager'\
                                                                    '(only supported on mac and linux)')
    parser.add_argument('-p', '--no_vcpkg', action='store_true', help='Use system package manager (brew, apt, choco) for dependencies(instead of vcpkg)')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    main()
