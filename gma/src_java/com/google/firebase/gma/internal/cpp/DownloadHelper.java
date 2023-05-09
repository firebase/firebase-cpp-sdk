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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
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
  // URL to download the asset from.
  private URL url;

  // Request headers.
  private final HashMap<String, String> headers;

  // HTTP response code.
  private int responseCode;

  // Create a new DownloadHelper with a default URL.
  public DownloadHelper(String urlString) throws MalformedURLException {
    this.headers = new HashMap<>();
    setUrl(urlString);
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

  /**
   * Perform a HTTP GET request to the given URL, with the given headers. This method blocks until
   * getting a response. Returns the downloaded file as a byte array.
   */
  public byte[] download() throws IOException {
    if (this.url == null) {
      return null;
    }

    HttpURLConnection connection = (HttpURLConnection) url.openConnection();
    connection.setRequestMethod("GET");
    for (Map.Entry<String, String> entry : this.headers.entrySet()) {
      connection.setRequestProperty(entry.getKey(), entry.getValue());
    }

    responseCode = connection.getResponseCode();

    if (responseCode != HttpURLConnection.HTTP_OK) {
      return null;
    }

    InputStream inputStream = connection.getInputStream();
    ByteArrayOutputStream bytestream = new ByteArrayOutputStream();
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
  }
}
