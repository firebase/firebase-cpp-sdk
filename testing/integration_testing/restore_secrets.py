# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Script for restoring secrets into the integration test projects.

Usage:

python restore_secrets.py --passphrase <passphrase> --repo_dir <path_to_repo>

repo_dir refers to the C++ SDK github repository. Defaults to current directory.

As an alternative to passing the passphrase as a flag, the password can be
stored in a file and passed via the --passphrase_file flag.

This will perform the following:

- Google Service files (plist and json) will be restored into the
  integration_test directories.
- The server key will be patched into the Messaging project.
- The uri prefix will be patched into the Dynamic Links project.

"""

import argparse
import os
import subprocess

parser = argparse.ArgumentParser(
    description="Decrypt and restore the secrets. Must specify one of"
    " passphrase or passphrase_file.")
parser.add_argument("--passphrase", help="The passphrase itself.")
parser.add_argument("--passphrase_file", help="Path to file with passphrase.")
parser.add_argument("--repo_dir", default=os.getcwd(), help="Path to SDK Repo")
args = parser.parse_args()


def main():
  repo_dir = args.repo_dir
  # The passphrase is sensitive, do not log.
  if args.passphrase:
    passphrase = args.passphrase
  elif args.passphrase_file:
    with open(args.passphrase_file, "r") as f:
      passphrase = f.read()
  else:
    raise ValueError("Must supply passphrase or passphrase_file arg.")

  secrets_dir = os.path.join(repo_dir, "scripts", "gha-encrypted")
  encrypted_files = _find_encrypted_files(secrets_dir)
  print("Found these encrypted files:\n%s" % "\n".join(encrypted_files))

  for path in encrypted_files:
    if "google-services" in path or "GoogleService" in path:
      print("Encrypted Google Service file found: %s" % path)
      # We infer the destination from the file's directory, example:
      # /scripts/gha-encrypted/auth/google-services.json.gpg turns into
      # /<repo_dir>/auth/integration_test/google-services.json
      api = os.path.basename(os.path.dirname(path))
      file_name = os.path.basename(path).replace(".gpg", "")
      dest_path = os.path.join(repo_dir, api, "integration_test", file_name)
      decrypted_text = _decrypt(path, passphrase)
      with open(dest_path, "w") as f:
        f.write(decrypted_text)
      print("Copied decrypted google service file to %s" % dest_path)

  print("Attempting to patch Dynamic Links uri prefix.")
  uri_path = os.path.join(secrets_dir, "dynamic_links", "uri_prefix.txt.gpg")
  uri_prefix = _decrypt(uri_path, passphrase)
  dlinks_project = os.path.join(repo_dir, "dynamic_links", "integration_test")
  _patch_main_src(dlinks_project, "REPLACE_WITH_YOUR_URI_PREFIX", uri_prefix)

  print("Attempting to patch Messaging server key.")
  server_key_path = os.path.join(secrets_dir, "messaging", "server_key.txt.gpg")
  server_key = _decrypt(server_key_path, passphrase)
  messaging_project = os.path.join(repo_dir, "messaging", "integration_test")
  _patch_main_src(messaging_project, "REPLACE_WITH_YOUR_SERVER_KEY", server_key)


def _find_encrypted_files(directory_to_search):
  """Returns a list of full paths to all files encrypted with gpg."""
  encrypted_files = []
  for prefix, _, files in os.walk(directory_to_search):
    for relative_path in files:
      if relative_path.endswith(".gpg"):
        encrypted_files.append(os.path.join(prefix, relative_path))
  return encrypted_files


def _decrypt(encrypted_file, passphrase):
  """Generates a decrypted file with same path minus the '.gpg' extension."""
  print("Decrypting %s" % encrypted_file)
  # Note: if setting check=True, be sure to catch the error and not rethrow it
  # or print a traceback, as the message will include the passphrase.
  result = subprocess.run(
      args=[
          "gpg",
          "--passphrase", passphrase,
          "--quiet",
          "--batch",
          "--yes",
          "--decrypt",
          encrypted_file],
      check=False,
      text=True,
      capture_output=True)
  if result.returncode:
    raise RuntimeError("ERROR: Failed to decrypt %s" % (encrypted_file))
  print("Decryption successful")
  return result.stdout


def _patch_main_src(project_dir, placeholder, value):
  """Patches the integration_test.cc file in the integration test project."""
  path = os.path.join(project_dir, "src", "integration_test.cc")
  with open(path, "r") as f_read:
    text = f_read.read()
  # Count number of times placeholder appears for debugging purposes.
  replacements = text.count(placeholder)
  patched_text = text.replace(placeholder, value)
  with open(path, "w") as f_write:
    f_write.write(patched_text)
  print("Patched %d instances of %s in %s " % (replacements, placeholder, path))


if __name__ == "__main__":
  main()
