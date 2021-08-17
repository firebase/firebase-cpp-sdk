/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/android/field_path_portable.h"

#include "firestore/src/common/macros.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

#if defined(__ANDROID__)

// The test cases are copied from
//     Firestore/core/test/firebase/firestore/model/field_path_test.cc

TEST(FieldPathPortableTest, Indexing) {
  const FieldPathPortable path({"rooms", "Eros", "messages"});

  EXPECT_EQ(path[0], "rooms");
  EXPECT_EQ(path[1], "Eros");
  EXPECT_EQ(path[2], "messages");
}

TEST(FieldPathPortableTest, Comparison) {
  const FieldPathPortable abc({"a", "b", "c"});
  const FieldPathPortable abc2({"a", "b", "c"});
  const FieldPathPortable xyz({"x", "y", "z"});
  EXPECT_EQ(abc, abc2);
  EXPECT_NE(abc, xyz);

  const FieldPathPortable empty({});
  const FieldPathPortable a({"a"});
  const FieldPathPortable b({"b"});
  const FieldPathPortable ab({"a", "b"});

  EXPECT_TRUE(empty < a);
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(a < ab);

  EXPECT_TRUE(a > empty);
  EXPECT_TRUE(b > a);
  EXPECT_TRUE(ab > a);
}

TEST(FieldPathPortableTest, CanonicalStringOfSubstring) {
  EXPECT_EQ(FieldPathPortable({"foo", "bar", "baz"}).CanonicalString(),
            "foo.bar.baz");
  EXPECT_EQ(FieldPathPortable({"foo", "bar"}).CanonicalString(), "foo.bar");
  EXPECT_EQ(FieldPathPortable({"foo"}).CanonicalString(), "foo");
  EXPECT_EQ(FieldPathPortable({}).CanonicalString(), "");
}

TEST(FieldPath, CanonicalStringEscaping) {
  // Should be escaped
  EXPECT_EQ(FieldPathPortable({"1"}).CanonicalString(), "`1`");
  EXPECT_EQ(FieldPathPortable({"1ab"}).CanonicalString(), "`1ab`");
  EXPECT_EQ(FieldPathPortable({"ab!"}).CanonicalString(), "`ab!`");
  EXPECT_EQ(FieldPathPortable({"/ab"}).CanonicalString(), "`/ab`");
  EXPECT_EQ(FieldPathPortable({"a#b"}).CanonicalString(), "`a#b`");
  EXPECT_EQ(FieldPathPortable({"foo", "", "bar"}).CanonicalString(),
            "foo.``.bar");

  // Should not be escaped
  EXPECT_EQ(FieldPathPortable({"_ab"}).CanonicalString(), "_ab");
  EXPECT_EQ(FieldPathPortable({"a1"}).CanonicalString(), "a1");
  EXPECT_EQ(FieldPathPortable({"a_"}).CanonicalString(), "a_");
}

TEST(FieldPathPortableTest, FromDotSeparatedString) {
  EXPECT_EQ(FieldPathPortable::FromDotSeparatedString("a"),
            FieldPathPortable({"a"}));
  EXPECT_EQ(FieldPathPortable::FromDotSeparatedString("foo"),
            FieldPathPortable({"foo"}));
  EXPECT_EQ(FieldPathPortable::FromDotSeparatedString("a.b"),
            FieldPathPortable({"a", "b"}));
  EXPECT_EQ(FieldPathPortable::FromDotSeparatedString("foo.bar"),
            FieldPathPortable({"foo", "bar"}));
  EXPECT_EQ(FieldPathPortable::FromDotSeparatedString("foo.bar.baz"),
            FieldPathPortable({"foo", "bar", "baz"}));
}

#if FIRESTORE_HAVE_EXCEPTIONS
// When exceptions are disabled, invalid input will crash the process, but our
// gtest integration into the Java testing harness makes this impossible to
// record.

TEST(FieldPathPortableDeathTest, FromDotSeparatedStringParseFailures) {
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString(""),
               std::invalid_argument);
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString("."),
               std::invalid_argument);
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString(".."),
               std::invalid_argument);
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString(".foo"),
               std::invalid_argument);
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString("foo."),
               std::invalid_argument);
  EXPECT_THROW(FieldPathPortable::FromDotSeparatedString("foo..bar"),
               std::invalid_argument);
}

#endif  // FIRESTORE_HAVE_EXCEPTIONS

TEST(FieldPathPortableTest, KeyFieldPath) {
  const auto& key_field_path = FieldPathPortable::KeyFieldPath();
  EXPECT_TRUE(key_field_path.IsKeyFieldPath());
  EXPECT_EQ(key_field_path, FieldPathPortable{key_field_path});
  EXPECT_EQ(key_field_path.CanonicalString(), "__name__");
  EXPECT_EQ(key_field_path,
            FieldPathPortable::FromDotSeparatedString("__name__"));
  EXPECT_NE(key_field_path, FieldPathPortable::FromDotSeparatedString(
                                key_field_path.CanonicalString().substr(1)));
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
