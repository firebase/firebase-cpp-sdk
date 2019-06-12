// Copyright 2019 Google LLC
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

#include "app/instance_id/instance_id_desktop_impl.h"

#include <assert.h>

#include <algorithm>

#include "app/instance_id/iid_data_generated.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/rest/util.h"
#include "app/rest/www_form_url_encoded.h"
#include "app/src/app_common.h"
#include "app/src/app_identifier.h"
#include "app/src/base64.h"
#include "app/src/callback.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/locale.h"
#include "app/src/time.h"
#include "app/src/uuid.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace instance_id {
namespace internal {

using firebase::app::secure::UserSecureManager;
using firebase::callback::NewCallback;

// Response that signals this class when it's complete or canceled.
class SignalSemaphoreResponse : public rest::Response {
 public:
  explicit SignalSemaphoreResponse(Semaphore* complete) : complete_(complete) {}

  void MarkCompleted() override {
    rest::Response::MarkCompleted();
    complete_->Post();
  }

  void MarkFailed() override {
    rest::Response::MarkFailed();
    complete_->Post();
  }

  void Wait() { complete_->Wait(); }

 private:
  Semaphore* complete_;
};

// State for the current network operation.
struct NetworkOperation {
  NetworkOperation(const std::string& request_data, Semaphore* complete)
      : request(request_data.c_str(), request_data.length()),
        response(complete) {}

  // Schedule the network operation.
  void Perform(rest::Transport* transport) {
    transport->Perform(request, &response, &controller);
  }

  // Cancel the current operation.
  void Cancel() {
    rest::Controller* ctrl = controller.get();
    if (ctrl) ctrl->Cancel();
  }

  // Data sent to the server.
  rest::Request request;
  // Data returned by the server.
  SignalSemaphoreResponse response;
  // Progress of the request and allows us to cancel the request.
  flatbuffers::unique_ptr<rest::Controller> controller;
};

// Check-in backend.
static const char kCheckinUrl[] =
    "https://device-provisioning.googleapis.com/checkin";
// Check-in refresh period (7 days) in milliseconds.
static const uint64_t kCheckinRefreshPeriodMs =
    7 * 24 * 60 * firebase::internal::kMillisecondsPerMinute;
// Request type and protocol for check-in requests.
static const int kCheckinRequestType = 2;
static const int kCheckinProtocolVersion = 2;
// Instance ID backend.
static const char kInstanceIdUrl[] = "https://fcmtoken.googleapis.com/register";

// Backend had a temporary failure, the client should retry. http://b/27043795
static const char kPhoneRegistrationError[] = "PHONE_REGISTRATION_ERROR";
// Server detected the token is no longer valid.
static const char kTokenResetError[] = "RST";
// Wildcard scope used to delete all tokens and the ID in a server registration.
static const char kWildcardTokenScope[] = "*";

// Minimum retry time (in seconds) when a fetch token request fails.
static const uint64_t kMinimumFetchTokenDelaySec = 30;
// Maximum retry time (in seconds) when a fetch token request fails.
static const uint64_t kMaximumFetchTokenDelaySec =
    8 * firebase::internal::kMinutesPerHour *
    firebase::internal::kSecondsPerMinute;

std::map<App*, InstanceIdDesktopImpl*>
    InstanceIdDesktopImpl::instance_id_by_app_;          // NOLINT
Mutex InstanceIdDesktopImpl::instance_id_by_app_mutex_;  // NOLINT

void InstanceIdDesktopImpl::FetchServerTokenCallback::Run() {
  bool retry;
  bool fetched_token = iid_->FetchServerToken(scope_.c_str(), &retry);
  if (fetched_token) {
    iid_->ref_future()->CompleteWithResult(
        future_handle_, kErrorNone, "", iid_->FindCachedToken(scope_.c_str()));
  } else if (retry) {
    // Retry with an expodential backoff.
    retry_delay_time_ =
        std::min(std::max(retry_delay_time_ * 2, kMinimumFetchTokenDelaySec),
                 kMaximumFetchTokenDelaySec);
    iid_->scheduler_.Schedule(this, retry_delay_time_);
  } else {
    iid_->ref_future()->Complete(future_handle_, kErrorUnknownError,
                                 "FetchToken failed");
  }
}

InstanceIdDesktopImpl::InstanceIdDesktopImpl(App* app)
    : storage_semaphore_(0),
      app_(app),
      locale_(firebase::internal::GetLocale()),
      logging_id_(rand()),  // NOLINT
      ios_device_model_(app_common::kOperatingSystem),
      ios_device_version_("0.0.0" /* TODO */),
      app_version_("1.2.3" /* TODO */),
      os_version_(app_common::kOperatingSystem /* TODO add: version */),
      platform_(0),
      network_operation_complete_(0),
      terminating_(false) {
  rest::InitTransportCurl();
  rest::util::Initialize();
  transport_.reset(new rest::TransportCurl());

  future_manager().AllocFutureApi(this, kInstanceIdFnCount);

  std::string package_name = app->options().package_name();
  std::string project_id = app->options().project_id();
  std::string app_identifier;

  user_secure_manager_ = MakeUnique<UserSecureManager>(
      "iid", firebase::internal::CreateAppIdentifierFromOptions(app_->options())
                 .c_str());

  // Guarded through GetInstance() already.
  instance_id_by_app_[app] = this;

  // Cleanup this object if app is destroyed.
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  assert(notifier);
  notifier->RegisterObject(this, [](void* object) {
    // Since this is going to be shared by many other modules, ex. function and
    // instance_id, nothing should delete InstanceIdDesktopImpl until App is
    // deleted.
    // TODO(chkuang): Maybe reference count by module?
    InstanceIdDesktopImpl* instance_id_to_delete =
        reinterpret_cast<InstanceIdDesktopImpl*>(object);
    LogDebug(
        "InstanceIdDesktopImpl object 0x%08x is deleted when the App 0x%08x it "
        "depends upon is deleted.",
        static_cast<int>(reinterpret_cast<intptr_t>(instance_id_to_delete)),
        static_cast<int>(
            reinterpret_cast<intptr_t>(&instance_id_to_delete->app())));
    delete instance_id_to_delete;
  });
}

InstanceIdDesktopImpl::~InstanceIdDesktopImpl() {
  // Cancel any pending network operations.
  {
    MutexLock lock(network_operation_mutex_);
    // All outstanding operations should complete with an error.
    terminating_ = true;
    NetworkOperation* operation = network_operation_.get();
    if (operation) operation->Cancel();
  }
  // Cancel scheduled tasks and shut down the scheduler to prevent any
  // additional tasks being executed.
  scheduler_.CancelAllAndShutdownWorkerThread();

  rest::CleanupTransportCurl();
  rest::util::Terminate();
  {
    MutexLock lock(instance_id_by_app_mutex_);
    auto it = instance_id_by_app_.find(app_);
    if (it == instance_id_by_app_.end()) return;
    assert(it->second == this);
    instance_id_by_app_.erase(it);
  }

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
  assert(notifier);
  notifier->UnregisterObject(this);
}

InstanceIdDesktopImpl* InstanceIdDesktopImpl::GetInstance(App* app) {
#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
  MutexLock lock(instance_id_by_app_mutex_);
  auto it = instance_id_by_app_.find(app);
  return it != instance_id_by_app_.end() ? it->second
                                         : new InstanceIdDesktopImpl(app);
#else
  // Callers know that nullptr means "act as a stub".
  return nullptr;
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW
}

Future<std::string> InstanceIdDesktopImpl::GetId() {
  // GetId() -> Check-in, generate an ID / get cached ID, return the ID
  SafeFutureHandle<std::string> handle =
      ref_future()->SafeAlloc<std::string>(kInstanceIdFnGetId);

  if (terminating_) {
    ref_future()->Complete(handle, kErrorShutdown,
                           "Failed due to App shutdown in progress");
  } else {
    auto callback = NewCallback(
        [](InstanceIdDesktopImpl* _this,
           SafeFutureHandle<std::string> _handle) {
          if (_this->InitialOrRefreshCheckin()) {
            _this->ref_future()->CompleteWithResult(_handle, kErrorNone, "",
                                                    _this->instance_id_);
          } else {
            _this->ref_future()->Complete(_handle, kErrorUnavailable,
                                          "Error in checkin");
          }
        },
        this, handle);

    scheduler_.Schedule(callback);
  }

  return MakeFuture(ref_future(), handle);
}

Future<std::string> InstanceIdDesktopImpl::GetIdLastResult() {
  return static_cast<const Future<std::string>&>(
      ref_future()->LastResult(kInstanceIdFnGetId));
}

Future<void> InstanceIdDesktopImpl::DeleteId() {
  // DeleteId() -> Delete all tokens and remove them from the cache, then clear
  // ID.
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kInstanceIdFnRemoveId);

  if (terminating_) {
    ref_future()->Complete(handle, kErrorShutdown,
                           "Failed due to App shutdown in progress");
  } else {
    auto callback = NewCallback(
        [](InstanceIdDesktopImpl* _this, SafeFutureHandle<void> _handle) {
          if (_this->DeleteServerToken(nullptr, true)) {
            _this->ref_future()->Complete(_handle, kErrorNone, "");
          } else {
            _this->ref_future()->Complete(_handle, kErrorUnknownError,
                                          "DeleteId failed");
          }
        },
        this, handle);

    scheduler_.Schedule(callback);
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> InstanceIdDesktopImpl::DeleteIdLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kInstanceIdFnRemoveId));
}

Future<std::string> InstanceIdDesktopImpl::GetToken(const char* scope) {
  // GetToken() -> GetId() then get token from cache or using check-in
  // information, return the token

  SafeFutureHandle<std::string> handle =
      ref_future()->SafeAlloc<std::string>(kInstanceIdFnGetToken);

  if (terminating_) {
    ref_future()->Complete(handle, kErrorShutdown,
                           "Failed due to App shutdown in progress");
  } else {
    scheduler_.Schedule(new FetchServerTokenCallback(this, scope, handle));
  }

  return MakeFuture(ref_future(), handle);
}

Future<std::string> InstanceIdDesktopImpl::GetTokenLastResult() {
  return static_cast<const Future<std::string>&>(
      ref_future()->LastResult(kInstanceIdFnGetToken));
}

Future<void> InstanceIdDesktopImpl::DeleteToken(const char* scope) {
  // DeleteToken() --> delete token request and remove from the cache
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kInstanceIdFnRemoveToken);

  if (terminating_) {
    ref_future()->Complete(handle, kErrorShutdown,
                           "Failed due to App shutdown in progress");
  } else {
    std::string scope_str(scope);

    auto callback = NewCallback(
        [](InstanceIdDesktopImpl* _this, std::string _scope_str,
           SafeFutureHandle<void> _handle) {
          if (_this->DeleteServerToken(_scope_str.c_str(), false)) {
            _this->ref_future()->Complete(_handle, kErrorNone, "");
          } else {
            _this->ref_future()->Complete(_handle, kErrorUnknownError,
                                          "DeleteToken failed");
          }
        },
        this, scope_str, handle);

    scheduler_.Schedule(callback);
  }

  return MakeFuture(ref_future(), handle);
}

Future<void> InstanceIdDesktopImpl::DeleteTokenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kInstanceIdFnRemoveToken));
}

// Save the instance ID to local secure storage.
bool InstanceIdDesktopImpl::SaveToStorage() {
  if (terminating_) return false;

  // Build up a serialized buffer algorithmically:
  flatbuffers::FlatBufferBuilder builder;

  std::vector<flatbuffers::Offset<Token>> token_offsets;
  for (auto it = tokens_.begin(); it != tokens_.end(); ++it) {
    token_offsets.push_back(CreateTokenDirect(builder,
                                              it->first.c_str() /* scope */,
                                              it->second.c_str() /* token */));
  }
  auto iid_data_table = CreateInstanceIdDesktopDataDirect(
      builder, instance_id_.c_str(), checkin_data_.device_id.c_str(),
      checkin_data_.security_token.c_str(), checkin_data_.digest.c_str(),
      checkin_data_.last_checkin_time_ms, &token_offsets);
  builder.Finish(iid_data_table);

  std::string save_string;
  auto bufferpointer =
      reinterpret_cast<const char*>(builder.GetBufferPointer());
  save_string.assign(bufferpointer, bufferpointer + builder.GetSize());
  // Encode flatbuffer
  std::string encoded;
  UserSecureManager::BinaryToAscii(save_string, &encoded);

  Future<void> future =
      user_secure_manager_->SaveUserData(app_->name(), encoded);

  future.OnCompletion(
      [](const Future<void>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  return future.error() == 0;
}

// Load the instance ID from local secure storage.
bool InstanceIdDesktopImpl::LoadFromStorage() {
  Future<std::string> future = user_secure_manager_->LoadUserData(app_->name());

  future.OnCompletion(
      [](const Future<std::string>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  if (future.error() == 0) {
    ReadStoredInstanceIdData(*future.result());
  }
  return future.error() == 0;
}

// Delete the instance ID from local secure storage.
bool InstanceIdDesktopImpl::DeleteFromStorage() {
  if (terminating_) return false;

  Future<void> future = user_secure_manager_->DeleteUserData(app_->name());

  future.OnCompletion(
      [](const Future<void>& result, void* sem_void) {
        Semaphore* sem_ptr = reinterpret_cast<Semaphore*>(sem_void);
        sem_ptr->Post();
      },
      &storage_semaphore_);
  storage_semaphore_.Wait();
  return future.error() == 0;
}

// Returns true if data was successfully read.
bool InstanceIdDesktopImpl::ReadStoredInstanceIdData(
    const std::string& loaded_string) {
  // Decode to flatbuffer
  std::string decoded;
  if (!UserSecureManager::AsciiToBinary(loaded_string, &decoded)) {
    LogWarning("Error decoding saved Instance ID.");
    return false;
  }
  // Verify the Flatbuffer is valid.
  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(decoded.c_str()), decoded.length());
  if (!VerifyInstanceIdDesktopDataBuffer(verifier)) {
    LogWarning("Error verifying saved Instance ID.");
    return false;
  }

  auto iid_data_fb = GetInstanceIdDesktopData(decoded.c_str());
  if (iid_data_fb == nullptr) {
    LogWarning("Error reading table for saved Instance ID.");
    return false;
  }

  instance_id_ = iid_data_fb->instance_id()->c_str();
  checkin_data_.device_id = iid_data_fb->device_id()->c_str();
  checkin_data_.security_token = iid_data_fb->security_token()->c_str();
  checkin_data_.digest = iid_data_fb->digest()->c_str();
  checkin_data_.last_checkin_time_ms = iid_data_fb->last_checkin_time_ms();
  tokens_.clear();
  auto fbtokens = iid_data_fb->tokens();
  for (auto it = fbtokens->begin(); it != fbtokens->end(); ++it) {
    tokens_[it->scope()->c_str()] = it->token()->c_str();
  }
  return true;
}

bool InstanceIdDesktopImpl::InitialOrRefreshCheckin() {
  if (terminating_) return false;

  // Load check-in data from storage if it hasn't already been loaded.
  if (checkin_data_.last_checkin_time_ms == 0) {
    // Try loading from storage.  Since we can't tell whether this failed
    // because the data doesn't exist or if there is an I/O error, just
    // continue.
    LoadFromStorage();
  }

  // If we don't have an instance_id_ yet, generate one now.
  if (instance_id_.empty()) {
    instance_id_ = GenerateAppId();
  }

  if (checkin_data_.device_id.empty() || checkin_data_.security_token.empty() ||
      checkin_data_.digest.empty()) {
    // If any of these aren't set, checkin data is incomplete, so clear it.
    checkin_data_.Clear();
  }

  // If we've already checked in.
  if (checkin_data_.last_checkin_time_ms > 0) {
    // Make sure the device ID and token aren't expired.
    uint64_t time_elapsed_ms =
        firebase::internal::GetTimestamp() - checkin_data_.last_checkin_time_ms;
    if (time_elapsed_ms < kCheckinRefreshPeriodMs) {
      // Everything is up to date.
      return true;
    }
    checkin_data_.Clear();
  }

  // Construct the JSON request.
  flexbuffers::Builder fbb;
  struct BuilderScope {
    BuilderScope(flexbuffers::Builder* fbb_, const CheckinData* checkin_data_,
                 const char* locale_, const char* timezone_, int logging_id_,
                 const char* ios_device_model_, const char* ios_device_version_)
        : fbb(fbb_),
          checkin_data(checkin_data_),
          locale(locale_),
          timezone(timezone_),
          logging_id(logging_id_),
          ios_device_model(ios_device_model_),
          ios_device_version(ios_device_version_) {}

    flexbuffers::Builder* fbb;
    const CheckinData* checkin_data;
    const char* locale;
    const char* timezone;
    int logging_id;
    const char* ios_device_model;
    const char* ios_device_version;
  } builder_scope(&fbb, &checkin_data_, locale_.c_str(),
                  timezone_.empty() ? firebase::internal::GetTimezone().c_str()
                                    : timezone_.c_str(),
                  logging_id_, ios_device_model_.c_str(),
                  ios_device_version_.c_str());
  fbb.Map(
      [](BuilderScope& builder_scope) {
        flexbuffers::Builder* fbb = builder_scope.fbb;
        const CheckinData& checkin_data = *builder_scope.checkin_data;
        fbb->Map(
            "checkin",
            [](BuilderScope& builder_scope) {
              flexbuffers::Builder* fbb = builder_scope.fbb;
              const CheckinData& checkin_data = *builder_scope.checkin_data;
              fbb->Map(
                  "iosbuild",
                  [](BuilderScope& builder_scope) {
                    flexbuffers::Builder* fbb = builder_scope.fbb;
                    fbb->String("model", builder_scope.ios_device_model);
                    fbb->String("os_version", builder_scope.ios_device_version);
                  },
                  builder_scope);
              fbb->Int("type", kCheckinRequestType);
              fbb->Int("user_number", 0 /* unused at the moment */);
              fbb->Int("last_checkin_msec", checkin_data.last_checkin_time_ms);
            },
            builder_scope);
        fbb->Int("fragment", 0 /* unused at the moment */);
        fbb->Int("logging_id", builder_scope.logging_id);
        fbb->String("locale", builder_scope.locale);
        fbb->Int("version", kCheckinProtocolVersion);
        fbb->String("digest", checkin_data.digest.c_str());
        fbb->String("timezone", builder_scope.timezone);
        fbb->Int("user_serial_number", 0 /* unused at the moment */);
        fbb->Int("id",
                 flatbuffers::StringToInt(checkin_data.device_id.c_str()));
        fbb->Int("security_token",
                 flatbuffers::StringToInt(checkin_data.security_token.c_str()));
      },
      builder_scope);
  fbb.Finish();
  request_buffer_.clear();
  flexbuffers::GetRoot(fbb.GetBuffer()).ToString(true, true, request_buffer_);
  // Send request to the server then wait for the response or for the request
  // to be canceled by another thread.
  {
    MutexLock lock(network_operation_mutex_);
    network_operation_.reset(
        new NetworkOperation(request_buffer_, &network_operation_complete_));
    rest::Request* request = &network_operation_->request;
    request->set_url(kCheckinUrl);
    request->set_method(rest::util::kPost);
    request->add_header(rest::util::kAccept, rest::util::kApplicationJson);
    request->add_header(rest::util::kContentType, rest::util::kApplicationJson);
    network_operation_->Perform(transport_.get());
  }
  network_operation_complete_.Wait();

  logging_id_ = rand();  // NOLINT
  {
    MutexLock lock(network_operation_mutex_);
    assert(network_operation_.get());
    const rest::Response& response = network_operation_->response;
    // Check for errors
    if (response.status() != rest::util::HttpSuccess) {
      LogError("Check-in failed with response %d '%s'", response.status(),
               response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    // Parse the response.
    flexbuffers::Builder fbb;
    flatbuffers::Parser parser;
    if (!parser.ParseFlexBuffer(response.GetBody(), nullptr, &fbb)) {
      LogError("Unable to parse response '%s'", response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    auto root = flexbuffers::GetRoot(fbb.GetBuffer()).AsMap();
    if (!root["stats_ok"].AsBool()) {
      LogError("Unexpected stats_ok field '%s' vs 'true'",
               root["stats_ok"].ToString().c_str());
      network_operation_.reset(nullptr);
      return false;
    }
    checkin_data_.device_id = root["android_id"].ToString();
    checkin_data_.security_token = root["security_token"].ToString();
    checkin_data_.digest = root["digest"].AsString().c_str();
    checkin_data_.last_checkin_time_ms = firebase::internal::GetTimestamp();
    network_operation_.reset(nullptr);
    if (!SaveToStorage()) {
      checkin_data_.Clear();
      LogError("Unable to save check-in information to storage.");
      return false;
    }
  }
  return true;
}

std::string InstanceIdDesktopImpl::GenerateAppId() {
  firebase::internal::Uuid uuid;
  uuid.Generate();
  // Collapse into 8 bytes, forcing the top 4 bits to be 0x70.
  uint8_t buffer[8];
  buffer[0] = ((uuid.data[0] ^ uuid.data[8]) & 0x0f) | 0x70;
  buffer[1] = uuid.data[1] ^ uuid.data[9];
  buffer[2] = uuid.data[2] ^ uuid.data[10];
  buffer[3] = uuid.data[3] ^ uuid.data[11];
  buffer[4] = uuid.data[4] ^ uuid.data[12];
  buffer[5] = uuid.data[5] ^ uuid.data[13];
  buffer[6] = uuid.data[6] ^ uuid.data[14];
  buffer[7] = uuid.data[7] ^ uuid.data[15];

  std::string input(reinterpret_cast<char*>(buffer), sizeof(buffer));
  std::string output;

  if (firebase::internal::Base64EncodeUrlSafe(input, &output)) {
    return output;
  }
  return "";  // Error encoding.
}

std::string InstanceIdDesktopImpl::FindCachedToken(const char* scope) {
  auto cached_token = tokens_.find(scope);
  if (cached_token == tokens_.end()) return std::string();
  return cached_token->second;
}

void InstanceIdDesktopImpl::DeleteCachedToken(const char* scope) {
  auto cached_token = tokens_.find(scope);
  if (cached_token != tokens_.end()) tokens_.erase(cached_token);
}

void InstanceIdDesktopImpl::ServerTokenOperation(
    const char* scope,
    void (*request_callback)(rest::Request* request, void* state),
    void* state) {
  const AppOptions& app_options = app_->options();
  request_buffer_.clear();
  rest::WwwFormUrlEncoded form(&request_buffer_);
  form.Add("sender", app_options.messaging_sender_id());
  form.Add("app", app_options.package_name());
  form.Add("app_ver", app_version_.c_str());
  form.Add("device", checkin_data_.device_id.c_str());
  // NOTE: We're not providing the Xcliv field here as we don't support
  // FCM on desktop yet.
  form.Add("X-scope", scope);
  form.Add("X-subtype", app_options.messaging_sender_id());
  form.Add("X-osv", os_version_.c_str());
  form.Add("plat", flatbuffers::NumToString(platform_).c_str());
  form.Add("app_id", instance_id_.c_str());

  // Send request to the server then wait for the response or for the request
  // to be canceled by another thread.
  {
    MutexLock lock(network_operation_mutex_);
    network_operation_.reset(
        new NetworkOperation(request_buffer_, &network_operation_complete_));
    rest::Request* request = &network_operation_->request;
    request->set_url(kInstanceIdUrl);
    request->set_method(rest::util::kPost);
    request->add_header(rest::util::kAccept,
                        rest::util::kApplicationWwwFormUrlencoded);
    request->add_header(rest::util::kContentType,
                        rest::util::kApplicationWwwFormUrlencoded);
    request->add_header(rest::util::kAuthorization,
                        (std::string("AidLogin ") + checkin_data_.device_id +
                         std::string(":") + checkin_data_.security_token)
                            .c_str());
    if (request_callback) request_callback(request, state);
    network_operation_->Perform(transport_.get());
  }
  network_operation_complete_.Wait();
}

bool InstanceIdDesktopImpl::FetchServerToken(const char* scope, bool* retry) {
  *retry = false;
  if (terminating_ || !InitialOrRefreshCheckin()) return false;

  // If we already have a token, don't refresh.
  std::string token = FindCachedToken(scope);
  if (!token.empty()) return true;

  ServerTokenOperation(scope, nullptr, nullptr);
  {
    MutexLock lock(network_operation_mutex_);
    assert(network_operation_.get());
    const rest::Response& response = network_operation_->response;
    // Check for errors
    if (response.status() != rest::util::HttpSuccess) {
      LogError("Instance ID token fetch failed with response %d '%s'",
               response.status(), response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    // Parse the response.
    auto form_data = rest::WwwFormUrlEncoded::Parse(response.GetBody());
    std::string error;
    // Search the response for a token or an error.
    for (size_t i = 0; i < form_data.size(); ++i) {
      const auto& item = form_data[i];
      if (item.key == "token") {
        token = item.value;
      } else if (item.key == "Error") {
        error = item.value;
      }
    }

    // Parse any returned errors.
    if (!error.empty()) {
      size_t component_start = 0;
      size_t component_end;
      do {
        component_end = error.find(":", component_start);
        std::string error_component =
            error.substr(component_start, component_end - component_start);
        if (error_component == kPhoneRegistrationError) {
          network_operation_.reset(nullptr);
          *retry = true;
          return false;
        } else if (error_component == kTokenResetError) {
          // Server requests that the token is reset.
          DeleteServerToken(nullptr, true);
          network_operation_.reset(nullptr);
          return false;
        }
        component_start = component_end + 1;
      } while (component_end != std::string::npos);
    }

    if (token.empty()) {
      LogError(
          "No token returned in instance ID token fetch. "
          "Responded with '%s'",
          response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }

    // Cache the token.
    tokens_[scope] = token;
    network_operation_.reset(nullptr);
  }
  if (!SaveToStorage()) {
    LogError("Failed to save token for scope %s to storage", scope);
    return false;
  }
  return true;
}

bool InstanceIdDesktopImpl::DeleteServerToken(const char* scope,
                                              bool delete_id) {
  if (terminating_) return false;

  // Load credentials from storage as we'll need them to delete the ID.
  LoadFromStorage();

  if (delete_id) {
    if (tokens_.empty() && instance_id_.empty()) return true;
    scope = kWildcardTokenScope;
  } else {
    // If we don't have a token, we have nothing to do.
    std::string token = FindCachedToken(scope);
    if (token.empty()) return true;
  }

  ServerTokenOperation(
      scope,
      [](rest::Request* request, void* delete_id) {
        std::string body;
        request->ReadBodyIntoString(&body);
        rest::WwwFormUrlEncoded form(&body);
        form.Add("delete", "true");
        if (delete_id) form.Add("iid-operation", "delete");
        request->set_post_fields(body.c_str(), body.length());
      },
      reinterpret_cast<void*>(delete_id ? 1 : 0));

  {
    MutexLock lock(network_operation_mutex_);
    assert(network_operation_.get());
    const rest::Response& response = network_operation_->response;
    // Check for errors
    if (response.status() != rest::util::HttpSuccess) {
      LogError("Instance ID token delete failed with response %d '%s'",
               response.status(), response.GetBody());
      network_operation_.reset(nullptr);
      return false;
    }
    // Parse the response.
    auto form_data = rest::WwwFormUrlEncoded::Parse(response.GetBody());
    bool found_valid_response = false;
    for (int i = 0; i < form_data.size(); ++i) {
      const auto& item = form_data[i];
      if (item.key == "deleted" || item.key == "token") {
        found_valid_response = true;
        break;
      }
    }
    if (found_valid_response) {
      if (delete_id) {
        checkin_data_.Clear();
        instance_id_.clear();
        tokens_.clear();
        DeleteFromStorage();
      } else {
        DeleteCachedToken(scope);
        if (!SaveToStorage()) {
          LogError("Failed to delete tokens for scope %s from storage", scope);
          network_operation_.reset(nullptr);
          return false;
        }
      }
    } else {
      LogError(
          "Instance ID token delete failed, server returned invalid "
          "response '%s'",
          response.GetBody());
      network_operation_.reset(nullptr);
    }
    return found_valid_response;
  }
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
