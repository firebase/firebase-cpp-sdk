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

package com.google.firebase.firestore.internal.cpp;

import java.util.IllegalFormatException;
import java.util.Locale;

/**
 * <p>This class was adapted from the {@code LongObjectHashMap} class of HPPC v0.9.1:
 * https://search.maven.org/artifact/com.carrotsearch/hppc/0.9.1/jar
 */
public final class LongObjectHashMap {

  private static final float DEFAULT_LOAD_FACTOR = 0.75f;
  private static final int DEFAULT_EXPECTED_ELEMENTS = 4;
  private static final int MIN_HASH_ARRAY_LENGTH = 4;
  private static final int MAX_HASH_ARRAY_LENGTH = 0x80000000 >>> 1;
  private static final int PHI_C32 = 0x9e3779b9;
  private static final long PHI_C64 = 0x9e3779b97f4a7c15L;

  private final double loadFactor = DEFAULT_LOAD_FACTOR;

  private long[] keys;
  private Object[] values;
  private int assigned;
  private int resizeAt;
  private int mask;
  private long nextKey;

  public LongObjectHashMap() {
    ensureCapacity(DEFAULT_EXPECTED_ELEMENTS);
  }

  public void put(long key, Object value) {
    if (key == 0) {
      throw new IllegalArgumentException("0 is a special value that is not supported for use as a key");
    }
    assert assigned < mask + 1;

    final int mask = this.mask;
    final long[] keys = this.keys;
    int slot = hashKey(key) & mask;

    long existing;
    while (!((existing = keys[slot]) == 0)) {
      if (key == existing) {
        values[slot] = value;
        return;
      }
      slot = (slot + 1) & mask;
    }

    if (assigned == resizeAt) {
      allocateThenInsertThenRehash(slot, key, value);
    } else {
      keys[slot] = key;
      values[slot] = value;
    }

    assigned++;
  }

  public void remove(long key) {
    if (key == 0) {
      // Just return if a key of 0 is given, since it's not a valid key.
      return;
    }

    final int mask = this.mask;
    final long[] keys = this.keys;
    int slot = hashKey(key) & mask;

    long existing;
    while (!((existing = keys[slot]) == 0)) {
      if (key == existing) {
        shiftConflictingKeys(slot);
        return;
      }
      slot = (slot + 1) & mask;
    }
  }

  public Object get(long key) {
    if (key == 0) {
      // Just return null if a key of 0 is given, since it's not a valid key and, therefore, does
      // not have a value.
      return null;
    }

    final long[] keys = this.keys;
    final int mask = this.mask;
    int slot = hashKey(key) & mask;

    long existing;
    while (!((existing = keys[slot]) == 0)) {
      if (((key) == (existing))) {
        return values[slot];
      }
      slot = (slot + 1) & mask;
    }

    return null;
  }

  public int size() {
    return assigned;
  }

  private boolean isEmpty() {
    return size() == 0;
  }

  /**
   * Ensure this container can hold at least the given number of keys (entries) without resizing its
   * buffers.
   *
   * @param expectedElements The total number of keys, inclusive.
   */
  private void ensureCapacity(int expectedElements) {
    if (expectedElements > resizeAt || keys == null) {
      final long[] prevKeys = this.keys;
      final Object[] prevValues = this.values;
      allocateBuffers(minBufferSize(expectedElements, loadFactor));
      if (prevKeys != null && !isEmpty()) {
        rehash(prevKeys, prevValues);
      }
    }
  }

  /**
   * Allocate new internal buffers. This method attempts to allocate and assign internal buffers
   * atomically (either allocations succeed or not).
   */
  private void allocateBuffers(int arraySize) {
    assert Integer.bitCount(arraySize) == 1;

    // Ensure no change is done if we hit an OOM.
    long[] prevKeys = this.keys;
    Object[] prevValues = this.values;
    try {
      int emptyElementSlot = 1;
      this.keys = (new long[arraySize + emptyElementSlot]);
      this.values = (new Object[arraySize + emptyElementSlot]);
    } catch (OutOfMemoryError e) {
      this.keys = prevKeys;
      this.values = prevValues;
      throw new BufferAllocationException(
          "Not enough memory to allocate buffers for rehashing: %,d -> %,d",
          e, this.mask + 1, arraySize);
    }

    this.resizeAt = expandAtCount(arraySize, loadFactor);
    this.mask = arraySize - 1;
  }

  private void rehash(long[] fromKeys, Object[] fromValues) {
    assert fromKeys.length == fromValues.length && checkPowerOfTwo(fromKeys.length - 1);

    // Rehash all stored key/value pairs into the new buffers.
    final long[] keys = this.keys;
    final Object[] values = this.values;
    final int mask = this.mask;
    long existing;

    // Copy the zero element's slot, then rehash everything else.
    int from = fromKeys.length - 1;
    keys[keys.length - 1] = fromKeys[from];
    values[values.length - 1] = fromValues[from];
    while (--from >= 0) {
      if (!((existing = fromKeys[from]) == 0)) {
        int slot = hashKey(existing) & mask;
        while (!((keys[slot]) == 0)) {
          slot = (slot + 1) & mask;
        }
        keys[slot] = existing;
        values[slot] = fromValues[from];
      }
    }
  }

  /**
   * Returns a hash code for the given key.
   *
   * <p>The output from this function should evenly distribute keys across the entire integer range.
   */
  private int hashKey(long key) {
    assert !((key) == 0); // Handled as a special case (empty slot marker).
    return mixPhi(key);
  }

  /**
   * This method is invoked when there is a new key/ value pair to be inserted into the buffers but
   * there is not enough empty slots to do so.
   *
   * <p>New buffers are allocated. If this succeeds, we know we can proceed with rehashing so we
   * assign the pending element to the previous buffer (possibly violating the invariant of having
   * at least one empty slot) and rehash all keys, substituting new buffers at the end.
   */
  private void allocateThenInsertThenRehash(int slot, long pendingKey, Object pendingValue) {
    assert assigned == resizeAt && ((keys[slot]) == 0) && !((pendingKey) == 0);

    // Try to allocate new buffers first. If we OOM, we leave in a consistent state.
    final long[] prevKeys = this.keys;
    final Object[] prevValues = this.values;
    allocateBuffers(nextBufferSize(mask + 1, size(), loadFactor));
    assert this.keys.length > prevKeys.length;

    // We have succeeded at allocating new data so insert the pending key/value at
    // the free slot in the old arrays before rehashing.
    prevKeys[slot] = pendingKey;
    prevValues[slot] = pendingValue;

    // Rehash old keys, including the pending key.
    rehash(prevKeys, prevValues);
  }

  /**
   * Shift all the slot-conflicting keys and values allocated to (and including) <code>slot</code>.
   */
  private void shiftConflictingKeys(int gapSlot) {
    final long[] keys = this.keys;
    final Object[] values = this.values;
    final int mask = this.mask;

    // Perform shifts of conflicting keys to fill in the gap.
    int distance = 0;
    while (true) {
      final int slot = (gapSlot + (++distance)) & mask;
      final long existing = keys[slot];
      if (((existing) == 0)) {
        break;
      }

      final int idealSlot = hashKey(existing);
      final int shift = (slot - idealSlot) & mask;
      if (shift >= distance) {
        // Entry at this position was originally at or before the gap slot.
        // Move the conflict-shifted entry to the gap's position and repeat the procedure
        // for any entries to the right of the current position, treating it
        // as the new gap.
        keys[gapSlot] = existing;
        values[gapSlot] = values[slot];
        gapSlot = slot;
        distance = 0;
      }
    }

    // Mark the last found gap slot without a conflict as empty.
    keys[gapSlot] = 0L;
    values[gapSlot] = null;
    assigned--;
  }

  private static int nextBufferSize(int arraySize, int elements, double loadFactor) {
    assert checkPowerOfTwo(arraySize);
    if (arraySize == MAX_HASH_ARRAY_LENGTH) {
      throw new BufferAllocationException(
          "Maximum array size exceeded for this load factor (elements: %d, load factor: %f)",
          elements, loadFactor);
    }
    return arraySize << 1;
  }

  private static int mixPhi(long k) {
    final long h = k * PHI_C64;
    return (int) (h ^ (h >>> 32));
  }

  private static int mixPhi(int k) {
    final int h = k * PHI_C32;
    return h ^ (h >>> 16);
  }

  private static int expandAtCount(int arraySize, double loadFactor) {
    assert checkPowerOfTwo(arraySize);
    // Take care of hash container invariant (there has to be at least one empty slot to ensure
    // the lookup loop finds either the element or an empty slot).
    return Math.min(arraySize - 1, (int) Math.ceil(arraySize * loadFactor));
  }

  private static boolean checkPowerOfTwo(int arraySize) {
    // These are internals, we can just assert without retrying.
    assert arraySize > 1;
    assert nextHighestPowerOfTwo(arraySize) == arraySize;
    return true;
  }

  private static int minBufferSize(int elements, double loadFactor) {
    if (elements < 0) {
      throw new IllegalArgumentException("Number of elements must be >= 0: " + elements);
    }

    long length = (long) Math.ceil(elements / loadFactor);
    if (length == elements) {
      length++;
    }
    length = Math.max(MIN_HASH_ARRAY_LENGTH, nextHighestPowerOfTwo(length));

    if (length > MAX_HASH_ARRAY_LENGTH) {
      throw new BufferAllocationException(
          "Maximum array size exceeded for this load factor (elements: %d, load factor: %f)",
          elements, loadFactor);
    }

    return (int) length;
  }

  /**
   * returns the next highest power of two, or the current value if it's already a power of two or
   * zero
   */
  private static int nextHighestPowerOfTwo(int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
  }

  /**
   * returns the next highest power of two, or the current value if it's already a power of two or
   * zero
   */
  public static long nextHighestPowerOfTwo(long v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
  }

  private static final class BufferAllocationException extends RuntimeException {
    public BufferAllocationException(String message) {
      super(message);
    }

    public BufferAllocationException(String message, Object... args) {
      this(message, null, args);
    }

    public BufferAllocationException(String message, Throwable t, Object... args) {
      super(formatMessage(message, t, args), t);
    }

    private static String formatMessage(String message, Throwable t, Object... args) {
      try {
        return String.format(Locale.ROOT, message, args);
      } catch (IllegalFormatException e) {
        BufferAllocationException substitute =
            new BufferAllocationException(message + " [ILLEGAL FORMAT, ARGS SUPPRESSED]");
        if (t != null) {
          substitute.addSuppressed(t);
        }
        substitute.addSuppressed(e);
        throw substitute;
      }
    }
  }
}
