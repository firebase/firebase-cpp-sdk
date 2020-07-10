#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_ANDROID_H_

#include <jni.h>

#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/firebase_firestore_exception_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/query_snapshot_android.h"

namespace firebase {
namespace firestore {

// This class simplifies the implementation of Future APIs for Android wrappers.
// PublicType is the public type, say Foo, and InternalType is FooInternal,
// which is required to be a subclass of WrapperFuture. FnEnumType is an enum
// class that defines a set of APIs returning a Future. For example, to
// implement Future<DocumentReference> CollectionReferenceInternal::Add(),
// PublicType is DocumentReference, InternalType is DocumentReferenceInternal,
// and FnEnumType is CollectionReferenceFn.
template <typename PublicType, typename InternalType, typename FnEnumType>
class Promise {
 public:
  // One can add a completion to execute right after the Future is resolved.
  // The Games's Future library does not support chaining-up of completions yet.
  // So we add the interface here to allow executing code after Future is
  // resolved.
  template <typename PublicT>
  class Completion {
   public:
    virtual ~Completion() {}
    virtual void CompleteWith(Error error_code, const char* error_message,
                              PublicT* result) = 0;
  };

  Promise(ReferenceCountedFutureImpl* impl, FirestoreInternal* firestore,
          Completion<PublicType>* completion = nullptr)
      : completer_{new Completer<PublicType, InternalType>{impl, firestore,
                                                           completion}},
        impl_(impl) {}

  ~Promise() { delete completer_; }

  void RegisterForTask(FnEnumType op, jobject task) {
    JNIEnv* env = completer_->firestore()->app()->GetJNIEnv();
    handle_ = completer_->Alloc(static_cast<int>(op));

    // Ownership of the completer will pass to to RegisterCallbackOnTask
    Completer<PublicType, InternalType>* completer = completer_;
    completer_ = nullptr;

    util::RegisterCallbackOnTask(env, task, ResultCallback, completer,
                                 kApiIdentifier);
  }

  Future<PublicType> GetFuture() { return MakeFuture(impl_, handle_); }

 private:
  template <typename PublicT>
  class CompleterBase {
   public:
    CompleterBase(ReferenceCountedFutureImpl* impl,
                  FirestoreInternal* firestore, Completion<PublicT>* completion)
        : impl_{impl}, firestore_{firestore}, completion_(completion) {}

    virtual ~CompleterBase() {}

    FirestoreInternal* firestore() { return firestore_; }

    SafeFutureHandle<PublicT> Alloc(int fn_index) {
      handle_ = impl_->SafeAlloc<PublicT>(fn_index);
      return handle_;
    }

    virtual void CompleteWithResult(jobject result,
                                    util::FutureResult result_code,
                                    const char* status_message) {
      // result can be either the resolved object or exception, depending on
      // result_code.

      if (result_code == util::kFutureResultSuccess) {
        // When succeeded, result is the resolved object of the Future.
        SucceedWithResult(result);
        return;
      }

      Error error_code = Error::kErrorUnknown;
      switch (result_code) {
        case util::kFutureResultFailure:
          // When failed, result is the exception raised.
          error_code = FirebaseFirestoreExceptionInternal::ToErrorCode(
              this->firestore_->app()->GetJNIEnv(), result);
          break;
        case util::kFutureResultCancelled:
          error_code = Error::kErrorCancelled;
          break;
        default:
          error_code = Error::kErrorUnknown;
          FIREBASE_ASSERT_MESSAGE(false, "unknown FutureResult %d",
                                  result_code);
          break;
      }
      this->impl_->Complete(this->handle_, error_code, status_message);
      if (this->completion_ != nullptr) {
        this->completion_->CompleteWith(error_code, status_message, nullptr);
      }
      delete this;
    }

    virtual void SucceedWithResult(jobject result) = 0;

   protected:
    SafeFutureHandle<PublicT> handle_;
    ReferenceCountedFutureImpl* impl_;    // not owning
    FirestoreInternal* firestore_;        // not owning
    Completion<PublicType>* completion_;  // not owning
  };

  // Partial specialization of a nested class is allowed. So adding the no-op
  // Dummy parameter just to suppress the error:
  //     explicit specialization of 'Completer' in class scope
  template <typename PublicT, typename InternalT, typename Dummy = void>
  class Completer : public CompleterBase<PublicT> {
   public:
    using CompleterBase<PublicT>::CompleterBase;

    void SucceedWithResult(jobject result) override {
      PublicT future_result = FirestoreInternal::Wrap<InternalT>(
          new InternalT(this->firestore_, result));

      this->impl_->CompleteWithResult(this->handle_, Error::kErrorOk,
                                      /*error_msg=*/"", future_result);
      if (this->completion_ != nullptr) {
        this->completion_->CompleteWith(Error::kErrorOk, /*error_message*/ "",
                                        &future_result);
      }
      delete this;
    }
  };

  template <typename Dummy>
  class Completer<void, void, Dummy> : public CompleterBase<void> {
   public:
    using CompleterBase<void>::CompleterBase;

    void SucceedWithResult(jobject result) override {
      this->impl_->Complete(this->handle_, Error::kErrorOk, /*error_msg=*/"");
      if (this->completion_ != nullptr) {
        this->completion_->CompleteWith(Error::kErrorOk, /*error_message*/ "",
                                        nullptr);
      }
      delete this;
    }
  };

  static void ResultCallback(JNIEnv* env, jobject result,
                             util::FutureResult result_code,
                             const char* status_message, void* callback_data) {
    if (callback_data != nullptr) {
      auto* data =
          static_cast<Completer<PublicType, InternalType>*>(callback_data);
      data->CompleteWithResult(result, result_code, status_message);
    }
  }

  Completer<PublicType, InternalType>* completer_;

  // Keep these values separate from the Completer in case completion happens
  // before the future is constructed.
  ReferenceCountedFutureImpl* impl_;
  SafeFutureHandle<PublicType> handle_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_PROMISE_ANDROID_H_
