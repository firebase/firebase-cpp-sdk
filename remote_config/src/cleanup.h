#ifndef FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_
#define FIREBASE_REMOTE_CONFIG_SRC_CLEANUP_H_
namespace firebase {
namespace remote_config {
template <typename T, typename R>
struct CleanupFn {
  using CreateInvalidObjectFn = T (*)();
  static CreateInvalidObjectFn create_invalid_object;
  static void Cleanup(void* obj_void) {
    T* obj = static_cast<T*>(obj_void);
    obj->Cleanup();
  }
  static void Register(T* obj, R* remote_config) {
    if (remote_config) {
      remote_config->cleanup_notifier().RegisterObject(
          obj, CleanupFn<T, R>::Cleanup);
    }
  }
  static void Unregister(T* obj, R* remote_config) {
    if (remote_config) {
      remote_config->cleanup_notifier().UnregisterObject(obj);
    }
  }
};
}  // namespace remote_config
}  // namespace firebase
#endif
