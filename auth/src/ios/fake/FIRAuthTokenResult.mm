/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import "auth/src/ios/fake/FIRAuthTokenResult.h"

NS_ASSUME_NONNULL_BEGIN

@implementation FIRAuthTokenResult

- (instancetype)init {
  self = [super init];
  if (self) {
    _token = @"a fake token";
    _expirationDate = [NSDate dateWithTimeIntervalSince1970:1];
    _authDate = [NSDate dateWithTimeIntervalSince1970:1];
    _issuedAtDate = [NSDate dateWithTimeIntervalSince1970:1];
    _signInProvider = @"custom";
    _signInSecondFactor = @"totp";
    _claims = @{};
  }
  return self;
}

@end

NS_ASSUME_NONNULL_END
