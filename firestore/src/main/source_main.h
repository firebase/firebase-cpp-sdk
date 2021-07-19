// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_SOURCE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_SOURCE_MAIN_H_

#include "Firestore/core/src/api/source.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/include/firebase/firestore/source.h"

namespace firebase {
namespace firestore {

inline api::Source ToCoreApi(Source source) {
  switch (source) {
    case Source::kDefault:
      return api::Source::Default;
    case Source::kServer:
      return api::Source::Server;
    case Source::kCache:
      return api::Source::Cache;
  }
  UNREACHABLE();
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_SOURCE_MAIN_H_
