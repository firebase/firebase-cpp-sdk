#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>

#include <fstream>
#include <sstream>

#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

void InitializeFirestore(Firestore* instance) {
  Firestore::set_log_level(LogLevel::kLogLevelDebug);
}

}  // namespace firestore
}  // namespace firebase
