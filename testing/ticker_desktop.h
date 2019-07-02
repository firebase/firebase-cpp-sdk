#ifndef FIREBASE_TESTING_CPPSDK_TICKER_DESKTOP_H_
#define FIREBASE_TESTING_CPPSDK_TICKER_DESKTOP_H_

#include <cstdint>

namespace firebase {
namespace testing {
namespace cppsdk {

class TickerObserver {
 public:
  TickerObserver() {}
  virtual ~TickerObserver();

  virtual void Elapse() = 0;
};

void RegisterTicker(TickerObserver* observer);

int64_t TickerNow();

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_TICKER_DESKTOP_H_
