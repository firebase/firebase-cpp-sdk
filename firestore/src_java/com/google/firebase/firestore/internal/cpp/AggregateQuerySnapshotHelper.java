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

package com.google.firebase.firestore.internal.cpp;

import com.google.firestore.v1.Value;
import java.util.HashMap;
import java.util.Map;

public final class AggregateQuerySnapshotHelper {
  private AggregateQuerySnapshotHelper() {}

  /**
   * Creates an object appropriate for specifying to the AggregateQuerySnapshot
   * constructor that conveys the given "count" as the lone aggregate result.
   *
   * This class should be deleted and replaced with a proper mechanism once
   * SUM/AVERAGE are ported to this SDK.
   */
  public static Map<String, Value> createAggregateQuerySnapshotConstructorArg(
      long count) {
    HashMap<String, Value> map = new HashMap<>();
    map.put("count", Value.newBuilder().setIntegerValue(count).build());
    return map;
  }
}
