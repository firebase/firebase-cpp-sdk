// Copyright 2026 Google
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

import Foundation
import StoreKit
import FirebaseAnalytics

@objc public class StoreKit2Bridge: NSObject {

    @objc public static func logTransaction(_ transactionId: String, context: UnsafeRawPointer, completion: @escaping @convention(c) (UnsafeRawPointer, Bool) -> Void) {
        if #available(iOS 15.0, tvOS 15.0, macOS 12.0, watchOS 8.0, *) {
            Task {
                for await case .verified(let transaction) in Transaction.all {
                    if String(transaction.id) == transactionId {
                        Analytics.logTransaction(transaction)
                        completion(context, true)
                        return
                    }
                }
                completion(context, false)
            }
        } else {
            completion(context, false)
        }
    }
}