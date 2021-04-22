// Copyright 2018 Google LLC
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

#include "database/src/desktop/transaction_data.h"

#include "database/src/desktop/database_desktop.h"

namespace firebase {
namespace database {
namespace internal {

const int TransactionData::kTransactionMaxRetries = 25;

TransactionData::~TransactionData() {
  if (delete_context) {
    delete_context(context);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
