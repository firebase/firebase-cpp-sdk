// Copyright 2021 Google LLC

#import "performance/src_ios/fake/FIRHTTPMetric.h"

#include "testing/reporter_impl.h"

@interface FIRHTTPMetric ()

/* A placeholder URLRequest used for SDK metric tracking. */
@property(nonatomic) NSURLRequest *URLRequest;

/** Custom attributes. */
@property(nonatomic) NSMutableDictionary<NSString *, NSString *> *customAttributes;

/** Network request start time. */
@property(nonatomic, readwrite) NSDate *startTime;

/** Network request stop time. */
@property(nonatomic, readwrite) NSDate *stopTime;

/** @return YES if http metric has been started, NO if it hasn't been started or has been stopped.
 */
@property(nonatomic, assign, getter=isStarted) BOOL started;

@end

@implementation FIRHTTPMetric

- (nullable instancetype)initWithURL:(nonnull NSURL *)URL HTTPMethod:(FIRHTTPMethod)httpMethod {
  NSMutableURLRequest *URLRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
  NSString *HTTPMethodString = nil;
  switch (httpMethod) {
    case FIRHTTPMethodGET:
      HTTPMethodString = @"GET";
      break;

    case FIRHTTPMethodPUT:
      HTTPMethodString = @"PUT";
      break;

    case FIRHTTPMethodPOST:
      HTTPMethodString = @"POST";
      break;

    case FIRHTTPMethodHEAD:
      HTTPMethodString = @"HEAD";
      break;

    case FIRHTTPMethodDELETE:
      HTTPMethodString = @"DELETE";
      break;

    case FIRHTTPMethodPATCH:
      HTTPMethodString = @"PATCH";
      break;

    case FIRHTTPMethodOPTIONS:
      HTTPMethodString = @"OPTIONS";
      break;

    case FIRHTTPMethodTRACE:
      HTTPMethodString = @"TRACE";
      break;

    case FIRHTTPMethodCONNECT:
      HTTPMethodString = @"CONNECT";
      break;
  }
  [URLRequest setHTTPMethod:HTTPMethodString];

  if (HTTPMethodString) {
    FakeReporter->AddReport("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                            {[URL.absoluteString UTF8String], [HTTPMethodString UTF8String]});
  }

  if (URLRequest && HTTPMethodString != nil) {
    self = [super init];
    _URLRequest = [URLRequest copy];
    return self;
  }

  NSLog(@"Invalid URL");
  return nil;
}

- (void)start {
  if (!self.isStarted) {
    self.startTime = [NSDate date];
    self.started = YES;
    NSLog(@"Started http metric: %@", [self description]);
    FakeReporter->AddReport("-[FIRHTTPMetric start]", {});
  }
}

- (void)stop {
  if (self.isStarted) {
    self.stopTime = [NSDate date];
    self.started = NO;
    NSLog(@"Stopped http metric: %@.", [self description]);
    FakeReporter->AddReport("-[FIRHTTPMetric stop]", {});
  }
}

#pragma mark - Custom attributes related methods

- (NSDictionary<NSString *, NSString *> *)attributes {
  return [self.customAttributes copy];
}

- (void)setValue:(NSString *)value forAttribute:(nonnull NSString *)attribute {
  FakeReporter->AddReport("-[FIRHTTPMetric setValue:forAttribute:]",
                          {[value UTF8String], [attribute UTF8String]});
  self.customAttributes[attribute] = value;
}

- (NSString *)valueForAttribute:(NSString *)attribute {
  FakeReporter->AddReport("-[FIRHTTPMetric valueForAttribute:]", {[attribute UTF8String]});
  return self.customAttributes[attribute];
}

- (void)removeAttribute:(NSString *)attribute {
  FakeReporter->AddReport("-[FIRHTTPMetric removeAttribute:]", {[attribute UTF8String]});
  [self.customAttributes removeObjectForKey:attribute];
}

#pragma mark - Property setter overrides for unit tests

- (void)setResponseCode:(NSInteger)responseCode {
  FakeReporter->AddReport("-[FIRHTTPMetric setResponseCode:]",
                          {[[NSString stringWithFormat:@"%ld", responseCode] UTF8String]});
  _responseCode = responseCode;
}

- (void)setRequestPayloadSize:(long)requestPayloadSize {
  FakeReporter->AddReport("-[FIRHTTPMetric setRequestPayloadSize:]",
                          {[[NSString stringWithFormat:@"%ld", requestPayloadSize] UTF8String]});
  _requestPayloadSize = requestPayloadSize;
}

- (void)setResponsePayloadSize:(long)responsePayloadSize {
  FakeReporter->AddReport("-[FIRHTTPMetric setResponsePayloadSize:]",
                          {[[NSString stringWithFormat:@"%ld", responsePayloadSize] UTF8String]});
  _responsePayloadSize = responsePayloadSize;
}

- (void)setResponseContentType:(NSString *)responseContentType {
  FakeReporter->AddReport("-[FIRHTTPMetric setResponseContentType:]",
                          {[responseContentType UTF8String]});
  _responseContentType = responseContentType;
}

- (NSString *)description {
  return
      [NSString stringWithFormat:
                    @"url: %@, response code: %ld, requestPayloadSize: %ld, responsePayloadSize: "
                    @"%ld, responseContentType: %@, startTime: %@, stopTime: %@",
                    self.URLRequest.URL.absoluteString, (long)self.responseCode,
                    self.requestPayloadSize, self.responsePayloadSize, self.responseContentType,
                    [self.startTime description], [self.stopTime description]];
}

@end
