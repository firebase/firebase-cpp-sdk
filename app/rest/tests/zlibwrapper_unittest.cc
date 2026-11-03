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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

constexpr size_t kMaxBufSize = 1048500;
constexpr size_t kMaxBufFlex = 1048576;
constexpr uLongf kMaxUncompressedDataSize = 10 * 1024 * 1024;
constexpr int kMinCompLevel = 6;
constexpr int kMaxCompLevel = 6;
constexpr int kReadPastWindowIterations = 64;

std::string MakeSampleCorpus(size_t target_size) {
  static constexpr std::string_view kParagraph =
      "this is a sample dictionary of the and or but not We URL "
      "firebase rest transport curl http2 gzip deflate stream chunk "
      "compression decompression verification header footer crc32\n";
  std::string out;
  out.reserve(target_size);
  uint32_t counter = 0;
  while (out.size() < target_size) {
    size_t remaining = target_size - out.size();
    if (remaining >= kParagraph.size()) {
      out.append(kParagraph.data(), kParagraph.size());
    } else {
      out.append(kParagraph.data(), remaining);
    }
    if (out.size() + 8 <= target_size) {
      char tag[9];
      std::snprintf(tag, sizeof(tag), "%08x", counter++);
      out.append(tag, 8);
    }
  }
  return out;
}

void TestCompression(ZLib* zlib, const std::string& uncompbuf,
                     const char* msg) {
  uLongf complen = ZLib::MinCompressbufSize(uncompbuf.size());
  std::string compbuf(complen, '\0');
  int err = zlib->Compress(reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                           reinterpret_cast<const Bytef*>(uncompbuf.data()),
                           static_cast<uLong>(uncompbuf.size()));
  EXPECT_EQ(Z_OK, err) << "  " << uncompbuf.size() << " bytes down to "
                       << complen << " bytes.";

  // Output data size should match input data size.
  uLongf uncomplen2 = static_cast<uLongf>(uncompbuf.size());
  std::string uncompbuf2(uncomplen2, '\0');
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_OK, err);

  if (msg != nullptr && uncomplen2 > 0) {
    std::printf("Orig: %7lu  Compressed: %7lu  %5.3f %s\n",
                static_cast<unsigned long>(uncomplen2),
                static_cast<unsigned long>(complen),
                static_cast<double>(complen) / static_cast<double>(uncomplen2),
                msg);
  }

  EXPECT_EQ(std::string_view(uncompbuf),
            std::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";
}

// Due to a bug in old versions of zlibwrapper, we appended the gzip
// footer even in non-gzip mode. This tests that we can correctly
// uncompress this buggily-compressed data.
void TestBuggyCompression(ZLib* zlib, const std::string& uncompbuf) {
  std::string compbuf(kMaxBufSize, '\0');
  std::string uncompbuf2(kMaxBufFlex, '\0');
  uLongf complen = static_cast<uLongf>(compbuf.size());
  int err = zlib->Compress(reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                           reinterpret_cast<const Bytef*>(uncompbuf.data()),
                           static_cast<uLong>(uncompbuf.size()));
  EXPECT_EQ(Z_OK, err) << "  " << uncompbuf.size() << " bytes down to "
                       << complen << " bytes.";

  complen += 8;  // 8 bytes is size of gzip footer

  uLongf uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(std::string_view(uncompbuf),
            std::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // Make sure uncompress-chunk works as well
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(std::string_view(uncompbuf),
            std::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  ASSERT_TRUE(zlib->UncompressChunkDone());

  // Try to uncompress an incomplete chunk (missing 4 bytes from the
  // gzip footer, which we're ignoring).
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              complen - 4);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(std::string_view(uncompbuf),
            std::string_view(uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // Repeat UncompressChunk with the rest of the gzip footer.
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(
      reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
      reinterpret_cast<const Bytef*>(compbuf.data()) + complen - 4, 4);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(0u, uncomplen2);

  ASSERT_TRUE(zlib->UncompressChunkDone());

  // Uncompress works on a complete input, so it should be able to
  // assume that either the gzip footer is all there or it's not there at all.
  complen -= 4;  // now we're smaller than the footer size
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_DATA_ERROR, err);

  complen += 8;  // now we're bigger than the footer size
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_DATA_ERROR, err);
}

// Make sure we uncompress right even if the first chunk is in the middle
// of the gzip headers, or in the middle of the gzip footers.
void TestGzipHeaderUncompress(ZLib* zlib) {
  struct ChunkSpec {
    const char* s;
    int len;
    int level;
  };
  const ChunkSpec comp_chunks[][10] = {
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
           "\300\337",
           18, 0},
          {"\061\266\016\000", 4, 1},
          {"\000\000", 2, 1},
          {"\n", 1, 2},
          {"", 0, 0},
      }};

  struct FooterFlexibilityRestore {
    ~FooterFlexibilityRestore() {
      ZLib::set_should_be_flexible_with_gzip_footer(false);
    }
  } restore_guard;

  std::string uncompbuf2(kMaxBufFlex, '\0');
  constexpr size_t kNumChunkSets =
      sizeof(comp_chunks) / sizeof(comp_chunks[0]);
  for (int k = 0; k < 6; ++k) {
    const int level = k % 3;
    ZLib::set_should_be_flexible_with_gzip_footer(k < 3);
    for (size_t j = 0; j < kNumChunkSets; ++j) {
      uLongf bytes_uncompressed = 0;
      zlib->Reset();
      int err = Z_OK;
      for (int i = 0; comp_chunks[j][i].len != 0; ++i) {
        if (comp_chunks[j][i].level <= level) {
          uLongf uncomplen2 =
              static_cast<uLongf>(uncompbuf2.size()) - bytes_uncompressed;
          err = zlib->UncompressChunk(
              reinterpret_cast<Bytef*>(&uncompbuf2[0]) + bytes_uncompressed,
              &uncomplen2,
              reinterpret_cast<const Bytef*>(comp_chunks[j][i].s),
              static_cast<uLong>(comp_chunks[j][i].len));
          if (err != Z_OK) {
            break;
          }
          bytes_uncompressed += uncomplen2;
        }
      }
      if (k == 3 || k == 5) {
        ASSERT_TRUE(err != Z_OK || !zlib->UncompressChunkDone());
      } else {
        ASSERT_TRUE(zlib->UncompressChunkDone());
        EXPECT_EQ(sizeof("hello, world!\n") - 1, bytes_uncompressed);
        EXPECT_EQ(0, std::strncmp(uncompbuf2.data(), "hello, world!\n",
                                  bytes_uncompressed));
      }
    }
  }
}

// Take some test inputs and pass them to zlib, fragmenting the input
// in many different random ways.
void TestRandomGzipHeaderUncompress(ZLib* zlib) {
  std::mt19937 rnd(1);

  struct TestCase {
    const char* str;
    int len;
  };
  const TestCase tests[] = {
      {
          "\037\213\010\000\216\176\356\075\002\003"
          "\313\110\315\311\311\327\121\050\317\057\312\111\121\344\002\000"
          "\300\337\061\266\016\000\000\000",
          34,
      },
  };

  std::string uncompbuf2(kMaxBufFlex, '\0');
  constexpr size_t kNumTests = sizeof(tests) / sizeof(tests[0]);
  for (size_t i = 0; i < kNumTests; ++i) {
    for (int j = 0; j < 1000; ++j) {
      const char* p = tests[i].str;
      int bytes_left = tests[i].len;
      int bytes_read = 0;
      uLongf bytes_uncompressed = 0;
      zlib->Reset();

      std::uniform_int_distribution<int> dist(0, bytes_left - 1);
      const int num_fragments = dist(rnd);
      std::vector<int> fragment_starts;
      fragment_starts.reserve(num_fragments);
      for (int frag_num = 0; frag_num < num_fragments; ++frag_num) {
        fragment_starts.push_back(dist(rnd));
      }
      std::sort(fragment_starts.begin(), fragment_starts.end());

      int frag_num = 0;
      while (bytes_left > 0) {
        const int fragment_len = (frag_num < num_fragments)
                                     ? (fragment_starts[frag_num] - bytes_read)
                                     : (tests[i].len - bytes_read);
        ASSERT_GE(fragment_len, 0);
        if (fragment_len != 0) {
          uLongf uncomplen2 =
              static_cast<uLongf>(uncompbuf2.size()) - bytes_uncompressed;
          int err = zlib->UncompressChunk(
              reinterpret_cast<Bytef*>(&uncompbuf2[0]) + bytes_uncompressed,
              &uncomplen2, reinterpret_cast<const Bytef*>(p),
              static_cast<uLong>(fragment_len));
          ASSERT_EQ(err, Z_OK);
          bytes_uncompressed += uncomplen2;
          bytes_read += fragment_len;
          bytes_left -= fragment_len;
          ASSERT_GE(bytes_left, 0);
          p += fragment_len;
        }
        frag_num++;
      }

      ASSERT_TRUE(zlib->UncompressChunkDone());
      EXPECT_EQ(sizeof("hello, world!\n") - 1, bytes_uncompressed);
      EXPECT_EQ(0, std::strncmp(uncompbuf2.data(), "hello, world!\n",
                                bytes_uncompressed));
    }
  }
}

// Make sure we give the proper error codes when inputs aren't quite kosher
void TestErrors(ZLib* zlib, const std::string& uncompbuf_str) {
  const Bytef* uncompbuf =
      reinterpret_cast<const Bytef*>(uncompbuf_str.data());
  const uLongf uncomplen = static_cast<uLongf>(uncompbuf_str.size());
  std::string compbuf(kMaxBufSize, '\0');
  std::string uncompbuf2(kMaxBufFlex, '\0');
  int err;

  uLongf complen = 23;  // don't give it enough space to compress
  err = zlib->Compress(reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                       uncompbuf, uncomplen);
  EXPECT_EQ(Z_BUF_ERROR, err);

  // OK, now successfully compress
  complen = static_cast<uLongf>(compbuf.size());
  err = zlib->Compress(reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                       uncompbuf, uncomplen);
  EXPECT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                       << " bytes.";

  uLongf uncomplen2 = 100;  // not enough space to uncompress
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_BUF_ERROR, err);

  // Here we check what happens when we don't try to uncompress enough bytes
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()), 23);
  EXPECT_EQ(Z_BUF_ERROR, err);

  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              23);
  EXPECT_EQ(Z_OK, err);  // it's ok if a single chunk is too small
  if (err == Z_OK) {
    EXPECT_FALSE(zlib->UncompressChunkDone())
        << "UncompressChunkDone() was happy with truncated compressed data";
  }

  const size_t changepos = 0;
  const char oldval = compbuf[changepos];  // corrupt the input
  compbuf[changepos]++;
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_NE(Z_OK, err);

  compbuf[changepos] = oldval;

  // Make sure our memory-allocating uncompressor deals with problems gracefully
  Bytef* tmpbuf = nullptr;
  const Bytef tmp_compbuf[10] = {0xad, 0xad, 0xad, 0xad, 0xad,
                                 0xad, 0xad, 0xad, 0xad, 0x00};
  uncomplen2 = kMaxUncompressedDataSize;
  err = zlib->UncompressGzipAndAllocate(&tmpbuf, &uncomplen2, tmp_compbuf,
                                        sizeof(tmp_compbuf));
  EXPECT_NE(Z_OK, err);
  EXPECT_EQ(nullptr, tmpbuf);
}

// Make sure that UncompressGzipAndAllocate returns a correct error
// when asked to uncompress data that isn't gzipped.
void TestBogusGunzipRequest(ZLib* zlib) {
  const Bytef compbuf[] = "This is not compressed";
  const uLongf complen = sizeof(compbuf);
  Bytef* uncompbuf = nullptr;
  uLongf uncomplen = 0;
  int err =
      zlib->UncompressGzipAndAllocate(&uncompbuf, &uncomplen, compbuf, complen);
  EXPECT_EQ(Z_DATA_ERROR, err);
  EXPECT_EQ(nullptr, uncompbuf);
}

void TestGzip(ZLib* zlib, const std::string& uncompbuf_str) {
  const char* uncompbuf = uncompbuf_str.data();
  const uLongf uncomplen = static_cast<uLongf>(uncompbuf_str.size());
  std::string compbuf(kMaxBufSize, '\0');
  std::string uncompbuf2(kMaxBufFlex, '\0');

  uLongf complen = static_cast<uLongf>(compbuf.size());
  int err = zlib->Compress(reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                           reinterpret_cast<const Bytef*>(uncompbuf),
                           uncomplen);
  EXPECT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                       << " bytes.";

  uLongf uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->Uncompress(reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
                         reinterpret_cast<const Bytef*>(compbuf.data()),
                         complen);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(uncomplen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, std::memcmp(uncompbuf, uncompbuf2.data(), uncomplen))
      << "Uncompression mismatch!";

  // Also try the auto-allocate uncompressor
  Bytef* tmpbuf = nullptr;
  uncomplen2 = kMaxUncompressedDataSize;
  err = zlib->UncompressGzipAndAllocate(
      &tmpbuf, &uncomplen2, reinterpret_cast<const Bytef*>(compbuf.data()),
      complen);
  EXPECT_EQ(Z_OK, err);
  ASSERT_NE(nullptr, tmpbuf);
  EXPECT_EQ(uncomplen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, std::memcmp(uncompbuf, tmpbuf, uncomplen))
      << "Uncompression mismatch!";
  std::free(tmpbuf);
}

void TestChunkedGzip(ZLib* zlib, const std::string& uncompbuf_str,
                     int num_chunks) {
  const char* uncompbuf = uncompbuf_str.data();
  const uLongf uncomplen = static_cast<uLongf>(uncompbuf_str.size());
  std::string compbuf(kMaxBufSize, '\0');
  std::string uncompbuf2(kMaxBufFlex, '\0');
  ASSERT_GT(num_chunks, 2);
  ASSERT_GT(uncomplen, 0u);

  const uLongf chunklen = std::max<uLongf>(
      1, (uncomplen + static_cast<uLongf>(num_chunks) - 1) /
             static_cast<uLongf>(num_chunks));
  int err = Z_OK;
  std::vector<uLongf> cum_len;
  cum_len.push_back(0);
  size_t chunknum = 0;
  for (uLongf i = 0; i < uncomplen; i += chunklen, ++chunknum) {
    uLongf complen = static_cast<uLongf>(compbuf.size()) - cum_len[chunknum];
    uLongf chunksize =
        (uncomplen - i) < chunklen ? (uncomplen - i) : chunklen;
    err = zlib->CompressChunk(
        reinterpret_cast<Bytef*>(&compbuf[0]) + cum_len[chunknum], &complen,
        reinterpret_cast<const Bytef*>(uncompbuf) + i, chunksize);
    ASSERT_EQ(Z_OK, err) << "  " << uncomplen << " bytes down to " << complen
                         << " bytes.";
    cum_len.push_back(cum_len[chunknum] + complen);
  }
  uLongf complen = static_cast<uLongf>(compbuf.size()) - cum_len[chunknum];
  err = zlib->CompressChunkDone(
      reinterpret_cast<Bytef*>(&compbuf[0]) + cum_len[chunknum], &complen);
  EXPECT_EQ(Z_OK, err);
  cum_len.push_back(cum_len[chunknum] + complen);

  chunknum = 0;
  for (uLongf i = 0; i < uncomplen; i += chunklen, ++chunknum) {
    uLongf uncomplen2 = uncomplen - i;
    uLongf expected = uncomplen2 < chunklen ? uncomplen2 : chunklen;
    err = zlib->UncompressChunk(
        reinterpret_cast<Bytef*>(&uncompbuf2[0]) + i, &uncomplen2,
        reinterpret_cast<const Bytef*>(compbuf.data()) + cum_len[chunknum],
        cum_len[chunknum + 1] - cum_len[chunknum]);
    EXPECT_EQ(Z_OK, err);
    EXPECT_EQ(expected, uncomplen2)
        << "Uncompress size is " << uncomplen2 << ", not " << expected;
  }
  // There should be no further uncompressed bytes after uncomplen bytes.
  uLongf uncomplen2 = static_cast<uLongf>(uncompbuf2.size()) - uncomplen;
  EXPECT_NE(0u, uncomplen2);
  err = zlib->UncompressChunk(
      reinterpret_cast<Bytef*>(&uncompbuf2[0]) + uncomplen, &uncomplen2,
      reinterpret_cast<const Bytef*>(compbuf.data()) + cum_len[chunknum],
      cum_len[chunknum + 1] - cum_len[chunknum]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(0u, uncomplen2);
  EXPECT_TRUE(zlib->UncompressChunkDone());

  // Those uncomplen bytes should match.
  EXPECT_EQ(0, std::memcmp(uncompbuf, uncompbuf2.data(), uncomplen))
      << "Uncompression mismatch!";

  // Now test to make sure resetting works properly
  // (1) First, uncompress the first chunk and make sure it's ok
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              cum_len[1]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(chunklen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, std::memcmp(uncompbuf, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // (2) Now, try the first chunk again and see that there's an error
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              cum_len[1]);
  EXPECT_EQ(Z_DATA_ERROR, err);

  // (3) Now reset it and try again, and see that it's ok
  zlib->Reset();
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size());
  err = zlib->UncompressChunk(reinterpret_cast<Bytef*>(&uncompbuf2[0]),
                              &uncomplen2,
                              reinterpret_cast<const Bytef*>(compbuf.data()),
                              cum_len[1]);
  EXPECT_EQ(Z_OK, err);
  EXPECT_EQ(chunklen, uncomplen2) << "Uncompression mismatch!";
  EXPECT_EQ(0, std::memcmp(uncompbuf, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // (4) Make sure we can tackle output buffers that are too small
  // with the *AtMost() interfaces.
  uLong source_len = cum_len[2] - cum_len[1];
  ASSERT_GT(source_len, 1u);
  uncomplen2 = source_len / 2;
  err = zlib->UncompressAtMost(
      reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
      reinterpret_cast<const Bytef*>(compbuf.data() + cum_len[1]), &source_len);
  EXPECT_EQ(Z_BUF_ERROR, err);

  EXPECT_EQ(0, std::memcmp(uncompbuf + chunklen, uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  const uLongf saveuncomplen2 = uncomplen2;
  uncomplen2 = static_cast<uLongf>(uncompbuf2.size()) - uncomplen2;
  // Uncompress the rest of the chunk.
  err = zlib->UncompressAtMost(
      reinterpret_cast<Bytef*>(&uncompbuf2[0]), &uncomplen2,
      reinterpret_cast<const Bytef*>(compbuf.data() + cum_len[2] - source_len),
      &source_len);

  EXPECT_EQ(Z_OK, err);

  EXPECT_EQ(0, std::memcmp(uncompbuf + chunklen + saveuncomplen2,
                           uncompbuf2.data(), uncomplen2))
      << "Uncompression mismatch!";

  // (5) Finally, reset again so the rest of the tests can succeed.
  zlib->Reset();
}

void TestFooterBufferTooSmall(ZLib* zlib) {
  uLongf footer_len = zlib->MinFooterSize() - 1;
  ASSERT_EQ(9u, footer_len);
  std::vector<Bytef> footer_buffer(footer_len);
  int err = zlib->CompressChunkDone(footer_buffer.data(), &footer_len);
  ASSERT_EQ(Z_BUF_ERROR, err);
  ASSERT_EQ(0u, footer_len);
}

std::string UncompressString(const std::string& input) {
  Bytef* dest = nullptr;
  uLongf dest_len = kMaxUncompressedDataSize;
  ZLib z;
  z.SetGzipHeaderMode();
  int err = z.UncompressGzipAndAllocate(
      &dest, &dest_len, reinterpret_cast<const Bytef*>(input.data()),
      static_cast<uLong>(input.size()));
  EXPECT_EQ(err, Z_OK);
  if (err != Z_OK || dest == nullptr) {
    return std::string();
  }
  std::string result(reinterpret_cast<char*>(dest), dest_len);
  std::free(dest);
  return result;
}

class ZLibWrapperTest : public ::testing::Test {
 protected:
  std::string GetDict() const {
    return "this is a sample dictionary of the and or but not We URL";
  }

  const std::string& GetTestCorpus() const {
    static const std::string* const kCorpus =
        new std::string(MakeSampleCorpus(128 * 1024));
    return *kCorpus;
  }
};

TEST(ZLibWrapperStandaloneTest, MinCompressbufSizeOverflowSafe) {
  // Verify 64-bit calculation does not overflow 32-bit unsigned arithmetic.
  const uLong kHugeDataSize = static_cast<uLong>(0x81000000ULL);
  if (sizeof(uLong) >= 8) {
    EXPECT_GT(ZLib::MinCompressbufSize(kHugeDataSize), kHugeDataSize);
  }
  std::string uncompbuf(1024 * 1024, 'A');
  ZLib zlib;
  zlib.SetCompressionLevel(kMinCompLevel);
  TestCompression(&zlib, uncompbuf, nullptr);
}

TEST_F(ZLibWrapperTest, Compression) {
  const std::string dict = GetDict();
  const std::string& uncompbuf = GetTestCorpus();

  for (int lvl = kMinCompLevel; lvl <= kMaxCompLevel; lvl++) {
    for (int no_header_mode = 0; no_header_mode <= 1; no_header_mode++) {
      ZLib zlib;
      zlib.SetCompressionLevel(lvl);
      zlib.SetNoHeaderMode(no_header_mode != 0);

      TestCompression(&zlib, uncompbuf, " No dict");
      TestCompression(&zlib, uncompbuf, nullptr);

      if (!no_header_mode) {
        char dict_msg[64];
        std::snprintf(dict_msg, sizeof(dict_msg), " Dict %u",
                      static_cast<unsigned int>(dict.size()));
        zlib.SetDictionary(dict.data(), static_cast<unsigned int>(dict.size()));
        TestCompression(&zlib, uncompbuf, dict_msg);
        TestCompression(&zlib, uncompbuf, nullptr);
      }
    }
  }
}

TEST_F(ZLibWrapperTest, BuggyCompression) {
  const std::string& uncompbuf = GetTestCorpus();
  ZLib zlib;
  TestBuggyCompression(&zlib, uncompbuf);
  TestBuggyCompression(&zlib, uncompbuf);
}

TEST_F(ZLibWrapperTest, OtherErrors) {
  const std::string& uncompbuf = GetTestCorpus();
  ZLib zlib;

  zlib.SetNoHeaderMode(false);
  TestErrors(&zlib, uncompbuf);

  zlib.SetNoHeaderMode(true);
  TestErrors(&zlib, uncompbuf);

  zlib.SetGzipHeaderMode();
  TestErrors(&zlib, uncompbuf);

  TestBogusGunzipRequest(&zlib);
}

TEST_F(ZLibWrapperTest, TestBuffersTooBigFails) {
  if (sizeof(uLongf) < 8) {
    GTEST_SKIP() << "uLongf is 32-bit on this platform";
  }
  uLongf valid_len = 100;
  uLongf invalid_len = static_cast<uLongf>(5000000000ULL);
  const Bytef* data = reinterpret_cast<const Bytef*>("test");
  uLongf data_len = 5;
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

TEST_F(ZLibWrapperTest, UncompressChunked) {
  {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    TestGzipHeaderUncompress(&zlib);
  }
  {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    TestRandomGzipHeaderUncompress(&zlib);
  }
}

TEST_F(ZLibWrapperTest, GzipCompression) {
  const std::string& uncompbuf = GetTestCorpus();
  ZLib zlib;

  zlib.SetGzipHeaderMode();
  TestGzip(&zlib, uncompbuf);
  TestGzip(&zlib, uncompbuf);
}

TEST_F(ZLibWrapperTest, ChunkedCompression) {
  const std::string& uncompbuf = GetTestCorpus();
  ZLib zlib;

  zlib.SetGzipHeaderMode();
  TestChunkedGzip(&zlib, uncompbuf, 21);
  TestChunkedGzip(&zlib, uncompbuf, 20);
  TestGzip(&zlib, uncompbuf);
  TestChunkedGzip(&zlib, uncompbuf, 100);
  TestFooterBufferTooSmall(&zlib);
  TestGzip(&zlib, uncompbuf);
}

TEST(ZLibWrapperStandaloneTest, GzipCompatibility) {
  // Known RFC 1952 gzip payload for "hello world\n".
  const std::string gzip_output(
      "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\xcb\x48\xcd\xc9\xc9"
      "\x57\x28\xcf\x2f\xca\x49\xe1\x02\x00\x2d\x3b\x08\xaf\x0c\x00"
      "\x00\x00",
      32);
  ASSERT_EQ("hello world\n", UncompressString(gzip_output));
}

TEST(ZLibWrapperStandaloneTest, DecompressMultiChunkFileWithFooter) {
  ZLib compressor;
  compressor.SetGzipHeaderMode();

  ZLib decompressor;
  decompressor.SetGzipHeaderMode();

  const size_t chunk_size = 256 * 1024;
  const int num_chunks = 8;
  std::string inbuf(chunk_size, 'Z');
  std::string compbuf(ZLib::MinCompressbufSize(chunk_size), '\0');
  std::string outbuf(chunk_size, '\0');

  for (int i = 0; i < num_chunks; ++i) {
    uLongf complen = static_cast<uLongf>(compbuf.size());
    ASSERT_EQ(Z_OK,
              compressor.CompressChunk(
                  reinterpret_cast<Bytef*>(&compbuf[0]), &complen,
                  reinterpret_cast<const Bytef*>(inbuf.data()),
                  static_cast<uLong>(inbuf.size())));

    uLongf outlen = static_cast<uLongf>(outbuf.size());
    ASSERT_EQ(Z_OK,
              decompressor.UncompressChunk(
                  reinterpret_cast<Bytef*>(&outbuf[0]), &outlen,
                  reinterpret_cast<const Bytef*>(compbuf.data()), complen));
    ASSERT_EQ(outlen, static_cast<uLongf>(inbuf.size()));
  }

  uLongf complen = static_cast<uLongf>(compbuf.size());
  ASSERT_EQ(Z_OK, compressor.CompressChunkDone(
                      reinterpret_cast<Bytef*>(&compbuf[0]), &complen));

  uLongf outlen = static_cast<uLongf>(outbuf.size());
  ASSERT_EQ(Z_OK,
            decompressor.UncompressChunk(
                reinterpret_cast<Bytef*>(&outbuf[0]), &outlen,
                reinterpret_cast<const Bytef*>(compbuf.data()), complen));
  ASSERT_TRUE(decompressor.UncompressChunkDone());
}

TEST(ZLibWrapperStandalone, ReadPastEndOfWindow) {
  const std::string uncompbuf = MakeSampleCorpus(128 * 1024);
  const uLongf uncomplen = static_cast<uLongf>(uncompbuf.size());
  ASSERT_GE(uncomplen, 0x10000u);

  std::vector<std::unique_ptr<ZLib>> used_zlibs;
  used_zlibs.reserve(kReadPastWindowIterations);
  const uLongf max_comprlen = ZLib::MinCompressbufSize(uncomplen);
  std::string compr(max_comprlen, '\0');

  for (int i = 0; i < kReadPastWindowIterations; ++i) {
    auto zlib = std::make_unique<ZLib>();
    zlib->SetGzipHeaderMode();
    uLongf comprlen = static_cast<uLongf>(compr.size());
    int rc = zlib->Compress(reinterpret_cast<Bytef*>(&compr[0]), &comprlen,
                            reinterpret_cast<const Bytef*>(uncompbuf.data()),
                            uncomplen);
    ASSERT_EQ(rc, Z_OK);
    used_zlibs.push_back(std::move(zlib));
  }
}

TEST(ZLibWrapperStandalone, BytewiseRead) {
  const std::string text =
      "v nedrah tundry vydra v getrah tyrit v vedrah yadra kedra";
  const uLong text_len = static_cast<uLong>(text.size());
  uLongf archive_len = ZLib::MinCompressbufSize(text_len);
  std::string archive(archive_len, '\0');
  const uLongf decompressed_len = text_len + 1;
  std::string decompressed(decompressed_len, '\0');
  uLongf decompressed_offset = 0;

  ZLib compressor;
  compressor.SetGzipHeaderMode();
  int rc = compressor.Compress(reinterpret_cast<Bytef*>(&archive[0]),
                               &archive_len,
                               reinterpret_cast<const Bytef*>(text.data()),
                               text_len);
  ASSERT_EQ(rc, Z_OK);

  ZLib zlib;
  zlib.SetGzipHeaderMode();
  for (uLongf i = 0; i < archive_len; ++i) {
    uLong source_len = 1;
    uLongf dest_len = decompressed_len - decompressed_offset;
    rc = zlib.UncompressAtMost(
        reinterpret_cast<Bytef*>(&decompressed[0]) + decompressed_offset,
        &dest_len, reinterpret_cast<const Bytef*>(archive.data()) + i,
        &source_len);
    ASSERT_EQ(rc, Z_OK);
    ASSERT_EQ(source_len, 0u);
    decompressed_offset += dest_len;
  }

  ASSERT_TRUE(zlib.IsGzipFooterValid());
  ASSERT_EQ(decompressed_offset, text_len);

  std::string truncated_output(decompressed.data(), text_len);
  ASSERT_EQ(truncated_output, text);
}

TEST(ZLibWrapperStandaloneTest, TruncatedData) {
  constexpr int kBufferLen = 64;
  const std::string uncompressed = "Hello, World!";
  const std::string compressed(
      "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\xf3\x48\xcd\xc9\xc9"
      "\xd7\x51\x08\xcf\x2f\xca\x49\x51\x04\x00\xd0\xc3\x4a\xec\x0d"
      "\x00\x00\x00",
      33);

  // Verify that "compressed" contains valid gzip data.
  {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    char uncompbuf[kBufferLen];
    std::memset(uncompbuf, 0, kBufferLen);
    uLongf uncomplen = kBufferLen;
    int err = zlib.Uncompress(
        reinterpret_cast<Bytef*>(uncompbuf), &uncomplen,
        reinterpret_cast<const Bytef*>(compressed.c_str()),
        static_cast<uLong>(compressed.size()));
    ASSERT_EQ(err, Z_OK);
    ASSERT_EQ(uncompressed, std::string_view(uncompbuf, uncomplen));
  }

  // Test truncated data with ZLib::Uncompress().
  for (size_t len = compressed.size() - 1; len > 0; len--) {
    SCOPED_TRACE("Decompressing truncated prefix of length " +
                 std::to_string(len));
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    char uncompbuf[kBufferLen];
    std::memset(uncompbuf, 0, kBufferLen);
    uLongf uncomplen = kBufferLen;
    int err = zlib.Uncompress(
        reinterpret_cast<Bytef*>(uncompbuf), &uncomplen,
        reinterpret_cast<const Bytef*>(compressed.c_str()),
        static_cast<uLong>(len));
    ASSERT_NE(err, Z_OK);
  }

  // Test truncated data with ZLib::UncompressAtMost() and
  // ZLib::UncompressChunkDone().
  for (size_t len = compressed.size() - 1; len > 0; len--) {
    SCOPED_TRACE("Streaming truncated prefix of length " +
                 std::to_string(len));
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    char uncompbuf[kBufferLen];
    std::memset(uncompbuf, 0, kBufferLen);
    uLongf uncomplen = kBufferLen;
    uLong complen = static_cast<uLong>(len);
    int err = zlib.UncompressAtMost(
        reinterpret_cast<Bytef*>(uncompbuf), &uncomplen,
        reinterpret_cast<const Bytef*>(compressed.c_str()), &complen);
    ASSERT_EQ(err, Z_OK);
    ASSERT_EQ(complen, 0u);
    if (uncomplen > 0) {
      EXPECT_EQ(0, std::strncmp(uncompressed.data(), uncompbuf, uncomplen));
    }
    ASSERT_FALSE(zlib.UncompressChunkDone());
  }
}

TEST(ZLibWrapperStandalone, GzipUncompressedLength) {
  ZLib zlib;

  // "Hello, World!", compressed.
  const std::string hello_world(
      "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\xf3\x48\xcd\xc9\xc9"
      "\xd7\x51\x08\xcf\x2f\xca\x49\x51\x04\x00\xd0\xc3\x4a\xec\x0d"
      "\x00\x00\x00",
      33);
  EXPECT_EQ(13u, zlib.GzipUncompressedLength(
                     reinterpret_cast<const Bytef*>(hello_world.c_str()),
                     static_cast<uLong>(hello_world.size())));

  // Empty string, "", compressed.
  const std::string empty(
      "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\x03\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00",
      20);
  EXPECT_EQ(0u, zlib.GzipUncompressedLength(
                    reinterpret_cast<const Bytef*>(empty.c_str()),
                    static_cast<uLong>(empty.size())));

  const std::string bad_data("\x01\x01\x01\x01", 4);
  for (size_t len = 0; len <= bad_data.size(); len++) {
    EXPECT_EQ(0u, zlib.GzipUncompressedLength(
                      reinterpret_cast<const Bytef*>(bad_data.c_str()),
                      static_cast<uLong>(len)));
  }
}

}  // namespace
}  // namespace firebase
