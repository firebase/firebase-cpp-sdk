// Copyright 2017 Google Inc. All Rights Reserved.

#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_

#if __ANDROID__
#include "firestore/src/android/firestore_android.h"
#else
#include "firestore/src/ios/firestore_ios.h"
#endif  // __ANDROID__

#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

// T is a Firestore public type. U is a internal type that can provide a
// FirestoreInternal instance. Normally U is just the internal type of T.
//
// F is almost always FirestoreInternal unless one wants something else to
// manage the cleanup process. We define type F to make this CleanupFn
// implementation platform-independent.
template <typename T, typename U = InternalType<T>,
          typename F = FirestoreInternal>
struct CleanupFn {
  static void Cleanup(void* obj_void) { DoCleanup(static_cast<T*>(obj_void)); }

  static void Register(T* obj, F* firestore) {
    if (firestore) {
      firestore->cleanup().RegisterObject(obj, CleanupFn<T, U>::Cleanup);
    }
  }

  static void Register(T* obj, U* internal) {
    if (internal) {
      Register(obj, internal->firestore_internal());
    }
  }

  static void Unregister(T* obj, F* firestore) {
    if (firestore) {
      firestore->cleanup().UnregisterObject(obj);
    }
  }

  static void Unregister(T* obj, U* internal) {
    if (internal) {
      Unregister(obj, internal->firestore_internal());
    }
  }

 private:
  template <typename Object>
  static void DoCleanup(Object* obj) {
    // Order is crucially important here: under rare conditions, during cleanup,
    // the destructor of the `internal_` object can trigger the deletion of the
    // containing object. For example, this can happen when the `internal_`
    // object destroys its Future API, which deletes a Future referring to the
    // public object containing this `internal_` object. See
    // http://go/paste/4669581387890688 for an example of what this looks like.
    //
    // By setting `internal_` to null before deleting it, the destructor of the
    // outer object is prevented from deleting `internal_` twice.
    auto internal = obj->internal_;
    obj->internal_ = nullptr;

    delete internal;
  }

  // `ListenerRegistration` objects differ from the common pattern.
  static void DoCleanup(ListenerRegistration* obj) { obj->Cleanup(); }
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_CLEANUP_H_
