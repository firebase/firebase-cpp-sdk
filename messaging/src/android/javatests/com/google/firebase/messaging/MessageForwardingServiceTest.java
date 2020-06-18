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

package com.google.firebase.messaging;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.content.Context;
import android.content.Intent;
import com.google.firebase.messaging.cpp.MessageWriter;
import com.google.thirdparty.robolectric.GoogleRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(GoogleRobolectricTestRunner.class)
public final class MessageForwardingServiceTest {

  @Mock private Context context;
  @Mock private MessageWriter messageWriter;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
  }

  @Test
  public void testHandleIntent() throws Exception {
    Intent intent = new Intent(MessageForwardingService.ACTION_REMOTE_INTENT);
    intent.putExtra("from", "from");
    intent.putExtra("google.message_id", "id");
    ArgumentCaptor<RemoteMessage> captor = ArgumentCaptor.forClass(RemoteMessage.class);
    MessageForwardingService.handleIntent(context, intent, messageWriter);
    verify(messageWriter).writeMessage(any(), captor.capture(), eq(true), eq(null));
    RemoteMessage message = captor.getValue();
    assertThat(message.getMessageId()).isEqualTo("id");
    assertThat(message.getFrom()).isEqualTo("from");
  }

  @Test
  public void testHandleIntent_noFrom() throws Exception {
    Intent intent = new Intent(MessageForwardingService.ACTION_REMOTE_INTENT);
    intent.putExtra("from", "from");
    MessageForwardingService.handleIntent(context, intent, messageWriter);
    verifyZeroInteractions(messageWriter);
  }

  @Test
  public void testHandleIntent_noId() throws Exception {
    Intent intent = new Intent(MessageForwardingService.ACTION_REMOTE_INTENT);
    intent.putExtra("google.message_id", "id");
    MessageForwardingService.handleIntent(context, intent, messageWriter);
    verifyZeroInteractions(messageWriter);
  }

  @Test
  public void testHandleIntent_wrongAction() throws Exception {
    Intent intent = new Intent("wrong_action");
    intent.putExtra("from", "from");
    intent.putExtra("google.message_id", "id");
    MessageForwardingService.handleIntent(context, intent, messageWriter);
    verifyZeroInteractions(messageWriter);
  }
}
