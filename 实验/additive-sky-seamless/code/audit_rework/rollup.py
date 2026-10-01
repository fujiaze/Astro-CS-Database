# -*- coding: utf-8 -*-
r"""audit_rework 子树的**门汇总器**：把每条落盘 JSON 里的假值判据显式分类并据此退出码。

要闭合的失效域
==============
本子树 38 个脚本**无一 `sys.exit(1)`、无一 `exit(1)`**（实测 `grep -rn "sys.exit\|exit(1)"`
零命中），而 `run_all.sh` 用 `set -eu` 并在 `run()` 里把子壳的 `exit 1` 向上传。
⇒ **红灯在物理上无法传播到退出码**：任何判红都不会让 `run_all.sh` 失败。
前车实测已有 **四条已判红的门**在固化 JSON 与报告里照样被当作通过项引用
（违反规范 08 §5「红灯不以 waiver 覆盖，SKIP 不计通过」）。

为什么不能用「见�� `false` 就红」
================================
落盘 JSON 里的 `false` **不是同一种东西**，一刀切会产生大量假警报，
而假警报会让本汇总器恒红——恒红门会把真实缺陷永久藏在红灯里（与恒真门同等无效）。
实测 101 个 `false` 落在 **20** 个 `(文件, 叶名)` 组合上，四类语义：

| 类别 | 含义 | 是否让汇总器红 |
|---|---|---|
| `demonstration` | **负例演示**：字段为假**正是**被演示的内容（漏检面、恒绿门、相消） | 否 |
| `diagnostic` | 诊断读数，为假是被记录的事实，不承担通过/失败 | 否 |
| `table_cell` | 网格里逐格的分类标记（如 `lam_h <= 2.0`），本就不是门 | 否 |
| `open_red` | **真实的判据主张且当前为红**，或结构性恒红 | **是（不豁免）** |

⇒ 本汇总器按 `GATE_DISCLOSURE.json` 做**逐点显式披露**，且 **fail-closed**：
任何一个 `false` 没有被披露 ⇒ 记 `UNDECLARED` ⇒ **判红**。
披露文件里的 `open_red` 条目**不豁免**，照样让汇总器红。

读哪些 JSON
============
1. 本次 `run_all.sh` 刚产出的 `audit_rework/results/**/*.json`（gitignored 的工作副本）；
2. **固化正本** `实验/additive-sky-seamless/results/audit_rework/**/*.json`（已核对逐位一致）。

两个来源都扫：只扫工作副本会在「本机没跑过」时静默全绿，只扫固化正本则新产物不被检查。

用法
====
    python3 实验/additive-sky-seamless/code/audit_rework/rollup.py [--json OUT]
退出码：0 = 无 `open_red` 且无 `UNDECLARED`；1 = 有；2 = 披露文件本身有陈旧/冲突条目。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent                       # code/audit_rework/
UNIT = HERE.parents[1]                                      # 实验/additive-sky-seamless/
FROZEN = UNIT / "results" / "audit_rework"                  # 固化正本
WORK = HERE / "results"                                     # 本次运行产物（gitignored）
DISCLOSURE = HERE / "GATE_DISCLOSURE.json"

_INDEX = re.compile(r"\[\d+\]")


def norm(path: str) -> str:
    """把 `a.b[3].c` 归一为 `a.b[].c` —— 数组下标不是门的身份。"""
    return _INDEX.sub("[]", path)


def walk(node, prefix=""):
    """产出 (点路径, 值)。只对布尔取 False，不碰数值与字符串。"""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from walk(v, f"{prefix}.{k}" if prefix else str(k))
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from walk(v, f"{prefix}[{i}]")
    elif node is False:
        yield prefix, False


#: `_gate_rollup.json` 是门状态**清单**（另一个汇总器的产物），不是门读数本身；
#: 它的每一条都在本文件里有逐点披露，因此按清单类处理并另做交叉核对，避免重复计票。
META_FILE = "_gate_rollup.json"


def collect(root: Path):
    out = []
    if not root.exists():
        return out
    for f in sorted(root.rglob("*.json")):
        if f.name == META_FILE:
            continue
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
        except Exception as exc:                      # 解析失败必须可见，不得静默跳过
            out.append({"file": f.name, "dot": "<parse-error>",
                        "raw": f"{type(exc).__name__}: {exc}"})
            continue
        for dot, _ in walk(d):
            # 键用**文件名 + 归一点路径**：工作副本是平铺的（c3_seam_gate.json），
            # 固化正本带路由前缀（route1/c3_seam_gate.json），按相对路径会让同一条门
            # 在两个来源下算两次。文件名在两处一致，故用它做身份。
            out.append({"file": f.name, "dot": dot, "raw": norm(dot)})
    return out


def key(rec) -> str:
    return f"{rec['file']}|{rec['raw']}"


def cross_check(open_red_keys):
    """与工作副本里的 `_gate_rollup.json` 交叉核对（若存在）。

    两个汇总器互相独立；任何一方标 RED 而另一方未标 `open_red`，都说明披露不全
    ⇒ 判红。缺该文件不算通过（返回 None 表示「无法核对」，由调用方决定）。
    """
    out = {}
    for root in (WORK, FROZEN):
        f = root / META_FILE
        if not f.exists():
            continue
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
        except Exception as exc:
            out[str(f)] = ["<parse-error> %s" % exc]
            continue
        for line in d.get("red", []) or []:
            m = re.match(r"([^:]+):(\S+)\s*=", line)
            if not m:
                out.setdefault(str(f), []).append("<unparsed> %s" % line)
                continue
            k = f"{m.group(1)}|{norm(m.group(2))}"
            out.setdefault(str(f), []).append(k)
    missing = sorted({k for v in out.values() for k in v} - set(open_red_keys))
    return {"checked": sorted(out), "red_keys": sorted({k for v in out.values() for k in v}),
            "not_disclosed_as_open_red": missing}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=None, help="把汇总结果写到这个路径（不写默认只打印）")
    args = ap.parse_args(argv)

    disc = json.loads(DISCLOSURE.read_text(encoding="utf-8"))["entries"]
    # 披露键是「文件名|归一点路径」；归一化后再建索引，容忍披露里写成未归一的形式
    disc_keys = {norm(k) for k in disc}

    found, seen = [], set()
    for root in (WORK, FROZEN):
        for rec in collect(root):
            k = key(rec)
            if k in seen:
                continue
            seen.add(k)
            found.append(dict(rec, source=str(root)))

    declared, undeclared = [], []
    for rec in found:
        (declared if key(rec) in disc_keys else undeclared).append(rec)

    by_class = {}
    for k, meta in disc.items():
        by_class.setdefault(meta["class"], []).append(k)

    open_red = []
    open_red_keys = []
    for rec in declared:
        meta = disc.get(key(rec))
        if meta is None:
            continue
        if meta.get("class") == "open_red":
            open_red.append(dict(rec, why=meta.get("why", "")))
            open_red_keys.append(rec["file"] + "|" + rec["raw"])

    xchk = cross_check(open_red_keys)

    stale = [k for k in disc if k not in seen]

    report = dict(
        cross_check=xchk,
        note=("每个 false 必须被 GATE_DISCLOSURE.json 逐点披露；未披露即判红（fail-closed）。"
              "open_red 不豁免。"),
        counts=dict(sources=[str(WORK), str(FROZEN)],
                    false_seen=len(found), declared=len(declared),
                    undeclared=len(undeclared), open_red=len(open_red),
                    stale_disclosure=len(stale),
                    cross_check_mismatch=len(xchk["not_disclosed_as_open_red"])),
        by_class={c: len(v) for c, v in sorted(by_class.items())},
        undeclared=sorted((r["file"] + "|" + r["raw"] for r in undeclared)),
        open_red=[dict(key=r["file"] + "|" + r["raw"], why=r["why"]) for r in open_red],
        stale_disclosure=sorted(stale),
    )

    print("== audit_rework 门汇总 ==")
    print("  false 总数 %d；已披露 %d；未披露 %d；open_red %d；陈旧披露 %d"
          % (len(found), len(declared), len(undeclared), len(open_red), len(stale)))
    print("  按类别:", report["by_class"])
    for u in report["undeclared"]:
        print("  [UNDECLARED] %s" % u)
    for r in report["open_red"]:
        print("  [OPEN-RED ] %s" % r["key"])
        print("             %s" % r["why"])
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2, ensure_ascii=False),
                                   encoding="utf-8")
        print("  -> %s" % args.json)

    if xchk["not_disclosed_as_open_red"]:
        print("  [XCHECK ] 另一汇总器(_gate_rollup.json)标红但本表未列为 open_red:")
        for k in xchk["not_disclosed_as_open_red"]:
            print("             %s" % k)
    if stale:
        # 披露文件里有已不存在的条目：披露本身失准，同样不让静默通过
        return 2
    return 1 if (open_red or undeclared or xchk["not_disclosed_as_open_red"]) else 0


if __name__ == "__main__":
    sys.exit(main())