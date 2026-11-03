//
// Copyright 2003 Google LLC All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Neal Cardwell

#include "app/rest/gzipheader.h"

#include <stdio.h>

#include <algorithm>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {

// Take some test headers and pass them to a GZipHeader, fragmenting
// the headers in many different random ways.
TEST(GzipHeader, FragmentTest) {
  std::mt19937 rng(301);
  auto uniform = [&rng](int n) -> int {
    if (n <= 0) return 0;
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(rng);
  };

  struct TestCase {
    const char* str;
    int len;        // total length of the string
    int cruft_len;  // length of the gzip header part
  };
  TestCase tests[] = {
      // Basic header:
      {"\037\213\010\000\216\176\356\075\002\003", 10, 0},

      // Basic headers with crud on the end:
      {"\037\213\010\000\216\176\356\075\002\003X", 11, 1},
      {"\037\213\010\000\216\176\356\075\002\003XXX", 13, 3},

      {
          "\037\213\010\010\321\135\265\100\000\003"
          "emacs\000",
          16, 0  // with an FNAME of "emacs"
      },
      {
          "\037\213\010\010\321\135\265\100\000\003"
          "\000",
          11, 0  // with an FNAME of zero bytes
      },
      {
          "\037\213\010\020\321\135\265\100\000\003"
          "emacs\000",
          16, 0,  // with an FCOMMENT of "emacs"
      },
      {
          "\037\213\010\020\321\135\265\100\000\003"
          "\000",
          11, 0,  // with an FCOMMENT of zero bytes
      },
      {
          "\037\213\010\002\321\135\265\100\000\003"
          "\001\002",
          12, 0  // with an FHCRC
      },
      {
          "\037\213\010\004\321\135\265\100\000\003"
          "\003\000foo",
          15, 0  // with an extra of "foo"
      },
      {
          "\037\213\010\004\321\135\265\100\000\003"
          "\000\000",
          12, 0  // with an extra of zero bytes
      },
      {
          "\037\213\010\032\321\135\265\100\000\003"
          "emacs\000"
          "emacs\000"
          "\001\002",
          24, 0  // with an FNAME of "emacs", FCOMMENT of "emacs", and FHCRC
      },
      {
          "\037\213\010\036\321\135\265\100\000\003"
          "\003\000foo"
          "emacs\000"
          "emacs\000"
          "\001\002",
          29, 0  // with an FNAME of "emacs", FCOMMENT of "emacs", FHCRC, "foo"
      },
      {
          "\037\213\010\036\321\135\265\100\000\003"
          "\003\000foo"
          "emacs\000"
          "emacs\000"
          "\001\002"
          "XXX",
          32, 3  // FNAME of "emacs", FCOMMENT of "emacs", FHCRC, "foo", crud
      },
  };

  // Test all the headers test cases.
  for (size_t i = 0; i < std::size(tests); ++i) {
    // Test many random ways they might be fragmented.
    for (int j = 0; j < 2000; ++j) {
      // Get the test case set up.
      const char* p = tests[i].str;
      int bytes_left = tests[i].len;
      int bytes_read = 0;

      // Pick some random places to fragment the headers.
      const int num_fragments = uniform(bytes_left);
      std::vector<int> fragment_starts;
      for (int frag_num = 0; frag_num < num_fragments; ++frag_num) {
        fragment_starts.push_back(uniform(bytes_left));
      }
      std::sort(fragment_starts.begin(), fragment_starts.end());

      GZipHeader gzip_headers;
      // Go through several fragments and pass them to the headers for parsing.
      int frag_num = 0;
      while (bytes_left > 0) {
        const int fragment_len = (frag_num < num_fragments)
                                     ? (fragment_starts[frag_num] - bytes_read)
                                     : (tests[i].len - bytes_read);
        ASSERT_GE(fragment_len, 0);
        const char* header_end = nullptr;
        GZipHeader::Status status =
            gzip_headers.ReadMore(p, fragment_len, &header_end);
        bytes_read += fragment_len;
        bytes_left -= fragment_len;
        ASSERT_GE(bytes_left, 0);
        p += fragment_len;
        frag_num++;
        if (bytes_left <= tests[i].cruft_len) {
          ASSERT_EQ(status, GZipHeader::COMPLETE_HEADER);
          break;
        } else {
          ASSERT_EQ(status, GZipHeader::INCOMPLETE_HEADER);
        }
      }  // while
    }    // for many fragmentations
  }      // for all test case headers
}

TEST(GzipHeader, RejectsReservedFlags) {
  // RFC 1952 section 2.3.1.2: FLG bits 5..7 (0xE0) are reserved and must be 0.
  const char invalid_flg_header[] =
      "\037\213\010\040\216\176\356\075\002\003";
  GZipHeader gzip_headers;
  const char* header_end = nullptr;
  EXPECT_EQ(GZipHeader::INVALID_HEADER,
            gzip_headers.ReadMore(invalid_flg_header, 10, &header_end));
}

}  // namespace firebase
