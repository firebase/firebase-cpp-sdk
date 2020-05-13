#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_SOURCE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_SOURCE_IOS_H_

#include "firestore/src/include/firebase/firestore/source.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "Firestore/core/src/api/source.h"

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_SOURCE_IOS_H_
