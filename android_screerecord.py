import argparse
import subprocess
import signal
import sys
import time

def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument("output_file")
  parsed_args = arg_parser.parse_args()

  args = ["adb", "shell", "screenrecord", "--bugreport", "/sdcard/out.mp4"]
  proc = subprocess.Popen(args)

  print("Press ENTER to stop recording")
  sys.stdin.readline()

  proc.send_signal(signal.SIGINT)
  proc.wait()
  time.sleep(2)

  subprocess.check_call(["adb", "pull", "/sdcard/out.mp4", parsed_args.output_file])


if __name__ == "__main__":
  main()
