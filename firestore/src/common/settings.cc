#include "firestore/src/include/firebase/firestore/settings.h"

#include <ostream>

#include "app/meta/move.h"

#if !defined(__ANDROID__) && !defined(FIRESTORE_STUB_BUILD)
#include "Firestore/core/src/util/executor.h"
#endif

namespace firebase {
namespace firestore {

namespace {

const char kDefaultHost[] = "firestore.googleapis.com";

}

Settings::Settings() : host_(kDefaultHost) {}

void Settings::set_host(std::string host) { host_ = firebase::Move(host); }

void Settings::set_ssl_enabled(bool enabled) { ssl_enabled_ = enabled; }

void Settings::set_persistence_enabled(bool enabled) {
  persistence_enabled_ = enabled;
}

std::string Settings::ToString() const {
  auto to_str = [](bool v) { return v ? "true" : "false"; };
  return std::string("Settings(host='") + host() +
         "', is_ssl_enabled=" + to_str(is_ssl_enabled()) +
         ", is_persistence_enabled=" + to_str(is_persistence_enabled()) + ")";
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
