// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_

#include <cstddef>
#include <string>

#include "firebase/app.h"
#include "firebase/internal/common.h"
#include "firebase/log.h"
#include "firebase/storage/common.h"
#include "firebase/storage/controller.h"
#include "firebase/storage/list_result.h"
#include "firebase/storage/listener.h"
#include "firebase/storage/metadata.h"
#include "firebase/storage/storage_reference.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(storage)
#endif

namespace firebase {
namespace storage {

namespace internal {
class StorageInternal;
class MetadataInternal;
}  // namespace internal

class StorageReference;

class Storage {
 public:
  ~Storage();

  static Storage* GetInstance();
  static Storage* GetInstance(const std::string& url,
                              InitResult* init_result_out = nullptr);
  static Storage* GetInstance(::firebase::App* app,
                              InitResult* init_result_out = nullptr);
  static Storage* GetInstance(::firebase::App* app, const char* url,
                              InitResult* init_result_out = nullptr);
  static Storage* GetInstance(::firebase::App* app, const std::string& url,
                              InitResult* init_result_out = nullptr) {
    return GetInstance(app, url.c_str(), init_result_out);
  }

  ::firebase::App* app() const;
  std::string url() const;

  StorageReference GetReference() const;
  StorageReference GetReference(const char* path) const;
  StorageReference GetReference(const std::string& path) const {
    return GetReference(path.c_str());
  }
  StorageReference GetReferenceFromUrl(const char* url) const;
  StorageReference GetReferenceFromUrl(const std::string& url) const {
    return GetReferenceFromUrl(url.c_str());
  }

  double max_download_retry_time() const;
  void set_max_download_retry_time(double max_transfer_retry_seconds);
  double max_upload_retry_time() const;
  void set_max_upload_retry_time(double max_transfer_retry_seconds);
  double max_operation_retry_time() const;
  void set_max_operation_retry_time(double max_operation_retry_seconds);

  void UseEmulator(const std::string& host, int port) {
    UseEmulator(host.c_str(), port);
  }
  void UseEmulator(const char* host, int port);

  void set_log_level(LogLevel log_level);
  LogLevel log_level() const;

#if defined(FIREBASE_STORAGE_BUILDING_SDK) && !defined(SWIG) && \
    !defined(DOXYGEN)
  ::firebase::App* app();
  std::string url();
  double max_download_retry_time();
  double max_upload_retry_time();
  double max_operation_retry_time();
#endif

 private:
  friend class Metadata;
  friend class internal::MetadataInternal;

  Storage(::firebase::App* app, const char* url);
  Storage(const Storage& src) = delete;
  Storage& operator=(const Storage& src) = delete;
  void DeleteInternal();

  internal::StorageInternal* internal_ = nullptr;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_H_
