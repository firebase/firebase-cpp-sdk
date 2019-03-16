/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_JSON_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_JSON_H_

#include <string>
#include "app/rest/request.h"
#include "app/rest/util.h"
#include "app/src/assert.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace rest {

// HTTP/REST request with Content-Type: application/json. FbsType is FlatBuffer
// type that contains the application data. Before the conversion between JSON
// and FlexBuffers are supported, we need to specify FlatBuffers type here.
//
// See request_json_test.cc for an example using this template class.
// Here we inline everything in header because this is a template class.
template <typename FbsType, typename FbsTypeT>
class RequestJson : public Request {
 public:
  // Constructs from a FlatBuffer schema, which should match FbsType.
  explicit RequestJson(const char* schema) : application_data_(new FbsTypeT()) {
    flatbuffers::IDLOptions fbs_options;
    fbs_options.skip_unexpected_fields_in_json = true;
    parser_.reset(new flatbuffers::Parser(fbs_options));

    bool parse_status = parser_->Parse(schema);
    FIREBASE_ASSERT_MESSAGE(parse_status, parser_->error_.c_str());

    set_method(util::kPost);
    add_header(util::kContentType, util::kApplicationJson);
  }

  // Constructs from a FlatBuffer schema, which should match FbsType.
  explicit RequestJson(const unsigned char* schema)
      : RequestJson(reinterpret_cast<const char*>(schema)) {}

 protected:
  // Updates POST field from application data.
  virtual void UpdatePostFields() {
    // Build FlatBuffer from application data object.
    flatbuffers::FlatBufferBuilder builder;
    builder.Finish(FbsType::Pack(builder, application_data_.get()));

    // Generate JSON string.
    std::string json;
    bool generate_status =
        GenerateText(*parser_, builder.GetBufferPointer(), &json);
    FIREBASE_ASSERT_RETURN_VOID(generate_status);

    set_post_fields(json.c_str());
  }

  // The FlatBuffer parser used to prepare the request JSON string.
  flatbuffers::unique_ptr<flatbuffers::Parser> parser_;

  // The application data in a request is stored here.
  flatbuffers::unique_ptr<FbsTypeT> application_data_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_JSON_H_
