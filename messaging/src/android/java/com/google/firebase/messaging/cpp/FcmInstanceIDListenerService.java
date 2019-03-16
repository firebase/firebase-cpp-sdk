// Copyright 2016 Google LLC
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

package com.google.firebase.messaging.cpp;

import com.google.firebase.iid.FirebaseInstanceIdService;

/**
 * A Service which notifies your app that a new token needs to be generated.
 *
 * Tokens are unique and secure, but your app or the Instance ID service may need to refresh tokens
 * in the event of a security issue or when a user uninstalls and reinstalls your app during device
 * restoration. This class implements a listener to respond to token refresh requests from the
 * Instance ID service.
 *
 * See https://developers.google.com/instance-id/ for more details.
 */
public class FcmInstanceIDListenerService extends FirebaseInstanceIdService {

  @Override
  public void onTokenRefresh() {
    // Fetch updated Instance ID token and notify our app's server of any changes (if applicable).
    RegistrationIntentService.refreshToken(this);
  }
}
