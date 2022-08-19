import argparse
import subprocess
import platform
import signal
import sys
import time

def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument("output_file")
  parsed_args = arg_parser.parse_args()

  args = ["adb", "shell", "screenrecord", "--bugreport", "/sdcard/out.mp4"]
  if platform.system() == "Windows":
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
  else:
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  print("Press ENTER to stop recording")
  sys.stdin.readline()

  proc.send_signal(signal.CTRL_C_SIGNAL if platform.system() == "Windows" else signal.SIGINT)
  proc.wait()
  time.sleep(2)

  subprocess.check_call(["adb", "pull", "/sdcard/out.mp4", parsed_args.output_file])


if __name__ == "__main__":
  main()
