/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/include/firebase/firestore/settings.h"

#include <dispatch/dispatch.h>
#include <memory>

#include "Firestore/core/src/util/executor_libdispatch.h"

namespace firebase {
namespace firestore {

namespace {

const char kDefaultHost[] = "firestore.googleapis.com";

}

using util::Executor;
using util::ExecutorLibdispatch;

Settings::Settings()
    : host_(kDefaultHost),
      executor_(
          Executor::CreateSerial("com.google.firebase.firestore.callback")) {}

std::unique_ptr<Executor> Settings::CreateExecutor() const {
  return std::make_unique<ExecutorLibdispatch>(dispatch_queue());
}

dispatch_queue_t Settings::dispatch_queue() const {
  if (!executor_) {
    return dispatch_get_main_queue();
  }
  auto* executor_libdispatch =
      static_cast<const ExecutorLibdispatch*>(executor_.get());
  return executor_libdispatch->dispatch_queue();
}

void Settings::set_dispatch_queue(dispatch_queue_t queue) {
  executor_ = std::make_unique<ExecutorLibdispatch>(queue);
}

}  // namespace firestore
}  // namespace firebase
