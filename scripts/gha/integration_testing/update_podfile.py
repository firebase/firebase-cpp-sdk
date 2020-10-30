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

r""" Update integration testapps' Podfiles to match the SDK's Podfile.
Usage:
  python update_podfile.py --sdk_podfile ios_pod/Podfile \ 
    --app_podfile admob/integration_test/Podfile
"""

from absl import app
from absl import flags
import re
import fileinput

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "sdk_podfile", None, "Path to CPP SDK's iOS podfile.",
    short_name="s")
flags.DEFINE_string(
    "app_podfile", None, "Path to integration testapp's iOS podfile.",
    short_name="a")


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  sdk_podfile = FLAGS.sdk_podfile
  app_podfile = FLAGS.app_podfile

  # dict to store podName & podVersion from sdk's podfile
  pod_dict = {}
  with open(sdk_podfile) as f:
    for line in f:
      tokens = re.split(' |,', line)
      tokens = list(filter(None, tokens))
      if tokens[0] == 'pod':
        pod_dict[tokens[1]] = tokens[2]

  # update podVersion in app's podfile
  with fileinput.input(app_podfile, inplace=True) as f:
    for line in f:
      tokens = re.split(' |,', line)
      tokens = list(filter(None, tokens))
      if tokens[0] == 'pod' and tokens[1] in pod_dict:
        print(line.replace(tokens[2], pod_dict[tokens[1]]).rstrip())
      else:
        print(line.rstrip())


if __name__ == "__main__":
  flags.mark_flag_as_required("sdk_podfile")
  flags.mark_flag_as_required("app_podfile")
  app.run(main)