// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// WARNING: Code from this file is included verbatim in the Messaging C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

package com.google.firebase.messaging.cpp.samples;

import android.app.Activity;
import android.content.Intent;
import android.util.Log;

// [START messaging_on_new_intent]
import com.google.firebase.messaging.MessageForwardingService;

class MyActivity extends Activity {
  private static final String TAG = "MyActvity";

  @Override
  protected void onNewIntent(Intent intent) {
    Log.d(TAG, "A message was sent to this app while it was in the background.");
    Intent message = new Intent(this, MessageForwardingService.class);
    message.setAction(MessageForwardingService.ACTION_REMOTE_INTENT);
    message.putExtras(intent);
    message.setData(intent.getData());
    startService(message);
  }
}
// [END messaging_on_new_intent]
