// Copyright 2021 Google LLC

#include "firestore/src/common/futures.h"

namespace firebase {
namespace firestore {
namespace internal {

ReferenceCountedFutureImpl* GetSharedReferenceCountedFutureImpl() {
  static auto* futures =
      new ReferenceCountedFutureImpl(/*last_result_count=*/0);
  return futures;
}

}  // namespace internal
}  // namespace firestore
}  // namespace firebase
