#include "firestore/src/ios/hard_assert_ios.h"

namespace firebase {
namespace firestore {
namespace util {

ABSL_ATTRIBUTE_NORETURN void ThrowInvalidArgumentIos(
    const std::string& message) {
  Throw(ExceptionType::InvalidArgument, nullptr, nullptr, 0, message);
}

}  // namespace util
}  // namespace firestore
}  // namespace firebase
