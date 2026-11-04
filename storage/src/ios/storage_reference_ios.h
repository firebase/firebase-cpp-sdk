// Copyright 2016 Google LLC

#ifndef FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_

#include <memory>
#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/storage_reference.h"
#include "storage/src/ios/storage_ios.h"

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"
typedef void (^FIRStorageVoidDataError)(NSData* _Nullable, NSError* _Nullable);
#endif  // __OBJC__

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

OBJ_C_PTR_WRAPPER(FIRStorageReference);
OBJ_C_PTR_WRAPPER(FIRStorageObservableTask);

class StorageReferenceInternal {
 public:
  explicit StorageReferenceInternal(
      StorageInternal* _Nonnull storage,
      std::unique_ptr<FIRStorageReferencePointer> impl);
  ~StorageReferenceInternal();

  StorageReferenceInternal(const StorageReferenceInternal& reference);
  StorageReferenceInternal& operator=(
      const StorageReferenceInternal& reference);
  StorageReferenceInternal(StorageReferenceInternal&& other) noexcept;
  StorageReferenceInternal& operator=(
      StorageReferenceInternal&& other) noexcept;

  Storage* _Nullable storage() const;
  StorageReferenceInternal* _Nullable Child(const char* _Nullable path) const;
  std::string bucket() const;
  std::string full_path() const;
  std::string name() const;
  StorageReferenceInternal* _Nullable GetParent() const;

  void DeleteImpl(SafeFutureHandle<void> handle,
                  ReferenceCountedFutureImpl* _Nonnull api);
  void GetFileImpl(const char* _Nullable path, Listener* _Nullable listener,
                   Controller* _Nullable controller_out,
                   SafeFutureHandle<size_t> handle,
                   ReferenceCountedFutureImpl* _Nonnull api);
  void GetBytesImpl(void* _Nullable buffer, size_t buffer_size,
                    Listener* _Nullable listener,
                    Controller* _Nullable controller_out,
                    SafeFutureHandle<size_t> handle,
                    ReferenceCountedFutureImpl* _Nonnull api);
  void GetDownloadUrlImpl(SafeFutureHandle<std::string> handle,
                          ReferenceCountedFutureImpl* _Nonnull api);
  void GetMetadataImpl(SafeFutureHandle<Metadata> handle,
                       ReferenceCountedFutureImpl* _Nonnull api);
  void UpdateMetadataImpl(const Metadata* _Nullable metadata,
                          SafeFutureHandle<Metadata> handle,
                          ReferenceCountedFutureImpl* _Nonnull api);
  void PutBytesImpl(const void* _Nullable buffer, size_t buffer_size,
                    Listener* _Nullable listener,
                    Controller* _Nullable controller_out,
                    SafeFutureHandle<Metadata> handle,
                    ReferenceCountedFutureImpl* _Nonnull api);
  void PutBytesImpl(const void* _Nullable buffer, size_t buffer_size,
                    const Metadata* _Nullable metadata,
                    Listener* _Nullable listener,
                    Controller* _Nullable controller_out,
                    SafeFutureHandle<Metadata> handle,
                    ReferenceCountedFutureImpl* _Nonnull api);
  void PutFileImpl(const char* _Nullable path, Listener* _Nullable listener,
                   Controller* _Nullable controller_out,
                   SafeFutureHandle<Metadata> handle,
                   ReferenceCountedFutureImpl* _Nonnull api);
  void PutFileImpl(const char* _Nullable path,
                   const Metadata* _Nullable metadata,
                   Listener* _Nullable listener,
                   Controller* _Nullable controller_out,
                   SafeFutureHandle<Metadata> handle,
                   ReferenceCountedFutureImpl* _Nonnull api);
  void ListImpl(int max_results_per_page, const char* _Nullable page_token,
                SafeFutureHandle<StorageListResult> handle,
                ReferenceCountedFutureImpl* _Nonnull api);
  void ListAllImpl(SafeFutureHandle<StorageListResult> handle,
                   ReferenceCountedFutureImpl* _Nonnull api);

  StorageInternal* _Nullable storage_internal() const { return storage_; }
  ReferenceCountedFutureImpl* _Nullable future() const { return &future_api_; }

#ifdef __OBJC__
  FIRStorageReference* _Nullable impl() const {
    return impl_ ? impl_->get() : nil;
  }
#endif  // __OBJC__

 private:
  StorageInternal* _Nullable storage_ = nullptr;
  mutable ReferenceCountedFutureImpl future_api_{kStorageReferenceFnCount};
  std::unique_ptr<FIRStorageReferencePointer> impl_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_IOS_STORAGE_REFERENCE_IOS_H_
