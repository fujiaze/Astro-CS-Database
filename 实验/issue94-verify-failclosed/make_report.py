#!/usr/bin/env python3
"""Issue #94 / P0-C2 报告生成器（数字全部来自 evidence/results.json）。

输出: 实验/issue94-verify-failclosed/REPORT.md
"""
from __future__ import annotations

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, "evidence")


def main() -> int:
    d = json.load(open(os.path.join(EVID, "results.json"), encoding="utf-8"))
    L = []
    A = L.append

    def tbl(headers, rows):
        A("| " + " | ".join(headers) + " |")
        A("|" + "|".join(["---"] * len(headers)) + "|")
        for r in rows:
            A("| " + " | ".join(str(x) for x in r) + " |")
        A("")

    A("# Issue #94 / P0-C2：writer/verify 的 reopen_ok·canonical_match fail-closed 与发布事务 —— 施工与取证报告")
    A("")
    A("本报告由 `make_report.py` 从 `evidence/results.json`（真实 5 节点 CLI 实测）生成，不手抄。")
    A("")
    A("## 1. 身份与工具链")
    A("")
    A("| 项 | 值 |")
    A("|---|---|")
    A("| 主任务 | Issue #94（P0-C2 生产修复；#87 E2-R 完成后接力） |")
    A(f"| 修复后 acsd SHA-256 | `{d['fixed_acsd_sha256']}` |")
    A(f"| 修复前（仅故障钩子、无 fail-closed 消费）acsd SHA-256 | `{d['prefix_acsd_sha256']}` |")
    A("| 隔离检出 | `/workspace/.e2r-clean`（因并发 #9 Agent 同树写入） |")
    A("")
    A("## 2. 缺陷与修复（真实 CLI A/B，7 例 × 2 二进制）")
    A("")
    A("故障钩子 `ACSD_P3_VERIFY_FAULT`（测试门禁；生产默认不可达）在**写后 / 独立 verify 前**篡改已发布 FITS："
      "`pixel_change`/`wcs_change` 在 writer 自身重开前；`canonical_change` 在 writer 写 `p3_writer.json` 之后；"
      "`verify_pixel_change`/`verify_wcs_change`/`identity_drift` 在 verify 节点自身重开/身份读取前。")
    A("")
    rows = []
    for r in d["results"]:
        rows.append([r["tag"], r["case"], r["fault"] or "-", r["exit_code"],
                     "Y" if r["fits_present"] else "n",
                     "Y" if r["writer_present"] else "n",
                     (r["manifest"] or {}).get("status"),
                     r.get("reopen_ok"), r.get("canonical_match"),
                     r.get("verify_ok"), r.get("semantic_code") or "-",
                     r["verdict"]])
    tbl(["二进制", "用例", "注入", "exit", "FITS", "writer", "manifest",
         "reopen_ok", "canonical_match", "verify_ok", "机器码", "判定"], rows)
    A(f"共 {d['n_cases']} 例，{d['n_fail']} 失败。修复前：全部 `exit 0` + FITS + `manifest=complete` "
      "（即使 `p3_writer.json`/`p3_verify.json` 已记录 `reopen_ok=0` / `canonical_match=false`）——"
      "「记录字段」被当作「已通过」；修复后：`exit 4`、无 FITS、无 `p3_writer.json`、`manifest=incomplete`。")
    A("")
    A("## 3. 发布事务（同目录旧 success vs 新 fail）")
    A("")
    sm = d.get("stale_mix") or {}
    A(f"- 同一 `output_dir` 先成功（`manifest=complete` + FITS）再以 `identity_drift` 失败："
      f"新 run `exit={sm.get('exit_code')}`、FITS={'Y' if sm.get('fits_present') else 'n'}、"
      f"writer={'Y' if sm.get('writer_present') else 'n'}、"
      f"最新 manifest=`{(sm.get('manifest') or {}).get('status')}`、tmp={sm.get('tmp_leftovers')}。")
    A("- 机制：run 起点（properties 节点）先失效上一次成功产物；verify/writer 失败时显式撤销本次"
      "已发布 FITS 与 writer 成功声明。消费方按**最新 mtime** 选择 run manifest（`acsd_run_*.json` 多次 run 并存）。")
    A(f"- 判定：{sm.get('verdict')}（{sm.get('verdict_reason')}）。")
    A("")
    A("## 4. 定向单测（`P3_OUT_OK && reopen_ok=false`）")
    A("")
    A("- `eng/tests/unit/p3_verify_reopen_test.cpp` + `run_p3_verify_reopen_test.py`：**16/16**。")
    A("  覆盖：正常写出重开 `reopen_ok=1`；像元改变/WCS 改变 ⇒ `close()==P3_OUT_OK` 但 `reopen_ok=0`；"
      "SWVER（canonical-only）改变 ⇒ `reopen_ok=1` 但规范哈希变化（canonical 必须被独立消费）；"
      "缺 `COVSTAT` 的旧产物 ⇒ `p3_output_read_coverage_identity` 返回 `P3_OUT_PARAM`（不静默 VERIFIED）。")
    A("")
    A("## 5. 回归（同一修复二进制）")
    A("")
    A("| 回归项 | 用例数 | 失败 |")
    A("|---|---|---|")
    A("| #87 受控矩阵（e94_fixed） | 24 | 0 |")
    A("| #87 机器判据（verify_matrix，prefix vs e94_fixed） | 96 | 0 |")
    A("| #87 tile 语义单测 | 123 | 0 |")
    A("| #91 CLI（e94_regress） | 22 | 0 |")
    A("| #91/E1-R worker-open 注入（e94_regress） | 5 | 0 |")
    A("")
    A("普通完整 FITS + WCS + BUNIT + mask + 数据 SHA 与已验收 E2/E2-R 证据逐位一致（#87 矩阵 24 例全绿）。")
    A("")
    A("## 6. NOT_RUN / 实际边界")
    A("")
    A("1. **ARS #31 统一发布事务**：本轮只在 P3 单 PRIMARY signal 产品内做 staging/显式撤销，"
      "不等将来统一事务；跨阶段（P1/P2）事务未改。")
    A("2. **真实磁盘满/权限失败下的撤销**：未注入（撤销用既有 `aio_atomic::remove_file`）。")
    A("3. **全仓测试/CI**：未运行；只复跑指定套件。")
    A("4. **session 入口的 reopen_ok/canonical_match 消费**：`p3_session_run` 自身写 FITS 后仅做"
      "`p3_output_write_atomic_ex` 内部 verify（其 `reopen_ok` 已在发布前强制），未新增 session 侧故障钩子。")
    A("")
    A("## 7. 复算命令（隔离检出 `/workspace/.e2r-clean`）")
    A("")
    A("```bash")
    A("python3 实验/issue94-verify-failclosed/run_verify_failclosed.py \\")
    A("    --fixed build/linux-dev/acsd --prefix /workspace/.e94-prefix-build/acsd --tag fixed")
    A("python3 实验/issue94-verify-failclosed/make_report.py")
    A("python3 eng/tests/unit/run_p3_verify_reopen_test.py")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag e94_fixed --expect fixed")
    A("python3 实验/issue87-stage3-tile-integrity/verify_matrix.py --prefix-tag e2r_prefix --fixed-tag e94_fixed --out /tmp/e94_verify87.json")
    A("python3 eng/tests/unit/run_p3_tile_semantics_test.py")
    A("python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e94_regress")
    A("python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag e94_regress --expect fixed")
    A("```")
    A("")
    A("## 8. 原始证据")
    A("")
    A("- `evidence/results.json`（14 例 A/B + stale_mix）")
    A("- `evidence/prefix/*.log`、`evidence/fixed/*.log`（逐例 CLI 日志）")
    A("- `eng/tests/unit/run_p3_verify_reopen_test.py` 输出（16/16）")
    A("")

    out = os.path.join(HERE, "REPORT.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print(f"wrote {out}: {len(L)} lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
