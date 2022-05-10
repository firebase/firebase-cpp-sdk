// Copyright 2019 Google Inc. All rights reserved.
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

package com.google.firebase.example;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

/**
 * A simple client for performing synchronous HTTP/HTTPS requests, used for testing purposes only.
 */
public final class SimpleHttpRequest {
  private URL url;
  private final HashMap<String, String> headers;
  private byte[] postData;
  private int responseCode;
  /** Create a new SimpleHttpRequest with a default URL. */
  public SimpleHttpRequest(String urlString) throws MalformedURLException {
    this.headers = new HashMap<>();
    this.postData = null;
    setUrl(urlString);
  }
  /** Set the URL to the given string, or null if it can't be parsed. */
  public void setUrl(String urlString) throws MalformedURLException {
    this.url = new URL(urlString);
  }
  /** Get the previously-set URL. */
  public URL getUrl() {
    return this.url;
  }
  /** Set the HTTP POST body, and set this request to a POST request. */
  public void setPostData(byte[] postData) {
    this.postData = postData;
  }
  /** Clear out the HTTP POST body, setting this request back to a GET request. */
  public void clearPostData() {
    this.postData = null;
  }
  /** Add a header key-value pair. */
  public void addHeader(String key, String value) {
    this.headers.put(key, value);
  }
  /** Clear previously-set headers. */
  public void clearHeaders() {
    this.headers.clear();
  }

  /** Get the response code returned by the server, after perform() is finished. */
  public int getResponseCode() {
    return this.responseCode;
  }

  /**
   * Perform a HTTP request to the given URL, with the given headers. If postData is non-null, use a
   * POST request, else use a GET request. This method blocks until getting a response.
   */
  public String perform() throws IOException {
    if (this.url == null) {
      return null;
    }

    HttpURLConnection connection = (HttpURLConnection) url.openConnection();
    connection.setRequestMethod(postData != null ? "POST" : "GET");
    for (Map.Entry<String, String> entry : this.headers.entrySet()) {
      connection.setRequestProperty(entry.getKey(), entry.getValue());
    }
    if (this.postData != null) {
      connection.setDoOutput(true);
      connection.setFixedLengthStreamingMode(postData.length);
      connection.getOutputStream().write(postData);
    }
    responseCode = connection.getResponseCode();
    StringBuilder result = new StringBuilder();
    BufferedReader inputStream =
        new BufferedReader(new InputStreamReader(connection.getInputStream()));
    String line;
    while ((line = inputStream.readLine()) != null) {
      result.append(line);
    }
    connection.disconnect();
    return result.toString();
  }

  /** A one-off helper method to simply open a URL in a browser window. */
  public static void openUrlInBrowser(String urlString, Activity activity) {
    if (urlString.startsWith("data:")) {
      // Use makeMainSelectorActivity to handle data: URLs.
      activity.startActivity(
          Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, Intent.CATEGORY_APP_BROWSER)
              .setData(Uri.parse(urlString)));
    } else {
      // Otherwise use the default intent handler for the URL.
      Intent intent = new Intent(Intent.ACTION_VIEW);
      intent.setData(Uri.parse(urlString));
      activity.startActivity(intent);
    }
  }
}
