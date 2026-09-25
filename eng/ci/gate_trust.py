#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GATE-TRUST-01 共用设施：门的三态（PASS/FAIL/CRASH）与判别力自证面。

权威依据
  - docs/ci/CI_SPEC.md §4（红灯不豁免、只有负责人可批豁免）、§7（失败必须留可复现证据）；
  - ENGINEERING_SPEC.md §8（每项能绿能红）、§10（fail-closed）；
  - AGENTS.md §9（门禁判据本身不合理时改进门禁本身，配可执行正例/负例与 --self-test，
    而不是放松判据）；
  - 独立审查节点一页纸 S1 第 1 条的「判完成」：门输出 PASS/FAIL/CRASH 三态、
    崩溃用独立退出码、判词带文件名与行号且不截断；每道门带「注入已知违规必判红」
    的负例，且**至少一条负例落在该门自身的豁免分支内**。

为什么需要本模块（根因，不是风格偏好）
  「门的判别力坏了」与「被判对象不合规」是两件不同的事。旧实现把两者都塌成
  rc=1 与一行 `SELFTEST_FAIL`，于是：
    * 消费者分不清「这道门还能不能信」与「被它判的对象真不合规」；
    * 门自检里一旦混进一条「真实仓库必须绿」的内容断言，该断言变红就会被读成
      「门自检不绿 ⇒ 门不可信」，把一道有牙的门误判成废门，或者反过来，
      用「门不可信」把真红掩盖成噪声。
  本模块把两条轴拆成**独立退出码**（与 eng/ci/run_checks.py 的退出码同号）：
    rc=0  PASS ：判别力面全绿且对象面全绿；
    rc=1  FAIL ：判别力面全绿（门有牙），**被判对象不合规**（内容红，逐条带文件:行）；
    rc=2  runner 配置/环境错误（保留号，本模块不返回）；
    rc=3  CRASH：**门自身不可信**——注入的已知违规没有判红、保护性正例被破坏、
          或本门根本没登记「落在自身豁免分支内」的负例。此时它给的红与绿都不具
          证据资格，必须与「内容红」区分开。
"""
from __future__ import annotations

import io
import json
import os
import sys

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_RUNNER_ERROR = 2
EXIT_TRUST = 3

# ── 用例种类 ────────────────────────────────────────────────────────────────
# discriminating：注入一条**已知违规** ⇒ 判定必须判红。它测的是「门有没有牙」。
KIND_DISCRIMINATING = "discriminating"
# protective：保护性正例 ⇒ 合规输入不得被误伤（防「一律判红」的假门）。
KIND_PROTECTIVE = "protective"
# exemption：负例**落在本门自身的豁免分支内**——即注入一条「看上去会被本门的
#   豁免/放行/白名单/降级规则放过」的违规，断言它**仍然判红**。只验正例会漏掉
#   「豁免面把该红的东西放过去了」这种失效；缺这一条的门按不可信处理。
KIND_EXEMPTION = "exemption"
# content：被判对象（真实仓库 / 真实册子 / 真实产物）必须合规。它是**内容断言**，
#   不是判别力断言；它变红说明对象错，不说明门错。
KIND_CONTENT = "content"

TRUST_KINDS = (KIND_DISCRIMINATING, KIND_PROTECTIVE, KIND_EXEMPTION)
ALL_KINDS = TRUST_KINDS + (KIND_CONTENT,)

VERDICT_PASS = "PASS"
VERDICT_FAIL = "FAIL"
VERDICT_CRASH = "CRASH"


class Case:
    """一条自检用例。`detail` 是判词：带文件名与行号，**不截断**。"""

    __slots__ = ("name", "ok", "kind", "detail")

    def __init__(self, name, ok, kind=KIND_DISCRIMINATING, detail=""):
        if kind not in ALL_KINDS:
            raise ValueError("unknown case kind: %r" % (kind,))
        self.name = str(name)
        self.ok = bool(ok)
        self.kind = kind
        self.detail = "" if detail is None else str(detail)

    def as_dict(self):
        return {"case": self.name, "ok": self.ok, "kind": self.kind,
                "detail": self.detail}


def as_case(item):
    if isinstance(item, Case):
        return item
    if isinstance(item, dict):
        return Case(item["case"], item["ok"], item.get("kind", KIND_DISCRIMINATING),
                    item.get("detail", ""))
    name, ok = item[0], item[1]
    kind = item[2] if len(item) > 2 else KIND_DISCRIMINATING
    detail = item[3] if len(item) > 3 else ""
    return Case(name, ok, kind, detail)


def emit(cases, *, tool, json_out=None, stream=None, extra=None,
         require_exemption=True) -> int:
    """打印并落盘自检结果；返回三态退出码（0 PASS / 1 FAIL / 3 CRASH）。

    `cases`：Case 或 (name, ok[, kind[, detail]]) 序列。
    `require_exemption`：为 True 时，缺 KIND_EXEMPTION 用例 ⇒ 判 CRASH（门不可信）。
    判词**逐条全量打印，不做任何截断**：截断会让读者无法定位被放宽的判据。
    """
    stream = stream or sys.stdout
    items = [as_case(c) for c in cases]
    trust_bad = [c for c in items if not c.ok and c.kind in TRUST_KINDS]
    content_bad = [c for c in items if not c.ok and c.kind == KIND_CONTENT]
    has_exemption = any(c.kind == KIND_EXEMPTION for c in items)

    for c in items:
        stream.write("SELFTEST %s %-9s %s%s\n" % (
            "PASS" if c.ok else "FAIL", c.kind, c.name,
            ("\n    " + c.detail.replace("\n", "\n    ")) if (c.detail and not c.ok) else ""))

    trust_reasons = []
    if trust_bad:
        trust_reasons.append("注入的已知违规没有判红 / 保护性正例被破坏："
                             + repr([c.name for c in trust_bad]))
    if require_exemption and not has_exemption:
        trust_reasons.append(
            "本门未登记「落在自身豁免分支内」的负例（kind=%s）：只验正例会漏掉"
            "「豁免面把该红的东西放过去了」这种失效" % KIND_EXEMPTION)

    if trust_reasons:
        verdict, rc = VERDICT_CRASH, EXIT_TRUST
        stream.write("GATE_TRUST_FAIL: 门自身不可信（红绿都不具证据资格）：%s\n"
                     % "；".join(trust_reasons))
    elif content_bad:
        verdict, rc = VERDICT_FAIL, EXIT_FAIL
        stream.write("CONTENT_FAIL: 门有牙（判别力面 %d/%d 绿），被判对象不合规，"
                     "共 %d 条（逐条列全，不截断）：\n"
                     % (len([c for c in items if c.kind in TRUST_KINDS and c.ok]),
                        len([c for c in items if c.kind in TRUST_KINDS]),
                        len(content_bad)))
        for c in content_bad:
            stream.write("  %s: %s\n" % (c.name, c.detail))
    else:
        verdict, rc = VERDICT_PASS, EXIT_PASS
        stream.write("SELFTEST_PASS: %d/%d（判别力面 %d 条、豁免分支负例 %d 条）\n"
                     % (len(items), len(items),
                        len([c for c in items if c.kind in TRUST_KINDS]),
                        len([c for c in items if c.kind == KIND_EXEMPTION])))
    stream.flush()

    if json_out:
        doc = {"tool": tool, "mode": "self-test", "verdict": verdict, "rc": rc,
               "cases": [c.as_dict() for c in items],
               "trust_failures": [c.name for c in trust_bad],
               "content_failures": [c.name for c in content_bad],
               "exemption_cases": [c.name for c in items if c.kind == KIND_EXEMPTION]}
        if extra:
            doc.update(extra)
        parent = os.path.dirname(json_out)
        if parent:
            os.makedirs(parent, exist_ok=True)
        io.open(json_out, "w", encoding="utf-8").write(
            json.dumps(doc, ensure_ascii=False, indent=1) + "\n")
    return rc


def report_findings(prefix, findings, stream=None):
    """逐条全量打印判定结果（不截断）并返回行数。

    判词必须带文件名与行号：调用方负责让每条 finding 自身含 `文件:行`；
    本函数只保证「不截断、不吞条数」。
    """
    stream = stream or sys.stdout
    items = list(findings)
    stream.write("%s: %d finding(s)\n" % (prefix, len(items)))
    for item in items:
        stream.write("  - %s\n" % (item if isinstance(item, str)
                                    else json.dumps(item, ensure_ascii=False)))
    stream.flush()
    return len(items)
