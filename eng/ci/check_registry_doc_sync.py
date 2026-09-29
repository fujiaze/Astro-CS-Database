#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-REGISTRY-DOC-SYNC：注册表 ↔ 文档 双向一致门。

依据 `ENGINEERING_SPEC.md §8`（2026-09-16 增补）:「**注册表双向一致**：`eng/ci/checks.json`
与 `docs/engineering/01_CHECKS.md §2` 必须双向对齐（既不得「注册未登记」，也不得「文档承诺
P0 但无实现」）」。

判据:
  R1 注册未登记: checks.json 的注册项 ID 必须出现在 docs/engineering/01_CHECKS.md §2 表；
  R2 登记未注册: §2 表的 ID 必须存在于 checks.json；
  R3 退役项回归: §2.1「已退役检查器」记录里的 ID **不得**重新出现在注册表（退役只减不增）；
  R4 RESERVED 误注册: §2.1 RESERVED 表（文档承诺但无实现）里的 ID **不得**被注册 ——
     否则就是「文档说没实现、注册表却说有」的假绿；
  R5 双向对差可打印: --json-out 输出 added/missing/retired/reserved 四组差集。

用法:
  python3 eng/ci/check_registry_doc_sync.py [--registry eng/ci/checks.json] [--doc docs/engineering/01_CHECKS.md]
                                        [--json-out <file>] [--self-test]
exit 0 = 双向一致；exit 1 = 不一致；exit 2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
REGISTRY = "eng/ci/checks.json"
DOC = "docs/engineering/01_CHECKS.md"

ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.\-]*$")
BACKTICK_ID_RE = re.compile(r"`([A-Za-z0-9][A-Za-z0-9_.\-]*)`")


def _rows(section_text: str):
    """Markdown 表格数据行 -> 单元格列表（跳过表头与分隔行）。"""
    out = []
    for line in section_text.splitlines():
        s = line.strip()
        if not s.startswith("|"):
            continue
        cells = [c.strip() for c in s.strip("|").split("|")]
        if not cells:
            continue
        first = cells[0]
        if first.lower() in ("id", "退役项", "reserved项", "reserved") or set(first) <= set("-: "):
            continue
        out.append(cells)
    return out


def _slice(text: str, start_marker: str, end_markers) -> str:
    i = text.find(start_marker)
    if i < 0:
        return ""
    i += len(start_marker)
    j = len(text)
    for em in end_markers:
        k = text.find(em, i)
        if k >= 0:
            j = min(j, k)
    return text[i:j]


def parse_doc(doc_text: str):
    """解析面与 docs/engineering/01_CHECKS.md 实际结构逐一对齐：
    - §2 清单表 = 列表主体；
    - 退役记录 = §2.1 内「已退役检查器：」bullet 行（反引号 ID 序列）；
    - RESERVED = §2.1 内表头首格为 RESERVED 的表格（按表头锚定，
      在 `#### ` 子节标题处截断，避免把 §2.1.1 处置台账与登记面表格
      的反引号 token 误摘进 retired/reserved（垃圾 token 防线）。"""
    sec2 = _slice(doc_text, "## 2. 检查项清单", ["### 2.1", "## 3."])
    sec21 = _slice(doc_text, "### 2.1", ["#### ", "## 3."])
    listed = []
    for cells in _rows(sec2):
        if ID_RE.match(cells[0]):
            listed.append(cells[0])
    retired, reserved = [], []
    for line in sec21.splitlines():
        if "已退役检查器" in line:
            retired.extend(m.group(1) for m in BACKTICK_ID_RE.finditer(line))
    in_reserved_table = False
    for line in sec21.splitlines():
        s = line.strip()
        if not s.startswith("|"):
            in_reserved_table = False
            continue
        cells = [c.strip() for c in s.strip("|").split("|")]
        if not cells:
            continue
        if cells[0].startswith("RESERVED"):
            in_reserved_table = True
            continue
        if not in_reserved_table or set(cells[0]) <= set("-: "):
            continue
        m = BACKTICK_ID_RE.search(cells[0])
        if m:
            reserved.append(m.group(1))
        elif ID_RE.match(cells[0]):
            reserved.append(cells[0])
    return listed, retired, reserved


def load_registry(path: pathlib.Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    return [c["id"] for c in data.get("checks", []) if isinstance(c, dict) and "id" in c]


def evaluate(registry_ids, listed, retired, reserved):
    reg, lst = set(registry_ids), set(listed)
    ret, res = set(retired), set(reserved)
    return {
        "registry_count": len(registry_ids),
        "doc_listed_count": len(listed),
        "doc_retired_count": len(retired),
        "doc_reserved_count": len(reserved),
        # R1：注册未登记（注册表有、§2 无）
        "registered_not_documented": sorted(reg - lst),
        # R2：登记未注册（§2 有、注册表无）
        "documented_not_registered": sorted(lst - reg),
        # R3：退役项回归
        "retired_but_registered": sorted(ret & reg),
        # R4：RESERVED 却已注册
        "reserved_but_registered": sorted(res & reg),
    }


def _self_test() -> int:
    """正例 1 组 + 负例 5 组（纯 tempfile 夹具，不依赖真仓；夹具形态与
    docs/engineering/01_CHECKS.md 实际结构一致：退役记录为 §2.1 bullet 行、
    RESERVED 为 §2.1 表、§2.1.1 处置台账含易误摘的反引号 token）。"""
    failures = []
    doc_template = """# CI 检查项目录

## 2. 检查项清单

| ID | 类别 | 名称 | 命令/入口 | 门禁 |
|---|---|---|---|---|
{listed}
### 2.1 检查器退役与预留

- 已退役检查器：{retired}；退役明细以仓库台账为准；
- RESERVED（文档登记但无实现，重新注册前须先有实现与可执行负例）：

| RESERVED 项 | 重新注册前置条件 |
|---|---|
{reserved}
#### 未注册检查器处置台账（§2.1.1）

| 登记面 | 落点 | 状态 |
|---|---|---|
| `eng/ci/checks.json` 的 `changed_paths` glob | `CHK-NOISE-TOKEN` | 在册 |

## 3. 门禁分级
"""
    def render(listed=(), retired=(), reserved=()):
        f = lambda ids: "\n".join(f"| {i} | cat | n | c | P0 |" for i in ids)
        r = lambda ids: "、".join(f"`{i}`" for i in ids)
        return doc_template.format(listed=f(listed), retired=r(retired), reserved=f(reserved))

    pos = evaluate(["A-1", "B-2"], ["A-1", "B-2"], ["OLD-1"], ["GHOST-1"])
    if any(pos[k] for k in ("registered_not_documented", "documented_not_registered",
                            "retired_but_registered", "reserved_but_registered")):
        failures.append(f"正例应为全空差集，实得 {pos}")
    parsed = parse_doc(render(["A-1", "B-2"], ["OLD-1"], ["GHOST-1"]))
    if parsed[0] != ["A-1", "B-2"]:
        failures.append(f"正例：§2 表解析失败，实得 {parsed[0]}")
    if parsed[1] != ["OLD-1"]:
        failures.append(f"正例：退役记录解析失败，实得 {parsed[1]}")
    if parsed[2] != ["GHOST-1"]:
        failures.append(f"正例：RESERVED 表解析失败，实得 {parsed[2]}")
    if "CHK-NOISE-TOKEN" in parsed[1] or "CHK-NOISE-TOKEN" in parsed[2] or \
            "changed_paths" in parsed[1] or "changed_paths" in parsed[2]:
        failures.append(f"正例：§2.1.1 登记面表格 token 被误摘（垃圾 token 防线失效）：{parsed}")

    neg1 = evaluate(["A-1", "C-3"], ["A-1"], [], [])
    if neg1["registered_not_documented"] != ["C-3"]:
        failures.append(f"负例 R1（注册未登记）未命中：{neg1['registered_not_documented']}")

    neg2 = evaluate(["A-1"], ["A-1", "D-4"], [], [])
    if neg2["documented_not_registered"] != ["D-4"]:
        failures.append(f"负例 R2（登记未注册）未命中：{neg2['documented_not_registered']}")

    neg3 = evaluate(["A-1", "OLD-1"], ["A-1", "OLD-1"], ["OLD-1"], [])
    if neg3["retired_but_registered"] != ["OLD-1"]:
        failures.append(f"负例 R3（退役项回归）未命中：{neg3['retired_but_registered']}")

    neg4 = evaluate(["A-1", "GHOST-1"], ["A-1", "GHOST-1"], [], ["GHOST-1"])
    if neg4["reserved_but_registered"] != ["GHOST-1"]:
        failures.append(f"负例 R4（RESERVED 误注册）未命中：{neg4['reserved_but_registered']}")

    # 负例 5：doc 缺 §2 段 / 文档不存在 ⇒ fail-closed（由 main 判 2，这里验证解析为空）
    if parse_doc("## 1. 别的\n")[0] != []:
        failures.append("负例 5：缺 §2 时应解析为空（fail-closed 触发）")

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例差集全空且三面解析对位（含 §2.1.1 token 不误摘）；"
          "负例 R1/R2/R3/R4（注册未登记/登记未注册/退役项回归/RESERVED 误注册）"
          "均按预期命中；缺 §2 解析为空（fail-closed）")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="注册表 ↔ docs/engineering/01_CHECKS.md §2 双向一致门")
    ap.add_argument("--registry", default=str(REPO / REGISTRY))
    ap.add_argument("--doc", default=str(REPO / DOC))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()

    reg_path, doc_path = pathlib.Path(args.registry), pathlib.Path(args.doc)
    for p, label in ((reg_path, "注册表"), (doc_path, "文档")):
        if not p.is_file():
            print(f"REGISTRY_DOC_SYNC_FAIL: {label}不可用 {p}（fail-closed）", file=sys.stderr)
            return 2
    try:
        registry_ids = load_registry(reg_path)
    except Exception as exc:  # noqa: BLE001
        print(f"REGISTRY_DOC_SYNC_FAIL: 注册表不可解析: {exc}（fail-closed）", file=sys.stderr)
        return 2
    if not registry_ids:
        print("REGISTRY_DOC_SYNC_FAIL: 注册表零注册项（fail-closed）", file=sys.stderr)
        return 2

    listed, retired, reserved = parse_doc(doc_path.read_text(encoding="utf-8"))
    if not listed:
        print(f"REGISTRY_DOC_SYNC_FAIL: {doc_path} 的 §2 检查项表为空或不可解析"
              "（fail-closed，不得把「解析不到」当「一致」）", file=sys.stderr)
        return 2

    diff = evaluate(registry_ids, listed, retired, reserved)
    bad = {k: v for k, v in diff.items() if k.endswith(("_not_documented", "_not_registered",
                                                        "_but_registered")) and v}
    out = {"tool": "check_registry_doc_sync", "registry": str(reg_path), "doc": str(doc_path),
           **diff, "verdict": "PASS" if not bad else "FAIL"}
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        pathlib.Path(args.json_out).write_text(
            json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    if bad:
        print("REGISTRY_DOC_SYNC_FAIL:")
        for k, v in bad.items():
            print(f"  {k}: {v}")
        return 1
    print(f"REGISTRY_DOC_SYNC_PASS: 注册项 {diff['registry_count']} == §2 表 "
          f"{diff['doc_listed_count']}；退役 {diff['doc_retired_count']}、"
          f"RESERVED {diff['doc_reserved_count']}，双向差集全空")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
