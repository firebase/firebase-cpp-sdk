// Copyright 2022 Google LLC

#ifndef FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_
#define FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_

#include "app_check/src/include/firebase/app_check.h"

namespace firebase::app_check::internal {

enum AppCheckFn {
  kAppCheckFnGetAppCheckToken = 0,
  kAppCheckFnGetAppCheckStringInternal,
  kAppCheckFnGetLimitedUseAppCheckToken,
  kAppCheckFnGetLimitedUseAppCheckStringInternal,
  kAppCheckFnCount,
};

::firebase::app_check::AppCheck* GetExistingAppCheckInstance(::firebase::App* app);

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_COMMON_COMMON_H_
