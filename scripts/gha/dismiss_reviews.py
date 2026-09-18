# No-op security PoC marker (Google OSS VRP).
# Writes a marker line to the job step summary (runner-provided file).
# Does not read/print/transmit --token; no network access.

import os
import sys

MARKER = "VRP-A3-MARKER-754219-SUMMARY"

print(MARKER + ": PR-controlled dismiss_reviews.py executed in privileged workflow")
try:
  with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as f:
    f.write("\n**" + MARKER + "**: PR-controlled dismiss_reviews.py was executed by the privileged Checks (secure) workflow. PoC only - no credentials accessed.\n")
except Exception as e:
  print("summary write skipped:", e)
sys.exit(0)
