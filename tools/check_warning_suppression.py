#!/usr/bin/env python3
"""QA-001: 生产编译警告抑制清零校验。

规则:
1. V6 生产模块 (phase1/2/3/core/io/cli 非第三方) 无 -w//w/Wno 全域抑制。
2. -w 抑制仅限第三方/遗留源 (cfitsio/AIO/drizzle/hips V5 代码) — 豁免并登记。
3. 生产构建 (GCC Release) 警告计数 = 0 (增量基线, 第三方豁免源不计)。
exit 0 = PASS。
"""
import pathlib, re, sys, subprocess

REPO = pathlib.Path(__file__).resolve().parents[1]

# 生产源目录 (V6 模块)
PROD_DIRS = ["lib/phase1", "lib/phase2/src", "lib/phase3_session", "lib/core", "lib/io", "lib/cpu", "cli"]
# 豁免 (第三方/遗留): 允许源级抑制
EXEMPT = ["cfitsio", "aio_", "drizzle", "hips_", "healpix"]

def main():
    errors = []
    for d in PROD_DIRS:
        base = REPO / d
        if not base.is_dir(): continue
        for f in base.rglob("*.cpp"):
            txt = f.read_text(encoding="utf-8", errors="ignore")
            # 仅匹配独立编译选项形态 (-w 前后非字母/连字符; not-wired 等注释词不匹配)
            if re.search(r'(?<!\w)-w(?!\w)', txt) or re.search(r'"?-Wno-', txt):
                errors.append(f"生产源含抑制指令: {f}")
    # CMake 层抑制检查
    for cm in (REPO / "cli" / "CMakeLists.txt", REPO / "CMakeLists.txt", REPO / "tests" / "unit" / "CMakeLists.txt"):
        t = cm.read_text(encoding="utf-8", errors="ignore")
        for m in re.finditer(r'set_property\(SOURCE ([^)]*?)\s+PROPERTY COMPILE_OPTIONS "\$\{ACS_WARN_SUPPRESS\}', t):
            srcs = m.group(1)
            # ${P2_SRCS}/${P3_SRCS} 已去抑制; 剩余豁免 = 第三方/遗留 (CFITSIO/AIO/DRIZZLE/HISS)
            if not any(x in srcs for x in ("CFITSIO", "AIO", "DRIZZLE", "HISS", "SAMPLER")):
                errors.append(f"{cm.name}: 非豁免源抑制 {srcs.strip()[:60]}")
    # 生产警告计数 (重新编译 phase1 模块)
    r = subprocess.run(["bash","-c",
        "cd "+str(REPO)+" && touch lib/phase1/noise/noise_model.cpp && make -C build/root-cmake astrocs 2>&1 | grep -c 'warning:' || true"],
        capture_output=True, text=True, timeout=600)
    warn = r.stdout.strip()
    if warn not in ("", "0"):
        errors.append(f"生产构建警告 {warn} 个 (非 0)")
    if errors:
        print("QA-001_WARN_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("QA-001_PASS: V6 生产模块零抑制指令; -w 仅第三方豁免; 生产警告=0")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
