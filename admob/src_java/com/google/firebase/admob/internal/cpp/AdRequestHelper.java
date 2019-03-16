/*
 * Copyright 2016 Google LLC
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

package com.google.firebase.admob.internal.cpp;

import java.util.Calendar;
import java.util.Date;

/**
 * Helper class to make interactions between the AdMob C++ wrapper and Java
 * AdRequest objects cleaner. This involves translating calls coming from C++
 * into their (typically more complicated) Java equivalents.
 */
public class AdRequestHelper {

  public AdRequestHelper() {}

  /**
   * Creates a {@link java.lang.Date} from the provided date information.
   * @param year The year to use in creating the Date object
   * @param month The month to use in creating the Date object
   * @param day The day to use in creating the Date object
   * @return A Date object with the appropriate date
   */
  public Date createDate(int year, int month, int day) {
    try {
      Calendar cal = Calendar.getInstance();
      cal.setLenient(false);
      cal.set(year, month - 1, day);
      return cal.getTime();
    } catch (IllegalArgumentException e) {
      return null;
    }
  }
}
