#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-002 资源监控 CSV 机器检查器（tools/monitoring 域，owner SA-LOG-08）。

用途：对 `runtime/monitoring/monitor.py` 产出的原始 CSV 做完整性/合同校验。
exit 0 = PASS；任何违例 => 非 0 且输出 machine JSON verdict=FAIL。

检查项（对应 LOG-002 验收"原始 CSV 不可手工合成/篡改"）：
  1. header 精确等于 monitor.HEADER（合同列）；
  2. seed 行（seq=0）指纹 == sha256(salt|seed|sorted(HEADER)|run_id)；
  3. 链式指纹：每一行 row_fingerprint == _fingerprint(prev_fp, 行字符串, seq)；
     改任意字节 / 手工追加行 → 该行及其后全部失配；
  4. seq 严格 1..N 无空洞/重复；run_id 全行一致（可 --run-id 指定）；
  5. 时间戳单调（--no-ts-monotonic 关闭）；run_phase ∈ PHASES；
  6. 采样间隔统计（--stats 打印 min/mean/max）。

用法：
  python3 tools/monitoring/verify_monitor_csv.py --csv <file> [--run-id R] [--stats]
零第三方依赖（仅 Python 标准库）。不做任何科学判定。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

try:
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
    from runtime.monitoring.monitor import (  # type: ignore
        HEADER, PHASES, load_rows, verify_csv)
except Exception:  # pragma: no cover - 独立运行异常兜底
    HEADER, PHASES = [], ()
    def verify_csv(*_a, **_k):  # type: ignore
        return {"ok": False, "errors": ["monitor 模块不可导入（仓库根不可达）"]}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--csv", required=True, type=pathlib.Path)
    ap.add_argument("--run-id", default=None, help="期望 run_id（可选）")
    ap.add_argument("--no-ts-monotonic", action="store_true",
                    help="关闭时间戳单调断言")
    ap.add_argument("--stats", action="store_true", help="打印采样间隔统计")
    args = ap.parse_args(argv)

    if not args.csv.is_file():
        print(json.dumps({"verdict": "FAIL", "errors": [f"文件不存在: {args.csv}"]},
                         ensure_ascii=False))
        return 1

    res = verify_csv(args.csv, run_id=args.run_id,
                     require_timestamp_monotonic=not args.no_ts_monotonic)
    out: dict = {"verdict": "PASS" if res["ok"] else "FAIL",
                 "csv": str(args.csv), "errors": res["errors"]}
    if args.stats and res["ok"]:
        try:
            rows = load_rows(args.csv)
            data = [float(r["interval_s"]) for r in rows
                    if isinstance(r.get("interval_s"), (int, float))]
            if data:
                # 去掉 seed 行（seq=0，run_phase 为空）后统计
                sample_rows = [r for r in rows
                               if str(r.get("seq", "")).strip() not in ("", "0")]
                out["stats"] = {
                    "rows": len(sample_rows),
                    "interval_min_s": round(min(data), 4),
                    "interval_mean_s": round(sum(data) / len(data), 4),
                    "interval_max_s": round(max(data), 4),
                    "phases": sorted({r.get("run_phase")
                                      for r in sample_rows}),
                }
        except Exception as exc:  # pragma: no cover
            out["stats_error"] = str(exc)
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0 if res["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
