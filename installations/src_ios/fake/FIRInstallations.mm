/*
 * Copyright 2020 Google LLC
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

#import "installations/src_ios/fake/FIRInstallations.h"
#import "installations/src_ios/fake/FIRInstallationsAuthTokenResult.h"

#include "testing/config.h"
#include "testing/reporter_impl.h"

namespace {

NSError* _Nullable FakeErrorForKey(const char* key) {
  const auto* config_row = firebase::testing::cppsdk::ConfigGet(key);
  if (config_row == nullptr || !config_row->future_generic_error()) {
    return nil;
  }
  return [NSError errorWithDomain:@"com.firebase.installations"
                             code:1
                         userInfo:@{
                           NSLocalizedDescriptionKey :
                               @"Fake installations error"
                         }];
}

// Run a block on a background thread.
void RunBlockOnBackgroundThread(const char* config_key,
                                void (^block)(NSError* _Nullable error)) {
  NSError* _Nullable error = FakeErrorForKey(config_key);
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        block(error);
      });
}

}  // namespace

@implementation FIRInstallations

+ (FIRInstallations*)installationsWithApp:(FIRApp*)app {
  (void)app;
  FakeReporter->AddReport("FirebaseInstallations.initialize", {});
  return [[FIRInstallations alloc] init];
}

- (void)installationIDWithCompletion:(FIRInstallationsIDHandler)completion {
  RunBlockOnBackgroundThread(
      "FirebaseInstallations.getId", ^(NSError* _Nullable error) {
        FakeReporter->AddReport("FirebaseInstallations.getId", {});
        completion(error ? nil : @"FakeId", error);
      });
}

- (void)authTokenWithCompletion:(FIRInstallationsTokenHandler)completion {
  [self authTokenForcingRefresh:NO completion:completion];
}

- (void)authTokenForcingRefresh:(BOOL)forceRefresh
                     completion:(FIRInstallationsTokenHandler)completion {
  RunBlockOnBackgroundThread(
      "FirebaseInstallations.getToken", ^(NSError* _Nullable error) {
        FakeReporter->AddReport("FirebaseInstallations.getToken", {});
        NSString* token =
            forceRefresh ? @"FakeTokenForceRefresh" : @"FakeToken";
        FIRInstallationsAuthTokenResult* result =
            [[FIRInstallationsAuthTokenResult alloc]
                 initWithToken:token
                expirationDate:[[NSDate alloc] init]];
        completion(error ? nil : result, error);
      });
}

- (void)deleteWithCompletion:(void (^)(NSError* __nullable error))completion {
  RunBlockOnBackgroundThread(
      "FirebaseInstallations.delete", ^(NSError* _Nullable error) {
        FakeReporter->AddReport("FirebaseInstallations.delete", {});
        completion(error);
      });
}

@end
