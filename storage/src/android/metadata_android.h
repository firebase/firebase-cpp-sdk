#ifndef FIREBASE_STORAGE_SRC_ANDROID_METADATA_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_METADATA_ANDROID_H_
#include <jni.h>
#include "app/src/include/firebase/app.h"
#include "storage/src/common/metadata_internal.h"
namespace firebase {
namespace storage {
namespace internal {
bool InitializeMetadataConverter(App* app);
void TerminateMetadataConverter(App* app);
jobject MetadataToStorageMetadata(JNIEnv* env, const MetadataInternal& metadata);
MetadataInternal* StorageMetadataToMetadata(JNIEnv* env, jobject storage_metadata, StorageInternal* storage);
}
}
}
#endif
