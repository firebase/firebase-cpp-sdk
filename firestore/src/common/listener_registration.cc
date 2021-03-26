#include "firestore/src/include/firebase/firestore/listener_registration.h"

#include <utility>

#include "firestore/src/common/cleanup.h"

#if defined(__ANDROID__)
#include "firestore/src/android/listener_registration_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/listener_registration_stub.h"
#else
#include "firestore/src/ios/listener_registration_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {

namespace firestore {
// ListenerRegistration specific fact:
//   ListenerRegistration does NOT own the ListenerRegistrationInternal object,
//   which is different from other wrapper types. FirestoreInternal owns all
//   ListenerRegistrationInternal objects instead. So FirestoreInternal can
//   remove all listeners upon destruction.

using CleanupFnListenerRegistration = CleanupFn<ListenerRegistration>;

ListenerRegistration::ListenerRegistration() : ListenerRegistration(nullptr) {}

ListenerRegistration::ListenerRegistration(
    const ListenerRegistration& registration)
    : firestore_(registration.firestore_) {
  internal_ = registration.internal_;
  CleanupFnListenerRegistration::Register(this, firestore_);
}

ListenerRegistration::ListenerRegistration(ListenerRegistration&& registration)
    : firestore_(registration.firestore_) {
  CleanupFnListenerRegistration::Unregister(&registration,
                                            registration.firestore_);
  std::swap(internal_, registration.internal_);
  CleanupFnListenerRegistration::Register(this, firestore_);
}

ListenerRegistration::ListenerRegistration(
    ListenerRegistrationInternal* internal)
    : firestore_(internal == nullptr ? nullptr
                                     : internal->firestore_internal()),
      internal_(internal) {
  CleanupFnListenerRegistration::Register(this, firestore_);
}

ListenerRegistration::~ListenerRegistration() {
  CleanupFnListenerRegistration::Unregister(this, firestore_);
  internal_ = nullptr;
}

ListenerRegistration& ListenerRegistration::operator=(
    const ListenerRegistration& registration) {
  if (this == &registration) {
    return *this;
  }

  firestore_ = registration.firestore_;
  CleanupFnListenerRegistration::Unregister(this, firestore_);
  internal_ = registration.internal_;
  CleanupFnListenerRegistration::Register(this, firestore_);
  return *this;
}

ListenerRegistration& ListenerRegistration::operator=(
    ListenerRegistration&& registration) {
  if (this == &registration) {
    return *this;
  }

  firestore_ = registration.firestore_;
  CleanupFnListenerRegistration::Unregister(&registration,
                                            registration.firestore_);
  CleanupFnListenerRegistration::Unregister(this, firestore_);
  internal_ = registration.internal_;
  CleanupFnListenerRegistration::Register(this, firestore_);
  return *this;
}

void ListenerRegistration::Remove() {
  // The check for firestore_ is required. User can hold a ListenerRegistration
  // instance indefinitely even after Firestore is destructed, in which case
  // firestore_ will be reset to nullptr by the Cleanup function.
  // The check for internal_ is optional. Actually internal_ can be of following
  // cases:
  //   * nullptr if already called Remove() on the same instance;
  //   * not nullptr but invalid if Remove() is called on a copy of this;
  //   * not nullptr and valid.
  // Unregister a nullptr or invalid ListenerRegistration is a no-op.
  if (internal_ && firestore_) {
    firestore_->UnregisterListenerRegistration(internal_);
    internal_ = nullptr;
  }
}

void ListenerRegistration::Cleanup() {
  Remove();
  firestore_ = nullptr;
}

}  // namespace firestore
}  // namespace firebase
