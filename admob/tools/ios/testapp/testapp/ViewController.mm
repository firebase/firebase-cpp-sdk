//  Copyright © 2016 Google. All rights reserved.

#import "admob/tools/ios/testapp/testapp/ViewController.h"

#import "admob/tools/ios/testapp/testapp/game_engine.h"

@interface ViewController () <GLKViewDelegate, GLKViewControllerDelegate> {
  /// The AdMob C++ Wrapper Game Engine.
  GameEngine *_gameEngine;

  /// The GLKView provides a default implementation of an OpenGL ES view.
  GLKView *_glkView;
}

@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // Set up the GLKView.
  _glkView = [[GLKView alloc] init];
  [self setUpLayoutConstraintsForGLKView];
  [self.view addSubview:_glkView];

  [self setUpGL];
}

#pragma mark - GLKView Setup Methods

- (void)setUpGL {
  // Set the OpenGL ES rendering context.
  _glkView.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  [EAGLContext setCurrentContext:_glkView.context];
  _glkView.drawableDepthFormat = GLKViewDrawableDepthFormat24;
  _glkView.delegate = self;

  // Allocate and initialize a GLKViewController to implement an OpenGL ES rendering loop.
  GLKViewController *viewController = [[GLKViewController alloc] initWithNibName:nil bundle:nil];
  viewController.view = _glkView;
  viewController.delegate = self;
  [self addChildViewController:viewController];

  // Create a C++ GameEngine object and call the set up methods.
  _gameEngine = new GameEngine();
  self->_gameEngine->Initialize(self.view);
  self->_gameEngine->onSurfaceCreated();
  // Making the assumption that the glkView is equal to the mainScreen size. In other words, the
  // glkView is full screen.
  CGSize screenSize = [[[UIScreen mainScreen] currentMode] size];
  self->_gameEngine->onSurfaceChanged(screenSize.width, screenSize.height);

  // Set up the UITapGestureRecognizer for the GLKView.
  UITapGestureRecognizer *tapRecognizer =
  [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
  tapRecognizer.numberOfTapsRequired = 1;
  [self.view addGestureRecognizer:tapRecognizer];
}

- (void)setUpLayoutConstraintsForGLKView {
  [self.view addSubview:_glkView];
  _glkView.translatesAutoresizingMaskIntoConstraints = NO;

  // Layout constraints that match the parent view.
  [self.view addConstraint:[NSLayoutConstraint constraintWithItem:_glkView
                                                        attribute:NSLayoutAttributeLeading
                                                        relatedBy:NSLayoutRelationEqual
                                                           toItem:self.view
                                                        attribute:NSLayoutAttributeLeading
                                                       multiplier:1
                                                         constant:0]];
  [self.view addConstraint:[NSLayoutConstraint constraintWithItem:_glkView
                                                        attribute:NSLayoutAttributeTrailing
                                                        relatedBy:NSLayoutRelationEqual
                                                           toItem:self.view
                                                        attribute:NSLayoutAttributeTrailing
                                                       multiplier:1
                                                         constant:0]];
  [self.view addConstraint:[NSLayoutConstraint constraintWithItem:_glkView
                                                        attribute:NSLayoutAttributeHeight
                                                        relatedBy:NSLayoutRelationEqual
                                                           toItem:self.view
                                                        attribute:NSLayoutAttributeHeight
                                                       multiplier:1
                                                         constant:0]];
  [self.view addConstraint:[NSLayoutConstraint constraintWithItem:_glkView
                                                        attribute:NSLayoutAttributeWidth
                                                        relatedBy:NSLayoutRelationEqual
                                                           toItem:self.view
                                                        attribute:NSLayoutAttributeWidth
                                                       multiplier:1
                                                         constant:0]];
}

#pragma mark - GLKViewDelegate

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
  self->_gameEngine->onDrawFrame();
}

#pragma mark - GLKViewController

- (void)glkViewControllerUpdate:(GLKViewController *)controller {
  self->_gameEngine->onUpdate();
}

#pragma mark - Actions

- (void)handleTap:(UITapGestureRecognizer *)recognizer {
  if (recognizer.state == UIGestureRecognizerStateEnded) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      CGFloat scale = [[UIScreen mainScreen] scale];
      CGPoint tapLocation = [recognizer locationInView:self->_glkView];
      // Map the x and y coordinates to pixel values using the scale factor associated with the
      // device's screen.
      int scaledX = tapLocation.x * scale;
      int scaledY = tapLocation.y * scale;
      self->_gameEngine->onTap(scaledX, scaledY);
    });
  }
}

#pragma mark - Log Message

// Log a message that can be viewed in the console.
int LogMessage(const char* format, ...) {
  va_list list;
  int rc = 0;
  va_start(list, format);
  NSLogv(@(format), list);
  va_end(list);
  return rc;
}

@end
