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

#ifndef FIREBASE_APP_REST_RESPONSE_JSON_H_
#define FIREBASE_APP_REST_RESPONSE_JSON_H_

#include <atomic>
#include <string>
#include <utility>

#include "app/rest/response.h"
#include "app/src/assert.h"
#include "app/src/log.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace rest {

// HTTP/REST response with Content-Type: application/json. FbsType is FlatBuffer
// type that contains the application data. Before the conversion between JSON
// and FlexBuffers are supported, we need to specify FlatBuffers type here.
//
// See response_json_test.cc for an example using this template class.
template <typename FbsType, typename FbsTypeT>
class ResponseJson : public Response {
 public:
  // Constructs from a FlatBuffer schema, which should match FbsType.
  explicit ResponseJson(const char* schema) : uid_(generate_uid()) {
    LogWarning("zzyzx ResponseJson(uid=%d) created via the normal constructor", uid_);
    flatbuffers::IDLOptions fbs_options;
    fbs_options.skip_unexpected_fields_in_json = true;
    parser_.reset(new flatbuffers::Parser(fbs_options));

    bool parse_status = parser_->Parse(schema);
    FIREBASE_ASSERT_MESSAGE(parse_status, parser_->error_.c_str());
  }

  // Constructs from a FlatBuffer schema, which should match FbsType.
  explicit ResponseJson(const unsigned char* schema)
      : ResponseJson(reinterpret_cast<const char*>(schema)) {}

  // Note: remove if support for Visual Studio <2015 is no longer
  // needed.
  // Prior to version 2015, Visual Studio didn't support implicitly
  // defined move constructors, so one has to be provided. Copy constructor is
  // implicitly deleted anyway (because parser_ and application_data_ are
  // non-copyable).
  ResponseJson(ResponseJson&& rhs)
      : Response(std::move(rhs)),
        uid_(generate_uid()),
        parser_(std::move(rhs.parser_)),
        application_data_(std::move(rhs.application_data_)) {
    LogWarning("zzyzx ResponseJson(uid=%d) created via the move constructor from ResponseJson(uid=%d); application_data_ is %s", uid_, rhs.uid_, application_data_ ? "null" : "non-null");
  }

  virtual ~ResponseJson() {
    LogWarning("zzyzx ResponseJson(uid=%d) destroyed", uid_);
  }

  // When transmission is completed, we parse the response JSON string.
  void MarkCompleted() override {
    LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted() start", uid_);
    // Body could be empty if request failed. Deal this case first since
    // flatbuffer parser does not allow empty input.
    if (strlen(GetBody()) == 0) {
      LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted(); strlen(GetBody()) == 0); setting application_data_ to new FbsTypeT()", uid_);
      application_data_.reset(new FbsTypeT());
      Response::MarkCompleted();
      return;
    }

    // Parse and verify JSON string in body. FlatBuffer parser does not support
    // online parsing. So we only parse the body when we get everything.
    bool parse_status = parser_->Parse(GetBody());
    LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted(); parse_status=%s", uid_, parse_status ? "true" : "false");
    FIREBASE_ASSERT_RETURN_VOID(parse_status);
    const flatbuffers::FlatBufferBuilder& builder = parser_->builder_;
    flatbuffers::Verifier verifier(builder.GetBufferPointer(),
                                   builder.GetSize());
    bool verify_status = verifier.VerifyBuffer<FbsType>(nullptr);
    LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted(); verify_status=%s", uid_, verify_status ? "true" : "false");
    FIREBASE_ASSERT_RETURN_VOID(verify_status);

    // UnPack application data object from FlatBuffer.
    const FbsType* body_fbs =
        flatbuffers::GetRoot<FbsType>(builder.GetBufferPointer());
    auto body_fbs_unpacked = body_fbs->UnPack();
    LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted(); body_fbs_unpacked is %s", uid_, body_fbs_unpacked ? "non-null" : "null");
    LogWarning("zzyzx ResponseJson(uid=%d).MarkCompleted(); setting application_data_ to %s", uid_, body_fbs_unpacked ? "non-null" : "null");
    application_data_.reset(body_fbs_unpacked);

    Response::MarkCompleted();
  }

 protected:
  static int generate_uid() {
    static std::atomic<int> next_uid(32100);
    return ++next_uid;
  }

  int uid_;

  // The FlatBuffer parser used to parse the response JSON string.
  flatbuffers::unique_ptr<flatbuffers::Parser> parser_;

  // The application data in a response is stored here.
  flatbuffers::unique_ptr<FbsTypeT> application_data_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_RESPONSE_JSON_H_
