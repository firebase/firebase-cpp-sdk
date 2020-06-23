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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;
import com.google.common.io.ByteStreams;
import com.google.firebase.messaging.RemoteMessage;
import com.google.firebase.messaging.RemoteMessageUtil;
import com.google.thirdparty.robolectric.GoogleRobolectricTestRunner;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(GoogleRobolectricTestRunner.class)
public final class MessageWriterTest {

  private static final Path STORAGE_FILE_PATH = Paths.get("/tmp/" + MessageWriter.STORAGE_FILE);

  @Mock private Context context;
  private MessageWriter messageWriter;

  @Before
  public void setUp() throws Exception {
    MockitoAnnotations.initMocks(this);
    messageWriter = new MessageWriter();
    Files.deleteIfExists(STORAGE_FILE_PATH);
    when(context.openFileOutput(eq(MessageWriter.LOCK_FILE), anyInt()))
        .thenReturn(new FileOutputStream("/tmp/" + MessageWriter.LOCK_FILE));
    when(context.openFileOutput(eq(MessageWriter.STORAGE_FILE), anyInt()))
        .thenReturn(new FileOutputStream(STORAGE_FILE_PATH.toFile(), true));
  }

  @Test
  public void testMessageWriter() throws Exception {
    Bundle bundle = new Bundle();
    bundle.putString("from", "my_from");
    bundle.putString("google.message_id", "my_message_id");
    bundle.putString("some_data", "my_data");
    bundle.putString("collapse_key", "a_key");
    bundle.putString("google.priority", "high");
    bundle.putString("google.original_priority", "normal");
    bundle.putLong("google.sent_time", 1234);
    bundle.putInt("google.ttl", 8765);
    RemoteMessage message = RemoteMessageUtil.remoteMessage(bundle);
    messageWriter.writeMessage(context, message, false, null);
    ByteBuffer byteBuffer = ByteBuffer.wrap(readStorageFile()).order(ByteOrder.LITTLE_ENDIAN);
    byteBuffer.getInt();  // Discard size.
    SerializedEvent event = SerializedEvent.getRootAsSerializedEvent(byteBuffer);
    SerializedMessage result = (SerializedMessage) event.event(new SerializedMessage());
    assertThat(result.from()).isEqualTo("my_from");
    assertThat(result.messageId()).isEqualTo("my_message_id");
    assertThat(result.collapseKey()).isEqualTo("a_key");
    assertThat(result.priority()).isEqualTo("high");
    assertThat(result.originalPriority()).isEqualTo("normal");
    assertThat(result.sentTime()).isEqualTo(1234);
    assertThat(result.timeToLive()).isEqualTo(8765);
    assertThat(result.data(0).key()).isEqualTo("some_data");
    assertThat(result.data(0).value()).isEqualTo("my_data");
  }

  private byte[] readStorageFile() throws Exception {
    return ByteStreams.toByteArray(new FileInputStream(STORAGE_FILE_PATH.toFile()));
  }
}
