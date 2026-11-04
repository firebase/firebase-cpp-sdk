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

#include "app/src/util_android.h"

#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/app_resources.h"
#include "app/src/app_common.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/jni/collection.h"
#include "app/src/log.h"

namespace firebase {

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_app_internal_cpp_Log_nativeLog(
    JNIEnv* env, jobject instance, jint priority, jstring tag, jstring msg);

namespace util {

#define CLASS_LOADER_METHODS(X)                                       \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"), \
  X(FindLoadedClass, "findLoadedClass", "(Ljava/lang/String;)Ljava/lang/Class;")
METHOD_LOOKUP_DECLARATION(class_loader, CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(class_loader, "java/lang/ClassLoader",
                         CLASS_LOADER_METHODS)

#define JNI_RESULT_CALLBACK_METHODS(X)                                  \
  X(Constructor, "<init>", "(Lcom/google/android/gms/tasks/Task;JJ)V"), \
  X(Cancel, "cancel", "()V")
METHOD_LOOKUP_DECLARATION(jniresultcallback, JNI_RESULT_CALLBACK_METHODS)
METHOD_LOOKUP_DEFINITION(
    jniresultcallback, "com/google/firebase/app/internal/cpp/JniResultCallback",
    JNI_RESULT_CALLBACK_METHODS)

#define CPP_THREAD_DISPATCHER_CONTEXT_METHODS(X)                  \
  X(Constructor, "<init>", "(JJJ)V"),                             \
  X(Cancel, "cancel", "()V"),                                     \
  X(ReleaseExecuteCancelLock, "releaseExecuteCancelLock", "()V"), \
  X(AcquireExecuteCancelLock, "acquireExecuteCancelLock", "()Z")
METHOD_LOOKUP_DECLARATION(cppthreaddispatchercontext,
                          CPP_THREAD_DISPATCHER_CONTEXT_METHODS)
METHOD_LOOKUP_DEFINITION(
    cppthreaddispatchercontext,
    "com/google/firebase/app/internal/cpp/CppThreadDispatcherContext",
    CPP_THREAD_DISPATCHER_CONTEXT_METHODS)

#define CPP_THREAD_DISPATCHER_METHODS(X)                                     \
  X(RunOnMainThread, "runOnMainThread",                                      \
    "(Landroid/app/Activity;"                                                \
    "Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V",   \
    kMethodTypeStatic),                                                      \
  X(RunOnBackgroundThread, "runOnBackgroundThread",                          \
    "(Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V",  \
    kMethodTypeStatic)
METHOD_LOOKUP_DECLARATION(cppthreaddispatcher, CPP_THREAD_DISPATCHER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cppthreaddispatcher,
    "com/google/firebase/app/internal/cpp/CppThreadDispatcher",
    CPP_THREAD_DISPATCHER_METHODS)

#define LOG_METHODS(X) X(Shutdown, "shutdown", "()V", util::kMethodTypeStatic)
METHOD_LOOKUP_DECLARATION(log, LOG_METHODS)
METHOD_LOOKUP_DEFINITION(log, "com/google/firebase/app/internal/cpp/Log",
                         LOG_METHODS)

#define URL_METHODS(X)                               \
  X(Constructor, "<init>", "(Ljava/lang/String;)V"), \
  X(ConstructorWithURL, "<init>", "(Ljava/net/URL;Ljava/lang/String;)V")
METHOD_LOOKUP_DECLARATION(url, URL_METHODS)
METHOD_LOOKUP_DEFINITION(url, "java/net/URL", URL_METHODS)

#define FILE_OUTPUT_STREAM_METHODS(X)                \
  X(ConstructorFile, "<init>", "(Ljava/io/File;)V"), \
  X(Write, "write", "([BII)V"),                      \
  X(Close, "close", "()V")
METHOD_LOOKUP_DECLARATION(file_output_stream, FILE_OUTPUT_STREAM_METHODS)
METHOD_LOOKUP_DEFINITION(file_output_stream, "java/io/FileOutputStream",
                         FILE_OUTPUT_STREAM_METHODS)

#define DEX_CLASS_LOADER_METHODS(X)                                 \
  X(Constructor, "<init>",                                          \
    "(Ljava/lang/String;Ljava/lang/String;"                         \
    "Ljava/lang/String;Ljava/lang/ClassLoader;)V"),                 \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;")
METHOD_LOOKUP_DECLARATION(dex_class_loader, DEX_CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(dex_class_loader,
                         PROGUARD_KEEP_CLASS "dalvik/system/DexClassLoader",
                         DEX_CLASS_LOADER_METHODS)

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define URL_CLASS_LOADER_METHODS(X)                                      \
  X(Constructor, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"), \
  X(LoadClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;")
METHOD_LOOKUP_DECLARATION(url_class_loader, URL_CLASS_LOADER_METHODS)
METHOD_LOOKUP_DEFINITION(url_class_loader, "java/net/URLClassLoader",
                         URL_CLASS_LOADER_METHODS)

#define JAVA_URI_METHODS(X) X(ToUrl, "toURL", "()Ljava/net/URL;")
METHOD_LOOKUP_DECLARATION(java_uri, JAVA_URI_METHODS)
METHOD_LOOKUP_DEFINITION(java_uri, "java/net/URI", JAVA_URI_METHODS)
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#define RESOURCES_METHODS(X)         \
  X(GetIdentifier, "getIdentifier",  \
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I")
METHOD_LOOKUP_DECLARATION(resources, RESOURCES_METHODS)
METHOD_LOOKUP_DEFINITION(resources, "android/content/res/Resources",
                         RESOURCES_METHODS)

METHOD_LOOKUP_DEFINITION(activity, "android/app/Activity", ACTIVITY_METHODS)
METHOD_LOOKUP_DEFINITION(array_list, "java/util/ArrayList", ARRAY_LIST_METHODS)
METHOD_LOOKUP_DEFINITION(asset_file_descriptor,
                         "android/content/res/AssetFileDescriptor",
                         ASSETFILEDESCRIPTOR_METHODS)
METHOD_LOOKUP_DEFINITION(boolean_class, "java/lang/Boolean", BOOLEAN_METHODS)
METHOD_LOOKUP_DEFINITION(bundle, "android/os/Bundle", BUNDLE_METHODS)
METHOD_LOOKUP_DEFINITION(byte_class, "java/lang/Byte", BYTE_METHODS)
METHOD_LOOKUP_DEFINITION(character_class, "java/lang/Character",
                         CHARACTER_METHODS)
METHOD_LOOKUP_DEFINITION(class_class, "java/lang/Class", CLASS_METHODS)
METHOD_LOOKUP_DEFINITION(content_resolver, "android/content/ContentResolver",
                         CONTENTRESOLVER_METHODS)
METHOD_LOOKUP_DEFINITION(context, "android/content/Context", CONTEXT_METHODS)
METHOD_LOOKUP_DEFINITION(cursor, "android/database/Cursor", CURSOR_METHODS)
METHOD_LOOKUP_DEFINITION(date, "java/util/Date", DATE_METHODS)
METHOD_LOOKUP_DEFINITION(double_class, "java/lang/Double", DOUBLE_METHODS)
METHOD_LOOKUP_DEFINITION(enum_class, "java/lang/Enum", ENUM_METHODS)
METHOD_LOOKUP_DEFINITION(file, "java/io/File", FILE_METHODS)
METHOD_LOOKUP_DEFINITION(float_class, "java/lang/Float", FLOAT_METHODS)
METHOD_LOOKUP_DEFINITION(hash_map, "java/util/HashMap", HASHMAP_METHODS)
METHOD_LOOKUP_DEFINITION(integer_class, "java/lang/Integer", INTEGER_METHODS)
METHOD_LOOKUP_DEFINITION(intent, "android/content/Intent", INTENT_METHODS)
METHOD_LOOKUP_DEFINITION(iterable, "java/lang/Iterable", ITERABLE_METHODS)
METHOD_LOOKUP_DEFINITION(iterator, "java/util/Iterator", ITERATOR_METHODS)
METHOD_LOOKUP_DEFINITION(list, "java/util/List", LIST_METHODS)
METHOD_LOOKUP_DEFINITION(long_class, "java/lang/Long", LONG_METHODS)
METHOD_LOOKUP_DEFINITION(map, "java/util/Map", MAP_METHODS)
METHOD_LOOKUP_DEFINITION(parcel_file_descriptor,
                         "android/os/ParcelFileDescriptor",
                         PARCELFILEDESCRIPTOR_METHODS)
METHOD_LOOKUP_DEFINITION(set, "java/util/Set", SET_METHODS)
METHOD_LOOKUP_DEFINITION(short_class, "java/lang/Short", SHORT_METHODS)
METHOD_LOOKUP_DEFINITION(string, "java/lang/String", STRING_METHODS)
METHOD_LOOKUP_DEFINITION(throwable, "java/lang/Throwable", THROWABLE_METHODS)
METHOD_LOOKUP_DEFINITION(uri, "android/net/Uri", URI_METHODS)
METHOD_LOOKUP_DEFINITION(object, "java/lang/Object", OBJECT_METHODS)

static unsigned int g_initialized_activity_count = 0;
static unsigned int g_initialized_count = 0;

struct CallbackData {
  jobject callback_reference = nullptr;
  void* data = nullptr;
  std::string api_id;
  std::list<std::shared_ptr<CallbackData>>::iterator iterator;
  bool complete = false;
  bool in_list = false;
};
using CallbackList = std::list<std::shared_ptr<CallbackData>>;
static std::map<std::string, CallbackList>* g_task_callbacks = nullptr;
static pthread_mutex_t g_task_callbacks_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static std::unordered_map<uintptr_t, std::shared_ptr<CallbackData>>&
GetLiveCallbacksLocked() {
  static auto* live_callbacks =
      new std::unordered_map<uintptr_t, std::shared_ptr<CallbackData>>();
  return *live_callbacks;
}

static void RemoveCallbackDataLocked(
    const std::shared_ptr<CallbackData>& data) {
  if (!data || !data->in_list) return;
  data->in_list = false;
  if (!g_task_callbacks) return;
  auto map_it = g_task_callbacks->find(data->api_id);
  if (map_it != g_task_callbacks->end()) {
    map_it->second.erase(data->iterator);
    if (map_it->second.empty()) g_task_callbacks->erase(map_it);
  }
}

static std::vector<jobject>* g_class_loaders = nullptr;
static bool g_jniresultcallback_loaded = false;

JNIEXPORT void JNICALL JniResultCallback_nativeOnResult(
    JNIEnv* env, jobject clazz, jobject result, jboolean success,
    jboolean cancelled, jstring status_message, jlong callback_fn_param,
    jlong callback_data);

static const JNINativeMethod kJniCallbackMethod = {
    "nativeOnResult", "(Ljava/lang/Object;ZZLjava/lang/String;JJ)V",
    reinterpret_cast<void*>(JniResultCallback_nativeOnResult)};

static const JNINativeMethod kNativeLogMethods[] = {
    {"nativeLog", "(ILjava/lang/String;Ljava/lang/String;)V",
     reinterpret_cast<void*>(
         firebase::Java_com_google_firebase_app_internal_cpp_Log_nativeLog)}};

static const char* kResourceTypeStrings[] = {
    RESOURCE_TYPES(RESOURCE_TYPE_STRING),
};

// LINT.IfChange
static const char kMissingJavaClassError[] =
    "Java class %s not found.  "
    "Please verify the AAR which contains the %s class is included "
    "in your app.";

static const char kMissingJavaMethodFieldError[] =
    "Unable to find %s.  "
    "Please verify the AAR which contains the %s class is included "
    "in your app.";
// LINT.ThenChange(//depot_firebase_cpp/app/client/unity/src/swig/app.SWIG)

static void AddClassLoader(JNIEnv* env, jobject class_loader_object) {
  if (!env || !class_loader_object) return;
  pthread_mutex_lock(&g_init_mutex);
  if (!g_class_loaders) {
    pthread_mutex_unlock(&g_init_mutex);
    env->DeleteLocalRef(class_loader_object);
    return;
  }
  jobject global_loader = env->NewGlobalRef(class_loader_object);
  if (!CheckAndClearJniExceptions(env) && global_loader != nullptr) {
    g_class_loaders->push_back(global_loader);
  } else if (global_loader != nullptr) {
    env->DeleteGlobalRef(global_loader);
  }
  pthread_mutex_unlock(&g_init_mutex);
  env->DeleteLocalRef(class_loader_object);
}

static jobject GetParentLoader() {
  pthread_mutex_lock(&g_init_mutex);
  jobject loader = (g_class_loaders && !g_class_loaders->empty())
                       ? g_class_loaders->back()
                       : nullptr;
  pthread_mutex_unlock(&g_init_mutex);
  return loader;
}

static std::string JniClassNameToBinaryName(const char* class_name) {
  if (!class_name) return std::string();
  std::string binary_name(class_name);
  std::replace(binary_name.begin(), binary_name.end(), '/', '.');
  return binary_name;
}

static jclass FindOrLoadClassFromLoaders(JNIEnv* env, const char* class_name) {
  if (!env || !class_name) return nullptr;
  std::vector<jobject> loaders;
  pthread_mutex_lock(&g_init_mutex);
  if (g_class_loaders && !g_class_loaders->empty()) loaders = *g_class_loaders;
  pthread_mutex_unlock(&g_init_mutex);
  if (loaders.empty()) return nullptr;

  static const class_loader::Method kFindLoadClassMethods[] = {
      class_loader::kFindLoadedClass, class_loader::kLoadClass};
  jstring class_name_object =
      jni::NewRawJString(env, JniClassNameToBinaryName(class_name).c_str());
  if (!class_name_object) return nullptr;
  jclass class_object = nullptr;
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kFindLoadClassMethods); ++i) {
    for (auto loader : loaders) {
      if (!loader) continue;
      class_object = static_cast<jclass>(env->CallObjectMethod(
          loader, class_loader::GetMethodId(kFindLoadClassMethods[i]),
          class_name_object));
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (class_object) {
          env->DeleteLocalRef(class_object);
          class_object = nullptr;
        }
      }
      if (class_object) break;
    }
    if (class_object) break;
  }
  env->DeleteLocalRef(class_name_object);
  return class_object;
}

static void InitializeClassLoaders(JNIEnv* env, jobject activity_object) {
  pthread_mutex_lock(&g_init_mutex);
  if (!g_class_loaders) g_class_loaders = new std::vector<jobject>();
  pthread_mutex_unlock(&g_init_mutex);
  if (!env || !activity_object) return;
  jobject class_loader = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetClassLoader));
  if (!CheckAndClearJniExceptions(env) && class_loader != nullptr) {
    AddClassLoader(env, class_loader);
  } else if (class_loader != nullptr) {
    env->DeleteLocalRef(class_loader);
  }
}

static void TerminateClassLoaders(JNIEnv* env) {
  std::vector<jobject> loaders;
  pthread_mutex_lock(&g_init_mutex);
  if (g_class_loaders) {
    loaders = std::move(*g_class_loaders);
    delete g_class_loaders;
    g_class_loaders = nullptr;
  }
  pthread_mutex_unlock(&g_init_mutex);
  for (auto loader : loaders) env->DeleteGlobalRef(loader);
}

template <typename T>
class ScopedCleanup {
 public:
  ScopedCleanup(void (*callback)(T*), T* user_data)
      : callback_(callback), user_data_(user_data) {}
  ~ScopedCleanup() {
    if (callback_) callback_(user_data_);
  }
  void Cancel() { callback_ = nullptr; }

 private:
  void (*callback_)(T*);
  T* user_data_;
};

static void ReleaseClasses(JNIEnv* env) {
  asset_file_descriptor::ReleaseClass(env);
  array_list::ReleaseClass(env);
  boolean_class::ReleaseClass(env);
  bundle::ReleaseClass(env);
  byte_class::ReleaseClass(env);
  character_class::ReleaseClass(env);
  class_class::ReleaseClass(env);
  content_resolver::ReleaseClass(env);
  context::ReleaseClass(env);
  cursor::ReleaseClass(env);
  date::ReleaseClass(env);
  dex_class_loader::ReleaseClass(env);
  double_class::ReleaseClass(env);
  enum_class::ReleaseClass(env);
  file::ReleaseClass(env);
  file_output_stream::ReleaseClass(env);
  float_class::ReleaseClass(env);
  hash_map::ReleaseClass(env);
  integer_class::ReleaseClass(env);
  intent::ReleaseClass(env);
  iterable::ReleaseClass(env);
  iterator::ReleaseClass(env);
  log::ReleaseClass(env);
  long_class::ReleaseClass(env);
  list::ReleaseClass(env);
  map::ReleaseClass(env);
  parcel_file_descriptor::ReleaseClass(env);
  resources::ReleaseClass(env);
  set::ReleaseClass(env);
  short_class::ReleaseClass(env);
  string::ReleaseClass(env);
  throwable::ReleaseClass(env);
  uri::ReleaseClass(env);
  object::ReleaseClass(env);
  url::ReleaseClass(env);
  ::firebase::jni::ReleaseCollectionClasses(env);
  if (g_jniresultcallback_loaded || jniresultcallback::GetClass() != nullptr) {
    jniresultcallback::ReleaseClass(env);
    g_jniresultcallback_loaded = false;
  }
  JavaThreadContext::Terminate(env);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  java_uri::ReleaseClass(env);
  url_class_loader::ReleaseClass(env);
#endif
}

static void ReleaseActivityClasses(JNIEnv* env) {
  activity::ReleaseClass(env);
  class_loader::ReleaseClass(env);
}

bool InitializeActivityClasses(JNIEnv* env, jobject activity_object) {
  if (env) jni::internal::GlobalRefReaper::RecordJavaVM(env);
  pthread_mutex_lock(&g_init_mutex);
  g_initialized_activity_count++;
  if (g_initialized_activity_count > 1) {
    pthread_mutex_unlock(&g_init_mutex);
    return true;
  }
  pthread_mutex_unlock(&g_init_mutex);

  ScopedCleanup<JNIEnv> cleanup(TerminateActivityClasses, env);
  if (!(activity::CacheMethodIds(env, activity_object) &&
        class_loader::CacheMethodIds(env, activity_object))) {
    return false;
  }
  InitializeClassLoaders(env, activity_object);
  CheckAndClearJniExceptions(env);

  pthread_mutex_lock(&g_init_mutex);
  g_initialized_count++;
  pthread_mutex_unlock(&g_init_mutex);

  cleanup.Cancel();
  return true;
}

void TerminateActivityClasses(JNIEnv* env) {
  pthread_mutex_lock(&g_init_mutex);
  FIREBASE_ASSERT(g_initialized_activity_count);
  g_initialized_activity_count--;
  bool is_last = (g_initialized_activity_count == 0);
  pthread_mutex_unlock(&g_init_mutex);
  if (is_last) {
    ReleaseActivityClasses(env);
    TerminateClassLoaders(env);
  }
}

bool Initialize(JNIEnv* env, jobject activity_object) {
  if (env) {
    jni::internal::GlobalRefReaper::RecordJavaVM(env);
    jni::internal::GlobalRefReaper::Flush(env);
  }
  pthread_mutex_lock(&g_init_mutex);
  if (g_initialized_count != 0) {
    g_initialized_count++;
    pthread_mutex_unlock(&g_init_mutex);
    return true;
  }
  pthread_mutex_unlock(&g_init_mutex);

  if (!InitializeActivityClasses(env, activity_object)) return false;

  if (!(array_list::CacheMethodIds(env, activity_object) &&
        asset_file_descriptor::CacheMethodIds(env, activity_object) &&
        boolean_class::CacheMethodIds(env, activity_object) &&
        bundle::CacheMethodIds(env, activity_object) &&
        byte_class::CacheMethodIds(env, activity_object) &&
        character_class::CacheMethodIds(env, activity_object) &&
        class_class::CacheMethodIds(env, activity_object) &&
        content_resolver::CacheMethodIds(env, activity_object) &&
        context::CacheMethodIds(env, activity_object) &&
        cursor::CacheMethodIds(env, activity_object) &&
        date::CacheMethodIds(env, activity_object) &&
        dex_class_loader::CacheMethodIds(env, activity_object) &&
        double_class::CacheMethodIds(env, activity_object) &&
        enum_class::CacheMethodIds(env, activity_object) &&
        file::CacheMethodIds(env, activity_object) &&
        file_output_stream::CacheMethodIds(env, activity_object) &&
        float_class::CacheMethodIds(env, activity_object) &&
        hash_map::CacheMethodIds(env, activity_object) &&
        integer_class::CacheMethodIds(env, activity_object) &&
        intent::CacheMethodIds(env, activity_object) &&
        iterable::CacheMethodIds(env, activity_object) &&
        iterator::CacheMethodIds(env, activity_object) &&
        list::CacheMethodIds(env, activity_object) &&
        long_class::CacheMethodIds(env, activity_object) &&
        map::CacheMethodIds(env, activity_object) &&
        parcel_file_descriptor::CacheMethodIds(env, activity_object) &&
        resources::CacheMethodIds(env, activity_object) &&
        set::CacheMethodIds(env, activity_object) &&
        short_class::CacheMethodIds(env, activity_object) &&
        string::CacheMethodIds(env, activity_object) &&
        throwable::CacheMethodIds(env, activity_object) &&
        uri::CacheMethodIds(env, activity_object) &&
        object::CacheMethodIds(env, activity_object) &&
        url::CacheMethodIds(env, activity_object) &&
        ::firebase::jni::CacheCollectionMethodIds(env, activity_object))) {
    ReleaseClasses(env);
    TerminateActivityClasses(env);
    return false;
  }

  ScopedCleanup<JNIEnv> cleanup(Terminate, env);
  g_initialized_count++;

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  if (!(java_uri::CacheMethodIds(env, activity_object) &&
        url_class_loader::CacheMethodIds(env, activity_object))) {
    return false;
  }
#endif

  const std::vector<internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, activity_object,
          internal::EmbeddedFile::ToVector(firebase_app::app_resources_filename,
                                           firebase_app::app_resources_data,
                                           firebase_app::app_resources_size));

  if (!(log::CacheClassFromFiles(env, activity_object, &embedded_files) !=
            nullptr &&
        log::CacheMethodIds(env, activity_object) &&
        log::RegisterNatives(env, kNativeLogMethods,
                             FIREBASE_ARRAYSIZE(kNativeLogMethods)))) {
    return false;
  }

  g_jniresultcallback_loaded =
      (jniresultcallback::CacheClassFromFiles(env, activity_object,
                                              &embedded_files) &&
       jniresultcallback::CacheMethodIds(env, activity_object) &&
       jniresultcallback::RegisterNatives(env, &kJniCallbackMethod, 1));
  if (!g_jniresultcallback_loaded && jniresultcallback::GetClass() != nullptr) {
    jniresultcallback::ReleaseClass(env);
  }

  if (!JavaThreadContext::Initialize(env, activity_object, embedded_files)) {
    return false;
  }
  CheckAndClearJniExceptions(env);

  pthread_mutex_lock(&g_task_callbacks_mutex);
  if (!g_task_callbacks) {
    g_task_callbacks = new std::map<std::string, CallbackList>();
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);
  cleanup.Cancel();
  return true;
}

void Terminate(JNIEnv* env) {
  pthread_mutex_lock(&g_init_mutex);
  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  bool is_last = (g_initialized_count == 0);
  pthread_mutex_unlock(&g_init_mutex);
  if (is_last) {
    if (g_task_callbacks) {
      CancelCallbacks(env, nullptr);
      pthread_mutex_lock(&g_task_callbacks_mutex);
      delete g_task_callbacks;
      g_task_callbacks = nullptr;
      GetLiveCallbacksLocked().clear();
      pthread_mutex_unlock(&g_task_callbacks_mutex);
    }

    jclass log_class = log::GetClass();
    jmethodID shutdown_method =
        log_class ? log::GetMethodId(log::kShutdown) : nullptr;
    if (log_class && shutdown_method) {
      env->CallStaticVoidMethod(log_class, shutdown_method);
      CheckAndClearJniExceptions(env);
    }

    ReleaseClasses(env);
    TerminateActivityClasses(env);
    jni::internal::GlobalRefReaper::Flush(env);
  }
}

bool LookupMethodIds(JNIEnv* env, jclass clazz,
                     const MethodNameSignature* method_name_signatures,
                     size_t number_of_method_name_signatures,
                     jmethodID* method_ids, const char* class_name) {
  FIREBASE_ASSERT(method_name_signatures &&
                  number_of_method_name_signatures > 0 && method_ids);
  FIREBASE_ASSERT_MESSAGE_RETURN(false, clazz, kMissingJavaClassError,
                                 class_name, class_name);
  LogDebug("Looking up methods for %s", class_name);
  for (size_t i = 0; i < number_of_method_name_signatures; ++i) {
    const MethodNameSignature& method = method_name_signatures[i];
    if (method.optional == kMethodOptional && method.name == nullptr) continue;
    method_ids[i] = (method.type == kMethodTypeInstance)
                        ? env->GetMethodID(clazz, method.name, method.signature)
                        : env->GetStaticMethodID(clazz, method.name,
                                                 method.signature);
    if (CheckAndClearJniExceptions(env)) method_ids[i] = 0;
    std::string method_message =
        std::string("Method ") + (class_name ? class_name : "") + "." +
        (method.name ? method.name : "") + " (signature '" +
        (method.signature ? method.signature : "") + "', " +
        (method.type == kMethodTypeInstance ? "instance" : "static") + ")";
    LogDebug("%s (optional %d) %p%s", method_message.c_str(),
             (method.optional == kMethodOptional) ? 1 : 0,
             reinterpret_cast<void*>(method_ids[i]),
             method_ids[i] ? "" : " (not found)");
    FIREBASE_ASSERT_MESSAGE_RETURN(
        false, method_ids[i] || (method.optional == kMethodOptional),
        kMissingJavaMethodFieldError, method_message.c_str(), class_name);
  }
  return true;
}

bool LookupFieldIds(JNIEnv* env, jclass clazz,
                    const FieldDescriptor* field_descriptors,
                    size_t number_of_field_descriptors, jfieldID* field_ids,
                    const char* class_name) {
  FIREBASE_ASSERT(field_descriptors && number_of_field_descriptors > 0 &&
                  field_ids);
  FIREBASE_ASSERT_MESSAGE_RETURN(false, clazz, kMissingJavaClassError,
                                 class_name, class_name);
  LogDebug("Looking up fields for %s", class_name);
  for (size_t i = 0; i < number_of_field_descriptors; ++i) {
    const FieldDescriptor& field = field_descriptors[i];
    if (field.optional == kMethodOptional && field.name == nullptr) continue;
    field_ids[i] = (field.type == kFieldTypeInstance)
                       ? env->GetFieldID(clazz, field.name, field.signature)
                       : env->GetStaticFieldID(clazz, field.name,
                                               field.signature);
    if (CheckAndClearJniExceptions(env)) field_ids[i] = 0;
    std::string field_message =
        std::string("Field ") + (class_name ? class_name : "") + "." +
        (field.name ? field.name : "") + " (signature '" +
        (field.signature ? field.signature : "") + "', " +
        (field.type == kFieldTypeInstance ? "instance" : "static") + ")";
    LogDebug("%s (optional %d) %p%s", field_message.c_str(),
             (field.optional == kMethodOptional) ? 1 : 0,
             reinterpret_cast<void*>(field_ids[i]),
             field_ids[i] ? "" : " (not found)");
    FIREBASE_ASSERT_MESSAGE_RETURN(
        false, field_ids[i] || (field.optional == kMethodOptional),
        kMissingJavaMethodFieldError, field_message.c_str(), class_name);
  }
  return true;
}

namespace {
template <typename Container>
jobject StringContainerToJavaList(JNIEnv* env, const Container& container) {
  if (!env) return nullptr;
  jobject java_list =
      env->NewObject(array_list::GetClass(),
                     array_list::GetMethodId(array_list::kConstructor));
  if (CheckAndClearJniExceptions(env) || !java_list) {
    if (java_list) env->DeleteLocalRef(java_list);
    return nullptr;
  }
  jmethodID add_method = array_list::GetMethodId(array_list::kAdd);
  for (const auto& s : container) {
    jstring value = jni::NewRawJString(env, s);
    if (!CheckAndClearJniExceptions(env) && value) {
      env->CallBooleanMethod(java_list, add_method, value);
      CheckAndClearJniExceptions(env);
    }
    if (value) env->DeleteLocalRef(value);
  }
  return java_list;
}
}  // namespace

jobject StdVectorToJavaList(JNIEnv* env,
                            const std::vector<std::string>& string_vector) {
  return StringContainerToJavaList(env, string_vector);
}

jobject StdUnorderedSetToJavaList(
    JNIEnv* env, const std::unordered_set<std::string>& string_set) {
  return StringContainerToJavaList(env, string_set);
}

jobject VariantMapToJavaMap(JNIEnv* env,
                            const std::map<Variant, Variant>& variant_map) {
  if (!env) return nullptr;
  jobject java_map = env->NewObject(
      hash_map::GetClass(), hash_map::GetMethodId(hash_map::kConstructor));
  if (CheckAndClearJniExceptions(env) || !java_map) {
    if (java_map) env->DeleteLocalRef(java_map);
    return nullptr;
  }
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (const auto& kv : variant_map) {
    jobject key = VariantToJavaObject(env, kv.first);
    jobject value = VariantToJavaObject(env, kv.second);
    jobject previous = env->CallObjectMethod(java_map, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    if (value) env->DeleteLocalRef(value);
    if (key) env->DeleteLocalRef(key);
  }
  return java_map;
}

void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<const char*, const char*>& string_map) {
  if (!env || !to || !*to) return;
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (const auto& kv : string_map) {
    jstring key = jni::NewRawJString(env, kv.first ? kv.first : "");
    jstring value = jni::NewRawJString(env, kv.second ? kv.second : "");
    CheckAndClearJniExceptions(env);
    jobject previous = env->CallObjectMethod(*to, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    if (value) env->DeleteLocalRef(value);
    if (key) env->DeleteLocalRef(key);
  }
}

void StdMapToJavaMap(JNIEnv* env, jobject* to,
                     const std::map<std::string, std::string>& from) {
  if (!env || !to || !*to) return;
  jmethodID put_method = map::GetMethodId(map::kPut);
  for (const auto& kv : from) {
    jstring key = jni::NewRawJString(env, kv.first);
    jstring value = jni::NewRawJString(env, kv.second);
    CheckAndClearJniExceptions(env);
    jobject previous = env->CallObjectMethod(*to, put_method, key, value);
    CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    if (value) env->DeleteLocalRef(value);
    if (key) env->DeleteLocalRef(key);
  }
}

template <typename T, typename ConvertFn>
static void JavaMapToStdMapTemplate(JNIEnv* env, std::map<T, T>* to,
                                    jobject from, ConvertFn convert) {
  if (!to) return;
  to->clear();
  if (!env || !from) return;
  jobject key_set = env->CallObjectMethod(from, map::GetMethodId(map::kKeySet));
  if (CheckAndClearJniExceptions(env) || !key_set) {
    if (key_set) env->DeleteLocalRef(key_set);
    return;
  }
  jobject iter =
      env->CallObjectMethod(key_set, set::GetMethodId(set::kIterator));
  if (CheckAndClearJniExceptions(env) || !iter) {
    if (iter) env->DeleteLocalRef(iter);
    env->DeleteLocalRef(key_set);
    return;
  }
  while (
      env->CallBooleanMethod(iter, iterator::GetMethodId(iterator::kHasNext))) {
    if (CheckAndClearJniExceptions(env)) break;
    jobject key_object =
        env->CallObjectMethod(iter, iterator::GetMethodId(iterator::kNext));
    if (CheckAndClearJniExceptions(env)) {
      if (key_object) env->DeleteLocalRef(key_object);
      break;
    }
    jobject value_object =
        env->CallObjectMethod(from, map::GetMethodId(map::kGet), key_object);
    CheckAndClearJniExceptions(env);
    const T key = convert(env, key_object);
    const T value = convert(env, value_object);
    if (key_object) env->DeleteLocalRef(key_object);
    if (value_object) env->DeleteLocalRef(value_object);
    (*to)[key] = value;
  }
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

void JavaMapToStdMap(JNIEnv* env, std::map<std::string, std::string>* to,
                     jobject from) {
  JavaMapToStdMapTemplate<std::string>(env, to, from, JStringToString);
}

void JavaMapToVariantMap(JNIEnv* env, std::map<Variant, Variant>* to,
                         jobject from) {
  JavaMapToStdMapTemplate<Variant>(env, to, from, JavaObjectToVariant);
}

void JavaSetToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                              jobject from) {
  if (!to) return;
  to->clear();
  if (!env || !from) return;
  jobject iter =
      env->CallObjectMethod(from, util::set::GetMethodId(util::set::kIterator));
  if (CheckAndClearJniExceptions(env) || !iter) {
    if (iter) env->DeleteLocalRef(iter);
    return;
  }
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    if (CheckAndClearJniExceptions(env)) break;
    jobject elem_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    if (CheckAndClearJniExceptions(env)) {
      if (elem_object) env->DeleteLocalRef(elem_object);
      break;
    }
    to->push_back(JniStringToString(env, elem_object));
  }
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(iter);
}

jobject VariantVectorToJavaList(JNIEnv* env,
                                const std::vector<Variant>& variant_vector) {
  if (!env) return nullptr;
  jobject java_list =
      env->NewObject(array_list::GetClass(),
                     array_list::GetMethodId(array_list::kConstructor));
  if (CheckAndClearJniExceptions(env) || !java_list) {
    if (java_list) env->DeleteLocalRef(java_list);
    return nullptr;
  }
  jmethodID add_method = array_list::GetMethodId(array_list::kAdd);
  for (const auto& v : variant_vector) {
    jobject value = VariantToJavaObject(env, v);
    env->CallBooleanMethod(java_list, add_method, value);
    CheckAndClearJniExceptions(env);
    if (value) env->DeleteLocalRef(value);
  }
  return java_list;
}

void JavaListToStdStringVector(JNIEnv* env, std::vector<std::string>* vector,
                               jobject from) {
  if (!vector) return;
  vector->clear();
  if (!env || !from) return;
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  if (CheckAndClearJniExceptions(env) || size <= 0) return;
  vector->reserve(static_cast<size_t>(size));
  for (int i = 0; i < size; i++) {
    jobject element =
        env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);
    vector->push_back(JniStringToString(env, element));
  }
}

void JavaObjectListToStdStringVector(JNIEnv* env, std::vector<std::string>* to,
                                     jobject from) {
  if (!to) return;
  to->clear();
  if (!env || !from) return;
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  if (CheckAndClearJniExceptions(env) || size <= 0) return;
  to->reserve(static_cast<size_t>(size));
  for (int i = 0; i < size; i++) {
    jobject element =
        env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);
    to->push_back(JniObjectToString(env, element));
  }
}

void JavaListToVariantList(JNIEnv* env, std::vector<Variant>* to,
                           jobject from) {
  if (!to) return;
  to->clear();
  if (!env || !from) return;
  int size = env->CallIntMethod(from, list::GetMethodId(list::kSize));
  if (CheckAndClearJniExceptions(env) || size <= 0) return;
  to->reserve(static_cast<size_t>(size));
  for (int i = 0; i < size; i++) {
    jobject obj = env->CallObjectMethod(from, list::GetMethodId(list::kGet), i);
    CheckAndClearJniExceptions(env);
    to->push_back(JavaObjectToVariant(env, obj));
    if (obj) env->DeleteLocalRef(obj);
  }
}

std::string JniStringToString(JNIEnv* env, jobject string_object) {
  return jni::JniStringToStringAndReleaseLocalRef(env, string_object);
}

std::string JniObjectToString(JNIEnv* env, jobject obj) {
  if (!env || obj == nullptr) return "";
  jobject str =
      env->CallObjectMethod(obj, object::GetMethodId(object::kToString));
  env->DeleteLocalRef(obj);
  if (CheckAndClearJniExceptions(env)) {
    if (str) env->DeleteLocalRef(str);
    return "";
  }
  return JniStringToString(env, str);
}

std::string JStringToString(JNIEnv* env, jobject string_object) {
  return jni::JStringToString(env, string_object);
}

bool IsJArray(JNIEnv* env, jobject obj) {
  if (!env || !obj) return false;
  jclass obj_class = env->GetObjectClass(obj);
  if (CheckAndClearJniExceptions(env) || !obj_class) return false;
  const bool is_array = env->CallBooleanMethod(
      obj_class, class_class::GetMethodId(class_class::kIsArray));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(obj_class);
  return is_array;
}

std::string JObjectClassName(JNIEnv* env, jobject obj) {
  if (!env || !obj) return "";
  jclass obj_class = env->GetObjectClass(obj);
  if (CheckAndClearJniExceptions(env) || !obj_class) return "";
  jobject obj_name = env->CallObjectMethod(
      obj_class, class_class::GetMethodId(class_class::kGetName));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(obj_class);
  return JniStringToString(env, obj_name);
}

#define FIREBASE_JNI_PRIMITIVE_TYPES(X)                         \
  X(boolean_class, "[Z", jboolean, Boolean, bool, Bool, "%d")   \
  X(byte_class, "[B", jbyte, Byte, int8_t, Int8, "%d")          \
  X(character_class, "[C", jchar, Char, uint16_t, UInt16, "%u") \
  X(short_class, "[S", jshort, Short, int16_t, Int16, "%d")     \
  X(integer_class, "[I", jint, Int, int, Int, "%d")             \
  X(long_class, "[J", jlong, Long, int64_t, Int64, "%ld")       \
  X(float_class, "[F", jfloat, Float, float, Float, "%f")       \
  X(double_class, "[D", jdouble, Double, double, Double, "%f")

#define FIREBASE_JTYPE_TO_TYPE(type_class, array_format, jtype, jname, ctype, \
                               cname, printf_type)                            \
  ctype J##jname##To##cname(JNIEnv* env, jobject obj) {                       \
    if (!env || !obj) return static_cast<ctype>(0);                           \
    ctype ret = static_cast<ctype>(env->Call##jname##Method(                  \
        obj, type_class::GetMethodId(type_class::kValue)));                   \
    CheckAndClearJniExceptions(env);                                          \
    return ret;                                                               \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_JTYPE_TO_TYPE)

uint8_t JByteToUInt8(JNIEnv* env, jobject obj) {
  return static_cast<uint8_t>(JByteToInt8(env, obj));
}
char JCharToChar(JNIEnv* env, jobject obj) {
  return static_cast<char>(JCharToUInt16(env, obj));
}
int JIntegerToInt(JNIEnv* env, jobject obj) { return JIntToInt(env, obj); }

#define FIREBASE_IS_JARRAY(type_class, array_format, jtype, jname, ctype,      \
                           cname, printf_type)                                 \
  bool IsJ##jname##Array(JNIEnv* env, jobject obj) {                           \
    if (!env || !obj) return false;                                            \
    jclass array_class = env->FindClass(array_format);                         \
    if (CheckAndClearJniExceptions(env) || !array_class) return false;         \
    const bool is_array = env->IsInstanceOf(obj, array_class) == JNI_TRUE;     \
    CheckAndClearJniExceptions(env);                                           \
    env->DeleteLocalRef(array_class);                                          \
    return is_array;                                                           \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_IS_JARRAY)

#define FIREBASE_JARRAY_TO_VARIANT(type_class, array_format, jtype, jname, \
                                   ctype, cname, printf_type)              \
  Variant J##jname##ArrayToVariant(JNIEnv* env, jtype##Array array) {      \
    if (!env || !array) return Variant::EmptyVector();                     \
    const jsize jlen = env->GetArrayLength(array);                         \
    if (CheckAndClearJniExceptions(env) || jlen <= 0)                      \
      return Variant::EmptyVector();                                       \
    const size_t len = static_cast<size_t>(jlen);                          \
    jtype* c_array = env->Get##jname##ArrayElements(array, nullptr);       \
    if (CheckAndClearJniExceptions(env) || !c_array)                       \
      return Variant::EmptyVector();                                       \
    auto vec = new std::vector<Variant>(len);                              \
    using VariantValueType =                                               \
        std::conditional_t<std::is_same<ctype, uint16_t>::value, int64_t,  \
                           ctype>;                                         \
    for (size_t i = 0; i < len; ++i)                                       \
      (*vec)[i] = Variant(static_cast<VariantValueType>(c_array[i]));      \
    Variant v;                                                             \
    v.AssignVector(&vec);                                                  \
    env->Release##jname##ArrayElements(array, c_array, JNI_ABORT);         \
    return v;                                                              \
  }
FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_JARRAY_TO_VARIANT)

Variant JObjectArrayToVariant(JNIEnv* env, jobjectArray array) {
  if (!env || !array) return Variant::EmptyVector();
  const jsize jlen = env->GetArrayLength(array);
  if (CheckAndClearJniExceptions(env) || jlen <= 0)
    return Variant::EmptyVector();
  const size_t len = static_cast<size_t>(jlen);
  auto vec = new std::vector<Variant>();
  vec->reserve(len);
  for (size_t i = 0; i < len; ++i) {
    jobject obj = env->GetObjectArrayElement(array, static_cast<jsize>(i));
    CheckAndClearJniExceptions(env);
    vec->push_back(JavaObjectToVariant(env, obj));
    if (obj) env->DeleteLocalRef(obj);
  }
  Variant v;
  v.AssignVector(&vec);
  return v;
}

#define FIREBASE_CONVERT_TO_VARIANT_ARRAY(type_class, array_format, jtype,  \
                                          jname, ctype, cname, printf_type) \
  if (IsJ##jname##Array(env, array)) {                                      \
    return J##jname##ArrayToVariant(env, static_cast<jtype##Array>(array)); \
  }

Variant JArrayToVariant(JNIEnv* env, jarray array) {
  if (!env || !array) return Variant::EmptyVector();
  FIREBASE_ASSERT(IsJArray(env, array));
  if (IsJByteArray(env, array)) {
    jbyteArray byte_array = static_cast<jbyteArray>(array);
    const jsize jlen = env->GetArrayLength(byte_array);
    if (CheckAndClearJniExceptions(env) || jlen <= 0)
      return Variant::EmptyMutableBlob(0);
    Variant v = Variant::FromMutableBlob(nullptr, static_cast<size_t>(jlen));
    env->GetByteArrayRegion(byte_array, 0, jlen,
                            reinterpret_cast<jbyte*>(v.mutable_blob_data()));
    CheckAndClearJniExceptions(env);
    return v;
  }
  FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_CONVERT_TO_VARIANT_ARRAY)
  return JObjectArrayToVariant(env, static_cast<jobjectArray>(array));
}

#define FIREBASE_CONVERT_TO_VARIANT(type_class, array_format, jtype, jname, \
                                    ctype, cname, printf_type)              \
  if (env->IsInstanceOf(object, type_class::GetClass())) {                  \
    using VariantValueType =                                                \
        std::conditional_t<std::is_same<ctype, uint16_t>::value, int64_t,   \
                           ctype>;                                          \
    return Variant(                                                         \
        static_cast<VariantValueType>(J##jname##To##cname(env, object)));   \
  }

Variant JavaObjectToVariant(JNIEnv* env, jobject object) {
  if (!env || object == nullptr) return Variant();
  if (env->IsInstanceOf(object, string::GetClass()))
    return Variant(JStringToString(env, object));
  if (env->IsInstanceOf(object, date::GetClass())) {
    jlong millis = env->CallLongMethod(object, date::GetMethodId(date::kGetTime));
    CheckAndClearJniExceptions(env);
    return Variant(static_cast<int64_t>(millis));
  }
  FIREBASE_JNI_PRIMITIVE_TYPES(FIREBASE_CONVERT_TO_VARIANT)
  if (env->IsInstanceOf(object, map::GetClass())) {
    Variant v;
    auto c_map = new std::map<Variant, Variant>();
    JavaMapToVariantMap(env, c_map, object);
    v.AssignMap(&c_map);
    return v;
  }
  if (env->IsInstanceOf(object, list::GetClass())) {
    Variant v;
    auto c_vector = new std::vector<Variant>();
    JavaListToVariantList(env, c_vector, object);
    v.AssignVector(&c_vector);
    return v;
  }
  if (IsJByteArray(env, object)) {
    jbyteArray byte_array = static_cast<jbyteArray>(object);
    const jsize jlen = env->GetArrayLength(byte_array);
    if (CheckAndClearJniExceptions(env) || jlen <= 0)
      return Variant::EmptyMutableBlob(0);
    Variant v = Variant::FromMutableBlob(nullptr, static_cast<size_t>(jlen));
    env->GetByteArrayRegion(byte_array, 0, jlen,
                            reinterpret_cast<jbyte*>(v.mutable_blob_data()));
    CheckAndClearJniExceptions(env);
    return v;
  }
  if (IsJArray(env, object)) return JArrayToVariant(env, static_cast<jarray>(object));
  LogWarning("Class %s cannot be converted to Variant, leaving empty.",
             JObjectClassName(env, object).c_str());
  return Variant();
}

jobject VariantToJavaObject(JNIEnv* env, const Variant& variant) {
  if (!env || variant.is_null()) return nullptr;
  if (variant.is_int64()) {
    jobject obj = env->NewObject(
        long_class::GetClass(),
        long_class::GetMethodId(long_class::kConstructor),
        variant.int64_value());
    if (CheckAndClearJniExceptions(env)) {
      if (obj) env->DeleteLocalRef(obj);
      return nullptr;
    }
    return obj;
  }
  if (variant.is_double()) {
    jobject obj = env->NewObject(
        double_class::GetClass(),
        double_class::GetMethodId(double_class::kConstructor),
        variant.double_value());
    if (CheckAndClearJniExceptions(env)) {
      if (obj) env->DeleteLocalRef(obj);
      return nullptr;
    }
    return obj;
  }
  if (variant.is_bool()) {
    jobject obj = env->NewObject(
        boolean_class::GetClass(),
        boolean_class::GetMethodId(boolean_class::kConstructor),
        variant.bool_value());
    if (CheckAndClearJniExceptions(env)) {
      if (obj) env->DeleteLocalRef(obj);
      return nullptr;
    }
    return obj;
  }
  if (variant.is_string()) {
    jstring str = jni::NewRawJString(
        env, variant.string_value() ? variant.string_value() : "");
    if (CheckAndClearJniExceptions(env)) {
      if (str) env->DeleteLocalRef(str);
      return nullptr;
    }
    return str;
  }
  if (variant.is_blob()) {
    return static_cast<jobject>(ByteBufferToJavaByteArray(
        env, variant.blob_data(), variant.blob_size()));
  }
  if (variant.is_vector()) return VariantVectorToJavaList(env, variant.vector());
  if (variant.is_map()) return VariantMapToJavaMap(env, variant.map());
  LogWarning("Variant cannot be converted to Java Object, returning null.");
  return nullptr;
}

std::string JniUriToString(JNIEnv* env, jobject uri) {
  if (!env || uri == nullptr) return "";
  jobject path = env->CallObjectMethod(uri, uri::GetMethodId(uri::kToString));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(uri);
  return JniStringToString(env, path);
}

std::string JniUriGetPath(JNIEnv* env, jobject uri) {
  if (!env || uri == nullptr) return "";
  jobject path = env->CallObjectMethod(uri, uri::GetMethodId(uri::kGetPath));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(uri);
  return JniStringToString(env, path);
}

jobject CharsToJniUri(JNIEnv* env, const char* uri) {
  return ParseUriString(env, uri);
}

jobject ParseUriString(JNIEnv* env, const char* uri_string) {
  if (!env || !uri_string) return nullptr;
  jstring path_str = jni::NewRawJString(env, uri_string);
  if (CheckAndClearJniExceptions(env) || !path_str) {
    if (path_str) env->DeleteLocalRef(path_str);
    return nullptr;
  }
  jobject parsed_uri = env->CallStaticObjectMethod(
      uri::GetClass(), uri::GetMethodId(uri::kParse), path_str);
  if (CheckAndClearJniExceptions(env)) {
    if (parsed_uri) env->DeleteLocalRef(parsed_uri);
    parsed_uri = nullptr;
  }
  env->DeleteLocalRef(path_str);
  return parsed_uri;
}

jobject CharsToURL(JNIEnv* env, const char* url_string) {
  if (!env || !url_string || url_string[0] == '\0') return nullptr;
  jstring url_jstring = jni::NewRawJString(env, url_string);
  if (CheckAndClearJniExceptions(env) || !url_jstring) {
    if (url_jstring) env->DeleteLocalRef(url_jstring);
    return nullptr;
  }
  jobject url_obj = env->NewObject(
      url::GetClass(), url::GetMethodId(url::kConstructor), url_jstring);
  if (CheckAndClearJniExceptions(env)) {
    if (url_obj) env->DeleteLocalRef(url_obj);
    url_obj = nullptr;
  }
  env->DeleteLocalRef(url_jstring);
  return url_obj;
}

std::vector<unsigned char> JniByteArrayToVector(JNIEnv* env, jobject array) {
  std::vector<unsigned char> value;
  if (!env || !array) return value;
  jbyteArray byte_array = static_cast<jbyteArray>(array);
  jsize byte_array_length = env->GetArrayLength(byte_array);
  if (!CheckAndClearJniExceptions(env) && byte_array_length > 0) {
    value.resize(static_cast<size_t>(byte_array_length));
    env->GetByteArrayRegion(byte_array, 0, byte_array_length,
                            reinterpret_cast<jbyte*>(value.data()));
    CheckAndClearJniExceptions(env);
  }
  env->DeleteLocalRef(array);
  return value;
}

jbyteArray ByteBufferToJavaByteArray(JNIEnv* env, const uint8_t* data,
                                     size_t size) {
  if (!env || (size > 0 && !data)) return nullptr;
  const jsize jlen = jni::SizeToJniSize(size);
  jbyteArray output_array = env->NewByteArray(jlen);
  if (CheckAndClearJniExceptions(env) || !output_array) {
    if (output_array) env->DeleteLocalRef(output_array);
    return nullptr;
  }
  if (jlen > 0 && data != nullptr) {
    env->SetByteArrayRegion(output_array, 0, jlen,
                            reinterpret_cast<const jbyte*>(data));
    if (CheckAndClearJniExceptions(env)) {
      env->DeleteLocalRef(output_array);
      return nullptr;
    }
  }
  return output_array;
}

jobject LocalToGlobalReference(JNIEnv* env, jobject local_reference) {
  jobject global_reference = nullptr;
  if (env && local_reference) {
    global_reference = env->NewGlobalRef(local_reference);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(local_reference);
  }
  return global_reference;
}

jobject ContinueBuilder(JNIEnv* env, jobject old_builder, jobject new_builder) {
  if (env && old_builder) env->DeleteLocalRef(old_builder);
  return new_builder;
}

void RegisterCallbackOnTask(JNIEnv* env, jobject task,
                            TaskCallbackFn callback_fn, void* callback_data,
                            const char* api_identifier) {
  if (!env || !callback_fn) return;
  if (!task) {
    callback_fn(env, nullptr, kFutureResultFailure, "Task is null.",
                callback_data);
    return;
  }
  if (!g_jniresultcallback_loaded || !jniresultcallback::GetClass()) {
    callback_fn(env, nullptr, kFutureResultFailure,
                "JniResultCallback is not initialized.", callback_data);
    return;
  }

  const std::string key = api_identifier ? api_identifier : "";
  auto data = std::make_shared<CallbackData>();
  const uintptr_t data_key = reinterpret_cast<uintptr_t>(data.get());
  pthread_mutex_lock(&g_task_callbacks_mutex);
  if (!g_task_callbacks) {
    pthread_mutex_unlock(&g_task_callbacks_mutex);
    callback_fn(env, nullptr, kFutureResultFailure,
                "JniResultCallback is not initialized.", callback_data);
    return;
  }
  {
    auto& callback_list = (*g_task_callbacks)[key];
    callback_list.push_back(data);
    auto list_it = callback_list.end();
    --list_it;
    data->data = callback_data;
    data->callback_reference = nullptr;
    data->api_id = key;
    data->iterator = list_it;
    data->complete = false;
    data->in_list = true;
    GetLiveCallbacksLocked()[data_key] = data;
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  jobject jni_result_callback = env->NewObject(
      jniresultcallback::GetClass(),
      jniresultcallback::GetMethodId(jniresultcallback::kConstructor), task,
      reinterpret_cast<jlong>(callback_fn), static_cast<jlong>(data_key));
  const bool create_failed =
      CheckAndClearJniExceptions(env) || jni_result_callback == nullptr;

  bool should_cancel_new_callback = false;
  pthread_mutex_lock(&g_task_callbacks_mutex);
  if (create_failed) {
    data->complete = true;
    RemoveCallbackDataLocked(data);
    GetLiveCallbacksLocked().erase(data_key);
  } else if (data->complete) {
    should_cancel_new_callback = true;
    RemoveCallbackDataLocked(data);
  } else {
    data->callback_reference = env->NewGlobalRef(jni_result_callback);
    if (CheckAndClearJniExceptions(env) || !data->callback_reference) {
      data->complete = true;
      RemoveCallbackDataLocked(data);
      should_cancel_new_callback = true;
    }
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  if (jni_result_callback) {
    if (should_cancel_new_callback) {
      env->CallVoidMethod(
          jni_result_callback,
          jniresultcallback::GetMethodId(jniresultcallback::kCancel));
      CheckAndClearJniExceptions(env);
      pthread_mutex_lock(&g_task_callbacks_mutex);
      GetLiveCallbacksLocked().erase(data_key);
      pthread_mutex_unlock(&g_task_callbacks_mutex);
    }
    env->DeleteLocalRef(jni_result_callback);
  }
}

JNIEXPORT void JNICALL JniResultCallback_nativeOnResult(
    JNIEnv* env, jobject clazz, jobject result, jboolean success,
    jboolean cancelled, jstring status_message, jlong callback_fn_param,
    jlong callback_data) {
  (void)clazz;
  jni::JniCallbackScope callback_scope(env);
  void* user_callback_data = nullptr;
  bool should_invoke_callback = (callback_data == 0);
  pthread_mutex_lock(&g_task_callbacks_mutex);
  if (callback_data != 0) {
    auto& live = GetLiveCallbacksLocked();
    auto it = live.find(static_cast<uintptr_t>(callback_data));
    if (it != live.end()) {
      std::shared_ptr<CallbackData> data = std::move(it->second);
      live.erase(it);
      user_callback_data = data->data;
      should_invoke_callback = true;
      data->complete = true;
      if (data->callback_reference) {
        env->DeleteGlobalRef(data->callback_reference);
        data->callback_reference = nullptr;
      }
      RemoveCallbackDataLocked(data);
    }
  }
  pthread_mutex_unlock(&g_task_callbacks_mutex);

  assert(!(success && cancelled));
  if (!should_invoke_callback) return;

  std::string status_message_c = JStringToString(env, status_message);
  TaskCallbackFn* callback_fn =
      reinterpret_cast<TaskCallbackFn*>(callback_fn_param);
  FutureResult result_code =
      success ? kFutureResultSuccess
              : (cancelled ? kFutureResultCancelled : kFutureResultFailure);
  if (callback_fn) {
    callback_fn(env, result, result_code, status_message_c.c_str(),
                user_callback_data);
  }
}

JNIEXPORT void JNICALL CppThreadDispatcherContext_nativeFunction(
    JNIEnv* env, jobject clazz, jlong function_ptr, jlong function_data) {
  (void)clazz;
  jni::JniCallbackScope callback_scope(env);
  void (*func)(void*) = reinterpret_cast<void (*)(void*)>(function_ptr);
  if (func) func(reinterpret_cast<void*>(function_data));
}

JavaThreadContext::JavaThreadContext(JNIEnv* env) : object_(env, nullptr) {}

JavaThreadContext::~JavaThreadContext() {}

void JavaThreadContext::Cancel() {
  JNIEnv* env = GetThreadsafeJNIEnv(object_.jvm());
  jobject dispatcher = object_.get();
  if (env && dispatcher) {
    env->CallVoidMethod(dispatcher, cppthreaddispatchercontext::GetMethodId(
                                        cppthreaddispatchercontext::kCancel));
    CheckAndClearJniExceptions(env);
  }
}

void JavaThreadContext::ReleaseExecuteCancelLock() {
  JNIEnv* env = GetThreadsafeJNIEnv(object_.jvm());
  jobject dispatcher = object_.get();
  if (env && dispatcher) {
    env->CallVoidMethod(
        dispatcher, cppthreaddispatchercontext::GetMethodId(
                        cppthreaddispatchercontext::kReleaseExecuteCancelLock));
    CheckAndClearJniExceptions(env);
  }
}

bool JavaThreadContext::AcquireExecuteCancelLock() {
  JNIEnv* env = GetThreadsafeJNIEnv(object_.jvm());
  bool acquired = false;
  jobject dispatcher = object_.get();
  if (env && dispatcher) {
    acquired =
        env->CallBooleanMethod(
            dispatcher,
            cppthreaddispatchercontext::GetMethodId(
                cppthreaddispatchercontext::kAcquireExecuteCancelLock)) != 0;
    CheckAndClearJniExceptions(env);
  }
  return acquired;
}

bool JavaThreadContext::Initialize(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  static const JNINativeMethod kCppThreadMethods[] = {
      {"nativeFunction", "(JJ)V",
       reinterpret_cast<void*>(CppThreadDispatcherContext_nativeFunction)}};

  return cppthreaddispatchercontext::CacheClassFromFiles(env, activity_object,
                                                         &embedded_files) &&
         cppthreaddispatchercontext::CacheMethodIds(env, activity_object) &&
         cppthreaddispatchercontext::RegisterNatives(
             env, kCppThreadMethods, FIREBASE_ARRAYSIZE(kCppThreadMethods)) &&
         cppthreaddispatcher::CacheClassFromFiles(env, activity_object,
                                                  &embedded_files) &&
         cppthreaddispatcher::CacheMethodIds(env, activity_object);
}

void JavaThreadContext::Terminate(JNIEnv* env) {
  cppthreaddispatchercontext::ReleaseClass(env);
  cppthreaddispatcher::ReleaseClass(env);
}

jobject JavaThreadContext::SetupInstance(JNIEnv* env, Callback function_ptr,
                                         void* function_data,
                                         Callback cancel_function_ptr,
                                         JavaThreadContext* context) {
  jobject java_context =
      env->NewObject(cppthreaddispatchercontext::GetClass(),
                     cppthreaddispatchercontext::GetMethodId(
                         cppthreaddispatchercontext::kConstructor),
                     reinterpret_cast<jlong>(function_ptr),
                     reinterpret_cast<jlong>(function_data),
                     reinterpret_cast<jlong>(cancel_function_ptr));
  CheckAndClearJniExceptions(env);
  if (context) context->object_.reset(env, java_context);
  return java_context;
}

void JavaThreadContext::RunOnMainThread(
    JNIEnv* env, jobject activity_object,
    JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr,
    JavaThreadContext* context) {
  jobject java_context = SetupInstance(env, function_ptr, function_data,
                                       cancel_function_ptr, context);
  if (java_context) {
    env->CallStaticVoidMethod(
        cppthreaddispatcher::GetClass(),
        cppthreaddispatcher::GetMethodId(cppthreaddispatcher::kRunOnMainThread),
        activity_object, java_context);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(java_context);
  }
}

void JavaThreadContext::RunOnBackgroundThread(
    JNIEnv* env, JavaThreadContext::Callback function_ptr, void* function_data,
    JavaThreadContext::Callback cancel_function_ptr,
    JavaThreadContext* context) {
  jobject java_context = SetupInstance(env, function_ptr, function_data,
                                       cancel_function_ptr, context);
  if (java_context) {
    env->CallStaticVoidMethod(cppthreaddispatcher::GetClass(),
                              cppthreaddispatcher::GetMethodId(
                                  cppthreaddispatcher::kRunOnBackgroundThread),
                              java_context);
    CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(java_context);
  }
}

void RunOnMainThread(JNIEnv* env, jobject activity_object,
                     JavaThreadContext::Callback function_ptr,
                     void* function_data,
                     JavaThreadContext::Callback cancel_function_ptr,
                     JavaThreadContext* context) {
  JavaThreadContext::RunOnMainThread(env, activity_object, function_ptr,
                                     function_data, cancel_function_ptr,
                                     context);
}

void RunOnBackgroundThread(JNIEnv* env,
                           JavaThreadContext::Callback function_ptr,
                           void* function_data,
                           JavaThreadContext::Callback cancel_function_ptr,
                           JavaThreadContext* context) {
  JavaThreadContext::RunOnBackgroundThread(env, function_ptr, function_data,
                                           cancel_function_ptr, context);
}

void CancelCallbacks(JNIEnv* env, const char* api_identifier) {
  if (!env || !g_task_callbacks) return;
  LogDebug("Cancel pending callbacks for \"%s\"",
           api_identifier ? api_identifier : "<all>");
  for (;;) {
    std::shared_ptr<CallbackData> to_cancel;
    jobject callback_reference = nullptr;
    pthread_mutex_lock(&g_task_callbacks_mutex);
    if (g_task_callbacks) {
      while (g_task_callbacks && !g_task_callbacks->empty()) {
        auto map_it = api_identifier ? g_task_callbacks->find(api_identifier)
                                     : g_task_callbacks->begin();
        if (map_it == g_task_callbacks->end()) break;
        if (map_it->second.empty()) {
          g_task_callbacks->erase(map_it);
          if (api_identifier) break;
          continue;
        }
        std::shared_ptr<CallbackData> front = map_it->second.front();
        front->complete = true;
        callback_reference = front->callback_reference;
        front->callback_reference = nullptr;
        RemoveCallbackDataLocked(front);
        if (callback_reference != nullptr) {
          to_cancel = std::move(front);
          break;
        }
      }
    }
    pthread_mutex_unlock(&g_task_callbacks_mutex);
    if (!to_cancel || !callback_reference) break;

    env->CallVoidMethod(callback_reference, jniresultcallback::GetMethodId(
                                                jniresultcallback::kCancel));
    CheckAndClearJniExceptions(env);
    env->DeleteGlobalRef(callback_reference);
    pthread_mutex_lock(&g_task_callbacks_mutex);
    GetLiveCallbacksLocked().erase(
        reinterpret_cast<uintptr_t>(to_cancel.get()));
    pthread_mutex_unlock(&g_task_callbacks_mutex);
  }
}

jclass FindClassGlobal(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>* embedded_files,
    const char* class_name, ClassRequirement optional) {
  if (!env || !class_name) return nullptr;
  LogDebug("Looking up class %s", class_name);
  jclass local_class = FindClass(env, class_name);
  if (!local_class && embedded_files) {
    local_class =
        FindClassInFiles(env, activity_object, *embedded_files, class_name);
  }
  LogDebug("Class %s, lref %p", class_name,
           reinterpret_cast<void*>(local_class));
  if (!local_class) {
    if (optional == kClassRequired) {
      LogError(kMissingJavaClassError, class_name, class_name);
    }
    return nullptr;
  }
  jclass global_class = static_cast<jclass>(env->NewGlobalRef(local_class));
  env->DeleteLocalRef(local_class);
  LogDebug("Class %s, gref %p", class_name,
           reinterpret_cast<void*>(global_class));
  CheckAndClearJniExceptions(env);
  if (!global_class && optional == kClassRequired) {
    LogError(kMissingJavaClassError, class_name, class_name);
  }
  return global_class;
}

jclass FindClass(JNIEnv* env, const char* class_name) {
  if (!env || !class_name) return nullptr;
  jclass class_object = env->FindClass(class_name);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    class_object = FindOrLoadClassFromLoaders(env, class_name);
  }
  return class_object;
}

const std::vector<internal::EmbeddedFile>& CacheEmbeddedFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  if (!env || !activity_object || embedded_files.empty()) return embedded_files;
  jobject cache_dir = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetCacheDir));
  if (CheckAndClearJniExceptions(env) || !cache_dir) {
    if (cache_dir) env->DeleteLocalRef(cache_dir);
    return embedded_files;
  }
  for (const auto& ef : embedded_files) {
    LogDebug("Caching %s", ef.name);
    jstring filename = jni::NewRawJString(env, ef.name);
    jobject output_file = env->NewObject(
        file::GetClass(), file::GetMethodId(file::kConstructorFilePath),
        cache_dir, filename);
    if (filename) env->DeleteLocalRef(filename);
    if (CheckAndClearJniExceptions(env) || !output_file) {
      if (output_file) env->DeleteLocalRef(output_file);
      break;
    }
    if (env->CallBooleanMethod(output_file, file::GetMethodId(file::kExists))) {
      CheckAndClearJniExceptions(env);
      env->CallBooleanMethod(output_file, file::GetMethodId(file::kSetWritable),
                             JNI_TRUE);
      CheckAndClearJniExceptions(env);
      env->CallBooleanMethod(output_file, file::GetMethodId(file::kDelete));
      CheckAndClearJniExceptions(env);
    }
    CheckAndClearJniExceptions(env);
    jobject output_stream = env->NewObject(
        file_output_stream::GetClass(),
        file_output_stream::GetMethodId(file_output_stream::kConstructorFile),
        output_file);
    bool failed = CheckAndClearJniExceptions(env) || !output_stream;
    if (!failed) {
      jboolean did_set_readonly = env->CallBooleanMethod(
          output_file, file::GetMethodId(file::kSetReadOnly));
      if (CheckAndClearJniExceptions(env)) did_set_readonly = JNI_FALSE;
      const jsize jsize_bytes = jni::SizeToJniSize(ef.size);
      jbyteArray output_array = env->NewByteArray(jsize_bytes);
      failed |= CheckAndClearJniExceptions(env) || !output_array;
      if (!failed && output_array) {
        if (jsize_bytes > 0 && ef.data != nullptr) {
          env->SetByteArrayRegion(output_array, 0, jsize_bytes,
                                  reinterpret_cast<const jbyte*>(ef.data));
          failed |= CheckAndClearJniExceptions(env);
        }
        if (!failed) {
          env->CallVoidMethod(
              output_stream,
              file_output_stream::GetMethodId(file_output_stream::kWrite),
              output_array, 0, jsize_bytes);
          failed |= CheckAndClearJniExceptions(env);
        }
      }
      env->CallVoidMethod(output_stream, file_output_stream::GetMethodId(
                                             file_output_stream::kClose));
      failed |= CheckAndClearJniExceptions(env);
      if (failed) {
        env->CallBooleanMethod(output_file,
                               file::GetMethodId(file::kSetWritable), JNI_TRUE);
        CheckAndClearJniExceptions(env);
        env->CallBooleanMethod(output_file, file::GetMethodId(file::kDelete));
        CheckAndClearJniExceptions(env);
      } else if (!did_set_readonly) {
        env->CallBooleanMethod(output_file,
                               file::GetMethodId(file::kSetReadOnly));
        util::CheckAndClearJniExceptions(env);
      }
      if (output_array) env->DeleteLocalRef(output_array);
      env->DeleteLocalRef(output_stream);
    }
    env->DeleteLocalRef(output_file);
    if (failed) {
      LogError(
          "Unable to cache file %s, embedded Java class loading will fail.",
          ef.name);
      break;
    }
  }
  env->DeleteLocalRef(cache_dir);
  return embedded_files;
}

jclass FindClassInFiles(
    JNIEnv* env, jobject activity_object,
    const std::vector<internal::EmbeddedFile>& embedded_files,
    const char* class_name) {
  if (!env || !activity_object || embedded_files.empty() || !class_name)
    return nullptr;

  jobject cache_dir = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetCacheDir));
  if (CheckAndClearJniExceptions(env) || !cache_dir) {
    if (cache_dir) env->DeleteLocalRef(cache_dir);
    return nullptr;
  }
  jobject cache_dir_path_jstring = env->CallObjectMethod(
      cache_dir, file::GetMethodId(file::kGetAbsolutePath));
  if (CheckAndClearJniExceptions(env) || !cache_dir_path_jstring) {
    if (cache_dir_path_jstring) env->DeleteLocalRef(cache_dir_path_jstring);
    env->DeleteLocalRef(cache_dir);
    return nullptr;
  }
  std::string cache_dir_path =
      firebase::util::JniStringToString(env, cache_dir_path_jstring);

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  jobject cache_uri =
      env->CallObjectMethod(cache_dir, file::GetMethodId(file::kToUri));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(cache_dir);
  if (!cache_uri) return nullptr;
  jobject cache_url =
      env->CallObjectMethod(cache_uri, java_uri::GetMethodId(java_uri::kToUrl));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(cache_uri);
  if (!cache_url) return nullptr;
  jobjectArray url_path_array = env->NewObjectArray(
      jni::SizeToJniSize(embedded_files.size()), url::GetClass(), nullptr);
  if (CheckAndClearJniExceptions(env) || !url_path_array) {
    if (url_path_array) env->DeleteLocalRef(url_path_array);
    env->DeleteLocalRef(cache_url);
    return nullptr;
  }
  for (size_t i = 0; i < embedded_files.size(); ++i) {
    jstring embedded_file_string =
        jni::NewRawJString(env, embedded_files[i].name);
    jobject jar_url = env->NewObject(url::GetClass(),
                                     url::GetMethodId(url::kConstructorWithURL),
                                     cache_url, embedded_file_string);
    CheckAndClearJniExceptions(env);
    if (jar_url) {
      env->SetObjectArrayElement(url_path_array, static_cast<jsize>(i),
                                 jar_url);
      CheckAndClearJniExceptions(env);
      env->DeleteLocalRef(jar_url);
    }
    if (embedded_file_string) env->DeleteLocalRef(embedded_file_string);
  }
  env->DeleteLocalRef(cache_url);

  jobject class_loader_obj = env->NewObject(
      url_class_loader::GetClass(),
      url_class_loader::GetMethodId(url_class_loader::kConstructor),
      url_path_array, GetParentLoader());
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(url_path_array);
  if (!class_loader_obj) return nullptr;

  std::string binary_name = JniClassNameToBinaryName(class_name);
  LogDebug("Load class %s (a.k.a. %s)", binary_name.c_str(), class_name);
  jstring class_name_object = jni::NewRawJString(env, binary_name.c_str());
  jclass loaded_class = static_cast<jclass>(env->CallObjectMethod(
      class_loader_obj,
      url_class_loader::GetMethodId(url_class_loader::kLoadClass),
      class_name_object));
  const bool load_failed =
      CheckAndClearJniExceptions(env) || loaded_class == nullptr;
#else
  const jmethodID get_code_cache_dir_method_id =
      activity::GetMethodId(activity::kGetCodeCacheDir) != 0
          ? activity::GetMethodId(activity::kGetCodeCacheDir)
          : activity::GetMethodId(activity::kGetCacheDir);
  jobject code_cache_dir =
      env->CallObjectMethod(activity_object, get_code_cache_dir_method_id);
  if (CheckAndClearJniExceptions(env) || !code_cache_dir) {
    if (code_cache_dir) env->DeleteLocalRef(code_cache_dir);
    env->DeleteLocalRef(cache_dir);
    return nullptr;
  }
  jobject code_cache_dir_path = env->CallObjectMethod(
      code_cache_dir, file::GetMethodId(file::kGetAbsolutePath));
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(code_cache_dir);
  env->DeleteLocalRef(cache_dir);
  if (!code_cache_dir_path) return nullptr;

  std::string dex_path;
  for (const auto& ef : embedded_files) {
    dex_path += cache_dir_path + '/' + std::string(ef.name) + ':';
  }
  if (!dex_path.empty()) dex_path.pop_back();
  LogDebug("Set class path to %s", dex_path.c_str());

  jstring dex_path_string = jni::NewRawJString(env, dex_path.c_str());
  jobject class_loader_obj = env->NewObject(
      dex_class_loader::GetClass(),
      dex_class_loader::GetMethodId(dex_class_loader::kConstructor),
      dex_path_string, code_cache_dir_path, nullptr, GetParentLoader());
  CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(code_cache_dir_path);
  if (dex_path_string) env->DeleteLocalRef(dex_path_string);
  if (!class_loader_obj) return nullptr;

  std::string binary_name = JniClassNameToBinaryName(class_name);
  LogDebug("Load class %s (a.k.a. %s)", binary_name.c_str(), class_name);
  jstring class_name_object = jni::NewRawJString(env, binary_name.c_str());
  jclass loaded_class = static_cast<jclass>(env->CallObjectMethod(
      class_loader_obj,
      dex_class_loader::GetMethodId(dex_class_loader::kLoadClass),
      class_name_object));
  const bool load_failed =
      CheckAndClearJniExceptions(env) || loaded_class == nullptr;
#endif

  if (!load_failed) {
    LogDebug("%s loaded.", class_name);
    AddClassLoader(env, class_loader_obj);
  } else {
    LogDebug("%s *not* loaded", class_name);
    if (loaded_class) {
      env->DeleteLocalRef(loaded_class);
      loaded_class = nullptr;
    }
    if (class_loader_obj) env->DeleteLocalRef(class_loader_obj);
  }
  if (class_name_object) env->DeleteLocalRef(class_name_object);
  return loaded_class;
}

int GetResourceIdFromActivity(JNIEnv* env, jobject activity_object,
                              const char* resource_name,
                              ResourceType resource_type) {
  if (!env || !activity_object || !resource_name) return 0;
  const size_t type_idx = static_cast<size_t>(resource_type);
  if (type_idx >= FIREBASE_ARRAYSIZE(kResourceTypeStrings)) return 0;
  jobject resources_object = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetResources));
  if (CheckAndClearJniExceptions(env) || !resources_object) {
    if (resources_object) env->DeleteLocalRef(resources_object);
    return 0;
  }
  jobject package_name = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetPackageName));
  if (CheckAndClearJniExceptions(env) || !package_name) {
    if (package_name) env->DeleteLocalRef(package_name);
    env->DeleteLocalRef(resources_object);
    return 0;
  }
  jobject resource_type_string =
      jni::NewRawJString(env, kResourceTypeStrings[type_idx]);
  jobject resource_name_string = jni::NewRawJString(env, resource_name);
  int resource_id = 0;
  if (!CheckAndClearJniExceptions(env) && resource_type_string &&
      resource_name_string) {
    resource_id = env->CallIntMethod(
        resources_object, resources::GetMethodId(resources::kGetIdentifier),
        resource_name_string, resource_type_string, package_name);
    CheckAndClearJniExceptions(env);
  }
  if (resource_name_string) env->DeleteLocalRef(resource_name_string);
  if (resource_type_string) env->DeleteLocalRef(resource_type_string);
  env->DeleteLocalRef(package_name);
  env->DeleteLocalRef(resources_object);
  return resource_id;
}

std::string GetResourceStringFromActivity(JNIEnv* env, jobject activity_object,
                                          int resource_id) {
  if (!env || !activity_object || resource_id == 0) return std::string();
  jobject resource_value_string = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetString),
      resource_id);
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, resource_value_string);
}

std::string GetPackageName(JNIEnv* env, jobject activity_object) {
  if (!env || !activity_object) return std::string();
  jobject package_name_string = env->CallObjectMethod(
      activity_object, activity::GetMethodId(activity::kGetPackageName));
  CheckAndClearJniExceptions(env);
  return JniStringToString(env, package_name_string);
}

bool CheckAndClearJniExceptions(JNIEnv* env) {
  if (env && env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
  }
  return false;
}

std::string GetAndClearExceptionMessage(JNIEnv* env) {
  if (!env || !env->ExceptionCheck()) return std::string();
  jobject exception = env->ExceptionOccurred();
  if (exception) {
    env->ExceptionClear();
    std::string exception_message_out = GetMessageFromException(env, exception);
    env->DeleteLocalRef(exception);
    return exception_message_out;
  }
  return std::string();
}

std::string GetMessageFromException(JNIEnv* env, jobject exception) {
  if (!env || !exception) return std::string();
  jstring message = static_cast<jstring>(env->CallObjectMethod(
      exception, throwable::GetMethodId(throwable::kGetLocalizedMessage)));
  CheckAndClearJniExceptions(env);
  if (!message) {
    message = static_cast<jstring>(env->CallObjectMethod(
        exception, throwable::GetMethodId(throwable::kGetMessage)));
    CheckAndClearJniExceptions(env);
  }
  if (!message || env->GetStringUTFLength(message) == 0) {
    if (message) env->DeleteLocalRef(message);
    message = static_cast<jstring>(env->CallObjectMethod(
        exception, throwable::GetMethodId(throwable::kToString)));
    CheckAndClearJniExceptions(env);
  }
  return message ? JniStringToString(env, message)
                 : std::string("Unknown Exception.");
}

bool LogException(JNIEnv* env, LogLevel log_level, const char* log_fmt, ...) {
  if (!env || !env->ExceptionCheck()) return false;
  jobject exception = env->ExceptionOccurred();
  if (!exception) return false;
  env->ExceptionClear();
  std::string message_str = GetMessageFromException(env, exception);
  env->DeleteLocalRef(exception);
  if (!message_str.empty()) {
    if (log_fmt != nullptr) {
      char buf[512] = {0};
      va_list list;
      va_start(list, log_fmt);
      vsnprintf(buf, sizeof(buf), log_fmt, list);
      va_end(list);
      message_str = std::string(buf) + ": " + message_str;
    }
    LogMessage(log_level, "%s", message_str.c_str());
  }
  return true;
}

pthread_key_t jni_env_key;
pthread_once_t pthread_key_initialized = PTHREAD_ONCE_INIT;

namespace {
extern "C" void DetachJVMThreads(void* stored_java_vm) {
  if (!stored_java_vm) return;
  JavaVM* java_vm = static_cast<JavaVM*>(stored_java_vm);
  JNIEnv* jni_env = nullptr;
  if (java_vm->GetEnv(jni::internal::JniEnvOut(&jni_env), JNI_VERSION_1_6) ==
          JNI_OK &&
      jni_env != nullptr) {
    if (jni_env->ExceptionCheck()) jni_env->ExceptionClear();
    jni::internal::GlobalRefReaper::Flush(jni_env);
    java_vm->DetachCurrentThread();
  }
}

extern "C" void SetupJvmDetachOnThreadDestruction() {
  pthread_key_create(&jni_env_key, DetachJVMThreads);
}
}  // namespace

JNIEnv* GetThreadsafeJNIEnv(JavaVM* java_vm) {
  if (!java_vm) return nullptr;
  jni::internal::GlobalRefReaper::SetJavaVM(java_vm);
  (void)pthread_once(&pthread_key_initialized,
                     SetupJvmDetachOnThreadDestruction);

  JNIEnv* env = nullptr;
  jint get_env_result =
      java_vm->GetEnv(jni::internal::JniEnvOut(&env), JNI_VERSION_1_6);
  if (get_env_result == JNI_OK && env != nullptr) {
    jni::internal::GlobalRefReaper::Flush(env);
    return env;
  }
  if (get_env_result == JNI_EDETACHED) {
    jint result = firebase::util::AttachCurrentThread(java_vm, &env);
    if (result == JNI_OK && env != nullptr) {
      pthread_setspecific(jni_env_key, java_vm);
      jni::internal::GlobalRefReaper::Flush(env);
      return env;
    }
  }
  return nullptr;
}

jint AttachCurrentThread(JavaVM* java_vm, JNIEnv** env) {
  return java_vm->AttachCurrentThread(
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      reinterpret_cast<void**>(env),
#else
      env,
#endif
      nullptr);
}

JNIEnv* GetJNIEnvFromApp() {
  App* app = app_common::GetDefaultApp();
  if (!app) app = app_common::GetAnyApp();
  return app ? app->GetJNIEnv() : nullptr;
}

}  // namespace util
}  // namespace firebase
