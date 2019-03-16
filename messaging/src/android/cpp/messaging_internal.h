// Copyright 2017 Google LLC
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

#ifndef FIREBASE_MESSAGING_CLIENT_CPP_SRC_ANDROID_CPP_MESSAGING_INTERNAL_H_
#define FIREBASE_MESSAGING_CLIENT_CPP_SRC_ANDROID_CPP_MESSAGING_INTERNAL_H_

namespace firebase {
namespace messaging {

// LINT.IfChange
static const char kLockfile[] = "FIREBASE_CLOUD_MESSAGING_LOCKFILE";
// LINT.ThenChange(//depot_firebase_cpp/messaging/client/cpp/src/android/java/com/google/firebase/messaging/cpp/ListenerService.java)

// LINT.IfChange
static const char kStorageFile[] = "FIREBASE_CLOUD_MESSAGING_LOCAL_STORAGE";
// LINT.ThenChange(//depot_firebase_cpp/messaging/client/cpp/src/android/java/com/google/firebase/messaging/cpp/ListenerService.java)

// Acquires a lock on a lock file and releases when this object goes out
// of scope.
class FileLocker {
 public:
  // Lock a lock file.
  explicit FileLocker(const char* lock_filename);

  // Release the lock, if it was successfully required.
  ~FileLocker();

  // Acquires a lock on the lockfile which acts as a mutex between separate
  // processes. We use this to prevent race conditions when writing or consuming
  // the contents of the storage file. This should be called at the beginning
  // of a critical section.
  static int AcquireLock(const char* lock_filename);

  // Releases the lock on the lockfile. This should be called at the end of a
  // critical section.
  static void ReleaseLock(const char* lock_filename, int fd);

 private:
  const char* lock_filename_;
  int lock_file_descriptor_;
};

}  // namespace messaging
}  // namespace firebase

#endif  // FIREBASE_MESSAGING_CLIENT_CPP_SRC_ANDROID_CPP_MESSAGING_INTERNAL_H_
