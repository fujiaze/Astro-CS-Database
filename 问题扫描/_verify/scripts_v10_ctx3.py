#!/usr/bin/env python3
def show(f,a,b,tag=""):
    print("="*16,f,a,b,tag)
    try: L=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception as e: print("READFAIL",e); return
    for i in range(a,min(b,len(L))+1): print("%5d: %s"%(i,L[i-1]))
show("lib/healpix_db/healpix_drizzle/fits_reader.cpp",289,345,"BITPIX 白名单与尺寸")
show("lib/astro_image_io/src/hips/aio_hips_reader.cpp",50,70,"valid_product_params")
import subprocess
print(subprocess.run(["grep","-rn","bad_snr_rows","lib/","cli/","include/"],capture_output=True,text=True).stdout[:2500])
