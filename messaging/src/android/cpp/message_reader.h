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

#ifndef FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGE_READER_H_
#define FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGE_READER_H_

#include <string>

#include "flatbuffers/util.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/include/firebase/messaging.h"

namespace firebase {
namespace messaging {
namespace internal {

// Reads messages from a buffer or a file and notifies a callback for each
// received message or token.
class MessageReader {
 public:
  // Notifies a caller of a message.
  typedef void (*MessageCallback)(const Message& message, void* callback_data);

  // Notify the currently set listener of a new token.
  typedef void (*TokenCallback)(const char* token, void* callback_data);

  // Notify the currently set listener of a new registration installation ID.
  typedef void (*RegistrationCallback)(const char* installationId,
                                       void* callback_data);

  // Notify the currently set listener of an unregistration installation ID.
  typedef void (*UnregistrationCallback)(const char* installationId,
                                         void* callback_data);

  // Construct a reader with message and token callbacks.
  MessageReader(MessageCallback message_callback, void* message_callback_data,
                TokenCallback token_callback, void* token_callback_data,
                RegistrationCallback registration_callback = nullptr,
                void* registration_callback_data = nullptr,
                UnregistrationCallback unregistration_callback = nullptr,
                void* unregistration_callback_data = nullptr)
      : message_callback_(message_callback),
        message_callback_data_(message_callback_data),
        token_callback_(token_callback),
        token_callback_data_(token_callback_data),
        registration_callback_(registration_callback),
        registration_callback_data_(registration_callback_data),
        unregistration_callback_(unregistration_callback),
        unregistration_callback_data_(unregistration_callback_data) {}

  // Read messages or tokens from a buffer, calling message_callback and
  // token_callback (set on construction) on each message or token respectively.
  void ReadFromBuffer(const std::string& buffer) const;

  // Convert the SerializedMessage to a Message and calls the registered
  // message_callback.
  void ConsumeMessage(
      const com::google::firebase::messaging::cpp::SerializedMessage*
          serialized_message) const;

  // Convert the SerializedTokenReceived to a token and calls the registered
  // token_callback.
  void ConsumeTokenReceived(
      const com::google::firebase::messaging::cpp::SerializedTokenReceived*
          serialized_token_received) const;

  // Convert the SerializedRegistrationReceived to an installation ID and calls
  // the registered registration_callback.
  void ConsumeRegistrationReceived(
      const com::google::firebase::messaging::cpp::
          SerializedRegistrationReceived* serialized_registration_received)
      const;

  // Convert the SerializedUnregistrationReceived to an installation ID and
  // calls the registered unregistration_callback.
  void ConsumeUnregistrationReceived(
      const com::google::firebase::messaging::cpp::
          SerializedUnregistrationReceived* serialized_unregistration_received)
      const;

  // Get the message callback function.
  MessageCallback message_callback() const { return message_callback_; }

  // Get the message callback data.
  void* message_callback_data() const { return message_callback_data_; }

  // Get the token callback function.
  TokenCallback token_callback() const { return token_callback_; }

  // Get the token callback data.
  void* token_callback_data() const { return token_callback_data_; }

 private:
  static const char* SafeFlatbufferString(const flatbuffers::String* str) {
    return str ? str->c_str() : "";
  }

 private:
  MessageCallback message_callback_;
  void* message_callback_data_;
  TokenCallback token_callback_;
  void* token_callback_data_;
  RegistrationCallback registration_callback_;
  void* registration_callback_data_;
  UnregistrationCallback unregistration_callback_;
  void* unregistration_callback_data_;
};

}  // namespace internal
}  // namespace messaging
}  // namespace firebase

#endif  // FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGE_READER_H_
