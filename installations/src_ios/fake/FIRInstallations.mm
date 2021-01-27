/*
 * Copyright 2020 Google
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

#include "testing/reporter_impl.h"

static dispatch_semaphore_t gThreadStarted;
static dispatch_semaphore_t gThreadComplete;

// Wait 1 second while trying to acquire a semaphore, returning false on timeout.
static bool WaitForSemaphore(dispatch_semaphore_t semaphore) {
  static const int64_t kSemaphoreWaitTimeoutNanoseconds = 1000000000 /* 1s */;
  return dispatch_semaphore_wait(semaphore,
                                 dispatch_time(DISPATCH_TIME_NOW,
                                               kSemaphoreWaitTimeoutNanoseconds)) == 0;
}

// Run a block on a background thread.
static void RunBlockOnBackgroundThread(void (^block)(NSError* _Nullable error)) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      dispatch_semaphore_signal(gThreadStarted);
      NSError * _Nullable error = nil;
      block(error);
      dispatch_semaphore_signal(gThreadComplete);
    });
}

@implementation FIRInstallations

+ (FIRInstallations *)installationsWithApp:(FIRApp *)app {
  FakeReporter->AddReport("FirebaseInstallations.initialize", {});
  return [[FIRInstallations alloc] init];
}

- (void)installationIDWithCompletion:(FIRInstallationsIDHandler)completion {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      FakeReporter->AddReport("FirebaseInstallations.getId", {});
      completion(error ? nil : @"FakeId", error);
    });
}

- (void)authTokenWithCompletion:(FIRInstallationsTokenHandler)completion {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      FakeReporter->AddReport("FirebaseInstallations.getToken", {});
      FIRInstallationsAuthTokenResult *result = [[FIRInstallationsAuthTokenResult alloc]
             initWithToken:@"FakeToken"
            expirationDate:[NSDate init]];
      completion(error ? nil : result, error);
    });
}

- (void)authTokenForcingRefresh:(BOOL)forceRefresh
                     completion:(FIRInstallationsTokenHandler)completion {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      FakeReporter->AddReport("FirebaseInstallations.getToken", {});
      FIRInstallationsAuthTokenResult *result = [[FIRInstallationsAuthTokenResult alloc]
             initWithToken:@"FakeTokenForceRefresh"
            expirationDate:[NSDate init]];
      completion(error ? nil : result, error);
    });
}

- (void)deleteWithCompletion:(void (^)(NSError *__nullable error))completion {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      if (!error) FakeReporter->AddReport("FirebaseInstallations.delete", {});
      completion(error);
    });
}

@end
