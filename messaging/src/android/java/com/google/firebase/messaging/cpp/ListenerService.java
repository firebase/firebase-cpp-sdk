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

package com.google.firebase.messaging.cpp;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

/**
 * Listens for Message events from the Firebase Cloud Messaging server and invokes the native
 * OnMessage function.
 */
public class ListenerService extends FirebaseMessagingService {

  // TODO(amablue): Add an IfChange/ThenChange block around this, and the other copy of these
  // variables in com.google.firebase.messaging.RemoteMessageBuilder.
  public static final String MESSAGE_TYPE_DELETED = "deleted_messages";
  public static final String MESSAGE_TYPE_SEND_EVENT = "send_event";
  public static final String MESSAGE_TYPE_SEND_ERROR = "send_error";

  private static final String TAG = "FIREBASE_LISTENER";

  private final MessageWriter messageWriter;

  public ListenerService() {
    this(MessageWriter.defaultInstance());
  }

  public ListenerService(MessageWriter messageWriter) {
    this.messageWriter = messageWriter;
  }

  @Override
  public void onDeletedMessages() {
    DebugLogging.log(TAG, "onDeletedMessages");
    messageWriter.writeMessageEventToInternalStorage(this, null, MESSAGE_TYPE_DELETED, null);
  }

  @Override
  public void onMessageReceived(RemoteMessage message) {
    messageWriter.writeMessage(this, message, false, null);
  }

  @Override
  public void onMessageSent(String messageId) {
    DebugLogging.log(TAG, String.format("onMessageSent messageId=%s", messageId));
    messageWriter.writeMessageEventToInternalStorage(
        this, messageId, MESSAGE_TYPE_SEND_EVENT, null);
  }

  @Override
  public void onSendError(String messageId, Exception exception) {
    DebugLogging.log(
        TAG,
        String.format("onSendError messageId=%s exception=%s", messageId, exception.toString()));
    messageWriter.writeMessageEventToInternalStorage(
        this, messageId, MESSAGE_TYPE_SEND_ERROR, exception.toString());
  }

  @Override
  public void onNewToken(String token) {
    DebugLogging.log(TAG, String.format("onNewToken token=%s", token));
    RegistrationIntentService.writeTokenToInternalStorage(this, token);
  }
}
