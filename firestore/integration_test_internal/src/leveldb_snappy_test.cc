/*
 * Copyright 2022 Google LLC
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

// This test file does not compile on Android because Android uses the built-in
// sqlite database, not LevelDb.
#if defined(__ANDROID__)
#error "This test should not be included in Android."
#endif

// Just re-use the unit test from the iOS SDK.
// TODO(dconeybe) Import ALL the unit tests from the iOS SDK by adding them
// to the CMakeLists.txt in the parent directory of this file. That way we can
// run all of the tests from the iOS SDK on each platform targeted by this repo.
// (This currently fails due to include paths on iOS, so it's been commented out for now.)
// #include "FirebaseFirestore/Firestore/core/test/unit/local/leveldb_snappy_test.cc"
