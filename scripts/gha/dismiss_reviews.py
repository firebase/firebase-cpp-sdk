# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0

"""No-op security PoC marker (Google OSS VRP).

This file is intentionally inert: it does not read, print, or transmit the
--token argument it receives, and it performs no network access.
"""

MARKER = "VRP-A3-MARKER-754219"


def main(argv):
  print(MARKER + ": attacker-controlled script executed inside the "
        "privileged 'Checks (secure)' workflow. Token argument "
        "intentionally untouched.")
  return 0


if __name__ == "__main__":
  raise SystemExit(main([]))
