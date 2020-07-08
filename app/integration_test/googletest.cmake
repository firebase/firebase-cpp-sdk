# Download GoogleTest from GitHub as an external project.
# This CMake file is taken from:
# https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project

cmake_minimum_required(VERSION 2.8.2)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           master
  SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/src"
  BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
