/*
 * Copyright 2025 Google LLC
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

#include "ai/src/common/http_sender.h"

#import <Foundation/Foundation.h>

#include <string>

// Delegate for incremental SSE streaming via NSURLSessionDataTask.
@interface FAISessionStreamDelegate : NSObject <NSURLSessionDataDelegate> {
 @private
  firebase::ai::internal::HttpStreamChunkCallback _onChunk;
  firebase::ai::internal::HttpCompletionCallback _onComplete;
  int _statusCode;
  NSMutableData* _errorData;
}

- (instancetype)
    initWithChunkCallback:(const firebase::ai::internal::HttpStreamChunkCallback&)onChunk
       completionCallback:(const firebase::ai::internal::HttpCompletionCallback&)onComplete;

@end

@implementation FAISessionStreamDelegate

- (instancetype)
    initWithChunkCallback:(const firebase::ai::internal::HttpStreamChunkCallback&)onChunk
       completionCallback:(const firebase::ai::internal::HttpCompletionCallback&)onComplete {
  self = [super init];
  if (self) {
    _onChunk = onChunk;
    _onComplete = onComplete;
    _statusCode = 0;
    _errorData = [[NSMutableData alloc] init];
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler {
  if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
    _statusCode = static_cast<int>(httpResponse.statusCode);
  }
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  if (!data || data.length == 0) return;
  if (_statusCode >= 200 && _statusCode < 300) {
    if (_onChunk) {
      bool keepGoing =
          _onChunk(reinterpret_cast<const char*>(data.bytes), static_cast<size_t>(data.length));
      if (!keepGoing) {
        [dataTask cancel];
      }
    }
  } else {
    [_errorData appendData:data];
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  std::string transportError;
  if (error) {
    if (error.code == NSURLErrorTimedOut) {
      transportError = "HTTP request timed out.";
    } else if (error.localizedDescription) {
      transportError = [error.localizedDescription UTF8String];
    } else {
      transportError = "NSURLSession request failed.";
    }
  }
  std::string bodyStr;
  if (_errorData && _errorData.length > 0) {
    bodyStr.assign(reinterpret_cast<const char*>(_errorData.bytes),
                   static_cast<size_t>(_errorData.length));
  }
  if (_onComplete) {
    _onComplete(_statusCode, bodyStr, transportError);
  }
  [session finishTasksAndInvalidate];
}

@end

namespace firebase {
namespace ai {
namespace internal {

namespace {

NSMutableURLRequest* BuildUrlRequest(const HttpRequest& request) {
  NSString* urlStr = [NSString stringWithUTF8String:request.url.c_str()];
  NSURL* url = [NSURL URLWithString:urlStr];
  NSMutableURLRequest* urlRequest = [NSMutableURLRequest requestWithURL:url];
  urlRequest.HTTPMethod = [NSString stringWithUTF8String:request.method.c_str()];
  if (request.timeout_ms > 0) {
    urlRequest.timeoutInterval = static_cast<NSTimeInterval>(request.timeout_ms) / 1000.0;
  }
  for (const auto& kv : request.headers) {
    NSString* headerName = [NSString stringWithUTF8String:kv.first.c_str()];
    NSString* headerVal = [NSString stringWithUTF8String:kv.second.c_str()];
    [urlRequest setValue:headerVal forHTTPHeaderField:headerName];
  }
  if (!request.body.empty()) {
    urlRequest.HTTPBody = [NSData dataWithBytes:request.body.data() length:request.body.size()];
  }
  return urlRequest;
}

}  // namespace

void HttpSender::Initialize() {}

void HttpSender::Cleanup() {}

void HttpSender::SendUnary(::firebase::App* /*app*/, const HttpRequest& request,
                           const HttpCompletionCallback& on_complete) {
  @autoreleasepool {
    NSMutableURLRequest* urlRequest = BuildUrlRequest(request);
    HttpCompletionCallback callbackCopy = on_complete;
    NSURLSessionDataTask* task = [[NSURLSession sharedSession]
        dataTaskWithRequest:urlRequest
          completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            int statusCode = 0;
            if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
              NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)response;
              statusCode = static_cast<int>(httpResp.statusCode);
            }
            std::string bodyStr;
            if (data && data.length > 0) {
              bodyStr.assign(reinterpret_cast<const char*>(data.bytes),
                             static_cast<size_t>(data.length));
            }
            std::string transportError;
            if (error) {
              if (error.code == NSURLErrorTimedOut) {
                transportError = "HTTP request timed out.";
              } else if (error.localizedDescription) {
                transportError = [error.localizedDescription UTF8String];
              } else {
                transportError = "NSURLSession request failed.";
              }
            }
            if (callbackCopy) {
              callbackCopy(statusCode, bodyStr, transportError);
            }
          }];
    [task resume];
  }
}

void HttpSender::SendStream(::firebase::App* /*app*/, const HttpRequest& request,
                            const HttpStreamChunkCallback& on_chunk,
                            const HttpCompletionCallback& on_complete) {
  @autoreleasepool {
    NSMutableURLRequest* urlRequest = BuildUrlRequest(request);
    FAISessionStreamDelegate* delegate =
        [[FAISessionStreamDelegate alloc] initWithChunkCallback:on_chunk
                                             completionCallback:on_complete];
    NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                          delegate:delegate
                                                     delegateQueue:nil];
    NSURLSessionDataTask* task = [session dataTaskWithRequest:urlRequest];
    [task resume];
  }
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
