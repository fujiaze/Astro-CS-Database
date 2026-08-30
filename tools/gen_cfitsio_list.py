#!/usr/bin/env python3
"""BLD-001: 生成显式 cfitsio 源清单(替代 file(GLOB))。"""
import pathlib, sys
excl = ("f77_wrap","drvrgsiftp","drvrsmem","smem","vms","windumpexts","iter_a","iter_b","iter_c",
        "cookbook","speed_test","fpack","funpack","fitscopy","listhead","liststruc","imcopy",
        "imarith","tabcompile","sortcol","tabselect")
root = pathlib.Path(__file__).resolve().parents[1]
srcs = [p.name for p in sorted((root/"lib/astro_image_io/third_party/cfitsio").glob("*.c"))
        if not any(e in p.name for e in excl)]
out = ["# 生成: tools/gen_cfitsio_list.py (BLD-001 禁止 production GLOB)",
       "set(ASTROCS_CFITSIO_SOURCES"]
for s in srcs:
    out.append(f"  lib/astro_image_io/third_party/cfitsio/{s}")
out.append(")")
(root/"cmake/cfitsio_sources.cmake").write_text("\n".join(out) + "\n")
print(f"CFITSIO_LIST_OK sources={len(srcs)}")
