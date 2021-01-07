/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_INFO_DESKTOP_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_INFO_DESKTOP_H_

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Gets the heartbeat code for a given SDK, updating the "last sent" time if
// necessary. See
// https://docs.google.com/document/d/10HSyILJ3l2XiDxuq_F3xlsSJpyLiYp3T9MkREXAuM6Q
// for further details.
//
// This class should only be used on desktop platforms. Mobile platforms should
// rely on the platform-specific implementation of the heartbeat to avoid
// double-counting.
class HeartbeatInfo {
 public:
  enum class Code { None = 0, Sdk = 1, Global = 2, Combined = 3 };

  // Gets the heartbeat code for the SDK identified by the given `tag`. If the
  // returned code is not `None`, the "last sent" time for the corresponding SDK
  // is updated (and persisted).
  static Code GetHeartbeatCode(const char* tag);
};

}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_HEARTBEAT_INFO_DESKTOP_H_
