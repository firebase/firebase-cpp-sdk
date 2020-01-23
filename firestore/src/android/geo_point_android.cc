#include "firestore/src/android/geo_point_android.h"

#include <stdint.h>

#include "app/src/util_android.h"
#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define GEO_POINT_METHODS(X)                                    \
  X(Constructor, "<init>", "(DD)V", util::kMethodTypeInstance), \
  X(GetLatitude, "getLatitude", "()D"),                         \
  X(GetLongitude, "getLongitude", "()D")
// clang-format on

METHOD_LOOKUP_DECLARATION(geo_point, GEO_POINT_METHODS)
METHOD_LOOKUP_DEFINITION(geo_point,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/GeoPoint",
                         GEO_POINT_METHODS)

/* static */
jobject GeoPointInternal::GeoPointToJavaGeoPoint(JNIEnv* env,
                                                 const GeoPoint& point) {
  jobject result = env->NewObject(
      geo_point::GetClass(), geo_point::GetMethodId(geo_point::kConstructor),
      static_cast<jdouble>(point.latitude()),
      static_cast<jdouble>(point.longitude()));
  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
GeoPoint GeoPointInternal::JavaGeoPointToGeoPoint(JNIEnv* env, jobject obj) {
  jdouble latitude = env->CallDoubleMethod(
      obj, geo_point::GetMethodId(geo_point::kGetLatitude));
  jdouble longitude = env->CallDoubleMethod(
      obj, geo_point::GetMethodId(geo_point::kGetLongitude));
  CheckAndClearJniExceptions(env);
  return GeoPoint{static_cast<double>(latitude),
                  static_cast<double>(longitude)};
}

/* static */
jclass GeoPointInternal::GetClass() { return geo_point::GetClass(); }

/* static */
bool GeoPointInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = geo_point::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void GeoPointInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  geo_point::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
