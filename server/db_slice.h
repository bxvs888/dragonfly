// Copyright 2021, Roman Gershman.  All rights reserved.
// See LICENSE for licensing terms.
//

#pragma once

#include "server/common_types.h"
#include "core/op_status.h"
#include "server/table.h"

namespace util {
class ProactorBase;
}

namespace dfly {

class DbSlice {
  struct InternalDbStats {
    // Object memory usage besides hash-table capacity.
    size_t obj_memory_usage = 0;
  };

 public:
  DbSlice(uint32_t index, EngineShard* owner);
  ~DbSlice();

  // Activates `db_ind` database if it does not exist (see ActivateDb below).
  void Reserve(DbIndex db_ind, size_t key_size);

  //! UpdateExpireClock updates the expire clock for this db slice.
  //! Must be a wall clock so we could replicate it betweeen machines.
  void UpdateExpireClock(uint64_t now_ms) {
    now_ms_ = now_ms;
  }

  // returns wall clock in millis as it has been set via UpdateExpireClock.
  uint64_t Now() const {
    return now_ms_;
  }

  OpResult<MainIterator> Find(DbIndex db_index, std::string_view key) const;

  // Returns (value, expire) dict entries if key exists, null if it does not exist or has expired.
  std::pair<MainIterator, ExpireIterator> FindExt(DbIndex db_ind, std::string_view key) const;


  // Return .second=true if insertion ocurred, false if we return the existing key.
  std::pair<MainIterator, bool> AddOrFind(DbIndex db_ind, std::string_view key);

  // Either adds or removes (if at == 0) expiry. Returns true if a change was made.
  // Does not change expiry if at != 0 and expiry already exists.
  bool Expire(DbIndex db_ind, MainIterator main_it, uint64_t at);

  // Adds a new entry. Requires: key does not exist in this slice.
  void AddNew(DbIndex db_ind, std::string_view key, MainValue obj, uint64_t expire_at_ms);

  // Adds a new entry if a key does not exists. Returns true if insertion took place,
  // false otherwise. expire_at_ms equal to 0 - means no expiry.
  bool AddIfNotExist(DbIndex db_ind, std::string_view key, MainValue obj, uint64_t expire_at_ms);

  // Creates a database with index `db_ind`. If such database exists does nothing.
  void ActivateDb(DbIndex db_ind);

  ShardId shard_id() const { return shard_id_;}

  size_t db_array_size() const {
    return db_arr_.size();
  }

  bool IsDbValid(DbIndex id) const {
    return bool(db_arr_[id].main_table);
  }

  // Returns existing keys count in the db.
  size_t DbSize(DbIndex db_ind) const;

 private:

  void CreateDbRedis(unsigned index);

  ShardId shard_id_;

  EngineShard* owner_;

  uint64_t now_ms_ = 0;  // Used for expire logic, represents a real clock.

  struct DbRedis {
    std::unique_ptr<MainTable> main_table;
    std::unique_ptr<ExpireTable> expire_table;

    mutable InternalDbStats stats;
  };

  std::vector<DbRedis> db_arr_;
};

}  // namespace dfly
