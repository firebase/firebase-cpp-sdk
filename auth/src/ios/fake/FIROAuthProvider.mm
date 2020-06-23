/*
 * Copyright 2017 Google
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

#import "auth/src/ios/fake/FIROAuthProvider.h"

#import "auth/src/ios/fake/FIRAuthCredential.h"
#import "auth/src/ios/fake/FIROAuthCredential.h"

NS_ASSUME_NONNULL_BEGIN

@implementation FIROAuthProvider

- (void)getCredentialWithUIDelegate:(nullable id<FIRAuthUIDelegate>)UIDelegate
                         completion:(nullable FIRAuthCredentialCallback)completion {}

+ (FIROAuthProvider *)providerWithProviderID:(NSString *)providerID {
  return [[FIROAuthProvider alloc] initWithProviderID:providerID];
}

+ (FIROAuthProvider *)providerWithProviderID:(NSString *)providerID
                                        auth:(FIRAuth *)auth {
  return [[FIROAuthProvider alloc] initWithProviderID:providerID];
}

+ (FIROAuthCredential *)credentialWithProviderID:(NSString *)providerID
                                        IDToken:(NSString *)IDToken
                                    accessToken:(nullable NSString *)accessToken {
  return [[FIROAuthCredential alloc] initWithProvider:providerID];
}

+ (FIROAuthCredential *)credentialWithProviderID:(NSString *)providerID
                                     accessToken:(NSString *)accessToken {
  return [[FIROAuthCredential alloc] initWithProvider:providerID];
}

+ (FIROAuthCredential *)credentialWithProviderID:(NSString *)providerID
                                         IDToken:(NSString *)IDToken
                                        rawNonce:(nullable NSString *)rawNonce
                                     accessToken:(nullable NSString *)accessToken {
  return [[FIROAuthCredential alloc] initWithProvider:providerID];
}

#pragma mark - Internal Methods

- (instancetype)initWithProviderID:(NSString *)providerID {
  self = [super init];
  if (self) {
    _providerID = providerID;
  }
  return self;
}

@end

NS_ASSUME_NONNULL_END
