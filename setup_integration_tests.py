#!/usr/bin/python

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
"""Copy files from test & sample frameworks into integration_test directories.

Usage: %s [destination directories]
"""

import os
import sys
from distutils.dir_util import copy_tree

# Where to copy framework files from, relative to this script's location.
FRAMEWORK_DIRECTORIES = [
    'testing/sample_framework',
    'testing/test_framework',
]

# If running without specifying a directory, these are the destination
# directories to use instead, relative to this script's location.
DEFAULT_DESTINATIONS = [
    'analytics/integration_test',
    'app/integration_test',
    'app_check/integration_test',
    'auth/integration_test',
    'database/integration_test',
    'dynamic_links/integration_test',
    'firestore/integration_test',
    'firestore/integration_test_internal',
    'functions/integration_test',
    'gma/integration_test',
    'installations/integration_test',
    'messaging/integration_test',
    'remote_config/integration_test',
    'storage/integration_test',
]

destinations = sys.argv[1:] if len(sys.argv) > 1 else DEFAULT_DESTINATIONS
destinations = map(os.path.abspath, destinations)
os.chdir(os.path.dirname(os.path.abspath(__file__)))

for dest in destinations:
    if not os.path.exists(dest):
        print('Error, destination path "%s" does not exist' % dest) 
        continue
    for src in FRAMEWORK_DIRECTORIES:
        print('Copying %s to %s' % (src, dest))
        copy_tree(src, dest, update=1)
