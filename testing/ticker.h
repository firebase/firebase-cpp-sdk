#ifndef FIREBASE_TESTING_CPPSDK_TICKER_H_
#define FIREBASE_TESTING_CPPSDK_TICKER_H_

namespace firebase {
namespace testing {
namespace cppsdk {

// Make one tick pass by.
void TickerElapse();

// Reset the current ticker.
void TickerReset();

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_TICKER_H_
