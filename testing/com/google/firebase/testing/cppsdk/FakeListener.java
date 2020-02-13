// Copyright 2020 Google LLC
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

package com.google.firebase.testing.cppsdk;

/**
 * A generic listener to make the fake code compact. It is OnSuccessListener, a OnFailureListener,
 * and a ResultCallback all-in-one. Please feel free to add more no-op on-method. Subclasses only
 * have to override the relevant method.
 */
public class FakeListener<R> {
  public void onSuccess(R result) {}

  public void onFailure(Exception e) {}

  public void onResult(R result) {}
}
