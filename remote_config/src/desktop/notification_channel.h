#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_NOTIFICATION_CHANNEL_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_NOTIFICATION_CHANNEL_H_
#include <condition_variable>
namespace firebase {
namespace remote_config {
namespace internal {
class NotificationChannel {
 public:
  NotificationChannel();
  bool Get();
  void Put();
  void Close();
 private:
  bool have_item_;
  bool closed_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
};
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
