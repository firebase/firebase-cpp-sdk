#include "firestore/src/ios/collection_reference_ios.h"

#include <future>  // NOLINT(build/c++11)
#include <utility>

#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/document_reference_ios.h"
#include "firestore/src/ios/field_value_ios.h"
#include "firestore/src/ios/user_data_converter_ios.h"
#include "Firestore/core/src/core/user_data.h"

namespace firebase {
namespace firestore {

using core::ParsedSetData;

CollectionReferenceInternal::CollectionReferenceInternal(
    api::CollectionReference&& collection)
    : QueryInternal{std::move(collection)} {}

const api::CollectionReference&
CollectionReferenceInternal::collection_core_api() const {
  // TODO(varconst): this is undefined behavior (mirroring
  // `FIRCollectionReference`). While `api::CollectionReference` and
  // `api::Query` are derived and base classes, respectively, in this case the
  // object being cast is actually a base class object. This only "works"
  // because the derived object happens to have no member variables.
  static_assert(sizeof(api::Query) == sizeof(api::CollectionReference),
                "The query-to-collection casting hack relies on "
                "CollectionReference having no member variables of its own, "
                "see comment for more details");
  return static_cast<const api::CollectionReference&>(query_core_api());
}

const std::string& CollectionReferenceInternal::id() const {
  return collection_core_api().collection_id();
}

std::string CollectionReferenceInternal::path() const {
  return collection_core_api().path();
}

DocumentReference CollectionReferenceInternal::Parent() const {
  absl::optional<api::DocumentReference> parent =
      collection_core_api().parent();
  if (parent) {
    return MakePublic(std::move(*parent));
  } else {
    return DocumentReference{};
  }
}

DocumentReference CollectionReferenceInternal::Document() const {
  return MakePublic(collection_core_api().Document());
}

DocumentReference CollectionReferenceInternal::Document(
    const std::string& document_path) const {
  return MakePublic(collection_core_api().Document(document_path));
}

Future<DocumentReference> CollectionReferenceInternal::Add(
    const MapFieldValue& data) {
  auto promise = promise_factory().CreatePromise<DocumentReference>(
      AsyncApis::kCollectionReferenceAdd);
  ParsedSetData parsed = converter().ParseSetData(data);

  // There is a chicken-and-egg problem here: the callback needs the new
  // document returned by `AddDocument`, but `AddDocument` needs to be given the
  // callback in order to run. To work around it, use a (standard) promise --
  // callback gets the promise of a document, not the document itself.
  std::promise<api::DocumentReference> new_doc_promise;
  std::shared_future<api::DocumentReference> future_doc{
      new_doc_promise.get_future()};
  // TODO(c++14): move the future into lambda, avoiding the need for
  // `shared_future`.
  auto callback = [promise, future_doc](const util::Status& status) mutable {
    if (status.ok()) {
      api::DocumentReference api_doc = future_doc.get();
      promise.SetValue(MakePublic(std::move(api_doc)));
    } else {
      promise.SetError(status);
    }
  };

  api::DocumentReference new_doc =
      collection_core_api().AddDocument(std::move(parsed), std::move(callback));
  new_doc_promise.set_value(std::move(new_doc));

  return promise.future();
}

}  // namespace firestore
}  // namespace firebase
