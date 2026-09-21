#!/usr/bin/env python3
"""CPU-001: C ABI 边界检查。

规则:
1. ABI 头族（common_abi_v1.h + lib/include/astrocs/abi/*.h + *_abi_v1.h）内每个
   typedef struct 必须带 uint32_t struct_size/abi_version（或 acs_head 头块）。
2. 上述跨边界头不得 include <string>/<vector>/<iostream> 等 STL。
3. 头内不得出现 noexcept(false)/throw 声明(异常不跨边界)。
4. 任一登记 ABI 头缺失 → fail-closed 判红（不得把「文件不存在」当「无违规」）。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[2]

# 跨 DLL/ABI 边界头族（唯一事实源 lib/include/astrocs/** 下的 C ABI 头）
ABI_HEADER_RELS = [
    "lib/include/astrocs/common_abi_v1.h",
    "lib/include/astrocs/io/aio_abi_v1.h",
    "lib/include/astrocs/contracts/artifact_abi_v1.h",
]

STL_INC = re.compile(r"#include\s*[<\"](string|vector|iostream|sstream|fstream|map|unordered_map|set|memory|exception|stdexcept|thread|mutex)")
THROW = re.compile(r"\bthrow\b|\bnoexcept\s*\(\s*false\s*\)")
# typedef struct [tag] { body } name;（body 允许一层嵌套花括号）
STRUCT_BLOCK = re.compile(r"typedef\s+struct(?:\s+\w+)?\s*\{((?:[^{}]|\{[^{}]*\})*)\}\s*(\w+)\s*;", re.S)


def abi_headers() -> list[pathlib.Path]:
    """返回头部文件清单；目录缺失时由调用方判红。"""
    rels = list(ABI_HEADER_RELS)
    abi_dir = REPO / "lib/include/astrocs/abi"
    if abi_dir.is_dir():
        rels += [str(p.relative_to(REPO)).replace("\\", "/")
                 for p in sorted(abi_dir.glob("*.h"))]
    else:
        rels.append("lib/include/astrocs/abi/")  # 目录缺失哨兵，下面按目录判红
    return [REPO / r for r in rels]


def check() -> int:
    errors: list[str] = []
    headers = abi_headers()
    scanned = 0
    structs = 0
    missing_dirs = []
    for h in headers:
        if h.is_dir() or not h.is_file():
            if h.is_dir():
                missing_dirs.append(str(h))
                errors.append(f"ABI 头目录缺失（fail-closed）：{h}")
            else:
                errors.append(f"ABI 头缺失（fail-closed）：{h}")
            continue
        scanned += 1
        text = h.read_text(encoding="utf-8", errors="replace")
        rel = h.relative_to(REPO).as_posix()
        # 1) 每个 ABI 结构块必须带 size/version（块级判定，不得用全文命中代替）
        blocks = STRUCT_BLOCK.findall(text)
        for body, name in blocks:
            structs += 1
            # acs_head 头块承载 struct_size/abi_version（允许任意空白/多字段申明）
            has_acs_head = re.search(r"\bacs_head\s+\w+", body) is not None
            has_head = has_acs_head or re.search(r"\bstruct_size\b", body) is not None
            has_ver = has_acs_head or re.search(r"\babi_version\b", body) is not None
            if not has_head:
                errors.append(f"{rel}: ABI struct {name}: missing uint32_t struct_size/acs_head")
            if not has_ver:
                errors.append(f"{rel}: ABI struct {name}: missing abi_version/acs_head")
        # 2) 跨边界头不得 include STL
        for m in STL_INC.finditer(text):
            errors.append(f"{rel}:{text.count(chr(10), 0, m.start()) + 1}: "
                          f"STL include across boundary: {m.group(0).strip()}")
        # 3) 不得声明 throw/noexcept(false)
        for m in THROW.finditer(text):
            errors.append(f"{rel}:{text.count(chr(10), 0, m.start()) + 1}: "
                          f"exception/throw across boundary: {m.group(0).strip()}")
    # backend/provider 公开头存在性（ARCH-001 迁移后路径）
    _BACKEND_HOST = REPO / "lib/infrastructure/benchmark/backend_host"
    if not _BACKEND_HOST.is_dir():
        errors.append("backend_host 公开头目录缺失（fail-closed）：%s" % _BACKEND_HOST)
    if errors:
        print("CPU-001_ABI_VIOLATION:")
        for e in errors:
            print("  " + e)
        return 1
    print(f"CPU-001_PASS: {scanned} 个 ABI 头 / {structs} 个结构均带 size/version; "
          f"{scanned} 头无 STL/异常跨边界")
    return 0


if __name__ == "__main__":
    raise SystemExit(check())
