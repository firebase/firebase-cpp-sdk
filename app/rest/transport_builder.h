/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_BUILDER_H_
#define FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_BUILDER_H_

#include <limits>
#include <memory>
#include "app/rest/transport_interface.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace rest {

// Build a new Transport. Use this to get a default Transport object instead of
// creating an object of specific transport type. This way, it is easier to
// select which specific type to use by the particular environment, say in an
// actual game or in unit test.
flatbuffers::unique_ptr<Transport> CreateTransport();

// Set a custom builder to use for new Transport.
void SetTransportBuilder(flatbuffers::unique_ptr<Transport>(* builder)());

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_BUILDER_H_
