// Copyright 2021 Google LLC

#import "performance/src_ios/fake/FIRTrace.h"

#import "performance/src_ios/fake/FIRPerformance.h"
#include "testing/reporter_impl.h"

@interface FIRPerformance (Private_Methods)

- (NSDictionary *)customAttributes;

@end

@interface FIRTrace ()

@property(nonatomic, copy, readwrite) NSString *name;

/** Custom attributes managed internally. */
@property(nonatomic) NSMutableDictionary<NSString *, NSString *> *customAttributes;

/** Counters/Metrics for this fake version of FIRTrace. */
@property(nonatomic) NSMutableDictionary<NSString *, NSNumber *> *counterList;

/** Trace start time. */
@property(nonatomic, readwrite) NSDate *startTime;

/** Trace stop time. */
@property(nonatomic, readwrite) NSDate *stopTime;

@end

@implementation FIRTrace

- (instancetype)initTraceWithName:(NSString *)name {
  self = [super init];
  if (self) {
    _name = [name copy];
    _counterList = [[NSMutableDictionary alloc] init];
    _customAttributes = [[NSMutableDictionary<NSString *, NSString *> alloc] init];
  }

  FakeReporter->AddReport("-[FIRTrace initTraceWithName:]", {[name UTF8String]});

  return self;
}

- (instancetype)init {
  NSAssert(NO, @"Not a valid initializer.");
  return nil;
}

#pragma mark - Public instance methods

- (void)start {
  if (![self isTraceStarted]) {
    self.startTime = [NSDate date];
    NSLog(@"Starting trace: %@", self);
  } else {
    NSLog(@"Trace has already been started.");
  }

  FakeReporter->AddReport("-[FIRTrace start]", {});
}

- (void)stop {
  if ([self isTraceActive]) {
    self.stopTime = [NSDate date];
    NSLog(@"Stopping trace: %@", self);
  } else {
    NSLog(@"Trace hasn't been started or has already been stopped.");
  }

  FakeReporter->AddReport("-[FIRTrace stop]", {});
}

#pragma mark - Counter related methods

- (void)incrementCounterNamed:(nonnull NSString *)counterName {
  [self incrementCounterNamed:counterName by:1];
}

- (void)incrementCounterNamed:(NSString *)counterName by:(NSInteger)incrementValue {
  [self incrementMetric:counterName byInt:incrementValue];
}

#pragma mark - Metrics related methods

- (int64_t)valueForIntMetric:(nonnull NSString *)metricName {
  NSNumber *counterValue = self.counterList[metricName];
  int64_t counterLongLongValue = counterValue ? [counterValue longLongValue] : 0;
  FakeReporter->AddReport("-[FIRTrace valueForIntMetric:]", {[metricName UTF8String]});
  return counterLongLongValue;
}

- (void)setIntValue:(int64_t)value forMetric:(nonnull NSString *)metricName {
  FakeReporter->AddReport(
      "-[FIRTrace setIntValue:forMetric:]",
      {[metricName UTF8String], [[NSString stringWithFormat:@"%lld", value] UTF8String]});
  if ([self isTraceActive]) {
    self.counterList[metricName] = [NSNumber numberWithLongLong:value];
  }
}

- (void)incrementMetric:(nonnull NSString *)metricName byInt:(int64_t)incrementValue {
  FakeReporter->AddReport(
      "-[FIRTrace incrementMetric:byInt:]",
      {[metricName UTF8String], [[NSString stringWithFormat:@"%lld", incrementValue] UTF8String]});
  if ([self isTraceActive]) {
    NSNumber *counterValue = self.counterList[metricName];
    counterValue = counterValue ? counterValue : [NSNumber numberWithLongLong:0];
    counterValue = [NSNumber numberWithLongLong:[counterValue longLongValue] + incrementValue];
    self.counterList[metricName] = counterValue;
  }
}

#pragma mark - Custom attributes related methods

- (NSDictionary<NSString *, NSString *> *)attributes {
  return [self.customAttributes copy];
}

- (void)setValue:(NSString *)value forAttribute:(nonnull NSString *)attribute {
  FakeReporter->AddReport("-[FIRTrace setValue:forAttribute:]",
                          {[value UTF8String], [attribute UTF8String]});
  self.customAttributes[attribute] = value;
}

- (NSString *)valueForAttribute:(NSString *)attribute {
  FakeReporter->AddReport("-[FIRTrace valueForAttribute:]", {[attribute UTF8String]});
  return self.customAttributes[attribute];
}

- (void)removeAttribute:(NSString *)attribute {
  FakeReporter->AddReport("-[FIRTrace removeAttribute:]", {[attribute UTF8String]});
  [self.customAttributes removeObjectForKey:attribute];
}

#pragma mark - Utility methods

/**
 * Tells us if the trace has been started.
 *
 * @return YES if trace has been started, NO otherwise.
 */
- (BOOL)isTraceStarted {
  return self.startTime != nil;
}

/**
 * Tells us if the trace has been stopped.
 *
 * @return YES if trace has been stopped, NO otherwise.
 */
- (BOOL)isTraceStopped {
  return (self.startTime != nil && self.stopTime != nil);
}

/**
 * Tells us if the trace is active (has been started, but not stopped).
 *
 * @return YES if trace is active, NO otherwise.
 */
- (BOOL)isTraceActive {
  return (self.startTime != nil && self.stopTime == nil);
}

- (NSString *)description {
  return [NSString stringWithFormat:@"Trace with name: %@, startTime: %@, stopTime: %@, metrics: "
                                    @"%@, attributes: %@, global attributes: %@",
                                    self.name, self.startTime, self.stopTime, self.counterList,
                                    self.customAttributes,
                                    [[FIRPerformance sharedInstance] customAttributes]];
}

@end
