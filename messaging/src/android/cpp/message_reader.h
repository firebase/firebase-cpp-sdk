#ifndef FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGE_READER_H_
#define FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGE_READER_H_
#include <string>
#include "flatbuffers/util.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/include/firebase/messaging.h"
namespace firebase {
namespace messaging {
namespace internal {
class MessageReader {
 public:
  typedef void (*MessageCallback)(const Message& message, void* callback_data);
  typedef void (*TokenCallback)(const char* token, void* callback_data);
  typedef void (*RegistrationCallback)(const char* installationId,
                                       void* callback_data);
  typedef void (*UnregistrationCallback)(const char* installationId,
                                         void* callback_data);
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
  void ReadFromBuffer(const std::string& buffer) const;
  void ConsumeMessage(
      const com::google::firebase::messaging::cpp::SerializedMessage*
          serialized_message) const;
  void ConsumeTokenReceived(
      const com::google::firebase::messaging::cpp::SerializedTokenReceived*
          serialized_token_received) const;
  void ConsumeRegistrationReceived(
      const com::google::firebase::messaging::cpp::
          SerializedRegistrationReceived* serialized_registration_received)
      const;
  void ConsumeUnregistrationReceived(
      const com::google::firebase::messaging::cpp::
          SerializedUnregistrationReceived* serialized_unregistration_received)
      const;
  MessageCallback message_callback() const { return message_callback_; }
  void* message_callback_data() const { return message_callback_data_; }
  TokenCallback token_callback() const { return token_callback_; }
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
#endif
