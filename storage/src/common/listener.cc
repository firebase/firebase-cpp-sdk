// Copyright 2016 Google LLC
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

#include "storage/src/include/firebase/storage/listener.h"

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/stub/listener_stub.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/listener_ios.h"
#else
#include "storage/src/desktop/listener_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase::storage {

Listener::Listener() : impl_(new internal::ListenerInternal(this)) {}

void Listener::Detach() {
  if (impl_) {
    impl_->Detach();
  }
}

// Non-inline implementation of Listener's virtual destructor
// to prevent its vtable being emitted in each translation unit.
//
// Detach() never invokes OnProgress()/OnPaused(), so calling it here (after
// the derived class has been destroyed) cannot cause a pure-virtual call. On
// every platform it only unbinds this Listener from in-flight operations and
// waits for user callbacks already running on *other* threads to return; a
// callback that destroys its own Listener does not wait for itself. Derived
// classes whose callbacks touch derived state should call Detach() at the
// start of their own destructor (see listener.h).
Listener::~Listener() {
  Detach();
  delete impl_;
}

}  // namespace firebase::storage
