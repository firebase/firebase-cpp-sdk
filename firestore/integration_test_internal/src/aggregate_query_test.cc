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

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using AggrgegateQueryTest = FirestoreIntegrationTest;

size_t AggregateQueryHash(const AggregateQuery& aggregate_query) {
  return aggregate_query.Hash();
}

TEST_F(AggrgegateQueryTest, TestHashCode) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}}});
  Query query1 =
      collection.Limit(2).OrderBy("sort", Query::Direction::kAscending);
  Query query2 =
      collection.Limit(2).OrderBy("sort", Query::Direction::kDescending);
  EXPECT_NE(AggregateQueryHash(query1.Count()),
            AggregateQueryHash(query2.Count()));
  EXPECT_EQ(AggregateQueryHash(query1.Count()),
            AggregateQueryHash(query1.Count()));
}

}  // namespace firestore
}  // namespace firebase
