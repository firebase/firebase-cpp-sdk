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

package com.google.firebase.messaging.cpp;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.verify;

import android.net.Uri;
import android.os.Bundle;
import com.google.firebase.messaging.RemoteMessage;
import com.google.firebase.messaging.RemoteMessageUtil;
import com.google.thirdparty.robolectric.GoogleRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(GoogleRobolectricTestRunner.class)
public final class ListenerServiceTest {

  @Mock private MessageWriter messageWriter;

  private ListenerService listenerService;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    listenerService = new ListenerService(messageWriter);
  }

  @Test
  public void testOnDeletedMessages() throws Exception {
    listenerService.onDeletedMessages();
    verify(messageWriter)
        .writeMessageEventToInternalStorage(
            eq(listenerService),
            (String) isNull(),
            eq(ListenerService.MESSAGE_TYPE_DELETED),
            (String) isNull());
  }

  @Test
  public void testOnMessageReceived() {
    RemoteMessage message = RemoteMessageUtil.remoteMessage(new Bundle());
    listenerService.onMessageReceived(message);
    verify(messageWriter).writeMessage(any(), eq(message), eq(false), (Uri) isNull());
  }

  @Test
  public void testOnMessageSent() {
    listenerService.onMessageSent("message_id");
    verify(messageWriter)
        .writeMessageEventToInternalStorage(
            eq(listenerService),
            eq("message_id"),
            eq(ListenerService.MESSAGE_TYPE_SEND_EVENT),
            (String) isNull());
  }

  @Test
  public void testOnSendError() {
    listenerService.onSendError(
        "message_id", RemoteMessageUtil.sendException("service_not_available"));
    verify(messageWriter)
        .writeMessageEventToInternalStorage(
            eq(listenerService),
            eq("message_id"),
            eq(ListenerService.MESSAGE_TYPE_SEND_ERROR),
            eq("com.google.firebase.messaging.SendException: service_not_available"));
  }
}
