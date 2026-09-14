#!/usr/bin/env python3
import re
def show(f,a,b,tag=""):
    print("="*18,f,a,b,tag)
    try: L=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception as e: print("READFAIL",e); return
    for i in range(a,min(b,len(L))+1): print("%5d: %s"%(i,L[i-1]))
show("lib/healpix_db/healpix_drizzle/drizzle_engine.cpp",1118,1132,"gain stod")
show("lib/healpix_db/healpix_drizzle/drizzle_engine.cpp",2005,2020,"gain stod 2")
show("lib/astro_image_io/src/hips/aio_hips_reader.cpp",296,325,"valid_product_params?")
import subprocess
print(subprocess.run(["grep","-rn","valid_product_params","lib/astro_image_io/src/"],capture_output=True,text=True).stdout)
print("== node_count/rootPosition 越界读检查 ==")
show("lib/gaia_xpsd_client/src/gaia_client.c",1300,1345,"tree nodes")
