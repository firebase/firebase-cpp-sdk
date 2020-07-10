#include "firestore/src/include/firebase/firestore/settings.h"

#include <dispatch/dispatch.h>

#include "absl/memory/memory.h"
#include "Firestore/core/src/firebase/firestore/util/executor_libdispatch.h"

namespace firebase {
namespace firestore {

using util::Executor;
using util::ExecutorLibdispatch;

std::unique_ptr<Executor> Settings::CreateExecutor() const {
  return absl::make_unique<ExecutorLibdispatch>(dispatch_queue());
}

dispatch_queue_t Settings::dispatch_queue() const {
  if (!executor_) {
    return dispatch_get_main_queue();
  }
  auto* executor_libdispatch = static_cast<const ExecutorLibdispatch*>(executor_.get());
  return executor_libdispatch->dispatch_queue();
}

void Settings::set_dispatch_queue(dispatch_queue_t queue) {
  executor_ = absl::make_unique<ExecutorLibdispatch>(queue);
}

}  // namespace firestore
}  // namespace firebase
