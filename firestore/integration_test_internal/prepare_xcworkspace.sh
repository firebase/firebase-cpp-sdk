# !/bin/bash
#
# Copyright 2022 Google LLC
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
#

# Preapres integration_test.xcworkspace to run tests for iOS and tvOS.

# Fail on any error.
set -e
# Display commands being run.
set -x

# Absolute path to this script
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in
SCRIPTPATH=$(dirname "$SCRIPT")
# Directory to build xcframeworks
BUILDDIR="$SCRIPTPATH/ios_tvos_build/"
# SDK ROOT
SDKROOT=$(readlink -f "$SCRIPTPATH/../../")

RUBY=`which ruby`
PYTHON=`which python3`
POD=`which pod`

# Clean up build directory, incremental builds not supported yet.
rm -rf $BUILDDIR

# Build required xcframeworks to the BUILDDIR
cd $SDKROOT
$PYTHON $SDKROOT/scripts/gha/build_ios_tvos.py -t firebase_auth firebase_firestore -b $BUILDDIR

# Install the underlying Firestore C++ Core SDK from cocoapods and build the integration test
# xcode workspace.
cd $SCRIPTPATH
$POD install

# Add built xcframework from `build_ios_tvos.py` to ios tests
$RUBY $SDKROOT/scripts/gha/integration_testing/xcode_tool.rb --XCodeCPP.xcodeProjectDir . --XCodeCPP.target integration_test --XCodeCPP.frameworks ./ios_tvos_build/xcframeworks/firebase.xcframework,./ios_tvos_build/xcframeworks/firebase_firestore.xcframework,./ios_tvos_build/xcframeworks/firebase_auth.xcframework

# Add built xcframework from `build_ios_tvos.py` to tvos tests
$RUBY $SDKROOT/scripts/gha/integration_testing/xcode_tool.rb --XCodeCPP.xcodeProjectDir . --XCodeCPP.target integration_test_tvos --XCodeCPP.frameworks ./ios_tvos_build/xcframeworks/firebase.xcframework,./ios_tvos_build/xcframeworks/firebase_firestore.xcframework,./ios_tvos_build/xcframeworks/firebase_auth.xcframework

