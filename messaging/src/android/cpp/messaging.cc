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

#include <set>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
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
// * g_pending_subscriptions
// * g_pending_unsubscriptions
static Mutex* g_registration_token_mutex = nullptr;

// Calls to Subscribe/Unsubscribe can happen before the registration token
// has been received, and thus can't be handled immediately. If so, add to
// the appriopriate set to be handled later.
typedef std::pair<std::string, SafeFutureHandle<void>> PendingTopic;
static std::vector<PendingTopic>* g_pending_subscriptions;
static std::vector<PendingTopic>* g_pending_unsubscriptions;

static std::string* g_local_storage_file_path;
static std::string* g_lockfile_path;

static Mutex* g_file_locker_mutex;

static const char* kMessagingNotInitializedError = "Messaging not initialized.";

static void HandlePendingSubscriptions();

static void InstallationsGetToken();

// Condition variable / mutex pair used to control MessageProcessingThread()
// polling.  This allows another thread to interrupt MessageProcessingThread()
// while it's waiting.
static pthread_mutex_t g_thread_wait_mutex;
static pthread_cond_t g_thread_wait_cond;
// The background messaging polling thread.
static pthread_t g_poll_thread;

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
    X(DeleteToken, "deleteToken", "()Lcom/google/android/gms/tasks/Task;")
// clang-format on
METHOD_LOOKUP_DECLARATION(firebase_messaging, FIREBASE_MESSAGING_METHODS);
METHOD_LOOKUP_DEFINITION(firebase_messaging,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/messaging/FirebaseMessaging",
                         FIREBASE_MESSAGING_METHODS);

// Used to cache RegistrationIntentService methods.
// clang-format off
#define REGISTRATION_INTENT_SERVICE_METHODS(X) \
  X(Constructor, "<init>", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(registration_intent_service,
                          REGISTRATION_INTENT_SERVICE_METHODS);
METHOD_LOOKUP_DEFINITION(
    registration_intent_service,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/messaging/cpp/RegistrationIntentService",
    REGISTRATION_INTENT_SERVICE_METHODS);

// clang-format off
#define ILLEGAL_ARGUMENT_EXCEPTION_METHODS(X) \
  X(Constructor, "<init>", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(illegal_argument_exception,
                          ILLEGAL_ARGUMENT_EXCEPTION_METHODS);
METHOD_LOOKUP_DEFINITION(illegal_argument_exception,
                         PROGUARD_KEEP_CLASS
                         "java/lang/IllegalArgumentException",
                         ILLEGAL_ARGUMENT_EXCEPTION_METHODS);

static void FireIntentMessage(JNIEnv* env);

// Acquires a lock on a lock file and releases when this object goes out
// of scope.
FileLocker::FileLocker(const char* lock_filename)
    : lock_filename_(lock_filename), lock_file_descriptor_(-1) {
  lock_file_descriptor_ = AcquireLock(lock_filename_);
}

// Release the lock, if it was successfully required.
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
  mode_t m = umask(0);
  int fd = open(lock_filename, O_RDWR | O_CREAT, 0666);
  umask(m);
  if (fd < 0 || flock(fd, LOCK_EX) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

// Releases the lock on the lockfile. This should be called at the end of a
// critical section.
void FileLocker::ReleaseLock(const char* lock_filename, int fd) {
  if (fd >= 0) {
    remove(lock_filename);
    close(fd);
  }
  if (g_file_locker_mutex) {
    g_file_locker_mutex->Release();
  }
}

// Lock the file referenced by g_lockfile_path.
class MessageLockFileLocker : private FileLocker {
 public:
  MessageLockFileLocker() : FileLocker(g_lockfile_path->c_str()) {}
  ~MessageLockFileLocker() {}
};

static bool LoadFile(const char* name, std::string* buf) {
  FILE* file = fopen(name, "rb");
  if (file == nullptr) return false;
  fseek(file, 0L, SEEK_END);
  buf->resize(static_cast<size_t>(ftell(file)));
  fseek(file, 0L, SEEK_SET);
  fread(&(*buf)[0], buf->size(), 1, file);
  bool success = ferror(file) == 0;
  success &= fclose(file) == 0;
  return success;
}

static void ConsumeEvents() {
  // Read file contents into a buffer.
  std::string buffer;
  {
    MessageLockFileLocker file_lock;
    FIREBASE_ASSERT_RETURN_VOID(
        LoadFile(g_local_storage_file_path->c_str(), &buffer));

    // Clear the file if there was anything in it.
    if (buffer.size()) {
      // Clear the file by opening then closing without writing to it.
      FILE* erase_data_file = fopen(g_local_storage_file_path->c_str(), "w");
      fclose(erase_data_file);
    }
  }

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
  if (!listener || !g_app) return;
  {
    MessageLockFileLocker file_lock;
    FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
    if (storage_file != nullptr) {
      fclose(storage_file);
    }
  }
}

Future<void> RequestPermission() {
  FIREBASE_ASSERT_RETURN(RequestPermissionLastResult(),
                         internal::IsInitialized());
  // No behavior on Android - immediately complete and return.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kMessagingFnRequestPermission);
  api->Complete(handle, kErrorNone);
  return MakeFuture(api, handle);
}

Future<void> RequestPermissionLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
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
  JavaVM* jvm;
  {
    MutexLock lock(g_app_mutex);
    jvm = g_app ? g_app->java_vm() : nullptr;
    if (jvm == nullptr) return nullptr;
  }

  // Set up inotify.
  int file_descriptor = inotify_init();
  FIREBASE_ASSERT_RETURN(nullptr, file_descriptor >= 0);
  int watch_descriptor = inotify_add_watch(
      file_descriptor, g_local_storage_file_path->c_str(), IN_CLOSE_WRITE);
  FIREBASE_ASSERT_RETURN(nullptr, watch_descriptor >= 0);
  char buf[INOTIFY_EVENT_BUFFER_SIZE]
      __attribute__((aligned(__alignof__(inotify_event))));

  // Consume any messages received while this thread wasn't running.
  ProcessMessages();

  while (true) {
    // Wait for inotify event.
    ssize_t numRead = read(file_descriptor, buf, sizeof(buf));
    // If Terminate has been requested, finish the thread.
    if (TerminateRequested()) {
      return nullptr;
    }
    if (numRead <= 0) {
      // It's possible to get SIGINT here on some Android versions if the
      // app was bought to the foreground.
      LogDebug("Reading message file, errno=%d", errno);
      ProcessMessages();
      continue;
    }

    // Respond to inotify event.
    const inotify_event* event;
    for (int i = 0; i < numRead; i += sizeof(inotify_event) + event->len) {
      event = reinterpret_cast<inotify_event*>(&buf[i]);
      ProcessMessages();
    }
  }
  jvm->DetachCurrentThread();
  return nullptr;
}

// Wake up the thread, wait for it to complete and clean up resources.
static void TerminateMessageProcessingThread() {
  {
    MessageLockFileLocker file_lock;
    FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
    FIREBASE_ASSERT_RETURN_VOID(storage_file != nullptr);
    fclose(storage_file);
  }

  pthread_cond_signal(&g_thread_wait_cond);
  pthread_join(g_poll_thread, nullptr);
  pthread_mutex_destroy(&g_thread_wait_mutex);
  pthread_cond_destroy(&g_thread_wait_cond);
}

static bool StringStartsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool StringEquals(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

static void NotificationField(JNIEnv* env, std::string* field, jobject from,
                              const char* key) {
  jstring key_jstring = env->NewStringUTF(key);
  jstring value_jstring = static_cast<jstring>(env->CallObjectMethod(
      from, util::map::GetMethodId(util::map::kGet), key_jstring));
  assert(env->ExceptionCheck() == false);
  *field = util::JniStringToString(env, value_jstring);
  env->DeleteLocalRef(key_jstring);
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
  assert(env->ExceptionCheck() == false);
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set, util::set::GetMethodId(util::set::kIterator));
  assert(env->ExceptionCheck() == false);
  // while (iter.hasNext())
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    assert(env->ExceptionCheck() == false);
    // String key = iter.next();
    jobject key_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    assert(env->ExceptionCheck() == false);
    const char* key =
        env->GetStringUTFChars(static_cast<jstring>(key_object), nullptr);
    if (IsValidKey(key)) {
      // String value = from.getString(key);
      jstring value_jstring = static_cast<jstring>(env->CallObjectMethod(
          from, util::bundle::GetMethodId(util::bundle::kGetString),
          key_object));
      assert(env->ExceptionCheck() == false);
      (*to)[key] = util::JniStringToString(env, value_jstring);
    }
    env->ReleaseStringUTFChars(static_cast<jstring>(key_object), key);
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
  assert(env->ExceptionCheck() == false);
  std::string value = util::JniStringToString(env, value_jstring);
  env->DeleteLocalRef(key_jstring);
  return value;
}

static void FireIntentMessage(JNIEnv* env) {
  // TODO(amablue): Change this to expose a Firebase Messaging specific method
  // to set the Intent as the app can continue to run (i.e without a restart)
  // when retrieving a notification via an Intent. http://b/32316101
  if (g_intent_message_fired || !HasListener()) return;
  g_intent_message_fired = true;
  jobject activity;
  {
    MutexLock lock(g_app_mutex);
    if (!g_app) return;
    activity = env->NewLocalRef(g_app->activity());
    assert(env->ExceptionCheck() == false);
  }
  // Intent intent = app.getIntent();
  jobject intent = env->CallObjectMethod(
      activity, util::activity::GetMethodId(util::activity::kGetIntent));
  assert(env->ExceptionCheck() == false);
  env->DeleteLocalRef(activity);

  if (intent != nullptr) {
    // Bundle bundle = intent.getExtras();
    jobject bundle = env->CallObjectMethod(
        intent, util::intent::GetMethodId(util::intent::kGetExtras));
    assert(env->ExceptionCheck() == false);
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
  registration_intent_service::ReleaseClass(env);
}

InitResult Initialize(const ::firebase::App& app, Listener* listener) {
  return Initialize(app, listener, MessagingOptions());
}

InitResult Initialize(const ::firebase::App& app, Listener* listener,
                      const MessagingOptions& options) {
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
  if (!(firebase_messaging::CacheMethodIds(env, app.activity()) &&
        registration_intent_service::CacheMethodIds(env, app.activity()))) {
    ReleaseClasses(env);
    util::Terminate(env);
    LogError("Failed to initialize messaging");  // DEBUG
    return kInitResultFailedMissingDependency;
  }

  {
    MutexLock lock(g_app_mutex);
    g_app = &app;
  }
  g_registration_token_mutex = new Mutex();
  g_file_locker_mutex = new Mutex();
  g_pending_subscriptions = new std::vector<PendingTopic>();
  g_pending_unsubscriptions = new std::vector<PendingTopic>();
  g_intent_message_fired = false;

  // Cache the local storage file and lockfile.
  jobject file = env->CallObjectMethod(
      app.activity(), util::context::GetMethodId(util::context::kGetFilesDir));
  assert(env->ExceptionCheck() == false);
  jstring path_jstring = static_cast<jstring>(env->CallObjectMethod(
      file, util::file::GetMethodId(util::file::kGetPath)));
  assert(env->ExceptionCheck() == false);
  std::string local_storage_dir = util::JniStringToString(env, path_jstring);
  env->DeleteLocalRef(file);
  g_lockfile_path = new std::string(local_storage_dir + "/" + kLockfile);
  g_local_storage_file_path =
      new std::string(local_storage_dir + "/" + kStorageFile);

  // Ensure storage file exists.
  FILE* storage_file = fopen(g_local_storage_file_path->c_str(), "a");
  FIREBASE_ASSERT(storage_file != nullptr);
  fclose(storage_file);

  // Get / create the Firebase Cloud Messaging singleton.
  jobject firebase_messaging_instance = env->CallStaticObjectMethod(
      firebase_messaging::GetClass(),
      firebase_messaging::GetMethodId(firebase_messaging::kGetInstance));
  // In debug builds, after JNI method calls assert that no exception was
  // thrown so we can crash immediately instead of the next time a JNI method
  // call is made.
  assert(env->ExceptionCheck() == false);

  // Keep a reference to the firebase messaging singleton.
  g_firebase_messaging = env->NewGlobalRef(firebase_messaging_instance);
  FIREBASE_ASSERT(g_firebase_messaging);
  env->DeleteLocalRef(firebase_messaging_instance);

  // Kick off polling thread.
  g_thread_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
  g_thread_wait_cond = PTHREAD_COND_INITIALIZER;
  int result =
      pthread_create(&g_poll_thread, nullptr, MessageProcessingThread, nullptr);
  FIREBASE_ASSERT(result == 0);

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
    SetTokenRegistrationOnInitEnabled(
        g_delivery_metrics_export_to_big_query_state ==
        kDeliveryMetricsExportToBigQueryEnable);
  }

  FutureData::Create();

  // Supposedly App creation also creates a registration token, but this seems
  // to happen before the C++ listeners are able to capture it.
  // So this may seem redundant but at least both are respecting the same flag
  // so it should be GDPR compliant.
  if (IsTokenRegistrationOnInitEnabled()) {
    // Request a registration token.
    InstallationsGetToken();
  }

  LogInfo("Firebase Cloud Messaging API Initialized");
  internal::RegisterTerminateOnDefaultAppDestroy();

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
  // Dereference the app.
  {
    MutexLock lock(g_app_mutex);
    g_app = nullptr;
  }
  TerminateMessageProcessingThread();

  delete g_registration_token_mutex;
  g_registration_token_mutex = nullptr;
  delete g_file_locker_mutex;
  g_file_locker_mutex = nullptr;
  delete g_pending_subscriptions;
  g_pending_subscriptions = nullptr;
  delete g_pending_unsubscriptions;
  g_pending_unsubscriptions = nullptr;
  delete g_local_storage_file_path;
  g_local_storage_file_path = nullptr;
  delete g_lockfile_path;
  g_lockfile_path = nullptr;

  g_delivery_metrics_export_to_big_query_state =
      kDeliveryMetricsExportToBigQueryNone;

  env->DeleteGlobalRef(g_firebase_messaging);
  g_firebase_messaging = nullptr;
  SetListener(nullptr);
  ReleaseClasses(env);
  FutureData::Destroy();
  util::Terminate(env);
}

// Start a service which will communicate with the Firebase Cloud Messaging
// server to generate a registration token for the app.
static void InstallationsGetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(internal::IsInitialized(),
                                      kMessagingNotInitializedError);
  JNIEnv* env = g_app->GetJNIEnv();

  // Intent intent = new Intent(this, RegistrationIntentService.class);
  jobject new_intent = env->NewObject(
      util::intent::GetClass(),
      util::intent::GetMethodId(util::intent::kConstructor), g_app->activity(),
      registration_intent_service::GetClass());

  // startService(intent);
  jobject component_name = env->CallObjectMethod(
      g_app->activity(),
      util::context::GetMethodId(util::context::kStartService), new_intent);
  assert(env->ExceptionCheck() == false);
  env->DeleteLocalRef(component_name);
  env->DeleteLocalRef(new_intent);
}

static void SubscriptionUpdateComplete(JNIEnv* env, jobject result,
                                       util::FutureResult result_code,
                                       const char* status_message,
                                       void* callback_data) {
  SafeFutureHandle<void>* handle =
      reinterpret_cast<SafeFutureHandle<void>*>(callback_data);
  Error error =
      (result_code == util::kFutureResultSuccess) ? kErrorNone : kErrorUnknown;
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  api->Complete(*handle, error, status_message);
  delete handle;
}

static void SubscribeInternal(const char* topic,
                              const SafeFutureHandle<void>& handle) {
  assert(g_registration_token_received);
  LogDebug("Subscribe to topic %s", topic);

  JNIEnv* env = g_app->GetJNIEnv();
  jstring java_topic = env->NewStringUTF(topic);
  jobject result = env->CallObjectMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(firebase_messaging::kSubscribeToTopic),
      java_topic);
  // If this function threw an exception, it as almost ceratinly due to an
  // invalid topic name, so we should complete the future with the proper error.
  if (env->ExceptionCheck()) {
    jobject exception = static_cast<jobject>(env->ExceptionOccurred());
    env->ExceptionClear();
    std::string message = util::GetMessageFromException(env, exception);
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    api->Complete(handle, kErrorInvalidTopicName, message.c_str());
  } else if (result) {
    util::RegisterCallbackOnTask(
        env, result, SubscriptionUpdateComplete,
        reinterpret_cast<void*>(new SafeFutureHandle<void>(handle)),
        kApiIdentifier);
    util::CheckAndClearJniExceptions(env);

    env->DeleteLocalRef(result);
  }
  env->DeleteLocalRef(java_topic);
}

static void UnsubscribeInternal(const char* topic,
                                const SafeFutureHandle<void>& handle) {
  assert(g_registration_token_received);
  LogDebug("Unsubscribe from topic %s", topic);

  JNIEnv* env = g_app->GetJNIEnv();
  jstring java_topic = env->NewStringUTF(topic);
  jobject result =
      env->CallObjectMethod(g_firebase_messaging,
                            firebase_messaging::GetMethodId(
                                firebase_messaging::kUnsubscribeFromTopic),
                            java_topic);
  // If this function threw an exception, it as almost ceratinly due to an
  // invalid topic name, so we should complete the future with the proper error.
  if (env->ExceptionCheck()) {
    jobject exception = static_cast<jobject>(env->ExceptionOccurred());
    env->ExceptionClear();
    std::string message = util::GetMessageFromException(env, exception);
    ReferenceCountedFutureImpl* api = FutureData::Get()->api();
    api->Complete(handle, kErrorInvalidTopicName, message.c_str());
  } else if (result) {
    util::RegisterCallbackOnTask(
        env, result, SubscriptionUpdateComplete,
        reinterpret_cast<void*>(new SafeFutureHandle<void>(handle)),
        kApiIdentifier);
    util::CheckAndClearJniExceptions(env);

    env->DeleteLocalRef(result);
  }
  env->DeleteLocalRef(java_topic);
}

// NOTE: Should be called when g_registration_token_mutex is held.
static void HandlePendingSubscriptions() {
  assert(g_registration_token_mutex);
  if (g_pending_subscriptions) {
    for (auto it = g_pending_subscriptions->begin();
         it != g_pending_subscriptions->end(); ++it) {
      SubscribeInternal(it->first.c_str(), it->second);
    }
    g_pending_subscriptions->clear();
  }
  if (g_pending_unsubscriptions) {
    for (auto it = g_pending_unsubscriptions->begin();
         it != g_pending_unsubscriptions->end(); ++it) {
      UnsubscribeInternal(it->first.c_str(), it->second);
    }
    g_pending_unsubscriptions->clear();
  }
}

static void CompleteVoidCallback(JNIEnv* env, jobject result,
                                 util::FutureResult result_code,
                                 const char* status_message,
                                 void* callback_data) {
  FutureHandleId future_id =
                reinterpret_cast<FutureHandleId>(callback_data);
  FutureHandle handle(future_id);
  Error error =
      (result_code == util::kFutureResultSuccess) ? kErrorNone : kErrorUnknown;
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  api->Complete(handle, error, status_message);
  if (result) env->DeleteLocalRef(result);
}

static void CompleteStringCallback(JNIEnv* env, jobject result,
                                   util::FutureResult result_code,
                                   const char* status_message,
                                   void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_value = util::JniStringToString(env, result);
  }
  SafeFutureHandle<std::string>* handle =
      reinterpret_cast<SafeFutureHandle<std::string>*>(callback_data);
  Error error = success ? kErrorNone : kErrorUnknown;
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  api->CompleteWithResult(*handle, error, status_message, result_value);
  delete handle;
}

static const char kErrorMessageNoRegistrationToken[] =
    "Cannot update subscription when SetTokenRegistrationOnInitEnabled is set "
    "to false.";

Future<void> Subscribe(const char* topic) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnSubscribe);
  if (g_registration_token_received) {
    SubscribeInternal(topic, handle);
  } else if (g_registration_token_request_state ==
             kRegistrationTokenRequestStateDisable) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
  } else if (g_pending_subscriptions) {
    g_pending_subscriptions->push_back(PendingTopic(topic, handle));
  }
  return MakeFuture(api, handle);
}

Future<void> SubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnSubscribe));
}

Future<void> Unsubscribe(const char* topic) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnSubscribe);
  if (g_registration_token_received) {
    UnsubscribeInternal(topic, handle);
  } else if (g_registration_token_request_state ==
             kRegistrationTokenRequestStateDisable) {
    api->Complete(handle, kErrorNoRegistrationToken,
                  kErrorMessageNoRegistrationToken);
  } else if (g_pending_unsubscriptions) {
    g_pending_unsubscriptions->push_back(PendingTopic(topic, handle));
  }
  return MakeFuture(api, handle);
}

Future<void> UnsubscribeLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
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
  assert(env->ExceptionCheck() == false);
  return static_cast<bool>(result);
}

void SetDeliveryMetricsExportToBigQuery(bool enabled) {
  // If this is called before JNI is initialized, we'll just cache the intent
  // and handle it on actual init.
  // Otherwise if we've already initialized, the underlying API will persist the
  // value.
  if (internal::IsInitialized()) {
    JNIEnv* env = g_app->GetJNIEnv();
    env->CallVoidMethod(
        g_firebase_messaging,
        firebase_messaging::GetMethodId(
            firebase_messaging::kSetDeliveryMetricsExportToBigQuery),
        static_cast<jboolean>(enabled));
    assert(env->ExceptionCheck() == false);
  } else {
    g_delivery_metrics_export_to_big_query_state =
        enabled ? kDeliveryMetricsExportToBigQueryEnable
                : kDeliveryMetricsExportToBigQueryDisable;
  }
}

void SetTokenRegistrationOnInitEnabled(bool enabled) {
  // If this is called before JNI is initialized, we'll just cache the intent
  // and handle it on actual init.
  // Otherwise if we've already initialized, the underlying API will persist the
  // value.
  if (internal::IsInitialized()) {
    JNIEnv* env = g_app->GetJNIEnv();

    bool was_enabled = IsTokenRegistrationOnInitEnabled();

    env->CallVoidMethod(g_firebase_messaging,
                        firebase_messaging::GetMethodId(
                            firebase_messaging::kSetAutoInitEnabled),
                        static_cast<jboolean>(enabled));
    assert(env->ExceptionCheck() == false);

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
  assert(env->ExceptionCheck() == false);
  return static_cast<bool>(result);
}

Future<std::string> GetToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<std::string>(),
                                 internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<std::string> handle =
      api->SafeAlloc<std::string>(kMessagingFnGetToken);

  JNIEnv* env = g_app->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(firebase_messaging::kGetToken));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    util::RegisterCallbackOnTask(
        env, task, CompleteStringCallback,
        reinterpret_cast<void*>(new SafeFutureHandle<std::string>(handle)),
        kApiIdentifier);
  } else {
    api->CompleteWithResult(handle, -1, error.c_str(), std::string());
  }
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(api, handle);
}

Future<std::string> GetTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<std::string>&>(
      api->LastResult(kMessagingFnGetToken));
}

Future<void> DeleteToken() {
  FIREBASE_ASSERT_MESSAGE_RETURN(Future<void>(), internal::IsInitialized(),
                                 kMessagingNotInitializedError);
  MutexLock lock(*g_registration_token_mutex);
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(kMessagingFnDeleteToken);

  JNIEnv* env = g_app->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      g_firebase_messaging,
      firebase_messaging::GetMethodId(firebase_messaging::kDeleteToken));
  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    util::RegisterCallbackOnTask(
        env, task, CompleteVoidCallback,
        reinterpret_cast<void*>(handle.get().id()),
        kApiIdentifier);
  } else {
    api->Complete(handle, -1, error.c_str());
  }
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(api, handle);
}

Future<void> DeleteTokenLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kMessagingFnDeleteToken));
}

}  // namespace messaging
}  // namespace firebase
