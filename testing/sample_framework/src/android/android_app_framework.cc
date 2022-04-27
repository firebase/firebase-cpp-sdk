// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <android/log.h>
#include <android_native_app_glue.h>
#include <pthread.h>
#include <unistd.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "app_framework.h"  // NOLINT

// This implementation is derived from http://github.com/google/fplutil

// Number of seconds to delay after the app is finished before exiting.
// (Plus a longer delay if the app returns an error code other than 0, to give
// the user more time to see the errors.)
const int kExitDelaySeconds = 10;
const int kExitDelaySecondsIfError = 60;

static struct android_app* g_app_state = nullptr;
static bool g_destroy_requested = false;
static bool g_started = false;
static bool g_restarted = false;
static pthread_mutex_t g_started_mutex;

// Handle state changes from via native app glue.
static void OnAppCmd(struct android_app* app, int32_t cmd) {
  g_destroy_requested |= cmd == APP_CMD_DESTROY;
}

namespace app_framework {

// Process events pending on the main thread.
// Returns true when the app receives an event requesting exit.
bool ProcessEvents(int msec) {
  int looperId;
  do {
    struct android_poll_source* source = nullptr;
    int events;
    looperId = ALooper_pollAll(msec, nullptr, &events,
                               reinterpret_cast<void**>(&source));
    if (looperId >= 0 && source) {
      source->process(g_app_state, source);
    }
  } while (looperId != ALOOPER_POLL_TIMEOUT && !g_destroy_requested &&
           !g_restarted);
  return g_destroy_requested | g_restarted;
}

std::string PathForResource() {
  ANativeActivity* nativeActivity = g_app_state->activity;
  std::string result(nativeActivity->internalDataPath);
  return result + "/";
}

// Get the activity.
jobject GetActivity() { return g_app_state->activity->clazz; }

// Get the window context. For Android, it's a jobject pointing to the Activity.
jobject GetWindowContext() { return g_app_state->activity->clazz; }

// Get the window controller. For Android, this is the same as the
// WindowContext.
jobject GetWindowController() { return GetWindowContext(); }

// Find a class, attempting to load the class if it's not found.
jclass FindClass(JNIEnv* env, jobject activity_object, const char* class_name) {
  jclass class_object = env->FindClass(class_name);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    // If the class isn't found it's possible NativeActivity is being used by
    // the application which means the class path is set to only load system
    // classes.  The following falls back to loading the class using the
    // Activity before retrieving a reference to it.
    jclass activity_class = env->FindClass("android/app/Activity");
    jmethodID activity_get_class_loader = env->GetMethodID(
        activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");

    jobject class_loader_object =
        env->CallObjectMethod(activity_object, activity_get_class_loader);

    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    jmethodID class_loader_load_class =
        env->GetMethodID(class_loader_class, "loadClass",
                         "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring class_name_object = env->NewStringUTF(class_name);

    class_object = static_cast<jclass>(env->CallObjectMethod(
        class_loader_object, class_loader_load_class, class_name_object));

    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      class_object = nullptr;
    }
    env->DeleteLocalRef(class_name_object);
    env->DeleteLocalRef(class_loader_object);
  }
  return class_object;
}

// Vars that we need available for appending text to the log window:
class LoggingUtilsData {
 public:
  LoggingUtilsData()
      : logging_utils_class_(nullptr),
        logging_utils_add_log_text_(0),
        logging_utils_init_log_window_(0),
        logging_utils_get_did_touch_(0) {}

  ~LoggingUtilsData() {
    JNIEnv* env = GetJniEnv();
    assert(env);
    if (logging_utils_class_) {
      env->DeleteGlobalRef(logging_utils_class_);
    }
  }

  void Init() {
    JNIEnv* env = GetJniEnv();
    assert(env);

    jclass logging_utils_class = FindClass(
        env, GetActivity(), "com/google/firebase/example/LoggingUtils");
    assert(logging_utils_class != 0);

    // Need to store as global references so it don't get moved during garbage
    // collection.
    logging_utils_class_ =
        static_cast<jclass>(env->NewGlobalRef(logging_utils_class));
    env->DeleteLocalRef(logging_utils_class);

    logging_utils_init_log_window_ = env->GetStaticMethodID(
        logging_utils_class_, "initLogWindow", "(Landroid/app/Activity;)V");
    logging_utils_add_log_text_ = env->GetStaticMethodID(
        logging_utils_class_, "addLogText", "(Ljava/lang/String;)V");
    logging_utils_get_did_touch_ =
        env->GetStaticMethodID(logging_utils_class_, "getDidTouch", "()Z");
    logging_utils_get_log_file_ = env->GetStaticMethodID(
        logging_utils_class_, "getLogFile", "()Ljava/lang/String;");
    logging_utils_start_log_file_ =
        env->GetStaticMethodID(logging_utils_class_, "startLogFile",
                               "(Landroid/app/Activity;Ljava/lang/String;)Z");
    logging_utils_should_run_uitests_ =
        env->GetStaticMethodID(logging_utils_class_, "shouldRunUITests", "()Z");
    logging_utils_should_run_nonuitests_ = env->GetStaticMethodID(
        logging_utils_class_, "shouldRunNonUITests", "()Z");

    env->CallStaticVoidMethod(logging_utils_class_,
                              logging_utils_init_log_window_, GetActivity());
  }

  void AppendText(const char* text) {
    if (logging_utils_class_ == 0) return;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    jstring text_string = env->NewStringUTF(text);
    env->CallStaticVoidMethod(logging_utils_class_, logging_utils_add_log_text_,
                              text_string);
    env->DeleteLocalRef(text_string);
  }

  bool DidTouch() {
    if (logging_utils_class_ == 0)
      return false;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    return env->CallStaticBooleanMethod(logging_utils_class_,
                                        logging_utils_get_did_touch_);
  }

  bool ShouldRunUITests() {
    if (logging_utils_class_ == 0)
      return false;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    return env->CallStaticBooleanMethod(logging_utils_class_,
                                        logging_utils_should_run_uitests_);
  }

  bool ShouldRunNonUITests() {
    if (logging_utils_class_ == 0)
      return false;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    return env->CallStaticBooleanMethod(logging_utils_class_,
                                        logging_utils_should_run_nonuitests_);
  }

  bool IsLoggingToFile() {
    if (logging_utils_class_ == 0)
      return false;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    jobject file_uri = env->CallStaticObjectMethod(logging_utils_class_,
                                                   logging_utils_get_log_file_);
    if (file_uri == nullptr) {
      return false;
    } else {
      env->DeleteLocalRef(file_uri);
      return true;
    }
  }

  bool StartLoggingToFile(const char* path) {
    if (logging_utils_class_ == 0)
      return false;  // haven't been initialized yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    jstring path_string = env->NewStringUTF(path);
    jboolean b = env->CallStaticBooleanMethod(logging_utils_class_,
                                              logging_utils_start_log_file_,
                                              GetActivity(), path_string);
    env->DeleteLocalRef(path_string);
    return b ? true : false;
  }

 private:
  jclass logging_utils_class_;
  jmethodID logging_utils_add_log_text_;
  jmethodID logging_utils_init_log_window_;
  jmethodID logging_utils_get_did_touch_;
  jmethodID logging_utils_get_log_file_;
  jmethodID logging_utils_start_log_file_;
  jmethodID logging_utils_should_run_uitests_;
  jmethodID logging_utils_should_run_nonuitests_;
};

LoggingUtilsData* g_logging_utils_data;

// Checks if a JNI exception has happened, and if so, logs it to the console.
void CheckJNIException() {
  JNIEnv* env = GetJniEnv();
  if (env->ExceptionCheck()) {
    // Get the exception text.
    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();

    // Convert the exception to a string.
    jclass object_class = env->FindClass("java/lang/Object");
    jmethodID toString =
        env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    jstring s = (jstring)env->CallObjectMethod(exception, toString);
    const char* exception_text = env->GetStringUTFChars(s, nullptr);

    // Log the exception text.
    __android_log_print(ANDROID_LOG_INFO, TESTAPP_NAME,
                        "-------------------JNI exception:");
    __android_log_print(ANDROID_LOG_INFO, TESTAPP_NAME, "%s", exception_text);
    __android_log_print(ANDROID_LOG_INFO, TESTAPP_NAME, "-------------------");

    // Also, assert fail.
    assert(false);

    // In the event we didn't assert fail, clean up.
    env->ReleaseStringUTFChars(s, exception_text);
    env->DeleteLocalRef(s);
    env->DeleteLocalRef(exception);
  }
}

// Log a message that can be viewed in "adb logcat".
void LogMessageV(bool suppress, const char* format, va_list list) {
  static const int kLineBufferSize = 1024;
  char buffer[kLineBufferSize + 2];

  int string_len = vsnprintf(buffer, kLineBufferSize, format, list);
  string_len = string_len < kLineBufferSize ? string_len : kLineBufferSize;
  // append a linebreak to the buffer:
  buffer[string_len] = '\n';
  buffer[string_len + 1] = '\0';

  if (GetPreserveFullLog()) {
    AddToFullLog(buffer);
  }
  if (!suppress) {
    fputs(buffer, stdout);
    fflush(stdout);
  }
}

void LogMessage(const char* format, ...) {
  va_list list;
  va_start(list, format);
  LogMessageV(false, format, list);
  va_end(list);
}

static bool g_save_full_log = false;
static std::vector<std::string> g_full_logs;  // NOLINT

void AddToFullLog(const char* str) { g_full_logs.push_back(std::string(str)); }

bool GetPreserveFullLog() { return g_save_full_log; }
void SetPreserveFullLog(bool b) { g_save_full_log = b; }

void ClearFullLog() { g_full_logs.clear(); }

void OutputFullLog() {
  for (int i = 0; i < g_full_logs.size(); ++i) {
    fputs(g_full_logs[i].c_str(), stdout);
  }
  fflush(stdout);
  ClearFullLog();
}

// Log a message that can be viewed in the console.
void AddToTextView(const char* str) {
  app_framework::g_logging_utils_data->AppendText(str);
  CheckJNIException();
}

// Get the JNI environment.
JNIEnv* GetJniEnv() {
  JavaVM* vm = g_app_state->activity->vm;
  JNIEnv* env;
  jint result = vm->AttachCurrentThread(&env, nullptr);
  return result == JNI_OK ? env : nullptr;
}

bool ShouldRunUITests() {
  return app_framework::g_logging_utils_data->ShouldRunUITests();
}

bool ShouldRunNonUITests() {
  return app_framework::g_logging_utils_data->ShouldRunNonUITests();
}

bool IsLoggingToFile() {
  return app_framework::g_logging_utils_data->IsLoggingToFile();
}

bool StartLoggingToFile(const char* path) {
  return app_framework::g_logging_utils_data->StartLoggingToFile(path);
}

// Remove all lines starting with these strings.
static const char* const filter_lines[] = {"referenceTable ", nullptr};

bool should_filter(const char* str) {
  for (int i = 0; filter_lines[i] != nullptr; ++i) {
    if (strncmp(str, filter_lines[i], strlen(filter_lines[i])) == 0)
      return true;
  }
  return false;
}

void* stdout_logger(void* filedes_ptr) {
  int fd = reinterpret_cast<int*>(filedes_ptr)[0];
  static std::string buffer;
  char bufchar;
  while (int n = read(fd, &bufchar, 1)) {
    if (bufchar == '\0') {
      break;
    } else if (bufchar == '\n') {
      if (!should_filter(buffer.c_str())) {
        __android_log_print(ANDROID_LOG_INFO, TESTAPP_NAME, "%s",
                            buffer.c_str());
        buffer = buffer + bufchar;  // Add the newline
        app_framework::AddToTextView(buffer.c_str());
      }
      buffer.clear();
    } else {
      buffer = buffer + bufchar;
    }
  }
  JavaVM* jvm;
  if (app_framework::GetJniEnv()->GetJavaVM(&jvm) == 0) {
    jvm->DetachCurrentThread();
  }
  return nullptr;
}

struct FunctionData {
  void* (*func)(void*);
  void* data;
};

static void* CallFunction(void* bg_void) {
  FunctionData* bg = reinterpret_cast<FunctionData*>(bg_void);
  void* (*func)(void*) = bg->func;
  void* data = bg->data;
  // Clean up the data that was passed to us.
  delete bg;
  // Call the background function.
  void* result = func(data);
  // Then clean up the Java thread.
  JavaVM* jvm;
  if (app_framework::GetJniEnv()->GetJavaVM(&jvm) == 0) {
    jvm->DetachCurrentThread();
  }
  return result;
}

void RunOnBackgroundThread(void* (*func)(void*), void* data) {
  pthread_t thread;
  // Rather than running pthread_create(func, data), we must package them into a
  // struct, because the c++ thread needs to clean up the JNI thread after it
  // finishes.
  FunctionData* bg = new FunctionData;
  bg->func = func;
  bg->data = data;
  pthread_create(&thread, nullptr, CallFunction, bg);
  pthread_detach(thread);
}

// Vars that we need available for reading text from the user.
class TextEntryFieldData {
 public:
  TextEntryFieldData()
      : text_entry_field_class_(nullptr), text_entry_field_read_text_(0) {}

  ~TextEntryFieldData() {
    JNIEnv* env = GetJniEnv();
    assert(env);
    if (text_entry_field_class_) {
      env->DeleteGlobalRef(text_entry_field_class_);
    }
  }

  void Init() {
    JNIEnv* env = GetJniEnv();
    assert(env);

    jclass text_entry_field_class = FindClass(
        env, GetActivity(), "com/google/firebase/example/TextEntryField");
    if (text_entry_field_class == 0) {
      // No text entry allowed in this testapp, the Java class is not loaded.
      return;
    }

    // Need to store as global references so it don't get moved during garbage
    // collection.
    text_entry_field_class_ =
        static_cast<jclass>(env->NewGlobalRef(text_entry_field_class));
    env->DeleteLocalRef(text_entry_field_class);

    static const JNINativeMethod kNativeMethods[] = {
        {"nativeSleep", "(I)Z", reinterpret_cast<void*>(ProcessEvents)}};
    env->RegisterNatives(text_entry_field_class_, kNativeMethods,
                         sizeof(kNativeMethods) / sizeof(kNativeMethods[0]));
    text_entry_field_read_text_ = env->GetStaticMethodID(
        text_entry_field_class_, "readText",
        "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;)Ljava/lang/String;");
  }

  // Call TextEntryField.readText(), which shows a text entry dialog and spins
  // until the user enters some text (or cancels). If the user cancels, returns
  // an empty string.
  std::string ReadText(const char* title, const char* message,
                       const char* placeholder) {
    if (text_entry_field_class_ == 0) {
      LogMessage(
          "ERROR: ReadText() failed, no TextEntryField Java class is loaded.");
      return "";  // Haven't been initialized.
    }
    JNIEnv* env = GetJniEnv();
    assert(env);
    jstring title_string = env->NewStringUTF(title);
    jstring message_string = env->NewStringUTF(message);
    jstring placeholder_string = env->NewStringUTF(placeholder);
    jobject result_string = env->CallStaticObjectMethod(
        text_entry_field_class_, text_entry_field_read_text_, GetActivity(),
        title_string, message_string, placeholder_string);
    env->DeleteLocalRef(title_string);
    env->DeleteLocalRef(message_string);
    env->DeleteLocalRef(placeholder_string);
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    if (result_string == nullptr) {
      // Check if readText() returned null, which will be the case if an
      // exception occurred or if TextEntryField returned null for some reason.
      return "";
    }
    const char* result_buffer =
        env->GetStringUTFChars(static_cast<jstring>(result_string), 0);
    std::string result(result_buffer);
    env->ReleaseStringUTFChars(static_cast<jstring>(result_string),
                               result_buffer);
    return result;
  }

 private:
  jclass text_entry_field_class_;
  jmethodID text_entry_field_read_text_;
};

TextEntryFieldData* g_text_entry_field_data;

// Use a Java class, TextEntryField, to prompt the user to enter some text.
// This function blocks until text was entered or the dialog was canceled.
// If the user cancels, returns an empty string.
std::string ReadTextInput(const char* title, const char* message,
                          const char* placeholder) {
  assert(g_text_entry_field_data);
  return g_text_entry_field_data->ReadText(title, message, placeholder);
}

void SetEnvironmentVariableFromStringExtra(JNIEnv* env, const char* extra_name,
                                           jobject intent) {
  jclass intent_class = env->GetObjectClass(intent);
  jmethodID get_string_extra = env->GetMethodID(
      intent_class, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
  env->DeleteLocalRef(intent_class);

  jstring extra_name_jstring = env->NewStringUTF(extra_name);
  jstring extra_value_jstring = (jstring)env->CallObjectMethod(
      intent, get_string_extra, extra_name_jstring);
  env->DeleteLocalRef(extra_name_jstring);

  if (extra_value_jstring != nullptr) {
    const char* extra_value =
        env->GetStringUTFChars(extra_value_jstring, nullptr);
    setenv(extra_name, extra_value, /*overwrite=*/1);
    env->ReleaseStringUTFChars(extra_value_jstring, extra_value);
    env->DeleteLocalRef(extra_value_jstring);
  }
}

void SetExtrasAsEnvironmentVariables() {
  JNIEnv* env = app_framework::GetJniEnv();
  jobject activity = app_framework::GetActivity();

  jclass activity_class = env->GetObjectClass(activity);
  jmethodID get_intent = env->GetMethodID(activity_class, "getIntent",
                                          "()Landroid/content/Intent;");
  env->DeleteLocalRef(activity_class);

  jobject intent = env->CallObjectMethod(activity, get_intent);
  SetEnvironmentVariableFromStringExtra(env, "USE_FIRESTORE_EMULATOR", intent);
  SetEnvironmentVariableFromStringExtra(env, "FIRESTORE_EMULATOR_PORT", intent);

  env->DeleteLocalRef(intent);
}

}  // namespace app_framework

// Execute common_main(), flush pending events and finish the activity.
extern "C" void android_main(struct android_app* state) {
  // native_app_glue spawns a new thread, calling android_main() when the
  // activity onStart() or onRestart() methods are called.  This code handles
  // the case where we're re-entering this method on a different thread by
  // signalling the existing thread to exit, waiting for it to complete before
  // reinitializing the application.
  if (g_started) {
    g_restarted = true;
    // Wait for the existing thread to exit.
    pthread_mutex_lock(&g_started_mutex);
    pthread_mutex_unlock(&g_started_mutex);
  } else {
    g_started_mutex = PTHREAD_MUTEX_INITIALIZER;
  }
  pthread_mutex_lock(&g_started_mutex);
  g_started = true;

  // Save native app glue state and setup a callback to track the state.
  g_destroy_requested = false;
  g_app_state = state;
  g_app_state->onAppCmd = OnAppCmd;

  // Create the logging display.
  app_framework::g_logging_utils_data = new app_framework::LoggingUtilsData();
  app_framework::g_logging_utils_data->Init();

  // Create the text entry dialog.
  app_framework::g_text_entry_field_data =
      new app_framework::TextEntryFieldData();
  app_framework::g_text_entry_field_data->Init();

  // Pipe stdout to AddToTextView so we get the gtest output.
  int filedes[2];
  assert(pipe(filedes) != -1);
  assert(dup2(filedes[1], STDOUT_FILENO) != -1);
  pthread_t thread;
  pthread_create(&thread, nullptr, app_framework::stdout_logger,
                 reinterpret_cast<void*>(filedes));

  app_framework::SetExtrasAsEnvironmentVariables();

  // Execute cross platform entry point.
  // Copy the app name into a non-const array, as googletest requires that
  // main() take non-const char* argv[] so it can modify the arguments.
  char* argv[1];
  argv[0] = new char[strlen(TESTAPP_NAME) + 1];
  strcpy(argv[0], TESTAPP_NAME);  // NOLINT
  int return_value = common_main(1, argv);
  delete[] argv[0];
  argv[0] = nullptr;

  app_framework::ProcessEvents(10);

  // Signal to stdout_logger to exit.
  write(filedes[1], "\0", 1);
  pthread_join(thread, nullptr);
  close(filedes[0]);
  close(filedes[1]);
  // Pause a few seconds so you can see the results. If the user touches
  // the screen during that time, don't exit until they choose to.
  bool should_exit = false;
  int exit_delay_seconds =
      return_value ? kExitDelaySecondsIfError : kExitDelaySeconds;
  do {
    should_exit = app_framework::ProcessEvents(exit_delay_seconds * 1000);
  } while (app_framework::g_logging_utils_data->DidTouch() && !should_exit);

  // Clean up logging display.
  delete app_framework::g_logging_utils_data;
  app_framework::g_logging_utils_data = nullptr;

  // Finish the activity.
  if (!g_restarted) ANativeActivity_finish(state->activity);

  g_app_state->activity->vm->DetachCurrentThread();
  g_started = false;
  g_restarted = false;
  pthread_mutex_unlock(&g_started_mutex);
}
