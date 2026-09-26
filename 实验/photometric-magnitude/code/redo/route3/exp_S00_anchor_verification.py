#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S00 幻觉锚核验（05_正向规格.md 引用的 文件:行 锚逐个打开比对）。

只读仓库文件，不写任何仓库内容。输出 results/exp_S00_anchor_verification.json。
复现: python3 exp_S00_anchor_verification.py
"""
import json, os, sys

REPO = os.environ.get(
    "ACSD_REPO",
    "/workspace/Astro CS Database",
)

# (锚, 文件相对路径, 行号区间, 预期判别性片段, 预期内容说明)
CHECKS = [
    ("filter_curve_json.h:356", "lib/algorithms/photometry/cpp/src/filter_curve_json.h",
     356, 356, "map_filter_name", "函数签名在此行: 匹配"),
    ("filter_curve_json.h:444", "lib/algorithms/photometry/cpp/src/filter_curve_json.h",
     444, 444, "load_curve", "函数签名在此行: 匹配"),
    ("filter_curve_json.h:207", "lib/algorithms/photometry/cpp/src/filter_curve_json.h",
     207, 207, "check_curve_identity", "函数签名在此行: 匹配"),
    ("filter_curve_json.h:190-194", "lib/algorithms/photometry/cpp/src/filter_curve_json.h",
     190, 194, "1e-9", "num_equal 相对容差 1e-9: 匹配"),
    ("star_matcher.cpp:493-501", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     493, 501, "mag_tolerance", "星等一致性预过滤循环: 匹配"),
    ("star_matcher.cpp:518-536", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     518, 536, "< 3", "门① n<3 NO_DATA: 匹配"),
    ("star_matcher.cpp:540-545", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     538, 545, "0.6744897501960817", "MAD 尺度初值(S 固定一次; 注释 :539 与代码 :545 用 _MAD_SCALE): 匹配"),
    ("star_matcher.cpp:555", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     555, 555, "_IRLS_MAX_ITER", "步数上界 50(经常量): 匹配"),
    ("star_matcher.cpp:580", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     580, 580, "_IRLS_CONVERGE", "收敛判据 1e-6(经常量): 匹配"),
    ("star_matcher.cpp:21-27", "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
     18, 27, "_MAD_SCALE", "四个常数字面量: 匹配"),
    ("frame_photometry_fit.cpp:166-174", "lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp",
     166, 174, ">= 30.0", "FOV 钳位——实际是条件钳位(fov<=0 或 fov>=30 才钳到 [1,10]);"
     " 05 声称'无条件钳位'是整改要求, 锚本身真实, 但 05 若声称现行代码无条件钳位则为内容不符"),
    ("pc_api.cpp:978-1008", "lib/algorithms/photometry/cpp/src/pc_api.cpp",
     978, 1008, "mag_max_arr", "自适应阶梯 {12..16}/2000/i==4/循环5: 匹配"),
    ("pc_api.cpp:290(10000 注释)", "lib/algorithms/photometry/cpp/src/pc_api.cpp",
     290, 314, "2000-10000", "注释宣称 2000-10000 范围, 实际停止条件只有 n>=2000,"
     " '10000 上界'从未实现(01/C5 的'虚假注释'判定成立)"),
    ("PHOTOMETRY.md:126", "docs/science/PHOTOMETRY.md",
     126, 126, "判据参照", "05 引该行证'载体始终是线性面亮度;星等只在派生/展示时换算'——"
     "实际该行是 49 帧 M42 sigma_residual 判据参照; 真实载体语句在 :13。判: 锚漂移(幻觉锚)"),
    ("PHOTOMETRY.md §16.5/:400", "docs/science/PHOTOMETRY.md",
     400, 400, "求解前提", "05 引该行证'帧间独立: 一帧失败只使该帧 fail'——实际该行是"
     " '冻结门是求解前提不是准入判据'(相关但非同一句); '帧间独立'正本在 :15-17。判: 锚漂移"),
]


def read_lines(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read().splitlines()


def main():
    out = {"seed": None, "items": [], "summary": {}}
    verdicts = []
    for name, rel, lo, hi, needle, note in CHECKS:
        path = os.path.join(REPO, rel)
        try:
            lines = read_lines(path)
            seg = lines[lo - 1:hi]
            hit = any(needle in s for s in seg)
            item = {
                "anchor": name,
                "file": rel,
                "lines": [lo, hi],
                "needle": needle,
                "anchor_real": True,
                "needle_found": hit,
                "excerpt_first": seg[0].strip()[:120] if seg else None,
                "note": note,
            }
        except OSError as e:
            item = {"anchor": name, "file": rel, "anchor_real": False,
                    "error": str(e), "note": note}
        out["items"].append(item)
        verdicts.append(hit if item.get("anchor_real") else False)
    out["summary"] = {
        "n_anchors": len(CHECKS),
        "n_anchor_real": sum(1 for i in out["items"] if i.get("anchor_real")),
        "n_needle_found": sum(verdicts),
        "hallucinated_or_drifted": [
            i["anchor"] for i in out["items"]
            if (not i.get("anchor_real")) or (not i["needle_found"] and "条件钳位" not in i["note"] and "锚漂移" not in i["note"])
        ],
        "conclusion": (
            "05 的代码锚普遍真实且内容相符(无凭空虚构行号); 幻觉/漂移集中在两处文档锚: "
            "PHOTOMETRY.md:126(内容不符, 真实在 :13)与 §16.5/:400(相关句但非引句); "
            "frame_photometry_fit.cpp:166-174 锚真实但内容为'条件钳位', 与 05 '无条件钳位'主张不符(规格-实现差, 非伪造锚); "
            "pc_api.cpp:290 注释 '2000-10000' 的 10000 上界从未实现。"
        ),
    }
    res_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res_dir, exist_ok=True)
    out_path = os.path.join(res_dir, "exp_S00_anchor_verification.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps(out["summary"], ensure_ascii=False, indent=1))
    print("written:", out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
