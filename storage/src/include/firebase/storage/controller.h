// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_CONTROLLER_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_CONTROLLER_H_

#include <cstdint>

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {

namespace internal {
class ControllerInternal;
class ListenerInternal;
class RestOperation;
}  // namespace internal

class Controller {
 public:
  Controller();
  ~Controller();
  Controller(const Controller& other);
  Controller& operator=(const Controller& other);
  Controller(Controller&& other) noexcept;
  Controller& operator=(Controller&& other) noexcept;

  bool Pause();
  bool Resume();
  bool Cancel();
  bool is_paused() const;
  int64_t bytes_transferred() const;
  int64_t total_byte_count() const;
  StorageReference GetReference() const;
  bool is_valid() const;

#ifndef SWIG
  bool operator==(const Controller& other) const;
  bool operator!=(const Controller& other) const { return !(*this == other); }
#endif  // SWIG

 private:
  friend class internal::StorageReferenceInternal;
  friend class internal::ControllerInternal;
  friend class internal::ListenerInternal;
  friend class internal::RestOperation;

  explicit Controller(internal::ControllerInternal* internal);
  internal::ControllerInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_CONTROLLER_H_
