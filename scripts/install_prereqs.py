#!/usr/bin/python
"""
This script will take care of installing python and C++ dependencies before 
starting the build. 

The goal is to avoid any major changes in system configurations of users. 
Hence we require the following set of prerequisites for prerequisites:
- Python3
- Pip 
- System package manager
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


def get_repo_root_dir():
    """ Get root directory of current git repo

    This function takes the guesswork out when executing commands
    or locating files relative to root of the repo

    Raises:
        IOError: Current directory is not in a git repo

    Returns:
        (str): Root directory of git repo
    """
    try:
        root_dir = subprocess.check_output('git rev-parse --show-toplevel', shell=True)
    except subprocess.CalledProcessError:
        raise IOError('Current working directory is not a git repository')
    return root_dir.decode('utf-8').strip()


def run_command(cmd, as_root_user=False):
    """Run a command

    Args:
        cmd (:obj:`list` of :obj:`str`): Command to run as a list object
                                         Eg: ['ls', '-l']
        as_root_user (bool): Run command as root user with admin priveleges (supported on mac and linux)
    Returns:
        (`subprocess.Popen.returncode`): Return code from command execution
    """
    if as_root_user and (is_mac_os() or is_linux_os()):
        cmd.insert(0, 'sudo')

    print ('Running cmd: {0}\n'.format(' '.join(cmd)))
    return subprocess.call(cmd)


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
    if isWindows():
        if not distutils.spawn.find_executable(PackageManager.WINDOWS):
            raise ValueError('Did not find {0} on the system. \
                                Please install it or \
                                try setting install_if_not_found=True'.format(PackageManager.WINDOWS))
        return PackageManager.WINDOWS

    elif isMac():
        if not distutils.spawn.find_executable(PackageManager.MAC):
            raise ValueError('Did not find {0} on the system. \
                            Please install it or \
                            try setting install_if_not_found=True'.format(PackageManager.MAC))
        return PackageManager.MAC

    elif isLinux():
        if not distutils.spawn.find_executable(PackageManager.LINUX):
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
        print 'No packages specified to install via system pacakge manager'
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
    if args.ninja:
        system_packages_to_install.append('ninja')
    if args.ccache:
        system_packages_to_install.append('ccache')
    if system_packages_to_install:
        install_system_packages(system_packages_to_install)

    install_python_dependencies()

    if not args.no_vcpkg:
        install_packages_with_vcpkg(args.arch)
    else:
        print "installing with system package manager not yet implemented"       


def parseArgs():
    parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
    parser.add_argument('-a', '--arch', default='x64', help='Platform architecture (x64, x86)')
    parser.add_argument('-n', '--ninja', action='store_true', help='Install ninja using system package manager')
    parser.add_argument('-c', '--ccache', action='store_true', help='Install ccache using system package manager')
    parser.add_argument('-p', '--no_vcpkg', action='store_true', 
                                                                                    help='Use system package manager (brew, apt, choco) for dependencies(instead of vcpkg)')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    main()
