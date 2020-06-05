#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_VECTOR_HELPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_VECTOR_HELPER_H_

#include "app/meta/move.h"
#include "firebase/firestore/document_change.h"
#include "firebase/firestore/document_snapshot.h"
#include "firebase/firestore/field_path.h"
#include "firebase/firestore/field_value.h"

namespace firebase {
namespace firestore {
namespace csharp {

// Simple wrappers to avoid exposing C++ standard library containers to SWIG.
//
// While it's normally possible to work with standard library containers in SWIG
// (by instantiating them for each template type used via the `%template`
// directive), issues in the build environment make that approach too
// complicated to be worth it. Instead, use simple wrappers and make sure the
// underlying containers are never exposed to C#.
//
// Note: most of the time, these classes should be declared with a `using`
// statement to ensure predictable lifetime of the object when dealing with
// iterators or unsafe views.

// Wraps `std::vector<FieldValue>` for use in C# code.
class FieldValueVector {
 public:
  FieldValueVector() = default;

  explicit FieldValueVector(const FieldValue& value)
      : container_(value.array_value()) {}

  std::size_t Size() const { return container_.size(); }

  // The returned reference is only valid as long as this `FieldValueVector` is
  // valid. In C#, declare the vector with a `using` statement to ensure its
  // lifetime exceeds the lifetime of the reference.
  const FieldValue& GetUnsafeView(std::size_t i) const { return container_[i]; }

  FieldValue GetCopy(std::size_t i) const { return container_[i]; }

  void PushBack(const FieldValue& value) {
    container_.push_back(value);
  }

 private:
  friend FieldValue ArrayToFieldValue(const FieldValueVector& wrapper);

  const std::vector<FieldValue>& Unwrap() const { return container_; }

  std::vector<FieldValue> container_;
};

FieldValue ArrayToFieldValue(const FieldValueVector& wrapper) {
  return FieldValue::Array(wrapper.Unwrap());
}

// TODO(varconst): handle everything below in a similar way.

inline std::size_t vector_size(const std::vector<FieldValue>& self) {
  return self.size();
}

// Similarly, DocumentSnapshot is converted to DocumentSnapshotInternal and we
// defined a few helper function to deal with std::vector<DocumentSnapshot>.

inline const DocumentSnapshot& vector_get(
    const std::vector<DocumentSnapshot>& self, std::size_t index) {
  return self[index];
}

// Similarly, DocumentChange is converted to DocumentChangeInternal and we
// defined a few helper function to deal with std::vector<DocumentChange>.

inline std::size_t vector_size(
    const std::vector<DocumentChange>& self){
  return self.size();
}

inline const DocumentChange& vector_get(
    const std::vector<DocumentChange>& self, std::size_t index) {
  return self[index];
}

// Similarly, FieldPath is converted to FieldPathInternal and we defined a few
// helper function to deal with std::vector<FieldPath>.

inline std::vector<FieldPath> vector_fp_create() {
  return std::vector<FieldPath>{};
}

inline void vector_push_back(std::vector<FieldPath>* self,
                             FieldPath field_path) {
  self->push_back(firebase::Move(field_path));
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_VECTOR_HELPER_H_
