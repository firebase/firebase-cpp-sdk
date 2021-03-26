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
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "util/random/acmrandom.h"

namespace firebase {

// Take some test headers and pass them to a GZipHeader, fragmenting
// the headers in many different random ways.
TEST(GzipHeader, FragmentTest) {
  ACMRandom rnd(ACMRandom::DeprecatedDefaultSeed());

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
  for (int i = 0; i < ABSL_ARRAYSIZE(tests); ++i) {
    // Test many random ways they might be fragmented.
    for (int j = 0; j < 100 * 1000; ++j) {
      // Get the test case set up.
      const char* p = tests[i].str;
      int bytes_left = tests[i].len;
      int bytes_read = 0;

      // Pick some random places to fragment the headers.
      const int num_fragments = rnd.Uniform(bytes_left);
      std::vector<int> fragment_starts;
      for (int frag_num = 0; frag_num < num_fragments; ++frag_num) {
        fragment_starts.push_back(rnd.Uniform(bytes_left));
      }
      std::sort(fragment_starts.begin(), fragment_starts.end());

      VLOG(1) << "=====";
      GZipHeader gzip_headers;
      // Go through several fragments and pass them to the headers for parsing.
      int frag_num = 0;
      while (bytes_left > 0) {
        const int fragment_len = (frag_num < num_fragments)
                                     ? (fragment_starts[frag_num] - bytes_read)
                                     : (tests[i].len - bytes_read);
        CHECK_GE(fragment_len, 0);
        const char* header_end = NULL;
        VLOG(1) << absl::StrFormat("Passing %2d bytes at %2d..%2d: %s",
                                   fragment_len, bytes_read,
                                   bytes_read + fragment_len,
                                   absl::CEscape(std::string(p, fragment_len)));
        GZipHeader::Status status =
            gzip_headers.ReadMore(p, fragment_len, &header_end);
        bytes_read += fragment_len;
        bytes_left -= fragment_len;
        CHECK_GE(bytes_left, 0);
        p += fragment_len;
        frag_num++;
        if (bytes_left <= tests[i].cruft_len) {
          CHECK_EQ(status, GZipHeader::COMPLETE_HEADER);
          break;
        } else {
          CHECK_EQ(status, GZipHeader::INCOMPLETE_HEADER);
        }
      }  // while
    }    // for many fragmentations
  }      // for all test case headers
}

}  // namespace firebase
