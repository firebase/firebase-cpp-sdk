#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>

#include <fstream>
#include <sstream>

#include "firestore/src/include/firebase/firestore.h"

namespace firebase {
namespace firestore {

bool ProcessEvents(int millis) {
  NSLog(@"ProcessEvents(%d)", millis);

  // Instead of waiting using the C++ standard library, force the main run loop
  // to run. This will cause any callbacks scheduled on the main queue to be
  // processed while we wait, allowing callbacks which always call out on the
  // main queue to be processed. This is especially important for interacting
  // with Firebase Auth, which always calls back on the main queue.
  auto interval = static_cast<NSTimeInterval>(millis) / 1000.0f;
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:interval]];

  // `false` means "don't shut down the application".
  return false;
}

void InitializeFirestore(Firestore* instance) {
  Firestore::set_log_level(LogLevel::kLogLevelDebug);
}

}  // namespace firestore
}  // namespace firebase
