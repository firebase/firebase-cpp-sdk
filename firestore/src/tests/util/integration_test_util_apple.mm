#include <dispatch/dispatch.h>

#include <fstream>
#include <sstream>

#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

// Note: currently, this file has to be Objective-C++ (`.mm`), because `Settings` are defined in
// such a way that configuring the dispatch queue is only possible within Objective-C++ translation
// units.
// TODO(varconst): fix this somehow.

void InitializeFirestore(Firestore* instance) {
  Firestore::set_log_level(LogLevel::kLogLevelDebug);

  // By default, Firestore runs user callbacks on the main thread; because the
  // test also runs on the main thread, the callback will never be invoked. Use
  // a different dispatch queue instead.
  auto settings = instance->settings();
  auto queue = dispatch_queue_create("user_executor", DISPATCH_QUEUE_SERIAL);
  settings.set_dispatch_queue(queue);

  instance->set_settings(settings);
}

}  // namespace firestore
}  // namespace firebase
