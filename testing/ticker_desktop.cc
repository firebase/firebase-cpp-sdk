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
