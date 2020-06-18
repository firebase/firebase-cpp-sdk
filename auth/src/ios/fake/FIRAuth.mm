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

#import "auth/src/ios/fake/FIRAuth.h"
#import "auth/src/ios/fake/FIRAuthErrors.h"
#import "auth/src/ios/fake/FIRAuthDataResult.h"
#import "auth/src/ios/fake/FIRAuthUIDelegate.h"
#import "auth/src/ios/fake/FIRUser.h"

#include <regex>
#include <string>
#include "testing/util_ios.h"

NS_ASSUME_NONNULL_BEGIN

NSString *const FIRAuthErrorUserInfoUpdatedCredentialKey =
    @"FIRAuthErrorUserInfoUpdatedCredentialKey";

@implementation FIRAuth {
  // Manages callbacks for testing.
  firebase::testing::cppsdk::CallbackTickerManager _callbackManager;
}

- (instancetype)init {
  return [super init];
}

+ (FIRAuth *)auth {
  return [[FIRAuth alloc] init];
}

+ (FIRAuth *)authWithApp:(FIRApp *)app {
  FIRAuth *result = [[FIRAuth alloc] init];
  return result;
}

static int AuthErrorFromConfig(const char *config_key) {
  const firebase::testing::cppsdk::ConfigRow *row =
      firebase::testing::cppsdk::ConfigGet(config_key);
  if (row != nullptr && row->futuregeneric()->throwexception()) {
    std::regex expression("^\\[.*[?!:]:?(.*)\\].*");
    std::smatch result;
    std::string search_str(row->futuregeneric()->exceptionmsg()->c_str());
    if (std::regex_search(search_str, result, expression)) {
      // The messages that throw errors should have:
      // "[AndroidNamedException:ERROR_FIREBASE_PROBLEM] <the rest of the message>".
      // result.str(1) contains the "ERROR_FIREBASE_PROBLEM" part.
      // The mapping between ios, android, and generic firebase errors is here:
      // https://docs.google.com/spreadsheets/d/1U5ESSHoc10Vd7sDoQO-CbbQ46_ThGol2lhViFs8Eg2g/
      std::string error_code = result.str(1);
      if (error_code == "ERROR_INVALID_CUSTOM_TOKEN") return FIRAuthErrorCodeInvalidCustomToken;
      if (error_code == "ERROR_INVALID_EMAIL") return FIRAuthErrorCodeInvalidEmail;
      if (error_code == "ERROR_OPERATION_NOT_ALLOWED") return FIRAuthErrorCodeOperationNotAllowed;
      if (error_code == "ERROR_WRONG_PASSWORD") return FIRAuthErrorCodeWrongPassword;
      if (error_code == "ERROR_EMAIL_ALREADY_IN_USE") return FIRAuthErrorCodeEmailAlreadyInUse;
      if (error_code == "ERROR_INVALID_MESSAGE_PAYLOAD")
        return FIRAuthErrorCodeInvalidMessagePayload;
    }
  }
  return -1;
}

- (void)updateCurrentUser:(FIRUser *)user completion:(nullable FIRUserUpdateCallback)completion {}

- (void)fetchProvidersForEmail:(NSString *)email {}

- (void)fetchProvidersForEmail:(NSString *)email
                    completion:(nullable FIRProviderQueryCallback)completion {}

- (void)fetchSignInMethodsForEmail:(NSString *)email
                        completion:(nullable FIRSignInMethodQueryCallback)completion {}

- (void)signInWithEmail:(NSString *)email
               password:(NSString *)password
             completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRAuth.signInWithEmail:password:completion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.signInWithEmail:password:completion:"));
}

- (void)signInWithCredential:(FIRAuthCredential *)credential
                  completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRAuth.signInWithCredential:completion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.signInWithCredential:completion:"));
}

- (void)signInAndRetrieveDataWithCredential:(FIRAuthCredential *)credential
                                 completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(
      @"FIRAuth.signInAndRetrieveDataWithCredential:completion:", completion,
      [[FIRAuthDataResult alloc] init],
      AuthErrorFromConfig("FIRAuth.signInAndRetrieveDataWithCredential:completion:"));
}

- (void)signInAnonymouslyWithCompletion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRAuth.signInAnonymouslyWithCompletion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.signInAnonymouslyWithCompletion:"));
}

- (void)signInWithCustomToken:(NSString *)token
                   completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRAuth.signInWithCustomToken:completion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.signInWithCustomToken:completion:"));
}

- (void)signInWithEmail:(NSString *)email
                   link:(NSString *)link
             completion:(nullable FIRAuthDataResultCallback)completion {}

- (void)signInWithProvider:(id<FIRFederatedAuthProvider>)provider
                UIDelegate:(nullable id<FIRAuthUIDelegate>)UIDelegate
                completion:(nullable FIRAuthDataResultCallback)completion {

  _callbackManager.Add(@"FIRAuth.signInWithProvider:completion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.signInWithProvider:completion:"));
}

- (void)createUserWithEmail:(NSString *)email
                   password:(NSString *)password
                 completion:(nullable FIRAuthDataResultCallback)completion {
  _callbackManager.Add(@"FIRAuth.createUserWithEmail:password:completion:", completion,
                       [[FIRAuthDataResult alloc] init],
                       AuthErrorFromConfig("FIRAuth.createUserWithEmail:password:completion:"));
}

- (void)confirmPasswordResetWithCode:(NSString *)code
                         newPassword:(NSString *)newPassword
                          completion:(FIRConfirmPasswordResetCallback)completion {}

- (void)checkActionCode:(NSString *)code completion:(FIRCheckActionCodeCallBack)completion {}

- (void)verifyPasswordResetCode:(NSString *)code
                     completion:(FIRVerifyPasswordResetCodeCallback)completion {}

- (void)applyActionCode:(NSString *)code
             completion:(FIRApplyActionCodeCallback)completion {}

- (void)sendPasswordResetWithEmail:(NSString *)email
                        completion:(nullable FIRSendPasswordResetCallback)completion {
  _callbackManager.Add(@"FIRAuth.sendPasswordResetWithEmail:completion:", completion,
                       AuthErrorFromConfig("FIRAuth.sendPasswordResetWithEmail:completion:"));
}

- (void)sendPasswordResetWithEmail:(NSString *)email
                 actionCodeSettings:(FIRActionCodeSettings *)actionCodeSettings
                         completion:(nullable FIRSendPasswordResetCallback)completion {}

- (void)sendSignInLinkToEmail:(NSString *)email
           actionCodeSettings:(FIRActionCodeSettings *)actionCodeSettings
                   completion:(nullable FIRSendSignInLinkToEmailCallback)completion {}

- (BOOL)signOut:(NSError *_Nullable *_Nullable)error {
  return YES;
}

- (BOOL)isSignInWithEmailLink:(NSString *)link {
  return YES;
}

- (FIRAuthStateDidChangeListenerHandle)addAuthStateDidChangeListener:
    (FIRAuthStateDidChangeListenerBlock)listener {
  return nil;
}

- (void)removeAuthStateDidChangeListener:(FIRAuthStateDidChangeListenerHandle)listenerHandle {}

- (FIRIDTokenDidChangeListenerHandle)addIDTokenDidChangeListener:
    (FIRIDTokenDidChangeListenerBlock)listener {
  return nil;
}

- (void)removeIDTokenDidChangeListener:(FIRIDTokenDidChangeListenerHandle)listenerHandle {}

- (void)useAppLanguage {}

- (BOOL)canHandleURL:(nonnull NSURL *)URL {
  return NO;
}

- (void)setAPNSToken:(NSData *)token type:(FIRAuthAPNSTokenType)type {}

- (BOOL)canHandleNotification:(NSDictionary *)userInfo {
  return NO;
}

- (BOOL)useUserAccessGroup:(NSString *_Nullable)accessGroup
                     error:(NSError *_Nullable *_Nullable)outError {
  return NO;
}

- (nullable FIRUser *)getStoredUserForAccessGroup:(NSString *_Nullable)accessGroup
                                            error:(NSError *_Nullable *_Nullable)outError {
  return [[FIRUser alloc] init];
}
@end

NS_ASSUME_NONNULL_END
