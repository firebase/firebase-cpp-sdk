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

"""Detects duplicate global symbols between static libraries before linking.

When the Firebase Apple SDK is embedded in a macOS desktop binary its archives
are force-loaded, so every global symbol they define ends up in the link. If
another archive in the same link (e.g. desktop Firestore's nanopb, or a static
zlib/BoringSSL) defines the same symbol, ld64 either fails with "duplicate
symbol" or, worse, silently binds callers to whichever copy it saw first.

This checker reports strong global symbols defined by more than one input
where at least one of the definitions is force-loaded:

  check_duplicate_symbols.py --force-load a.a b.a --link c.a d.a

Duplicates *only* between --link inputs are not reported: the linker only
pulls archive members on demand, so they don't conflict by construction.

Symbols matching --ignore (regex, repeatable) are skipped. By default C++
mangled names (weak/ODR inline functions) and compiler-local labels are
ignored.
"""

import argparse
import collections
import re
import subprocess
import sys

DEFAULT_IGNORES = [
    r"^__Z",    # C++ (Mach-O adds a leading underscore): weak / ODR symbols.
    r"^_Z",     # C++ on ELF, for running the checker on Linux.
    r"^l_",     # Local labels.
    r"^ltmp",   # Local labels.
    r"^___swift_",  # Swift runtime helper thunks (linkonce).
    r"^_\$s",   # Swift symbols; the C++ SDK contains no Swift on desktop.
]
# nm -P type letters for defined, external, non-weak symbols.
STRONG_DEFINED_TYPES = set("TDBSCRIA")


def parse_nm_output(text):
  """Parses `nm -g -P -A` output into (member, symbol, type) tuples.

  Handles both "archive.a[member.o]: sym T 0 0" (llvm-nm) and
  "archive.a:member.o: sym T 0 0" (GNU nm) styles, plus llvm-nm's
  "(for architecture x)" suffix on universal binaries.
  """
  results = []
  for line in text.splitlines():
    line = line.rstrip()
    if not line or line.endswith(":"):
      continue
    # Split "<location>: <symbol> <type> [value] [size]" at the last ": ".
    location, sep, rest = line.rpartition(": ")
    if not sep:
      continue
    fields = rest.split()
    if len(fields) < 2:
      continue
    symbol, symbol_type = fields[0], fields[1]
    location = re.sub(r"\s*\(for architecture [^)]*\)$", "", location)
    results.append((location, symbol, symbol_type))
  return results


def strong_definitions(entries, ignores):
  patterns = [re.compile(p) for p in ignores]
  for member, symbol, symbol_type in entries:
    if symbol_type not in STRONG_DEFINED_TYPES:
      continue
    if any(p.search(symbol) for p in patterns):
      continue
    yield member, symbol


def find_conflicts(force_loaded, linked, ignores):
  """Finds conflicting definitions.

  Args:
    force_loaded: {input_name: [(member, symbol)]} for force-loaded inputs.
    linked: {input_name: [(member, symbol)]} for on-demand archives.
    ignores: regexes of symbols to skip.

  Returns:
    {symbol: [(input_name, member)]} for symbols defined by >1 input where at
    least one definition is force-loaded.
  """
  definitions = collections.defaultdict(list)
  for group, forced in ((force_loaded, True), (linked, False)):
    for input_name, entries in group.items():
      for member, symbol in strong_definitions(
          [(m, s, t) for m, s, t in entries], ignores):
        definitions[symbol].append((input_name, member, forced))
  conflicts = {}
  for symbol, defs in definitions.items():
    inputs = {d[0] for d in defs}
    if len(inputs) > 1 and any(d[2] for d in defs):
      conflicts[symbol] = sorted({(d[0], d[1]) for d in defs})
  return conflicts


def run_nm(nm, path, arch):
  cmd = [nm, "-g", "-P", "-A"]
  if arch:
    cmd += ["-arch", arch]
  cmd.append(path)
  result = subprocess.run(cmd, check=False, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  if result.returncode != 0:
    raise RuntimeError("%s failed for %s:\n%s" % (nm, path, result.stderr))
  return parse_nm_output(result.stdout)


def main(argv):
  parser = argparse.ArgumentParser(
      description=__doc__.splitlines()[0],
      formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
  parser.add_argument("--force-load", nargs="*", default=[],
                      help="Inputs whose members are all loaded.")
  parser.add_argument("--link", nargs="*", default=[],
                      help="Archives linked on demand.")
  parser.add_argument("--ignore", action="append", default=[],
                      help="Regex of symbols to ignore (repeatable).")
  parser.add_argument("--no-default-ignores", action="store_true")
  parser.add_argument("--nm", default="nm")
  parser.add_argument("--arch", default="",
                      help="Architecture to inspect in universal binaries.")
  parser.add_argument("--warn-only", action="store_true",
                      help="Report conflicts without failing.")
  args = parser.parse_args(argv)

  ignores = list(args.ignore)
  if not args.no_default_ignores:
    ignores += DEFAULT_IGNORES
  try:
    force_loaded = {p: run_nm(args.nm, p, args.arch) for p in args.force_load}
    linked = {p: run_nm(args.nm, p, args.arch) for p in args.link
              if p not in force_loaded}
  except (OSError, RuntimeError) as error:
    print("check_duplicate_symbols: %s" % error, file=sys.stderr)
    return 2
  conflicts = find_conflicts(force_loaded, linked, ignores)
  if not conflicts:
    print("check_duplicate_symbols: no conflicts across %d inputs." %
          (len(force_loaded) + len(linked)))
    return 0
  stream = sys.stderr
  print("check_duplicate_symbols: %d conflicting symbol(s):" % len(conflicts),
        file=stream)
  for symbol in sorted(conflicts):
    print("  %s" % symbol, file=stream)
    for input_name, member in conflicts[symbol]:
      print("      %s (%s)" % (member, input_name), file=stream)
  return 0 if args.warn_only else 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
