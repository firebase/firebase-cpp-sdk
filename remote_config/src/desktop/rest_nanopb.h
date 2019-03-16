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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_H_

// This isn't strictly necessary, however the generated names from the protos
// are very long. This macro generates a few short aliases to variables present
// in the generated files based on the name passed in. It also makes the names
// more idiomatic for constants.
#define NPB_ALIAS_DEF(name, longname)                  \
  using name = longname;                               \
  const name kDefault##name = longname##_init_default; \
  const pb_field_t* const k##name##Fields = longname##_fields;

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_H_
