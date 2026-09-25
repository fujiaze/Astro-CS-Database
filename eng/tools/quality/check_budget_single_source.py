#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_budget_single_source.py —— 内存/资源预算「源唯一 + 有判别力」门（一页纸 S2-E）。

要修的缺陷（审查节点原话）
  · 一处「内存安全」结论是**比自身影子文件**得出的（构造恒真，没有判别力）；
  · 帧级内存闸门与调度器预算构成**两个并存分母**，而文档把单一来源登记为唯一。

判据（S2-E「判完成」）
  E1 源唯一：每个被登记的分母都必须在**唯一源文件**
     eng/packaging/config/runtime_resources.json 里有取值；实现侧（consumer）若自持
     同名字面量，必须与源**逐位相等**（不等即红）——这就是「两个并存分母」的机器判据。
  E2 有判别力：对每个分母做敏感性实测 —— 扰动该分母必须使结论（允许的并发帧数 cap
     与预算字节数）发生变化。扰动后结论不变 ⇒ 该分母对结论无判别力（构造恒真形态）
     ⇒ 红。判据不承认「比自身影子文件」这类恒真比较。
  E3 表述同源：文档里自称「唯一预算来源」的行必须指向唯一源文件（出现第二个自称
     唯一来源 ⇒ 红）。
  E4 状态词：分母登记 status 必须落在 ASTROCS_DESIGN.md §12.5 阶梯内。

用法
  python3 eng/tools/quality/check_budget_single_source.py [--root DIR] [--json-out PATH] [--quiet]
  python3 eng/tools/quality/check_budget_single_source.py --self-test
  python3 eng/tools/quality/check_budget_single_source.py --fault-inject <literal|tone|source|all>
退出码 0 = 全绿；1 = 至少一条判红；2 = 用法/夹具错误。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
try:
    from check_conclusion_truth import parse_ladder  # noqa: E402
except Exception:  # noqa: BLE001
    parse_ladder = None

REG = "eng/tools/quality/budget_sources.json"
DESIGN = "ASTROCS_DESIGN.md"


def _read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def _load(root, rel):
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        return None, ["registry 缺失: %s（fail-closed）" % rel]
    try:
        return json.loads(_read(p)), []
    except Exception as exc:  # noqa: BLE001
        return None, ["registry 不可解析: %s: %s" % (rel, exc)]


def _dig(doc, dotted):
    cur = doc
    for tok in dotted.split("."):
        if not isinstance(cur, dict) or tok not in cur:
            return None
        cur = cur[tok]
    return cur


def sched_budget(total_ram_bytes, percent):
    """调度器字节预算 = 当前可用内存 × percent/100（与 runtime_resources.json 的
    memory_budget_percent 语义、lib/infrastructure/scheduler/src/memory_budget.cpp 同式）。"""
    return float(total_ram_bytes) * float(percent) / 100.0


def frame_cap(avail_bytes, pixels, bytes_per_px, safety_frac):
    """帧级并发上限 = floor(可用内存 × 安全系数 / (帧像素数 × 每像素字节))，下限 1。

    与生产实现同式（lib/infrastructure/scheduler/src/module_adapters.cpp 的 p1_memory_cap）；
    本门用它做**结论的敏感性实测**，而不是把结论与自己比。"""
    denom = pixels * bytes_per_px
    if denom <= 0:
        return 1
    return max(1, int(math.floor(avail_bytes * safety_frac / denom)))


def run(root):
    fails = []
    reg, rf = _load(root, REG)
    fails += rf
    if reg is None:
        return {"tool": "check_budget_single_source", "pass": False, "fails": fails}
    src_rel = reg["single_source"]["path"]
    src_path = os.path.join(root, src_rel)
    if not os.path.isfile(src_path):
        fails.append("唯一预算源缺失: %s（fail-closed）" % src_rel)
        return {"tool": "check_budget_single_source", "pass": False, "fails": fails}
    try:
        src = json.loads(_read(src_path))
    except Exception as exc:  # noqa: BLE001
        fails.append("唯一预算源不可解析: %s: %s" % (src_rel, exc))
        return {"tool": "check_budget_single_source", "pass": False, "fails": fails}
    ladder, lf = ([], ["状态阶梯解析器不可用（parse_ladder 缺位）"])
    if parse_ladder is not None:
        ladder, lf = parse_ladder(root)
    fails += lf
    # E1 源唯一 + 实现侧字面量逐位相等
    for d in reg.get("denominators", []):
        did = d["id"]
        val = _dig(src, d["source_key"])
        if val is None:
            fails.append("%s: 分母 %s 未定义在唯一源 %s" % (did, d["source_key"], src_rel))
            continue
        if not isinstance(val, (int, float)) or (isinstance(val, (int, float)) and val <= 0):
            fails.append("%s: 源取值非法: %r" % (did, val))
            continue
        for c in d.get("consumers", []):
            p = os.path.join(root, c["file"])
            if not os.path.isfile(p):
                fails.append("%s: 实现侧 %s 不存在" % (did, c["file"]))
                continue
            text = _read(p)
            if c.get("must_reference") and c["must_reference"] not in text:
                fails.append("%s: 实现侧 %s 未引用 %s（未接线到唯一源）"
                             % (did, c["file"], c["must_reference"]))
            rx = c.get("literal_regex")
            if rx:
                m = re.search(rx, text)
                if m is None:
                    fails.append("%s: 实现侧 %s 找不到字面量锚 %s" % (did, c["file"], rx))
                    continue
                got = float(m.group(1))
                if abs(got - float(val)) > 1e-12:
                    fails.append("%s: 两个分母并存 —— 源 %s=%r 而实现侧 %s 写 %r"
                                 % (did, d["source_key"], val, c["file"], got))
    # E2 有判别力（敏感性实测）：结论的输入**从唯一源取值**，再扰动本分母
    probed = 0
    for d in reg.get("denominators", []):
        c = d.get("conclusion")
        if not c:
            fails.append("%s: 未接入任何结论 ⇒ 无判别力（构造恒真形态）" % d["id"])
            continue
        bind = c.get("bind") or {}
        params = [p for p, k in bind.items() if k == d["source_key"]]
        if not params:
            fails.append("%s: 分母 %s 未出现在结论 bind 里 ⇒ 对结论无影响"
                         % (d["id"], d["source_key"]))
            continue
        base = dict(c.get("inputs") or {})
        missing = False
        for p, k in bind.items():
            v = _dig(src, k)
            if v is None:
                fails.append("%s: 结论输入 %s 缺源取值（分母未定义在唯一源）" % (d["id"], k))
                missing = True
            else:
                base[p] = float(v)
        if missing:
            continue
        kind = c["kind"]
        fn = {"frame_cap": lambda b: frame_cap(b["avail_bytes"], b["pixels"],
                                               b["bytes_per_pixel"], b["safety_frac"]),
              "sched_budget": lambda b: sched_budget(b["total_ram_bytes"], b["percent"])}[kind]
        concl = fn(base)
        delta = float(d.get("perturb", 0.2))
        hits = []
        for sign in (-1.0, 1.0):
            alt = dict(base)
            alt[params[0]] = base[params[0]] * (1.0 + sign * delta)
            hits.append(fn(alt))
        probed += 1
        if all(h == concl for h in hits):
            fails.append("%s: 扰动该分母后结论恒为 %r ⇒ 无判别力（构造恒真形态）"
                         % (d["id"], concl))
    if not probed:
        fails.append("没有任何分母接入结论 ⇒ 无法证明判别力（fail-closed）")
    # E3 表述同源
    mark = (reg.get("single_source") or {}).get("claim_marker")
    if mark:
        for base in ("docs", "lib", "eng"):
            for dirpath, dirnames, filenames in os.walk(os.path.join(root, base)):
                dirnames[:] = [d for d in dirnames if d not in ("__pycache__", ".git")]
                for fn in filenames:
                    if not fn.endswith((".md", ".txt")):
                        continue
                    full = os.path.join(dirpath, fn)
                    try:
                        text = _read(full)
                    except Exception:  # noqa: BLE001
                        continue
                    for ln, line in enumerate(text.splitlines(), 1):
                        if mark in line and src_rel not in line:
                            fails.append("自称唯一预算来源却未指向 %s: %s:%d"
                                         % (src_rel, os.path.relpath(full, root), ln))
    # E4 状态词
    for d in reg.get("denominators", []):
        if ladder and d.get("status") not in ladder:
            fails.append("%s: status=%r 越出 §12.5 阶梯 %s" % (d["id"], d.get("status"), ladder))
    return {"tool": "check_budget_single_source", "root": os.path.abspath(root),
            "pass": not fails, "fails": fails}


MINI_DESIGN = """# d

### 12.5 状态阶梯（唯一口径）

| 状态 | 语义 |
|---|---|
| CONTRACT_READY | a |
| IMPLEMENTED | b |
| INSTALLED | c |
| VERIFIED | d |
| READY_FOR_OWNER_REVIEW | e |
| NOT_IMPLEMENTED / NOT_VERIFIED / DEFERRED / DORMANT / FAIL | 负向 |

---
"""


def _mini(root, *, src_val=116.0, impl_val=116.0, safety=0.75, status="IMPLEMENTED"):
    os.makedirs(os.path.join(root, "eng/tools/quality"), exist_ok=True)
    os.makedirs(os.path.join(root, "lib/sched"), exist_ok=True)
    os.makedirs(os.path.join(root, "docs"), exist_ok=True)
    with open(os.path.join(root, DESIGN), "w", encoding="utf-8") as f:
        f.write(MINI_DESIGN)
    os.makedirs(os.path.join(root, "eng/packaging/config"), exist_ok=True)
    with open(os.path.join(root, "eng/packaging/config/runtime_resources.json"), "w") as f:
        json.dump({"memory_budget_percent": 95,
                   "frame_memory_gate": {"bytes_per_pixel": src_val, "safety_frac": safety}}, f)
    with open(os.path.join(root, "lib/sched/module_adapters.cpp"), "w", encoding="utf-8") as f:
        f.write("static constexpr double kP1FrameBytesPerPixel = %r;\n" % impl_val)
    with open(os.path.join(root, "docs/KNOWN_LIMITATIONS.md"), "w", encoding="utf-8") as f:
        f.write("生产唯一预算来源 = eng/packaging/config/runtime_resources.json\n")
    reg = {"schema_version": 1,
           "single_source": {"path": "eng/packaging/config/runtime_resources.json",
                             "claim_marker": "唯一预算来源"},
           "denominators": [
               {"id": "bpp", "source_key": "frame_memory_gate.bytes_per_pixel",
                "perturb": 0.2,
                "conclusion": {"kind": "frame_cap",
                               "inputs": {"avail_bytes": 20.77e9, "pixels": 4096.0 * 4096.0},
                               "bind": {"bytes_per_pixel": "frame_memory_gate.bytes_per_pixel",
                                        "safety_frac": "frame_memory_gate.safety_frac"}},
                "consumers": [{"file": "lib/sched/module_adapters.cpp",
                               "literal_regex":
                               r"kP1FrameBytesPerPixel\s*=\s*([0-9.]+)"}],
                "status": status},
               {"id": "safety", "source_key": "frame_memory_gate.safety_frac",
                "perturb": 0.2,
                "conclusion": {"kind": "frame_cap",
                               "inputs": {"avail_bytes": 20.77e9, "pixels": 4096.0 * 4096.0},
                               "bind": {"bytes_per_pixel": "frame_memory_gate.bytes_per_pixel",
                                        "safety_frac": "frame_memory_gate.safety_frac"}},
                "consumers": [], "status": status}]}
    with open(os.path.join(root, REG), "w", encoding="utf-8") as f:
        json.dump(reg, f)
    return reg


def self_test():
    cases = []
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "p")
        _mini(p)
        cases.append(("pos-single-source", run(p)["pass"], True))
        n1 = os.path.join(td, "n1")
        _mini(n1, src_val=116.0, impl_val=200.0)
        cases.append(("neg-two-denominators", run(n1)["pass"], False))
        n2 = os.path.join(td, "n2")
        reg = _mini(n2, safety=0.0001)
        cases.append(("neg-constant-conclusion", run(n2)["pass"], False))
        n3 = os.path.join(td, "n3")
        _mini(n3)
        with open(os.path.join(n3, "docs/other.md"), "w", encoding="utf-8") as f:
            f.write("生产唯一预算来源 = lib/sched/budget.h\n")
        cases.append(("neg-second-claim", run(n3)["pass"], False))
        n4 = os.path.join(td, "n4")
        _mini(n4, status="SHIPPED")
        cases.append(("neg-status-out-of-ladder", run(n4)["pass"], False))
    ok = all(got is want for _, got, want in cases)
    for name, got, want in cases:
        print("SELFTEST_%s %s (pass=%s want=%s)" % ("PASS" if got is want else "FAIL",
                                                    name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


def fault_inject(name, root):
    names = ("literal", "tone", "source")
    if name == "all":
        rc = 0
        for nm in names:
            rc |= fault_inject(nm, root)
        print("FAULT_INJECT_ALL %s" % ("PASS" if rc == 0 else "FAIL"))
        return rc
    if name not in names:
        print("unknown fault: %s" % name, file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as td:
        dst = os.path.join(td, "repo")
        reg = json.loads(_read(os.path.join(root, REG)))
        rels = [REG, DESIGN, reg["single_source"]["path"]]
        for d in reg.get("denominators", []):
            rels += [c["file"] for c in d.get("consumers", [])]
        for rel in sorted(set(rels)):
            src = os.path.join(root, rel)
            if os.path.isfile(src):
                d = os.path.join(dst, rel)
                os.makedirs(os.path.dirname(d), exist_ok=True)
                shutil.copy(src, d)
        if name == "literal":
            # 只在实现侧改动字面量（源不动）⇒ 两个分母并存，必须红
            touched = 0
            for d in reg.get("denominators", []):
                for c in d.get("consumers", []):
                    rx = c.get("literal_regex")
                    fp = os.path.join(dst, c["file"])
                    if not rx or not os.path.isfile(fp):
                        continue
                    t = _read(fp)
                    new = re.sub(rx, lambda m: m.group(0).replace(m.group(1), "200.0"), t)
                    if new != t:
                        with open(fp, "w", encoding="utf-8") as f:
                            f.write(new)
                        touched += 1
            if touched == 0:
                print("fault 'literal' 未命中任何实现侧字面量", file=sys.stderr)
                return 2
        elif name == "tone":
            # 把分母压到结论恒为下限 1 的区间（结论对分母不再敏感 = 构造恒真形态）
            p = os.path.join(dst, reg["single_source"]["path"])
            doc = json.loads(_read(p))
            doc["frame_memory_gate"]["safety_frac"] = 2e-06
            with open(p, "w", encoding="utf-8") as f:
                json.dump(doc, f)
            t = _read(os.path.join(dst, reg["denominators"][0]["consumers"][0]["file"]))
            t = re.sub(r"(kP1FrameBytesPerPixel\s*=\s*)([0-9.]+)",
                       lambda m: m.group(1) + "0.002", t)
            with open(os.path.join(dst, reg["denominators"][0]["consumers"][0]["file"]),
                      "w", encoding="utf-8") as f:
                f.write(t)
        elif name == "source":
            p = os.path.join(dst, reg["single_source"]["path"])
            doc = json.loads(_read(p))
            doc.pop("frame_memory_gate", None)
            with open(p, "w", encoding="utf-8") as f:
                json.dump(doc, f)
        res = run(dst)
        print("FAULT_INJECT %s red=%d" % (name, len(res["fails"])))
        for m in res["fails"][:3]:
            print("    %s" % m)
        return 0 if res["fails"] else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="预算源唯一 + 有判别力门（S2-E）")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--fault-inject", default="")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if a.fault_inject:
        return fault_inject(a.fault_inject, a.root)
    res = run(a.root)
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(a.json_out, "w", encoding="utf-8") as f:
            json.dump(res, f, ensure_ascii=False, indent=1, sort_keys=True)
        print("REPORT_WRITTEN %s" % a.json_out)
    if a.quiet:
        print("BUDGET_SINGLE_SOURCE %s fails=%d" % ("PASS" if res["pass"] else "FAIL",
                                                   len(res["fails"])))
        for m in res["fails"][:8]:
            print("  - %s" % m)
    else:
        print(json.dumps(res, ensure_ascii=False, indent=1, sort_keys=True))
    return 0 if res["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
