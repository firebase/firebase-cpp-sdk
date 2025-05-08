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

package com.google.firebase.gma.internal.cpp;

import android.os.AsyncTask;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to make interactions between the GMA C++ wrapper and Java objects cleaner. It's
 * designed to download static ad assets from network.
 */
public class DownloadHelper {
  // C++ nullptr for use with the callbacks.
  private static final long CPP_NULLPTR = 0;

  // URL to download the asset from.
  private URL url;

  // Request headers.
  private final HashMap<String, String> headers;

  // HTTP response code.
  private int responseCode;

  // Error message.
  private String errorMessage;

  // Error code.
  private int errorCode;

  // Pointer to a FutureCallbackData in the C++ wrapper that will be used to
  // complete the Future associated with the latest call to LoadImage.
  private long mDownloadCallbackDataPtr;

  // Synchronization object for thread safe access to:
  // * mDownloadCallbackDataPtr
  private final Object mDownloadLock;

  // Create a new DownloadHelper with a default URL.
  public DownloadHelper(String urlString) throws MalformedURLException {
    this.headers = new HashMap<>();
    setUrl(urlString);

    mDownloadLock = new Object();
    errorCode = ConstantsHelper.CALLBACK_ERROR_NONE;

    // Test the callbacks and fail quickly if something's wrong.
    completeNativeImageFutureCallback(CPP_NULLPTR, 0, ConstantsHelper.CALLBACK_ERROR_MESSAGE_NONE);
  }

  // Set the URL to the given string, or null if it can't be parsed.
  public void setUrl(String urlString) throws MalformedURLException {
    this.url = new URL(urlString);
  }

  // Get the previously-set URL.
  public URL getUrl() {
    return this.url;
  }

  // Add a header key-value pair.
  public void addHeader(String key, String value) {
    this.headers.put(key, value);
  }

  // Clear previously-set headers.
  public void clearHeaders() {
    this.headers.clear();
  }

  // Get the response code returned by the server, after download() is finished.
  public int getResponseCode() {
    return this.responseCode;
  }

  /** Triggers an async HTTP GET request to the given URL, with the given headers. */
  public void download(long callbackDataPtr) {
    synchronized (mDownloadLock) {
      if (mDownloadCallbackDataPtr != CPP_NULLPTR) {
        completeNativeLoadImageError(callbackDataPtr,
            ConstantsHelper.CALLBACK_ERROR_LOAD_IN_PROGRESS,
            ConstantsHelper.CALLBACK_ERROR_MESSAGE_LOAD_IN_PROGRESS);
        return;
      }
      mDownloadCallbackDataPtr = callbackDataPtr;
    }

    try {
      /** Invokes download as an async background task. */
      new DownloadFilesTask().execute();
    } catch (Exception ex) {
      completeNativeLoadImageError(
          callbackDataPtr, ConstantsHelper.CALLBACK_ERROR_INTERNAL_ERROR, ex.getMessage());
    }

    return;
  }

  /** Performs Download task in a background worker thread. */
  private class DownloadFilesTask extends AsyncTask<Void, Void, byte[]> {
    protected byte[] doInBackground(Void... params) {
      HttpURLConnection connection = null;
      ByteArrayOutputStream bytestream = null;
      try {
        connection = (HttpURLConnection) url.openConnection();
        connection.setRequestMethod("GET");
        for (Map.Entry<String, String> entry : headers.entrySet()) {
          connection.setRequestProperty(entry.getKey(), entry.getValue());
        }

        responseCode = connection.getResponseCode();

        if (responseCode != HttpURLConnection.HTTP_OK) {
          errorCode = ConstantsHelper.CALLBACK_ERROR_NETWORK_ERROR;
          errorMessage = connection.getResponseMessage();

          connection.disconnect();
          return null;
        }

        InputStream inputStream = connection.getInputStream();
        bytestream = new ByteArrayOutputStream();
        int ch = 0;

        // Buffer to read 1024 bytes in at a time.
        byte[] buffer = new byte[1024];
        while ((ch = inputStream.read(buffer)) != -1) {
          bytestream.write(buffer, 0, ch);
        }
        byte imgdata[] = bytestream.toByteArray();

        bytestream.close();
        connection.disconnect();

        return imgdata;
      } catch (Exception ex) {
        errorCode = ConstantsHelper.CALLBACK_ERROR_INVALID_REQUEST;
        errorMessage = ex.getMessage();
      }

      try {
        if (bytestream != null) {
          bytestream.close();
        }
        if (connection != null) {
          connection.disconnect();
        }
      } catch (Exception ex) {
        // Connection and bytestream close exceptions can be ignored as download errors are already
        // recorded.
      }
      return null;
    }

    protected void onPostExecute(byte[] result) {
      synchronized (mDownloadLock) {
        if (mDownloadCallbackDataPtr != CPP_NULLPTR) {
          if (errorCode != ConstantsHelper.CALLBACK_ERROR_NONE) {
            completeNativeLoadImageError(mDownloadCallbackDataPtr, errorCode, errorMessage);
          } else {
            completeNativeLoadedImage(mDownloadCallbackDataPtr, result);
          }
        }
      }
    }
  }

  /** Native callback to instruct the C++ wrapper to complete the corresponding future. */
  public static native void completeNativeImageFutureCallback(
      long nativeImagePtr, int errorCode, String errorMessage);

  /** Native callback invoked upon successfully downloading an image. */
  public static native void completeNativeLoadedImage(long nativeImagePtr, byte[] image);

  /** Native callback upon encountering an error downloading an image. */
  public static native void completeNativeLoadImageError(
      long nativeImagePtr, int errorCode, String errorMessage);
}
