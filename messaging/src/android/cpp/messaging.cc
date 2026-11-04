#include "messaging/src/include/firebase/messaging.h"
#include <assert.h>
#include <jni.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/ownership.h"
#include "app/src/jni/string.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/android/cpp/message_reader.h"
#include "messaging/src/android/cpp/messaging_internal.h"
#include "messaging/src/common.h"
#define INOTIFY_EVENT_BUFFER_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
namespace firebase {
namespace messaging {
DEFINE_FIREBASE_VERSION_STRING(FirebaseMessaging);
static const char kApiIdentifier[] = "Messaging";
static const char kGcmPrefix[] = "gcm.";
static const char kReservedPrefix[] = "google.";
static const char kExtraFrom[] = "from";
static const char kExtraMessageType[] = "message_type";
static const char kExtraCollapseKey[] = "collapse_key";
static const char kExtraMessageIdServer[] = "message_id";
static const char kExtraTo[] = "google.to";
static const char kExtraMessageId[] = "google.message_id";
static const ::firebase::App* g_app = nullptr;
static Mutex g_app_mutex;
static jobject g_firebase_messaging = nullptr;
enum RegistrationTokenRequestState {
  kRegistrationTokenRequestStateNone,
  kRegistrationTokenRequestStateEnable,
  kRegistrationTokenRequestStateDisable,
};
static RegistrationTokenRequestState g_registration_token_request_state =
    kRegistrationTokenRequestStateNone;
enum DeliveryMetricsExportToBigQueryState {
  kDeliveryMetricsExportToBigQueryNone,
  kDeliveryMetricsExportToBigQueryEnable,
  kDeliveryMetricsExportToBigQueryDisable,
};
static DeliveryMetricsExportToBigQueryState
    g_delivery_metrics_export_to_big_query_state =
        kDeliveryMetricsExportToBigQueryNone;
static bool g_registration_token_received = false;
static Mutex g_registration_token_mutex;
enum PendingTopicOpType {
  kPendingTopicSubscribe,
  kPendingTopicUnsubscribe,
};
struct PendingTopicOp {
  PendingTopicOp(PendingTopicOpType op_type, const char* topic_name,
                 const SafeFutureHandle<void>& future_handle)
      : type(op_type), topic(topic_name), handle(future_handle) {}
  PendingTopicOpType type;
  std::string topic;
  SafeFutureHandle<void> handle;
};
static std::unique_ptr<std::vector<PendingTopicOp>> g_pending_topic_ops;
static std::unique_ptr<std::string> g_local_storage_file_path;
static std::unique_ptr<std::string> g_lockfile_path;
static Mutex g_file_locker_mutex;
static Mutex g_future_data_mutex;
static const char* kMessagingNotInitializedError = "Messaging not initialized.";
static void HandlePendingSubscriptions();
static void InstallationsGetToken();
static void FireIntentMessage(JNIEnv* env);
static pthread_t g_poll_thread;
static bool g_poll_thread_started = false;
static bool g_intent_message_fired = false;
#define FIREBASE_MESSAGING_METHODS(X)                                    \
  X(IsAutoInitEnabled, "isAutoInitEnabled", "()Z")                       \
  X(SetAutoInitEnabled, "setAutoInitEnabled", "(Z)V")                    \
  X(SubscribeToTopic, "subscribeToTopic",                                \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")           \
  X(UnsubscribeFromTopic, "unsubscribeFromTopic",                        \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")           \
  X(GetInstance, "getInstance",                                          \
    "()Lcom/google/firebase/messaging/FirebaseMessaging;",               \
    firebase::jni::kMethodTypeStatic)                                    \
  X(DeliveryMetricsExportToBigQueryEnabled,                              \
    "deliveryMetricsExportToBigQueryEnabled", "()Z")                     \
  X(SetDeliveryMetricsExportToBigQuery,                                  \
    "setDeliveryMetricsExportToBigQuery", "(Z)V")                        \
  X(GetToken, "getToken", "()Lcom/google/android/gms/tasks/Task;")       \
  X(DeleteToken, "deleteToken", "()Lcom/google/android/gms/tasks/Task;") \
  X(Register, "register", "()Lcom/google/android/gms/tasks/Task;")       \
  X(Unregister, "unregister", "()Lcom/google/android/gms/tasks/Task;")
JNI_METHOD_LOOKUP_DECLARATION(
    firebase_messaging, FIREBASE_MESSAGING_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/messaging/FirebaseMessaging")
FileLocker::FileLocker(const char* lock_filename)
    : lock_filename_(lock_filename),
      lock_file_descriptor_(AcquireLock(lock_filename_)) {}
FileLocker::~FileLocker() {
  if (lock_file_descriptor_ >= 0) {
    ReleaseLock(lock_filename_, lock_file_descriptor_);
  }
}
int FileLocker::AcquireLock(const char* lock_filename) {
  g_file_locker_mutex.Acquire();
  int fd = open(lock_filename, O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    g_file_locker_mutex.Release();
    return -1;
  }
  int lock_result = 0;
  do {
    lock_result = flock(fd, LOCK_EX);
  } while (lock_result < 0 && errno == EINTR);
  if (lock_result < 0) {
    close(fd);
    g_file_locker_mutex.Release();
    return -1;
  }
  return fd;
}
void FileLocker::ReleaseLock(const char* lock_filename, int fd) {
  (void)lock_filename;
  if (fd >= 0) {
    flock(fd, LOCK_UN);
    close(fd);
  }
  g_file_locker_mutex.Release();
}
class MessageLockFileLocker : private FileLocker {
 public:
  explicit MessageLockFileLocker(
      const char* lockfile_path = g_lockfile_path->c_str())
      : FileLocker(lockfile_path) {}
};
static bool LoadFile(const char* name, std::string* buf) {
  if (!name || !buf) return false;
  FILE* file = fopen(name, "rb");
  if (!file) return false;
  if (fseek(file, 0L, SEEK_END) != 0) {
    fclose(file);
    return false;
  }
  long file_size = ftell(file);
  if (file_size < 0 || fseek(file, 0L, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }
  buf->resize(static_cast<size_t>(file_size));
  if (file_size > 0 && fread(&(*buf)[0], 1, buf->size(), file) != buf->size()) {
    fclose(file);
    buf->clear();
    return false;
  }
  bool ok = ferror(file) == 0;
  return fclose(file) == 0 && ok;
}
static void ConsumeEvents() {
  if (!g_local_storage_file_path || !g_lockfile_path) return;
  std::string buffer;
  {
    MessageLockFileLocker file_lock;
    FIREBASE_ASSERT_RETURN_VOID(
        LoadFile(g_local_storage_file_path->c_str(), &buffer));
    if (!buffer.empty() &&
        truncate(g_local_storage_file_path->c_str(), 0) != 0) {
      if (FILE* f = fopen(g_local_storage_file_path->c_str(), "w")) fclose(f);
    }
  }
  if (buffer.empty()) return;
  internal::MessageReader reader(
      [](const Message& message, void*) { NotifyListenerOnMessage(message); },
      nullptr,
      [](const char* token, void*) {
        {
          MutexLock lock(g_registration_token_mutex);
          g_registration_token_received = true;
          HandlePendingSubscriptions();
        }
        NotifyListenerOnTokenReceived(token);
      },
      nullptr,
      [](const char* installationId, void*) {
        {
          MutexLock lock(g_registration_token_mutex);
          g_registration_token_received = true;
          HandlePendingSubscriptions();
        }
        NotifyListenerOnRegistrationReceived(installationId);
      },
      nullptr,
      [](const char* installationId, void*) {
        NotifyListenerOnUnregistrationReceived(installationId);
      },
      nullptr);
  reader.ReadFromBuffer(buffer);
}
static bool TerminateRequested() {
  MutexLock lock(g_app_mutex);
  return !g_app;
}
void NotifyListenerSet(Listener* listener) {
  if (!listener) return;
  std::string local_storage, lockfile;
  {
    MutexLock lock(g_app_mutex);
    if (!g_app || !g_local_storage_file_path || !g_lockfile_path) return;
    local_storage = *g_local_storage_file_path;
    lockfile = *g_lockfile_path;
  }
  MessageLockFileLocker file_lock(lockfile.c_str());
  if (FILE* f = fopen(local_storage.c_str(), "a")) fclose(f);
}
template <typename T>
static Future<T> GetMessagingLastResult(MessagingFn fn) {
  FIREBASE_ASSERT_RETURN(Future<T>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) return Future<T>();
  return static_cast<const Future<T>&>(FutureData::Get()->api()->LastResult(fn));
}
Future<void> RequestPermission() {
  FIREBASE_ASSERT_RETURN(RequestPermissionLastResult(),
                         internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kMessagingFnRequestPermission);
  api->Complete(handle, kErrorNone);
  return MakeFuture(api, handle);
}
Future<void> RequestPermissionLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnRequestPermission);
}
void ProcessMessages() {
  JNIEnv* env;
  {
    MutexLock lock(g_app_mutex);
    env = g_app ? g_app->GetJNIEnv() : nullptr;
  }
  if (HasListener() && env) {
    FireIntentMessage(env);
    ConsumeEvents();
  }
}
static void* MessageProcessingThread(void*) {
  JavaVM* jvm = nullptr;
  {
    MutexLock lock(g_app_mutex);
    jvm = g_app ? g_app->java_vm() : nullptr;
    if (!jvm || !g_local_storage_file_path) return nullptr;
  }
  int fd = inotify_init();
  FIREBASE_ASSERT_RETURN(nullptr, fd >= 0);
  int wd = inotify_add_watch(fd, g_local_storage_file_path->c_str(),
                             IN_CLOSE_WRITE);
  if (wd < 0) {
    close(fd);
    FIREBASE_ASSERT_RETURN(nullptr, false);
  }
  char buf[INOTIFY_EVENT_BUFFER_SIZE]
      __attribute__((aligned(__alignof__(inotify_event))));
  ProcessMessages();
  while (!TerminateRequested()) {
    ssize_t numRead = read(fd, buf, sizeof(buf));
    if (TerminateRequested()) break;
    if (numRead <= 0) LogDebug("Reading message file, errno=%d", errno);
    ProcessMessages();
  }
  inotify_rm_watch(fd, wd);
  close(fd);
  JNIEnv* attached_env = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&attached_env), JNI_VERSION_1_6) ==
          JNI_OK &&
      attached_env != nullptr) {
    jvm->DetachCurrentThread();
  }
  return nullptr;
}
static void TerminateMessageProcessingThread() {
  if (!g_poll_thread_started) return;
  if (g_local_storage_file_path && g_lockfile_path) {
    MessageLockFileLocker file_lock;
    if (FILE* f = fopen(g_local_storage_file_path->c_str(), "a")) {
      fclose(f);
    } else {
      LogWarning("Failed to open storage file to wake message thread.");
    }
  }
  pthread_join(g_poll_thread, nullptr);
  g_poll_thread_started = false;
}
static bool IsValidKey(const char* key) {
  return strncmp(key, kReservedPrefix, strlen(kReservedPrefix)) != 0 &&
         strncmp(key, kGcmPrefix, strlen(kGcmPrefix)) != 0 &&
         strcmp(key, kExtraFrom) != 0 && strcmp(key, kExtraMessageType) != 0 &&
         strcmp(key, kExtraCollapseKey) != 0;
}
static void BundleToMessageData(JNIEnv* env,
                                std::map<std::string, std::string>* to,
                                jobject from) {
  jclass from_class = env->GetObjectClass(from);
  jmethodID key_set_method =
      env->GetMethodID(from_class, "keySet", "()Ljava/util/Set;");
  jmethodID get_string_method = env->GetMethodID(
      from_class, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
  env->DeleteLocalRef(from_class);
  if (::firebase::jni::CheckAndClearException(env) || !key_set_method ||
      !get_string_method) {
    return;
  }
  jobject key_set = env->CallObjectMethod(from, key_set_method);
  if (::firebase::jni::CheckAndClearException(env) || !key_set) return;
  jclass set_class = env->GetObjectClass(key_set);
  jmethodID iterator_method =
      env->GetMethodID(set_class, "iterator", "()Ljava/util/Iterator;");
  env->DeleteLocalRef(set_class);
  if (::firebase::jni::CheckAndClearException(env) || !iterator_method) {
    env->DeleteLocalRef(key_set);
    return;
  }
  jobject iter = env->CallObjectMethod(key_set, iterator_method);
  if (::firebase::jni::CheckAndClearException(env) || !iter) {
    env->DeleteLocalRef(key_set);
    return;
  }
  jclass iter_class = env->GetObjectClass(iter);
  jmethodID has_next = env->GetMethodID(iter_class, "hasNext", "()Z");
  jmethodID next = env->GetMethodID(iter_class, "next", "()Ljava/lang/Object;");
  env->DeleteLocalRef(iter_class);
  if (!::firebase::jni::CheckAndClearException(env) && has_next && next) {
    while (env->CallBooleanMethod(iter, has_next)) {
      if (::firebase::jni::CheckAndClearException(env)) break;
      jobject key_obj = env->CallObjectMethod(iter, next);
      if (::firebase::jni::CheckAndClearException(env) || !key_obj) break;
      const char* key =
          env->GetStringUTFChars(static_cast<jstring>(key_obj), nullptr);
      if (key && IsValidKey(key)) {
        jstring val_jstr = static_cast<jstring>(
            env->CallObjectMethod(from, get_string_method, key_obj));
        if (!::firebase::jni::CheckAndClearException(env) && val_jstr) {
          (*to)[key] = ::firebase::jni::JStringToString(env, val_jstr);
        }
        if (val_jstr) env->DeleteLocalRef(val_jstr);
      }
      if (key) env->ReleaseStringUTFChars(static_cast<jstring>(key_obj), key);
      env->DeleteLocalRef(key_obj);
    }
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}
static std::string BundleGetString(JNIEnv* env, jobject bundle,
                                   const char* key) {
  jclass bundle_class = env->GetObjectClass(bundle);
  jmethodID get_string_method = env->GetMethodID(
      bundle_class, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
  env->DeleteLocalRef(bundle_class);
  if (::firebase::jni::CheckAndClearException(env) || !get_string_method) {
    return "";
  }
  jstring key_jstr = env->NewStringUTF(key);
  jstring val_jstr = static_cast<jstring>(
      env->CallObjectMethod(bundle, get_string_method, key_jstr));
  env->DeleteLocalRef(key_jstr);
  if (::firebase::jni::CheckAndClearException(env) || !val_jstr) {
    if (val_jstr) env->DeleteLocalRef(val_jstr);
    return "";
  }
  std::string val = ::firebase::jni::JStringToString(env, val_jstr);
  env->DeleteLocalRef(val_jstr);
  return val;
}
static void FireIntentMessage(JNIEnv* env) {
  if (g_intent_message_fired || !HasListener() || !env) return;
  jobject activity;
  {
    MutexLock lock(g_app_mutex);
    if (!g_app) return;
    activity = env->NewLocalRef(g_app->activity());
    if (::firebase::jni::CheckAndClearException(env) || !activity) return;
  }
  g_intent_message_fired = true;
  jclass activity_class = env->GetObjectClass(activity);
  jmethodID get_intent = env->GetMethodID(activity_class, "getIntent",
                                          "()Landroid/content/Intent;");
  env->DeleteLocalRef(activity_class);
  jobject intent = nullptr;
  if (!::firebase::jni::CheckAndClearException(env) && get_intent) {
    intent = env->CallObjectMethod(activity, get_intent);
    ::firebase::jni::CheckAndClearException(env);
  }
  env->DeleteLocalRef(activity);
  if (!intent) return;
  jclass intent_class = env->GetObjectClass(intent);
  jmethodID get_extras =
      env->GetMethodID(intent_class, "getExtras", "()Landroid/os/Bundle;");
  jmethodID get_data =
      env->GetMethodID(intent_class, "getData", "()Landroid/net/Uri;");
  env->DeleteLocalRef(intent_class);
  jobject bundle = nullptr;
  if (!::firebase::jni::CheckAndClearException(env) && get_extras) {
    bundle = env->CallObjectMethod(intent, get_extras);
    ::firebase::jni::CheckAndClearException(env);
  }
  if (bundle) {
    Message message;
    message.message_id = BundleGetString(env, bundle, kExtraMessageId);
    if (message.message_id.empty()) {
      message.message_id = BundleGetString(env, bundle, kExtraMessageIdServer);
    }
    message.from = BundleGetString(env, bundle, kExtraFrom);
    if (!message.message_id.empty() && !message.from.empty()) {
      message.to = BundleGetString(env, bundle, kExtraTo);
      message.message_type = BundleGetString(env, bundle, kExtraMessageType);
      message.collapse_key = BundleGetString(env, bundle, kExtraCollapseKey);
      BundleToMessageData(env, &message.data, bundle);
      message.notification_opened = true;
      jobject uri_obj =
          get_data ? env->CallObjectMethod(intent, get_data) : nullptr;
      ::firebase::jni::CheckAndClearException(env);
      if (uri_obj) {
        jclass uri_class = env->FindClass("android/net/Uri");
        jmethodID to_str = uri_class ? env->GetMethodID(uri_class, "toString",
                                                        "()Ljava/lang/String;")
                                     : nullptr;
        if (!::firebase::jni::CheckAndClearException(env) && to_str) {
          jstring str =
              static_cast<jstring>(env->CallObjectMethod(uri_obj, to_str));
          if (!::firebase::jni::CheckAndClearException(env) && str) {
            message.link = ::firebase::jni::JStringToString(env, str);
          }
          if (str) env->DeleteLocalRef(str);
        }
        if (uri_class) env->DeleteLocalRef(uri_class);
        env->DeleteLocalRef(uri_obj);
      }
      NotifyListenerOnMessage(message);
    }
    env->DeleteLocalRef(bundle);
  }
  env->DeleteLocalRef(intent);
}
static void ReleaseClasses(JNIEnv* env) {
  firebase_messaging::ReleaseClass(env);
}
InitResult Initialize(const ::firebase::App& app, Listener* listener) {
  return Initialize(app, listener, MessagingOptions());
}
InitResult Initialize(const ::firebase::App& app, Listener* listener,
                      const MessagingOptions& options) {
  (void)options;
  FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app);
  SetListenerIfNotNull(listener);
  if (g_app) {
    LogError("Messaging already initialized.");
    return kInitResultSuccess;
  }
  JNIEnv* env = app.GetJNIEnv();
  if (!firebase_messaging::CacheMethodIds(env, app.activity())) {
    ReleaseClasses(env);
    LogError("Failed to initialize messaging");
    return kInitResultFailedMissingDependency;
  }
  jclass context_class = env->FindClass("android/content/Context");
  if (::firebase::jni::CheckAndClearException(env) || !context_class) {
    if (context_class) env->DeleteLocalRef(context_class);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  jmethodID get_files_dir =
      env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");
  if (::firebase::jni::CheckAndClearException(env) || !get_files_dir) {
    env->DeleteLocalRef(context_class);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  jobject file = env->CallObjectMethod(app.activity(), get_files_dir);
  env->DeleteLocalRef(context_class);
  if (::firebase::jni::CheckAndClearException(env) || !file) {
    if (file) env->DeleteLocalRef(file);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  jclass file_class = env->FindClass("java/io/File");
  if (::firebase::jni::CheckAndClearException(env) || !file_class) {
    if (file_class) env->DeleteLocalRef(file_class);
    env->DeleteLocalRef(file);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  jmethodID get_path =
      env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
  if (::firebase::jni::CheckAndClearException(env) || !get_path) {
    env->DeleteLocalRef(file_class);
    env->DeleteLocalRef(file);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  jstring path_jstr =
      static_cast<jstring>(env->CallObjectMethod(file, get_path));
  env->DeleteLocalRef(file_class);
  if (::firebase::jni::CheckAndClearException(env) || !path_jstr) {
    if (path_jstr) env->DeleteLocalRef(path_jstr);
    env->DeleteLocalRef(file);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  std::string local_storage_dir =
      ::firebase::jni::JStringToString(env, path_jstr);
  env->DeleteLocalRef(path_jstr);
  env->DeleteLocalRef(file);
  jobject fm_inst = env->CallStaticObjectMethod(
      firebase_messaging::GetClass(),
      firebase_messaging::GetMethodId(firebase_messaging::kGetInstance));
  if (::firebase::jni::CheckAndClearException(env) || !fm_inst) {
    if (fm_inst) env->DeleteLocalRef(fm_inst);
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  g_firebase_messaging = env->NewGlobalRef(fm_inst);
  env->DeleteLocalRef(fm_inst);
  if (::firebase::jni::CheckAndClearException(env) || !g_firebase_messaging) {
    ReleaseClasses(env);
    return kInitResultFailedMissingDependency;
  }
  {
    MutexLock lock(g_registration_token_mutex);
    g_pending_topic_ops = std::make_unique<std::vector<PendingTopicOp>>();
  }
  g_intent_message_fired = false;
  g_lockfile_path =
      std::make_unique<std::string>(local_storage_dir + "/" + kLockfile);
  g_local_storage_file_path =
      std::make_unique<std::string>(local_storage_dir + "/" + kStorageFile);
  FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
  FIREBASE_ASSERT(storage_file != nullptr);
  if (storage_file) fclose(storage_file);
  {
    MutexLock lock(g_future_data_mutex);
    FutureData::Create();
  }
  {
    MutexLock lock(g_app_mutex);
    g_app = &app;
  }
  if (g_registration_token_request_state !=
      kRegistrationTokenRequestStateNone) {
    assert(internal::IsInitialized());
    SetTokenRegistrationOnInitEnabled(g_registration_token_request_state ==
                                      kRegistrationTokenRequestStateEnable);
  }
  if (g_delivery_metrics_export_to_big_query_state !=
      kDeliveryMetricsExportToBigQueryNone) {
    assert(internal::IsInitialized());
    SetDeliveryMetricsExportToBigQuery(
        g_delivery_metrics_export_to_big_query_state ==
        kDeliveryMetricsExportToBigQueryEnable);
  }
  int result =
      pthread_create(&g_poll_thread, nullptr, MessageProcessingThread, nullptr);
  FIREBASE_ASSERT(result == 0);
  g_poll_thread_started = (result == 0);
  if (IsTokenRegistrationOnInitEnabled()) {
    InstallationsGetToken();
  }
  LogInfo("Firebase Cloud Messaging API Initialized");
  internal::RegisterTerminateOnDefaultAppDestroy(app);
  return kInitResultSuccess;
}
namespace internal {
bool IsInitialized() { return g_app != nullptr; }
}  // namespace internal
void Terminate() {
  if (!g_app) {
    LogError("Messaging already shut down.");
    return;
  }
  internal::UnregisterTerminateOnDefaultAppDestroy();
  JNIEnv* env = g_app->GetJNIEnv();
  firebase::util::CancelCallbacks(env, kApiIdentifier);
  {
    MutexLock lock(g_app_mutex);
    g_app = nullptr;
  }
  TerminateMessageProcessingThread();
  {
    MutexLock lock(g_registration_token_mutex);
    g_pending_topic_ops.reset();
    g_registration_token_received = false;
    g_registration_token_request_state = kRegistrationTokenRequestStateNone;
  }
  g_local_storage_file_path.reset();
  g_lockfile_path.reset();
  g_delivery_metrics_export_to_big_query_state =
      kDeliveryMetricsExportToBigQueryNone;
  g_intent_message_fired = false;
  env->DeleteGlobalRef(g_firebase_messaging);
  g_firebase_messaging = nullptr;
  SetListener(nullptr);
  ReleaseClasses(env);
  {
    MutexLock lock(g_future_data_mutex);
    FutureData::Destroy();
  }
}
static void InstallationsGetTokenComplete(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  (void)status_message;
  (void)callback_data;
  bool success = (result_code == firebase::util::kFutureResultSuccess);
  std::string token;
  if (success && result) token = ::firebase::jni::JStringToString(env, result);
  if (result) env->DeleteLocalRef(result);
  if (success && !token.empty()) {
    {
      MutexLock lock(g_registration_token_mutex);
      {
        MutexLock app_lock(g_app_mutex);
        if (!g_app) return;
      }
      g_registration_token_received = true;
      HandlePendingSubscriptions();
    }
    NotifyListenerOnTokenReceived(token.c_str());
  }
}
static jobject AcquireMessagingLocalRef(JNIEnv** out_env) {
  MutexLock lock(g_app_mutex);
  if (g_app && g_firebase_messaging) {
    JNIEnv* env = g_app->GetJNIEnv();
    if (env) {
      jobject obj = env->NewLocalRef(g_firebase_messaging);
      if (::firebase::jni::CheckAndClearException(env)) {
        if (obj) env->DeleteLocalRef(obj);
        return nullptr;
      }
      *out_env = env;
      return obj;
    }
  }
  return nullptr;
}
static void InstallationsGetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(internal::IsInitialized(),
                                      kMessagingNotInitializedError);
  JNIEnv* env = nullptr;
  jobject messaging_obj = AcquireMessagingLocalRef(&env);
  if (!messaging_obj) return;
  jobject result = env->CallObjectMethod(
      messaging_obj,
      firebase_messaging::GetMethodId(firebase_messaging::kGetToken));
  env->DeleteLocalRef(messaging_obj);
  if (!::firebase::jni::CheckAndClearException(env) && result) {
    firebase::util::RegisterCallbackOnTask(
        env, result, InstallationsGetTokenComplete, nullptr, kApiIdentifier);
    ::firebase::jni::CheckAndClearException(env);
  }
  if (result) env->DeleteLocalRef(result);
}
static void SubscriptionUpdateComplete(JNIEnv* env, jobject result,
                                       firebase::util::FutureResult result_code,
                                       const char* status_message,
                                       void* callback_data) {
  auto handle = SafeFutureHandle<void>(
      FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)));
  Error error = (result_code == firebase::util::kFutureResultSuccess)
                    ? kErrorNone
                    : kErrorUnknown;
  {
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, error, status_message);
    } else {
      LogWarning("Failed to complete Future as it was likely already deleted.");
    }
  }
  if (result) env->DeleteLocalRef(result);
}
static const char kErrorMessageInvalidTopicName[] =
    "Topic name must not be null or empty.";
static const char kErrorMessageNoRegistrationToken[] =
    "Cannot update subscription when SetTokenRegistrationOnInitEnabled is set "
    "to false.";
static const char kErrorMessageSubscriptionUnknown[] =
    "Cannot update subscription for unknown reason.";
static void UpdateTopicSubscriptionInternal(
    const char* topic, const SafeFutureHandle<void>& handle,
    firebase_messaging::Method method) {
  assert(g_registration_token_received);
  JNIEnv* env = nullptr;
  jobject messaging_obj = AcquireMessagingLocalRef(&env);
  if (!messaging_obj) {
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorUnknown,
                                         "Messaging is uninitialized.");
    }
    return;
  }
  jstring java_topic = env->NewStringUTF(topic);
  jobject result = env->CallObjectMethod(
      messaging_obj, firebase_messaging::GetMethodId(method), java_topic);
  env->DeleteLocalRef(messaging_obj);
  if (env->ExceptionCheck()) {
    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string message =
        ::firebase::jni::ExtractExceptionMessage(env, exception);
    env->DeleteLocalRef(exception);
    if (result) env->DeleteLocalRef(result);
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorInvalidTopicName,
                                         message.c_str());
    }
  } else if (result) {
    firebase::util::RegisterCallbackOnTask(
        env, result, SubscriptionUpdateComplete,
        reinterpret_cast<void*>(handle.get().id()), kApiIdentifier);
    ::firebase::jni::CheckAndClearException(env);
    env->DeleteLocalRef(result);
  } else {
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorUnknown,
                                         kErrorMessageSubscriptionUnknown);
    }
  }
  env->DeleteLocalRef(java_topic);
}
static void SubscribeInternal(const char* topic,
                              const SafeFutureHandle<void>& handle) {
  LogDebug("Subscribe to topic %s", topic);
  UpdateTopicSubscriptionInternal(topic, handle,
                                  firebase_messaging::kSubscribeToTopic);
}
static void UnsubscribeInternal(const char* topic,
                                const SafeFutureHandle<void>& handle) {
  LogDebug("Unsubscribe from topic %s", topic);
  UpdateTopicSubscriptionInternal(topic, handle,
                                  firebase_messaging::kUnsubscribeFromTopic);
}
static void HandlePendingSubscriptions() {
  if (!g_pending_topic_ops) return;
  for (const auto& op : *g_pending_topic_ops) {
    if (op.type == kPendingTopicSubscribe) {
      SubscribeInternal(op.topic.c_str(), op.handle);
    } else {
      UnsubscribeInternal(op.topic.c_str(), op.handle);
    }
  }
  g_pending_topic_ops->clear();
}
static void CompleteVoidCallback(JNIEnv* env, jobject result,
                                 firebase::util::FutureResult result_code,
                                 const char* status_message,
                                 void* callback_data) {
  FutureHandle handle(reinterpret_cast<FutureHandleId>(callback_data));
  Error error = (result_code == firebase::util::kFutureResultSuccess)
                    ? kErrorNone
                    : kErrorUnknown;
  MutexLock lock(g_future_data_mutex);
  if (FutureData::Get() && FutureData::Get()->api()) {
    FutureData::Get()->api()->Complete(handle, error, status_message);
  } else {
    LogWarning("Failed to complete Future as it was likely already deleted.");
  }
  if (result) env->DeleteLocalRef(result);
}
static void CompleteStringCallback(JNIEnv* env, jobject result,
                                   firebase::util::FutureResult result_code,
                                   const char* status_message,
                                   void* callback_data) {
  bool success = (result_code == firebase::util::kFutureResultSuccess);
  std::string result_value;
  if (success && result) {
    result_value = ::firebase::jni::JStringToString(env, result);
  }
  if (result) env->DeleteLocalRef(result);
  auto handle = SafeFutureHandle<std::string>(
      FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)));
  Error error = success ? kErrorNone : kErrorUnknown;
  MutexLock lock(g_future_data_mutex);
  if (FutureData::Get() && FutureData::Get()->api()) {
    FutureData::Get()->api()->CompleteWithResult(handle, error, status_message,
                                                 result_value);
  } else {
    LogWarning("Failed to complete Future as it was likely already deleted.");
  }
}
static Future<void> PerformTopicOp(const char* topic, MessagingFn fn,
                                   PendingTopicOpType op_type) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(g_registration_token_mutex);
  bool initialized = false;
  {
    MutexLock app_lock(g_app_mutex);
    initialized = (g_app && g_firebase_messaging);
  }
  if (!initialized) {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
    api->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle;
  {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
    handle = FutureData::Get()->api()->SafeAlloc<void>(fn);
  }
  if (!topic || topic[0] == '\0') {
    MutexLock future_lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorInvalidTopicName,
                                         kErrorMessageInvalidTopicName);
    }
  } else if (g_registration_token_received) {
    if (op_type == kPendingTopicSubscribe) {
      SubscribeInternal(topic, handle);
    } else {
      UnsubscribeInternal(topic, handle);
    }
  } else if (g_registration_token_request_state ==
             kRegistrationTokenRequestStateDisable) {
    MutexLock future_lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorNoRegistrationToken,
                                         kErrorMessageNoRegistrationToken);
    }
  } else if (g_pending_topic_ops) {
    g_pending_topic_ops->emplace_back(op_type, topic, handle);
  } else {
    MutexLock future_lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorUnknown,
                                         kErrorMessageSubscriptionUnknown);
    }
  }
  MutexLock future_lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
  return MakeFuture(FutureData::Get()->api(), handle);
}
Future<void> Subscribe(const char* topic) {
  return PerformTopicOp(topic, kMessagingFnSubscribe, kPendingTopicSubscribe);
}
Future<void> SubscribeLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnSubscribe);
}
Future<void> Unsubscribe(const char* topic) {
  return PerformTopicOp(topic, kMessagingFnUnsubscribe,
                        kPendingTopicUnsubscribe);
}
Future<void> UnsubscribeLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnUnsubscribe);
}
bool DeliveryMetricsExportToBigQueryEnabled() {
  if (!internal::IsInitialized()) {
    return g_delivery_metrics_export_to_big_query_state ==
           kDeliveryMetricsExportToBigQueryEnable;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(
          firebase_messaging::kDeliveryMetricsExportToBigQueryEnabled));
  return !::firebase::jni::CheckAndClearException(env) &&
         static_cast<bool>(result);
}
void SetDeliveryMetricsExportToBigQuery(bool enabled) {
  g_delivery_metrics_export_to_big_query_state =
      enabled ? kDeliveryMetricsExportToBigQueryEnable
              : kDeliveryMetricsExportToBigQueryDisable;
  if (internal::IsInitialized()) {
    JNIEnv* env = g_app->GetJNIEnv();
    env->CallVoidMethod(
        g_firebase_messaging,
        firebase_messaging::GetMethodId(
            firebase_messaging::kSetDeliveryMetricsExportToBigQuery),
        static_cast<jboolean>(enabled));
    ::firebase::jni::CheckAndClearException(env);
  }
}
void SetRegistrationOnInitEnabled(bool enabled) {
  SetTokenRegistrationOnInitEnabled(enabled);
}
bool IsRegistrationOnInitEnabled() {
  return IsTokenRegistrationOnInitEnabled();
}
void SetTokenRegistrationOnInitEnabled(bool enabled) {
  if (internal::IsInitialized()) {
    {
      MutexLock lock(g_registration_token_mutex);
      g_registration_token_request_state =
          enabled ? kRegistrationTokenRequestStateEnable
                  : kRegistrationTokenRequestStateDisable;
    }
    JNIEnv* env = g_app->GetJNIEnv();
    bool was_enabled = IsTokenRegistrationOnInitEnabled();
    env->CallVoidMethod(g_firebase_messaging,
                        firebase_messaging::GetMethodId(
                            firebase_messaging::kSetAutoInitEnabled),
                        static_cast<jboolean>(enabled));
    ::firebase::jni::CheckAndClearException(env);
    if (!was_enabled && IsTokenRegistrationOnInitEnabled()) {
      InstallationsGetToken();
    }
  } else {
    g_registration_token_request_state =
        enabled ? kRegistrationTokenRequestStateEnable
                : kRegistrationTokenRequestStateDisable;
  }
}
bool IsTokenRegistrationOnInitEnabled() {
  FIREBASE_ASSERT_MESSAGE(internal::IsInitialized(),
                          kMessagingNotInitializedError);
  if (!internal::IsInitialized()) return true;
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(firebase_messaging::kIsAutoInitEnabled));
  return !::firebase::jni::CheckAndClearException(env) &&
         static_cast<bool>(result);
}
static Future<void> CallVoidTaskMethod(MessagingFn fn,
                                       firebase_messaging::Method method) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(g_registration_token_mutex);
  JNIEnv* env = nullptr;
  jobject messaging_obj = AcquireMessagingLocalRef(&env);
  if (!messaging_obj) {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
    api->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle;
  {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) {
      env->DeleteLocalRef(messaging_obj);
      return Future<void>();
    }
    handle = FutureData::Get()->api()->SafeAlloc<void>(fn);
  }
  jobject task =
      env->CallObjectMethod(messaging_obj, firebase_messaging::GetMethodId(method));
  env->DeleteLocalRef(messaging_obj);
  std::string error = ::firebase::jni::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    firebase::util::RegisterCallbackOnTask(
        env, task, CompleteVoidCallback,
        reinterpret_cast<void*>(handle.get().id()), kApiIdentifier);
  } else {
    MutexLock future_lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorUnknown, error.c_str());
    }
  }
  if (task) env->DeleteLocalRef(task);
  ::firebase::jni::CheckAndClearException(env);
  MutexLock future_lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
  return MakeFuture(FutureData::Get()->api(), handle);
}
Future<void> Register() {
  return CallVoidTaskMethod(kMessagingFnRegister, firebase_messaging::kRegister);
}
Future<void> RegisterLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnRegister);
}
Future<void> Unregister() {
  return CallVoidTaskMethod(kMessagingFnUnregister,
                            firebase_messaging::kUnregister);
}
Future<void> UnregisterLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnUnregister);
}
Future<std::string> GetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<std::string>(),
                                 internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(g_registration_token_mutex);
  JNIEnv* env = nullptr;
  jobject messaging_obj = AcquireMessagingLocalRef(&env);
  if (!messaging_obj) {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) {
      return Future<std::string>();
    }
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<std::string> handle =
        api->SafeAlloc<std::string>(kMessagingFnGetToken);
    api->CompleteWithResult(handle, kErrorUnknown, "Messaging is uninitialized.",
                            std::string());
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<std::string> handle;
  {
    MutexLock future_lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) {
      env->DeleteLocalRef(messaging_obj);
      return Future<std::string>();
    }
    handle =
        FutureData::Get()->api()->SafeAlloc<std::string>(kMessagingFnGetToken);
  }
  jobject task = env->CallObjectMethod(
      messaging_obj,
      firebase_messaging::GetMethodId(firebase_messaging::kGetToken));
  env->DeleteLocalRef(messaging_obj);
  std::string error = ::firebase::jni::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    firebase::util::RegisterCallbackOnTask(
        env, task, CompleteStringCallback,
        reinterpret_cast<void*>(handle.get().id()), kApiIdentifier);
  } else {
    MutexLock future_lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->CompleteWithResult(
          handle, kErrorUnknown, error.c_str(), std::string());
    }
  }
  if (task) env->DeleteLocalRef(task);
  ::firebase::jni::CheckAndClearException(env);
  MutexLock future_lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<std::string>();
  }
  return MakeFuture(FutureData::Get()->api(), handle);
}
Future<std::string> GetTokenLastResult() {
  return GetMessagingLastResult<std::string>(kMessagingFnGetToken);
}
Future<void> DeleteToken() {
  return CallVoidTaskMethod(kMessagingFnDeleteToken,
                            firebase_messaging::kDeleteToken);
}
Future<void> DeleteTokenLastResult() {
  return GetMessagingLastResult<void>(kMessagingFnDeleteToken);
}
}  // namespace messaging
}  // namespace firebase
