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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_UTIL_DESKTOP_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_UTIL_DESKTOP_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"

namespace firebase {
namespace database {
namespace internal {

class VariantFilter;

extern const Variant kNullVariant;
extern const char kValueKey[];
extern const char kPriorityKey[];

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

template <typename T>
void Extend(std::vector<T>* v, const std::vector<T>& extension) {
  v->insert(v->end(), extension.begin(), extension.end());
}

bool PatchVariant(const Variant& patch_data, Variant* out_data);
const Variant& VariantGetChild(const Variant* variant, const Path& path);
const Variant& VariantGetChild(const Variant* variant, const std::string& key);
void VariantUpdateChild(Variant* variant, const Path& path,
                        const Variant& value);
void VariantUpdateChild(Variant* variant, const std::string& key,
                        const Variant& value);
Variant* GetInternalVariant(Variant* variant, const Path& path);
const Variant* GetInternalVariant(const Variant* variant, const Path& path);
Variant* GetInternalVariant(Variant* variant, const Variant& key);
const Variant* GetInternalVariant(const Variant* variant, const Variant& key);
Variant* MakeVariantAtPath(Variant* variant, const Path& path);
void SetVariantAtPath(Variant* variant, const Path& path, const Variant& value);

struct ParseUrl {
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

  ParseResult Parse(const std::string& url);

  std::string hostname;
  std::string ns;
  bool secure;
  std::string path;
};

size_t CountEffectiveChildren(const Variant& variant);
size_t GetEffectiveChildren(const Variant& variant,
                            std::map<Variant, const Variant*>* output);
bool HasVector(const Variant& variant);
bool ParseInteger(const char* str, int64_t* output);
void PrunePrioritiesAndConvertVector(Variant* variant);
void ConvertVectorToMap(Variant* variant);
void PrunePriorities(Variant* variant, bool recursive = true);
void PruneNulls(Variant* variant, bool recursive = true);
const Variant* GetVariantValue(const Variant* variant);
Variant* GetVariantValue(Variant* variant);
const Variant& GetVariantPriority(const Variant& variant);
Variant CombineValueAndPriority(const Variant& value, const Variant& priority);
bool VariantIsLeaf(const Variant& variant);
bool VariantIsEmpty(const Variant& variant);
bool VariantsAreEquivalent(const Variant& a, const Variant& b);
const std::string& GetBase64SHA1(const std::string& input, std::string* output);
int ChildKeyCompareTo(const Variant& left, const Variant& right);
const std::string& GetHashRepresentation(const Variant& data,
                                         std::string* output);
const std::string& GetHash(const Variant& data, std::string* output);
std::pair<Variant, Variant> MakePost(const QueryParams& params,
                                     const std::string& name,
                                     const Variant& value);
bool IsValidPriority(const Variant& variant);
bool HasStart(const QueryParams& params);
bool HasEnd(const QueryParams& params);
std::string GetStartName(const QueryParams& params);
std::string GetEndName(const QueryParams& params);
const Variant& GetStartValue(const QueryParams& params);
const Variant& GetEndValue(const QueryParams& params);
std::pair<Variant, Variant> GetStartPost(const QueryParams& params);
std::pair<Variant, Variant> GetEndPost(const QueryParams& params);
bool QuerySpecLoadsAllData(const QuerySpec& query_spec);
bool QueryParamsLoadsAllData(const QueryParams& params);
bool QuerySpecIsDefault(const QuerySpec& query_spec);
bool QueryParamsIsDefault(const QueryParams& params);
QuerySpec MakeDefaultQuerySpec(const QuerySpec& query_spec);
std::unique_ptr<VariantFilter> VariantFilterFromQueryParams(
    const QueryParams& params);
std::string WireProtocolPathToString(const Path& path);
Variant GetWireProtocolParams(const QueryParams& query_params);
std::string GetAppDataPath(const char* app_name, bool should_create = true);
std::vector<std::string> split_string(const std::string& s,
                                      const char delimiter = '/');
std::map<Path, Variant> VariantToPathMap(const Variant& data);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_UTIL_DESKTOP_H_
