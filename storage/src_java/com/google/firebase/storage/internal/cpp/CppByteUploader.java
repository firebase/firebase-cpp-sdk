// Copyright 2018 Google LLC
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

import java.io.IOException;
import java.io.InputStream;

/** Reads from a C++ byte array. * */
public class CppByteUploader extends InputStream {
  /** Arbitrary number of bytes available for unbound streams. */
  private static final int DEFAULT_BYTES_AVAILABLE = (64 * 1024);
  /** Value that indicates the end of the stream. */
  private static final int END_OF_STREAM = -1;

  private long cppBufferPointer = 0;
  private long cppBufferSize = 0;
  private long cppBufferOffset = 0;
  private final Object lockObject = new Object();
  private final byte[] readByteBuffer = new byte[1];

  /**
   * Construct a CppByteUploader. This takes a pointer to a C++ buffer, size of the buffer in bytes
   * and the offset to read from the buffer. If cppBufferSize is < 0 the stream will report
   * DEFAULT_BYTES_AVAILABLE as the amount of data left in the stream unless the end of stream is
   * reached. Reporting a default value for the data available allows the C++ callback to return an
   * unbound length stream.
   *
   * <p>cppBufferPointer does not have to be an actual pointer into the C++ buffer, it can be used
   * by the C++ implementation to hold a pointer to an object that tracks the stream state.
   * cppBufferOffset is reported to the C++ implementation simply indicate how far the reader of the
   * CppByteUploader has progressed into the stream.
   */
  public CppByteUploader(long cppBufferPointer, long cppBufferSize, long cppBufferOffset) {
    this.cppBufferPointer = cppBufferPointer;
    this.cppBufferSize = cppBufferSize;
    this.cppBufferOffset = cppBufferOffset;
  }

  /** Discard native pointers from this instance. */
  public void discardPointers() {
    synchronized (lockObject) {
      cppBufferPointer = 0;
      cppBufferSize = 0;
      cppBufferOffset = 0;
    }
  }

  @Override
  public int available() {
    long remaining = cppBufferSize - cppBufferOffset;
    if (cppBufferSize < 0 || remaining > Integer.MAX_VALUE) {
      return DEFAULT_BYTES_AVAILABLE;
    }
    return (int) remaining;
  }

  @Override
  public void close() {
    discardPointers();
  }

  // com.google.firebase.storage.StreamUploadTask does not use this.
  @Override
  public void mark(int readLimit) {
    // Mark is not supported.
  }

  // com.google.firebase.storage.StreamUploadTask does not use this.
  @Override
  public boolean markSupported() {
    return false;
  }

  @Override
  public int read() throws IOException {
    synchronized (lockObject) {
      return read(readByteBuffer, 0, 1) == 1 ? readByteBuffer[0] : END_OF_STREAM;
    }
  }

  @Override
  public int read(byte[] bytes, int bytesOffset, int numBytes) throws IOException {
    if (numBytes == 0) {
      return 0;
    }
    if (bytes == null) {
      throw new NullPointerException("bytes argument is null");
    }
    // Make sure numBytes will fit into the buffer.
    int spaceAvailable = bytes.length - bytesOffset;
    if (numBytes < 0 || bytesOffset < 0 || numBytes > spaceAvailable) {
      throw new IndexOutOfBoundsException(
          String.format(
              "Attempt to read %d bytes into byte array with %d bytes in space available",
              numBytes, spaceAvailable));
    }
    // Clamp the number of bytes to read by the data remaining in C++ buffer.
    int bytesRead;
    synchronized (lockObject) {
      if (cppBufferPointer == 0) {
        throw new IOException("Underlying buffer was discarded.");
      }
      bytesRead =
          readBytes(cppBufferPointer, cppBufferSize, cppBufferOffset, bytes, bytesOffset, numBytes);
      if (bytesRead >= 0) {
        cppBufferOffset += bytesRead;
      } else if (bytesRead != END_OF_STREAM) {
        throw new IOException(
            String.format("An error occurred while reading stream error code=%d", bytesRead));
      }
    }
    return bytesRead;
  }

  @Override
  public int read(byte[] bytes) throws IOException {
    return read(bytes, 0, bytes.length);
  }

  @Override
  public void reset() {
    // Mark is not supported.
  }

  @Override
  public long skip(long bytesToSkip) throws IOException {
    throw new IOException("CppByteUploader does not support seek.");
  }

  /**
   * Reads numBytes from cppBufferPointer at cppBufferOffset up to cppBufferSize bytes into "bytes"
   * at bytesOffset.
   *
   * <p>e.g memcpy(bytes[bytesOffset], cppBufferPointer + cppBufferOffset, min(cppBufferSize -
   * cppBufferOffset, numBytesToRead));
   *
   * <p>cppBufferPointer can simply pointer to the state that tracks the C++ buffer, cppBufferOffset
   * is a value that increases as this function succeeds. cppBufferSize can be -1 if the stream is
   * initialized as unbound.
   *
   * <p>This method should return the number of bytes copied into the Java byte array, -1 if at the
   * of the stream or < -1 if an error occurred or streaming was aborted.
   */
  private static native int readBytes(
      long cppBufferPointer,
      long cppBufferSize,
      long cppBufferOffset,
      byte[] bytes,
      int bytesOffset,
      int numBytesToRead);
};
