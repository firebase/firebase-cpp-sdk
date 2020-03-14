#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TRANSACTION_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TRANSACTION_ANDROID_H_

#include <string>

#include "app/memory/shared_ptr.h"
#include "app/meta/move.h"
#include "app/src/embedded_file.h"
#include "firestore/src/android/wrapper_future.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/transaction.h"

namespace firebase {
namespace firestore {

class TransactionInternal : public Wrapper {
 public:
  using ApiType = Transaction;

  TransactionInternal(FirestoreInternal* firestore, jobject obj)
      : Wrapper(firestore, obj), first_exception_{new jthrowable{nullptr}} {}

  TransactionInternal(const TransactionInternal& rhs)
      : Wrapper(rhs), first_exception_(rhs.first_exception_) {}

  TransactionInternal(TransactionInternal&& rhs)
      : Wrapper(firebase::Move(rhs)), first_exception_(rhs.first_exception_) {}

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options);

  void Update(const DocumentReference& document, const MapFieldValue& data);

  void Update(const DocumentReference& document, const MapFieldPathValue& data);

  void Delete(const DocumentReference& document);

  DocumentSnapshot Get(const DocumentReference& document, Error* error_code,
                       std::string* error_message);

  static jobject ToJavaObject(JNIEnv* env, FirestoreInternal* firestore,
                              TransactionFunction* function);

  static jobject TransactionFunctionNativeApply(JNIEnv* env, jclass clazz,
                                                jlong firestore_ptr,
                                                jlong transaction_function_ptr,
                                                jobject transaction);

 private:
  // If this is the first exception, then store it. Otherwise, preserve the
  // current exception. Passing nullptr has no effect.
  void PreserveException(jthrowable exception);

  // Clear the global reference of the first exception, if any. The SharedPtr
  // does not support custom deleters and thus we must call this explicitly.
  // The workflow of RunTransaction allows us to do so.
  void ClearException();

  // A helper function to replace util::CheckAndClearJniExceptions(env). It will
  // check and clear all exceptions just as what the one under util is doing. In
  // addition, it also preserves the first ever exception during a Transaction.
  // We do not preserve each and every exception. Only the first one matters and
  // more than likely the subsequent exception is caused by the first one.
  bool CheckAndClearJniExceptions();

  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static bool InitializeEmbeddedClasses(
      App* app, const std::vector<internal::EmbeddedFile>* embedded_files);
  static void Terminate(App* app);

  // The first exception that occurred. Because exceptions must be cleared
  // before calling other JNI methods, we cannot rely on the Java exception
  // mechanism to properly handle native calls via JNI. The first exception is
  // shared by a transaction and its copies. User is allowed to make copy and
  // call transaction operation on the copy.
  SharedPtr<jthrowable> first_exception_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_TRANSACTION_ANDROID_H_
