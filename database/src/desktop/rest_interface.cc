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

#include "database/src/desktop/rest_interface.h"

#include <string>

#include "app/rest/controller_interface.h"
#include "app/rest/request.h"
#include "app/rest/response.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace database {
namespace internal {

const QueryUrlOptions kJustUrlOptions(QueryUrlOptions::kValueUrl,
                                      QueryUrlOptions::kNoToken, nullptr);
const QueryUrlOptions kAuthorizedUrlOptions(QueryUrlOptions::kValueUrl,
                                            QueryUrlOptions::kIncludeAuthToken,
                                            nullptr);
const QueryUrlOptions kAuthorizedPriorityUrlOptions(
    QueryUrlOptions::kPriorityUrl, QueryUrlOptions::kIncludeAuthToken, nullptr);

static Error ParseErrorString(const char* error_string) {
  const char kErrorStringPermissionDenied[] = "Permission denied";

  if (strcmp(error_string, kErrorStringPermissionDenied) == 0) {
    return kErrorPermissionDenied;
  } else {
    // TODO(amablue): What other error string can we get here?
    return kErrorUnknownError;
  }
}

class DatabaseRequest : public rest::Request {
 public:
  DatabaseRequest(DatabaseInternal* database) {
    // NOTE: We're not sending the x-goog-api-client user agent header returned
    // by App::GetUserAgent() to avoid bloating each request.
    add_header("User-Agent", database->host_info().user_agent().c_str());
  }
};

void SseResponse::MarkCompleted() {
  rest::Response::MarkCompleted();
  Finalize();
}

void SseResponse::MarkCanceled() {
  rest::Response::MarkCanceled();
  Finalize();
}

void SseResponse::Finalize() { semaphore_.Post(); }

void SseResponse::WaitForCompletion() {
  // This is here purely to block until the mutex is released.
  semaphore_.Wait();
}

ParseStatus ParseResponse(const std::string& body, Path* out_relative_path,
                          Variant* out_diff, bool* is_overwrite,
                          Error* out_error, std::string* out_error_string) {
  // Responses take the following form (everything between the ---)
  //
  // ---
  // event: <Event Name>
  // data: {"path":<Path>,"data":<JSON Data>}
  // ---
  //
  // For more information, see:
  // https://firebase.google.com/docs/reference/rest/database/#section-streaming
  const std::string kEventPrefix = "event: ";
  const std::string kDataPrefix = "data: ";

  std::istringstream input(body);
  std::string line;

  // Get the method.
  std::getline(input, line);
  if (!StringStartsWith(line, kEventPrefix)) {
    // If we didn't get a segment starting with "event: " then this is an error
    // string.
    *out_error = ParseErrorString(line.c_str());
    *out_error_string = line;
    return kParseError;
  }
  std::string method(line, kEventPrefix.size());

  // put, patch, keep-alive, cancel and auth_revoked are the only values the
  // server will send down.
  // https://firebase.google.com/docs/reference/rest/database/#section-streaming
  if (method != "put" && method != "patch") {
    if (method == "keep-alive") {
      *out_error = kErrorNone;
      return kParseKeepAlive;
    } else if (method == "cancel") {
      *out_error = kErrorPermissionDenied;
      return kParseError;
    } else if (method == "auth_revoked") {
      *out_error = kErrorPermissionDenied;
      return kParseError;
    } else {
      logger_->LogError("Unexpected method (%s).", method.c_str());
      // If we've errored at this point something has gone wrong on the server
      // and it sent us bad data.
      *out_error = kErrorUnknownError;
      return kParseError;
    }
  }

  // Get the JSON string.
  std::getline(input, line);
  if (!StringStartsWith(line, kDataPrefix)) {
    logger_->LogError("Malformed data sent to client: Expected %s, got %s.",
                      kDataPrefix.c_str(), line.c_str());
    // If we've errored at this point something has gone wrong on the server and
    // it sent us bad data.
    *out_error = kErrorUnknownError;
    return kParseError;
  }
  std::string json(line.begin() + kDataPrefix.size(), line.end());

  // Now that we have the JSON string, convert it into a variant. This variant
  // should be a map that consists of two fields: "path", which is a string, and
  // "data", which is the data to be placed at the location given by "path".
  Variant json_data = util::JsonToVariant(json.c_str());
  if (!json_data.is_map()) {
    logger_->LogError("Malformed JSON sent to client: Expected object, got %s.",
                      Variant::TypeName(json_data.type()));
    // If we've errored at this point something has gone wrong on the server and
    // it sent us bad data.
    *out_error = kErrorUnknownError;
    return kParseError;
  }
  // Get the path from the variant.
  auto path_iter = json_data.map().find(Variant("path"));
  if (path_iter == json_data.map().end() || !path_iter->second.is_string()) {
    logger_->LogError(
        "Malformed JSON sent to client: Expected \"path\" field.");
    // If we've errored at this point something has gone wrong on the server and
    // it sent us bad data.
    *out_error = kErrorUnknownError;
    return kParseError;
  }
  Path path(path_iter->second.mutable_string());

  // Get the data from the variant.
  auto data_iter = json_data.map().find(Variant("data"));
  if (data_iter == json_data.map().end()) {
    logger_->LogError(
        "Malformed JSON sent to client: Expected \"data\" field.");
    // If we've errored at this point something has gone wrong on the server and
    // it sent us bad data.
    *out_error = kErrorUnknownError;
    return kParseError;
  }
  Variant& incoming_data = data_iter->second;

  *out_relative_path = path;
  *out_diff = incoming_data;
  *is_overwrite = (method == "put");
  *out_error = kErrorNone;
  return kParseSuccess;
}

QueryResponse::~QueryResponse() {
  database_->UnregisterQueryResponse(query_spec_, value_listener_,
                                     child_listener_);
}

bool QueryResponse::ProcessBody(const char* buffer, size_t length) {
  MutexLock lock(mutex_);
  if (value_listener_ == nullptr && child_listener_ == nullptr) {
    // Listener has been unregistered already.
    return false;
  }

  std::string body(buffer, length);
  Path relative_path;
  Variant diff;
  bool is_overwrite;
  switch (ParseResponse(body, &relative_path, &diff, &is_overwrite, &error_,
                        &error_string_)) {
    case kParseSuccess: {
      Path full_path = query_spec_.path.GetChild(relative_path);
      if (is_overwrite) {
        database_->ApplyServerOverwrite(full_path, diff);
      } else {
        database_->ApplyServerMerge(full_path, diff);
      }
      return true;
    }
    case kParseKeepAlive: {
      return true;
    }
    case kParseError: {
      // After an error has been detected the response MarkComplete will be
      // called automatically.
      return false;
    }
  }
}

void QueryResponse::ClearListener() {
  MutexLock lock(mutex_);
  value_listener_ = nullptr;
  child_listener_ = nullptr;
}

void QueryResponse::MarkCompleted() { SseResponse::MarkCompleted(); }

void QueryResponse::MarkCanceled() {
  error_ = kErrorWriteCanceled;
  SseResponse::MarkCanceled();
}

void QueryResponse::Finalize() {
  MutexLock lock(mutex_);
  const char* effective_error_string =
      error_string_.empty() ? GetErrorMessage(error_) : error_string_.c_str();
  // Only one will be nonnull.
  if (value_listener_)
    value_listener_->OnCancelled(error_, effective_error_string);
  if (child_listener_)
    child_listener_->OnCancelled(error_, effective_error_string);
  SseResponse::Finalize();
}

SetValueResponse::SetValueResponse(DatabaseInternal* database, const Path& path,
                                   SafeFutureHandle<void> handle,
                                   ReferenceCountedFutureImpl* ref_future,
                                   WriteId write_id)
    : database_(database),
      path_(path),
      handle_(handle),
      ref_future_(ref_future),
      write_id_(write_id),
      error_(kErrorNone),
      error_string_() {}

bool SetValueResponse::ProcessBody(const char* buffer, size_t length) {
  std::string body(buffer, length);
  // The response takes the form of a JSON string. If the response contains
  // the field "error" then something has gone wrong. Revert the change and
  // report the error.
  Variant json_data = util::JsonToVariant(body.c_str());
  if (json_data.is_map()) {
    auto iter = json_data.map().find("error");
    if (iter != json_data.map().end()) {
      const Variant& error_variant = iter->second;
      if (error_variant.is_string()) {
        error_string_ = error_variant.string_value();
        error_ = ParseErrorString(error_string_.c_str());
      } else {
        // If our error wasn't a string variant something very weird happened.
        error_ = kErrorUnknownError;
      }
    }
  }
  return rest::Response::ProcessBody(buffer, length);
}

void SetValueResponse::MarkCompleted() {
  rest::Response::MarkCompleted();
  switch (status()) {
    case rest::util::HttpSuccess: {  // 200
      // No error.
      break;
    }
    case rest::util::HttpBadRequest: {  // 400
      error_ = kErrorOperationFailed;
      break;
    }
    case rest::util::HttpUnauthorized: {  // 401
      error_ = kErrorPermissionDenied;
      break;
    }
    case 503: {  // Service unavailable.
      error_ = kErrorUnavailable;
      break;
    }
    default: {
      error_ = kErrorUnknownError;
      break;
    }
  }

  // If there was an error, revert the change locally.
  if (error_ != kErrorNone) {
    database_->RevertWriteId(path_, write_id_);
  }
  // Complete the future.
  const char* effective_error_string =
      error_string_.empty() ? GetErrorMessage(error_) : error_string_.c_str();
  ref_future_->Complete(handle_, error_, effective_error_string);
}

void SetValueResponse::MarkCanceled() {
  rest::Response::MarkCompleted();
  // If the operation was canceled, revert the change and complete the future.
  database_->RevertWriteId(path_, write_id_);
  ref_future_->Complete(handle_, kErrorWriteCanceled);
}

SingleValueListener::SingleValueListener(DatabaseInternal* database,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : database_(database), future_(future), handle_(handle) {}

SingleValueListener::~SingleValueListener() {}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  database_->RemoveSingleValueListener(this);
  future_->CompleteWithResult<DataSnapshot>(handle_, kErrorNone, "", snapshot);
  delete this;
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  database_->RemoveSingleValueListener(this);
  future_->Complete(handle_, error_code, error_message);
  delete this;
}

void GetValueResponse::MarkCompleted() {
  rest::Response::MarkCompleted();
  const char* body = GetBody();
  if (!body) {
    // No body received - return error.
    future_->CompleteWithResult<DataSnapshot>(
        handle_, kErrorUnknownError,
        DataSnapshotInternal::GetInvalidDataSnapshot());
  }
  if (*single_value_listener_holder_) {
    // TODO(b/68864049): Check status() for error code.
    DataSnapshot snapshot(
        new DataSnapshotInternal(database_, path_, util::JsonToVariant(body)));
    (*single_value_listener_holder_)->OnValueChanged(snapshot);
  }
}

void RestCall(DatabaseInternal* database, const std::string& url,
              const char* method, const std::string& post_fields,
              rest::Response* response) {
  struct RestCallArgs {
    DatabaseInternal* database;
    std::string url;
    const char* method;
    std::string post_fields;
    rest::Response* response;
  };

  auto* args = new RestCallArgs();
  args->database = database;
  args->url = url;
  args->method = method;
  args->post_fields = post_fields;
  args->response = response;
  Thread(
      [](void* userdata) {
        auto* args = static_cast<RestCallArgs*>(userdata);
        auto transport = firebase::rest::CreateTransport();
        DatabaseRequest request(args->database);
        request.set_url(args->url.c_str());
        request.set_method(args->method);
        request.set_post_fields(args->post_fields.c_str());
        transport->Perform(request, args->response);
        delete args->response;
        delete args;
      },
      args)
      .Detach();
}

void SseRestCall(DatabaseInternal* database, const QuerySpec& query_spec,
                 const char* url, SseResponse* response,
                 flatbuffers::unique_ptr<rest::Controller>* controller_out) {
  struct SseRestCallArgs {
    SseRestCallArgs(DatabaseInternal* database_, const QuerySpec& query_spec_,
                    rest::TransportCurl* transport_)
        : database(database_),
          query_spec(query_spec_),
          transport(transport_),
          request(database_),
          response(nullptr) {}

    DatabaseInternal* database;
    QuerySpec query_spec;
    rest::TransportCurl* transport;
    DatabaseRequest request;
    SseResponse* response;
  };

  auto* args =
      new SseRestCallArgs(database, query_spec, new rest::TransportCurl());
  args->transport->set_is_async(true);
  args->request.set_url(url);
  args->request.set_method("GET");
  args->request.add_header("Accept", "text/event-stream");
  args->response = response;

  args->transport->Perform(args->request, args->response, controller_out);
  Thread(
      [](void* userdata) {
        auto* args = static_cast<SseRestCallArgs*>(userdata);
        args->response->WaitForCompletion();
        delete args->transport;
        delete args->response;
        delete args;
      },
      args)
      .Detach();
}

static const char* ArgumentSeparator(bool* first) {
  const char* result = *first ? "?" : "&";
  if (*first) *first = false;
  return result;
}

// Returns the auth token for the current user, if there is a current user,
// and they have a token, and auth exists as part of the app.
// Otherwise, returns an empty string.
static std::string GetAuthToken(DatabaseInternal* database) {
  std::string result;
  App* app = database->GetApp();
  if (app) {
    app->function_registry()->CallFunction(
        ::firebase::internal::FnAuthGetCurrentToken, app, nullptr, &result);
  }
  return result;
}

std::string GetUrlWithQuery(const QueryUrlOptions& options,
                            DatabaseInternal* database,
                            const QuerySpec& query_spec) {
  bool first = true;
  std::string url;
  url += database->database_url();
  url += "/";
  url += query_spec.path.str();

  if (options.url_type == QueryUrlOptions::kPriorityUrl) {
    url += "/.priority.json";
  } else {
    url += ".json";
  }

  if (options.url_type == QueryUrlOptions::kValuePriorityUrl) {
    url += ArgumentSeparator(&first);
    url += "format=export";
  }

  if (options.use_auth_token == QueryUrlOptions::kIncludeAuthToken) {
    // Grab the current user's auth token, if any.
    std::string credential = GetAuthToken(database);

    if (!credential.empty()) {
      url += ArgumentSeparator(&first);
      url += "auth=";
      url += rest::util::EncodeUrl(credential);
    }
  }

  if (options.query_spec) {
    switch (options.query_spec->params.order_by) {
      case QueryParams::kOrderByPriority: {
        url += ArgumentSeparator(&first);
        url += "orderBy=\"$priority\"";
        break;
      }
      case QueryParams::kOrderByChild: {
        url += ArgumentSeparator(&first);
        url += "orderBy=\"";
        url += rest::util::EncodeUrl(options.query_spec->params.order_by_child);
        url += "\"";
        break;
      }
      case QueryParams::kOrderByKey: {
        url += ArgumentSeparator(&first);
        url += "orderBy=\"$key\"";
        break;
      }
      case QueryParams::kOrderByValue: {
        url += ArgumentSeparator(&first);
        url += "orderBy=\"$value\"";
        break;
      }
    }

    if (!options.query_spec->params.start_at_value.is_null()) {
      url += ArgumentSeparator(&first);
      url += "startAt=";
      url += rest::util::EncodeUrl(
          util::VariantToJson(options.query_spec->params.start_at_value));
    } else if (!options.query_spec->params.start_at_child_key.empty()) {
      url += ArgumentSeparator(&first);
      url += "startAt=";
      url +=
          rest::util::EncodeUrl(options.query_spec->params.start_at_child_key);
    }

    if (!options.query_spec->params.end_at_value.is_null()) {
      url += ArgumentSeparator(&first);
      url += "endAt=";
      url += rest::util::EncodeUrl(
          util::VariantToJson(options.query_spec->params.end_at_value));
    } else if (!options.query_spec->params.end_at_child_key.empty()) {
      url += ArgumentSeparator(&first);
      url += "endAt=";
      url += rest::util::EncodeUrl(options.query_spec->params.end_at_child_key);
    }

    if (!options.query_spec->params.equal_to_value.is_null()) {
      url += ArgumentSeparator(&first);
      url += "equalTo=";
      url += rest::util::EncodeUrl(
          util::VariantToJson(options.query_spec->params.equal_to_value));
    } else if (!options.query_spec->params.equal_to_child_key.empty()) {
      url += ArgumentSeparator(&first);
      url += "equalTo=";
      url +=
          rest::util::EncodeUrl(options.query_spec->params.equal_to_child_key);
    }

    if (options.query_spec->params.limit_first != 0) {
      url += ArgumentSeparator(&first);
      url += "limitToFirst=";
      url += std::to_string(options.query_spec->params.limit_first);
    }
    if (options.query_spec->params.limit_last != 0) {
      url += ArgumentSeparator(&first);
      url += "limitToLast=";
      url += std::to_string(options.query_spec->params.limit_last);
    }
  }

  return url;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
