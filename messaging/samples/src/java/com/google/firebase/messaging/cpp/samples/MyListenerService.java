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

// WARNING: Code from this file is included verbatim in the Messaging C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

package com.google.firebase.messaging.cpp.samples;

import android.util.Log;
import com.google.firebase.messaging.RemoteMessage;

// [START messaging_extend_listener_service]
import com.google.firebase.messaging.cpp.ListenerService;

class MyListenerService extends ListenerService {
// [END messaging_extend_listener_service]
  private static final String TAG = "MyListenerService";

  // [START messaging_on_message_received_override]
  @Override
  public void onMessageReceived(RemoteMessage message) {
    Log.d(TAG, "A message has been received.");
    // Do additional logic...
    super.onMessageReceived(message);
  }
  // [END messaging_on_message_received_override]

  // [START messaging_other_method_overrides]
  @Override
  public void onDeletedMessages() {
    Log.d(TAG, "Messages have been deleted on the server.");
    // Do additional logic...
    super.onDeletedMessages();
  }

  @Override
  public void onMessageSent(String messageId) {
    Log.d(TAG, "An outgoing message has been sent.");
    // Do additional logic...
    super.onMessageSent(messageId);
  }

  @Override
  public void onSendError(String messageId, Exception exception) {
    Log.d(TAG, "An outgoing message encountered an error.");
    // Do additional logic...
    super.onSendError(messageId, exception);
  }
  // [END messaging_other_method_overrides]
}
