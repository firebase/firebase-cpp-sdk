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

#include "app/src/thread.h"

namespace firebase {

Thread::Thread() {}

Thread::~Thread() {}

Thread::Thread(Thread&& other) = default;

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg)
    : thread_(start_routine, arg) {}

Thread::Thread(Thread::NoArgRoutine start_routine) : thread_(start_routine) {}

Thread& Thread::operator=(Thread&& other) = default;

bool Thread::Joinable() const { return thread_.joinable(); }

void Thread::Join() { thread_.join(); }

void Thread::Detach() { thread_.detach(); }

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
