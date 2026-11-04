#ifndef FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGING_INTERNAL_H_
#define FIREBASE_MESSAGING_SRC_ANDROID_CPP_MESSAGING_INTERNAL_H_
namespace firebase {
namespace messaging {
static const char kLockfile[] = "FIREBASE_CLOUD_MESSAGING_LOCKFILE";
static const char kStorageFile[] = "FIREBASE_CLOUD_MESSAGING_LOCAL_STORAGE";
class FileLocker {
 public:
  explicit FileLocker(const char* lock_filename);
  FileLocker(const FileLocker&) = delete;
  FileLocker& operator=(const FileLocker&) = delete;
  ~FileLocker();
  static int AcquireLock(const char* lock_filename);
  static void ReleaseLock(const char* lock_filename, int fd);
 private:
  const char* lock_filename_;
  int lock_file_descriptor_;
};
}  // namespace messaging
}  // namespace firebase
#endif
