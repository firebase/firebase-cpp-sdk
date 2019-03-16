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

package com.google.firebase.storage.internal.cpp;

import com.google.firebase.storage.StreamDownloadTask;
import java.io.IOException;
import java.io.InputStream;

/** A stream downloader into a C++ byte array. * */
public class CppByteDownloader implements StreamDownloadTask.StreamProcessor {
  protected long cppBufferPointer = 0;
  protected long cppBufferSize = 0;
  protected final Object lockObject = new Object();

  /**
   * Construct a CppByteDownloader. First parameter is a C++ pointers, second is a number of bytes.
   */
  public CppByteDownloader(long cppBufferPointer, long cppBufferSize) {
    this.cppBufferPointer = cppBufferPointer;
    this.cppBufferSize = cppBufferSize;
  }

  /** Discard native pointers from this instance. */
  public void discardPointers() {
    synchronized (lockObject) {
      this.cppBufferPointer = 0;
      this.cppBufferSize = 0;
    }
  }

  @Override
  public void doInBackground(StreamDownloadTask.TaskSnapshot state, InputStream stream)
      throws IOException {
    try {
      long bufferOffset = 0;
      int bytesRead;
      byte[] bytes = new byte[16384];
      while ((bytesRead = stream.read(bytes, 0, bytes.length)) != -1) {
        if (bufferOffset + bytesRead <= this.cppBufferSize) {
          writeBytesToBuffer(bufferOffset, bytes, bytesRead);
          bufferOffset += bytesRead;
        } else {
          throw new IndexOutOfBoundsException("The maximum allowed buffer size was exceeded.");
        }
      }
    } finally {
      stream.close();
    }
  }

  public void writeBytesToBuffer(long bufferOffset, byte[] bytes, long numBytes) {
    synchronized (lockObject) {
      if (this.cppBufferPointer != 0) {
        writeBytes(this.cppBufferPointer, this.cppBufferSize, bufferOffset, bytes, numBytes);
      }
    }
  }

  private static native void writeBytes(
      long cppBufferPointer, long cppBufferSize, long cppBufferOffset, byte[] bytes, long numBytes);
}
