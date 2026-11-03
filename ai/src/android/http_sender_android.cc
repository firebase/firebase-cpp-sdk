/*
 * Copyright 2025 Google LLC
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

#include <string>
#include <vector>

#include "ai/src/common/http_sender.h"
#include "app/src/thread.h"
#include "app/src/util_android.h"

namespace firebase {
namespace ai {
namespace internal {

namespace {

struct AndroidHttpTask {
  ::firebase::App* app;
  HttpRequest request;
  bool is_stream;
  HttpStreamChunkCallback on_chunk;
  HttpCompletionCallback on_complete;
};

std::string GetJniExceptionMessage(JNIEnv* env) {
  if (!env->ExceptionCheck()) return "";
  jthrowable exception = env->ExceptionOccurred();
  env->ExceptionClear();
  if (!exception) return "Unknown JNI exception";

  std::string message =
      ::firebase::util::GetMessageFromException(env, exception);
  env->DeleteLocalRef(exception);
  return message.empty() ? "Java exception in HttpURLConnection" : message;
}

void ExecuteAndroidHttpTask(AndroidHttpTask* task) {
  ::firebase::App* app = task->app;
  if (!app) {
    if (task->on_complete) {
      task->on_complete(0, "", "Firebase App is null on Android.");
    }
    delete task;
    return;
  }

  JNIEnv* env = app->GetJNIEnv();
  if (!env) {
    if (task->on_complete) {
      task->on_complete(0, "", "Failed to obtain JNIEnv on Android.");
    }
    delete task;
    return;
  }

  int status_code = 0;
  std::string response_body;
  std::string transport_error;

  jclass url_class = env->FindClass("java/net/URL");
  jclass conn_class = env->FindClass("java/net/HttpURLConnection");
  jclass output_stream_class = env->FindClass("java/io/OutputStream");
  jclass input_stream_class = env->FindClass("java/io/InputStream");

  jobject url_obj = nullptr;
  jobject conn_obj = nullptr;

  do {
    if (!url_class || !conn_class || !output_stream_class ||
        !input_stream_class) {
      transport_error = GetJniExceptionMessage(env);
      if (transport_error.empty()) {
        transport_error =
            "Failed to locate java.net.HttpURLConnection classes.";
      }
      break;
    }

    jmethodID url_ctor =
        env->GetMethodID(url_class, "<init>", "(Ljava/lang/String;)V");
    jmethodID open_conn = env->GetMethodID(url_class, "openConnection",
                                           "()Ljava/net/URLConnection;");

    jstring url_jstr = env->NewStringUTF(task->request.url.c_str());
    url_obj = env->NewObject(url_class, url_ctor, url_jstr);
    env->DeleteLocalRef(url_jstr);
    if (env->ExceptionCheck() || !url_obj) {
      transport_error = GetJniExceptionMessage(env);
      break;
    }

    conn_obj = env->CallObjectMethod(url_obj, open_conn);
    if (env->ExceptionCheck() || !conn_obj) {
      transport_error = GetJniExceptionMessage(env);
      break;
    }

    jmethodID set_method = env->GetMethodID(conn_class, "setRequestMethod",
                                            "(Ljava/lang/String;)V");
    jmethodID set_connect_timeout =
        env->GetMethodID(conn_class, "setConnectTimeout", "(I)V");
    jmethodID set_read_timeout =
        env->GetMethodID(conn_class, "setReadTimeout", "(I)V");
    jmethodID set_req_prop =
        env->GetMethodID(conn_class, "setRequestProperty",
                         "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID set_do_output =
        env->GetMethodID(conn_class, "setDoOutput", "(Z)V");
    jmethodID get_output_stream = env->GetMethodID(
        conn_class, "getOutputStream", "()Ljava/io/OutputStream;");
    jmethodID get_response_code =
        env->GetMethodID(conn_class, "getResponseCode", "()I");
    jmethodID get_input_stream = env->GetMethodID(conn_class, "getInputStream",
                                                  "()Ljava/io/InputStream;");
    jmethodID get_error_stream = env->GetMethodID(conn_class, "getErrorStream",
                                                  "()Ljava/io/InputStream;");

    jstring method_jstr = env->NewStringUTF(task->request.method.c_str());
    env->CallVoidMethod(conn_obj, set_method, method_jstr);
    env->DeleteLocalRef(method_jstr);
    if (env->ExceptionCheck()) {
      transport_error = GetJniExceptionMessage(env);
      break;
    }

    jint timeout_ms = static_cast<jint>(task->request.timeout_ms);
    env->CallVoidMethod(conn_obj, set_connect_timeout, timeout_ms);
    env->CallVoidMethod(conn_obj, set_read_timeout, timeout_ms);

    for (const auto& kv : task->request.headers) {
      jstring k_str = env->NewStringUTF(kv.first.c_str());
      jstring v_str = env->NewStringUTF(kv.second.c_str());
      env->CallVoidMethod(conn_obj, set_req_prop, k_str, v_str);
      env->DeleteLocalRef(k_str);
      env->DeleteLocalRef(v_str);
    }

    if (!task->request.body.empty()) {
      env->CallVoidMethod(conn_obj, set_do_output, JNI_TRUE);
      jobject out_stream = env->CallObjectMethod(conn_obj, get_output_stream);
      if (env->ExceptionCheck() || !out_stream) {
        transport_error = GetJniExceptionMessage(env);
        break;
      }

      jmethodID write_bytes =
          env->GetMethodID(output_stream_class, "write", "([B)V");
      jmethodID flush_stream =
          env->GetMethodID(output_stream_class, "flush", "()V");
      jmethodID close_out =
          env->GetMethodID(output_stream_class, "close", "()V");

      jsize body_len = static_cast<jsize>(task->request.body.size());
      jbyteArray body_array = env->NewByteArray(body_len);
      env->SetByteArrayRegion(
          body_array, 0, body_len,
          reinterpret_cast<const jbyte*>(task->request.body.data()));
      env->CallVoidMethod(out_stream, write_bytes, body_array);
      env->DeleteLocalRef(body_array);
      if (!env->ExceptionCheck()) {
        env->CallVoidMethod(out_stream, flush_stream);
      }
      if (!env->ExceptionCheck()) {
        env->CallVoidMethod(out_stream, close_out);
      }
      env->DeleteLocalRef(out_stream);
      if (env->ExceptionCheck()) {
        transport_error = GetJniExceptionMessage(env);
        break;
      }
    }

    status_code = env->CallIntMethod(conn_obj, get_response_code);
    if (env->ExceptionCheck()) {
      transport_error = GetJniExceptionMessage(env);
      break;
    }

    jobject in_stream = nullptr;
    if (status_code >= 200 && status_code < 300) {
      in_stream = env->CallObjectMethod(conn_obj, get_input_stream);
    } else {
      in_stream = env->CallObjectMethod(conn_obj, get_error_stream);
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }

    if (in_stream) {
      jmethodID read_bytes =
          env->GetMethodID(input_stream_class, "read", "([B)I");
      jmethodID close_in = env->GetMethodID(input_stream_class, "close", "()V");

      const jsize kBufferSize = 4096;
      jbyteArray buffer_array = env->NewByteArray(kBufferSize);
      std::vector<char> native_buf(kBufferSize);

      while (true) {
        jint bytes_read =
            env->CallIntMethod(in_stream, read_bytes, buffer_array);
        if (env->ExceptionCheck()) {
          transport_error = GetJniExceptionMessage(env);
          break;
        }
        if (bytes_read <= 0) {
          break;
        }
        env->GetByteArrayRegion(buffer_array, 0, bytes_read,
                                reinterpret_cast<jbyte*>(native_buf.data()));
        if (task->is_stream && status_code >= 200 && status_code < 300) {
          if (task->on_chunk &&
              !task->on_chunk(native_buf.data(),
                              static_cast<size_t>(bytes_read))) {
            break;
          }
        } else {
          response_body.append(native_buf.data(),
                               static_cast<size_t>(bytes_read));
        }
      }

      env->DeleteLocalRef(buffer_array);
      env->CallVoidMethod(in_stream, close_in);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      }
      env->DeleteLocalRef(in_stream);
    }
  } while (false);

  if (conn_obj) {
    jmethodID disconnect_method =
        env->GetMethodID(conn_class, "disconnect", "()V");
    if (disconnect_method) {
      env->CallVoidMethod(conn_obj, disconnect_method);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      }
    }
    env->DeleteLocalRef(conn_obj);
  }
  if (url_obj) env->DeleteLocalRef(url_obj);
  if (url_class) env->DeleteLocalRef(url_class);
  if (conn_class) env->DeleteLocalRef(conn_class);
  if (output_stream_class) env->DeleteLocalRef(output_stream_class);
  if (input_stream_class) env->DeleteLocalRef(input_stream_class);

  HttpCompletionCallback cb = task->on_complete;
  delete task;
  if (cb) {
    cb(status_code, response_body, transport_error);
  }
}

}  // namespace

void HttpSender::Initialize() {}

void HttpSender::Cleanup() {}

void HttpSender::SendUnary(::firebase::App* app, const HttpRequest& request,
                           const HttpCompletionCallback& on_complete) {
  AndroidHttpTask* task = new AndroidHttpTask{
      app, request, false, HttpStreamChunkCallback(), on_complete};
  Thread worker(ExecuteAndroidHttpTask, task);
  worker.Detach();
}

void HttpSender::SendStream(::firebase::App* app, const HttpRequest& request,
                            const HttpStreamChunkCallback& on_chunk,
                            const HttpCompletionCallback& on_complete) {
  AndroidHttpTask* task =
      new AndroidHttpTask{app, request, true, on_chunk, on_complete};
  Thread worker(ExecuteAndroidHttpTask, task);
  worker.Detach();
}

}  // namespace internal
}  // namespace ai
}  // namespace firebase
