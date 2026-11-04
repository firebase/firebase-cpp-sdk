#include "remote_config/src/desktop/remote_config_request.h"
#include <map>
#include <string>
#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/variant_util.h"
#include "remote_config/src/value_parser.h"
namespace firebase::remote_config::internal {
RemoteConfigRequest::RemoteConfigRequest(const char* schema)
    : RequestJson(schema), custom_signals_() {
  add_header(app_common::kApiClientHeader, App::GetUserAgent());
}
void RemoteConfigRequest::UpdatePostFields() {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(fbs::Request::Pack(builder, application_data_.get()));
  std::string json;
  bool generate_status =
      GenerateText(*parser_, builder.GetBufferPointer(), &json) == nullptr;
  FIREBASE_ASSERT_RETURN_VOID(generate_status);
  if (!custom_signals_.empty()) {
    std::map<Variant, Variant> variant_map;
    for (const auto& [key, val] : custom_signals_) {
      const Variant key_var = Variant::FromMutableString(key);
      if (val.is_mutable_string()) {
        variant_map[key_var] = Variant::FromMutableString(val.mutable_string());
      } else if (val.is_static_string()) {
        const char* s = val.string_value();
        variant_map[key_var] = Variant::FromMutableString(s ? s : "");
      } else if (val.is_int64()) {
        variant_map[key_var] =
            Variant::FromMutableString(std::to_string(val.int64_value()));
      } else if (val.is_double()) {
        variant_map[key_var] = Variant::FromMutableString(
            ValueParser::FormatGeneralDouble(val.double_value()));
      }
    }
    std::string custom_signals_json = util::VariantToJson(variant_map);
    if (size_t last_brace = json.rfind('}'); last_brace != std::string::npos) {
      size_t first_brace = json.find('{');
      std::string insertion;
      if (first_brace != std::string::npos &&
          json.find_first_not_of(" \t\n\r", first_brace + 1) != last_brace) {
        insertion = ",\n  \"customSignals\": " + custom_signals_json + "\n";
      } else {
        insertion = "\n  \"customSignals\": " + custom_signals_json + "\n";
      }
      json.insert(last_brace, insertion);
    }
  }
  set_post_fields(json.c_str());
}
}  // namespace firebase::remote_config::internal
