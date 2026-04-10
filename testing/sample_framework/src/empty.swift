// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This empty Swift file is needed to ensure the Swift runtime is included.
// along with forcing the swift async libraries to be linked on older versions of iOS/tvOS

import Foundation

// This dummy async function forces Xcode to automatically link the 
// Swift Concurrency backwards-compatibility libraries (libswift_Concurrency) 
// on older iOS versions (iOS 15/16).
@available(iOS 13.0, tvOS 13.0, macOS 10.15, watchOS 6.0, *)
func _firebase_sdk_dummy_async_func() async {
}
