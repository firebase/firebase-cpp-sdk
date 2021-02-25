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

import android.content.Context;
import android.net.Uri;
import com.google.firebase.messaging.RemoteMessage;
import com.google.firebase.messaging.RemoteMessage.Notification;
import com.google.flatbuffers.FlatBufferBuilder;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileLock;
import java.util.Map;

/** */
public class MessageWriter {
  // LINT.IfChange
  static final String LOCK_FILE = "FIREBASE_CLOUD_MESSAGING_LOCKFILE";
  // LINT.ThenChange(//depot_firebase_cpp/messaging/client/cpp/src/android/cpp/messaging_internal.h)

  // LINT.IfChange
  static final String STORAGE_FILE = "FIREBASE_CLOUD_MESSAGING_LOCAL_STORAGE";
  // LINT.ThenChange(//depot_firebase_cpp/messaging/client/cpp/src/android/cpp/messaging_internal.h)

  private static final String TAG = "FIREBASE_MESSAGE_WRITER";

  private static final MessageWriter DEFAULT_INSTANCE = new MessageWriter();

  public static MessageWriter defaultInstance() {
    return DEFAULT_INSTANCE;
  }

  /** Send a message to the application. */
  public void writeMessage(
      Context context, RemoteMessage message, boolean notificationOpened, Uri linkUri) {
    String from = message.getFrom();
    String to = message.getTo();
    String messageId = message.getMessageId();
    String messageType = message.getMessageType();
    Map<String, String> data = message.getData();
    byte[] rawData = message.getRawData();
    Notification notification = message.getNotification();
    String collapseKey = message.getCollapseKey();
    int priority = message.getPriority();
    int originalPriority = message.getOriginalPriority();
    long sentTime = message.getSentTime();
    int timeToLive = message.getTtl();

    // Links can either come from the intent or the notification.
    if (linkUri == null && notification != null) {
      linkUri = notification.getLink();
    }
    String link = (linkUri != null) ? linkUri.toString() : null;
    DebugLogging.log(
        TAG,
        String.format(
            "onMessageReceived from=%s message_id=%s, data=%s, notification=%s",
            from,
            messageId,
            (data == null ? "(null)" : data.toString()),
            (notification == null ? "(null)" : notification.toString())));
    writeMessageToInternalStorage(
        context,
        from,
        to,
        messageId,
        messageType,
        null,
        data,
        rawData,
        notification,
        notificationOpened,
        link,
        collapseKey,
        priority,
        originalPriority,
        sentTime,
        timeToLive);
  }

  /** Writes an event associated with a message to internal storage. */
  void writeMessageEventToInternalStorage(
      Context context, String messageId, String messageType, String error) {
    writeMessageToInternalStorage(
        context,
        null,
        null,
        messageId,
        messageType,
        null,
        null,
        null,
        null,
        false,
        null,
        null,
        0,
        0,
        0,
        0);
  }

  /**
   * Writes a message to internal storage so that the app can respond to it.
   *
   * <p>Because the native code in the activity will not not always be running, we write out the
   * messages recieved to internal storage for the activity to consume later. The current way we do
   * this is just by writing flatbuffers out to a file which can then be consumed by the app
   * whenever it gets around to it.
   */
  @SuppressWarnings("CatchAndPrintStackTrace")
  void writeMessageToInternalStorage(
      Context context,
      String from,
      String to,
      String messageId,
      String messageType,
      String error,
      Map<String, String> data,
      byte[] rawData,
      Notification notification,
      boolean notificationOpened,
      String link,
      String collapseKey,
      int priority,
      int originalPriority,
      long sentTime,
      int timeToLive) {
    byte[] buffer =
        generateMessageByteBuffer(
            from,
            to,
            messageId,
            messageType,
            error,
            data,
            rawData,
            notification,
            notificationOpened,
            link,
            collapseKey,
            priority,
            originalPriority,
            sentTime,
            timeToLive);
    ByteBuffer sizeBuffer = ByteBuffer.allocate(4);
    // Write out the buffer length into the first four bytes.
    sizeBuffer.order(ByteOrder.LITTLE_ENDIAN);
    sizeBuffer.putInt(buffer.length);
    FileLock lock = null;
    try {
      // Acquire lock. This prevents the C++ code from consuming and clearing the file while we
      // append to it.
      FileOutputStream lockFileStream = context.openFileOutput(LOCK_FILE, 0);
      lock = lockFileStream.getChannel().lock();

      FileOutputStream outputStream = context.openFileOutput(STORAGE_FILE, Context.MODE_APPEND);
      // We send both the buffer length and the buffer itself so that we can potentially process
      // more than one message in the case where they get queued up.
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

  private static String emptyIfNull(String str) {
    return str != null ? str : "";
  }

  private static String priorityToString(int priority) {
    switch (priority) {
      case RemoteMessage.PRIORITY_HIGH:
        return "high";
      case RemoteMessage.PRIORITY_NORMAL:
        return "normal";
      default:
        return "";
    }
  }

  private static byte[] generateMessageByteBuffer(
      String from,
      String to,
      String messageId,
      String messageType,
      String error,
      Map<String, String> data,
      byte[] rawData,
      Notification notification,
      boolean notificationOpened,
      String link,
      String collapseKey,
      int priority,
      int originalPriority,
      long sentTime,
      int timeToLive) {
    FlatBufferBuilder builder = new FlatBufferBuilder(0);
    int fromOffset = builder.createString(emptyIfNull(from));
    int toOffset = builder.createString(emptyIfNull(to));
    int messageIdOffset = builder.createString(emptyIfNull(messageId));
    int messageTypeOffset = builder.createString(emptyIfNull(messageType));
    int errorOffset = builder.createString(emptyIfNull(error));
    int linkOffset = builder.createString(emptyIfNull(link));
    int collapseKeyOffset = builder.createString(emptyIfNull(collapseKey));
    int priorityStringOffset = builder.createString(priorityToString(priority));
    int originalPriorityStringOffset = builder.createString(priorityToString(originalPriority));
    int dataOffset = 0;
    if (data != null) {
      int[] dataPairOffsets = new int[data.size()];
      int index = 0;
      for (Map.Entry<String, String> entry : data.entrySet()) {
        int key = builder.createString(entry.getKey());
        int value = builder.createString(entry.getValue());
        dataPairOffsets[index++] = DataPair.createDataPair(builder, key, value);
      }
      dataOffset = SerializedMessage.createDataVector(builder, dataPairOffsets);
    }
    int rawDataOffset = 0;
    if (rawData != null) {
      rawDataOffset = builder.createByteVector(rawData);
    }
    int notificationOffset = 0;
    if (notification != null) {
      int titleOffset = builder.createString(emptyIfNull(notification.getTitle()));
      int bodyOffset = builder.createString(emptyIfNull(notification.getBody()));
      int iconOffset = builder.createString(emptyIfNull(notification.getIcon()));
      int soundOffset = builder.createString(emptyIfNull(notification.getSound()));
      int badgeOffset = builder.createString(""); // Badges are iOS only.
      int tagOffset = builder.createString(emptyIfNull(notification.getTag()));
      int colorOffset = builder.createString(emptyIfNull(notification.getColor()));
      int clickActionOffset = builder.createString(emptyIfNull(notification.getClickAction()));
      int androidChannelIdOffset = builder.createString(emptyIfNull(notification.getChannelId()));
      int bodyLocalizationKeyOffset =
          builder.createString(emptyIfNull(notification.getBodyLocalizationKey()));
      int bodyLocalizationArgsOffset = 0;
      String[] bodyLocalizationArgs = notification.getBodyLocalizationArgs();
      if (bodyLocalizationArgs != null) {
        int index = 0;
        int[] bodyArgOffsets = new int[bodyLocalizationArgs.length];
        for (String arg : bodyLocalizationArgs) {
          bodyArgOffsets[index++] = builder.createString(arg);
        }
        bodyLocalizationArgsOffset =
            SerializedNotification.createBodyLocArgsVector(builder, bodyArgOffsets);
      }
      int titleLocalizationKeyOffset =
          builder.createString(emptyIfNull(notification.getTitleLocalizationKey()));
      int titleLocalizationArgsOffset = 0;
      String[] titleLocalizationArgs = notification.getTitleLocalizationArgs();
      if (titleLocalizationArgs != null) {
        int index = 0;
        int[] titleArgOffsets = new int[titleLocalizationArgs.length];
        for (String arg : titleLocalizationArgs) {
          titleArgOffsets[index++] = builder.createString(arg);
        }
        titleLocalizationArgsOffset =
            SerializedNotification.createTitleLocArgsVector(builder, titleArgOffsets);
      }
      SerializedNotification.startSerializedNotification(builder);
      SerializedNotification.addTitle(builder, titleOffset);
      SerializedNotification.addBody(builder, bodyOffset);
      SerializedNotification.addIcon(builder, iconOffset);
      SerializedNotification.addSound(builder, soundOffset);
      SerializedNotification.addBadge(builder, badgeOffset);
      SerializedNotification.addTag(builder, tagOffset);
      SerializedNotification.addColor(builder, colorOffset);
      SerializedNotification.addClickAction(builder, clickActionOffset);
      SerializedNotification.addAndroidChannelId(builder, androidChannelIdOffset);
      SerializedNotification.addBodyLocKey(builder, bodyLocalizationKeyOffset);
      SerializedNotification.addBodyLocArgs(builder, bodyLocalizationArgsOffset);
      SerializedNotification.addTitleLocKey(builder, titleLocalizationKeyOffset);
      SerializedNotification.addTitleLocArgs(builder, titleLocalizationArgsOffset);
      notificationOffset = SerializedNotification.endSerializedNotification(builder);
    }
    SerializedMessage.startSerializedMessage(builder);
    SerializedMessage.addFrom(builder, fromOffset);
    SerializedMessage.addTo(builder, toOffset);
    SerializedMessage.addMessageId(builder, messageIdOffset);
    SerializedMessage.addMessageType(builder, messageTypeOffset);
    SerializedMessage.addPriority(builder, priorityStringOffset);
    SerializedMessage.addOriginalPriority(builder, originalPriorityStringOffset);
    SerializedMessage.addSentTime(builder, sentTime);
    SerializedMessage.addTimeToLive(builder, timeToLive);
    SerializedMessage.addError(builder, errorOffset);
    SerializedMessage.addCollapseKey(builder, collapseKeyOffset);
    if (data != null) {
      SerializedMessage.addData(builder, dataOffset);
    }
    if (rawData != null) {
      SerializedMessage.addRawData(builder, rawDataOffset);
    }
    if (notification != null) {
      SerializedMessage.addNotification(builder, notificationOffset);
    }
    SerializedMessage.addNotificationOpened(builder, notificationOpened);
    SerializedMessage.addLink(builder, linkOffset);
    int messageOffset = SerializedMessage.endSerializedMessage(builder);
    SerializedEvent.startSerializedEvent(builder);
    SerializedEvent.addEventType(builder, SerializedEventUnion.SerializedMessage);
    SerializedEvent.addEvent(builder, messageOffset);
    builder.finish(SerializedEvent.endSerializedEvent(builder));
    return builder.sizedByteArray();
  }
}
