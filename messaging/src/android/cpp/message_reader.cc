#include "messaging/src/android/cpp/message_reader.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "app/src/log.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/include/firebase/messaging.h"
namespace firebase {
namespace messaging {
namespace internal {
using com::google::firebase::messaging::cpp::DataPair;
using com::google::firebase::messaging::cpp::GetSerializedEvent;
using com::google::firebase::messaging::cpp::SerializedEvent;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedMessage;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedRegistrationReceived;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedTokenReceived;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedUnregistrationReceived;
using com::google::firebase::messaging::cpp::SerializedMessage;
using com::google::firebase::messaging::cpp::SerializedNotification;
using com::google::firebase::messaging::cpp::SerializedRegistrationReceived;
using com::google::firebase::messaging::cpp::SerializedTokenReceived;
using com::google::firebase::messaging::cpp::SerializedUnregistrationReceived;
using com::google::firebase::messaging::cpp::VerifySerializedEventBuffer;
static const char kMessageReadError[] =
    "%s Failed to load FCM messages, some messages may have been dropped! "
    "This may be due to, (1) the device being out of space, (2) a crash on "
    "a previous run of the application, (3) a change in internal "
    "serialization format following an upgrade.";
void MessageReader::ReadFromBuffer(const std::string& buffer) const {
  const char* ptr = buffer.data();
  size_t remaining = buffer.size();
  while (remaining) {
    if (remaining <= sizeof(int32_t)) {
      LogError(kMessageReadError,
               "Detected premature end of a FCM message buffer.");
      break;
    }
    int32_t raw_len = 0;
    std::memcpy(&raw_len, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);
    remaining -= sizeof(int32_t);
    if (raw_len <= 0 || static_cast<size_t>(raw_len) > remaining) {
      LogError(kMessageReadError, "Detected malformed FCM event header.");
      break;
    }
    const size_t buffer_len = static_cast<size_t>(raw_len);
    std::vector<uint8_t> aligned_buffer(
        reinterpret_cast<const uint8_t*>(ptr),
        reinterpret_cast<const uint8_t*>(ptr) + buffer_len);
    ptr += buffer_len;
    remaining -= buffer_len;
    ::flatbuffers::Verifier verifier(aligned_buffer.data(), buffer_len);
    if (!VerifySerializedEventBuffer(verifier)) {
      LogError(kMessageReadError, "FCM buffer verification failed.");
      break;
    }
    const SerializedEvent* serialized_event =
        GetSerializedEvent(aligned_buffer.data());
    if (!serialized_event) {
      LogError(kMessageReadError, "FCM event was null.");
      break;
    }
    switch (serialized_event->event_type()) {
      case SerializedEventUnion_SerializedMessage:
        ConsumeMessage(
            static_cast<const SerializedMessage*>(serialized_event->event()));
        break;
      case SerializedEventUnion_SerializedTokenReceived:
        ConsumeTokenReceived(static_cast<const SerializedTokenReceived*>(
            serialized_event->event()));
        break;
      case SerializedEventUnion_SerializedRegistrationReceived:
        ConsumeRegistrationReceived(
            static_cast<const SerializedRegistrationReceived*>(
                serialized_event->event()));
        break;
      case SerializedEventUnion_SerializedUnregistrationReceived:
        ConsumeUnregistrationReceived(
            static_cast<const SerializedUnregistrationReceived*>(
                serialized_event->event()));
        break;
      default:
        LogError(kMessageReadError, "Detected invalid FCM event type.");
        break;
    }
  }
}
void MessageReader::ConsumeMessage(
    const SerializedMessage* serialized_message) const {
  if (!serialized_message) return;
  Message message;
  Notification notification;
  AndroidNotificationParams android;
  message.from = SafeFlatbufferString(serialized_message->from());
  message.to = SafeFlatbufferString(serialized_message->to());
  if (serialized_message->data()) {
    flatbuffers::uoffset_t size = serialized_message->data()->size();
    for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
      if (const DataPair* pair = serialized_message->data()->Get(i)) {
        const char* key = SafeFlatbufferString(pair->key());
        const char* value = SafeFlatbufferString(pair->value());
        if (key && value) message.data[key] = value;
      }
    }
  }
  if (serialized_message->raw_data()) {
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
  if (const SerializedNotification* sn = serialized_message->notification()) {
    notification.title = SafeFlatbufferString(sn->title());
    notification.body = SafeFlatbufferString(sn->body());
    notification.icon = SafeFlatbufferString(sn->icon());
    notification.sound = SafeFlatbufferString(sn->sound());
    notification.badge = SafeFlatbufferString(sn->badge());
    notification.tag = SafeFlatbufferString(sn->tag());
    notification.color = SafeFlatbufferString(sn->color());
    notification.click_action = SafeFlatbufferString(sn->click_action());
    notification.body_loc_key = SafeFlatbufferString(sn->body_loc_key());
    if (sn->body_loc_args()) {
      flatbuffers::uoffset_t size = sn->body_loc_args()->size();
      notification.body_loc_args.resize(size);
      for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
        notification.body_loc_args[i] =
            SafeFlatbufferString(sn->body_loc_args()->Get(i));
      }
    }
    notification.title_loc_key = SafeFlatbufferString(sn->title_loc_key());
    if (sn->title_loc_args()) {
      flatbuffers::uoffset_t size = sn->title_loc_args()->size();
      notification.title_loc_args.resize(size);
      for (flatbuffers::uoffset_t i = 0; i < size; ++i) {
        notification.title_loc_args[i] =
            SafeFlatbufferString(sn->title_loc_args()->Get(i));
      }
    }
    android.channel_id = SafeFlatbufferString(sn->android_channel_id());
    notification.android = &android;
    message.notification = &notification;
  }
  if (message_callback_) message_callback_(message, message_callback_data_);
  if (message.notification) {
    message.notification->android = nullptr;
    message.notification = nullptr;
  }
}
void MessageReader::ConsumeTokenReceived(
    const SerializedTokenReceived* serialized_token_received) const {
  if (!serialized_token_received) return;
  if (token_callback_) {
    token_callback_(SafeFlatbufferString(serialized_token_received->token()),
                    token_callback_data_);
  }
}
void MessageReader::ConsumeRegistrationReceived(
    const SerializedRegistrationReceived* serialized_registration_received)
    const {
  if (!serialized_registration_received) return;
  if (registration_callback_) {
    registration_callback_(
        SafeFlatbufferString(
            serialized_registration_received->installation_id()),
        registration_callback_data_);
  }
}
void MessageReader::ConsumeUnregistrationReceived(
    const SerializedUnregistrationReceived* serialized_unregistration_received)
    const {
  if (!serialized_unregistration_received) return;
  if (unregistration_callback_) {
    unregistration_callback_(
        SafeFlatbufferString(
            serialized_unregistration_received->installation_id()),
        unregistration_callback_data_);
  }
}
}  // namespace internal
}  // namespace messaging
}  // namespace firebase
