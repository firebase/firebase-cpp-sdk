#ifndef FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_
#define FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

namespace firebase {
namespace testing {
namespace cppsdk {
namespace util {

// Check for JNI exceptions, print them to the log (if any were raised) and
// clear the exception state returning whether an exception was raised.
bool CheckAndClearException(JNIEnv *env);

// Converts a `java.util.List<String>` to a `std::vector<std::string>`.
std::vector<std::string> JavaStringListToStdStringVector(JNIEnv* env,
                                                         jobject list);

// Convert a `jstring` to a `std::string`.
std::string JavaStringToStdString(JNIEnv* env, jobject string_object);

}  // namespace util
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_TESTING_CPPSDK_UTIL_ANDROID_H_
