// Copyright 2021 Google LLC

#include <string>

#include "app/src/include/firebase/app.h"
#include "performance/src/include/firebase/performance.h"

firebase::InitResult firebase::performance::Initialize(
    const firebase::App& app) {
  return kInitResultSuccess;
}

void firebase::performance::Terminate() {}

bool firebase::performance::GetPerformanceCollectionEnabled() { return true; }

void firebase::performance::SetPerformanceCollectionEnabled(bool enabled) {}
