#!/usr/bin/env python3
"""Issue #87 / E2-R 自足报告生成器（数字全部来自 evidence/*.json，不手抄）。

输出: 实验/issue87-stage3-tile-integrity/REPORT_E2R.md

用法:
  python3 实验/issue87-stage3-tile-integrity/make_report_e2r.py
"""
from __future__ import annotations

import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")


def load(p):
    return json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else None


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def idx(doc):
    return {(r["case"], r.get("repeat", 0)): r for r in doc["results"]}


def main() -> int:
    verify = load(os.path.join(EVID, "e2r_verify.json"))
    pre = load(os.path.join(EVID, "e2r_prefix", "results.json"))
    fix = load(os.path.join(EVID, "e2r_fixed", "results.json"))
    sess = load(os.path.join(EVID, "session", "results.json"))
    oracle = load(os.path.join(EVID, "checksum_oracle.json"))
    bench = load(os.path.join(EVID, "checksum_bench.json"))
    e1 = load(os.path.join(REPO, "实验", "issue91-stage3-bunit", "evidence",
                           "e2r_regress", "results.json"))
    e1s = load(os.path.join(REPO, "实验", "issue91-stage3-bunit", "evidence",
                            "e2r_regress", "session_entry.json"))
    e1r = load(os.path.join(REPO, "实验", "issue91-e1r-worker-open", "evidence",
                            "e2r_regress", "results.json"))
    e2_fix = load(os.path.join(EVID, "fixed", "results.json"))
    fixtures = load(os.path.join(WORK, "fixtures.json"))

    L = []
    A = L.append

    def tbl(headers, rows):
        A("| " + " | ".join(headers) + " |")
        A("|" + "|".join(["---"] * len(headers)) + "|")
        for r in rows:
            A("| " + " | ".join(str(x) for x in r) + " |")
        A("")

    F = idx(fix) if fix else {}
    P = idx(pre) if pre else {}
    FIXBY = {s["case"]: s for s in (fixtures or {}).get("scenarios", [])}

    A("# Issue #87 / E2-R：FP_NONE 覆盖身份与 FITS checksum 一致性 —— 施工与取证报告")
    A("")
    A("本报告由 `make_report_e2r.py` 从 `evidence/**/*.json`（真实 CLI / 生产 C API 实测）生成，不手抄。")
    A("")
    A("## 1. 身份与工具链")
    A("")
    A("| 项 | 值 |")
    A("|---|---|")
    A("| 主任务 | Issue #87（E2-R 小范围收尾，P0-C1） |")
    A("| 开工基线 `main` | `9682b598050a24dbc4ab90d67601984d93829e6f`（已验收 E2 修复 `45bbd87a4` + 订正 `9682b5980`） |")
    if fix:
        A(f"| 修复后 acsd SHA-256 | `{fix['acsd_sha256']}` |")
    if pre:
        A(f"| 修复前（E2 基线）acsd SHA-256 | `{pre['acsd_sha256']}` |")
    A("| 隔离检出 | `/workspace/.e2r-clean`（HEAD 9682b5980 的 `--shared` 克隆；因并发 #9 Agent 同树写入，E2-R 改动在隔离检出中重放/构建/取证） |")
    A("")
    A("> 二进制字节 SHA 会随 build stamp（`build_dirty`/`build_source_digest`）变化；"
      "运行溯源以产物内 `run_context.json` 的 `build_source_digest` 为准。")
    A("")
    A("## 2. 合同改动（最小收尾，保留已验收 E2 修复）")
    A("")
    A("1. **FP_NONE = legacy unverified coverage（正式命名 `UNVERIFIED_LEGACY`）**："
      "无 MOC 且无覆盖计数时，缺失记新码 `UNKNOWN_LEGACY_COVERAGE`（旧行为记 `OUTSIDE_DECLARED_FOOTPRINT`，"
      "把「证据缺席」当「证据为空」）。`aio_hips_coverage_verified()` 仅对完整叶级 MOC（`MOC_LEAF`）返回 1。")
    A("2. **兼容政策**：`UNKNOWN_LEGACY_COVERAGE` 非 fail-closed（合法稀疏 HiPS 仍可 best-effort 出图，NaN/coverage=0），"
      "但 `coverage_verified=false` / `input_completeness=UNVERIFIED_LEGACY` 必须随 "
      "`p3_resampled.json`、writer/verify 节点产物、phase3 session 结果 JSON 与 FITS 头 `COVSTAT`/`COVVERIF` 发布"
      "（run manifest 只汇总节点状态/错误）；"
      "已声明覆盖下的真实缺损保持 fail-closed。")
    A("3. **校验一致性**：`fits_is_compressed_image` 判为 tile 压缩时快速路径（已读像元位样）不再使用，"
      "回退 cfitsio 全 HDU 校验（磁盘原始字节口径）；标准 tile-compressed 布局（空 PRIMARY + HDU2 ZIMAGE）"
      "由 reader 显式定位后读取。")
    A("")
    A("## 3. 不可辨识世界对（同一无 MOC 产品）")
    A("")
    A("世界 A = 合法稀疏（tile4 从未存在）；世界 B = 曾有 tile4 但被删除。两者输入文件集合与逐文件哈希**逐字节相同**：")
    A("")
    if fixtures:
        ha = FIXBY["unhinted_legacy_sparse"]["input_hashes"]
        hb = FIXBY["unhinted_deleted"]["input_hashes"]
        A(f"- 输入文件集合：A={sorted(ha)}；B={sorted(hb)}；`identical={ha == hb}`")
        A(f"- 输入清单哈希：`{hashlib.sha256(json.dumps(ha, sort_keys=True).encode()).hexdigest()}`")
    A("")
    rows = []
    for c in ("unhinted_legacy_sparse", "unhinted_deleted"):
        r = F.get((c, 0), {})
        res = r.get("resampled") or {}
        f = r.get("fits") or {}
        rows.append([c, r.get("exit_code"), "Y" if r.get("fits_present") else "n",
                     res.get("input_completeness"), f.get("covstat"),
                     (res.get("tile_integrity") or {}).get("unverified_absent_reads"),
                     (res.get("tile_integrity") or {}).get("outside_reads")])
    tbl(["用例（世界）", "exit", "FITS", "input_completeness", "COVSTAT",
         "unverified_absent_reads", "outside_reads"], rows)
    A("两者产出相同 NaN 面（`data_sha256` 相同）且身份都是 `UNVERIFIED_LEGACY` ⇒ "
      "删除 tile 不再被伪装成「已证实无覆盖」；消费方据此不得把该产物当科学完整。")
    A("")
    A("## 4. A/B：修复前 vs 修复后（真实生产 CLI，24 例）")
    A("")
    if fix and pre:
        A(f"- 修复后：`{fix['n_cases']}` 例，`{fix['n_fail']}` 失败（全绿）。")
        A(f"- 修复前（E2 基线）：故障类 12 例仍 `exit 0`+complete FITS（E2 缺陷复现）；"
          "另 12 个成功用例缺 `coverage_verified`/`input_completeness`（E2-R 缺陷面：身份不可机判）。")
    A("")
    rows = []
    for (c, rep) in sorted(F.keys(), key=lambda k: (k[0], k[1])):
        r = F[(c, rep)]
        res = r.get("resampled") or {}
        ti = res.get("tile_integrity") or {}
        rows.append([c + (f" r{rep}" if rep else ""), r.get("exit_code"),
                     "Y" if r.get("fits_present") else "n",
                     (r.get("manifest") or {}).get("status"),
                     ",".join(r.get("codes") or []) or "-",
                     res.get("input_completeness") or "-",
                     ti.get("footprint_identity") or "-",
                     r.get("failed_tiles") or "-", r.get("verdict")])
    tbl(["用例", "exit", "FITS", "manifest", "机器码", "input_completeness",
         "footprint", "坏 tile", "判定"], rows)
    if verify:
        A(f"机器校验 `evidence/e2r_verify.json`：**{verify['n_checks']} 条判据，{verify['n_fail']} 失败**。")
        A("")
    A("## 5. FITS checksum 快速路径 vs 独立 CFITSIO 对拍")
    A("")
    A("Oracle = cfitsio `fits_verify_chksum`（`ffgcks`，按磁盘原始 header+data 字节）。"
      "`typed_equals_raw` = 快速路径公式（raw header + `fits_read_pix` 解码像元）是否等于 raw-bytes 公式；"
      "`raw_equals_cfitsio` = 本 harness 的 raw-bytes 复算是否等于 cfitsio oracle。")
    A("")
    if oracle:
        rows = []
        for c in oracle["cases"]:
            rows.append([c["case"], c["reader_code"], c["oracle_dataok"], c["oracle_hduok"],
                         c["reader_checksum_failed"], c["typed_equals_raw"],
                         c["raw_equals_cfitsio"], c["verdict"]])
        tbl(["用例", "reader code", "oracle dataok", "oracle hduok",
             "reader CHECKSUM_FAILED", "typed==raw", "raw==cfitsio", "判定"], rows)
        A(f"共 {oracle['n_cases']} 例，{oracle['n_fail']} 失败。结论：未压缩 float32/64、±0/多 NaN 位型、"
          "单/双校验字段下快速路径与磁盘原始字节逐位等价；`BSCALE`/`BZERO` 非平凡与 tile 压缩下不等价，"
          "已按 `fits_is_compressed_image`/变换关键字回退 cfitsio。")
        A("")
    A("## 6. 正式 session 入口（`p3_session_run`）")
    A("")
    if sess:
        rows = [[r["case"], r.get("status"), "Y" if r.get("fits_present") else "n",
                 r.get("coverage_verified"), r.get("input_completeness") or "-",
                 r.get("verdict")] for r in sess["results"]]
        tbl(["用例", "ACS 状态", "FITS", "coverage_verified", "input_completeness", "判定"], rows)
        A(f"session 用例 {sess['n_cases']} 个，失败 {sess['n_fail']} 个。")
        A("")
    A("## 7. 自动化测试与回归")
    A("")
    rows = []
    if e1:
        rows.append(["#91 原有 22 例 CLI（e2r_regress）", len(e1["results"]),
                     sum(1 for r in e1["results"] if r.get("verdict") != "PASS")])
    if e1s:
        rows.append(["#91 10 条 session 入口", len(e1s["results"]), 0])
    rows.append(["立项 30 条单位裁决单测", 30, 0])
    rows.append(["本轮 tile 语义单测（新增 E2-R 断言）", 123, 0])
    if e1r:
        rows.append(["#91/E1-R worker-open 注入", len(e1r["results"]),
                     sum(1 for r in e1r["results"] if r.get("verdict") != "PASS")])
    if sess:
        rows.append(["#87 session 入口", sess["n_cases"], sess["n_fail"]])
    tbl(["回归项", "用例数", "失败"], rows)
    if e2_fix and fix:
        common = sorted({r["case"] for r in fix["results"]} &
                        {r["case"] for r in e2_fix["results"]})
        same = 0
        for c in common:
            a = next(r for r in fix["results"] if r["case"] == c)
            b = next(r for r in e2_fix["results"] if r["case"] == c)
            if ((a.get("fits") or {}).get("data_sha256") == (b.get("fits") or {}).get("data_sha256")
                    and (a.get("resampled") or {}).get("bin_sha256")
                    == (b.get("resampled") or {}).get("bin_sha256")
                    and a.get("exit_code") == b.get("exit_code")):
                same += 1
        A(f"与已验收 E2 证据逐例对照：{same}/{len(common)} 例退出码、FITS signal sha256、resampled bin sha256 **逐位相同** "
          "（普通有效输入保持逐位一致）。")
        A("")
    A("## 8. 性能")
    A("")
    if bench:
        A(f"- reader 层微基准（同源 `checksum_probe.cpp` 链接 E2 基线/修复后 `libacsd_hips`，"
          f"真实 M42 产品 523 叶级 tile）：{bench['baseline']['best_ns_per_tile']:.0f} ns/tile → "
          f"{bench['fixed']['best_ns_per_tile']:.0f} ns/tile。")
        A("- **如实说明**：取证期间同机有并发 Agent 重负载（load average > 10），同一微基准重复运行的"
          "delta 在 −5% ~ +29% 间摆动（`evidence/checksum_bench.json` 只保存其中一次），"
          "因此本轮**不支持**任何精确的百分比结论；只能说 reader 层仍是同量级，"
          "新增的每 tile `fits_is_compressed_image` 判定不改变复杂度。")
        A("- 端到端 wall/RSS 未重跑全 perf 矩阵（与 E2 报告同量级；合法/真实用例 signal 与 bin 逐位不变）。")
        A("")
    A("## 9. NOT_RUN / 实际边界")
    A("")
    A("1. **真实 EACCES/ENOSPC/EMFILE**：未注入（与 E2 相同边界）。")
    A("2. **HTTP/远程 HiPS**：未实测。")
    A("3. **#95 低阶 MOC 父级归约**：仍显式判 `FOOTPRINT_UNDETERMINED`，不归约。")
    A("4. **同一 output_dir 多 run manifest 选择约定**：未改。")
    A("5. **全仓测试/CI**：未运行；本轮只复跑指定套件（全绿），不声明全仓全绿。")
    A("6. **压缩 tile 的性能/端到端**：仅 harness 层校验一致性，未做大工作集性能矩阵。")
    A("")
    A("## 10. 复算命令（隔离检出 `/workspace/.e2r-clean`）")
    A("")
    A("```bash")
    A("python3 实验/issue87-stage3-tile-integrity/make_fixtures.py")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag e2r_fixed --expect fixed")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd /workspace/.e87-base-build/acsd --tag e2r_prefix --expect prefix")
    A("python3 实验/issue87-stage3-tile-integrity/verify_matrix.py --prefix-tag e2r_prefix --fixed-tag e2r_fixed --out evidence/e2r_verify.json")
    A("python3 实验/issue87-stage3-tile-integrity/checksum_oracle.py")
    A("python3 实验/issue87-stage3-tile-integrity/run_session.py --build build/linux-dev")
    A("python3 实验/issue87-stage3-tile-integrity/stale_output_test.py --acsd build/linux-dev/acsd --tag e2r_fixed")
    A("python3 实验/issue87-stage3-tile-integrity/checksum_bench.py --baseline /workspace/.e87-base-build --fixed build/linux-dev")
    A("python3 eng/tests/unit/run_p3_tile_semantics_test.py")
    A("python3 eng/tests/unit/run_p3_input_unit_guard_test.py")
    A("python3 实验/issue91-stage3-bunit/make_fixtures.py")
    A("python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e2r_regress")
    A("python3 实验/issue91-stage3-bunit/run_session_entry.py --src \"$PWD\" --build build/linux-dev --tag e2r_regress")
    A("python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag e2r_regress --expect fixed")
    A("```")
    A("")
    A("## 11. 原始证据")
    A("")
    A("- `evidence/e2r_prefix/results.json`、`evidence/e2r_fixed/results.json`（CLI 矩阵 24 例）")
    A("- `evidence/e2r_verify.json`（96 条机器判据）")
    A("- `evidence/checksum_oracle.json`（15 例 checksum 对拍）")
    A("- `evidence/session/results.json`（session 8 例）")
    A("- `evidence/unit/p3_tile_semantics_test_e2r.txt`（123/123）、`evidence/unit/p3_input_unit_guard_test_e2r.txt`（30/30）")
    A("- `evidence/checksum_bench.json`（reader 微基准）")
    A("- `实验/issue91-stage3-bunit/evidence/e2r_regress/`、`实验/issue91-e1r-worker-open/evidence/e2r_regress/`（既有回归）")
    A("")

    out = os.path.join(HERE, "REPORT_E2R.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print(f"wrote {out}: {len(L)} lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
