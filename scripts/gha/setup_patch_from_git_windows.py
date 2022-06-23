import os
import pathlib
import shutil
import tempfile

from absl import app
from absl import logging

def main(argv):
  if len(argv) > 1:
    raise app.UsageError(f"unexpected argument: {argv[1]}")

  logging.info("Searching for git.exe")
  git_executable_path_str = shutil.which("git")
  if not git_executable_path_str:
    raise Exception("git not found in the PATH")
  git_executable = pathlib.Path(git_executable_path_str)
  logging.info("Found: %s", git_executable)

  logging.info("Searching for patch.exe")
  patch_executable = git_executable.parent.parent / "usr" / "bin" / "patch.exe"
  if not patch_executable.exists():
    raise Exception(f"file not found: {patch_executable}")
  logging.info("Found: %s", patch_executable)

  logging.info("Searching for msys-2.0.dll")
  msys_dll = patch_executable.parent / "msys-2.0.dll"
  if not msys_dll.exists():
    raise Exception(f"file not found: {msys_dll}")
  logging.info("Found %s", msys_dll)

  temp_dir = pathlib.Path(tempfile.mkdtemp("patch"))
  logging.info("Created temporary directory: %s", temp_dir)
  logging.info("Copying %s to %s", patch_executable, temp_dir)
  shutil.copy2(patch_executable, temp_dir)
  logging.info("Copying %s to %s", msys_dll, temp_dir)
  shutil.copy2(msys_dll, temp_dir)

  print(str(temp_dir))


if __name__ == "__main__":
  app.run(main)
