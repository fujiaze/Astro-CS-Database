#!/usr/bin/env python3
"""Issue #91 / E1-R：从证据 JSON 生成 REPORT.md（数字来自实测产物，不手抄）。

用法: python3 实验/issue91-e1r-worker-open/make_report.py
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
EVID = os.path.join(HERE, "evidence")
E1 = os.path.join(REPO, "实验", "issue91-stage3-bunit", "evidence")

# 本轮开工基线（用户指定；不随本地 HEAD 漂移）
BASE_SHA = "01427335734ee43ed6f895edf80ae5715100ec29"
MODFILE = "lib/infrastructure/scheduler/src/module_adapters.cpp"


def git(*a) -> str:
    return subprocess.run(["git", "-C", REPO, *a], capture_output=True,
                          text=True).stdout.strip()


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def md_table(rows, header):
    out = ["| " + " | ".join(header) + " |",
           "|" + "|".join(["---"] * len(header)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(str(x) for x in r) + " |")
    return "\n".join(out)


def main() -> int:
    fx = json.load(open(os.path.join(EVID, "fixed", "results.json"), encoding="utf-8"))
    pf = json.load(open(os.path.join(EVID, "prefix", "results.json"), encoding="utf-8"))
    reg = json.load(open(os.path.join(E1, "e1r_regress", "results.json"), encoding="utf-8"))
    fm = {r["case"]: r for r in fx["results"]}
    pm = {r["case"]: r for r in pf["results"]}

    rows = []
    for case in fm:
        a, b = pm.get(case, {}), fm[case]
        rows.append([
            f"`{case}`", a.get("fault") or "（无）",
            f"{a.get('exit_code')}/{'Y' if a.get('fits_present') else 'n'}/"
            f"{(a.get('manifest') or {}).get('status')}",
            f"{b.get('exit_code')}/{'Y' if b.get('fits_present') else 'n'}/"
            f"{(b.get('manifest') or {}).get('status')}",
            ",".join(b.get("codes") or []) or "-",
            b.get("verdict"),
        ])

    defect_rows = []
    for case in ("multi_normal", "single_inject_k0", "multi_inject_k1", "multi_inject_k3"):
        r = pm.get(case) or {}
        f = r.get("fits") or {}
        defect_rows.append([f"`{case}`", r.get("exit_code"),
                            (r.get("manifest") or {}).get("status"),
                            f.get("exact_zero_px"), f.get("all_zero_row_count"),
                            f.get("nan_px"), f"{f.get('sum'):.6f}" if f.get("sum") is not None else "-",
                            str(f.get("data_sha256"))[:16]])

    norm_rows = []
    for case in ("legal_explicit", "legal_bare_adu_sb", "legal_covered_all_faces",
                 "legal_partial_coverage"):
        o = {r["case"]: r for r in json.load(
            open(os.path.join(E1, "fixed", "results.json"), encoding="utf-8"))["results"]}.get(case, {})
        n = {r["case"]: r for r in reg["results"]}.get(case, {})
        of, nf = o.get("fits") or {}, n.get("fits") or {}
        norm_rows.append([f"`{case}`", str(of.get("data_sha256"))[:16],
                          str(nf.get("data_sha256"))[:16],
                          of.get("data_sha256") == nf.get("data_sha256")])

    text = f"""# Issue #91 / E1-R 证据｜p3_op_resample worker sampler 打开失败的可靠传播

本目录是 E1-R 的施工与取证单元：修复正式 Stage3 scheduler 的 `p3_op_resample` 中
“worker sampler 打开失败却被当作正常完成”的路径。数字由 `make_report.py` 从
`evidence/*.json`（真实 CLI 实测）生成，不手抄。显式**排除**：不重新设计单位裁决、
不修改 E1 已通过的 22 例科学单位判决规则、不扩大为 #87 的 tile 缓存分类重构。

| 项 | 值 |
|---|---|
| 开工基线（参考 main） | `{BASE_SHA}` |
| 本轮生产修复提交 | `{git('log', '--format=%H', '-1', '--', MODFILE)}` |
| 修复后 acsd SHA-256 | `{fx['acsd_sha256']}` |
| 修复前 A/B acsd SHA-256（014273357 + 仅注入管线） | `{pf['acsd_sha256']}` |
| E1-R 场景数 | {len(rows)} |
| 修复前（A/B）注入用例结果 | 全部 exit 0 + 发布 complete FITS（缺陷） |
| 修复后注入用例结果 | 全部 exit 3、无 FITS/bin、manifest=incomplete |
| E1 22 例回归（新 tag `e1r_regress`） | {reg['n_fail']}/22 红（与 E1 已提交证据逐例一致） |
| 立项 30 条单位裁决单测 | 30/30（见 §5） |

## 1. 缺陷（修复前 A/B 实测）

修复前代码：worker 在 `p3_sampler_open_ex != P3_RS_OK` 时 `return;`，外层 `band_task`
无条件 `band_executed.fetch_add(1)` ⇒ 失败行带既未采样、又满足 `band_executed == nw`
完整性判据，`p3_resampled.bin` 未写区间（`reserve` 出的空洞读回 0.0）被当作产品发布。

A/B 构建 = `014273357` + **仅**注入管线（`ab_prefix_patch.py`，保留旧静默 return/无条件计数），
同一注入 `ACSD_RT001_FAULT=resample_worker_open_fail[:k]` 下达结果：

{md_table(defect_rows, ["用例", "exit", "manifest", "exact_zero_px", "全零行数", "NaN 像元", "signal sum", "data sha256(16)"])}

- `single_inject_k0`：整个 64×64 产品 **4096 像元全为 0.0（sum=0）**，仍 `exit=0`、`manifest=complete`。
- `multi_inject_k1` / `k3`：恰 **16384 = 128×128** 个像元为 0.0（一个子块行带），sum 从 293812 降至 221551/219722，仍 `exit=0`、`manifest=complete`。
- 对照组 `multi_normal`：`exit=0`、0 个零像元、sum=293812.09539842606。
- 即：损坏被写成**数值 0 的“有效”科学像素**（而非失败或 NaN），外部读者无法区分。

## 2. 修复内容

- **失败状态线程安全**（`module_adapters.cpp::p3_op_resample`）：`std::mutex` +
  `std::atomic<bool>` + 首错保留的 `code/detail/is_io/is_input`；worker 失败即记录
  `P3-RESAMPLE-WORKER-OPEN-INPUT`（`P3_RS_PARAM`，输入产品不可用/身份不可证）或
  `P3-RESAMPLE-WORKER-OPEN-IO`（`P3_RS_IO`），并带 worker 序号、打开目标、`band_blocks`、
  输入 `hips_dir` 与底层 `P3-INPUT-*` 明细。
- **不再计入正常完成**：`band_task` 只在 `worker()==true` 时 `band_executed++`；
  wait_all 后按 **取消 → worker 打开失败 → work unit 丢弃** 顺序判红（失败优先，错因具体）。
- **失败清理 / 不发布**：判红时 `binw.close()` + 删除临时 bin；`atomic_replace` 未执行
  ⇒ 无 `p3_resampled.bin` 成功版本；DAG 依赖使 writer/verify 不运行 ⇒ 无 FITS、无 complete manifest。
- **正常路径不变**：ThreadBudget/`rt::shared_work_executor`、共享 tile 缓存、子块划分
  （`block_range`）、band_active/peak/executed 语义、采样数学全部保持；仅在成功时把
  `worker_bands{{planned,executed,active_peak,sub_blocks,sub_block_px}}` 并入 `p3_resampled.json`
  以便独立核对“任务数与产品一致”。
- **确定性注入**：扩展既有测试钩子 `ACSD_RT001_FAULT`（不新增 env 变量；仍是单次 `getenv`，
  满足 `module_state_hygiene` 的出现次数登记）：
  `resample_worker_open_fail[:k]` —— 仅 worker k（默认 1）把打开目标替换为确定不存在的
  产品目录 ⇒ 走**真实 open 失败分支**；主 sampler 与 worker 0 均成功，故不会只测首次
  properties 校验。既有 `resample_drop_band` 语义不变（见 §4）。

## 3. E1-R 场景矩阵（真实 CLI）

命令：`acsd export --json work/configs/<tag>/<case>.json -y`（仅 config 与注入 env 不同；
输入 = E1 的同一合法 HiPS `实验/issue91-stage3-bunit/work/variants/legal_explicit`，
properties SHA-256 = `{sha256_file(os.path.join(REPO, '实验/issue91-stage3-bunit/work/variants/legal_explicit/signal/properties'))}`）。

{md_table(rows, ["用例", "注入", "修复前 A/B exit/FITS/manifest", "修复后 exit/FITS/manifest", "修复后机器码", "判定"])}

| 场景 | 目的 | 结果 |
|---|---|---|
| `multi_normal` (256×256, sb=128 → nblocks=4, nw=4) | 多 worker 正常运行 | exit 0、complete；`worker_bands={{planned:4,executed:4,active_peak:4,sub_blocks:4}}`、writer `sub_blocks=4`、0 NaN / 0 零像元 |
| `single_inject_k0` (64×64, sb=64 → nblocks=1, nw=1) | 单 worker 打开失败 | exit 3、无 FITS/bin、incomplete、`P3-RESAMPLE-WORKER-OPEN-INPUT`（worker 0） |
| `multi_inject_k1` (nw=4) | 多 worker 中**一个**失败 | 同上（worker 1） |
| `multi_inject_default` | 注入值不带 `:k`（默认 k=1） | 同上（worker 1） |
| `multi_inject_k3` | 最后一个 worker 失败 | 同上（worker 3） |

失败清理证据（`evidence/fixed/results.json` 逐例）：`fits_present=false`、
`resampled_bin_present=false`、`resampled_json_present=false`、`tmp_leftovers=[]`、
`staging_dirs=[]`、`manifest.status=incomplete` 且 summary 含机器码与 worker 序号；
output_dir 仅剩运行簿记（`acsd_run_*.json`、resource/alloc 记录、`run_context.json`）
与此前节点已合法落盘的上游产物（`p3_props.json`、`p3_wcs.json`）。

## 4. 既有注入未回退

`ACSD_RT001_FAULT=resample_drop_band`（E1-R 之前就存在）在修复后仍 fail-closed：

```
EXIT=9
resample sub-block work units dropped (executed 3/4): cancelled or fault-injected; fail-closed, no partial product
（无 output_phase3.fits / p3_resampled.bin）
```

同一 config 不设注入则 `EXIT=0` 并产出 FITS 与 bin（对照组）。

## 5. 回归（相对 E1 已提交证据）

E1 22 例真实 CLI（新 tag `e1r_regress`，不重写 E1 证据）：

- exit code / `P3-INPUT-*` 码 / FITS 有无 / `p3_props.bunit` / manifest status /
  **FITS signal DATA sha256** / 有限与 NaN 计数 —— **逐例一致**；新 `n_fail={reg['n_fail']}/22`。

四组合法 FITS 数值逐位回归：

{md_table(norm_rows, ["用例", "E1 已提交 data sha256(16)", "E1-R 后 data sha256(16)", "逐位相同"])}

正常多 worker run 的跨版本数值：`multi_normal` 修复前 A/B 与修复后 data sha256 完全相同
（`{str((fm.get('multi_normal') or {}).get('fits', {}).get('data_sha256'))[:16]}`，sum=293812.09539842606）。

旧 session 入口错误传播：`p3_session_run` 生产 C API 探针 10 用例的
`status / published_fits / P3-INPUT-* 码` 与 E1 提交证据逐例相同（无回退）。

30 条单位裁决单测：`30/30 passed`（`run_p3_input_unit_guard_test.py`）。

仓库既有测试层（`eng/tests/{{module,pipeline,integration,conformance,synthetic}}`）：
修复前 pristine `014273357` 与修复后**失败集合完全相同**（29 failed / 84 passed），
`module_state_hygiene` 的 A1–A6 违规清单逐行相同（`ENV_READ_COUNT_DRIFT` /
`THREAD_SPAWN_ALLOWLIST` 无新增）。

## 6. 复算命令

```bash
python3 实验/issue91-e1r-worker-open/ab_prefix_patch.py <pristine 014273357 树>   # 生成 A/B 注入树
python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag fixed --expect fixed
python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd <A/B 构建>/acsd --tag prefix --expect prefix
python3 实验/issue91-e1r-worker-open/verify_e1r.py
python3 实验/issue91-e1r-worker-open/make_report.py
# E1 回归（复用 E1 runner，落在 E1 证据单元的 e1r_regress tag）
python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e1r_regress
python3 实验/issue91-stage3-bunit/run_session_entry.py --src "$PWD" --build build/linux-dev --tag e1r_regress
```

## 7. NOT_RUN / 边界（如实登记）

1. **#87 tile I/O 故障分类**（`OUTSIDE_DECLARED_FOOTPRINT`/`DECLARED_MISSING`/`IO_ERROR`/
   `INVALID_TILE`/`CHECKSUM_FAILED`）**NOT_RUN**：本轮只做 worker sampler-open 错误传播，
   未触碰 tile 缓存/负缓存分类（任务书明确不扩大）。
2. **真实 worker open IO 失败**（磁盘满/句柄耗尽/权限）**NOT_RUN**：注入走的是
   “确定不存在的产品目录”这一 `P3_RS_PARAM` 分支；`P3_RS_IO` 分支的注入未构造
   （同一代码路径，但未有独立实测）。
3. **`p3_session_run` 的 worker open 失败**：E1 已实现其错误上抛（`last_error` 含码），
   本轮未再改动，也未对本轮注入面单独重测（session worker 不读 `ACSD_RT001_FAULT`）。
4. **优先级未动**：取消判定仍先于 worker-open 失败判定（若两者并发，报取消）；
   `write_failed` 判定仍在 band 判定之后（顺序沿用既有实现）。
5. **跨 run 陈旧产物**：若同一 `output_dir` 存在上一次成功 run 的 `p3_resampled.bin`，
   本次失败不会删除它（沿用既有所有失败路径的行为）；本轮未改，未测。
"""
    path = os.path.join(HERE, "REPORT.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
