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
}

}  // namespace firestore
}  // namespace firebase
