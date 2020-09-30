// Copyright 2016 Google LLC
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

package com.google.firebase.messaging;

import android.content.Context;
import android.content.Intent;
import androidx.core.app.JobIntentService;
import com.google.firebase.messaging.cpp.DebugLogging;
import com.google.firebase.messaging.cpp.MessageWriter;

/**
 * Listens for Message intents from the application and sends them to the C++ app via the
 * ListenerService.
 */
public class MessageForwardingService extends JobIntentService {
  private static final String TAG = "FIREBASE_MSG_FWDR";
  public static final String ACTION_REMOTE_INTENT = "com.google.android.c2dm.intent.RECEIVE";

  // Handle message intents sent from the ListenerService.
  @Override
  protected void onHandleWork(Intent intent) {
    handleIntent(this, intent, MessageWriter.defaultInstance());
  }

  // TODO(b/79994182): see go/objecttostring-lsc
  @SuppressWarnings("ObjectToString")
  static void handleIntent(Context context, Intent intent, MessageWriter messageWriter) {
    DebugLogging.log(
        TAG,
        "onHandleIntent "
            + (intent == null
                ? "null intent"
                : (intent.getAction() == null ? "(null)" : intent.getAction())));
    if (intent != null
        && intent.getAction() != null
        && intent.getAction().equals(ACTION_REMOTE_INTENT)) {
      android.os.Bundle extras = intent.getExtras();
      DebugLogging.log(TAG, "extras: " + (extras == null ? "(null)" : extras.toString()));
      if (extras != null) {
        RemoteMessage message = new RemoteMessage(extras);
        // TODO(b/79994182): RemoteMessage does not implement toString() in message
        DebugLogging.log(TAG, "message: " + message.toString());
        // If we don't have a sender, this intent was not a message and should not be handled.
        if (message.getFrom() != null && message.getMessageId() != null) {
          messageWriter.writeMessage(context, message, true, intent.getData());
        }
      }
    }
  }
}
