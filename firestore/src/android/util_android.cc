#include "firestore/src/android/util_android.h"

#include "firestore/src/jni/env.h"

namespace firebase {
namespace firestore {

void GlobalUnhandledExceptionHandler(jni::Env& env,
                                     jni::Local<jni::Throwable>&& exception,
                                     void* context) {
#if __cpp_exceptions
  // TODO(b/149105903): Handle different underlying Java exceptions differently.
  env.ExceptionClear();
  throw FirestoreException(exception.GetMessage(env));

#else
  // Just clear the pending exception. The exception was already logged when
  // first caught.
  env.ExceptionClear();
#endif
}

}  // namespace firestore
}  // namespace firebase
