/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/invites/android/invites_android_helper.h"

#include <assert.h>
#include <string.h>

#include "app/invites_resources.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/invites/receiver_interface.h"
#include "app/src/invites/sender_receiver_interface.h"
#include "app/src/log.h"
#include "app/src/util_android.h"

namespace firebase {
namespace invites {
namespace internal {

Mutex AndroidHelper::init_mutex_;  // NOLINT
int AndroidHelper::initialize_count_ = 0;

METHOD_LOOKUP_DEFINITION(
    dynamic_links_native_wrapper,
    "com/google/firebase/dynamiclinks/internal/cpp/DynamicLinksNativeWrapper",
    DYNAMIC_LINKS_NATIVE_WRAPPER_METHODS)

extern "C" {

JNIEXPORT void JNICALL
Java_com_google_firebase_dynamiclinks_internal_cpp_DynamicLinksNativeWrapper_receivedDynamicLinkCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring deep_link_url_java,
    jint result, jstring error_string_java);

}  // extern "C"

AndroidHelper::AndroidHelper(const ::firebase::App& app,
                             SenderReceiverInterface* sender_receiver)
    : app_(&app), wrapper_obj_(nullptr) {
  {
    MutexLock init_lock(init_mutex_);
    if (initialize_count_ == 0) {
      JNIEnv* env = app_->GetJNIEnv();
      if (!util::Initialize(env, app.activity())) {
        app_ = nullptr;
        return;
      }

      static const JNINativeMethod kNativeMethods[] = {
          {"receivedDynamicLinkCallback",
           "(JLjava/lang/String;ILjava/lang/String;)V",
           reinterpret_cast<void*>(
               &Java_com_google_firebase_dynamiclinks_internal_cpp_DynamicLinksNativeWrapper_receivedDynamicLinkCallback)}  // NOLINT
      };
      const std::vector<firebase::internal::EmbeddedFile> embedded_files =
          util::CacheEmbeddedFiles(
              env, app_->activity(),
              firebase::internal::EmbeddedFile::ToVector(
                  firebase_invites::invites_resources_filename,
                  firebase_invites::invites_resources_data,
                  firebase_invites::invites_resources_size));
      if (!(dynamic_links_native_wrapper::CacheClassFromFiles(
                env, app_->activity(), &embedded_files) &&
            // Prepare to instantiate the DynamicLinksNativeWrapper Java class,
            // which we will use to talk to the Firebase Dynamic Links Java
            // library. Note that we need global references to everything
            // because, again, we may be running from different threads.
            dynamic_links_native_wrapper::CacheMethodIds(env,
                                                         app_->activity()) &&
            dynamic_links_native_wrapper::RegisterNatives(
                env, kNativeMethods, FIREBASE_ARRAYSIZE(kNativeMethods)))) {
        util::Terminate(env);
        app_ = nullptr;
        return;
      }
    }
    initialize_count_++;
  }
  // Actually create the DynamicLinksNativeWrapper object now.
  CreateWrapperObject(sender_receiver);
}

AndroidHelper::~AndroidHelper() {
  // If initialization failed there is nothing to clean up.
  if (app_ == nullptr) return;

  // Ensure that no further JNI callbacks refer to deleted instances.
  CallMethod(dynamic_links_native_wrapper::kDiscardNativePointer);

  JNIEnv* env = app_->GetJNIEnv();
  env->DeleteGlobalRef(wrapper_obj_);
  wrapper_obj_ = nullptr;
  {
    MutexLock init_lock(init_mutex_);
    assert(initialize_count_ > 0);
    initialize_count_--;
    if (initialize_count_ == 0) {
      util::Terminate(env);
      dynamic_links_native_wrapper::ReleaseClass(env);
    }
  }
  app_ = nullptr;
}

void AndroidHelper::CreateWrapperObject(
    SenderReceiverInterface* sender_receiver) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject obj = env->NewObject(dynamic_links_native_wrapper::g_class,
                               dynamic_links_native_wrapper::GetMethodId(
                                   dynamic_links_native_wrapper::kConstructor),
                               reinterpret_cast<jlong>(sender_receiver),
                               app_->activity(), nullptr);
  CheckJNIException();
  wrapper_obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
}

bool AndroidHelper::CallBooleanMethod(
    dynamic_links_native_wrapper::Method method) {
  JNIEnv* env = app_->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      wrapper_obj(), dynamic_links_native_wrapper::GetMethodId(method));
  CheckJNIException();
  return (result != JNI_FALSE);
}

bool AndroidHelper::CallBooleanMethodString(
    dynamic_links_native_wrapper::Method method, const char* strparam) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param = env->NewStringUTF(strparam);
  jboolean result = env->CallBooleanMethod(
      wrapper_obj(), dynamic_links_native_wrapper::GetMethodId(method), param);
  CheckJNIException();
  env->DeleteLocalRef(param);

  return (result != JNI_FALSE);
}

int AndroidHelper::CallIntMethodString(
    dynamic_links_native_wrapper::Method method, const char* strparam) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param = env->NewStringUTF(strparam);
  jint result = env->CallBooleanMethod(
      wrapper_obj(), dynamic_links_native_wrapper::GetMethodId(method), param);
  CheckJNIException();
  env->DeleteLocalRef(param);

  return result;
}

void AndroidHelper::CallMethod(dynamic_links_native_wrapper::Method method) {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(wrapper_obj(),
                      dynamic_links_native_wrapper::GetMethodId(method));
  CheckJNIException();
}

void AndroidHelper::CallMethodStringString(
    dynamic_links_native_wrapper::Method method, const char* strparam1,
    const char* strparam2) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param1 = env->NewStringUTF(strparam1);
  jstring param2 = env->NewStringUTF(strparam2);
  env->CallVoidMethod(wrapper_obj(),
                      dynamic_links_native_wrapper::GetMethodId(method), param1,
                      param2);
  CheckJNIException();

  env->DeleteLocalRef(param2);
  env->DeleteLocalRef(param1);
}

void AndroidHelper::CheckJNIException() {
  JNIEnv* env = app_->GetJNIEnv();
  if (env->ExceptionCheck()) {
    // Get the exception text.
    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();

    // Convert the exception to a string.
    jclass object_class = env->FindClass("java/lang/Object");
    jmethodID toString =
        env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    jstring s = (jstring)env->CallObjectMethod(exception, toString);
    const char* exception_text = env->GetStringUTFChars(s, nullptr);

    // Log the exception text.
    LogError("JNI exception: %s", exception_text);

    // Also, assert fail.
    assert(false);

    // In the event we didn't assert fail, clean up.
    env->ReleaseStringUTFChars(s, exception_text);
    env->DeleteLocalRef(s);
    env->DeleteLocalRef(exception);
  }
}

void AndroidHelper::ConnectionFailedCallback(int error_code) {
  // TODO(jsimantov): Set a flag so the calling class can see an error occurred.
  (void)error_code;
}

extern "C" {

// A function that receives the callback from the Java side. The
// "data_ptr" parameter is actually a pointer to our instance of
// InviteReceiverInternalAndroid, so we can call the proper
// ReceivedInviteCallback method.
JNIEXPORT void JNICALL
Java_com_google_firebase_dynamiclinks_internal_cpp_DynamicLinksNativeWrapper_receivedDynamicLinkCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring deep_link_url_java,
    jint result, jstring error_string_java) {
  if (data_ptr == 0) return;  // test call only

  // Be careful - we are in a different thread now. No JNI calls are allowed
  // except on the JNIEnv we were passed, and we have to take care changing the
  // InvitesReceiverInternal data -- anything we touch needs a lock.
  std::string invitation_id;  // Will remain empty.
  std::string deep_link_url;
  std::string error_string;
  if (result == 0) {
    if (deep_link_url_java != nullptr) {
      const char* chars = env->GetStringUTFChars(deep_link_url_java, nullptr);
      deep_link_url = chars;
      env->ReleaseStringUTFChars(deep_link_url_java, chars);
    }
  } else {
    // result != 0
    if (error_string_java != nullptr) {
      const char* chars = env->GetStringUTFChars(error_string_java, nullptr);
      error_string = chars;
      env->ReleaseStringUTFChars(error_string_java, chars);
    }
  }
  firebase::invites::internal::SenderReceiverInterface* sender_receiver =
      reinterpret_cast<firebase::invites::internal::SenderReceiverInterface*>(
          data_ptr);

  // On Android, we are always considered a perfect match.
  sender_receiver->ReceivedInviteCallback(invitation_id, deep_link_url,
                                          kLinkMatchStrengthPerfectMatch,
                                          result, error_string);
}

}  // extern "C"

}  // namespace internal
}  // namespace invites
}  // namespace firebase
