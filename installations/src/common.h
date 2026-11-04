// Copyright 2020 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_COMMON_H_
#define FIREBASE_INSTALLATIONS_SRC_COMMON_H_

#include <memory>
#include <string>

#include "app/src/reference_counted_future_impl.h"
#include "installations/src/include/firebase/installations.h"

namespace firebase {
namespace installations {
namespace internal {

enum InstallationsFn {
  kInstallationsFnGetId = 0,
  kInstallationsFnDelete = 1,
  kInstallationsFnGetToken = 2,
  kInstallationsFnGetTokenResult = 3,
  kInstallationsFnCount = 4,
};

template <InstallationsFn Slot>
struct SlotTraits;
template <>
struct SlotTraits<kInstallationsFnGetId> { using Type = std::string; };
template <>
struct SlotTraits<kInstallationsFnDelete> { using Type = void; };
template <>
struct SlotTraits<kInstallationsFnGetToken> { using Type = std::string; };
template <>
struct SlotTraits<kInstallationsFnGetTokenResult> {
  using Type = InstallationTokenResult;
};

template <InstallationsFn Slot>
using SlotResultT = typename SlotTraits<Slot>::Type;

class InstallationsFutures {
 public:
  InstallationsFutures()
      : impl_(std::make_shared<ReferenceCountedFutureImpl>(
            kInstallationsFnCount)) {}

  template <InstallationsFn Slot>
  SafeFutureHandle<SlotResultT<Slot>> SafeAlloc() {
    return impl_->SafeAlloc<SlotResultT<Slot>>(static_cast<int>(Slot));
  }

  template <InstallationsFn Slot>
  void Complete(SafeFutureHandle<SlotResultT<Slot>> handle, int error,
                const char* error_msg = "") {
    impl_->Complete(handle, error, error_msg ? error_msg : "");
  }

  template <InstallationsFn Slot, typename ValueT>
  void CompleteWithResult(SafeFutureHandle<SlotResultT<Slot>> handle, int error,
                          const char* error_msg, const ValueT& value) {
    impl_->CompleteWithResult(handle, error, error_msg ? error_msg : "", value);
  }

  template <InstallationsFn Slot>
  Future<SlotResultT<Slot>> MakeFuture(
      SafeFutureHandle<SlotResultT<Slot>> handle) {
    return ::firebase::MakeFuture<SlotResultT<Slot>>(impl_.get(), handle);
  }

  template <InstallationsFn Slot>
  Future<SlotResultT<Slot>> LastResult() const {
    return static_cast<const Future<SlotResultT<Slot>>&>(
        impl_->LastResult(static_cast<int>(Slot)));
  }

  std::weak_ptr<ReferenceCountedFutureImpl> weak() const noexcept {
    return impl_;
  }

 private:
  std::shared_ptr<ReferenceCountedFutureImpl> impl_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_COMMON_H_
