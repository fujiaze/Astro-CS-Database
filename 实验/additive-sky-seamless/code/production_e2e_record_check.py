# -*- coding: utf-8 -*-
"""生产 49 帧端到端接缝读数 / 天光面几何：冻结记录的自检器（能红能绿）。

被测对象 = results/production_e2e_seam_record.json（源 run 树的逐字冻结拷贝）。
本脚本**不重跑生产**，只判两件事：
  ① 记录的**内部断言**是否支持论文的陈述（门覆盖 114/196、三个诊断量越门、
     同一产品两次运行节点数不同 ⇒ 节点密度是运行期旋钮）；
  ② 源文件仍在时，记录与源文件**逐字一致**（哈希 + 关键读数）。
源文件被轮次回收时，第 ② 项如实报 SOURCE-ABSENT，不冒充已核对。

用法：
  python3 code/production_e2e_record_check.py                       # 正例：默认记录
  python3 code/production_e2e_record_check.py --record <路径>       # 负例：换一份被改过的记录
  python3 code/production_e2e_record_check.py --no-source           # 只判内部断言
退出码：0 = 全部断言通过；1 = 有断言失败（红）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

UNIT = Path(__file__).resolve().parents[1]
ROOT = UNIT.parents[1]
DEFAULT_RECORD = UNIT / "results" / "production_e2e_seam_record.json"
GATE = 1e-2


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--record", default=str(DEFAULT_RECORD))
    ap.add_argument("--no-source", action="store_true",
                    help="跳过源文件核对（源树已被回收时）")
    a = ap.parse_args()

    checks: list[tuple[str, bool, str]] = []

    def ck(name: str, ok: bool, detail) -> None:
        checks.append((name, bool(ok), str(detail)))

    rec_path = Path(a.record)
    if not rec_path.exists():
        print("FAIL: record not found:", rec_path)
        return 1
    rec = json.loads(rec_path.read_text(encoding="utf-8"))

    # ── ① 内部断言（对应论文 §3/§4/§5 的陈述）──────────────────────────
    sr = rec.get("seam_readings", {})
    ck("schema 正确", rec.get("schema") == "SCI-C/PRODUCTION-E2E-RECORD/1", rec.get("schema"))
    ck("门判决 = PASS", sr.get("verdict") == "PASS", sr.get("verdict"))
    ck("门内读数 = 7.2561e-03（<= 门）",
       abs(sr.get("max_abs_rel", 1.0) - 0.007256101148201211) < 1e-15
       and sr.get("max_abs_rel", 1.0) <= GATE, sr.get("max_abs_rel"))
    ck("门覆盖 114/196 条边界（未计入 82 条 not_interior）",
       (sr.get("n_edges_total"), sr.get("n_edges_valid"),
        sr.get("n_edges_excluded_not_interior")) == (196, 114, 82),
       (sr.get("n_edges_total"), sr.get("n_edges_valid"), sr.get("n_edges_excluded_not_interior")))
    d_net, d_d4x, d_leg = (sr.get("rel_step_net_max"), sr.get("rel_step_d4x_max"),
                           sr.get("legacy_rel_max"))
    for nm, v in (("rel_step_net_max", d_net), ("rel_step_d4x_max", d_d4x),
                  ("legacy_rel_max", d_leg)):
        ck("%s 存在且 **越门**（> %g）" % (nm, GATE), isinstance(v, float) and v > GATE, v)
    ck("三个诊断量与门内读数不同源（均大于门内最大读数）",
       isinstance(d_net, float) and d_net > sr.get("max_abs_rel", 0.0), (d_net, sr.get("max_abs_rel")))
    geo = rec.get("sky_plane_geometry", {})
    runs = geo.get("runs", [])
    ck("同一产品两次运行节点数不同（运行期旋钮）",
       len(runs) == 2 and runs[0].get("n_nodes") != runs[1].get("n_nodes"),
       [r.get("n_nodes") for r in runs])
    # ── ①b 运行档 adopted 必须与几何 trace 源日志逐字一致 ───────────────
    # P5 订正点：原记录把 rc=7 的细化档标成 adopted=true，与同一条 source 日志行
    # 「[trace 1] ... adopted=0 rc=7」以及它自己的 derived_note 自相矛盾。
    import re as _re
    _traces = {}
    for s in rec.get("sources", []):
        # 源标签是中文（"生产几何导出与自适应 trace…"）⇒ 按**结构**选：带 lines 数组的源
        if "lines" not in s and "trace" not in s.get("kind", ""):
            continue
        gp = ROOT / s["path"]
        if not gp.exists():
            continue
        for ln in gp.read_text(encoding="utf-8", errors="replace").splitlines():
            m = _re.search(r"\[trace (\d+)\]\s+h=(\S+)\s+deg.*?adopted=(\d+)\s+rc=(\d+)", ln)
            if m:
                _traces[int(m.group(1))] = (float(m.group(2)), m.group(3) == "1", int(m.group(4)))
    for _i, _run in enumerate(runs):
        if _i not in _traces:
            continue
        _h, _ad, _rc = _traces[_i]
        ck("运行档 %d 的 adopted 与 source trace 日志逐字一致（trace: h=%.9f adopted=%d rc=%d）"
           % (_i, _h, _ad, _rc),
           abs(_run.get("h_deg", -1.0) - _h) < 1e-12 and bool(_run.get("adopted")) == _ad,
           (_run.get("h_deg"), _run.get("adopted"), _h, _ad, _rc))
    ck("几何导出纪律：h_hi = 0.5×overlap_band（规则 1）",
       abs(geo.get("overlap_band_deg", 0.0) / 2.0 - runs[0].get("h_deg", 0.0)) < 1e-9
       if runs else False,
       (geo.get("overlap_band_deg"), runs[0].get("h_deg") if runs else None))
    # 实验网格像素尺度 = 1.0″/px（code/c3_public_plane.py:27 的 SCALE_DEG = 1.0/3600，
    # sci_c_common.pix_to_sky 用 TAN WCS + cdelt=[-SCALE_DEG, SCALE_DEG]）。原标签写
    # 「0.989″/px」无仓内来源（HST M16 模板本身是 0.04000038″/px，但模板只作纯信号
    # 数组、不重采样 ⇒ 与本换算无关）⇒ 已按 P5-04 订正为 1.0″/px，并把该断言从
    # 「是浮点数」加强为「与实验网格尺度可分辨」。
    _exp_scale = 1.0
    _ps = geo.get("pixel_scale_arcsec_derived")
    ck("生产像素尺度与实验网格（1.0″/px）不同源 ⇒ 换算必须显式",
       isinstance(_ps, float) and 0.5 < _ps < 2.0 and abs(_ps - _exp_scale) > 1e-3,
       (_ps, "exp_grid=%.4f″/px" % _exp_scale))

    # ── ② 源文件核对 ─────────────────────────────────────────────────
    src_status = []
    for s in rec.get("sources", []):
        p = ROOT / s["path"]
        if a.no_source or not p.exists():
            src_status.append((s["path"], "SOURCE-ABSENT"))
            continue
        got = sha256(p)
        ok = (got == s.get("sha256"))
        src_status.append((s["path"], "MATCH" if ok else "MISMATCH"))
        ck("源文件逐字一致: " + s["path"], ok, (got[:12], str(s.get("sha256"))[:12]))

    print(json.dumps({"record": str(rec_path),
                      "n_checks": len(checks),
                      "n_failed": sum(1 for _, ok, _ in checks if not ok),
                      "sources": src_status}, ensure_ascii=False))
    for name, ok, detail in checks:
        print(("  [OK]   " if ok else "  [FAIL] ") + name + ("  " + detail if not ok else ""))
    failed = [n for n, ok, _ in checks if not ok]
    if failed:
        print("VERDICT=FAIL (" + "; ".join(failed) + ")")
        return 1
    print("VERDICT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
