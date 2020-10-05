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
