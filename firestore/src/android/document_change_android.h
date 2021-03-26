#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_ANDROID_H_

#include <cstdint>

#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class DocumentChangeInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  static void Initialize(jni::Loader& loader);

  DocumentChange::Type type() const;
  DocumentSnapshot document() const;
  std::size_t old_index() const;
  std::size_t new_index() const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_CHANGE_ANDROID_H_
