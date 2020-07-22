#include "firebase/csharp/firestore_instance_management.h"

#include "firebase/app/client/unity/src/cpp_instance_manager.h"
#include "firebase/firestore.h"

namespace firebase {
namespace firestore {
namespace csharp {

namespace {

CppInstanceManager<Firestore>& GetFirestoreInstanceManager() {
  // Allocate the CppInstanceManager on the heap to prevent its destructor
  // from executing (go/totw/110#the-fix-safe-initialization-no-destruction).
  static CppInstanceManager<Firestore>* firestore_instance_manager =
      new CppInstanceManager<Firestore>();
  return *firestore_instance_manager;
}

}  // namespace

Firestore* GetFirestoreInstance(App* app) {
  auto& firestore_instance_manager = GetFirestoreInstanceManager();
  // Acquire the lock used internally by CppInstanceManager::ReleaseReference()
  // to avoid racing with deletion of Firestore instances.
  MutexLock lock(firestore_instance_manager.mutex());
  Firestore* instance =
      Firestore::GetInstance(app, /*init_result_out=*/nullptr);
  firestore_instance_manager.AddReference(instance);
  return instance;
}

void ReleaseFirestoreInstance(Firestore* firestore) {
  GetFirestoreInstanceManager().ReleaseReference(firestore);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
