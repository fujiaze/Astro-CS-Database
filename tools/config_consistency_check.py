#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CONFIG-CONSISTENCY | Stage2 配置默认值一致性门（struct / parser / schema /
template / docs 五面）。

审计根因（CONFORM-SWEEP-3-017）
  本工具是 docs/development/CONFIG_SCHEMA.md:3-4 自述的默认值一致性唯一校验器，
  但四条事实源路径**全部失效**，只输出 env_missing×4 ⇒ C4（默认值不符）类缺陷
  在全仓无机器门。根因两条：
    1. 模块搬迁：lib/phase2/{include/astro/phase2,src}/stage2_common.{h,cpp}
       → lib/algorithms/coverage/{include/astro/phase2,src}/stage2_common.{h,cpp}
       （AGENTS.md §6 lib/algorithms/ 并联放置；CMakeLists 同步改名）；
    2. 控制包归档：工程控制/{schemas/stage2.schema.json,configs/stage2.template.json}
       在 GOV-002（commit b7b2dea70dbcdacdcf6eb762609a908abdeab697
       「docs(governance): GOV-002 归档非当前工程文档」）被**从工作树删除**，
       engineering/control/ 亦已清空 —— 树内不再有 stage2 schema/template。

事实源与判据（任一 finding ⇒ exit 1）
  L0 输入面（fail-closed）
     L0a 活体事实源必须存在：header / parser / docs/development/CONFIG_SCHEMA.md；
     L0b 已退役事实源必须**确实已退役**：归档路径不得重新出现在工作树
         （出现 ⇒ fact_source_resurrected：必须重新启用直读腿，不得装作没看见）；
     L0c 退役声明必须可验证：`git log --diff-filter=D -1 -- <path>` 必须等于登记的
         归档 commit（否则 retirement_unverified）；
     L0d 归档内容必须可从 git 对象库读出（`git show <commit>^:<path>`）；
     L0e parser 提取数 = 0 ⇒ parser_extraction（禁止空转判绿，E-P2）。
  L1 struct ↔ parser（stage2_common.h ↔ stage2_common.cpp）—— 原判据，保留
  L2 归档 template ↔ 归档 schema 默认值
  L3 归档 schema/template ↔ 当前 struct（冻结记录 vs 现行实现）
  L4 docs/development/CONFIG_SCHEMA.md 自述默认 ↔ 现行实现（parser 优先，缺则 struct）
  L5 docs 的条件式默认 `key(auto→v)` ↔ parser 中该字段的实际赋值字面量

已登记差异（registry）
  tools/fixtures/config_consistency_known_divergences.json 承载**已审计且未修**的差异，
  逐条必须有 kind/reason/owner/audit_ref/exit_condition（缺字段 ⇒ exit 2）。
  未登记的差异一律判红；登记项若不再复现 ⇒ registry_stale 判红（豁免必须存活）。
  `--strict` 忽略 registry（审计用：暴露全部差异）。

用法
  python3 tools/config_consistency_check.py [--root .] [--json-out F] [--strict]
  python3 tools/config_consistency_check.py --self-test
exit 0 = 一致（或差异已登记）；1 = 判据违规；2 = 输入/台账不可用（fail-closed）。

只读；仅 stdlib；stdout 末段 JSON 保留 legacy 键 checked_keys/mismatches/pass
（tools/api_doc_consistency.py 消费）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HDR_REL = "lib/algorithms/coverage/include/astro/phase2/stage2_common.h"
SRC_REL = "lib/algorithms/coverage/src/stage2_common.cpp"
DOC_REL = "docs/development/CONFIG_SCHEMA.md"
# GOV-002 归档 commit（stage2 schema/template 的最后存续版本 = 该 commit 的父提交）
ARCHIVE_COMMIT = "b7b2dea70dbcdacdcf6eb762609a908abdeab697"
ARCHIVED = {
    "schema": "工程控制/schemas/stage2.schema.json",
    "template": "工程控制/configs/stage2.template.json",
}
REGISTRY_REL = "tools/fixtures/config_consistency_known_divergences.json"
OUT_REL = "run/temp/p2_v15/evidence/config_consistency.json"

LEGACY_PATHS = {  # 审计点名已失效的旧路径（用于报错时给出根因提示）
    "lib/phase2/include/astro/phase2/stage2_common.h": HDR_REL,
    "lib/phase2/src/stage2_common.cpp": SRC_REL,
}

REQUIRED_REGISTRY_FIELDS = ("kind", "reason", "owner", "audit_ref", "exit_condition")

# parser key → (struct field, enum_map) —— 原判据表（V15/V16/V17 冻结面）
ALIAS = {
    "method": ("reject_method", {"\"auto\"": "P2_REJECT_AUTO"}),
    "profile": ("reject_profile", {"\"wbpp_current\"": "\"wbpp_2_9_1\""}),
    "normalization": ("reject_normalization",
                      {"\"median_center\"": "\"astrocs_median_center_v1\"",
                       "\"median_scale\"": "\"astrocs_median_scale_v1\""}),
    "underdetermined_n": ("reject_underdetermined_n", {}),
    "robust_mad_clip.lower_sigma": ("sigma_lower", {}),
    "robust_mad_clip.upper_sigma": ("sigma_upper", {}),
    "robust_mad_clip.max_iterations": ("sigma_max_iterations", {}),
    "winsorized_sigma.lower_sigma": ("winsor_lower", {}),
    "winsorized_sigma.upper_sigma": ("winsor_upper", {}),
    "winsorized_sigma.max_iterations": ("winsor_max_iterations", {}),
    "averaged_sigma.lower_sigma": ("avg_lower", {}),
    "averaged_sigma.upper_sigma": ("avg_upper", {}),
    "averaged_sigma.max_iterations": ("avg_max_iterations", {}),
    "linear_fit.lower": ("linfit_lower", {}),
    "linear_fit.upper": ("linfit_upper", {}),
    "linear_fit.max_iterations": ("linfit_max_iterations", {}),
    "generalized_esd.alpha": ("esd_alpha", {}),
    "generalized_esd.max_outliers": ("esd_max_outliers", {}),
    "percentile.low_fraction": ("pct_low_fraction", {}),
    "percentile.high_fraction": ("pct_high_fraction", {}),
    "median_sigma.lower_sigma": ("medsig_lower", {}),
    "median_sigma.upper_sigma": ("medsig_upper", {}),
    "median_sigma.max_iterations": ("medsig_max_iterations", {}),
    "minmax.reject_low_count": ("minmax_low_count", {}),
    "minmax.reject_high_count": ("minmax_high_count", {}),
    "minmax.min_kept": ("minmax_min_kept", {}),
    "large_scale.enabled": ("large_scale_enabled", {}),
    "large_scale.min_structure_pixels": ("large_scale_min_structure_pixels", {}),
    "large_scale.low_grow_radius_pixels": ("large_scale_low_grow_pixels", {}),
    "large_scale.high_grow_radius_pixels": ("large_scale_high_grow_pixels", {}),
}
# docs 条件式默认 key → struct field（`key(auto→v)` 的落点）
DOC_CONDITIONAL_FIELD = {"smoothing": "smoothing_lambda"}
# 归档 model 键取 "auto" 时由 parser 落到 struct 默认（逐条验证赋值存在，不假定）
ARCHIVE_AUTO = {"model.patch_radius_pixels": "patch_radius_leaf"}
# parser 条件默认（auto → 字面量）↔ struct 默认：审计 009 smoothing_lambda 双口径
CONDITIONAL_STRUCT = {"smoothing": "smoothing_lambda"}
# 值语义哨兵：该字面量表示「按权威 resolver/profile 运行时解析」而非固定默认；
# 命中即报 parser_sentinel:<key> —— doc/struct 的固定值必须与 resolver 逐档默认一致。
SENTINELS = {
    "underdetermined_n": ("0", "0 = 按 profile/request 默认由 p2_reject_plan_resolve 解析"),
}


def read_text(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def git(root, args):
    r = subprocess.run(["git"] + args, cwd=root, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


# --------------------------------------------------------------- 源码侧提取
def struct_defaults(text, consts=None):
    d = {}
    for m in re.finditer(
            r"^\s*(?:int|double|bool|std::uint64_t|std::string|std::uint32_t)"
            r"\s+([a-z_0-9]+)\s*=\s*([^;]+);", text, re.M):
        d[m.group(1)] = resolve_const(m.group(2).strip(), consts)
    return d


def include_consts(root, src_text):
    """从 parser 的 include 闭包收集**具名常量**：字符串/数值 #define 与 constexpr。

    必要性：CONFORM-FIX-B 之后 parser/struct 用 P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL、
    P2_SMOOTHING_LAMBDA_AUTO 等具名常量而非字面量；只认字面量的提取器会静默丢键
    （=判据塌缩），故按 include 闭包把常量解析回字面量。解析不到的具名常量由
    evaluate 报 macro_unresolved（fail-closed，禁止静默丢键）。
    """
    consts = {}
    seen = set()
    queue = re.findall(r'#include\s+"([^"]+)"', src_text)
    inc_root = os.path.join(root, "lib/algorithms/coverage/include")
    while queue:
        rel = queue.pop(0)
        if rel in seen:
            continue
        seen.add(rel)
        path = os.path.join(inc_root, rel)
        if not os.path.isfile(path):
            continue
        text = read_text(path)
        for m in re.finditer(r'#define\s+([A-Z][A-Z0-9_]*)\s+"([^"]*)"', text):
            consts.setdefault(m.group(1), '"' + m.group(2) + '"')
        for m in re.finditer(r'#define\s+([A-Z][A-Z0-9_]*)\s+'
                             r'([-0-9.]+(?:[eE][-+]?[0-9]+)?)\b', text):
            consts.setdefault(m.group(1), m.group(2))
        for m in re.finditer(r'constexpr\s+[\w:<> ]+?\s+([A-Z][A-Z0-9_]*)\s*=\s*'
                             r'([-0-9.]+(?:[eE][-+]?[0-9]+)?)\s*;', text):
            consts.setdefault(m.group(1), m.group(2))
        queue.extend(re.findall(r'#include\s+"([^"]+)"', text))
    return consts


def resolve_const(val, consts):
    """具名常量 -> 字面量；非具名或解析不到则原样返回。"""
    v = str(val).strip()
    if re.fullmatch(r"[A-Z][A-Z0-9_]*", v) and v in (consts or {}):
        return consts[v]
    return v


def extract_values(seg, consts=None):
    """-> ({key: literal}, [(key, 未解析具名常量)])。literal 统一为 "str"/数值/true|false。"""
    consts = consts or {}
    out, unresolved = {}, []
    for m in re.finditer(
            r'\.value\(\s*"([a-z_0-9]+)"\s*,\s*'
            r'(?:std::string\("([^"]*)"\)|std::string\(([A-Z][A-Z0-9_]*)\)|'
            r'\(std::uint64_t\)([0-9]+)|\(std::uint32_t\)([0-9]+)|'
            r'([-0-9.]+(?:[eE][-+]?[0-9]+)?)|([A-Z][A-Z0-9_]*)|(true|false))', seg):
        key = m.group(1)
        if m.group(2) is not None:
            val = '"' + m.group(2) + '"'
        elif m.group(3) is not None:
            name = m.group(3)
            if name not in consts:
                unresolved.append((key, name))
                continue
            val = consts[name]
        elif m.group(4) is not None:
            val = m.group(4)
        elif m.group(5) is not None:
            val = m.group(5)
        elif m.group(6) is not None:
            val = m.group(6)
        elif m.group(7) is not None:
            name = m.group(7)
            if name not in consts:
                unresolved.append((key, name))
                continue
            val = consts[name]
        else:
            val = m.group(8)
        out.setdefault(key, val)
    return out, unresolved


def parser_defaults(text, macros=None):
    """→ ({parser key: literal}, [(key, 未解析宏名)])。"""
    d = {}
    unresolved = []

    def take(seg, prefix=""):
        vals, un = extract_values(seg, macros)
        for k, v in vals.items():
            d.setdefault(prefix + k, v)
        unresolved.extend((prefix + k, n) for k, n in un)

    mstart = text.find('"model"')
    mend = text.find('"integration"')
    take(text[mstart:mend if mend > mstart else len(text)])
    # integration 顶层段（precision/memory_limit_mb/weight_mode/acr_route/
    # legacy_allow_weight_fallback）——原实现只覆盖 rejection 子段，是覆盖缺口。
    istart = text.find('"integration"')
    iend = text.find('"output"', istart)
    take(text[istart:iend if iend > istart else len(text)])
    rstart = text.find('"rejection"')
    wstart = text.find('"weight_mode"', rstart)
    rej_seg = text[rstart:wstart if wstart > rstart else len(text)]
    take(rej_seg)
    for block in re.finditer(
            r'if\s*\(\s*rj\.contains\("([a-z_0-9]+)"\)\s*\)\s*\{'
            r'(.*?)\n\s*\}', rej_seg, re.S):
        name = block.group(1)
        vals, un = extract_values(block.group(2), macros)
        for k, v in vals.items():
            d["%s.%s" % (name, k)] = v
        unresolved.extend(("%s.%s" % (name, k), n) for k, n in un)
    ls_start = rej_seg.find('rj.contains("large_scale")')
    if ls_start >= 0:
        brace = rej_seg.find('{', ls_start)
        depth = 0
        end = brace
        while end < len(rej_seg):
            if rej_seg[end] == '{':
                depth += 1
            elif rej_seg[end] == '}':
                depth -= 1
                if depth == 0:
                    break
            end += 1
        vals, un = extract_values(rej_seg[brace:end + 1], macros)
        for k, v in vals.items():
            d["large_scale.%s" % k] = v
        unresolved.extend(("large_scale.%s" % k, n) for k, n in un)
    return d, unresolved


# --------------------------------------------------------------- docs 侧提取
def doc_block(text):
    """取 CONFIG_SCHEMA.md 中含 integration/rejection 的 text 围栏块。"""
    for m in re.finditer(r"```text\n(.*?)```", text, re.S):
        if "integration:" in m.group(1) and "rejection{" in m.group(1):
            return m.group(1)
    return ""


def _balanced(text, i):
    """text[i] == '(' ⇒ 返回 (inner, next_index)；不匹配返回 (None, i)。"""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == '(':
            depth += 1
        elif text[j] == ')':
            depth -= 1
            if depth == 0:
                return text[i + 1:j], j + 1
    return None, i


TRANSPARENT_BRACES = {"rejection"}   # 该组不产生键前缀（键名与判据表同层）
_KV_VALUE = re.compile(r"([-0-9.]+(?:[eE][-+]?[0-9]+)?|true|false"
                       r"|[A-Za-z_][A-Za-z0-9_]*)\b")


_LEGAL_SET = re.compile(r"^[A-Za-z_][A-Za-z0-9_.]*"
                        r"(?:\s*[|/]\s*[A-Za-z_][A-Za-z0-9_.]*)+$")


_LEGAL_PART = re.compile(r"^[A-Za-z_][A-Za-z0-9_.]*"
                         r"(?:\s*\([^()]*\))?(?:\s+[A-Za-z_][A-Za-z0-9_]*)?$")


def _is_legal_set(inner):
    """合法值集（`a|b|c`、`cpu/auto`、`x(注记)|y(注记)`）而非单一默认值。"""
    s = inner.strip()
    if _LEGAL_SET.match(s):
        return True
    if "|" in s:
        return all(_LEGAL_PART.match(p.strip()) for p in s.split("|"))
    return False


def _brace_end(seg, i):
    depth = 0
    for j in range(i, len(seg)):
        if seg[j] == '{':
            depth += 1
        elif seg[j] == '}':
            depth -= 1
            if depth == 0:
                return j
    return len(seg) - 1


def doc_tokens(block):
    """→ [(key, content, form)]，form ∈ {value, set, conditional}；含 {} 前缀展开。"""
    out = []

    def walk(seg, prefix, in_brace):
        i = 0
        while i < len(seg):
            m = re.match(r"[a-z_][a-z_0-9]*", seg[i:])
            if not m:
                i += 1
                continue
            name = m.group(0)
            i += len(name)
            while i < len(seg) and seg[i] in " \t\n\r":
                i += 1
            if i < len(seg) and seg[i] == '(':
                inner, nxt = _balanced(seg, i)
                if inner is None:
                    continue
                i = nxt
                key = prefix + name
                if _is_legal_set(inner):
                    dm = re.search(r"([A-Za-z_][A-Za-z0-9_.]*)\s*\(\s*生产默认", inner)
                    out.append((key, dm.group(1) if dm else None,
                                "value" if dm else "set"))
                elif "→" in inner:
                    a, _, b = inner.partition("→")
                    out.append((key, (a.strip(), b.strip()), "conditional"))
                else:
                    out.append((key, inner.strip(), "value"))
            elif i < len(seg) and seg[i] == '{':
                j = _brace_end(seg, i)
                sub = prefix if name in TRANSPARENT_BRACES else prefix + name + "."
                walk(seg[i + 1:j], sub, True)
                i = j + 1
            elif in_brace:
                # 空格分隔的 subkey value（如 robust_mad_clip{lower_sigma 4 ...}）；
                # 值后必须紧跟空白或组末，避免把 method\n none|sigma|... 的合法集当默认值
                vm = _KV_VALUE.match(seg[i:])
                if vm:
                    end = i + len(vm.group(0))
                    if end >= len(seg) or seg[end] in " \t\n\r":
                        out.append((prefix + name, vm.group(1), "value"))
                        i = end
    walk(block, "", False)
    return out


def sentinel_of(key, val):
    """命中值语义哨兵则返回 (哨兵字面量, 语义说明)，否则 None。"""
    spec = SENTINELS.get(key)
    if spec and norm(val) == norm(spec[0]):
        return spec
    return None


def norm(v):
    return str(v).replace(" ", "")


def same_value(a, b):
    a, b = norm(a), norm(b)
    if a == b:
        return True
    try:
        return abs(float(a) - float(b)) < 1e-12
    except ValueError:
        return a.strip('"') == b.strip('"')


# --------------------------------------------------------------- 归档事实源
def archived_text(root, rel, commit):
    rc, out, err = git(root, ["show", "%s^:%s" % (commit, rel)])
    if rc != 0:
        return None, (err or out).strip()
    return out, None


def verify_retirement(root, rel, commit):
    """→ (ok, code, detail)"""
    if os.path.exists(os.path.join(root, rel)):
        return (False, "fact_source_resurrected",
                "%s 重新出现在工作树：归档腿必须改回直读（不得静默跳过）" % rel)
    rc, out, err = git(root, ["log", "--diff-filter=D", "-1", "--format=%H", "--", rel])
    if rc != 0:
        return (False, "retirement_unverified",
                "git log 失败，无法验证 %s 的退役（%s）" % (rel, (err or "").strip()))
    sha = out.strip().splitlines()[0].strip() if out.strip() else ""
    if sha != commit:
        return (False, "retirement_unverified",
                "%s 的删除 commit = %s，登记值 = %s（退役声明与历史不符）"
                % (rel, sha or "<none>", commit))
    return (True, None, None)


# --------------------------------------------------------------- 主判定
def evaluate(root, archive_commit, registry, strict=False):
    findings = []
    checked = []
    legs = {}

    def add(leg, key, **kw):
        rec = {"leg": leg, "key": key}
        rec.update(kw)
        findings.append(rec)

    # L0a 活体事实源
    live = {"header": HDR_REL, "parser": SRC_REL, "docs": DOC_REL}
    for label, rel in live.items():
        if not os.path.isfile(os.path.join(root, rel)):
            hint = LEGACY_PATHS.get(rel)
            add("L0", "env_missing:%s" % label,
                issue="%s 不存在: %s%s" % (label, rel,
                                          ("（旧路径 %s 已搬迁）" % hint) if hint else ""))
    if findings:
        return {"findings": findings, "checked_keys": [], "legs": legs,
                "registry_used": {}, "doc_keys": {}}

    hdr_text = read_text(os.path.join(root, HDR_REL))
    src_text = read_text(os.path.join(root, SRC_REL))
    doc_text = read_text(os.path.join(root, DOC_REL))
    consts = include_consts(root, src_text)
    sd = struct_defaults(hdr_text, consts)
    pd, macro_unresolved = parser_defaults(src_text, consts)
    for key, name in macro_unresolved:
        if key in pd:
            continue
        add("L0", "macro_unresolved:%s" % key,
            issue="parser 默认值引用宏 %s，无法从 include 闭包解析 ⇒ 判据不可比"
                  "（fail-closed，禁止静默丢键）" % name)

    # L0b/L0c/L0d 归档事实源
    arch = {}
    for label, rel in ARCHIVED.items():
        ok, code, detail = verify_retirement(root, rel, archive_commit)
        if not ok:
            add("L0", "%s:%s" % (code, label), issue=detail)
            continue
        text, err = archived_text(root, rel, archive_commit)
        if text is None:
            add("L0", "archive_unreadable:%s" % label,
                issue="%s 无法从 git %s^ 读出: %s" % (rel, archive_commit[:10], err))
            continue
        try:
            arch[label] = json.loads(text)
        except ValueError as exc:
            add("L0", "archive_unreadable:%s" % label,
                issue="%s 归档内容非法 JSON: %s" % (rel, exc))
    if any(f["key"].startswith(("fact_source_resurrected", "retirement_unverified",
                                "archive_unreadable")) for f in findings):
        return {"findings": findings, "checked_keys": [], "legs": legs,
                "registry_used": {}, "doc_keys": {}}

    # L0e 空转面
    if not pd:
        add("L0", "parser_extraction",
            issue="parser 未提取到任何默认值 (checked=0)，段定位或源码格式已漂移")

    # L1 struct ↔ parser
    for pk, (field, enum_map) in ALIAS.items():
        if pk not in pd:
            continue
        if field not in sd:
            add("L1", "struct_vs_parser:%s" % pk, issue="struct 无此字段",
                parser_default=pd[pk])
            continue
        if sentinel_of(pk, pd[pk]):
            checked.append(pk)   # 哨兵语义由 L7 单点判定（避免同键重复）
            continue
        a = resolve_const(sd[field], consts)
        b = resolve_const(enum_map.get(pd[pk], pd[pk]), consts)
        if not same_value(norm(a), norm(b)):
            add("L1", "struct_vs_parser:%s" % pk, struct_field=field,
                struct_default=sd[field], parser_default=pd[pk])
        checked.append(pk)

    # L2 归档 template ↔ 归档 schema
    if "template" in arch and "schema" in arch:
        tj = arch["template"].get("integration", {}).get("rejection", {})
        sj = (arch["schema"].get("properties", {}).get("integration", {})
              .get("properties", {}).get("rejection", {}).get("properties", {}))
        for key, val in [("method", tj.get("method")), ("profile", tj.get("profile"))]:
            if key in sj and val is not None and not same_value(val, sj[key].get("default")):
                add("L2", "template_vs_schema:%s" % key, template=val,
                    schema_default=sj[key].get("default"))
        for mname in ["robust_mad_clip", "winsorized_sigma", "averaged_sigma",
                      "linear_fit", "generalized_esd", "percentile",
                      "median_sigma", "minmax", "rcr", "large_scale"]:
            tsub = tj.get(mname, {})
            sprops = sj.get(mname, {}).get("properties", {})
            for k, v in tsub.items():
                dfl = sprops.get(k, {}).get("default")
                if dfl is not None and not same_value(v, dfl):
                    add("L2", "template_vs_schema:%s.%s" % (mname, k), template=v,
                        schema_default=dfl)
        # model 段
        tm = arch["template"].get("model", {})
        sm = (arch["schema"].get("properties", {}).get("model", {})
              .get("properties", {}))
        for k, v in tm.items():
            dfl = sm.get(k, {}).get("default")
            if dfl is not None and not same_value(v, dfl):
                add("L2", "template_vs_schema:model.%s" % k, template=v, schema_default=dfl)

    # L3 归档 schema/template ↔ 现行 struct
    if "schema" in arch:
        sm = (arch["schema"].get("properties", {}).get("model", {})
              .get("properties", {}))
        for key, spec in sorted(sm.items()):
            if "default" not in spec:
                continue
            field = "patch_radius_leaf" if key == "patch_radius_pixels" else key
            if field not in sd:
                continue
            if spec["default"] == "auto" and ("model.%s" % key) in ARCHIVE_AUTO:
                continue   # 由 L3b 逐条验证 auto → cfg-><field> = <struct 默认>;
            if not same_value(spec["default"], sd[field]):
                add("L3", "struct_vs_archive:model.%s" % key,
                    archive_default=spec["default"], struct_field=field,
                    struct_default=sd[field])

    # L3b 归档 auto 值 ↔ parser 条件落点（auto → struct 默认，逐条验证赋值存在）
    for akey, field in sorted(ARCHIVE_AUTO.items()):
        spec = (arch.get("schema", {}).get("properties", {}).get("model", {})
                .get("properties", {}).get(akey.split(".", 1)[1], {}))
        if spec.get("default") != "auto" or field not in sd:
            continue
        pat = r"cfg->%s\s*=\s*%s\s*;" % (re.escape(field), re.escape(str(sd[field])))
        if not re.search(pat, src_text):
            add("L3", "archive_auto_unverified:%s" % akey,
                issue="归档 %s=auto 应落到 cfg->%s = %s;，parser 中未找到该赋值"
                % (akey, field, sd[field]))

    # L6 parser 条件默认（key: auto → v）↔ struct 默认（审计 009：smoothing 双口径）
    for key, field in sorted(CONDITIONAL_STRUCT.items()):
        m = re.search(r'if\s*\(\s*m\.contains\("' + re.escape(key) + r'"\)\s*\)'
                      r'(.*?)cfg->' + re.escape(field) +
                      r'\s*=\s*([-0-9.]+(?:[eE][-+]?[0-9]+)?|[A-Z][A-Z0-9_]*)\s*;',
                      src_text, re.S)
        if not m or field not in sd:
            continue
        if not same_value(resolve_const(m.group(2), consts), sd[field]):
            add("L6", "struct_vs_parser_conditional:%s" % key,
                parser_auto_default=m.group(2), struct_field=field,
                struct_default=sd[field])

    # L4 docs ↔ 现行实现
    block = doc_block(doc_text)
    tokens = doc_tokens(block) if block else []
    doc_keys = {"value": [], "set": [], "conditional": []}
    for key, val, form in tokens:
        doc_keys[form].append(key)
        if form == "set":
            continue
        if form == "conditional":
            in_val, out_val = val
            field = DOC_CONDITIONAL_FIELD.get(key)
            if not field:
                doc_keys.setdefault("conditional_unmapped", []).append(key)
                continue
            pat = (r"cfg->%s\s*=\s*([-0-9.]+(?:[eE][-+]?[0-9]+)?|[A-Z][A-Z0-9_]*)\s*;"
                   % re.escape(field))
            hit = re.search(pat, src_text)
            if not hit or not same_value(resolve_const(hit.group(1), consts), out_val):
                add("L5", "doc_conditional:%s" % key, doc_input=in_val,
                    doc_value=out_val, struct_field=field,
                    issue="parser 中未找到 cfg->%s = %s; 的赋值" % (field, out_val))
            continue
        if val is None:
            continue
        src_kind = None
        actual = None
        if key in pd:
            if sentinel_of(key, pd[key]):
                checked.append("doc:%s" % key)   # 哨兵语义由 L7 单点判定
                continue
            actual, src_kind = pd[key], "parser"
        else:
            field = ALIAS.get(key, (key, {}))[0]
            if field in sd:
                actual, src_kind = sd[field], "struct"
        if actual is None:
            doc_keys.setdefault("unmapped", []).append(key)
            continue
        if not same_value(val, actual):
            add("L4", "doc_vs_%s:%s" % (src_kind, key), doc_default=val,
                actual_default=actual)
        checked.append("doc:%s" % key)
    if block and not tokens:
        add("L0", "doc_extraction",
            issue="CONFIG_SCHEMA.md stage2 块未提取到任何键（docs 腿空转）")

    # L7 值语义哨兵：parser 用「按 profile/request 运行时解析」的哨兵值而非固定默认
    doc_val_map = {k: v for k, v, form in tokens if form == "value"}
    for key, spec in sorted(SENTINELS.items()):
        if key not in pd or norm(pd[key]) != norm(spec[0]):
            continue
        field = ALIAS.get(key, (key, {}))[0]
        add("L7", "parser_sentinel:%s" % key, parser_default=pd[key],
            struct_field=field, struct_default=sd.get(field),
            doc_default=doc_val_map.get(key),
            issue="parser 用哨兵 %s（%s）⇒ 有效默认由运行时按 profile 解析；"
                  "docs/struct 的固定值必须与 resolver 逐档默认一致"
                  % (spec[0], spec[1]))

    # registry 归并
    reg = {}
    for e in registry:
        eid = str(e.get("id", ""))
        key = eid.split("config_consistency_divergence:", 1)[-1]
        reg[key] = e
    registered, unregistered = [], []
    for f in findings:
        if not strict and f["key"] in reg:
            registered.append(dict(f, registry=reg[f["key"]]))
        else:
            unregistered.append(f)
    stale = sorted(k for k in reg if k not in {f["key"] for f in findings})
    for k in stale:
        unregistered.append({"leg": "REG", "key": "registry_stale:%s" % k,
                             "issue": "登记项不再复现（差异已修复或判据已漂移）"
                                      "——满足 exit_condition 后必须删除该登记"})
    legs = {"L1": len([f for f in findings if f["leg"] == "L1"]),
            "L2": len([f for f in findings if f["leg"] == "L2"]),
            "L3": len([f for f in findings if f["leg"] == "L3"]),
            "L4": len([f for f in findings if f["leg"] == "L4"]),
            "L5": len([f for f in findings if f["leg"] == "L5"]),
            "L6": len([f for f in findings if f["leg"] == "L6"]),
            "L7": len([f for f in findings if f["leg"] == "L7"]),
            "L0": len([f for f in findings if f["leg"] == "L0"])}
    return {"findings": unregistered, "registered": registered,
            "checked_keys": sorted(set(checked)), "legs": legs,
            "registry_used": reg, "doc_keys": doc_keys,
            "hard_input_error": any(f["leg"] == "L0" and
                                    f["key"].startswith(("env_missing", "fact_source",
                                                         "retirement", "archive_"))
                                    for f in unregistered)}


def load_registry(root):
    path = os.path.join(root, REGISTRY_REL)
    if not os.path.isfile(path):
        return [], None
    try:
        doc = json.loads(read_text(path))
    except ValueError as exc:
        return None, "registry 非法 JSON: %s" % exc
    entries = doc.get("entries", [])
    for e in entries:
        missing = [f for f in REQUIRED_REGISTRY_FIELDS if not e.get(f)]
        if not e.get("id") or missing:
            return None, ("registry 条目 %r 缺字段 %s（kind/reason/owner/audit_ref/"
                          "exit_condition 必填）" % (e.get("id"), missing))
        if e.get("kind") != "known_divergence":
            return None, "registry 条目 %s 的 kind 必须为 known_divergence" % e.get("id")
    return entries, None


def emit(result, root, json_out, strict):
    res = {
        "schema": "astrocs/config-consistency/v2",
        "task": "GUARD-TOOLS-FIX / CONFORM-SWEEP-3-017",
        "fact_sources": {"header": HDR_REL, "parser": SRC_REL, "docs": DOC_REL,
                         "archive_commit": ARCHIVE_COMMIT,
                         "archived": ARCHIVED},
        "strict": strict,
        "legs": result.get("legs", {}),
        "checked_keys": result.get("checked_keys", []),
        "registered_divergences": [
            {"key": f["key"], "leg": f["leg"], "audit_ref": f["registry"].get("audit_ref"),
             "owner": f["registry"].get("owner"),
             "exit_condition": f["registry"].get("exit_condition")}
            for f in result.get("registered", [])],
        "mismatches": result["findings"],
        "doc_keys": result.get("doc_keys", {}),
        "pass": len(result["findings"]) == 0,
    }
    out_path = json_out or os.path.join(root, OUT_REL)
    parent = os.path.dirname(os.path.abspath(out_path))
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    if not res["pass"]:
        print("CONFIG_CONSISTENCY_FAIL: %d findings (legs=%s)"
              % (len(res["mismatches"]),
                 ",".join("%s:%d" % kv for kv in sorted(res["legs"].items()) if kv[1])))
        for f in res["mismatches"]:
            extra = " ".join("%s=%s" % (k, v) for k, v in sorted(f.items())
                             if k not in ("key", "leg", "issue"))
            print("  [%s] %s %s" % (f["leg"], f["key"], extra))
            if f.get("issue"):
                print("      %s" % f["issue"])
    else:
        print("CONFIG_CONSISTENCY_PASS: legs=%s checked=%d registered=%d"
              % (",".join("%s:%d" % kv for kv in sorted(res["legs"].items())),
                 len(res["checked_keys"]), len(res["registered_divergences"])))
        for r in res["registered_divergences"]:
            print("  REGISTERED_DIVERGENCE %s [%s] audit=%s owner=%s"
                  % (r["key"], r["leg"], r["audit_ref"], r["owner"]))
            print("      exit_condition: %s" % r["exit_condition"])
    print(json.dumps(res, ensure_ascii=False, indent=2))
    return 0 if res["pass"] else (2 if result.get("hard_input_error") else 1)


# ------------------------------------------------------------------ self-test
def _write(root, rel, text):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        fh.write(text)


FIX_HDR = '''#pragma once
#include <string>
struct P2Stage2Config {
    int control_grid_per_tile = 8;
    int sigma_max_iterations = 8;
    std::string reject_profile = "wbpp_2_9_1";
    double smoothing_lambda = 0.0;
};
'''
FIX_SRC = '''#include "astro/phase2/stage2_common.h"
bool p2_stage2_parse_config(const nlohmann::json& j, P2Stage2Config* cfg, std::string* err) {
    if (j.contains("model")) {
        const auto& m = j["model"];
        cfg->control_grid_per_tile = m.value("control_grid_per_tile", 8);
        if (m.contains("smoothing")) {
            const auto& sm = m["smoothing"];
            if (sm.is_string()) { cfg->smoothing_lambda = 0.1; }
        }
    }
    if (j.contains("integration")) {
        const auto& in = j["integration"];
        const std::string prec = in.value("precision", std::string("fp32"));
        if (in.contains("rejection")) {
            const auto& rj = in["rejection"];
            cfg->reject_profile = rj.value("profile", std::string("wbpp_2_9_1"));
            if (rj.contains("robust_mad_clip")) {
                const auto& s = rj["robust_mad_clip"];
                cfg->sigma_max_iterations = s.value("max_iterations", 8);
            }
        }
        const std::string wm = in.value("weight_mode", std::string("auto"));
    }
    return true;
}
'''
FENCE = chr(96) * 3
FIX_DOC = ('# Config / Schema（单一事实来源）\n\n'
           '规则：C++ struct 默认值、parser 默认值、JSON schema、template config、'
           'docs、tests 必须一致。\n\n## Stage2 config 段\n\n' + FENCE + 'text\n'
           'model: control_grid_per_tile(8) smoothing(auto→0.1)\n'
           'integration: precision(fp32) rejection{method\n'
           '             none|auto\n'
           '             profile(astrocs_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档))\n'
           '             robust_mad_clip{max_iterations 8}\n'
           '             weight_mode(auto)}\n' + FENCE + '\n')
FIX_SCHEMA = {"properties": {
    "model": {"properties": {"control_grid_per_tile": {"default": 8}}},
    "integration": {"properties": {"rejection": {"properties": {
        "method": {"default": "auto"},
        "profile": {"default": "wbpp_2_9_1"}}}}}}}
FIX_TEMPLATE = {"model": {"control_grid_per_tile": 8},
                "integration": {"rejection": {"method": "auto",
                                                 "profile": "wbpp_2_9_1"}}}
FIX_REGISTRY = {"entries": [
    {"id": "config_consistency_divergence:doc_vs_parser:profile",
     "kind": "known_divergence", "reason": "fixture：docs 自述自研档 vs parser 对照档",
     "owner": "selftest", "audit_ref": "CONFORM-SWEEP-3-007",
     "exit_condition": "fixture"},
    {"id": "config_consistency_divergence:struct_vs_parser_conditional:smoothing",
     "kind": "known_divergence", "reason": "fixture：auto→0.1 vs struct 0.0",
     "owner": "selftest", "audit_ref": "CONFORM-SWEEP-3-009",
     "exit_condition": "fixture"}]}


def _mk_fixture():
    """**自造**最小夹具仓（不读真仓，避免与并行分片的 lib/ 改动竞态）。

    含真实 git 删除 commit，用于验证归档腿与退役声明腿。
    """
    root = tempfile.mkdtemp(prefix="cfg_consistency_selftest_")
    _write(root, HDR_REL, FIX_HDR)
    _write(root, SRC_REL, FIX_SRC)
    _write(root, DOC_REL, FIX_DOC)
    _write(root, REGISTRY_REL, json.dumps(FIX_REGISTRY, ensure_ascii=False, indent=2))
    _write(root, ARCHIVED["schema"], json.dumps(FIX_SCHEMA, ensure_ascii=False, indent=2))
    _write(root, ARCHIVED["template"], json.dumps(FIX_TEMPLATE, ensure_ascii=False, indent=2))
    env = dict(os.environ)
    env.update({"GIT_AUTHOR_NAME": "selftest", "GIT_AUTHOR_EMAIL": "selftest@local",
                "GIT_COMMITTER_NAME": "selftest", "GIT_COMMITTER_EMAIL": "selftest@local"})
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"],
                ["git", "commit", "-q", "-m", "live"],
                ["git", "rm", "-r", "-q", "工程控制"],
                ["git", "commit", "-q", "-m", "archive"]):
        subprocess.run(cmd, cwd=root, check=True, capture_output=True, env=env)
    rc, out, _ = git(root, ["rev-parse", "HEAD"])
    return root, out.strip()



def self_test():
    fails = []
    checks = []

    def run(root, commit, strict=False):
        reg, err = load_registry(root)
        if err:
            return {"findings": [{"leg": "REG", "key": "registry_invalid", "issue": err}],
                    "checked_keys": [], "legs": {}}
        return evaluate(root, commit, reg, strict)

    def expect(name, res, want_pass, want_key=None):
        keys = {f["key"] for f in res["findings"]}
        ok = (len(res["findings"]) == 0) == want_pass and (want_key is None or want_key in keys)
        checks.append((name, "PASS" if not res["findings"] else "FAIL", sorted(keys)[:4], ok))
        if not ok:
            fails.append("%s: 期望 pass=%s key=%s 实得 keys=%s"
                         % (name, want_pass, want_key, sorted(keys)))

    root, commit = _mk_fixture()
    try:
        expect("N0 正例（活体源一致 + 归档腿可验证）", run(root, commit), True)
        expect("N0' --strict 暴露已登记差异", run(root, commit, strict=True), False,
               "doc_vs_parser:profile")

        src_path = os.path.join(root, SRC_REL)
        src0 = read_text(src_path)
        # N1 注入 parser 默认值不符 ⇒ 必须红
        _write(root, SRC_REL, src0.replace('s.value("max_iterations", 8)',
                                           's.value("max_iterations", 9)'))
        expect("N1 注入 parser 默认值不符", run(root, commit), False,
               "struct_vs_parser:robust_mad_clip.max_iterations")
        # N1' 恢复 ⇒ 必须绿
        _write(root, SRC_REL, src0)
        expect("N1' 恢复后回绿", run(root, commit), True)

        # N2 struct 默认值不符 ⇒ 必须红
        hdr_path = os.path.join(root, HDR_REL)
        hdr0 = read_text(hdr_path)
        _write(root, HDR_REL, hdr0.replace("int sigma_max_iterations = 8;",
                                           "int sigma_max_iterations = 9;"))
        expect("N2 注入 struct 默认值不符", run(root, commit), False,
               "struct_vs_parser:robust_mad_clip.max_iterations")
        _write(root, HDR_REL, hdr0)
        expect("N2' 恢复后回绿", run(root, commit), True)

        # N3 docs 默认值不符（未登记键）⇒ 必须红
        doc_path = os.path.join(root, DOC_REL)
        doc0 = read_text(doc_path)
        _write(root, DOC_REL, doc0.replace("control_grid_per_tile(8)",
                                           "control_grid_per_tile(9)"))
        expect("N3 注入 docs 默认值不符", run(root, commit), False,
               "doc_vs_parser:control_grid_per_tile")
        _write(root, DOC_REL, doc0)
        expect("N3' 恢复后回绿", run(root, commit), True)

        # N4 parser 提取面塌缩 ⇒ 必须红（禁止空转判绿）
        _write(root, SRC_REL, src0.replace(".value(", ".valu3("))
        expect("N4 parser 提取面塌缩", run(root, commit), False, "parser_extraction")
        _write(root, SRC_REL, src0)
        expect("N4' 恢复后回绿", run(root, commit), True)

        # N5 归档事实源复活 ⇒ 必须红
        for rel in ARCHIVED.values():
            _write(root, rel, "{}\n")
        expect("N5 归档事实源复活", run(root, commit), False, None)
        res = run(root, commit)
        if not any(f["key"].startswith("fact_source_resurrected") for f in res["findings"]):
            fails.append("N5 未报 fact_source_resurrected：%s" % res["findings"])
        for rel in ARCHIVED.values():
            os.remove(os.path.join(root, rel))
        expect("N5' 移除复活文件后回绿", run(root, commit), True)

        # N6 退役声明不可验证（错 commit）⇒ 必须红
        expect("N6 归档 commit 不符", run(root, "0" * 40), False, None)

        # N7 registry 条目失活 ⇒ 必须红
        reg_path = os.path.join(root, REGISTRY_REL)
        reg0 = read_text(reg_path)
        reg = json.loads(reg0)
        reg["entries"].append({"id": "config_consistency_divergence:ghost_key",
                               "kind": "known_divergence", "reason": "fixture",
                               "owner": "selftest", "audit_ref": "N7",
                               "exit_condition": "n/a"})
        _write(root, REGISTRY_REL, json.dumps(reg, ensure_ascii=False, indent=2))
        expect("N7 registry 条目失活", run(root, commit), False, "registry_stale:ghost_key")
        _write(root, REGISTRY_REL, reg0)
        # N8 registry 缺字段 ⇒ fail-closed
        reg["entries"].append({"id": "config_consistency_divergence:x", "kind": "known_divergence"})
        _write(root, REGISTRY_REL, json.dumps(reg, ensure_ascii=False, indent=2))
        expect("N8 registry 缺必填字段", run(root, commit), False, "registry_invalid")
        _write(root, REGISTRY_REL, reg0)
        expect("N8' 恢复后回绿", run(root, commit), True)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    for name, verdict, keys, ok in checks:
        print("  %-34s verdict=%-4s keys=%-46s %s"
              % (name, verdict, ",".join(keys) or "-", "OK" if ok else "**FAIL**"))
    if fails:
        print("CONFIG_CONSISTENCY_SELFTEST_FAIL:")
        for f in fails:
            print("  " + f)
        return 1
    print("CONFIG_CONSISTENCY_SELFTEST_PASS: %d 组（正例 1 + 注入红/恢复绿 + fail-closed）"
          "全部符合预期" % len(checks))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Stage2 config default consistency gate")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--archive-commit", default=ARCHIVE_COMMIT)
    ap.add_argument("--strict", action="store_true",
                    help="忽略已登记差异（审计用：暴露全部差异）")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("CONFIG_CONSISTENCY_FAIL: ANCHOR_STALE --root 不存在 %s" % root)
        return 2
    registry, err = load_registry(root)
    if err:
        print("CONFIG_CONSISTENCY_FAIL: registry_invalid: %s" % err)
        return 2
    return emit(evaluate(root, args.archive_commit, registry, args.strict),
                root, args.json_out, args.strict)


if __name__ == "__main__":
    sys.exit(main())
