#include "firestore/src/jni/ownership.h"

#include <jni.h>

#include "app/memory/unique_ptr.h"
#include "firestore/src/jni/jni.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/traits.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

using testing::Contains;
using testing::Mock;
using testing::Not;
using testing::Return;
using testing::UnorderedElementsAreArray;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "MemberFunctionCanBeStatic"

/**
 * Tracks live local and global references created through the use of JNIEnv
 * by patching the function table in the JNIEnv object. When RefTracker goes
 * out of scope, it automatically unpatches the JNIEnv, to avoid affecting any
 * tests that follow.
 */
class RefTracker {
 public:
  RefTracker() {
    // Disallow `nullptr` from ever being a valid reference. This prevents bugs
    // in the reference wrappers arising from accidentally asserting that
    // nullptr is a valid live reference.
    invalid_refs_.insert(nullptr);

    instance_ = this;
    MaybePatchFunctions();
  }

  ~RefTracker() {
    MaybeUnpatchFunctions();
    instance_ = nullptr;
  }

  /**
   * Creates a new local reference to an arbitrary Java object.
   */
  jobject NewLocalObject() {
    JNIEnv* env = GetEnv();
    jobject object = env->NewStringUTF("fake");
    EXPECT_NE(object, nullptr);
    return object;
  }

  /**
   * Creates a new global reference to an arbitrary Java object.
   */
  jobject NewGlobalObject() {
    JNIEnv* env = GetEnv();
    jobject local = env->NewStringUTF("fake");
    jobject global = env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    EXPECT_NE(global, nullptr);
    return global;
  }

  /**
   * Makes an assertion that the given objects constitute the set of live JNI
   * object references. The objects can be any type convertible to a JNI object
   * reference type with `ToJni`.
   */
  template <typename... Objects>
  void ExpectLiveIsExactly(Objects&&... objects) {
    std::set<jobject> refs = {ToJni(std::forward<Objects>(objects))...};
    EXPECT_THAT(refs, testing::ContainerEq(valid_refs_));
    for (jobject ref : refs) {
      EXPECT_THAT(invalid_refs_, Not(Contains(ref)));
    }
  }

  /**
   * Makes an assertion that all the given objects have a null value. These
   * objects can be any type convertible to a JNI object reference with `ToJni`.
   *
   * This is largely only useful for verifying the default or moved-from states
   * of reference wrapper types.
   */
  template <typename... Objects>
  void ExpectNull(Objects&&... objects) {
    std::vector<jobject> refs = {ToJni(std::forward<Objects>(objects))...};
    for (jobject ref : refs) {
      EXPECT_EQ(ref, nullptr);
    }
  }

 private:
  /**
   * Patches the function table of the current thread's JNIEnv instance, saving
   * aside the current function table in `old_functions_`.
   */
  void MaybePatchFunctions() {
    JNIEnv* env = GetEnv();
    if (env->functions->NewStringUTF == &PatchedNewStringUTF) return;

    patched_ = true;
    old_functions_ = env->functions;
    functions_ = *env->functions;

    // Patch functions
    functions_.NewGlobalRef = &PatchedNewGlobalRef;
    functions_.NewLocalRef = &PatchedNewLocalRef;
    functions_.NewStringUTF = &PatchedNewStringUTF;
    functions_.DeleteGlobalRef = &PatchedDeleteGlobalRef;
    functions_.DeleteLocalRef = &PatchedDeleteLocalRef;
    env->functions = &functions_;
  }

  /**
   * Restores the current thread's JNIEnv instance to its former state, only if
   * it was patched by `MaybePatchFunctions`.
   */
  void MaybeUnpatchFunctions() {
    if (!patched_) return;

    JNIEnv* env = GetEnv();
    if (env->functions->NewStringUTF != &PatchedNewStringUTF) return;

    env->functions = old_functions_;
  }

  static jobject MarkValid(jobject object) {
    if (object != nullptr) {
      instance_->valid_refs_.insert(object);
      instance_->invalid_refs_.erase(object);
    }
    return object;
  }

  static void MarkInvalid(jobject object) {
    instance_->valid_refs_.erase(object);
    instance_->invalid_refs_.insert(object);
  }

  static jobject PatchedNewGlobalRef(JNIEnv* env, jobject object) {
    jobject result = instance_->old_functions_->NewGlobalRef(env, object);
    return MarkValid(result);
  }

  static jobject PatchedNewLocalRef(JNIEnv* env, jobject object) {
    jobject result = instance_->old_functions_->NewLocalRef(env, object);
    return MarkValid(result);
  }

  static jstring PatchedNewStringUTF(JNIEnv* env, const char* chars) {
    jstring result = instance_->old_functions_->NewStringUTF(env, chars);
    EXPECT_NE(result, nullptr);
    MarkValid(result);
    return result;
  }

  static void PatchedDeleteGlobalRef(JNIEnv* env, jobject object) {
    MarkInvalid(object);
    instance_->old_functions_->DeleteGlobalRef(env, object);
  }

  static void PatchedDeleteLocalRef(JNIEnv* env, jobject object) {
    MarkInvalid(object);
    instance_->old_functions_->DeleteLocalRef(env, object);
  }

  std::set<jobject> valid_refs_;
  std::set<jobject> invalid_refs_;

  bool patched_ = false;
  JNINativeInterface functions_;
  const JNINativeInterface* old_functions_ = nullptr;

  static RefTracker* instance_;
};

#pragma clang diagnostic pop

RefTracker* RefTracker::instance_ = nullptr;

class OwnershipTest : public FirestoreIntegrationTest {
 public:
  OwnershipTest() : env_(GetEnv()) {}

  ~OwnershipTest() override { refs_.ExpectLiveIsExactly(); }

 protected:
  JNIEnv* env_ = nullptr;
  RefTracker refs_;
};

// Local(JNIEnv*, jobject) adopts a local reference returned by JNI so it
// should not call NewLocalRef
TEST_F(OwnershipTest, LocalDeletes) {
  jobject local_java = refs_.NewLocalObject();
  {
    Local<Object> local(env_, local_java);
    refs_.ExpectLiveIsExactly(local);
  }
  refs_.ExpectLiveIsExactly();
}

TEST_F(OwnershipTest, LocalReleaseDoesNotDelete) {
  jobject local_java = refs_.NewLocalObject();
  {
    Local<Object> local(env_, local_java);
    refs_.ExpectLiveIsExactly(local);
    EXPECT_EQ(local_java, local.release());
  }
  refs_.ExpectLiveIsExactly(local_java);
  env_->DeleteLocalRef(local_java);
}

TEST_F(OwnershipTest, LocalAcceptsNullptr) {
  Local<Object> local(env_, nullptr);
  EXPECT_EQ(local.get(), nullptr);
}

TEST_F(OwnershipTest, GlobalCopyFromLocal) {
  Local<Object> local(env_, refs_.NewLocalObject());
  {
    Global<Object> global(local);
    refs_.ExpectLiveIsExactly(local, global);
  }
  refs_.ExpectLiveIsExactly(local);
}

TEST_F(OwnershipTest, GlobalCopyFromDefaultConstructedLocal) {
  Local<Object> local;
  Global<Object> global(local);
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, GlobalCopyAssignFromLocal) {
  Local<Object> local(env_, refs_.NewLocalObject());
  {
    Global<Object> global;
    global = local;
    refs_.ExpectLiveIsExactly(local, global);
  }
  refs_.ExpectLiveIsExactly(local);
}

TEST_F(OwnershipTest, GlobalCopyAssignFromDefaultConstructedLocal) {
  Local<Object> local;
  Global<Object> global;
  global = local;
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, GlobalMoveFromLocal) {
  Local<Object> local(env_, refs_.NewLocalObject());
  {
    Global<Object> global(Move(local));
    refs_.ExpectLiveIsExactly(global);
  }
}

TEST_F(OwnershipTest, GlobalMoveFromDefaultConstructedLocal) {
  Local<Object> local;
  Global<Object> global(Move(local));
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, GlobalMoveAssignFromLocal) {
  Local<Object> local(env_, refs_.NewLocalObject());
  {
    Global<Object> global;
    global = Move(local);
    refs_.ExpectLiveIsExactly(global);
  }
}

TEST_F(OwnershipTest, GlobalMoveAssignFromDefaultConstructedLocal) {
  Local<Object> local;
  Global<Object> global;
  global = Move(local);
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, GlobalImplicitMoveAssignFromLocal) {
  {
    Global<Object> global = Local<Object>(env_, refs_.NewLocalObject());
    refs_.ExpectLiveIsExactly(global);
  }
  refs_.ExpectLiveIsExactly();
}

TEST_F(OwnershipTest, LocalCopyFromGlobal) {
  Global<Object> global(refs_.NewGlobalObject(), AdoptExisting::kYes);
  {
    Local<Object> local(global);
    refs_.ExpectLiveIsExactly(local, global);
  }
  refs_.ExpectLiveIsExactly(global);
}

TEST_F(OwnershipTest, LocalCopyFromDefaultConstructedGlobal) {
  Global<Object> global;
  Local<Object> local(global);
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, LocalCopyAssignFromGlobal) {
  Global<Object> global(refs_.NewGlobalObject(), AdoptExisting::kYes);
  {
    Local<Object> local;
    local = global;
    refs_.ExpectLiveIsExactly(local, global);
  }
  refs_.ExpectLiveIsExactly(global);
}

TEST_F(OwnershipTest, LocalCopyAssignFromDefaultConstructedGlobal) {
  Global<Object> global;
  Local<Object> local;
  local = global;
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, LocalMoveGlobal) {
  Global<Object> global(refs_.NewGlobalObject(), AdoptExisting::kYes);
  {
    Local<Object> local(Move(global));
    refs_.ExpectLiveIsExactly(local);
  }
}

TEST_F(OwnershipTest, LocalMoveFromDefaultConstructedGlobal) {
  Global<Object> global;
  Local<Object> local(Move(global));
  refs_.ExpectNull(local, global);
}

TEST_F(OwnershipTest, LocalMoveAssignFromGlobal) {
  Global<Object> global(refs_.NewGlobalObject(), AdoptExisting::kYes);
  {
    Local<Object> local;
    local = Move(global);
    refs_.ExpectLiveIsExactly(local);
  }
}

TEST_F(OwnershipTest, LocalMoveAssignFromDefaultConstructedGlobal) {
  Global<Object> global;
  Local<Object> local;
  local = Move(global);
  refs_.ExpectNull(local, global);
}

}  // namespace
}  // namespace jni
}  // namespace firestore
}  // namespace firebase
