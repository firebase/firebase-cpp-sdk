cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(binary_to_array)

project(firebase_testing NONE)
enable_language(C)
enable_language(CXX)

# Build the testdata_config generated files using flatbuffers
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
build_flatbuffers("${CMAKE_CURRENT_LIST_DIR}/testdata_config.fbs"
                  ""
                  "generate_testing_fps"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/testing"
                  ""
                  "")
binary_to_array("testdata_config_resource"
    "${CMAKE_CURRENT_LIST_DIR}/testdata_config.fbs"
    "firebase::testing::cppsdk"
    "${FIREBASE_GEN_FILE_DIR}/testing")


set(config_common_SRCS
    config.h
    config.cc
    config_test.cc
    ${FIREBASE_GEN_FILE_DIR}/testing/testdata_config_generated.h
    ${FIREBASE_GEN_FILE_DIR}/testing/testdata_config_resource.h
    ${FIREBASE_GEN_FILE_DIR}/testing/testdata_config_resource.cc)
set(config_android_SRCS
    config_android.cc)
set(config_ios_SRCS
    config_ios.h
    config_ios.mm)
set(config_desktop_SRCS
    config_desktop.h
    config_desktop.cc)
if(ANDROID)
    set(config_SRCS
        "${config_common_SRCS}"
        "${config_android_SRCS}")
elseif(IOS)
    set(config_SRCS
        "${config_common_SRCS}"
        "${config_ios_SRCS}")
else()
    set(config_SRCS
        "${config_common_SRCS}"
        "${config_desktop_SRCS}")
endif()

set(reporter_common_SRCS
    reporter.h
    reporter.cc
    reporter_test.cc
    reporter_impl.h
    reporter_impl.cc
    reporter_impl_test.cc
)
set(reporter_android_SRCS
    reporter_android.cc)
if(ANDROID)
    set(reporter_SRCS
        "${reporter_common_SRCS}"
        "${reporter_android_SRCS}")
else()
    set(reporter_SRCS
        "${reporter_common_SRCS}")
endif()

set(ticker_common_SRCS
    ticker.h
    ticker_test.cc)
set(ticker_android_SRCS
    ticker_android.cc)
set(ticker_ios_SRCS
    ticker_ios.h)
set(ticker_desktop_SRCS
    ticker_desktop.h
    ticker_desktop.cc)
if(ANDROID)
    set(ticker_SRCS
        "${ticker_common_SRCS}"
        "${ticker_android_SRCS}")
elseif(IOS)
    set(ticker_SRCS
        "${ticker_common_SRCS}"
        "${ticker_ios_SRCS}")
else()
    set(ticker_SRCS
        "${ticker_common_SRCS}"
        "${ticker_desktop_SRCS}")
endif()

set(util_android_SRCS
    util_android.h
    util_android.cc
    util_android_test.cc)
set(util_ios_SRCS
    util_ios.h
    util_ios.mm
    util_ios_test.mm)
if(ANDROID)
    set(util_SRCS
        "${util_android_SRCS}")
elseif(IOS)
    set(util_SRCS
        "${util_ios_SRCS}")
else()
    set(util_SRCS "")
endif()

add_library(firebase_testing STATIC
    ${config_SRCS}
    ${reporter_SRCS}
    ${ticker_SRCS}
    ${util_SRCS})

target_include_directories(firebase_testing
    PUBLIC
        ${FLATBUFFERS_SOURCE_DIR}/include
    PRIVATE
        ${FIREBASE_CPP_SDK_ROOT_DIR}
        ${FIREBASE_GEN_FILE_DIR}
)

target_link_libraries(firebase_testing
    PRIVATE
        gtest
        gmock
)
