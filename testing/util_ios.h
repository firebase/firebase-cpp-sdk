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

#ifndef FIREBASE_TESTING_CPPSDK_UTIL_IOS_H_
#define FIREBASE_TESTING_CPPSDK_UTIL_IOS_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

#include "testing/ticker_ios.h"
#include "testing/config_ios.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace testing {
namespace cppsdk {

typedef void (^ParamCallback)(id param, NSError* _Nullable error);
typedef void (^Callback)(NSError* _Nullable error);

// A test helper class to make tickers which call back at certain ticks.
class CallbackTicker : public firebase::testing::cppsdk::TickerObserver {
 public:
  explicit CallbackTicker(NSString* config_key, ParamCallback completion,
                          id param, int error_code);

  explicit CallbackTicker(NSString* config_key, Callback completion,
                          int error_code);

  void Elapse() override;

 private:
  // Initialize callback-type-independent members and return whether fake will succeed or throw
  // exceptions.
  bool Initialize(NSString* config_key);

  // The registered config key.
  NSString* key_;
  // ETA to call back.
  int64_t eta_;
  // Expected error.
  NSError* error_;
  // Whether has param in callback.
  const bool has_param_;
  // Expected param, if has, to call back with.
  id param_;

  // Error code to use for the ResultType when the config is set to raise an
  // exception.
  int error_code_;
  // The registered callback, either with or without parameter.
  const Callback completion_;
  const ParamCallback param_completion_;
};

// Manage and own callback tickers.
class CallbackTickerManager {
 public:
  // Add a callback ticker.
  // The error is API dependent so it's passed in with the config.
  // It gets set for the completion callback if the config is set to throw an
  // exception.
  void Add(NSString* config_key, ParamCallback completion, id param, int error);
  // Add a callback ticker.
  // The error is API dependent so it's passed in with the config.
  // It gets set for the completion callback if the config is set to throw an
  // exception.
  void Add(NSString* config_key, Callback completion, int error);

  // Deprecated.
  void Add(NSString* config_key, ParamCallback completion, id param);
  // Deprecated.
  void Add(NSString* config_key, Callback completion);

 private:
  std::vector<std::shared_ptr<CallbackTicker>> tickers_;
};
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

NS_ASSUME_NONNULL_END

#endif  // FIREBASE_TESTING_CPPSDK_UTIL_IOS_H_
