/*
 * Copyright 2025 Google LLC
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

import Foundation
import FirebaseCore
import FirebaseStorage

@objc(FIRStorageBridge)
public class StorageBridge: NSObject {
  let storage: Storage

  @objc public init(app: FirebaseApp, url: String?) {
    if let url = url, !url.isEmpty {
      self.storage = Storage.storage(app: app, url: url)
    } else {
      self.storage = Storage.storage(app: app)
    }
  }

  @objc public func reference(path: String) -> StorageReferenceBridge {
    return StorageReferenceBridge(reference: storage.reference(withPath: path))
  }

  @objc public func reference(url: String) -> StorageReferenceBridge {
    return StorageReferenceBridge(reference: storage.reference(forURL: url))
  }

  @objc public var maxDownloadRetryTime: TimeInterval {
    get { return storage.maxDownloadRetryTime }
    set { storage.maxDownloadRetryTime = newValue }
  }

  @objc public var maxUploadRetryTime: TimeInterval {
    get { return storage.maxUploadRetryTime }
    set { storage.maxUploadRetryTime = newValue }
  }

  @objc public var maxOperationRetryTime: TimeInterval {
    get { return storage.maxOperationRetryTime }
    set { storage.maxOperationRetryTime = newValue }
  }

  @objc public func useEmulator(host: String, port: Int) {
    storage.useEmulator(withHost: host, port: port)
  }
}

@objc(FIRStorageReferenceBridge)
public class StorageReferenceBridge: NSObject {
  let reference: StorageReference

  @objc public init(reference: StorageReference) {
    self.reference = reference
  }

  @objc public func child(_ path: String) -> StorageReferenceBridge {
    return StorageReferenceBridge(reference: reference.child(path))
  }

  @objc public func parent() -> StorageReferenceBridge? {
    guard let parent = reference.parent() else { return nil }
    return StorageReferenceBridge(reference: parent)
  }

  @objc public func root() -> StorageReferenceBridge {
    return StorageReferenceBridge(reference: reference.root())
  }

  @objc public var name: String { return reference.name }
  @objc public var bucket: String { return reference.bucket }
  @objc public var fullPath: String { return reference.fullPath }
}
