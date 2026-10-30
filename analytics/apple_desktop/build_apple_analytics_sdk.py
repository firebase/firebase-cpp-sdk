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

"""Builds the Firebase Apple Analytics SDK as embeddable macOS archives.

The Firebase Apple SDK is distributed for Swift Package Manager as a mix of
source targets (FirebaseCore, FirebaseCoreInternal (Swift), FirebaseInstallations,
GoogleUtilities, nanopb, Promises, ...) and binary xcframeworks
(FirebaseAnalytics, GoogleAppMeasurement). Xcode links all of those directly
into an app; CMake-based macOS desktop builds (the C++ SDK testapps, the Unity
FirebaseCppApp bundle) cannot consume SwiftPM directly. This script:

  1. Renders analytics/apple_desktop/Package.swift.in with the Firebase Apple
     SDK version pinned in ios_spm/Package.swift.
  2. Builds it with `swift build` once per requested architecture.
  3. Archives each SwiftPM module separately (lib<Module>.a, universal), so
     CMake can choose which ones to embed (e.g. skip nanopb when desktop
     Firestore already provides it).
  4. Extracts the static macOS slices of the binary xcframeworks.
  5. Merges every PrivacyInfo.xcprivacy into a single manifest.
  6. Writes manifest.json, a size report, and
     firebase_apple_analytics_sdk.cmake, which is consumed by
     cmake/firebase_apple_desktop_sdk.cmake.

Only steps 2-4 need macOS / Xcode; the helpers are pure Python and are unit
tested in build_apple_analytics_sdk_test.py.
"""

import argparse
import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys

DEPS_TARGET = "FirebaseCppAppleAnalyticsDeps"
MANIFEST_JSON = "manifest.json"
MANIFEST_CMAKE = "firebase_apple_analytics_sdk.cmake"
PRIVACY_MANIFEST = "PrivacyInfo.xcprivacy"
STAMP_FILE = ".build_stamp"
# Minimum macOS version supported by the Firebase Apple SDK
# (firebase-ios-sdk Package.swift: .macOS(.v11)).
MIN_MACOS_DEPLOYMENT_TARGET = "11.0"
# Frameworks / libraries declared by the FirebaseAnalytics dependency graph
# (linkerSettings in firebase-ios-sdk, GoogleUtilities and GoogleAppMeasurement
# Package.swift files). Clang modules auto-link most of these as well; listing
# them keeps links deterministic when autolinking is disabled.
SYSTEM_FRAMEWORKS = [
    "AppKit",
    "CoreFoundation",
    "Foundation",
    "Network",
    "Security",
    "StoreKit",
    "SystemConfiguration",
]
SYSTEM_LIBRARIES = ["c++", "objc", "sqlite3", "z"]

_VERSION_RE = re.compile(
    r'firebase-ios-sdk(?:\.git)?"\s*,\s*exact:\s*"([0-9][0-9A-Za-z.\-]*)"')


# ---------------------------------------------------------------------------
# Pure helpers (unit tested).
# ---------------------------------------------------------------------------


def read_firebase_ios_sdk_version(package_swift_text):
  """Returns the firebase-ios-sdk version pinned in a Package.swift."""
  match = _VERSION_RE.search(package_swift_text)
  if not match:
    raise ValueError("Could not find an exact firebase-ios-sdk version.")
  return match.group(1)


def version_tuple(version):
  return tuple(int(part) for part in re.findall(r"\d+", version))


def clamp_deployment_target(requested):
  """Returns the deployment target to build with and an optional warning."""
  if not requested:
    return MIN_MACOS_DEPLOYMENT_TARGET, None
  if version_tuple(requested) < version_tuple(MIN_MACOS_DEPLOYMENT_TARGET):
    return MIN_MACOS_DEPLOYMENT_TARGET, (
        "The Firebase Apple SDK requires macOS %s or later, but the requested "
        "deployment target is %s. The embedded SDK is built for %s; the final "
        "binary will only run Analytics correctly on macOS %s+." %
        (MIN_MACOS_DEPLOYMENT_TARGET, requested, MIN_MACOS_DEPLOYMENT_TARGET,
         MIN_MACOS_DEPLOYMENT_TARGET))
  return requested, None


def render_template(text, values):
  """Replaces @KEY@ placeholders; fails on any unreplaced placeholder."""
  for key, value in values.items():
    text = text.replace("@%s@" % key, value)
  leftover = re.findall(r"@[A-Z][A-Z0-9_]*@", text)
  if leftover:
    raise ValueError("Unreplaced template placeholders: %s" %
                     ", ".join(sorted(set(leftover))))
  return text


def sanitize_module_name(name):
  return re.sub(r"[^A-Za-z0-9_]", "_", name)


def module_name_from_build_dir(dirname):
  """'FirebaseCore.build' -> 'FirebaseCore'."""
  base = os.path.basename(dirname.rstrip("/"))
  return base[:-len(".build")] if base.endswith(".build") else base


def select_macos_library(xcframework_info):
  """Returns the AvailableLibraries entry for plain macOS, or None."""
  for library in xcframework_info.get("AvailableLibraries", []):
    if (library.get("SupportedPlatform") == "macos" and
        not library.get("SupportedPlatformVariant")):
      return library
  return None


def framework_binary_relpath(library):
  """Relative path of the binary inside an xcframework library slice."""
  if library.get("BinaryPath"):
    return library["BinaryPath"]
  library_path = library["LibraryPath"]
  if library_path.endswith(".framework"):
    return os.path.join(library_path, library_path[:-len(".framework")])
  return library_path


def is_static_archive_description(file_description):
  """Interprets `file -b` output for a (possibly universal) binary."""
  return "ar archive" in file_description


def _merge_unique(items, key=None):
  seen = set()
  merged = []
  for item in items:
    marker = key(item) if key else (
        json.dumps(item, sort_keys=True) if isinstance(item, (dict, list))
        else item)
    if marker in seen:
      continue
    seen.add(marker)
    merged.append(item)
  return merged


def merge_privacy_manifests(manifests):
  """Merges PrivacyInfo.xcprivacy dictionaries into one.

  * NSPrivacyTracking: true if any manifest tracks.
  * NSPrivacyTrackingDomains: union.
  * NSPrivacyCollectedDataTypes: one entry per data type; Linked / Tracking
    are OR-ed and purposes are unioned.
  * NSPrivacyAccessedAPITypes: one entry per API type; reasons are unioned.
  """
  tracking = False
  domains = []
  collected = {}
  collected_order = []
  accessed = {}
  accessed_order = []
  for manifest in manifests:
    tracking = tracking or bool(manifest.get("NSPrivacyTracking", False))
    domains.extend(manifest.get("NSPrivacyTrackingDomains", []))
    for entry in manifest.get("NSPrivacyCollectedDataTypes", []):
      data_type = entry.get("NSPrivacyCollectedDataType")
      if data_type not in collected:
        collected[data_type] = {
            "NSPrivacyCollectedDataType": data_type,
            "NSPrivacyCollectedDataTypeLinked": False,
            "NSPrivacyCollectedDataTypeTracking": False,
            "NSPrivacyCollectedDataTypePurposes": [],
        }
        collected_order.append(data_type)
      merged = collected[data_type]
      merged["NSPrivacyCollectedDataTypeLinked"] |= bool(
          entry.get("NSPrivacyCollectedDataTypeLinked", False))
      merged["NSPrivacyCollectedDataTypeTracking"] |= bool(
          entry.get("NSPrivacyCollectedDataTypeTracking", False))
      merged["NSPrivacyCollectedDataTypePurposes"] = _merge_unique(
          merged["NSPrivacyCollectedDataTypePurposes"] +
          list(entry.get("NSPrivacyCollectedDataTypePurposes", [])))
    for entry in manifest.get("NSPrivacyAccessedAPITypes", []):
      api_type = entry.get("NSPrivacyAccessedAPIType")
      if api_type not in accessed:
        accessed[api_type] = {
            "NSPrivacyAccessedAPIType": api_type,
            "NSPrivacyAccessedAPITypeReasons": [],
        }
        accessed_order.append(api_type)
      accessed[api_type]["NSPrivacyAccessedAPITypeReasons"] = _merge_unique(
          accessed[api_type]["NSPrivacyAccessedAPITypeReasons"] +
          list(entry.get("NSPrivacyAccessedAPITypeReasons", [])))
  return {
      "NSPrivacyTracking": tracking,
      "NSPrivacyTrackingDomains": _merge_unique(domains),
      "NSPrivacyCollectedDataTypes": [collected[k] for k in collected_order],
      "NSPrivacyAccessedAPITypes": [accessed[k] for k in accessed_order],
  }


def _cmake_quote(value):
  return '"%s"' % str(value).replace("\\", "\\\\").replace('"', '\\"')


def _cmake_list(values):
  return _cmake_quote(";".join(values))


def render_cmake_manifest(manifest):
  """Renders firebase_apple_analytics_sdk.cmake from a manifest dict.

  Paths in the manifest are relative to the output directory and are emitted
  relative to ${CMAKE_CURRENT_LIST_DIR} so the output is relocatable (e.g.
  restored from a CI cache via FIREBASE_APPLE_ANALYTICS_SDK_DIR).
  """
  def rel(path):
    return "${CMAKE_CURRENT_LIST_DIR}/" + path

  modules = [m["name"] for m in manifest["modules"]]
  archives = [rel(m["archive"]) for m in manifest["modules"]]
  lines = [
      "# Generated by analytics/apple_desktop/build_apple_analytics_sdk.py.",
      "# Do not edit.",
      "set(FIREBASE_APPLE_ANALYTICS_SDK_VERSION %s)" %
      _cmake_quote(manifest["firebase_ios_sdk_version"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_ARCHS %s)" %
      _cmake_list(manifest["archs"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_DEPLOYMENT_TARGET %s)" %
      _cmake_quote(manifest["deployment_target"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_MODULES %s)" % _cmake_list(modules),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_ARCHIVES %s)" % _cmake_list(archives),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_HAS_SWIFT %s)" %
      ("ON" if manifest["has_swift"] else "OFF"),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_SWIFT_LIBRARY_DIRS %s)" %
      _cmake_list(manifest["swift_library_dirs"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_SYSTEM_FRAMEWORKS %s)" %
      _cmake_list(manifest["system_frameworks"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_SYSTEM_LIBRARIES %s)" %
      _cmake_list(manifest["system_libraries"]),
      "set(FIREBASE_APPLE_ANALYTICS_SDK_PRIVACY_MANIFEST %s)" %
      _cmake_quote(rel(manifest["privacy_manifest"])
                   if manifest.get("privacy_manifest") else ""),
  ]
  return "\n".join(lines) + "\n"


def render_size_report(manifest):
  """Human-readable per-module size table (sizes of universal archives)."""
  rows = sorted(manifest["modules"], key=lambda m: m["size_bytes"],
                reverse=True)
  total = sum(m["size_bytes"] for m in rows)
  width = max([len(m["name"]) for m in rows] + [6])
  lines = ["Firebase Apple Analytics SDK %s (%s), archive sizes before "
           "dead-stripping:" % (manifest["firebase_ios_sdk_version"],
                                ", ".join(manifest["archs"]))]
  for module in rows:
    lines.append("  %-*s %8.1f KiB  %s" % (width, module["name"],
                                          module["size_bytes"] / 1024.0,
                                          module["kind"]))
  lines.append("  %-*s %8.1f KiB" % (width, "TOTAL", total / 1024.0))
  return "\n".join(lines) + "\n"


def compute_stamp(version, archs, deployment_target, template_text):
  digest = hashlib.sha256()
  for part in (version, ",".join(archs), deployment_target, template_text):
    digest.update(part.encode("utf-8"))
    digest.update(b"\0")
  return digest.hexdigest()


# ---------------------------------------------------------------------------
# macOS build steps.
# ---------------------------------------------------------------------------


def _run(cmd, env=None, capture=False):
  print("+ " + " ".join(cmd), flush=True)
  result = subprocess.run(cmd, check=True, env=env, text=True,
                          stdout=subprocess.PIPE if capture else None)
  return result.stdout.strip() if capture else None


def _swift_env():
  env = dict(os.environ)
  # Same workaround as ios_spm/CMakeLists.txt for SwiftPM's bare checkouts.
  env.update({
      "GIT_CONFIG_COUNT": "1",
      "GIT_CONFIG_KEY_0": "safe.bareRepository",
      "GIT_CONFIG_VALUE_0": "all",
  })
  return env


def render_package(source_dir, package_dir, version, deployment_target):
  if os.path.isdir(package_dir):
    shutil.rmtree(package_dir)
  shutil.copytree(os.path.join(source_dir, "Sources"),
                  os.path.join(package_dir, "Sources"))
  with open(os.path.join(source_dir, "Package.swift.in")) as template:
    text = render_template(template.read(), {
        "FIREBASE_IOS_SDK_VERSION": version,
        "MACOS_DEPLOYMENT_TARGET": deployment_target,
    })
  with open(os.path.join(package_dir, "Package.swift"), "w") as package:
    package.write(text)


def build_arch(package_dir, scratch_dir, arch, configuration):
  base = ["swift", "build", "--package-path", package_dir, "--scratch-path",
          scratch_dir, "-c", configuration, "--arch", arch]
  _run(base + ["--product", DEPS_TARGET], env=_swift_env())
  return _run(base + ["--show-bin-path"], env=_swift_env(), capture=True)


def collect_module_objects(bin_path):
  """Returns {module_name: [object files]} for a SwiftPM bin directory."""
  modules = {}
  for entry in sorted(os.listdir(bin_path)):
    build_dir = os.path.join(bin_path, entry)
    if not entry.endswith(".build") or not os.path.isdir(build_dir):
      continue
    name = module_name_from_build_dir(entry)
    if name == DEPS_TARGET:
      continue
    objects = []
    for root, _, files in os.walk(build_dir):
      objects.extend(os.path.join(root, f) for f in files if f.endswith(".o"))
    if objects:
      modules[name] = sorted(objects)
  return modules


def archive_modules(per_arch_objects, work_dir, lib_dir):
  """Creates universal lib<Module>.a archives. Returns module records."""
  os.makedirs(lib_dir, exist_ok=True)
  names = sorted(set().union(*[set(m) for m in per_arch_objects.values()]))
  records = []
  for name in names:
    thin = []
    has_swift = False
    for arch, modules in sorted(per_arch_objects.items()):
      objects = modules.get(name)
      if not objects:
        raise RuntimeError("Module %s was not built for %s" % (name, arch))
      has_swift = has_swift or any(o.endswith(".swift.o") for o in objects)
      arch_dir = os.path.join(work_dir, arch)
      os.makedirs(arch_dir, exist_ok=True)
      archive = os.path.join(arch_dir, "lib%s.a" % sanitize_module_name(name))
      if os.path.exists(archive):
        os.remove(archive)
      _run(["libtool", "-static", "-no_warning_for_no_symbols", "-o", archive]
           + objects)
      thin.append(archive)
    universal = os.path.join(lib_dir, "lib%s.a" % sanitize_module_name(name))
    if len(thin) == 1:
      shutil.copyfile(thin[0], universal)
    else:
      _run(["lipo", "-create", "-output", universal] + thin)
    records.append({
        "name": name,
        "kind": "swift" if has_swift else "source",
        "archive": os.path.relpath(universal, os.path.dirname(lib_dir)),
        "size_bytes": os.path.getsize(universal),
    })
  return records


def extract_binary_frameworks(scratch_dir, frameworks_dir, output_dir):
  """Copies static macOS slices of downloaded binary xcframeworks."""
  records = []
  artifacts = os.path.join(scratch_dir, "artifacts")
  os.makedirs(frameworks_dir, exist_ok=True)
  for root, dirs, _ in os.walk(artifacts):
    for directory in sorted(dirs):
      if not directory.endswith(".xcframework"):
        continue
      xcframework = os.path.join(root, directory)
      with open(os.path.join(xcframework, "Info.plist"), "rb") as info_file:
        library = select_macos_library(plistlib.load(info_file))
      name = directory[:-len(".xcframework")]
      if library is None:
        print("Skipping %s: no macOS slice." % name)
        continue
      slice_dir = os.path.join(xcframework, library["LibraryIdentifier"])
      binary = os.path.join(slice_dir, framework_binary_relpath(library))
      description = _run(["file", "-b", binary], capture=True)
      if not is_static_archive_description(description):
        raise RuntimeError(
            "%s's macOS slice is not a static library (%s); it cannot be "
            "embedded." % (name, description))
      destination = os.path.join(frameworks_dir, library["LibraryPath"])
      if os.path.exists(destination):
        shutil.rmtree(destination)
      shutil.copytree(os.path.join(slice_dir, library["LibraryPath"]),
                      destination, symlinks=True)
      archive = os.path.join(frameworks_dir, framework_binary_relpath(library))
      records.append({
          "name": name,
          "kind": "binary",
          "archive": os.path.relpath(archive, output_dir),
          "size_bytes": os.path.getsize(archive),
      })
    # Don't descend into xcframeworks.
    dirs[:] = [d for d in dirs if not d.endswith(".xcframework")]
  return records


def find_privacy_manifests(*roots):
  found = []
  for root in roots:
    for dirpath, _, files in os.walk(root):
      if PRIVACY_MANIFEST in files:
        found.append(os.path.join(dirpath, PRIVACY_MANIFEST))
  return sorted(set(found))


def swift_library_dirs():
  sdk = _run(["xcrun", "--sdk", "macosx", "--show-sdk-path"], capture=True)
  swift = _run(["xcrun", "--find", "swift"], capture=True)
  toolchain_lib = os.path.join(os.path.dirname(os.path.dirname(swift)), "lib",
                               "swift", "macosx")
  return [os.path.join(sdk, "usr", "lib", "swift"), toolchain_lib]


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  script_dir = os.path.dirname(os.path.abspath(__file__))
  repo_root = os.path.dirname(os.path.dirname(script_dir))
  parser.add_argument("--output-dir", required=True)
  parser.add_argument("--archs", nargs="+", default=["arm64", "x86_64"])
  parser.add_argument("--deployment-target", default="")
  parser.add_argument("--configuration", default="release",
                      choices=["release", "debug"])
  parser.add_argument("--firebase-ios-sdk-version", default="",
                      help="Defaults to the version in ios_spm/Package.swift.")
  parser.add_argument("--force", action="store_true",
                      help="Rebuild even if the output is up to date.")
  args = parser.parse_args(argv)

  if sys.platform != "darwin":
    parser.error("This script must run on macOS with Xcode installed.")

  version = args.firebase_ios_sdk_version
  if not version:
    with open(os.path.join(repo_root, "ios_spm", "Package.swift")) as f:
      version = read_firebase_ios_sdk_version(f.read())
  deployment_target, warning = clamp_deployment_target(args.deployment_target)
  if warning:
    print("WARNING: " + warning, file=sys.stderr)

  output_dir = os.path.abspath(args.output_dir)
  with open(os.path.join(script_dir, "Package.swift.in")) as f:
    stamp = compute_stamp(version, args.archs, deployment_target, f.read())
  stamp_path = os.path.join(output_dir, STAMP_FILE)
  if (not args.force and os.path.exists(stamp_path) and
      os.path.exists(os.path.join(output_dir, MANIFEST_CMAKE))):
    with open(stamp_path) as f:
      if f.read().strip() == stamp:
        print("Firebase Apple Analytics SDK is up to date in " + output_dir)
        return 0

  package_dir = os.path.join(output_dir, "package")
  scratch_dir = os.path.join(output_dir, "spm")
  render_package(script_dir, package_dir, version, deployment_target)
  _run(["swift", "package", "--package-path", package_dir, "--scratch-path",
        scratch_dir, "resolve"], env=_swift_env())

  per_arch_objects = {}
  bin_paths = []
  for arch in args.archs:
    bin_path = build_arch(package_dir, scratch_dir, arch, args.configuration)
    bin_paths.append(bin_path)
    per_arch_objects[arch] = collect_module_objects(bin_path)

  for stale in ("lib", "frameworks"):
    shutil.rmtree(os.path.join(output_dir, stale), ignore_errors=True)
  modules = archive_modules(per_arch_objects,
                            os.path.join(output_dir, "thin"),
                            os.path.join(output_dir, "lib"))
  modules += extract_binary_frameworks(scratch_dir,
                                       os.path.join(output_dir, "frameworks"),
                                       output_dir)

  privacy_sources = find_privacy_manifests(
      *(bin_paths + [os.path.join(output_dir, "frameworks")]))
  manifests = []
  for path in privacy_sources:
    with open(path, "rb") as f:
      manifests.append(plistlib.load(f))
  privacy_manifest = None
  if manifests:
    privacy_manifest = PRIVACY_MANIFEST
    with open(os.path.join(output_dir, PRIVACY_MANIFEST), "wb") as f:
      plistlib.dump(merge_privacy_manifests(manifests), f)

  has_swift = any(m["kind"] == "swift" for m in modules)
  manifest = {
      "firebase_ios_sdk_version": version,
      "archs": list(args.archs),
      "deployment_target": deployment_target,
      "modules": modules,
      "has_swift": has_swift,
      "swift_library_dirs": swift_library_dirs() if has_swift else [],
      "system_frameworks": SYSTEM_FRAMEWORKS,
      "system_libraries": SYSTEM_LIBRARIES,
      "privacy_manifest": privacy_manifest,
      "privacy_manifest_sources": [os.path.relpath(p, output_dir)
                                   for p in privacy_sources],
  }
  with open(os.path.join(output_dir, MANIFEST_JSON), "w") as f:
    json.dump(manifest, f, indent=2, sort_keys=True)
  with open(os.path.join(output_dir, MANIFEST_CMAKE), "w") as f:
    f.write(render_cmake_manifest(manifest))
  report = render_size_report(manifest)
  with open(os.path.join(output_dir, "size_report.txt"), "w") as f:
    f.write(report)
  print(report)
  with open(stamp_path, "w") as f:
    f.write(stamp + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
