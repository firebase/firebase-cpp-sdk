// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_REST_INTERFACE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_REST_INTERFACE_H_

#include <string>

#include "app/rest/controller_interface.h"
#include "app/rest/response.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/logger.h"
#include "app/src/path.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/semaphore.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
namespace database {
namespace internal {

typedef int64_t WriteId;

class DatabaseInternal;

class SseResponse : public rest::Response {
 public:
  SseResponse() : semaphore_(0) {}

  virtual ~SseResponse() {}

  void MarkCompleted() override;
  void MarkCanceled() override;
  virtual void Finalize();

  void WaitForCompletion();

 private:
  Semaphore semaphore_;
};

class QueryResponse : public SseResponse {
 public:
  QueryResponse(DatabaseInternal* database, ChildListener* child_listener,
                const QuerySpec& query_spec)
      : SseResponse(),
        database_(database),
        value_listener_(nullptr),
        child_listener_(child_listener),
        query_spec_(query_spec),
        error_(kErrorNone),
        error_string_() {}

  QueryResponse(DatabaseInternal* database, ValueListener* value_listener,
                const QuerySpec& query_spec)
      : SseResponse(),
        database_(database),
        value_listener_(value_listener),
        child_listener_(nullptr),
        query_spec_(query_spec),
        error_(kErrorNone),
        error_string_() {}

  virtual ~QueryResponse();

  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;
  void MarkCanceled() override;
  void Finalize() override;

  ValueListener* value_listener() { return value_listener_; }
  ChildListener* child_listener() { return child_listener_; }
  Variant& data() { return data_; }
  const QuerySpec& query_spec() const { return query_spec_; }

  Mutex& mutex() { return mutex_; }

  void ClearListener();

 private:
  DatabaseInternal* database_;
  ValueListener* value_listener_;
  ChildListener* child_listener_;
  QuerySpec query_spec_;
  Variant data_;
  Error error_;
  std::string error_string_;
  // Used to guard listener_
  Mutex mutex_;
};

class SetValueResponse : public rest::Response {
 public:
  SetValueResponse(DatabaseInternal* database, const Path& path,
                   SafeFutureHandle<void> handle,
                   ReferenceCountedFutureImpl* ref_future, WriteId write_id);

  bool ProcessBody(const char* buffer, size_t length) override;

  void MarkCompleted() override;

  void MarkCanceled() override;

 private:
  DatabaseInternal* database_;
  Path path_;
  SafeFutureHandle<void> handle_;
  ReferenceCountedFutureImpl* ref_future_;
  WriteId write_id_;
  Error error_;
  std::string error_string_;
};

using RemoveValueResponse = SetValueResponse;
using SetPriorityResponse = SetValueResponse;
using SetValueAndPriorityResponse = SetValueResponse;
using UpdateChildrenResponse = SetValueResponse;

class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* database,
                      ReferenceCountedFutureImpl* future,
                      SafeFutureHandle<DataSnapshot> handle);
  // Unregister ourselves from the database.
  ~SingleValueListener() override;
  void OnValueChanged(const DataSnapshot& snapshot) override;
  void OnCancelled(const Error& error_code, const char* error_message) override;

 private:
  DatabaseInternal* database_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
};

class GetValueResponse : public rest::Response {
 public:
  GetValueResponse(DatabaseInternal* database, const Path& path,
                   SafeFutureHandle<DataSnapshot> handle,
                   ReferenceCountedFutureImpl* future,
                   SingleValueListener** single_value_listener_holder)
      : database_(database),
        path_(path),
        handle_(handle),
        future_(future),
        single_value_listener_holder_(single_value_listener_holder) {}

  void MarkCompleted() override;

 private:
  DatabaseInternal* database_;
  Path path_;
  SafeFutureHandle<DataSnapshot> handle_;
  ReferenceCountedFutureImpl* future_;
  SingleValueListener** single_value_listener_holder_;
};

enum ParseStatus {
  // The incoming data was successfully parsed, we should pass the results to
  // the listener.
  kParseSuccess,
  // This was just a keep-alive message. We shouldn't do anything with the data,
  // but we shouldn't report an error either.
  kParseKeepAlive,
  // There was an error parsing the data. This listener might have been
  // canceled, or the authorization was revoked, or the data received was
  // malformed.
  kParseError,
};

ParseStatus ParseResponse(const std::string& body, Path* out_relative_path,
                          Variant* out_diff, bool* is_overwrite,
                          Error* out_error, std::string* out_error_string);

void RestCall(DatabaseInternal* database, const std::string& url,
              const char* method, const std::string& post_fields,
              rest::Response* response);

void SseRestCall(DatabaseInternal* database, const QuerySpec& query_spec,
                 const char* url, SseResponse* response,
                 flatbuffers::unique_ptr<rest::Controller>* controller_out);

struct QueryUrlOptions {
  enum UrlType { kValueUrl, kPriorityUrl, kValuePriorityUrl };
  enum UseAuthToken { kNoToken, kIncludeAuthToken };

  QueryUrlOptions(UrlType _url_type, UseAuthToken _use_auth_token,
                  const QuerySpec* _query_spec)
      : url_type(_url_type),
        use_auth_token(_use_auth_token),
        query_spec(_query_spec) {}

  // Use the priority url instead of the standard one. This is used when setting
  // or getting the priority value at a location.
  UrlType url_type;

  // Whether the auth token should be appended to the URL.
  UseAuthToken use_auth_token;

  // Add query args (such as orderBy, limits, etc) if a QuerySpec is present.
  const QuerySpec* query_spec;
};

extern const QueryUrlOptions kJustUrlOptions;
extern const QueryUrlOptions kAuthorizedUrlOptions;
extern const QueryUrlOptions kAuthorizedPriorityUrlOptions;

// Returns the URL to this location in the database, according to the options
// supplied. The resulting URL will be the concatenation of the database URL
// given by the App object, followed by a slash, followed the full path to the
// value this query represents and either '.json' or '/.priority.json'
// depending on whether the options specified a priority location. Finally,
// the URL arguments are appened, including the Auth token (if present). This
// URL is valid for both Query operations as well as DatabaseReference
// operations.
//
// An example URL might look like this:
//
//     https://[PROJECT_ID].firebaseio.com/path/to/object.json?orderBy="height"&startAt=3
//
// See https://firebase.google.com/docs/reference/rest/database/ for more
// details.
std::string GetUrlWithQuery(const QueryUrlOptions& options,
                            DatabaseInternal* database,
                            const QuerySpec& query_spec);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_REST_INTERFACE_H_
