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

#include "firestore/src/android/transaction_android.h"

#include <stdexcept>
#include <utility>

#include "Firestore/core/src/util/firestore_exceptions.h"
#include "app/meta/move.h"
#include "app/src/include/firebase/internal/common.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::Throwable;

constexpr char kTransactionClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Transaction";
Method<Object> kSet(
    "set",
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/lang/Object;"
    "Lcom/google/firebase/firestore/SetOptions;)"
    "Lcom/google/firebase/firestore/Transaction;");
Method<Object> kUpdate(
    "update",
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/util/Map;)"
    "Lcom/google/firebase/firestore/Transaction;");
Method<Object> kUpdateVarargs(
    "update",
    "(Lcom/google/firebase/firestore/DocumentReference;"
    "Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"
    "[Ljava/lang/Object;)Lcom/google/firebase/firestore/Transaction;");
Method<Object> kDelete("delete",
                       "(Lcom/google/firebase/firestore/DocumentReference;)"
                       "Lcom/google/firebase/firestore/Transaction;");
Method<Object> kGet("get",
                    "(Lcom/google/firebase/firestore/DocumentReference;)"
                    "Lcom/google/firebase/firestore/DocumentSnapshot;");

constexpr char kTransactionFunctionClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/TransactionFunction";

Constructor<Object> kNewTransactionFunction("(JJ)V");

}  // namespace

void TransactionInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kTransactionClassName, kSet, kUpdate, kUpdateVarargs,
                   kDelete, kGet);

  static const JNINativeMethod kTransactionFunctionNatives[] = {
      {"nativeApply",
       "(JJLcom/google/firebase/firestore/Transaction;)Ljava/lang/Exception;",
       reinterpret_cast<void*>(
           &TransactionInternal::TransactionFunctionNativeApply)}};
  loader.LoadClass(kTransactionFunctionClassName, kNewTransactionFunction);
  loader.RegisterNatives(kTransactionFunctionNatives,
                         FIREBASE_ARRAYSIZE(kTransactionFunctionNatives));
}

void TransactionInternal::Set(const DocumentReference& document,
                              const MapFieldValue& data,
                              const SetOptions& options) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  Local<Object> java_options = SetOptionsInternal::Create(env, options);
  env.Call(obj_, kSet, document.internal_->ToJava(), java_data, java_options);
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldValue& data) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  env.Call(obj_, kUpdate, document.internal_->ToJava(), java_data);
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldPathValue& data) {
  if (data.empty()) {
    Update(document, MapFieldValue{});
    return;
  }

  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);
  env.Call(obj_, kUpdateVarargs, document.internal_->ToJava(), args.first_field,
           args.first_value, args.varargs);
}

void TransactionInternal::Delete(const DocumentReference& document) {
  Env env = GetEnv();
  env.Call(obj_, kDelete, document.internal_->ToJava());
}

DocumentSnapshot TransactionInternal::Get(const DocumentReference& document,
                                          Error* error_code,
                                          std::string* error_message) {
  Env env = GetEnv();

  Local<Object> snapshot = env.Call(obj_, kGet, document.internal_->ToJava());
  Local<Throwable> exception = env.ClearExceptionOccurred();

  if (exception) {
    if (error_code != nullptr) {
      *error_code = ExceptionInternal::GetErrorCode(env, exception);
    }
    if (error_message != nullptr) {
      *error_message = ExceptionInternal::ToString(env, exception);
    }

    if (!ExceptionInternal::IsFirestoreException(env, exception)) {
      // Only preserve the exception if it is not `FirebaseFirestoreException`.
      // For Firestore exceptions, the user decides whether to raise the error
      // or let the transaction succeed through the error code/message the
      // `TransactionFunction` returns.
      PreserveException(env, Move(exception));
    }
    return DocumentSnapshot{};

  } else {
    if (error_code != nullptr) {
      *error_code = Error::kErrorOk;
    }
    if (error_message != nullptr) {
      *error_message = "";
    }
  }

  return firestore_->NewDocumentSnapshot(env, snapshot);
}

Env TransactionInternal::GetEnv() {
#if FIRESTORE_HAVE_EXCEPTIONS
  // If exceptions are enabled, report Java exceptions by translating them to
  // their C++ equivalents in the usual way. These will throw out to the
  // user-supplied TransactionFunction and ultimately out to
  // `TransactionFunctionNativeApply`, below.
  return FirestoreInternal::GetEnv();

#else
  Env env;
  env.SetUnhandledExceptionHandler(ExceptionHandler, this);
  return env;
#endif
}

void TransactionInternal::ExceptionHandler(Env& env,
                                           Local<Throwable>&& exception,
                                           void* context) {
  auto* transaction = static_cast<TransactionInternal*>(context);
  env.ExceptionClear();
  transaction->PreserveException(env, Move(exception));
}

void TransactionInternal::PreserveException(jni::Env& env,
                                            Local<Throwable>&& exception) {
  // Only preserve the first real exception.
  if (*first_exception_ || !exception) {
    return;
  }

  if (ExceptionInternal::IsAnyExceptionThrownByFirestore(env, exception)) {
    exception = ExceptionInternal::Wrap(env, Move(exception));
  }
  *first_exception_ = std::move(exception);
}

Local<Throwable> TransactionInternal::ClearExceptionOccurred() {
  if (!*first_exception_) return {};
  return std::move(*first_exception_);
}

Local<Object> TransactionInternal::Create(Env& env,
                                          FirestoreInternal* firestore,
                                          TransactionFunction* function) {
  return env.New(kNewTransactionFunction, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(function));
}

jobject TransactionInternal::TransactionFunctionNativeApply(
    JNIEnv* raw_env,
    jclass clazz,
    jlong firestore_ptr,
    jlong transaction_function_ptr,
    jobject java_transaction) {
  if (firestore_ptr == 0 || transaction_function_ptr == 0) {
    return nullptr;
  }

  FirestoreInternal* firestore =
      reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  TransactionFunction* transaction_function =
      reinterpret_cast<TransactionFunction*>(transaction_function_ptr);

  Transaction transaction(
      new TransactionInternal(firestore, Object(java_transaction)));

  std::string message;
  Error code;

#if FIRESTORE_HAVE_EXCEPTIONS
  try {
#endif

    code = transaction_function->Apply(transaction, message);

#if FIRESTORE_HAVE_EXCEPTIONS
  } catch (const FirestoreException& e) {
    message = e.what();
    code = e.code();
  } catch (const std::invalid_argument& e) {
    message = e.what();
    code = Error::kErrorInvalidArgument;
  } catch (const std::logic_error& e) {
    message = e.what();
    code = Error::kErrorFailedPrecondition;
  } catch (const std::exception& e) {
    message = std::string("Unknown exception: ") + e.what();
    code = Error::kErrorUnknown;
  } catch (...) {
    message = "Unknown exception";
    code = Error::kErrorUnknown;
  }
#endif  // FIRESTORE_HAVE_EXCEPTIONS

  // Verify that `internal_` is not null before using it. It could be set to
  // `nullptr` if the `FirestoreInternal` is destroyed during the invocation of
  // transaction_function->Apply() (b/171804663).
  if (transaction.internal_) {
    Local<Throwable> first_exception =
        transaction.internal_->ClearExceptionOccurred();
    if (first_exception) {
      return first_exception.release();
    }
  }

  Env env(raw_env);
  return ExceptionInternal::Create(env, code, message).release();
}

}  // namespace firestore
}  // namespace firebase
