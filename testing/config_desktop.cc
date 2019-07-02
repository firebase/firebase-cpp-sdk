#include "testing/config_desktop.h"
#include "testing/config.h"

#include <cstdint>

#include "base/logging.h"
#include "app/src/mutex.h"
#include "testing/testdata_config_generated.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace testing {
namespace cppsdk {

// We keep this raw data in order to be able to return it back for merge.
static uint8_t* g_test_data_config = nullptr;
// Mutex to make sure we don't delete the pointer while someone else is reading.
static Mutex testing_mutex;  // NOLINT
// List of all the test data we've used so far, so we can clean it up later.
// (we can't delete it when it's replaced because old threads might still
// be looking at it.)
static std::vector<uint8_t*> g_all_test_data;  // NOLINT

const ConfigRow* ConfigGet(const char* fake) {
  MutexLock lock(testing_mutex);
  CHECK(g_test_data_config != nullptr) << "No test data at all";
  const TestDataConfig* config = GetTestDataConfig(g_test_data_config);
  // LookupByKey() does not work because the data passed in may not conform. So
  // we just iterate over the test data.
  for (const ConfigRow* row : *(config->config())) {
    if (strcmp(row->fake()->c_str(), fake) == 0) {
      return row;
    }
  }
  return nullptr;
}

namespace internal {
void ConfigSetImpl(const uint8_t* test_data_binary,
                   flatbuffers::uoffset_t size) {
  MutexLock lock(testing_mutex);
  if (test_data_binary != nullptr && size > 0) {
    g_test_data_config = new uint8_t[size];
    memcpy(g_test_data_config, test_data_binary, size);
    g_all_test_data.push_back(g_test_data_config);
  } else {
    g_test_data_config = nullptr;
    for (int i = 0; i < g_all_test_data.size(); i++) {
      delete[] g_all_test_data[i];
    }
    g_all_test_data.clear();
  }
}
}  // namespace internal

}  // namespace cppsdk
}  // namespace testing
}  // namespace firebase
