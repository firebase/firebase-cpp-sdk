/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_IOS_STARTUP_H_
#define FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_IOS_STARTUP_H_

#include <vector>

#include <UIKit/UIApplication.h>

namespace firebase {
namespace invites {
namespace internal {

// Derive from this class and instance it to hook UIApplicationDelegate methods
// required to receive dynamic links or invites.
// This is handled via a class with virtual methods so that when the
// invites_ios_startup.mm module is linked into application all other
// dynamic links / modules (e.g invites_receiver_internal_ios.mm) can be dead
// stripped.
class InvitesIosStartup {
 public:
  // Create an object to hook swizzled UIApplicationDelegate methods.
  // Priority is an arbitrary value used to determine the execution order
  // of each method of this object.  Lower values of priority are executed
  // first (e.g 0 is executed before 1).
  explicit InvitesIosStartup(int priority) : priority_(priority) { Register(); }

  virtual ~InvitesIosStartup() { Unregister(); }

  // Register this object with the set of instances that are called from
  // static methods (e.g OpenUrl) of this class.
  // This method does nothing if the object is already registered.
  void Register();
  // Unregister this object from the set of instances that are called from
  // static methods (e.g OpenUrl) of this class.
  // This method does nothing if the object is already unregistered.
  void Unregister();

  // Call HandleDidBecomeActive methods of registered instances of this class.
  static void DidBecomeActive(UIApplication *application);

  // Call HandleOpenUrl methods of registered instances of this class.
  static BOOL OpenUrl(UIApplication *application, NSURL *url,
                      NSString *sourceApplication, id annotation);
  static BOOL OpenUrl(UIApplication *application, NSURL *url,
                      NSDictionary *options);

  // Call HandleContinueUserActivity methods of registered instances of this
  // class.
  static BOOL ContinueUserActivity(UIApplication *application,
                                   NSUserActivity *userActivity,
                                   void (^restorationHandler)(NSArray *));

 protected:
  // Called from UIApplicationDelegate application:didBecomeActive.
  // All registered methods are called.
  virtual void HandleDidBecomeActive(UIApplication *application) = 0;

  // Called from
  // UIApplicationDelegate openURL:application:url:sourceApplication:annotation
  // If this method returns true methods of lower priority (e.g N+1) are not
  // called.
  virtual BOOL HandleOpenUrl(UIApplication *application, NSURL *url,
                             NSString *sourceApplication, id annotation) = 0;
  // Called from
  // UIApplicationDelegate openURL:application:url:options
  // If this method returns true methods of lower priority (e.g N+1) are not
  // called.
  virtual BOOL HandleOpenUrl(UIApplication *application, NSURL *url,
                             NSDictionary *options) = 0;
  // Called from
  // UIApplicationDelegate
  // continueUserActivity:application::userActivity:restorationHandler
  // If this method returns true methods of lower priority (e.g N+1) are not
  // called.
  virtual BOOL HandleContinueUserActivity(
      UIApplication *application, NSUserActivity *userActivity,
      void (^restorationHandler)(NSArray *)) = 0;

 private:
  int priority_;

  static std::vector<InvitesIosStartup *> *s_invites_ios_startups;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_INVITES_CLIENT_CPP_SRC_IOS_INVITES_IOS_STARTUP_H_
