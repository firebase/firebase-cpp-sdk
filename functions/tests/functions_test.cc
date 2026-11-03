// Copyright 2024 Google LLC
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

#include "functions/src/include/firebase/functions.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_interface.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "functions/src/desktop/callable_reference_desktop.h"
#include "functions/src/desktop/functions_desktop.h"
#include "functions/src/desktop/serialization.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"

namespace firebase {
namespace functions {
namespace internal {
namespace {

TEST(FunctionsSerializationTest, EncodeAndDecodePrimitivesAndContainers) {
  EXPECT_EQ(Encode(Variant::Null()), Variant::Null());
  EXPECT_EQ(Encode(Variant(true)), Variant(true));
  EXPECT_EQ(Encode(Variant(3.5)), Variant(3.5));
  EXPECT_EQ(Encode(Variant("hello")), Variant("hello"));

  // Non-finite doubles cannot be serialized to JSON.
  EXPECT_EQ(Encode(Variant(std::numeric_limits<double>::infinity())),
            Variant::Null());
  EXPECT_EQ(Encode(Variant(std::nan(""))), Variant::Null());

  // Int64 encodes to @type Int64Value map and round-trips via Decode.
  Variant encoded_int = Encode(Variant(int64_t{-9223372036854775807LL}));
  ASSERT_TRUE(encoded_int.is_map());
  EXPECT_EQ(encoded_int.map()["@type"],
            Variant("type.googleapis.com/google.protobuf.Int64Value"));
  EXPECT_EQ(encoded_int.map()["value"], Variant("-9223372036854775807"));
  EXPECT_EQ(Decode(encoded_int), Variant(int64_t{-9223372036854775807LL}));

  // Legacy type.google.com/google.protobuf.Int64Value is also decoded.
  Variant legacy_int = Variant::EmptyMap();
  legacy_int.map()["@type"] = "type.google.com/google.protobuf.Int64Value";
  legacy_int.map()["value"] = " 42 ";
  EXPECT_EQ(Decode(legacy_int), Variant(int64_t{42}));

  // UInt64Value decoding, including values > INT64_MAX.
  Variant uint_map = Variant::EmptyMap();
  uint_map.map()["@type"] = "type.googleapis.com/google.protobuf.UInt64Value";
  uint_map.map()["value"] = "18446744073709551615";
  Variant decoded_uint = Decode(uint_map);
  ASSERT_TRUE(decoded_uint.is_int64());
  EXPECT_EQ(static_cast<uint64_t>(decoded_uint.int64_value()),
            std::numeric_limits<uint64_t>::max());

  // Negative UInt64Value or trailing garbage must fail decoding.
  uint_map.map()["value"] = "-1";
  EXPECT_EQ(Decode(uint_map), Variant::Null());
  uint_map.map()["value"] = "123abc";
  EXPECT_EQ(Decode(uint_map), Variant::Null());
}

TEST(FunctionsErrorMappingTest, HttpAndGrpcStatusMappings) {
  EXPECT_EQ(ErrorFromHttpStatus(200), kErrorNone);
  EXPECT_EQ(ErrorFromHttpStatus(201), kErrorNone);
  EXPECT_EQ(ErrorFromHttpStatus(204), kErrorCancelled);
  EXPECT_EQ(ErrorFromHttpStatus(400), kErrorInvalidArgument);
  EXPECT_EQ(ErrorFromHttpStatus(401), kErrorUnauthenticated);
  EXPECT_EQ(ErrorFromHttpStatus(403), kErrorPermissionDenied);
  EXPECT_EQ(ErrorFromHttpStatus(404), kErrorNotFound);
  EXPECT_EQ(ErrorFromHttpStatus(408), kErrorDeadlineExceeded);
  EXPECT_EQ(ErrorFromHttpStatus(409), kErrorAborted);
  EXPECT_EQ(ErrorFromHttpStatus(429), kErrorResourceExhausted);
  EXPECT_EQ(ErrorFromHttpStatus(499), kErrorCancelled);
  EXPECT_EQ(ErrorFromHttpStatus(500), kErrorInternal);
  EXPECT_EQ(ErrorFromHttpStatus(501), kErrorUnimplemented);
  EXPECT_EQ(ErrorFromHttpStatus(503), kErrorUnavailable);
  EXPECT_EQ(ErrorFromHttpStatus(504), kErrorDeadlineExceeded);
  EXPECT_EQ(ErrorFromHttpStatus(0), kErrorUnavailable);

  EXPECT_EQ(ErrorFromStatus("OK"), kErrorNone);
  EXPECT_EQ(ErrorFromStatus("CANCELLED"), kErrorCancelled);
  EXPECT_EQ(ErrorFromStatus("UNKNOWN"), kErrorUnknown);
  EXPECT_EQ(ErrorFromStatus("INVALID_ARGUMENT"), kErrorInvalidArgument);
  EXPECT_EQ(ErrorFromStatus("DEADLINE_EXCEEDED"), kErrorDeadlineExceeded);
  EXPECT_EQ(ErrorFromStatus("NOT_FOUND"), kErrorNotFound);
  EXPECT_EQ(ErrorFromStatus("ALREADY_EXISTS"), kErrorAlreadyExists);
  EXPECT_EQ(ErrorFromStatus("PERMISSION_DENIED"), kErrorPermissionDenied);
  EXPECT_EQ(ErrorFromStatus("UNAUTHENTICATED"), kErrorUnauthenticated);
  EXPECT_EQ(ErrorFromStatus("RESOURCE_EXHAUSTED"), kErrorResourceExhausted);
  EXPECT_EQ(ErrorFromStatus("FAILED_PRECONDITION"), kErrorFailedPrecondition);
  EXPECT_EQ(ErrorFromStatus("ABORTED"), kErrorAborted);
  EXPECT_EQ(ErrorFromStatus("OUT_OF_RANGE"), kErrorOutOfRange);
  EXPECT_EQ(ErrorFromStatus("UNIMPLEMENTED"), kErrorUnimplemented);
  EXPECT_EQ(ErrorFromStatus("INTERNAL"), kErrorInternal);
  EXPECT_EQ(ErrorFromStatus("UNAVAILABLE"), kErrorUnavailable);
  EXPECT_EQ(ErrorFromStatus("DATA_LOSS"), kErrorDataLoss);
  EXPECT_EQ(ErrorFromStatus("BOGUS_STATUS"), kErrorInternal);

  EXPECT_STREQ(
      internal::GetErrorMessage(kErrorUnauthenticated),
      "The request does not have valid authentication credentials for "
      "the operation");
}

TEST(FunctionsResolveFutureTest, HandlesEdgeCaseEnvelopesCorrectly) {
  ReferenceCountedFutureImpl future_api(1);

  // 1. {"error": null, "data": 42} -> success with 42 (BUG-2697)
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    std::string header = "HTTP/1.1 200 OK\r\n";
    std::string body = R"({"error": null, "data": 42})";
    resp.ProcessHeader(header.data(), header.size());
    resp.ProcessBody(body.data(), body.size());
    resp.MarkCompleted();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorNone);
    ASSERT_NE(fut.result(), nullptr);
    EXPECT_EQ(fut.result()->data(), Variant(42));
  }

  // 2. Non-JSON body with HTTP 404 -> preserves kErrorNotFound (BUG-2276)
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    std::string header = "HTTP/1.1 404 Not Found\r\n";
    std::string body = "<html>Not Found</html>";
    resp.ProcessHeader(header.data(), header.size());
    resp.ProcessBody(body.data(), body.size());
    resp.MarkCompleted();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorNotFound);
  }

  // 3. Error envelope with "status": "OK" -> kErrorInternal (BUG-2403)
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    std::string header = "HTTP/1.1 200 OK\r\n";
    std::string body = R"({"error": {"status": "OK", "message": "weird"}})";
    resp.ProcessHeader(header.data(), header.size());
    resp.ProcessBody(body.data(), body.size());
    resp.MarkCompleted();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorInternal);
    EXPECT_STREQ(fut.error_message(), "weird");
  }

  // 4. Error envelope with non-string "status" -> kErrorInternal (BUG-2760)
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    std::string header = "HTTP/1.1 500 Internal Server Error\r\n";
    std::string body = R"({"error": {"status": 123}})";
    resp.ProcessHeader(header.data(), header.size());
    resp.ProcessBody(body.data(), body.size());
    resp.MarkCompleted();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorInternal);
  }

  // 5. Error envelope with unknown status preserves server message (BUG-2722)
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    std::string header = "HTTP/1.1 400 Bad Request\r\n";
    std::string body =
        R"({"error": {"status": "NEW_CODE", "message": "server explanation"}})";
    resp.ProcessHeader(header.data(), header.size());
    resp.ProcessBody(body.data(), body.size());
    resp.MarkCompleted();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorInternal);
    EXPECT_STREQ(fut.error_message(), "server explanation");
  }

  // 6. MarkFailed() on response fails the future even if status was 200
  {
    auto handle = future_api.SafeAlloc<HttpsCallableResult>(0);
    FunctionsCallableResponse resp;
    resp.set_status(200);
    resp.MarkFailed();
    ResolveFuture(&future_api, handle, &resp);
    Future<HttpsCallableResult> fut(&future_api, handle.get());
    EXPECT_EQ(fut.error(), kErrorUnavailable);
  }
}

TEST(FunctionsDesktopTest, UrlConstructionAndLifecycle) {
  rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
    return flatbuffers::unique_ptr<rest::Transport>(new rest::TransportMock);
  });

  InitResult init_result = kInitResultSuccess;
  EXPECT_EQ(Functions::GetInstance(nullptr, &init_result), nullptr);
  EXPECT_EQ(init_result, kInitResultFailedMissingDependency);

  AppOptions options = testing::MockAppOptions();
  options.set_project_id("test-proj");
  App* app = testing::CreateApp(options);
  ASSERT_NE(app, nullptr);

  Functions* fn = Functions::GetInstance(app, "europe-west1", &init_result);
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(init_result, kInitResultSuccess);
  EXPECT_STREQ(fn->region(), "europe-west1");

  // Callable created BEFORE UseFunctionsEmulator dynamically updates its URL.
  HttpsCallableReference ref = fn->GetHttpsCallable("/myFunc");
  EXPECT_TRUE(ref.is_valid());
  EXPECT_EQ(ref.functions(), fn);
  EXPECT_EQ(
      ref.internal()->url(),
      "https://europe-west1-test-proj.cloudfunctions.net/myFunc");

  fn->UseFunctionsEmulator("https://localhost:5005/");
  EXPECT_EQ(ref.internal()->url(),
            "https://localhost:5005/test-proj/europe-west1/myFunc");

  fn->UseEmulator("127.0.0.1", 5001);
  EXPECT_EQ(ref.internal()->url(),
            "http://127.0.0.1:5001/test-proj/europe-west1/myFunc");

  // Custom domain support.
  Functions* custom_fn =
      Functions::GetInstance(app, "https://api.example.com/");
  ASSERT_NE(custom_fn, nullptr);
  HttpsCallableReference custom_ref = custom_fn->GetHttpsCallable("runTask");
  EXPECT_EQ(custom_ref.internal()->url(), "https://api.example.com/runTask");

  // Invalid URL in GetHttpsCallableFromURL produces an invalid reference.
  HttpsCallableReference bad_url_ref =
      fn->GetHttpsCallableFromURL("not-a-valid-url");
  EXPECT_FALSE(bad_url_ref.is_valid());

  // Mock HTTP call and destroy HttpsCallableReference immediately after Call().
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'http://127.0.0.1:5001/test-proj/europe-west1/myFunc',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok'],"
      "       body: ['{\"result\": {\"answer\": 99}}']"
      "     }"
      "    }"
      "  ]"
      "}");

  Future<HttpsCallableResult> call_future;
  {
    HttpsCallableReference temp_ref = fn->GetHttpsCallable("myFunc");
    call_future = temp_ref.Call(Variant("input"));
  }
  EXPECT_EQ(call_future.status(), kFutureStatusComplete);
  EXPECT_EQ(call_future.error(), kErrorNone);
  ASSERT_NE(call_future.result(), nullptr);
  EXPECT_EQ(call_future.result()->data().map().at("answer"), Variant(99));

  delete custom_fn;
  delete fn;
  delete app;
}

}  // namespace
}  // namespace internal
}  // namespace functions
}  // namespace firebase
