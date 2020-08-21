#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_API_HEADERS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_API_HEADERS_H_

#include "firebase/firestore.h"

namespace firebase {
namespace firestore {
namespace csharp {

// This class allows limited access to private functions of `Firestore` related
// to sending Cloud headers.
class ApiHeaders {
 public:
  static void SetClientLanguage(const std::string& language_token) {
    Firestore::SetClientLanguage(language_token);
  }
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_API_HEADERS_H_
