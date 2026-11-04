// Copyright 2022 Google LLC

#include "app/src/log.h"
#include "firebase/app_check/play_integrity_provider.h"

namespace firebase::app_check {

PlayIntegrityProviderFactory* PlayIntegrityProviderFactory::GetInstance() {
  LogError("Play Integrity is not supported on this platform.");
  return nullptr;
}

PlayIntegrityProviderFactory::PlayIntegrityProviderFactory() = default;
PlayIntegrityProviderFactory::~PlayIntegrityProviderFactory() = default;
AppCheckProvider* PlayIntegrityProviderFactory::CreateProvider(App*) { return nullptr; }

}  // namespace firebase::app_check
