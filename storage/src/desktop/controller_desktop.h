// Copyright 2017 Google LLC

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

class ControllerBinding {
 public:
  ControllerBinding() = default;
  ControllerBinding(const ControllerBinding&) = delete;
  ControllerBinding& operator=(const ControllerBinding&) = delete;

  void Bind(std::shared_ptr<RestOperation::OperationHandle> handle);
  void MarkFinished();
  bool Cancel();
  bool Pause();
  bool Resume();
  [[nodiscard]] bool IsPaused() const;
  [[nodiscard]] bool is_cancelled() const;
  [[nodiscard]] std::shared_ptr<RestOperation::OperationHandle> current() const;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<RestOperation::OperationHandle> current_;
  bool cancelled_ = false;
  bool finished_ = false;
  bool pending_pause_ = false;
};

class ControllerInternal {
 public:
  ControllerInternal();
  ~ControllerInternal();
  ControllerInternal(const ControllerInternal& other);
  ControllerInternal& operator=(const ControllerInternal& other);
  ControllerInternal(ControllerInternal&& other) noexcept;
  ControllerInternal& operator=(ControllerInternal&& other) noexcept;

  bool Pause();
  bool Resume();
  bool Cancel();
  [[nodiscard]] bool is_paused() const;
  [[nodiscard]] int64_t bytes_transferred() const;
  [[nodiscard]] int64_t total_byte_count() const;
  [[nodiscard]] StorageReferenceInternal* GetReference() const;
  [[nodiscard]] bool is_valid() const;

  bool operator==(const ControllerInternal& other) const;
  bool operator!=(const ControllerInternal& other) const {
    return !(*this == other);
  }

  void Initialize(const StorageReference& reference, RestOperation* operation);
  void SetBinding(std::shared_ptr<ControllerBinding> binding);
  [[nodiscard]] std::shared_ptr<RestOperation::OperationHandle>
  operation_handle() const;

 private:
  struct State {
    std::shared_ptr<ControllerBinding> binding;
    StorageReference reference;
    std::shared_ptr<RestOperation::OperationHandle> op_handle;
    std::shared_ptr<RestOperation::OperationHandle> cached_handle;
    int64_t bytes_transferred = 0;
    int64_t total_byte_count = -1;
    bool is_valid = false;
  };

  explicit ControllerInternal(State&& state) noexcept;
  State Snapshot() const;
  State TakeState() noexcept;
  void Assign(State&& state) noexcept;
  void UpdateFromOperation(int64_t* transferred, int64_t* total) const;
  static void ReadHandle(RestOperation::OperationHandle& handle,
                         int64_t* transferred, int64_t* total);
  template <typename BindingFn, typename OpFn>
  bool WithTarget(BindingFn binding_fn, OpFn op_fn) const;

  mutable std::mutex mutex_;
  std::shared_ptr<ControllerBinding> binding_;
  StorageReference reference_;
  std::shared_ptr<RestOperation::OperationHandle> op_handle_;
  mutable std::shared_ptr<RestOperation::OperationHandle> cached_handle_;
  mutable int64_t bytes_transferred_ = 0;
  mutable int64_t total_byte_count_ = -1;
  bool is_valid_ = false;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
