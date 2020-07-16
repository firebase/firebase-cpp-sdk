#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_

#include <jni.h>

#include <string>

#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/string.h"
#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * A wrapper around a JNIEnv pointer that makes dealing with JNI simpler in C++,
 * by:
 *
 *   * Automatically converting arguments of C++ types to their JNI equivalents;
 *   * handling C++ strings naturally;
 *   * wrapping JNI references in `Local` RAII wrappers automatically; and
 *   * simplifying error handling related to JNI calls (see below).
 *
 * Normally JNI requires that each call be followed by an explicit check to see
 * if an exception happened. This is tedious and clutters the code. Instead,
 * `Env` automatically checks for a JNI exception and short circuits any further
 * calls. This means that JNI-intensive code can be written straightforwardly
 * with a single, final check for errors. Exceptions can still be handled
 * inline if required.
 */
class Env {
 public:
  Env() : env_(GetEnv()) {}

  explicit Env(JNIEnv* env) : env_(env) {}

  /** Returns true if the Env has not encountered an exception. */
  bool ok() const { return last_exception_ == nullptr; }

  /** Returns the underlying JNIEnv pointer. */
  JNIEnv* get() const { return env_; }

  // MARK: String Operations

  /**
   * Creates a new proxy for a Java String from a sequences of modified UTF-8
   * bytes.
   */
  Local<String> NewStringUtf(const char* bytes);
  Local<String> NewStringUtf(const std::string& bytes) {
    return NewStringUtf(bytes.c_str());
  }

  /** Returns the length of the string in modified UTF-8 bytes. */
  size_t GetStringUtfLength(jstring string) {
    jsize result = env_->GetStringUTFLength(string);
    RecordException();
    return static_cast<size_t>(result);
  }
  size_t GetStringUtfLength(const String& string) {
    return GetStringUtfLength(string.get());
  }

  /**
   * Copies the contents of a region of a Java string to a C++ string. The
   * resulting string has a modified UTF-8 encoding.
   */
  std::string GetStringUtfRegion(jstring string, size_t start, size_t len);
  std::string GetStringUtfRegion(const String& string, size_t start,
                                 size_t len) {
    return GetStringUtfRegion(string.get(), start, len);
  }

 private:
  void RecordException();

  JNIEnv* env_ = nullptr;
  jthrowable last_exception_ = nullptr;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_JNI_ENV_H_
