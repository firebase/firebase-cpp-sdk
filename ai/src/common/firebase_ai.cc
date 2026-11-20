/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <map>
#include <memory>
#include <utility>

#include "ai/src/common/firebase_ai_internal.h"
#include "ai/src/common/generative_model_internal.h"
#include "ai/src/common/http_sender.h"
#include "ai/src/common/template_generative_model_internal.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "firebase/ai.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    ai,
    {
      (void)app;
      // Nothing to do on App creation; FirebaseAI is initialized lazily via
      // FirebaseAI::GetInstance.
      return ::firebase::kInitResultSuccess;
    },
    {
      (void)app;
      // Nothing to do on App teardown; CleanupNotifier handles instance
      // cleanup.
    },
    false);

namespace firebase {
namespace ai {

namespace {

typedef std::pair<::firebase::App*, Backend> InstanceKey;
Mutex g_ai_instances_mutex;  // NOLINT
std::map<InstanceKey, FirebaseAI*>* g_ai_instances = nullptr;

}  // namespace

namespace internal {

FirebaseAIInternal::FirebaseAIInternal(::firebase::App* app,
                                       const Backend& backend)
    : app_(app), backend_(backend) {
  HttpSender::Initialize();
}

FirebaseAIInternal::~FirebaseAIInternal() {
  cleanup_.CleanupAll();
  HttpSender::Cleanup();
}

GenerativeModel FirebaseAIInternal::GetGenerativeModel(
    const std::string& model_name,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    const Optional<RequestOptions>& request_options,
    const Optional<HybridParams>& hybrid_params) {
  std::shared_ptr<GenerativeModelInternal> model_internal(
      new GenerativeModelInternal(
          app_, backend_, model_name, generation_config, safety_settings, tools,
          tool_config, system_instruction, request_options, hybrid_params));
  return GenerativeModel(model_internal);
}

TemplateGenerativeModel FirebaseAIInternal::GetTemplateGenerativeModel(
    const Optional<RequestOptions>& request_options) {
  std::shared_ptr<TemplateGenerativeModelInternal> model_internal(
      new TemplateGenerativeModelInternal(app_, backend_, request_options));
  return TemplateGenerativeModel(model_internal);
}

}  // namespace internal

// --- FirebaseAI public implementation ---

FirebaseAI* FirebaseAI::GetInstance(const Backend& backend) {
  ::firebase::App* app = ::firebase::App::GetInstance();
  if (!app) {
    LogError("FirebaseAI::GetInstance() called before App::Create().");
    return nullptr;
  }
  return GetInstance(app, backend);
}

FirebaseAI* FirebaseAI::GetInstance(::firebase::App* app,
                                    const Backend& backend) {
  if (!app) {
    LogError("FirebaseAI::GetInstance() called with null App.");
    return nullptr;
  }
  MutexLock lock(g_ai_instances_mutex);
  if (!g_ai_instances) {
    g_ai_instances = new std::map<InstanceKey, FirebaseAI*>();
  }
  InstanceKey key(app, backend);
  auto it = g_ai_instances->find(key);
  if (it != g_ai_instances->end()) {
    return it->second;
  }
  FirebaseAI* instance = new FirebaseAI(app, backend);
  (*g_ai_instances)[key] = instance;
  return instance;
}

FirebaseAI::FirebaseAI(::firebase::App* app, const Backend& backend)
    : internal_(new internal::FirebaseAIInternal(app, backend)) {
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  if (notifier) {
    notifier->RegisterObject(this, [](void* object) {
      FirebaseAI* ai = reinterpret_cast<FirebaseAI*>(object);
      LogWarning(
          "FirebaseAI object 0x%08x should be deleted before the App 0x%08x it "
          "depends upon",
          static_cast<int>(reinterpret_cast<intptr_t>(ai)),
          static_cast<int>(reinterpret_cast<intptr_t>(ai->app())));
      ai->DeleteInternal();
    });
  }
}

FirebaseAI::~FirebaseAI() { DeleteInternal(); }

void FirebaseAI::DeleteInternal() {
  MutexLock lock(g_ai_instances_mutex);
  if (!internal_) return;

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app());
  if (notifier) {
    notifier->UnregisterObject(this);
  }

  if (g_ai_instances) {
    InstanceKey key(internal_->app(), internal_->backend());
    g_ai_instances->erase(key);
    if (g_ai_instances->empty()) {
      delete g_ai_instances;
      g_ai_instances = nullptr;
    }
  }

  delete internal_;
  internal_ = nullptr;
}

::firebase::App* FirebaseAI::app() {
  return internal_ ? internal_->app() : nullptr;
}

const ::firebase::App* FirebaseAI::app() const {
  return internal_ ? internal_->app() : nullptr;
}

const Backend& FirebaseAI::backend() const {
  static const Backend kDefaultBackend = Backend::GoogleAI();
  return internal_ ? internal_->backend() : kDefaultBackend;
}

GenerativeModel FirebaseAI::GetGenerativeModel(
    const std::string& model_name,
    const Optional<GenerationConfig>& generation_config,
    const std::vector<SafetySetting>& safety_settings,
    const std::vector<Tool>& tools, const Optional<ToolConfig>& tool_config,
    const Optional<ModelContent>& system_instruction,
    const Optional<RequestOptions>& request_options,
    const Optional<HybridParams>& hybrid_params) {
  if (!internal_) return GenerativeModel();
  return internal_->GetGenerativeModel(
      model_name, generation_config, safety_settings, tools, tool_config,
      system_instruction, request_options, hybrid_params);
}

GenerativeModel FirebaseAI::GetGenerativeModel(
    const std::string& model_name, const HybridParams& hybrid_params,
    const Optional<GenerationConfig>& generation_config,
    const Optional<ModelContent>& system_instruction) {
  return GetGenerativeModel(
      model_name, generation_config, std::vector<SafetySetting>(),
      std::vector<Tool>(), Optional<ToolConfig>(), system_instruction,
      Optional<RequestOptions>(), Optional<HybridParams>(hybrid_params));
}

TemplateGenerativeModel FirebaseAI::GetTemplateGenerativeModel(
    const Optional<RequestOptions>& request_options) {
  if (!internal_) return TemplateGenerativeModel();
  return internal_->GetTemplateGenerativeModel(request_options);
}

}  // namespace ai
}  // namespace firebase
