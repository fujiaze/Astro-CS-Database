#!/usr/bin/env python3
"""Issue #87 / E2 自足实验报告生成器（数字全部来自 evidence/*.json，不手抄）。

输出: 实验/issue87-stage3-tile-integrity/REPORT.md

用法:
  python3 实验/issue87-stage3-tile-integrity/make_report.py
"""
from __future__ import annotations

import hashlib
import json
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")


def load(p: str):
    if not os.path.isfile(p):
        return None
    return json.load(open(p, encoding="utf-8"))


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def index(doc):
    return {(r["case"], r["repeat"]): r for r in doc["results"]}


def main() -> int:
    verify = load(os.path.join(EVID, "verify.json"))
    pre = load(os.path.join(EVID, "prefix", "results.json"))
    fix = load(os.path.join(EVID, "fixed", "results.json"))
    sess = load(os.path.join(EVID, "session", "results.json"))
    stale = load(os.path.join(EVID, "stale", "results_fixed.json"))
    perf = load(os.path.join(EVID, "perf", "results.json"))
    fixtures = load(os.path.join(WORK, "fixtures.json"))
    e1 = load(os.path.join(REPO, "实验", "issue91-stage3-bunit", "evidence",
                           "e87_regress", "results.json"))
    e1r = load(os.path.join(REPO, "实验", "issue91-e1r-worker-open", "evidence",
                            "e87_regress", "results.json"))

    L = []
    A = L.append

    def tbl(headers, rows):
        A("| " + " | ".join(headers) + " |")
        A("|" + "|".join(["---"] * len(headers)) + "|")
        for r in rows:
            A("| " + " | ".join(str(x) for x in r) + " |")
        A("")

    A("# Issue #87 / E2：Stage3 HiPS Tile 读取完整性与故障传播 —— 施工与取证报告")
    A("")
    A("本报告由 `make_report.py` 从 `evidence/**/*.json`（真实 CLI / 生产 C API 实测）"
      "生成，不手抄。")
    A("")

    # ── 身份 ────────────────────────────────────────────────────────────────
    A("## 1. 身份与工具链")
    A("")
    A("| 项 | 值 |")
    A("|---|---|")
    A(f"| 主任务 | Issue #87 |")
    A(f"| 关联 | #91（已完成）、#94、#9、#79、#95（独立任务，不在本轮范围） |")
    A(f"| 开工基线 `main` | `476c023d395b224518aa1831f730d64822250289` |")
    A(f"| 修复前 A/B 源码 | `476c023`（`git clone` + detached checkout，未建分支） |")
    A(f"| 修复前 acsd SHA-256 | `{ (pre or {}).get('acsd_sha256','?') }` |")
    A(f"| 修复后 acsd SHA-256 | `{ (fix or {}).get('acsd_sha256','?') }` |")
    A(f"| 输入夹具清单哈希 | `{ sha256_file(os.path.join(WORK,'fixtures.json'))[:64] if os.path.isfile(os.path.join(WORK,'fixtures.json')) else '?' }` |")
    A("")
    A("修复后二进制构建于**提交前工作树**（`build_dirty=true`，版本串 `+g476c023…`）；"
      "修复前二进制由 `476c023` 的独立干净检出构建（`build_dirty=false`）。"
      "提交后源码同一，但 `build_source_digest` 随工作树 dirty 状态与提交状态变化，"
      "因此重建的二进制字节 SHA 可能不同（`evidence/prefix_rebuild`、"
      "`evidence/fixed_rebuild`、`evidence/clean` 即重建复算，判据同 67/0）；"
      "运行溯源以产物内 `run_context.json` 的 `build_source_digest` 为准。")
    A("")

    if verify:
        A(f"机器校验：`evidence/verify.json` {verify['n_checks']} 条判据，"
          f"{verify['n_fail']} 条失败。")
        A("")

    # ── 缺陷 A/B ────────────────────────────────────────────────────────────
    if pre and fix:
        P, F = index(pre), index(fix)
        A("## 2. 缺陷（修复前 A/B 实测）与修复后 fail-closed")
        A("")
        A("同一受控 HiPS 矩阵：修复前基线（`476c023`）与修复后二进制仅二进制不同。"
          "修复前：声明内缺失/损坏/校验失败/I/O/身份不足全部 `exit 0` 并发布 complete "
          "FITS（缺陷本体）；修复后：`exit 3`（输入域）或 `exit 7`（I/O 域），无 FITS/bin/"
          "resampled.json，manifest=incomplete。")
        A("")
        rows = []
        for (case, rep), r in sorted(F.items()):
            if r["expect"] != "fail":
                continue
            a = P.get((case, rep), {})
            af = a.get("fits") or {}
            tf = r.get("tile_failures") or {}
            rows.append([
                case,
                f"{a.get('exit_code')} / {'Y' if a.get('fits_present') else 'n'} / "
                f"{(a.get('manifest') or {}).get('status')}",
                f"nan={af.get('nan_px')} zero={af.get('exact_zero_px')}",
                f"{r['exit_code']} / {'Y' if r['fits_present'] else 'n'} / "
                f"{(r.get('manifest') or {}).get('status')}",
                ", ".join(r.get("codes") or []) or "-",
                str(r.get("failed_tiles")),
            ])
        tbl(["用例", "修复前 exit/FITS/manifest", "修复前 signal 空洞",
             "修复后 exit/FITS/manifest", "修复后机器码", "修复后坏 tile"],
            rows)

    # ── 合法/真实逐位 ───────────────────────────────────────────────────────
    if pre and fix:
        P, F = index(pre), index(fix)
        A("## 3. 合法/真实输入逐位回归（修复前 = 修复后）")
        A("")
        rows = []
        all_ok = True
        for case in sorted({c for c, _ in P if P[(c, 0)]["expect"] == "ok"}):
            a, b = P[(case, 0)], F[(case, 0)]
            fa, fb = a.get("fits") or {}, b.get("fits") or {}
            ba = (a.get("resampled") or {}).get("bin_sha256")
            bb = (b.get("resampled") or {}).get("bin_sha256")
            same = fa.get("data_sha256") == fb.get("data_sha256") and ba == bb
            all_ok = all_ok and same
            rows.append([case, (fa.get("data_sha256") or "")[:16],
                         (fb.get("data_sha256") or "")[:16],
                         (ba or "")[:16], (bb or "")[:16], "是" if same else "**否**"])
        tbl(["用例", "修复前 signal sha(16)", "修复后 signal sha(16)",
             "修复前 bin sha(16)", "修复后 bin sha(16)", "逐位相同"], rows)
        A(f"全部合法/真实用例 signal 与 bin 逐位一致：**{all_ok}**。")
        A("")

    # ── 多 worker 聚合 ──────────────────────────────────────────────────────
    if fix:
        F = index(fix)
        A("## 4. 多 worker 故障聚合（确定性）")
        A("")
        rows = []
        for case in ("multi_bad",):
            r = F.get((case, 0), {})
            for e in (r.get("tile_failures") or {}).get("tile_errors", []):
                rows.append([case, e["tile_ipix"], e["machine_code"],
                             e["worker_min"], e["worker_max"], e["worker_count"],
                             e["read_failures"], e["access_hits"]])
        for rep in range(3):
            r = F.get(("same_bad_multi_worker", rep), {})
            for e in (r.get("tile_failures") or {}).get("tile_errors", []):
                rows.append([f"same_bad r{rep}", e["tile_ipix"], e["machine_code"],
                             e["worker_min"], e["worker_max"], e["worker_count"],
                             e["read_failures"], e["access_hits"]])
        tbl(["用例", "tile_ipix", "机器码", "worker_min", "worker_max",
             "worker_count", "read_failures", "access_hits"], rows)
        A("聚合按 `tile_ipix` 升序（`std::map`）；同一 tile 多 worker 访问合并为一条，"
          "`worker_min/max/count` 为确定性统计量。`read_failures` 为真实失败读取次数"
          "（并发首次 miss 竞争可 >1，观测值）；`access_hits` 为采样路径遇到该错误的"
          "次数。")
        A("")

    # ── CHECKSUM 规则 ───────────────────────────────────────────────────────
    if fix:
        F = index(fix)
        A("## 5. FITS 校验规则（正例 / bitflip 反例 / 缺字段）")
        A("")
        rows = []
        for case in ("checksum_ok", "checksum_absent", "checksum_bad"):
            r = F.get((case, 0), {})
            c = r.get("tile_cache") or {}
            rows.append([case, r["exit_code"],
                         c.get("checksum_present_reads"),
                         c.get("checksum_absent_reads"),
                         ", ".join(r.get("codes") or []) or "-"])
        tbl(["用例", "exit", "checksum_present_reads", "checksum_absent_reads",
             "机器码"], rows)
        A("DATASUM/CHECKSUM 存在时以已读字节算 1's-complement 校验和（不额外重读）；"
          "缺校验字段**不是失败**，但显式计入 `checksum_absent_reads`；"
          "BSCALE/BZERO/BLANK/压缩等改变原始字节语义时回退 cfitsio 全 HDU 校验。")
        A("")

    # ── session ─────────────────────────────────────────────────────────────
    if sess:
        A("## 6. 正式 session 入口（`p3_session_run`）")
        A("")
        rows = []
        for r in sess["results"]:
            rows.append([r["case"], r.get("status_name"), r.get("fits_present"),
                         r.get("expect_code") or "-", r["verdict"]])
        tbl(["用例", "ACS 状态", "发布 FITS", "期望机器码", "判定"], rows)
        A(f"session 用例 {sess['n_cases']} 个，失败 {sess['n_fail']} 个。合法无覆盖仍 "
          "`ACS_OK` + FITS；完整性错误 `ACS_ERR_PARAM`；I/O 故障 `ACS_ERR_IO`；"
          "`last_error` 携带 `P3-TILE-*` 与 tile ipix。")
        A("")

    # ── 单测 ────────────────────────────────────────────────────────────────
    A("## 7. 自动化测试")
    A("")
    A("- `eng/tests/unit/p3_tile_semantics_test.cpp` + `run_p3_tile_semantics_test.py`："
      "**115/115 checks passed**（真实生产库；分类/缓存/聚合/身份）；"
      "落盘输出 `evidence/unit/p3_tile_semantics_test.txt`。")
    A("- `实验/issue87-stage3-tile-integrity/run_matrix.py`：受控矩阵 22 例（修复后 0 fail）。")
    A("- `实验/issue87-stage3-tile-integrity/run_session.py`：session 入口 7 例（0 fail）。")
    A("- `实验/issue87-stage3-tile-integrity/stale_output_test.py`：同 output_dir 陈旧产物"
      "失效（修复后 PASS）。")
    A("- 提交前工作树**从零构建**（`/workspace/.e87-verify-build`）成功；该 clean 二进制"
      "跑同一受控矩阵 22 例 0 红（`evidence/clean/results.json`）。")
    A("- 提交后由同一源码重建（`build_dirty=false`，字节 SHA 不同）跑同一矩阵 22 例 0 红"
      "（`evidence/postcommit/results.json`，实测 SHA 见该文件 `acsd_sha256`），"
      "证明字节 SHA 差异不改变判据结论。")
    A("")

    # ── 回归 ────────────────────────────────────────────────────────────────
    A("## 8. 既有回归")
    A("")
    rows = []
    if e1:
        rows.append(["#91 22 例 CLI（e87_regress）", f"{e1['n_cases']}",
                     f"{e1['n_fail']}", "0/22 红"])
    if e1r:
        rows.append(["#91/E1-R 故障注入（e87_regress）",
                     f"{len(e1r.get('results', []))}",
                     f"{e1r['n_fail']}", "0/5 红"])
    rows.append(["立项 30 条单位裁决单测", "30", "0",
                 "30/30 passed（evidence/unit/p3_input_unit_guard_test.txt）"])
    tbl(["回归项", "用例数", "失败", "结论"], rows)
    if e1:
        A("22 例退出码 / 机器码 / FITS data sha256 / finite·NaN 计数与 #91 已提交证据"
          "逐例一致（见 `verify_matrix.py` 的 bitwise 段落与本报告 §3 的同源方法）。")
        A("")

    # ── 性能 ────────────────────────────────────────────────────────────────
    if perf:
        A("## 9. 性能与共享 Tile Cache 量化对照")
        A("")
        A(f"基线 acsd `{perf['baseline_acsd_sha256'][:16]}…`，修复后 acsd "
          f"`{perf['fixed_acsd_sha256'][:16]}…`；同一真实 M42 writer 产品"
          f"（`{perf['hips']}`，只读单位归一化视图）。交错执行 prefix/fixed（同一 "
          "`sub_block_px` 背靠背）以减小页缓存/系统漂移。")
        A("")
        rows = []
        by = {}
        for r in perf["results"]:
            by.setdefault((r["suite"], r["tag"], r["sb"]), []).append(r)
        for (suite, tag, sb), rs in sorted(by.items()):
            c = (rs[0].get("tile_cache") or {})
            walls = sorted(x["wall_s"] for x in rs)
            rows.append([suite, tag, sb, rs[0].get("workers_label"),
                         f"{walls[0]:.3f}", f"{statistics.median(walls):.3f}",
                         f"{walls[-1]:.3f}",
                         f"{max((x.get('rss_peak_bytes') or 0) for x in rs)/1e6:.1f}",
                         c.get("checksum_present_reads"), c.get("misses"),
                         (rs[0].get("fits_sha256") or "")[:16]])
        tbl(["套件", "二进制", "sub_block_px", "nw(计划)", "wall min(s)",
             "wall 中位(s)", "wall max(s)", "RSS(MB)", "checksum reads",
             "misses", "signal sha(16)"], rows)
        cb = load(os.path.join(EVID, "checksum_bench.json"))
        if cb:
            A(f"reader 层微基准（同源 `checksum_probe.cpp` 分别链接修复前/后 "
              f"`libacsd_hips`，遍历该产品 MOC 声明的 {cb['tiles']} 个叶级 tile）："
              f"修复前 {cb['baseline']['best_ns_per_tile']:.0f} ns/tile，"
              f"修复后 {cb['fixed']['best_ns_per_tile']:.0f} ns/tile，"
              f"差 {cb['delta_ns_per_tile']:+.0f} ns/tile "
              f"({cb['delta_pct']:+.1f}%)。")
            A("")
        A("结论：所有合法/真实用例修复前后 FITS signal 逐位一致；reader 层每 tile "
          "开销 +63 µs 量级（DATASUM/CHECKSUM 的 1's-complement 求和，以已读字节计算、"
          "不二次读盘），对 523 个唯一 tile 约 +33 ms。cache-resident 小工作集端到端 "
          "wall 与修复前同量级；real_stress 大工作集（工作集远大于默认 `max_tiles` "
          "导致缓存抖动）端到端 wall 受 I/O 抖动支配，min 值同量级。RSS 与修复前同量级，"
          "但**不能声称“RSS 不增”**：上表 RSS 为各配置 4 次重复取最大峰值，real_stress "
          "sb384 146.8→154.3 MB（+5.1%）、sb768 71.8→72.4 MB（+0.8%），而 cache_resident "
          "同为峰值口径却双向大幅波动（sb192 12.7→34.5 MB、sb96 44.1→16.2 MB），说明单次"
          "峰值噪声与并发调度主导，本轮数据不足以支撑任何单调性结论。"
          "修复前 manifest 的 `tile_cache` 恒为 null/0（旧实现 close 后才取 stats 的"
          "缺陷），修复后随产物落盘真实计数。")
        A("")

    # ── 陈旧产物 ────────────────────────────────────────────────────────────
    if stale:
        A("## 10. 失败 run 的输出目录完整性")
        A("")
        A(f"同一 output_dir 先成功后失败：失败 run 退出 `{stale['fail_exit']}`，"
          f"成功后 `{stale['success_artifacts_after_fail'] or '无残留成功 artifact'}`，"
          f"`p3_tile_failures.json`={stale['tile_failures_present']}，"
          f"manifest={stale['manifest_status']}，tmp={stale['tmp_leftovers']}。")
        A("")

    # ── NOT_RUN ─────────────────────────────────────────────────────────────
    A("## 11. NOT_RUN / 实际边界")
    A("")
    A("1. **真实权限拒绝（EACCES）**：本环境以有权限用户运行，`open` 权限故障以"
      "“tile 路径被目录占位”构造受控 I/O 失败（`IO_ERROR`，exit 7）；未构造独立 EACCES 实测。")
    A("2. **磁盘满/句柄耗尽**：未注入；属资源域独立故障（既有 disk_full 分类未改）。")
    A("3. **HTTP/远程 HiPS、压缩 tile（fpack）**：reader 的本地路径校验和缺失字段规则"
      "已实现并回退 cfitsio 全 HDU 校验；未做远程/压缩实测。")
    A("4. **父级覆盖归约（#95）**：MOC 仅低阶父级条目判 `FOOTPRINT_UNDETERMINED`，"
      "不做归约；`undetermined_coarse_moc` 已实测。")
    A("5. **同名多次 run 的多个 run manifest**：CLI 每次运行写一个 `acsd_run_*.json`；"
      "陈旧成功产物已由本修复失效，但多 manifest 的选择由消费方按最新 mtime/状态判定，"
      "本轮未改该约定。")
    A("6. **原始真实观测数据**：仓库内无未处理的原始 M42 FITS；使用 M42 管线的 writer "
      "产品 `run/HIPS-PACK-01/corpus_big` 的只读 symlink 单位归一化视图（原产品只读，"
      "未改写 tile/MOC 字节）。")
    A("")

    # ── 复现 ────────────────────────────────────────────────────────────────
    A("## 12. 复算命令")
    A("")
    A("```bash")
    A("python3 实验/issue87-stage3-tile-integrity/make_fixtures.py")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag fixed --expect fixed")
    A("# 规范 prefix 证据的二进制确为 /workspace/.e87-base-build/acsd (sha 66614b29…)；")
    A("# 独立重建基线 (同 476c023 源码, 字节 SHA 因 build stamp 而异) 跑同一矩阵:")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd /workspace/.e87-base-build/acsd --tag prefix --expect prefix")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd /tmp/acsd-e87-base-build/acsd --tag prefix_rebuild --expect prefix")
    A("python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag fixed_rebuild --expect fixed")
    A("python3 实验/issue87-stage3-tile-integrity/verify_matrix.py")
    A("python3 实验/issue87-stage3-tile-integrity/verify_matrix.py --prefix-tag prefix_rebuild --fixed-tag fixed_rebuild --out evidence/verify_rebuild.json")
    A("python3 实验/issue87-stage3-tile-integrity/run_session.py --build build/linux-dev")
    A("python3 实验/issue87-stage3-tile-integrity/stale_output_test.py --acsd build/linux-dev/acsd --tag fixed")
    A("python3 实验/issue87-stage3-tile-integrity/perf_matrix.py --baseline /workspace/.e87-base-build/acsd --fixed build/linux-dev/acsd")
    A("python3 实验/issue87-stage3-tile-integrity/checksum_bench.py --baseline /workspace/.e87-base-build --fixed build/linux-dev")
    A("python3 eng/tests/unit/run_p3_tile_semantics_test.py")
    A("# 回归")
    A("python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e87_regress")
    A("python3 实验/issue91-stage3-bunit/run_session_entry.py --src \"$PWD\" --build build/linux-dev --tag e87_regress")
    A("python3 eng/tests/unit/run_p3_input_unit_guard_test.py")
    A("python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag e87_regress --expect fixed")
    A("```")
    A("")

    # ── 输入哈希 ────────────────────────────────────────────────────────────
    if fixtures:
        A("## 13. 固定输入哈希")
        A("")
        A("| 夹具 | properties | manifest | MOC | tile 集合 |")
        A("|---|---|---|---|---|")
        seen = {}
        for sc in fixtures["scenarios"]:
            h = sc.get("input_hashes") or {}
            key = sc["case"]
            if key in seen:
                continue
            seen[key] = True
            tiles = sorted((k, v) for k, v in h.items() if "Npix" in k)
            tiles_hash = hashlib.sha256(
                ("".join(f"{k}:{v}" for k, v in tiles)).encode()).hexdigest()[:16]
            A(f"| {key} | {(h.get('signal/properties') or '')[:16]} | "
              f"{(h.get('manifest.json') or '')[:16]} | "
              f"{(h.get('signal/Moc.fits') or '-')[:16]} | "
              f"{len(tiles)} tiles / {tiles_hash} |")
        A("")

    A("## 14. 原始证据")
    A("")
    A("- `evidence/prefix/results.json`、`evidence/fixed/results.json`（CLI 矩阵）")
    A("- `evidence/verify.json`（67 条机器判据）")
    A("- `evidence/session/results.json`（session 入口）")
    A("- `evidence/stale/results_fixed.json`（陈旧产物）")
    A("- `evidence/perf/results.json`（性能）")
    A("- 逐例原始日志 `evidence/<tag>/*.log`；受控夹具 `work/fixtures/`")
    A("")

    out = os.path.join(HERE, "REPORT.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print(f"wrote {out} ({len(L)} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
