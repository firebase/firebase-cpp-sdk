/*
* Copyright 2023 Google LLC
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

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

namespace firebase {
namespace firestore {
namespace {

using FilterTest = FirestoreIntegrationTest;

TEST_F(FilterTest, IdenticalShouldBeEqual) {
  FieldPath foo_path{std::vector<std::string>{"foo"}};

  Filter filter1a = Filter::ArrayContains("foo", FieldValue::Integer(42));
  Filter filter1b = Filter::ArrayContains(foo_path, FieldValue::Integer(42));

  Filter filter2a = Filter::ArrayContainsAny("foo", std::vector<FieldValue>{FieldValue::Integer(42)});
  Filter filter2b = Filter::ArrayContainsAny(foo_path, std::vector<FieldValue>{FieldValue::Integer(42)});

  Filter filter3a = Filter::EqualTo("foo", FieldValue::Integer(42));
  Filter filter3b = Filter::EqualTo(foo_path, FieldValue::Integer(42));

  Filter filter4a = Filter::NotEqualTo("foo", FieldValue::Integer(42));
  Filter filter4b = Filter::NotEqualTo(foo_path, FieldValue::Integer(42));

  Filter filter5a = Filter::GreaterThan("foo", FieldValue::Integer(42));
  Filter filter5b = Filter::GreaterThan(foo_path, FieldValue::Integer(42));

  Filter filter6a = Filter::GreaterThanOrEqualTo("foo", FieldValue::Integer(42));
  Filter filter6b = Filter::GreaterThanOrEqualTo(foo_path, FieldValue::Integer(42));

  Filter filter7a = Filter::LessThan("foo", FieldValue::Integer(42));
  Filter filter7b = Filter::LessThan(foo_path, FieldValue::Integer(42));

  Filter filter8a = Filter::LessThanOrEqualTo("foo", FieldValue::Integer(42));
  Filter filter8b = Filter::LessThanOrEqualTo(foo_path, FieldValue::Integer(42));

  Filter filter9a = Filter::In("foo", std::vector<FieldValue>{FieldValue::Integer(42)});
  Filter filter9b = Filter::In(foo_path, std::vector<FieldValue>{FieldValue::Integer(42)});

  Filter filter10a = Filter::NotIn("foo", std::vector<FieldValue>{FieldValue::Integer(42)});
  Filter filter10b = Filter::NotIn(foo_path, std::vector<FieldValue>{FieldValue::Integer(42)});

  EXPECT_TRUE(filter1a == filter1a);
  EXPECT_TRUE(filter2a == filter2a);
  EXPECT_TRUE(filter3a == filter3a);
  EXPECT_TRUE(filter4a == filter4a);
  EXPECT_TRUE(filter5a == filter5a);
  EXPECT_TRUE(filter6a == filter6a);
  EXPECT_TRUE(filter7a == filter7a);
  EXPECT_TRUE(filter8a == filter8a);
  EXPECT_TRUE(filter9a == filter9a);
  EXPECT_TRUE(filter10a == filter10a);

  EXPECT_TRUE(filter1a == filter1b);
  EXPECT_TRUE(filter2a == filter2b);
  EXPECT_TRUE(filter3a == filter3b);
  EXPECT_TRUE(filter4a == filter4b);
  EXPECT_TRUE(filter5a == filter5b);
  EXPECT_TRUE(filter6a == filter6b);
  EXPECT_TRUE(filter7a == filter7b);
  EXPECT_TRUE(filter8a == filter8b);
  EXPECT_TRUE(filter9a == filter9b);
  EXPECT_TRUE(filter10a == filter10b);

  EXPECT_FALSE(filter1a != filter1a);
  EXPECT_FALSE(filter2a != filter2a);
  EXPECT_FALSE(filter3a != filter3a);
  EXPECT_FALSE(filter4a != filter4a);
  EXPECT_FALSE(filter5a != filter5a);
  EXPECT_FALSE(filter6a != filter6a);
  EXPECT_FALSE(filter7a != filter7a);
  EXPECT_FALSE(filter8a != filter8a);
  EXPECT_FALSE(filter9a != filter9a);
  EXPECT_FALSE(filter10a != filter10a);

  EXPECT_FALSE(filter1a != filter1b);
  EXPECT_FALSE(filter2a != filter2b);
  EXPECT_FALSE(filter3a != filter3b);
  EXPECT_FALSE(filter4a != filter4b);
  EXPECT_FALSE(filter5a != filter5b);
  EXPECT_FALSE(filter6a != filter6b);
  EXPECT_FALSE(filter7a != filter7b);
  EXPECT_FALSE(filter8a != filter8b);
  EXPECT_FALSE(filter9a != filter9b);
  EXPECT_FALSE(filter10a != filter10b);

  EXPECT_TRUE(filter1a != filter2a);
  EXPECT_TRUE(filter1a != filter3a);
  EXPECT_TRUE(filter1a != filter4a);
  EXPECT_TRUE(filter1a != filter5a);
  EXPECT_TRUE(filter1a != filter6a);
  EXPECT_TRUE(filter1a != filter7a);
  EXPECT_TRUE(filter1a != filter8a);
  EXPECT_TRUE(filter1a != filter9a);
  EXPECT_TRUE(filter1a != filter10a);
  EXPECT_TRUE(filter2a != filter3a);
  EXPECT_TRUE(filter2a != filter4a);
  EXPECT_TRUE(filter2a != filter5a);
  EXPECT_TRUE(filter2a != filter6a);
  EXPECT_TRUE(filter2a != filter7a);
  EXPECT_TRUE(filter2a != filter8a);
  EXPECT_TRUE(filter2a != filter9a);
  EXPECT_TRUE(filter2a != filter10a);
  EXPECT_TRUE(filter3a != filter4a);
  EXPECT_TRUE(filter3a != filter5a);
  EXPECT_TRUE(filter3a != filter6a);
  EXPECT_TRUE(filter3a != filter7a);
  EXPECT_TRUE(filter3a != filter8a);
  EXPECT_TRUE(filter3a != filter9a);
  EXPECT_TRUE(filter3a != filter10a);
  EXPECT_TRUE(filter4a != filter5a);
  EXPECT_TRUE(filter4a != filter6a);
  EXPECT_TRUE(filter4a != filter7a);
  EXPECT_TRUE(filter4a != filter8a);
  EXPECT_TRUE(filter4a != filter9a);
  EXPECT_TRUE(filter4a != filter10a);
  EXPECT_TRUE(filter5a != filter6a);
  EXPECT_TRUE(filter5a != filter7a);
  EXPECT_TRUE(filter5a != filter8a);
  EXPECT_TRUE(filter5a != filter9a);
  EXPECT_TRUE(filter5a != filter10a);
  EXPECT_TRUE(filter6a != filter7a);
  EXPECT_TRUE(filter6a != filter8a);
  EXPECT_TRUE(filter6a != filter9a);
  EXPECT_TRUE(filter6a != filter10a);
  EXPECT_TRUE(filter7a != filter8a);
  EXPECT_TRUE(filter7a != filter9a);
  EXPECT_TRUE(filter7a != filter10a);
  EXPECT_TRUE(filter8a != filter9a);
  EXPECT_TRUE(filter8a != filter10a);
  EXPECT_TRUE(filter9a != filter10a);
}

TEST_F(FilterTest, DifferentValuesAreNotEqual) {
  Filter filter1a = Filter::ArrayContains("foo", FieldValue::Integer(24));
  Filter filter1b = Filter::ArrayContains("foo", FieldValue::Integer(42));

  Filter filter2a = Filter::EqualTo("foo", FieldValue::Integer(24));
  Filter filter2b = Filter::EqualTo("foo", FieldValue::Integer(42));

  Filter filter3a = Filter::NotEqualTo("foo", FieldValue::Integer(24));
  Filter filter3b = Filter::NotEqualTo("foo", FieldValue::Integer(42));

  Filter filter4a = Filter::GreaterThan("foo", FieldValue::Integer(24));
  Filter filter4b = Filter::GreaterThan("foo", FieldValue::Integer(42));

  Filter filter5a = Filter::GreaterThanOrEqualTo("foo", FieldValue::Integer(24));
  Filter filter5b = Filter::GreaterThanOrEqualTo("foo", FieldValue::Integer(42));

  Filter filter6a = Filter::LessThan("foo", FieldValue::Integer(24));
  Filter filter6b = Filter::LessThan("foo", FieldValue::Integer(42));

  Filter filter7a = Filter::LessThanOrEqualTo("foo", FieldValue::Integer(24));
  Filter filter7b = Filter::LessThanOrEqualTo("foo", FieldValue::Integer(42));

  EXPECT_FALSE(filter1a == filter1b);
  EXPECT_FALSE(filter2a == filter2b);
  EXPECT_FALSE(filter3a == filter3b);
  EXPECT_FALSE(filter4a == filter4b);
  EXPECT_FALSE(filter5a == filter5b);
  EXPECT_FALSE(filter6a == filter6b);
  EXPECT_FALSE(filter7a == filter7b);

  EXPECT_TRUE(filter1a != filter1b);
  EXPECT_TRUE(filter2a != filter2b);
  EXPECT_TRUE(filter3a != filter3b);
  EXPECT_TRUE(filter4a != filter4b);
  EXPECT_TRUE(filter5a != filter5b);
  EXPECT_TRUE(filter6a != filter6b);
  EXPECT_TRUE(filter7a != filter7b);
}

TEST_F(FilterTest, DifferentOrderOfValuesAreNotEqual) {
  const std::vector<FieldValue>& valueOrderA = std::vector<FieldValue>{
      FieldValue::Integer(1),
      FieldValue::Integer(2)
  };
  const std::vector<FieldValue>& valueOrderB = std::vector<FieldValue>{
      FieldValue::Integer(2),
      FieldValue::Integer(1)
  };

  {
    Filter filter1 = Filter::ArrayContainsAny("foo", valueOrderA);
    Filter filter2 = Filter::ArrayContainsAny("foo", valueOrderB);
    EXPECT_FALSE(filter1 == filter2);
    EXPECT_TRUE(filter1 != filter2);
  }

  {
    Filter filter1 = Filter::In("foo", valueOrderA);
    Filter filter2 = Filter::In("foo", valueOrderB);
    EXPECT_FALSE(filter1 == filter2);
    EXPECT_TRUE(filter1 != filter2);
  }

  {
    Filter filter1 = Filter::NotIn("foo", valueOrderA);
    Filter filter2 = Filter::NotIn("foo", valueOrderB);
    EXPECT_FALSE(filter1 == filter2);
    EXPECT_TRUE(filter1 != filter2);
  }
}

}

}  // namespace firestore
}  // namespace firebase
