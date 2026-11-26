/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/http/http_transport.h"
#include "app/src/util_android.h"

namespace firebase {
namespace http {
namespace {

std::atomic<JavaVM*> g_android_jvm{nullptr};

JavaVM* ResolveJavaVM() {
  JavaVM* vm = g_android_jvm.load(std::memory_order_acquire);
  if (vm != nullptr) {
    return vm;
  }
  JNIEnv* env = ::firebase::util::GetJNIEnvFromApp();
  if (env != nullptr && env->GetJavaVM(&vm) == JNI_OK && vm != nullptr) {
    g_android_jvm.store(vm, std::memory_order_release);
    return vm;
  }
  return nullptr;
}

std::string ToLowerAscii(std::string_view s) {
  std::string out(s.size(), '\0');
  for (size_t i = 0; i < s.size(); ++i) {
    out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
  }
  return out;
}

template <typename T>
class LocalRef {
 public:
  LocalRef(JNIEnv* env, T ref) : env_(env), ref_(ref) {}
  ~LocalRef() { reset(); }

  LocalRef(const LocalRef&) = delete;
  LocalRef& operator=(const LocalRef&) = delete;

  LocalRef(LocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
    other.ref_ = nullptr;
  }

  LocalRef& operator=(LocalRef&& other) noexcept {
    if (this != &other) {
      reset();
      env_ = other.env_;
      ref_ = other.ref_;
      other.ref_ = nullptr;
    }
    return *this;
  }

  T get() const { return ref_; }
  explicit operator bool() const { return ref_ != nullptr; }

  void reset(T next = nullptr) {
    if (ref_ != nullptr && env_ != nullptr) {
      env_->DeleteLocalRef(ref_);
    }
    ref_ = next;
  }

 private:
  JNIEnv* env_ = nullptr;
  T ref_ = nullptr;
};

struct JniCache {
  bool initialized = false;

  jclass url_cls = nullptr;
  jmethodID url_ctor = nullptr;
  jmethodID url_open_conn = nullptr;
  jmethodID url_open_conn_proxy = nullptr;

  jclass proxy_cls = nullptr;
  jmethodID proxy_ctor = nullptr;
  jobject proxy_type_http = nullptr;

  jclass inet_sock_addr_cls = nullptr;
  jmethodID inet_sock_addr_ctor = nullptr;

  jclass http_conn_cls = nullptr;
  jmethodID set_request_method = nullptr;
  jmethodID set_request_property = nullptr;
  jmethodID add_request_property = nullptr;
  jmethodID set_use_caches = nullptr;
  jmethodID set_connect_timeout = nullptr;
  jmethodID set_read_timeout = nullptr;
  jmethodID set_instance_follow_redirects = nullptr;
  jmethodID set_do_output = nullptr;
  jmethodID set_fixed_len_streaming = nullptr;
  jmethodID set_chunked_streaming = nullptr;
  jmethodID get_output_stream = nullptr;
  jmethodID get_response_code = nullptr;
  jmethodID get_response_message = nullptr;
  jmethodID get_header_fields = nullptr;
  jmethodID get_input_stream = nullptr;
  jmethodID get_error_stream = nullptr;
  jmethodID disconnect = nullptr;

  jclass output_stream_cls = nullptr;
  jmethodID output_write = nullptr;
  jmethodID output_flush = nullptr;
  jmethodID output_close = nullptr;

  jclass input_stream_cls = nullptr;
  jmethodID input_read = nullptr;
  jmethodID input_close = nullptr;

  jclass map_cls = nullptr;
  jmethodID map_entry_set = nullptr;

  jclass set_cls = nullptr;
  jmethodID set_iterator = nullptr;

  jclass iter_cls = nullptr;
  jmethodID iter_has_next = nullptr;
  jmethodID iter_next = nullptr;

  jclass entry_cls = nullptr;
  jmethodID entry_get_key = nullptr;
  jmethodID entry_get_value = nullptr;

  jclass list_cls = nullptr;
  jmethodID list_size = nullptr;
  jmethodID list_get = nullptr;

  jclass throwable_cls = nullptr;
  jmethodID throwable_to_string = nullptr;
};

JniCache& GetJniCache() {
  static auto* cache = new JniCache();
  return *cache;
}

std::string JStringToStdString(JNIEnv* env, jstring jstr) {
  if (jstr == nullptr) {
    return std::string();
  }
  const char* chars = env->GetStringUTFChars(jstr, nullptr);
  if (chars == nullptr) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
    return std::string();
  }
  std::string out(chars);
  env->ReleaseStringUTFChars(jstr, chars);
  return out;
}

std::optional<std::string> TakeJniException(JNIEnv* env) {
  if (!env->ExceptionCheck()) {
    return std::nullopt;
  }
  LocalRef<jthrowable> ex(env, env->ExceptionOccurred());
  env->ExceptionClear();
  if (!ex) {
    return std::string("Unknown JNI exception");
  }
  auto& c = GetJniCache();
  if (c.throwable_to_string != nullptr) {
    LocalRef<jstring> msg(env, static_cast<jstring>(env->CallObjectMethod(
                                   ex.get(), c.throwable_to_string)));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return std::string("Java exception thrown during HTTP request");
    }
    return JStringToStdString(env, msg.get());
  }
  return std::string("Java exception thrown during HTTP request");
}

jclass CacheGlobalClass(JNIEnv* env, const char* name) {
  LocalRef<jclass> local(env, env->FindClass(name));
  if (env->ExceptionCheck() || !local) {
    env->ExceptionClear();
    return nullptr;
  }
  jobject global = env->NewGlobalRef(local.get());
  if (env->ExceptionCheck() || global == nullptr) {
    env->ExceptionClear();
    return nullptr;
  }
  return static_cast<jclass>(global);
}

jmethodID CacheMethod(JNIEnv* env, jclass cls, const char* name,
                      const char* sig, bool* ok) {
  if (!*ok || cls == nullptr) {
    *ok = false;
    return nullptr;
  }
  jmethodID mid = env->GetMethodID(cls, name, sig);
  if (env->ExceptionCheck() || mid == nullptr) {
    env->ExceptionClear();
    *ok = false;
    return nullptr;
  }
  return mid;
}

bool EnsureJniInitialized(JNIEnv* env) {
  static std::mutex init_mutex;
  std::lock_guard<std::mutex> lock(init_mutex);
  auto& c = GetJniCache();
  if (c.initialized) {
    return true;
  }

  bool ok = true;
  c.throwable_cls = CacheGlobalClass(env, "java/lang/Throwable");
  c.throwable_to_string = CacheMethod(env, c.throwable_cls, "toString",
                                      "()Ljava/lang/String;", &ok);
  if (!ok) {
    return false;
  }

  c.url_cls = CacheGlobalClass(env, "java/net/URL");
  c.proxy_cls = CacheGlobalClass(env, "java/net/Proxy");
  c.inet_sock_addr_cls = CacheGlobalClass(env, "java/net/InetSocketAddress");
  c.http_conn_cls = CacheGlobalClass(env, "java/net/HttpURLConnection");
  c.output_stream_cls = CacheGlobalClass(env, "java/io/OutputStream");
  c.input_stream_cls = CacheGlobalClass(env, "java/io/InputStream");
  c.map_cls = CacheGlobalClass(env, "java/util/Map");
  c.set_cls = CacheGlobalClass(env, "java/util/Set");
  c.iter_cls = CacheGlobalClass(env, "java/util/Iterator");
  c.entry_cls = CacheGlobalClass(env, "java/util/Map$Entry");
  c.list_cls = CacheGlobalClass(env, "java/util/List");

  if (!c.url_cls || !c.proxy_cls || !c.inet_sock_addr_cls || !c.http_conn_cls ||
      !c.output_stream_cls || !c.input_stream_cls || !c.map_cls || !c.set_cls ||
      !c.iter_cls || !c.entry_cls || !c.list_cls) {
    return false;
  }

  LocalRef<jclass> proxy_type_cls(env, env->FindClass("java/net/Proxy$Type"));
  if (env->ExceptionCheck() || !proxy_type_cls) {
    env->ExceptionClear();
    return false;
  }
  jfieldID http_field = env->GetStaticFieldID(proxy_type_cls.get(), "HTTP",
                                              "Ljava/net/Proxy$Type;");
  if (env->ExceptionCheck() || http_field == nullptr) {
    env->ExceptionClear();
    return false;
  }
  LocalRef<jobject> http_enum(
      env, env->GetStaticObjectField(proxy_type_cls.get(), http_field));
  if (env->ExceptionCheck() || !http_enum) {
    env->ExceptionClear();
    return false;
  }
  c.proxy_type_http = env->NewGlobalRef(http_enum.get());
  if (env->ExceptionCheck() || c.proxy_type_http == nullptr) {
    env->ExceptionClear();
    return false;
  }

  c.url_ctor =
      CacheMethod(env, c.url_cls, "<init>", "(Ljava/lang/String;)V", &ok);
  c.url_open_conn = CacheMethod(env, c.url_cls, "openConnection",
                                "()Ljava/net/URLConnection;", &ok);
  c.url_open_conn_proxy =
      CacheMethod(env, c.url_cls, "openConnection",
                  "(Ljava/net/Proxy;)Ljava/net/URLConnection;", &ok);

  c.proxy_ctor =
      CacheMethod(env, c.proxy_cls, "<init>",
                  "(Ljava/net/Proxy$Type;Ljava/net/SocketAddress;)V", &ok);
  c.inet_sock_addr_ctor = CacheMethod(env, c.inet_sock_addr_cls, "<init>",
                                      "(Ljava/lang/String;I)V", &ok);

  c.set_request_method = CacheMethod(env, c.http_conn_cls, "setRequestMethod",
                                     "(Ljava/lang/String;)V", &ok);
  c.set_request_property =
      CacheMethod(env, c.http_conn_cls, "setRequestProperty",
                  "(Ljava/lang/String;Ljava/lang/String;)V", &ok);
  c.add_request_property =
      CacheMethod(env, c.http_conn_cls, "addRequestProperty",
                  "(Ljava/lang/String;Ljava/lang/String;)V", &ok);
  c.set_use_caches =
      CacheMethod(env, c.http_conn_cls, "setUseCaches", "(Z)V", &ok);
  c.set_connect_timeout =
      CacheMethod(env, c.http_conn_cls, "setConnectTimeout", "(I)V", &ok);
  c.set_read_timeout =
      CacheMethod(env, c.http_conn_cls, "setReadTimeout", "(I)V", &ok);
  c.set_instance_follow_redirects = CacheMethod(
      env, c.http_conn_cls, "setInstanceFollowRedirects", "(Z)V", &ok);
  c.set_do_output =
      CacheMethod(env, c.http_conn_cls, "setDoOutput", "(Z)V", &ok);
  c.set_fixed_len_streaming = CacheMethod(
      env, c.http_conn_cls, "setFixedLengthStreamingMode", "(J)V", &ok);
  c.set_chunked_streaming =
      CacheMethod(env, c.http_conn_cls, "setChunkedStreamingMode", "(I)V", &ok);
  c.get_output_stream = CacheMethod(env, c.http_conn_cls, "getOutputStream",
                                    "()Ljava/io/OutputStream;", &ok);
  c.get_response_code =
      CacheMethod(env, c.http_conn_cls, "getResponseCode", "()I", &ok);
  c.get_response_message = CacheMethod(
      env, c.http_conn_cls, "getResponseMessage", "()Ljava/lang/String;", &ok);
  c.get_header_fields = CacheMethod(env, c.http_conn_cls, "getHeaderFields",
                                    "()Ljava/util/Map;", &ok);
  c.get_input_stream = CacheMethod(env, c.http_conn_cls, "getInputStream",
                                   "()Ljava/io/InputStream;", &ok);
  c.get_error_stream = CacheMethod(env, c.http_conn_cls, "getErrorStream",
                                   "()Ljava/io/InputStream;", &ok);
  c.disconnect = CacheMethod(env, c.http_conn_cls, "disconnect", "()V", &ok);

  c.output_write =
      CacheMethod(env, c.output_stream_cls, "write", "([BII)V", &ok);
  c.output_flush = CacheMethod(env, c.output_stream_cls, "flush", "()V", &ok);
  c.output_close = CacheMethod(env, c.output_stream_cls, "close", "()V", &ok);

  c.input_read = CacheMethod(env, c.input_stream_cls, "read", "([BII)I", &ok);
  c.input_close = CacheMethod(env, c.input_stream_cls, "close", "()V", &ok);

  c.map_entry_set =
      CacheMethod(env, c.map_cls, "entrySet", "()Ljava/util/Set;", &ok);
  c.set_iterator =
      CacheMethod(env, c.set_cls, "iterator", "()Ljava/util/Iterator;", &ok);
  c.iter_has_next = CacheMethod(env, c.iter_cls, "hasNext", "()Z", &ok);
  c.iter_next =
      CacheMethod(env, c.iter_cls, "next", "()Ljava/lang/Object;", &ok);
  c.entry_get_key =
      CacheMethod(env, c.entry_cls, "getKey", "()Ljava/lang/Object;", &ok);
  c.entry_get_value =
      CacheMethod(env, c.entry_cls, "getValue", "()Ljava/lang/Object;", &ok);
  c.list_size = CacheMethod(env, c.list_cls, "size", "()I", &ok);
  c.list_get =
      CacheMethod(env, c.list_cls, "get", "(I)Ljava/lang/Object;", &ok);

  if (!ok) {
    return false;
  }

  c.initialized = true;
  return true;
}

struct AndroidCancelHandle {
  std::mutex mutex;
  jobject conn_global = nullptr;
  jobject out_stream_global = nullptr;
  jobject in_stream_global = nullptr;

  void ClearAll(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(mutex);
    if (in_stream_global != nullptr) {
      env->DeleteGlobalRef(in_stream_global);
      in_stream_global = nullptr;
    }
    if (out_stream_global != nullptr) {
      env->DeleteGlobalRef(out_stream_global);
      out_stream_global = nullptr;
    }
    if (conn_global != nullptr) {
      env->DeleteGlobalRef(conn_global);
      conn_global = nullptr;
    }
  }

  void AbortNow(JNIEnv* env) {
    if (env == nullptr) {
      return;
    }
    auto& c = GetJniCache();
    jobject conn_ref = nullptr;
    jobject in_ref = nullptr;
    jobject out_ref = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (conn_global != nullptr) {
        conn_ref = env->NewLocalRef(conn_global);
      }
      if (in_stream_global != nullptr) {
        in_ref = env->NewLocalRef(in_stream_global);
      }
      if (out_stream_global != nullptr) {
        out_ref = env->NewLocalRef(out_stream_global);
      }
    }
    LocalRef<jobject> local_conn(env, conn_ref);
    LocalRef<jobject> local_in(env, in_ref);
    LocalRef<jobject> local_out(env, out_ref);

    // Call conn.disconnect() FIRST to unblock any worker thread waiting inside
    // InputStream.read() / OutputStream.write() before closing the streams,
    // avoiding cross-thread stream monitor contention in OkHttp/Conscrypt.
    if (local_conn && c.disconnect != nullptr) {
      env->CallVoidMethod(local_conn.get(), c.disconnect);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (local_in && c.input_close != nullptr) {
      env->CallVoidMethod(local_in.get(), c.input_close);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (local_out && c.output_close != nullptr) {
      env->CallVoidMethod(local_out.get(), c.output_close);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }
};

// Dedicated background dispatcher for asynchronous JNI socket cancellation so
// `HttpCancelRequestInternal` / `HttpDestroyRequestInternal` invoked on the
// Android UI or Unity main thread never triggers `NetworkOnMainThreadException`
// or blocks the caller on Java stream teardown.
class AndroidCancelDispatcher {
 public:
  static AndroidCancelDispatcher& Instance() {
    static auto* dispatcher = new AndroidCancelDispatcher();
    return *dispatcher;
  }

  void Enqueue(const std::shared_ptr<AndroidCancelHandle>& handle) {
    bool start_thread = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push_back(handle);
      if (!started_) {
        started_ = true;
        start_thread = true;
      }
    }
    cv_.notify_one();
    if (start_thread) {
      std::thread([this]() { RunLoop(); }).detach();
    }
  }

 private:
  static JNIEnv* AttachCancelThread(JavaVM** out_vm, bool* out_attached) {
    JavaVM* vm = ResolveJavaVM();
    if (vm == nullptr) {
      return nullptr;
    }
    JNIEnv* env = nullptr;
    jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
      JavaVMAttachArgs args{};
      args.version = JNI_VERSION_1_6;
      args.name = const_cast<char*>("FirebaseHttpCancel");
      args.group = nullptr;
      if (vm->AttachCurrentThread(&env, &args) == JNI_OK) {
        *out_vm = vm;
        *out_attached = true;
      } else {
        return nullptr;
      }
    } else if (status == JNI_OK) {
      *out_vm = vm;
    }
    return env;
  }

  void RunLoop() {
    JavaVM* vm = nullptr;
    bool attached = false;
    JNIEnv* env = AttachCancelThread(&vm, &attached);
    while (true) {
      std::shared_ptr<AndroidCancelHandle> next;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty(); });
        next = std::move(queue_.front());
        queue_.pop_front();
      }
      if (env == nullptr) {
        env = AttachCancelThread(&vm, &attached);
      }
      if (env != nullptr && next) {
        next->AbortNow(env);
      }
    }
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<AndroidCancelHandle>> queue_;
  bool started_ = false;
};

bool IsRedirectStatus(int32_t status) {
  return status == 301 || status == 302 || status == 303 || status == 307 ||
         status == 308;
}

std::string_view FindHeaderValueIgnoreCase(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view target) {
  for (const auto& kv : headers) {
    if (AsciiEqualsIgnoreCase(kv.first, target)) {
      return kv.second;
    }
  }
  return {};
}

void ExecuteAndroidOperation(JNIEnv* env,
                             const std::shared_ptr<HttpOperation>& op) {
  if (!EnsureJniInitialized(env)) {
    op->MarkFailed("Failed to initialize Android JNI HTTP bindings.");
    return;
  }
  if (!IsHttpOrHttpsUrl(op->url)) {
    op->MarkFailed("Unsupported URI scheme; only http and https are allowed.");
    return;
  }

  auto cancel_handle = std::make_shared<AndroidCancelHandle>();
  {
    std::lock_guard<std::mutex> lock(op->mutex);
    if (op->IsCanceled()) {
      return;
    }
    op->on_cancel = [cancel_handle]() {
      AndroidCancelDispatcher::Instance().Enqueue(cancel_handle);
    };
  }

  bool completed_cleanly = false;
  struct CleanupGuard {
    JNIEnv* env;
    std::shared_ptr<AndroidCancelHandle> handle;
    bool* completed_cleanly;
    ~CleanupGuard() {
      if (!*completed_cleanly) {
        handle->AbortNow(env);
      }
      handle->ClearAll(env);
    }
  } guard{env, cancel_handle, &completed_cleanly};

  auto& c = GetJniCache();
  std::string current_url = op->url;
  std::string active_method = op->method;
  std::vector<std::pair<std::string, std::string>> active_headers =
      op->request_headers;
  int64_t active_content_len = op->content_length;

  for (int redirect_count = 0; redirect_count <= kMaxRedirects;
       ++redirect_count) {
    cancel_handle->ClearAll(env);

    LocalRef<jstring> jurl_str(env, env->NewStringUTF(current_url.c_str()));
    if (auto ex = TakeJniException(env); ex || !jurl_str) {
      op->MarkFailed(ex.value_or("Failed to allocate JNI URL string"));
      return;
    }
    LocalRef<jobject> jurl(
        env, env->NewObject(c.url_cls, c.url_ctor, jurl_str.get()));
    if (auto ex = TakeJniException(env); ex || !jurl) {
      op->MarkFailed(ex.value_or("Invalid URL"));
      return;
    }

    LocalRef<jobject> conn(env, nullptr);
    if (auto proxy_hp = ParseProxyHostPort(op->proxy_url)) {
      LocalRef<jstring> jhost(env, env->NewStringUTF(proxy_hp->first.c_str()));
      if (auto ex = TakeJniException(env); ex || !jhost) {
        op->MarkFailed(ex.value_or("Failed to allocate proxy host string"));
        return;
      }
      LocalRef<jobject> jsock_addr(
          env,
          env->NewObject(c.inet_sock_addr_cls, c.inet_sock_addr_ctor,
                         jhost.get(), static_cast<jint>(proxy_hp->second)));
      if (auto ex = TakeJniException(env); ex || !jsock_addr) {
        op->MarkFailed(ex.value_or("Failed to construct proxy socket address"));
        return;
      }
      LocalRef<jobject> jproxy(
          env, env->NewObject(c.proxy_cls, c.proxy_ctor, c.proxy_type_http,
                              jsock_addr.get()));
      if (auto ex = TakeJniException(env); ex || !jproxy) {
        op->MarkFailed(ex.value_or("Failed to construct Java Proxy"));
        return;
      }
      conn.reset(env->CallObjectMethod(jurl.get(), c.url_open_conn_proxy,
                                       jproxy.get()));
    } else {
      conn.reset(env->CallObjectMethod(jurl.get(), c.url_open_conn));
    }
    if (auto ex = TakeJniException(env); ex || !conn) {
      op->MarkFailed(ex.value_or("Failed to open HttpURLConnection"));
      return;
    }
    if (env->IsInstanceOf(conn.get(), c.http_conn_cls) != JNI_TRUE) {
      op->MarkFailed(
          "Unsupported URI scheme; URLConnection is not an HttpURLConnection.");
      return;
    }

    {
      jobject g_conn = env->NewGlobalRef(conn.get());
      if (auto ex = TakeJniException(env); ex || g_conn == nullptr) {
        op->MarkFailed(ex.value_or("Failed to retain HttpURLConnection"));
        return;
      }
      std::lock_guard<std::mutex> lock(cancel_handle->mutex);
      cancel_handle->conn_global = g_conn;
    }
    if (op->IsCanceled()) {
      return;
    }

    env->CallVoidMethod(conn.get(), c.set_use_caches, JNI_FALSE);
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(*ex);
      return;
    }
    env->CallVoidMethod(conn.get(), c.set_connect_timeout, 60000);
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(*ex);
      return;
    }
    env->CallVoidMethod(conn.get(), c.set_read_timeout, 0);
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(*ex);
      return;
    }
    // Handle redirects explicitly so cross-origin redirects strip sensitive
    // Firebase credentials and HTTPS -> HTTP downgrades are rejected.
    env->CallVoidMethod(conn.get(), c.set_instance_follow_redirects, JNI_FALSE);
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(*ex);
      return;
    }

    LocalRef<jstring> jmethod(env, env->NewStringUTF(active_method.c_str()));
    if (auto ex = TakeJniException(env); ex || !jmethod) {
      op->MarkFailed(ex.value_or("Failed to allocate HTTP method string"));
      return;
    }
    env->CallVoidMethod(conn.get(), c.set_request_method, jmethod.get());
    if (auto ex = TakeJniException(env)) {
      if (AsciiEqualsIgnoreCase(active_method, "PATCH")) {
        // Fallback for HttpURLConnection implementations that reject PATCH.
        LocalRef<jstring> jpost(env, env->NewStringUTF("POST"));
        if (auto ex_post = TakeJniException(env); ex_post || !jpost) {
          op->MarkFailed(ex_post.value_or("Failed to allocate POST string"));
          return;
        }
        LocalRef<jstring> joverride_k(
            env, env->NewStringUTF("X-HTTP-Method-Override"));
        if (auto ex_k = TakeJniException(env); ex_k || !joverride_k) {
          op->MarkFailed(ex_k.value_or("Failed to allocate override header"));
          return;
        }
        LocalRef<jstring> joverride_v(env, env->NewStringUTF("PATCH"));
        if (auto ex_v = TakeJniException(env); ex_v || !joverride_v) {
          op->MarkFailed(ex_v.value_or("Failed to allocate PATCH string"));
          return;
        }
        env->CallVoidMethod(conn.get(), c.set_request_method, jpost.get());
        if (auto ex2 = TakeJniException(env)) {
          op->MarkFailed(*ex2);
          return;
        }
        env->CallVoidMethod(conn.get(), c.set_request_property,
                            joverride_k.get(), joverride_v.get());
        if (auto ex3 = TakeJniException(env)) {
          op->MarkFailed(*ex3);
          return;
        }
      } else {
        op->MarkFailed(*ex);
        return;
      }
    }

    bool has_accept_encoding = false;
    std::unordered_set<std::string> seen_headers;
    for (const auto& kv : active_headers) {
      if (AsciiEqualsIgnoreCase(kv.first, "Accept-Encoding")) {
        has_accept_encoding = true;
      }
      LocalRef<jstring> jk(env, env->NewStringUTF(kv.first.c_str()));
      if (auto ex = TakeJniException(env); ex || !jk) {
        op->MarkFailed(ex.value_or("Failed to allocate header name"));
        return;
      }
      LocalRef<jstring> jv(env, env->NewStringUTF(kv.second.c_str()));
      if (auto ex = TakeJniException(env); ex || !jv) {
        op->MarkFailed(ex.value_or("Failed to allocate header value"));
        return;
      }
      jmethodID header_mid = seen_headers.insert(ToLowerAscii(kv.first)).second
                                 ? c.set_request_property
                                 : c.add_request_property;
      env->CallVoidMethod(conn.get(), header_mid, jk.get(), jv.get());
      if (auto ex = TakeJniException(env)) {
        op->MarkFailed(*ex);
        return;
      }
    }
    if (!has_accept_encoding) {
      // Match .NET HttpClientHandler (AutomaticDecompression = None) and
      // Desktop libcurl so Android's OkHttp BridgeInterceptor does not inject
      // `Accept-Encoding: gzip`, strip Content-Length, buffer SSE frames, or
      // corrupt Range ETag / byte offsets.
      LocalRef<jstring> jae_k(env, env->NewStringUTF("Accept-Encoding"));
      if (auto ex = TakeJniException(env); ex || !jae_k) {
        op->MarkFailed(ex.value_or("Failed to allocate Accept-Encoding key"));
        return;
      }
      LocalRef<jstring> jae_v(env, env->NewStringUTF("identity"));
      if (auto ex = TakeJniException(env); ex || !jae_v) {
        op->MarkFailed(ex.value_or("Failed to allocate identity value"));
        return;
      }
      env->CallVoidMethod(conn.get(), c.set_request_property, jae_k.get(),
                          jae_v.get());
      if (auto ex = TakeJniException(env)) {
        op->MarkFailed(*ex);
        return;
      }
    }

    // Stream request body if present. Never call setDoOutput(true) on bodyless
    // GET/HEAD/DELETE requests because HttpURLConnection rewrites GET -> POST.
    bool send_body = active_content_len >= 0 ||
                     active_content_len == kChunkedRequestContent ||
                     (active_content_len == kNoRequestContent &&
                      (AsciiEqualsIgnoreCase(active_method, "POST") ||
                       AsciiEqualsIgnoreCase(active_method, "PUT") ||
                       AsciiEqualsIgnoreCase(active_method, "PATCH")));
    if (send_body) {
      env->CallVoidMethod(conn.get(), c.set_do_output, JNI_TRUE);
      if (auto ex = TakeJniException(env)) {
        op->MarkFailed(*ex);
        return;
      }
      if (active_content_len == kNoRequestContent || active_content_len == 0) {
        env->CallVoidMethod(conn.get(), c.set_fixed_len_streaming,
                            static_cast<jlong>(0));
      } else if (active_content_len > 0) {
        env->CallVoidMethod(conn.get(), c.set_fixed_len_streaming,
                            static_cast<jlong>(active_content_len));
      } else {
        env->CallVoidMethod(conn.get(), c.set_chunked_streaming,
                            static_cast<jint>(65536));
      }
      if (auto ex = TakeJniException(env)) {
        op->MarkFailed(*ex);
        return;
      }

      LocalRef<jobject> out_stream(
          env, env->CallObjectMethod(conn.get(), c.get_output_stream));
      if (auto ex = TakeJniException(env); ex || !out_stream) {
        if (!op->IsCanceled()) {
          op->MarkFailed(ex.value_or("Failed to open request OutputStream"));
        }
        return;
      }
      {
        jobject g_out = env->NewGlobalRef(out_stream.get());
        if (auto ex = TakeJniException(env); ex || g_out == nullptr) {
          op->MarkFailed(ex.value_or("Failed to retain request OutputStream"));
          return;
        }
        std::lock_guard<std::mutex> lock(cancel_handle->mutex);
        cancel_handle->out_stream_global = g_out;
      }

      if (active_content_len > 0 ||
          active_content_len == kChunkedRequestContent) {
        constexpr size_t kWriteSlice = 64 * 1024;
        LocalRef<jbyteArray> jwrite_buf(
            env, env->NewByteArray(static_cast<jsize>(kWriteSlice)));
        if (auto ex = TakeJniException(env); ex || !jwrite_buf) {
          op->MarkFailed(ex.value_or("Failed to allocate JNI write buffer"));
          return;
        }
        std::vector<uint8_t> scratch(kWriteSlice);
        while (!op->IsCanceled()) {
          bool eof = false;
          size_t n = op->PopRequestBytes(scratch.data(), kWriteSlice, &eof);
          if (n > 0) {
            env->SetByteArrayRegion(
                jwrite_buf.get(), 0, static_cast<jsize>(n),
                reinterpret_cast<const jbyte*>(scratch.data()));
            if (auto ex = TakeJniException(env)) {
              op->MarkFailed(*ex);
              return;
            }
            env->CallVoidMethod(out_stream.get(), c.output_write,
                                jwrite_buf.get(), 0, static_cast<jsize>(n));
            if (auto ex = TakeJniException(env)) {
              if (!op->IsCanceled()) op->MarkFailed(*ex);
              return;
            }
          } else if (eof) {
            break;
          } else {
            std::unique_lock<std::mutex> lock(op->mutex);
            op->req_cv.wait(lock, [&op]() {
              return !op->req_chunks.empty() || op->req_eof || op->failed ||
                     op->IsCanceled();
            });
          }
        }
      }

      env->CallVoidMethod(out_stream.get(), c.output_flush);
      if (env->ExceptionCheck()) env->ExceptionClear();
      env->CallVoidMethod(out_stream.get(), c.output_close);
      if (auto ex = TakeJniException(env)) {
        if (!op->IsCanceled()) op->MarkFailed(*ex);
        return;
      }
      {
        std::lock_guard<std::mutex> lock(cancel_handle->mutex);
        if (cancel_handle->out_stream_global != nullptr) {
          env->DeleteGlobalRef(cancel_handle->out_stream_global);
          cancel_handle->out_stream_global = nullptr;
        }
      }
    }

    if (op->IsCanceled()) {
      return;
    }

    jint status_code = env->CallIntMethod(conn.get(), c.get_response_code);
    if (auto ex = TakeJniException(env)) {
      if (!op->IsCanceled()) op->MarkFailed(*ex);
      return;
    }
    LocalRef<jstring> jreason(env, static_cast<jstring>(env->CallObjectMethod(
                                       conn.get(), c.get_response_message)));
    if (env->ExceptionCheck()) env->ExceptionClear();
    std::string reason = JStringToStdString(env, jreason.get());

    std::vector<std::pair<std::string, std::string>> resp_headers;
    LocalRef<jobject> header_map(
        env, env->CallObjectMethod(conn.get(), c.get_header_fields));
    if (!TakeJniException(env) && header_map) {
      LocalRef<jobject> entry_set(
          env, env->CallObjectMethod(header_map.get(), c.map_entry_set));
      if (!TakeJniException(env) && entry_set) {
        LocalRef<jobject> iter(
            env, env->CallObjectMethod(entry_set.get(), c.set_iterator));
        if (!TakeJniException(env) && iter) {
          while (true) {
            jboolean has_next =
                env->CallBooleanMethod(iter.get(), c.iter_has_next);
            if (TakeJniException(env) || has_next != JNI_TRUE) {
              break;
            }
            LocalRef<jobject> entry(
                env, env->CallObjectMethod(iter.get(), c.iter_next));
            if (TakeJniException(env) || !entry) break;
            LocalRef<jstring> jkey(
                env, static_cast<jstring>(
                         env->CallObjectMethod(entry.get(), c.entry_get_key)));
            if (TakeJniException(env) || !jkey) {
              // HttpURLConnection maps the status line under a null key.
              continue;
            }
            std::string key_str = JStringToStdString(env, jkey.get());
            if (key_str.empty()) continue;

            LocalRef<jobject> val_list(
                env, env->CallObjectMethod(entry.get(), c.entry_get_value));
            if (TakeJniException(env) || !val_list) continue;
            jint count = env->CallIntMethod(val_list.get(), c.list_size);
            if (TakeJniException(env)) continue;
            for (jint i = 0; i < count; ++i) {
              LocalRef<jstring> jval(env,
                                     static_cast<jstring>(env->CallObjectMethod(
                                         val_list.get(), c.list_get, i)));
              if (TakeJniException(env)) break;
              if (jval) {
                resp_headers.emplace_back(key_str,
                                          JStringToStdString(env, jval.get()));
              }
            }
          }
        }
      }
    }

    std::string_view location =
        FindHeaderValueIgnoreCase(resp_headers, "Location");
    if (IsRedirectStatus(static_cast<int32_t>(status_code)) &&
        !location.empty()) {
      // Drain/close response stream on the redirect hop so the pooled socket
      // stays reusable before following.
      LocalRef<jobject> redir_stream(
          env, env->CallObjectMethod(conn.get(), c.get_input_stream));
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (redir_stream) {
        env->CallVoidMethod(redir_stream.get(), c.input_close);
        if (env->ExceptionCheck()) env->ExceptionClear();
      }

      if (redirect_count == kMaxRedirects) {
        op->MarkFailed("Maximum HTTP redirects exceeded.");
        return;
      }
      auto resolved = ResolveRedirectUrl(current_url, location);
      if (!resolved || (IsHttpsUrl(op->url) && !IsHttpsUrl(*resolved))) {
        op->MarkFailed("Blocked unsafe or scheme-downgraded HTTP redirect.");
        return;
      }
      bool same_origin = IsSameOrigin(op->url, *resolved) &&
                         IsSameOrigin(current_url, *resolved);
      if (status_code == 303 ||
          ((status_code == 301 || status_code == 302) &&
           !AsciiEqualsIgnoreCase(active_method, "GET") &&
           !AsciiEqualsIgnoreCase(active_method, "HEAD"))) {
        active_method = "GET";
        active_content_len = kNoRequestContent;
      } else if (active_content_len != kNoRequestContent &&
                 active_content_len != 0) {
        op->MarkFailed("Cannot replay streamed request body across redirect.");
        return;
      }

      if (!same_origin || active_content_len == kNoRequestContent) {
        std::vector<std::pair<std::string, std::string>> filtered;
        filtered.reserve(active_headers.size());
        for (const auto& kv : active_headers) {
          if (!same_origin && (IsSensitiveRedirectHeader(kv.first) ||
                               AsciiEqualsIgnoreCase(kv.first, "Host"))) {
            continue;
          }
          if (active_content_len == kNoRequestContent &&
              (AsciiEqualsIgnoreCase(kv.first, "Content-Type") ||
               AsciiEqualsIgnoreCase(kv.first, "Content-Length") ||
               AsciiEqualsIgnoreCase(kv.first, "Transfer-Encoding"))) {
            continue;
          }
          filtered.push_back(kv);
        }
        active_headers = std::move(filtered);
      }
      current_url = std::move(*resolved);
      continue;
    }

    op->CommitResponseHeaders(static_cast<int32_t>(status_code), reason,
                              std::move(resp_headers));
    if (op->IsCanceled()) {
      return;
    }

    LocalRef<jobject> in_stream(
        env, env->CallObjectMethod(conn.get(), c.get_input_stream));
    if (env->ExceptionCheck() || !in_stream) {
      env->ExceptionClear();
      in_stream.reset(env->CallObjectMethod(conn.get(), c.get_error_stream));
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        in_stream.reset(nullptr);
      }
    }

    if (!in_stream) {
      completed_cleanly = true;
      op->MarkResponseComplete();
      return;
    }

    {
      jobject g_in = env->NewGlobalRef(in_stream.get());
      if (auto ex = TakeJniException(env); ex || g_in == nullptr) {
        op->MarkFailed(ex.value_or("Failed to retain response InputStream"));
        return;
      }
      std::lock_guard<std::mutex> lock(cancel_handle->mutex);
      cancel_handle->in_stream_global = g_in;
    }
    if (op->IsCanceled()) {
      return;
    }

    constexpr size_t kReadSlice = 64 * 1024;
    LocalRef<jbyteArray> jread_buf(
        env, env->NewByteArray(static_cast<jsize>(kReadSlice)));
    if (auto ex = TakeJniException(env); ex || !jread_buf) {
      op->MarkFailed(ex.value_or("Failed to allocate JNI read buffer"));
      return;
    }
    std::vector<uint8_t> read_scratch(kReadSlice);

    while (!op->IsCanceled()) {
      jint n =
          env->CallIntMethod(in_stream.get(), c.input_read, jread_buf.get(), 0,
                             static_cast<jsize>(kReadSlice));
      if (auto ex = TakeJniException(env)) {
        if (!op->IsCanceled()) op->MarkFailed(*ex);
        return;
      }
      if (n < 0) {
        break;
      }
      if (n == 0) {
        continue;
      }
      env->GetByteArrayRegion(jread_buf.get(), 0, n,
                              reinterpret_cast<jbyte*>(read_scratch.data()));
      if (auto ex = TakeJniException(env)) {
        op->MarkFailed(*ex);
        return;
      }
      bool over_high_water =
          op->PushResponseBytes(read_scratch.data(), static_cast<size_t>(n));
      if (over_high_water && !op->IsCanceled()) {
        std::unique_lock<std::mutex> lock(op->mutex);
        op->resp_cv.wait(lock, [&op]() {
          return op->resp_buffered_bytes <= kLowWaterResponseBytes ||
                 op->IsCanceled() || op->failed;
        });
      }
    }

    if (op->IsCanceled()) {
      return;
    }

    // Close the response stream so Android's ConnectionPool can reuse the live
    // TLS connection; do not call conn.disconnect() on normal completion.
    env->CallVoidMethod(in_stream.get(), c.input_close);
    if (env->ExceptionCheck()) env->ExceptionClear();
    completed_cleanly = true;
    op->MarkResponseComplete();
    return;
  }
}

class AndroidHttpThreadPool {
 public:
  static AndroidHttpThreadPool& Instance() {
    static auto* pool = new AndroidHttpThreadPool();
    return *pool;
  }

  void Enqueue(const std::shared_ptr<HttpOperation>& op) {
    // Capture JavaVM on the caller thread if an App is already initialized so
    // newly spawned workers immediately see a cached JavaVM*.
    (void)ResolveJavaVM();
    bool spawn_worker = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push_back(op);
      if (queue_.size() > idle_workers_ + starting_workers_ &&
          active_workers_ < kMaxWorkers) {
        ++active_workers_;
        ++starting_workers_;
        spawn_worker = true;
      }
    }
    cv_.notify_one();
    if (spawn_worker) {
      std::thread([this]() { WorkerLoop(); }).detach();
    }
  }

 private:
  // Matches Desktop's CURLMOPT_MAX_TOTAL_CONNECTIONS (64) so concurrent open
  // SSE / GetStreamAsync() streams never starve short requests.
  static constexpr size_t kMaxWorkers = 64;

  static JNIEnv* AttachWorkerThread(JavaVM** out_vm, bool* out_attached) {
    JavaVM* vm = ResolveJavaVM();
    if (vm == nullptr) {
      return nullptr;
    }
    JNIEnv* env = nullptr;
    jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
      JavaVMAttachArgs args{};
      args.version = JNI_VERSION_1_6;
      args.name = const_cast<char*>("FirebaseHttpWorker");
      args.group = nullptr;
      if (vm->AttachCurrentThread(&env, &args) == JNI_OK) {
        *out_vm = vm;
        *out_attached = true;
      } else {
        return nullptr;
      }
    } else if (status == JNI_OK) {
      *out_vm = vm;
    }
    return env;
  }

  void WorkerLoop() {
    JavaVM* vm = nullptr;
    bool attached = false;
    JNIEnv* env = AttachWorkerThread(&vm, &attached);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      --starting_workers_;
    }

    while (true) {
      std::shared_ptr<HttpOperation> next;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
          ++idle_workers_;
          bool has_work = cv_.wait_for(lock, std::chrono::seconds(30),
                                       [this]() { return !queue_.empty(); });
          --idle_workers_;
          if (!has_work) {
            --active_workers_;
            break;
          }
        }
        next = std::move(queue_.front());
        queue_.pop_front();
      }

      if (env == nullptr) {
        env = AttachWorkerThread(&vm, &attached);
      }
      if (env == nullptr) {
        next->MarkFailed("Android JavaVM / JNIEnv is unavailable.");
      } else {
        ExecuteAndroidOperation(env, next);
      }
    }

    if (attached && vm != nullptr) {
      vm->DetachCurrentThread();
    }
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<HttpOperation>> queue_;
  size_t active_workers_ = 0;
  size_t idle_workers_ = 0;
  size_t starting_workers_ = 0;
};

}  // namespace

void SetAndroidJavaVM(JavaVM* vm) {
  if (vm != nullptr) {
    g_android_jvm.store(vm, std::memory_order_release);
  }
}

void StartPlatformHttpOperation(const std::shared_ptr<HttpOperation>& op) {
  AndroidHttpThreadPool::Instance().Enqueue(op);
}

}  // namespace http
}  // namespace firebase
