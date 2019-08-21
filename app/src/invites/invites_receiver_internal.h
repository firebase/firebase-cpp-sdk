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

// Internal header file for Android InvitesReceiver functionality.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_INVITES_INVITES_RECEIVER_INTERNAL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_INVITES_INVITES_RECEIVER_INTERNAL_H_

#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/invites/cached_receiver.h"
#include "app/src/invites/receiver_interface.h"
#include "app/src/invites/sender_receiver_interface.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace invites {
namespace internal {

// This class performs the general functionality of InvitesReceiver class,
// including setting up the Future results, and processing callbacks from Java.
// A subclass will handle platform-specific operations by implementing
// PerformFetch and PerformConvertInvitation.
class InvitesReceiverInternal : public SenderReceiverInterface {
 public:
  // Create an instance of whichever subclass of InvitesReceiverInternal is
  // appropriate for our platform.
  static InvitesReceiverInternal* CreateInstance(
      const ::firebase::App& app, ReceiverInterface* receiver_implementation);

  // Decrement ref count / destroy instance of InvitesReceiverInternal.
  static void DestroyInstance(InvitesReceiverInternal* receiver,
                              ReceiverInterface* receiver_implementation);

  // The next time an instance would be created via a call to CreateInstance(),
  // return this instance instead. Use this for testing (to stub the
  // platform-specific subclass of InvitesReceiverInternal). It will be deleted
  // normally when the InvitesReceiver class is deleted.
  static void SetNextCreatedInstance(InvitesReceiverInternal* new_instance);

  // Start checking to see if we've received an invite. This will call
  // PerformFetch() which does the application-specific part. If PerformFetch()
  // returns true, the invite will be sent to the Listener via
  // ReceivedInviteCallback. Otherwise an error happened, and this will pass
  // the error to the Listener.
  void Fetch();

  // If this returns true, we are currently checking for incoming invites. If
  // this returns false, something went wrong. If it returns true, then
  // ReceivedInviteCallback will eventually get called with the results.
  virtual bool PerformFetch() = 0;

  // Not used by the receiver.
  void SentInviteCallback(const std::vector<std::string>& /*invitation_ids*/,
                          int /*result_code*/,
                          const std::string& /*error_message*/) override {}

  // Callback called when an invite is received. If an error occurred,
  // result_code should be non-zero. Otherwise, either invitation_id should be
  // set, or deep_link_url should be set, or both.
  void ReceivedInviteCallback(const std::string& invitation_id,
                              const std::string& deep_link_url,
                              InternalLinkMatchStrength match_strength,
                              int result_code,
                              const std::string& error_message) override;

  // Start the process of conversion on this invitation ID. This will call
  // PerformConvertInvitation() which does the application-specific part. If
  // PerformConvertInvitation() returns true, the Future will complete once
  // there is a call to ConvertedInviteCallback. Otherwise an error happened and
  // the Future will complete immediately.
  Future<void> ConvertInvitation(const char* id);

  // Get an already existing future result.
  Future<void> ConvertInvitationLastResult();

  // Start trying to mark the invitation as a "conversion" on the Firebase
  // backend. If this returns false, something went wrong. If it returns true,
  // then ConvertedInviteCallback will eventually get called with the results.
  virtual bool PerformConvertInvitation(const char* invitation_id) = 0;

  // Callback called when an invite conversion occurs. If an error occurred,
  // result_code will be non-zero. Otherwise, the conversion was successful.
  void ConvertedInviteCallback(const std::string& invitation_id,
                               int result_code,
                               std::string error_message) override;

  // Get the app this is attached to.
  const App* app() const { return app_; }

 protected:
  enum InvitesFn { kInvitesFnConvert, kInvitesFnCount };

  // Use CreateInstance() to create an appropriate one for the platform we are
  // on.
  explicit InvitesReceiverInternal(const ::firebase::App& app)
      : app_(&app),
        future_impl_(kInvitesFnCount),
        future_handle_convert_(ReferenceCountedFutureImpl::kInvalidHandle),
        ref_count_(0) {
    receiver_implementations_.push_back(&cached_receiver_);
  }

  // Virtual destructor is required.
  virtual ~InvitesReceiverInternal() {}

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

 protected:
  // Keep a pointer to the App in case we need to call Initialize().
  const App* app_;

 private:
  // Futures implementation, and the corresponding mutex.
  ReferenceCountedFutureImpl future_impl_;

  // When a conversion begins, future_handle_convert_ will be non-0
  // until the conversion finishes. The future for the convert can be accessed
  // via `future_impl_.LastResult(kInvitesFnConvert)`.
  SafeFutureHandle<void> future_handle_convert_;

  // Need to add a cache which stores the last invite and forwards it to the
  // newly registered receiver.
  CachedReceiver cached_receiver_;

  // Routes callbacks to library specific methods.
  std::vector<ReceiverInterface*> receiver_implementations_;

  // Number of references to this instance.
  int ref_count_;
};

}  // namespace internal
}  // namespace invites
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_INVITES_INVITES_RECEIVER_INTERNAL_H_
