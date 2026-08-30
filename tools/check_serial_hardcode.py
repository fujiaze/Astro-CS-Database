#!/usr/bin/env python3
"""QA-002: 串行与硬编码静态禁令检查。

禁令 (控制包):
1. 生产禁止 workers=1 / cpu_workers=1 硬编码 (P2-002; AGENTS.md 禁 workers=1)。
2. 禁止 nside=2048 / 2048 魔数硬编码 (P1-006/P3-002; 必须 config 派生)。
3. 禁止 OMP 硬编码线程数 (omp_set_num_threads(固定值); 必须 config/Runtime lease)。
4. 禁止生产目标 file(GLOB) (BLD-001)。

豁免: 测试/工具/注释; 第三方 (cfitsio/AIO/drizzle/HISS)。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

PROD_FILES = [
    "lib/phase1", "lib/phase2/src", "lib/phase3_session",
    "lib/core", "lib/io", "lib/cpu", "lib/calibration/src", "cli/main.cpp",
]
EXEMPT_SUBSTR = ("test", "tool", "README", "fixture", "cfitsio", "third_party")

def main():
    errors = []
    for d in PROD_FILES:
        base = REPO / d
        files = [base] if base.is_file() else (base.rglob("*.cpp") if base.is_dir() else [])
        for f in files:
            if not f.is_file(): continue
            rel = str(f)
            if any(x in rel for x in EXEMPT_SUBSTR): continue
            txt = f.read_text(encoding="utf-8", errors="ignore")
            for ln, line in enumerate(txt.splitlines(), 1):
                # 去注释 (// 和 /* */ 简化)
                code = re.sub(r"//.*$", "", line)
                code = re.sub(r"/\*.*?\*/", "", code)
                if "workers" in code and re.search(r"=\s*1\b", code):
                    errors.append(f"{rel}:{ln} workers=1 硬编码")
                if re.search(r"nside\s*=\s*2048\b", code) or re.search(r'"2048"', code):
                    errors.append(f"{rel}:{ln} nside=2048 硬编码")
                if re.search(r"omp_set_num_threads\(\s*\d+\s*\)", code):
                    errors.append(f"{rel}:{ln} OMP 线程数硬编码")
                if "omp_set_num_threads" in code and "(" in code:
                    m = re.search(r"omp_set_num_threads\(\s*(\d+)\s*\)", code)
                    if m: errors.append(f"{rel}:{ln} OMP 线程数硬编码")
    # GLOB 禁令
    cmake = (REPO / "cli" / "CMakeLists.txt").read_text(encoding="utf-8", errors="ignore")
    for m in re.finditer(r"file\(GLOB[^)]*\)", cmake):
        errors.append(f"cli/CMakeLists.txt GLOB: {m.group(0)[:40]}")
    if errors:
        print("QA-002_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("QA-002_PASS: 无 workers=1/nside=2048/OMP 线程数硬编码; 无生产 GLOB")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
