#!/bin/bash

source googletest.sh || exit 1

# Copied from `googletest.sh`. The only difference is the original uses an
# absolute path to python, which does not exist in some environments we are
# running. There is always a python in PATH in our environments though.
reserve_random_unused_tcp_port () {
    python -c '
import os, random, socket, sys, time
timeout = float(sys.argv[1])
def CheckPort(port):
  ts = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP)
  us = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
  try:
    ts.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ts.bind(("", port))
    ts.listen(1)
    us.bind(("", port))
    # Return the sockets to prevent garbage collection.
    return (ts, us)
  except socket.error:
    ts.close()
    us.close()
    return None
for i in range(128):
  port = random.randint(32760, 59759)
  bound_sockets = CheckPort(port)
  if bound_sockets:
    print port
    if timeout > 0:
      sys.stdin.close()
      sys.stdout.close()
      sys.stderr.close()
      if os.fork() == 0:
        time.sleep(timeout)
    sys.exit(0)
print "NO_FREE_PORT_FOUND"
sys.exit(1)
' "${1:-0}"
}

# Find JAVA
JAVABIN=$( "$TEST_SRCDIR"/google3/third_party/java/jdk/java_path )
if [[ ! -f "$JAVABIN" ]]; then
  echo "Failed to locate java.";
exit 1
fi

# Find executable and shared libs
BINDIR="$TEST_SRCDIR/google3/firebase/firestore/client/cpp"
FIRESTORE_EMULATOR_JAR=$BINDIR/CloudFirestore_emulator.jar
# Pick a random available port with timeout 0
EMULATOR_PORT=$( reserve_random_unused_tcp_port 0 )

# Start Firestore Emulator
echo "Running $JAVABIN -jar $FIRESTORE_EMULATOR_JAR --port=$EMULATOR_PORT"
"$JAVABIN" -jar "$FIRESTORE_EMULATOR_JAR" --port="$EMULATOR_PORT" &
EMULATOR_PID=$!
sleep 3

# Running Android tests.
if [[ -f "$(ls "$BINDIR"/*_test_android_prod)" ]]; then
 for test in $(ls "$BINDIR"/*_test_android_prod); do
   "$test" --test_args=firestore_emulator_port="$EMULATOR_PORT" || \
           die "Test $test failed" $?
 done
fi

# Running iOS tests.
if [[ -f "$(ls "$BINDIR"/*_test_ios)" ]]; then
 # Write Firestore Emulator address to a temp file
 echo "localhost:$EMULATOR_PORT" > /tmp/emulator_address
 for TEST in $(ls "$BINDIR"/*_test_ios); do
   "$TEST" || die "Test $TEST failed" $?
 done
 rm /tmp/emulator_address
fi

# Running Linux tests.
if [[ -f "$(ls "$BINDIR"/*_test)" ]]; then
 # Write Firestore Emulator address to a temp file
 echo "localhost:$EMULATOR_PORT" > /tmp/emulator_address
 for TEST in $(ls "$BINDIR"/*_test); do
   "$TEST" || die "Test $TEST failed" $?
 done
 rm /tmp/emulator_address
fi

kill $EMULATOR_PID
echo "Pass"
