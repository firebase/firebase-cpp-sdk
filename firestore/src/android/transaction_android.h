/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_

#include <memory>
#include <string>

#include "app/meta/move.h"
#include "app/src/embedded_file.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/common/transaction_function.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/transaction.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/throwable.h"

namespace firebase {
namespace firestore {

class TransactionInternal : public Wrapper {
 public:
  static void Initialize(jni::Loader& loader);

  TransactionInternal(FirestoreInternal* firestore, const jni::Object& obj)
      : Wrapper(firestore, obj),
        first_exception_(std::make_shared<jni::Local<jni::Throwable>>()) {}

  TransactionInternal(const TransactionInternal& rhs)
      : Wrapper(rhs), first_exception_(rhs.first_exception_) {}

  TransactionInternal(TransactionInternal&& rhs)
      : Wrapper(firebase::Move(rhs)),
        first_exception_(std::move(rhs.first_exception_)) {}

  void Set(const DocumentReference& document,
           const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);

  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  DocumentSnapshot Get(const DocumentReference& document,
                       Error* error_code,
                       std::string* error_message);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        FirestoreInternal* firestore,
                                        TransactionFunction* function);

  static jobject TransactionFunctionNativeApply(JNIEnv* env,
                                                jclass clazz,
                                                jlong firestore_ptr,
                                                jlong transaction_function_ptr,
                                                jobject transaction);

 private:
  jni::Env GetEnv();

  static void ExceptionHandler(jni::Env& env,
                               jni::Local<jni::Throwable>&& exception,
                               void* context);

  // If this is the first exception, then store it. Otherwise, preserve the
  // current exception. Passing nullptr has no effect.
  void PreserveException(jni::Env& env, jni::Local<jni::Throwable>&& exception);

  // Returns and clears the global reference of the first exception, if any.
  jni::Local<jni::Throwable> ClearExceptionOccurred();

  // The first exception that occurred. Because exceptions must be cleared
  // before calling other JNI methods, we cannot rely on the Java exception
  // mechanism to properly handle native calls via JNI. The first exception is
  // shared by a transaction and its copies. User is allowed to make copy and
  // call transaction operation on the copy.
  std::shared_ptr<jni::Local<jni::Throwable>> first_exception_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_
