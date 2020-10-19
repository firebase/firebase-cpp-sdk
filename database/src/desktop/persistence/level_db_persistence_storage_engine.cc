// Copyright 2020 Google LLC
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

#include "database/src/desktop/persistence/level_db_persistence_storage_engine.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/persistence/flatbuffer_conversions.h"
#include "database/src/desktop/persistence/in_memory_persistence_storage_engine.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/persistence/prune_forest.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/view_cache.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"

using firebase::database::internal::persistence::GetPersistedTrackedQuery;
using firebase::database::internal::persistence::GetPersistedUserWriteRecord;
using firebase::database::internal::persistence::PersistedTrackedQuery;
using firebase::database::internal::persistence::PersistedUserWriteRecord;
using leveldb::DB;
using leveldb::Iterator;
using leveldb::Options;
using leveldb::ReadOptions;
using leveldb::Slice;
using leveldb::Status;
using leveldb::WriteBatch;
using leveldb::WriteOptions;

// Special database paths
//
// These are special database paths contain data we need to track, but that
// don't want the developer or user editing. These keys are intentionally
// invalid database paths to ensure that.
static const char kDbKeyUserWriteRecords[] = "$user_write_records/";
static const char kDbKeyTrackedQueries[] = "$tracked_queries/";
static const char kDbKeyTrackedQueryKeys[] = "$tracked_query_keys/";

static const char kSeparator = '/';

static const Slice kValueSlice(".value/");

namespace firebase {
namespace database {
namespace internal {

// A utility function to get the pointer one character past the end of a string.
// This is used for finding the sentinal to pass to functions that take pairs of
// iterators (specifically, vector::insert).
template <size_t n>
const char* StringEnd(const char (&str)[n]) {
  return str + strlen(str);
}

// A utility class to make iterating over paths easier.
// The way iterators are used in LevelDB is somewhat verbose and unidiomatic.
// This helper class wraps the creation, destruction and iteration of LevelDB
// iterators.
//
// Example usage:
//     for (auto& child : ChildrenAtPath(database, path)) { ... }
class ChildrenAtPath {
 public:
  ChildrenAtPath(DB* database, Slice path) : database_(database), path_(path) {}

  class iterator {
   public:
    iterator(std::unique_ptr<leveldb::Iterator> it, Slice path)
        : impl_(std::move(it)), path_(path) {
      if (impl_) impl_->Seek(path);
    }

    leveldb::Iterator& operator*() { return *(impl_.get()); }

    iterator& operator++() {
      if (impl_) impl_->Next();
      return *this;
    }

    bool operator!=(const iterator& rhs) const {
      return impl_->Valid() && impl_->key().starts_with(rhs.path_);
    }

   private:
    std::unique_ptr<leveldb::Iterator> impl_;
    Slice path_;
  };

  iterator begin() {
    return iterator(std::unique_ptr<leveldb::Iterator>(
                        database_->NewIterator(ReadOptions())),
                    path_);
  }

  iterator end() { return iterator(nullptr, path_); }

 private:
  DB* database_;
  Slice path_;
};

// Call the given function on each leaf of the given variant.
// Returns true on success, false if there was a failure. Failure can mean that
// the callback reported a failure by returning false, or that it encountered a
// type it doesn't know how to handle (vector, blobs).
template <typename Func>
bool CallOnEachLeaf(const Path& path, const Variant& variant,
                    const Func& func) {
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeInt64:
    case Variant::kTypeDouble:
    case Variant::kTypeBool:
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      if (!func(path, variant)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      for (const auto& key_value : variant.map()) {
        const Variant& key = key_value.first;
        const Variant& value = key_value.second;
        if (!key.is_string()) {
          return false;
        }
        if (!CallOnEachLeaf(path.GetChild(key.string_value()), value, func)) {
          return false;
        }
      }
      break;
    }
    case Variant::kTypeVector: {
      // We expect for vectors to have been converted to maps by the time they
      // reach this point.
      assert(false);
      return false;
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      // Blobs are not supported types.
      assert(false);
      return false;
    }
    default: {
      // All types have cases. If we reach this point something has gone wrong.
      assert(false);
      return false;
    }
  }
  return true;
}

static bool SliceEndsWith(const Slice& slice, const Slice& end) {
  return slice.size() >= end.size() &&
         Slice(slice.data() + slice.size() - end.size(), end.size()) == end;
}

class BufferedWriteBatch {
 public:
  explicit BufferedWriteBatch(DB* database)
      : database_(database),
        buffer_(),
        offset_slices_(),
        batch_(),
        has_operation_to_write_(false),
        error_detected_(false) {}

  template <typename KeyFunc, typename ValueFunc>
  bool AddWrite(const KeyFunc& key_func, const ValueFunc value_func) {
    offset_slices_.emplace_back();
    KeyValuePair& key_value_pair = offset_slices_.back();

    // Write key bytes to buffer.
    OffsetSlice& key_slice = key_value_pair.first;
    key_slice.offset = buffer_.size();
    if (!key_func(&buffer_)) {
      error_detected_ = true;
      return false;
    }
    key_slice.size = buffer_.size() - key_slice.offset;

    // If the key ends in .value, we prune it off to make reconstructing the
    // cache simpler. This ensures that values are always stored in leveldb at
    // foo/bar and never at foo/bar/.value. Since there can only be one
    // representation of a value's path instead of two, rebuilding the cache is
    // simpler.
    if (SliceEndsWith(ToSlice(key_slice), kValueSlice)) {
      key_slice.size -= kValueSlice.size();
    }

    // Write value bytes to buffer.
    OffsetSlice& value_slice = key_value_pair.second;
    value_slice.offset = buffer_.size();
    if (!value_func(&buffer_)) {
      error_detected_ = true;
      return false;
    }
    value_slice.size = buffer_.size() - value_slice.offset;

    return true;
  }

  // Delete the old data at this location.
  void DeleteLocation(const std::string& path) {
    for (auto& child : ChildrenAtPath(database_, path)) {
      batch_.Delete(child.key());
      has_operation_to_write_ = true;
    }
  }

  void Commit() {
    // We should not attempt to commit if an error was detected.
    FIREBASE_ASSERT(error_detected_ == false);

    for (const KeyValuePair& key_value_pair : offset_slices_) {
      const OffsetSlice& key = key_value_pair.first;
      const OffsetSlice& value = key_value_pair.second;

      batch_.Put(ToSlice(key), ToSlice(value));
      has_operation_to_write_ = true;
    }

    if (has_operation_to_write_) {
      WriteOptions options;
      database_->Write(options, &batch_);
    }
  }

 private:
  // OffsetSlice is basically the same thing as a leveldb::Slice, except using
  // an offset instead of a pointer. Since the buffer may resize as the list of
  // writes is built up pointer values are not stable.
  struct OffsetSlice {
    size_t offset;
    size_t size;
  };

  Slice ToSlice(const OffsetSlice& offset_slice) {
    return Slice(
        reinterpret_cast<const char*>(buffer_.data()) + offset_slice.offset,
        offset_slice.size);
  }

  // A key/value pair to insert into the database, represented as OffsetSlices.
  typedef std::pair<OffsetSlice, OffsetSlice> KeyValuePair;

  DB* database_;

  // Buffer to populate with the data that we're going to be adding to leveldb.
  std::vector<uint8_t> buffer_;

  // The full list of key/value pairs to insert into the database.
  std::vector<KeyValuePair> offset_slices_;

  // The complete list of operations to perform atomically.
  WriteBatch batch_;

  // We should not call DB::Write if we have nothing to write.
  bool has_operation_to_write_;

  // An error was detected while collecting data to write. This should not be
  // committed.
  bool error_detected_;
};

LevelDbPersistenceStorageEngine::LevelDbPersistenceStorageEngine(
    LoggerBase* logger)
    : database_(nullptr), inside_transaction_(false), logger_(logger) {}

bool LevelDbPersistenceStorageEngine::Initialize(
    const std::string& level_db_path) {
  Options options;
  options.create_if_missing = true;
  DB* database;
  Status status = DB::Open(options, level_db_path, &database);
  if (!status.ok()) {
    logger_->LogError(
        "Failed to initialize persistence storage engine at path %s: %s",
        level_db_path.c_str(), status.ToString().c_str());
    assert(false);
  }
  database_.reset(database);
  return status.ok();
}

LevelDbPersistenceStorageEngine::~LevelDbPersistenceStorageEngine() {}

void LevelDbPersistenceStorageEngine::SaveUserOverwrite(const Path& path,
                                                        const Variant& data,
                                                        WriteId write_id) {
  VerifyInsideTransaction();
  UserWriteRecord user_write_record(write_id, path, data, true);
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.AddWrite(
      // Key
      [&write_id](std::vector<uint8_t>* buffer) {
        buffer->insert(buffer->end(), kDbKeyUserWriteRecords,
                       StringEnd(kDbKeyUserWriteRecords));
        std::string id_str = std::to_string(write_id);
        buffer->insert(buffer->end(), id_str.begin(), id_str.end());
        buffer->insert(buffer->end(), static_cast<uint8_t>(kSeparator));
        return true;
      },
      // Value
      [&user_write_record](std::vector<uint8_t>* buffer) {
        flatbuffers::FlatBufferBuilder builder;
        auto persisted_user_write_record =
            FlatbufferFromUserWriteRecord(&builder, user_write_record);
        builder.Finish(persisted_user_write_record);
        buffer->insert(buffer->end(), builder.GetBufferPointer(),
                       builder.GetBufferPointer() + builder.GetSize());
        return true;
      });
  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::SaveUserMerge(
    const Path& path, const CompoundWrite& children, WriteId write_id) {
  VerifyInsideTransaction();
  UserWriteRecord user_write_record(write_id, path, children);
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.AddWrite(
      // Key
      [&write_id](std::vector<uint8_t>* buffer) {
        buffer->insert(buffer->end(), kDbKeyUserWriteRecords,
                       StringEnd(kDbKeyUserWriteRecords));
        std::string id_str = std::to_string(write_id);
        buffer->insert(buffer->end(), id_str.begin(), id_str.end());
        buffer->insert(buffer->end(), static_cast<uint8_t>(kSeparator));
        return true;
      },
      // Value
      [&user_write_record](std::vector<uint8_t>* buffer) {
        flatbuffers::FlatBufferBuilder builder;
        auto persisted_user_write_record =
            FlatbufferFromUserWriteRecord(&builder, user_write_record);
        builder.Finish(persisted_user_write_record);
        buffer->insert(buffer->end(), builder.GetBufferPointer(),
                       builder.GetBufferPointer() + builder.GetSize());
        return true;
      });
  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::RemoveUserWrite(WriteId write_id) {
  VerifyInsideTransaction();
  std::string key =
      kDbKeyUserWriteRecords + std::to_string(write_id) + kSeparator;
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.DeleteLocation(key);
  buffered_write_batch.Commit();
}

std::vector<UserWriteRecord> LevelDbPersistenceStorageEngine::LoadUserWrites() {
  std::vector<UserWriteRecord> result;
  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyUserWriteRecords)) {
    const PersistedUserWriteRecord* user_write_record =
        GetPersistedUserWriteRecord(child.value().data());
    result.push_back(UserWriteRecordFromFlatbuffer(user_write_record));
  }
  return result;
}
void LevelDbPersistenceStorageEngine::RemoveAllUserWrites() {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.DeleteLocation(kDbKeyUserWriteRecords);
  buffered_write_batch.Commit();
}

// This adds the value into the given value at the given path. There are other
// utility functions that handle this, but they have more complex logic to
// handle all possible cases of adding a value to a variant. This version of the
// function is simpler and faster, because it can rely on the fact that all
// fields being stored are leaves (as in, not maps or vectors), and it does not
// have to deal with the rules about merging .value and .priority fields, as
// that is all handled before it is written to the database.
static void VariantAddCachedValue(Variant* variant, const Path& path,
                                  const Variant& value) {
  for (const std::string& directory : path.GetDirectories()) {
    // Ensure we're operating on a map.
    if (!variant->is_map()) {
      // Special case: If we are adding a priority, then ensure we do not blow
      // away the value, which at this point will be directly in the
      // variant and not in a .value field. Note that values will never be
      // stored in a .value peudofield.
      if (IsPriorityKey(directory)) {
        *variant = std::map<Variant, Variant>{
            std::make_pair(kValueKey, std::move(*variant)),
        };
      } else {
        // In all other cases, just add an empty map.
        *variant = Variant::EmptyMap();
      }
    }

    // Get the child Variant at the given path.
    auto& map = variant->map();

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

  // Now that we have the variant we are to operate on, insert the value in.
  *variant = value;
}

Variant LevelDbPersistenceStorageEngine::ServerCache(const Path& path) {
  Variant result;
  std::string full_path;
  if (!path.empty()) {
    full_path += kSeparator;
    full_path += path.str();
  }
  full_path += kSeparator;
  for (auto& child : ChildrenAtPath(database_.get(), full_path)) {
    flexbuffers::Reference reference = flexbuffers::GetRoot(
        reinterpret_cast<const uint8_t*>(child.value().data()),
        child.value().size());
    Variant variant = FlexbufferToVariant(reference);
    Path key_path(child.key().ToString());
    Optional<Path> relative_path = Path::GetRelative(path, key_path);
    assert(relative_path.has_value());
    VariantAddCachedValue(&result, *relative_path, variant);
  }
  return result;
}

// Note: these are copied from variant_util, until the problem with packaging
// can be solved.
static bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                                   flexbuffers::Builder* fbb);
static bool VariantVectorToFlexbuffer(const std::vector<Variant>& vector,
                                      flexbuffers::Builder* fbb);

static bool VariantToFlexbuffer(const Variant& variant,
                                flexbuffers::Builder* fbb) {
  switch (variant.type()) {
    case Variant::kTypeNull: {
      fbb->Null();
      break;
    }
    case Variant::kTypeInt64: {
      fbb->Int(variant.int64_value());
      break;
    }
    case Variant::kTypeDouble: {
      fbb->Double(variant.double_value());
      break;
    }
    case Variant::kTypeBool: {
      fbb->Bool(variant.bool_value());
      break;
    }
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      const char* str = variant.string_value();
      size_t len = variant.is_mutable_string() ? variant.mutable_string().size()
                                               : strlen(str);
      fbb->String(str, len);
      break;
    }
    case Variant::kTypeVector: {
      if (!VariantVectorToFlexbuffer(variant.vector(), fbb)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      if (!VariantMapToFlexbuffer(variant.map(), fbb)) {
        return false;
      }
      break;
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      LogError("Variants containing blobs are not supported.");
      return false;
    }
  }
  return true;
}

static bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                                   flexbuffers::Builder* fbb) {
  auto start = fbb->StartMap();
  for (auto iter = map.begin(); iter != map.end(); ++iter) {
    // Flexbuffers only supports string keys, return false if the key is not a
    // type that can be coerced to a string.
    if (iter->first.is_null() || !iter->first.is_fundamental_type()) {
      LogError(
          "Variants of non-fundamental types may not be used as map keys.");
      fbb->EndMap(start);
      return false;
    }
    // Add key.
    fbb->Key(iter->first.AsString().string_value());
    // Add value.
    if (!VariantToFlexbuffer(iter->second, fbb)) {
      fbb->EndMap(start);
      return false;
    }
  }
  fbb->EndMap(start);
  return true;
}

static bool VariantVectorToFlexbuffer(const std::vector<Variant>& vector,
                                      flexbuffers::Builder* fbb) {
  auto start = fbb->StartVector();
  for (auto iter = vector.begin(); iter != vector.end(); ++iter) {
    if (!VariantToFlexbuffer(*iter, fbb)) {
      fbb->EndVector(start, false, false);
      return false;
    }
  }
  fbb->EndVector(start, false, false);
  return true;
}

static bool PrepareBatchOverwrite(const Path& path, const Variant& data,
                                  BufferedWriteBatch* buffered_write_batch) {
  // Reuse a single builder for all values so that we don't keep reallocating
  // each iteration.
  flexbuffers::Builder builder;

  // Delete the old data at this location.
  buffered_write_batch->DeleteLocation(kSeparator + path.str() + kSeparator);

  // Add all the new data.
  return CallOnEachLeaf(
      path, data,
      [&buffered_write_batch, &builder](const Path& local_path,
                                        const Variant& leaf) {
        if (leaf.is_null()) return true;  // Skip nulls.

        // Write key bytes to buffer.
        return buffered_write_batch->AddWrite(
            // Key
            [&local_path](std::vector<uint8_t>* buffer) {
              if (!local_path.empty()) {
                buffer->insert(buffer->end(), static_cast<uint8_t>(kSeparator));
                buffer->insert(buffer->end(), local_path.str().begin(),
                               local_path.str().end());
              }
              buffer->insert(buffer->end(), static_cast<uint8_t>(kSeparator));
              return true;
            },
            // Value
            [&leaf, &builder](std::vector<uint8_t>* buffer) {
              // Build FlexBuffer representation of the value.
              if (!VariantToFlexbuffer(leaf, &builder)) {
                return false;
              }
              // Write FlexBuffer value to buffer.
              builder.Finish();
              buffer->insert(buffer->end(), builder.GetBuffer().begin(),
                             builder.GetBuffer().end());
              // Prepare for next iteration.
              builder.Clear();

              return true;
            });
      });
}

void LevelDbPersistenceStorageEngine::OverwriteServerCache(
    const Path& path, const Variant& data) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());

  bool success = PrepareBatchOverwrite(path, data, &buffered_write_batch);
  if (!success) return;

  // Overwrite prepared successfully, time to commit.
  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const Variant& data) {
  VerifyInsideTransaction();
  if (!data.is_map()) {
    // Merges should always take the form of a map.
    return;
  }

  BufferedWriteBatch buffered_write_batch(database_.get());

  // Gather the changes in the merge.
  for (const auto& key_value : data.map()) {
    const Variant& key = key_value.first;
    const Variant& value = key_value.second;
    assert(key.is_string());
    bool success = PrepareBatchOverwrite(path.GetChild(key.string_value()),
                                         value, &buffered_write_batch);
    if (!success) return;
  }

  // Merge prepared successfully, time to commit.
  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const CompoundWrite& children) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());

  // Gather the changes in the merge.
  bool success = true;
  children.write_tree().CallOnEach(
      Path(), [&path, &buffered_write_batch, &success](const Path& data_path,
                                                       const Variant& data) {
        success = PrepareBatchOverwrite(path.GetChild(data_path), data,
                                        &buffered_write_batch);
        if (!success) return;
      });
  if (!success) return;

  // Merge prepared successfully, time to commit.
  buffered_write_batch.Commit();
}

uint64_t LevelDbPersistenceStorageEngine::ServerCacheEstimatedSizeInBytes()
    const {
  uint64_t result = 0;
  for (auto& child : ChildrenAtPath(database_.get(), "/")) {
    result += child.key().size();
    result += child.value().size();
  }
  return result;
}

void LevelDbPersistenceStorageEngine::SaveTrackedQuery(
    const TrackedQuery& tracked_query) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.AddWrite(
      // Key
      [&tracked_query](std::vector<uint8_t>* buffer) {
        buffer->insert(buffer->end(), kDbKeyTrackedQueries,
                       StringEnd(kDbKeyTrackedQueries));
        std::string id_str = std::to_string(tracked_query.query_id);
        buffer->insert(buffer->end(), id_str.begin(), id_str.end());
        buffer->insert(buffer->end(), static_cast<uint8_t>(kSeparator));
        return true;
      },
      // Value
      [&tracked_query](std::vector<uint8_t>* buffer) {
        flatbuffers::FlatBufferBuilder builder;
        auto persisted_tracked_query =
            FlatbufferFromTrackedQuery(&builder, tracked_query);
        builder.Finish(persisted_tracked_query);
        buffer->insert(buffer->end(), builder.GetBufferPointer(),
                       builder.GetBufferPointer() + builder.GetSize());
        return true;
      });

  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::DeleteTrackedQuery(QueryId query_id) {
  VerifyInsideTransaction();
  std::string key = kDbKeyTrackedQueries + std::to_string(query_id);
  BufferedWriteBatch buffered_write_batch(database_.get());
  buffered_write_batch.DeleteLocation(key);
  buffered_write_batch.Commit();
}

std::vector<TrackedQuery>
LevelDbPersistenceStorageEngine::LoadTrackedQueries() {
  std::vector<TrackedQuery> result;
  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyTrackedQueries)) {
    const PersistedTrackedQuery* tracked_query =
        GetPersistedTrackedQuery(child.value().data());
    result.push_back(TrackedQueryFromFlatbuffer(tracked_query));
  }
  return result;
}

void LevelDbPersistenceStorageEngine::ResetPreviouslyActiveTrackedQueries(
    uint64_t last_use) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());

  flatbuffers::FlatBufferBuilder builder;

  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyTrackedQueries)) {
    const PersistedTrackedQuery* persisted_tracked_query =
        GetPersistedTrackedQuery(child.value().data());
    if (persisted_tracked_query->active()) {
      // Mutate the tracked query.
      TrackedQuery tracked_query =
          TrackedQueryFromFlatbuffer(persisted_tracked_query);
      tracked_query.active = false;
      tracked_query.last_use = last_use;

      // Write it back out.
      buffered_write_batch.AddWrite(
          // Key
          [&child](std::vector<uint8_t>* buffer) {
            buffer->insert(buffer->end(), child.key().data(),
                           child.key().data() + child.key().size());
            return true;
          },
          // Value
          [&builder, &tracked_query](std::vector<uint8_t>* buffer) {
            // Write FlexBuffer value to buffer.
            auto new_persisted_tracked_query =
                FlatbufferFromTrackedQuery(&builder, tracked_query);
            builder.Finish(new_persisted_tracked_query);
            buffer->insert(buffer->end(), builder.GetBufferPointer(),
                           builder.GetBufferPointer() + builder.GetSize());
            // Prepare for next iteration.
            builder.Clear();

            return true;
          });
    }
  }
  buffered_write_batch.Commit();
}

static bool SaveTrackedQueryKeysInternal(
    BufferedWriteBatch* buffered_write_batch, DB* database, QueryId query_id,
    const std::set<std::string>& keys) {
  std::string prefix =
      kDbKeyTrackedQueryKeys + std::to_string(query_id) + kSeparator;
  for (const std::string& key : keys) {
    bool success = buffered_write_batch->AddWrite(
        // Key
        [&prefix, &key](std::vector<uint8_t>* buffer) {
          buffer->insert(buffer->end(), prefix.begin(), prefix.end());
          buffer->insert(buffer->end(), key.begin(), key.end());
          return true;
        },
        // Value
        [&key](std::vector<uint8_t>* buffer) {
          buffer->insert(buffer->end(), key.begin(), key.end());
          return true;
        });
    if (!success) return false;
  }
  return true;
}

void LevelDbPersistenceStorageEngine::SaveTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& keys) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());
  SaveTrackedQueryKeysInternal(&buffered_write_batch, database_.get(), query_id,
                               keys);
  buffered_write_batch.Commit();
}

void LevelDbPersistenceStorageEngine::UpdateTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  VerifyInsideTransaction();
  BufferedWriteBatch buffered_write_batch(database_.get());
  for (const std::string& key_to_remove : removed) {
    std::string path_to_remove = kDbKeyTrackedQueryKeys +
                                 std::to_string(query_id) + kSeparator +
                                 key_to_remove + kSeparator;
    buffered_write_batch.DeleteLocation(path_to_remove);
  }
  SaveTrackedQueryKeysInternal(&buffered_write_batch, database_.get(), query_id,
                               added);
  buffered_write_batch.Commit();
}

static void LoadTrackedQueryKeysInternal(DB* database, QueryId query_id,
                                         std::set<std::string>* out_result) {
  std::string path =
      kDbKeyTrackedQueryKeys + std::to_string(query_id) + kSeparator;
  for (auto& child : ChildrenAtPath(database, path)) {
    out_result->insert(child.value().ToString());
  }
}

std::set<std::string> LevelDbPersistenceStorageEngine::LoadTrackedQueryKeys(
    QueryId query_id) {
  std::set<std::string> result;
  LoadTrackedQueryKeysInternal(database_.get(), query_id, &result);
  return result;
}

std::set<std::string> LevelDbPersistenceStorageEngine::LoadTrackedQueryKeys(
    const std::set<QueryId>& query_ids) {
  std::set<std::string> result;
  for (QueryId query_id : query_ids) {
    LoadTrackedQueryKeysInternal(database_.get(), query_id, &result);
  }
  return result;
}

void LevelDbPersistenceStorageEngine::PruneCache(
    const Path& root, const PruneForestRef& prune_forest) {
  VerifyInsideTransaction();
  if (!prune_forest.PrunesAnything()) {
    return;
  }

  WriteBatch batch;
  bool has_operation_to_write = false;
  std::string root_str = kSeparator + root.str() + kSeparator;
  for (auto& child : ChildrenAtPath(database_.get(), root_str)) {
    Slice key = child.key();
    key.remove_prefix(root.str().size() + 1 /* leading slash */);
    Path path(key.ToString());
    if (prune_forest.AffectsPath(path) && !prune_forest.ShouldKeep(path)) {
      batch.Delete(child.key());
      has_operation_to_write = true;
    }
  }

  if (has_operation_to_write) {
    WriteOptions options;
    database_->Write(options, &batch);
  }
}

bool LevelDbPersistenceStorageEngine::BeginTransaction() {
  FIREBASE_ASSERT_MESSAGE(!inside_transaction_,
                          "RunInTransaction called when an existing "
                          "transaction is already in progress.");
  logger_->LogDebug("Starting transaction.");
  inside_transaction_ = true;
  return true;
}

void LevelDbPersistenceStorageEngine::EndTransaction() {
  FIREBASE_ASSERT_MESSAGE(inside_transaction_,
                          "EndTransaction called when not in a transaction");
  inside_transaction_ = false;
  logger_->LogDebug("Transaction completed.");
}

void LevelDbPersistenceStorageEngine::SetTransactionSuccessful() {}

void LevelDbPersistenceStorageEngine::VerifyInsideTransaction() {
  FIREBASE_ASSERT_MESSAGE(inside_transaction_,
                          "Transaction expected to already be in progress.");
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
