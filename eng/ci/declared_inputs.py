#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/declared_inputs.py —— 注册项「声明输入面」的单一实现点（GATE-SOLID-01）。

问题（独立审查节点一页纸 S2-A「空扫描恒真通过」）
  注册项的**扫描对象**（输入文件 / 目录）从未在注册表里被声明 ⇒「命令的扫描对象
  路径不存在」与「扫描面为空」都不产生任何判定：命令 rc=0 即记 PASS，门在空面上
  恒真通过。审查给的原话是「步记 rc=0 而其扫描对象路径不存在」。

判据（S2-A「判完成」第 4 条）
  **步内声明的输入路径不存在即红**；扫描面为空同样判红（真值无效应必须判红，
  不许判绿）；「这台机器上本就不该有的可选产物」记 skip 而非 pass。

字段语义（eng/ci/checks.schema.json 同步定义，注册项与 step 共有）
  inputs          必须存在的输入；缺失 ⇒ inputs_missing，为空面 ⇒ inputs_empty
  optional_inputs 可选产物；**全部**不可用时记 optional_input_absent（skip，不是 pass），
                  任一可用则照常执行（缺失项由 report() 逐条留痕）

形态约定
  以 '/' 结尾      ⇒ 扫描面目录：必须存在且递归文件数 > 0
  含 * ? [         ⇒ glob 展开：0 命中 = missing；命中集合递归文件数 0 = empty
  其余             ⇒ 文件或目录：不存在 = missing；目录须递归非空

本模块被 eng/ci/run_checks.py 与 eng/ci/run.py 共用（两入口对同一情形的判定必须
一致，W4-A3）；只读仓库，不做任何写操作。
"""
from __future__ import annotations

import pathlib

STATE_OK = "ok"
STATE_MISSING = "missing"
STATE_EMPTY = "empty"

GAP_MISSING = "inputs_missing"
GAP_EMPTY = "inputs_empty"
GAP_OPTIONAL_ABSENT = "optional_input_absent"


def probe(repo: pathlib.Path, rel: str) -> tuple:
    """单个声明输入的可用性：返回 (state, detail)，state ∈ {ok, missing, empty}。"""
    rel = str(rel)
    if any(ch in rel for ch in "*?["):
        hits = sorted(p for p in repo.glob(rel) if p.exists())
        if not hits:
            return (STATE_MISSING, f"{rel}（glob 展开 0 命中）")
        files = sum(1 for p in hits if p.is_file())
        files += sum(1 for p in hits if p.is_dir()
                     for _ in p.rglob("*") if _.is_file())
        if files == 0:
            return (STATE_EMPTY, f"{rel}（glob 命中 {len(hits)} 项但 0 个文件）")
        return (STATE_OK, f"{rel}（glob 命中 {len(hits)} 项 / {files} 个文件）")
    target = repo / rel
    if not target.exists():
        return (STATE_MISSING, rel)
    if target.is_dir():
        files = sum(1 for _ in target.rglob("*") if _.is_file())
        if files == 0:
            return (STATE_EMPTY, f"{rel}（目录存在但 0 个文件：扫描面为空）")
        return (STATE_OK, f"{rel}（目录 / {files} 个文件）")
    return (STATE_OK, rel)


def declared_lists(step: dict) -> tuple:
    """(required, optional) 两个路径列表（字段缺失即空列表）。"""
    required = [str(x) for x in (step.get("inputs") or [])]
    optional = [str(x) for x in (step.get("optional_inputs") or [])]
    return required, optional


def gap(step: dict, repo: pathlib.Path):
    """执行前判定：返回 (kind, reason) 或 None（None = 可执行）。

    kind ∈ {GAP_MISSING, GAP_EMPTY, GAP_OPTIONAL_ABSENT}。调用方自行映射到
    自己的 verdict 常量（两个 runner 的常量名不同，语义必须一致）。
    本函数只增加红/跳过，不减少任何既有判定面：未声明两个字段的 step 行为不变。
    """
    required, optional = declared_lists(step)
    states = [(rel,) + probe(repo, rel) for rel in required]
    missing = [d for _rel, st, d in states if st == STATE_MISSING]
    if missing:
        return (GAP_MISSING,
                "声明的输入路径不存在（fail-closed，S2-A：输入不存在即红）："
                + "；".join(missing))
    empty = [d for _rel, st, d in states if st == STATE_EMPTY]
    if empty:
        return (GAP_EMPTY,
                "声明的扫描面为空（真值无效应必须判红，不得判绿）：" + "；".join(empty))
    if not required and optional:
        opt_states = [(rel,) + probe(repo, rel) for rel in optional]
        if all(s[1] != STATE_OK for s in opt_states):
            return (GAP_OPTIONAL_ABSENT,
                    "optional_inputs 全部不可用（本机不该有的可选产物，记 skip 非 pass）："
                    + "；".join(f"{rel}={st}" for rel, st, _ in opt_states))
    return None


def report(step: dict, repo: pathlib.Path) -> dict:
    """声明输入面的逐条状态（进 per-step 结果，供复核者核对"到底扫了什么"）。"""
    required, optional = declared_lists(step)
    out = {"inputs": [], "optional_inputs": []}
    for rel in required:
        state, detail = probe(repo, rel)
        out["inputs"].append({"path": rel, "state": state, "detail": detail})
    for rel in optional:
        state, detail = probe(repo, rel)
        out["optional_inputs"].append({"path": rel, "state": state, "detail": detail})
    return out
