// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

enum StorageReferenceFn {
  kStorageReferenceFnDelete = 0,
  kStorageReferenceFnGetBytes,
  kStorageReferenceFnGetFile,
  kStorageReferenceFnGetDownloadUrl,
  kStorageReferenceFnGetMetadata,
  kStorageReferenceFnUpdateMetadata,
  kStorageReferenceFnPutBytes,
  kStorageReferenceFnPutFile,
  kStorageReferenceFnList,
  kStorageReferenceFnListAll,
  kStorageReferenceFnCount,
};

struct JniListenerToken;

class StorageReferenceInternal {
 public:
  StorageReferenceInternal(StorageInternal* storage, jobject ref_obj);
  StorageReferenceInternal(const StorageReferenceInternal& ref);
  StorageReferenceInternal& operator=(const StorageReferenceInternal& ref);
  StorageReferenceInternal(StorageReferenceInternal&& ref) noexcept;
  StorageReferenceInternal& operator=(StorageReferenceInternal&& ref) noexcept;
  ~StorageReferenceInternal();

  ReferenceCountedFutureImpl* future() const {
    return const_cast<ReferenceCountedFutureImpl*>(&future_api_);
  }

  Storage* storage() const;
  StorageReferenceInternal* Child(const char* path) const;
  std::string bucket() const;
  std::string full_path() const;
  std::string name() const;
  StorageReferenceInternal* GetParent() const;

  void DeleteImpl(SafeFutureHandle<void> handle,
                  ReferenceCountedFutureImpl* api);
  void GetFileImpl(const char* path, Listener* listener,
                   Controller* controller_out, SafeFutureHandle<size_t> handle,
                   ReferenceCountedFutureImpl* api);
  void GetBytesImpl(void* buffer, size_t buffer_size, Listener* listener,
                    Controller* controller_out, SafeFutureHandle<size_t> handle,
                    ReferenceCountedFutureImpl* api);
  void GetDownloadUrlImpl(SafeFutureHandle<std::string> handle,
                          ReferenceCountedFutureImpl* api);
  void GetMetadataImpl(SafeFutureHandle<Metadata> handle,
                       ReferenceCountedFutureImpl* api);
  void UpdateMetadataImpl(const Metadata* metadata,
                          SafeFutureHandle<Metadata> handle,
                          ReferenceCountedFutureImpl* api);
  void PutBytesImpl(const void* buffer, size_t buffer_size, Listener* listener,
                    Controller* controller_out,
                    SafeFutureHandle<Metadata> handle,
                    ReferenceCountedFutureImpl* api);
  void PutBytesImpl(const void* buffer, size_t buffer_size,
                    const Metadata* metadata, Listener* listener,
                    Controller* controller_out,
                    SafeFutureHandle<Metadata> handle,
                    ReferenceCountedFutureImpl* api);
  void PutFileImpl(const char* path, Listener* listener,
                   Controller* controller_out,
                   SafeFutureHandle<Metadata> handle,
                   ReferenceCountedFutureImpl* api);
  void PutFileImpl(const char* path, const Metadata* metadata,
                   Listener* listener, Controller* controller_out,
                   SafeFutureHandle<Metadata> handle,
                   ReferenceCountedFutureImpl* api);
  void ListImpl(int max_results_per_page, const char* page_token,
                SafeFutureHandle<StorageListResult> handle,
                ReferenceCountedFutureImpl* api);
  void ListAllImpl(SafeFutureHandle<StorageListResult> handle,
                   ReferenceCountedFutureImpl* api);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static void CppByteDownloaderWriteBytes(JNIEnv* env, jclass clazz,
                                          jlong buffer_ptr, jlong buffer_size,
                                          jlong buffer_offset,
                                          jbyteArray byte_array,
                                          jlong num_bytes_to_copy);
  static jint CppByteUploaderReadBytes(JNIEnv* env, jclass clazz,
                                       jlong cpp_buffer_pointer,
                                       jlong cpp_buffer_size,
                                       jlong cpp_buffer_offset, jobject bytes,
                                       jint bytes_offset,
                                       jint num_bytes_to_read);

  StorageInternal* storage_internal() const { return storage_; }
  std::shared_ptr<JniListenerToken> AssignListenerToTask(Listener* listener,
                                                         jobject task);

 private:
  static void FutureCallback(JNIEnv* env, jobject result,
                             firebase::util::FutureResult result_code,
                             const char* status_message, void* callback_data);
  void BindController(Controller* controller_out, jobject task) const;
  std::string CallStringGetter(int method_id) const;

  StorageInternal* storage_ = nullptr;
  ReferenceCountedFutureImpl future_api_{kStorageReferenceFnCount};
  jobject obj_ = nullptr;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_ANDROID_STORAGE_REFERENCE_ANDROID_H_
