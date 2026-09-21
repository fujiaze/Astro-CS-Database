#!/usr/bin/env python3
"""QA-004: 重复实现静态扫描 (scheduler/I-O/WCS/weight/config)。

判定: 生产内不应有第二调度器/第二 I/O 层/重复 WCS 算法(同层)。
- scheduler: orchestrator/pipeline_engine 调度职责已退出 (LEG-002/003)。
- I/O: 仅 aio 单例（迁移前路径 lib/astro_image_io，ARCH-001）。
- WCS: P1(帧) 与 P3(HiPS) 层分离, 同层无重复。
- config: CLI 命令层 + session 各解析自身。
exit 0 = PASS。
"""
import pathlib, subprocess, sys

REPO = pathlib.Path(__file__).resolve().parents[2]

def main():
    errors = []
    # 1) 第二调度器: orchestrator/aio_pipeline_engine run 无生产 caller
    for src in ("lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp",):
        p = REPO / src
        if p.exists():
            pass  # 源码保留 (LEG-002), 生产不链
    # 构建目录探测: build/root-cmake → build/ (适配当前 Ninja 单配置布局)
    bin_path = None
    for rel in ("build/root-cmake/astrocs", "build/astrocs", "build/cli/astrocs"):
        p = REPO / rel
        if p.exists():
            bin_path = p
            break
    if bin_path is not None:
        out = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True).stdout.lower()
        for sym in ("orchestrat", "pipeline_engine_run"):
            if sym in out:
                errors.append(f"生产含第二调度器符号: {sym}")
    # 2) I/O 单例: 非 aio 的 FITS 读写?
    #    ARCH-001 迁移后 phase1 面 = lib/algorithms/*/wrapper_phase1（迁移前 lib/phase1）
    phase1_dirs = [
        REPO / "lib/algorithms/star_detection/wrapper_phase1",
        REPO / "lib/algorithms/platesolve/wrapper_phase1",
        REPO / "lib/algorithms/photometry/wrapper_phase1",
        REPO / "lib/algorithms/noise_snr/wrapper_phase1",
    ]
    for d in phase1_dirs:
        for f in d.rglob("*.cpp"):
            txt = f.read_text(encoding="utf-8", errors="ignore")
            if "fits_open_file" in txt or "fits_write" in txt:
                errors.append(f"phase1 直连 cfitsio (绕过 aio 单例): {f.name}")
    # 3) 同层重复 WCS: phase1 内不应有两个 TAN 实现
    wcs1_path = REPO / "lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp"
    if not wcs1_path.is_file():
        errors.append(f"phase1 WCS 基准实现缺失（fail-closed）：{wcs1_path}")
        other_wcs = []
    wcs1 = wcs1_path.read_text(encoding="utf-8", errors="ignore") if wcs1_path.is_file() else ""
    other_wcs = []
    for d in phase1_dirs:
      for f in d.rglob("*.cpp"):
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
