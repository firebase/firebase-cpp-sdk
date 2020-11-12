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
#include <cstdlib>
#include <iostream>
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

// If building against OpenSSL, use the evp header for base64 encoding logic,
// otherwise assume to be building against BoringSSL, which uses a different
// header file. Note that this is purposefully after including a header shared
// across both implementations, which will make this be defined.
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

// Wire protocol keys/values for QueryParams
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
  if (!patch_data.is_map() || !out_data->is_map()) {
    return false;
  }
  for (const auto& key_value : patch_data.map()) {
    const auto& key = key_value.first;
    const auto& value = key_value.second;
    out_data->map()[key] = value;
  }
  return true;
}

static const Variant& VariantGetImmediateChild(const Variant* variant,
                                               const std::string& key) {
  if (VariantIsLeaf(*variant)) {
    if (IsPriorityKey(key)) {
      return GetVariantPriority(*variant);
    } else {
      return kNullVariant;
    }
  } else {
    if (IsPriorityKey(key)) {
      return GetVariantPriority(*variant);
    } else {
      const Variant* result = MapGet(&variant->map(), key);
      return result ? *result : kNullVariant;
    }
  }
}

const Variant& VariantGetChild(const Variant* variant, const Path& path) {
  if (VariantIsLeaf(*variant)) {
    if (path.empty()) {
      return *variant;
    } else if (IsPriorityKey(path.FrontDirectory().str())) {
      return GetVariantPriority(*variant);
    } else {
      return kNullVariant;
    }
  } else {
    std::string front = path.FrontDirectory().str();
    if (front.empty()) {
      return *variant;
    } else {
      const Variant& child = VariantGetImmediateChild(variant, front);
      return VariantGetChild(&child, path.PopFrontDirectory());
    }
  }
}

const Variant& VariantGetChild(const Variant* variant, const std::string& key) {
  return VariantGetChild(variant, Path(key));
}

void VariantUpdateChild(Variant* variant, const Path& path,
                        const Variant& value) {
  std::string front = path.FrontDirectory().str();
  if (front.empty()) {
    *variant = value;
  } else if (variant->is_null()) {
    *variant = Variant::EmptyMap();
    Variant& immediate_child = variant->map()[front];
    VariantUpdateChild(&immediate_child, path.PopFrontDirectory(), value);
    if (VariantIsEmpty(immediate_child)) {
      variant->map().erase(variant->map().find(front));
    }
    if (VariantIsEmpty(*variant)) {
      *variant = Variant::Null();
    }
  } else if (VariantIsLeaf(*variant)) {
    std::string front = path.FrontDirectory().str();
    if (VariantIsEmpty(value) && !IsPriorityKey(front)) {
      // Do nothing.
    } else if (IsPriorityKey(front)) {
      CombineValueAndPriorityInPlace(variant, value);
    } else {
      if (!variant->is_map()) {
        *variant = Variant::EmptyMap();
      }
      auto dot_value_iter = variant->map().find(kValueKey);
      if (dot_value_iter != variant->map().end()) {
        variant->map().erase(dot_value_iter);
      }
      Variant& immediate_child = variant->map()[front];
      VariantUpdateChild(&immediate_child, path.PopFrontDirectory(), value);
      if (VariantIsEmpty(immediate_child)) {
        variant->map().erase(variant->map().find(front));
      }
      if (VariantIsEmpty(*variant)) {
        *variant = Variant::Null();
      }
    }
  } else {
    if (IsPriorityKey(front)) {
      CombineValueAndPriorityInPlace(variant, value);
    } else {
      Variant& immediate_child = variant->map()[front];
      VariantUpdateChild(&immediate_child, path.PopFrontDirectory(), value);
      if (VariantIsEmpty(immediate_child)) {
        variant->map().erase(variant->map().find(front));
      }
      if (VariantIsEmpty(*variant)) {
        *variant = Variant::Null();
      }
    }
  }
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
  // Ensure we're operating on a map.
  if (!variant->is_map()) {
    return nullptr;
  }
  // Get the child Variant at the given path.
  return MapGet(&variant->map(), key);
}

const Variant* GetInternalVariant(const Variant* variant, const Variant& key) {
  return GetInternalVariant(const_cast<Variant*>(variant), key);
}

Variant* MakeVariantAtPath(Variant* variant, const Path& path) {
  for (const std::string& directory : path.GetDirectories()) {
    // Ensure we're operating on a map.
    if (!variant->is_map()) *variant = Variant::EmptyMap();

    // Get the child Variant at the given path.
    auto& map = variant->map();

    // If there was a .value key, remove it as it is no longer valid.
    map.erase(kValueKey);

    // Create the new map if necessary.
    auto iter = map.find(directory);
    if (iter == map.end()) {
      auto insertion = map.insert(std::make_pair(directory, Variant::Null()));
      iter = insertion.first;
      bool success = insertion.second;
      assert(success);
      (void)success;
    }
    // Prepare the next iteration.
    variant = &iter->second;
  }
  return variant;
}

void SetVariantAtPath(Variant* variant, const Path& path,
                      const Variant& value) {
  Variant* target = MakeVariantAtPath(variant, path);
  if (target->is_map()) {
    // Get the child Variant at the given path.
    auto& target_map = target->map();

    if (value.is_map()) {
      // If there was a .value key, remove it as it is no longer valid.
      target_map.erase(kValueKey);

      // Fill in the new values.
      for (auto& kvp : value.map()) {
        target_map[kvp.first] = kvp.second;
      }
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

  if (url.empty()) {
    return kParseErrorEmpty;
  }

  // Find the protocol.  It is ok to not specify any protocol.  If not, it
  // defaults to a secured connection.
  int protocol_end = url.find("://");
  int hostname_start = 0;

  if (protocol_end != std::string::npos) {
    if (protocol_end == 4 && url.compare(0, protocol_end, "http") == 0) {
      hostname_start = 7;  // "http://"
      secure = false;
    } else if (protocol_end == 5 &&
               url.compare(0, protocol_end, "https") == 0) {
      hostname_start = 8;  // "https://"
    } else {
      return kParseErrorUnknownProtocol;
    }
  }

  // hostname_end is the index of the first '/' or the length of the hostname if
  // no '/' is found
  int hostname_end = url.find_first_of('/', hostname_start);
  if (hostname_end == std::string::npos) {
    // Hostname does not end with '/'
    hostname_end = url.length();
  }

  if (hostname_end == hostname_start) {
    return kParseErrorEmptyHostname;
  }

  hostname = url.substr(hostname_start, hostname_end - hostname_start);

  if ((hostname_end + 1) < url.length()) {
    path = url.substr(hostname_end + 1, url.length() - hostname_end - 1);
  }

  // Starting position of current segment, which is separated by '.'
  auto it_seg_start = hostname.begin();

  // Starting position of port number after the first ':'
  // Port number is optional.
  auto it_port_start = hostname.end();

  for (auto it_char = hostname.begin(); it_char != hostname.end(); ++it_char) {
    if (it_port_start == hostname.end()) {
      // Parsing non-port section
      if (*it_char == '.' || *it_char == ':') {
        if (it_char == it_seg_start) {
          return kParseErrorEmptySegment;
        } else {
          // If this is the end of the first segment, this segment should be the
          // namespace
          if (it_seg_start == hostname.begin()) {
            ns = std::string(it_seg_start, it_char);
          }

          it_seg_start = it_char + 1;

          // Start port parsing
          if (*it_char == ':') {
            it_port_start = it_char + 1;
          }
        }
      } else if (!isalnum(*it_char) && *it_char != '-') {
        // unsupported character
        return kParseErrorUnsupportedCharacter;
      }
    } else {
      // parsing the port section
      if (!isdigit(*it_char)) {
        return kParseErrorInvalidPort;
      }
    }
  }

  // Check the last segment
  if (it_seg_start == hostname.end()) {
    return kParseErrorEmptySegment;
  }

  if (ns.empty()) {
    return kParseErrorEmptyNamespace;
  }

  return kParseOk;
}

// Returns the number of children of a variant, not including special fields
// such as .priority or .value.  Uses similar logic to PrunePriorities.
size_t CountEffectiveChildren(const Variant& variant) {
  if (variant.is_map()) {
    auto* map = &variant.map();
    if (map->find(kValueKey) != map->end()) {
      // This is a fundamental type, with a priority.   No children!
      return 0;
    } else {
      bool has_priority = map->find(kPriorityKey) != map->end();

      // This is a basic map.  It might still have priority though,
      // so we need to remove those.
      return map->size() - (has_priority ? 1 : 0);
    }
  }
  // If we got here, this is a fundamental type without .priority.  No children!
  return 0;
}

void PruneNulls(Variant* variant, bool recursive) {
  if (!variant->is_map()) {
    return;
  }
  auto& map = variant->map();
  for (auto iter = map.begin(); iter != map.end();) {
    if (recursive) {
      PruneNulls(&iter->second, true);
    }
    if (VariantIsEmpty(iter->second)) {
      iter = map.erase(iter);
    } else {
      ++iter;
    }
  }
}

size_t GetEffectiveChildren(const Variant& variant,
                            std::map<Variant, const Variant*>* output) {
  assert(output);

  output->clear();

  if (variant.is_map()) {
    auto* map = &variant.map();
    // If the map has ".value", this is a fundamental type with a priority.
    // i.e. no children
    if (map->find(kValueKey) == map->end()) {
      auto it_priority = map->find(kPriorityKey);

      for (auto it_child = map->begin(); it_child != map->end(); ++it_child) {
        if (it_child != it_priority) {
          output->insert(std::make_pair(it_child->first, &it_child->second));
        }
      }
    }
  }

  return output->size();
}

bool HasVector(const Variant& variant) {
  if (variant.is_vector()) {
    return true;
  } else if (variant.is_map()) {
    for (auto& it_child : variant.map()) {
      if (HasVector(it_child.second)) {
        return true;
      }
    }
  }
  return false;
}

bool ParseInteger(const char* str, int64_t* output) {
  assert(output);
  assert(str);
  // Check if the key is numeric
  bool is_int = false;
  char* end_ptr = nullptr;
  int64_t parse_value = strtoll(str, &end_ptr, 10);  // NOLINT
  if (end_ptr != nullptr && end_ptr != str && *end_ptr == '\0') {
    is_int = true;
    *output = parse_value;
  }
  return is_int;
}

// A Variant map can be converted into a Variant vector if:
//   1. map is not empty and
//   2. All the key are integer (no leading 0) and
//   3. If less or equal to half of the keys in the array is missing.
// Return whether the map can be converted into a vector, and output
// max_index_out as the highest numeric key found in the map.
bool CanConvertVariantMapToVector(const Variant& variant,
                                  int64_t* max_index_out) {
  if (!variant.is_map() || variant.map().empty()) return false;

  int64_t max_index = -1;
  for (auto& it_child : variant.map()) {
    assert(it_child.first.is_string());
    // Integers must not have leading zeroes.
    if (it_child.first.string_value()[0] == '0' &&
        it_child.first.string_value()[1] != '\0') {
      return false;
    }
    // Check if the key is numeric
    int64_t parse_value = 0;
    bool is_number = ParseInteger(it_child.first.string_value(), &parse_value);
    if (!is_number || parse_value < 0) {
      // If any one of the key is not numeric, there is no need to verify
      // other keys
      return false;
    }
    max_index = max_index < parse_value ? parse_value : max_index;
  }

  if (max_index_out) *max_index_out = max_index;
  return max_index < (2 * variant.map().size());
}

// Convert one level of map to vector if applicable.
// This function assume no priority information remains in the variant.
void ConvertMapToVector(Variant* variant) {
  assert(variant);

  int64_t max_index = -1;
  if (CanConvertVariantMapToVector(*variant, &max_index)) {
    Variant array_result(std::vector<Variant>(max_index + 1, Variant::Null()));
    for (int i = 0; i <= max_index; ++i) {
      std::stringstream ss;
      ss << i;
      auto it_child = variant->map().find(ss.str());
      if (it_child != variant->map().end()) {
        array_result.vector()[i] = it_child->second;
      }
    }
    *variant = array_result;
  }
}

void PrunePrioritiesAndConvertVector(Variant* variant) {
  assert(variant);
  assert(!HasVector(*variant));

  // Recursively process child value first since the map can be converted to
  // vector later.
  if (variant->is_map() && !variant->map().empty()) {
    for (auto& it_child : variant->map()) {
      PrunePrioritiesAndConvertVector(&it_child.second);
    }
  }

  PrunePriorities(variant, false);

  ConvertMapToVector(variant);
}

void ConvertVectorToMap(Variant* variant) {
  assert(variant);

  if (variant->is_vector()) {
    // If the variant is a vector, convert into map.
    // Ex. [null,1,2,null,4] => {"1":1,"2":2,"4":4}
    Variant map = Variant::EmptyMap();
    auto& vector = variant->vector();
    for (int i = 0; i < vector.size(); ++i) {
      if (vector[i] != Variant::Null()) {
        std::stringstream ss;
        ss << i;
        map.map()[ss.str()] = vector[i];
      }
    }
    *variant = map;

    // Recursively convert children
    for (auto& it_child : variant->map()) {
      ConvertVectorToMap(&it_child.second);
    }
  } else if (variant->is_map()) {
    const Variant* value = GetVariantValue(variant);
    assert(value);
    const Variant& priority = GetVariantPriority(*variant);

    // Handle the case like
    //   {".value":[0,1],".priority":1} => {"0":0,"1":1,".priority":1}
    // Surprisingly the other SDK supports such case
    if (value->is_vector()) {
      // If the value is vector, it is impossible that priority is nullptr
      Variant new_data = *value;
      ConvertVectorToMap(&new_data);
      assert(new_data.is_map());
      new_data.map()[kPriorityKey] = priority;
      *variant = new_data;
    }

    // Recursively convert children.  It is fine to include priority here
    for (auto& it_child : variant->map()) {
      ConvertVectorToMap(&it_child.second);
    }
  }
}

void PrunePriorities(Variant* variant, bool recursive /* = true */) {
  // There are three possible cases:
  //
  //  1. This is a map reprenting a fundamental type that contains a value and
  //     priority field. Set the whole variant to be the value (which
  //     implicitly removes the priority)
  //  2. This is a map that contains values as well as a priority. Remove the
  //     priority field.
  //  3. This is a plain value. Do nothing.
  if (variant->is_map()) {
    auto* map = &variant->map();
    auto value_iter = map->find(kValueKey);
    auto priority_iter = map->find(kPriorityKey);
    if (value_iter != map->end()) {  // Checking for case 1.
      // This needs to be done in two steps. If you try to do
      //   *variant = std::move(value_iter->second);
      // then while updating the type of the target variant, it will delete the
      // very data you are trying to put into it since it's a child.
      Variant temp = std::move(value_iter->second);
      *variant = std::move(temp);
    } else if (priority_iter != map->end()) {  // Checking for case 2.
      map->erase(priority_iter);
    }

    // Repeat recursively over any elements in the map.
    // Note that the map might have changed, so we need to get it again.
    if (recursive && variant->is_map()) {
      map = &variant->map();
      for (auto& key_value : *map) {
        PrunePriorities(&key_value.second, recursive);
      }
    }
  }
  // If this isn't a map, then we do nothing.
}

const Variant* GetVariantValue(const Variant* variant) {
  if (!variant->is_map()) {
    return variant;
  }
  auto iter = variant->map().find(kValueKey);
  if (iter == variant->map().end()) {
    return variant;
  }
  return &iter->second;
}

Variant* GetVariantValue(Variant* variant) {
  if (!variant->is_map()) {
    return variant;
  }
  auto iter = variant->map().find(kValueKey);
  if (iter == variant->map().end()) {
    return variant;
  }
  return &iter->second;
}

const Variant& GetVariantPriority(const Variant& variant) {
  if (!variant.is_map()) {
    return kNullVariant;
  }
  auto iter = variant.map().find(kPriorityKey);
  if (iter == variant.map().end()) {
    return kNullVariant;
  }
  return iter->second;
}

void CombineValueAndPriorityInPlace(Variant* value, const Variant& priority) {
  // If the value is already null, return null regardless of the priority
  // If the priority is null, only the value
  if (VariantIsEmpty(*value)) {
    *value = Variant::Null();
  } else if (VariantIsEmpty(priority)) {
    PrunePriorities(value, false);
  } else {
    if (!value->is_map()) {
      // If the value is not a map, ex. int, double or vector, create a map to
      // wrap the value under ".value" key
      *value = Variant(std::map<Variant, Variant>{
          std::make_pair(kValueKey, *value),
      });
    }
    value->map()[kPriorityKey] = priority;
  }
}

Variant CombineValueAndPriority(const Variant& value, const Variant& priority) {
  Variant result;

  // If the value is already null, return null regardless of the priority
  // If the priority is null, only the value
  if (VariantIsEmpty(value) || VariantIsEmpty(priority)) {
    // If we are operating on a map, remove the priority entry.
    result = value;
    PrunePriorities(&result, false);
  } else {
    if (value.is_map()) {
      // If the value is a map, just inline priority later
      result = value;
    } else {
      // If the value is not a map, ex. int, double or vector, create a map to
      // wrap the value under ".value" key
      result = Variant::EmptyMap();
      result.map()[kValueKey] = value;
    }
    result.map()[kPriorityKey] = priority;
  }
  return result;
}

bool VariantIsLeaf(const Variant& variant) {
  const Variant* value = GetVariantValue(&variant);
  return !value->is_container_type();
}

bool VariantIsEmpty(const Variant& variant) {
  const Variant* value = GetVariantValue(&variant);
  if (value->is_null()) return true;
  if (value->is_vector()) return value->vector().empty();
  if (value->is_map()) {
    const std::map<Variant, Variant>& map = value->map();
    if (map.empty()) return true;
    // If there's only one element and it's the priority then this is
    // effectively an empty map.
    if (map.size() == 1 && !GetVariantPriority(value->map()).is_null()) {
      return true;
    }
  }
  return false;
}

bool VariantsAreEquivalent(const Variant& a, const Variant& b) {
  if (QueryParamsComparator::CompareValues(a, b) != 0 ||
      QueryParamsComparator::ComparePriorities(a, b) != 0) {
    return false;
  }
  if (a.is_map() && b.is_map()) {
    auto iter_a = a.map().begin();
    auto iter_b = b.map().begin();
    for (; iter_a != a.map().end() && iter_b != b.map().end();
         ++iter_a, ++iter_b) {
      const auto& key_a = iter_a->first;
      const auto& key_b = iter_b->first;
      if (QueryParamsComparator::CompareValues(key_a, key_b) != 0) {
        return false;
      }
      const auto& value_a = iter_a->second;
      const auto& value_b = iter_b->second;

      if (!VariantsAreEquivalent(value_a, value_b)) {
        return false;
      }
    }
    // Ensure both map iterators reached their respective ends.
    return (iter_a == a.map().end()) && (iter_b == b.map().end());
  }
  return true;
}

size_t GetBase64Length(size_t len) {
  // Based on the OpenSSL documentation, for every 3 bytes of input provided,
  // 4 bytes will be produced. If len is not divisible by 3, then it will be
  // padded to be divisible by 3. Finally, a NUL character will be added.
  return 1 + 4 * ((len + 2) / 3);
}

const std::string& GetBase64SHA1(const std::string& input,
                                 std::string* output) {
  assert(output != nullptr);

  uint8_t sha_hash[SHA_DIGEST_LENGTH];
  memset(sha_hash, 0x0, SHA_DIGEST_LENGTH);
  SHA1(reinterpret_cast<const uint8_t*>(input.c_str()), input.size(), sha_hash);

  // The length of base64 encoded SHA-1 string is always 29, including '\0',
  // assuming SHA_DIGEST_LENGTH is 20
  assert(GetBase64Length(SHA_DIGEST_LENGTH) == 29);
  uint8_t encode[29];
  EVP_EncodeBlock(encode, sha_hash, SHA_DIGEST_LENGTH);
  *output = reinterpret_cast<char*>(encode);
  return *output;
}

void AppendHashRepAsDouble(std::stringstream* ss, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(value));

  // We use big-endian to encode the bytes
  for (int i = 7; i >= 0; i--) {
    uint8_t byteValue = static_cast<uint8_t>((bits >> (8 * i)) & 0xff);
    uint8_t high = ((byteValue >> 4) & 0xf);
    uint8_t low = (byteValue & 0xf);
    *ss << static_cast<char>(high < 10 ? '0' + high : 'a' + high - 10);
    *ss << static_cast<char>(low < 10 ? '0' + low : 'a' + low - 10);
  }
}

// Private function to serialize a fundamental typed Variant to a hash
// representation format.
void AppendHashRepAsFundamental(std::stringstream* ss, const Variant& data) {
  assert(data.is_fundamental_type());
  assert(ss != nullptr);

  switch (data.type()) {
    case Variant::kTypeNull:
      // Empty
      break;
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      *ss << "string:";
      // Note: Use HashVersion.V1 since ChildrenNode only support V1
      //       HashVersion.V2 would convert '\\' to "\\\\" and '"' to "\\\""
      //       and is used for CompoundHash
      *ss << data.string_value();
    } break;
    case Variant::kTypeBool:
      *ss << "boolean:" << (data.bool_value() ? "true" : "false");
      break;
    case Variant::kTypeDouble:
      *ss << "number:";
      AppendHashRepAsDouble(ss, data.double_value());
      break;
    case Variant::kTypeInt64:
      *ss << "number:";
      // This conversion is agreed in all platforms, including the server
      AppendHashRepAsDouble(ss, static_cast<double>(data.int64_value()));
      break;
    default:
      break;
  }
}

// LeafType enum used for sorting purpose.  This is the same to
// LeafNode.LeafType in Android SDK
// Boolean < Number < String
enum UtilLeafType {
  kUtilLeafTypeOther = 0,
  kUtilLeafTypeBoolean,
  kUtilLeafTypeNumber,
  kUtilLeafTypeString
};

// Private function to convert Variant type to LeafType for sorting
UtilLeafType GetLeafType(Variant::Type type) {
  UtilLeafType leaf_type;

  switch (type) {
    case Variant::kTypeBool:
      leaf_type = kUtilLeafTypeBoolean;
      break;
    case Variant::kTypeDouble:
    case Variant::kTypeInt64:
      leaf_type = kUtilLeafTypeNumber;
      break;
    case Variant::kTypeMutableString:
    case Variant::kTypeStaticString:
      leaf_type = kUtilLeafTypeString;
      break;
    default:
      leaf_type = kUtilLeafTypeOther;
      break;
  }
  // Leaf node should not be any other types.
  assert(leaf_type != kUtilLeafTypeOther);
  return leaf_type;
}

// Store the pointers to the key and the value Variant in a map for sorting
typedef std::pair<const Variant*, const Variant*> NodeSortingData;

int ChildKeyCompareTo(const Variant& left, const Variant& right) {
  const Variant kMinChildKey(QueryParamsComparator::kMinKey);
  const Variant kMaxChildKey(QueryParamsComparator::kMaxKey);

  FIREBASE_DEV_ASSERT(left.is_string());
  FIREBASE_DEV_ASSERT(right.is_string());

  if (left == right) {
    return 0;
  } else if (left == kMinChildKey || right == kMaxChildKey) {
    return -1;
  } else if (right == kMinChildKey || left == kMaxChildKey) {
    return 1;
  } else {
    int64_t left_int_key = -1;
    bool left_is_int = ParseInteger(left.string_value(), &left_int_key);
    int64_t right_int_key = -1;
    bool right_is_int = ParseInteger(right.string_value(), &right_int_key);
    if (left_is_int) {
      if (right_is_int) {
        int cmp = left_int_key - right_int_key;
        return cmp == 0 ? (strlen(left.string_value()) -
                           strlen(right.string_value()))
                        : cmp;
      } else {
        return -1;
      }
    } else if (right_is_int) {
      return 1;
    } else {
      return left > right ? 1 : -1;
    }
  }
}

// Private function to serialize all child nodes
void ProcessChildNodes(std::stringstream* ss,
                       std::vector<NodeSortingData>* nodes, bool saw_priority) {
  // If any node has priority, sort using priority.
  if (saw_priority) {
    QueryParams params;
    assert(params.order_by == QueryParams::kOrderByPriority);
    std::sort(nodes->begin(), nodes->end(), QueryParamsLesser(&params));
  } else {
    // Otherwise, use default sorting function.
    std::sort(nodes->begin(), nodes->end(),
              [](const NodeSortingData& left, const NodeSortingData& right) {
                return ChildKeyCompareTo(*left.first, *right.first) < 0;
              });
  }

  // Serialize each child with its key and its hashed value
  for (auto& node : *nodes) {
    std::string hash;
    if (!GetHash(*node.second, &hash).empty()) {
      *ss << ':' << node.first->string_value() << ':' << hash;
    }
  }
}

// Private function to process node with children, such as map and list
void AppendHashRepAsContainer(std::stringstream* ss, const Variant& data) {
  assert(data.is_container_type());
  assert(ss != nullptr);

  std::vector<NodeSortingData> nodes;

  if (data.is_vector()) {
    // Store index as string to be used for hash calculating, since
    // NodeSortingData stores the pointer to the Variant.
    // This is to avoid making copies of Variant from data.
    std::vector<Variant> index_variants;
    index_variants.reserve(data.vector().size());
    bool saw_priority = false;
    for (int i = 0; i < data.vector().size(); ++i) {
      std::stringstream index_stream;
      index_stream << i;
      index_variants.push_back(index_stream.str());
      nodes.push_back(NodeSortingData(&index_variants[i], &data.vector()[i]));
      saw_priority =
          saw_priority || !GetVariantPriority(data.vector()[i]).is_null();
    }
    ProcessChildNodes(ss, &nodes, saw_priority);
  } else if (data.is_map()) {
    bool saw_priority = false;
    for (auto& it_child : data.map()) {
      nodes.push_back(NodeSortingData(&it_child.first, &it_child.second));
      saw_priority =
          saw_priority || !GetVariantPriority(it_child.second).is_null();
    }
    ProcessChildNodes(ss, &nodes, saw_priority);
  }
}

// Private function to determine if the container typed Variant actually has
// children nodes or just a LeafNode with priority.
// If a map typed Variant contains ".priority", serialize the priority first.
void CheckHashRepAsContainer(std::stringstream* ss, const Variant& data) {
  assert(data.is_container_type());
  assert(ss != nullptr);
  if (data.is_map()) {
    auto& map = data.map();
    auto priority_iter = map.find(kPriorityKey);
    if (priority_iter != map.end()) {
      auto& priority = priority_iter->second;
      assert(priority.is_fundamental_type());
      *ss << "priority:";
      AppendHashRepAsFundamental(ss, priority);
      *ss << ":";

      // Determine if this Variant just a LeafNode with priority.
      Variant prune = data;
      PrunePriorities(&prune, false);
      if (prune.is_fundamental_type()) {
        AppendHashRepAsFundamental(ss, prune);
      } else {
        AppendHashRepAsContainer(ss, prune);
      }
    } else {
      AppendHashRepAsContainer(ss, data);
    }
  } else {
    AppendHashRepAsContainer(ss, data);
  }
}

const std::string& GetHashRepresentation(const Variant& data,
                                         std::string* output) {
  assert(output != nullptr);
  assert(data.is_container_type() || data.is_fundamental_type());

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

  std::string hash_rep;
  GetHashRepresentation(data, &hash_rep);
  std::string base64_encoded;

  *output = hash_rep.empty() ? "" : GetBase64SHA1(hash_rep, &base64_encoded);
  return *output;
}

bool IsValidPriority(const Variant& variant) {
  return variant.is_numeric() || variant.is_string();
}

std::pair<Variant, Variant> MakePost(const QueryParams& params,
                                     const std::string& name,
                                     const Variant& value) {
  switch (params.order_by) {
    case QueryParams::kOrderByPriority: {
      return std::make_pair(name, std::map<Variant, Variant>{
                                      std::make_pair(".priority", value),
                                  });
    }
    case QueryParams::kOrderByChild: {
      Variant variant;
      SetVariantAtPath(&variant, Path(params.order_by_child), value);
      return std::make_pair(name, variant);
    }
    case QueryParams::kOrderByKey: {
      FIREBASE_DEV_ASSERT(value.is_string());
      // We just use empty node, but it'll never be compared, since our
      // comparator only looks at name.
      return std::make_pair(value.string_value(), Variant::Null());
    }
    case QueryParams::kOrderByValue: {
      return std::make_pair(name, value);
    }
  }
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Invalid QueryParams::OrderBy");
  return std::pair<Variant, Variant>();
}

bool HasStart(const QueryParams& params) {
  return !params.start_at_value.is_null() || !params.equal_to_value.is_null();
}

bool HasEnd(const QueryParams& params) {
  return !params.end_at_value.is_null() || !params.equal_to_value.is_null();
}

std::string GetStartName(const QueryParams& params) {
  if (!params.start_at_child_key.empty()) {
    return params.start_at_child_key;
  } else if (!params.equal_to_child_key.empty()) {
    return params.equal_to_child_key;
  } else {
    return QueryParamsComparator::kMinKey;
  }
}

std::string GetEndName(const QueryParams& params) {
  if (!params.end_at_child_key.empty()) {
    return params.end_at_child_key;
  } else if (!params.equal_to_child_key.empty()) {
    return params.equal_to_child_key;
  } else {
    return QueryParamsComparator::kMaxKey;
  }
}

const Variant& GetStartValue(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasStart(params),
      "Cannot get index start value if start has not been set");
  return params.equal_to_value.is_null() ? params.start_at_value
                                         : params.equal_to_value;
}

const Variant& GetEndValue(const QueryParams& params) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      HasEnd(params), "Cannot get index end value if end has not been set");
  return params.equal_to_value.is_null() ? params.end_at_value
                                         : params.equal_to_value;
}

std::pair<Variant, Variant> GetStartPost(const QueryParams& params) {
  if (HasStart(params)) {
    return MakePost(params, GetStartName(params), GetStartValue(params));
  } else {
    return QueryParamsComparator::kMinNode;
  }
}

std::pair<Variant, Variant> GetEndPost(const QueryParams& params) {
  if (HasEnd(params)) {
    return MakePost(params, GetEndName(params), GetEndValue(params));
  } else {
    return QueryParamsComparator::kMaxNode;
  }
}

bool QuerySpecLoadsAllData(const QuerySpec& query_spec) {
  return QueryParamsLoadsAllData(query_spec.params);
}

bool QueryParamsLoadsAllData(const QueryParams& params) {
  return params.start_at_value.is_null() && params.start_at_child_key.empty() &&
         params.end_at_value.is_null() && params.end_at_child_key.empty() &&
         params.equal_to_value.is_null() && params.equal_to_child_key.empty() &&
         params.limit_first == 0 && params.limit_last == 0;
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

UniquePtr<VariantFilter> VariantFilterFromQueryParams(
    const QueryParams& params) {
  if (QueryParamsLoadsAllData(params)) {
    return MakeUnique<IndexedFilter>(params);
  } else if (params.limit_first || params.limit_last) {
    return MakeUnique<LimitedFilter>(params);
  } else {
    return MakeUnique<RangedFilter>(params);
  }
}

std::string WireProtocolPathToString(const Path& path) {
  if (path.empty()) {
    return "/";
  } else {
    return path.str();
  }
}

Variant GetWireProtocolParams(const QueryParams& query_params) {
  Variant result = Variant::EmptyMap();

  if (!query_params.start_at_value.is_null()) {
    result.map()[kQueryParamsIndexStartValue] = query_params.start_at_value;
    if (!query_params.start_at_child_key.empty()) {
      result.map()[kQueryParamsIndexStartName] =
          query_params.start_at_child_key;
    }
  }

  if (!query_params.end_at_value.is_null()) {
    result.map()[kQueryParamsIndexEndValue] = query_params.end_at_value;
    if (!query_params.end_at_child_key.empty()) {
      result.map()[kQueryParamsIndexEndName] = query_params.end_at_child_key;
    }
  }

  // QueryParams in Android implementation does not really have "equal_to"
  // property.  Instead, it is converted into "start_at" and "end_at" with the
  // same value.
  if (!query_params.equal_to_value.is_null()) {
    result.map()[kQueryParamsIndexStartValue] = query_params.equal_to_value;
    result.map()[kQueryParamsIndexEndValue] = query_params.equal_to_value;
    if (!query_params.equal_to_child_key.empty()) {
      result.map()[kQueryParamsIndexStartName] =
          query_params.equal_to_child_key;
      result.map()[kQueryParamsIndexEndName] = query_params.equal_to_child_key;
    }
  }

  if (query_params.limit_first != 0) {
    result.map()[kQueryParamsLimit] =
        Variant::FromInt64(query_params.limit_first);
    result.map()[kQueryParamsViewFrom] = kQueryParamsViewFromLeft;
  }

  if (query_params.limit_last != 0) {
    result.map()[kQueryParamsLimit] =
        Variant::FromInt64(query_params.limit_last);
    result.map()[kQueryParamsViewFrom] = kQueryParamsViewFromRight;
  }

  // No need to specify it if it is ordered by priority.
  if (query_params.order_by != QueryParams::kOrderByPriority) {
    switch (query_params.order_by) {
      case QueryParams::kOrderByKey: {
        result.map()[kQueryParamsIndex] = kQueryParamsIndexByKey;
        break;
      }
      case QueryParams::kOrderByValue: {
        result.map()[kQueryParamsIndex] = kQueryParamsIndexByValue;
        break;
      }
      case QueryParams::kOrderByChild: {
        Path child_path(query_params.order_by_child);
        if (child_path.empty()) {
          result.map()[kQueryParamsIndex] = "/";
        } else {
          result.map()[kQueryParamsIndex] = child_path.str();
        }
        break;
      }
      default:
        break;
    }
  }

  return result;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
