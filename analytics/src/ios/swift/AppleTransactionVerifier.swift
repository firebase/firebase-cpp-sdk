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

@objc public class AppleTransactionVerifier: NSObject {
    
    @objc public static func verify(transactionId: String, completion: @escaping @Sendable (Bool) -> Void) {
        if #available(iOS 15.0, tvOS 15.0, macOS 12.0, watchOS 8.0, *) {
            Task {
                var iterator = Transaction.all.makeAsyncIterator()
                while let verificationResult = await iterator.next() {
                    if case .verified(let transaction) = verificationResult {
                        if String(transaction.id) == transactionId {
                            Analytics.logTransaction(transaction)
                            completion(true)
                            return
                        }
                    }
                }
                completion(false)
            }
        } else {
            completion(false)
        }
    }
}
