/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_CALLBACK_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_CALLBACK_H_

#include "app/meta/move.h"
#include "firebase/internal/common.h"

#ifdef FIREBASE_USE_STD_FUNCTION
#include <functional>
#endif

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

/// @cond FIREBASE_APP_INTERNAL

/// This file is intended for use with the C# interfaces, to allow registration
/// of callbacks so that they can be handled in the desired context, as opposed
/// to the threads created to handle them internally.

#include <string>

namespace FIREBASE_NAMESPACE {
namespace callback {

/// Initialize the Callback system.
void Initialize();

/// Determines whether the Callback system is initialized.
bool IsInitialized();

/// Destroys the Callback system.  If flush_all is set all callbacks are flushed
/// from the queue.
void Terminate(bool flush_all);

// TODO(73547295): Use CallbackArg to simplify the versions of Callback that
// accept strings.
/// A simple class to hold a value, or if the value's type is const char*, it
/// holds a std::string.
template <typename T>
class CallbackArg {
 public:
  explicit CallbackArg(const T value) : value_(value) {}
  const T value() const { return value_; }
  T value() { return value_; }

 private:
  T value_;
};

// The template specialization to hold a std::string instead of a const char*.
template <>
class CallbackArg<const char*> {
 public:
  explicit CallbackArg(const char* value) : value_(SafeString(value)) {}
  const char* value() { return value_.c_str(); }

 private:
  std::string value_;
  static const char* SafeString(const char* string) {
    return string ? string : "";
  }
};

/// Interface for Callbacks that will later be called from the correct context.
/// We don't use std::function because stlport has weak support for it, and we
/// need to guarantee that any string conversion happens on the C# thread.
class Callback {
 public:
  virtual ~Callback() {}
  /// Function to execute from the proper context.
  virtual void Run() = 0;

 protected:
  // Return the specified string or empty string if the specified string
  // is nullptr.
  static const char* SafeString(const char* string) {
    return string ? string : "";
  }
};

/// Callback implementation that takes a function with no arguments.
class CallbackVoid : public Callback {
 public:
  typedef void (*UserCallback)();

  CallbackVoid(UserCallback user_callback) : user_callback_(user_callback) {}
  ~CallbackVoid() override {}
  void Run() override { user_callback_(); }

 private:
  UserCallback user_callback_;
};

/// Callback implementation that takes a function and single argument of type T.
template <typename T>
class Callback1 : public Callback {
 public:
  typedef void (*UserCallback)(void* data);

  Callback1(const T& data, UserCallback user_callback)
      : data_(data), user_callback_(user_callback) {}
  ~Callback1() override {}
  void Run() override { user_callback_(&data_); }

 private:
  T data_;
  UserCallback user_callback_;
};

/// A special callback is needed for basic types that need the value returned,
/// instead of a pointer to the data.
template <typename T>
class CallbackValue1 : public Callback {
 public:
  typedef void (*UserCallback)(T data);

  CallbackValue1(const T data, UserCallback user_callback)
      : data_(data), user_callback_(user_callback) {}
  ~CallbackValue1() override {}
  void Run() override { user_callback_(data_); }

 private:
  T data_;
  UserCallback user_callback_;
};

/// A special callback is needed for two basic types that need the value
/// returned, instead of a pointer to the data.
template <typename T1, typename T2>
class CallbackValue2 : public Callback {
 public:
  typedef void (*UserCallback)(T1 data1, T2 data2);

  CallbackValue2(const T1 data1, const T2 data2, UserCallback user_callback)
      : data1_(data1), data2_(data2), user_callback_(user_callback) {}
  ~CallbackValue2() override {}
  void Run() override { user_callback_(data1_, data2_); }

 private:
  T1 data1_;
  T2 data2_;
  UserCallback user_callback_;
};

/// A special callback is used for strings, as they need to be converted.
class CallbackString : public Callback {
 public:
  typedef void (*UserCallback)(const char* data);

  CallbackString(const char* data, UserCallback user_callback)
      : data_(SafeString(data)), user_callback_(user_callback) {}
  ~CallbackString() override {}
  void Run() override { user_callback_(data_.c_str()); }

 private:
  std::string data_;
  UserCallback user_callback_;
};

/// Callback implementation that passes along a value type and a string.
template <typename T>
class CallbackValue1String1 : public Callback {
 public:
  typedef void (*UserCallback)(T data, const char* str);

  CallbackValue1String1(const T data, const char* str,
                        UserCallback user_callback)
      : data_(data), str_(SafeString(str)), user_callback_(user_callback) {}
  ~CallbackValue1String1() override {}
  void Run() override { user_callback_(data_, str_.c_str()); }

 private:
  T data_;
  std::string str_;
  UserCallback user_callback_;
};

/// Callback implementation that passes along two strings, and a value type.
template <typename T>
class CallbackString2Value1 : public Callback {
 public:
  typedef void (*UserCallback)(const char* str1, const char* str2, T data);

  CallbackString2Value1(const char* str1, const char* str2, const T data,
                        UserCallback user_callback)
      : str1_(SafeString(str1)),
        str2_(SafeString(str2)),
        data_(data),
        user_callback_(user_callback) {}
  ~CallbackString2Value1() override {}
  void Run() override { user_callback_(str1_.c_str(), str2_.c_str(), data_); }

 private:
  std::string str1_;
  std::string str2_;
  T data_;
  UserCallback user_callback_;
};

/// Callback implementation that passes along two value types and a string.
template <typename T1, typename T2>
class CallbackValue2String1 : public Callback {
 public:
  typedef void (*UserCallback)(T1 data1, T2 data2, const char* str);

  CallbackValue2String1(const T1 data1, const T2 data2, const char* str,
                        UserCallback user_callback)
      : data1_(data1),
        data2_(data2),
        str_(SafeString(str)),
        user_callback_(user_callback) {}
  ~CallbackValue2String1() override {}
  void Run() override { user_callback_(data1_, data2_, str_.c_str()); }

 private:
  T1 data1_;
  T2 data2_;
  std::string str_;
  UserCallback user_callback_;
};

/// Callback implementation that passes along three value types and a string.
template <typename T1, typename T2, typename T3>
class CallbackValue3String1 : public Callback {
 public:
  typedef void (*UserCallback)(T1 data1, T2 data2, T3 data3, const char* str);

  CallbackValue3String1(const T1 data1, const T2 data2, T3 data3,
                        const char* str, UserCallback user_callback)
      : data1_(data1),
        data2_(data2),
        data3_(data3),
        str_(SafeString(str)),
        user_callback_(user_callback) {}
  ~CallbackValue3String1() override {}
  void Run() override { user_callback_(data1_, data2_, data3_, str_.c_str()); }

 private:
  T1 data1_;
  T2 data2_;
  T3 data3_;
  std::string str_;
  UserCallback user_callback_;
};

/// Callback implementation that takes a function with one argument and claim
/// the ownership of the argument
template <typename T>
class CallbackMoveValue1 : public Callback {
 public:
  /// The callback method should not delete data.
  typedef void (*UserCallback)(T* data);

  CallbackMoveValue1(T data, UserCallback user_callback)
      : data_(Move(data)), user_callback_(user_callback) {}
  ~CallbackMoveValue1() override {}
  void Run() override { user_callback_(&data_); }

 private:
  T data_;
  UserCallback user_callback_;
};

#ifdef FIREBASE_USE_STD_FUNCTION
/// Callback implementation that wraps std::function
class CallbackStdFunction : public Callback {
 public:
  explicit CallbackStdFunction(const std::function<void()>& func)
      : func_(func) {}
  ~CallbackStdFunction() override {}
  void Run() override { func_(); }

 private:
  std::function<void()> func_;
};
#endif

// STLPort doens't support std::tuple or std::get, which are needed for
// CallbackVariadic to work.
#if !defined(_STLPORT_VERSION)
template <int...>
struct IntegerSequence {};

// This generates a sequence where the first value is a counter, and the rest of
// the sequence counts up to the target value.
//
// For example, if you start with GenerateSequence<5> (for a function with 5
// arguments) the inheritance evaluates to this:
//
// GenerateSequence<5> :
// GenerateSequence<4, 4> :
// GenerateSequence<3, 3, 4> :
// GenerateSequence<2, 2, 3, 4> :
// GenerateSequence<1, 1, 2, 3, 4> :
// GenerateSequence<0, 0, 1, 2, 3, 4>
//
// Finally, since GenerateSequence<0, 0, 1, 2, 3, 4> starts with a 0, the
// IntegerSequence that it holds in it's typedef Type contains <0, 1, 2, 3, 4>.
template <int counter, int... sequence>
struct GenerateSequence
    : GenerateSequence<counter - 1, counter - 1, sequence...> {};

template <int... sequence>
struct GenerateSequence<0, sequence...> {
  typedef IntegerSequence<sequence...> Type;
};

// A Variadic Callback that can be used on any number of arguments.
template <typename... ArgTypes>
class CallbackVariadic : public Callback {
 public:
  typedef void (*UserCallback)(ArgTypes... args);

  explicit CallbackVariadic(UserCallback user_callback, ArgTypes... args)
      : user_callback_(user_callback), args_(CallbackArg<ArgTypes>(args)...) {}
  ~CallbackVariadic() override {}
  void Run() override {
    // Get the number of args and generate an IntegerSequence from them.
    RunInternal(typename GenerateSequence<sizeof...(ArgTypes)>::Type());
  }

 private:
  template <int... Indexes>
  void RunInternal(IntegerSequence<Indexes...> unused) {
    // We only pass the unused variable so that we can unpack the Indexes stored
    // in the templated type.
    (void)unused;
    user_callback_(std::get<Indexes>(args_).value()...);
  }

  UserCallback user_callback_;
  std::tuple<CallbackArg<ArgTypes>...> args_;
};

template <typename UserCallback, typename... ArgTypes>
Callback* NewCallbackHelper(UserCallback user_callback, ArgTypes... args) {
  return new CallbackVariadic<ArgTypes...>(user_callback, args...);
}

// The first argument should be the Callback, the remaining arguments (if any)
// are the function arguments.
template <typename... CallbackAndArgs>
Callback* NewCallback(CallbackAndArgs... callback_and_args) {
  return NewCallbackHelper(callback_and_args...);
}
#endif  // !defined(_STLPORT_VERSION)

/// Adds a Callback to be called on the next PollCallbacks call.
/// This function returns a reference in the queue that can be used to remove
/// the callback from the queue.  The reference is only valid until the
/// callback is executed.
void* AddCallback(Callback* callback);

/// Adds a Callback to be called on the next PollCallbacks call.
/// This function returns a reference in the queue that can be used to remove
/// the callback from the queue.  The reference is only valid until the
/// callback is executed.
/// If AddCallbackWithThreadCheck is called from the callback thread,
/// the new callback is executed immediately to avoid deadlocks and return a
/// nullptr.
/// This is a temporary solution for C++->C# log callback.
void* AddCallbackWithThreadCheck(Callback* callback);

/// Adds a Callback to be called on the next PollCallbacks call.
/// This function blocks until the callback has been executed or removed from
/// the queue.  If AddBlockingCallback is called from the callback thread,
/// the new callback is executed immediately to avoid deadlocks.
/// NOTE: PollCallbacks() must have been previously called on the polling thread
/// before calling this method to avoid deadlock.
void AddBlockingCallback(Callback* callback);

/// Removes a Callback, using the reference returned by AddCallback(), from
/// the queue to be called from PollCallbacks().
void RemoveCallback(void* callback_reference);

/// Calls all pending callbacks added using AddCallback() since the last call.
/// Clears the list of pending callbacks.
/// NOTE: This must be always be called on the same thread.
void PollCallbacks();

}  // namespace callback
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
/// @endcond

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_CALLBACK_H_
