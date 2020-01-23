#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "Firestore/core/include/firebase/firestore/geo_point.h"

namespace firebase {
namespace firestore {

// This is the non-wrapper Android implementation of GeoPoint. Since GeoPoint
// has most methods inlined, we use it directly instead of wrapping around a
// Java GeoPoint object. We still need the helper functions to convert between
// the two types. In addition, we also need proper initializer and terminator
// for the Java class cache/uncache.
class GeoPointInternal {
 public:
  using ApiType = GeoPoint;

  // Convert a C++ GeoPoint into a Java GeoPoint.
  static jobject GeoPointToJavaGeoPoint(JNIEnv* env, const GeoPoint& point);

  // Convert a Java GeoPoint into a C++ GeoPoint.
  static GeoPoint JavaGeoPointToGeoPoint(JNIEnv* env, jobject obj);

  // Gets the class object of Java GeoPoint class.
  static jclass GetClass();

 private:
  friend class FirestoreInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_
