/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/rest/zlibwrapper.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "util/random/acmrandom.h"

// 1048576 == 2^20 == 1 MB
#define MAX_BUF_SIZE 1048500
#define MAX_BUF_FLEX 1048576

DEFINE_int32(min_comp_lvl, 6, "Minimum compression level");
DEFINE_int32(max_comp_lvl, 6, "Maximum compression level");
DEFINE_string(dict, "", "Dictionary file to use (overrides default text)");
DEFINE_string(files_to_process, "",
              "Comma separated list of filenames to read in for our tests. "
              "If empty, a default file from testdata is used.");
DEFINE_int32(zlib_max_size_uncompressed_data, 10 * 1024 * 1024,  // 10MB
             "Maximum expected size of the uncompress length "
             "in the gzip footer.");
DEFINE_string(read_past_window_data_file, "",
              "Data to use for reproducing read-past-window bug;"
              " defaults to zlib/testdata/read_past_window.data");
DEFINE_int32(read_past_window_iterations, 4000,
             "Number of attempts to read past end of window");
ABSL_FLAG(absl::Duration, slow_test_deadline, absl::Minutes(2),
          "The voluntary time limit some of the slow tests attempt to "
          "adhere to.  Used only if the build is detected as an unusually "
          "slow one according to ValgrindSlowdown().  Set to \"inf\" to "
          "disable.");

namespace firebase {

namespace {

// A helper class for build configurations that really slow down the build.
//
// Some of this file's tests are so CPU intensive that they no longer
// finish in a reasonable time under "sanitizer" builds.  These builds
// advertise themselves with a ValgrindSlowdown() > 1.0.  Use this class to
// abandon tests after reasonable deadlines.
class SlowTestLimiter {
 public:
  // Initializes the deadline relative to absl::Now().
  SlowTestLimiter();

  // A human readable reason for the limiter's policy.
  std::string reason() { return reason_; }

  // Returns true if this known to be a slow build.
  bool IsSlowBuild() const { return deadline_ < absl::InfiniteFuture(); }

  // Returns true iff absl::Now() > deadline().  This class is passive; the
  // test must poll.
  bool DeadlineExceeded() const { return absl::Now() > deadline_; }

 private:
  std::string reason_;
  absl::Time deadline_;
};

SlowTestLimiter::SlowTestLimiter() {
  deadline_ = absl::InfiniteFuture();
  double slowdown = ValgrindSlowdown();
  reason_ =
      absl::StrCat("ValgrindSlowdown() of ", absl::LegacyPrecision(slowdown));
  if (slowdown <= 1.0) return;
  absl::Duration relative_deadline = absl::GetFlag(FLAGS_slow_test_deadline);
  absl::StrAppend(&reason_, " with --slow_test_deadline=",
                  absl::FormatDuration(relative_deadline));
  deadline_ = absl::Now() + relative_deadline;
}

REGISTER_MODULE_INITIALIZER(zlibwrapper_unittest, {
  SlowTestLimiter limiter;
  LOG(WARNING)
      << "SlowTestLimiter policy "
      << (limiter.IsSlowBuild()
              ? "limited; slow tests will voluntarily limit execution time."
              : "unlimited.")
      << "  Reason: " << limiter.reason();
});

bool ReadFileToString(absl::string_view filename, std::string* output,
                      int64 max_size) {
  std::ifstream f;
  f.open(filename);
  if (f.fail()) {
    return false;
  }
  f.seekg(0, std::ios::end);
  int64 length = std::min(static_cast<int64>(f.tellg()), max_size);
  f.seekg(0, std::ios::beg);
  output->resize(length);
  f.read(&*output->begin(), length);
  f.close();
  return !f.fail();
}

void TestCompression(ZLib* zlib, const std::string& uncompbuf,
                     const char* msg) {
  LOG(INFO) << "TestCompression of " << uncompbuf.size() << " bytes.";

  uLongf complen = ZLib::MinCompressbufSize(uncompbuf.size());
  std::string compbuf(complen, '\0');
  int err = zlib->Compress((Bytef*)compbuf.data(), &complen,
                           (Bytef*)uncompbuf.data(), uncompbuf.size());
  EXPECT_EQ(Z_OK, err) << "  " << uncompbuf.size() << " bytes down to "
                       << complen << " bytes.";

  // Output data size should match input data size.
  uLongf uncomplen2 = uncompbuf.size();
  std::string uncompbuf2(uncomplen2, '\0');
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_OK, err);

  if (msg != nullptr) {
    printf("Orig: %7lu  Compressed: %7lu  %5.3f %s\n", uncomplen2, complen,
           (float)complen / uncomplen2, msg);
  }

  EXPECT_EQ(uncompbuf, absl::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";
}

// Due to a bug in old versions of zlibwrapper, we appended the gzip
// footer even in non-gzip mode.  This tests that we can correctly
// uncompress this buggily-compressed data.
void TestBuggyCompression(ZLib* zlib, const std::string& uncompbuf) {
  std::string compbuf(MAX_BUF_SIZE, '\0');
  std::string uncompbuf2(MAX_BUF_FLEX, '\0');
  uLongf complen = compbuf.size();
  int err = zlib->Compress((Bytef*)compbuf.data(), &complen,
                           (Bytef*)uncompbuf.data(), uncompbuf.size());
  EXPECT_EQ(Z_OK, err) << "  " << uncompbuf.size() << " bytes down to "
                       << complen << " bytes.";

  complen += 8;  // 8 bytes is size of gzip footer

  uLongf uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncompbuf, absl::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // Make sure uncompress-chunk works as well
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncompbuf, absl::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  ASSERT_TRUE(zlib->UncompressChunkDone());

  // Try to uncompress an incomplete chunk (missing 4 bytes from the
  // gzip header, which we're ignoring).
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), complen - 4);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncompbuf, absl::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // Repeat UncompressChunk with the rest of the gzip header.
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data() + complen - 4, 4);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(0, uncomplen2);

  ASSERT_TRUE(zlib->UncompressChunkDone());

  // Uncompress works on a complete input, so it should be able to
  // assume that either the gzip footer is all there or its not there at all.
  // Make sure it doesn't work on things that don't look like gzip footers.
  complen -= 4;  // now we're smaller than the footer size
  uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_DATA_ERROR, err);

  complen += 8;  // now we're bigger than the footer size
  uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_DATA_ERROR, err);
}

// Make sure we uncompress right even if the first chunk is in the middle
// of the gzip headers, or in the middle of the gzip footers.  (TODO)
void TestGzipHeaderUncompress(ZLib* zlib) {
  struct {
    const char* s;
    int len;
    int level;
  } comp_chunks[][10] = {
      // Level 0: no gzip footer (except partial footer for the last case)
      // Level 1: normal gzip footer
      // Level 2: extra byte after gzip footer
      {
          // divide up: header, body ("hello, world!\n"), footer
          {"\037\213\010\000\216\176\356\075\002\003", 10, 0},
          {"\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000",
           16, 0},
          {"\300\337\061\266\016\000\000\000", 8, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      },
      {
          // divide up: partial header, partial header,
          // body ("hello, world!\n"), footer
          {"\037\213\010\000\216", 5, 0},
          {"\176\356\075\002\003", 5, 0},
          {"\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000",
           16, 0},
          {"\300\337\061\266\016\000\000\000", 8, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      },
      {
          // divide up: full header,
          // body ("hello, world!\n"), partial footer, partial footer
          {"\037\213\010\000\216\176\356\075\002\003", 10, 0},
          {"\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000",
           16, 0},
          {"\300\337\061\266", 4, 1},
          {"\016\000\000\000", 4, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      },
      {
          // divide up: partial hdr, partial header,
          // body ("hello, world!\n"), partial footer, partial footer
          {"\037\213\010\000\216", 5, 0},
          {"\176\356\075\002\003", 5, 0},
          {"\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000",
           16, 0},
          {"\300\337\061\266", 4, 1},
          {"\016\000\000\000", 4, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      },
      {
          // divide up: partial hdr, partial header,
          // body ("hello, world!\n") with partial footer,
          // partial footer, partial footer
          {"\037\213\010\000\216", 5, 0},
          {"\176\356\075\002\003", 5, 0},
          {"\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000"
           // start of footer here.
           "\300\337",
           18, 0},
          {"\061\266\016\000", 4, 1},
          {"\000\000", 2, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      }};

  std::string uncompbuf2(MAX_BUF_FLEX, '\0');
  for (int k = 0; k < 6; ++k) {
    // k: < 3 with ZLib::should_be_flexible_with_gzip_footer_ true
    //    >= 3 with ZLib::should_be_flexible_with_gzip_footer_ false
    // 0/3: no footer (partial footer for the last testing case)
    // 1/4: normal footer
    // 2/5: extra byte after footer
    const int level = k % 3;
    ZLib::set_should_be_flexible_with_gzip_footer(k < 3);
    for (int j = 0; j < ABSL_ARRAYSIZE(comp_chunks); ++j) {
      int bytes_uncompressed = 0;
      zlib->Reset();
      int err = Z_OK;
      for (int i = 0; comp_chunks[j][i].len != 0; ++i) {
        if (comp_chunks[j][i].level <= level) {
          uLongf uncomplen2 = uncompbuf2.size() - bytes_uncompressed;
          err = zlib->UncompressChunk(
              (Bytef*)&uncompbuf2[0] + bytes_uncompressed, &uncomplen2,
              (const Bytef*)comp_chunks[j][i].s, comp_chunks[j][i].len);
          if (err != Z_OK) {
            LOG(INFO) << "err = " << err << " comp_chunks[" << j << "][" << i
                      << "] failed.";
            break;
          } else {
            bytes_uncompressed += uncomplen2;
          }
        }
      }
      // With ZLib::should_be_flexible_with_gzip_footer_ being false, the no or
      // partial footer (k == 3) and extra byte after footer (k == 5) cases
      // should not work. With ZLib::should_be_flexible_with_gzip_footer_ being
      // true, all cases should work.
      if (k == 3 || k == 5) {
        ASSERT_TRUE(err != Z_OK || !zlib->UncompressChunkDone());
      } else {
        ASSERT_TRUE(zlib->UncompressChunkDone());
        LOG(INFO) << "Got " << bytes_uncompressed << " bytes: "
                  << absl::string_view(uncompbuf2.data(), bytes_uncompressed);
        EXPECT_EQ(sizeof("hello, world!\n") - 1, bytes_uncompressed);
        EXPECT_EQ(0, strncmp(uncompbuf2.data(), "hello, world!\n",
                             bytes_uncompressed))
            << "Uncompression mismatch, expected 'hello, world!\\n', "
            << "got '"
            << absl::string_view(uncompbuf2.data(), bytes_uncompressed) << "'";
      }
    }
  }
}

// Take some test inputs and pass them to zlib, fragmenting the input
// in many different random ways.
void TestRandomGzipHeaderUncompress(ZLib* zlib) {
  ACMRandom rnd(ACMRandom::DeprecatedDefaultSeed());

  struct TestCase {
    const char* str;
    int len;  // total length of the string
  };
  TestCase tests[] = {
      {
          // header, body ("hello, world!\n"), footer
          "\037\213\010\000\216\176\356\075\002\003"
          "\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000"
          "\300\337\061\266\016\000\000\000",
          34,
      },
  };

  std::string uncompbuf2(MAX_BUF_FLEX, '\0');
  // Test all the headers test cases.
  for (int i = 0; i < ABSL_ARRAYSIZE(tests); ++i) {
    // Test many random ways they might be fragmented.
    for (int j = 0; j < 5 * 1000; ++j) {
      // Get the test case set up.
      const char* p = tests[i].str;
      int bytes_left = tests[i].len;
      int bytes_read = 0;
      int bytes_uncompressed = 0;
      zlib->Reset();

      // Pick some random places to fragment the headers.
      const int num_fragments = rnd.Uniform(bytes_left);
      std::vector<int> fragment_starts;
      for (int frag_num = 0; frag_num < num_fragments; ++frag_num) {
        fragment_starts.push_back(rnd.Uniform(bytes_left));
      }
      std::sort(fragment_starts.begin(), fragment_starts.end());

      VLOG(1) << "=====";

      // Go through several fragments and pass them in for parsing.
      int frag_num = 0;
      while (bytes_left > 0) {
        const int fragment_len = (frag_num < num_fragments)
                                     ? (fragment_starts[frag_num] - bytes_read)
                                     : (tests[i].len - bytes_read);
        ASSERT_GE(fragment_len, 0);
        if (fragment_len != 0) {  // zlib doesn't like 0-length buffers
          VLOG(1) << absl::StrFormat(
              "Passing %2d bytes at %2d..%2d: %s", fragment_len, bytes_read,
              bytes_read + fragment_len,
              absl::CEscape(std::string(p, fragment_len)));

          uLongf uncomplen2 = uncompbuf2.size() - bytes_uncompressed;
          int err =
              zlib->UncompressChunk((Bytef*)&uncompbuf2[0] + bytes_uncompressed,
                                    &uncomplen2, (const Bytef*)p, fragment_len);
          ASSERT_EQ(err, Z_OK);
          bytes_uncompressed += uncomplen2;
          bytes_read += fragment_len;
          bytes_left -= fragment_len;
          ASSERT_GE(bytes_left, 0);
          p += fragment_len;
        }
        frag_num++;
      }  // while bytes left to uncompress

      ASSERT_TRUE(zlib->UncompressChunkDone());
      VLOG(1) << "Got " << bytes_uncompressed << " bytes: "
              << absl::string_view(uncompbuf2.data(), bytes_uncompressed);
      EXPECT_EQ(sizeof("hello, world!\n") - 1, bytes_uncompressed);
      EXPECT_EQ(
          0, strncmp(uncompbuf2.data(), "hello, world!\n", bytes_uncompressed))
          << "Uncompression mismatch, expected 'hello, world!\\n', "
          << "got '" << absl::string_view(uncompbuf2.data(), bytes_uncompressed)
          << "'";
    }  // for many fragmentations
  }    // for all test case headers
}

// Make sure we give the proper error codes when inputs aren't quite kosher
void TestErrors(ZLib* zlib, const std::string& uncompbuf_str) {
  const char* uncompbuf = uncompbuf_str.data();
  const uLongf uncomplen = uncompbuf_str.size();
  std::string compbuf(MAX_BUF_SIZE, '\0');
  std::string uncompbuf2(MAX_BUF_FLEX, '\0');
  int err;

  uLongf complen = 23;  // don't give it enough space to compress
  err = zlib->Compress((Bytef*)compbuf.data(), &complen, (Bytef*)uncompbuf,
                       uncomplen);
  EXPECT_EQ(Z_BUF_ERROR, err);

  // OK, now sucessfully compress
  complen = compbuf.size();
  err = zlib->Compress((Bytef*)compbuf.data(), &complen, (Bytef*)uncompbuf,
                       uncomplen);
  EXPECT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                       << " bytes.";

  uLongf uncomplen2 = 100;  // not enough space to uncompress
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_BUF_ERROR, err);

  // Here we check what happens when we don't try to uncompress enough bytes
  uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), 23);
  EXPECT_EQ(Z_BUF_ERROR, err);

  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), 23);
  EXPECT_EQ(Z_OK, err);  // it's ok if a single chunk is too small
  if (err == Z_OK) {
    EXPECT_FALSE(zlib->UncompressChunkDone())
        << "UncompresDone() was happy with its 3 bytes of compressed data";
  }

  const int changepos = 0;
  const char oldval = compbuf[changepos];  // corrupt the input
  compbuf[changepos]++;
  uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_NE(Z_OK, err);

  compbuf[changepos] = oldval;

  // Make sure our memory-allocating uncompressor deals with problems gracefully
  char* tmpbuf;
  char tmp_compbuf[10] = "\255\255\255\255\255\255\255\255\255";
  uncomplen2 = FLAGS_zlib_max_size_uncompressed_data;
  err = zlib->UncompressGzipAndAllocate(
      (Bytef**)&tmpbuf, &uncomplen2, (Bytef*)tmp_compbuf, sizeof(tmp_compbuf));
  EXPECT_NE(Z_OK, err);
  EXPECT_EQ(nullptr, tmpbuf);
}

// Make sure that UncompressGzipAndAllocate returns a correct error
// when asked to uncompress data that isn't gzipped.
void TestBogusGunzipRequest(ZLib* zlib) {
  const Bytef compbuf[] = "This is not compressed";
  const uLongf complen = sizeof(compbuf);
  Bytef* uncompbuf;
  uLongf uncomplen = 0;
  int err =
      zlib->UncompressGzipAndAllocate(&uncompbuf, &uncomplen, compbuf, complen);
  EXPECT_EQ(Z_DATA_ERROR, err);
}

void TestGzip(ZLib* zlib, const std::string& uncompbuf_str) {
  const char* uncompbuf = uncompbuf_str.data();
  const uLongf uncomplen = uncompbuf_str.size();
  std::string compbuf(MAX_BUF_SIZE, '\0');
  std::string uncompbuf2(MAX_BUF_FLEX, '\0');

  uLongf complen = compbuf.size();
  int err = zlib->Compress((Bytef*)compbuf.data(), &complen, (Bytef*)uncompbuf,
                           uncomplen);
  EXPECT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                       << " bytes.";

  uLongf uncomplen2 = uncompbuf2.size();
  err = zlib->Uncompress((Bytef*)&uncompbuf2[0], &uncomplen2,
                         (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncomplen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, memcmp(uncompbuf, uncompbuf2.data(), uncomplen))
      << "Uncompression mismatch!";

  // Also try the auto-allocate uncompressor
  char* tmpbuf;
  err = zlib->UncompressGzipAndAllocate((Bytef**)&tmpbuf, &uncomplen2,
                                        (Bytef*)compbuf.data(), complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncomplen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, memcmp(uncompbuf, uncompbuf2.data(), uncomplen))
      << "Uncompression mismatch!";
  if (tmpbuf) free(tmpbuf);
}

void TestChunkedGzip(ZLib* zlib, const std::string& uncompbuf_str,
                     int num_chunks) {
  const char* uncompbuf = uncompbuf_str.data();
  const uLongf uncomplen = uncompbuf_str.size();
  std::string compbuf(MAX_BUF_SIZE, '\0');
  std::string uncompbuf2(MAX_BUF_FLEX, '\0');
  CHECK_GT(num_chunks, 2);

  // uncompbuf2 is larger than uncompbuf to test for decoding too much and for
  // historical reasons.
  //
  // Note that it is possible to receive num_chunks+1 total
  // chunks, due to rounding error.
  const int chunklen = uncomplen / num_chunks;
  int chunknum, i, err;
  int cum_len[num_chunks + 10];  // cumulative compressed length
  cum_len[0] = 0;
  for (chunknum = 0, i = 0; i < uncomplen; i += chunklen, chunknum++) {
    uLongf complen = compbuf.size() - cum_len[chunknum];
    // Make sure the last chunk gets the correct chunksize.
    int chunksize = (uncomplen - i) < chunklen ? (uncomplen - i) : chunklen;
    err = zlib->CompressChunk((Bytef*)compbuf.data() + cum_len[chunknum],
                              &complen, (Bytef*)uncompbuf + i, chunksize);
    ASSERT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                         << " bytes.";
    cum_len[chunknum + 1] = cum_len[chunknum] + complen;
  }
  uLongf complen = compbuf.size() - cum_len[chunknum];
  err = zlib->CompressChunkDone((Bytef*)compbuf.data() + cum_len[chunknum],
                                &complen);
  EXPECT_EQ(Z_OK, err);
  cum_len[chunknum + 1] = cum_len[chunknum] + complen;

  for (chunknum = 0, i = 0; i < uncomplen; i += chunklen, chunknum++) {
    uLongf uncomplen2 = uncomplen - i;
    // Make sure the last chunk gets the correct chunksize.
    int expected = uncomplen2 < chunklen ? uncomplen2 : chunklen;
    err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0] + i, &uncomplen2,
                                (Bytef*)compbuf.data() + cum_len[chunknum],
                                cum_len[chunknum + 1] - cum_len[chunknum]);
    EXPECT_EQ(Z_OK, err);
    EXPECT_EQ(expected, uncomplen2)
        << "Uncompress size is " << uncomplen2 << ", not " << expected;
  }
  // There should be no further uncompressed bytes, after uncomplen bytes.
  uLongf uncomplen2 = uncompbuf2.size() - uncomplen;
  EXPECT_NE(0, uncomplen2);
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0] + uncomplen, &uncomplen2,
                              (Bytef*)compbuf.data() + cum_len[chunknum],
                              cum_len[chunknum + 1] - cum_len[chunknum]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(0, uncomplen2);
  EXPECT_TRUE(zlib->UncompressChunkDone());

  // Those uncomplen bytes should match.
  EXPECT_EQ(0, memcmp(uncompbuf, uncompbuf2.data(), uncomplen))
      << "Uncompression mismatch!";

  // Now test to make sure resetting works properly
  // (1) First, uncompress the first chunk and make sure it's ok
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), cum_len[1]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(chunklen, uncomplen2) << "Uncompression mismatch!";
  // The first uncomplen2 bytes should match, where uncomplen2 is the number of
  // successfully uncompressed bytes by the most recent UncompressChunk call.
  // The remaining (uncomplen - uncomplen2) bytes would still match if the
  // uncompression guaranteed not to modify the buffer other than those first
  // uncomplen2 bytes, but there is no such guarantee.
  EXPECT_EQ(0, memcmp(uncompbuf, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // (2) Now, try the first chunk again and see that there's an error
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), cum_len[1]);
  EXPECT_EQ(Z_DATA_ERROR, err);

  // (3) Now reset it and try again, and see that it's ok
  zlib->Reset();
  uncomplen2 = uncompbuf2.size();
  err = zlib->UncompressChunk((Bytef*)&uncompbuf2[0], &uncomplen2,
                              (Bytef*)compbuf.data(), cum_len[1]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(chunklen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, memcmp(uncompbuf, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // (4) Make sure we can tackle output buffers that are too small
  // with the *AtMost() interfaces.
  uLong source_len = cum_len[2] - cum_len[1];
  CHECK_GT(source_len, 1);
  uncomplen2 = source_len / 2;
  err = zlib->UncompressAtMost((Bytef*)&uncompbuf2[0], &uncomplen2,
                               (Bytef*)(compbuf.data() + cum_len[1]),
                               &source_len);
  EXPECT_EQ(Z_BUF_ERROR, err);

  EXPECT_EQ(0, memcmp(uncompbuf + chunklen, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  const int saveuncomplen2 = uncomplen2;
  uncomplen2 = uncompbuf2.size() - uncomplen2;
  // Uncompress the rest of the chunk.
  err = zlib->UncompressAtMost(
      (Bytef*)&uncompbuf2[0], &uncomplen2,
      (Bytef*)(compbuf.data() + cum_len[2] - source_len), &source_len);

  EXPECT_EQ(Z_OK, err);

  EXPECT_EQ(0, memcmp(uncompbuf + chunklen + saveuncomplen2, uncompbuf2.data(),
                      uncomplen2))
      << "Uncompression mismatch!";

  // (5) Finally, reset again so the rest of the tests can succeed.  :-)
  zlib->Reset();
}

void TestFooterBufferTooSmall(ZLib* zlib) {
  uLongf footer_len = zlib->MinFooterSize() - 1;
  ASSERT_EQ(9, footer_len);
  Bytef footer_buffer[footer_len];
  int err = zlib->CompressChunkDone(footer_buffer, &footer_len);
  ASSERT_EQ(Z_BUF_ERROR, err);
  ASSERT_EQ(0, footer_len);
}

// Helper routine for running a program and capturing its output
std::string RunCommand(const std::string& cmd) {
  LOG(INFO) << "Running [" << cmd << "]";
  FILE* f = popen(cmd.c_str(), "r");
  CHECK(f != nullptr) << ": " << cmd << " failed";
  std::string result;
  while (!feof(f) && !ferror(f)) {
    char buf[1000];
    int n = fread(buf, 1, sizeof(buf), f);
    CHECK(n >= 0);
    result.append(buf, n);
  }
  CHECK(!ferror(f));
  pclose(f);
  return result;
}

// Helper routine to get uncompressed format of a string
std::string UncompressString(const std::string& input) {
  Bytef* dest;
  uLongf dest_len = FLAGS_zlib_max_size_uncompressed_data;
  ZLib z;
  z.SetGzipHeaderMode();
  int err = z.UncompressGzipAndAllocate(&dest, &dest_len, (Bytef*)input.data(),
                                        input.size());
  CHECK_EQ(err, Z_OK);
  std::string result((char*)dest, dest_len);
  free(dest);
  return result;
}

class ZLibWrapperTest : public ::testing::TestWithParam<std::string> {
 protected:
  // Returns the dictionary to use in our tests. If --dict is specified, the
  // file pointed to by that flag is read in and used as the dictionary.
  // Otherwise, a short default dictionary is used.
  std::string GetDict() {
    std::string dict;
    const long kMaxDictLen = 32768;

    // Read in dictionary if specified, else use a default one.
    if (!FLAGS_dict.empty()) {
      CHECK(ReadFileToString(FLAGS_dict, &dict, kMaxDictLen));
      LOG(INFO) << "Read dictionary from " << FLAGS_dict << " (size "
                << dict.size() << ").";
    } else {
      dict = "this is a sample dictionary of the and or but not We URL";
      LOG(INFO) << "Using built-in dictionary (size " << dict.size() << ").";
    }
    return dict;
  }

  std::string ReadFileToTest(absl::string_view filename) {
    std::string uncompbuf;
    LOG(INFO) << "Testing file: " << filename;
    CHECK(ReadFileToString(filename, &uncompbuf, MAX_BUF_SIZE));
    return uncompbuf;
  }
};

TEST(ZLibWrapperTest, HugeCompression) {
  SlowTestLimiter limiter;
  if (limiter.IsSlowBuild()) {
    LOG(WARNING) << "Skipping test.  Reason: " << limiter.reason();
    return;
  }

  int lvl = FLAGS_min_comp_lvl;

  // Just big enough to trigger 32 bit overflow in MinCompressbufSize()
  // calculation.
  const uLong HUGE_DATA_SIZE = 0x81000000;

  // Construct an easily compressible huge buffer.
  std::string uncompbuf(HUGE_DATA_SIZE, 'A');

  LOG(INFO) << "Huge compression at level " << lvl;
  ZLib zlib;
  zlib.SetCompressionLevel(lvl);
  TestCompression(&zlib, uncompbuf, nullptr);
}

TEST_P(ZLibWrapperTest, Compression) {
  const std::string dict = GetDict();
  const std::string uncompbuf = ReadFileToTest(GetParam());

  for (int lvl = FLAGS_min_comp_lvl; lvl <= FLAGS_max_comp_lvl; lvl++) {
    for (int no_header_mode = 0; no_header_mode <= 1; no_header_mode++) {
      ZLib zlib;
      zlib.SetCompressionLevel(lvl);
      zlib.SetNoHeaderMode(no_header_mode);

      // TODO(gromer): Restructure the following code to minimize use of helper
      // functions and LOG(INFO).
      LOG(INFO) << "Level " << lvl << ", no_header_mode " << no_header_mode
                << " (No dict)";
      TestCompression(&zlib, uncompbuf, " No dict");
      LOG(INFO) << "Level " << lvl << ", no_header_mode " << no_header_mode;
      TestCompression(&zlib, uncompbuf, nullptr);

      // Try with a dictionary.  For reasons I don't entirely understand,
      // no_header_mode does not coexist with preloaded dictionaries.
      if (!no_header_mode) {  // try it with a dictionary
        char dict_msg[64];
        snprintf(dict_msg, sizeof(dict_msg), " Dict %u",
                 static_cast<uint>(dict.size()));
        zlib.SetDictionary(dict.data(), dict.size());
        LOG(INFO) << "Level " << lvl << " dict: " << dict_msg;
        TestCompression(&zlib, uncompbuf, dict_msg);
        LOG(INFO) << "Level " << lvl;
        TestCompression(&zlib, uncompbuf, nullptr);
      }
    }
  }
}

// Make sure we deal correctly with a bug in old versions of zlibwrapper
TEST_P(ZLibWrapperTest, BuggyCompression) {
  const std::string uncompbuf = ReadFileToTest(GetParam());
  ZLib zlib;

  LOG(INFO) << "workaround for old zlibwrapper bug";
  TestBuggyCompression(&zlib, uncompbuf);

  // Try compressing again using the same ZLib
  LOG(INFO) << "workaround for old zlibwrapper bug: same ZLib";
  TestBuggyCompression(&zlib, uncompbuf);
}

// Test other problems
TEST_P(ZLibWrapperTest, OtherErrors) {
  const std::string uncompbuf = ReadFileToTest(GetParam());
  ZLib zlib;

  zlib.SetNoHeaderMode(false);
  LOG(INFO)
      << "Testing robustness against various errors: no_header_mode = false";
  TestErrors(&zlib, uncompbuf);

  zlib.SetNoHeaderMode(true);
  LOG(INFO)
      << "Testing robustness against various errors: no_header_mode = true";
  TestErrors(&zlib, uncompbuf);

  zlib.SetGzipHeaderMode();
  LOG(INFO) << "Testing robustness against various errors: gzip_header_mode";
  TestErrors(&zlib, uncompbuf);

  LOG(INFO)
      << "Testing robustness against various errors: bogus gunzip request";
  TestBogusGunzipRequest(&zlib);
}

// Make sure that (Un-)compress returns a correct error when asked to
// (un-)compress into a buffer bigger than 2^32 bytes.
// Running this with blaze --config=msan exposed the bug underlying
// http://b/25308089.
TEST_P(ZLibWrapperTest, TestBuffersTooBigFails) {
  uLongf valid_len = 100;
  uLongf invalid_len = 5000000000;  // Bigger than 32 bit supported by zlib.
  const Bytef* data = reinterpret_cast<const Bytef*>("test");
  uLongf data_len = 5;
  // This test is not reusing the Zlib object so msan can determine
  // when it's used uninitialized.
  {
    ZLib zlib;
    EXPECT_EQ(Z_BUF_ERROR,
              zlib.Compress(nullptr, &invalid_len, data, data_len));
  }
  {
    ZLib zlib;
    EXPECT_EQ(Z_BUF_ERROR,
              zlib.Compress(nullptr, &valid_len, nullptr, invalid_len));
  }
  {
    ZLib zlib;
    EXPECT_EQ(Z_BUF_ERROR,
              zlib.Uncompress(nullptr, &invalid_len, data, data_len));
  }
  {
    ZLib zlib;
    EXPECT_EQ(Z_BUF_ERROR,
              zlib.Uncompress(nullptr, &valid_len, nullptr, invalid_len));
  }
}

// Make sure we deal correctly with compressed headers chunked weirdly
TEST_P(ZLibWrapperTest, UncompressChunked) {
  {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    LOG(INFO) << "Uncompressing gzip headers";
    TestGzipHeaderUncompress(&zlib);
  }
  {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    LOG(INFO) << "Uncompressing randomly-fragmented gzip headers";
    TestRandomGzipHeaderUncompress(&zlib);
  }
}

// Now test gzip compression.
TEST_P(ZLibWrapperTest, GzipCompression) {
  const std::string uncompbuf = ReadFileToTest(GetParam());
  ZLib zlib;

  zlib.SetGzipHeaderMode();
  LOG(INFO) << "gzip compression";
  TestGzip(&zlib, uncompbuf);

  // Try compressing again using the same ZLib
  LOG(INFO) << "gzip compression: same ZLib";
  TestGzip(&zlib, uncompbuf);
}

// Now test chunked compression.
TEST_P(ZLibWrapperTest, ChunkedCompression) {
  const std::string uncompbuf = ReadFileToTest(GetParam());
  ZLib zlib;

  zlib.SetGzipHeaderMode();
  LOG(INFO) << "chunked gzip compression";
  // At this point is the minimum between MAX_BUF_SIZE (1048500)
  // and the size of the last input file processed. With a larger file,
  // uncompen is a multiple of both 10, 20 and 100.
  // Using 21 chunks cause the last chunk to be smaller than the others.
  TestChunkedGzip(&zlib, uncompbuf, 21);

  // Try compressing again using the same ZLib
  LOG(INFO) << "chunked gzip compression: same ZLib";
  TestChunkedGzip(&zlib, uncompbuf, 20);

  // In theory we can mix and match the type of compression we do
  LOG(INFO) << "chunked gzip compression: different compression type";
  TestGzip(&zlib, uncompbuf);
  LOG(INFO) << "chunked gzip compression: original compression type";
  TestChunkedGzip(&zlib, uncompbuf, 100);

  // Test writing final chunk and footer into buffer that's too small.
  LOG(INFO) << "chunked gzip compression: buffer too small";
  TestFooterBufferTooSmall(&zlib);

  LOG(INFO) << "chunked gzip compression: not chunked";
  TestGzip(&zlib, uncompbuf);
}

// Simple helper to force specialization of absl::StrSplit.
std::vector<std::string> GetFilesToProcess() {
  std::string files_to_process =
      FLAGS_files_to_process.empty()
          ? absl::StrCat(FLAGS_test_srcdir, "/google3/util/gtl/testdata/words")
          : FLAGS_files_to_process;
  return absl::StrSplit(files_to_process, ",", absl::SkipWhitespace());
}

INSTANTIATE_TEST_SUITE_P(AllTests, ZLibWrapperTest,
                         ::testing::ValuesIn(GetFilesToProcess()));

TEST(ZLibWrapperStandaloneTest, GzipCompatibility) {
  LOG(INFO) << "Testing compatibility with gzip output";
  const std::string input = "hello world";
  std::string gzip_output =
      RunCommand(absl::StrCat("echo ", input, " | gzip -c"));
  ASSERT_EQ(absl::StrCat(input, "\n"), UncompressString(gzip_output));
}

/*
  The Gzip footer contains the lower four bytes of the uncompressed length.
  Previously IsGzipFooterValid() compared the value from the footer with the
  entire length, not the lower four bytes of the length.

  To test this, compress a 4 GB file a chunk at a time.
 */
TEST(ZLibWrapperStandaloneTest, DecompressHugeFileWithFooter) {
  SlowTestLimiter limiter;

  ZLib compressor;
  // We specifically want to test that we validate the footer correctly.
  compressor.SetGzipHeaderMode();

  ZLib decompressor;
  decompressor.SetGzipHeaderMode();

  const int64 uncompressed_size = 1LL << 32;  // too big for a 4-byte int
  int64 uncompressed_bytes_sent = 0;

  const int64 chunk_size = 10 * 1024 * 1024;
  std::string inbuf(chunk_size, '\0');    // The input data
  std::string compbuf(chunk_size, '\0');  // The compressed data
  std::string outbuf(chunk_size, '\0');   // The output data
  while (uncompressed_bytes_sent < uncompressed_size) {
    if (limiter.DeadlineExceeded()) {
      LOG(WARNING) << "Ending test early, after " << uncompressed_bytes_sent
                   << " of " << uncompressed_size
                   << " bytes.  Reason: " << limiter.reason();
      return;
    }

    // Compress a chunk.
    uLongf complen = chunk_size;
    ASSERT_EQ(Z_OK,
              compressor.CompressChunk((Bytef*)compbuf.data(), &complen,
                                       (Bytef*)inbuf.data(), inbuf.size()));

    // Uncompress a chunk.
    uLongf outlen = chunk_size;
    ASSERT_EQ(Z_OK,
              decompressor.UncompressChunk((Bytef*)outbuf.data(), &outlen,
                                           (Bytef*)compbuf.data(), complen));

    ASSERT_EQ(outlen, inbuf.size());
    uncompressed_bytes_sent += inbuf.size();
  }

  // Write the footer chunk.
  uLongf complen = chunk_size;
  ASSERT_EQ(Z_OK,
            compressor.CompressChunkDone((Bytef*)compbuf.data(), &complen));

  // Read the footer chunk.
  uLongf outlen = chunk_size;
  ASSERT_EQ(Z_OK,
            decompressor.UncompressChunk((Bytef*)outbuf.data(), &outlen,
                                         (Bytef*)compbuf.data(), complen));

  // This will fail if we validate the footer incorrectly.
  ASSERT_TRUE(decompressor.UncompressChunkDone());
}

/*
  Try to reproduce a bug in deflate, by repeatedly allocating compressors
  and compressing a particular block of data (a Google News homepage that
  I extracted from the core file of a crashed NFE).

  To see the bug you'll need to run an optimized build of the unittest (so
  that malloc doesn't add headers) and remove the workaround from deflate.c
  (see the comment in deflateInit2_ for details).

  The full story:

  The inner loop of deflate is in the function longest_match, comparing two
  byte strings to find the length of the common prefix up to a maximum length
  of 258.  The string being examined is in a 64K byte buffer that zlib
  allocates internally (called "window"); the data to be compressed is streamed
  into it.  The code that calls longest_match (deflate_slow) ensures that there
  are always at least MIN_LOOKAHEAD (=262) bytes in the window beyond the start
  of the string being examined.

  For performance longest_match has been rewritten in assembler (match.S), and
  the inner loop compares 8 bytes at a time.  The loop is written to always
  examine 264 bytes, starting within a few bytes of the start of the string
  (depending on alignment).  If you start with a string of length 263 right at
  the end of the buffer, you end up looking at a byte or two beyond the end of
  the buffer.  It doesn't matter whether those bytes match or not, since the
  match length will get maxed against 258 anyway, but if you're unlucky and
  the page after the buffer isn't mapped, you'll die.

  This never happened to GWS because it doesn't generate pages that are 64K
  long.  It doesn't happen to anyone outside google because everyone else's
  malloc, when asked to alloc a 64K block, will actually allocate an extra
  page to allow for the headers.  But the NFE, a google front-end that
  routinely generates 120K result pages, hit this bug about 20 times a day.
*/
TEST(ZLibWrapperStandalone, ReadPastEndOfWindow) {
  SlowTestLimiter limiter;

  std::string fname = FLAGS_read_past_window_data_file;
  if (fname.empty()) {
    fname = FLAGS_test_srcdir +
            "/google3/third_party/zlib/testdata/read_past_window.data";
  }
  std::string uncompbuf;
  ASSERT_TRUE(ReadFileToString(fname, &uncompbuf, MAX_BUF_SIZE));
  const uLongf uncomplen = uncompbuf.size();
  ASSERT_TRUE(uncomplen >= 0x10000) << "not enough test data in " << fname;

  std::vector<std::unique_ptr<ZLib>> used_zlibs;
  unsigned long comprlen = ZLib::MinCompressbufSize(uncomplen);
  std::string compr(comprlen, '\0');

  for (int i = 0; i < FLAGS_read_past_window_iterations; ++i) {
    if (limiter.DeadlineExceeded()) {
      LOG(WARNING) << "Ending test after only " << i
                   << " of --read_past_window_iteratons="
                   << FLAGS_read_past_window_iterations
                   << " iterations.  Reason: " << limiter.reason();
      break;
    }

    ZLib* zlib = new ZLib;
    zlib->SetGzipHeaderMode();
    int rc = zlib->Compress((Bytef*)&compr[0], &comprlen,
                            (Bytef*)uncompbuf.data(), uncomplen);
    ASSERT_EQ(rc, Z_OK);
    used_zlibs.emplace_back(zlib);
  }

  // if we haven't segfaulted by now, we pass
  LOG(INFO) << "passed read-past-end-of-window test";
}

}  // namespace
}  // namespace firebase
