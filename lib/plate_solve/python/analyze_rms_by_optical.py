# -*- coding: utf-8 -*-
"""
RMS 按光学系统分组分析 + 异常帧挑出
====================================
功能: 读取 visualize_reproject.py --batch 输出的 per_frame.json, 按 (focallen, xpixsz)
      分组为光学系统, 统计各组 RMS 像素分布, 挑出 RMS 高于组内阈值 (P75 + 1.5*IQR
      或绝对阈值 1.0 px) 的帧, 复制其 PNG 到 review/ 文件夹供人工确认。
用途: 全量 790 帧 WCS 精度校验, 自动定位异常帧。

输出:
    - review/ 目录: 异常帧 PNG 副本
    - optical_summary.json: 各光学系统统计
    - review_report.md: 人类可读报告

用法:
    py analyze_rms_by_optical.py
    py analyze_rms_by_optical.py --per-frame <path> --out-dir <path>
"""
import os
import sys
import json
import shutil
import argparse
import statistics
from collections import defaultdict

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

PROJECT_ROOT = r"F:\Astro dev\Astro CS Normalization Database"
DEFAULT_PER_FRAME = os.path.join(
    PROJECT_ROOT, "lib", "plate_solve", "logs", "visualize_reproject", "per_frame.json"
)
DEFAULT_OUT_DIR = os.path.join(
    PROJECT_ROOT, "lib", "plate_solve", "logs", "visualize_reproject"
)


def optical_key(r):
    """光学系统分组键: (focallen, xpixsz)。
    focallen 四舍五入到 0.1mm, xpixsz 到 0.1um, 避免浮点抖动。
    """
    fl = round(float(r.get("focallen", 0)), 1)
    ps = round(float(r.get("xpixsz", 0)), 1)
    return (fl, ps)


def percentile(sorted_list, p):
    """计算百分位数 p (0-100)"""
    if not sorted_list:
        return 0.0
    n = len(sorted_list)
    k = (n - 1) * p / 100.0
    f = int(k)
    c = min(f + 1, n - 1)
    if f == c:
        return sorted_list[f]
    return sorted_list[f] + (sorted_list[c] - sorted_list[f]) * (k - f)


def main():
    parser = argparse.ArgumentParser(description="RMS 按光学系统分组分析")
    parser.add_argument("--per-frame", type=str, default=DEFAULT_PER_FRAME,
                        help="per_frame.json 路径")
    parser.add_argument("--out-dir", type=str, default=DEFAULT_OUT_DIR,
                        help="输出目录")
    parser.add_argument("--abs-threshold", type=float, default=1.0,
                        help="绝对 RMS 阈值 (px), 超过则标记异常 (默认 1.0 px)")
    parser.add_argument("--iqr-mult", type=float, default=1.5,
                        help="IQR 倍数 (默认 1.5, 即 P75 + 1.5*IQR)")
    args = parser.parse_args()

    with open(args.per_frame, "r", encoding="utf-8") as f:
        data = json.load(f)
    results = data.get("results", [])
    print(f"加载 {len(results)} 帧数据")

    # 分组
    groups = defaultdict(list)
    for r in results:
        k = optical_key(r)
        groups[k].append(r)

    # 各组统计
    print(f"\n{'='*80}")
    print(f"{'光学系统':<20} {'fl(mm)':<8} {'ps(um)':<8} {'s0(\"/px)':<10} {'帧数':<6} "
          f"{'P50(px)':<10} {'P75(px)':<10} {'P90(px)':<10} {'max(px)':<10} {'success':<8}")
    print(f"{'='*80}")
    optical_stats = []
    for k in sorted(groups.keys()):
        frames = groups[k]
        rms_list = sorted([float(r["rms_px"]) for r in frames if r.get("success") and "rms_px" in r])
        n_total = len(frames)
        n_ok = len(rms_list)
        if not rms_list:
            print(f"{str(k):<20} {k[0]:<8} {k[1]:<8} {'?':<10} {n_total:<6} "
                  f"{'-':<10} {'-':<10} {'-':<10} {'-':<10} {n_ok}/{n_total}")
            optical_stats.append({
                "focallen": k[0], "xpixsz": k[1], "n_total": n_total, "n_success": 0,
            })
            continue
        s0 = float(frames[0].get("s0_arcsec_per_px", 0))
        p50 = percentile(rms_list, 50)
        p75 = percentile(rms_list, 75)
        p90 = percentile(rms_list, 90)
        mx = max(rms_list)
        print(f"{str(k):<20} {k[0]:<8} {k[1]:<8} {s0:<10.4f} {n_total:<6} "
              f"{p50:<10.4f} {p75:<10.4f} {p90:<10.4f} {mx:<10.4f} {n_ok}/{n_total}")
        optical_stats.append({
            "focallen": k[0], "xpixsz": k[1], "s0_arcsec_per_px": s0,
            "n_total": n_total, "n_success": n_ok,
            "rms_px_p50": p50, "rms_px_p75": p75, "rms_px_p90": p90, "rms_px_max": mx,
        })

    # 挑异常帧: 超过 组内 P75 + 1.5*IQR 或 绝对阈值
    review_items = []
    for k in groups:
        frames = groups[k]
        rms_list = sorted([float(r["rms_px"]) for r in frames if r.get("success") and "rms_px" in r])
        if not rms_list:
            # 失败帧也加入 review
            for r in frames:
                if not r.get("success"):
                    review_items.append((r, k, "solve_failed"))
            continue
        p75 = percentile(rms_list, 75)
        p25 = percentile(rms_list, 25)
        iqr = p75 - p25
        group_threshold = p75 + args.iqr_mult * iqr
        s0 = float(frames[0].get("s0_arcsec_per_px", 0))
        for r in frames:
            if not r.get("success"):
                review_items.append((r, k, "solve_failed"))
                continue
            rms = float(r.get("rms_px", 0))
            reasons = []
            if rms > group_threshold and rms > p75 * 1.5:
                reasons.append(f"组内高 ({rms:.3f}>{group_threshold:.3f}=P75+{args.iqr_mult}*IQR)")
            if rms > args.abs_threshold:
                reasons.append(f"绝对超阈 ({rms:.3f}>{args.abs_threshold:.3f}px)")
            if rms > 0.5 and s0 > 3.0:
                reasons.append(f"广角高残差 ({rms:.3f}px, s0={s0:.2f})")
            if reasons:
                review_items.append((r, k, "; ".join(reasons)))

    # 复制异常帧 PNG 到 review/
    review_dir = os.path.join(args.out_dir, "review")
    if os.path.isdir(review_dir):
        shutil.rmtree(review_dir)
    os.makedirs(review_dir, exist_ok=True)

    print(f"\n{'='*80}")
    print(f"待人工校验帧: {len(review_items)} / {len(results)}")
    print(f"{'='*80}")
    review_records = []
    for r, k, reason in sorted(review_items, key=lambda x: -float(x[0].get("rms_px", 999))):
        label = r.get("label", "?")
        rms = float(r.get("rms_px", -1))
        rms_arc = float(r.get("rms_arcsec", -1))
        png_src = os.path.join(args.out_dir, f"{label}_reproject.png")
        png_dst = os.path.join(review_dir, f"{label}_reproject.png")
        if os.path.isfile(png_src):
            shutil.copy2(png_src, png_dst)
        print(f"  [{k[0]}mm/{k[1]}um] {label:<70} RMS={rms:.3f}px ({rms_arc:.3f}\")  {reason}")
        review_records.append({
            "label": label,
            "focallen": k[0], "xpixsz": k[1],
            "rms_px": rms, "rms_arcsec": rms_arc,
            "reason": reason,
            "png": png_dst,
        })

    # 输出 JSON + MD 报告
    summary_path = os.path.join(args.out_dir, "optical_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump({
            "n_total": len(results),
            "n_groups": len(groups),
            "optical_stats": optical_stats,
            "n_review": len(review_items),
            "review_records": review_records,
        }, f, ensure_ascii=False, indent=2)
    print(f"\n光学系统统计: {summary_path}")

    md_path = os.path.join(args.out_dir, "review_report.md")
    with open(md_path, "w", encoding="utf-8") as f:
        f.write("# WCS RMS 像素误差按光学系统分组分析报告\n\n")
        f.write(f"总帧数: {len(results)}\n\n")
        f.write("## 各光学系统统计\n\n")
        f.write("| fl(mm) | ps(um) | s0(\"/px) | 帧数 | success | P50(px) | P75(px) | P90(px) | max(px) |\n")
        f.write("|--------|--------|----------|------|----------|---------|---------|---------|---------|\n")
        for s in optical_stats:
            if s.get("n_success", 0) == 0:
                f.write(f"| {s['focallen']} | {s['xpixsz']} | - | {s['n_total']} | 0/{s['n_total']} | - | - | - | - |\n")
            else:
                f.write(f"| {s['focallen']} | {s['xpixsz']} | {s.get('s0_arcsec_per_px',0):.4f} | "
                        f"{s['n_total']} | {s['n_success']}/{s['n_total']} | "
                        f"{s['rms_px_p50']:.4f} | {s['rms_px_p75']:.4f} | "
                        f"{s['rms_px_p90']:.4f} | {s['rms_px_max']:.4f} |\n")
        f.write(f"\n## 待人工校验帧 ({len(review_items)})\n\n")
        if not review_records:
            f.write("无异常帧。\n")
        else:
            f.write("| label | fl(mm) | ps(um) | RMS(px) | RMS(\") | 原因 |\n")
            f.write("|-------|--------|--------|---------|--------|------|\n")
            for rec in review_records:
                f.write(f"| {rec['label']} | {rec['focallen']} | {rec['xpixsz']} | "
                        f"{rec['rms_px']:.4f} | {rec['rms_arcsec']:.4f} | {rec['reason']} |\n")
        f.write(f"\n异常帧 PNG 位于: review/\n")
    print(f"报告: {md_path}")
    print(f"异常帧 PNG: {review_dir}")


if __name__ == "__main__":
    main()
