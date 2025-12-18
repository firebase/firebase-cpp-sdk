import Foundation
import FirebaseCore
import FirebaseFirestore

@objc(FirestoreBridge)
public class FirestoreBridge: NSObject {
  private let firestore: Firestore

  @objc public init(app: FirebaseApp) {
    self.firestore = Firestore.firestore(app: app)
    super.init()
  }

  @objc public var settings: FirestoreSettings {
    get {
      return firestore.settings
    }
    set {
      firestore.settings = newValue
    }
  }

  @objc public func enableNetwork(completion: @escaping (Error?) -> Void) {
    firestore.enableNetwork(completion: completion)
  }

  @objc public func disableNetwork(completion: @escaping (Error?) -> Void) {
    firestore.disableNetwork(completion: completion)
  }

  @objc public func clearPersistence(completion: @escaping (Error?) -> Void) {
    firestore.clearPersistence(completion: completion)
  }

  @objc public func waitForPendingWrites(completion: @escaping (Error?) -> Void) {
    firestore.waitForPendingWrites(completion: completion)
  }
}