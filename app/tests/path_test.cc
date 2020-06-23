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

#include "app/src/path.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {

using ::firebase::Optional;
using ::firebase::Path;
using ::testing::Eq;
using ::testing::StrEq;

TEST(PathTests, DefaultConstructor) {
  Path path;
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_TRUE(path.empty());
}

TEST(PathTests, StringConstructor) {
  Path path;

  // Empty string
  path = Path("");
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_THAT(path.str(), StrEq(""));
  EXPECT_THAT(path.c_str(), StrEq(""));
  EXPECT_TRUE(path.empty());

  // Root folder
  path = Path("/");
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_THAT(path.str(), StrEq(""));
  EXPECT_THAT(path.c_str(), StrEq(""));
  EXPECT_TRUE(path.empty());

  // Root Folder with plenty slashes
  path = Path("//////");
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_THAT(path.str(), StrEq(""));
  EXPECT_THAT(path.c_str(), StrEq(""));
  EXPECT_TRUE(path.empty());

  // Correctly formatted string.
  path = Path("test/foo/bar");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Leading slash.
  path = Path("/test/foo/bar");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Trailing slash.
  path = Path("test/foo/bar/");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Leading and trailing slash.
  path = Path("/test/foo/bar/");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Internal slashes.
  path = Path("/test/////foo/bar");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Slashes everywhere!
  path = Path("///test/////foo//bar///");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Backslashes.
  path = Path("///test\\foo\\bar///");
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq("test\\foo\\bar"));
  EXPECT_THAT(path.str(), StrEq("test\\foo\\bar"));
  EXPECT_THAT(path.c_str(), StrEq("test\\foo\\bar"));
  EXPECT_FALSE(path.empty());
}

TEST(PathTests, VectorConstructor) {
  Path path;
  std::vector<std::string> directories;

  // Directories with no slashes.
  directories = {"test", "foo", "bar"};
  path = Path(directories);
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Directories with extraneous slashes.
  directories = {"/test/", "/foo", "bar/"};
  path = Path(directories);
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Multiple directories being added in one string.
  directories = {"test/foo", "bar"};
  path = Path(directories);
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Multiple directories being added in one string with extraneous slashes.
  directories = {"/test/", "/foo/bar/"};
  path = Path(directories);
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());
}

TEST(PathTests, VectorIteratorConstructor) {
  Path path;
  std::vector<std::string> directories;

  // Directories with no slashes.
  directories = {"test", "foo", "bar"};
  path = Path(directories.begin(), directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Directories with extraneous slashes.
  directories = {"/test/", "/foo", "bar/"};
  path = Path(directories.begin(), directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Multiple directories being added in one string.
  directories = {"test/foo", "bar"};
  path = Path(directories.begin(), directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Multiple directories being added in one string with extraneous slashes.
  directories = {"/test/", "/foo/bar/"};
  path = Path(directories.begin(), directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  // Directories with no slashes, starting from the second element.
  directories = {"test", "foo", "bar"};
  path = Path(++directories.begin(), directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("foo/bar"));
  EXPECT_FALSE(path.empty());

  // Directories with no slashes, ending before the last element.
  directories = {"test", "foo", "bar"};
  path = Path(directories.begin(), --directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq("test"));
  EXPECT_THAT(path.GetBaseName(), StrEq("foo"));
  EXPECT_THAT(path.str(), StrEq("test/foo"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo"));
  EXPECT_FALSE(path.empty());

  // Directories with no slashes, starting from the second element and ending
  // before the last element.
  directories = {"test", "foo", "bar"};
  path = Path(++directories.begin(), --directories.end());
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq("foo"));
  EXPECT_THAT(path.str(), StrEq("foo"));
  EXPECT_THAT(path.c_str(), StrEq("foo"));
  EXPECT_FALSE(path.empty());

  // Starting and ending at the sample place.
  directories = {"test", "foo", "bar"};
  path = Path(directories.begin(), directories.begin());
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_THAT(path.str(), StrEq(""));
  EXPECT_THAT(path.c_str(), StrEq(""));
  EXPECT_TRUE(path.empty());
}

TEST(PathTests, GetParent) {
  Path path;

  path = Path("/test/foo/bar");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo"));
  EXPECT_THAT(path.GetBaseName(), StrEq("bar"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar"));
  EXPECT_FALSE(path.empty());

  path = path.GetParent();
  EXPECT_THAT(path.GetParent().str(), StrEq("test"));
  EXPECT_THAT(path.GetBaseName(), StrEq("foo"));
  EXPECT_THAT(path.str(), StrEq("test/foo"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo"));
  EXPECT_FALSE(path.empty());

  path = path.GetParent();
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq("test"));
  EXPECT_THAT(path.str(), StrEq("test"));
  EXPECT_THAT(path.c_str(), StrEq("test"));
  EXPECT_FALSE(path.empty());

  path = path.GetParent();
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq(""));
  EXPECT_THAT(path.str(), StrEq(""));
  EXPECT_THAT(path.c_str(), StrEq(""));
  EXPECT_TRUE(path.empty());
}

TEST(PathTests, GetChildWithString) {
  Path path;

  path = path.GetChild("test");
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq("test"));
  EXPECT_THAT(path.str(), StrEq("test"));
  EXPECT_THAT(path.c_str(), StrEq("test"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild("foo");
  EXPECT_THAT(path.GetParent().str(), StrEq("test"));
  EXPECT_THAT(path.GetBaseName(), StrEq("foo"));
  EXPECT_THAT(path.str(), StrEq("test/foo"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild("bar/baz");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.GetBaseName(), StrEq("baz"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar/baz"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar/baz"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild("///quux///quaaz///");
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo/bar/baz/quux"));
  EXPECT_THAT(path.GetBaseName(), StrEq("quaaz"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar/baz/quux/quaaz"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar/baz/quux/quaaz"));
  EXPECT_FALSE(path.empty());
}

TEST(PathTests, GetChildWithPath) {
  Path path;

  path = path.GetChild(Path("test"));
  EXPECT_THAT(path.GetParent().str(), StrEq(""));
  EXPECT_THAT(path.GetBaseName(), StrEq("test"));
  EXPECT_THAT(path.str(), StrEq("test"));
  EXPECT_THAT(path.c_str(), StrEq("test"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild(Path("foo"));
  EXPECT_THAT(path.GetParent().str(), StrEq("test"));
  EXPECT_THAT(path.GetBaseName(), StrEq("foo"));
  EXPECT_THAT(path.str(), StrEq("test/foo"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild(Path("bar/baz"));
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo/bar"));
  EXPECT_THAT(path.GetBaseName(), StrEq("baz"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar/baz"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar/baz"));
  EXPECT_FALSE(path.empty());

  path = path.GetChild(Path("///quux///quaaz///"));
  EXPECT_THAT(path.GetParent().str(), StrEq("test/foo/bar/baz/quux"));
  EXPECT_THAT(path.GetBaseName(), StrEq("quaaz"));
  EXPECT_THAT(path.str(), StrEq("test/foo/bar/baz/quux/quaaz"));
  EXPECT_THAT(path.c_str(), StrEq("test/foo/bar/baz/quux/quaaz"));
  EXPECT_FALSE(path.empty());
}

TEST(PathTests, IsParent) {
  Path path("foo/bar/baz");

  EXPECT_TRUE(Path().IsParent(Path()));

  EXPECT_TRUE(Path().IsParent(path));
  EXPECT_TRUE(Path("foo").IsParent(path));
  EXPECT_TRUE(Path("foo/").IsParent(path));
  EXPECT_TRUE(Path("foo/bar").IsParent(path));
  EXPECT_TRUE(Path("foo/bar/").IsParent(path));
  EXPECT_TRUE(Path("foo/bar/baz").IsParent(path));
  EXPECT_TRUE(Path("foo/bar/baz/").IsParent(path));
  EXPECT_TRUE(path.IsParent(Path("foo/bar/baz")));
  EXPECT_TRUE(path.IsParent(Path("foo/bar/baz/")));
  EXPECT_FALSE(path.IsParent(Path("foo")));
  EXPECT_FALSE(path.IsParent(Path("foo/")));
  EXPECT_FALSE(path.IsParent(Path("foo/bar")));
  EXPECT_FALSE(path.IsParent(Path("foo/bar/")));

  EXPECT_FALSE(Path("completely/wrong").IsParent(path));
  EXPECT_FALSE(Path("f").IsParent(path));
  EXPECT_FALSE(Path("fo").IsParent(path));
  EXPECT_FALSE(Path("foo/b").IsParent(path));
  EXPECT_FALSE(Path("foo/ba").IsParent(path));
  EXPECT_FALSE(Path("foo/bar/b").IsParent(path));
  EXPECT_FALSE(Path("foo/bar/ba").IsParent(path));
  EXPECT_FALSE(Path("foo/bar/baz/q").IsParent(path));
  EXPECT_FALSE(Path("foo/bar/baz/quux").IsParent(path));
}

TEST(PathTests, GetDirectories) {
  std::vector<std::string> golden = {"foo", "bar", "baz"};
  Path path;

  path = Path("foo/bar/baz");
  EXPECT_THAT(path.GetDirectories(), Eq(golden));

  path = Path("//foo/bar///baz///");
  EXPECT_THAT(path.GetDirectories(), Eq(golden));
}

TEST(PathTests, FrontDirectory) {
  EXPECT_EQ(Path().FrontDirectory(), Path());
  EXPECT_EQ(Path("single_level").FrontDirectory(), Path("single_level"));
  EXPECT_EQ(Path("multi/level/directory/structure").FrontDirectory(),
            Path("multi"));
}

TEST(PathTests, PopFrontDirectory) {
  EXPECT_EQ(Path().PopFrontDirectory(), Path());
  EXPECT_EQ(Path("single_level").PopFrontDirectory(), Path());
  EXPECT_EQ(Path("multi/level/directory/structure").PopFrontDirectory(),
            Path("level/directory/structure"));
}

TEST(PathTests, GetRelative) {
  Path result;

  EXPECT_TRUE(
      Path::GetRelative(Path(""), Path("starting/from/empty/path"), &result));
  EXPECT_EQ(result, Path("starting/from/empty/path"));

  EXPECT_TRUE(Path::GetRelative(Path("a/b/c/d/e"),
                                Path("a/b/c/d/e/f/g/h/i/j/k"), &result));
  EXPECT_THAT(result.str(), StrEq("f/g/h/i/j/k"));

  EXPECT_TRUE(Path::GetRelative(
      Path("first_star/on_left"),
      Path("first_star/on_left/straight_on/till_morning"), &result));
  EXPECT_THAT(result.str(), StrEq("straight_on/till_morning"));

  result = Path("result/left/untouched");

  EXPECT_FALSE(Path::GetRelative(Path("some/overlap/but/failure"),
                                 Path("some/overlap/and/unsuccessful"),
                                 &result));
  EXPECT_THAT(result.str(), StrEq("result/left/untouched"));

  EXPECT_FALSE(Path::GetRelative(Path("no/overlap/at/all"),
                                 Path("apple/banana/carrot"), &result));
  EXPECT_THAT(result.str(), StrEq("result/left/untouched"));

  EXPECT_FALSE(Path::GetRelative(Path("the/longer/path/comes/first/now"),
                                 Path("the/longer/path"), &result));
  EXPECT_THAT(result.str(), StrEq("result/left/untouched"));
}

TEST(PathTests, GetRelativeOptional) {
  Optional<Path> result;

  result = Path::GetRelative(Path(""), Path("starting/from/empty/path"));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, Path("starting/from/empty/path"));

  result = Path::GetRelative(Path("a/b/c/d/e"), Path("a/b/c/d/e/f/g/h/i/j/k"));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, Path("f/g/h/i/j/k"));

  result =
      Path::GetRelative(Path("first_star/on_left"),
                        Path("first_star/on_left/straight_on/till_morning"));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, Path("straight_on/till_morning"));

  result = Path::GetRelative(Path("some/overlap/but/failure"),
                             Path("some/overlap/and/unsuccessful"));
  EXPECT_FALSE(result.has_value());

  result =
      Path::GetRelative(Path("no/overlap/at/all"), Path("apple/banana/carrot"));
  EXPECT_FALSE(result.has_value());

  result = Path::GetRelative(Path("the/longer/path/comes/first/now"),
                             Path("the/longer/path"));
  EXPECT_FALSE(result.has_value());
}

}  // namespace
