// Copyright 2026 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


/**
 * Import function triggers from their respective submodules:
 *
 * const {onCall} = require("firebase-functions/v1/https"); // wait, V1 doesn't have /v1/https
 *
 * See a full list of supported triggers at https://firebase.google.com/docs/functions
 */

const functions = require("firebase-functions");

// Creates a function that consumes limited-use App Check tokens
exports.addtwowithlimiteduse = functions.runWith({
  enforceAppCheck: true,
  consumeAppCheckToken: true,
  maxInstances: 10 // Setting maxInstances the V1 way
}).https.onCall((data, context) => {
  // context.app will be defined if a valid App Check token was provided
  if (context.app === undefined) {
    throw new functions.https.HttpsError(
        'failed-precondition',
        'The function must be called from an App Check verified app.');
  }

  const firstNumber = data.firstNumber;
  const secondNumber = data.secondNumber;

  if (firstNumber === undefined || secondNumber === undefined) {
     throw new functions.https.HttpsError('invalid-argument', 'The function must be called with "firstNumber" and "secondNumber".');
  }

  return {
    firstNumber: firstNumber,
    secondNumber: secondNumber,
    operator: '+',
    operationResult: Number(firstNumber) + Number(secondNumber),
  };
});
