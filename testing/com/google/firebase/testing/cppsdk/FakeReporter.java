// Copyright 2020 Google LLC
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

package com.google.firebase.testing.cppsdk;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Report what function was called and with wich arguments. */
public final class FakeReporter {

  private static Map<String, ReportRow> reports = new HashMap<String, ReportRow>();

  public static void reset() {
    reports.clear();
  }

  public static ReportRow addReport(String fake, String... args) {
    return addReportWithResult(fake, "", args);
  }

  public static ReportRow addReportWithResult(String fake, String result, String... args) {
    ReportRow report = new ReportRow(fake);
    report.addResult(result);
    for (String arg : args) {
      report.addArg(arg);
    }
    reports.put(report.getFake(), report);
    return report;
  }

  public static List<String> getAllFakes() {
    return new ArrayList<String>(reports.keySet());
  }

  public static List<String> getFakeArgs(String fake) {
    ReportRow report = reports.get(fake);
    if (report == null) {
      return new ArrayList<String>();
    }
    return report.getArgs();
  }

  public static String getFakeResult(String fake) {
    ReportRow report = reports.get(fake);
    if (report == null) {
      return null;
    }
    return report.getResult();
  }

  /** Report for each fake calling */
  public static class ReportRow {

    private String fake;

    private List<String> args;

    private String result;

    public ReportRow(String fake) {
      this.fake = fake;
      this.args = new ArrayList<String>();
    }

    public ReportRow addArg(String arg) {
      this.args.add(arg);
      return this;
    }

    public ReportRow addResult(String result) {
      this.result = result;
      return this;
    }

    public String getFake() {
      return this.fake;
    }

    public List<String> getArgs() {
      return args;
    }

    public String getResult() {
      return result;
    }
  }
}
