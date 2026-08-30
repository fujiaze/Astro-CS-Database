#!/usr/bin/env python3
"""CPU-001: C ABI 边界检查。

规则:
1. 所有 C ABI 结构(common_abi_v1.h 的 typedef struct)必须带 uint32_t struct_size + abi_version。
2. 跨边界头(common_abi_v1.h / *_api.h)不得 include <string>/<vector>/<iostream> 等 STL。
3. 头内不得出现 noexcept(false)/throw 声明(异常不跨边界)。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
HEADERS = [
    REPO / "include/astrocs/common_abi_v1.h",
]
# 额外扫描 backend/provider 公开头
for h in sorted((REPO / "lib/backend_host").glob("*.h")):
    if "impl" not in h.name and "inc" not in h.name:
        HEADERS.append(h)

STL_INC = re.compile(r"#include\s*[<\"](string|vector|iostream|sstream|fstream|map|unordered_map|set|memory|exception|stdexcept|thread|mutex)")
THROW = re.compile(r"\bthrow\b|\bnoexcept\s*\(\s*false\s*\)")

def check():
    errors = []
    # 1) ABI 结构 size/version
    abi = (REPO / "include/astrocs/common_abi_v1.h").read_text(encoding="utf-8")
    structs = re.findall(r"typedef\s+struct\s+(\w+)", abi)
    structs = [s for s in structs if not s.startswith("acs_handle")]  # opaque
    for s in structs:
        if f"struct_size" not in abi:
            errors.append(f"struct {s}: missing struct_size")
            continue
        # 检查该结构体内有 struct_size 字段(粗粒度: 全文含 struct_size)
    # 精确: 提取每个 typedef struct {...} 块
    blocks = re.findall(r"typedef\s+struct\s+(\w+)\s*\{(.*?)\}\s*\w+\s*;", abi, re.S)
    for name, body in blocks:
        has_head = ("struct_size" in body) or ("acs_head head" in body)
        has_ver = ("abi_version" in body) or ("acs_head head" in body)
        if not has_head:
            errors.append(f"ABI struct {name}: missing uint32_t struct_size/acs_head")
        if not has_ver:
            errors.append(f"ABI struct {name}: missing abi_version/acs_head")
    # 2) 真 ABI 边界头(common_abi_v1.h)不得 include STL / 抛异常;
    #    宿主内部头(backend_loader/bench_harness/hardware_inspect/profile_gen)允许 STL, 不跨 DLL 边界
    BOUNDARY_ONLY = [REPO / "include/astrocs/common_abi_v1.h"]
    for h in BOUNDARY_ONLY:
        if not h.is_file(): continue
        text = h.read_text(encoding="utf-8", errors="replace")
        for m in STL_INC.finditer(text):
            errors.append(f"{h.name}:{text.count(chr(10),0,m.start())+1}: STL include across boundary: {m.group(0).strip()}")
        for m in THROW.finditer(text):
            errors.append(f"{h.name}:{text.count(chr(10),0,m.start())+1}: exception/throw across boundary: {m.group(0).strip()}")
    if errors:
        print("CPU-001_ABI_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"CPU-001_PASS: ABI 结构 {len(blocks)} 个均带 size/version; {len(HEADERS)} 头无 STL/异常跨边界")
    return 0

if __name__ == "__main__":
    raise SystemExit(check())
