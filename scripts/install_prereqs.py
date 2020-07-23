#!/usr/bin/python
"""
This script will take care of installing python and C++ dependencies before 
starting the build. 

The goal is to avoid any major changes in system configurations of users. 
With vcpkg, we pretty much download almost all of the dependencies within the repo directory
itself. But we can't install ninja or ccache with vcpkg. Hence, if you plan to use those
in your builds, you have to install them via a system package manager

Requirements before running this script,

- Python and pip
- System package manager (if you plan to use ninja or ccache)
    Windows: choco (Chocolatey)
    Mac: brew (homebrew)
    Linux: apt (ubuntu's default package manager)
"""
import os
import sys
import argparse
import subprocess
import distutils.spawn


class PackageManager(object):
    WINDOWS = 'choco'
    LINUX = 'apt'
    MAC = 'brew'


def is_tool_installed(tool):
    """Check if a tool is installed on the system

    Args:
        tool (str): Name of the tool

    Returns:
        (bool) : True if installed on the system, False otherwise
    """
    return distutils.spawn.find_executable(tool)


def run_command(cmd, as_root_user=False, collect_output=False):
    """Run a command

    Args:
        cmd (:obj:`list` of :obj:`str`): Command to run as a list object
                                         Eg: ['ls', '-l']
        as_root_user (bool): Run command as root user with admin priveleges (supported on mac and linux)
        collect_output (bool): Get output from the command
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
        run_command(cmd, as_root=True)
    else:
        run_command(cmd)


def install_python_dependencies():
    """ Install python packages needed as dependencies for cpp sdk build
    """
    repo_root_dir = get_repo_root_dir()
    requirements_file = os.path.join(repo_root_dir, 'external', 'pip_requirements.txt')
    cmd = ['python', '-m', 'pip', 'install', '-r', requirements_file, '--user']
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


def install_packages_with_vcpkg(arch):
    """Install packages with vcpkg

    This does the following,
        - initialize/update the vcpkg git submodule
        - build vcpkg executable
        - install packages via vcpkg
    Args:
        arch (str): Architecture (eg: 'x86', 'x64')
    """
    # vcpkg is a submodule of the main cpp repo
    # Ensure that the submodule is initialized and updated
    run_command(['git', 'submodule', 'init'])
    run_command(['git', 'submodule', 'update'])

    # install vcpkg
    repo_root_dir = get_repo_root_dir()
    vcpkg_root_dir = os.path.join(repo_root_dir, 'external', 'vcpkg')
    if is_windows_os():
        script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.bat')
        run_command([script_absolute_path])
    else:
        script_absolute_path = os.path.join(vcpkg_root_dir, 'bootstrap-vcpkg.sh')
        run_command([script_absolute_path, '--disableMetrics'])

    # for each desktop platform, there is an existing vcpkg response file 
    # in the repo (external/vcpkg_<triplet>_response_file.txt) defined for each target triplet
    vcpkg_response_file = os.path.join(repo_root_dir, 'external', 
                                       'vcpkg_' + get_vcpkg_triplet(arch) + '_response_file.txt')
    if is_windows_os():
        # Windows has issues executing a relative path bat file
        script_absolute_path = os.path.join(vcpkg_root_dir, 'vcpkg.exe')
    else:
        script_absolute_path = os.path.join(vcpkg_root_dir, 'vcpkg')
 
    run_command([script_absolute_path, 'install', '@' + vcpkg_response_file])


def main():
    args = parseArgs()

    system_packages_to_install = []
    # Install ninja and/or ccache if needed
    # check if ninja is already installed before attempting to install it
    if args.generator == 'Ninja' and not is_tool_installed('ninja'):
        system_packages_to_install.append('ninja')
    # ccache is supported only on mac and linux and check if it is already installed
    if args.ccache and (is_linux_os() | is_mac_os()) and not is_tool_installed('ccache'):
        system_packages_to_install.append('ccache')
    if system_packages_to_install:
        install_system_packages(system_packages_to_install)

    install_python_dependencies()

    if not args.no_vcpkg:
        install_packages_with_vcpkg(args.arch)
    else:
        # Do not install dependencies automatically as we want users to have control
        # on their system level package installations
        repo_root_dir = get_repo_root_dir()
        vcpkg_response_file = os.path.join(repo_root_dir, 'external', 
                                       'vcpkg_' + get_vcpkg_triplet(arch) + '_response_file.txt')
        print('Please install packages listed in {0} with your system package manager'.format(vcpkg_response_file))


def parseArgs():
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
