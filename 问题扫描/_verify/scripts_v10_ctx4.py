#!/usr/bin/env python3
def show(f,a,b,tag=""):
    print("="*14,f,a,b,tag)
    try: L=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception as e: print("READFAIL",e); return
    for i in range(a,min(b,len(L))+1): print("%5d: %s"%(i,L[i-1]))
show("lib/phase2/src/sampler.cpp",100,124,"pixfrac 来源")
show("lib/phase2/src/sampler.cpp",140,175,"pixfrac 消费")
show("lib/astro_image_io/src/hips/aio_hips_writer.cpp",408,425,"cards 来源")
show("cli/commands.cpp",196,210,"sleep_ms")
show("runtime/registry/module_registry.c",335,365,"abi 使用")
