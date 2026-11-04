// Copyright 2022 Google LLC

#include "app/src/log.h"
#include "firebase/app_check/device_check_provider.h"

namespace firebase::app_check {

DeviceCheckProviderFactory* DeviceCheckProviderFactory::GetInstance() {
  LogError("Device Check is not supported on this platform.");
  return nullptr;
}

DeviceCheckProviderFactory::DeviceCheckProviderFactory() = default;
DeviceCheckProviderFactory::~DeviceCheckProviderFactory() = default;
AppCheckProvider* DeviceCheckProviderFactory::CreateProvider(App*) { return nullptr; }

}  // namespace firebase::app_check
