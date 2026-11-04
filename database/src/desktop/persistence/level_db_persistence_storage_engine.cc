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

#include <cassert>
#include <cstdint>
#include <cstring>
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

static const char kDbKeyUserWriteRecords[] = "$user_write_records/";
static const char kDbKeyTrackedQueries[] = "$tracked_queries/";
static const char kDbKeyTrackedQueryKeys[] = "$tracked_query_keys/";
static const char kSeparator = '/';
static const Slice kValueSlice(".value/");

namespace firebase {
namespace database {
namespace internal {

template <size_t n>
const char* StringEnd(const char (&str)[n]) {
  return str + strlen(str);
}

class ChildrenAtPath {
 public:
  ChildrenAtPath(DB* database, Slice path) : database_(database), path_(path) {}

  class iterator {
   public:
    iterator(std::unique_ptr<leveldb::Iterator> it, Slice path)
        : impl_(std::move(it)), path_(path) {
      if (impl_) impl_->Seek(path);
    }
    leveldb::Iterator& operator*() { return *impl_; }
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

template <typename Func>
bool CallOnEachLeaf(const Path& path, const Variant& variant,
                    const Func& func) {
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeInt64:
    case Variant::kTypeDouble:
    case Variant::kTypeBool:
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString:
      return func(path, variant);
    case Variant::kTypeMap:
      for (const auto& [key, value] : variant.map()) {
        if (!key.is_string() ||
            !CallOnEachLeaf(path.GetChild(key.string_value()), value, func)) {
          return false;
        }
      }
      return true;
    default:
      assert(false);
      return false;
  }
}

static bool SliceEndsWith(const Slice& slice, const Slice& end) {
  return slice.size() >= end.size() &&
         Slice(slice.data() + slice.size() - end.size(), end.size()) == end;
}

class BufferedWriteBatch {
 public:
  explicit BufferedWriteBatch(DB* database)
      : database_(database),
        has_operation_to_write_(false),
        error_detected_(false) {}

  template <typename KeyFunc, typename ValueFunc>
  bool AddWrite(const KeyFunc& key_func, const ValueFunc value_func) {
    offset_slices_.emplace_back();
    auto& [key_slice, value_slice] = offset_slices_.back();
    key_slice.offset = buffer_.size();
    if (!key_func(&buffer_)) {
      error_detected_ = true;
      return false;
    }
    key_slice.size = buffer_.size() - key_slice.offset;
    if (SliceEndsWith(ToSlice(key_slice), kValueSlice)) {
      key_slice.size -= kValueSlice.size();
    }
    value_slice.offset = buffer_.size();
    if (!value_func(&buffer_)) {
      error_detected_ = true;
      return false;
    }
    value_slice.size = buffer_.size() - value_slice.offset;
    return true;
  }

  void DeleteLocation(const std::string& path) {
    for (auto& child : ChildrenAtPath(database_, path)) {
      batch_.Delete(child.key());
      has_operation_to_write_ = true;
    }
  }

  void Commit() {
    FIREBASE_ASSERT(!error_detected_);
    for (const auto& [key, value] : offset_slices_) {
      batch_.Put(ToSlice(key), ToSlice(value));
      has_operation_to_write_ = true;
    }
    if (has_operation_to_write_) database_->Write(WriteOptions(), &batch_);
  }

 private:
  struct OffsetSlice {
    size_t offset;
    size_t size;
  };

  Slice ToSlice(const OffsetSlice& offset_slice) {
    return Slice(
        reinterpret_cast<const char*>(buffer_.data()) + offset_slice.offset,
        offset_slice.size);
  }

  typedef std::pair<OffsetSlice, OffsetSlice> KeyValuePair;
  DB* database_;
  std::vector<uint8_t> buffer_;
  std::vector<KeyValuePair> offset_slices_;
  WriteBatch batch_;
  bool has_operation_to_write_;
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

static void SaveUserWriteRecordInternal(DB* database,
                                        const UserWriteRecord& record) {
  BufferedWriteBatch batch(database);
  batch.AddWrite(
      [&record](std::vector<uint8_t>* buffer) {
        std::string key = kDbKeyUserWriteRecords +
                          std::to_string(record.write_id) + kSeparator;
        buffer->insert(buffer->end(), key.begin(), key.end());
        return true;
      },
      [&record](std::vector<uint8_t>* buffer) {
        flatbuffers::FlatBufferBuilder builder;
        builder.Finish(FlatbufferFromUserWriteRecord(&builder, record));
        buffer->insert(buffer->end(), builder.GetBufferPointer(),
                       builder.GetBufferPointer() + builder.GetSize());
        return true;
      });
  batch.Commit();
}

void LevelDbPersistenceStorageEngine::SaveUserOverwrite(const Path& path,
                                                        const Variant& data,
                                                        WriteId write_id) {
  VerifyInsideTransaction();
  SaveUserWriteRecordInternal(database_.get(),
                              UserWriteRecord(write_id, path, data, true));
}

void LevelDbPersistenceStorageEngine::SaveUserMerge(
    const Path& path, const CompoundWrite& children, WriteId write_id) {
  VerifyInsideTransaction();
  SaveUserWriteRecordInternal(database_.get(),
                              UserWriteRecord(write_id, path, children));
}

void LevelDbPersistenceStorageEngine::RemoveUserWrite(WriteId write_id) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  batch.DeleteLocation(kDbKeyUserWriteRecords + std::to_string(write_id) +
                       kSeparator);
  batch.Commit();
}

std::vector<UserWriteRecord> LevelDbPersistenceStorageEngine::LoadUserWrites() {
  std::vector<UserWriteRecord> result;
  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyUserWriteRecords)) {
    result.push_back(UserWriteRecordFromFlatbuffer(
        GetPersistedUserWriteRecord(child.value().data())));
  }
  return result;
}

void LevelDbPersistenceStorageEngine::RemoveAllUserWrites() {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  batch.DeleteLocation(kDbKeyUserWriteRecords);
  batch.Commit();
}

static void VariantAddCachedValue(Variant* variant, const Path& path,
                                  const Variant& value) {
  for (const std::string& directory : path.GetDirectories()) {
    if (!variant->is_map()) {
      *variant = IsPriorityKey(directory)
                     ? Variant(std::map<Variant, Variant>{
                           {kValueKey, std::move(*variant)}})
                     : Variant::EmptyMap();
    }
    variant = &variant->map()[directory];
  }
  *variant = value;
}

Variant LevelDbPersistenceStorageEngine::ServerCache(const Path& path) {
  Variant result;
  std::string full_path =
      (path.empty() ? "" : (kSeparator + path.str())) + kSeparator;
  for (auto& child : ChildrenAtPath(database_.get(), full_path)) {
    flexbuffers::Reference reference = flexbuffers::GetRoot(
        reinterpret_cast<const uint8_t*>(child.value().data()),
        child.value().size());
    auto relative_path =
        Path::GetRelative(path, Path(child.key().ToString()));
    assert(relative_path.has_value());
    VariantAddCachedValue(&result, *relative_path,
                          FlexbufferToVariant(reference));
  }
  return result;
}

static bool VariantToFlexbuffer(const Variant& variant,
                                flexbuffers::Builder* fbb);

static bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                                   flexbuffers::Builder* fbb) {
  auto start = fbb->StartMap();
  for (const auto& [k, v] : map) {
    if (k.is_null() || !k.is_fundamental_type()) {
      LogError(
          "Variants of non-fundamental types may not be used as map keys.");
      fbb->EndMap(start);
      return false;
    }
    fbb->Key(k.AsString().string_value());
    if (!VariantToFlexbuffer(v, fbb)) {
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
  for (const auto& item : vector) {
    if (!VariantToFlexbuffer(item, fbb)) {
      fbb->EndVector(start, false, false);
      return false;
    }
  }
  fbb->EndVector(start, false, false);
  return true;
}

static bool VariantToFlexbuffer(const Variant& variant,
                                flexbuffers::Builder* fbb) {
  switch (variant.type()) {
    case Variant::kTypeNull:
      fbb->Null();
      return true;
    case Variant::kTypeInt64:
      fbb->Int(variant.int64_value());
      return true;
    case Variant::kTypeDouble:
      fbb->Double(variant.double_value());
      return true;
    case Variant::kTypeBool:
      fbb->Bool(variant.bool_value());
      return true;
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      const char* str = variant.string_value();
      size_t len = variant.is_mutable_string() ? variant.mutable_string().size()
                                               : strlen(str);
      fbb->String(str, len);
      return true;
    }
    case Variant::kTypeVector:
      return VariantVectorToFlexbuffer(variant.vector(), fbb);
    case Variant::kTypeMap:
      return VariantMapToFlexbuffer(variant.map(), fbb);
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob:
      LogError("Variants containing blobs are not supported.");
      return false;
  }
  return true;
}

static bool PrepareBatchOverwrite(const Path& path, const Variant& data,
                                  BufferedWriteBatch* buffered_write_batch) {
  flexbuffers::Builder builder;
  buffered_write_batch->DeleteLocation(kSeparator + path.str() + kSeparator);
  return CallOnEachLeaf(
      path, data,
      [&buffered_write_batch, &builder](const Path& local_path,
                                        const Variant& leaf) {
        if (leaf.is_null()) return true;
        return buffered_write_batch->AddWrite(
            [&local_path](std::vector<uint8_t>* buffer) {
              if (!local_path.empty()) {
                buffer->push_back(static_cast<uint8_t>(kSeparator));
                buffer->insert(buffer->end(), local_path.str().begin(),
                               local_path.str().end());
              }
              buffer->push_back(static_cast<uint8_t>(kSeparator));
              return true;
            },
            [&leaf, &builder](std::vector<uint8_t>* buffer) {
              if (!VariantToFlexbuffer(leaf, &builder)) return false;
              builder.Finish();
              buffer->insert(buffer->end(), builder.GetBuffer().begin(),
                             builder.GetBuffer().end());
              builder.Clear();
              return true;
            });
      });
}

void LevelDbPersistenceStorageEngine::OverwriteServerCache(
    const Path& path, const Variant& data) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  if (PrepareBatchOverwrite(path, data, &batch)) batch.Commit();
}

void LevelDbPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const Variant& data) {
  VerifyInsideTransaction();
  if (!data.is_map()) return;
  BufferedWriteBatch batch(database_.get());
  for (const auto& [key, value] : data.map()) {
    assert(key.is_string());
    if (!PrepareBatchOverwrite(path.GetChild(key.string_value()), value,
                               &batch)) {
      return;
    }
  }
  batch.Commit();
}

void LevelDbPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const CompoundWrite& children) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  bool success = true;
  children.write_tree().CallOnEach(
      Path(),
      [&path, &batch, &success](const Path& data_path, const Variant& data) {
        if (success) {
          success =
              PrepareBatchOverwrite(path.GetChild(data_path), data, &batch);
        }
      });
  if (success) batch.Commit();
}

uint64_t LevelDbPersistenceStorageEngine::ServerCacheEstimatedSizeInBytes()
    const {
  uint64_t result = 0;
  for (auto& child : ChildrenAtPath(database_.get(), "/")) {
    result += child.key().size() + child.value().size();
  }
  return result;
}

void LevelDbPersistenceStorageEngine::SaveTrackedQuery(
    const TrackedQuery& tracked_query) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  batch.AddWrite(
      [&tracked_query](std::vector<uint8_t>* buffer) {
        std::string key = kDbKeyTrackedQueries +
                          std::to_string(tracked_query.query_id) + kSeparator;
        buffer->insert(buffer->end(), key.begin(), key.end());
        return true;
      },
      [&tracked_query](std::vector<uint8_t>* buffer) {
        flatbuffers::FlatBufferBuilder builder;
        builder.Finish(FlatbufferFromTrackedQuery(&builder, tracked_query));
        buffer->insert(buffer->end(), builder.GetBufferPointer(),
                       builder.GetBufferPointer() + builder.GetSize());
        return true;
      });
  batch.Commit();
}

void LevelDbPersistenceStorageEngine::DeleteTrackedQuery(QueryId query_id) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  batch.DeleteLocation(kDbKeyTrackedQueries + std::to_string(query_id));
  batch.Commit();
}

std::vector<TrackedQuery>
LevelDbPersistenceStorageEngine::LoadTrackedQueries() {
  std::vector<TrackedQuery> result;
  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyTrackedQueries)) {
    result.push_back(TrackedQueryFromFlatbuffer(
        GetPersistedTrackedQuery(child.value().data())));
  }
  return result;
}

void LevelDbPersistenceStorageEngine::ResetPreviouslyActiveTrackedQueries(
    uint64_t last_use) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  flatbuffers::FlatBufferBuilder builder;
  for (auto& child : ChildrenAtPath(database_.get(), kDbKeyTrackedQueries)) {
    const PersistedTrackedQuery* persisted =
        GetPersistedTrackedQuery(child.value().data());
    if (persisted->active()) {
      TrackedQuery tracked_query = TrackedQueryFromFlatbuffer(persisted);
      tracked_query.active = false;
      tracked_query.last_use = last_use;
      batch.AddWrite(
          [&child](std::vector<uint8_t>* buffer) {
            buffer->insert(buffer->end(), child.key().data(),
                           child.key().data() + child.key().size());
            return true;
          },
          [&builder, &tracked_query](std::vector<uint8_t>* buffer) {
            builder.Finish(FlatbufferFromTrackedQuery(&builder, tracked_query));
            buffer->insert(buffer->end(), builder.GetBufferPointer(),
                           builder.GetBufferPointer() + builder.GetSize());
            builder.Clear();
            return true;
          });
    }
  }
  batch.Commit();
}

static bool SaveTrackedQueryKeysInternal(BufferedWriteBatch* batch,
                                         QueryId query_id,
                                         const std::set<std::string>& keys) {
  std::string prefix =
      kDbKeyTrackedQueryKeys + std::to_string(query_id) + kSeparator;
  for (const std::string& key : keys) {
    if (!batch->AddWrite(
            [&prefix, &key](std::vector<uint8_t>* buffer) {
              buffer->insert(buffer->end(), prefix.begin(), prefix.end());
              buffer->insert(buffer->end(), key.begin(), key.end());
              return true;
            },
            [&key](std::vector<uint8_t>* buffer) {
              buffer->insert(buffer->end(), key.begin(), key.end());
              return true;
            })) {
      return false;
    }
  }
  return true;
}

void LevelDbPersistenceStorageEngine::SaveTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& keys) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  SaveTrackedQueryKeysInternal(&batch, query_id, keys);
  batch.Commit();
}

void LevelDbPersistenceStorageEngine::UpdateTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  VerifyInsideTransaction();
  BufferedWriteBatch batch(database_.get());
  for (const std::string& key_to_remove : removed) {
    batch.DeleteLocation(kDbKeyTrackedQueryKeys + std::to_string(query_id) +
                         kSeparator + key_to_remove + kSeparator);
  }
  SaveTrackedQueryKeysInternal(&batch, query_id, added);
  batch.Commit();
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
  if (!prune_forest.PrunesAnything()) return;
  WriteBatch batch;
  bool has_operation_to_write = false;
  std::string root_str = kSeparator + root.str() + kSeparator;
  for (auto& child : ChildrenAtPath(database_.get(), root_str)) {
    Slice key = child.key();
    key.remove_prefix(root.str().size() + 1);
    Path path(key.ToString());
    if (prune_forest.AffectsPath(path) && !prune_forest.ShouldKeep(path)) {
      batch.Delete(child.key());
      has_operation_to_write = true;
    }
  }
  if (has_operation_to_write) database_->Write(WriteOptions(), &batch);
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
