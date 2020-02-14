// Copyright 2020 Google
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

#include "testing/ticker_desktop.h"
#include "testing/ticker.h"

#include <vector>

namespace firebase {
namespace testing {
namespace cppsdk {

// The mimic time in integer.
static int64_t g_ticker = 0;

// The list of observers to notify when time elapses.
static std::vector<TickerObserver*>* g_observers = nullptr;

void RegisterTicker(TickerObserver* observer) {
  if (g_observers == nullptr) {
    g_observers = new std::vector<TickerObserver*>();
  }
  g_observers->push_back(observer);
  // This mimics the case that an update happens immediately.
  observer->Elapse();
}

int64_t TickerNow() { return g_ticker; }

TickerObserver::~TickerObserver() {
  if (g_observers == nullptr) {
    return;
  }
  for (auto iter = g_observers->begin(); iter != g_observers->end(); ++iter) {
    if (*iter == this) {
      g_observers->erase(iter);
      break;
    }
  }
}

void TickerElapse() {
  ++g_ticker;
  if (g_observers == nullptr) {
    return;
  }
  for (TickerObserver* observer : *g_observers) {
    observer->Elapse();
  }
}

void TickerReset() {
  g_ticker = 0;
  delete g_observers;
  g_observers = nullptr;
}

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
