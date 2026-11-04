// Copyright 2022 Google LLC

#include "app/src/log.h"
#include "firebase/app_check/app_attest_provider.h"

namespace firebase::app_check {

AppAttestProviderFactory* AppAttestProviderFactory::GetInstance() {
  LogError("App Attest is not supported on this platform.");
  return nullptr;
}

AppAttestProviderFactory::AppAttestProviderFactory() : internal_(nullptr) {}
AppAttestProviderFactory::~AppAttestProviderFactory() = default;
AppCheckProvider* AppAttestProviderFactory::CreateProvider(App*) { return nullptr; }

}  // namespace firebase::app_check
