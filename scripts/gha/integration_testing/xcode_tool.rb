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
#
#
# Configures C++ integration test XCode projects.
#
# This automates the configuration of XCode projects as outlined in the
# associated testapp readme files. This includes adding frameworks, enabling
# capabilities, and patching files with required data.
# See the C++ testapp readmes for details on these changes.
#
# Usage: xcode_tool.rb [options]
# -d [dir],               XCode project directory (required)
#   --XCodeCPP.xcodeProjectDir
# -t [target],            Assume target to match the .xcodeproj name. (required)
#   --XCodeCPP.target
# -f [frameworks],        Paths to the custom frameworks. (required)
#   --XCodeCPP.frameworks
# -e [entitlement_path],  Path to entitlements (optional)
#   --XCodeCPP.entitlement
# -i [include_path],      Path to additional include files (optional)
#   --XCodeCPP.include

require 'optparse'
require 'xcodeproj'

# Performs all the product-specific changes to the xcode project provided by
# the xcodeProjectDir flag.
def main
  OptionParser.new do |opts|
    opts.banner = 'Usage: xcode_tool.rb [options]'
    opts.on('-d', '--XCodeCPP.xcodeProjectDir [dir]',
      'XCode project directory (required)') do |xcode_project_dir|
      @xcode_project_dir = xcode_project_dir
    end
    opts.on('-t', '--XCodeCPP.target [target]',
      'Assume target to match the .xcodeproj name. (required)') do |target|
      @target_name = target
    end
    opts.on('-f', '--XCodeCPP.frameworks [frameworks]',
      'Paths to the custom frameworks. (required)') do |frameworks|
      @frameworks = frameworks.split(",")
    end
    opts.on('-e', '--XCodeCPP.entitlement [entitlement_path]',
      'Path to entitlements (optional)') do |entitlement_path|
      @entitlement_path = entitlement_path
    end
    opts.on('-i', '--XCodeCPP.include [include_path]',
      'Path to additional include files (optional)') do |include_path|
      @include_path = include_path
    end
  end.parse!

  raise OptionParser::MissingArgument,'-d' if @xcode_project_dir.nil?
  raise OptionParser::MissingArgument,'-t' if @target_name.nil?
  raise OptionParser::MissingArgument,'-f' if @frameworks.nil?

  project_path = "#@xcode_project_dir/integration_test.xcodeproj"
  @project = Xcodeproj::Project.open(project_path)
  for t in @project.targets
    if t.name == @target_name
      @target = t
      puts "Find target #@target"
      break
    end 
  end

  # Examine components rather than substrings to minimize false positives.
  # Note: this is not ideal. This tool should not be responsible for figuring
  # out which project it's modifying. That responsibility should belong to
  # the Python tool invoking this.
  path_components = project_path.split('/')
  if path_components.include?('FirebaseAuth')
    make_changes_for_auth
  elsif path_components.include?('FirebaseMessaging')
    make_changes_for_messaging
  elsif path_components.include?('FirebaseDynamicLinks')
    make_changes_for_dynamiclinks
  end

  framework_dir = "#@xcode_project_dir/Frameworks"
  set_build_setting('FRAMEWORK_SEARCH_PATHS', ['${inherited}', framework_dir])
  if !@include_path.nil?
    append_to_build_setting('HEADER_SEARCH_PATHS', @include_path)
  end

  @frameworks.each do |framework|
    add_custom_framework(framework)
  end

  # Bitcode is unnecessary, as we are not submitting these to the Apple store.
  # Disabling bitcode significantly speeds up builds.
  set_build_setting('ENABLE_BITCODE', 'NO')

  @project.save
end

def make_changes_for_auth
  puts 'Auth testapp detected.'
  add_entitlements
  add_system_framework('UserNotifications')
  puts 'Finished making auth-specific changes.'
end

def make_changes_for_messaging
  puts 'Messaging testapp detected.'
  add_entitlements
  add_system_framework('UserNotifications')
  enable_remote_notification
  puts 'Finished making messaging-specific changes.'
end

def make_changes_for_dynamiclinks
  puts 'Dynamic Links testapp detected.'
  add_entitlements
  puts 'Finished making Dynamic Links-specific changes.'
end

def add_entitlements
  raise OptionParser::MissingArgument,'-e' if @entitlement_path.nil?

  puts "Adding entitlement: #@entitlement_path"
  entitlement_name = File.basename(@entitlement_path)
  relative_entitlement_destination = "#@xcode_project_dir/#{entitlement_name}"
  @project.new_file(relative_entitlement_destination)
  set_build_setting('CODE_SIGN_ENTITLEMENTS', relative_entitlement_destination)
  puts 'Added entitlement to xcode project.'
end

# This only involves patching a plist file, which should be moved to the
# Python tool.
def enable_remote_notification
  puts 'Adding remote-notification to UIBackgroundModes...'
  info_plist_path = "#@xcode_project_dir/Info.plist"
  info_plist =  Xcodeproj::Plist.read_from_path(info_plist_path)
  url_types = {'UIBackgroundModes' => ['remote-notification']}
  Xcodeproj::Plist.write_to_path(info_plist.merge(url_types), info_plist_path)
  puts 'Finished adding remote-notification.'
end

# A system framework is a framework included with MacOS.
#
# Args:
# - framework: string
#
def add_system_framework(framework)
  puts "Adding framework to xcode project: #{framework}."
  @target.add_system_framework(framework);
  puts 'Finished adding framework.'
end

# A custom framework is not included with MacOS, e.g. firebase_auth.framework.
#
# Args:
# - framework_path: string
#
def add_custom_framework(framework_path)
  puts "Adding framework to xcode project: #{framework_path}."
  framework_name = File.basename(framework_path);
  local_framework_path = "Frameworks/#{framework_name}"
  # Add the lib file as a reference
  libRef = @project['Frameworks'].new_file(framework_path)
  # Add it to the build phase
  @target.frameworks_build_phase.add_file_reference(libRef)
  puts 'Finished adding framework.'
end

def set_build_setting(key, value)
  @target.build_configurations.each do |config|
    config.build_settings[key] = value
  end
end

def append_to_build_setting(key, value)
  @target.build_configurations.each do |config|
    config.build_settings[key].append(value)
  end
end

if __FILE__ == $0
  main()
end
