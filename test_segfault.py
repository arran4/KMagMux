import subprocess
import time
import os

env = os.environ.copy()
env["LD_LIBRARY_PATH"] = "./build"
env["QT_QPA_PLATFORM"] = "offscreen"
env["HOME"] = "/home/jules"

p = subprocess.Popen(["./build/KMagMux"], env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(2)
subprocess.run(["xdotool", "search", "--name", "KMagMux", "windowclose"])
out, err = p.communicate()
print("Exit code:", p.returncode)
