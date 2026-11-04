#include "remote_config/src/desktop/remote_config_response.h"
#include "app/src/variant_util.h"
#include "flatbuffers/flatbuffers.h"
namespace firebase {
namespace remote_config {
namespace internal {
RemoteConfigResponse::RemoteConfigResponse(const char* schema)
    : ResponseJson(schema) {}
void RemoteConfigResponse::MarkCompleted() {
  entries_ = Variant::Null();
  ResponseJson::MarkCompleted();
  if (!application_data_) {
    return;
  }
  const char* body = GetBody();
  if (!body || body[0] == '\0' || sdk_error_code() != 0) {
    return;
  }
  const flatbuffers::FlatBufferBuilder& builder = parser_->builder_;
  if (builder.GetSize() == 0 || builder.GetBufferPointer() == nullptr) {
    return;
  }
  flatbuffers::Verifier verifier(builder.GetBufferPointer(), builder.GetSize());
  if (!fbs::VerifyResponseBuffer(verifier)) {
    return;
  }
  const fbs::Response* body_fbs =
      flatbuffers::GetRoot<fbs::Response>(builder.GetBufferPointer());
  if (body_fbs && body_fbs->entries() && body_fbs->entries()->size() > 0) {
    entries_ = util::FlexbufferToVariant(body_fbs->entries_flexbuffer_root());
  }
}
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
