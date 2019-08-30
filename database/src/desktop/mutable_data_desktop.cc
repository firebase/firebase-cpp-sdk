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

#include "database/src/desktop/mutable_data_desktop.h"

#include <stdlib.h>

#include <cassert>
#include <sstream>

#include "app/memory/shared_ptr.h"
#include "app/src/log.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {
namespace internal {

MutableDataInternal::MutableDataInternal(DatabaseInternal* database,
                                         const Variant& data)
    : db_(database), path_(), holder_(MakeShared<Variant>(data)) {
  if (HasVector(*holder_)) {
    ConvertVectorToMap(holder_.get());
  }
}

MutableDataInternal::MutableDataInternal(const MutableDataInternal& other,
                                         const Path& path)
    : db_(other.db_), path_(path), holder_(other.holder_) {}

MutableDataInternal* MutableDataInternal::Clone() {
  // Just use the copy constructor
  return new MutableDataInternal(*this);
}

MutableDataInternal* MutableDataInternal::Child(const char* path) {
  return new MutableDataInternal(*this, path_.GetChild(path));
}

std::vector<MutableData> MutableDataInternal::GetChildren() {
  std::vector<MutableData> result;
  Variant* node = GetNode();
  if (node != nullptr) {
    std::map<Variant, const Variant*> children;
    GetEffectiveChildren(*node, &children);

    for (auto& child : children) {
      assert(child.first.is_string());
      result.push_back(MutableData(new MutableDataInternal(
          *this, path_.GetChild(child.first.string_value()))));
    }
  }

  return result;
}

size_t MutableDataInternal::GetChildrenCount() {
  return CountEffectiveChildren(*GetNode());
}

const char* MutableDataInternal::GetKey() const { return path_.GetBaseName(); }

std::string MutableDataInternal::GetKeyString() const {
  return path_.GetBaseName();
}

Variant MutableDataInternal::GetValue() {
  Variant* node = GetNode();

  if (node != nullptr) {
    // TODO(chkuang): Simplify this. This is expensive because Variant stored
    //                both value and priority in a quite complicated way.
    Variant clone = *node;
    PrunePrioritiesAndConvertVector(&clone);
    return clone;
  } else {
    return Variant::Null();
  }
}

Variant MutableDataInternal::GetPriority() {
  Variant* node = GetNode();
  if (node != nullptr) {
    return GetVariantPriority(*node);
  } else {
    return Variant::Null();
  }
}

bool MutableDataInternal::HasChild(const char* path) const {
  return !VariantIsEmpty(VariantGetChild(holder_.get(), path_.GetChild(path)));
}

void MutableDataInternal::SetValue(const Variant& value) {
  Variant value_converted = value;
  ConvertVectorToMap(&value_converted);
  VariantUpdateChild(holder_.get(), path_, value_converted);
}

void MutableDataInternal::SetPriority(const Variant& priority) {
  if (!priority.is_fundamental_type()) {
    db_->logger()->LogError(kErrorMsgInvalidVariantForPriority);
    return;
  }
  VariantUpdateChild(holder_.get(), path_.GetChild(kPriorityKey), priority);
}

Variant* MutableDataInternal::GetNode() {
  return GetInternalVariant(holder_.get(), path_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
