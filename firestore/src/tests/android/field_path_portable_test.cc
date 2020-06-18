#include "firestore/src/android/field_path_portable.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

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

TEST(FieldPathPortableTest, Parsing) {
  EXPECT_EQ(FieldPathPortable::FromServerFormat("foo"),
            FieldPathPortable({"foo"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat("foo.bar"),
            FieldPathPortable({"foo", "bar"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat("foo.bar.baz"),
            FieldPathPortable({"foo", "bar", "baz"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat(R"(`.foo\\`)"),
            FieldPathPortable({".foo\\"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat(R"(`.foo\\`.`.foo`)"),
            FieldPathPortable({".foo\\", ".foo"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat(R"(foo.`\``.bar)"),
            FieldPathPortable({"foo", "`", "bar"}));
  EXPECT_EQ(FieldPathPortable::FromServerFormat(R"(foo\.bar)"),
            FieldPathPortable({"foo.bar"}));
}

// This is a special case in C++: std::string may contain embedded nulls. To
// fully mimic behavior of Objective-C code, parsing must terminate upon
// encountering the first null terminator in the string.
TEST(FieldPathPortableTest, ParseEmbeddedNull) {
  std::string str{"foo"};
  str += '\0';
  str += ".bar";

  const auto path = FieldPathPortable::FromServerFormat(str);
  EXPECT_EQ(path.size(), 1u);
  EXPECT_EQ(path.CanonicalString(), "foo");
}

TEST(FieldPathPortableDeathTest, ParseFailures) {
  EXPECT_DEATH(FieldPathPortable::FromServerFormat(""), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("."), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat(".."), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("foo."), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat(".bar"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("foo..bar"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat(R"(foo\)"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat(R"(foo.\)"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("foo`"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("foo```"), "");
  EXPECT_DEATH(FieldPathPortable::FromServerFormat("`foo"), "");
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

TEST(FieldPathPortableDeathTest, FromDotSeparatedStringParseFailures) {
  EXPECT_DEATH(FieldPathPortable::FromDotSeparatedString(""), "");
  EXPECT_DEATH(FieldPathPortable::FromDotSeparatedString("."), "");
  EXPECT_DEATH(FieldPathPortable::FromDotSeparatedString(".foo"), "");
  EXPECT_DEATH(FieldPathPortable::FromDotSeparatedString("foo."), "");
  EXPECT_DEATH(FieldPathPortable::FromDotSeparatedString("foo..bar"), "");
}

TEST(FieldPathPortableTest, KeyFieldPath) {
  const auto& key_field_path = FieldPathPortable::KeyFieldPath();
  EXPECT_TRUE(key_field_path.IsKeyFieldPath());
  EXPECT_EQ(key_field_path, FieldPathPortable{key_field_path});
  EXPECT_EQ(key_field_path.CanonicalString(), "__name__");
  EXPECT_EQ(key_field_path, FieldPathPortable::FromServerFormat("__name__"));
  EXPECT_NE(key_field_path, FieldPathPortable::FromServerFormat(
                                key_field_path.CanonicalString().substr(1)));
}

}  // namespace firestore
}  // namespace firebase
