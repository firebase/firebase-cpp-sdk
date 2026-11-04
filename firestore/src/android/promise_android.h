// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/reference_counted_future_impl.h"
#include "firestore/src/android/aggregate_query_snapshot_android.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/task.h"

namespace firebase::firestore {

template <typename EnumT>
class PromiseFactory;

template <typename PublicType, typename InternalType, typename FnEnumType>
class Promise {
  friend class PromiseFactory<FnEnumType>;

 public:
  class Completion {
   public:
    virtual ~Completion() = default;
    virtual void CompleteWith(Error error_code, const char* error_message,
                              PublicType* result) = 0;
  };

  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;

  Promise(Promise&& other) noexcept
      : firestore_ref_(std::move(other.firestore_ref_)),
        completer_(std::move(other.completer_)),
        impl_(std::exchange(other.impl_, nullptr)),
        handle_(std::exchange(other.handle_, SafeFutureHandle<PublicType>{})) {}

  Promise& operator=(Promise&& other) noexcept {
    if (this != &other) {
      firestore_ref_ = std::move(other.firestore_ref_);
      completer_ = std::move(other.completer_);
      impl_ = std::exchange(other.impl_, nullptr);
      handle_ = std::exchange(other.handle_, SafeFutureHandle<PublicType>{});
    }
    return *this;
  }

  void RegisterForTask(jni::Env& env, FnEnumType op, const jni::Task& task,
                       const char* fallback_error = "Unknown error.") {
    if (!completer_) return;
    handle_ = completer_->Alloc(static_cast<int>(op));
    if (!env.ok() || !task) {
      firestore_ref_.RunIfValid([this, fallback_error](FirestoreInternal&) {
        impl_->Complete(handle_, Error::kErrorUnknown, fallback_error);
        if (completer_->completion_) {
          completer_->completion_->CompleteWith(Error::kErrorUnknown,
                                                fallback_error, nullptr);
        }
      });
      return;
    }
    ::firebase::util::RegisterCallbackOnTask(env.get(), task.get(),
                                             ResultCallback,
                                             completer_.release(),
                                             kApiIdentifier);
  }

  [[nodiscard]] Future<PublicType> GetFuture() {
    return firestore_ref_.Run([this](FirestoreInternal* firestore) {
      return firestore ? MakeFuture(impl_, handle_) : Future<PublicType>{};
    });
  }

 private:
  Promise(const FirestoreInternalWeakReference& firestore_ref,
          ReferenceCountedFutureImpl* impl, Completion* completion)
      : firestore_ref_(firestore_ref),
        completer_(std::make_unique<Completer>(firestore_ref, impl, completion)),
        impl_(impl) {}

  class Completer {
    friend class Promise;

   public:
    Completer(const FirestoreInternalWeakReference& firestore_ref,
              ReferenceCountedFutureImpl* impl, Completion* completion)
        : firestore_ref_{firestore_ref}, impl_{impl}, completion_(completion) {}

    SafeFutureHandle<PublicType> Alloc(int fn_index) {
      return firestore_ref_.Run([this, fn_index](FirestoreInternal* firestore) {
        if (!firestore) return SafeFutureHandle<PublicType>{};
        handle_ = impl_->SafeAlloc<PublicType>(fn_index);
        return handle_;
      });
    }

    void CompleteWithResult(JNIEnv* env_ptr, jobject raw_result,
                            ::firebase::util::FutureResult result_code,
                            const char* status_message) {
      jni::Env env(env_ptr);
      jni::ExceptionClearGuard exception_clear_guard(env);
      jni::Object result(raw_result);

      if (result_code == ::firebase::util::kFutureResultSuccess) {
        firestore_ref_.RunIfValid(
            [this, &env, &result](FirestoreInternal& firestore) {
              if constexpr (std::is_void_v<PublicType>) {
                (void)env;
                (void)result;
                impl_->Complete(handle_, Error::kErrorOk, "");
                if (completion_) {
                  completion_->CompleteWith(Error::kErrorOk, "", nullptr);
                }
              } else {
                auto future_result = MakePublic<PublicType, InternalType>(
                    env, &firestore, result);
                impl_->CompleteWithResult(handle_, Error::kErrorOk, "",
                                          future_result);
                if (completion_) {
                  completion_->CompleteWith(Error::kErrorOk, "",
                                            &future_result);
                }
              }
            });
        return;
      }

      Error error_code = Error::kErrorUnknown;
      if (result_code == ::firebase::util::kFutureResultFailure) {
        error_code = ExceptionInternal::GetErrorCode(env, result);
      } else if (result_code == ::firebase::util::kFutureResultCancelled) {
        error_code = Error::kErrorCancelled;
      } else {
        FIREBASE_ASSERT_MESSAGE(false, "unknown FutureResult %d", result_code);
      }
      firestore_ref_.RunIfValid(
          [this, error_code, status_message](FirestoreInternal&) {
            impl_->Complete(handle_, error_code, status_message);
            if (completion_) {
              completion_->CompleteWith(error_code, status_message, nullptr);
            }
          });
    }

   private:
    FirestoreInternalWeakReference firestore_ref_;
    SafeFutureHandle<PublicType> handle_{};
    ReferenceCountedFutureImpl* impl_;
    Completion* completion_;
  };

  static void ResultCallback(JNIEnv* env, jobject result,
                             ::firebase::util::FutureResult result_code,
                             const char* status_message, void* callback_data) {
    if (!callback_data) return;
    std::unique_ptr<Completer> data(static_cast<Completer*>(callback_data));
#if FIRESTORE_HAVE_EXCEPTIONS
    try {
#endif
      data->CompleteWithResult(env, result, result_code, status_message);
#if FIRESTORE_HAVE_EXCEPTIONS
    } catch (...) {
    }
#endif
  }

  FirestoreInternalWeakReference firestore_ref_;
  std::unique_ptr<Completer> completer_;
  ReferenceCountedFutureImpl* impl_;
  SafeFutureHandle<PublicType> handle_{};
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_PROMISE_ANDROID_H_
