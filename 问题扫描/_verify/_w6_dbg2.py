import os
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
print("cwd:", os.getcwd())
for p in ["include/astrocs/abi/status_codes.h", "include/astrocs/abi", "modules/services/io/include/astrocs/io/fits_stream_v1.h"]:
    print("exists", p, os.path.exists(p), "| isfile", os.path.isfile(p))
print("listdir include/astrocs:", sorted(os.listdir("include/astrocs")) if os.path.isdir("include/astrocs") else "NO DIR")
print("listdir include/astrocs/abi:", sorted(os.listdir("include/astrocs/abi")) if os.path.isdir("include/astrocs/abi") else "NO DIR")