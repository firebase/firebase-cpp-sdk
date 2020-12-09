// Copyright 2017 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_UTIL_DESKTOP_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_UTIL_DESKTOP_H_

#include <string>

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"

namespace firebase {
namespace database {
namespace internal {

class VariantFilter;

// Just a null variant.
// In many places in the database code, Variants are passed by pointer, and a
// non-existent value might be represented by a nullptr or a pointer to a null
// variant. This leads to this pattern in code in a handful of places:
//
//     Variant null_variant;
//     Variant* variant = ValueThatMightNotExist();
//     if (variant == nullptr) variant = &null_variant;
//
// Rather than construct a null_variant in the current scope any place where
// this happens, it can be helpful to have a persistent one that can be used
// from anywhere.
extern const Variant kNullVariant;

// The virtual key for the value. This has special meaning to the database, and
// for all intents and purposes the value at this location in the map should be
// treated as the value of the variant itself.
extern const char kValueKey[];

// The virtual key for the priority. This has special meaning to the database.
// The value at this location is used to sort the variant when the sorting order
// at this location is set to kOrderByPriority.
extern const char kPriorityKey[];

// Check if the input string is ".priority".
// This is to reduce the number of places to hardcode string ".priority".
bool IsPriorityKey(const std::string& priority_key);

bool StringStartsWith(const std::string& str, const std::string& prefix);

template <typename KeyType, typename ValueType, typename ArgType>
ValueType* MapGet(std::map<KeyType, ValueType>* map, const ArgType& key) {
  auto iter = map->find(key);
  return (iter != map->end()) ? &iter->second : nullptr;
}

template <typename KeyType, typename ValueType, typename ArgType>
const ValueType* MapGet(const std::map<KeyType, ValueType>* map,
                        const ArgType& key) {
  return MapGet(const_cast<std::map<KeyType, ValueType>*>(map), key);
}

// Adds all elements from `extension` to `v`.
template <typename T>
void Extend(std::vector<T>* v, const std::vector<T>& extension) {
  v->insert(v->end(), extension.begin(), extension.end());
}

// Patch one variant onto another. What this means is for any field present in
// the patch_data, overwrite the data in out_data. However, fields in the
// out_data that don't appear in the patch_data should be left undisturbed. If
// either Variant is not a map, this function fails and returns false.
bool PatchVariant(const Variant& patch_data, Variant* out_data);

// Get a child of a given variant, respecting .value and .priorty virtual keys
// appropriately. This will always return some value. If there is a value at the
// given path, a reference to that value will be returned. If there is not, a
// reference to kNullVariant will be returned.
//
// This function is designed to perfectly mimic the behavior of Node.getChild in
// the Java API. This should be used in place of GetInternalVariant, which is
// more naive in how it gets the child variant.
const Variant& VariantGetChild(const Variant* variant, const Path& path);
const Variant& VariantGetChild(const Variant* variant, const std::string& key);

// Update the child of variant at the given path with value. If necessary this
// will convert the given Variant into a map and recursively add child map
// Variants as needed.
//
// This function is designed to perfectly mimic the behavior of Node.updateChild
// in the Java API. This should be used in place of SetVariantAtPath, which is
// more naive in how it updates the child variant, and is not guaranteed to
// update the .priority or .value keys correctly.
void VariantUpdateChild(Variant* variant, const Path& path,
                        const Variant& value);
void VariantUpdateChild(Variant* variant, const std::string& key,
                        const Variant& value);

// Given a root Variant and a Path, get the Variant at that path. This returns a
// pointer to the variant within the given variant (the result will be the same
// as the input if the path is the root). If the path could not be completed for
// whatever reason (key not present, trying to traverse though a non-map) this
// function returns nullptr.
Variant* GetInternalVariant(Variant* variant, const Path& path);
const Variant* GetInternalVariant(const Variant* variant, const Path& path);

// Given a root Variant and a Variant key, get the Variant at that key. This
// returns a pointer to the variant within the given root variant. If the root
// Variant is not a map or does not have a value at the given key, this function
// returns nullptr.
Variant* GetInternalVariant(Variant* variant, const Variant& key);
const Variant* GetInternalVariant(const Variant* variant, const Variant& key);

// Given a Path, get the Variant at that path (or create it if it doesn't
// exist). If this needs to traverse through a Variant that does not represent a
// map, it will be converted into a map and the data at that location will be
// discarded.
Variant* MakeVariantAtPath(Variant* variant, const Path& path);

// Set a value in the variant at the given path, creating intermediate map
// variants as necessary.
//
// TODO(amablue): Remove remaining references to this function.
void SetVariantAtPath(Variant* variant, const Path& path, const Variant& value);

// The Parse() function take the input url and breakdown a url into hostname,
// namespace, secure flag and path.
//
// Ex. https://test.firebaseio.com:443/path/to/key
// --> Hostname:  test.firebaseio.com:443
//     Namespace: test
//     Secure:    true
//     Path:      path/to/key
//
// The Parse() function does some basic validation, mostly for hostname and
// namespace. It does NOT support
// * URL encoding
// * Validation for path
// * Expect no params in the url or they all be part of the path
struct ParseUrl {
  ParseUrl() {}
  enum ParseResult {
    kParseOk = 0,
    kParseErrorEmpty,
    kParseErrorUnknownProtocol,
    kParseErrorEmptyHostname,
    kParseErrorEmptySegment,
    kParseErrorEmptyNamespace,
    kParseErrorInvalidPort,
    kParseErrorUnsupportedCharacter,
  };

  // Parse the input url.  All the class variable are valid only if the parse
  // succeeded (return kParseOk).
  // This function can be reused.
  ParseResult Parse(const std::string& url);

  std::string hostname;
  std::string ns;
  bool secure;
  std::string path;
};

// Count the number of actual children a variant has.  This is complicated by
// the fact that even fundamental types can have children, in the internal
// data representation.  (Because fundamental types with priorities are
// represented as maps with '.value' and '.priority' fields.)
// This function handles that logic, and returns the number of children a
// variant has, as far as the user is concerned.
size_t CountEffectiveChildren(const Variant& variant);

// Similar to CountEffectiveChildren() but fill in all children to the output
// map<key Variant, child Variant pointer>.
// This function returns the number of effective children
size_t GetEffectiveChildren(const Variant& variant,
                            std::map<Variant, const Variant*>* output);

// Check if the variant or any of its children is vector.
bool HasVector(const Variant& variant);

// Parse a base-ten input string into 64-bit integer. Strings are parsed as
// integers if they do not have leading 0's - if they do they are simply treated
// as strings. This is done to match the behavior of the other database
// implementations.
bool ParseInteger(const char* str, int64_t* output);

// Prune the priorities and convert map into vector if applicable, to the
// variant and its children.  This function assumes the variant or its children
// are not vector.  Primarily use when the user calls MutableData::value() or
// DataSnapshot::value()
void PrunePrioritiesAndConvertVector(Variant* variant);

// Convert any vector in the variant or its children to map and keep the
// priority.  Primarily use when the user calls MutableData::SetValue()
void ConvertVectorToMap(Variant* variant);

// Modify a given variant to remove any virtual children named ".priority", and
// if there are any variants that contain ".value" fields, collapse them into
// the Variant itself.
// If recursive is false, it only prunes the ".priority" from the first level.
void PrunePriorities(Variant* variant, bool recursive = true);

// Modify a given variant to remove any null values if it is a map.
// If recursive is false, it only prunes nulls from the first level.
void PruneNulls(Variant* variant, bool recursive = true);

// Returns the Variant representing a database value. Most values the database
// are represented by Variants directly, but if a leaf node has a priority is
// may be represnted by a variant map containing a .value and .priority field.
// If a .value field exist, this function will return the .value field.
// Otherwise, a pointer to the input variant is returned.
const Variant* GetVariantValue(const Variant* variant);
Variant* GetVariantValue(Variant* variant);

// Returns the Variant representing a Priority if one exists. Returns true if a
// priority is found and sets the priorty_out variable. Otherwise returns a
// nullptr.
const Variant& GetVariantPriority(const Variant& variant);

// A function to merge value and priority into one Variant.  This due to the
// nature of ".priority" being inlined in map but not other types.
// If value is null, it returns null regardless of the priority
Variant CombineValueAndPriority(const Variant& value, const Variant& priority);

// Returns true if the given variant represents a leaf node in the database. A
// leaf node is a value that is not a map or vector. Note: A map variant can
// still be considered a leaf node if its .value is not a map. This function
// cares about the variant's value, not the variant itself (although in many
// cases these will be the same thing).
bool VariantIsLeaf(const Variant& variant);

// Returns true if this variant is Null, or if it is a map or vector with no
// elements. False otherwise.
bool VariantIsEmpty(const Variant& variant);

// To property compare samey values, like 0.0 and 0, we need to use the function
// QueryParamsComparator::CompareValues. However, when sorting, this function
// treats all maps as equal which is not what we want when checking for
// equality. We need to check if the maps themselves are actually equal too, so
// this function performs that additional recursive equality check on submaps.
bool VariantsAreEquivalent(const Variant& a, const Variant& b);

// Returns a string which is hashed with SHA-1 and then Base64 encoded
// using the input string.
const std::string& GetBase64SHA1(const std::string& input, std::string* output);

// Compare two Variant as child keys for sorting.
// Expect both Variants are string.
// Returns 0 if both Variants are equal.
// Returns greater than 0 if left is greater than right.
// Returns less than 0 if left is less than right.
// Child key comparison is based on the following rules.
// 1. "[MAX_KEY]" is greater than everything
// 2. "[MIN_KEY]" is less than everything
// 3. Integer key is less than string key
// 4. If both keys are integer and are equal, ex. "1" and "001", one with the
//    shorter length as a string is less than the other.
// 5. Otherwise, compare as a string.
int ChildKeyCompareTo(const Variant& left, const Variant& right);

// Returns serialized string of a Variant to be used for GetHash() function.
// The implementation is based on Node.java and its derived classes from Android
// SDK
const std::string& GetHashRepresentation(const Variant& data,
                                         std::string* output);

// Return a hash string from a Variant used for Transaction.
// The implementation is based on Node.java and its derived classes from Android
// SDK
const std::string& GetHash(const Variant& data, std::string* output);

std::pair<Variant, Variant> MakePost(const QueryParams& params,
                                     const std::string& name,
                                     const Variant& value);

bool IsValidPriority(const Variant& variant);

// Check whether the given params contain either a start_at_value or an
// equal_to_value.
bool HasStart(const QueryParams& params);

// Check whether the given params contain either a end_at_value or an
// equal_to_value.
bool HasEnd(const QueryParams& params);

// Get the lower bound of the key for the earliest element that can appear in
// an IndexedVariant associated with these QueryParams. This will either be the
// start_at_child_key or the equal_to_child_key if either is set.
std::string GetStartName(const QueryParams& params);

// Get the upper bound of the key for the latest element that can appear in
// an IndexedVariant associated with these QueryParams. This will either be the
// end_at_child_key or the equal_to_child_key if either is set.
std::string GetEndName(const QueryParams& params);

// Get the lower bound of the value for the earliest element that can appear in
// an IndexedVariant associated with these QueryParams. This will either be the
// start_at_value or the equal_to_value if either is set.
const Variant& GetStartValue(const QueryParams& params);

// Get the upper bound of the value for the latest element that can appear in
// an IndexedVariant associated with these QueryParams. This will either be the
// end_at_value or the equal_to_value if either is set.
const Variant& GetEndValue(const QueryParams& params);

// Get the earliest key/value pair that can appear in a given IndexedVariant,
// based on the sorting order and range given in the QueryParams.
std::pair<Variant, Variant> GetStartPost(const QueryParams& params);

// Get the latest key/value pair that can appear in a given IndexedVariant,
// based on the sorting order and range given in the QueryParams.
std::pair<Variant, Variant> GetEndPost(const QueryParams& params);

// Returns true if the QuerySpec does no filtering of child data, meaning that
// the data loaded locally under this QuerySpec is a complete view of the data
// and not just a subset.
bool QuerySpecLoadsAllData(const QuerySpec& query_spec);
bool QueryParamsLoadsAllData(const QueryParams& params);

// Returns true if the QuerySpec does no filtering of child data and has the
// default order_by sorting.
bool QuerySpecIsDefault(const QuerySpec& query_spec);
bool QueryParamsIsDefault(const QueryParams& params);

// Converts an existing QuerySpec into a 'default' query spec - one that only
// names a path but does not have any other parameters set on it.
QuerySpec MakeDefaultQuerySpec(const QuerySpec& query_spec);

UniquePtr<VariantFilter> VariantFilterFromQueryParams(
    const QueryParams& params);

// Convert Path which is used in wire protocol to string
std::string WireProtocolPathToString(const Path& path);

// Convert QuerySpec into websocket wire protocol into Variant, which will be
// further converted into Json.
// NOTE: Don't change this unless you're changing the wire protocol!
Variant GetWireProtocolParams(const QueryParams& query_params);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_UTIL_DESKTOP_H_
