// Copyright 2017 Google LLC
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

#import "instance_id/src_ios/fake/FIRInstanceID.h"

#include <cstdint>

#include <dispatch/dispatch.h>

#import <Foundation/Foundation.h>

#include "testing/reporter_impl.h"

static FIRInstanceIDErrorCode gNextErrorCode = kFIRInstanceIDErrorCodeNone;
static bool gBlockingEnabled = false;

static dispatch_semaphore_t gBlocking;
static dispatch_semaphore_t gThreadStarted;
static dispatch_semaphore_t gThreadComplete;

// Initialize the mock module.
void FIRInstanceIDInitialize() {
  gBlocking = dispatch_semaphore_create(0);
  gThreadStarted = dispatch_semaphore_create(0);
  gThreadComplete = dispatch_semaphore_create(0);
  FIRInstanceIDSetNextErrorCode(kFIRInstanceIDErrorCodeNone);
}

// Set the next error to be raised by the mock.
void FIRInstanceIDSetNextErrorCode(FIRInstanceIDErrorCode errorCode) {
  gNextErrorCode = errorCode;
}

// Retrieve the next error code and clear the current error code.
static FIRInstanceIDErrorCode GetAndClearErrorCode() {
  FIRInstanceIDErrorCode errorCode = gNextErrorCode;
  gNextErrorCode = kFIRInstanceIDErrorCodeNone;
  return errorCode;
}

// Wait 1 second while trying to acquire a semaphore, returning false on timeout.
static bool WaitForSemaphore(dispatch_semaphore_t semaphore) {
  static const int64_t kSemaphoreWaitTimeoutNanoseconds = 1000000000 /* 1s */;
  return dispatch_semaphore_wait(semaphore,
                                 dispatch_time(DISPATCH_TIME_NOW,
                                               kSemaphoreWaitTimeoutNanoseconds)) == 0;
}

// Enable / disable blocking on an asynchronous operation.
bool FIRInstanceIDSetBlockingMethodCallsEnable(bool enable) {
  bool stateChanged = gBlockingEnabled != enable;
  if (stateChanged) {
    if (enable) {
      gBlockingEnabled = enable;
    } else {
      gBlockingEnabled = enable;
      dispatch_semaphore_signal(gBlocking);
      if (!WaitForSemaphore(gThreadComplete)) return false;
    }
  }
  return true;
}

// Wait for an operation to start.
bool FIRInstanceIDWaitForBlockedThread() {
  return WaitForSemaphore(gThreadStarted);
}

// Run a block on a background thread.
static void RunBlockOnBackgroundThread(void (^block)(NSError* _Nullable error)) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      dispatch_semaphore_signal(gThreadStarted);
      int error_code = GetAndClearErrorCode();
      NSError * _Nullable error = nil;
      if (error_code != kFIRInstanceIDErrorCodeNone) {
        NSDictionary<id, id>* userInfo = @{
           NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Mock error code %d", error_code]
        };
        error = [NSError errorWithDomain:@"Mock error"
                                    code:error_code
                                userInfo:userInfo];
      }
      else if (gBlockingEnabled) {
        if (!WaitForSemaphore(gBlocking)) {
          error = [NSError errorWithDomain:@"Timeout"
                                      code:-1
                                  userInfo:nil];
        }
      }
      block(error);
      dispatch_semaphore_signal(gThreadComplete);
    });
}

@implementation FIRInstanceID

+ (instancetype)instanceID {
  if (GetAndClearErrorCode() != kFIRInstanceIDErrorCodeNone) return nil;
  FakeReporter->AddReport("FirebaseInstanceId.construct", {});
  return [[FIRInstanceID alloc] init];
}

- (NSString*)token {
  FakeReporter->AddReport("FirebaseInstanceId.getToken", {});
  return @"FakeToken";
}

- (void)tokenWithAuthorizedEntity:(nonnull NSString *)authorizedEntity
                            scope:(nonnull NSString *)scope
                          options:(nullable NSDictionary *)options
                          handler:(nonnull FIRInstanceIDTokenHandler)handler {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      if (!error) {
        FakeReporter->AddReport("FirebaseInstanceId.getToken",
                                { authorizedEntity.UTF8String, scope.UTF8String });
      }
      handler(error ? nil : @"FakeToken", error);
    });
}

- (void)deleteTokenWithAuthorizedEntity:(nonnull NSString *)authorizedEntity
                                  scope:(nonnull NSString *)scope
                                handler:(nonnull FIRInstanceIDDeleteTokenHandler)handler {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      if (!error) {
        FakeReporter->AddReport("FirebaseInstanceId.deleteToken",
                                { authorizedEntity.UTF8String, scope.UTF8String });
      }
      handler(error);
    });
}

- (void)getIDWithHandler:(nonnull FIRInstanceIDHandler)handler {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      if (!error) FakeReporter->AddReport("FirebaseInstanceId.getId", {});
      handler(error ? nil : @"FakeId", error);
    });
}

- (void)deleteIDWithHandler:(nonnull FIRInstanceIDDeleteHandler)handler {
  RunBlockOnBackgroundThread(^(NSError *_Nullable error) {
      if (!error) FakeReporter->AddReport("FirebaseInstanceId.deleteId", {});
      handler(error);
    });
}

@end
