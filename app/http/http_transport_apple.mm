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

#import <Foundation/Foundation.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/http/http_transport.h"

#if !__has_feature(objc_arc)
#error "http_transport_apple.mm requires Objective-C ARC (-fobjc-arc)."
#endif

@class FIRFirebaseHttpStreamBridge;
@class FIRFirebaseHttpSessionDelegate;

namespace firebase {
namespace http {
namespace {

NSString* ToNSString(std::string_view s) {
  NSString* ns = [[NSString alloc] initWithBytes:s.data()
                                          length:s.size()
                                        encoding:NSUTF8StringEncoding];
  if (ns == nil) {
    ns = [[NSString alloc] initWithBytes:s.data()
                                  length:s.size()
                                encoding:NSISOLatin1StringEncoding];
  }
  return ns != nil ? ns : @"";
}

std::string FromNSString(NSString* ns) {
  if (ns == nil) {
    return std::string();
  }
  const char* utf8 = [ns UTF8String];
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

std::string ToLowerAscii(std::string_view s) {
  std::string out(s.size(), '\0');
  for (size_t i = 0; i < s.size(); ++i) {
    out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
  }
  return out;
}

std::string CanonicalReasonPhrase(int32_t status) {
  switch (status) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 206:
      return "Partial Content";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 303:
      return "See Other";
    case 304:
      return "Not Modified";
    case 307:
      return "Temporary Redirect";
    case 308:
      return "Permanent Redirect";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 408:
      return "Request Timeout";
    case 409:
      return "Conflict";
    case 410:
      return "Gone";
    case 412:
      return "Precondition Failed";
    case 413:
      return "Payload Too Large";
    case 416:
      return "Range Not Satisfiable";
    case 429:
      return "Too Many Requests";
    case 500:
      return "Internal Server Error";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return FromNSString([NSHTTPURLResponse localizedStringForStatusCode:status]);
  }
}

HttpErrorClass ClassifyNSError(NSError* error) {
  NSError* current = error;
  for (int depth = 0; current != nil && depth < 6; ++depth) {
    NSString* domain = current.domain;
    NSInteger code = current.code;
    if ([domain isEqualToString:NSURLErrorDomain]) {
      switch (code) {
        case NSURLErrorCancelled:
          return kHttpErrorCanceled;
        case NSURLErrorTimedOut:
          return kHttpErrorTimeout;
        case NSURLErrorSecureConnectionFailed:
        case NSURLErrorServerCertificateHasBadDate:
        case NSURLErrorServerCertificateUntrusted:
        case NSURLErrorServerCertificateHasUnknownRoot:
        case NSURLErrorServerCertificateNotYetValid:
        case NSURLErrorClientCertificateRejected:
        case NSURLErrorClientCertificateRequired:
        case NSURLErrorAppTransportSecurityRequiresSecureConnection:
          return kHttpErrorTls;
        case NSURLErrorCannotFindHost:
        case NSURLErrorCannotConnectToHost:
        case NSURLErrorNetworkConnectionLost:
        case NSURLErrorDNSLookupFailed:
        case NSURLErrorNotConnectedToInternet:
        case NSURLErrorInternationalRoamingOff:
        case NSURLErrorCallIsActive:
        case NSURLErrorDataNotAllowed:
          return kHttpErrorConnect;
        default:
          break;
      }
    } else if ([domain isEqualToString:@"kCFErrorDomainCFNetwork"]) {
      if (code <= -1200 && code >= -1206) {
        return kHttpErrorTls;
      }
      if (code == -1001) {
        return kHttpErrorTimeout;
      }
    } else if ([domain isEqualToString:NSPOSIXErrorDomain]) {
      return kHttpErrorConnect;
    }

    id underlying = current.userInfo[NSUnderlyingErrorKey];
    if ([underlying isKindOfClass:[NSError class]]) {
      current = static_cast<NSError*>(underlying);
    } else {
      break;
    }
  }
  return kHttpErrorOther;
}

struct AppleSessionSlot {
  NSURLSession* session = nil;
  FIRFirebaseHttpSessionDelegate* delegate = nil;
  bool pooled = false;
};

struct AppleTransferState {
  std::shared_ptr<HttpOperation> op;
  std::shared_ptr<AppleSessionSlot> session_slot;
  NSURLSessionDataTask* task = nil;
  NSOutputStream* body_output_stream = nil;
  FIRFirebaseHttpStreamBridge* stream_bridge = nil;
  bool task_started = false;
  bool task_suspended = false;
  bool body_closed = false;
  bool slot_released = false;
  int redirect_count = 0;
  std::atomic<bool> any_body_bytes_popped{false};
  std::vector<uint8_t> pending_write_chunk;
  size_t pending_write_offset = 0;
  std::mutex state_mutex;
};

void CancelAppleTransferTask(const std::shared_ptr<AppleTransferState>& state);
void ReleaseSessionSlot(const std::shared_ptr<AppleSessionSlot>& slot, bool cancel_now);

}  // namespace
}  // namespace http
}  // namespace firebase

@interface FIRFirebaseHttpStreamBridge : NSObject <NSStreamDelegate>
- (instancetype)initWithState:(std::shared_ptr<firebase::http::AppleTransferState>)state;
- (void)scheduleOutputStream:(NSOutputStream*)outputStream;
- (void)pumpWriteStream;
- (void)closeWriteStream;
@end

@implementation FIRFirebaseHttpStreamBridge {
  std::shared_ptr<firebase::http::AppleTransferState> _state;
  NSOutputStream* _ownedStream;
  // NSStream.delegate is an unsafe_unretained (assign) property. Retain `self`
  // explicitly from `scheduleOutputStream:` until `closeOnStreamThread` clears
  // `stream.delegate = nil` and removes `stream` from the run loop.
  FIRFirebaseHttpStreamBridge* _selfRetain;
}

+ (NSThread*)streamThread {
  static NSThread* sThread = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    dispatch_semaphore_t ready = dispatch_semaphore_create(0);
    sThread = [[NSThread alloc] initWithTarget:self
                                      selector:@selector(streamThreadMain:)
                                        object:ready];
    sThread.name = @"com.google.firebase.http.stream";
    sThread.qualityOfService = NSQualityOfServiceUserInitiated;
    [sThread start];
    // Wait until `streamThreadMain:` has created its NSRunLoop and attached its
    // keep-alive port so the first `performSelector:onThread:` is never dropped.
    dispatch_semaphore_wait(ready, DISPATCH_TIME_FOREVER);
  });
  return sThread;
}

+ (void)streamThreadMain:(id)readySemaphore {
  @autoreleasepool {
    NSRunLoop* runLoop = [NSRunLoop currentRunLoop];
    [runLoop addPort:[NSMachPort port] forMode:NSDefaultRunLoopMode];
    if (readySemaphore != nil) {
      dispatch_semaphore_signal(static_cast<dispatch_semaphore_t>(readySemaphore));
    }
    while (true) {
      @autoreleasepool {
        [runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
      }
    }
  }
}

- (instancetype)initWithState:(std::shared_ptr<firebase::http::AppleTransferState>)state {
  self = [super init];
  if (self != nil) {
    _state = std::move(state);
  }
  return self;
}

- (void)scheduleOutputStream:(NSOutputStream*)outputStream {
  _ownedStream = outputStream;
  _selfRetain = self;
  [self performSelector:@selector(openOnStreamThread:)
               onThread:[FIRFirebaseHttpStreamBridge streamThread]
             withObject:outputStream
          waitUntilDone:NO];
}

- (void)openOnStreamThread:(NSOutputStream*)outputStream {
  auto state = _state;
  if (!state || state->op->IsCanceled()) {
    [self closeOnStreamThread];
    return;
  }
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    active = !state->body_closed && state->body_output_stream == outputStream;
  }
  if (!active) {
    outputStream.delegate = nil;
    [outputStream close];
    _ownedStream = nil;
    _state.reset();
    _selfRetain = nil;
    return;
  }
  outputStream.delegate = self;
  [outputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  [outputStream open];
  [self pumpOnStreamThread];
}

- (void)pumpWriteStream {
  [self performSelector:@selector(pumpOnStreamThread)
               onThread:[FIRFirebaseHttpStreamBridge streamThread]
             withObject:nil
          waitUntilDone:NO];
}

- (void)closeWriteStream {
  [self performSelector:@selector(closeOnStreamThread)
               onThread:[FIRFirebaseHttpStreamBridge streamThread]
             withObject:nil
          waitUntilDone:NO];
}

- (void)closeOnStreamThread {
  auto state = _state;
  NSOutputStream* stream = _ownedStream;
  _ownedStream = nil;
  if (state) {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    if (state->stream_bridge == self) {
      state->stream_bridge = nil;
    }
    if (state->body_output_stream == stream) {
      state->body_closed = true;
      state->body_output_stream = nil;
    }
  }
  if (stream != nil) {
    stream.delegate = nil;
    [stream close];
    [stream removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  }
  _state.reset();
  _selfRetain = nil;
}

- (void)pumpOnStreamThread {
  auto state = _state;
  if (!state || state->op->IsCanceled()) {
    [self closeOnStreamThread];
    return;
  }

  NSOutputStream* stream = nil;
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    if (state->body_closed || state->body_output_stream != _ownedStream || _ownedStream == nil) {
      return;
    }
    stream = _ownedStream;
  }

  constexpr size_t kSliceSize = 64 * 1024;
  while ([stream hasSpaceAvailable]) {
    if (state->pending_write_offset >= state->pending_write_chunk.size()) {
      state->pending_write_chunk.resize(kSliceSize);
      state->pending_write_offset = 0;
      bool eof = false;
      size_t popped =
          state->op->PopRequestBytes(state->pending_write_chunk.data(), kSliceSize, &eof);
      state->pending_write_chunk.resize(popped);
      if (popped > 0) {
        state->any_body_bytes_popped.store(true, std::memory_order_release);
      } else {
        if (eof) {
          [self closeOnStreamThread];
        }
        return;
      }
    }

    const uint8_t* ptr = state->pending_write_chunk.data() + state->pending_write_offset;
    NSUInteger rem =
        static_cast<NSUInteger>(state->pending_write_chunk.size() - state->pending_write_offset);
    NSInteger written = [stream write:ptr maxLength:rem];
    if (written > 0) {
      state->pending_write_offset += static_cast<size_t>(written);
    } else if (written < 0) {
      // NSURLSession closes the bound input stream early when the server sends
      // an immediate 4xx/5xx (such as 413) or a 301-303 redirect to GET. Do
      // NOT call MarkFailed or cancel the task here; close the upload pipe and
      // wake any blocked caller so NSURLSession can deliver the valid HTTP
      // response or report the real transport NSError via didCompleteWithError.
      state->op->MarkRequestWriteClosed();
      [self closeOnStreamThread];
      return;
    } else {
      break;
    }
  }
}

- (void)stream:(NSStream*)aStream handleEvent:(NSStreamEvent)eventCode {
  switch (eventCode) {
    case NSStreamEventHasSpaceAvailable:
      [self pumpOnStreamThread];
      break;
    case NSStreamEventErrorOccurred:
    case NSStreamEventEndEncountered: {
      auto state = _state;
      if (state) {
        state->op->MarkRequestWriteClosed();
      }
      [self closeOnStreamThread];
      break;
    }
    default:
      break;
  }
}

@end

@interface FIRFirebaseHttpSessionDelegate : NSObject <NSURLSessionDataDelegate>
- (void)registerState:(std::shared_ptr<firebase::http::AppleTransferState>)state
           forTaskKey:(NSString*)key;
- (std::shared_ptr<firebase::http::AppleTransferState>)takeState:(NSURLSessionTask*)task;
@end

@implementation FIRFirebaseHttpSessionDelegate {
  std::mutex _mapMutex;
  std::unordered_map<std::string, std::shared_ptr<firebase::http::AppleTransferState>> _tasks;
}

- (std::shared_ptr<firebase::http::AppleTransferState>)findState:(NSURLSessionTask*)task {
  NSString* desc = task.taskDescription;
  if (desc == nil) {
    return nullptr;
  }
  std::string key = firebase::http::FromNSString(desc);
  std::lock_guard<std::mutex> lock(_mapMutex);
  auto it = _tasks.find(key);
  return it != _tasks.end() ? it->second : nullptr;
}

- (std::shared_ptr<firebase::http::AppleTransferState>)takeState:(NSURLSessionTask*)task {
  NSString* desc = task.taskDescription;
  if (desc == nil) {
    return nullptr;
  }
  std::string key = firebase::http::FromNSString(desc);
  std::lock_guard<std::mutex> lock(_mapMutex);
  auto it = _tasks.find(key);
  if (it == _tasks.end()) {
    return nullptr;
  }
  auto state = std::move(it->second);
  _tasks.erase(it);
  return state;
}

- (void)registerState:(std::shared_ptr<firebase::http::AppleTransferState>)state
           forTaskKey:(NSString*)key {
  std::string k = firebase::http::FromNSString(key);
  std::lock_guard<std::mutex> lock(_mapMutex);
  _tasks[k] = std::move(state);
}

- (void)URLSession:(NSURLSession*)session
                 task:(NSURLSessionTask*)task
    needNewBodyStream:(void (^)(NSInputStream* _Nullable bodyStream))completionHandler {
  auto state = [self findState:task];
  if (!state || state->op->IsCanceled()) {
    completionHandler(nil);
    return;
  }

  if (state->any_body_bytes_popped.load(std::memory_order_acquire)) {
    bool is_redirect = false;
    {
      std::lock_guard<std::mutex> lock(state->state_mutex);
      is_redirect = state->redirect_count > 0;
    }
    state->op->MarkFailed(is_redirect ? "Cannot replay streamed request body across redirect."
                                      : "Cannot replay streamed request body.",
                          firebase::http::kHttpErrorOther);
    [task cancel];
    completionHandler(nil);
    return;
  }

  // No bytes were consumed from the upload pipe yet (for example NSURLSession
  // retrying a request on a stale keep-alive connection before sending body
  // bytes). Supply a fresh bound stream pair.
  NSInputStream* in_stream = nil;
  NSOutputStream* out_stream = nil;
  [NSStream getBoundStreamsWithBufferSize:firebase::http::kMaxRequestBufferBytes
                              inputStream:&in_stream
                             outputStream:&out_stream];
  FIRFirebaseHttpStreamBridge* old_bridge = nil;
  FIRFirebaseHttpStreamBridge* new_bridge =
      [[FIRFirebaseHttpStreamBridge alloc] initWithState:state];
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    old_bridge = state->stream_bridge;
    state->body_closed = false;
    state->body_output_stream = out_stream;
    state->stream_bridge = new_bridge;
  }
  if (old_bridge != nil) {
    [old_bridge closeWriteStream];
  }
  [new_bridge scheduleOutputStream:out_stream];
  completionHandler(in_stream);
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest* _Nullable))handler {
  auto state = [self findState:task];
  int32_t status = static_cast<int32_t>(response.statusCode);
  if (state) {
    bool exceeded = false;
    {
      std::lock_guard<std::mutex> lock(state->state_mutex);
      if (state->redirect_count >= firebase::http::kMaxRedirects) {
        exceeded = true;
      } else {
        ++state->redirect_count;
      }
    }
    if (exceeded) {
      state->op->MarkFailed("Maximum HTTP redirects exceeded.", firebase::http::kHttpErrorOther);
      [task cancel];
      handler(nil);
      return;
    }
  }

  std::string orig_url = firebase::http::FromNSString(task.originalRequest.URL.absoluteString);
  NSString* prev_ns =
      response.URL != nil ? response.URL.absoluteString : task.currentRequest.URL.absoluteString;
  std::string prev_url = firebase::http::FromNSString(prev_ns);

  // Resolve Location through the shared RFC 3986 redirect validator so userinfo
  // rejection, spaces, non-HTTP(S) schemes, and HTTPS -> HTTP downgrades match
  // Desktop and Android identically.
  NSString* raw_location = nil;
  for (id rawKey in response.allHeaderFields) {
    if ([rawKey isKindOfClass:[NSString class]] &&
        firebase::http::AsciiEqualsIgnoreCase(
            firebase::http::FromNSString(static_cast<NSString*>(rawKey)), "Location")) {
      id val = response.allHeaderFields[rawKey];
      if ([val isKindOfClass:[NSString class]]) {
        raw_location = static_cast<NSString*>(val);
      }
      break;
    }
  }

  std::string next_url;
  if (raw_location != nil) {
    auto resolved =
        firebase::http::ResolveRedirectUrl(prev_url, firebase::http::FromNSString(raw_location));
    if (!resolved) {
      if (state) {
        state->op->MarkFailed("Blocked unsafe or scheme-downgraded HTTP redirect.",
                              firebase::http::kHttpErrorOther);
      }
      [task cancel];
      handler(nil);
      return;
    }
    next_url = std::move(*resolved);
  } else {
    next_url = firebase::http::FromNSString(request.URL.absoluteString);
  }

  bool was_https = firebase::http::IsHttpsUrl(orig_url) || firebase::http::IsHttpsUrl(prev_url);
  if (!firebase::http::IsHttpOrHttpsUrl(next_url) ||
      (was_https && !firebase::http::IsHttpsUrl(next_url))) {
    // Fail the operation and cancel the task on non-HTTP(S) schemes or
    // HTTPS -> HTTP downgrades; passing handler(nil) alone would complete the
    // task with the raw 3xx response as success.
    if (state) {
      state->op->MarkFailed("Blocked unsafe or scheme-downgraded HTTP redirect.",
                            firebase::http::kHttpErrorOther);
    }
    [task cancel];
    handler(nil);
    return;
  }

  std::string cur_method = firebase::http::FromNSString(task.currentRequest.HTTPMethod);
  bool has_streamed_body =
      state && (state->op->content_length > 0 ||
                state->op->content_length == firebase::http::kChunkedRequestContent);
  bool rewrites_to_get = (status == 301 || status == 302 || status == 303) &&
                         !firebase::http::AsciiEqualsIgnoreCase(cur_method, "GET") &&
                         !firebase::http::AsciiEqualsIgnoreCase(cur_method, "HEAD");

  if (!rewrites_to_get && (status == 307 || status == 308) && has_streamed_body) {
    if (state) {
      state->op->MarkFailed("Cannot replay streamed request body across redirect.",
                            firebase::http::kHttpErrorOther);
    }
    [task cancel];
    handler(nil);
    return;
  }

  NSMutableURLRequest* next_req = [request mutableCopy];
  NSURL* validated_ns_url = [NSURL URLWithString:firebase::http::ToNSString(next_url)];
  if (validated_ns_url != nil) {
    next_req.URL = validated_ns_url;
  }

  if (rewrites_to_get) {
    next_req.HTTPMethod = @"GET";
    next_req.HTTPBody = nil;
    next_req.HTTPBodyStream = nil;
    [next_req setValue:nil forHTTPHeaderField:@"Content-Type"];
    [next_req setValue:nil forHTTPHeaderField:@"Content-Length"];
    [next_req setValue:nil forHTTPHeaderField:@"Transfer-Encoding"];
    if (state) {
      state->op->MarkRequestWriteClosed();
      FIRFirebaseHttpStreamBridge* bridge = nil;
      {
        std::lock_guard<std::mutex> lock(state->state_mutex);
        bridge = state->stream_bridge;
        state->stream_bridge = nil;
      }
      if (bridge != nil) {
        [bridge closeWriteStream];
      }
    }
  } else if (status == 303 && firebase::http::AsciiEqualsIgnoreCase(cur_method, "HEAD")) {
    next_req.HTTPMethod = @"HEAD";
    next_req.HTTPBody = nil;
    next_req.HTTPBodyStream = nil;
  }

  if (!firebase::http::IsSameOrigin(orig_url, next_url) ||
      !firebase::http::IsSameOrigin(prev_url, next_url)) {
    NSDictionary<NSString*, NSString*>* fields = [next_req.allHTTPHeaderFields copy];
    for (NSString* key in fields) {
      if (firebase::http::IsSensitiveRedirectHeader(firebase::http::FromNSString(key)) ||
          firebase::http::AsciiEqualsIgnoreCase(firebase::http::FromNSString(key), "Host")) {
        [next_req setValue:nil forHTTPHeaderField:key];
      }
    }
    [next_req setValue:nil forHTTPHeaderField:@"Authorization"];
    [next_req setValue:nil forHTTPHeaderField:@"Proxy-Authorization"];
    [next_req setValue:nil forHTTPHeaderField:@"Cookie"];
    [next_req setValue:nil forHTTPHeaderField:@"X-Firebase-AppCheck"];
    [next_req setValue:nil forHTTPHeaderField:@"x-goog-api-key"];
    handler(next_req);
    return;
  }

  // CFNetwork strips the reserved Authorization header when constructing
  // `newRequest` even on same-origin redirects. Copy it from `currentRequest`
  // (never `originalRequest`, so a credential stripped on an earlier
  // cross-origin hop can never be resurrected on a later bounce back).
  NSString* cur_auth = [task.currentRequest valueForHTTPHeaderField:@"Authorization"];
  if (cur_auth != nil && [next_req valueForHTTPHeaderField:@"Authorization"] == nil) {
    [next_req setValue:cur_auth forHTTPHeaderField:@"Authorization"];
  }

  handler(next_req);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
  auto state = [self findState:dataTask];
  if (!state || state->op->IsCanceled()) {
    completionHandler(NSURLSessionResponseCancel);
    return;
  }

  if (![response isKindOfClass:[NSHTTPURLResponse class]]) {
    state->op->MarkFailed("Unexpected non-HTTP URLResponse.", firebase::http::kHttpErrorOther);
    completionHandler(NSURLSessionResponseCancel);
    return;
  }

  auto* httpResp = static_cast<NSHTTPURLResponse*>(response);
  int32_t status = static_cast<int32_t>(httpResp.statusCode);
  std::string reason = firebase::http::CanonicalReasonPhrase(status);
  std::vector<std::pair<std::string, std::string>> headers;
  NSDictionary* fields = httpResp.allHeaderFields;
  for (id rawKey in fields) {
    id rawVal = [fields objectForKey:rawKey];
    if ([rawKey isKindOfClass:[NSString class]] && [rawVal isKindOfClass:[NSString class]]) {
      headers.emplace_back(firebase::http::FromNSString(static_cast<NSString*>(rawKey)),
                           firebase::http::FromNSString(static_cast<NSString*>(rawVal)));
    }
  }

  state->op->CommitResponseHeaders(status, reason, std::move(headers));
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  auto state = [self findState:dataTask];
  if (!state || state->op->IsCanceled() || data == nil || data.length == 0) {
    return;
  }

  // Push incoming bytes in bounded slices and enforce true socket backpressure:
  // `[dataTask suspend]` alone is asynchronous in CFNetwork and can allow tens
  // of MiB to enqueue before taking effect. Because each active task runs on
  // its own serial NSOperationQueue (from AppleSessionPool), suspending the
  // task and waiting on `op->resp_cv` here bounds both `resp_buffered_bytes`
  // and process resident memory without head-of-line blocking other concurrent
  // requests.
  constexpr size_t kSliceBytes = 256 * 1024;
  [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange byteRange, BOOL* stop) {
    const auto* ptr = static_cast<const uint8_t*>(bytes);
    size_t remaining = static_cast<size_t>(byteRange.length);
    size_t offset = 0;
    while (remaining > 0) {
      if (state->op->IsCanceled()) {
        *stop = YES;
        return;
      }
      size_t take = (std::min)(remaining, kSliceBytes);
      bool over_high_water = state->op->PushResponseBytes(ptr + offset, take);
      offset += take;
      remaining -= take;

      if (over_high_water && !state->op->IsCanceled()) {
        bool did_suspend = false;
        {
          std::lock_guard<std::mutex> slock(state->state_mutex);
          if (!state->task_suspended && !state->op->IsCanceled() && state->task != nil) {
            state->task_suspended = true;
            [state->task suspend];
            did_suspend = true;
          }
        }
        {
          std::unique_lock<std::mutex> op_lock(state->op->mutex);
          state->op->resp_cv.wait(op_lock, [&state]() {
            return state->op->resp_buffered_bytes <= firebase::http::kLowWaterResponseBytes ||
                   state->op->IsCanceled() || state->op->failed;
          });
        }
        if (did_suspend) {
          std::lock_guard<std::mutex> slock(state->state_mutex);
          if (state->task_suspended && state->task != nil) {
            state->task_suspended = false;
            [state->task resume];
          }
        }
      }
    }
  }];
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  auto state = [self takeState:task];
  if (!state) {
    return;
  }
  FIRFirebaseHttpStreamBridge* bridge = nil;
  std::shared_ptr<firebase::http::AppleSessionSlot> slot_to_release;
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    state->task = nil;
    state->task_suspended = false;
    bridge = state->stream_bridge;
    state->stream_bridge = nil;
    if (!state->slot_released) {
      state->slot_released = true;
      slot_to_release = std::move(state->session_slot);
    }
  }
  if (bridge != nil) {
    [bridge closeWriteStream];
  }
  if (slot_to_release) {
    firebase::http::ReleaseSessionSlot(slot_to_release, /*cancel_now=*/false);
  }
  if (state->op->IsCanceled()) {
    return;
  }
  if (error != nil) {
    firebase::http::HttpErrorClass err_class = firebase::http::ClassifyNSError(error);
    state->op->MarkFailed(firebase::http::FromNSString([error localizedDescription]), err_class);
  } else {
    state->op->MarkResponseComplete();
  }
}

@end

namespace firebase {
namespace http {
namespace {

NSURLSessionConfiguration* MakeBaseSessionConfig() {
  NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
  config.URLCache = nil;
  config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
  config.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
  config.HTTPShouldSetCookies = NO;
  config.HTTPMaximumConnectionsPerHost = 16;
  // NSURLSession does not expose a connect-only timeout separate from the idle
  // read timeout (`timeoutIntervalForRequest`). Keep these high so idle SSE
  // streams are never killed; TCP connect relies on the OS SYN timeout, and
  // caller CancellationTokens / watchdogs govern request lifetimes.
  config.timeoutIntervalForRequest = 86400.0;
  config.timeoutIntervalForResource = 86400.0;
  return config;
}

std::shared_ptr<AppleSessionSlot> CreateSessionSlot(NSURLSessionConfiguration* config,
                                                    bool pooled) {
  auto slot = std::make_shared<AppleSessionSlot>();
  slot->pooled = pooled;
  slot->delegate = [[FIRFirebaseHttpSessionDelegate alloc] init];
  NSOperationQueue* queue = [[NSOperationQueue alloc] init];
  queue.name = @"com.google.firebase.http.delegate";
  queue.maxConcurrentOperationCount = 1;
  queue.qualityOfService = NSQualityOfServiceUserInitiated;
  slot->session = [NSURLSession sessionWithConfiguration:config
                                                delegate:slot->delegate
                                           delegateQueue:queue];
  return slot;
}

// Pool of reusable NSURLSessions (each with its own serial NSOperationQueue) so
// at most one in-flight task occupies a delegate queue at any time. This gives:
//   - HTTP/2 and TLS keep-alive reuse across up to 16 concurrent connections.
//   - True bounded download memory via blocking in `didReceiveData:` without
//     head-of-line blocking other concurrent transfers or SSE streams.
//   - Zero session leaks when >16 requests overlap (overflow slots are
//     invalidated cleanly on completion).
class AppleSessionPool {
 public:
  static AppleSessionPool& Instance() {
    static auto* pool = new AppleSessionPool();
    return *pool;
  }

  std::shared_ptr<AppleSessionSlot> AcquireDefault() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!idle_.empty()) {
      auto slot = std::move(idle_.back());
      idle_.pop_back();
      return slot;
    }
    if (created_pooled_ < kMaxPooledSessions) {
      ++created_pooled_;
      return CreateSessionSlot(MakeBaseSessionConfig(), /*pooled=*/true);
    }
    return CreateSessionSlot(MakeBaseSessionConfig(), /*pooled=*/false);
  }

  void Release(const std::shared_ptr<AppleSessionSlot>& slot, bool cancel_now) {
    if (!slot) {
      return;
    }
    if (slot->pooled) {
      std::lock_guard<std::mutex> lock(mutex_);
      idle_.push_back(slot);
      return;
    }
    if (slot->session != nil) {
      if (cancel_now) {
        [slot->session invalidateAndCancel];
      } else {
        [slot->session finishTasksAndInvalidate];
      }
      slot->session = nil;
    }
  }

 private:
  static constexpr size_t kMaxPooledSessions = 16;
  std::mutex mutex_;
  std::vector<std::shared_ptr<AppleSessionSlot>> idle_;
  size_t created_pooled_ = 0;
};

void ReleaseSessionSlot(const std::shared_ptr<AppleSessionSlot>& slot, bool cancel_now) {
  AppleSessionPool::Instance().Release(slot, cancel_now);
}

void CancelAppleTransferTask(const std::shared_ptr<AppleTransferState>& state) {
  if (!state) {
    return;
  }
  FIRFirebaseHttpStreamBridge* bridge = nil;
  NSURLSessionDataTask* t = nil;
  FIRFirebaseHttpSessionDelegate* del = nil;
  std::shared_ptr<AppleSessionSlot> unstarted_slot;
  bool was_suspended = false;
  bool was_started = false;
  {
    std::lock_guard<std::mutex> slock(state->state_mutex);
    bridge = state->stream_bridge;
    state->stream_bridge = nil;
    t = state->task;
    was_suspended = state->task_suspended;
    was_started = state->task_started;
    state->task_suspended = false;
    state->task = nil;
    if (state->session_slot) {
      del = state->session_slot->delegate;
      if (!was_started && !state->slot_released) {
        state->slot_released = true;
        unstarted_slot = std::move(state->session_slot);
      }
    }
  }
  if (bridge != nil) {
    [bridge closeWriteStream];
  }
  if (t != nil) {
    if (!was_started && del != nil) {
      // NSURLSession does not fire `didCompleteWithError:` for a task canceled
      // before its initial `resume`; remove from `_tasks` here to avoid a leak.
      [del takeState:t];
    }
    // Balance any active suspend before cancelling; NSURLSession asserts if a
    // task is cancelled or deallocated with suspend > 0.
    if (was_suspended) {
      [t resume];
    }
    [t cancel];
  }
  if (unstarted_slot) {
    ReleaseSessionSlot(unstarted_slot, /*cancel_now=*/true);
  }
}

}  // namespace

void StartPlatformHttpOperation(const std::shared_ptr<HttpOperation>& op) {
  @autoreleasepool {
    if (op->IsCanceled()) {
      return;
    }
    if (!IsHttpOrHttpsUrl(op->url)) {
      op->MarkFailed("Unsupported URI scheme; only http and https are allowed.",
                     kHttpErrorOther);
      return;
    }

    bool is_get = AsciiEqualsIgnoreCase(op->method, "GET");
    bool is_head = AsciiEqualsIgnoreCase(op->method, "HEAD");
    if ((is_get || is_head) &&
        (op->content_length > 0 || op->content_length == kChunkedRequestContent)) {
      op->MarkFailed("GET and HEAD requests cannot send a request body.", kHttpErrorOther);
      return;
    }

    NSURL* ns_url = [NSURL URLWithString:ToNSString(op->url)];
    if (ns_url == nil) {
      op->MarkFailed("Invalid request URL.", kHttpErrorOther);
      return;
    }

    // Validate proxy URL upfront and never silently fall back to direct.
    std::shared_ptr<AppleSessionSlot> slot;
    if (!op->proxy_url.empty()) {
      auto proxy_hp = ParseProxyHostPort(op->proxy_url);
      if (!proxy_hp) {
        op->MarkFailed("Invalid or unsupported proxy URL.", kHttpErrorOther);
        return;
      }
      NSString* host = ToNSString(proxy_hp->first);
      NSNumber* port = @(proxy_hp->second);
      NSURLSessionConfiguration* config = MakeBaseSessionConfig();
      config.connectionProxyDictionary = @{
        @"HTTPEnable" : @YES,
        @"HTTPProxy" : host,
        @"HTTPPort" : port,
        @"HTTPSEnable" : @YES,
        @"HTTPSProxy" : host,
        @"HTTPSPort" : port,
      };
      slot = CreateSessionSlot(config, /*pooled=*/false);
    } else {
      slot = AppleSessionPool::Instance().AcquireDefault();
    }

    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:ns_url];
    req.HTTPMethod = ToNSString(op->method);
    req.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    req.HTTPShouldHandleCookies = NO;

    bool has_content_length = false;
    bool has_accept_encoding = false;
    std::unordered_set<std::string> seen_headers;
    for (const auto& kv : op->request_headers) {
      if (AsciiEqualsIgnoreCase(kv.first, "Content-Length")) {
        has_content_length = true;
      } else if (AsciiEqualsIgnoreCase(kv.first, "Accept-Encoding")) {
        has_accept_encoding = true;
      }
      NSString* field = ToNSString(kv.first);
      NSString* value = ToNSString(kv.second);
      if (seen_headers.insert(ToLowerAscii(kv.first)).second) {
        [req setValue:value forHTTPHeaderField:field];
      } else {
        [req addValue:value forHTTPHeaderField:field];
      }
    }
    if (!has_accept_encoding) {
      // Match .NET HttpClientHandler (AutomaticDecompression = None) and
      // Desktop libcurl so NSURLSession does not inject `Accept-Encoding: gzip`,
      // strip Content-Length, buffer SSE frames, or corrupt Range offsets.
      [req setValue:@"identity" forHTTPHeaderField:@"Accept-Encoding"];
    }

    auto state = std::make_shared<AppleTransferState>();
    state->op = op;
    state->session_slot = slot;

    FIRFirebaseHttpStreamBridge* stream_bridge = nil;
    NSOutputStream* out_stream = nil;
    if (op->content_length == 0 && !is_get && !is_head) {
      req.HTTPBody = [NSData data];
      if (!has_content_length) {
        [req setValue:@"0" forHTTPHeaderField:@"Content-Length"];
      }
    } else if (op->content_length > 0 || op->content_length == kChunkedRequestContent) {
      NSInputStream* in_stream = nil;
      [NSStream getBoundStreamsWithBufferSize:kMaxRequestBufferBytes
                                  inputStream:&in_stream
                                 outputStream:&out_stream];
      req.HTTPBodyStream = in_stream;
      if (op->content_length > 0 && !has_content_length) {
        [req setValue:[NSString
                          stringWithFormat:@"%lld", static_cast<long long>(op->content_length)]
            forHTTPHeaderField:@"Content-Length"];
      }
      stream_bridge = [[FIRFirebaseHttpStreamBridge alloc] initWithState:state];
      state->body_output_stream = out_stream;
      state->stream_bridge = stream_bridge;
    } else if (!has_content_length && (AsciiEqualsIgnoreCase(op->method, "POST") ||
                                       AsciiEqualsIgnoreCase(op->method, "PUT") ||
                                       AsciiEqualsIgnoreCase(op->method, "PATCH"))) {
      req.HTTPBody = [NSData data];
      [req setValue:@"0" forHTTPHeaderField:@"Content-Length"];
    }

    NSURLSessionDataTask* task = [slot->session dataTaskWithRequest:req];
    NSString* key = [NSString stringWithFormat:@"%lld", static_cast<long long>(op->id)];
    task.taskDescription = key;
    {
      // Write `state->task` under `state_mutex` before registering or starting
      // the stream thread (P4).
      std::lock_guard<std::mutex> slock(state->state_mutex);
      state->task = task;
    }

    [slot->delegate registerState:state forTaskKey:key];

    bool already_canceled = false;
    {
      std::weak_ptr<AppleTransferState> weak_state = state;
      std::lock_guard<std::mutex> lock(op->mutex);
      if (op->IsCanceled()) {
        already_canceled = true;
      } else {
        if (stream_bridge != nil) {
          op->on_request_data_changed = [weak_state]() {
            auto locked = weak_state.lock();
            if (!locked) {
              return;
            }
            FIRFirebaseHttpStreamBridge* bridge = nil;
            {
              std::lock_guard<std::mutex> slock(locked->state_mutex);
              bridge = locked->stream_bridge;
            }
            if (bridge != nil) {
              [bridge pumpWriteStream];
            }
          };
        }
        op->on_cancel = [weak_state]() { CancelAppleTransferTask(weak_state.lock()); };
      }
    }

    if (already_canceled) {
      [slot->delegate takeState:task];
      std::shared_ptr<AppleSessionSlot> to_release;
      {
        std::lock_guard<std::mutex> slock(state->state_mutex);
        state->stream_bridge = nil;
        state->body_output_stream = nil;
        state->body_closed = true;
        state->task = nil;
        if (!state->slot_released) {
          state->slot_released = true;
          to_release = std::move(state->session_slot);
        }
      }
      [task cancel];
      if (to_release) {
        ReleaseSessionSlot(to_release, /*cancel_now=*/true);
      }
      return;
    }

    // Coordinate initial `[task resume]` with `CancelAppleTransferTask` under
    // `state_mutex` so a cancel racing before `resume` cannot leave a never-
    // resumed task stranded in `_tasks` or leak a session slot.
    bool should_cleanup_unstarted = false;
    bool started_now = false;
    std::shared_ptr<AppleSessionSlot> unstarted_release;
    {
      std::lock_guard<std::mutex> slock(state->state_mutex);
      if (state->task != nil && !op->IsCanceled()) {
        state->task_started = true;
        started_now = true;
        [task resume];
      } else if (state->task != nil) {
        state->task = nil;
        state->stream_bridge = nil;
        state->body_output_stream = nil;
        state->body_closed = true;
        should_cleanup_unstarted = true;
        if (!state->slot_released) {
          state->slot_released = true;
          unstarted_release = std::move(state->session_slot);
        }
      }
    }
    if (started_now && stream_bridge != nil && out_stream != nil) {
      [stream_bridge scheduleOutputStream:out_stream];
    }
    if (should_cleanup_unstarted) {
      [slot->delegate takeState:task];
      [task cancel];
      if (unstarted_release) {
        ReleaseSessionSlot(unstarted_release, /*cancel_now=*/true);
      }
    }
  }
}

}  // namespace http
}  // namespace firebase
