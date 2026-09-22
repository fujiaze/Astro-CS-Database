#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EVT-FIELD-SETS：事件协议 per-kind 冻结扩展字段集的跨面一致门（fail-closed）。

权威链（AGENTS.md §1.1「先到最高文档确定规范」）：
  * docs/api/CLI_PROTOCOL_V1.md §4：运行事件流的**唯一 schema = 实现正本**
    lib/infrastructure/cli/protocol.h（ValidateEventV1 发送侧硬闸）+ jsonl.h；
    机器 schema eng/contracts/schemas/jsonl_event_v1.schema.json 是**派生件**，
    「不得自成第二份定义」；
  * docs/contracts/LOG_AND_ERROR_CONTRACT.md §1：LOG-004 合同**不冻结**运行事件流
    （protocol.h/jsonl.h 各有一份正本）⇒ 事件流字段集的规范依据在 CLI_PROTOCOL_V1 §4；
  * docs/ci/01_CHECKS.md §1 + ENGINEERING_SPEC.md §8：fail-closed（输入缺失/路径不存在
    必须判红，不得把「解析不到」当「无违规」）、锚存活（ANCHOR_STALE: <面> <路径>
    显式失败并点名）、每项检查必须能绿能红（机器可执行负例入口 --self-test）。

判据（正本 = protocol.h::missing_required_extension_v1 的 kExt 表）：
  C1 逐 kind 扩展字段集**集合相等**（顺序无关），差异**逐 kind 逐字段**点名（缺/多/改名）；
  C2 kind 集合各面一致（多一个/少一个 kind 同样判红，防止"按正本键集单向比对"漏掉多余分支）；
  C3 正本自洽：kExt 键集 == 同文件 registered_event_kinds_v1() 名册（正本内部两处同面）；
  C4 派生件自认：schema 的 x-astrocs-event-kind-registry.implementation_authority
     必须指向 protocol.h（防派生件自成第二份定义）；
  C5 fail-closed：任一面文件缺失/块缺失/形态漂移/解析面退化（零 kind、分支缺 then.required、
     读侧字段集非字面量）⇒ ANCHOR_STALE 点名 + rc=2，**不得静默跳过**——静默跳过正是
     本缺口（per-kind 字段集没有机器门）能潜伏的原因。

面（**全部从各面源码解析，本检查器内不手抄任何字段清单**，否则只是把手工面变成第五个）：
  A protocol_h_kext      正本：lib/infrastructure/cli/protocol.h 的 kExt
  B schema_then_required 派生：jsonl_event_v1.schema.json 的 allOf[].then.required
  C schema_x_registry    派生：同文件 x-astrocs-event-kind-registry.kinds
  D readside_cli004      读侧对偶：eng/tests/cli/test_cli004_process_protocol.py 的 KIND_EXT
  E readside_fix208      读侧对偶：eng/tests/cli/test_fix208_event_stream_default.py 的 KIND_EXT

用法：
  python3 eng/ci/check_event_field_sets.py [--repo-root .] [--json-out <file>] [--self-test]
exit 0 = 五面逐 kind 字段集一致；exit 1 = 判红（不一致，逐项点名）；
exit 2 = 输入不可用 / 锚失效（ANCHOR_STALE，fail-closed）。
"""
from __future__ import annotations

import argparse
import ast
import copy
import json
import os
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

# 面 → 仓库相对路径（只登记路径，**不登记字段清单**）。
FACE_PATH = {
    "protocol_h_kext": "lib/infrastructure/cli/protocol.h",
    "schema_then_required": "eng/contracts/schemas/jsonl_event_v1.schema.json",
    "schema_x_registry": "eng/contracts/schemas/jsonl_event_v1.schema.json",
    "readside_cli004": "eng/tests/cli/test_cli004_process_protocol.py",
    "readside_fix208": "eng/tests/cli/test_fix208_event_stream_default.py",
}
AUTHORITY_FACE = "protocol_h_kext"          # 正本
FIELD_FACES = ("protocol_h_kext", "schema_then_required", "schema_x_registry",
               "readside_cli004", "readside_fix208")
PROTOCOL_H = "lib/infrastructure/cli/protocol.h"
SCHEMA = "eng/contracts/schemas/jsonl_event_v1.schema.json"


class AnchorStale(Exception):
    """锚失效 / 解析不到 —— fail-closed，不得静默跳过。"""

    def __init__(self, face: str, detail: str):
        super().__init__("ANCHOR_STALE: %s %s" % (face, detail))
        self.face = face
        self.detail = detail


# ---------------------------------------------------------------- 通用解析原语

def mask_cpp(text: str) -> str:
    """把 C/C++ 注释与字符串字面量抹成同长空格（保留偏移），用于结构定位。"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(" " * (j - i))
            i = j
        elif c == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    break
                j += 1
            j = min(j + 1, n)
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def match_brace(masked: str, open_i: int, face: str) -> int:
    """返回 masked[open_i] == '{' 的配对 '}' 下标；未配对 ⇒ ANCHOR_STALE。"""
    if open_i >= len(masked) or masked[open_i] != "{":
        raise AnchorStale(face, "内部错误：位置 %d 不是 '{'" % open_i)
    depth = 0
    for i in range(open_i, len(masked)):
        if masked[i] == "{":
            depth += 1
        elif masked[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    raise AnchorStale(face, "位置 %d 的 '{' 未配对（源码块被截断？）" % open_i)


def parse_protocol_h_kext(text: str) -> dict:
    """正本：解析 missing_required_extension_v1 内的 kExt 静态表。"""
    face = "protocol_h_kext"
    masked = mask_cpp(text)
    m = re.search(r"\bkExt\s*=\s*\{", masked)
    if not m:
        raise AnchorStale(face, "%s::missing_required_extension_v1 未找到 'kExt = {' 静态表" % PROTOCOL_H)
    open_i = masked.index("{", m.start())
    close_i = match_brace(masked, open_i, face)
    body_raw, body_msk = text[open_i + 1:close_i], masked[open_i + 1:close_i]

    entries: dict = {}
    i = 0
    while True:
        j = body_msk.find("{", i)
        if j < 0:
            break
        k = match_brace(body_msk, j, face)
        chunk_raw, chunk_msk = body_raw[j:k + 1], body_msk[j:k + 1]
        # 形态用 raw 匹配（masked 里字符串已被抹成空格），括号配对仍用 masked（同长同偏移）。
        mm = re.match(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*\{', chunk_raw)
        if not mm:
            raise AnchorStale(face, "kExt 第 %d 个条目形态不认识：%r" % (len(entries) + 1, chunk_raw[:70]))
        kind = mm.group(1)
        io = mm.end() - 1                      # 字段集 '{'
        ic = match_brace(chunk_msk, io, face)
        fields = set(re.findall(r'"([A-Za-z0-9_]+)"', chunk_raw[io + 1:ic]))
        if kind in entries:
            raise AnchorStale(face, "kExt 重复 kind %r" % kind)
        entries[kind] = fields
        i = k + 1
    if not entries:
        raise AnchorStale(face, "kExt 解析到 0 个 kind（退化，fail-closed 拒判绿）")
    return entries


def parse_protocol_h_kind_registry(text: str) -> set:
    """正本：解析 registered_event_kinds_v1() 的 kind 名册（C3 用）。"""
    face = "protocol_h_kinds"
    masked = mask_cpp(text)
    m = re.search(r"registered_event_kinds_v1\s*\([^)]*\)\s*\{", masked)
    if not m:
        raise AnchorStale(face, "%s::registered_event_kinds_v1 定义未找到" % PROTOCOL_H)
    open_i = masked.index("{", m.end() - 1)
    close_i = match_brace(masked, open_i, face)
    body_raw, body_msk = text[open_i + 1:close_i], masked[open_i + 1:close_i]
    eq = re.search(r"=\s*\{", body_msk)
    if not eq:
        raise AnchorStale(face, "registered_event_kinds_v1 内未找到名册初始化块 '= {'")
    io = body_msk.index("{", eq.start())
    ic = match_brace(body_msk, io, face)
    kinds = set(re.findall(r'"([A-Za-z0-9_]+)"', body_raw[io + 1:ic]))
    if not kinds:
        raise AnchorStale(face, "registered_event_kinds_v1 解析到 0 个 kind（退化）")
    return kinds


def parse_schema(text: str):
    """派生件：解析 kind enum / allOf[].then.required / x-astrocs-event-kind-registry。"""
    try:
        data = json.loads(text)
    except Exception as exc:  # noqa: BLE001
        raise AnchorStale("schema_then_required", "%s 不可解析：%s" % (SCHEMA, exc)) from exc
    if not isinstance(data, dict):
        raise AnchorStale("schema_then_required", "%s 顶层不是对象" % SCHEMA)

    try:
        enum = data["properties"]["kind"]["enum"]
    except Exception:  # noqa: BLE001
        raise AnchorStale("schema_kind_enum", "%s 缺 properties.kind.enum" % SCHEMA)
    if not isinstance(enum, list) or not enum or not all(isinstance(x, str) for x in enum):
        raise AnchorStale("schema_kind_enum", "properties.kind.enum 形态非法：%r" % (enum,))

    allof = data.get("allOf")
    if not isinstance(allof, list) or not allof:
        raise AnchorStale("schema_then_required", "%s 缺 allOf 分支（kind 扩展字段无判据面）" % SCHEMA)
    then_req: dict = {}
    for idx, br in enumerate(allof):
        if not isinstance(br, dict):
            raise AnchorStale("schema_then_required", "allOf[%d] 不是对象" % idx)
        kind = br.get("if", {}).get("properties", {}).get("kind", {}).get("const")
        if not isinstance(kind, str):
            raise AnchorStale("schema_then_required",
                              "allOf[%d] 不是 'if.properties.kind.const' 形态（派生件形态漂移）" % idx)
        req = br.get("then", {}).get("required")
        if not isinstance(req, list) or not all(isinstance(x, str) for x in req):
            raise AnchorStale("schema_then_required",
                              "allOf[%d] (%s) 缺 then.required 字符串数组" % (idx, kind))
        if kind in then_req:
            raise AnchorStale("schema_then_required", "allOf 重复 kind 分支 %r" % kind)
        then_req[kind] = set(req)

    xreg = data.get("x-astrocs-event-kind-registry")
    if not isinstance(xreg, dict):
        raise AnchorStale("schema_x_registry", "%s 缺 x-astrocs-event-kind-registry" % SCHEMA)
    kinds = xreg.get("kinds")
    if not isinstance(kinds, dict) or not kinds:
        raise AnchorStale("schema_x_registry", "x-astrocs-event-kind-registry.kinds 缺失或为空（退化）")
    x_fields: dict = {}
    for kind, fields in kinds.items():
        if not isinstance(kind, str) or not isinstance(fields, list) or \
                not all(isinstance(x, str) for x in fields):
            raise AnchorStale("schema_x_registry", "kinds[%r] 形态非法：%r" % (kind, fields))
        x_fields[kind] = set(fields)
    authority = xreg.get("implementation_authority")
    return set(enum), then_req, x_fields, authority


def parse_py_kind_ext(text: str, face: str, path: str) -> dict:
    """读侧对偶：用 AST 解析模块顶层 KIND_EXT 字典（集合字面量 / set()）。"""
    try:
        tree = ast.parse(text)
    except SyntaxError as exc:
        raise AnchorStale(face, "%s 语法不可解析：%s" % (path, exc)) from exc
    for node in tree.body:
        if not (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name) and node.targets[0].id == "KIND_EXT"):
            continue
        val = node.value
        if not isinstance(val, ast.Dict):
            raise AnchorStale(face, "%s::KIND_EXT 不是字典字面量（%s）" % (path, type(val).__name__))
        out: dict = {}
        for k, v in zip(val.keys, val.values):
            if not isinstance(k, ast.Constant) or not isinstance(k.value, str):
                raise AnchorStale(face, "%s::KIND_EXT 含非字符串 kind 键" % path)
            if isinstance(v, ast.Set):
                fields = set()
                for e in v.elts:
                    if not isinstance(e, ast.Constant) or not isinstance(e.value, str):
                        raise AnchorStale(face, "%s::KIND_EXT[%r] 含非字符串字段名" % (path, k.value))
                    fields.add(e.value)
            elif (isinstance(v, ast.Call) and isinstance(v.func, ast.Name)
                  and v.func.id == "set" and not v.args):
                fields = set()
            else:
                raise AnchorStale(face, "%s::KIND_EXT[%r] 字段集形态不认识（%s；"
                                        "只接受集合字面量或 set()）" % (path, k.value, type(v).__name__))
            if k.value in out:
                raise AnchorStale(face, "%s::KIND_EXT 重复 kind %r" % (path, k.value))
            out[k.value] = fields
        if not out:
            raise AnchorStale(face, "%s::KIND_EXT 解析到 0 个 kind（退化）" % path)
        return out
    raise AnchorStale(face, "%s 顶层未找到 KIND_EXT 赋值" % path)


# ---------------------------------------------------------------- 收集与比对

def collect(root: pathlib.Path):
    """收集五面 + 正本 kind 名册 + schema enum。返回 (fields, kind_lists, anchors)。"""
    fields: dict = {}
    kind_lists: dict = {}
    anchors: list = []
    cache: dict = {}

    def read(rel: str, face: str = None) -> str:
        if rel not in cache:
            p = root / rel
            if not p.is_file():
                raise AnchorStale(face or rel, "%s 文件不存在（锚失效，fail-closed）" % rel)
            cache[rel] = p.read_text(encoding="utf-8")
        return cache[rel]

    # A 正本（字段集 + kind 名册）
    try:
        src = read(PROTOCOL_H, "protocol_h_kext")
        fields["protocol_h_kext"] = parse_protocol_h_kext(src)
        kind_lists["protocol_h_kinds"] = parse_protocol_h_kind_registry(src)
    except AnchorStale as exc:
        anchors.append(exc)

    # B/C 派生件
    try:
        enum, then_req, x_fields, authority = parse_schema(read(SCHEMA, "schema_then_required"))
        kind_lists["schema_kind_enum"] = enum
        fields["schema_then_required"] = then_req
        fields["schema_x_registry"] = x_fields
        if not isinstance(authority, str) or "protocol.h" not in authority:
            anchors.append(AnchorStale(
                "schema_x_registry",
                "implementation_authority 未指向实现正本 protocol.h（现值 %r）"
                "——派生件不得自成第二份定义" % (authority,)))
    except AnchorStale as exc:
        anchors.append(exc)

    # D/E 读侧对偶
    for face, rel in (("readside_cli004", FACE_PATH["readside_cli004"]),
                      ("readside_fix208", FACE_PATH["readside_fix208"])):
        try:
            fields[face] = parse_py_kind_ext(read(rel, face), face, rel)
        except AnchorStale as exc:
            anchors.append(exc)

    for face, fmap in fields.items():
        kind_lists[face] = set(fmap)
    return fields, kind_lists, anchors


def compare(fields: dict, kind_lists: dict):
    """返回 (kind_set_diffs, field_diffs)：逐 kind 逐字段点名。"""
    kind_set_diffs, field_diffs = [], []
    if AUTHORITY_FACE not in fields:
        return kind_set_diffs, field_diffs          # 正本不可用 ⇒ 由 anchors 判红
    base_kinds = set(fields[AUTHORITY_FACE])
    for face in sorted(kind_lists):
        ks = kind_lists[face]
        missing, extra = sorted(base_kinds - ks), sorted(ks - base_kinds)
        if missing or extra:
            kind_set_diffs.append({"face": face, "missing": missing, "extra": extra})

    all_kinds = set(base_kinds)
    for fmap in fields.values():
        all_kinds |= set(fmap)
    for kind in sorted(all_kinds):
        base = set(fields[AUTHORITY_FACE].get(kind, set()))
        for face in FIELD_FACES:
            if face == AUTHORITY_FACE or face not in fields:
                continue
            if kind not in fields[face]:
                continue                            # kind 缺失已在 kind_set_diffs 点名
            got = fields[face][kind]
            missing, extra = sorted(base - got), sorted(got - base)
            if missing or extra:
                field_diffs.append({"kind": kind, "face": face,
                                    "missing": missing, "extra": extra})
    return kind_set_diffs, field_diffs


def check(root: pathlib.Path):
    """真判定函数：返回 (rc, text, payload)。"""
    fields, kind_lists, anchors = collect(root)
    kind_set_diffs, field_diffs = compare(fields, kind_lists)
    payload = {
        "tool": "check_event_field_sets",
        "authority": "%s::missing_required_extension_v1" % PROTOCOL_H,
        "faces": {face: FACE_PATH[face] for face in FIELD_FACES},
        "face_kinds": {face: sorted(fields.get(face, {})) for face in FIELD_FACES},
        "face_field_counts": {face: {k: len(v) for k, v in sorted(fields.get(face, {}).items())}
                              for face in FIELD_FACES},
        "kind_set_diffs": kind_set_diffs,
        "field_diffs": field_diffs,
        "anchor_errors": ["ANCHOR_STALE: %s %s" % (a.face, a.detail) for a in anchors],
    }
    lines = []
    if anchors:
        lines.append("EVT_FIELD_SETS_FAIL(fail-closed): %d 个面锚失效/解析不到" % len(anchors))
        lines += ["  %s" % a for a in anchors]
        lines.append("  注：解析不到 ⇒ 判红（不得把「解析不到」当「无违规」）")
        payload["verdict"] = "FAIL"
        payload["rc"] = 2
        return 2, "\n".join(lines), payload
    if kind_set_diffs or field_diffs:
        lines.append("EVT_FIELD_SETS_FAIL: %d 处不一致（正本 = %s）"
                     % (len(kind_set_diffs) + len(field_diffs), payload["authority"]))
        for d in kind_set_diffs:
            lines.append("  [kind 集合] %s 缺 %s 多 %s" % (d["face"], d["missing"], d["extra"]))
        for d in field_diffs:
            lines.append("  [kind %s] %s 缺 %s 多 %s"
                         % (d["kind"], d["face"], d["missing"], d["extra"]))
        lines.append("  各面 kind 数：%s" % {f: len(fields.get(f, {})) for f in FIELD_FACES})
        payload["verdict"] = "FAIL"
        payload["rc"] = 1
        return 1, "\n".join(lines), payload
    n_kinds = len(fields[AUTHORITY_FACE])
    n_fields = sum(len(v) for v in fields[AUTHORITY_FACE].values())
    lines.append("EVT_FIELD_SETS_PASS: %d 面 × %d kind 逐 kind 字段集完全一致（集合相等，顺序无关）"
                 % (len(FIELD_FACES), n_kinds))
    lines.append("  正本 = %s（%d 个冻结扩展字段；字段清单由正本解析，检查器内不手抄）"
                 % (payload["authority"], n_fields))
    lines.append("  面 = %s" % " / ".join(FIELD_FACES))
    payload["verdict"] = "PASS"
    payload["rc"] = 0
    return 0, "\n".join(lines), payload


# ---------------------------------------------------------------- 自检（能绿能红）

_FIX_PROTOCOL_H = """// fixture（合成，非真仓字段名）
inline std::string missing_required_extension_v1(const std::string& kind,
                                                 const nlohmann::json& ev) {
    static const std::map<std::string, std::vector<std::string>> kExt = {
        {"alpha", {"f_a1", "f_a2"}},
        {"beta", {"f_b1"}},
        {"gamma", {}},
    };
    auto it = kExt.find(kind);
    if (it == kExt.end()) return {};
    return {};
}
inline const std::vector<std::string>& registered_event_kinds_v1() {
    static const std::vector<std::string> k = {
        "alpha",
        "beta",
        "gamma",
    };
    return k;
}
"""

_FIX_SCHEMA_OBJ = {
    "properties": {"kind": {"enum": ["alpha", "beta", "gamma"]}},
    "allOf": [
        {"if": {"properties": {"kind": {"const": "alpha"}}},
         "then": {"required": ["f_a1", "f_a2"]}},
        {"if": {"properties": {"kind": {"const": "beta"}}},
         "then": {"required": ["f_b1"]}},
        {"if": {"properties": {"kind": {"const": "gamma"}}}, "then": {"required": []}},
    ],
    "x-astrocs-event-kind-registry": {
        "spec": "astrocs.event-kind-registry/v1",
        "implementation_authority": "lib/infrastructure/cli/protocol.h::registered_event_kinds_v1()",
        "kinds": {"alpha": ["f_a1", "f_a2"], "beta": ["f_b1"], "gamma": []},
    },
}

_FIX_PY = """import json
KIND_EXT = {
    "alpha": {"f_a1", "f_a2"},
    "beta": {"f_b1"},
    "gamma": set(),
}
"""


def _sub(text: str, old: str, new: str, where: str) -> str:
    if text.count(old) != 1:
        raise AssertionError("self-test 夹具变异失败（%s）：%r 命中 %d 次"
                             % (where, old, text.count(old)))
    return text.replace(old, new, 1)


def _fixture_repo(tmp: pathlib.Path, protocol_h: str, schema_text: str,
                  cli004: str, fix208: str) -> pathlib.Path:
    root = tmp / "repo"
    for rel, text in ((PROTOCOL_H, protocol_h), (SCHEMA, schema_text),
                      (FACE_PATH["readside_cli004"], cli004),
                      (FACE_PATH["readside_fix208"], fix208)):
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return root


def _self_test() -> int:
    """正例 1 组 + 逐面负例 15 组（5 面 × 删/加/改名）+ fail-closed 负例 11 组
    （块缺失/形态漂移/退化/文件缺失/授权未指向正本）+ kind 集合差异与正本自洽负例。"""
    failures: list = []
    def schema_json(mutate=None):
        obj = copy.deepcopy(_FIX_SCHEMA_OBJ)
        if mutate:
            mutate(obj)
        return json.dumps(obj, ensure_ascii=False, indent=2)

    schema_text = schema_json()
    base = {"protocol_h": _FIX_PROTOCOL_H, "schema": schema_text,
            "cli004": _FIX_PY, "fix208": _FIX_PY}

    def run(files):
        with tempfile.TemporaryDirectory(prefix="evtfields-selftest-") as td:
            root = _fixture_repo(pathlib.Path(td), files["protocol_h"], files["schema"],
                                 files["cli004"], files["fix208"])
            return check(root)

    rc, text, _ = run(base)
    if rc != 0:
        failures.append("正例应判绿（rc=0），实得 rc=%d：%s" % (rc, text))

    def variant(**kw):
        files = dict(base)
        files.update(kw)
        return files

    # 逐面负例：删一个字段 / 加一个字段 / 改一个字段名 ⇒ 必须 rc=1 且逐 kind 逐字段点名
    cases = [
        ("A-del", variant(protocol_h=_sub(base["protocol_h"], '"f_a1", ', "", "A-del")),
         1, ["alpha", "f_a1"]),
        ("A-add", variant(protocol_h=_sub(base["protocol_h"], '"f_a2"}', '"f_a2", "f_a9"}', "A-add")),
         1, ["alpha", "f_a9"]),
        ("A-ren", variant(protocol_h=_sub(base["protocol_h"], '"f_a1"', '"f_a1x"', "A-ren")),
         1, ["alpha", "f_a1", "f_a1x"]),
        ("B-del", variant(schema=schema_json(
            lambda o: o["allOf"][1]["then"]["required"].remove("f_b1"))),
         1, ["beta", "f_b1"]),
        ("B-add", variant(schema=schema_json(
            lambda o: o["allOf"][1]["then"]["required"].append("f_b9"))),
         1, ["beta", "f_b9"]),
        ("B-ren", variant(schema=schema_json(
            lambda o: o["allOf"][1]["then"]["required"].__setitem__(0, "f_b1x"))),
         1, ["beta", "f_b1", "f_b1x"]),
        ("C-del", variant(schema=schema_json(
            lambda o: o["x-astrocs-event-kind-registry"]["kinds"].__setitem__("alpha", ["f_a2"]))),
         1, ["alpha", "f_a1"]),
        ("C-add", variant(schema=schema_json(
            lambda o: o["x-astrocs-event-kind-registry"]["kinds"]["alpha"].append("f_a9"))),
         1, ["alpha", "f_a9"]),
        ("C-ren", variant(schema=schema_json(
            lambda o: o["x-astrocs-event-kind-registry"]["kinds"].__setitem__("alpha",
                                                                             ["f_a1", "f_a2x"]))),
         1, ["alpha", "f_a2", "f_a2x"]),
        ("D-del", variant(cli004=_sub(base["cli004"], '"f_a1", ', "", "D-del")),
         1, ["alpha", "f_a1"]),
        ("D-add", variant(cli004=_sub(base["cli004"], '"f_a2"}', '"f_a2", "f_a9"}', "D-add")),
         1, ["alpha", "f_a9"]),
        ("D-ren", variant(cli004=_sub(base["cli004"], '"f_b1"', '"f_b1x"', "D-ren")),
         1, ["beta", "f_b1", "f_b1x"]),
        ("E-del", variant(fix208=_sub(base["fix208"], '"beta": {"f_b1"}', '"beta": set()',
                                        "E-del")),
         1, ["beta", "f_b1"]),
        ("E-add", variant(fix208=_sub(base["fix208"], '"f_b1"}', '"f_b1", "f_b9"}', "E-add")),
         1, ["beta", "f_b9"]),
        ("E-ren", variant(fix208=_sub(base["fix208"], '"gamma": set()',
                                      '"gamma": {"f_g9"}', "E-ren")),
         1, ["gamma", "f_g9"]),
    ]
    for name, files, want_rc, tokens in cases:
        rc, text, _ = run(files)
        if rc != want_rc:
            failures.append("负例 %s 应 rc=%d，实得 rc=%d：%s" % (name, want_rc, rc, text))
            continue
        missing = [t for t in tokens if t not in text]
        if missing:
            failures.append("负例 %s 未点名 %s：%s" % (name, missing, text))

    # fail-closed 负例：解析不到 / 形态漂移 / 退化 ⇒ rc=2 且 ANCHOR_STALE 点名
    fc_cases = [
        ("FC-A-块缺失", variant(protocol_h=_sub(base["protocol_h"], "kExt = {", "kExtX = {", "FC-A")),
         2, ["ANCHOR_STALE", "protocol_h_kext"]),
        ("FC-A-名册缺失", variant(protocol_h=_sub(base["protocol_h"],
                                                  "registered_event_kinds_v1() {",
                                                  "registered_event_kinds_v1_x() {", "FC-A2")),
         2, ["ANCHOR_STALE", "protocol_h_kinds"]),
        ("FC-B-then缺失",
         variant(schema=schema_json(lambda o: o["allOf"][1].__setitem__("then", {}))),
         2, ["ANCHOR_STALE", "schema_then_required"]),
        ("FC-C-xregistry缺失",
         variant(schema=schema_json(lambda o: o.pop("x-astrocs-event-kind-registry"))),
         2, ["ANCHOR_STALE", "schema_x_registry"]),
        ("FC-C-授权未指向正本",
         variant(schema=schema_json(lambda o: o["x-astrocs-event-kind-registry"].__setitem__(
             "implementation_authority", "somewhere_else()"))),
         2, ["ANCHOR_STALE", "implementation_authority"]),
        ("FC-D-KIND_EXT改名",
         variant(cli004=_sub(base["cli004"], "KIND_EXT", "KIND_EXT_X", "FC-D")),
         2, ["ANCHOR_STALE", "readside_cli004"]),
        ("FC-D-字段集非字面量",
         variant(cli004=_sub(base["cli004"], '"alpha": {"f_a1", "f_a2"}',
                             '"alpha": make_fields()', "FC-D2")),
         2, ["ANCHOR_STALE", "readside_cli004"]),
        ("FC-E-文件缺失", variant(fix208=""), 2, ["ANCHOR_STALE", "readside_fix208"]),
        ("FC-A-退化零kind",
         variant(protocol_h=_sub(base["protocol_h"],
                                 '{"alpha", {"f_a1", "f_a2"}},\n'
                                 '        {"beta", {"f_b1"}},\n'
                                 '        {"gamma", {}},\n    ', "", "FC-A3")),
         2, ["ANCHOR_STALE", "protocol_h_kext", "0 个 kind"]),
        ("FC-B-退化空enum",
         variant(schema=schema_json(lambda o: o["properties"]["kind"].__setitem__("enum", []))),
         2, ["ANCHOR_STALE", "schema_kind_enum"]),
        ("FC-C-退化空kinds",
         variant(schema=schema_json(
             lambda o: o["x-astrocs-event-kind-registry"].__setitem__("kinds", {}))),
         2, ["ANCHOR_STALE", "schema_x_registry"]),
    ]
    for name, files, want_rc, tokens in fc_cases:
        if files["fix208"] == "":
            with tempfile.TemporaryDirectory(prefix="evtfields-selftest-") as td:
                root = _fixture_repo(pathlib.Path(td), files["protocol_h"], files["schema"],
                                     files["cli004"], _FIX_PY)
                os.remove(root / FACE_PATH["readside_fix208"])
                rc, text, _ = check(root)
        else:
            rc, text, _ = run(files)
        if rc != want_rc:
            failures.append("fail-closed 负例 %s 应 rc=%d，实得 rc=%d：%s" % (name, want_rc, rc, text))
            continue
        missing = [t for t in tokens if t not in text]
        if missing:
            failures.append("fail-closed 负例 %s 未点名 %s：%s" % (name, missing, text))

    # 正本内部不自洽负例（C3）：registered_event_kinds_v1 少一个 kind ⇒ rc=1 且点名 kind 集合
    rc, text, _ = run(variant(protocol_h=_sub(base["protocol_h"], '        "gamma",\n', "",
                                               "C3-名册少kind")))
    if rc != 1 or "kind 集合" not in text or "protocol_h_kinds" not in text:
        failures.append("C3 负例（正本名册少 kind）应 rc=1 且点名 kind 集合/protocol_h_kinds，"
                        "实得 rc=%d：%s" % (rc, text))

    # kind 集合差异负例：enum + x-registry + allOf 分支都多一个 kind ⇒ rc=1 且点名 kind 集合
    def add_delta(o):
        o["properties"]["kind"]["enum"].append("delta")
        o["allOf"].append({"if": {"properties": {"kind": {"const": "delta"}}},
                           "then": {"required": ["f_d1"]}})
        o["x-astrocs-event-kind-registry"]["kinds"]["delta"] = ["f_d1"]

    rc, text, _ = run(variant(schema=schema_json(add_delta)))
    if rc != 1 or "delta" not in text or "kind 集合" not in text:
        failures.append("kind 集合负例应 rc=1 且点名 delta/kind 集合，实得 rc=%d：%s" % (rc, text))

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例判绿；逐面负例 %d 组（删/加/改名，逐 kind 逐字段点名）与 "
          "fail-closed 负例 %d 组（块缺失/形态漂移/退化/文件缺失 ⇒ ANCHOR_STALE + rc=2）"
          "全部按预期命中；kind 集合差异单独判红" % (len(cases), len(fc_cases)))
    return 0


# ---------------------------------------------------------------- main

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="事件协议 per-kind 冻结扩展字段集跨面一致门"
                    "（正本 = protocol.h::missing_required_extension_v1）")
    ap.add_argument("--repo-root", default=str(REPO))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()

    root = pathlib.Path(args.repo_root)
    if not root.is_dir():
        print("EVT_FIELD_SETS_FAIL: --repo-root 不可用 %s（fail-closed）" % root, file=sys.stderr)
        return 2
    rc, text, payload = check(root)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(text)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
