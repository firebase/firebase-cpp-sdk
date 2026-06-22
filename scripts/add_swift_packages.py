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

"""Surgically patches a CMake-generated Xcode project to inject Swift Package Manager dependencies.

This script runs post-CMake generation to configure the native Firebase iOS SDK
Swift Package dependencies directly inside the Xcode project.
"""

import argparse
import os
import re
import sys
import uuid

# Mapping from Firebase C++ target names to their corresponding Swift Package product names
TARGET_TO_PRODUCTS = {
    "firebase_app": ["FirebaseCore"],
    "firebase_auth": ["FirebaseAuth"],
    "firebase_database": ["FirebaseDatabase"],
    "firebase_firestore": ["FirebaseFirestore"],
    "firebase_functions": ["FirebaseFunctions"],
    "firebase_messaging": ["FirebaseMessaging"],
    "firebase_storage": ["FirebaseStorage"],
    "firebase_app_check": ["FirebaseAppCheck"],
    "firebase_installations": ["FirebaseInstallations"],
    "firebase_remote_config": ["FirebaseRemoteConfig"],
    "firebase_analytics": ["FirebaseAnalytics"],
}

PACKAGE_URL = "https://github.com/firebase/firebase-ios-sdk.git"
PACKAGE_NAME = "firebase-ios-sdk"


def generate_xcode_uuid():
    """Generates a unique 24-character hexadecimal string suitable for Xcode UUIDs."""
    return uuid.uuid4().hex[:24].upper()


class PbxprojPatcher:
    def __init__(self, file_path, sdk_version):
        self.file_path = file_path
        self.sdk_version = sdk_version
        with open(file_path, "r", encoding="utf-8") as f:
            self.content = f.read()

        # Track existing UUIDs to maintain idempotency
        self.package_ref_uuid = None
        self.product_to_uuid = {}

    def patch(self):
        """Executes the patching process and writes changes back if modified."""
        print(f"Patching Xcode project file: {self.file_path}")
        
        # 1. Resolve or create the package reference for the Firebase iOS SDK
        self._resolve_package_reference()

        # 2. Find all target blocks and their names
        targets = self._find_targets()
        if not targets:
            print("Warning: No PBXNativeTarget objects found in the project.")
            return

        # 3. Resolve or create product dependency objects for needed products
        needed_products = set()
        for target_name in targets:
            if target_name in TARGET_TO_PRODUCTS:
                needed_products.update(TARGET_TO_PRODUCTS[target_name])

        for product in sorted(needed_products):
            self._resolve_product_dependency(product)

        # 4. Associate product dependencies with the C++ targets
        modified = False
        for target_name, target_uuid in targets.items():
            if target_name in TARGET_TO_PRODUCTS:
                products = TARGET_TO_PRODUCTS[target_name]
                product_uuids = [self.product_to_uuid[p] for p in products]
                if self._add_dependencies_to_target(target_uuid, target_name, product_uuids):
                    modified = True

        # 5. Add the package reference to the main PBXProject object
        if self._add_package_to_project():
            modified = True

        # 6. Strip legacy build settings (SYMROOT/OBJROOT) to enable SPM
        if self._strip_legacy_build_settings():
            modified = True

        if modified:
            with open(self.file_path, "w", encoding="utf-8") as f:
                f.write(self.content)
            print("Successfully patched Xcode project with SPM dependencies.")
        else:
            print("Xcode project is already up-to-date. No changes made.")

    def _resolve_package_reference(self):
        """Finds the existing package reference UUID or creates a new one."""
        # Check if XCRemoteSwiftPackageReference section exists
        section_match = re.search(
            r"(/\* Begin XCRemoteSwiftPackageReference section \*/\n)(.*?)(/\* End XCRemoteSwiftPackageReference section \*/)",
            self.content,
            re.DOTALL
        )

        if section_match:
            section_content = section_match.group(2)
            # Look for our package URL
            pkg_match = re.search(
                r"([0-9A-F]{24}) /\* [^*]+ \*/ = \{\n\s+isa = XCRemoteSwiftPackageReference;\n\s+repositoryURL = \"" + re.escape(PACKAGE_URL) + r"\";",
                section_content
            )
            if pkg_match:
                self.package_ref_uuid = pkg_match.group(1)
                print(f"Found existing Package Reference UUID: {self.package_ref_uuid}")
                return

        # If not found, generate a new one and inject it
        self.package_ref_uuid = generate_xcode_uuid()
        print(f"Creating new Package Reference UUID: {self.package_ref_uuid} for {PACKAGE_URL}")

        package_block = f"""\t\t{self.package_ref_uuid} /* {PACKAGE_NAME} */ = {{
			isa = XCRemoteSwiftPackageReference;
			repositoryURL = "{PACKAGE_URL}";
			requirement = {{
				kind = exactVersion;
				version = {self.sdk_version};
			}};
		}};\n"""

        if section_match:
            # Insert into existing section
            start_pos = section_match.start(2)
            self.content = self.content[:start_pos] + package_block + self.content[start_pos:]
        else:
            # Create the section and insert it before another section or at the end of objects
            new_section = f"""/* Begin XCRemoteSwiftPackageReference section */
{package_block}/* End XCRemoteSwiftPackageReference section */\n"""
            self._insert_section(new_section)

    def _resolve_product_dependency(self, product_name):
        """Finds the existing product dependency UUID for a product, or creates a new one."""
        section_match = re.search(
            r"(/\* Begin XCSwiftPackageProductDependency section \*/\n)(.*?)(/\* End XCSwiftPackageProductDependency section \*/)",
            self.content,
            re.DOTALL
        )

        if section_match:
            section_content = section_match.group(2)
            # Look for the product name within our package
            prod_match = re.search(
                r"([0-9A-F]{24}) /\* ([^*]+) \*/ = \{\n\s+isa = XCSwiftPackageProductDependency;\n\s+package = " + self.package_ref_uuid + r" /\* [^*]+ \*/;\n\s+productName = " + re.escape(product_name) + r";",
                section_content
            )
            if prod_match:
                uuid_found = prod_match.group(1)
                self.product_to_uuid[product_name] = uuid_found
                print(f"Found existing Product Dependency UUID for {product_name}: {uuid_found}")
                return

        # If not found, generate a new one and inject it
        prod_uuid = generate_xcode_uuid()
        self.product_to_uuid[product_name] = prod_uuid
        print(f"Creating new Product Dependency UUID: {prod_uuid} for {product_name}")

        product_block = f"""\t\t{prod_uuid} /* {product_name} */ = {{
			isa = XCSwiftPackageProductDependency;
			package = {self.package_ref_uuid} /* {PACKAGE_NAME} */;
			productName = {product_name};
		}};\n"""

        if section_match:
            # Insert into existing section
            start_pos = section_match.start(2)
            self.content = self.content[:start_pos] + product_block + self.content[start_pos:]
        else:
            new_section = f"""/* Begin XCSwiftPackageProductDependency section */
{product_block}/* End XCSwiftPackageProductDependency section */\n"""
            self._insert_section(new_section)

    def _find_targets(self):
        """Scans the file and returns a dictionary of target_name -> target_uuid."""
        targets = {}
        # Regex to find PBXNativeTarget definitions
        native_targets_match = re.findall(
            r"([0-9A-F]{24}) /\* ([^*]+) \*/ = \{\n\s+isa = PBXNativeTarget;.*?\n\s+name = ([^;]+);",
            self.content,
            re.DOTALL
        )
        for match in native_targets_match:
            uuid_val, comment_name, name_val = match
            # Strip quotes from target name if present
            name = name_val.strip('"').strip("'")
            targets[name] = uuid_val
        return targets

    def _add_dependencies_to_target(self, target_uuid, target_name, product_uuids):
        """Adds product dependencies to a specific target block. Returns True if modified."""
        # Find the target block
        target_pattern = r"(\t\t" + target_uuid + r" /\* " + re.escape(target_name) + r" \*/ = \{\n\s+isa = PBXNativeTarget;.*?\n\t\t\};)"
        target_match = re.search(target_pattern, self.content, re.DOTALL)
        if not target_match:
            print(f"Error: Could not find target block for {target_name} ({target_uuid})")
            return False

        target_block = target_match.group(1)
        original_block = target_block

        # Check if packageProductDependencies already exists in this target
        dep_list_match = re.search(r"packageProductDependencies = \(\n(.*?)\n\s+\);", target_block, re.DOTALL)

        if dep_list_match:
            existing_deps_content = dep_list_match.group(1)
            modified_deps_content = existing_deps_content
            for p_uuid in product_uuids:
                if p_uuid not in existing_deps_content:
                    # Find product name for comment
                    prod_name = [k for k, v in self.product_to_uuid.items() if v == p_uuid][0]
                    modified_deps_content += f"\t\t\t\t{p_uuid} /* {prod_name} */,\n"
            
            if modified_deps_content != existing_deps_content:
                new_dep_list = f"packageProductDependencies = (\n{modified_deps_content}\t\t\t);"
                target_block = target_block.replace(dep_list_match.group(0), new_dep_list)
        else:
            # Create packageProductDependencies list right before the closing };
            dep_items = ""
            for p_uuid in product_uuids:
                prod_name = [k for k, v in self.product_to_uuid.items() if v == p_uuid][0]
                dep_items += f"\t\t\t\t{p_uuid} /* {prod_name} */,\n"
            
            dep_list = f"\t\t\tpackageProductDependencies = (\n{dep_items}\t\t\t);\n\t\t}};"
            target_block = target_block.replace("\t\t};", dep_list)

        if target_block != original_block:
            self.content = self.content.replace(original_block, target_block)
            print(f"Added SPM dependencies to target: {target_name}")
            return True
        return False

    def _add_package_to_project(self):
        """Adds the package reference to the main PBXProject object's packageReferences list."""
        # Find the PBXProject block
        project_match = re.search(
            r"(\t\t[0-9A-F]{24} /\* Project object \*/ = \{\n\s+isa = PBXProject;.*?\n\t\t\};)",
            self.content,
            re.DOTALL
        )
        if not project_match:
            print("Error: Could not find the main PBXProject object.")
            return False

        project_block = project_match.group(1)
        original_block = project_block

        # Check if packageReferences already exists in PBXProject
        pkg_list_match = re.search(r"packageReferences = \(\n(.*?)\n\s+\);", project_block, re.DOTALL)

        if pkg_list_match:
            existing_pkgs = pkg_list_match.group(1)
            if self.package_ref_uuid not in existing_pkgs:
                modified_pkgs = existing_pkgs + f"\t\t\t\t{self.package_ref_uuid} /* {PACKAGE_NAME} */,\n"
                new_pkg_list = f"packageReferences = (\n{modified_pkgs}\t\t\t);"
                project_block = project_block.replace(pkg_list_match.group(0), new_pkg_list)
        else:
            # Create packageReferences list right before the closing }; of PBXProject
            pkg_list = f"\t\t\tpackageReferences = (\n\t\t\t\t{self.package_ref_uuid} /* {PACKAGE_NAME} */,\n\t\t\t);\n\t\t}};"
            project_block = project_block.replace("\t\t};", pkg_list)

        if project_block != original_block:
            self.content = self.content.replace(original_block, project_block)
            print("Added package reference to PBXProject object.")
            return True
        return False

    def _strip_legacy_build_settings(self):
        """Strips out SYMROOT and OBJROOT declarations from the project.pbxproj file.
        
        This forces Xcode to use the modern, unique build locations (DerivedData)
        which is a strict requirement for Swift Package Manager integration.
        """
        pattern = r"^[ \t]*(SYMROOT|OBJROOT)[ \t]*=[ \t]*[^;\n]+;\n"
        new_content, count = re.subn(pattern, "", self.content, flags=re.MULTILINE)
        if count > 0:
            print(f"Removed {count} legacy build settings (SYMROOT/OBJROOT) lines.")
            self.content = new_content
            return True
        return False

    def _insert_section(self, section_content):
        """Inserts a new object section inside the objects dictionary block."""
        # We can find the 'objects = {' line and insert it immediately after
        objects_pos = self.content.find("objects = {")
        if objects_pos == -1:
            print("Error: Could not find 'objects = {' in project.pbxproj")
            sys.exit(1)
        
        insert_pos = objects_pos + len("objects = {\n")
        self.content = self.content[:insert_pos] + section_content + self.content[insert_pos:]


def main():
    parser = argparse.ArgumentParser(description="Patch Xcode project with Swift Packages.")
    parser.add_argument(
        "--project_path",
        required=True,
        help="Path to the directory containing the Xcode project (or path to the .xcodeproj directory itself)."
    )
    parser.add_argument(
        "--sdk_version",
        default="12.15.0",
        help="Target Firebase iOS SDK version (default: 12.15.0)."
    )
    args = parser.parse_args()

    # Locate the project.pbxproj file
    project_dir = args.project_path
    if not project_dir.endswith(".xcodeproj"):
        # Check if there is a .xcodeproj folder inside the directory
        xcodeproj_folders = [f for f in os.listdir(project_dir) if f.endswith(".xcodeproj")]
        if not xcodeproj_folders:
            print(f"Error: No .xcodeproj directory found at {project_dir}")
            sys.exit(1)
        project_dir = os.path.join(project_dir, xcodeproj_folders[0])

    pbxproj_path = os.path.join(project_dir, "project.pbxproj")
    if not os.path.exists(pbxproj_path):
        print(f"Error: Could not find project.pbxproj at {pbxproj_path}")
        sys.exit(1)

    patcher = PbxprojPatcher(pbxproj_path, args.sdk_version)
    patcher.patch()


if __name__ == "__main__":
    main()
