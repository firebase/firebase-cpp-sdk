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

struct AppleTransferState {
  std::shared_ptr<HttpOperation> op;
  NSURLSessionDataTask* task = nil;
  NSOutputStream* body_output_stream = nil;
  FIRFirebaseHttpStreamBridge* stream_bridge = nil;
  bool task_started = false;
  bool task_suspended = false;
  bool body_closed = false;
  int redirect_count = 0;
  std::vector<uint8_t> pending_write_chunk;
  size_t pending_write_offset = 0;
  std::mutex state_mutex;
};

void CancelAppleTransferTask(const std::shared_ptr<AppleTransferState>& state);

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
  _selfRetain = self;
  [self performSelector:@selector(openOnStreamThread:)
               onThread:[FIRFirebaseHttpStreamBridge streamThread]
             withObject:outputStream
          waitUntilDone:NO];
}

- (void)openOnStreamThread:(NSOutputStream*)outputStream {
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
  if (!state) {
    _selfRetain = nil;
    return;
  }
  NSOutputStream* stream = nil;
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    state->stream_bridge = nil;
    if (!state->body_closed && state->body_output_stream != nil) {
      state->body_closed = true;
      stream = state->body_output_stream;
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
    if (state->body_closed || state->body_output_stream == nil) {
      return;
    }
    stream = state->body_output_stream;
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
      if (popped == 0) {
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
      NSError* err = [stream streamError];
      std::string msg = err != nil ? firebase::http::FromNSString([err localizedDescription])
                                   : "Request body stream write failed.";
      // Record the stream error before cancelling the task so an asynchronous
      // `didCompleteWithError:NSURLErrorCancelled` cannot overwrite `msg`.
      state->op->MarkFailed(msg);
      firebase::http::CancelAppleTransferTask(state);
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
    case NSStreamEventErrorOccurred: {
      NSError* err = [aStream streamError];
      std::string msg = err != nil ? firebase::http::FromNSString([err localizedDescription])
                                   : "Request body stream error.";
      auto state = _state;
      if (state) {
        state->op->MarkFailed(msg);
        firebase::http::CancelAppleTransferTask(state);
      }
      [self closeOnStreamThread];
      break;
    }
    case NSStreamEventEndEncountered:
      [self closeOnStreamThread];
      break;
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
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest* _Nullable))handler {
  auto state = [self findState:task];
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
      state->op->MarkFailed("Maximum HTTP redirects exceeded.");
      [task cancel];
      handler(nil);
      return;
    }
  }

  std::string orig_url = firebase::http::FromNSString(task.originalRequest.URL.absoluteString);
  NSString* prev_ns =
      response.URL != nil ? response.URL.absoluteString : task.currentRequest.URL.absoluteString;
  std::string prev_url = firebase::http::FromNSString(prev_ns);
  std::string next_url = firebase::http::FromNSString(request.URL.absoluteString);
  bool was_https = firebase::http::IsHttpsUrl(orig_url) || firebase::http::IsHttpsUrl(prev_url);
  if (!firebase::http::IsHttpOrHttpsUrl(next_url) ||
      (was_https && !firebase::http::IsHttpsUrl(next_url))) {
    // Fail the operation and cancel the task on non-HTTP(S) schemes or
    // HTTPS -> HTTP downgrades; passing handler(nil) alone would complete the
    // task with the raw 3xx response as success.
    if (state) {
      state->op->MarkFailed("Blocked unsafe or scheme-downgraded HTTP redirect.");
    }
    [task cancel];
    handler(nil);
    return;
  }

  if (!firebase::http::IsSameOrigin(orig_url, next_url) ||
      !firebase::http::IsSameOrigin(prev_url, next_url)) {
    NSMutableURLRequest* sanitized = [request mutableCopy];
    [sanitized setValue:nil forHTTPHeaderField:@"Authorization"];
    [sanitized setValue:nil forHTTPHeaderField:@"Proxy-Authorization"];
    [sanitized setValue:nil forHTTPHeaderField:@"X-Firebase-AppCheck"];
    [sanitized setValue:nil forHTTPHeaderField:@"x-goog-api-key"];
    handler(sanitized);
    return;
  }

  // CFNetwork strips the reserved Authorization header when constructing
  // `newRequest` even on same-origin redirects. Copy it from `currentRequest`
  // (never `originalRequest`, so a credential stripped on an earlier
  // cross-origin hop can never be resurrected on a later bounce back).
  NSString* cur_auth = [task.currentRequest valueForHTTPHeaderField:@"Authorization"];
  if (cur_auth != nil && [request valueForHTTPHeaderField:@"Authorization"] == nil) {
    NSMutableURLRequest* preserved = [request mutableCopy];
    [preserved setValue:cur_auth forHTTPHeaderField:@"Authorization"];
    handler(preserved);
    return;
  }

  handler(request);
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
    state->op->MarkFailed("Unexpected non-HTTP URLResponse.");
    completionHandler(NSURLSessionResponseCancel);
    return;
  }

  auto* httpResp = static_cast<NSHTTPURLResponse*>(response);
  int32_t status = static_cast<int32_t>(httpResp.statusCode);
  std::string reason = firebase::http::FromNSString(
      [NSHTTPURLResponse localizedStringForStatusCode:httpResp.statusCode]);
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

  __block bool over_high_water = false;
  [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange byteRange, BOOL* stop) {
    if (byteRange.length > 0) {
      if (state->op->PushResponseBytes(static_cast<const uint8_t*>(bytes),
                                       static_cast<size_t>(byteRange.length))) {
        over_high_water = true;
      }
    }
  }];

  if (over_high_water) {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    if (!state->task_suspended && !state->op->IsCanceled() && state->task != nil) {
      // Re-verify buffered byte count under `op->mutex` while holding
      // `state_mutex` (`state_mutex -> op->mutex` lock order) so a concurrent
      // consumer read draining the buffer to 0 cannot race past
      // `task_suspended` and leave the task suspended permanently.
      bool still_above_low_water = false;
      {
        std::lock_guard<std::mutex> op_lock(state->op->mutex);
        still_above_low_water =
            state->op->resp_buffered_bytes > firebase::http::kLowWaterResponseBytes;
      }
      if (still_above_low_water) {
        state->task_suspended = true;
        [state->task suspend];
      }
    }
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  auto state = [self takeState:task];
  if (!state) {
    return;
  }
  FIRFirebaseHttpStreamBridge* bridge = nil;
  {
    std::lock_guard<std::mutex> lock(state->state_mutex);
    state->task = nil;
    state->task_suspended = false;
    bridge = state->stream_bridge;
    state->stream_bridge = nil;
  }
  if (bridge != nil) {
    [bridge closeWriteStream];
  }
  if (state->op->IsCanceled()) {
    return;
  }
  if (error != nil) {
    state->op->MarkFailed(firebase::http::FromNSString([error localizedDescription]));
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
  // Timeouts are governed by the caller's CancellationToken / watchdog so
  // long-lived SSE streams are never prematurely killed while idle.
  config.timeoutIntervalForRequest = 86400.0;
  config.timeoutIntervalForResource = 86400.0;
  return config;
}

FIRFirebaseHttpSessionDelegate* SharedSessionDelegate() {
  static FIRFirebaseHttpSessionDelegate* sDelegate = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sDelegate = [[FIRFirebaseHttpSessionDelegate alloc] init];
  });
  return sDelegate;
}

NSOperationQueue* SharedDelegateQueue() {
  static NSOperationQueue* sQueue = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sQueue = [[NSOperationQueue alloc] init];
    sQueue.name = @"com.google.firebase.http.delegate";
    // NSURLSession requires a serial delegate queue to preserve ordering
    // between `didReceiveResponse:`, `didReceiveData:`, and
    // `didCompleteWithError:` callbacks.
    sQueue.maxConcurrentOperationCount = 1;
    sQueue.qualityOfService = NSQualityOfServiceUserInitiated;
  });
  return sQueue;
}

void CancelAppleTransferTask(const std::shared_ptr<AppleTransferState>& state) {
  if (!state) {
    return;
  }
  FIRFirebaseHttpStreamBridge* bridge = nil;
  NSURLSessionDataTask* t = nil;
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
  }
  if (bridge != nil) {
    [bridge closeWriteStream];
  }
  if (t != nil) {
    if (!was_started) {
      // NSURLSession does not fire `didCompleteWithError:` for a task canceled
      // before its initial `resume`; remove from `_tasks` here to avoid a leak.
      [SharedSessionDelegate() takeState:t];
    }
    // Balance any active suspend before cancelling; NSURLSession asserts if a
    // task is cancelled or deallocated with suspend > 0.
    if (was_suspended) {
      [t resume];
    }
    [t cancel];
  }
}

NSURLSession* SharedDefaultSession() {
  static NSURLSession* sSession = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sSession = [NSURLSession sessionWithConfiguration:MakeBaseSessionConfig()
                                             delegate:SharedSessionDelegate()
                                        delegateQueue:SharedDelegateQueue()];
  });
  return sSession;
}

NSURLSession* SessionForProxy(std::string_view proxy_url) {
  auto proxy_hp = ParseProxyHostPort(proxy_url);
  if (!proxy_hp) {
    return SharedDefaultSession();
  }

  static auto* proxy_mutex = new std::mutex();
  static auto* proxy_sessions = new std::unordered_map<std::string, NSURLSession*>();
  static auto* proxy_lru = new std::deque<std::string>();
  constexpr size_t kMaxCachedProxySessions = 8;

  std::string cache_key = proxy_hp->first + ":" + std::to_string(proxy_hp->second);
  NSURLSession* evicted = nil;
  NSURLSession* result = nil;
  {
    std::lock_guard<std::mutex> lock(*proxy_mutex);
    auto it = proxy_sessions->find(cache_key);
    if (it != proxy_sessions->end()) {
      return it->second;
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
    result = [NSURLSession sessionWithConfiguration:config
                                           delegate:SharedSessionDelegate()
                                      delegateQueue:SharedDelegateQueue()];
    if (proxy_sessions->size() >= kMaxCachedProxySessions && !proxy_lru->empty()) {
      std::string oldest = std::move(proxy_lru->front());
      proxy_lru->pop_front();
      auto old_it = proxy_sessions->find(oldest);
      if (old_it != proxy_sessions->end()) {
        evicted = old_it->second;
        proxy_sessions->erase(old_it);
      }
    }
    proxy_sessions->emplace(cache_key, result);
    proxy_lru->push_back(std::move(cache_key));
  }
  if (evicted != nil && evicted != SharedDefaultSession()) {
    [evicted finishTasksAndInvalidate];
  }
  return result;
}

}  // namespace

void StartPlatformHttpOperation(const std::shared_ptr<HttpOperation>& op) {
  @autoreleasepool {
    if (op->IsCanceled()) {
      return;
    }
    if (!IsHttpOrHttpsUrl(op->url)) {
      op->MarkFailed("Unsupported URI scheme; only http and https are allowed.");
      return;
    }

    NSURL* ns_url = [NSURL URLWithString:ToNSString(op->url)];
    if (ns_url == nil) {
      op->MarkFailed("Invalid request URL.");
      return;
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

    FIRFirebaseHttpStreamBridge* stream_bridge = nil;
    if (op->content_length == 0) {
      req.HTTPBody = [NSData data];
      if (!has_content_length) {
        [req setValue:@"0" forHTTPHeaderField:@"Content-Length"];
      }
    } else if (op->content_length > 0 || op->content_length == kChunkedRequestContent) {
      NSInputStream* in_stream = nil;
      NSOutputStream* out_stream = nil;
      [NSStream getBoundStreamsWithBufferSize:kMaxRequestBufferBytes
                                  inputStream:&in_stream
                                 outputStream:&out_stream];
      req.HTTPBodyStream = in_stream;
      if (op->content_length > 0 && !has_content_length) {
        [req setValue:[NSString
                          stringWithFormat:@"%lld", static_cast<long long>(op->content_length)]
            forHTTPHeaderField:@"Content-Length"];
      }
      state->body_output_stream = out_stream;
      stream_bridge = [[FIRFirebaseHttpStreamBridge alloc] initWithState:state];
      state->stream_bridge = stream_bridge;
      [stream_bridge scheduleOutputStream:out_stream];
    } else if (!has_content_length && (AsciiEqualsIgnoreCase(op->method, "POST") ||
                                       AsciiEqualsIgnoreCase(op->method, "PUT") ||
                                       AsciiEqualsIgnoreCase(op->method, "PATCH"))) {
      req.HTTPBody = [NSData data];
      [req setValue:@"0" forHTTPHeaderField:@"Content-Length"];
    }

    NSURLSession* session = SessionForProxy(op->proxy_url);
    NSURLSessionDataTask* task = [session dataTaskWithRequest:req];
    NSString* key = [NSString stringWithFormat:@"%lld", static_cast<long long>(op->id)];
    task.taskDescription = key;
    state->task = task;

    [SharedSessionDelegate() registerState:state forTaskKey:key];

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
        op->on_response_space_available = [weak_state]() {
          auto locked = weak_state.lock();
          if (!locked) {
            return;
          }
          std::lock_guard<std::mutex> slock(locked->state_mutex);
          if (locked->task_suspended && locked->task != nil && !locked->op->IsCanceled()) {
            locked->task_suspended = false;
            [locked->task resume];
          }
        };
        op->on_cancel = [weak_state]() { CancelAppleTransferTask(weak_state.lock()); };
      }
    }

    if (already_canceled) {
      [SharedSessionDelegate() takeState:task];
      {
        std::lock_guard<std::mutex> slock(state->state_mutex);
        state->stream_bridge = nil;
        state->task = nil;
      }
      if (stream_bridge != nil) {
        [stream_bridge closeWriteStream];
      }
      [task cancel];
      return;
    }

    // Coordinate initial `[task resume]` with `CancelAppleTransferTask` under
    // `state_mutex` so a cancel racing before `resume` cannot leave a never-
    // resumed task stranded in `SharedSessionDelegate()._tasks`.
    bool should_cleanup_unstarted = false;
    {
      std::lock_guard<std::mutex> slock(state->state_mutex);
      if (state->task != nil && !op->IsCanceled()) {
        state->task_started = true;
        [task resume];
      } else if (state->task != nil) {
        state->task = nil;
        should_cleanup_unstarted = true;
      }
    }
    if (should_cleanup_unstarted) {
      [SharedSessionDelegate() takeState:task];
      [task cancel];
    }
  }
}

}  // namespace http
}  // namespace firebase
