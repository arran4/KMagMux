import subprocess
import time
import os

env = os.environ.copy()
env["LD_LIBRARY_PATH"] = "./build"
env["QT_QPA_PLATFORM"] = "offscreen"
env["HOME"] = "/home/jules"

p = subprocess.Popen(["./build/KMagMux", "magnet:?xt=urn:btih:0123"], env=env)
time.sleep(2)
subprocess.run(["kill", "-s", "INT", str(p.pid)])
p.wait()
print("Exit code:", p.returncode)
