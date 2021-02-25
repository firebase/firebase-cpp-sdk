// Copyright 2017 Google LLC
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

package com.google.firebase.remoteconfig;

import com.google.android.gms.tasks.Task;
import com.google.firebase.testing.cppsdk.ConfigAndroid;
import com.google.firebase.testing.cppsdk.ConfigRow;
import com.google.firebase.testing.cppsdk.FakeReporter;
import com.google.firebase.testing.cppsdk.TickerAndroid;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/** Fake FirebaseRemoteConfig */
public class FirebaseRemoteConfig {

  private static final String FN_GET_LONG = "FirebaseRemoteConfig.getLong";
  private static final String FN_GET_STRING = "FirebaseRemoteConfig.getString";
  private static final String FN_GET_BOOLEAN = "FirebaseRemoteConfig.getBoolean";
  private static final String FN_GET_DOUBLE = "FirebaseRemoteConfig.getDouble";
  private static final String FN_GET_VALUE = "FirebaseRemoteConfig.getValue";
  private static final String FN_GET_INFO = "FirebaseRemoteConfig.getInfo";
  private static final String FN_GET_KEYS_BY_PREFIX = "FirebaseRemoteConfig.getKeysByPrefix";
  private static final String FN_GET_ALL = "FirebaseRemoteConfig.getAll";
  private static final String FN_FETCH = "FirebaseRemoteConfig.fetch";
  private static final String FN_ENSURE_INITIALIZED = "FirebaseRemoteConfig.ensureInitialized";
  private static final String FN_ACTIVATE = "FirebaseRemoteConfig.activate";
  private static final String FN_FETCH_AND_ACTIVATE = "FirebaseRemoteConfig.fetchAndActivate";
  private static final String FN_SET_DEFAULTS_ASYNC = "FirebaseRemoteConfig.setDefaultsAsync";
  private static final String FN_SET_CONFIG_SETTINGS_ASYNC =
      "FirebaseRemoteConfig.setConfigSettingsAsync";

  FirebaseRemoteConfig() {}

  public static FirebaseRemoteConfig getInstance() {
    return new FirebaseRemoteConfig();
  }

  public long getLong(String key) {
    ConfigRow row = ConfigAndroid.get(FN_GET_LONG);
    long result = row.returnvalue().tlong();
    FakeReporter.addReportWithResult(FN_GET_LONG, Long.toString(result), key);
    return result;
  }

  public long getLong(String key, String namespace) {
    ConfigRow row = ConfigAndroid.get(FN_GET_LONG);
    long result = row.returnvalue().tlong();
    FakeReporter.addReportWithResult(FN_GET_LONG, Long.toString(result), key, namespace);
    return result;
  }

  public String getString(String key) {
    ConfigRow row = ConfigAndroid.get(FN_GET_STRING);
    String result = row.returnvalue().tstring();
    FakeReporter.addReportWithResult(FN_GET_STRING, result, key);
    return result;
  }

  public String getString(String key, String namespace) {
    ConfigRow row = ConfigAndroid.get(FN_GET_STRING);
    String result = row.returnvalue().tstring();
    FakeReporter.addReportWithResult(FN_GET_STRING, result, key, namespace);
    return result;
  }

  public boolean getBoolean(String key) {
    ConfigRow row = ConfigAndroid.get(FN_GET_BOOLEAN);
    boolean result = row.returnvalue().tbool();
    FakeReporter.addReportWithResult(FN_GET_BOOLEAN, String.valueOf(result), key);
    return result;
  }

  public boolean getBoolean(String key, String namespace) {
    ConfigRow row = ConfigAndroid.get(FN_GET_BOOLEAN);
    boolean result = row.returnvalue().tbool();
    FakeReporter.addReportWithResult(FN_GET_BOOLEAN, String.valueOf(result), key, namespace);
    return result;
  }

  public double getDouble(String key) {
    ConfigRow row = ConfigAndroid.get(FN_GET_DOUBLE);
    double result = row.returnvalue().tdouble();
    FakeReporter.addReportWithResult(FN_GET_DOUBLE, String.format("%.3f", result), key);
    return result;
  }

  public double getDouble(String key, String namespace) {
    ConfigRow row = ConfigAndroid.get(FN_GET_DOUBLE);
    double result = row.returnvalue().tdouble();
    FakeReporter.addReportWithResult(FN_GET_DOUBLE, String.format("%.3f", result), key, namespace);
    return result;
  }

  public FirebaseRemoteConfigValue getValue(String key) {
    FakeReporter.addReport(FN_GET_VALUE, key);
    return new FirebaseRemoteConfigValue();
  }

  public FirebaseRemoteConfigValue getValue(String key, String namespace) {
    FakeReporter.addReport(FN_GET_VALUE, key, namespace);
    return new FirebaseRemoteConfigValue();
  }

  public FirebaseRemoteConfigInfo getInfo() {
    FakeReporter.addReport(FN_GET_INFO);
    return new FirebaseRemoteConfigInfo();
  }

  public Set<String> getKeysByPrefix(String prefix) {
    ConfigRow row = ConfigAndroid.get(FN_GET_KEYS_BY_PREFIX);
    Set<String> result = new TreeSet<>(stringToStringList(row.returnvalue().tstring()));
    FakeReporter.addReportWithResult(
        "FirebaseRemoteConfig.getKeysByPrefix", result.toString(), prefix);
    return result;
  }

  public Set<String> getKeysByPrefix(String prefix, String namespace) {
    ConfigRow row = ConfigAndroid.get(FN_GET_KEYS_BY_PREFIX);
    Set<String> result = new TreeSet<>(stringToStringList(row.returnvalue().tstring()));
    FakeReporter.addReportWithResult(
        "FirebaseRemoteConfig.getKeysByPrefix", result.toString(), prefix, namespace);
    return result;
  }

  public Map<String, FirebaseRemoteConfigValue> getAll() {
    FakeReporter.addReport(FN_GET_ALL);
    return new HashMap<>();
  }

  private static Task<Void> voidHelper(String configKey) {
    Task<Void> result = Task.forResult(configKey, null);
    TickerAndroid.register(result);
    return result;
  }

  public Task<Void> fetch() {
    FakeReporter.addReport(FN_FETCH);
    return voidHelper(FN_FETCH);
  }

  public Task<Void> fetch(long cacheExpirationSeconds) {
    FakeReporter.addReport(FN_FETCH, Long.toString(cacheExpirationSeconds));
    return voidHelper(FN_FETCH);
  }

  private static Task<FirebaseRemoteConfigInfo> eIHelper(String configKey) {
    Task<FirebaseRemoteConfigInfo> result =
        Task.forResult(configKey, new FirebaseRemoteConfigInfo());
    TickerAndroid.register(result);
    return result;
  }

  public Task<FirebaseRemoteConfigInfo> ensureInitialized() {
    FakeReporter.addReport(FN_ENSURE_INITIALIZED);
    return eIHelper(FN_ENSURE_INITIALIZED);
  }

  private static Task<Boolean> booleanHelper(String configKey) {
    Task<Boolean> result = Task.forResult(configKey, Boolean.TRUE);
    TickerAndroid.register(result);
    return result;
  }

  public Task<Boolean> activate() {
    FakeReporter.addReport(FN_ACTIVATE);
    return booleanHelper(FN_ACTIVATE);
  }

  public Task<Boolean> fetchAndActivate() {
    FakeReporter.addReport(FN_FETCH_AND_ACTIVATE);
    return booleanHelper(FN_FETCH_AND_ACTIVATE);
  }

  public Task<Void> setDefaultsAsync(int resourceId) {
    FakeReporter.addReport(FN_SET_DEFAULTS_ASYNC, Integer.toString(resourceId));
    return voidHelper(FN_SET_DEFAULTS_ASYNC);
  }

  public Task<Void> setDefaultsAsync(Map<String, Object> defaults) {
    Map<String, Object> sorted = new TreeMap<>(defaults);
    FakeReporter.addReport(FN_SET_DEFAULTS_ASYNC, sorted.toString());
    return voidHelper(FN_SET_DEFAULTS_ASYNC);
  }

  public Task<Void> setConfigSettingsAsync(FirebaseRemoteConfigSettings settings) {
    FakeReporter.addReport(FN_SET_CONFIG_SETTINGS_ASYNC);
    return voidHelper(FN_SET_CONFIG_SETTINGS_ASYNC);
  }

  private static List<String> stringToStringList(String s) {
    s = s.substring(1, s.length() - 1);
    if (s.length() == 0) {
      return new ArrayList<String>();
    }
    String[] arr = s.split(",");
    for (int i = 0; i < arr.length; i++) {
      arr[i] = arr[i].trim();
    }
    return Arrays.asList(arr);
  }

}
