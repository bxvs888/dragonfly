// Copyright 2022, Roman Gershman.  All rights reserved.
// See LICENSE for licensing terms.
//
#include <gmock/gmock.h>

extern "C" {
#include "redis/crc64.h"
#include "redis/zmalloc.h"
}

#include <mimalloc.h>

#include "base/gtest.h"
#include "base/logging.h"
#include "io/file.h"
#include "server/engine_shard_set.h"
#include "server/rdb_load.h"
#include "server/test_utils.h"
#include "util/uring/uring_pool.h"

using namespace testing;
using namespace std;
using namespace util;

namespace dfly {

class RdbTest : public BaseFamilyTest {
 protected:

 protected:
  io::FileSource GetSource(string name);
};

inline const uint8_t* to_byte(const void* s) {
  return reinterpret_cast<const uint8_t*>(s);
}

io::FileSource RdbTest::GetSource(string name) {
  string rdb_file = base::ProgramRunfile("testdata/" + name);
  auto open_res = io::OpenRead(rdb_file, io::ReadonlyFile::Options{});
  CHECK(open_res) << rdb_file;

  return io::FileSource(*open_res);
}

TEST_F(RdbTest, Crc) {
  std::string_view s{"TEST"};

  uint64_t c = crc64(0, to_byte(s.data()), s.size());
  ASSERT_NE(c, 0);

  uint64_t c2 = crc64(c, to_byte(s.data()), s.size());
  EXPECT_NE(c, c2);

  uint64_t c3 = crc64(c, to_byte(&c), sizeof(c));
  EXPECT_EQ(c3, 0);

  s = "COOLTEST";
  c = crc64(0, to_byte(s.data()), 8);
  c2 = crc64(0, to_byte(s.data()), 4);
  c3 = crc64(c2, to_byte(s.data() + 4), 4);
  EXPECT_EQ(c, c3);

  c2 = crc64(0, to_byte(s.data() + 4), 4);
  c3 = crc64(c2, to_byte(s.data()), 4);
  EXPECT_NE(c, c3);
}

TEST_F(RdbTest, LoadEmpty) {
  io::FileSource fs = GetSource("empty.rdb");
  RdbLoader loader;
  auto ec = loader.Load(&fs);
  CHECK(!ec);
}

TEST_F(RdbTest, LoadSmall) {
  io::FileSource fs = GetSource("small.rdb");
  RdbLoader loader;
  auto ec = loader.Load(&fs);
  CHECK(!ec);
}

TEST_F(RdbTest, Save) {
  Run({"set", "string_key", "val"});
  Run({"sadd", "set_key1", "val1", "val2"});
  Run({"sadd", "set_key2", "1", "2", "3"});

  // Run({"rpush", "list_key", "val"});  // TODO: invalid encoding when reading by redis 6.
  // Run({"rpush", "list_key", "val"});
  Run({"hset", "hset_key", "field1", "val1", "field2", "val2"});
  Run({"save"});
}

}  // namespace dfly