#include "remote_config/src/desktop/notification_channel.h"
#include <condition_variable>
namespace firebase {
namespace remote_config {
namespace internal {
NotificationChannel::NotificationChannel()
    : have_item_(false), closed_(false) {}
bool NotificationChannel::Get() {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_variable_.wait(lock, [this]() { return closed_ || have_item_; });
  have_item_ = false;
  return !closed_;
}
void NotificationChannel::Put() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!closed_) {
    have_item_ = true;
    condition_variable_.notify_one();
  }
}
void NotificationChannel::Close() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!closed_) {
    closed_ = true;
    condition_variable_.notify_all();
  }
}
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
