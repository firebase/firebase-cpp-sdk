#include "testing/config.h"

#include "base/logging.h"
#include "testing/testdata_config_resource.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace testing {
namespace cppsdk {

const char* kSchemaFilePath = "";

// Replace the current test data.
void ConfigSet(const char* test_data_in_json) {
  flatbuffers::Parser parser;
  const char* schema_str =
      reinterpret_cast<const char*>(testdata_config_resource_data);
  CHECK(parser.Parse(schema_str)) << "Failed to parse schema:" << parser.error_;
  CHECK(parser.Parse(test_data_in_json)) << "Invalid JSON:" << parser.error_;

  // Assign
  internal::ConfigSetImpl(parser.builder_.GetBufferPointer(),
                          parser.builder_.GetSize());
}

void ConfigReset() { internal::ConfigSetImpl(nullptr, 0); }

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
