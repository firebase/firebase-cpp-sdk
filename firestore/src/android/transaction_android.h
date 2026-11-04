// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_

#include <memory>
#include <string>
#include <utility>

#include "firestore/src/android/wrapper.h"
#include "firestore/src/common/transaction_function.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/transaction.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/throwable.h"

namespace firebase::firestore {

class TransactionInternal : public Wrapper {
 public:
  static void Initialize(jni::Loader& loader);

  TransactionInternal(FirestoreInternal* firestore, const jni::Object& obj)
      : Wrapper(firestore, obj),
        first_exception_(std::make_shared<jni::Global<jni::Throwable>>()) {}

  TransactionInternal(const TransactionInternal& rhs) = default;
  TransactionInternal(TransactionInternal&& rhs) noexcept = default;

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options);
  void Update(const DocumentReference& document, const MapFieldValue& data);
  void Update(const DocumentReference& document, const MapFieldPathValue& data);
  void Delete(const DocumentReference& document);
  DocumentSnapshot Get(const DocumentReference& document, Error* error_code,
                       std::string* error_message);

  static jni::Local<jni::Object> Create(jni::Env& env,
                                        FirestoreInternal* firestore,
                                        TransactionFunction* function);
  static jobject TransactionFunctionNativeApply(JNIEnv* env, jclass clazz,
                                                jlong firestore_ptr,
                                                jlong transaction_function_ptr,
                                                jobject transaction);

 private:
  jni::Env GetEnv();
  bool ValidateReference(const DocumentReference& document,
                         Error* error_code = nullptr,
                         std::string* error_message = nullptr);
  static void ExceptionHandler(jni::Env& env,
                               jni::Local<jni::Throwable>&& exception,
                               void* context);
  void PreserveException(jni::Env& env, jni::Local<jni::Throwable>&& exception);
  jni::Local<jni::Throwable> ClearExceptionOccurred();

  std::shared_ptr<jni::Global<jni::Throwable>> first_exception_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_TRANSACTION_ANDROID_H_
