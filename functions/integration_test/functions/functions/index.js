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

const functions = require('firebase-functions/v1');
const assert = require('assert');

// Creates a function that consumes limited-use App Check tokens
exports.addtwowithlimiteduse = functions.runWith({
  enforceAppCheck: true,
  consumeAppCheckToken: true,
  maxInstances: 10,
}).https.onCall((data, context) => {
  if (context.app === undefined) {
    throw new functions.https.HttpsError(
        'failed-precondition',
        'The function must be called from an App Check verified app.');
  }

  const firstNumber = data.firstNumber;
  const secondNumber = data.secondNumber;

  if (firstNumber === undefined || secondNumber === undefined) {
    throw new functions.https.HttpsError(
        'invalid-argument',
        'The function must be called with "firstNumber" and "secondNumber".');
  }

  return {
    firstNumber: firstNumber,
    secondNumber: secondNumber,
    operator: '+',
    operationResult: Number(firstNumber) + Number(secondNumber),
  };
});

exports.dataTest = functions.https.onRequest((req, res) => {
  if (!req.body || typeof req.body.data !== 'object' || req.body.data === null) {
    res.status(400).send({
      error: {status: 'INVALID_ARGUMENT', message: 'Missing or invalid data'},
    });
    return;
  }
  assert.deepEqual(req.body, {
    data: {
      bool: true,
      int: 2,
      long: {
        value: '3',
        '@type': 'type.google.com/google.protobuf.Int64Value',
      },
      string: 'four',
      array: [5, 6],
      null: null,
    },
  });
  res.send({
    result: {
      message: 'stub response',
      bool: true,
      int: 2,
      code: 42,
      long: {
        value: '420',
        '@type': 'type.google.com/google.protobuf.Int64Value',
      },
      array: [5, 6],
      string: 'four',
      null: null,
    },
  });
});

exports.scalarTest = functions.https.onCall((data) => {
  assert.equal(data, 17);
  return 76;
});

exports.tokenTest = functions.https.onRequest((req, res) => {
  assert.equal('Bearer token', req.get('Authorization'));
  assert.deepEqual(req.body, {data: {}});
  res.send({result: {}});
});

exports.instanceIdTest = functions.https.onCall((data, context) => {
  assert.equal(context.instanceIdToken, 'iid');
  assert.deepEqual(data, {});
  return {};
});

exports.nullTest = functions.https.onCall((data) => {
  assert.strictEqual(data, null);
  return null;
});

exports.missingResultTest = functions.https.onRequest((req, res) => {
  assert.deepEqual(req.body, {data: null});
  res.send({});
});

exports.unhandledErrorTest = functions.https.onCall(() => {
  throw new Error('nope');
});

exports.unknownErrorTest = functions.https.onRequest((req, res) => {
  res.status(400).send({
    error: {
      status: 'THIS_IS_NOT_VALID',
      message: 'this should be ignored',
    },
  });
});

exports.explicitErrorTest = functions.https.onCall(() => {
  throw new functions.https.HttpsError(
      'out-of-range', 'explicit nope', {
        start: 10,
        end: 20,
        long: {
          value: '30',
          '@type': 'type.google.com/google.protobuf.Int64Value',
        },
      });
});

exports.httpErrorTest = functions.https.onRequest((req, res) => {
  res.status(400).send('invalid http request');
});

exports.timeoutTest = functions.https.onCall(() => {
  return new Promise((resolve) => {
    setTimeout(() => resolve({}), 500);
  });
});
