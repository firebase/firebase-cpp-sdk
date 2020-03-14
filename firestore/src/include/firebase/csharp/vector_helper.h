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

// FieldValue is converted to FieldValueInternal of internal access, which makes
// it impossible to convert std::vector<FieldValue> with std_vector.i. Instead
// of making a fully working std::vector<FieldValue>, we defined a few function
// here.

// We cannot call std::vector<FieldValue>::foo() in C# since it is not exposed
// via the .dll interface as SWIG cannot convert it. Here we essentially define
// a function, which SWIG can convert and thus expose it via the .dll. So C#
// code can call it.

inline std::size_t vector_size(const std::vector<FieldValue>& self) {
  return self.size();
}

inline const FieldValue& vector_get(const std::vector<FieldValue>& self,
                                       std::size_t index) {
  return self[index];
}

inline std::vector<FieldValue> vector_fv_create(std::size_t size) {
  std::vector<FieldValue> result(size);
  return result;
}

// This is the only way to make it efficient for non-STLPort and also
// STLPort-compatible.
inline void vector_set(std::vector<FieldValue>* self, std::size_t index,
                          FieldValue field_value) {
  // This could be either a move-assignment or a normal assignment.
  (*self)[index] = firebase::Move(field_value);
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
