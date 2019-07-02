#include <jni.h>

#include "base/logging.h"
#include "testing/config.h"
#include "testing/run_all_tests.h"
#include "testing/testdata_config_generated.h"
#include "testing/util_android.h"

namespace firebase {
namespace testing {
namespace cppsdk {
namespace internal {
void ConfigSetImpl(const uint8_t* test_data_binary,
                   flatbuffers::uoffset_t size) {
  JNIEnv* android_jni_env = GetTestJniEnv();
  jbyteArray j_test_data = nullptr;
  if (test_data_binary != nullptr && size > 0) {
    j_test_data = android_jni_env->NewByteArray(size);
    android_jni_env->SetByteArrayRegion(
        j_test_data, 0, size,
        reinterpret_cast<jbyte*>(const_cast<uint8_t*>(test_data_binary)));
  }
  jclass cls = android_jni_env->FindClass(
      "com/google/testing/ConfigAndroid");
  android_jni_env->CallStaticVoidMethod(
      cls, android_jni_env->GetStaticMethodID(cls, "setImpl", "([B)V"),
      j_test_data);
  util::CheckAndClearException(android_jni_env);
  android_jni_env->DeleteLocalRef(j_test_data);
  android_jni_env->DeleteLocalRef(cls);
}
}  // namespace internal
}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
