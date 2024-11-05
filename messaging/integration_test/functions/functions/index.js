/**
 * Copyright 2024 Google Inc. All Rights Reserved.
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

// Import the Firebase SDK for Google Cloud Functions.
const functions = require('firebase-functions/v1');
// Import and initialize the Firebase Admin SDK.
const admin = require('firebase-admin');
admin.initializeApp();

// Import the FCM SDK
const messaging = admin.messaging();

exports.sendMessage = functions.https.onCall(async (data) => {
  const {sendTo, isToken, notificationTitle, notificationBody, messageFields} =
      data;

  const message = {
    notification: {
      title: notificationTitle,
      body: notificationBody,
    },
    data: messageFields,
  };

  if (isToken) {
    message.token = sendTo;
  } else {
    message.topic = sendTo;
  }

  try {
    const response = await messaging.send(message);
    console.log('Successfully sent message:', response);
    return response;  // Optionally return the FCM response to the client
  } catch (error) {
    console.error('Error sending message:', error);
    throw new functions.https.HttpsError('internal', error.message);
  }
});
