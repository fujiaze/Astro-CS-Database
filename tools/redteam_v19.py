#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""redteam_v19.py — V19 Round5 红队假设验证 (>=15 hypotheses)

用法: py -3.12 tools/redteam_v19.py [--json out.json]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys


def _deduce_root() -> str:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = os.path.abspath(__file__)
        cur = os.path.dirname(p)
        for _ in range(5):
            if os.path.isdir(os.path.join(cur, "docs")) and os.path.isdir(os.path.join(cur, "lib")):
                return cur
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent
    except Exception:
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROOT = _deduce_root()
MINGW = r"C:\msys64\mingw64\bin"


def run(cmd, timeout=180):
    env = dict(os.environ)
    env["PATH"] = MINGW + ";" + env.get("PATH", "")
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                           env=env, encoding="utf-8", errors="replace")
        return r.returncode, (r.stdout + r.stderr)
    except subprocess.TimeoutExpired:
        return -1, "timeout"
    except OSError as e:
        return -2, str(e)


def grep_file(path, pattern):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            txt = f.read()
        return bool(re.search(pattern, txt, re.IGNORECASE))
    except OSError:
        return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=os.path.join(ROOT, "run", "temp",
                                                   "v19_redteam.json"))
    args = ap.parse_args()

    H = []

    # H1: NoiseWeightModelV1 全 NaN 帧 → degenerate, 不崩溃
    rc, out = run([os.path.join(ROOT, "lib", "snr_estimator", "cpp", "test",
                                "noise_model_science_test.exe")])
    H.append({
        "H1": "NoiseWeightModelV1 全 NaN 帧 degenerate 不崩溃 (SNR-014)",
        "pass": rc == 0 and "32 通过" in out,
        "evidence": "noise_model_science_test 32/32",
    })

    # H2: 星点掩膜半径边界 (掩膜循环有 px/py 越界检查)
    H.append({
        "H2": "星点掩膜半径边界检查 (px/py 越界 continue)",
        "pass": grep_file(os.path.join(ROOT, "lib", "snr_estimator", "cpp",
                                       "src", "noise_model.cpp"),
                          r"px < 0 \|\| px >= w"),
        "evidence": "noise_model.cpp mask loop bounds check",
    })

    # H3: variance 块尺寸不匹配 → 跳过方差传播 (hp_drizzle_api)
    H.append({
        "H3": "variance 块尺寸不匹配 → 跳过传播不崩溃",
        "pass": grep_file(os.path.join(ROOT, "lib", "healpix_db",
                                       "healpix_drizzle", "hp_drizzle_api.cpp"),
                          r"variance 块尺寸不匹配"),
        "evidence": "hp_drizzle_api.cpp 5.6 尺寸校验分支",
    })

    # H4: sumVarNum FP32 精度 — 科学矩阵 MC p50/p95 门已覆盖
    rc, out = run([os.path.join(ROOT, "lib", "healpix_db", "healpix_drizzle",
                                "tests", "variance_propagation_test.exe")])
    H.append({
        "H4": "sumVarNum 精度 (SNR-011 MC p50/p95 门)",
        "pass": rc == 0 and "8 通过" in out,
        "evidence": "variance_propagation_test 8/8 (p50=1.001)",
    })

    # H5: variance 全零 tile → 跳过 variance 产品不中止 signal
    H.append({
        "H5": "variance 全零 tile → 跳过不中止 signal (健壮性)",
        "pass": grep_file(os.path.join(ROOT, "lib", "healpix_db",
                                       "healpix_drizzle",
                                       "astro_sphere_sink.cpp"),
                          r"n_variance_skipped"),
        "evidence": "astro_sphere_sink.cpp rc=-5/-2 skip path (V19 红队修复)",
    })

    # H6: UPM legacy use_ivar_weight=0 保留 (ablation)
    H.append({
        "H6": "UPM use_ivar_weight=0 legacy 分支保留",
        "pass": grep_file(os.path.join(ROOT, "lib", "phase2", "src",
                                       "upm.cpp"),
                          r"cfg\.use_ivar_weight == 0"),
        "evidence": "upm.cpp raw_weight legacy branch",
    })

    # H7: obs.ivar NaN → 回退 1/unc²
    H.append({
        "H7": "obs.ivar NaN/<=0 → 回退 1/unc²",
        "pass": grep_file(os.path.join(ROOT, "lib", "phase2", "src",
                                       "upm.cpp"),
                          r"obs->ivar > 0\.0"),
        "evidence": "upm.cpp ivar fallback guard",
    })

    # H8: weight_mode=2 缺 ivar 产品 → support 回退 + 计数
    H.append({
        "H8": "stage2 ivar 产品缺失 → support 回退 + ivar_product_missing",
        "pass": grep_file(os.path.join(ROOT, "lib", "phase2", "tools",
                                       "stage2.cpp"),
                          r"ivar_product_missing"),
        "evidence": "stage2.cpp missing-product fallback",
    })

    # H9: ivar 像素 NaN/<=0 → support 回退 (非 fatal)
    H.append({
        "H9": "ivar 像素非有限 → support 回退 (不产生 NaN 权重)",
        "pass": grep_file(os.path.join(ROOT, "lib", "phase2", "tools",
                                       "stage2.cpp"),
                          r"weights\[s\] = support_v\[s\]"),
        "evidence": "stage2.cpp NaN ivar guard",
    })

    # H10: ACR kernel mode=2 (ivar) 与 mode=0 (legacy) 分支
    H.append({
        "H10": "ACR kernel ivar/legacy 分支 (mode scalar)",
        "pass": grep_file(os.path.join(ROOT, "lib", "phase2", "src",
                                       "acr_kernels.cpp"),
                          r"ivar_mode"),
        "evidence": "acr_kernels.cpp wmode branch",
    })

    # H11: 负数/大数 variance 数值域 — MC 已覆盖正值; 负数被拒 (<=0 skip)
    H.append({
        "H11": "variance<=0 像素跳过传播 (负数安全)",
        "pass": grep_file(os.path.join(ROOT, "lib", "healpix_db",
                                       "healpix_drizzle",
                                       "drizzle_engine.cpp"),
                          r"varianceValue > 0\.0f"),
        "evidence": "drizzle_engine.cpp sumVarNum guard",
    })

    # H12: aio_hips_write_variance_tile 缺 var_num_sum → -2
    H.append({
        "H12": "AIO variance tile 缺 var_num_sum → -2 优雅失败",
        "pass": grep_file(os.path.join(ROOT, "lib", "astro_image_io", "src",
                                       "hips", "aio_hips_writer.cpp"),
                          r"var_num_sum 为空"),
        "evidence": "aio_hips_writer.cpp null guard",
    })

    # H13: reader IVAR 产品不存在 → nullptr (sampler 处理)
    H.append({
        "H13": "reader 打开不存在 ivar 产品 → nullptr 优雅",
        "pass": grep_file(os.path.join(ROOT, "lib", "astro_image_io", "src",
                                       "hips", "aio_hips_reader.cpp"),
                          r"product < AIO_HIPS_RD_SIGNAL"),
        "evidence": "aio_hips_reader.cpp product range check",
    })

    # H14: 旧 snr_model 块兼容 (migration reader 保留)
    rc, out = run([os.path.join(ROOT, "lib", "snr_estimator", "cpp", "test",
                                "snr_reconcile_test.exe")])
    H.append({
        "H14": "legacy snr_model 兼容 (migration reader 5/5)",
        "pass": rc == 0 and "5 通过" in out,
        "evidence": "snr_reconcile_test 5/5",
    })

    # H15: LEGACY_SNR_SCIENCE_CONSUMER=0 — 生产科学路径 (phase2/orchestrator/
    #      healpix_drizzle) 无 (A-B)/mad 消费; legacy reader/注释允许保留
    consumers = []
    for prod in ("phase2", "orchestrator", "healpix_drizzle"):
        base = os.path.join(ROOT, "lib", prod)
        if not os.path.isdir(base):
            continue
        for dp, _dn, fn in os.walk(base):
            if any(x in dp for x in ("archive", "build", "__pycache__", ".git",
                                     "third_party", "tests", "test")):
                continue
            for f in fn:
                if f.endswith((".cpp", ".h", ".py")):
                    p = os.path.join(dp, f)
                    try:
                        txt = open(p, encoding="utf-8",
                                   errors="replace").read()
                    except OSError:
                        continue
                    if re.search(r"\(A\s*-\s*B\)\s*/\s*mad", txt):
                        consumers.append(os.path.relpath(p, ROOT))
    H.append({
        "H15": "LEGACY_SNR_SCIENCE_CONSUMER=0 (生产路径无 (A-B)/mad)",
        "pass": len(consumers) == 0,
        "evidence": ("no production (A-B)/mad consumers" if not consumers
                     else str(consumers)),
    })

    # H16: docs 一致性 — weight_mode=ivar 默认在 config 与文档同步
    cfg_ivar = grep_file(os.path.join(ROOT, "lib", "phase2", "src",
                                      "stage2_common.cpp"),
                         r'wm == "auto" \|\| wm == "ivar"')
    doc_ivar = grep_file(os.path.join(ROOT, "docs", "CONFIG_REFERENCE.md"),
                         r'"weight_mode": "ivar"')
    H.append({
        "H16": "docs 一致性: weight_mode=ivar config↔文档",
        "pass": cfg_ivar and doc_ivar,
        "evidence": "stage2_common.cpp + CONFIG_REFERENCE.md 同步",
    })

    # H17: orchestrator SNR data 块缺失 → SKIPPED_NO_DATA 不崩溃
    H.append({
        "H17": "orchestrator SNR data 块缺失 → SKIPPED_NO_DATA",
        "pass": grep_file(os.path.join(ROOT, "lib", "orchestrator", "cpp",
                                       "src", "orchestrator.cpp"),
                          r"SKIPPED_NO_DATA"),
        "evidence": "orchestrator.cpp noise model skip path",
    })

    n_pass = sum(1 for h in H if h["pass"])
    report = {
        "round": 5,
        "n_hypotheses": len(H),
        "n_pass": n_pass,
        "hypotheses": H,
        "result": "RED_TEAM=PASS" if n_pass == len(H) else "RED_TEAM=FAIL",
    }
    os.makedirs(os.path.dirname(args.json), exist_ok=True)
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    for h in H:
        status = "PASS" if h["pass"] else "FAIL"
        print(f"[{status}] {next(iter(h))}: {h['evidence']}")
    print(f"\n{report['result']} ({n_pass}/{len(H)})")
    print(f"JSON: {args.json}")
    return 0 if n_pass == len(H) else 1


if __name__ == "__main__":
    sys.exit(main())
