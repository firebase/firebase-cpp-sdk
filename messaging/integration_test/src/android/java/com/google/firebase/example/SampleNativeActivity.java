// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.example;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;
import com.google.firebase.messaging.MessageForwardingService;

/**
 * SampleNativeActivity is a NativeActivity that updates its intent when new intents are sent to
 * it.
 *
 * This is a workaround for a known issue that prevents the native C++ library from responding to
 * data payloads when both a data and notification payload are sent to the app while it is in the
 * background.
 */
public class SampleNativeActivity extends NativeActivity {
  // The key in the intent's extras that maps to the incoming message's message ID. Only sent by
  // the server, GmsCore sends EXTRA_MESSAGE_ID_KEY below. Server can't send that as it would get
  // stripped by the client.
  private static final String EXTRA_MESSAGE_ID_KEY_SERVER = "message_id";

  // An alternate key value in the intent's extras that also maps to the incoming message's message
  // ID. Used by upstream, and set by GmsCore.
  private static final String EXTRA_MESSAGE_ID_KEY = "google.message_id";

  // The key in the intent's extras that maps to the incoming message's sender value.
  private static final String EXTRA_FROM = "google.message_id";

  /**
   * Workaround for when a message is sent containing both a Data and Notification payload.
   *
   * When the app is in the foreground all data payloads are sent to the method
   * `::firebase::messaging::Listener::OnMessage`. However, when the app is in the background, if a
   * message with both a data and notification payload is receieved the data payload is stored on
   * the notification Intent. NativeActivity does not provide native callbacks for onNewIntent, so
   * it cannot route the data payload that is stored in the Intent to the C++ function OnMessage. As
   * a workaround, we override onNewIntent so that it forwards the intent to the C++ library's
   * service which in turn forwards the data to the native C++ messaging library.
   */
  @Override
  protected void onNewIntent(Intent intent) {
    // If we do not have a 'from' field this intent was not a message and should not be handled. It
    // probably means this intent was fired by tapping on the app icon.
    Bundle extras = intent.getExtras();
    String from = extras.getString(EXTRA_FROM);
    String messageId = extras.getString(EXTRA_MESSAGE_ID_KEY);
    if (messageId == null) {
      messageId = extras.getString(EXTRA_MESSAGE_ID_KEY_SERVER);
    }
    if (from != null && messageId != null) {
      Intent message = new Intent(this, MessageForwardingService.class);
      message.setAction(MessageForwardingService.ACTION_REMOTE_INTENT);
      message.putExtras(intent);
      message.setData(intent.getData());
      // When running on Android O or later, the work will be dispatched as a job via
      // JobScheduler.enqueue. When running on older versions of the platform,
      // it will use Context.startService.
      MessageForwardingService.enqueueWork(this, message);
    }
    setIntent(intent);
  }
}
