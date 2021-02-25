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

#include <string.h>

#include <cstdint>
#include <string>
#include <vector>

#include "app/src/util.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/android/cpp/message_reader.h"
#include "messaging/src/include/firebase/messaging.h"
#include "gtest/gtest.h"

// Since we're compiling a subset of the Android library on all platforms,
// we need to register a stub module initializer referenced by messaging.h
// to satisfy the linker.
FIREBASE_APP_REGISTER_CALLBACKS(messaging, { return kInitResultSuccess; }, {});

namespace firebase {
namespace messaging {
namespace internal {

using com::google::firebase::messaging::cpp::CreateDataPairDirect;
using com::google::firebase::messaging::cpp::CreateSerializedEvent;
using com::google::firebase::messaging::cpp::CreateSerializedMessageDirect;
using com::google::firebase::messaging::cpp::CreateSerializedNotificationDirect;
using com::google::firebase::messaging::cpp::CreateSerializedTokenReceived;
using com::google::firebase::messaging::cpp::DataPair;
using com::google::firebase::messaging::cpp::FinishSerializedEventBuffer;
using com::google::firebase::messaging::cpp::SerializedEventUnion;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedMessage;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_SerializedTokenReceived;
using com::google::firebase::messaging::cpp::
    SerializedEventUnion_MAX;
using flatbuffers::FlatBufferBuilder;

class MessageReaderTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    messages_received_.clear();
    tokens_received_.clear();
  }

  // Stores the message in this class.
  static void MessageReceived(const Message& message, void* callback_data) {
    MessageReaderTest* test =
        reinterpret_cast<MessageReaderTest*>(callback_data);
    test->messages_received_.push_back(message);
  }

  // Stores the token in this class.
  static void TokenReceived(const char* token, void* callback_data) {
    MessageReaderTest* test =
        reinterpret_cast<MessageReaderTest*>(callback_data);
    test->tokens_received_.push_back(std::string(token));
  }

 protected:
  // Messages received by MessageReceived().
  std::vector<Message> messages_received_;
  // Tokens received by TokenReceived().
  std::vector<std::string> tokens_received_;
};

TEST_F(MessageReaderTest, Construct) {
  MessageReader reader(
      MessageReaderTest::MessageReceived, reinterpret_cast<void*>(1),
      MessageReaderTest::TokenReceived, reinterpret_cast<void*>(2));
  EXPECT_EQ(reinterpret_cast<void*>(MessageReaderTest::MessageReceived),
            reinterpret_cast<void*>(reader.message_callback()));
  EXPECT_EQ(reinterpret_cast<void*>(1), reader.message_callback_data());
  EXPECT_EQ(reinterpret_cast<void*>(MessageReaderTest::TokenReceived),
            reinterpret_cast<void*>(reader.token_callback()));
  EXPECT_EQ(reinterpret_cast<void*>(2), reader.token_callback_data());
}

// Read an empty buffer and ensure no data is parsed.
TEST_F(MessageReaderTest, ReadFromBufferEmpty) {
  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(std::string());
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
}

// Read from a buffer that is too small and ensure no data is parsed.
TEST_F(MessageReaderTest, ReadFromBufferTooSmall) {
  std::string buffer;
  buffer.push_back('b');
  buffer.push_back('d');

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
}

// Read from a buffer with a header length that overflows the buffer size.
TEST_F(MessageReaderTest, ReadFromBufferHeaderOverflow) {
  int32_t header = 9;
  std::string buffer;
  buffer.resize(sizeof(header));
  memcpy(&buffer[0], reinterpret_cast<void*>(&header), sizeof(header));
  buffer.push_back('5');
  buffer.push_back('6');
  buffer.push_back('7');
  buffer.push_back('8');

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
}

// Append a FlatBuffer to a string with the size of the FlatBuffer stored
// in a 32-bit integer header.
void AppendFlatBufferToString(std::string* output,
                              const FlatBufferBuilder& fbb) {
  int32_t flatbuffer_size = static_cast<int32_t>(fbb.GetSize());
  size_t buffer_offset = output->size();
  output->resize(buffer_offset + sizeof(flatbuffer_size) + flatbuffer_size);
  *(reinterpret_cast<int32_t*>(&((*output)[buffer_offset]))) = flatbuffer_size;
  memcpy(&((*output)[buffer_offset + sizeof(flatbuffer_size)]),
         fbb.GetBufferPointer(), flatbuffer_size);
}

// Read tokens from a buffer.
TEST_F(MessageReaderTest, ReadFromBufferTokenReceived) {
  std::string buffer;
  std::string tokens[3];
  tokens[0] = "token1";
  tokens[1] = "token2";
  tokens[2] = "token3";
  for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i) {
    FlatBufferBuilder fbb;
    FinishSerializedEventBuffer(
        fbb, CreateSerializedEvent(
                 fbb, SerializedEventUnion_SerializedTokenReceived,
                 CreateSerializedTokenReceived(fbb, fbb.CreateString(tokens[i]))
                     .Union()));
    AppendFlatBufferToString(&buffer, fbb);
  }

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(3, tokens_received_.size());
  EXPECT_EQ(tokens[0], tokens_received_[0]);
  EXPECT_EQ(tokens[1], tokens_received_[1]);
  EXPECT_EQ(tokens[2], tokens_received_[2]);
}

// Read a message from a buffer.
TEST_F(MessageReaderTest, ReadFromBufferMessageReceived) {
  FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<DataPair>> data;
  data.push_back(CreateDataPairDirect(fbb, "foo", "bar"));
  data.push_back(CreateDataPairDirect(fbb, "bosh", "bash"));
  std::vector<uint8_t> rawdata;
  rawdata.push_back(1);
  std::vector<flatbuffers::Offset<flatbuffers::String>> body_loc_args;
  body_loc_args.push_back(fbb.CreateString("1"));
  body_loc_args.push_back(fbb.CreateString("2"));
  std::vector<flatbuffers::Offset<flatbuffers::String>> title_loc_args;
  title_loc_args.push_back(fbb.CreateString("3"));
  title_loc_args.push_back(fbb.CreateString("4"));
  FinishSerializedEventBuffer(
      fbb, CreateSerializedEvent(
               fbb, SerializedEventUnion_SerializedMessage,
               CreateSerializedMessageDirect(
                   fbb, "from:bob", "to:jane", "collapsekey", &data, &rawdata,
                   "message_id", "message_type",
                   "high",  // priority
                   10,      // TTL
                   "error0", "an error description",
                   CreateSerializedNotificationDirect(
                       fbb, "title", "body", "icon", "sound", "badge", "tag",
                       "color", "click_action", "body_loc_key", &body_loc_args,
                       "title_loc_key", &title_loc_args, "android_channel_id"),
                   true,  // opened
                   "http://alink.com",
                   1234,  // sent time
                   "normal" /* original_priority */)
                   .Union()));
  std::string buffer;
  AppendFlatBufferToString(&buffer, fbb);

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(1, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
  Message& message = messages_received_[0];
  EXPECT_EQ("from:bob", message.from);
  EXPECT_EQ("to:jane", message.to);
  EXPECT_EQ("collapsekey", message.collapse_key);
  EXPECT_EQ("bar", message.data["foo"]);
  EXPECT_EQ("bash", message.data["bosh"]);
  EXPECT_EQ(1234, message.sent_time);
  EXPECT_EQ("high", message.priority);
  EXPECT_EQ("normal", message.original_priority);
  EXPECT_EQ(10, message.time_to_live);
  EXPECT_EQ("error0", message.error);
  EXPECT_EQ("an error description", message.error_description);
  EXPECT_EQ(true, message.notification_opened);
  EXPECT_EQ("http://alink.com", message.link);
  Notification* notification = message.notification;
  EXPECT_NE(nullptr, notification);
  EXPECT_EQ("title", notification->title);
  EXPECT_EQ("body", notification->body);
  EXPECT_EQ("icon", notification->icon);
  EXPECT_EQ("sound", notification->sound);
  EXPECT_EQ("click_action", notification->click_action);
  EXPECT_EQ("body_loc_key", notification->body_loc_key);
  EXPECT_EQ(2, notification->body_loc_args.size());
  EXPECT_EQ("1", notification->body_loc_args[0]);
  EXPECT_EQ("2", notification->body_loc_args[1]);
  EXPECT_EQ("title_loc_key", notification->title_loc_key);
  EXPECT_EQ(2, notification->title_loc_args.size());
  EXPECT_EQ("3", notification->title_loc_args[0]);
  EXPECT_EQ("4", notification->title_loc_args[1]);
  AndroidNotificationParams* android = message.notification->android;
  EXPECT_NE(nullptr, android);
  EXPECT_EQ("android_channel_id", android->channel_id);
}

// Try to read from a buffer with a corrupt flatbuffer
TEST_F(MessageReaderTest, ReadFromBufferCorruptFlatbuffer) {
  FlatBufferBuilder fbb;
  FinishSerializedEventBuffer(
      fbb, CreateSerializedEvent(
               fbb, SerializedEventUnion_SerializedTokenReceived,
               CreateSerializedTokenReceived(fbb, fbb.CreateString("clobberme"))
                   .Union()));
  std::string buffer;
  AppendFlatBufferToString(&buffer, fbb);
  for (size_t i = 0; i < fbb.GetSize(); ++i) {
    buffer[sizeof(int32_t) + i] = 0xef;
  }

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
}

// Try reading from a buffer with an invalid event type.
TEST_F(MessageReaderTest, ReadFromBufferInvalidEventType) {
  FlatBufferBuilder fbb;
  FinishSerializedEventBuffer(
      fbb,
      CreateSerializedEvent(
          fbb,
          static_cast<SerializedEventUnion>(
              SerializedEventUnion_MAX + 1),
          CreateSerializedTokenReceived(fbb, fbb.CreateString("ignoreme"))
              .Union()));
  std::string buffer;
  AppendFlatBufferToString(&buffer, fbb);

  MessageReader reader(MessageReaderTest::MessageReceived, this,
                       MessageReaderTest::TokenReceived, this);
  reader.ReadFromBuffer(buffer);
  EXPECT_EQ(0, messages_received_.size());
  EXPECT_EQ(0, tokens_received_.size());
}

}  // namespace internal
}  // namespace messaging
}  // namespace firebase
