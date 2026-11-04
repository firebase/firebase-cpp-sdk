// Copyright 2016 Google LLC
#include "storage/src/include/firebase/storage/listener.h"

#include "app/src/include/firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/stub/listener_stub.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/listener_ios.h"
#else
#include "storage/src/desktop/listener_desktop.h"
#endif

namespace firebase::storage {

Listener::Listener() : impl_(new internal::ListenerInternal(this)) {}
void Listener::Detach() {
  if (impl_) impl_->Detach();
}
Listener::~Listener() {
  Detach();
  delete impl_;
}

}  // namespace firebase::storage
