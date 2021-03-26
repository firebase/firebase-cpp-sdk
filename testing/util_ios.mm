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

#include "testing/util_ios.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace testing {
namespace cppsdk {

static const int DEPRECATED_DEFAULT_ERROR_VALUE = 100;

CallbackTicker::CallbackTicker(NSString* config_key, ParamCallback completion, id param,
                               int error_code)
    : key_(config_key),
      eta_(0),
      error_(nil),
      has_param_(true),
      param_(nil),
      error_code_(error_code),
      completion_(nil),
      param_completion_(completion) {
  const ConfigRow* row = ConfigGet([config_key UTF8String]);

  if (row == nullptr) {
    // Default behavior when no config is set.
    completion(param, nil);
    return;
  }

  if (Initialize(config_key)) {
    param_ = param;
  }
  RegisterTicker(this);
}

CallbackTicker::CallbackTicker(NSString* config_key, Callback completion, int error_code)
    : eta_(0),
      error_(nil),
      has_param_(false),
      param_(nil),
      error_code_(error_code),
      completion_(completion),
      param_completion_(nil) {
  const ConfigRow* row = ConfigGet([config_key UTF8String]);

  if (row == nullptr) {
    // Default behavior when no config is set.
    completion(nil);
    return;
  }

  Initialize(config_key);
  RegisterTicker(this);
}

void CallbackTicker::Elapse() {
  if (eta_ == TickerNow()) {
    if (has_param_) {
      param_completion_(param_, error_);
    } else {
      completion_(error_);
    }
  }
}

bool CallbackTicker::Initialize(NSString* config_key) {
  const ConfigRow* row = ConfigGet([config_key UTF8String]);

  eta_ = row->futuregeneric()->ticker();
  if (row->futuregeneric()->throwexception()) {
    NSString* errorMsg =
        [NSString stringWithUTF8String:row->futuregeneric()->exceptionmsg()->c_str()];
    error_ = [NSError errorWithDomain:config_key
                                 code:error_code_
                             userInfo:@{@"Error reason":errorMsg}];
    return false;
  } else {
    return true;
  }
}

void CallbackTickerManager::Add(NSString* config_key, ParamCallback completion, id param,
                                int error) {
  tickers_.push_back(
      std::shared_ptr<CallbackTicker>(new CallbackTicker(config_key, completion, param, error)));
}

void CallbackTickerManager::Add(NSString* config_key, Callback completion, int error) {
  tickers_.push_back(
      std::shared_ptr<CallbackTicker>(new CallbackTicker(config_key, completion, error)));
}


// Deprecated.
void CallbackTickerManager::Add(NSString* config_key, ParamCallback completion, id param) {
  tickers_.push_back(
      std::shared_ptr<CallbackTicker>(new CallbackTicker(config_key, completion, param,
                                                         DEPRECATED_DEFAULT_ERROR_VALUE)));
}

// Deprecated.
void CallbackTickerManager::Add(NSString* config_key, Callback completion) {
  tickers_.push_back(
      std::shared_ptr<CallbackTicker>(new CallbackTicker(config_key, completion,
                                                         DEPRECATED_DEFAULT_ERROR_VALUE)));
}
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

NS_ASSUME_NONNULL_END
