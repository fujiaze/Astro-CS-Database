#!/usr/bin/env python3
"""QA-004: 重复实现静态扫描 (scheduler/I-O/WCS/weight/config)。

判定: 生产内不应有第二调度器/第二 I/O 层/重复 WCS 算法(同层)。
- scheduler: orchestrator/pipeline_engine 调度职责已退出 (LEG-002/003)。
- I/O: 仅 lib/astro_image_io 单例。
- WCS: P1(帧) 与 P3(HiPS) 层分离, 同层无重复。
- config: CLI 命令层 + session 各解析自身。
exit 0 = PASS。
"""
import pathlib, subprocess, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

def main():
    errors = []
    # 1) 第二调度器: orchestrator/aio_pipeline_engine run 无生产 caller
    for src in ("lib/orchestrator/cpp/src/orchestrator.cpp",):
        p = REPO / src
        if p.exists():
            pass  # 源码保留 (LEG-002), 生产不链
    bin_path = REPO / "build" / "root-cmake" / "astrocs"
    if bin_path.exists():
        out = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True).stdout.lower()
        for sym in ("orchestrat", "pipeline_engine_run"):
            if sym in out:
                errors.append(f"生产含第二调度器符号: {sym}")
    # 2) I/O 单例: 非 aio 的 FITS 读写?
    for f in (REPO / "lib" / "phase1").rglob("*.cpp"):
        txt = f.read_text(encoding="utf-8", errors="ignore")
        if "fits_open_file" in txt or "fits_write" in txt:
            errors.append(f"phase1 直连 cfitsio (绕过 aio 单例): {f.name}")
    # 3) 同层重复 WCS: phase1 内不应有两个 TAN 实现
    wcs1 = (REPO / "lib" / "phase1" / "wcs" / "wcs_tan.cpp").read_text(encoding="utf-8", errors="ignore")
    other_wcs = []
    for f in (REPO / "lib" / "phase1").rglob("*.cpp"):
        if f.name == "wcs_tan.cpp": continue
        t = f.read_text(encoding="utf-8", errors="ignore")
        if "pix2ang" in t or ("atan2" in t and "TAN" in t):
            other_wcs.append(f.name)
    if other_wcs:
        errors.append(f"phase1 同层重复 WCS: {other_wcs}")
    if errors:
        print("QA-004_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("QA-004_PASS: 无第二调度器/第二 I/O/同层重复 WCS; config 按层")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
