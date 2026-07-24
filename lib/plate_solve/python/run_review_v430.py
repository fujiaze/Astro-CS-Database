# -*- coding: utf-8 -*-
"""
V4.30 待校验帧 WCS 重投影调试图生成
功能: 对 6 帧 V4.30 待校验帧生成 WCS 重投影调试图 (Siril MTF 拉伸 + 前1000亮星红色十字标记)
用途: 与 V4.29 对比边缘精度改善, 输出到 review_v430 目录
调用: py run_review_v430.py
依赖: visualize_reproject.py (复用 init_environment + visualize_frame)
注意: 单帧内部 ipv_solver/star_detector 已用 OpenMP 16 线程, 6 帧串行避免与 OpenMP 过度并行争抢 CPU
"""
import os
import sys
import json
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from visualize_reproject import init_environment, visualize_frame, PROJECT_ROOT, OUTPUT_DIR

OUT_DIR = os.path.join(OUTPUT_DIR, "review_v430")

# 6 帧: 任务描述的帧名/时间戳在 testdata 中不存在 (NGC4945 T2 Lum 无 20250504, Galaxy_Center 无 T2, NGC247 T2 最早 20250816)
# 帧1 用任务指定帧 (V4.30 唯一失败帧), 帧2-6 用同类型 V4.30 RMS 最高的真实存在帧替代
FRAMES = [
    r"testdata\lights1\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts",
    r"testdata\lights\T2\Lum\NGC4945_FD_T2_flying_dutchman-20250207@051926-600S-Lum.fts",
    r"testdata\lights1\panel2\Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@012031-180S-Blue.fts",
    r"testdata\lights\NGC247_T2_flying_dutchman-20250902@084332-1200S-OIII.fts",
    r"testdata\lights\NGC247_T2_flying_dutchman-20250902@074953-600S-Red.fts",
    r"testdata\lights1\panel2\Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@011255-180S-Blue.fts",
]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"输出目录: {OUT_DIR}")
    print(f"待处理帧数: {len(FRAMES)}")
    print("=" * 70)

    env = init_environment()
    results = []
    t0 = time.time()

    for i, rel_path in enumerate(FRAMES, 1):
        full_path = os.path.join(PROJECT_ROOT, rel_path)
        label = os.path.splitext(os.path.basename(full_path))[0]
        out_path = os.path.join(OUT_DIR, f"{label}_v430_reproject.png")

        print(f"\n[{i}/{len(FRAMES)}] {label}")
        print(f"文件: {full_path}")

        if not os.path.isfile(full_path):
            print(f"  !!! 文件不存在, 跳过")
            results.append({"label": label, "success": False, "error": "文件不存在"})
            continue

        try:
            info = visualize_frame(env, full_path, out_path, top_n=1000)
            results.append({
                "label": label,
                "fits_path": full_path,
                "out_path": out_path,
                **info,
            })
            print(f"  结果: success={info['success']}, RMS={info['rms_px']:.4f}px "
                  f"({info['rms_arcsec']:.4f}\"), n_pairs={info['n_pairs']}, "
                  f"n_marked={info['n_marked']}, elapsed={info['elapsed']:.1f}s")
        except Exception as e:
            import traceback
            print(f"  !!! ERROR: {e}")
            traceback.print_exc()
            results.append({
                "label": label,
                "fits_path": full_path,
                "out_path": out_path,
                "success": False,
                "error": str(e),
            })

    env["solver"].close()
    total_elapsed = time.time() - t0

    print("\n" + "=" * 70)
    print("V4.30 待校验帧调试图生成汇总")
    print("=" * 70)
    for r in results:
        status = "OK" if r.get("success") else "FAIL"
        rms = r.get("rms_px", 0)
        rms_arc = r.get("rms_arcsec", 0)
        n = r.get("n_pairs", 0)
        print(f"  {r['label'][:65]:65} | {status:4} | RMS={rms:.4f}px ({rms_arc:.3f}\") | n={n}")
    print(f"\n总耗时: {total_elapsed:.1f}s")
    print(f"输出目录: {OUT_DIR}")

    summary_path = os.path.join(OUT_DIR, "v430_review_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump({
            "version": "V4.30",
            "total_elapsed_sec": round(total_elapsed, 2),
            "results": results,
        }, f, ensure_ascii=False, indent=2)
    print(f"汇总数据: {summary_path}")


if __name__ == "__main__":
    main()
