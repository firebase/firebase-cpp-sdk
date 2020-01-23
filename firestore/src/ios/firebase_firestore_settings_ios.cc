#include "firestore/src/ios/firebase_firestore_settings_ios.h"

namespace firebase {
namespace firestore {

api::Settings ToCoreApi(const Settings& from) {
  api::Settings to;
  to.set_host(from.host());
  to.set_ssl_enabled(from.is_ssl_enabled());
  to.set_persistence_enabled(from.is_persistence_enabled());
  // TODO(varconst): implement `cache_size_bytes` in public `Settings` and
  // uncomment. to.set_cache_size_bytes(from.cache_size_bytes());
  return to;
}

}  // namespace firestore
}  // namespace firebase
