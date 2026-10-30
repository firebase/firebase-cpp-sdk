#!/usr/bin/env python3
# Copyright 2026 Google LLC
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

"""Tests for build_apple_analytics_sdk.py (platform independent helpers)."""

import os
import unittest

import build_apple_analytics_sdk as sdk

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))


class VersionTest(unittest.TestCase):

  def test_reads_version_from_ios_spm_package(self):
    with open(os.path.join(_REPO_ROOT, "ios_spm", "Package.swift")) as f:
      version = sdk.read_firebase_ios_sdk_version(f.read())
    self.assertRegex(version, r"^\d+\.\d+\.\d+")

  def test_reads_version(self):
    text = ('.package(url: "https://github.com/firebase/firebase-ios-sdk.git",'
            ' exact: "13.1.1"),')
    self.assertEqual(sdk.read_firebase_ios_sdk_version(text), "13.1.1")

  def test_missing_version_raises(self):
    with self.assertRaises(ValueError):
      sdk.read_firebase_ios_sdk_version('.package(url: "x", from: "1.0.0")')

  def test_clamp_deployment_target(self):
    self.assertEqual(sdk.clamp_deployment_target(""), ("11.0", None))
    self.assertEqual(sdk.clamp_deployment_target("12.3"), ("12.3", None))
    target, warning = sdk.clamp_deployment_target("10.15")
    self.assertEqual(target, "11.0")
    self.assertIn("10.15", warning)


class TemplateTest(unittest.TestCase):

  def test_renders_package_template(self):
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "Package.swift.in")) as f:
      text = sdk.render_template(f.read(), {
          "FIREBASE_IOS_SDK_VERSION": "13.1.1",
          "MACOS_DEPLOYMENT_TARGET": "11.0",
      })
    self.assertIn('exact: "13.1.1"', text)
    self.assertIn('.macOS("11.0")', text)
    self.assertIn('name: "%s"' % sdk.DEPS_TARGET, text)

  def test_unreplaced_placeholder_raises(self):
    with self.assertRaises(ValueError):
      sdk.render_template("@A@ @B@", {"A": "x"})


class XcframeworkTest(unittest.TestCase):

  INFO = {
      "AvailableLibraries": [
          {"LibraryIdentifier": "ios-arm64", "SupportedPlatform": "ios",
           "LibraryPath": "FirebaseAnalytics.framework"},
          {"LibraryIdentifier": "ios-arm64_x86_64-maccatalyst",
           "SupportedPlatform": "ios", "SupportedPlatformVariant":
               "maccatalyst",
           "LibraryPath": "FirebaseAnalytics.framework"},
          {"LibraryIdentifier": "macos-arm64_x86_64",
           "SupportedPlatform": "macos",
           "LibraryPath": "FirebaseAnalytics.framework",
           "BinaryPath":
               "FirebaseAnalytics.framework/Versions/A/FirebaseAnalytics"},
      ]
  }

  def test_selects_plain_macos_slice(self):
    library = sdk.select_macos_library(self.INFO)
    self.assertEqual(library["LibraryIdentifier"], "macos-arm64_x86_64")

  def test_no_macos_slice(self):
    info = {"AvailableLibraries": self.INFO["AvailableLibraries"][:2]}
    self.assertIsNone(sdk.select_macos_library(info))

  def test_binary_path(self):
    self.assertEqual(
        sdk.framework_binary_relpath(self.INFO["AvailableLibraries"][2]),
        "FirebaseAnalytics.framework/Versions/A/FirebaseAnalytics")
    self.assertEqual(
        sdk.framework_binary_relpath({"LibraryPath": "Foo.framework"}),
        "Foo.framework/Foo")
    self.assertEqual(
        sdk.framework_binary_relpath({"LibraryPath": "libFoo.a"}), "libFoo.a")

  def test_static_archive_detection(self):
    self.assertTrue(sdk.is_static_archive_description(
        "Mach-O universal binary with 2 architectures: [x86_64:current ar "
        "archive random library] [arm64]"))
    self.assertFalse(sdk.is_static_archive_description(
        "Mach-O universal binary with 2 architectures: [x86_64:Mach-O 64-bit "
        "dynamically linked shared library x86_64]"))


class ModuleNameTest(unittest.TestCase):

  def test_module_names(self):
    self.assertEqual(sdk.module_name_from_build_dir("FirebaseCore.build"),
                     "FirebaseCore")
    self.assertEqual(sdk.module_name_from_build_dir("/x/y/nanopb.build/"),
                     "nanopb")
    self.assertEqual(sdk.sanitize_module_name("GoogleUtilities-Environment"),
                     "GoogleUtilities_Environment")


class PrivacyManifestTest(unittest.TestCase):

  def test_merge(self):
    a = {
        "NSPrivacyTracking": False,
        "NSPrivacyTrackingDomains": ["a.example"],
        "NSPrivacyCollectedDataTypes": [{
            "NSPrivacyCollectedDataType": "NSPrivacyCollectedDataTypeDeviceID",
            "NSPrivacyCollectedDataTypeLinked": False,
            "NSPrivacyCollectedDataTypeTracking": False,
            "NSPrivacyCollectedDataTypePurposes": ["Analytics"],
        }],
        "NSPrivacyAccessedAPITypes": [{
            "NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryUserDefaults",
            "NSPrivacyAccessedAPITypeReasons": ["CA92.1"],
        }],
    }
    b = {
        "NSPrivacyTracking": True,
        "NSPrivacyTrackingDomains": ["a.example", "b.example"],
        "NSPrivacyCollectedDataTypes": [{
            "NSPrivacyCollectedDataType": "NSPrivacyCollectedDataTypeDeviceID",
            "NSPrivacyCollectedDataTypeLinked": True,
            "NSPrivacyCollectedDataTypePurposes": ["Analytics",
                                                   "AppFunctionality"],
        }],
        "NSPrivacyAccessedAPITypes": [
            {"NSPrivacyAccessedAPIType":
                 "NSPrivacyAccessedAPICategoryUserDefaults",
             "NSPrivacyAccessedAPITypeReasons": ["1C8F.1"]},
            {"NSPrivacyAccessedAPIType":
                 "NSPrivacyAccessedAPICategoryFileTimestamp",
             "NSPrivacyAccessedAPITypeReasons": ["C617.1"]},
        ],
    }
    merged = sdk.merge_privacy_manifests([a, b, {}])
    self.assertTrue(merged["NSPrivacyTracking"])
    self.assertEqual(merged["NSPrivacyTrackingDomains"],
                     ["a.example", "b.example"])
    self.assertEqual(len(merged["NSPrivacyCollectedDataTypes"]), 1)
    device_id = merged["NSPrivacyCollectedDataTypes"][0]
    self.assertTrue(device_id["NSPrivacyCollectedDataTypeLinked"])
    self.assertFalse(device_id["NSPrivacyCollectedDataTypeTracking"])
    self.assertEqual(device_id["NSPrivacyCollectedDataTypePurposes"],
                     ["Analytics", "AppFunctionality"])
    apis = {e["NSPrivacyAccessedAPIType"]: e["NSPrivacyAccessedAPITypeReasons"]
            for e in merged["NSPrivacyAccessedAPITypes"]}
    self.assertEqual(apis["NSPrivacyAccessedAPICategoryUserDefaults"],
                     ["CA92.1", "1C8F.1"])
    self.assertEqual(apis["NSPrivacyAccessedAPICategoryFileTimestamp"],
                     ["C617.1"])

  def test_merge_empty(self):
    self.assertEqual(sdk.merge_privacy_manifests([]), {
        "NSPrivacyTracking": False,
        "NSPrivacyTrackingDomains": [],
        "NSPrivacyCollectedDataTypes": [],
        "NSPrivacyAccessedAPITypes": [],
    })


class ManifestTest(unittest.TestCase):

  MANIFEST = {
      "firebase_ios_sdk_version": "13.1.1",
      "archs": ["arm64", "x86_64"],
      "deployment_target": "11.0",
      "modules": [
          {"name": "FirebaseCore", "kind": "source",
           "archive": "lib/libFirebaseCore.a", "size_bytes": 2048},
          {"name": "FirebaseAnalytics", "kind": "binary",
           "archive": "frameworks/FirebaseAnalytics.framework/"
                      "FirebaseAnalytics", "size_bytes": 10240},
      ],
      "has_swift": True,
      "swift_library_dirs": ["/sdk/usr/lib/swift"],
      "system_frameworks": ["Foundation"],
      "system_libraries": ["z"],
      "privacy_manifest": "PrivacyInfo.xcprivacy",
  }

  def test_cmake_manifest(self):
    text = sdk.render_cmake_manifest(self.MANIFEST)
    self.assertIn('set(FIREBASE_APPLE_ANALYTICS_SDK_VERSION "13.1.1")', text)
    self.assertIn('set(FIREBASE_APPLE_ANALYTICS_SDK_ARCHS "arm64;x86_64")',
                  text)
    self.assertIn('set(FIREBASE_APPLE_ANALYTICS_SDK_MODULES '
                  '"FirebaseCore;FirebaseAnalytics")', text)
    self.assertIn('"${CMAKE_CURRENT_LIST_DIR}/lib/libFirebaseCore.a;'
                  '${CMAKE_CURRENT_LIST_DIR}/frameworks/', text)
    self.assertIn("set(FIREBASE_APPLE_ANALYTICS_SDK_HAS_SWIFT ON)", text)
    self.assertIn('${CMAKE_CURRENT_LIST_DIR}/PrivacyInfo.xcprivacy', text)

  def test_size_report_sorted_with_total(self):
    report = sdk.render_size_report(self.MANIFEST)
    lines = report.splitlines()
    self.assertIn("FirebaseAnalytics", lines[1])
    self.assertIn("FirebaseCore", lines[2])
    self.assertIn("TOTAL", lines[3])
    self.assertIn("12.0 KiB", lines[3])

  def test_stamp_changes_with_inputs(self):
    base = sdk.compute_stamp("13.1.1", ["arm64"], "11.0", "t")
    self.assertEqual(base, sdk.compute_stamp("13.1.1", ["arm64"], "11.0", "t"))
    self.assertNotEqual(base,
                        sdk.compute_stamp("13.1.2", ["arm64"], "11.0", "t"))
    self.assertNotEqual(
        base, sdk.compute_stamp("13.1.1", ["arm64", "x86_64"], "11.0", "t"))
    self.assertNotEqual(base,
                        sdk.compute_stamp("13.1.1", ["arm64"], "12.0", "t"))


if __name__ == "__main__":
  unittest.main()
