// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_

#include "firebase/storage/controller.h"

namespace firebase {
namespace storage {

namespace internal {
class ControllerInternal;
class ListenerInternal;
class StorageReferenceInternal;
class RestOperation;
}  // namespace internal

class Listener {
 public:
  Listener();
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  Listener(Listener&&) = delete;
  Listener& operator=(Listener&&) = delete;
  virtual ~Listener();

  void Detach();
  virtual void OnPaused(Controller* /*controller*/) {}
  virtual void OnProgress(Controller* controller) = 0;

 private:
  friend class internal::ControllerInternal;
  friend class internal::ListenerInternal;
  friend class internal::StorageReferenceInternal;
  friend class internal::RestOperation;

  internal::ListenerInternal* impl_ = nullptr;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LISTENER_H_
