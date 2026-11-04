// Copyright 2017 Google LLC

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "app/rest/request.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/metadata.h"
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

class BlockingResponse;
class ControllerBinding;
struct FutureApiOwner;
struct ListAllChainData;
struct ListenerSharedState;
struct MetadataChainData;
class Notifier;
class StorageInternal;

class StorageReferenceInternal {
 public:
  StorageReferenceInternal(const std::string& storage_uri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StoragePath& storage_uri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StorageReferenceInternal& other);
  StorageReferenceInternal& operator=(const StorageReferenceInternal& other);
  StorageReferenceInternal(StorageReferenceInternal&& other) noexcept;
  StorageReferenceInternal& operator=(
      StorageReferenceInternal&& other) noexcept;
  ~StorageReferenceInternal();

  template <class FutureType, class PrepareFunc>
  void RunAsyncOp(SafeFutureHandle<FutureType> final_handle,
                  double max_retry_time, Controller* controller_out,
                  PrepareFunc prepare_func);

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

  StorageInternal* storage_internal() const { return storage_; }
  StorageReference AsStorageReference() const;
  static StorageReference Adopt(
      std::unique_ptr<StorageReferenceInternal> internal);

  struct RequestAttempt {
    FutureBase future;
    std::shared_ptr<std::atomic<int>> http_status;
    bool local_failure = false;
  };
  using SendRequestFunct =
      std::function<RequestAttempt(Controller* attempt_controller_out)>;

  template <typename FutureType>
  void SendRequestWithRetry(SendRequestFunct send_request_funct,
                            SafeFutureHandle<FutureType> final_handle,
                            double max_retry_time_seconds,
                            Controller* controller_out = nullptr);

  template <typename FutureType>
  static void AsyncSendRequestWithRetry(
      StorageReference self_ref,
      std::shared_ptr<FutureApiOwner> future_api_owner,
      SendRequestFunct send_request_funct,
      SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
      bool retry_all_errors, std::shared_ptr<ControllerBinding> binding);

  static bool IsRetryableFailure(int http_status, bool retry_all_errors);

  void PutBytesInternalImpl(const void* buffer, size_t buffer_size,
                            Listener* listener, Controller* controller_out,
                            SafeFutureHandle<Metadata> handle,
                            ReferenceCountedFutureImpl* api,
                            const char* content_type = nullptr);
  void PutFileInternalImpl(const char* path, Listener* listener,
                           Controller* controller_out,
                           SafeFutureHandle<Metadata> handle,
                           ReferenceCountedFutureImpl* api,
                           const char* content_type = nullptr);
  void RestCall(rest::Request* request, Notifier* request_notifier,
                BlockingResponse* response, FutureHandle handle,
                Listener* listener,
                const std::shared_ptr<ListenerSharedState>& listener_state,
                Controller* controller_out);
  void PrepareRequestBlocking(rest::Request* request, const char* url,
                              const char* method,
                              const char* content_type = nullptr);
  void SetupMetadataChain(Future<Metadata> starting_future,
                          const std::shared_ptr<MetadataChainData>& data);
  static void ContinueListAll(Future<StorageListResult> page_future,
                              const std::shared_ptr<ListAllChainData>& data);
  ReferenceCountedFutureImpl* future() const;

  StorageInternal* storage_ = nullptr;
  StoragePath storage_uri_;
  std::shared_ptr<FutureApiOwner> future_api_owner_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
