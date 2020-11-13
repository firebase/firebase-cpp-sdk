import argparse
import sys
import os
import platform
import subprocess
import distutils.spawn

def is_path_valid(path):
  return os.path.exists(os.path.join(path, 'CMakeLists.txt'))

def is_sdk_path_source(path):
  # Not the most reliable way to detect if the sdk path is source or prebuilt but
  # should work for our purpose.
  return os.path.exists(os.path.join(path, 'build_tools'))

def get_vcpkg_triplet(arch, msvc_runtime_library='static'):
  """ Get vcpkg target triplet (platform definition).

  Args:
    arch (str): Architecture (eg: 'x86', 'x64').
    msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').

  Raises:
    ValueError: If current OS is not win,mac or linux.

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
    raise ValueError("Unsupported platform. "
                     "This function works only on Windows, Linux or Mac platforms.")

  triplet_name = '-'.join(triplet_name)
  print("Using vcpkg triplet: {0}".format(triplet_name))
  return triplet_name

def build_source_vcpkg_dependencies(sdk_source_dir, arch, msvc_runtime_library):
  # TODO: Remove this once dev has been merged onto master
  # This is required because vcpkg lives only in dev branch right now.
  subprocess.run(['git', 'checkout', 'feature/python-tool-build-apps-with-firebase'], cwd=sdk_source_dir)
  subprocess.run(['git', 'pull'], cwd=sdk_source_dir)

  python_exe = 'python3' if distutils.spawn.find_executable('python3') else 'python'
  subprocess.run([python_exe, "scripts/gha/install_prereqs_desktop.py"], cwd=sdk_source_dir)
  subprocess.run([python_exe, "scripts/gha/build_desktop.py", "--arch", arch,
                              "--msvc_runtime_library", msvc_runtime_library,
                              "--vcpkg_step_only"], cwd=sdk_source_dir)

def build_app_with_source(app_dir, sdk_dir, build_dir, arch,
                          msvc_runtime_library='static', config=None,
                          target_format=None):
  """ CMake configure.

  If you are seeing problems when running this multiple times,
  make sure to clean/delete previous build directory.

  Args:
   build_dir (str): Output build directory.
   arch (str): Platform Architecture (example: 'x64').
   msvc_runtime_library (str): Runtime library for MSVC (eg: 'static', 'dynamic').
   build_tests (bool): Build cpp unit tests.
   config (str): Release/Debug config.
          If its not specified, cmake's default is used (most likely Debug).
   target_format (str): If specified, build for this targetformat ('frameworks' or 'libraries').
  """
  # Cmake configure
  cmd = ['cmake', '-S', '.', '-B', build_dir]
  cmd.append('-DFIREBASE_CPP_SDK_DIR={0}'.format(sdk_dir))

  # If generator is not specifed, default for platform is used by cmake, else
  # use the specified value
  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))
    # workaround, absl doesn't build without tests enabled
    cmd.append('-DBUILD_TESTING=off')

  if platform.system() == 'Linux' and arch == 'x86':
    # Use a separate cmake toolchain for cross compiling linux x86 builds
    vcpkg_toolchain_file_path = os.path.join(sdk_dir, 'external', 'vcpkg',
                                             'scripts', 'buildsystems', 'linux_32.cmake')
  else:
    vcpkg_toolchain_file_path = os.path.join(sdk_dir, 'external',
                                           'vcpkg', 'scripts',
                                           'buildsystems', 'vcpkg.cmake')

  cmd.append('-DCMAKE_TOOLCHAIN_FILE={0}'.format(vcpkg_toolchain_file_path))

  vcpkg_triplet = get_vcpkg_triplet(arch, msvc_runtime_library)
  cmd.append('-DVCPKG_TARGET_TRIPLET={0}'.format(vcpkg_triplet))

  if platform.system() == 'Windows':
    # If building for x86, we should supply -A Win32 to cmake configure
    # Its a good habit to specify for x64 too as the default might be different
    # on different windows machines.
    cmd.append('-A')
    cmd.append('Win32') if arch == 'x86' else cmd.append('x64')

    # Use our special cmake option for /MD (dynamic).
    # If this option is not specified, the default value is /MT (static).
    if msvc_runtime_library == "dynamic":
      cmd.append('-DMSVC_RUNTIME_LIBRARY_STATIC=ON')

  if (target_format):
    cmd.append('-DFIREBASE_XCODE_TARGET_FORMAT={0}'.format(target_format))
  print("Running {0}".format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

  #CMake build
  cmd = ['cmake', '--build', build_dir, '-j', str(os.cpu_count()), '--config', config]
  print("Running {0}".format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

def build_app_with_prebuilt(app_dir, sdk_dir, build_dir, arch,
                            msvc_runtime_library='static', config=None):
  cmd = ['cmake', '-S', '.', '-B', build_dir]
  cmd.append('-DFIREBASE_CPP_SDK_DIR={0}'.format(sdk_dir))

  if platform.system() == 'Windows':
    if arch == 'x64':
      cmd.append('-DCMAKE_CL_64=ON')
    if msvc_runtime_library == 'dynamic':
      cmd.append('-DMSVC_RUNTIME_MODE=MD')
    else:
      cmd.append('-DMSVC_RUNTIME_MODE=MT')

  if config:
    cmd.append('-DCMAKE_BUILD_TYPE={0}'.format(config))

  print("Running {0}".format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

  #CMake build
  cmd = ['cmake', '--build', build_dir, '-j', str(os.cpu_count()), '--config', config]
  print("Running {0}".format(' '.join(cmd)))
  subprocess.run(cmd, cwd=app_dir, check=True)

def main():
  args = parse_cmdline_args()

  if not is_path_valid(args.sdk_dir):
    print ("SDK path provided is not valid. Could not find a CMakeLists.txt at the root level.\n"
           "Please check the argument to '--sdk_dir'")
    sys.exit(1)

  if not is_path_valid(args.app_dir):
    print ("App path provided is not valid. Could not find a CMakeLists.txt at the root level.\n"
           "Please check the argument to '--app_dir'")
    sys.exit(1)

  if is_sdk_path_source(args.sdk_dir):
    print("SDK path provided is Firebase C++ source directory. Building...")
    build_source_vcpkg_dependencies(args.sdk_dir, args.arch, args.msvc_runtime_library)
    build_app_with_source(args.app_dir, args.sdk_dir, args.build_dir, args.arch,
                          args.msvc_runtime_library, args.config, args.target_format)
  else:
    print("SDK path provided is Firebase C++ prebuilt libraries. Building...")
    build_app_with_prebuilt(args.app_dir, args.sdk_dir, args.build_dir, args.arch,
                            args.msvc_runtime_library, args.config)

  print ("Build successful! "
         "Please find your executables in build directory: {0}".format
                                    (os.path.join(args.app_dir, args.build_dir)))

def parse_cmdline_args():
  parser = argparse.ArgumentParser(description='Install Prerequisites for building cpp sdk')
  parser.add_argument('--sdk_dir', help='Path to Firebase SDK - source or prebuilt libraries.',
                      type=os.path.abspath)
  parser.add_argument('--app_dir', help="Path to application to build (directory containing app's CMakeLists.txt",
                      type=os.path.abspath)
  parser.add_argument('-a', '--arch', default='x64', help='Platform architecture (x64, x86)')
  parser.add_argument('--msvc_runtime_library', default='static',
                      help='Runtime library for MSVC (static(/MT) or dynamic(/MD)')
  parser.add_argument('--build_dir', default='build', help='Output build directory')
  parser.add_argument('--config', default='Release', help='Release/Debug config')
  parser.add_argument('--target_format', default=None, help='(Mac only) whether to output frameworks (default) or libraries.')
  args = parser.parse_args()
  return args

if __name__ == '__main__':
  main()