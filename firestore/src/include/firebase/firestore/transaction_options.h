/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_TRANSACTION_OPTIONS_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_TRANSACTION_OPTIONS_H_

#include <cstdint>
#include <iosfwd>
#include <string>

namespace firebase {
namespace firestore {

class TransactionOptions final {
 public:
  TransactionOptions() = default;

  int32_t max_attempts() const { return max_attempts_; }

  void set_max_attempts(int32_t max_attempts);

  /**
   * Returns a string representation of this `TransactionOptions` object for
   * logging/debugging purposes.
   *
   * @note the exact string representation is unspecified and subject to
   * change; don't rely on the format of the string.
   */
  std::string ToString() const;

  /**
   * Outputs the string representation of this `TransactionOptions` object to
   * the given stream.
   *
   * @see `ToString()` for comments on the representation format.
   */
  friend std::ostream& operator<<(std::ostream&, const TransactionOptions&);
 private:
  int32_t max_attempts_ = 5;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_TRANSACTION_OPTIONS_H_
