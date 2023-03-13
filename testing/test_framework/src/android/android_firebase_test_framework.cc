// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "firebase_test_framework.h"  // NOLINT

namespace firebase_test_framework {

using app_framework::LogDebug;
using app_framework::LogError;

// Blocking HTTP request helper function.
static bool SendHttpRequest(const char* url,
                            const std::map<std::string, std::string>& headers,
                            const std::string* post_body, int* response_code,
                            std::string* response_str) {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass simple_http_request_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/SimpleHttpRequest");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID constructor = env->GetMethodID(simple_http_request_class, "<init>",
                                           "(Ljava/lang/String;)V");
  jmethodID set_post_data =
      env->GetMethodID(simple_http_request_class, "setPostData", "([B)V");
  jmethodID add_header =
      env->GetMethodID(simple_http_request_class, "addHeader",
                       "(Ljava/lang/String;Ljava/lang/String;)V");
  jmethodID perform = env->GetMethodID(simple_http_request_class, "perform",
                                       "()Ljava/lang/String;");
  jmethodID get_response_code =
      env->GetMethodID(simple_http_request_class, "getResponseCode", "()I");

  jstring url_jstring = env->NewStringUTF(url);
  // http_request = new SimpleHttpRequestClass(url);
  jobject http_request =
      env->NewObject(simple_http_request_class, constructor, url_jstring);
  env->DeleteLocalRef(url_jstring);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    if (http_request) env->DeleteLocalRef(http_request);
    return false;
  }
  // for (header : headers) {
  //   http_request.addHeader(header.key, header.value);
  // }
  for (auto i = headers.begin(); i != headers.end(); ++i) {
    jstring key_jstring = env->NewStringUTF(i->first.c_str());
    jstring value_jstring = env->NewStringUTF(i->second.c_str());
    env->CallVoidMethod(http_request, add_header, key_jstring, value_jstring);
    env->DeleteLocalRef(key_jstring);
    env->DeleteLocalRef(value_jstring);
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      if (http_request) env->DeleteLocalRef(http_request);
      return false;
    }
  }
  if (post_body != nullptr) {
    // http_request.setPostBody(post_body);
    jbyteArray post_body_array = env->NewByteArray(post_body->length());
    env->SetByteArrayRegion(post_body_array, 0, post_body->length(),
                            reinterpret_cast<const jbyte*>(post_body->c_str()));
    env->CallVoidMethod(http_request, set_post_data, post_body_array);
    env->DeleteLocalRef(post_body_array);
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      if (http_request) env->DeleteLocalRef(http_request);
      return false;
    }
  }
  // String response = http_request.perform();
  jobject response = env->CallObjectMethod(http_request, perform);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
  jstring response_jstring = static_cast<jstring>(response);
  // int response_code = http_request.getResponseCode();
  jint response_code_jint = env->CallIntMethod(http_request, get_response_code);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
  LogDebug("HTTP status code %d", response_code_jint);
  if (response_code) *response_code = response_code_jint;

  env->DeleteLocalRef(http_request);
  if (response_jstring == nullptr) {
    return false;
  }
  const char* response_text = env->GetStringUTFChars(response_jstring, nullptr);
  LogDebug("Got response: %s", response_text);
  if (response_str) *response_str = response_text;
  env->ReleaseStringUTFChars(response_jstring, response_text);
  env->DeleteLocalRef(response);
  return true;
}

// Blocking HTTP request helper function, for testing only.
bool FirebaseTest::SendHttpGetRequest(
    const char* url, const std::map<std::string, std::string>& headers,
    int* response_code, std::string* response_str) {
  return SendHttpRequest(url, headers, nullptr, response_code, response_str);
}

bool FirebaseTest::SendHttpPostRequest(
    const char* url, const std::map<std::string, std::string>& headers,
    const std::string& post_body, int* response_code,
    std::string* response_str) {
  return SendHttpRequest(url, headers, &post_body, response_code, response_str);
}

bool FirebaseTest::OpenUrlInBrowser(const char* url) {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass simple_http_request_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/SimpleHttpRequest");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID open_url =
      env->GetStaticMethodID(simple_http_request_class, "openUrlInBrowser",
                             "(Ljava/lang/String;Landroid/app/Activity;)V");
  jstring url_jstring = env->NewStringUTF(url);
  env->CallStaticVoidMethod(simple_http_request_class, open_url, url_jstring,
                            activity);
  env->DeleteLocalRef(url_jstring);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}

bool FirebaseTest::SetPersistentString(const char* key, const char* value) {
  if (key == nullptr) {
    LogError("SetPersistentString: null key is not allowed.");
    return false;
  }
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass simple_persistent_storage_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/SimplePersistentStorage");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID set_string = env->GetStaticMethodID(
      simple_persistent_storage_class, "setString",
      "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;)V");
  jstring key_jstring = env->NewStringUTF(key);
  jstring value_jstring = value ? env->NewStringUTF(value) : nullptr;
  env->CallStaticVoidMethod(simple_persistent_storage_class, set_string,
                            activity, key_jstring, value_jstring);
  env->DeleteLocalRef(key_jstring);
  if (value_jstring) {
    env->DeleteLocalRef(value_jstring);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}

bool FirebaseTest::GetPersistentString(const char* key,
                                       std::string* value_out) {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass simple_persistent_storage_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/SimplePersistentStorage");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID get_string = env->GetStaticMethodID(
      simple_persistent_storage_class, "getString",
      "(Landroid/app/Activity;Ljava/lang/String;)Ljava/lang/String;");
  jstring key_jstring = env->NewStringUTF(key);
  jstring value_jstring = static_cast<jstring>(env->CallStaticObjectMethod(
      simple_persistent_storage_class, get_string, activity, key_jstring));
  env->DeleteLocalRef(key_jstring);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    if (value_jstring) {
      env->DeleteLocalRef(value_jstring);
    }
    return false;
  }
  if (value_jstring == nullptr) {
    return false;
  }
  const char* value_text = env->GetStringUTFChars(value_jstring, nullptr);
  if (value_out) *value_out = std::string(value_text);
  env->ReleaseStringUTFChars(value_jstring, value_text);
  env->DeleteLocalRef(value_jstring);
  return true;
}

bool FirebaseTest::IsRunningOnEmulator() {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass test_helper_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/TestHelper");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID is_running_on_emulator =
      env->GetStaticMethodID(test_helper_class, "isRunningOnEmulator", "()Z");
  jboolean result =
      env->CallStaticBooleanMethod(test_helper_class, is_running_on_emulator);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return result ? true : false;
}

int FirebaseTest::GetGooglePlayServicesVersion() {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();
  jclass test_helper_class = app_framework::FindClass(
      env, activity, "com/google/firebase/example/TestHelper");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jmethodID get_google_play_services_version =
      env->GetStaticMethodID(test_helper_class, "getGooglePlayServicesVersion",
                             "(Landroid/content/Context;)I");
  jint result = env->CallStaticIntMethod(
      test_helper_class, get_google_play_services_version, activity);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return static_cast<int>(result);
}

}  // namespace firebase_test_framework
