#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_

#include <string>
#include <vector>

#include "app/meta/move.h"
#include "firebase/future.h"
#include "firebase/firestore/document_reference.h"
#include "firebase/firestore/document_snapshot.h"
#include "firebase/firestore/field_path.h"
#include "firebase/firestore/field_value.h"
#include "firebase/firestore/set_options.h"
#include "firebase/firestore/transaction.h"
#include "firebase/firestore/write_batch.h"

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

// Wraps `MapFieldValue` for use in C# code.
class FieldValueMap {
 public:
  FieldValueMap() = default;
  explicit FieldValueMap(const FieldValue& value)
      : container_(value.map_value()) {}

  std::size_t Size() const { return container_.size(); }

  // The returned reference is only valid as long as this `FieldValueMap` is
  // valid. In C#, declare the map with a `using` statement to ensure its
  // lifetime exceeds the lifetime of the reference.
  const FieldValue& GetUnsafeView(const std::string& key) const {
    auto found = container_.find(key);
    if (found != container_.end()) {
      return found->second;
    }

    return InvalidFieldValue();
  }

  FieldValue GetCopy(const std::string& key) const {
    // Note: this is safe because the reference returned by `GetUnsafeView` is
    // copied into the return value.
    return GetUnsafeView(key);
  }

  void Insert(const std::string& key, const FieldValue& value) {
    container_[key] = value;
  }

  class FieldValueMapIterator {
   public:
    bool HasMore() const { return cur_ != end_; }

    void Advance() { ++cur_; }

    const std::string& UnsafeKeyView() const { return cur_->first; }
    const FieldValue& UnsafeValueView() const { return cur_->second; }

    std::string KeyCopy() const { return cur_->first; }
    FieldValue ValueCopy() const { return cur_->second; }

   private:
    friend class FieldValueMap;
    explicit FieldValueMapIterator(const FieldValueMap& wrapper)
        : cur_{wrapper.Unwrap().begin()}, end_{wrapper.Unwrap().end()} {}

    MapFieldValue::const_iterator cur_, end_;
  };

  FieldValueMapIterator Iterator() const {
    return FieldValueMapIterator(*this);
  }

  FieldValue ToFieldValue() const { return FieldValue::Map(Unwrap()); }

  static FieldValue SnapshotToFieldValue(
      const DocumentSnapshot& snapshot,
      DocumentSnapshot::ServerTimestampBehavior stb) {
    return FieldValue::Map(snapshot.GetData(stb));
  }

  static void TransactionUpdate(Transaction* transaction,
                                const DocumentReference& doc,
                                const FieldValue& field_value) {
    transaction->Update(doc, field_value.map_value());
  }

  static void TransactionUpdate(Transaction* transaction,
                                const DocumentReference& doc,
                                const FieldValueMap& wrapper) {
    transaction->Update(doc, wrapper.Unwrap());
  }

  static void WriteBatchUpdate(WriteBatch* batch, const DocumentReference& doc,
                               const FieldValue& field_value) {
    batch->Update(doc, field_value.map_value());
  }

  static void WriteBatchUpdate(WriteBatch* batch, const DocumentReference& doc,
                               const FieldValueMap& wrapper) {
    batch->Update(doc, wrapper.Unwrap());
  }

  static Future<void> DocumentReferenceSet(DocumentReference& doc,
                                           const FieldValue& field_value,
                                           const SetOptions& options) {
    return doc.Set(field_value.map_value(), options);
  }

  static Future<void> DocumentReferenceUpdate(DocumentReference& doc,
                                              const FieldValue& field_value) {
    return doc.Update(field_value.map_value());
  }

 private:
  const MapFieldValue& Unwrap() const { return container_; }

  static const FieldValue& InvalidFieldValue() {
    static FieldValue value;
    return value;
  }

  MapFieldValue container_;
};

// Wraps `MapFieldPathValue` for use in C# code.
// TODO(varconst): this should be a template instantiation, not a separate
// class.
class FieldPathValueMap {
 public:
  FieldPathValueMap() = default;

  std::size_t Size() const { return container_.size(); }

  // The returned reference is only valid as long as this `FieldPathValueMap` is
  // valid. In C#, declare the map with a `using` statement to ensure its
  // lifetime exceeds the lifetime of the reference.
  const FieldValue& GetUnsafeView(const FieldPath& key) const {
    auto found = container_.find(key);
    if (found != container_.end()) {
      return found->second;
    }

    return InvalidFieldValue();
  }

  FieldValue GetCopy(const FieldPath& key) const {
    // Note: this is safe because the reference returned by `GetUnsafeView` is
    // copied into the return value.
    return GetUnsafeView(key);
  }

  void Insert(const FieldPath& key, const FieldValue& value) {
    container_[key] = value;
  }

  class FieldPathValueMapIterator {
   public:
    bool HasMore() const { return cur_ != end_; }
    void Advance() { ++cur_; }

    const FieldPath& UnsafeKeyView() const { return cur_->first; }
    const FieldValue& UnsafeValueView() const { return cur_->second; }

    FieldPath KeyCopy() const { return cur_->first; }
    FieldValue ValueCopy() const { return cur_->second; }

   private:
    friend class FieldPathValueMap;
    explicit FieldPathValueMapIterator(const FieldPathValueMap& wrapper)
        : cur_{wrapper.Unwrap().begin()}, end_{wrapper.Unwrap().end()} {}

    MapFieldPathValue::const_iterator cur_, end_;
  };

  FieldPathValueMapIterator Iterator() const {
    return FieldPathValueMapIterator(*this);
  }

  static void TransactionUpdate(Transaction* transaction,
                                const DocumentReference& doc,
                                const FieldPathValueMap& wrapper) {
    transaction->Update(doc, wrapper.Unwrap());
  }

  static void WriteBatchUpdate(WriteBatch* batch, const DocumentReference& doc,
                               const FieldPathValueMap& wrapper) {
    batch->Update(doc, wrapper.Unwrap());
  }

  static Future<void> DocumentReferenceUpdate(
      DocumentReference& doc, const FieldPathValueMap& wrapper) {
    return doc.Update(wrapper.Unwrap());
  }

 private:
  const MapFieldPathValue& Unwrap() const { return container_; }

  static const FieldValue& InvalidFieldValue() {
    static FieldValue value;
    return value;
  }

  MapFieldPathValue container_;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_MAP_HELPER_H_
