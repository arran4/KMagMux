import subprocess
import time
import os

env = os.environ.copy()
env["LD_LIBRARY_PATH"] = "./build"
env["QT_QPA_PLATFORM"] = "offscreen"
env["HOME"] = "/home/jules"

p = subprocess.Popen(["gdb", "-batch", "-ex", "run", "-ex", "bt", "--args", "./build/KMagMux"], env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(2)
# try to send a real quit signal
subprocess.run(["xdotool", "search", "--name", "KMagMux", "windowclose"])
out, err = p.communicate()
print("Exit code:", p.returncode)
print("OUT:", out.decode('utf-8'))
print("ERR:", err.decode('utf-8'))
