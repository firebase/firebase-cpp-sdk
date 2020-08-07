#include "firestore/src/android/transaction_android.h"

#include <jni.h>

#include <utility>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firebase_firestore_exception_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::HashMap;
using jni::Local;
using jni::Object;

}  // namespace

// clang-format off
#define TRANSACTION_METHODS(X)                                                 \
  X(Set, "set",                                                                \
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/lang/Object;"     \
    "Lcom/google/firebase/firestore/SetOptions;)"                              \
    "Lcom/google/firebase/firestore/Transaction;"),                            \
  X(Update, "update",                                                          \
    "(Lcom/google/firebase/firestore/DocumentReference;Ljava/util/Map;)"       \
    "Lcom/google/firebase/firestore/Transaction;"),                            \
  X(UpdateVarargs, "update",                                                   \
    "(Lcom/google/firebase/firestore/DocumentReference;"                       \
    "Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;"              \
    "[Ljava/lang/Object;)Lcom/google/firebase/firestore/Transaction;"),        \
  X(Delete, "delete", "(Lcom/google/firebase/firestore/DocumentReference;)"    \
    "Lcom/google/firebase/firestore/Transaction;"),                            \
  X(Get, "get", "(Lcom/google/firebase/firestore/DocumentReference;)"          \
    "Lcom/google/firebase/firestore/DocumentSnapshot;")
// clang-format on

METHOD_LOOKUP_DECLARATION(transaction, TRANSACTION_METHODS)
METHOD_LOOKUP_DEFINITION(transaction,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/Transaction",
                         TRANSACTION_METHODS)

#define TRANSACTION_FUNCTION_METHODS(X) X(Constructor, "<init>", "(JJ)V")
METHOD_LOOKUP_DECLARATION(transaction_function, TRANSACTION_FUNCTION_METHODS)
METHOD_LOOKUP_DEFINITION(
    transaction_function,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/TransactionFunction",
    TRANSACTION_FUNCTION_METHODS)

void TransactionInternal::Set(const DocumentReference& document,
                              const MapFieldValue& data,
                              const SetOptions& options) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  Local<Object> java_options = SetOptionsInternal::Create(env, options);
  env.Call<Object>(obj_, transaction::GetMethodId(transaction::kSet),
                   document.internal_->ToJava(), java_data, java_options);
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldValue& data) {
  Env env = GetEnv();
  Local<HashMap> java_data = MakeJavaMap(env, data);
  env.Call<Object>(obj_, transaction::GetMethodId(transaction::kUpdate),
                   document.internal_->ToJava(), java_data);
}

void TransactionInternal::Update(const DocumentReference& document,
                                 const MapFieldPathValue& data) {
  if (data.empty()) {
    Update(document, MapFieldValue{});
    return;
  }

  Env env = GetEnv();
  UpdateFieldPathArgs args = MakeUpdateFieldPathArgs(env, data);
  env.Call<Object>(obj_, transaction::GetMethodId(transaction::kUpdateVarargs),
                   document.internal_->ToJava(), args.first_field,
                   args.first_value, args.varargs);
}

void TransactionInternal::Delete(const DocumentReference& document) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  env->CallObjectMethod(obj_, transaction::GetMethodId(transaction::kDelete),
                        document.internal_->java_object());
  CheckAndClearJniExceptions();
}

DocumentSnapshot TransactionInternal::Get(const DocumentReference& document,
                                          Error* error_code,
                                          std::string* error_message) {
  JNIEnv* env = firestore_->app()->GetJNIEnv();

  jobject snapshot =
      env->CallObjectMethod(obj_, transaction::GetMethodId(transaction::kGet),
                            document.internal_->java_object());
  jthrowable exception = env->ExceptionOccurred();

  // Now deal with exceptions, if any. Do not preserve yet. Do not call
  // this->CheckAndClearJniExceptions().
  util::CheckAndClearJniExceptions(env);
  if (exception != nullptr) {
    if (error_code != nullptr) {
      *error_code =
          FirebaseFirestoreExceptionInternal::ToErrorCode(env, exception);
    }
    if (error_message != nullptr) {
      *error_message =
          FirebaseFirestoreExceptionInternal::ToString(env, exception);
    }

    if (!FirebaseFirestoreExceptionInternal::IsInstance(env, exception)) {
      // We would only preserve exception if it is not
      // FirebaseFirestoreException. The user should decide whether to raise the
      // error or let the transaction succeed.
      PreserveException(exception);
    }

    env->DeleteLocalRef(exception);
    return DocumentSnapshot{};
  } else {
    if (error_code != nullptr) {
      *error_code = Error::kErrorOk;
    }
    if (error_message != nullptr) {
      *error_message = "";
    }
  }
  DocumentSnapshot result{new DocumentSnapshotInternal{firestore_, snapshot}};
  env->DeleteLocalRef(snapshot);
  return result;
}

Env TransactionInternal::GetEnv() {
  Env env;
  env.SetUnhandledExceptionHandler(ExceptionHandler, this);
  return env;
}

void TransactionInternal::ExceptionHandler(
    jni::Env& env, jni::Local<jni::Throwable>&& exception, void* context) {
  auto* transaction = static_cast<TransactionInternal*>(context);
  env.ExceptionClear();
  transaction->PreserveException(exception.get());
}

void TransactionInternal::PreserveException(jthrowable exception) {
  if (*first_exception_ != nullptr || exception == nullptr) {
    return;
  }

  JNIEnv* env = firestore_->app()->GetJNIEnv();
  if (FirebaseFirestoreExceptionInternal::IsFirestoreException(env,
                                                               exception)) {
    jobject firestore_exception =
        FirebaseFirestoreExceptionInternal::ToException(env, exception);
    *first_exception_ =
        static_cast<jthrowable>(env->NewGlobalRef(firestore_exception));
    env->DeleteLocalRef(firestore_exception);
  } else {
    *first_exception_ = static_cast<jthrowable>(env->NewGlobalRef(exception));
  }
}

void TransactionInternal::ClearException() {
  if (*first_exception_) {
    firestore_->app()->GetJNIEnv()->DeleteGlobalRef(*first_exception_);
    *first_exception_ = nullptr;
  }
}

bool TransactionInternal::CheckAndClearJniExceptions() {
  JNIEnv* env = firestore_->app()->GetJNIEnv();
  jthrowable exception = env->ExceptionOccurred();
  // We need to clear the exception before we can do anything useful. Only limit
  // JNI APIs have defined behavior when there is an active exception.
  bool result = util::CheckAndClearJniExceptions(env);
  PreserveException(exception);
  return result;
}

/* static */
jobject TransactionInternal::ToJavaObject(JNIEnv* env,
                                          FirestoreInternal* firestore,
                                          TransactionFunction* function) {
  jobject result = env->NewObject(
      transaction_function::GetClass(),
      transaction_function::GetMethodId(transaction_function::kConstructor),
      reinterpret_cast<jlong>(firestore), reinterpret_cast<jlong>(function));
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
jobject TransactionInternal::TransactionFunctionNativeApply(
    JNIEnv* env, jclass clazz, jlong firestore_ptr,
    jlong transaction_function_ptr, jobject transaction) {
  if (firestore_ptr == 0 || transaction_function_ptr == 0) {
    return nullptr;
  }

  FirestoreInternal* firestore =
      reinterpret_cast<FirestoreInternal*>(firestore_ptr);
  TransactionFunction* transaction_function =
      reinterpret_cast<TransactionFunction*>(transaction_function_ptr);
  Transaction cpp_transaction(new TransactionInternal{firestore, transaction});
  cpp_transaction.internal_->ClearException();
  std::string message;
  Error code = transaction_function->Apply(cpp_transaction, message);

  jobject first_exception =
      env->NewLocalRef(*(cpp_transaction.internal_->first_exception_));

  if (first_exception) {
    cpp_transaction.internal_->ClearException();
    return first_exception;
  } else {
    return FirebaseFirestoreExceptionInternal::ToException(env, code,
                                                           message.c_str());
  }
}

/* static */
bool TransactionInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = transaction::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
bool TransactionInternal::InitializeEmbeddedClasses(
    App* app, const std::vector<internal::EmbeddedFile>* embedded_files) {
  static const JNINativeMethod kTransactionFunctionNatives[] = {
      {"nativeApply",
       "(JJLcom/google/firebase/firestore/Transaction;)Ljava/lang/Exception;",
       reinterpret_cast<void*>(
           &TransactionInternal::TransactionFunctionNativeApply)}};
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = transaction_function::CacheClassFromFiles(env, activity,
                                                          embedded_files) &&
                transaction_function::CacheMethodIds(env, activity) &&
                transaction_function::RegisterNatives(
                    env, kTransactionFunctionNatives,
                    FIREBASE_ARRAYSIZE(kTransactionFunctionNatives));
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void TransactionInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  transaction::ReleaseClass(env);
  transaction_function::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
