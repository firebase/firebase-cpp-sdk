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
    invite, "com/google/firebase/invites/internal/cpp/AppInviteNativeWrapper",
    INVITE_METHODS)

extern "C" {

JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_receivedInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring invitation_id_java,
    jstring deep_link_url_java, jint result, jstring error_string_java);
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_convertedInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring invitation_id_java,
    jint result, jstring error_string_java);
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_sentInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr,
    jobjectArray invitation_ids_array, jint result, jstring error_string_java);
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_connectionFailedCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint error_code,
    jstring error_message);

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
          {"receivedInviteCallback",
           "(JLjava/lang/String;Ljava/lang/String;ILjava/lang/String;)V",
           reinterpret_cast<void*>(
               &Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_receivedInviteCallback)},  // NOLINT
          {"convertedInviteCallback",
           "(JLjava/lang/String;ILjava/lang/String;)V",
           reinterpret_cast<void*>(
               &Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_convertedInviteCallback)},  // NOLINT
          {"sentInviteCallback", "(J[Ljava/lang/String;ILjava/lang/String;)V",
           reinterpret_cast<void*>(
               &Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_sentInviteCallback)},  // NOLINT
          {"connectionFailedCallback", "(JILjava/lang/String;)V",
           reinterpret_cast<void*>(
               &Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_connectionFailedCallback)},  // NOLINT
      };
      const std::vector<util::EmbeddedFile> embedded_files =
          util::CacheEmbeddedFiles(
              env, app_->activity(),
              util::ArrayToEmbeddedFiles(
                  firebase_invites::invites_resources_filename,
                  firebase_invites::invites_resources_data,
                  firebase_invites::invites_resources_size));
      if (!(invite::CacheClassFromFiles(env, app_->activity(),
                                        &embedded_files) &&
            // Prepare to instantiate the AppInviteNativeWrapper Java class,
            // which
            // we will use to talk to the Firebase Invites Java library. Note
            // that
            // we need global references to everything because, again, we may be
            // running from different threads.
            invite::CacheMethodIds(env, app_->activity()) &&
            invite::RegisterNatives(
                env, kNativeMethods,
                sizeof(kNativeMethods) / sizeof(kNativeMethods[0])))) {
        util::Terminate(env);
        app_ = nullptr;
        return;
      }
    }
    initialize_count_++;
  }
  // Actually create the AppInviteNativeWrapper object now.
  CreateWrapperObject(sender_receiver);
}

AndroidHelper::~AndroidHelper() {
  // If initialization failed there is nothing to clean up.
  if (app_ == nullptr) return;

  // Ensure that no further JNI callbacks refer to deleted instances.
  CallMethod(invite::kDiscardNativePointer);

  JNIEnv* env = app_->GetJNIEnv();
  env->DeleteGlobalRef(wrapper_obj_);
  wrapper_obj_ = nullptr;
  {
    MutexLock init_lock(init_mutex_);
    assert(initialize_count_ > 0);
    initialize_count_--;
    if (initialize_count_ == 0) {
      util::Terminate(env);
      invite::ReleaseClass(env);
    }
  }
  app_ = nullptr;
}

void AndroidHelper::CreateWrapperObject(
    SenderReceiverInterface* sender_receiver) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject obj = env->NewObject(
      invite::g_class, invite::GetMethodId(invite::kConstructor),
      reinterpret_cast<jlong>(sender_receiver), app_->activity(), nullptr);
  CheckJNIException();
  wrapper_obj_ = env->NewGlobalRef(obj);
  env->DeleteLocalRef(obj);
}

bool AndroidHelper::CallBooleanMethod(invite::Method method) {
  JNIEnv* env = app_->GetJNIEnv();
  jboolean result =
      env->CallBooleanMethod(wrapper_obj(), invite::GetMethodId(method));
  CheckJNIException();
  return (result != JNI_FALSE);
}

bool AndroidHelper::CallBooleanMethodString(invite::Method method,
                                            const char* strparam) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param = env->NewStringUTF(strparam);
  jboolean result =
      env->CallBooleanMethod(wrapper_obj(), invite::GetMethodId(method), param);
  CheckJNIException();
  env->DeleteLocalRef(param);

  return (result != JNI_FALSE);
}

int AndroidHelper::CallIntMethodString(invite::Method method,
                                       const char* strparam) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param = env->NewStringUTF(strparam);
  jint result =
      env->CallBooleanMethod(wrapper_obj(), invite::GetMethodId(method), param);
  CheckJNIException();
  env->DeleteLocalRef(param);

  return result;
}

void AndroidHelper::CallMethod(invite::Method method) {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(wrapper_obj(), invite::GetMethodId(method));
  CheckJNIException();
}

void AndroidHelper::CallMethodStringString(invite::Method method,
                                           const char* strparam1,
                                           const char* strparam2) {
  JNIEnv* env = app_->GetJNIEnv();
  jstring param1 = env->NewStringUTF(strparam1);
  jstring param2 = env->NewStringUTF(strparam2);
  env->CallVoidMethod(wrapper_obj(), invite::GetMethodId(method), param1,
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
// "data_ptr" parameter is actually a pointer to our the AndroidHelper
// instance, so we can call the proper ConnectionFailed method.
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_connectionFailedCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jint error_code,
    jstring error_message) {
  if (data_ptr == 0) return;  // test call only

  // Be careful - we are in a different thread now. No JNI calls are allowed
  // except on the JNIEnv we were passed, and we have to take care changing the
  // InvitesReceiverInternal data -- anything we touch needs a lock.
  firebase::invites::internal::AndroidHelper* instance =
      reinterpret_cast<firebase::invites::internal::AndroidHelper*>(data_ptr);

  instance->ConnectionFailedCallback(error_code);
}

// A function that receives the callback from the Java side. The
// "data_ptr" parameter is actually a pointer to our instance of
// InviteSenderInternalAndroid, so we can call the proper
// SentInviteCallback method.

JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_sentInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr,
    jobjectArray invitation_ids_array, jint result, jstring error_string_java) {
  if (data_ptr == 0) return;  // Test call to ensure JNI is hooked up.

  // Be careful - we are in a different thread now. No JNI calls are allowed
  // except on the JNIEnv we were passed, and we have to take care changing the
  // AppInvitesSenderInternal data -- anything we touch needs a lock.
  std::vector<std::string> invitation_ids;
  std::string error_string;
  if (result == 0) {
    if (invitation_ids_array != nullptr) {
      size_t num_ids =
          static_cast<size_t>(env->GetArrayLength(invitation_ids_array));
      invitation_ids.reserve(num_ids);
      for (int i = 0; i < num_ids; i++) {
        jstring id = static_cast<jstring>(
            env->GetObjectArrayElement(invitation_ids_array, i));
        const char* chars = env->GetStringUTFChars(id, nullptr);
        invitation_ids.push_back(std::string(chars));
        env->ReleaseStringUTFChars(id, chars);
        env->DeleteLocalRef(id);
      }
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
  sender_receiver->SentInviteCallback(invitation_ids, result, error_string);
}

// A function that receives the callback from the Java side. The
// "data_ptr" parameter is actually a pointer to our instance of
// InviteReceiverInternalAndroid, so we can call the proper
// ReceivedInviteCallback method.
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_receivedInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring invitation_id_java,
    jstring deep_link_url_java, jint result, jstring error_string_java) {
  if (data_ptr == 0) return;  // test call only

  // Be careful - we are in a different thread now. No JNI calls are allowed
  // except on the JNIEnv we were passed, and we have to take care changing the
  // InvitesReceiverInternal data -- anything we touch needs a lock.
  std::string invitation_id;
  std::string deep_link_url;
  std::string error_string;
  if (result == 0) {
    if (invitation_id_java != nullptr) {
      const char* chars = env->GetStringUTFChars(invitation_id_java, nullptr);
      invitation_id = chars;
      env->ReleaseStringUTFChars(invitation_id_java, chars);
    }
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

// A function that receives the callback from the Java side. The
// "data_ptr" parameter is actually a pointer to our instance of
// InviteReceiverInternalAndroid, so we can call the proper
// ConvertedInviteCallback method.
JNIEXPORT void JNICALL
Java_com_google_firebase_invites_internal_cpp_AppInviteNativeWrapper_convertedInviteCallback(
    JNIEnv* env, jclass clazz, jlong data_ptr, jstring invitation_id_java,
    jint result, jstring error_string_java) {
  if (data_ptr == 0) return;  // test call only

  // Be careful - we are in a different thread now. No JNI calls are allowed
  // except on the JNIEnv we were passed, and we have to take care changing the
  // InvitesReceiverInternal data -- anything we touch needs a lock.
  std::string invitation_id;
  std::string error_string;
  if (result == 0) {
    if (invitation_id_java != nullptr) {
      const char* chars = env->GetStringUTFChars(invitation_id_java, nullptr);
      invitation_id = chars;
      env->ReleaseStringUTFChars(invitation_id_java, chars);
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

  sender_receiver->ConvertedInviteCallback(invitation_id, result, error_string);
}

}  // extern "C"

}  // namespace internal
}  // namespace invites
}  // namespace firebase
