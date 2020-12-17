// Copyright 2019 Google LLC
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

#include "messaging/src/android/cpp/message_reader.h"

#include <cstdint>
#include <string>

#include "app/src/log.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/include/firebase/messaging.h"

namespace firebase {
namespace messaging {
namespace internal {

using com::google::firebase::messaging::cpp::DataPair;
using com::google::firebase::messaging::cpp::GetSerializedEvent;
using com::google::firebase::messaging::cpp::SerializedEvent;
using com::google::firebase::messaging::cpp::SerializedEventUnion;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedMessage;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedTokenReceived;
using com::google::firebase::messaging::cpp::SerializedMessage;
using com::google::firebase::messaging::cpp::SerializedNotification;
using com::google::firebase::messaging::cpp::SerializedTokenReceived;
using com::google::firebase::messaging::cpp::VerifySerializedEventBuffer;

static const char kMessageReadError[] =
    "%s Failed to load FCM messages, some messages may have been dropped! "
    "This may be due to, (1) the device being out of space, (2) a crash on "
    "a previous run of the application, (3) a change in internal "
    "serialization format following an upgrade.";

void MessageReader::ReadFromBuffer(const std::string& buffer) const {
  // Process each serialized message from the buffer.
  const char* ptr = buffer.c_str();
  size_t remaining = buffer.size();
  while (remaining) {
    if (remaining <= sizeof(int32_t)) {
      LogError(kMessageReadError,
               "Detected premature end of a FCM message buffer.");
      break;
    }
    // Get the size of the flatbuffer so we know how far to increment the
    // pointer on the next iteration.
    size_t buffer_len = *reinterpret_cast<const int32_t*>(ptr);
    ptr += sizeof(int32_t);
    remaining -= sizeof(int32_t);
    if (buffer_len > remaining) {
      LogError(kMessageReadError, "Detected malformed FCM event header.");
      break;
    }

    ::flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(ptr),
                                     buffer_len);
    if (!VerifySerializedEventBuffer(verifier)) {
      // If we make a breaking change to the flatbuffers schemas any app that
      // has serialized events that have not been consumed will not be able to
      // parse those events anymore. Since the schema now has a union of
      // different kinds of events that it can represent it should be easier to
      // extend and breaking changes should not be necessary going forward, but
      // this is here just in case.
      LogError(kMessageReadError, "FCM buffer verification failed.");
      break;
    }

    // Decode the union of serialized event types and parse the message or
    // token.
    const SerializedEvent* serialized_event = GetSerializedEvent(ptr);
    ptr += buffer_len;
    remaining -= buffer_len;
    switch (serialized_event->event_type()) {
      case SerializedEventUnion_SerializedMessage: {
        const SerializedMessage* serialized_message =
            static_cast<const SerializedMessage*>(serialized_event->event());
        ConsumeMessage(serialized_message);
        break;
      }
      case SerializedEventUnion_SerializedTokenReceived: {
        const SerializedTokenReceived* serialized_token_received =
            static_cast<const SerializedTokenReceived*>(
                serialized_event->event());
        ConsumeTokenReceived(serialized_token_received);
        break;
      }
      default: {
        // This should never happen.
        LogError(kMessageReadError, "Detected invalid FCM event type.");
        break;
      }
    }
  }
}

void MessageReader::ConsumeMessage(
    const SerializedMessage* serialized_message) const {
  Message message;
  Notification notification;
  AndroidNotificationParams android;
  message.from = SafeFlatbufferString(serialized_message->from());
  message.to = SafeFlatbufferString(serialized_message->to());
  if (serialized_message->data()) {
    flatbuffers::uoffset_t size = serialized_message->data()->size();
    for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
      const DataPair* pair = serialized_message->data()->Get(i);
      const char* key = SafeFlatbufferString(pair->key());
      const char* value = SafeFlatbufferString(pair->value());
      if (key && value) {
        message.data[key] = value;
      }
    }
  }
  if (serialized_message->raw_data()) {
    message.raw_data.reserve(serialized_message->raw_data()->size());
    message.raw_data.assign(serialized_message->raw_data()->begin(),
                            serialized_message->raw_data()->end());
  }
  message.message_id = SafeFlatbufferString(serialized_message->message_id());
  message.message_type =
      SafeFlatbufferString(serialized_message->message_type());
  message.error = SafeFlatbufferString(serialized_message->error());
  message.error_description =
      SafeFlatbufferString(serialized_message->error_description());
  message.notification_opened = serialized_message->notification_opened();
  message.link = SafeFlatbufferString(serialized_message->link());
  message.collapse_key =
      SafeFlatbufferString(serialized_message->collapse_key());
  message.priority = SafeFlatbufferString(serialized_message->priority());
  message.original_priority =
      SafeFlatbufferString(serialized_message->original_priority());
  message.sent_time = serialized_message->sent_time();
  message.time_to_live = serialized_message->time_to_live();
  if (serialized_message->notification()) {
    const SerializedNotification* serialized_notification =
        serialized_message->notification();
    notification.title = SafeFlatbufferString(serialized_notification->title());
    notification.body = SafeFlatbufferString(serialized_notification->body());
    notification.icon = SafeFlatbufferString(serialized_notification->icon());
    notification.sound = SafeFlatbufferString(serialized_notification->sound());
    notification.badge = SafeFlatbufferString(serialized_notification->badge());
    notification.tag = SafeFlatbufferString(serialized_notification->tag());
    notification.color = SafeFlatbufferString(serialized_notification->color());
    notification.click_action =
        SafeFlatbufferString(serialized_notification->click_action());
    notification.body_loc_key =
        SafeFlatbufferString(serialized_notification->body_loc_key());
    if (serialized_notification->body_loc_args()) {
      flatbuffers::uoffset_t size =
          serialized_notification->body_loc_args()->size();
      notification.body_loc_args.resize(size);
      for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
        notification.body_loc_args[i] = SafeFlatbufferString(
            serialized_notification->body_loc_args()->Get(i));
      }
    }
    notification.title_loc_key =
        SafeFlatbufferString(serialized_notification->title_loc_key());
    if (serialized_notification->title_loc_args()) {
      flatbuffers::uoffset_t size =
          serialized_notification->title_loc_args()->size();
      notification.title_loc_args.resize(size);
      for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
        notification.title_loc_args[i] = SafeFlatbufferString(
            serialized_notification->title_loc_args()->Get(i));
      }
    }
    android.channel_id =
        SafeFlatbufferString(serialized_notification->android_channel_id());

    // The notification has been allocated on the stack for speed. Set this
    // pointer to null before the notification leaves scope or it will be
    // deleted.
    notification.android = &android;
    message.notification = &notification;
  }

  // Finally, process the message.
  message_callback_(message, message_callback_data_);
  // Ensure that the stack allocated pointer is null before it goes out of
  // scope so it doesn't get deleted.
  if (message.notification) {
    message.notification->android = nullptr;
    message.notification = nullptr;
  }
}

// Convert the SerializedTokenReceived to a token and calls the registered
// callback.
void MessageReader::ConsumeTokenReceived(
    const SerializedTokenReceived* serialized_token_received) const {
  const char* token = SafeFlatbufferString(serialized_token_received->token());
  token_callback_(token, token_callback_data_);
}

}  // namespace internal
}  // namespace messaging
}  // namespace firebase
