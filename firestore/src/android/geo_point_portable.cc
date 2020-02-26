/*
 * Copyright 2019 Google
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

/**
 * Note: this file has a self-contained implementation of `GeoPoint` that can
 * be used by the Android library. The resulting Android binary doesn't include
 * symbols from Firestore in third_party, so this implementation has to fill in.
 */

// TODO(b/138615769): remove this file.

#include <cmath>
#include <iostream>
#include <sstream>

#include "app/src/assert.h"
#include "firebase/firestore/geo_point.h"

namespace firebase {
namespace firestore {

GeoPoint::GeoPoint(double latitude, double longitude)
    : latitude_(latitude), longitude_(longitude) {
  FIREBASE_ASSERT_MESSAGE(
      !std::isnan(latitude) && -90 <= latitude && latitude <= 90,
      "Latitude must be in the range of [-90, 90]");
  FIREBASE_ASSERT_MESSAGE(
      !std::isnan(longitude) && -180 <= longitude && longitude <= 180,
      "Latitude must be in the range of [-180, 180]");
}

std::string GeoPoint::ToString() const {
  std::ostringstream stream;
  stream << *this;
  return stream.str();
}

std::ostream& operator<<(std::ostream& out, const GeoPoint& geo_point) {
  return out << "GeoPoint(latitude=" << geo_point.latitude()
             << ", longitude=" << geo_point.longitude() << ")";
}

bool operator<(const GeoPoint& lhs, const GeoPoint& rhs) {
  if (lhs.latitude() == rhs.latitude()) {
    return lhs.longitude() < rhs.longitude();
  } else {
    return lhs.latitude() < rhs.latitude();
  }
}

}  // namespace firestore
}  // namespace firebase
