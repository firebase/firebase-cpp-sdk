// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/mutable_data_desktop.h"

#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Eq;

namespace firebase {
namespace database {
namespace internal {

TEST(MutableDataTest, TestBasic) {
  {
    MutableDataInternal data(nullptr, Variant::Null());
    EXPECT_THAT(data.GetChildren().size(), Eq(0));
    EXPECT_THAT(data.GetChildrenCount(), Eq(0));
    EXPECT_THAT(data.GetKeyString(), Eq(""));
    EXPECT_THAT(data.GetValue(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_FALSE(data.HasChild("A"));
  }

  {
    MutableDataInternal data(nullptr, Variant(10));
    EXPECT_THAT(data.GetChildren().size(), Eq(0));
    EXPECT_THAT(data.GetChildrenCount(), Eq(0));
    EXPECT_THAT(data.GetKeyString(), Eq(""));
    EXPECT_THAT(data.GetValue(), Eq(Variant(10)));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_FALSE(data.HasChild("A"));
  }

  {
    MutableDataInternal data(
        nullptr, util::JsonToVariant("{\".value\":10,\".priority\":1}"));
    EXPECT_THAT(data.GetChildren().size(), Eq(0));
    EXPECT_THAT(data.GetChildrenCount(), Eq(0));
    EXPECT_THAT(data.GetKeyString(), Eq(""));
    EXPECT_THAT(data.GetValue(), Eq(Variant(10)));
    EXPECT_THAT(data.GetPriority(), Eq(Variant(1)));
    EXPECT_FALSE(data.HasChild("A"));
  }

  {
    MutableDataInternal data(
        nullptr,
        util::JsonToVariant("{\"A\":{\"B\":{\"C\":10}},\".priority\":1}"));
    EXPECT_THAT(data.GetChildren().size(), Eq(1));
    EXPECT_THAT(data.GetChildrenCount(), Eq(1));
    EXPECT_THAT(data.GetKeyString(), Eq(""));
    EXPECT_THAT(data.GetValue(),
                Eq(util::JsonToVariant("{\"A\":{\"B\":{\"C\":10}}}")));
    EXPECT_THAT(data.GetPriority(), Eq(Variant(1)));
    EXPECT_TRUE(data.HasChild("A"));
    EXPECT_TRUE(data.HasChild("A/B"));
    EXPECT_TRUE(data.HasChild("A/B/C"));
    EXPECT_FALSE(data.HasChild("A/B/C/D"));
    EXPECT_FALSE(data.HasChild("D"));

    auto child_a = data.Child("A");
    EXPECT_THAT(child_a->GetChildren().size(), Eq(1));
    EXPECT_THAT(child_a->GetChildrenCount(), Eq(1));
    EXPECT_THAT(child_a->GetKeyString(), Eq("A"));
    EXPECT_THAT(child_a->GetValue(),
                Eq(util::JsonToVariant("{\"B\":{\"C\":10}}")));
    EXPECT_THAT(child_a->GetPriority(), Eq(Variant::Null()));
    EXPECT_TRUE(child_a->HasChild("B"));
    EXPECT_TRUE(child_a->HasChild("B/C"));
    EXPECT_FALSE(child_a->HasChild("B/C/D"));
    EXPECT_FALSE(child_a->HasChild("D"));

    delete child_a;
  }
}

TEST(MutableDataTest, TestWrite) {
  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetValue(Variant(10));
    EXPECT_THAT(data.GetValue(), Eq(Variant(10)));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetHolder(), Eq(Variant(10)));
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetPriority(Variant(1));
    EXPECT_THAT(data.GetValue(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetHolder(), Eq(Variant::Null()));
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetValue(Variant(10));
    data.SetPriority(Variant(1));
    EXPECT_THAT(data.GetValue(), Eq(Variant(10)));
    EXPECT_THAT(data.GetPriority(), Eq(Variant(1)));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\".priority\":1,\".value\":10}")));
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetPriority(Variant(1));
    data.SetValue(Variant(10));
    EXPECT_THAT(data.GetValue(), Eq(Variant(10)));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetHolder(), Eq(util::JsonToVariant("10")));
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetValue(util::JsonToVariant("{\"A\":10,\"B\":20}"));
    data.SetPriority(Variant(1));
    EXPECT_THAT(data.GetValue(),
                Eq(util::JsonToVariant("{\"A\":10,\"B\":20}")));
    EXPECT_THAT(data.GetPriority(), Eq(Variant(1)));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\".priority\":1,\"A\":10,\"B\":20}")));
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetPriority(Variant(1));
    data.SetValue(util::JsonToVariant("{\"A\":10,\"B\":20}"));
    EXPECT_THAT(data.GetValue(),
                Eq(util::JsonToVariant("{\"A\":10,\"B\":20}")));
    EXPECT_THAT(data.GetPriority(), Eq(Variant::Null()));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\"A\":10,\"B\":20}")));
  }
}

TEST(MutableDataTest, TestChild) {
  {
    MutableDataInternal data(nullptr, Variant::Null());
    auto child_a = data.Child("A");
    child_a->SetValue(Variant(10));
    auto child_b = data.Child("B");
    child_b->SetValue(Variant(20));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\"A\":10,\"B\":20}")));

    delete child_a;
    delete child_b;
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    auto child_a = data.Child("A");
    child_a->SetValue(Variant(10));
    auto child_b = child_a->Child("B");
    child_b->SetValue(Variant(20));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\"A\":{\"B\":20}}")));

    delete child_a;
    delete child_b;
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    auto child = data.Child("A/B");
    child->SetValue(Variant(20));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\"A\":{\"B\":20}}")));

    delete child;
  }

  {
    MutableDataInternal data(nullptr, Variant::Null());
    auto child_1 = data.Child("A/B/C");
    child_1->SetValue(Variant(20));
    child_1->SetPriority(Variant(3));
    auto child_2 = data.Child("A");
    child_2->SetPriority(Variant(2));
    data.SetPriority(Variant(1));
    EXPECT_THAT(
        data.GetHolder(),
        Eq(util::JsonToVariant("{\".priority\":1,\"A\":{\".priority\":2,\"B\":{"
                               "\"C\":{\".priority\":3,\".value\":20}}}}")));

    delete child_1;
    delete child_2;
  }

  {
    // Test GetValue() to convert applicable map to vector
    MutableDataInternal data(nullptr, Variant::Null());
    auto child_1 = data.Child("0");
    child_1->SetValue(0);
    auto child_2 = data.Child("2");
    child_2->SetValue(2);
    child_2->SetPriority(20);
    data.SetPriority(1);
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\".priority\":1,\"0\":0,\"2\":{\"."
                                       "value\":2,\".priority\":20}}")));
    EXPECT_THAT(data.GetValue(), Eq(util::JsonToVariant("[0,null,2]")));

    delete child_1;
    delete child_2;
  }

  {
    // Set value with vector and priority
    MutableDataInternal data(nullptr, Variant::Null());
    data.SetValue(
        util::JsonToVariant("{\".priority\":1,\".value\":[0,null,{\".value\":2,"
                            "\".priority\":20}]}"));
    EXPECT_THAT(data.GetHolder(),
                Eq(util::JsonToVariant("{\".priority\":1,\"0\":0,\"2\":{\"."
                                       "value\":2,\".priority\":20}}")));
    EXPECT_THAT(data.GetValue(), Eq(util::JsonToVariant("[0,null,2]")));
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
