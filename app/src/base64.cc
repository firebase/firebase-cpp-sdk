/*
 * Copyright 2019 Google LLC
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

#include "app/src/base64.h"

#include <cstdint>

#include "app/src/assert.h"
#include "app/src/log.h"

namespace firebase {
namespace internal {

// Maps 6-bit index to base64 encoded character.
static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char kBase64TableUrlSafe[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Reverse lookup for kBase64Table above; -1 signifies an invalid character.
// Maps ASCII value to 6-bit index.
// clang-format off
static const int8_t kBase64TableReverse[256] = {
  /*   0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  16 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  32 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63,
  /*  48 */ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
  /*  64 */ -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
  /*  80 */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
  /*  96 */ -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  /* 112 */ 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
  /* 128 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 144 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 160 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 176 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 192 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 208 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 224 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 240 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};
// clang-format on

// If the last character is this, the decoded length has 1 subtracted from it.
// If the last 2 characters  are both this, the decoded length has 2 subtracted
// from it. (See GetBase64DecodedSize for implementation detail.)
static const char kBase64NullEnding = '=';

// Base64 encode a string (binary allowed). Returns true if successful.
static bool Base64EncodeInternal(const std::string& input, std::string* output,
                                 bool url_safe, bool pad_to_32_bits) {
  if (!output) {
    return false;
  }

  // Workaround for if input and output are the same string.
  bool inplace = (output == &input);
  std::string inplace_buffer;
  std::string* output_ptr = inplace ? &inplace_buffer : output;

  const char* base64_table = url_safe ? kBase64TableUrlSafe : kBase64Table;

  // The base64 algorithm is pretty simple: take 3 bytes = 24 bits of data at a
  // time, and encode them in four 6-bit chunks.
  output_ptr->resize(GetBase64EncodedSize(input));
  for (int i = 0, o = 0; i < input.size(); i += 3, o += 4) {
    uint32_t b0 = static_cast<uint8_t>(input[i]);
    uint32_t b1 =
        (i + 1 < input.size()) ? static_cast<uint8_t>(input[i + 1]) : 0;
    uint32_t b2 =
        (i + 2 < input.size()) ? static_cast<uint8_t>(input[i + 2]) : 0;

    uint32_t stream = (b2 & 0xFF) | ((b1 & 0xFF) << 8) | ((b0 & 0xFF) << 16);

    (*output_ptr)[o + 0] = base64_table[(stream >> 18) & 0x3F];
    (*output_ptr)[o + 1] = base64_table[(stream >> 12) & 0x3F];
    (*output_ptr)[o + 2] = (i + 1 < input.size())
                               ? base64_table[(stream >> 6) & 0x3F]
                               : kBase64NullEnding;
    (*output_ptr)[o + 3] = (i + 2 < input.size())
                               ? base64_table[(stream >> 0) & 0x3F]
                               : kBase64NullEnding;
  }
  if (!pad_to_32_bits) {
    // If the last 1 or 2 characters are '=', trim them.
    if (!output_ptr->empty() &&
        (*output_ptr)[output_ptr->size() - 1] == kBase64NullEnding) {
      if (output_ptr->size() > 1 &&
          (*output_ptr)[output_ptr->size() - 2] == kBase64NullEnding) {
        output_ptr->resize(output_ptr->size() - 2);
      } else {
        output_ptr->resize(output_ptr->size() - 1);
      }
    }
  }
  if (inplace) {
    *output = inplace_buffer;
  }
  return true;
}

bool Base64Encode(const std::string& input, std::string* output) {
  return Base64EncodeInternal(input, output, false, false);
}

bool Base64EncodeWithPadding(const std::string& input, std::string* output) {
  return Base64EncodeInternal(input, output, false, true);
}

bool Base64EncodeUrlSafe(const std::string& input, std::string* output) {
  return Base64EncodeInternal(input, output, true, false);
}

bool Base64EncodeUrlSafeWithPadding(const std::string& input,
                                    std::string* output) {
  return Base64EncodeInternal(input, output, true, true);
}

// Get the size that a given string would take up if encoded to Base64.
size_t GetBase64EncodedSize(const std::string& input) {
  return ((input.length() + 2) / 3) * 4;
}

// Base64 decode a string (may output binary). Returns true if successful.  If
// the string is not a multiple of 4 bytes in size, = or == are implied at the
// end.
bool Base64Decode(const std::string& input, std::string* output) {
  if (!output) {
    return false;
  }
  // All Base64 strings must be padded to 4 bytes, with optionally 1 or 2 of the
  // ending bytes removed.
  if (input.size() % 4 == 1) {
    return false;
  }
  // Workaround for if input and output are the same string.
  bool inplace = (output == &input);
  std::string inplace_buffer;
  std::string* output_ptr = inplace ? &inplace_buffer : output;
  output_ptr->resize(GetBase64DecodedSize(input));

  for (int i = 0, o = 0; i < input.size(); i += 4, o += 3) {
    char input0 = input[i + 0];
    char input1 = input[i + 1];
    // At the end of the string, missing bytes 2 and 3 are considered '='.
    char input2 = i + 2 < input.size() ? input[i + 2] : kBase64NullEnding;
    char input3 = i + 3 < input.size() ? input[i + 3] : kBase64NullEnding;
    // If any unknown characters appear, it's an error.
    if (kBase64TableReverse[input0] < 0 || kBase64TableReverse[input1] < 0 ||
        kBase64TableReverse[input2] < 0 || kBase64TableReverse[input3] < 0) {
      return false;
    }
    // If kBase64NullEnding appears anywhere but the last 1 or 2 characters,
    // or if it appears but input.size() % 4 != 0, it's an error.
    bool at_end = (i + 4 >= input.size());
    if ((input0 == kBase64NullEnding) || (input1 == kBase64NullEnding) ||
        (input2 == kBase64NullEnding && !at_end) ||
        (input2 == kBase64NullEnding && input3 != kBase64NullEnding) ||
        (input3 == kBase64NullEnding && !at_end)) {
      return false;
    }
    uint32_t b0 = kBase64TableReverse[input0] & 0x3f;
    uint32_t b1 = kBase64TableReverse[input1] & 0x3f;
    uint32_t b2 = kBase64TableReverse[input2] & 0x3f;
    uint32_t b3 = kBase64TableReverse[input3] & 0x3f;

    uint32_t stream = (b0 << 18) | (b1 << 12) | (b2 << 6) | b3;
    (*output_ptr)[o + 0] = (stream >> 16) & 0xFF;
    if (input2 != kBase64NullEnding) {
      (*output_ptr)[o + 1] = (stream >> 8) & 0xFF;
    } else if (((stream >> 8) & 0xFF) != 0) {
      // If there are any stale bits in this from input1, the text is malformed.
      return false;
    }
    if (input3 != kBase64NullEnding) {
      (*output_ptr)[o + 2] = (stream >> 0) & 0xFF;
    } else if (((stream >> 0) & 0xFF) != 0) {
      // If there are any stale bits in this from input2, the text is malformed.
      return false;
    }
  }
  if (inplace) {
    *output = inplace_buffer;
  }
  return true;
}

// Get the size that a given string would take up if decoded from base64.
size_t GetBase64DecodedSize(const std::string& input) {
  int mod = input.size() % 4;
  if (input.empty() || mod == 1) {
    // Special-cased so we don't have to check input.size() > 1 below.
    return 0;
  }
  size_t padded_size = ((input.size() + 3) / 4) * 3;
  if (mod >= 2 || (mod == 0 && input[input.size() - 1] == kBase64NullEnding)) {
    // If the last byte is '=', or the input size % 4 is 2 or 3 (thus there
    // are implied '='s), then the actual size is 1-2 bytes smaller.
    if (mod == 2 ||
        (mod == 0 && input[input.size() - 2] == kBase64NullEnding)) {
      // If the second-to-last byte is also '=', or the input size % 4 is 2
      // (implying a second '='), then the actual size is 2 bytes smaller.
      return padded_size - 2;
    } else {
      // Otherwise it's just the last character and the actual size is 1 byte
      // smaller.
      return padded_size - 1;
    }
  }
  return padded_size;
}

}  // namespace internal
}  // namespace firebase
