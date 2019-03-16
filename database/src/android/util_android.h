// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_

#include <jni.h>
#include "app/src/include/firebase/variant.h"

namespace firebase {
namespace database {
namespace internal {

// Converts a Variant to a java.lang.Object.
jobject VariantToJavaObject(JNIEnv* env, const Variant& variant);

// Converts a java.lang.Object into a Variant.
Variant JavaObjectToVariant(JNIEnv* env, jobject obj);

class Callbacks {
 public:
  static jobject TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                 jlong db_ptr,
                                                 jlong transaction_ptr,
                                                 jobject mutable_data);
  static void TransactionHandlerOnComplete(JNIEnv* env, jclass clazz,
                                           jlong db_ptr, jlong listener_ptr,
                                           jobject database_error,
                                           jboolean was_committed,
                                           jobject data_snapshot);
  static void ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                             jlong db_ptr, jlong listener_ptr,
                                             jobject database_error);

  static void ChildListenerNativeOnChildAdded(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot,
                                              jobject previous_child);

  static void ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child);

  static void ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot,
                                              jobject previous_child);

  static void ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot);

  static void ValueListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                             jlong db_ptr, jlong listener_ptr,
                                             jobject database_error);

  static void ValueListenerNativeOnDataChange(JNIEnv* env, jclass clazz,
                                              jlong db_ptr, jlong listener_ptr,
                                              jobject data_snapshot,
                                              jobject previous_child_name);
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_UTIL_ANDROID_H_
