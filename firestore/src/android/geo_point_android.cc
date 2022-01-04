/*
 * Copyright 2020 Google LLC
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

#include "firestore/src/android/geo_point_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Class;
using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/GeoPoint";
Constructor<GeoPointInternal> kConstructor("(DD)V");
Method<double> kGetLatitude("getLatitude", "()D");
Method<double> kGetLongitude("getLongitude", "()D");

jclass g_clazz = nullptr;

}  // namespace

void GeoPointInternal::Initialize(jni::Loader& loader) {
  g_clazz =
      loader.LoadClass(kClassName, kConstructor, kGetLatitude, kGetLongitude);
}

Class GeoPointInternal::GetClass() { return Class(g_clazz); }

Local<GeoPointInternal> GeoPointInternal::Create(Env& env,
                                                 const GeoPoint& point) {
  return env.New(kConstructor, point.latitude(), point.longitude());
}

GeoPoint GeoPointInternal::ToPublic(Env& env) const {
  double latitude = env.Call(*this, kGetLatitude);
  double longitude = env.Call(*this, kGetLongitude);
  return GeoPoint(latitude, longitude);
}

}  // namespace firestore
}  // namespace firebase
