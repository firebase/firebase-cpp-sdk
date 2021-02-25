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

#include "testing/config_ios.h"
#include "testing/ticker_ios.h"
#include "testing/util_ios.h"

#import "auth/src/ios/fake/FIRUser.h"

#import "auth/src/ios/fake/FIRUserMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@implementation FIRUser {
  // Manages callbacks for testing.
  firebase::testing::cppsdk::CallbackTickerManager _callbackManager;
}

// Properties from protocol need to be synthesized explicitly.
@synthesize providerID = _providerID;
@synthesize uid = _uid;
@synthesize displayName = _displayName;
@synthesize photoURL = _photoURL;
@synthesize email = _email;
@synthesize phoneNumber = _phoneNumber;

- (instancetype)init {
  self = [super init];
  if (self) {
    _anonymous = YES;
    _providerID = @"fake provider id";
    _uid = @"fake uid";
    _displayName = @"fake display name";
    _email = @"fake email";
    _phoneNumber = @"fake phone number";
    _metadata = [[FIRUserMetadata alloc] init];
  }
  return self;
}

- (void)updateEmail:(NSString *)email
         completion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager.Add(@"FIRUser.updateEmail:completion:",
                       ^(NSError *_Nullable error) {
                         _email = email;
                         completion(error);
                       });
}

- (void)updatePassword:(NSString *)password
            completion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager.Add(@"FIRUser.updatePassword:completion:", completion);
}

- (void)updatePhoneNumberCredential:(FIRPhoneAuthCredential *)phoneNumberCredential
                         completion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager.Add(@"FIRUser.updatePhoneNumberCredential:completion:", completion);
}

- (FIRUserProfileChangeRequest *)profileChangeRequest {
  return [[FIRUserProfileChangeRequest alloc] initWithCallbackManager:&_callbackManager];
}

- (void)reloadWithCompletion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager.Add(@"FIRUser.reloadWithCompletion:", completion);
}

- (void)reauthenticateWithCredential:(FIRAuthCredential *)credential
                          completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRUser.reauthenticateWithCredential:completion:", completion,
                       [[FIRAuthDataResult alloc] init]);
}

- (void)
      reauthenticateAndRetrieveDataWithCredential:(FIRAuthCredential *) credential
                                       completion:(nullable FIRAuthDataResultCallback) completion {
  _callbackManager.Add(@"FIRUser.reauthenticateAndRetrieveDataWithCredential:completion:",
                       completion, [[FIRAuthDataResult alloc] init]);
}

- (void)getIDTokenResultWithCompletion:(nullable FIRAuthTokenResultCallback)completion {}

- (void)getIDTokenResultForcingRefresh:(BOOL)forceRefresh
                            completion:(nullable FIRAuthTokenResultCallback)completion {}

- (void)getIDTokenForcingRefresh:(BOOL)forceRefresh
                      completion:(nullable FIRAuthTokenCallback)completion {
  _callbackManager.Add(@"FIRUser.getIDTokenForcingRefresh:completion:", completion,
                       @"a fake token");
}

- (void)getIDTokenWithCompletion:(nullable FIRAuthTokenCallback)completion {}

- (void)linkWithCredential:(FIRAuthCredential *)credential
                completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRUser.linkWithCredential:completion:", completion,
                       [[FIRAuthDataResult alloc] init]);
}

- (void)sendEmailVerificationWithCompletion:
    (nullable FIRSendEmailVerificationCallback)completion {
  _callbackManager.Add(@"FIRUser.sendEmailVerificationWithCompletion:", completion);
}

- (void)linkAndRetrieveDataWithCredential:(FIRAuthCredential *) credential
                               completion:(nullable FIRAuthDataResultCallback) completion {
  _callbackManager.Add(@"FIRUser.linkAndRetrieveDataWithCredential:completion:", completion,
                       [[FIRAuthDataResult alloc] init]);
}

- (void)unlinkFromProvider:(NSString *)provider
                completion:(nullable FIRAuthResultCallback)completion {
  _callbackManager.Add(@"FIRUser.unlinkFromProvider:completion:", completion,
                       [[FIRUser alloc] init]);
}

- (void)sendEmailVerificationWithActionCodeSettings:(FIRActionCodeSettings *)actionCodeSettings
                                         completion:(nullable FIRSendEmailVerificationCallback)
                                                    completion {
}

- (void)deleteWithCompletion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager.Add(@"FIRUser.deleteWithCompletion:", completion);
}

@end

@implementation FIRUserProfileChangeRequest {
  // Manages callbacks for testing. Does not own it.
  firebase::testing::cppsdk::CallbackTickerManager *_callbackManager;
}

- (instancetype)
      initWithCallbackManager:(firebase::testing::cppsdk::CallbackTickerManager *)callbackManager {
  self = [super init];
  if (self) {
    _callbackManager = callbackManager;
  }
  return self;
}

- (void)commitChangesWithCompletion:(nullable FIRUserProfileChangeCallback)completion {
  _callbackManager->Add(@"FIRUserProfileChangeRequest.commitChangesWithCompletion:", completion);
}

@end

NS_ASSUME_NONNULL_END
