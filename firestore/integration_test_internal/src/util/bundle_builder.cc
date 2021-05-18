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
#include "util/bundle_builder.h"

#include <utility>
#include <vector>

namespace firebase {
namespace firestore {
namespace {

// Adapted from
// https://stackoverflow.com/questions/2896600/how-to-replace-all-occurrences-of-a-character-in-string
std::string ReplaceAll(std::string str,
                       const std::string& from,
                       const std::string& to) {
  if (from.empty()) return str;
  std::string::size_type start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();  // In case 'to' contains 'from', like replacing
                               // 'x' with 'yx'
  }

  return str;
}

std::vector<std::string> BundleTemplate() {
  std::string metadata =
      R"|({
   "metadata":{
      "id":"test-bundle",
      "createTime":{
         "seconds":1001,
         "nanos":9999
      },
      "version":1,
      "totalDocuments":2,
      "totalBytes":{totalBytes}
   }
})|";

  std::string named_query1 =
      R"|({
   "namedQuery":{
      "name":"limit",
      "readTime":{
         "seconds":1000,
         "nanos":9999
      },
      "bundledQuery":{
         "parent":"projects/{projectId}/databases/(default)/documents",
         "structuredQuery":{
            "from":[
               {
                  "collectionId":"coll-1"
               }
            ],
            "orderBy":[
               {
                  "field":{
                     "fieldPath":"bar"
                  },
                  "direction":"DESCENDING"
               },
               {
                  "field":{
                     "fieldPath":"__name__"
                  },
                  "direction":"DESCENDING"
               }
            ],
            "limit":{
               "value":1
            }
         },
         "limitType":"FIRST"
      }
   }
})|";

  std::string named_query2 =
      R"|({
   "namedQuery":{
      "name":"limit-to-last",
      "readTime":{
         "seconds":1000,
         "nanos":9999
      },
      "bundledQuery":{
         "parent":"projects/{projectId}/databases/(default)/documents",
         "structuredQuery":{
            "from":[
               {
                  "collectionId":"coll-1"
               }
            ],
            "orderBy":[
               {
                  "field":{
                     "fieldPath":"bar"
                  },
                  "direction":"DESCENDING"
               },
               {
                  "field":{
                     "fieldPath":"__name__"
                  },
                  "direction":"DESCENDING"
               }
            ],
            "limit":{
               "value":1
            }
         },
         "limitType":"LAST"
      }
   }
})|";

  std::string document_metadata1 =
      R"|({
   "documentMetadata":{
      "name":"projects/{projectId}/databases/(default)/documents/coll-1/a",
      "readTime":{
         "seconds":1000,
         "nanos":9999
      },
      "exists":true
   }
})|";

  std::string document_1 =
      R"|({
   "document":{
      "name":"projects/{projectId}/databases/(default)/documents/coll-1/a",
      "createTime":{
         "seconds":1,
         "nanos":9
      },
      "updateTime":{
         "seconds":1,
         "nanos":9
      },
      "fields":{
         "k":{
            "stringValue":"a"
         },
         "bar":{
            "integerValue":1
         }
      }
   }
})|";

  std::string document_metadata2 =
      R"|({
   "documentMetadata":{
      "name":"projects/{projectId}/databases/(default)/documents/coll-1/b",
      "readTime":{
         "seconds":1000,
         "nanos":9999
      },
      "exists":true
   }
})|";

  std::string document_2 =
      R"|({
   "document":{
      "name":"projects/{projectId}/databases/(default)/documents/coll-1/b",
      "createTime":{
         "seconds":1,
         "nanos":9
      },
      "updateTime":{
         "seconds":1,
         "nanos":9
      },
      "fields":{
         "k":{
            "stringValue":"b"
         },
         "bar":{
            "integerValue":2
         }
      }
   }
})|";

  return {metadata,   named_query1,       named_query2, document_metadata1,
          document_1, document_metadata2, document_2};
}

}  // namespace

std::string CreateBundle(const std::string& project_id) {
  std::string bundle;

  auto bundle_template = BundleTemplate();
  for (size_t i = 1; i < bundle_template.size(); ++i) {
    std::string element =
        ReplaceAll(bundle_template[i], "{projectId}", project_id);
    bundle.append(std::to_string(element.size()));
    bundle.append(element);
  }

  std::string metadata = ReplaceAll(bundle_template[0], "{totalBytes}",
                                    std::to_string(bundle.size()));
  return std::to_string(metadata.size()) + metadata + bundle;
}

}  // namespace firestore
}  // namespace firebase
