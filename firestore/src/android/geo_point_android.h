#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firebase/firestore/geo_point.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `GeoPoint`. */
class GeoPointInternal : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  static jni::Class GetClass();

  /** Creates a C++ proxy for a Java `GeoPoint` object. */
  static jni::Local<GeoPointInternal> Create(jni::Env& env,
                                             const GeoPoint& point);

  /** Converts a Java GeoPoint to a public C++ GeoPoint. */
  GeoPoint ToPublic(jni::Env& env) const;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_GEO_POINT_ANDROID_H_
