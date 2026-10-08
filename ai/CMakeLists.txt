# Copyright 2025 Google LLC
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

# CMake file for the firebase_ai library

# Common C++ source files used across all platforms (Desktop, Android, iOS)
set(common_SRCS
    src/common/chat.cc
    src/common/firebase_ai.cc
    src/common/generative_model.cc
    src/common/http_client.cc
    src/common/litert_adapter.cc
    src/common/litert_c_bridge.cc
    src/common/model_content.cc
    src/common/serialization.cc
    src/common/template_generative_model.cc)

# Slim HTTP sender for Android (JNI -> java.net.HttpURLConnection)
set(android_SRCS
    src/android/http_sender_android.cc)

# Slim HTTP sender for iOS (Objective-C++ -> NSURLSession)
set(ios_SRCS
    src/ios/http_sender_ios.mm)

# Slim HTTP sender for Desktop / Linux / macOS / Windows (libcurl via firebase_rest_lib)
set(desktop_SRCS
    src/desktop/http_sender_desktop.cc)

if(ANDROID)
  set(ai_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(ai_platform_SRCS
      "${ios_SRCS}")
else()
  set(ai_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_link_LIB)
else()
  set(additional_link_LIB
      firebase_rest_lib
      libcurl)
endif()

add_library(firebase_ai STATIC
    ${common_SRCS}
    ${ai_platform_SRCS})

set_property(TARGET firebase_ai PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_ai
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)

# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_ai
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FLATBUFFERS_SOURCE_DIR}/include
    ${FIREBASE_GEN_FILE_DIR}
    ${FIREBASE_SPM_SWIFT_HEADERS_DIR}
    ${CURL_SOURCE_DIR}/include
)

target_compile_definitions(firebase_ai
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
    -DFIREBASE_AI_BUILD_LIB_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

# Automatically download Google AI Edge LiteRT C/C++ SDK and LiteRT-LM runtime
# libraries on a fresh checkout (models are NOT downloaded by CMake).
option(FIREBASE_AI_DOWNLOAD_LITERT
       "Automatically download Google AI Edge LiteRT and LiteRT-LM SDKs/runtimes"
       ON)
set(FIREBASE_AI_LITERT_VERSION "2.2.0" CACHE STRING "LiteRT release version")
set(FIREBASE_AI_LITERT_LM_VERSION "0.18.0" CACHE STRING "LiteRT-LM release version")

set(FIREBASE_AI_LITERT_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/litert_deps")
if(FIREBASE_AI_DOWNLOAD_LITERT)
  file(MAKE_DIRECTORY "${FIREBASE_AI_LITERT_DEPS_DIR}")

  # 1. LiteRT C/C++ SDK headers (litert_cc_sdk.zip)
  set(LITERT_CC_SDK_ZIP "${FIREBASE_AI_LITERT_DEPS_DIR}/litert_cc_sdk.zip")
  set(LITERT_CC_SDK_EXTRACTED "${FIREBASE_AI_LITERT_DEPS_DIR}/litert_cc_sdk")
  if(NOT EXISTS "${LITERT_CC_SDK_EXTRACTED}/litert/c/litert_compiled_model.h")
    set(LITERT_CC_SDK_URL
        "https://github.com/google-ai-edge/LiteRT/releases/download/v${FIREBASE_AI_LITERT_VERSION}/litert_cc_sdk.zip")
    message(STATUS "Firebase AI: Downloading LiteRT C/C++ SDK from ${LITERT_CC_SDK_URL}")
    file(DOWNLOAD "${LITERT_CC_SDK_URL}" "${LITERT_CC_SDK_ZIP}"
         STATUS LITERT_SDK_DL_STATUS)
    list(GET LITERT_SDK_DL_STATUS 0 LITERT_SDK_DL_CODE)
    if(LITERT_SDK_DL_CODE EQUAL 0)
      file(ARCHIVE_EXTRACT INPUT "${LITERT_CC_SDK_ZIP}"
           DESTINATION "${FIREBASE_AI_LITERT_DEPS_DIR}")
    else()
      message(WARNING "Firebase AI: Failed to download LiteRT C/C++ SDK (${LITERT_SDK_DL_STATUS})")
    endif()
  endif()
  if(NOT DEFINED FIREBASE_AI_LITERT_CC_SDK_DIR AND
     EXISTS "${LITERT_CC_SDK_EXTRACTED}/litert/c/litert_compiled_model.h")
    set(FIREBASE_AI_LITERT_CC_SDK_DIR "${LITERT_CC_SDK_EXTRACTED}")
  endif()

  # 2. Platform LiteRT & LiteRT-LM shared runtime libraries
  if(APPLE AND NOT IOS)
    set(LITERT_DYLIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/libLiteRt.dylib")
    if(NOT EXISTS "${LITERT_DYLIB_PATH}")
      set(LITERT_DYLIB_URL
          "https://storage.googleapis.com/litert/binaries/${FIREBASE_AI_LITERT_VERSION}/macos_arm64/libLiteRt.dylib")
      message(STATUS "Firebase AI: Downloading LiteRT runtime from ${LITERT_DYLIB_URL}")
      file(DOWNLOAD "${LITERT_DYLIB_URL}" "${LITERT_DYLIB_PATH}"
           STATUS LITERT_DYLIB_DL_STATUS)
    endif()

    set(CLITERTLM_DYLIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/libCLiteRTLM_mac.dylib")
    set(CLITERTLM_EXTRACTED_DYLIB
        "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM_mac.xcframework/macos-arm64_x86_64/libCLiteRTLM_mac.dylib")
    if(NOT EXISTS "${CLITERTLM_EXTRACTED_DYLIB}")
      set(CLITERTLM_ZIP "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM_mac.xcframework.zip")
      set(CLITERTLM_URL
          "https://github.com/google-ai-edge/LiteRT-LM/releases/download/v${FIREBASE_AI_LITERT_LM_VERSION}/CLiteRTLM_mac.xcframework.zip")
      message(STATUS "Firebase AI: Downloading LiteRT-LM macOS runtime from ${CLITERTLM_URL}")
      file(DOWNLOAD "${CLITERTLM_URL}" "${CLITERTLM_ZIP}"
           STATUS CLITERTLM_DL_STATUS)
      list(GET CLITERTLM_DL_STATUS 0 CLITERTLM_DL_CODE)
      if(CLITERTLM_DL_CODE EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT "${CLITERTLM_ZIP}"
             DESTINATION "${FIREBASE_AI_LITERT_DEPS_DIR}")
      else()
        message(WARNING "Firebase AI: Failed to download LiteRT-LM runtime (${CLITERTLM_DL_STATUS})")
      endif()
    endif()
    if(EXISTS "${CLITERTLM_EXTRACTED_DYLIB}" AND NOT EXISTS "${CLITERTLM_DYLIB_PATH}")
      file(COPY_FILE "${CLITERTLM_EXTRACTED_DYLIB}" "${CLITERTLM_DYLIB_PATH}")
    endif()
    if(EXISTS "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM_mac.xcframework/macos-arm64_x86_64/Headers")
      target_include_directories(firebase_ai PRIVATE
          "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM_mac.xcframework/macos-arm64_x86_64/Headers")
    endif()
  elseif(IOS)
    set(CLITERTLM_IOS_DIR "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM.xcframework")
    if(NOT EXISTS "${CLITERTLM_IOS_DIR}")
      set(CLITERTLM_IOS_ZIP "${FIREBASE_AI_LITERT_DEPS_DIR}/CLiteRTLM.xcframework.zip")
      set(CLITERTLM_IOS_URL
          "https://github.com/google-ai-edge/LiteRT-LM/releases/download/v${FIREBASE_AI_LITERT_LM_VERSION}/CLiteRTLM.xcframework.zip")
      message(STATUS "Firebase AI: Downloading LiteRT-LM iOS xcframework from ${CLITERTLM_IOS_URL}")
      file(DOWNLOAD "${CLITERTLM_IOS_URL}" "${CLITERTLM_IOS_ZIP}"
           STATUS CLITERTLM_IOS_DL_STATUS)
      list(GET CLITERTLM_IOS_DL_STATUS 0 CLITERTLM_IOS_DL_CODE)
      if(CLITERTLM_IOS_DL_CODE EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT "${CLITERTLM_IOS_ZIP}"
             DESTINATION "${FIREBASE_AI_LITERT_DEPS_DIR}")
      endif()
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LITERT_SO_PATH "${CMAKE_CURRENT_BINARY_DIR}/libLiteRt.so")
    if(NOT EXISTS "${LITERT_SO_PATH}")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(LITERT_LINUX_ARCH "linux_arm64")
      else()
        set(LITERT_LINUX_ARCH "linux_x86_64")
      endif()
      set(LITERT_SO_URL
          "https://storage.googleapis.com/litert/binaries/${FIREBASE_AI_LITERT_VERSION}/${LITERT_LINUX_ARCH}/libLiteRt.so")
      message(STATUS "Firebase AI: Downloading LiteRT runtime from ${LITERT_SO_URL}")
      file(DOWNLOAD "${LITERT_SO_URL}" "${LITERT_SO_PATH}"
           STATUS LITERT_SO_DL_STATUS)
    endif()
  endif()
endif()

if(DEFINED FIREBASE_AI_LITERT_CC_SDK_DIR)
  if(EXISTS "${FIREBASE_AI_LITERT_CC_SDK_DIR}/litert/c/litert_compiled_model.h")
    target_include_directories(firebase_ai PRIVATE "${FIREBASE_AI_LITERT_CC_SDK_DIR}")
  elseif(EXISTS "${FIREBASE_AI_LITERT_CC_SDK_DIR}/include")
    target_include_directories(firebase_ai PRIVATE "${FIREBASE_AI_LITERT_CC_SDK_DIR}/include")
    target_compile_definitions(firebase_ai PRIVATE -DFIREBASE_AI_USE_LITERT_CC_SDK=1)
    if(EXISTS "${FIREBASE_AI_LITERT_CC_SDK_DIR}/lib/libLiteRt.dylib")
      target_link_libraries(firebase_ai PRIVATE "${FIREBASE_AI_LITERT_CC_SDK_DIR}/lib/libLiteRt.dylib")
    endif()
  endif()
endif()

if(ANDROID)
  firebase_cpp_proguard_file(ai)
elseif(IOS)
  target_compile_options(firebase_ai
                         PUBLIC $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-arc>)

  setup_spm_headers(
    firebase_ai
    MODULES
      FirebaseCore
  )

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_ai PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

if(NOT IOS)
  add_library(firebase_ai_litert_bridge SHARED
      src/common/litert_c_bridge.cc)
  target_link_libraries(firebase_ai_litert_bridge
     PRIVATE
      firebase_ai
      firebase_app)
  if(APPLE)
    target_link_libraries(firebase_ai_litert_bridge PRIVATE "-framework Foundation")
  endif()
  target_include_directories(firebase_ai_litert_bridge
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/src/include
      ${FIREBASE_CPP_SDK_ROOT_DIR}
      ${FLATBUFFERS_SOURCE_DIR}/include
  )
endif()

if(NOT ANDROID AND NOT IOS)
  add_executable(firebase_ai_hybrid_chat
      samples/hybrid_chat_main.cc)
  target_link_libraries(firebase_ai_hybrid_chat
      firebase_ai
      firebase_app)
  if(APPLE)
    target_link_libraries(firebase_ai_hybrid_chat "-framework Foundation")
  endif()
  target_include_directories(firebase_ai_hybrid_chat
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/src/include
      ${FIREBASE_CPP_SDK_ROOT_DIR}
  )
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_ai "")
cpp_pack_public_headers()
