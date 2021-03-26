#include "firestore/src/include/firebase/firestore/settings.h"

#include <ostream>
#include <sstream>

#include "app/meta/move.h"

#if !defined(__ANDROID__) && !defined(FIRESTORE_STUB_BUILD)
#include "Firestore/core/src/util/executor.h"
#endif

namespace firebase {
namespace firestore {

namespace {

const char kDefaultHost[] = "firestore.googleapis.com";

std::string ToStr(bool v) { return v ? "true" : "false"; }

std::string ToStr(int64_t v) {
  // TODO(b/163140650): use `std::to_string` (which will make this function
  // unnecessary).
  std::ostringstream s;
  s << v;
  return s.str();
}

}  // namespace

#if !defined(__APPLE__)
Settings::Settings() : host_(kDefaultHost) {}
#endif

void Settings::set_host(std::string host) { host_ = firebase::Move(host); }

void Settings::set_ssl_enabled(bool enabled) { ssl_enabled_ = enabled; }

void Settings::set_persistence_enabled(bool enabled) {
  persistence_enabled_ = enabled;
}

void Settings::set_cache_size_bytes(int64_t value) {
  cache_size_bytes_ = value;
}

std::string Settings::ToString() const {
  return std::string("Settings(host='") + host() +
         "', is_ssl_enabled=" + ToStr(is_ssl_enabled()) +
         ", is_persistence_enabled=" + ToStr(is_persistence_enabled()) +
         ", cache_size_bytes=" + ToStr(cache_size_bytes()) + ")";
}

std::ostream& operator<<(std::ostream& out, const Settings& settings) {
  return out << settings.ToString();
}

// Apple uses a different mechanism, defined in `settings_ios.mm`.
#if !defined(__APPLE__) && !defined(__ANDROID__) && \
    !defined(FIRESTORE_STUB_BUILD)
std::unique_ptr<util::Executor> Settings::CreateExecutor() const {
  return util::Executor::CreateSerial("integration_tests");
}
#endif

}  // namespace firestore
}  // namespace firebase
