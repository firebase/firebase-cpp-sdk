#include "remote_config/src/common.h"
#include "app/src/util.h"
#include "remote_config/src/include/firebase/remote_config.h"
FIREBASE_APP_REGISTER_CALLBACKS(
    remote_config,
    {
      return ::firebase::kInitResultSuccess;
    },
    {},
    false);
