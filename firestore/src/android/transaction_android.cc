// Copyright 2020 Google LLC

#include "firestore/src/android/transaction_android.h"

#include <stdexcept>
#include <utility>

#include "Firestore/core/src/util/firestore_exceptions.h"
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

namespace firebase::firestore {
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
  if (!ValidateReference(document)) return;
  Env env = GetEnv();
  env.Call(obj_, kSet, document.internal_->ToJava(), MakeJavaMap(env, data),
           SetOptionsInternal::Create(env, options));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldValue& data) {
  if (!ValidateReference(document)) return;
  Env env = GetEnv();
  env.Call(obj_, kUpdate, document.internal_->ToJava(), MakeJavaMap(env, data));
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldPathValue& data) {
  if (!ValidateReference(document)) return;
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
  if (!ValidateReference(document)) return;
  Env env = GetEnv();
  env.Call(obj_, kDelete, document.internal_->ToJava());
}

DocumentSnapshot TransactionInternal::Get(const DocumentReference& document,
                                          Error* error_code,
                                          std::string* error_message) {
  if (!ValidateReference(document, error_code, error_message)) return {};
  Env env = GetEnv();
  Local<Object> snapshot = env.Call(obj_, kGet, document.internal_->ToJava());
  if (Local<Throwable> exception = env.ClearExceptionOccurred()) {
    if (error_code) *error_code = ExceptionInternal::GetErrorCode(env, exception);
    if (error_message) *error_message = ExceptionInternal::ToString(env, exception);
    if (!ExceptionInternal::IsFirestoreException(env, exception)) {
      PreserveException(env, std::move(exception));
    }
    return {};
  }
  if (error_code) *error_code = Error::kErrorOk;
  if (error_message) *error_message = "";
  return firestore_->NewDocumentSnapshot(env, snapshot);
}

bool TransactionInternal::ValidateReference(const DocumentReference& document,
                                            Error* error_code,
                                            std::string* error_message) {
  const char* error = nullptr;
  if (!document.internal_) {
    error = "Invalid document reference provided.";
  } else if (document.internal_->firestore_internal() != firestore_) {
    error =
        "Provided document reference is from a different Cloud Firestore "
        "instance.";
  }
  if (!error) return true;
  if (error_code) *error_code = Error::kErrorInvalidArgument;
  if (error_message) *error_message = error;
  Env env = GetEnv();
  PreserveException(
      env, ExceptionInternal::Create(env, Error::kErrorInvalidArgument, error));
#if FIRESTORE_HAVE_EXCEPTIONS
  throw std::invalid_argument(error);
#else
  return false;
#endif
}

Env TransactionInternal::GetEnv() {
#if FIRESTORE_HAVE_EXCEPTIONS
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
  env.ExceptionClear();
  static_cast<TransactionInternal*>(context)->PreserveException(
      env, std::move(exception));
}

void TransactionInternal::PreserveException(jni::Env& env,
                                            Local<Throwable>&& exception) {
  if (!first_exception_ || *first_exception_ || !exception) return;
  if (ExceptionInternal::IsAnyExceptionThrownByFirestore(env, exception)) {
    exception = ExceptionInternal::Wrap(env, std::move(exception));
  }
  *first_exception_ = std::move(exception);
}

Local<Throwable> TransactionInternal::ClearExceptionOccurred() {
  return (first_exception_ && *first_exception_)
             ? Local<Throwable>(std::move(*first_exception_))
             : Local<Throwable>{};
}

Local<Object> TransactionInternal::Create(Env& env,
                                          FirestoreInternal* firestore,
                                          TransactionFunction* function) {
  return env.New(kNewTransactionFunction, reinterpret_cast<jlong>(firestore),
                 reinterpret_cast<jlong>(function));
}

jobject TransactionInternal::TransactionFunctionNativeApply(
    JNIEnv* raw_env, jclass, jlong firestore_ptr,
    jlong transaction_function_ptr, jobject java_transaction) {
  if (!firestore_ptr || !transaction_function_ptr || !java_transaction) {
    return nullptr;
  }
  auto* firestore = reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  auto* transaction_function =
      reinterpret_cast<TransactionFunction*>(transaction_function_ptr);

  std::string message;
  Error code = Error::kErrorOk;
  Transaction transaction(
      new TransactionInternal(firestore, Object(java_transaction)));

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
#endif

  if (transaction.internal_) {
    if (Local<Throwable> first =
            transaction.internal_->ClearExceptionOccurred()) {
      return first.release();
    }
  }
  Env env(raw_env);
  return code != Error::kErrorOk
             ? ExceptionInternal::Create(env, code, message).release()
             : nullptr;
}

}  // namespace firebase::firestore
