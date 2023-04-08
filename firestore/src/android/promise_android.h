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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_

#include <jni.h>
#include <memory>

#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "firestore/src/android/aggregate_query_snapshot_android.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {

template <typename EnumT>
class PromiseFactory;

// This class simplifies the implementation of Future APIs for Android wrappers.
// PublicType is the public type, say Foo, and InternalType is FooInternal.
// FnEnumType is an enum class that defines a set of APIs returning a Future.
//
// For example, to implement:
//
//     Future<DocumentReference> CollectionReferenceInternal::Add()
//
// PublicType is DocumentReference, InternalType is DocumentReferenceInternal,
// and FnEnumType is CollectionReferenceFn.
template <typename PublicType, typename InternalType, typename FnEnumType>
class Promise {
  friend class PromiseFactory<FnEnumType>;

 public:
  // One can add a completion to execute right after the Future is resolved.
  // The Games's Future library does not support chaining-up of completions yet.
  // So we add the interface here to allow executing code after Future is
  // resolved.
  class Completion {
   public:
    virtual ~Completion() = default;
    virtual void CompleteWith(Error error_code,
                              const char* error_message,
                              PublicType* result) = 0;
  };

  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;

  Promise(Promise&& other) = default;
  Promise& operator=(Promise&& other) = default;

  void RegisterForTask(jni::Env& env, FnEnumType op, const jni::Task& task) {
    handle_ = completer_->Alloc(static_cast<int>(op));

    // Ownership of the completer will pass to to RegisterCallbackOnTask
    auto* completer = completer_.release();

    ::firebase::util::RegisterCallbackOnTask(
        env.get(), task.get(), ResultCallback, completer, kApiIdentifier);
  }

  Future<PublicType> GetFuture() {
    return firestore_ref_.Run([this](FirestoreInternal* firestore) {
      if (firestore == nullptr) {
        return Future<PublicType>{};
      }
      return MakeFuture(impl_, handle_);
    });
  }

 private:
  // The constructor is intentionally private.
  // Create instances with `PromiseFactory`.
  Promise(const FirestoreInternalWeakReference& firestore_ref,
          ReferenceCountedFutureImpl* impl,
          Completion* completion)
      : firestore_ref_(firestore_ref),
        completer_(std::make_unique<Completer<PublicType, InternalType>>(
            firestore_ref, impl, completion)),
        impl_(impl) {}

  template <typename PublicT>
  class CompleterBase {
   public:
    CompleterBase(const FirestoreInternalWeakReference& firestore_ref,
                  ReferenceCountedFutureImpl* impl,
                  Completion* completion)
        : firestore_ref_{firestore_ref}, impl_{impl}, completion_(completion) {}

    virtual ~CompleterBase() = default;

    SafeFutureHandle<PublicT> Alloc(int fn_index) {
      return firestore_ref_.Run([this, fn_index](FirestoreInternal* firestore) {
        if (firestore == nullptr) {
          return SafeFutureHandle<PublicT>{};
        }
        handle_ = impl_->SafeAlloc<PublicT>(fn_index);
        return handle_;
      });
    }

    virtual void CompleteWithResult(jobject raw_result,
                                    ::firebase::util::FutureResult result_code,
                                    const char* status_message) {
      // result can be either the resolved object or exception, depending on
      // result_code.
      jni::Env env;
      jni::Object result(raw_result);

      if (result_code == ::firebase::util::kFutureResultSuccess) {
        firestore_ref_.RunIfValid(
            [this, &env, &result](FirestoreInternal& firestore) {
              SucceedWithResult(env, result, firestore);
            });

        delete this;

        return;
      }

      Error error_code = Error::kErrorUnknown;
      switch (result_code) {
        case ::firebase::util::kFutureResultFailure:
          // When failed, result is the exception raised.
          error_code = ExceptionInternal::GetErrorCode(env, result);
          break;
        case ::firebase::util::kFutureResultCancelled:
          error_code = Error::kErrorCancelled;
          break;
        default:
          FIREBASE_ASSERT_MESSAGE(false, "unknown FutureResult %d",
                                  result_code);
          break;
      }
      firestore_ref_.RunIfValid(
          [this, error_code, status_message](FirestoreInternal& firestore) {
            impl_->Complete(handle_, error_code, status_message);
          });
      if (completion_ != nullptr) {
        completion_->CompleteWith(error_code, status_message, nullptr);
      }
      delete this;
    }

    virtual void SucceedWithResult(jni::Env& env,
                                   const jni::Object& result,
                                   FirestoreInternal& firestore) = 0;

   private:
    FirestoreInternalWeakReference firestore_ref_;

   protected:
    SafeFutureHandle<PublicT> handle_;
    ReferenceCountedFutureImpl* impl_;  // not owning
    Completion* completion_;            // not owning
  };

  // Partial specialization of a nested class is allowed. So adding the no-op
  // Dummy parameter just to suppress the error:
  //     explicit specialization of 'Completer' in class scope
  template <typename PublicT, typename InternalT, typename Dummy = void>
  class Completer : public CompleterBase<PublicT> {
   public:
    using CompleterBase<PublicT>::CompleterBase;

    void SucceedWithResult(jni::Env& env,
                           const jni::Object& result,
                           FirestoreInternal& firestore) override {
      auto future_result =
          MakePublic<PublicT, InternalT>(env, &firestore, result);

      this->impl_->CompleteWithResult(this->handle_, Error::kErrorOk,
                                      /*error_msg=*/"", future_result);
      if (this->completion_ != nullptr) {
        this->completion_->CompleteWith(Error::kErrorOk, /*error_message*/ "",
                                        &future_result);
      }
    }
  };

  template <typename Dummy>
  class Completer<void, void, Dummy> : public CompleterBase<void> {
   public:
    using CompleterBase<void>::CompleterBase;

    void SucceedWithResult(jni::Env& env,
                           const jni::Object& result,
                           FirestoreInternal&) override {
      this->impl_->Complete(this->handle_, Error::kErrorOk, /*error_msg=*/"");
      if (this->completion_ != nullptr) {
        this->completion_->CompleteWith(Error::kErrorOk, /*error_message*/ "",
                                        nullptr);
      }
    }
  };

  static void ResultCallback(JNIEnv* env,
                             jobject result,
                             ::firebase::util::FutureResult result_code,
                             const char* status_message,
                             void* callback_data) {
    if (callback_data != nullptr) {
      auto* data =
          static_cast<Completer<PublicType, InternalType>*>(callback_data);
      data->CompleteWithResult(result, result_code, status_message);
    }
  }

  FirestoreInternalWeakReference firestore_ref_;

  std::unique_ptr<Completer<PublicType, InternalType>> completer_;

  // Keep these values separate from the Completer in case completion happens
  // before the future is constructed.
  ReferenceCountedFutureImpl* impl_;
  SafeFutureHandle<PublicType> handle_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_
