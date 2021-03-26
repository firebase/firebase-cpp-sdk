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

#include "messaging/tests/messaging_test_util.h"

#include <assert.h>
#include <jni.h>
#include <sys/file.h>
#include <unistd.h>

#include "app/src/util.h"
#include "app/src/util_android.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/android/cpp/messaging_internal.h"
#include "messaging/src/include/firebase/messaging.h"
#include "testing/run_all_tests.h"
#include "flatbuffers/util.h"

using ::com::google::firebase::messaging::cpp::CreateDataPair;
using ::com::google::firebase::messaging::cpp::CreateSerializedEvent;
using ::com::google::firebase::messaging::cpp::CreateSerializedTokenReceived;
using ::com::google::firebase::messaging::cpp::DataPair;
using ::com::google::firebase::messaging::cpp::SerializedEventUnion;
using ::com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedMessage;
using ::com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedTokenReceived;
using ::com::google::firebase::messaging::cpp::SerializedMessageBuilder;
using ::com::google::firebase::messaging::cpp::SerializedNotification;
using ::com::google::firebase::messaging::cpp::SerializedNotificationBuilder;

namespace firebase {
namespace messaging {

static std::string* g_local_storage_file_path;
static std::string* g_lockfile_path;

// Lock the file referenced by g_lockfile_path.
class TestMessageLockFileLocker : private FileLocker {
 public:
  TestMessageLockFileLocker() : FileLocker(g_lockfile_path->c_str()) {}
  ~TestMessageLockFileLocker() {}
};

void InitializeMessagingTest() {
  JNIEnv* env = firebase::testing::cppsdk::GetTestJniEnv();
  jobject activity = firebase::testing::cppsdk::GetTestActivity();
  jobject file = env->CallObjectMethod(
      activity, util::context::GetMethodId(util::context::kGetFilesDir));
  assert(env->ExceptionCheck() == false);
  jstring path_jstring = reinterpret_cast<jstring>(env->CallObjectMethod(
      file, util::file::GetMethodId(util::file::kGetPath)));
  assert(env->ExceptionCheck() == false);
  std::string local_storage_dir = util::JniStringToString(env, path_jstring);
  env->DeleteLocalRef(file);
  g_lockfile_path = new std::string(local_storage_dir + "/" + kLockfile);
  g_local_storage_file_path =
      new std::string(local_storage_dir + "/" + kStorageFile);
}

void TerminateMessagingTest() {
  delete g_lockfile_path;
  g_lockfile_path = nullptr;
  delete g_local_storage_file_path;
  g_local_storage_file_path = nullptr;
}

static void WriteBuffer(const ::flatbuffers::FlatBufferBuilder& builder) {
  TestMessageLockFileLocker file_lock;
  FILE* data_file = fopen(g_local_storage_file_path->c_str(), "a");
  int size = builder.GetSize();
  fwrite(&size, sizeof(size), 1, data_file);
  fwrite(builder.GetBufferPointer(), size, 1, data_file);
  fclose(data_file);
}

void OnTokenReceived(const char* tokenstr) {
  flatbuffers::FlatBufferBuilder builder;
  auto token = builder.CreateString(tokenstr);
  auto tokenreceived = CreateSerializedTokenReceived(builder, token);
  auto event = CreateSerializedEvent(
      builder,
      SerializedEventUnion_SerializedTokenReceived,
      tokenreceived.Union());
  builder.Finish(event);
  WriteBuffer(builder);
}

void OnDeletedMessages() {
  ::flatbuffers::FlatBufferBuilder builder;
  auto from = builder.CreateString("");
  auto message_id = builder.CreateString("");
  auto message_type = builder.CreateString("deleted_messages");
  auto error = builder.CreateString("");
  auto link = builder.CreateString("");
  SerializedMessageBuilder message_builder(builder);
  message_builder.add_from(from);
  message_builder.add_message_id(message_id);
  message_builder.add_message_type(message_type);
  message_builder.add_error(error);
  message_builder.add_notification_opened(false);
  message_builder.add_link(link);
  auto message = message_builder.Finish();
  auto event = CreateSerializedEvent(
      builder,
      SerializedEventUnion_SerializedMessage,
      message.Union());
  builder.Finish(event);
  WriteBuffer(builder);
}

void OnMessageReceived(const Message& message) {
  ::flatbuffers::FlatBufferBuilder builder;
  auto from = builder.CreateString(message.from);
  auto to = builder.CreateString(message.to);
  auto message_id = builder.CreateString(message.message_id);
  auto message_type = builder.CreateString(message.message_type);
  auto error = builder.CreateString(message.error);
  auto priority = builder.CreateString(message.priority);
  auto original_priority = builder.CreateString(message.original_priority);
  auto collapse_key = builder.CreateString(message.collapse_key);

  std::vector<flatbuffers::Offset<DataPair>> data_pair_vector;
  for (auto const& entry : message.data) {
    auto key = builder.CreateString(entry.first);
    auto value = builder.CreateString(entry.second);
    auto data_pair = CreateDataPair(builder, key, value);
    data_pair_vector.push_back(data_pair);
  }
  auto data = builder.CreateVector(data_pair_vector);
  ::flatbuffers::Offset<SerializedNotification> notification;
  if (message.notification) {
    auto title = builder.CreateString(message.notification->title);
    auto body = builder.CreateString(message.notification->body);
    auto icon = builder.CreateString(message.notification->icon);
    auto sound = builder.CreateString(message.notification->sound);
    auto badge = builder.CreateString(message.notification->badge);
    auto tag = builder.CreateString(message.notification->tag);
    auto color = builder.CreateString(message.notification->color);
    auto click_action =
        builder.CreateString(message.notification->click_action);
    auto body_localization_key =
        builder.CreateString(message.notification->body_loc_key);

    std::vector<flatbuffers::Offset<flatbuffers::String>>
        body_localization_args_vector;
    for (auto const& value : message.notification->body_loc_args) {
      auto body_localization_item = builder.CreateString(value);
      body_localization_args_vector.push_back(body_localization_item);
    }
    auto body_localization_args =
        builder.CreateVector(body_localization_args_vector);

    auto title_localization_key =
        builder.CreateString(message.notification->title_loc_key);

    std::vector<flatbuffers::Offset<flatbuffers::String>>
        title_localization_args_vector;
    for (auto const& value : message.notification->title_loc_args) {
      auto title_localization_item = builder.CreateString(value);
      title_localization_args_vector.push_back(title_localization_item);
    }
    auto title_localization_args =
        builder.CreateVector(title_localization_args_vector);
    auto android_channel_id =
        message.notification->android
            ? builder.CreateString(message.notification->android->channel_id)
            : 0;

    SerializedNotificationBuilder notification_builder(builder);
    notification_builder.add_title(title);
    notification_builder.add_body(body);
    notification_builder.add_icon(icon);
    notification_builder.add_sound(sound);
    notification_builder.add_badge(badge);
    notification_builder.add_tag(tag);
    notification_builder.add_color(color);
    notification_builder.add_click_action(click_action);
    notification_builder.add_body_loc_key(body_localization_key);
    notification_builder.add_body_loc_args(body_localization_args);
    notification_builder.add_title_loc_key(title_localization_key);
    notification_builder.add_title_loc_args(title_localization_args);
    if (message.notification->android) {
      notification_builder.add_android_channel_id(android_channel_id);
    }
    notification = notification_builder.Finish();
  }
  auto link = builder.CreateString(message.link);
  SerializedMessageBuilder message_builder(builder);
  message_builder.add_from(from);
  message_builder.add_to(to);
  message_builder.add_message_id(message_id);
  message_builder.add_message_type(message_type);
  message_builder.add_priority(priority);
  message_builder.add_original_priority(original_priority);
  message_builder.add_sent_time(message.sent_time);
  message_builder.add_time_to_live(message.time_to_live);
  message_builder.add_collapse_key(collapse_key);
  if (!notification.IsNull()) {
    message_builder.add_notification(notification);
  }
  message_builder.add_error(error);
  message_builder.add_notification_opened(message.notification_opened);
  message_builder.add_link(link);
  message_builder.add_data(data);
  auto serialized_message = message_builder.Finish();
  auto event = CreateSerializedEvent(
      builder,
      SerializedEventUnion_SerializedMessage,
      serialized_message.Union());
  builder.Finish(event);
  WriteBuffer(builder);
}

void OnMessageSent(const char* message_id) {
  ::flatbuffers::FlatBufferBuilder builder;
  auto from = builder.CreateString("");
  auto message_id_offset = builder.CreateString(message_id);
  auto message_type = builder.CreateString("send_event");
  auto error = builder.CreateString("");
  auto link = builder.CreateString("");
  SerializedMessageBuilder message_builder(builder);
  message_builder.add_from(from);
  message_builder.add_message_id(message_id_offset);
  message_builder.add_message_type(message_type);
  message_builder.add_error(error);
  message_builder.add_notification_opened(false);
  message_builder.add_link(link);
  auto message = message_builder.Finish();
  auto event = CreateSerializedEvent(
      builder,
      SerializedEventUnion_SerializedMessage,
      message.Union());
  builder.Finish(event);
  WriteBuffer(builder);
}

void OnMessageSentError(const char* message_id, const char* error) {
  ::flatbuffers::FlatBufferBuilder builder;
  auto from = builder.CreateString("");
  auto message_id_offset = builder.CreateString(message_id);
  auto message_type = builder.CreateString("send_error");
  auto error_offset = builder.CreateString(error);
  auto link = builder.CreateString("");
  SerializedMessageBuilder message_builder(builder);
  message_builder.add_from(from);
  message_builder.add_message_id(message_id_offset);
  message_builder.add_message_type(message_type);
  message_builder.add_error(error_offset);
  message_builder.add_notification_opened(false);
  message_builder.add_link(link);
  auto message = message_builder.Finish();
  auto event = CreateSerializedEvent(
      builder,
      SerializedEventUnion_SerializedMessage,
      message.Union());
  builder.Finish(event);
  WriteBuffer(builder);
}

void SleepMessagingTest(double seconds) {
  sleep(static_cast<int>(seconds + 0.5));
}

}  // namespace messaging
}  // namespace firebase
