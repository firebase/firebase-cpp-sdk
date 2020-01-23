#include "firestore/src/common/wrapper_assertions.h"

namespace firebase {
namespace firestore {
namespace testutil {

#if defined(__ANDROID__)

template <>
ListenerRegistrationInternal* NewInternal<ListenerRegistrationInternal>() {
  return nullptr;
}

#endif  // defined(__ANDROID__)

}  // namespace testutil
}  // namespace firestore
}  // namespace firebase
