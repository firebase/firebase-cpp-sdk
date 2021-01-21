# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Applications/CMake.app/Contents/bin/cmake

# The command to remove a file.
RM = /Applications/CMake.app/Contents/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/cmake/external

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external

# Utility rule file for googletest.

# Include the progress variables for this target.
include CMakeFiles/googletest.dir/progress.make

CMakeFiles/googletest: CMakeFiles/googletest-complete


CMakeFiles/googletest-complete: src/googletest-stamp/googletest-install
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-mkdir
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-download
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-update
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-patch
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-configure
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-build
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-install
CMakeFiles/googletest-complete: src/googletest-stamp/googletest-test
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Completed 'googletest'"
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles
	/Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles/googletest-complete
	/Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-done

src/googletest-stamp/googletest-install: src/googletest-stamp/googletest-build
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "No install step for 'googletest'"
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E echo_append
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-install

src/googletest-stamp/googletest-mkdir:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Creating directories for 'googletest'"
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/tmp
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/downloads
	/Applications/CMake.app/Contents/bin/cmake -E make_directory /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp
	/Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-mkdir

src/googletest-stamp/googletest-download: src/googletest-stamp/googletest-urlinfo.txt
src/googletest-stamp/googletest-download: src/googletest-stamp/googletest-mkdir
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Performing download step (download, verify and extract) for 'googletest'"
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src && /Applications/CMake.app/Contents/bin/cmake -P /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/download-googletest.cmake
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src && /Applications/CMake.app/Contents/bin/cmake -P /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/verify-googletest.cmake
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src && /Applications/CMake.app/Contents/bin/cmake -P /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/extract-googletest.cmake
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src && /Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-download

src/googletest-stamp/googletest-update: src/googletest-stamp/googletest-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "No update step for 'googletest'"
	/Applications/CMake.app/Contents/bin/cmake -E echo_append
	/Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-update

src/googletest-stamp/googletest-patch: src/googletest-stamp/googletest-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "No patch step for 'googletest'"
	/Applications/CMake.app/Contents/bin/cmake -E echo_append
	/Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-patch

src/googletest-stamp/googletest-configure: tmp/googletest-cfgcmd.txt
src/googletest-stamp/googletest-configure: src/googletest-stamp/googletest-update
src/googletest-stamp/googletest-configure: src/googletest-stamp/googletest-patch
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "No configure step for 'googletest'"
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E echo_append
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-configure

src/googletest-stamp/googletest-build: src/googletest-stamp/googletest-configure
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "No build step for 'googletest'"
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E echo_append
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-build

src/googletest-stamp/googletest-test: src/googletest-stamp/googletest-install
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "No test step for 'googletest'"
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E echo_append
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-build && /Applications/CMake.app/Contents/bin/cmake -E touch /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/src/googletest-stamp/googletest-test

googletest: CMakeFiles/googletest
googletest: CMakeFiles/googletest-complete
googletest: src/googletest-stamp/googletest-install
googletest: src/googletest-stamp/googletest-mkdir
googletest: src/googletest-stamp/googletest-download
googletest: src/googletest-stamp/googletest-update
googletest: src/googletest-stamp/googletest-patch
googletest: src/googletest-stamp/googletest-configure
googletest: src/googletest-stamp/googletest-build
googletest: src/googletest-stamp/googletest-test
googletest: CMakeFiles/googletest.dir/build.make

.PHONY : googletest

# Rule to build all files generated by this target.
CMakeFiles/googletest.dir/build: googletest

.PHONY : CMakeFiles/googletest.dir/build

CMakeFiles/googletest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/googletest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/googletest.dir/clean

CMakeFiles/googletest.dir/depend:
	cd /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/cmake/external /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/cmake/external /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external /Users/amaurice/Documents/GitHub/firebase-cpp-sdk/macbuild/external/CMakeFiles/googletest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/googletest.dir/depend

