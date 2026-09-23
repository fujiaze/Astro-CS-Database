#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P3-REJ-COUNT：Phase3 重采样产品的**强制剔除计数**承载面判据（DATA-P3-REJ-001）。

规范依据（逐字）
  - docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md §2a「invalid_handling」块
    （**唯一正本**）：`rejection_counting="mandatory"`、`count_field="n_rejected_nonfinite"`；
    规则 3「**强制计数（禁止静默剔除）**：每个输出像素**必须**同时暴露被剔除样本的
    计数 n_rejected_nonfinite … 计数为 0 与「字段缺失」**必须可区分**；缺失该计数的
    产品**不得**声称满足本规则」；
    规则 1/2 给出「样本级掩膜 + 重归一 + 覆盖级 NaN」；
    §2a「与两类输入的对应」表给出四行（无覆盖 / 无信息 / 部分坏 / 全合格）以及
    「下游可仅凭 (isnan(signal), support, n_rejected) 三元组把上表四行**完全分开**」。
  - docs/contracts/DATA_SEMANTICS.md §30.7（Phase3 承载面冻结；形态沿用 §30.2 的
    阶段二诊断平面先例）：诊断统计平面**不进** science planes 枚举，由 artifact
    manifest 声明描述；int32、0 即「无」、**禁 −1 哨兵**。
  - docs/contracts/DATA_SEMANTICS.md §29（DATA-P3-RES）：p3_resampled.json 字段面。
  - ENGINEERING_SPEC.md §8：每项检查必须有正例与负例、能红能绿、fail-closed。

判据（六条，逐像素 + 总量）
  G1 声明存在且字段名一致：p3_resampled.json#diagnostic_planes.n_rejected_nonfinite
     存在、count_field == "n_rejected_nonfinite"、per_pixel == true。
     **缺失 ⇒ 判红 COUNT_FIELD_MISSING**（这条就是「0 与字段缺失可区分」的判据：
     缺字段不是「全 0」，而是不满足 §2a 规则 3）。
  G2 载体存在且长度正确：p3_rejection.bin 大小 == height_px*width_px*4（int32）。
  G3 shape 一致：diagnostic_planes.*.shape == [height_px, width_px]。
  G4 逐像素四行可分（§2a 表；n_cand = 4(bilinear) / 1(nearest)）：
       coverage==0                     ⇒ count == 0
       coverage==1 且 signal 非有限    ⇒ count == n_cand（零合格样本）
       coverage==1 且 signal 有限      ⇒ 0 <= count <= n_cand-1（至少一个合格样本）
     任一行不符 ⇒ COUNT_MISMATCH（即「计数与真实剔除数不符」）。
  G5 总量一致：n_rejected_nonfinite_total == Σ count。
  G6 值域：0 <= count <= n_cand；出现负数 ⇒ NEGATIVE_SENTINEL（§30.2 禁 −1 哨兵）。

退出码：0 = 全部判据通过；1 = 判据命中（判红）；2 = 依赖不可用（fail-closed，
产品/schema 不可读 ⇒ 不给结论）。
用法：
  python3 eng/tools/quality/check_p3_rejection_count.py --product-dir <phase3 out dir>
  python3 eng/tools/quality/check_p3_rejection_count.py --self-test
"""
from __future__ import annotations

import argparse
import json
import math
import os
import struct
import sys
import tempfile

COUNT_FIELD = "n_rejected_nonfinite"
CARRIER = "p3_rejection.bin"
DIAG_KEY = "diagnostic_planes"
RESAMPLED_JSON = "p3_resampled.json"
N_CAND = {"bilinear": 4, "nearest": 1}


class Unavailable(Exception):
    """产品/schema 不可读 —— fail-closed，rc=2，不给结论。"""


def _read_resampled(product_dir):
    p = os.path.join(product_dir, RESAMPLED_JSON)
    if not os.path.isfile(p):
        raise Unavailable("缺 %s（%s）" % (RESAMPLED_JSON, product_dir))
    try:
        with open(p, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError) as exc:
        raise Unavailable("%s 不可解析: %s" % (RESAMPLED_JSON, exc))


def _read_int32_plane(path, n):
    if not os.path.isfile(path):
        raise Unavailable("缺载体 %s" % path)
    size = os.path.getsize(path)
    if size != n * 4:
        raise Unavailable("载体 %s 大小 %d != %d（int32 × %d）" % (path, size, n * 4, n))
    with open(path, "rb") as fh:
        raw = fh.read()
    return list(struct.unpack("<%di" % n, raw))


def _read_f32_plane(path, n, what):
    if not os.path.isfile(path):
        raise Unavailable("缺 %s 平面载体 %s" % (what, path))
    if os.path.getsize(path) != n * 4:
        raise Unavailable("%s 平面大小不符: %s" % (what, path))
    with open(path, "rb") as fh:
        raw = fh.read()
    return list(struct.unpack("<%df" % n, raw))


def read_product(product_dir):
    """读出 (res_json, width, height, sampler, signal[], coverage[], counts[])。

    signal/coverage 取自 p3_resampled.bin 的前两个 f32 平面
    （平面连续拼接: signal, coverage[, variance, ivar]；见 module_adapters.cpp
    p3_op_resample 的 typed artifact 注记与 DATA_SEMANTICS §29.2）。
    """
    res = _read_resampled(product_dir)
    w = int(res.get("width_px", 0))
    h = int(res.get("height_px", 0))
    if w <= 0 or h <= 0:
        raise Unavailable("width_px/height_px 非法: %r/%r" % (res.get("width_px"), res.get("height_px")))
    n = w * h
    sampler = str(res.get("sampler", "bilinear"))
    if sampler not in N_CAND:
        raise Unavailable("sampler 未知: %r（期望 %s）" % (sampler, sorted(N_CAND)))
    binp = os.path.join(product_dir, str(res.get("bin", "p3_resampled.bin")))
    sig = _read_f32_plane(binp, n, "signal")
    cov = _read_f32_plane(binp, n, "coverage")   # 注意: 同文件第二平面, 见下方校验
    counts = _read_int32_plane(os.path.join(product_dir, CARRIER), n)
    return res, w, h, sampler, sig, cov, counts


def _split_planes(bin_path, n, nplanes):
    if not os.path.isfile(bin_path):
        raise Unavailable("缺 %s" % bin_path)
    size = os.path.getsize(bin_path)
    if size < n * 4 * nplanes:
        raise Unavailable("p3_resampled.bin 过短: %d < %d" % (size, n * 4 * nplanes))
    with open(bin_path, "rb") as fh:
        raw = fh.read()
    return [list(struct.unpack_from("<%df" % n, raw, i * n * 4)) for i in range(nplanes)]


def load(product_dir):
    """完整装载（signal/coverage 逐平面切分；供判据与 self-test 共用）。"""
    res = _read_resampled(product_dir)
    w = int(res.get("width_px", 0))
    h = int(res.get("height_px", 0))
    if w <= 0 or h <= 0:
        raise Unavailable("width_px/height_px 非法")
    n = w * h
    sampler = str(res.get("sampler", "bilinear"))
    if sampler not in N_CAND:
        raise Unavailable("sampler 未知: %r" % sampler)
    binp = os.path.join(product_dir, str(res.get("bin", "p3_resampled.bin")))
    planes = _split_planes(binp, n, 2)
    counts = _read_int32_plane(os.path.join(product_dir, CARRIER), n)
    return res, w, h, sampler, planes[0], planes[1], counts


def check(product_dir):
    """返回 (violations, notes)。violations 非空 ⇒ 判红。"""
    v, notes = [], {}
    res, w, h, sampler, sig, cov, counts = load(product_dir)
    n = w * h
    ncand = N_CAND[sampler]
    notes.update(width_px=w, height_px=h, sampler=sampler, n_cand=ncand, pixels=n)

    # ── G1 声明存在 + 字段名一致（「0 与字段缺失可区分」的判据）──────────────
    diag = res.get(DIAG_KEY)
    decl = None
    if not isinstance(diag, dict) or COUNT_FIELD not in diag:
        v.append("COUNT_FIELD_MISSING: %s#%s.%s 缺失 —— 按 DATA-002 §2a 规则 3，"
                 "计数为 0 与「字段缺失」必须可区分；缺失该计数的产品不得声称满足"
                 "本规则（count_field=%s）" % (RESAMPLED_JSON, DIAG_KEY, COUNT_FIELD, COUNT_FIELD))
    else:
        decl = diag[COUNT_FIELD]
        if not isinstance(decl, dict):
            v.append("COUNT_FIELD_MISSING: %s#%s.%s 不是对象" % (RESAMPLED_JSON, DIAG_KEY, COUNT_FIELD))
            decl = None
        else:
            if decl.get("count_field") != COUNT_FIELD:
                v.append("COUNT_FIELD_MISMATCH: count_field=%r != %r"
                         % (decl.get("count_field"), COUNT_FIELD))
            if decl.get("per_pixel") is not True:
                v.append("COUNT_FIELD_MISMATCH: per_pixel 必须为 true（§2a 规则 3「每个输出像素」）")
            if decl.get("dtype") != "int32":
                v.append("COUNT_FIELD_MISMATCH: dtype=%r != 'int32'（§30.7/§30.2）"
                         % (decl.get("dtype"),))
            if decl.get("carrier") != CARRIER:
                v.append("COUNT_FIELD_MISMATCH: carrier=%r != %r" % (decl.get("carrier"), CARRIER))

    # ── G3 shape 一致 ────────────────────────────────────────────────────────
    if decl is not None:
        shp = decl.get("shape")
        if shp != [h, w]:
            v.append("SHAPE_MISMATCH: shape=%r != [height_px, width_px]=[%d, %d]" % (shp, h, w))

    # ── G6 值域（禁 −1 哨兵）─────────────────────────────────────────────────
    neg = sum(1 for c in counts if c < 0)
    if neg:
        v.append("NEGATIVE_SENTINEL: %d 个像素计数为负（§30.2「0 即无，禁 −1 哨兵」）" % neg)
    over = sum(1 for c in counts if c > ncand)
    if over:
        v.append("COUNT_RANGE: %d 个像素计数 > n_cand=%d" % (over, ncand))

    # ── G4 逐像素四行可分（§2a 表）──────────────────────────────────────────
    row = {"no_coverage": 0, "zero_eligible": 0, "partial_bad": 0, "all_ok": 0}
    bad_no_cov = bad_zero_el = bad_finite = 0
    first = None
    for i in range(n):
        c = counts[i]
        covered = cov[i] > 0.5
        finite = math.isfinite(sig[i])
        if not covered:
            row["no_coverage"] += 1
            if c != 0:
                bad_no_cov += 1
                if first is None:
                    first = (i, "无覆盖(coverage=0) 但 count=%d（应 0）" % c)
        elif not finite:
            row["zero_eligible"] += 1
            if c != ncand:
                bad_zero_el += 1
                if first is None:
                    first = (i, "覆盖级 NaN(coverage=1,signal 非有限) 但 count=%d（应 %d）" % (c, ncand))
        else:
            if c > ncand - 1:
                bad_finite += 1
                if first is None:
                    first = (i, "有效输出(coverage=1,signal 有限) 但 count=%d（应 <= %d，"
                                "至少一个合格样本）" % (c, ncand - 1))
            elif c > 0:
                row["partial_bad"] += 1
            else:
                row["all_ok"] += 1
    if bad_no_cov or bad_zero_el or bad_finite:
        v.append("COUNT_MISMATCH: 计数与真实剔除数不符 —— 无覆盖行 %d 处、零合格行 %d 处、"
                 "有效输出行 %d 处；首例 像素#%s: %s"
                 % (bad_no_cov, bad_zero_el, bad_finite,
                    first[0] if first else "?", first[1] if first else ""))
    notes["rows"] = row

    # ── G5 总量一致 ──────────────────────────────────────────────────────────
    total = res.get("n_rejected_nonfinite_total")
    if total is None:
        v.append("TOTAL_MISSING: %s#n_rejected_nonfinite_total 缺失" % RESAMPLED_JSON)
    elif int(total) != sum(counts):
        v.append("TOTAL_MISMATCH: n_rejected_nonfinite_total=%s != Σ count=%d"
                 % (total, sum(counts)))
    notes["total"] = sum(counts)

    # science planes 不得被诊断平面污染（§30.2 先例：诊断平面不进 science 枚举）
    planes = res.get("planes")
    if isinstance(planes, list) and COUNT_FIELD in planes:
        v.append("SCIENCE_PLANES_POLLUTED: %r 混入 science planes=%r（§30.2/§30.7："
                 "诊断平面不进 science planes 枚举）" % (COUNT_FIELD, planes))
    return v, notes


# ── self-test（正例 + 负例；能红能绿）────────────────────────────────────────
def _write_product(d, w, h, sampler, sig, cov, counts, *, decl=True,
                   total=True, carrier=CARRIER):
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "p3_resampled.bin"), "wb") as fh:
        fh.write(struct.pack("<%df" % len(sig), *sig))
        fh.write(struct.pack("<%df" % len(cov), *cov))
    with open(os.path.join(d, carrier), "wb") as fh:
        fh.write(struct.pack("<%di" % len(counts), *counts))
    res = {"schema": "DATA-P3-RES", "width_px": w, "height_px": h, "sampler": sampler,
           "bin": "p3_resampled.bin", "planes": ["signal", "coverage"]}
    if decl:
        res[DIAG_KEY] = {COUNT_FIELD: {"carrier": CARRIER, "dtype": "int32",
                                       "units": "count", "shape": [h, w],
                                       "row_major": True, "per_pixel": True,
                                       "invalid_policy": "none_zero_no_sentinel",
                                       "count_field": COUNT_FIELD}}
    if total:
        res["n_rejected_nonfinite_total"] = sum(counts)
    with open(os.path.join(d, RESAMPLED_JSON), "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=1)


def self_test():
    """正例 1（四行齐备且自洽）+ 负例 5（计数不符 / 缺字段 / 负哨兵 / 总量不符 / 载体截断）。

    负例面逐条对准 §2a 规则 3 与 §30.2 的禁止项；正例面同时覆盖四行分类
    （无覆盖 / 零合格 / 部分坏 / 全合格），证明判据非退化。
    """
    nan = float("nan")
    cases, problems = [], []
    with tempfile.TemporaryDirectory(prefix="p3rej_") as td:
        # 4 像素（bilinear, n_cand=4）: 无覆盖 / 零合格 / 部分坏 / 全合格
        sig = [nan, nan, 1.5, 2.5]
        cov = [0.0, 1.0, 1.0, 1.0]
        cnt = [0, 4, 2, 0]
        d = os.path.join(td, "green")
        _write_product(d, 2, 2, "bilinear", sig, cov, cnt)
        v, notes = check(d)
        ok_green = not v and notes["rows"] == {"no_coverage": 1, "zero_eligible": 1,
                                               "partial_bad": 1, "all_ok": 1}
        cases.append(("正例(四行齐备且自洽)", ok_green, v))
        if not ok_green:
            problems.append("正例未判绿: %r / rows=%r" % (v, notes.get("rows")))

        # 负例 1：计数与真实剔除数不符（覆盖级 NaN 却记 0）
        d1 = os.path.join(td, "n1")
        _write_product(d1, 2, 2, "bilinear", sig, cov, [0, 0, 2, 0])
        v1, _ = check(d1)
        n1 = any("COUNT_MISMATCH" in x for x in v1)
        cases.append(("负例1(计数与真实剔除数不符)", n1, v1))

        # 负例 2：字段缺失（= §2a 规则 3「0 与字段缺失必须可区分」）
        d2 = os.path.join(td, "n2")
        _write_product(d2, 2, 2, "bilinear", sig, cov, cnt, decl=False)
        v2, _ = check(d2)
        n2 = any("COUNT_FIELD_MISSING" in x for x in v2)
        cases.append(("负例2(计数声明字段缺失)", n2, v2))

        # 负例 3：−1 哨兵
        d3 = os.path.join(td, "n3")
        _write_product(d3, 2, 2, "bilinear", sig, cov, [-1, 4, 2, 0])
        v3, _ = check(d3)
        n3 = any("NEGATIVE_SENTINEL" in x for x in v3)
        cases.append(("负例3(负哨兵 −1)", n3, v3))

        # 负例 4：总量与逐像素之和不符
        d4 = os.path.join(td, "n4")
        _write_product(d4, 2, 2, "bilinear", sig, cov, cnt)
        p4 = os.path.join(d4, RESAMPLED_JSON)
        j4 = json.load(open(p4, encoding="utf-8"))
        j4["n_rejected_nonfinite_total"] = 99
        json.dump(j4, open(p4, "w", encoding="utf-8"), ensure_ascii=False)
        v4, _ = check(d4)
        n4 = any("TOTAL_MISMATCH" in x for x in v4)
        cases.append(("负例4(总量与逐像素不符)", n4, v4))

        # 负例 5：载体截断（fail-closed 依赖不可用 ⇒ rc=2）
        d5 = os.path.join(td, "n5")
        _write_product(d5, 2, 2, "bilinear", sig, cov, cnt)
        with open(os.path.join(d5, CARRIER), "wb") as fh:
            fh.write(struct.pack("<i", 0))
        n5 = False
        try:
            check(d5)
        except Unavailable:
            n5 = True
        cases.append(("负例5(载体截断 fail-closed)", n5, ["Unavailable"]))

        # 负例 6：science planes 被诊断平面污染
        d6 = os.path.join(td, "n6")
        _write_product(d6, 2, 2, "bilinear", sig, cov, cnt)
        p6 = os.path.join(d6, RESAMPLED_JSON)
        j6 = json.load(open(p6, encoding="utf-8"))
        j6["planes"] = ["signal", "coverage", COUNT_FIELD]
        json.dump(j6, open(p6, "w", encoding="utf-8"), ensure_ascii=False)
        v6, _ = check(d6)
        n6 = any("SCIENCE_PLANES_POLLUTED" in x for x in v6)
        cases.append(("负例6(science planes 被污染)", n6, v6))

        # 负例 7：nearest 口径（n_cand=1）—— 覆盖级 NaN 必须记 1 而非 4
        d7 = os.path.join(td, "n7")
        _write_product(d7, 1, 2, "nearest", [nan, 3.0], [1.0, 1.0], [4, 0])
        v7, _ = check(d7)
        n7 = any("COUNT_MISMATCH" in x for x in v7)
        cases.append(("负例7(nearest 口径 n_cand=1)", n7, v7))

    for name, ok, detail in cases:
        print("  %-40s [%s]" % (name, "PASS" if ok else "FAIL"))
        if not ok:
            problems.append("%s: %r" % (name, detail[:2]))
    print("P3-REJ-COUNT_SELF-TEST_%s (positives=1 negatives=%d)"
          % ("PASS" if not problems else "FAIL", len(cases) - 1))
    for p in problems:
        print("  - %s" % p)
    return 0 if not problems else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="Phase3 强制剔除计数承载面判据 (DATA-P3-REJ-001)")
    ap.add_argument("--product-dir", default=None, help="Phase3 输出目录（含 p3_resampled.json）")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.product_dir:
        print("FAIL-CLOSED: 未给 --product-dir 也无 --self-test —— 不给结论", file=sys.stderr)
        return 2
    try:
        v, notes = check(args.product_dir)
    except Unavailable as exc:
        print("P3-REJ-COUNT_UNVERIFIABLE: %s" % exc, file=sys.stderr)
        return 2
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump({"violations": v, "notes": notes}, fh, ensure_ascii=False, indent=1)
    if v:
        print("P3-REJ-COUNT_FAIL: %d 项违规" % len(v))
        for x in v:
            print("  VIOLATION " + x)
        return 1
    print("P3-REJ-COUNT_PASS: 声明/载体/shape/四行可分/总量/值域 全部自洽"
          "（%d 像素, sampler=%s, Σcount=%d）"
          % (notes["pixels"], notes["sampler"], notes["total"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
