/*
 * Copyright 2023 Google LLC
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

package com.google.firebase.appcheck.internal.cpp;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;
import com.google.firebase.appcheck.AppCheckProvider;
import com.google.firebase.appcheck.AppCheckToken;

/**
 * Java AppCheckProvider that uses a C++ AppCheckProvider internally.
 */
public class JniAppCheckProvider implements AppCheckProvider {
  // Pointer to a C++ AppCheckProvider
  private long cProvider;

  public JniAppCheckProvider(long cProvider) {
    this.cProvider = cProvider;
  }
  @Override
  public Task<AppCheckToken> getToken() {
    TaskCompletionSource<AppCheckToken> taskCompletionSource =
        new TaskCompletionSource<AppCheckToken>();
    // Call the C++ provider to get an AppCheckToken and set the task result.
    // The C++ code will call handleGetTokenResult with the resulting token.
    nativeGetToken(cProvider, taskCompletionSource);
    return taskCompletionSource.getTask();
  }

  /**
   *  Called by C++ with a token in order to complete the java task.
   */
  public void handleGetTokenResult(TaskCompletionSource<AppCheckToken> taskCompletionSource,
      String token_string, long expiration_time_millis, int error_code, String error_message) {
    if (error_code == 0) {
      taskCompletionSource.setResult(new JniAppCheckToken(token_string, expiration_time_millis));
    } else {
      taskCompletionSource.setException(new Exception(error_message));
    }
  }

  /**
   * This function is implemented in the AppCheck C++ library (app_check_android.cc).
   */
  private native void nativeGetToken(
      long cProvider, TaskCompletionSource<AppCheckToken> task_completion_source);
}
