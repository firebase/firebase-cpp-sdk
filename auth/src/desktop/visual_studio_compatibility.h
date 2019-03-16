// Copyright 2017 Google LLC
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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VISUAL_STUDIO_COMPATIBILITY_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VISUAL_STUDIO_COMPATIBILITY_H_

#include <utility>

// Visual Studio 2013 and below don't generate implicitly-defined move
// constructors. Also, because they don't treat move constructor as a "special"
// function (they see it as a user-provided constructor), they refuse to
// generate a default constructor.
// To reduce related boilerplate, provide a macro to generate the simplest
// default and move constructors for a class with a base class and no data
// members of its own
// clang-format off
#define DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(ClassName, BaseClassName)       \
  ClassName() {}                                                               \
  ClassName(ClassName&& rhs)                                                   \
      : BaseClassName(std::move(rhs)) {}
// clang-format on

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VISUAL_STUDIO_COMPATIBILITY_H_
