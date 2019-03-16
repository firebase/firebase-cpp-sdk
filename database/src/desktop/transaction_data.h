// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_TRANSACTION_DATA_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_TRANSACTION_DATA_H_

#include <list>
#include <queue>
#include "app/memory/shared_ptr.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database/listener.h"
#include "database/src/include/firebase/database/transaction.h"

namespace firebase {
namespace database {
namespace internal {
class DatabaseInternal;

struct TransactionData {
  enum Status {
    kStatusInitializing = 0,
    // We've run the transaction and updated current_output value/priority with
    // the result, but it isn't currently sent to the server. A transaction will
    // go from RUN -> SENT -> RUN if it comes back from the server as rejected
    // due to mismatched hash.
    kStatusRun,
    // We've run the transaction and sent it to the server and it's currently
    // outstanding (hasn't come back as accepted or rejected yet).
    kStatusSent,
    // Temporary state used to mark completed transactions (whether successful
    // or aborted). The transaction will be removed when we get a chance to
    // prune completed ones.
    kStatusComplete,
    // Used when an already-sent transaction needs to be aborted (e.g. due to a
    // conflicting set() call that was made). If it comes back as unsuccessful,
    // we'll abort it.
    kStatusSentNeedsAbort,
    // Temporary state used to mark transactions_ that need to be aborted.
    kStatusNeedsAbort
  };

  // This constructor is primarily used for testing
  TransactionData() : delete_context(nullptr), outstanding_listener(nullptr) {}

  // Constructor to capture all data for a RunTransaction request
  TransactionData(const SafeFutureHandle<DataSnapshot>& handle,
                  ReferenceCountedFutureImpl* ref_future, const Path& path,
                  DoTransactionWithContext function, void* context,
                  void (*delete_context)(void*), bool trigger_local_events,
                  UniquePtr<ValueListener> outstanding_listener)
      : future_handle(handle),
        ref_future(ref_future),
        path(path),
        transaction_function(function),
        context(context),
        delete_context(delete_context),
        trigger_local_events(trigger_local_events),
        outstanding_listener(outstanding_listener),
        status(kStatusInitializing),
        retry_count(0) {}

  ~TransactionData();

  // Comparor for std::priority_queue to ensure transaction is run in order.
  // std::priority_queue has some quirk that when this function returns true,
  // this element has higher priority.  However, higher priority elements come
  // out last.  That is, lower number with lower priority comes out first.
  bool operator<(const TransactionData& other) const {
    return transaction_order > other.transaction_order;
  }

  // References for Future to fulfill
  SafeFutureHandle<DataSnapshot> future_handle;
  ReferenceCountedFutureImpl* ref_future;

  // Path, necessary for listener unregistration.
  Path path;

  // References for DoTransaction passed by the developer
  DoTransactionWithContext transaction_function;
  void* context;
  void (*delete_context)(void*);

  // Not sure what this is.  Should be related to SyncTree
  bool trigger_local_events;

  // Reference to the listener for this transaction to keep local cache fresh
  UniquePtr<ValueListener> outstanding_listener;

  // Transaction order to ensure transactions are rerun in order.
  uint64_t transaction_order;

  // Transaction status
  Status status;

  // Number of retry attempts so far
  int retry_count;

  static const int kTransactionMaxRetries;

  uint64_t current_write_id;

  // Value before DoTransaction
  Variant current_input_snapshot;

  // Value after DoTransaction
  Variant current_output_snapshot_raw;

  // Value after server values are resolved.
  Variant current_output_snapshot_resolved;

  // std::string abort_reason;
  Error abort_reason;
};
typedef SharedPtr<TransactionData> TransactionDataPtr;

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_TRANSACTION_DATA_H_
