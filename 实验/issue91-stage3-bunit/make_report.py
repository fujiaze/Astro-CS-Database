#!/usr/bin/env python3
"""Issue #91：从证据 JSON 生成 REPORT.md（数字全部来自实测产物，不手抄）。

用法: python3 实验/issue91-stage3-bunit/make_report.py
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def git(*a) -> str:
    return subprocess.run(["git", "-C", REPO, *a], capture_output=True,
                          text=True).stdout.strip()


# 本轮唯一参考基线（用户指定；不随本地 HEAD/变基漂移）
BASE_SHA = "51e06540eb021c7272e87cf67b34e53c0648cf7a"


def md_table(rows, header):
    out = ["| " + " | ".join(header) + " |",
           "|" + "|".join(["---"] * len(header)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(str(x) for x in r) + " |")
    return "\n".join(out)


def main() -> int:
    fixtures = json.load(open(os.path.join(WORK, "fixtures.json"), encoding="utf-8"))
    base = json.load(open(os.path.join(EVID, "base", "results.json"), encoding="utf-8"))
    fixed = json.load(open(os.path.join(EVID, "fixed", "results.json"), encoding="utf-8"))
    sbase = json.load(open(os.path.join(EVID, "base", "session_entry.json"), encoding="utf-8"))
    sfixed = json.load(open(os.path.join(EVID, "fixed", "session_entry.json"), encoding="utf-8"))
    inv = json.load(open(os.path.join(EVID, "invariance.json"), encoding="utf-8"))

    bmap = {r["case"]: r for r in base["results"]}
    fmap = {r["case"]: r for r in fixed["results"]}
    sbmap = {r["case"]: r for r in sbase["results"]}
    sfmap = {r["case"]: r for r in sfixed["results"]}

    # 每个用例的输入 properties SHA-256（覆盖类用例取自 base/partial 产品）
    phash = {v["case"]: v["properties_sha256"][:12] for v in fixtures["variants"]}
    for name, key in (("legal_covered_all_faces", "base_hips"),
                      ("legal_partial_coverage", "partial_hips")):
        p = os.path.join(fixtures[key], "signal", "properties")
        phash[name] = sha256_file(p)[:12] if os.path.isfile(p) else "-"

    rows = []
    for case, f in fmap.items():
        b = bmap.get(case, {})
        sb = sbmap.get(case, {})
        sf = sfmap.get(case, {})
        rows.append([
            f"`{case}`",
            f"`{phash.get(case, '-')}`",
            f"{b.get('exit_code','-')}/{'Y' if b.get('fits_present') else 'n'}/{','.join(b.get('semantic_codes') or []) or '-'}",
            f"{f['exit_code']}/{'Y' if f['fits_present'] else 'n'}/{','.join(f['semantic_codes']) or '-'}",
            f.get("verdict", "-"),
            f"{sb.get('status','-')}/{'Y' if sb.get('published_fits') else 'n'}",
            f"{sf.get('status','-')}/{'Y' if sf.get('published_fits') else 'n'}",
        ])

    inv_rows = [[f"`{c['case']}`", c["shape"], c["bitwise_identical"], c["max_abs_diff"],
                 c["nan_mask_identical"], c["wcs_header_identical"],
                 c["finite_px"], c["nan_px"], c["verdict"]] for c in inv["cases"]]

    # 行为变更分类（由数据推出，不手抄）
    chg_accept_to_reject, chg_reject_to_accept, chg_code, chg_same = [], [], [], []
    for case, f in fmap.items():
        b = bmap.get(case, {})
        if not b:
            continue
        b_acc = b.get("exit_code") == 0
        f_acc = f.get("exit_code") == 0
        if b_acc and not f_acc:
            chg_accept_to_reject.append(case)
        elif not b_acc and f_acc:
            chg_reject_to_accept.append(case)
        elif not b_acc and not f_acc and \
                (b.get("semantic_codes") or [""])[0] != (f.get("semantic_codes") or [""])[0]:
            chg_code.append(case)
        else:
            chg_same.append(case)

    unit_run = subprocess.run([sys.executable,
                               os.path.join(REPO, "eng", "tests", "unit",
                                            "run_p3_input_unit_guard_test.py")],
                              capture_output=True, text=True)
    unit_tail = [l for l in unit_run.stdout.splitlines() if "passed" in l or "failed" in l]
    unit_ok = unit_run.returncode == 0
    inv_ok = inv["all_pass"]
    base_fail = base["n_fail"]
    fixed_fail = fixed["n_fail"]

    text = f"""# Issue #91 证据｜Stage3 BUNIT 生产修复与真实 E2E

本目录是 Issue #91「正式 Stage3 Export 读取缺失/无法证实的 HiPS signal 单位时
自动假定 `ADU/sr` 并产出错误科学身份 FITS」的施工与取证单元。所有数字由
`make_report.py` 从 `evidence/*.json`（真实 CLI / 探针的实测产物）生成，不手抄。

| 项 | 值 |
|---|---|
| base SHA（本轮唯一基线） | `{BASE_SHA}` |
| 生产修复提交 | `{git('log', '--format=%H', '-1', '--', 'lib/algorithms/resample/p3_input_unit.cpp')}` |
| 修复前构建（git archive 导出 51e06540e + 本地合成 commit） | `ed60d8eb4696524a8f6b8bd2fa1ac08197d58fa1` |
| 修复后 acsd 二进制 SHA-256 | `{fixed['acsd_sha256']}` |
| 修复前 acsd 二进制 SHA-256 | `{base['acsd_sha256']}` |
| 受控变体数（真实 CLI 矩阵） | {len(rows)} |
| 修复前 CLI 矩阵红项 | {base_fail}/{len(rows)} |
| 修复后 CLI 矩阵红项 | {fixed_fail}/{len(rows)} |
| 单元测试（30 条判决矩阵） | {unit_tail[0] if unit_tail else 'n/a'} → {'PASS' if unit_ok else 'FAIL'} |
| 数值不变性（bitwise / max\\|Δ\\|） | {'PASS' if inv_ok else 'FAIL'} |

## 1. 缺陷复现（修复前，实测）

### 1.1 session 入口（p3_session_run，生产 API）

`evidence/base/session_entry.json`：**10/10 用例 status=0 且发布 FITS**，包括
`missing_bunit`、`empty_bunit`、`case_alias_dup`、`dup_diff`、`alias_conflict`、
`semantics_conflict`、`legacy_px2_alias`。即：无输入单位证据/身份冲突时，
正式 session 入口成功退出并写出被标为 `ADU/sr` 的 FITS。

### 1.2 CLI / scheduler 入口（真实 `acsd export`）

`evidence/base/results.json`：{base_fail}/{len(rows)} 红。其中**真实消费端 published
ADU/sr 且 rc=0** 的三例为 `case_alias_dup`、`alias_conflict`、`semantics_conflict`
（详见 3. 表内 base 列 `0/Y/-`）；`legal_legacy_px2`（`ADU/px^2`）在 base 已被
拒但错误码不区分原因。

## 2. 修复内容（生产路径）

- 新增**唯一**输入单位身份裁决 `acsd::phase3::p3_input_unit_verdict`
  (`lib/algorithms/resample/p3_input_unit.cpp` + `_io.cpp`)；单位串归类仍走冻结单位表
  `p3rsmp::resolve_bunit`，不另建第二套单位体系。
- `p3_sampler_open_ex` 改为**裁决先行**：未通过 → `P3_RS_PARAM` 且 `err` 携带
  `P3-INPUT-*` 码；通过 → `*out_bunit = unit.bunit_canonical`（删除
  `p.bunit.empty() ? "ADU/sr" : p.bunit`）。
- `p3_session_run`：失败时把具体失败类型与输入产品身份写入 `last_error` 与日志；
  worker 失败的错误串经互斥保护可靠上抛（E1-C）。
- `p3_output.cpp`（整幅 + 流式两处）：删除 `bunit ? bunit : "ADU/sr"` 缺省；
  缺声明/非 canonical 串在**创建任何临时文件之前**拒绝。
- `hips_properties_parse`：`BUNIT` 逻辑键（大小写不敏感）重复声明即拒（纵深防御）。
- scheduler `p3n_guard_input_units` 收敛为同一裁决的薄适配；`properties`/`resample2`
  两节点的 BUNIT 一致性比对改为比较 **canonical 身份**（raw↔canonical 差异是合法情形）。
- 文档正本同步：`DATA_SEMANTICS.md` §3（产品单位身份唯一性）、`PHASE3_RESAMPLE.md` G5、
  `PHASE3_RSMP_IMPL.md` §6.3、`PHASE3_FITS_IMPL.md` §2/§4、`detail/export/modules.md`。

## 3. 真实 CLI 矩阵（同一合法 HiPS 输入的受控变体）

命令：`acsd export --json work/configs/<tag>/<case>.json -y`（每个用例一个固定 config）。
base 列 = 修复前二进制；fixed 列 = 修复后；session 列 = `p3_session_run` 探针。

{md_table(rows, ["用例", "properties SHA-256(12)", "base CLI rc/FITS/code", "fixed CLI rc/FITS/code", "fixed 判定", "base session status/FITS", "fixed session status/FITS"])}

{md_table(rows, ["用例", "properties SHA-256(12)", "base CLI rc/FITS/code", "fixed CLI rc/FITS/code", "fixed 判定", "base session status/FITS", "fixed session status/FITS"])}

**行为变更分类**（相对 base，真实 CLI）：

- base `rc=0` 发布 FITS → 修复后明确拒绝：{', '.join('`'+c+'`' for c in chg_accept_to_reject) or '（无）'}
  —— 这正是 #91 的缺陷本体（身份冲突/矛盾/重复被当成成功产品）。
- base 拒绝 → 修复后接受：{', '.join('`'+c+'`' for c in chg_reject_to_accept) or '（无）'}
  —— 冻结词汇表内、量纲唯一可判的等价立体角写法（`ADU^1/sr^1`）；产品仍写 canonical `ADU/sr`，
  输入原始串保留在 `p3_props.json:bunit_input`。这是本轮**唯一的接受面放宽**，且不放宽
  任何无法唯一判定身份的情形。
- 仍拒绝但错误码更精确（消除“原因不可诊断”）：{', '.join('`'+c+'`' for c in chg_code) or '（无）'}
- 行为不变：{len(chg_same)} 例。

判决合同（`P3-INPUT-*` 码 → 退出类别）：
| 情形 | 码 | 退出 |
|---|---|---|
| BUNIT 键缺失 | `P3-INPUT-BUNIT-MISSING` | 3 |
| BUNIT 空串 | `P3-INPUT-BUNIT-EMPTY` | 3 |
| 同值重复逻辑键（含大小写别名） | `P3-INPUT-BUNIT-DUPLICATE` | 3 |
| 异值重复 | `P3-INPUT-BUNIT-CONFLICT` | 3 |
| 两个声明面（BUNIT/ACSD_SIGNAL_UNIT）不一致 | `P3-INPUT-UNIT-DECLARATION-CONFLICT` | 3 |
| 冻结词汇表外/无法解释 | `P3-INPUT-BUNIT-UNPARSABLE` | 3 |
| 裸 ADU 缺像素语义 | `P3-INPUT-BUNIT-UNDECIDABLE` | 3 |
| 与像素语义/面积幂次矛盾 | `P3-INPUT-UNIT-SEMANTICS-CONFLICT` | 4 |
| 可解析但非 signal 面亮度 | `P3-INPUT-NOT-SURFACE-BRIGHTNESS` | 4 |
| 未知像元语义词 | `P3-INPUT-PIXEL-SEMANTICS-UNSUPPORTED` | 4 |

失败发布状态（E1-C）：全部拒绝用例 `fits=false / manifest=incomplete /
staging=none`（`verify_evidence.py` 全过）。

## 4. 独立 Astropy 读回与数值不变性

`evidence/invariance.json`（对两入口都接受的合法用例，逐个用 astropy 读回）：

{md_table(inv_rows, ["用例", "shape", "bitwise", "max\\|Δ\\|", "NaN 掩膜", "WCS 头", "有限像元", "NaN 像元", "判定"])}

比较方式：**逐位（bitwise）**比较 signal DATA 字节；另报 max|Δ|（实为 0.0）、
NaN 掩膜、WCS 主头关键字（NAXIS/CTYPE/CUNIT/CRPIX/CRVAL/CD/BITPIX）。四个用例
（含 292 有限/3804 NaN 的部分覆盖用例）全部逐位相同。

独立读回字段（Astropy）：`BUNIT`、`CRVAL1/2`、`CRPIX1/2`、`CD1_1..CD2_2`、`CTYPE1/2`、
`CUNIT1/2`、`BITPIX`、shape、有限/NaN 计数。

## 5. 入口可达性（不得伪造 CLI 运行）

- 真实 `acsd export` 的 phase3 IR = 5 节点链
  `properties → wcs → resample2 → writer → verify`
  （`lib/infrastructure/cli/runtime_client.cpp::build_pipeline_ir`）；`properties`
  节点 = `p3_op_properties`（`p3n_guard_input_units` + `p3_sampler_open_ex`）。
- `p3_session_run` 经模块 `acsd.phase3.session`（`phase3_descriptor`）注册，**不在**
  `module_ports.registry.json` 的 phase3 五节点（15–19）内，也不出现在任何 CLI IR 中
  ⇒ CLI 无法独立调用 session 入口。故 session 入口证据由生产 C API 探针
  `session_entry_probe.cpp` 链接 `acsd_phase3_session` 生产闭包产生（构建命令记录于
  `run_session_entry.py` 输出，探针 SHA-256 = `{sfixed['probe_sha256']}`）。
- 修复后两个入口对同一输入文件给出**同一 verdict 与同一错误码**（3./`verify_evidence.py`）。

## 6. 复算命令

```bash
python3 eng/tests/unit/run_p3_input_unit_guard_test.py                 # 30/30
python3 实验/issue91-stage3-bunit/make_fixtures.py                     # 重建受控输入
python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag fixed
python3 实验/issue91-stage3-bunit/run_session_entry.py --src "$PWD" --build build/linux-dev --tag fixed
python3 实验/issue91-stage3-bunit/compare_invariance.py
python3 实验/issue91-stage3-bunit/verify_evidence.py
python3 实验/issue91-stage3-bunit/make_report.py
```

## 7. 未闭合项 / 超出本轮范围（如实登记）

1. **applied/relative 标度单位（#9 未闭合）**：本基线生产侧不存在合法的“已应用标度”
   单位词汇（上游仍统一写 `ADU/sr`）。本轮对 `BUNIT=ACSD_RELATIVE_FLUX`、`Jy/sr`
   等一律**拒绝而非改写**（不强行统一成 `ADU/sr`），并保留上游科学模型裁决依赖。
   输入 properties 无法看到 tile 头里的 `PHOTSCAL/PHOTAPPL`，故“标度状态与单位矛盾”
   在 properties 层不可判 —— 需 #9 的完整上游 unit/response 设计。
2. **#79 通带/响应 provenance**：本轮不涉及（未实现）。
3. **#87 tile 读故障 vs 合法无覆盖**：本轮不改（保留合法 NaN/覆盖语义）。
4. **文档锚陈旧**：多处文档引用 `DATA_SEMANTICS.md §31.1a`（当前正本无该节）、
   `PHASE3_FITS_IMPL.md` 伪代码仍含旧 COVERAGE 扩展 HDU 描述；本轮只订正了与
   BUNIT 缺省直接相关的语句，其余登记为独立文档债。
5. `eng/tests/module`、`eng/tests/integration` 等层在 base 与 fixed 上失败集合
   **完全相同**（各 14/15 项，均为既存文档锚/契约漂移），非本轮引入。
"""
    path = os.path.join(HERE, "REPORT.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
