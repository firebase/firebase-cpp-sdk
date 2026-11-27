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

JNIEnv* ResolveThreadsafeJNIEnv() {
  JavaVM* vm = ResolveJavaVM();
  if (vm == nullptr) {
    return nullptr;
  }
  return ::firebase::util::GetThreadsafeJNIEnv(vm);
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

struct JniExceptionInfo {
  std::string message;
  HttpErrorClass error_class = kHttpErrorOther;
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
  jmethodID throwable_get_cause = nullptr;

  // Optional exception classes for structured error classification (S4).
  jclass ssl_ex_cls = nullptr;
  jclass cert_ex_cls = nullptr;
  jclass socket_timeout_ex_cls = nullptr;
  jclass connect_ex_cls = nullptr;
  jclass unknown_host_ex_cls = nullptr;
  jclass no_route_ex_cls = nullptr;
  jclass socket_ex_cls = nullptr;
  jclass eof_ex_cls = nullptr;
};

JniCache& GetJniCache() {
  static auto* cache = new JniCache();
  return *cache;
}

HttpErrorClass ClassifyJavaException(JNIEnv* env, jthrowable root_ex) {
  auto& c = GetJniCache();
  if (root_ex == nullptr) {
    return kHttpErrorOther;
  }

  HttpErrorClass best = kHttpErrorOther;
  LocalRef<jobject> current(env, env->NewLocalRef(root_ex));
  for (int depth = 0; current && depth < 8; ++depth) {
    if ((c.ssl_ex_cls != nullptr &&
         env->IsInstanceOf(current.get(), c.ssl_ex_cls) == JNI_TRUE) ||
        (c.cert_ex_cls != nullptr &&
         env->IsInstanceOf(current.get(), c.cert_ex_cls) == JNI_TRUE)) {
      return kHttpErrorTls;
    }
    if (c.socket_timeout_ex_cls != nullptr &&
        env->IsInstanceOf(current.get(), c.socket_timeout_ex_cls) == JNI_TRUE) {
      if (best == kHttpErrorOther) {
        best = kHttpErrorTimeout;
      }
    } else if ((c.connect_ex_cls != nullptr &&
                env->IsInstanceOf(current.get(), c.connect_ex_cls) ==
                    JNI_TRUE) ||
               (c.unknown_host_ex_cls != nullptr &&
                env->IsInstanceOf(current.get(), c.unknown_host_ex_cls) ==
                    JNI_TRUE) ||
               (c.no_route_ex_cls != nullptr &&
                env->IsInstanceOf(current.get(), c.no_route_ex_cls) ==
                    JNI_TRUE) ||
               (c.socket_ex_cls != nullptr &&
                env->IsInstanceOf(current.get(), c.socket_ex_cls) ==
                    JNI_TRUE) ||
               (c.eof_ex_cls != nullptr &&
                env->IsInstanceOf(current.get(), c.eof_ex_cls) == JNI_TRUE)) {
      if (best == kHttpErrorOther) {
        best = kHttpErrorConnect;
      }
    }

    if (c.throwable_get_cause == nullptr) {
      break;
    }
    jobject next = env->CallObjectMethod(current.get(), c.throwable_get_cause);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (next == nullptr || env->IsSameObject(current.get(), next) == JNI_TRUE) {
      if (next != nullptr) {
        env->DeleteLocalRef(next);
      }
      break;
    }
    current.reset(next);
  }
  return best;
}

std::optional<JniExceptionInfo> TakeJniException(JNIEnv* env) {
  if (!env->ExceptionCheck()) {
    return std::nullopt;
  }
  LocalRef<jthrowable> ex(env, env->ExceptionOccurred());
  env->ExceptionClear();
  JniExceptionInfo info;
  if (!ex) {
    info.message = "Unknown JNI exception";
    return info;
  }
  info.error_class = ClassifyJavaException(env, ex.get());

  // Reuse firebase::util::GetMessageFromException when util_android has been
  // initialized, falling back to cached Throwable.toString() if HTTP is used
  // before FirebaseApp initialization.
  if (::firebase::util::throwable::GetClass() != nullptr &&
      ::firebase::util::throwable::GetMethodId(
          ::firebase::util::throwable::kToString) != nullptr) {
    info.message = ::firebase::util::GetMessageFromException(env, ex.get());
    if (!info.message.empty()) {
      return info;
    }
  }
  auto& c = GetJniCache();
  if (c.throwable_to_string != nullptr) {
    LocalRef<jstring> msg(env, static_cast<jstring>(env->CallObjectMethod(
                                   ex.get(), c.throwable_to_string)));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      info.message = "Java exception thrown during HTTP request";
      return info;
    }
    info.message = ::firebase::util::JStringToString(env, msg.get());
    if (!info.message.empty()) {
      return info;
    }
  }
  info.message = "Java exception thrown during HTTP request";
  return info;
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
  c.throwable_get_cause = CacheMethod(env, c.throwable_cls, "getCause",
                                      "()Ljava/lang/Throwable;", &ok);
  if (!ok) {
    return false;
  }

  // Exception classes for structured error classification (best-effort; all
  // exist on Android API 1+).
  c.ssl_ex_cls = CacheGlobalClass(env, "javax/net/ssl/SSLException");
  c.cert_ex_cls =
      CacheGlobalClass(env, "java/security/cert/CertificateException");
  c.socket_timeout_ex_cls =
      CacheGlobalClass(env, "java/net/SocketTimeoutException");
  c.connect_ex_cls = CacheGlobalClass(env, "java/net/ConnectException");
  c.unknown_host_ex_cls =
      CacheGlobalClass(env, "java/net/UnknownHostException");
  c.no_route_ex_cls = CacheGlobalClass(env, "java/net/NoRouteToHostException");
  c.socket_ex_cls = CacheGlobalClass(env, "java/net/SocketException");
  c.eof_ex_cls = CacheGlobalClass(env, "java/io/EOFException");

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

  void ClearOutputStream(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(mutex);
    if (out_stream_global != nullptr) {
      env->DeleteGlobalRef(out_stream_global);
      out_stream_global = nullptr;
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
  void RunLoop() {
    JNIEnv* env = ResolveThreadsafeJNIEnv();
    while (true) {
      std::shared_ptr<AndroidCancelHandle> next;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty(); });
        next = std::move(queue_.front());
        queue_.pop_front();
      }
      if (env == nullptr) {
        env = ResolveThreadsafeJNIEnv();
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

struct AndroidHopState {
  std::string current_url;
  std::string active_method;
  std::vector<std::pair<std::string, std::string>> active_headers;
  int64_t active_content_len = kNoRequestContent;
};

struct AndroidResponseHead {
  int32_t status_code = 0;
  std::string reason;
  std::vector<std::pair<std::string, std::string>> headers;
};

// Opens and configures a Java HttpURLConnection for one request hop.
LocalRef<jobject> BuildAndroidRequestConnection(
    JNIEnv* env, const std::shared_ptr<HttpOperation>& op,
    const std::shared_ptr<AndroidCancelHandle>& cancel_handle,
    const AndroidHopState& hop) {
  auto& c = GetJniCache();
  LocalRef<jobject> empty(env, nullptr);

  LocalRef<jstring> jurl_str(env, env->NewStringUTF(hop.current_url.c_str()));
  if (auto ex = TakeJniException(env); ex || !jurl_str) {
    op->MarkFailed(ex ? ex->message : "Failed to allocate JNI URL string",
                   ex ? ex->error_class : kHttpErrorOther);
    return empty;
  }
  LocalRef<jobject> jurl(
      env, env->NewObject(c.url_cls, c.url_ctor, jurl_str.get()));
  if (auto ex = TakeJniException(env); ex || !jurl) {
    op->MarkFailed(ex ? ex->message : "Invalid URL",
                   ex ? ex->error_class : kHttpErrorOther);
    return empty;
  }

  LocalRef<jobject> conn(env, nullptr);
  if (!op->proxy_url.empty()) {
    auto proxy_hp = ParseProxyHostPort(op->proxy_url);
    if (!proxy_hp) {
      op->MarkFailed("Invalid or unsupported proxy URL.", kHttpErrorOther);
      return empty;
    }
    LocalRef<jstring> jhost(env, env->NewStringUTF(proxy_hp->first.c_str()));
    if (auto ex = TakeJniException(env); ex || !jhost) {
      op->MarkFailed(ex ? ex->message : "Failed to allocate proxy host string",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    LocalRef<jobject> jsock_addr(
        env, env->NewObject(c.inet_sock_addr_cls, c.inet_sock_addr_ctor,
                            jhost.get(), static_cast<jint>(proxy_hp->second)));
    if (auto ex = TakeJniException(env); ex || !jsock_addr) {
      op->MarkFailed(
          ex ? ex->message : "Failed to construct proxy socket address",
          ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    LocalRef<jobject> jproxy(
        env, env->NewObject(c.proxy_cls, c.proxy_ctor, c.proxy_type_http,
                            jsock_addr.get()));
    if (auto ex = TakeJniException(env); ex || !jproxy) {
      op->MarkFailed(ex ? ex->message : "Failed to construct Java Proxy",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    conn.reset(env->CallObjectMethod(jurl.get(), c.url_open_conn_proxy,
                                     jproxy.get()));
  } else {
    conn.reset(env->CallObjectMethod(jurl.get(), c.url_open_conn));
  }
  if (auto ex = TakeJniException(env); ex || !conn) {
    op->MarkFailed(ex ? ex->message : "Failed to open HttpURLConnection",
                   ex ? ex->error_class : kHttpErrorConnect);
    return empty;
  }
  if (env->IsInstanceOf(conn.get(), c.http_conn_cls) != JNI_TRUE) {
    op->MarkFailed(
        "Unsupported URI scheme; URLConnection is not an HttpURLConnection.",
        kHttpErrorOther);
    return empty;
  }

  {
    jobject g_conn = env->NewGlobalRef(conn.get());
    if (auto ex = TakeJniException(env); ex || g_conn == nullptr) {
      op->MarkFailed(ex ? ex->message : "Failed to retain HttpURLConnection",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    std::lock_guard<std::mutex> lock(cancel_handle->mutex);
    cancel_handle->conn_global = g_conn;
  }
  if (op->IsCanceled()) {
    return empty;
  }

  env->CallVoidMethod(conn.get(), c.set_use_caches, JNI_FALSE);
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return empty;
  }
  env->CallVoidMethod(conn.get(), c.set_connect_timeout, 60000);
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return empty;
  }
  env->CallVoidMethod(conn.get(), c.set_read_timeout, 0);
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return empty;
  }
  // Handle redirects explicitly so cross-origin redirects strip sensitive
  // Firebase credentials and HTTPS -> HTTP downgrades are rejected.
  env->CallVoidMethod(conn.get(), c.set_instance_follow_redirects, JNI_FALSE);
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return empty;
  }

  LocalRef<jstring> jmethod(env, env->NewStringUTF(hop.active_method.c_str()));
  if (auto ex = TakeJniException(env); ex || !jmethod) {
    op->MarkFailed(ex ? ex->message : "Failed to allocate HTTP method string",
                   ex ? ex->error_class : kHttpErrorOther);
    return empty;
  }
  env->CallVoidMethod(conn.get(), c.set_request_method, jmethod.get());
  if (auto ex = TakeJniException(env)) {
    if (AsciiEqualsIgnoreCase(hop.active_method, "PATCH")) {
      // Fallback for HttpURLConnection implementations that reject PATCH.
      LocalRef<jstring> jpost(env, env->NewStringUTF("POST"));
      LocalRef<jstring> joverride_k(
          env, env->NewStringUTF("X-HTTP-Method-Override"));
      LocalRef<jstring> joverride_v(env, env->NewStringUTF("PATCH"));
      if (auto ex_alloc = TakeJniException(env);
          ex_alloc || !jpost || !joverride_k || !joverride_v) {
        op->MarkFailed(
            ex_alloc ? ex_alloc->message : "Failed to allocate PATCH override",
            ex_alloc ? ex_alloc->error_class : kHttpErrorOther);
        return empty;
      }
      env->CallVoidMethod(conn.get(), c.set_request_method, jpost.get());
      if (auto ex2 = TakeJniException(env)) {
        op->MarkFailed(ex2->message, ex2->error_class);
        return empty;
      }
      env->CallVoidMethod(conn.get(), c.set_request_property,
                          joverride_k.get(), joverride_v.get());
      if (auto ex3 = TakeJniException(env)) {
        op->MarkFailed(ex3->message, ex3->error_class);
        return empty;
      }
    } else {
      op->MarkFailed(ex->message, ex->error_class);
      return empty;
    }
  }

  bool has_accept_encoding = false;
  std::unordered_set<std::string> seen_headers;
  for (const auto& kv : hop.active_headers) {
    if (AsciiEqualsIgnoreCase(kv.first, "Accept-Encoding")) {
      has_accept_encoding = true;
    }
    LocalRef<jstring> jk(env, env->NewStringUTF(kv.first.c_str()));
    if (auto ex = TakeJniException(env); ex || !jk) {
      op->MarkFailed(ex ? ex->message : "Failed to allocate header name",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    LocalRef<jstring> jv(env, env->NewStringUTF(kv.second.c_str()));
    if (auto ex = TakeJniException(env); ex || !jv) {
      op->MarkFailed(ex ? ex->message : "Failed to allocate header value",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    jmethodID header_mid = seen_headers.insert(ToLowerAscii(kv.first)).second
                               ? c.set_request_property
                               : c.add_request_property;
    env->CallVoidMethod(conn.get(), header_mid, jk.get(), jv.get());
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(ex->message, ex->error_class);
      return empty;
    }
  }
  if (!has_accept_encoding) {
    // Match .NET HttpClientHandler (AutomaticDecompression = None) and
    // Desktop libcurl so Android's OkHttp BridgeInterceptor does not inject
    // `Accept-Encoding: gzip`, strip Content-Length, buffer SSE frames, or
    // corrupt Range ETag / byte offsets.
    LocalRef<jstring> jae_k(env, env->NewStringUTF("Accept-Encoding"));
    LocalRef<jstring> jae_v(env, env->NewStringUTF("identity"));
    if (auto ex = TakeJniException(env); ex || !jae_k || !jae_v) {
      op->MarkFailed(ex ? ex->message : "Failed to allocate Accept-Encoding",
                     ex ? ex->error_class : kHttpErrorOther);
      return empty;
    }
    env->CallVoidMethod(conn.get(), c.set_request_property, jae_k.get(),
                        jae_v.get());
    if (auto ex = TakeJniException(env)) {
      op->MarkFailed(ex->message, ex->error_class);
      return empty;
    }
  }

  return conn;
}

// Streams request body bytes when the HTTP method permits a body.
// Returns true if the caller should proceed to ReadAndroidResponseHeaders
// (including when an upload write failed mid-stream and an early 4xx response
// may be readable via getResponseCode()), or false if the operation already
// failed or was canceled.
bool SendAndroidRequestBody(
    JNIEnv* env, const std::shared_ptr<HttpOperation>& op,
    const std::shared_ptr<AndroidCancelHandle>& cancel_handle, jobject conn,
    const AndroidHopState& hop, std::optional<JniExceptionInfo>* out_write_err) {
  auto& c = GetJniCache();

  // Never call setDoOutput(true) on GET or HEAD because HttpURLConnection
  // silently rewrites GET into POST (A1).
  bool is_get_or_head = AsciiEqualsIgnoreCase(hop.active_method, "GET") ||
                        AsciiEqualsIgnoreCase(hop.active_method, "HEAD");
  if (is_get_or_head) {
    if (hop.active_content_len > 0 ||
        hop.active_content_len == kChunkedRequestContent) {
      op->MarkFailed("GET and HEAD requests cannot send a request body.",
                     kHttpErrorOther);
      return false;
    }
    return true;
  }

  bool send_body =
      hop.active_content_len >= 0 ||
      hop.active_content_len == kChunkedRequestContent ||
      (hop.active_content_len == kNoRequestContent &&
       (AsciiEqualsIgnoreCase(hop.active_method, "POST") ||
        AsciiEqualsIgnoreCase(hop.active_method, "PUT") ||
        AsciiEqualsIgnoreCase(hop.active_method, "PATCH")));
  if (!send_body) {
    return true;
  }

  env->CallVoidMethod(conn, c.set_do_output, JNI_TRUE);
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return false;
  }
  if (hop.active_content_len == kNoRequestContent ||
      hop.active_content_len == 0) {
    env->CallVoidMethod(conn, c.set_fixed_len_streaming, static_cast<jlong>(0));
  } else if (hop.active_content_len > 0) {
    env->CallVoidMethod(conn, c.set_fixed_len_streaming,
                        static_cast<jlong>(hop.active_content_len));
  } else {
    env->CallVoidMethod(conn, c.set_chunked_streaming, static_cast<jint>(65536));
  }
  if (auto ex = TakeJniException(env)) {
    op->MarkFailed(ex->message, ex->error_class);
    return false;
  }

  LocalRef<jobject> out_stream(env,
                               env->CallObjectMethod(conn, c.get_output_stream));
  if (auto ex = TakeJniException(env); ex || !out_stream) {
    if (!op->IsCanceled()) {
      op->MarkFailed(ex ? ex->message : "Failed to open request OutputStream",
                     ex ? ex->error_class : kHttpErrorConnect);
    }
    return false;
  }
  {
    jobject g_out = env->NewGlobalRef(out_stream.get());
    if (auto ex = TakeJniException(env); ex || g_out == nullptr) {
      op->MarkFailed(ex ? ex->message : "Failed to retain request OutputStream",
                     ex ? ex->error_class : kHttpErrorOther);
      return false;
    }
    std::lock_guard<std::mutex> lock(cancel_handle->mutex);
    cancel_handle->out_stream_global = g_out;
  }

  bool write_failed = false;
  if (hop.active_content_len > 0 ||
      hop.active_content_len == kChunkedRequestContent) {
    constexpr size_t kWriteSlice = 64 * 1024;
    LocalRef<jbyteArray> jwrite_buf(
        env, env->NewByteArray(static_cast<jsize>(kWriteSlice)));
    if (auto ex = TakeJniException(env); ex || !jwrite_buf) {
      cancel_handle->ClearOutputStream(env);
      op->MarkFailed(ex ? ex->message : "Failed to allocate JNI write buffer",
                     ex ? ex->error_class : kHttpErrorOther);
      return false;
    }
    std::vector<uint8_t> scratch(kWriteSlice);
    while (!op->IsCanceled()) {
      bool eof = false;
      size_t n = op->PopRequestBytes(scratch.data(), kWriteSlice, &eof);
      if (n > 0) {
        env->SetByteArrayRegion(jwrite_buf.get(), 0, static_cast<jsize>(n),
                                reinterpret_cast<const jbyte*>(scratch.data()));
        if (auto ex = TakeJniException(env)) {
          cancel_handle->ClearOutputStream(env);
          op->MarkFailed(ex->message, ex->error_class);
          return false;
        }
        env->CallVoidMethod(out_stream.get(), c.output_write, jwrite_buf.get(),
                            0, static_cast<jsize>(n));
        if (auto ex = TakeJniException(env)) {
          if (op->IsCanceled()) {
            cancel_handle->ClearOutputStream(env);
            return false;
          }
          // The server may have sent an early 4xx (such as 413) and closed
          // its read side. Save the write exception, close the upload pipe so
          // the caller stops writing, and fall through to try getResponseCode()
          // before failing the operation (A2).
          *out_write_err = std::move(*ex);
          write_failed = true;
          op->MarkRequestWriteClosed();
          break;
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

  if (!write_failed) {
    env->CallVoidMethod(out_stream.get(), c.output_flush);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  env->CallVoidMethod(out_stream.get(), c.output_close);
  if (auto ex = TakeJniException(env)) {
    if (!write_failed && !op->IsCanceled()) {
      *out_write_err = std::move(*ex);
      op->MarkRequestWriteClosed();
    }
  }
  cancel_handle->ClearOutputStream(env);
  return !op->IsCanceled();
}

// Reads HTTP status code, reason phrase, and response headers from `conn`.
bool ReadAndroidResponseHeaders(
    JNIEnv* env, const std::shared_ptr<HttpOperation>& op, jobject conn,
    const std::optional<JniExceptionInfo>& write_err,
    AndroidResponseHead* out_head) {
  auto& c = GetJniCache();

  jint status_code = env->CallIntMethod(conn, c.get_response_code);
  if (auto ex = TakeJniException(env)) {
    if (!op->IsCanceled()) {
      if (write_err && ex->error_class == kHttpErrorOther &&
          write_err->error_class != kHttpErrorOther) {
        op->MarkFailed(write_err->message, write_err->error_class);
      } else {
        op->MarkFailed(ex->message, ex->error_class);
      }
    }
    return false;
  }
  if (status_code <= 0) {
    if (!op->IsCanceled()) {
      if (write_err) {
        op->MarkFailed(write_err->message, write_err->error_class);
      } else {
        op->MarkFailed("HttpURLConnection returned invalid HTTP status code.",
                       kHttpErrorOther);
      }
    }
    return false;
  }

  out_head->status_code = static_cast<int32_t>(status_code);
  LocalRef<jstring> jreason(
      env,
      static_cast<jstring>(env->CallObjectMethod(conn, c.get_response_message)));
  if (env->ExceptionCheck()) env->ExceptionClear();
  out_head->reason = ::firebase::util::JStringToString(env, jreason.get());
  out_head->headers.clear();

  LocalRef<jobject> header_map(env,
                               env->CallObjectMethod(conn, c.get_header_fields));
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
          LocalRef<jobject> entry(env,
                                  env->CallObjectMethod(iter.get(), c.iter_next));
          if (TakeJniException(env) || !entry) break;
          LocalRef<jstring> jkey(
              env, static_cast<jstring>(
                       env->CallObjectMethod(entry.get(), c.entry_get_key)));
          if (TakeJniException(env) || !jkey) {
            // HttpURLConnection maps the status line under a null key.
            continue;
          }
          std::string key_str =
              ::firebase::util::JStringToString(env, jkey.get());
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
              out_head->headers.emplace_back(
                  key_str, ::firebase::util::JStringToString(env, jval.get()));
            }
          }
        }
      }
    }
  }
  return true;
}

// Updates `hop` for a 3xx redirect response. Returns true to continue to the
// next redirect hop, or false after marking `op` failed.
bool ApplyAndroidRedirect(JNIEnv* env, const std::shared_ptr<HttpOperation>& op,
                          jobject conn, int redirect_count, int32_t status_code,
                          std::string_view location, AndroidHopState* hop) {
  auto& c = GetJniCache();

  // Close the redirect response stream so Android's ConnectionPool can reuse
  // the socket before following the redirect.
  LocalRef<jobject> redir_stream(
      env, env->CallObjectMethod(conn, c.get_input_stream));
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (redir_stream) {
    env->CallVoidMethod(redir_stream.get(), c.input_close);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  if (redirect_count == kMaxRedirects) {
    op->MarkFailed("Maximum HTTP redirects exceeded.", kHttpErrorOther);
    return false;
  }
  auto resolved = ResolveRedirectUrl(hop->current_url, location);
  if (!resolved || (IsHttpsUrl(op->url) && !IsHttpsUrl(*resolved))) {
    op->MarkFailed("Blocked unsafe or scheme-downgraded HTTP redirect.",
                   kHttpErrorOther);
    return false;
  }
  bool same_origin =
      IsSameOrigin(op->url, *resolved) && IsSameOrigin(hop->current_url, *resolved);
  if ((status_code == 301 || status_code == 302 || status_code == 303) &&
      !AsciiEqualsIgnoreCase(hop->active_method, "GET") &&
      !AsciiEqualsIgnoreCase(hop->active_method, "HEAD")) {
    hop->active_method = "GET";
    hop->active_content_len = kNoRequestContent;
    op->MarkRequestWriteClosed();
  } else if (status_code == 303 &&
             AsciiEqualsIgnoreCase(hop->active_method, "HEAD")) {
    hop->active_content_len = kNoRequestContent;
  } else if (hop->active_content_len != kNoRequestContent &&
             hop->active_content_len != 0) {
    op->MarkFailed("Cannot replay streamed request body across redirect.",
                   kHttpErrorOther);
    return false;
  }

  if (!same_origin || hop->active_content_len == kNoRequestContent) {
    std::vector<std::pair<std::string, std::string>> filtered;
    filtered.reserve(hop->active_headers.size());
    for (const auto& kv : hop->active_headers) {
      if (!same_origin && (IsSensitiveRedirectHeader(kv.first) ||
                           AsciiEqualsIgnoreCase(kv.first, "Host"))) {
        continue;
      }
      if (hop->active_content_len == kNoRequestContent &&
          (AsciiEqualsIgnoreCase(kv.first, "Content-Type") ||
           AsciiEqualsIgnoreCase(kv.first, "Content-Length") ||
           AsciiEqualsIgnoreCase(kv.first, "Transfer-Encoding"))) {
        continue;
      }
      filtered.push_back(kv);
    }
    hop->active_headers = std::move(filtered);
  }
  hop->current_url = std::move(*resolved);
  return true;
}

// Streams response body bytes from `conn` into `op` with bounded backpressure.
// Returns true on clean EOF.
bool ReadAndroidResponseBody(
    JNIEnv* env, const std::shared_ptr<HttpOperation>& op,
    const std::shared_ptr<AndroidCancelHandle>& cancel_handle, jobject conn) {
  auto& c = GetJniCache();

  LocalRef<jobject> in_stream(env,
                              env->CallObjectMethod(conn, c.get_input_stream));
  if (env->ExceptionCheck() || !in_stream) {
    env->ExceptionClear();
    in_stream.reset(env->CallObjectMethod(conn, c.get_error_stream));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      in_stream.reset(nullptr);
    }
  }

  if (!in_stream) {
    op->MarkResponseComplete();
    return true;
  }

  {
    jobject g_in = env->NewGlobalRef(in_stream.get());
    if (auto ex = TakeJniException(env); ex || g_in == nullptr) {
      op->MarkFailed(ex ? ex->message : "Failed to retain response InputStream",
                     ex ? ex->error_class : kHttpErrorOther);
      return false;
    }
    std::lock_guard<std::mutex> lock(cancel_handle->mutex);
    cancel_handle->in_stream_global = g_in;
  }
  if (op->IsCanceled()) {
    return false;
  }

  constexpr size_t kReadSlice = 64 * 1024;
  LocalRef<jbyteArray> jread_buf(
      env, env->NewByteArray(static_cast<jsize>(kReadSlice)));
  if (auto ex = TakeJniException(env); ex || !jread_buf) {
    op->MarkFailed(ex ? ex->message : "Failed to allocate JNI read buffer",
                   ex ? ex->error_class : kHttpErrorOther);
    return false;
  }
  std::vector<uint8_t> read_scratch(kReadSlice);

  while (!op->IsCanceled()) {
    jint n = env->CallIntMethod(in_stream.get(), c.input_read, jread_buf.get(),
                                0, static_cast<jsize>(kReadSlice));
    if (auto ex = TakeJniException(env)) {
      if (!op->IsCanceled()) {
        op->MarkFailed(ex->message, ex->error_class);
      }
      return false;
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
      op->MarkFailed(ex->message, ex->error_class);
      return false;
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
    return false;
  }

  // Close the response stream so Android's ConnectionPool can reuse the live
  // TLS connection; do not call conn.disconnect() on normal completion.
  env->CallVoidMethod(in_stream.get(), c.input_close);
  if (env->ExceptionCheck()) env->ExceptionClear();
  op->MarkResponseComplete();
  return true;
}

void ExecuteAndroidOperation(JNIEnv* env,
                             const std::shared_ptr<HttpOperation>& op) {
  if (!EnsureJniInitialized(env)) {
    op->MarkFailed("Failed to initialize Android JNI HTTP bindings.",
                   kHttpErrorOther);
    return;
  }
  if (!IsHttpOrHttpsUrl(op->url)) {
    op->MarkFailed("Unsupported URI scheme; only http and https are allowed.",
                   kHttpErrorOther);
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

  AndroidHopState hop;
  hop.current_url = op->url;
  hop.active_method = op->method;
  hop.active_headers = op->request_headers;
  hop.active_content_len = op->content_length;

  for (int redirect_count = 0; redirect_count <= kMaxRedirects;
       ++redirect_count) {
    cancel_handle->ClearAll(env);

    LocalRef<jobject> conn =
        BuildAndroidRequestConnection(env, op, cancel_handle, hop);
    if (!conn) {
      return;
    }

    std::optional<JniExceptionInfo> write_err;
    if (!SendAndroidRequestBody(env, op, cancel_handle, conn.get(), hop,
                                &write_err)) {
      return;
    }

    AndroidResponseHead head;
    if (!ReadAndroidResponseHeaders(env, op, conn.get(), write_err, &head)) {
      return;
    }

    std::string_view location =
        FindHeaderValueIgnoreCase(head.headers, "Location");
    if (IsRedirectStatus(head.status_code) && !location.empty()) {
      if (!ApplyAndroidRedirect(env, op, conn.get(), redirect_count,
                                head.status_code, location, &hop)) {
        return;
      }
      continue;
    }

    op->CommitResponseHeaders(head.status_code, head.reason,
                              std::move(head.headers));
    if (op->IsCanceled()) {
      return;
    }

    completed_cleanly =
        ReadAndroidResponseBody(env, op, cancel_handle, conn.get());
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

  void WorkerLoop() {
    JNIEnv* env = ResolveThreadsafeJNIEnv();

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
        env = ResolveThreadsafeJNIEnv();
      }
      if (env == nullptr) {
        next->MarkFailed("Android JavaVM / JNIEnv is unavailable.",
                         kHttpErrorOther);
      } else {
        ExecuteAndroidOperation(env, next);
      }
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
