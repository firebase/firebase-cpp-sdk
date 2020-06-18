# Copyright 2019 Google LLC
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

if(ANDROID OR IOS)
    set(messaging_test_util_common_SRCS
        messaging_test_util.h)
    set(messaging_test_util_android_SRCS
        android/messaging_test_util.cc)
    set(messaging_test_util_ios_SRCS
        ios/messaging_test_util.mm)

    if(ANDROID)
        set(messaging_test_util_SRCS
            "${messaging_test_util_common_SRCS}"
            "${messaging_test_util_android_SRCS}")
    elseif(IOS)
        set(messaging_test_util_SRCS
            "${messaging_test_util_common_SRCS}"
            "${messaging_test_util_ios_SRCS}")
    else()
        set(messaging_test_util_SRCS
            "")
    endif()

    add_library(firebase_messaging_test_util STATIC
        ${messaging_test_util_SRCS})

    target_include_directories(firebase_messaging_test_util
        PRIVATE
            ${FIREBASE_CPP_SDK_ROOT_DIR}
            ${FIREBASE_CPP_SDK_ROOT_DIR}/app/src/include
            ${FIREBASE_GEN_FILE_DIR}
    )

    target_link_libraries(firebase_messaging_test_util
        PRIVATE
            gtest
            gmock
    )

    firebase_cpp_cc_test(
        firebase_messaging_test
    SOURCES
        messaging_test.cc
    DEPENDS
        firebase_app_for_testing
        firebase_messaging
        firebase_messaging_test_util
        firebase_testing
    )

    firebase_cpp_cc_test_on_ios(
        firebase_messaging_test
    HOST
        firebase_app_for_testing_ios
    SOURCES
        messaging_test.cc
    DEPENDS
        firebase_messaging
        firebase_messaging_test_util
        firebase_testing
    CUSTOM_FRAMEWORKS
        FirebaseMessaging
        Protobuf
    )

endif()
