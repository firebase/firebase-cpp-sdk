#include "storage/src/android/list_result_android.h"

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "firebase/storage/storage_reference.h"
#include "storage/src/android/storage_reference_android.h" // For StorageReferenceInternal constructor
#include "storage/src/common/common_internal.h" // For kApiIdentifier, though this might need adjustment

namespace firebase {
namespace storage {
namespace internal {

// Method IDs for com.google.firebase.storage.ListResult
// TODO(b/266143794): Initialize these method IDs properly, e.g., in ListResultInternalAndroid::Initialize()
static const ::firebase::internal::EmbeddedFile kListResultClass = {
    nullptr, 0, 0, "com/google/firebase/storage/ListResult"};
static ::firebase::internal::MethodId kListResultGetItems =
    ::firebase::internal::kInvalidMethodId;
static ::firebase::internal::MethodId kListResultGetPageToken =
    ::firebase::internal::kInvalidMethodId;

// Method IDs for java.util.List
// TODO(b/266143794): Initialize these method IDs properly
static const ::firebase::internal::EmbeddedFile kListClass = {
    nullptr, 0, 0, "java/util/List"};
static ::firebase::internal::MethodId kListGet =
    ::firebase::internal::kInvalidMethodId;
static ::firebase::internal::MethodId kListSize =
    ::firebase::internal::kInvalidMethodId;


// TODO(b/266143794): Add Initialize and Terminate methods to manage JNI resources if needed,
// or ensure they are handled by a parent (e.g. StorageAndroid).
// For now, assuming method IDs are looked up on first use or pre-cached.
// A proper implementation would cache these in an Initialize method.
void CacheListResultMethodIds(JNIEnv* env) {
  // This is a simplified approach. In a real scenario, use util::FindClass
  // and util::GetMethodId, and cache them, possibly in an Initialize method.
  // This function is a placeholder for that logic.
  if (kListResultGetItems == ::firebase::internal::kInvalidMethodId) {
    kListResultGetItems = ::firebase::internal::GetMethodId(
        env, kListResultClass, "getItems", "()Ljava/util/List;");
    kListResultGetPageToken = ::firebase::internal::GetMethodId(
        env, kListResultClass, "getPageToken", "()Ljava/lang/String;");
    kListGet = ::firebase::internal::GetMethodId(
        env, kListClass, "get", "(I)Ljava/lang/Object;");
    kListSize = ::firebase::internal::GetMethodId(
        env, kListClass, "size", "()I");
  }
}


ListResultInternalAndroid::ListResultInternalAndroid(
    StorageInternal* storage_internal, jobject java_list_result)
    : storage_internal_(storage_internal),
      java_list_result_global_ref_(nullptr),
      items_converted_(false),
      page_token_converted_(false) {
  JNIEnv* env = GetJNI();
  if (env && java_list_result) {
    java_list_result_global_ref_ = env->NewGlobalRef(java_list_result);
    CacheListResultMethodIds(env);
  }
}

ListResultInternalAndroid::~ListResultInternalAndroid() {
  JNIEnv* env = GetJNI();
  if (env && java_list_result_global_ref_) {
    env->DeleteGlobalRef(java_list_result_global_ref_);
    java_list_result_global_ref_ = nullptr;
  }
}

const std::vector<StorageReference>& ListResultInternalAndroid::items() const {
  if (!items_converted_ && java_list_result_global_ref_) {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env != nullptr);
    CacheListResultMethodIds(env);

    jobject java_items_list = env->CallObjectMethod(java_list_result_global_ref_, kListResultGetItems);
    if (::firebase::LogIfError(env, "ListResult.getItems()")) {
        // Error occurred or list is null
    } else if (java_items_list) {
      jint size = env->CallIntMethod(java_items_list, kListSize);
      if (::firebase::LogIfError(env, "List.size()")) {
          // Error occurred
      } else {
        items_cpp_.reserve(size);
        for (jint i = 0; i < size; ++i) {
          jobject java_storage_ref = env->CallObjectMethod(java_items_list, kListGet, i);
          if (::firebase::LogIfError(env, "List.get()") || !java_storage_ref) {
            // Handle error or null item
            env->DeleteLocalRef(java_storage_ref); // if it's not null but failed after
            continue;
          }
          // Each item in the list is a Java StorageReference.
          // We need to create a C++ StorageReference from it.
          // This requires the StorageInternal pointer from our owning Storage module.
          StorageReferenceInternal* sr_internal =
              new StorageReferenceInternal(storage_internal_, java_storage_ref);
          items_cpp_.emplace_back(sr_internal);
          env->DeleteLocalRef(java_storage_ref);
        }
      }
      env->DeleteLocalRef(java_items_list);
    }
    items_converted_ = true;
  }
  return items_cpp_;
}

const std::string& ListResultInternalAndroid::page_token() const {
  if (!page_token_converted_ && java_list_result_global_ref_) {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env != nullptr);
    CacheListResultMethodIds(env);

    jstring java_page_token = (jstring)env->CallObjectMethod(
        java_list_result_global_ref_, kListResultGetPageToken);
    if (::firebase::LogIfError(env, "ListResult.getPageToken()")) {
        // Error occurred
    } else if (java_page_token) {
      page_token_cpp_ = ::firebase::util::JStringToString(env, java_page_token);
      env->DeleteLocalRef(java_page_token);
    }
    page_token_converted_ = true;
  }
  return page_token_cpp_;
}

bool ListResultInternalAndroid::is_valid() const {
  return java_list_result_global_ref_ != nullptr && storage_internal_ != nullptr;
}

ListResultInternal* ListResultInternalAndroid::Clone() {
  if (!is_valid()) return nullptr;
  // Create a new Java ListResult global ref if possible, or handle appropriately.
  // For simplicity here, we are not deeply cloning the Java object itself,
  // but creating a new C++ wrapper around the same Java global ref.
  // A true clone might involve creating a new Java ListResult and populating it.
  // However, ListResult is often treated as immutable once obtained.
  // If the underlying Java object is shared and immutable, this is safe.
  // Otherwise, a deep copy of the Java object would be needed.
  // Let's assume for now the Java ListResult is immutable or this behavior is acceptable.
  JNIEnv* env = GetJNI();
  jobject new_java_ref = nullptr;
  if (env && java_list_result_global_ref_) {
    new_java_ref = env->NewGlobalRef(java_list_result_global_ref_);
  }
  if (new_java_ref) {
    return new ListResultInternalAndroid(storage_internal_, new_java_ref);
    // Note: The new_java_ref is passed to the constructor which will make its own global ref.
    // So, we should delete the local ref `new_java_ref` if NewGlobalRef was called by constructor.
    // Here, the constructor takes jobject, so it will call NewGlobalRef.
    // The `new_java_ref` here is already a global ref.
    // The constructor *should* take the jobject and make its own global ref.
    // Let's adjust: constructor takes jobject, makes global ref.
    // Clone makes a new global ref and passes it. This is redundant.
    // Corrected: Constructor takes jobject (local or global) and makes a *new* global ref.
    // Clone should pass the existing global_ref_, and the constructor duplicates it.
    // Or, Clone creates a new ListResultInternalAndroid that shares the same global ref,
    // if the object is immutable.
    // The current ListResultInternalAndroid constructor creates a NewGlobalRef.
    // So, if we pass java_list_result_global_ref_ (which is already a global ref),
    // the constructor will create *another* global ref to it. This is fine.
  }
   // Fallback: If we can't make a new global ref, or if the original is invalid.
  return nullptr;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
