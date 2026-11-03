// Copyright 2016 Google LLC
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

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "messaging/messaging_generated.h"
#include "messaging/src/android/cpp/message_reader.h"
#include "messaging/src/android/cpp/messaging_internal.h"
#include "messaging/src/common.h"

// This is the size the buffer needs to be to contain a single event, which is
// all we need for messaging.
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

// Used to retrieve the JNI environment in order to call methods on the
// Android Analytics class.
static const ::firebase::App* g_app = nullptr;

// Mutex used to arbitrate access to g_app between the thread calling this
// module and MessageProcessingThread().
static Mutex g_app_mutex;

// Global reference to the Firebase Cloud Messaging instance.
// This is initialized in messaging::Initialize() and never released
// during the lifetime of the application.
static jobject g_firebase_messaging = nullptr;

// Global flag indicating if data collection was requested enabled or disabled
// before app initialization.
enum RegistrationTokenRequestState {
  kRegistrationTokenRequestStateNone,
  kRegistrationTokenRequestStateEnable,
  kRegistrationTokenRequestStateDisable,
};
static RegistrationTokenRequestState g_registration_token_request_state =
    kRegistrationTokenRequestStateNone;

// Global flag indicating if metrics export to big query was enabled or
// disabled before app initialization.
enum DeliveryMetricsExportToBigQueryState {
  kDeliveryMetricsExportToBigQueryNone,
  kDeliveryMetricsExportToBigQueryEnable,
  kDeliveryMetricsExportToBigQueryDisable,
};
static DeliveryMetricsExportToBigQueryState
    g_delivery_metrics_export_to_big_query_state =
        kDeliveryMetricsExportToBigQueryNone;

// Indicates whether a registration token has been received, which is necessary
// to perform certain actions related to topic subscriptions.
// NOTE: The registration token is received by RegistrationIntentService which
// can happen any time outside of the lifetime of this module (i.e between
// Initialize(), Terminate()) so this flag is set only one for the lifetime of
// the app.
static bool g_registration_token_received = false;
// Recursive mutex which controls access of:
// * g_registration_token_received
// * g_pending_topic_ops
static std::unique_ptr<Mutex> g_registration_token_mutex;

// Calls to Subscribe/Unsubscribe can happen before the registration token
// has been received, and thus can't be handled immediately. Queue them in
// chronological FIFO order to be handled once the token arrives.
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

static std::unique_ptr<Mutex> g_file_locker_mutex;

// Mutex for managing if FutureData is safe to use, or if it has been deleted.
static Mutex g_future_data_mutex;

static const char* kMessagingNotInitializedError = "Messaging not initialized.";

static void HandlePendingSubscriptions();

static void InstallationsGetToken();

// The background messaging polling thread.
static pthread_t g_poll_thread;
static bool g_poll_thread_started = false;

// Whether the intent message has been fired.
static bool g_intent_message_fired = false;

// Used to setup the cache of FirebaseMessaging class method IDs to reduce time
// spent looking up methods by string.
// clang-format off
#define FIREBASE_MESSAGING_METHODS(X)                                          \
    X(IsAutoInitEnabled, "isAutoInitEnabled", "()Z"),                          \
    X(SetAutoInitEnabled, "setAutoInitEnabled", "(Z)V"),                       \
    X(SubscribeToTopic, "subscribeToTopic",                                    \
      "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;"),              \
    X(UnsubscribeFromTopic, "unsubscribeFromTopic",                            \
      "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;"),              \
    X(GetInstance, "getInstance",                                              \
      "()Lcom/google/firebase/messaging/FirebaseMessaging;",                   \
      firebase::util::kMethodTypeStatic),                                      \
    X(DeliveryMetricsExportToBigQueryEnabled,                                  \
      "deliveryMetricsExportToBigQueryEnabled", "()Z"),                        \
    X(SetDeliveryMetricsExportToBigQuery,                                      \
      "setDeliveryMetricsExportToBigQuery", "(Z)V"),                           \
    X(GetToken, "getToken", "()Lcom/google/android/gms/tasks/Task;"),          \
    X(DeleteToken, "deleteToken", "()Lcom/google/android/gms/tasks/Task;"),    \
    X(Register, "register", "()Lcom/google/android/gms/tasks/Task;"),          \
    X(Unregister, "unregister", "()Lcom/google/android/gms/tasks/Task;")
// clang-format on
METHOD_LOOKUP_DECLARATION(firebase_messaging, FIREBASE_MESSAGING_METHODS);
METHOD_LOOKUP_DEFINITION(firebase_messaging,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/messaging/FirebaseMessaging",
                         FIREBASE_MESSAGING_METHODS);

static void FireIntentMessage(JNIEnv* env);

// Acquires a lock on a lock file and releases when this object goes out
// of scope.
FileLocker::FileLocker(const char* lock_filename)
    : lock_filename_(lock_filename), lock_file_descriptor_(-1) {
  lock_file_descriptor_ = AcquireLock(lock_filename_);
}

// Release the lock, if it was successfully acquired.
FileLocker::~FileLocker() {
  if (lock_file_descriptor_ >= 0) {
    ReleaseLock(lock_filename_, lock_file_descriptor_);
  }
}

// Acquires a lock on the lockfile which acts as a mutex between separate
// processes. We use this to prevent race conditions when writing or consuming
// the contents of the storage file. This should be called at the beginning
// of a critical section.
int FileLocker::AcquireLock(const char* lock_filename) {
  if (g_file_locker_mutex) {
    g_file_locker_mutex->Acquire();
  }
  int fd = open(lock_filename, O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    if (g_file_locker_mutex) {
      g_file_locker_mutex->Release();
    }
    return -1;
  }
  int lock_result = 0;
  do {
    lock_result = flock(fd, LOCK_EX);
  } while (lock_result < 0 && errno == EINTR);
  if (lock_result < 0) {
    close(fd);
    if (g_file_locker_mutex) {
      g_file_locker_mutex->Release();
    }
    return -1;
  }
  return fd;
}

// Releases the lock on the lockfile. This should be called at the end of a
// critical section.
void FileLocker::ReleaseLock(const char* lock_filename, int fd) {
  (void)lock_filename;
  if (fd >= 0) {
    flock(fd, LOCK_UN);
    close(fd);
  }
  if (g_file_locker_mutex) {
    g_file_locker_mutex->Release();
  }
}

// Lock the file referenced by g_lockfile_path.
class MessageLockFileLocker : private FileLocker {
 public:
  MessageLockFileLocker(const char* lockfile_path = g_lockfile_path->c_str()) : FileLocker(lockfile_path) {}
  ~MessageLockFileLocker() {}
};

static bool LoadFile(const char* name, std::string* buf) {
  if (!name || !buf) return false;
  FILE* file = fopen(name, "rb");
  if (file == nullptr) return false;
  if (fseek(file, 0L, SEEK_END) != 0) {
    fclose(file);
    return false;
  }
  long file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return false;
  }
  if (fseek(file, 0L, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }
  buf->resize(static_cast<size_t>(file_size));
  if (file_size > 0) {
    size_t bytes_read = fread(&(*buf)[0], 1, buf->size(), file);
    if (bytes_read != buf->size()) {
      fclose(file);
      buf->clear();
      return false;
    }
  }
  bool success = ferror(file) == 0;
  success = (fclose(file) == 0) && success;
  return success;
}

static void ConsumeEvents() {
  if (!g_local_storage_file_path || !g_lockfile_path) return;
  // Read file contents into a buffer.
  std::string buffer;
  {
    MessageLockFileLocker file_lock;
    FIREBASE_ASSERT_RETURN_VOID(
        LoadFile(g_local_storage_file_path->c_str(), &buffer));

    // Clear the file if there was anything in it.
    if (!buffer.empty()) {
      // Truncate in-place first to avoid triggering an extra IN_CLOSE_WRITE
      // notification; fall back to fopen("w") if truncate fails.
      if (truncate(g_local_storage_file_path->c_str(), 0) != 0) {
        FILE* erase_data_file = fopen(g_local_storage_file_path->c_str(), "w");
        if (erase_data_file != nullptr) {
          fclose(erase_data_file);
        }
      }
    }
  }
  if (buffer.empty()) return;

  internal::MessageReader reader(
      [](const Message& message, void* callback_data) {
        NotifyListenerOnMessage(message);
      },
      nullptr,
      [](const char* token, void* callback_data) {
        if (g_registration_token_mutex) {
          MutexLock lock(*g_registration_token_mutex);
          g_registration_token_received = true;
          HandlePendingSubscriptions();
        }
        NotifyListenerOnTokenReceived(token);
      },
      nullptr,
      [](const char* installationId, void* callback_data) {
        if (g_registration_token_mutex) {
          MutexLock lock(*g_registration_token_mutex);
          g_registration_token_received = true;
          HandlePendingSubscriptions();
        }
        NotifyListenerOnRegistrationReceived(installationId);
      },
      nullptr,
      [](const char* installationId, void* callback_data) {
        NotifyListenerOnUnregistrationReceived(installationId);
      },
      nullptr);
  reader.ReadFromBuffer(buffer);
}

// Return true if Terminate() has been called. This is so that the background
// thread knows that it is time to quit.
static bool TerminateRequested() {
  bool result;
  MutexLock lock(g_app_mutex);
  // If the app is removed, Terminate() has been called.
  result = !g_app;
  return result;
}

// Wake up the message processing thread.
void NotifyListenerSet(Listener* listener) {
  if (!listener) return;
  std::string local_storage;
  std::string lockfile;
  {
    MutexLock lock(g_app_mutex);
    if (!g_app || !g_local_storage_file_path || !g_lockfile_path) return;
    local_storage = *g_local_storage_file_path;
    lockfile = *g_lockfile_path;
  }
  {
    MessageLockFileLocker file_lock(lockfile.c_str());
    FILE* storage_file = fopen(local_storage.c_str(), "a");
    if (storage_file != nullptr) {
      fclose(storage_file);
    }
  }
}

Future<void> RequestPermission() {
  FIREBASE_ASSERT_RETURN(RequestPermissionLastResult(),
                         internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  // No behavior on Android - immediately complete and return.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kMessagingFnRequestPermission);
  api->Complete(handle, kErrorNone);
  return MakeFuture(api, handle);
}

Future<void> RequestPermissionLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnRequestPermission));
}

// Process queued messages & tokens.
void ProcessMessages() {
  JNIEnv* env;
  {
    MutexLock lock(g_app_mutex);
    env = g_app ? g_app->GetJNIEnv() : nullptr;
  }
  if (HasListener() && env) {
    // Check to see if there was a message in the intent that started this
    // activity.
    FireIntentMessage(env);
    ConsumeEvents();
  }
}

// Each message that the ListenerService receives from the cloud server is
// converted to a flatbuffer and stored in a file. This thread listens for
// changes to that file and when messages are written it relays them to the
// OnMessage callback.
static void* MessageProcessingThread(void*) {
  JavaVM* jvm = nullptr;
  {
    MutexLock lock(g_app_mutex);
    jvm = g_app ? g_app->java_vm() : nullptr;
    if (jvm == nullptr || !g_local_storage_file_path) return nullptr;
  }

  // Set up inotify.
  int file_descriptor = inotify_init();
  FIREBASE_ASSERT_RETURN(nullptr, file_descriptor >= 0);
  int watch_descriptor = inotify_add_watch(
      file_descriptor, g_local_storage_file_path->c_str(), IN_CLOSE_WRITE);
  if (watch_descriptor < 0) {
    close(file_descriptor);
    FIREBASE_ASSERT_RETURN(nullptr, false);
  }
  char buf[INOTIFY_EVENT_BUFFER_SIZE]
      __attribute__((aligned(__alignof__(inotify_event))));

  // Consume any messages received while this thread wasn't running.
  ProcessMessages();

  while (!TerminateRequested()) {
    // Wait for inotify event.
    ssize_t numRead = read(file_descriptor, buf, sizeof(buf));
    // If Terminate has been requested, finish the thread.
    if (TerminateRequested()) {
      break;
    }
    if (numRead <= 0) {
      // It's possible to get EINTR here on some Android versions if the
      // app was brought to the foreground.
      LogDebug("Reading message file, errno=%d", errno);
      ProcessMessages();
      continue;
    }

    // Respond to inotify event(s). ProcessMessages() consumes the entire
    // storage file under lock, so one call handles the full batch.
    ProcessMessages();
  }
  inotify_rm_watch(file_descriptor, watch_descriptor);
  close(file_descriptor);
  JNIEnv* attached_env = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&attached_env), JNI_VERSION_1_6) ==
          JNI_OK &&
      attached_env != nullptr) {
    jvm->DetachCurrentThread();
  }
  return nullptr;
}

// Wake up the thread, wait for it to complete and clean up resources.
static void TerminateMessageProcessingThread() {
  if (!g_poll_thread_started) return;
  if (g_local_storage_file_path && g_lockfile_path) {
    MessageLockFileLocker file_lock;
    FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
    if (storage_file != nullptr) {
      fclose(storage_file);
    } else {
      LogWarning("Failed to open storage file to wake message thread.");
    }
  }
  pthread_join(g_poll_thread, nullptr);
  g_poll_thread_started = false;
}

static bool StringStartsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool StringEquals(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

// This is duplicating the checking done in
// com.google.firebase.messaging.RemoteMessage.getData().
static bool IsValidKey(const char* key) {
  return !StringStartsWith(key, kReservedPrefix) &&
         !StringStartsWith(key, kGcmPrefix) && !StringEquals(key, kExtraFrom) &&
         !StringEquals(key, kExtraMessageType) &&
         !StringEquals(key, kExtraCollapseKey);
}

// Converts an `android.os.Bundle` to an `std::map<std::string, std::string>`.
static void BundleToMessageData(JNIEnv* env,
                                std::map<std::string, std::string>* to,
                                jobject from) {
  // Set<String> key_set = from.keySet();
  jobject key_set = env->CallObjectMethod(
      from, util::bundle::GetMethodId(util::bundle::kKeySet));
  if (util::CheckAndClearJniExceptions(env) || !key_set) return;
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set, util::set::GetMethodId(util::set::kIterator));
  if (util::CheckAndClearJniExceptions(env) || !iter) {
    env->DeleteLocalRef(key_set);
    return;
  }
  // while (iter.hasNext())
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    if (util::CheckAndClearJniExceptions(env)) break;
    // String key = iter.next();
    jobject key_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    if (util::CheckAndClearJniExceptions(env) || !key_object) break;
    const char* key =
        env->GetStringUTFChars(static_cast<jstring>(key_object), nullptr);
    if (key != nullptr && IsValidKey(key)) {
      // String value = from.getString(key);
      jstring value_jstring = static_cast<jstring>(env->CallObjectMethod(
          from, util::bundle::GetMethodId(util::bundle::kGetString),
          key_object));
      if (!util::CheckAndClearJniExceptions(env) && value_jstring != nullptr) {
        (*to)[key] = util::JniStringToString(env, value_jstring);
      }
    }
    if (key != nullptr) {
      env->ReleaseStringUTFChars(static_cast<jstring>(key_object), key);
    }
    env->DeleteLocalRef(key_object);
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

// Returns the string associated with the given key in the bundle.
static std::string BundleGetString(JNIEnv* env, jobject bundle,
                                   const char* key) {
  jstring key_jstring = env->NewStringUTF(key);
  jstring value_jstring = static_cast<jstring>(env->CallObjectMethod(
      bundle, util::bundle::GetMethodId(util::bundle::kGetString),
      key_jstring));
  util::CheckAndClearJniExceptions(env);
  std::string value = util::JniStringToString(env, value_jstring);
  env->DeleteLocalRef(key_jstring);
  return value;
}

static void FireIntentMessage(JNIEnv* env) {
  // TODO(amablue): Change this to expose a Firebase Messaging specific method
  // to set the Intent as the app can continue to run (i.e without a restart)
  // when retrieving a notification via an Intent. http://b/32316101
  if (g_intent_message_fired || !HasListener() || !env) return;
  jobject activity;
  {
    MutexLock lock(g_app_mutex);
    if (!g_app) return;
    activity = env->NewLocalRef(g_app->activity());
    if (util::CheckAndClearJniExceptions(env) || !activity) return;
  }
  g_intent_message_fired = true;
  // Intent intent = app.getIntent();
  jobject intent = env->CallObjectMethod(
      activity, util::activity::GetMethodId(util::activity::kGetIntent));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(activity);

  if (intent != nullptr) {
    // Bundle bundle = intent.getExtras();
    jobject bundle = env->CallObjectMethod(
        intent, util::intent::GetMethodId(util::intent::kGetExtras));
    util::CheckAndClearJniExceptions(env);
    if (bundle != nullptr) {
      Message message;
      message.message_id = BundleGetString(env, bundle, kExtraMessageId);
      if (message.message_id.empty()) {
        message.message_id =
            BundleGetString(env, bundle, kExtraMessageIdServer);
      }
      message.from = BundleGetString(env, bundle, kExtraFrom);
      // All Bundles representing a message should contain at least a
      // message_id field (contained in either the key "message_id" or
      // "google.message_id") and a "from" field.
      //
      // This check is needed because when starting up the app manually (that
      // is, when starting it up without tapping on a notification), the intent
      // passes a bundle to the app containing an assortment of data that is
      // interpreted as message data unless we filter it out. By checking for
      // specific fields we expect to present we can filter out these false
      // positives.
      if (message.message_id.size() && message.from.size()) {
        message.to = BundleGetString(env, bundle, kExtraTo);
        message.message_type = BundleGetString(env, bundle, kExtraMessageType);
        message.collapse_key = BundleGetString(env, bundle, kExtraCollapseKey);
        BundleToMessageData(env, &message.data, bundle);
        message.notification_opened = true;

        // Check to see if we have a deep link on the intent.
        jobject uri_object = env->CallObjectMethod(
            intent, util::intent::GetMethodId(util::intent::kGetData));
        util::CheckAndClearJniExceptions(env);
        message.link = util::JniUriToString(env, uri_object);

        NotifyListenerOnMessage(message);
      }

      env->DeleteLocalRef(bundle);
    }
    env->DeleteLocalRef(intent);
  }
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
  if (!util::Initialize(env, app.activity())) {
    return kInitResultFailedMissingDependency;
  }

  // Cache method pointers.
  if (!firebase_messaging::CacheMethodIds(env, app.activity())) {
    ReleaseClasses(env);
    util::Terminate(env);
    LogError("Failed to initialize messaging");  // DEBUG
    return kInitResultFailedMissingDependency;
  }

  // Cache the local storage file and lockfile before publishing g_app.
  jobject file = env->CallObjectMethod(
      app.activity(), util::context::GetMethodId(util::context::kGetFilesDir));
  if (util::CheckAndClearJniExceptions(env) || !file) {
    if (file) env->DeleteLocalRef(file);
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }
  jstring path_jstring = static_cast<jstring>(env->CallObjectMethod(
      file, util::file::GetMethodId(util::file::kGetPath)));
  if (util::CheckAndClearJniExceptions(env) || !path_jstring) {
    if (path_jstring) env->DeleteLocalRef(path_jstring);
    env->DeleteLocalRef(file);
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }
  std::string local_storage_dir = util::JniStringToString(env, path_jstring);
  env->DeleteLocalRef(file);

  // Get / create the Firebase Cloud Messaging singleton.
  jobject firebase_messaging_instance = env->CallStaticObjectMethod(
      firebase_messaging::GetClass(),
      firebase_messaging::GetMethodId(firebase_messaging::kGetInstance));
  if (util::CheckAndClearJniExceptions(env) || !firebase_messaging_instance) {
    if (firebase_messaging_instance) {
      env->DeleteLocalRef(firebase_messaging_instance);
    }
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  // Keep a reference to the firebase messaging singleton.
  g_firebase_messaging = env->NewGlobalRef(firebase_messaging_instance);
  env->DeleteLocalRef(firebase_messaging_instance);
  if (util::CheckAndClearJniExceptions(env) || !g_firebase_messaging) {
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  g_registration_token_mutex = std::make_unique<Mutex>();
  g_file_locker_mutex = std::make_unique<Mutex>();
  g_pending_topic_ops = std::make_unique<std::vector<PendingTopicOp>>();
  g_intent_message_fired = false;
  g_lockfile_path = std::make_unique<std::string>(local_storage_dir + "/" + kLockfile);
  g_local_storage_file_path =
      std::make_unique<std::string>(local_storage_dir + "/" + kStorageFile);

  // Ensure storage file exists.
  FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
  FIREBASE_ASSERT(storage_file != nullptr);
  if (storage_file != nullptr) {
    fclose(storage_file);
  }

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
    // Calling this again, now that we're initialized.
    assert(internal::IsInitialized());
    SetTokenRegistrationOnInitEnabled(g_registration_token_request_state ==
                                      kRegistrationTokenRequestStateEnable);
  }

  if (g_delivery_metrics_export_to_big_query_state !=
      kDeliveryMetricsExportToBigQueryNone) {
    // Calling this again, now that we're initialized.
    assert(internal::IsInitialized());
    SetDeliveryMetricsExportToBigQuery(
        g_delivery_metrics_export_to_big_query_state ==
        kDeliveryMetricsExportToBigQueryEnable);
  }

  // Kick off polling thread after FutureData and JNI state are ready.
  int result =
      pthread_create(&g_poll_thread, nullptr, MessageProcessingThread, nullptr);
  FIREBASE_ASSERT(result == 0);
  g_poll_thread_started = (result == 0);

  // Supposedly App creation also creates a registration token, but this seems
  // to happen before the C++ listeners are able to capture it.
  // So this may seem redundant but at least both are respecting the same flag
  // so it should be GDPR compliant.
  if (IsTokenRegistrationOnInitEnabled()) {
    // Request a registration token.
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
  util::CancelCallbacks(env, kApiIdentifier);
  // Dereference the app.
  {
    MutexLock lock(g_app_mutex);
    g_app = nullptr;
  }
  TerminateMessageProcessingThread();

  // g_registration_token_mutex.reset();
  g_file_locker_mutex.reset();
  g_pending_topic_ops.reset();
  g_local_storage_file_path.reset();
  g_lockfile_path.reset();

  g_registration_token_received = false;
  g_registration_token_request_state = kRegistrationTokenRequestStateNone;
  g_delivery_metrics_export_to_big_query_state =
      kDeliveryMetricsExportToBigQueryNone;
  g_intent_message_fired = false;

  env->DeleteGlobalRef(g_firebase_messaging);
  g_firebase_messaging = nullptr;
  SetListener(nullptr);
  ReleaseClasses(env);
  util::Terminate(env);
  {
    MutexLock lock(g_future_data_mutex);
    FutureData::Destroy();
  }
}

// Start a service which will communicate with the Firebase Cloud Messaging
// server to generate a registration token for the app.
static void InstallationsGetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(internal::IsInitialized(),
                                      kMessagingNotInitializedError);
  Future<std::string> result = GetToken();

  result.OnCompletion(
      [](const Future<std::string>& result, void* voidptr) {
        (void)voidptr;
        if (result.error() == kErrorNone && result.result() != nullptr &&
            !result.result()->empty()) {
          MutexLock app_lock(g_app_mutex);
          if (g_app && g_registration_token_mutex) {
            MutexLock lock(*g_registration_token_mutex);
            g_registration_token_received = true;
            HandlePendingSubscriptions();
          }
          NotifyListenerOnTokenReceived(result.result()->c_str());
        }
      },
      nullptr);
}

static void SubscriptionUpdateComplete(JNIEnv* env, jobject result,
                                       util::FutureResult result_code,
                                       const char* status_message,
                                       void* callback_data) {
  FutureHandleId future_id = reinterpret_cast<FutureHandleId>(callback_data);
  auto handle = SafeFutureHandle<void>(FutureHandle(future_id));
  Error error =
      (result_code == util::kFutureResultSuccess) ? kErrorNone : kErrorUnknown;
  {
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      ReferenceCountedFutureImpl* api = FutureData::Get()->api();
      api->Complete(handle, error, status_message);
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
  jobject messaging_obj = nullptr;
  {
    MutexLock lock(g_app_mutex);
    if (g_app && g_firebase_messaging) {
      env = g_app->GetJNIEnv();
      if (env) {
        messaging_obj = env->NewLocalRef(g_firebase_messaging);
        if (util::CheckAndClearJniExceptions(env)) {
          if (messaging_obj) env->DeleteLocalRef(messaging_obj);
          messaging_obj = nullptr;
        }
      }
    }
  }
  if (!messaging_obj) {
    MutexLock lock(g_future_data_mutex);
    if (FutureData* fd = FutureData::Get()) {
      if (fd->api()) {
        fd->api()->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
      }
    }
    return;
  }
  jstring java_topic = env->NewStringUTF(topic);
  jobject result = env->CallObjectMethod(
      messaging_obj, firebase_messaging::GetMethodId(method), java_topic);
  env->DeleteLocalRef(messaging_obj);
  // If this function threw an exception, it is almost certainly due to an
  // invalid topic name, so we should complete the future with the proper error.
  if (env->ExceptionCheck()) {
    jobject exception = static_cast<jobject>(env->ExceptionOccurred());
    env->ExceptionClear();
    std::string message = util::GetMessageFromException(env, exception);
    env->DeleteLocalRef(exception);
    if (result) env->DeleteLocalRef(result);
    MutexLock lock(g_future_data_mutex);
    if (FutureData::Get() && FutureData::Get()->api()) {
      FutureData::Get()->api()->Complete(handle, kErrorInvalidTopicName,
                                         message.c_str());
    }
  } else if (result) {
    util::RegisterCallbackOnTask(
        env, result, SubscriptionUpdateComplete,
        reinterpret_cast<void*>(handle.get().id()),
        kApiIdentifier);
    util::CheckAndClearJniExceptions(env);

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

// NOTE: Should be called when g_registration_token_mutex is held.
static void HandlePendingSubscriptions() {
  assert(g_registration_token_mutex);
  if (g_pending_topic_ops) {
    for (const auto& op : *g_pending_topic_ops) {
      if (op.type == kPendingTopicSubscribe) {
        SubscribeInternal(op.topic.c_str(), op.handle);
      } else {
        UnsubscribeInternal(op.topic.c_str(), op.handle);
      }
    }
    g_pending_topic_ops->clear();
  }
}

static void CompleteVoidCallback(JNIEnv* env, jobject result,
                                 util::FutureResult result_code,
                                 const char* status_message,
                                 void* callback_data) {
  FutureHandleId future_id = reinterpret_cast<FutureHandleId>(callback_data);
  FutureHandle handle(future_id);
  Error error =
      (result_code == util::kFutureResultSuccess) ? kErrorNone : kErrorUnknown;
  MutexLock lock(g_future_data_mutex);
  if (FutureData::Get() && FutureData::Get()->api()) {
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    api->Complete(handle, error, status_message);
  } else {
    LogWarning("Failed to complete Future as it was likely already deleted.");
  }
  if (result) env->DeleteLocalRef(result);
}

static void CompleteStringCallback(JNIEnv* env, jobject result,
                                   util::FutureResult result_code,
                                   const char* status_message,
                                   void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value;
  if (success && result) {
    result_value = util::JniStringToString(env, result);
  }
  if (result) {
    env->DeleteLocalRef(result);
  }
  FutureHandleId future_id = reinterpret_cast<FutureHandleId>(callback_data);
  auto handle = SafeFutureHandle<std::string>(FutureHandle(future_id));
  Error error = success ? kErrorNone : kErrorUnknown;
  MutexLock lock(g_future_data_mutex);
  if (FutureData::Get() && FutureData::Get()->api()) {
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    api->CompleteWithResult(handle, error, status_message, result_value);
  } else {
    LogWarning("Failed to complete Future as it was likely already deleted.");
  }
}

Future<void> Subscribe(const char* topic) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  bool initialized = false;
  {
    MutexLock app_lock(g_app_mutex);
    initialized = (g_app && g_registration_token_mutex && g_firebase_messaging);
  }
  if (!initialized) {
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnSubscribe);
    api->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
    return MakeFuture(api, handle);
  }
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnSubscribe);
  if (!topic || topic[0] == '\0') {
    api->Complete(handle, kErrorInvalidTopicName,
                  kErrorMessageInvalidTopicName);
  } else if (g_registration_token_received) {
    SubscribeInternal(topic, handle);
  } else if (g_registration_token_request_state ==
             kRegistrationTokenRequestStateDisable) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
  } else if (g_pending_topic_ops) {
    g_pending_topic_ops->emplace_back(kPendingTopicSubscribe, topic, handle);
  } else {
    api->Complete(handle, kErrorUnknown, kErrorMessageSubscriptionUnknown);
  }
  return MakeFuture(api, handle);
}

Future<void> SubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnSubscribe));
}

Future<void> Unsubscribe(const char* topic) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  bool initialized = false;
  {
    MutexLock app_lock(g_app_mutex);
    initialized = (g_app && g_registration_token_mutex && g_firebase_messaging);
  }
  if (!initialized) {
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnUnsubscribe);
    api->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
    return MakeFuture(api, handle);
  }
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnUnsubscribe);
  if (!topic || topic[0] == '\0') {
    api->Complete(handle, kErrorInvalidTopicName,
                  kErrorMessageInvalidTopicName);
  } else if (g_registration_token_received) {
    UnsubscribeInternal(topic, handle);
  } else if (g_registration_token_request_state ==
             kRegistrationTokenRequestStateDisable) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
  } else if (g_pending_topic_ops) {
    g_pending_topic_ops->emplace_back(kPendingTopicUnsubscribe, topic, handle);
  } else {
    api->Complete(handle, kErrorUnknown, kErrorMessageSubscriptionUnknown);
  }
  return MakeFuture(api, handle);
}

Future<void> UnsubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnUnsubscribe));
}

bool DeliveryMetricsExportToBigQueryEnabled() {
  if (!internal::IsInitialized()) {
    // If the user previously called SetDeliveryMetricsExportToBigQuery(true),
    // then return true. If they did not set it, or set it to false, return
    // false.
    return g_delivery_metrics_export_to_big_query_state ==
           kDeliveryMetricsExportToBigQueryEnable;
  }

  JNIEnv* env = g_app->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(
          firebase_messaging::kDeliveryMetricsExportToBigQueryEnabled));
  if (util::CheckAndClearJniExceptions(env)) {
    return false;
  }
  return static_cast<bool>(result);
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
    util::CheckAndClearJniExceptions(env);
  }
}

void SetRegistrationOnInitEnabled(bool enabled) {
  SetTokenRegistrationOnInitEnabled(enabled);
}

bool IsRegistrationOnInitEnabled() {
  return IsTokenRegistrationOnInitEnabled();
}

void SetTokenRegistrationOnInitEnabled(bool enabled) {
  // Always keep g_registration_token_request_state in sync so Subscribe() and
  // Unsubscribe() reflect runtime changes to auto-init state.
  if (internal::IsInitialized()) {
    {
      MutexLock lock(*g_registration_token_mutex);
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
    util::CheckAndClearJniExceptions(env);

    // TODO(b/77659307): This shouldn't be required, but the native API
    // doesn't raise the event when flipping the bit to true, so we watch for
    // that here.
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
  if (util::CheckAndClearJniExceptions(env)) {
    return false;
  }
  return static_cast<bool>(result);
}

static Future<void> CallVoidTaskMethod(MessagingFn fn,
                                       firebase_messaging::Method method) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  JNIEnv* env = nullptr;
  jobject messaging_obj = nullptr;
  {
    MutexLock app_lock(g_app_mutex);
    if (g_app && g_firebase_messaging) {
      env = g_app->GetJNIEnv();
      if (env) {
        messaging_obj = env->NewLocalRef(g_firebase_messaging);
      }
    }
  }

  if (!messaging_obj || !g_registration_token_mutex) {
    if (messaging_obj) env->DeleteLocalRef(messaging_obj);
    MutexLock lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) return Future<void>();
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
    api->Complete(handle, kErrorUnknown, "Messaging is uninitialized.");
    return MakeFuture(api, handle);
  }

  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);

  jobject task = env->CallObjectMethod(
      messaging_obj, firebase_messaging::GetMethodId(method));
  env->DeleteLocalRef(messaging_obj);

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    util::RegisterCallbackOnTask(env, task, CompleteVoidCallback,
                                 reinterpret_cast<void*>(handle.get().id()),
                                 kApiIdentifier);
  } else {
    api->Complete(handle, kErrorUnknown, error.c_str());
  }
  if (task) env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(api, handle);
}

Future<void> Register() {
  return CallVoidTaskMethod(kMessagingFnRegister,
                            firebase_messaging::kRegister);
}

Future<void> RegisterLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnRegister));
}

Future<void> Unregister() {
  return CallVoidTaskMethod(kMessagingFnUnregister,
                            firebase_messaging::kUnregister);
}

Future<void> UnregisterLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnUnregister));
}

Future<std::string> GetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<std::string>(),
                                 internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  JNIEnv* env = nullptr;
  jobject messaging_obj = nullptr;
  {
    MutexLock app_lock(g_app_mutex);
    if (g_app && g_firebase_messaging) {
      env = g_app->GetJNIEnv();
      if (env) {
        messaging_obj = env->NewLocalRef(g_firebase_messaging);
      }
    }
  }

  if (!messaging_obj || !g_registration_token_mutex) {
    if (messaging_obj) env->DeleteLocalRef(messaging_obj);
    MutexLock lock(g_future_data_mutex);
    if (!FutureData::Get() || !FutureData::Get()->api()) return Future<std::string>();
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    SafeFutureHandle<std::string> handle = api->SafeAlloc<std::string>(kMessagingFnGetToken);
    api->CompleteWithResult(handle, kErrorUnknown, "Messaging is uninitialized.", std::string());
    return MakeFuture(api, handle);
  }

  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<std::string> handle =
      api->SafeAlloc<std::string>(kMessagingFnGetToken);

  jobject task = env->CallObjectMethod(
      messaging_obj,
      firebase_messaging::GetMethodId(firebase_messaging::kGetToken));
  env->DeleteLocalRef(messaging_obj);

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    util::RegisterCallbackOnTask(
        env, task, CompleteStringCallback,
        reinterpret_cast<void*>(handle.get().id()),
        kApiIdentifier);
  } else {
    api->CompleteWithResult(handle, kErrorUnknown, error.c_str(),
                            std::string());
  }
  if (task) env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(api, handle);
}

Future<std::string> GetTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<std::string>&>(
      api->LastResult(kMessagingFnGetToken));
}

Future<void> DeleteToken() {
  return CallVoidTaskMethod(kMessagingFnDeleteToken,
                            firebase_messaging::kDeleteToken);
}

Future<void> DeleteTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  MutexLock lock(g_future_data_mutex);
  if (!FutureData::Get() || !FutureData::Get()->api()) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnDeleteToken));
}

}  // namespace messaging
}  // namespace firebase
