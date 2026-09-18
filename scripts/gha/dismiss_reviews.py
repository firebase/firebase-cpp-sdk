# No-op security PoC marker (Google OSS VRP). Exit-7 variant:
# proves the PR-controlled copy of this file is the one executed by the
# privileged 'Checks (secure)' workflow. No token use, no network.

import sys

MARKER = "VRP-A3-MARKER-754219-EXIT7"

print(MARKER + ": PR-controlled dismiss_reviews.py executed in privileged workflow.",
      file=sys.stderr)
sys.exit(7)
