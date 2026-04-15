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

// Pure Swift worker to prevent compiler generation bugs with witness tables 
// when mixing @objc and Swift Concurrency on older iOS deployment targets.
@available(iOS 15.0, tvOS 15.0, macOS 12.0, watchOS 8.0, *)
internal struct AppleTransactionWorker {
    static func findAndLogTransaction(id: String) async -> Bool {
        // Using a standard while loop instead of `for await` avoids a known 
        // Xcode 15+ compiler bug where corrupted witness tables are generated 
        // for AsyncSequences when targeting iOS 15/16.
        var iterator = Transaction.all.makeAsyncIterator()
        while let verificationResult = await iterator.next() {
            if case .verified(let transaction) = verificationResult {
                if String(transaction.id) == id {
                    Analytics.logTransaction(transaction)
                    return true
                }
            }
        }
        return false
    }
}

@objc public class AppleTransactionVerifier: NSObject {
    
    // Notice: To avoid C-pointer ABI layout issues across Swift boundaries, 
    // this interface relies purely on a standard Swift closure. The calling 
    // C++ layer captures any necessary pointers within an Objective-C block.
    @objc public static func verify(transactionId: String, completion: @escaping @Sendable (Bool) -> Void) {
        if #available(iOS 15.0, tvOS 15.0, macOS 12.0, watchOS 8.0, *) {
            // Create a pure Swift String copy of the transaction ID.
            // This ensures we do not rely on the memory of the bridged Objective-C
            // NSString, which might be destroyed by the calling C++ thread before
            // the asynchronous Task completes.
            let safeId = String(transactionId)
            
            // Use a detached task to avoid inheriting any C++/ObjC thread context.
            // This prevents EXC_BAD_ACCESS during `swift_task_alloc` on iOS 15/16.
            Task.detached(priority: .userInitiated) { [safeId, completion] in
                let isFound = await AppleTransactionWorker.findAndLogTransaction(id: safeId)
                
                // Return to the main thread to ensure thread safety when the C++ 
                // layer receives the callback and manages internal state.
                DispatchQueue.main.async {
                    completion(isFound)
                }
            }
        } else {
            // Not on a supported OS, resolve immediately on the main thread 
            // to avoid potential deadlocks if called while holding a C++ mutex.
            DispatchQueue.main.async {
                completion(false)
            }
        }
    }
}
