// Copyright 2021 Google LLC

#import "performance/src_ios/fake/FIRPerformance.h"

#import "performance/src_ios/fake/FIRTrace.h"
#include "testing/reporter_impl.h"

@interface FIRTrace (Private_Methods)

- (instancetype)initTraceWithName:(NSString *)name;

@end

@interface FIRPerformance ()

/** Dictionary for global custom attributes. */
@property(nonatomic) NSMutableDictionary<NSString *, NSString *> *customAttributes;

@end

@implementation FIRPerformance

#pragma mark - Public methods

+ (instancetype)sharedInstance {
  static FIRPerformance *firebasePerformance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    firebasePerformance = [[FIRPerformance alloc] init];
  });
  return firebasePerformance;
}

+ (FIRTrace *)startTraceWithName:(NSString *)name {
  FIRTrace *trace = [[self sharedInstance] traceWithName:name];
  [trace start];
  return trace;
}

- (FIRTrace *)traceWithName:(NSString *)name {
  FIRTrace *trace = [[FIRTrace alloc] initTraceWithName:name];
  return trace;
}

- (void)setDataCollectionEnabled:(BOOL)dataCollectionEnabled {
  if (dataCollectionEnabled) {
    FakeReporter->AddReport("-[FIRPerformance setDataCollectionEnabled:]", {"YES"});
  } else {
    FakeReporter->AddReport("-[FIRPerformance setDataCollectionEnabled:]", {"NO"});
  }

  _dataCollectionEnabled = dataCollectionEnabled;
}

#pragma mark - Internal methods

- (instancetype)init {
  self = [super init];
  if (self) {
    _customAttributes = [[NSMutableDictionary<NSString *, NSString *> alloc] init];
  }
  return self;
}

#pragma mark - Custom attributes related methods

- (NSDictionary<NSString *, NSString *> *)attributes {
  return [self.customAttributes copy];
}

- (void)setValue:(NSString *)value forAttribute:(nonnull NSString *)attribute {
  self.customAttributes[attribute] = value;
}

- (NSString *)valueForAttribute:(NSString *)attribute {
  return self.customAttributes[attribute];
}

- (void)removeAttribute:(NSString *)attribute {
  [self.customAttributes removeObjectForKey:attribute];
}

@end
