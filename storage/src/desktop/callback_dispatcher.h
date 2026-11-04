// Copyright 2026 Google LLC

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_

#include <functional>
#include <memory>
#include <thread>

namespace firebase::storage::internal {

class CallbackDispatcher {
 public:
  CallbackDispatcher();
  ~CallbackDispatcher();

  CallbackDispatcher(const CallbackDispatcher&) = delete;
  CallbackDispatcher& operator=(const CallbackDispatcher&) = delete;
  CallbackDispatcher(CallbackDispatcher&&) = delete;
  CallbackDispatcher& operator=(CallbackDispatcher&&) = delete;

  bool Post(std::function<void()> task);
  void Shutdown();
  [[nodiscard]] bool IsWorkerThread() const;

 private:
  struct State;
  static void Run(std::shared_ptr<State> state);
  std::shared_ptr<State> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_
