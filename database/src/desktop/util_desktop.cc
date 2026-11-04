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

#include "database/src/desktop/util_desktop.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/view/indexed_filter.h"
#include "database/src/desktop/view/limited_filter.h"
#include "database/src/desktop/view/ranged_filter.h"
#include "database/src/desktop/view/variant_filter.h"
#include "openssl/sha.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "openssl/base64.h"
#else
#include "openssl/evp.h"
#endif

namespace firebase {
namespace database {
namespace internal {

const Variant kNullVariant = Variant::Null();  // NOLINT
const char kValueKey[] = ".value";
const char kPriorityKey[] = ".priority";

static const char kQueryParamsIndexStartValue[] = "sp";
static const char kQueryParamsIndexStartName[] = "sn";
static const char kQueryParamsIndexEndValue[] = "ep";
static const char kQueryParamsIndexEndName[] = "en";
static const char kQueryParamsLimit[] = "l";
static const char kQueryParamsViewFrom[] = "vf";
static const char kQueryParamsViewFromLeft[] = "l";
static const char kQueryParamsViewFromRight[] = "r";
static const char kQueryParamsIndex[] = "i";
static const char kQueryParamsIndexByValue[] = ".value";
static const char kQueryParamsIndexByKey[] = ".key";

void CombineValueAndPriorityInPlace(Variant* value, const Variant& priority);

bool IsPriorityKey(const std::string& priority_key) {
  return priority_key == kPriorityKey;
}

bool StringStartsWith(const std::string& str, const std::string& prefix) {
  return str.compare(0, prefix.length(), prefix, 0, prefix.length()) == 0;
}

bool PatchVariant(const Variant& patch_data, Variant* out_data) {
  if (!patch_data.is_map() || !out_data->is_map()) return false;
  for (const auto& [key, value] : patch_data.map()) out_data->map()[key] = value;
  return true;
}

static const Variant& VariantGetImmediateChild(const Variant* variant,
                                               const std::string& key) {
  if (IsPriorityKey(key)) return GetVariantPriority(*variant);
  if (VariantIsLeaf(*variant)) return kNullVariant;
  const Variant* result = MapGet(&variant->map(), key);
  return result ? *result : kNullVariant;
}

const Variant& VariantGetChild(const Variant* variant, const Path& path) {
  if (VariantIsLeaf(*variant)) {
    if (path.empty()) return *variant;
    return IsPriorityKey(path.FrontDirectory().str())
               ? GetVariantPriority(*variant)
               : kNullVariant;
  }
  std::string front = path.FrontDirectory().str();
  if (front.empty()) return *variant;
  const Variant& child = VariantGetImmediateChild(variant, front);
  return VariantGetChild(&child, path.PopFrontDirectory());
}

const Variant& VariantGetChild(const Variant* variant, const std::string& key) {
  return VariantGetChild(variant, Path(key));
}

void VariantUpdateChild(Variant* variant, const Path& path,
                        const Variant& value) {
  std::string front = path.FrontDirectory().str();
  if (front.empty()) {
    *variant = value;
    return;
  }
  if (IsPriorityKey(front) && !variant->is_null()) {
    CombineValueAndPriorityInPlace(variant, value);
    return;
  }
  if (VariantIsLeaf(*variant)) {
    if (VariantIsEmpty(value) && !IsPriorityKey(front)) return;
    if (!variant->is_map()) *variant = Variant::EmptyMap();
    variant->map().erase(kValueKey);
  }
  Variant& immediate_child = variant->map()[front];
  VariantUpdateChild(&immediate_child, path.PopFrontDirectory(), value);
  if (VariantIsEmpty(immediate_child)) variant->map().erase(front);
  if (VariantIsEmpty(*variant)) *variant = Variant::Null();
}

void VariantUpdateChild(Variant* variant, const std::string& key,
                        const Variant& value) {
  VariantUpdateChild(variant, Path(key), value);
}

Variant* GetInternalVariant(Variant* variant, const Path& path) {
  Variant* result = variant;
  for (const std::string& directory : path.GetDirectories()) {
    result = GetInternalVariant(result, directory);
    if (result == nullptr) break;
  }
  return result;
}

const Variant* GetInternalVariant(const Variant* variant, const Path& path) {
  return GetInternalVariant(const_cast<Variant*>(variant), path);
}

Variant* GetInternalVariant(Variant* variant, const Variant& key) {
  if (key != Variant::FromStaticString(kPriorityKey)) {
    variant = GetVariantValue(variant);
  }
  return variant->is_map() ? MapGet(&variant->map(), key) : nullptr;
}

const Variant* GetInternalVariant(const Variant* variant, const Variant& key) {
  return GetInternalVariant(const_cast<Variant*>(variant), key);
}

Variant* MakeVariantAtPath(Variant* variant, const Path& path) {
  for (const std::string& directory : path.GetDirectories()) {
    if (!variant->is_map()) *variant = Variant::EmptyMap();
    auto& map = variant->map();
    map.erase(kValueKey);
    variant = &map[directory];
  }
  return variant;
}

void SetVariantAtPath(Variant* variant, const Path& path,
                      const Variant& value) {
  Variant* target = MakeVariantAtPath(variant, path);
  if (target->is_map()) {
    if (value.is_map()) {
      auto& target_map = target->map();
      target_map.erase(kValueKey);
      for (const auto& [k, v] : value.map()) target_map[k] = v;
    } else {
      *GetVariantValue(target) = value;
    }
  } else {
    *target = value;
  }
}

ParseUrl::ParseResult ParseUrl::Parse(const std::string& url) {
  hostname.clear();
  ns.clear();
  secure = true;
  path.clear();
  if (url.empty()) return kParseErrorEmpty;

  size_t protocol_end = url.find("://");
  size_t hostname_start = 0;
  if (protocol_end != std::string::npos) {
    if (protocol_end == 4 && url.compare(0, 4, "http") == 0) {
      hostname_start = 7;
      secure = false;
    } else if (protocol_end == 5 && url.compare(0, 5, "https") == 0) {
      hostname_start = 8;
    } else {
      return kParseErrorUnknownProtocol;
    }
  }

  size_t hostname_end = url.find_first_of('/', hostname_start);
  if (hostname_end == std::string::npos) hostname_end = url.length();
  if (hostname_end == hostname_start) return kParseErrorEmptyHostname;

  hostname = url.substr(hostname_start, hostname_end - hostname_start);
  if ((hostname_end + 1) < url.length()) {
    path = url.substr(hostname_end + 1);
  }

  auto it_seg_start = hostname.begin();
  auto it_port_start = hostname.end();
  for (auto it_char = hostname.begin(); it_char != hostname.end(); ++it_char) {
    if (it_port_start == hostname.end()) {
      if (*it_char == '.' || *it_char == ':') {
        if (it_char == it_seg_start) return kParseErrorEmptySegment;
        if (it_seg_start == hostname.begin()) {
          ns = std::string(it_seg_start, it_char);
        }
        it_seg_start = it_char + 1;
        if (*it_char == ':') it_port_start = it_char + 1;
      } else if (!isalnum(*it_char) && *it_char != '-') {
        return kParseErrorUnsupportedCharacter;
      }
    } else if (!isdigit(*it_char)) {
      return kParseErrorInvalidPort;
    }
  }
  if (it_seg_start == hostname.end()) return kParseErrorEmptySegment;
  if (ns.empty()) return kParseErrorEmptyNamespace;
  return kParseOk;
}

size_t CountEffectiveChildren(const Variant& variant) {
  if (!variant.is_map()) return 0;
  const auto& map = variant.map();
  if (map.find(kValueKey) != map.end()) return 0;
  return map.size() - (map.find(kPriorityKey) != map.end() ? 1 : 0);
}

void PruneNulls(Variant* variant, bool recursive) {
  if (!variant->is_map()) return;
  auto& map = variant->map();
  for (auto iter = map.begin(); iter != map.end();) {
    if (recursive) PruneNulls(&iter->second, true);
    iter = VariantIsEmpty(iter->second) ? map.erase(iter) : std::next(iter);
  }
}

size_t GetEffectiveChildren(const Variant& variant,
                            std::map<Variant, const Variant*>* output) {
  assert(output);
  output->clear();
  if (variant.is_map() && variant.map().find(kValueKey) == variant.map().end()) {
    for (const auto& [k, v] : variant.map()) {
      if (k != kPriorityKey) output->emplace(k, &v);
    }
  }
  return output->size();
}

bool HasVector(const Variant& variant) {
  if (variant.is_vector()) return true;
  if (variant.is_map()) {
    for (const auto& [k, v] : variant.map()) {
      if (HasVector(v)) return true;
    }
  }
  return false;
}

bool ParseInteger(const char* str, int64_t* output) {
  assert(output && str);
  char* end_ptr = nullptr;
  int64_t parse_value = strtoll(str, &end_ptr, 10);  // NOLINT
  if (end_ptr != nullptr && end_ptr != str && *end_ptr == '\0') {
    *output = parse_value;
    return true;
  }
  return false;
}

bool CanConvertVariantMapToVector(const Variant& variant,
                                  int64_t* max_index_out) {
  if (!variant.is_map() || variant.map().empty()) return false;
  int64_t max_index = -1;
  for (const auto& [k, v] : variant.map()) {
    assert(k.is_string());
    const char* s = k.string_value();
    if (s[0] == '0' && s[1] != '\0') return false;
    int64_t parse_value = 0;
    if (!ParseInteger(s, &parse_value) || parse_value < 0) return false;
    max_index = std::max(max_index, parse_value);
  }
  if (max_index_out) *max_index_out = max_index;
  return max_index < static_cast<int64_t>(2 * variant.map().size());
}

void ConvertMapToVector(Variant* variant) {
  assert(variant);
  int64_t max_index = -1;
  if (CanConvertVariantMapToVector(*variant, &max_index)) {
    Variant array_result(std::vector<Variant>(max_index + 1, Variant::Null()));
    for (int64_t i = 0; i <= max_index; ++i) {
      auto it = variant->map().find(std::to_string(i));
      if (it != variant->map().end()) array_result.vector()[i] = it->second;
    }
    *variant = std::move(array_result);
  }
}

void PrunePrioritiesAndConvertVector(Variant* variant) {
  assert(variant && !HasVector(*variant));
  if (variant->is_map()) {
    for (auto& [k, v] : variant->map()) PrunePrioritiesAndConvertVector(&v);
  }
  PrunePriorities(variant, false);
  ConvertMapToVector(variant);
}

void ConvertVectorToMap(Variant* variant) {
  assert(variant);
  if (variant->is_vector()) {
    Variant map = Variant::EmptyMap();
    const auto& vec = variant->vector();
    for (size_t i = 0; i < vec.size(); ++i) {
      if (!vec[i].is_null()) map.map()[std::to_string(i)] = vec[i];
    }
    *variant = std::move(map);
    for (auto& [k, v] : variant->map()) ConvertVectorToMap(&v);
  } else if (variant->is_map()) {
    const Variant* value = GetVariantValue(variant);
    assert(value);
    if (value->is_vector()) {
      Variant priority = GetVariantPriority(*variant);
      Variant new_data = *value;
      ConvertVectorToMap(&new_data);
      new_data.map()[kPriorityKey] = priority;
      *variant = std::move(new_data);
    }
    for (auto& [k, v] : variant->map()) ConvertVectorToMap(&v);
  }
}

void PrunePriorities(Variant* variant, bool recursive) {
  if (!variant->is_map()) return;
  auto* map = &variant->map();
  auto value_iter = map->find(kValueKey);
  if (value_iter != map->end()) {
    Variant temp = std::move(value_iter->second);
    *variant = std::move(temp);
  } else {
    map->erase(kPriorityKey);
  }
  if (recursive && variant->is_map()) {
    for (auto& [k, v] : variant->map()) PrunePriorities(&v, true);
  }
}

const Variant* GetVariantValue(const Variant* variant) {
  if (!variant->is_map()) return variant;
  auto iter = variant->map().find(kValueKey);
  return iter == variant->map().end() ? variant : &iter->second;
}

Variant* GetVariantValue(Variant* variant) {
  return const_cast<Variant*>(
      GetVariantValue(static_cast<const Variant*>(variant)));
}

const Variant& GetVariantPriority(const Variant& variant) {
  if (!variant.is_map()) return kNullVariant;
  auto iter = variant.map().find(kPriorityKey);
  return iter == variant.map().end() ? kNullVariant : iter->second;
}

void CombineValueAndPriorityInPlace(Variant* value, const Variant& priority) {
  if (VariantIsEmpty(*value)) {
    *value = Variant::Null();
  } else if (VariantIsEmpty(priority)) {
    PrunePriorities(value, false);
  } else {
    if (!value->is_map()) {
      *value = Variant(std::map<Variant, Variant>{{kValueKey, *value}});
    }
    value->map()[kPriorityKey] = priority;
  }
}

Variant CombineValueAndPriority(const Variant& value, const Variant& priority) {
  Variant result = value;
  CombineValueAndPriorityInPlace(&result, priority);
  return result;
}

bool VariantIsLeaf(const Variant& variant) {
  return !GetVariantValue(&variant)->is_container_type();
}

bool VariantIsEmpty(const Variant& variant) {
  const Variant* value = GetVariantValue(&variant);
  if (value->is_null()) return true;
  if (value->is_vector()) return value->vector().empty();
  if (value->is_map()) {
    const auto& map = value->map();
    return map.empty() ||
           (map.size() == 1 && !GetVariantPriority(map).is_null());
  }
  return false;
}

bool VariantsAreEquivalent(const Variant& a, const Variant& b) {
  if (QueryParamsComparator::CompareValues(a, b) != 0 ||
      QueryParamsComparator::ComparePriorities(a, b) != 0) {
    return false;
  }
  if (a.is_map() && b.is_map()) {
    if (a.map().size() != b.map().size()) return false;
    for (auto ia = a.map().begin(), ib = b.map().begin(); ia != a.map().end();
         ++ia, ++ib) {
      if (QueryParamsComparator::CompareValues(ia->first, ib->first) != 0 ||
          !VariantsAreEquivalent(ia->second, ib->second)) {
        return false;
      }
    }
  }
  return true;
}

const std::string& GetBase64SHA1(const std::string& input,
                                 std::string* output) {
  assert(output != nullptr);
  uint8_t sha_hash[SHA_DIGEST_LENGTH] = {};
  SHA1(reinterpret_cast<const uint8_t*>(input.c_str()), input.size(), sha_hash);
  uint8_t encode[29];
  EVP_EncodeBlock(encode, sha_hash, SHA_DIGEST_LENGTH);
  *output = reinterpret_cast<char*>(encode);
  return *output;
}

void AppendHashRepAsDouble(std::stringstream* ss, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(value));
  for (int i = 7; i >= 0; i--) {
    uint8_t b = static_cast<uint8_t>((bits >> (8 * i)) & 0xff);
    uint8_t hi = (b >> 4) & 0xf, lo = b & 0xf;
    *ss << static_cast<char>(hi < 10 ? '0' + hi : 'a' + hi - 10)
        << static_cast<char>(lo < 10 ? '0' + lo : 'a' + lo - 10);
  }
}

void AppendHashRepAsFundamental(std::stringstream* ss, const Variant& data) {
  assert(data.is_fundamental_type() && ss != nullptr);
  switch (data.type()) {
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString:
      *ss << "string:" << data.string_value();
      break;
    case Variant::kTypeBool:
      *ss << "boolean:" << (data.bool_value() ? "true" : "false");
      break;
    case Variant::kTypeDouble:
      *ss << "number:";
      AppendHashRepAsDouble(ss, data.double_value());
      break;
    case Variant::kTypeInt64:
      *ss << "number:";
      AppendHashRepAsDouble(ss, static_cast<double>(data.int64_value()));
      break;
    default:
      break;
  }
}

typedef std::pair<const Variant*, const Variant*> NodeSortingData;

int ChildKeyCompareTo(const Variant& left, const Variant& right) {
  const Variant kMinChildKey(QueryParamsComparator::kMinKey);
  const Variant kMaxChildKey(QueryParamsComparator::kMaxKey);
  FIREBASE_DEV_ASSERT(left.is_string() && right.is_string());
  if (left == right) return 0;
  if (left == kMinChildKey || right == kMaxChildKey) return -1;
  if (right == kMinChildKey || left == kMaxChildKey) return 1;
  int64_t left_int = -1, right_int = -1;
  bool left_is_int = ParseInteger(left.string_value(), &left_int);
  bool right_is_int = ParseInteger(right.string_value(), &right_int);
  if (left_is_int && right_is_int) {
    int cmp = static_cast<int>(left_int - right_int);
    return cmp == 0 ? static_cast<int>(strlen(left.string_value()) -
                                       strlen(right.string_value()))
                    : cmp;
  }
  if (left_is_int) return -1;
  if (right_is_int) return 1;
  return left > right ? 1 : -1;
}

void ProcessChildNodes(std::stringstream* ss,
                       std::vector<NodeSortingData>* nodes, bool saw_priority) {
  if (saw_priority) {
    QueryParams params;
    std::sort(nodes->begin(), nodes->end(), QueryParamsLesser(&params));
  } else {
    std::sort(nodes->begin(), nodes->end(),
              [](const NodeSortingData& left, const NodeSortingData& right) {
                return ChildKeyCompareTo(*left.first, *right.first) < 0;
              });
  }
  for (auto& node : *nodes) {
    std::string hash;
    if (!GetHash(*node.second, &hash).empty()) {
      *ss << ':' << node.first->string_value() << ':' << hash;
    }
  }
}

void AppendHashRepAsContainer(std::stringstream* ss, const Variant& data) {
  assert(data.is_container_type() && ss != nullptr);
  std::vector<NodeSortingData> nodes;
  bool saw_priority = false;
  if (data.is_vector()) {
    std::vector<Variant> index_variants;
    index_variants.reserve(data.vector().size());
    for (size_t i = 0; i < data.vector().size(); ++i) {
      index_variants.emplace_back(std::to_string(i));
      nodes.emplace_back(&index_variants[i], &data.vector()[i]);
      saw_priority |= !GetVariantPriority(data.vector()[i]).is_null();
    }
    ProcessChildNodes(ss, &nodes, saw_priority);
  } else if (data.is_map()) {
    for (const auto& [k, v] : data.map()) {
      nodes.emplace_back(&k, &v);
      saw_priority |= !GetVariantPriority(v).is_null();
    }
    ProcessChildNodes(ss, &nodes, saw_priority);
  }
}

void CheckHashRepAsContainer(std::stringstream* ss, const Variant& data) {
  assert(data.is_container_type() && ss != nullptr);
  if (data.is_map()) {
    auto priority_iter = data.map().find(kPriorityKey);
    if (priority_iter != data.map().end()) {
      *ss << "priority:";
      AppendHashRepAsFundamental(ss, priority_iter->second);
      *ss << ":";
      Variant prune = data;
      PrunePriorities(&prune, false);
      if (prune.is_fundamental_type()) {
        AppendHashRepAsFundamental(ss, prune);
      } else {
        AppendHashRepAsContainer(ss, prune);
      }
      return;
    }
  }
  AppendHashRepAsContainer(ss, data);
}

const std::string& GetHashRepresentation(const Variant& data,
                                         std::string* output) {
  assert(output != nullptr);
  std::stringstream ss;
  if (data.is_fundamental_type()) {
    AppendHashRepAsFundamental(&ss, data);
  } else {
    CheckHashRepAsContainer(&ss, data);
  }
  *output = ss.str();
  return *output;
}

const std::string& GetHash(const Variant& data, std::string* output) {
  assert(output != nullptr);
  std::string hash_rep, base64_encoded;
  GetHashRepresentation(data, &hash_rep);
  *output = hash_rep.empty() ? "" : GetBase64SHA1(hash_rep, &base64_encoded);
  return *output;
}

bool IsValidPriority(const Variant& variant) {
  return variant.is_null() || variant.is_numeric() || variant.is_string();
}

std::pair<Variant, Variant> MakePost(const QueryParams& params,
                                     const std::string& name,
                                     const Variant& value) {
  switch (params.order_by) {
    case QueryParams::kOrderByPriority:
      return {name, std::map<Variant, Variant>{{".priority", value}}};
    case QueryParams::kOrderByChild: {
      Variant variant;
      SetVariantAtPath(&variant, Path(params.order_by_child), value);
      return {name, variant};
    }
    case QueryParams::kOrderByKey:
      FIREBASE_DEV_ASSERT(value.is_string());
      return {value.string_value(), Variant::Null()};
    case QueryParams::kOrderByValue:
      return {name, value};
  }
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Invalid QueryParams::OrderBy");
  return {};
}

bool HasStart(const QueryParams& params) {
  return params.start_at_value.has_value() || params.equal_to_value.has_value();
}

bool HasEnd(const QueryParams& params) {
  return params.end_at_value.has_value() || params.equal_to_value.has_value();
}

std::string GetStartName(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasStart(params),
      "Cannot get index start name if start has not been set");
  if (params.start_at_child_key.has_value()) return *params.start_at_child_key;
  if (params.equal_to_child_key.has_value()) return *params.equal_to_child_key;
  return QueryParamsComparator::kMinKey;
}

std::string GetEndName(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasEnd(params), "Cannot get index end name if end has not been set");
  if (params.end_at_child_key.has_value()) return *params.end_at_child_key;
  if (params.equal_to_child_key.has_value()) return *params.equal_to_child_key;
  return QueryParamsComparator::kMaxKey;
}

const Variant& GetStartValue(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasStart(params),
      "Cannot get index start value if start has not been set");
  return params.equal_to_value.has_value() ? *params.equal_to_value
                                           : *params.start_at_value;
}

const Variant& GetEndValue(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasEnd(params), "Cannot get index end value if end has not been set");
  return params.equal_to_value.has_value() ? *params.equal_to_value
                                           : *params.end_at_value;
}

std::pair<Variant, Variant> GetStartPost(const QueryParams& params) {
  return HasStart(params)
             ? MakePost(params, GetStartName(params), GetStartValue(params))
             : QueryParamsComparator::kMinNode;
}

std::pair<Variant, Variant> GetEndPost(const QueryParams& params) {
  return HasEnd(params)
             ? MakePost(params, GetEndName(params), GetEndValue(params))
             : QueryParamsComparator::kMaxNode;
}

bool QuerySpecLoadsAllData(const QuerySpec& query_spec) {
  return QueryParamsLoadsAllData(query_spec.params);
}

bool QueryParamsLoadsAllData(const QueryParams& params) {
  return !params.start_at_value.has_value() &&
         !params.start_at_child_key.has_value() &&
         !params.end_at_value.has_value() &&
         !params.end_at_child_key.has_value() &&
         !params.equal_to_value.has_value() &&
         !params.equal_to_child_key.has_value() && params.limit_first == 0 &&
         params.limit_last == 0;
}

bool QuerySpecIsDefault(const QuerySpec& query_spec) {
  return QueryParamsIsDefault(query_spec.params);
}

bool QueryParamsIsDefault(const QueryParams& params) {
  return QueryParamsLoadsAllData(params) &&
         params.order_by == QueryParams::kOrderByPriority;
}

QuerySpec MakeDefaultQuerySpec(const QuerySpec& query_spec) {
  return QuerySpec(query_spec.path);
}

std::unique_ptr<VariantFilter> VariantFilterFromQueryParams(
    const QueryParams& params) {
  if (QueryParamsLoadsAllData(params)) {
    return std::make_unique<IndexedFilter>(params);
  }
  if (params.limit_first || params.limit_last) {
    return std::make_unique<LimitedFilter>(params);
  }
  return std::make_unique<RangedFilter>(params);
}

std::string WireProtocolPathToString(const Path& path) {
  return path.empty() ? "/" : path.str();
}

Variant GetWireProtocolParams(const QueryParams& query_params) {
  Variant result = Variant::EmptyMap();
  auto& m = result.map();
  if (query_params.start_at_value.has_value()) {
    m[kQueryParamsIndexStartValue] = *query_params.start_at_value;
    if (query_params.start_at_child_key.has_value()) {
      m[kQueryParamsIndexStartName] = *query_params.start_at_child_key;
    }
  }
  if (query_params.end_at_value.has_value()) {
    m[kQueryParamsIndexEndValue] = *query_params.end_at_value;
    if (query_params.end_at_child_key.has_value()) {
      m[kQueryParamsIndexEndName] = *query_params.end_at_child_key;
    }
  }
  if (query_params.equal_to_value.has_value()) {
    m[kQueryParamsIndexStartValue] = *query_params.equal_to_value;
    m[kQueryParamsIndexEndValue] = *query_params.equal_to_value;
    if (query_params.equal_to_child_key.has_value()) {
      m[kQueryParamsIndexStartName] = *query_params.equal_to_child_key;
      m[kQueryParamsIndexEndName] = *query_params.equal_to_child_key;
    }
  }
  if (query_params.limit_first != 0) {
    m[kQueryParamsLimit] = Variant::FromInt64(query_params.limit_first);
    m[kQueryParamsViewFrom] = kQueryParamsViewFromLeft;
  }
  if (query_params.limit_last != 0) {
    m[kQueryParamsLimit] = Variant::FromInt64(query_params.limit_last);
    m[kQueryParamsViewFrom] = kQueryParamsViewFromRight;
  }
  switch (query_params.order_by) {
    case QueryParams::kOrderByKey:
      m[kQueryParamsIndex] = kQueryParamsIndexByKey;
      break;
    case QueryParams::kOrderByValue:
      m[kQueryParamsIndex] = kQueryParamsIndexByValue;
      break;
    case QueryParams::kOrderByChild:
      m[kQueryParamsIndex] =
          WireProtocolPathToString(Path(query_params.order_by_child));
      break;
    default:
      break;
  }
  return result;
}

std::vector<std::string> split_string(const std::string& s,
                                      const char delimiter) {
  std::vector<std::string> split_parts;
  size_t len = s.size(), start = 0;
  while (start < len && s[start] == delimiter) start++;
  while (start < len) {
    size_t pos = s.find(delimiter, start);
    if (pos == std::string::npos) {
      split_parts.push_back(s.substr(start));
      break;
    }
    split_parts.push_back(s.substr(start, pos - start));
    while (pos < len && s[pos] == delimiter) pos++;
    start = pos;
  }
  return split_parts;
}

std::map<Path, Variant> VariantToPathMap(const Variant& data) {
  std::map<Path, Variant> path_map;
  if (data.is_map()) {
    for (const auto& [k, v] : data.map()) {
      path_map.emplace(
          Path(k.is_string() ? k.string_value() : k.AsString().string_value()),
          v);
    }
  }
  return path_map;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
