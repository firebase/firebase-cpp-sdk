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

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import com.google.firebase.iid.FirebaseInstanceId;
import com.google.flatbuffers.FlatBufferBuilder;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileLock;

/**
 * A class that manages Registration Token generation and passes the generated tokens to the native
 * OnTokenReceived function.
 */
public class RegistrationIntentService extends IntentService {
  private static final String TAG = "FirebaseRegService";

  public RegistrationIntentService() {
    // The tag here is used only to name the worker thread; it's important only for debugging.
    // http://developer.android.com/reference/android/app/IntentService.html#IntentService(java.lang.String)
    super(TAG);
  }

  // Fetch the latest registration token and notify the C++ layer.
  @Override
  protected void onHandleIntent(Intent intent) {
    String token = FirebaseInstanceId.getInstance().getToken();
    DebugLogging.log(TAG, String.format("onHandleIntent token=%s", token));
    if (token != null) {
      writeTokenToInternalStorage(this, token);
    }
  }

  /** Write token to internal storage so it can be accessed by the C++ layer. */
  public static void writeTokenToInternalStorage(Context context, String token) {
    byte[] buffer = generateTokenByteBuffer(token);
    ByteBuffer sizeBuffer = ByteBuffer.allocate(4);
    // Write out the buffer length into the first four bytes.
    sizeBuffer.order(ByteOrder.LITTLE_ENDIAN);
    sizeBuffer.putInt(buffer.length);
    FileLock lock = null;
    try {
      // Acquire lock. This prevents the C++ code from consuming and clearing the file while we
      // append to it.
      FileOutputStream lockFileStream = context.openFileOutput(MessageWriter.LOCK_FILE, 0);
      lock = lockFileStream.getChannel().lock();

      FileOutputStream outputStream =
          context.openFileOutput(MessageWriter.STORAGE_FILE, Context.MODE_APPEND);
      // We send both the buffer length and the buffer itself so that we can potentially process
      // more than one event in the case where they get queued up.
      outputStream.write(sizeBuffer.array());
      outputStream.write(buffer);
      outputStream.close();
    } catch (Exception e) {
      e.printStackTrace();
    } finally {
      // Release the lock.
      try {
        if (lock != null) {
          lock.release();
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }

  private static byte[] generateTokenByteBuffer(String token) {
    FlatBufferBuilder builder = new FlatBufferBuilder(0);

    int tokenOffset = builder.createString(token != null ? token : "");

    SerializedTokenReceived.startSerializedTokenReceived(builder);
    SerializedTokenReceived.addToken(builder, tokenOffset);
    int tokenReceivedOffset = SerializedTokenReceived.endSerializedTokenReceived(builder);

    SerializedEvent.startSerializedEvent(builder);
    SerializedEvent.addEventType(builder, SerializedEventUnion.SerializedTokenReceived);
    SerializedEvent.addEvent(builder, tokenReceivedOffset);
    builder.finish(SerializedEvent.endSerializedEvent(builder));

    return builder.sizedByteArray();
  }
}
