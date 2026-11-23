#!/usr/bin/env python3
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Runs unit and multi-App stress tests under ASan/UBSan or TSan.

Runs each stress test case in an isolated subprocess with a deadlock watchdog
that captures a GDB thread backtrace before terminating hung processes, so a
single crash or deadlock does not prevent subsequent tests from reporting.
"""

import argparse
import os
import platform
import shutil
import signal
import subprocess
import sys
import time


def maybe_wrap_setarch(cmd, disable_aslr):
  if disable_aslr and shutil.which("setarch"):
    return ["setarch", platform.machine(), "-R"] + cmd
  return cmd


def list_gtest_cases(binary, disable_aslr):
  cmd = maybe_wrap_setarch([binary, "--gtest_list_tests"], disable_aslr)
  proc = subprocess.run(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
      check=False, timeout=30
  )
  cases = []
  suite = None
  for raw_line in proc.stdout.splitlines():
    line = raw_line.split("#", 1)[0].rstrip()
    if not line:
      continue
    if not line[0].isspace() and line.endswith("."):
      suite = line
    elif suite and line[0].isspace():
      cases.append(suite + line.strip())
  return cases


def capture_gdb_backtrace(pid):
  if not shutil.which("gdb"):
    return "(gdb not installed)"
  try:
    res = subprocess.run(
        ["gdb", "-batch", "-ex", "thread apply all bt", "-p", str(pid)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=20,
        check=False,
    )
    return res.stdout
  except Exception as exc:  # pylint: disable=broad-except
    return f"(gdb failed: {exc})"


def run_single_case(binary, test_filter, timeout_sec, disable_aslr, env):
  cmd = maybe_wrap_setarch(
      [
          binary,
          f"--gtest_filter={test_filter}",
          "--gtest_catch_exceptions=0",
      ],
      disable_aslr,
  )
  print(f"\n>>> RUNNING {os.path.basename(binary)} :: {test_filter}",
        flush=True)
  start = time.monotonic()
  proc = subprocess.Popen(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      errors="replace",
      env=env,
      start_new_session=True,
  )
  try:
    output, _ = proc.communicate(timeout=timeout_sec)
    elapsed = time.monotonic() - start
    status = "PASS" if proc.returncode == 0 else f"FAIL (rc={proc.returncode})"
    print(output, flush=True)
    return (test_filter, status, elapsed)
  except subprocess.TimeoutExpired:
    elapsed = time.monotonic() - start
    print(f"\n!!! TIMEOUT after {elapsed:.1f}s — capturing GDB backtrace "
          f"for pid {proc.pid} !!!", flush=True)
    bt = capture_gdb_backtrace(proc.pid)
    print(bt, flush=True)
    try:
      os.killpg(proc.pid, signal.SIGABRT)
      output, _ = proc.communicate(timeout=10)
    except Exception:  # pylint: disable=broad-except
      try:
        os.killpg(proc.pid, signal.SIGKILL)
      except OSError:
        pass
      output, _ = proc.communicate()
    print(output, flush=True)
    return (test_filter, f"TIMEOUT/DEADLOCK ({elapsed:.0f}s)", elapsed)


def run_gtest_binary_isolated(binary, timeout_sec, disable_aslr, env):
  if not os.path.exists(binary):
    print(f"ERROR: test binary not found: {binary}", flush=True)
    return [(binary, "MISSING", 0.0)]
  cases = list_gtest_cases(binary, disable_aslr)
  if not cases:
    print(f"ERROR: no gtest cases discovered in {binary}", flush=True)
    return [(binary, "NO_TESTS", 0.0)]
  results = []
  for case in cases:
    results.append(
        run_single_case(binary, case, timeout_sec, disable_aslr, env)
    )
  return results


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--build_dir", default="build")
  parser.add_argument("--mode", choices=["asan", "tsan"], required=True)
  parser.add_argument("--run_unit_ctest", action="store_true")
  parser.add_argument("--run_unit_stress", action="store_true")
  parser.add_argument("--run_emulator_stress", action="store_true")
  parser.add_argument("--case_timeout", type=int, default=120)
  args = parser.parse_args()

  disable_aslr = args.mode == "tsan"
  env = os.environ.copy()
  env.setdefault("LANG", "en_US.UTF-8")

  any_failed = False
  summary_rows = []

  if args.run_unit_ctest:
    print("\n================ PHASE 1: EXISTING UNIT SUITE ================",
          flush=True)
    cmd = maybe_wrap_setarch(
        [
            "ctest",
            "--test-dir",
            args.build_dir,
            "-LE",
            "multi_app_stress",
            "--output-on-failure",
            "--timeout",
            str(args.case_timeout),
            "-j",
            str(max(1, min(4, os.cpu_count() or 2))),
        ],
        disable_aslr,
    )
    proc = subprocess.run(cmd, env=env, check=False)
    status = "PASS" if proc.returncode == 0 else f"FAIL (rc={proc.returncode})"
    summary_rows.append(("existing_unit_ctest", status, 0.0))
    if proc.returncode != 0:
      any_failed = True

  if args.run_unit_stress:
    print("\n================ PHASE 2: OFFLINE MULTI-APP STRESS ================",
          flush=True)
    binary = os.path.join(
        args.build_dir,
        "testing",
        "stress",
        "firebase_multi_app_lifecycle_unit_test",
    )
    rows = run_gtest_binary_isolated(
        binary, args.case_timeout, disable_aslr, env
    )
    summary_rows.extend(rows)
    if any(r[1] != "PASS" for r in rows):
      any_failed = True

  if args.run_emulator_stress:
    print("\n================ PHASE 3: EMULATOR MULTI-APP STRESS ================",
          flush=True)
    binary = os.path.join(
        args.build_dir,
        "testing",
        "stress",
        "firebase_multi_app_emulator_stress_test",
    )
    rows = run_gtest_binary_isolated(
        binary, args.case_timeout, disable_aslr, env
    )
    summary_rows.extend(rows)
    if any(r[1] != "PASS" for r in rows):
      any_failed = True

  print("\n================ SANITIZER SUMMARY ================", flush=True)
  for name, status, elapsed in summary_rows:
    suffix = f" ({elapsed:.1f}s)" if elapsed > 0 else ""
    print(f"  {status:24s}  {name}{suffix}", flush=True)

  sys.exit(1 if any_failed else 0)


if __name__ == "__main__":
  main()
