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

python restore_secrets.py --passphrase [--repo_dir <path_to_repo>]
python restore_secrets.py --passphrase_file [--repo_dir <path_to_repo>]

--passphrase: Passphrase to decrypt the files. This option is insecure on a
    multi-user machine; use the --passphrase_file option instead.
--passphrase_file: Specify a file to read the passphrase from (only reads the
    first line).
--repo_dir: Path to C++ SDK Github repository. Defaults to current directory.

This script will perform the following:

- Google Service files (plist and json) will be restored into the
  integration_test directories.
- The server key will be patched into the Messaging project.
- The uri prefix will be patched into the Dynamic Links project.
- The reverse id will be patched into all Info.plist files, using the value from
  the decrypted Google Service plist files as the source of truth.

"""

import os
import plistlib
import subprocess

from absl import app
from absl import flags


FLAGS = flags.FLAGS

flags.DEFINE_string("repo_dir", os.getcwd(), "Path to C++ SDK Github repo.")
flags.DEFINE_string("passphrase", None, "The passphrase itself.")
flags.DEFINE_string("passphrase_file", None, "Path to file with passphrase.")


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  repo_dir = FLAGS.repo_dir
  # The passphrase is sensitive, do not log.
  if FLAGS.passphrase:
    passphrase = FLAGS.passphrase
  elif FLAGS.passphrase_file:
    with open(FLAGS.passphrase_file, "r") as f:
      passphrase = f.readline().strip()
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
      dest_paths = [os.path.join(repo_dir, api, "integration_test", file_name)]
      # Some apis like Firestore also have internal integration tests.
      if os.path.exists( os.path.join(repo_dir, api, "integration_test_internal")):
        dest_paths.append(os.path.join(repo_dir, api,
                                       "integration_test_internal", file_name))
      decrypted_text = _decrypt(path, passphrase)
      for dest_path in dest_paths:
        with open(dest_path, "w") as f:
          f.write(decrypted_text)
        print("Copied decrypted google service file to %s" % dest_path)
        # We use a Google Service file as the source of truth for the reverse id
        # that needs to be patched into the Info.plist files.
        if dest_path.endswith(".plist"):
          _patch_reverse_id(dest_path)
          _patch_bundle_id(dest_path)

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

  print("Attempting to decrypt GCS service account key file.")
  decrypted_key_file = os.path.join(secrets_dir, "gcs_key_file.json")
  encrypted_key_file = decrypted_key_file + ".gpg"
  decrypted_key_text = _decrypt(encrypted_key_file, passphrase)
  with open(decrypted_key_file, "w") as f:
    f.write(decrypted_key_text)
  print("Created decrypted key file at %s" % decrypted_key_file)


def _find_encrypted_files(directory_to_search):
  """Returns a list of full paths to all files encrypted with gpg."""
  encrypted_files = []
  for prefix, _, files in os.walk(directory_to_search):
    for relative_path in files:
      if relative_path.endswith(".gpg"):
        encrypted_files.append(os.path.join(prefix, relative_path))
  return encrypted_files


def _decrypt(encrypted_file, passphrase):
  """Returns the decrypted contents of the given .gpg file."""
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
    # Remove any instances of the passphrase from error before logging it.
    raise RuntimeError(result.stderr.replace(passphrase, "****"))
  print("Decryption successful")
  # rstrip to eliminate a linebreak that GPG may introduce.
  return result.stdout.rstrip()


def _patch_reverse_id(service_plist_path):
  """Patches the Info.plist file with the reverse id from the Service plist."""
  print("Attempting to patch reverse id in Info.plist")
  with open(service_plist_path, "rb") as f:
    service_plist = plistlib.load(f)
  _patch_file(
      path=os.path.join(os.path.dirname(service_plist_path), "Info.plist"),
      placeholder="REPLACE_WITH_REVERSED_CLIENT_ID",
      value=service_plist["REVERSED_CLIENT_ID"])


def _patch_bundle_id(service_plist_path):
  """Patches the Info.plist file with the bundle id from the Service plist."""
  print("Attempting to patch bundle id in Info.plist")
  with open(service_plist_path, "rb") as f:
    service_plist = plistlib.load(f)
  _patch_file(
      path=os.path.join(os.path.dirname(service_plist_path), "Info.plist"),
      placeholder="$(PRODUCT_BUNDLE_IDENTIFIER)",
      value=service_plist["BUNDLE_ID"])


def _patch_main_src(project_dir, placeholder, value):
  """Patches the integration_test.cc file in the integration test project."""
  path = os.path.join(project_dir, "src", "integration_test.cc")
  _patch_file(path, placeholder, value)


def _patch_file(path, placeholder, value):
  """Patches instances of the placeholder with the given value."""
  # Note: value may be sensitive, so do not log.
  with open(path, "r") as f_read:
    text = f_read.read()
  # Count number of times placeholder appears for debugging purposes.
  replacements = text.count(placeholder)
  patched_text = text.replace(placeholder, value)
  with open(path, "w") as f_write:
    f_write.write(patched_text)
  print("Patched %d instances of %s in %s" % (replacements, placeholder, path))


if __name__ == "__main__":
  app.run(main)
