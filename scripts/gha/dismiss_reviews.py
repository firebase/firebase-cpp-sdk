# No-op security PoC marker (Google OSS VRP), variant 6.
# Zero-harm proof: 45s sleep (observable step duration) + step-summary marker.
# No token access, no network, exit 0.

import os
import sys
import time

MARKER = "VRP-A3-MARKER-754219-RUN6"

print(MARKER + ": PR-controlled dismiss_reviews.py executed in privileged Checks (secure) workflow")
time.sleep(45)
try:
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as f:
        f.write("\n**VRP-A3-MARKER-754219-RUN6**: PR-controlled dismiss_reviews.py executed by the privileged pull_request_target workflow (Checks (secure)). 45s sleep proves attacker-controlled code ran. Zero-harm PoC: no credentials read, no network.\n")
except Exception as e:
    print("summary write skipped:", e)
sys.exit(0)
