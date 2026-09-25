#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""块流端口 ↔ 代码双向一致门（REGISTRY-ALIGN-01）。

防的复发缺口：**注册表声明的端口/边与代码真实数据流不一致**——
声明了不存在的边（消费者等一个永不产生的块）或漏声明真实存在的边
（节点偷偷读写未登记的产品）。二者都让「按注册表迁移到命名块」变成 facade。

判据（全部 fail-closed；每条都有负例自测）：
  C1 结构：registry_schema/v2 + carrier_contract + 每 module 恰一个 operation +
     每 port 的 name/direction/carrier/artifacts/code 齐全且 carrier 在词表内。
  C2 声明⇒实现：每个 code 锚点必须解析得到——文件存在、symbol 在本文件内解析到
     **唯一**函数体（symbol="file" 时退化为文件级）、token 必须落在该函数体内。
     声明了不存在的边 ⇒ 锚点无法解析 ⇒ 判红。
  C3 实现⇒声明：以**代码侧闭合文法**（不是注册表）扫描每个节点函数体，得到
     「该节点触碰的产物 token 集」；该集合必须 ⊆ 该节点声明的 token 集。
     漏声明真实数据流 ⇒ 判红。
  C4 方向一致：声明 token 必须在代码里出现（声明⊆代码），且由变量流分析推断出的
     读写角色必须包含声明的 direction；推断不出角色 ⇒ 判红（不得静默放过）。
  C5 载体合同：节点间端口 carrier ∈ {output_dir_file, hips_product_tree}；
     不存在跨阶段同名边（同一产物身份不得在一个阶段产出、在另一个阶段被消费）；
     carrier_contract.statement 必须显式声明跨阶段载体是 HiPS 产品树。
  C6 非退化：模块数/端口数/代码侧 token 数/边数都有下界；空注册表、恒真比较判红。

用法：
  python3 eng/tools/quality/check_block_flow_ports_vs_code.py [--repo ROOT] [--json-out F]
  python3 eng/tools/quality/check_block_flow_ports_vs_code.py --self-test
exit 0 = 一致；exit 1 = 有 finding；exit 2 = 锚点/文件不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import copy
import io
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_block_flow_conformance import symbol_ranges  # noqa: E402  单一符号解析实现

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, "eng", "ci"))
from gate_trust import (  # noqa: E402  三态（PASS/FAIL/CRASH）口径的单一实现点
    KIND_CONTENT, KIND_DISCRIMINATING, KIND_EXEMPTION, KIND_PROTECTIVE, emit)
REGISTRY = "lib/infrastructure/pipeline/module_ports.registry.json"
CODE = "lib/infrastructure/scheduler/src/module_adapters.cpp"
CARRIERS = ("output_dir_file", "hips_product_tree", "config_path")
INTER_NODE_CARRIERS = ("output_dir_file", "hips_product_tree")
PHASE_TO_STAGE = {"phase1": "normalize", "phase2": "mosaic", "phase3": "export"}
MIN_MODULES, MIN_PORTS, MIN_CODE_TOKENS, MIN_EDGES = 20, 60, 40, 20
FILE_SCOPE = "file"

# ── 代码侧闭合文法（与注册表无关；改这里就等于改判据强度，必须同步自测）──────
NODE_FN = re.compile(r"^Result<[^>]*>\s+(p[123]_op_[A-Za-z0-9_]+)\s*\(")
LITERAL = re.compile(r'"([^"]*)"')
ARTIFACT_LIT = re.compile(r"^/(p[123]_[a-z0-9_]+)(\.(?:json|bin|fits))?$")
PATH_PREFIX = re.compile(r"^/(calibrated_|cleaned_|photoapplied_)$")
SPECIAL_LIT = {"/output_phase3.fits": "output_phase3.fits", "/run_context.json": "run_context.json"}
PATH_HELPER = re.compile(r"\b(p1_calibrated_path|p1_cosmetic_path|p1_cleaned_input_path|p1_photoapplied_path)\b")

TREE_READ_VERBS = ("aio_hips_open", "aio_hips_read_tile_f32", "p3_sampler_open_ex",
                   "p3_uncertainty_open", "aio_fs::walk_tree", "p3n_input_manifest_hash",
                   "p2_coverage_build")
TREE_WRITE_VERBS = ("aio_hips_product_begin", "hp_drizzle_run_phase1_hips")

READ_VERBS = ("aio_fs::exists", "aio_fs::read_all", "aio_file::read_all", "aio_file::read_range",
              "p2_read_json", "p3n_read_json", "aio_read_fits", "aio_read_metadata", "p1_read_image",
              "aio_hips_open", "p2_upm_open", "p2_sky_plane_open", "p3_sampler_open_ex",
              "p3_uncertainty_open", "p3n_input_manifest_hash", "p3n_guard_input_units",
              "p3n_properties_scalar_keys", "aio_fs::walk_tree", "aio_atomic::path_exists")
WRITE_VERBS = ("p1_write_text", "p2_write_text_atomic", "p2_write_text", "p1_write_fits_atomic",
               "aio_write_fits", "aio_fs::write_atomic", "aio_hips_product_begin",
               "p2_upm_save", "p2_sky_plane_save", "hp_drizzle_run_phase1_hips",
               "aio_atomic::write_open_trunc", "aio_atomic::make_tmp_path",
               "p2_write_bin", "p3_output_write_atomic_ex")
ASSIGN = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=)")


class GateError(RuntimeError):
    pass


def node_bodies(lines):
    """返回 {node_fn: (start, end)}（1-based 闭区间），按 Result<...> pN_op_* 锚点切分。"""
    starts = []
    for i, l in enumerate(lines):
        m = NODE_FN.match(l)
        if m:
            starts.append((i, m.group(1)))
    starts.append((len(lines), None))
    out = {}
    for k in range(len(starts) - 1):
        s, name = starts[k]
        e = starts[k + 1][0]
        body = lines[s:e]
        depth, end = 0, None
        for j, line in enumerate(body):
            depth += line.count("{") - line.count("}")
            if j > 0 and depth == 0:
                end = j; break
        out[name] = (s + 1, s + (end + 1 if end is not None else len(body)))
    return out


# ── 代码侧 token 抽取（闭合文法）────────────────────────────────────────────
def token_key(lit, following=""):
    """把代码字面量/helper 名归一成注册表 token 键；不属文法 ⇒ None。

    无扩展名的 pN_* 字面量必须是**路径拼接**（右引号后紧跟 +），否则视为
    entry/标签串（如 "/p2_sky_plane_eval_delta_block"）而非产物前缀。
    """
    if lit in SPECIAL_LIT:
        return SPECIAL_LIT[lit]
    m = ARTIFACT_LIT.match(lit)
    if m:
        if not m.group(2) and following.strip()[:1] != "+":
            return None
        return m.group(1) + (m.group(2) or "")
    m = PATH_PREFIX.match(lit)
    if m:
        return m.group(1)
    if PATH_HELPER.fullmatch(lit):
        return lit
    return None


def _statement_start(lines, i):
    j = i
    while j > 0:
        prev = lines[j - 1].strip()
        if prev.endswith(";") or prev.endswith("{") or prev.endswith("}"):
            break
        j -= 1
    return j


def _statement_span(lines, i, s1):
    """返回包含第 i+1 行的语句跨度 (start, end)（1-based 闭区间）。"""
    a0 = _statement_start(lines, i)
    j = i
    while j < s1 - 1 and not lines[j].strip().endswith(";"):
        j += 1
    return a0 + 1, j + 1


def code_side_tokens(lines, s0, s1):
    """返回 {token_key: {"lines": [..], "roles": set()}}，只含文法内 token。"""
    body = lines[s0 - 1:s1]
    occ = {}
    for off, line in enumerate(body):
        ln = s0 + off
        for m in LITERAL.finditer(line):
            key = token_key(m.group(1), line[m.end():])
            if key:
                occ.setdefault(key, []).append(ln)
        for m in PATH_HELPER.finditer(line):
            occ.setdefault(m.group(1), []).append(ln)
    out = {}
    for key, lns in occ.items():
        roles = set()
        for ln in lns:
            st = _statement_start(lines, ln - 1)
            head = "\n".join(lines[st:ln])
            var = None
            for m in ASSIGN.finditer(head):
                var = m.group(1)
            for v in READ_VERBS:
                if v in lines[ln - 1]:
                    roles.add("input")
            for v in WRITE_VERBS:
                if v in lines[ln - 1]:
                    roles.add("output")
            names = {var} if var else set()
            # 别名跳（≤2 跳）：OTHER = ... var ... ⇒ OTHER 也纳入角色考察
            for _ in range(2):
                add = set()
                for k in range(s0, s1 + 1):
                    txt = lines[k - 1]
                    m = re.match(r"\s*(?:const\s+)?(?:std::)?string\s+([A-Za-z_]\w*)\s*=(.*)$", txt)
                    if m and any(re.search(r"\b%s\b" % re.escape(n), m.group(2)) for n in names):
                        add.add(m.group(1))
                if not add - names:
                    break
                names |= add
            names.discard(None)
            for k in range(s0, s1 + 1):
                txt = lines[k - 1]
                if not any(re.search(r"\b%s\b" % re.escape(n), txt) for n in names):
                    continue
                a0, a1 = _statement_span(lines, k - 1, s1)
                seg = "\n".join(lines[a0 - 1:a1])
                for v in READ_VERBS:
                    if v in seg:
                        roles.add("input")
                for v in WRITE_VERBS:
                    if v in seg:
                        roles.add("output")
        out[key] = {"lines": lns, "roles": roles}
    return out


def decl_key(tok):
    """注册表 token 归一成代码侧同一键（注册表 token 一律视为产物前缀）。"""
    return token_key(tok, "+") or tok


def declared_tokens(op):
    """注册表该 operation 的 token → (direction, [anchors])。"""
    out = {}
    for p in op.get("ports", []):
        for a in p.get("code") or []:
            tok = a.get("token", "")
            out.setdefault(tok, {"direction": p.get("direction"), "port": p.get("name"), "anchors": []})
            out[tok]["anchors"].append(a)
    return out


# ── 判据 ────────────────────────────────────────────────────────────────────
def evaluate(repo):
    # C4 的**豁免分支计数**：节点函数体没碰、锚点又在别的文件的条目。
    # 「未判定」必须与「已判定为合规」可区分（GATE-TRUST-01）。
    c4_skipped_foreign: list = []
    reg_path = os.path.join(repo, REGISTRY)
    code_path = os.path.join(repo, CODE)
    if not os.path.isfile(reg_path):
        raise GateError("ANCHOR_MISSING: %s" % REGISTRY)
    if not os.path.isfile(code_path):
        raise GateError("ANCHOR_MISSING: %s" % CODE)
    reg = json.loads(io.open(reg_path, encoding="utf-8").read())
    code_lines = io.open(code_path, encoding="utf-8", errors="replace").read().splitlines()
    errs = []

    # C1 结构
    if reg.get("registry_schema") != "astrocs.module-ports-registry/v2":
        errs.append("C1 registry_schema must be astrocs.module-ports-registry/v2, got %r"
                    % reg.get("registry_schema"))
    cc = reg.get("carrier_contract") or {}
    if not cc.get("statement") or not cc.get("carriers"):
        errs.append("C1 carrier_contract.statement/carriers missing (载体合同必须显式)")
    for c in CARRIERS:
        if c not in (cc.get("carriers") or {}):
            errs.append("C1 carrier_contract.carriers lacks %r" % c)
    modules = reg.get("modules") or []
    if not modules:
        errs.append("C6 registry has no modules (空注册表 = 未做一致性核查)")
    syms = symbol_ranges(code_lines)
    bodies = node_bodies(code_lines)
    file_cache = {}

    def load(rel):
        if rel not in file_cache:
            p = os.path.join(repo, rel)
            if not os.path.isfile(p):
                file_cache[rel] = None
            else:
                ls = io.open(p, encoding="utf-8", errors="replace").read().splitlines()
                file_cache[rel] = (ls, symbol_ranges(ls))
        return file_cache[rel]

    # 边：产物 token → 生产者/消费者（跨模块聚合）
    produced, consumed = {}, {}
    n_ports = 0
    for m in modules:
        mid, phase = m.get("module_id"), m.get("phase")
        ops = m.get("operations") or []
        if len(ops) != 1:
            errs.append("C1 %s must bind exactly one operation, got %d" % (mid, len(ops)))
            continue
        op = ops[0]
        seen = set()
        for p in op.get("ports") or []:
            n_ports += 1
            nm, d = p.get("name"), p.get("direction")
            if not nm or d not in ("input", "output"):
                errs.append("C1 %s port malformed: %r" % (mid, p))
                continue
            if nm in seen:
                errs.append("C1 %s duplicate port name %r" % (mid, nm))
            seen.add(nm)
            if p.get("carrier") not in CARRIERS:
                errs.append("C1 %s/%s carrier %r not in %s" % (mid, nm, p.get("carrier"), list(CARRIERS)))
            if not p.get("artifacts"):
                errs.append("C1 %s/%s artifacts empty (端口必须给出真实产物)" % (mid, nm))
            if not p.get("code"):
                errs.append("C1 %s/%s has no code anchor (端口必须可证伪)" % (mid, nm))
            # C5 节点间载体：非 config_path 的端口即节点间载体
            if p.get("carrier") in ("output_dir_file", "hips_product_tree"):
                pass
            (produced if d == "output" else consumed).setdefault(nm, []).append(
                (mid, phase, p.get("carrier")))
        # C2 声明⇒实现
        for tok, info in declared_tokens(op).items():
            for a in info["anchors"]:
                f, sym, tk = a.get("file"), a.get("symbol"), a.get("token")
                if not f or not sym or not tk:
                    errs.append("C2 %s anchor malformed %r" % (mid, a)); continue
                loaded = load(f)
                if loaded is None:
                    errs.append("C2 %s anchor file missing: %s" % (mid, f)); continue
                ls, sy = loaded
                if sym == FILE_SCOPE:
                    if not any(tk in t for t in ls):
                        errs.append("C2 %s token %r not found anywhere in %s" % (mid, tk, f))
                elif sym not in sy:
                    errs.append("C2 %s declared symbol %r not found in %s (renamed/deleted)"
                                % (mid, sym, f))
                elif len(sy[sym]) > 1:
                    errs.append("C2 %s declared symbol %r ambiguous in %s" % (mid, sym, f))
                else:
                    a0, a1 = sy[sym][0]
                    if not any(tk in ls[i - 1] for i in range(a0, min(a1, len(ls)) + 1)):
                        errs.append("C2 %s token %r is not inside %s() (%s:%d-%d)"
                                    % (mid, tk, sym, f, a0, a1))
        # C3/C4/C3b 代码侧（只对本文件的节点函数做）
        fn = node_fn_of(mid)
        if fn is None or fn not in bodies:
            errs.append("C3 %s has no node function in %s (无法做代码侧核对)" % (mid, CODE))
            continue
        s0, s1 = bodies[fn]
        cs = code_side_tokens(code_lines, s0, s1)
        decl = declared_tokens(op)
        by_key = {}
        for tok, info in decl.items():
            by_key.setdefault(decl_key(tok), []).append((tok, info))
        for key in sorted(cs):
            if key not in by_key:
                errs.append("C3 %s touches undeclared artifact %r (code %s:%s)"
                            % (mid, key, CODE, cs[key]["lines"][:3]))
        for key, items in sorted(by_key.items()):
            if token_key(items[0][0], "+") is None:
                continue   # 非产物文法 token（配置键 / 树动词 / 派生键）：由 C2 锚点负责
            want = {info["direction"] for _, info in items}
            if key in cs:
                # 代码侧**有证据**：角色一致性判据必须照常执行，与锚点落在哪个文件无关。
                # 旧实现在此之前就把「锚点全在别的文件」的条目 `continue` 掉了，于是
                # 「节点函数碰了这个产物、方向却与声明相反」被静默放过——这正是失效形态
                # 「豁免面把该红的东西放过去了」（负例 X1 锁定）。代码侧证据是本文件局部
                # 的，这条豁免在这里没有依据。
                got = cs[key]["roles"]
                if not got:
                    errs.append("C4 %s artifact %r role unclassifiable in %s() "
                                "(fail-closed: 声明方向无法被代码证据确认)" % (mid, key, fn))
                elif not (want & got):
                    errs.append("C4 %s artifact %r declared %s but code shows %s"
                                % (mid, key, sorted(want), sorted(got)))
                continue
            if all(a.get("file") != CODE for _, info in items for a in info["anchors"]):
                # 豁免分支（保留）：节点函数体没碰这个产物，且锚点全在别的文件 ⇒
                # 本门**没有**代码侧证据可判。但「未判定」必须**可见**：旧实现静默
                # `continue`，使「已判定为合规」与「根本没判」在证据面上无法区分。
                c4_skipped_foreign.append({
                    "module": mid, "artifact": key,
                    "anchors": sorted({a.get("file") for _, info in items
                                       for a in info["anchors"] if a.get("file")})})
                continue   # 由 C2 锚点负责（声明⇒实现方向）
            errs.append("C4 %s declares artifact %r but %s() never touches it"
                        % (mid, key, fn))
        # C3b 载体一致：节点触碰 HiPS 产品树 ⇒ 必须有对应 carrier 的端口
        body_txt = "\n".join(code_lines[s0 - 1:s1])
        ports = op.get("ports") or []
        if any(v + "(" in body_txt for v in TREE_WRITE_VERBS):
            if not any(p.get("carrier") == "hips_product_tree" and p.get("direction") == "output"
                       for p in ports):
                errs.append("C3b %s writes a HiPS product tree but declares no "
                            "hips_product_tree output port" % mid)
        elif any(v + "(" in body_txt for v in TREE_READ_VERBS):
            if not any(p.get("carrier") == "hips_product_tree" and p.get("direction") == "input"
                       for p in ports):
                errs.append("C3b %s reads a HiPS product tree but declares no "
                            "hips_product_tree input port" % mid)

    # C5 跨阶段边：同一 token 在一个阶段产出、在另一个阶段被消费 ⇒ 非法
    for key in sorted(set(produced) & set(consumed)):
        if "hips_product_tree" in {c for _, _, c in produced[key]} | {c for _, _, c in consumed[key]}:
            continue   # HiPS 产品树本身就是跨阶段载体（合同允许）
        pstages = {PHASE_TO_STAGE[p] for _, p, _ in produced[key]}
        cstages = {PHASE_TO_STAGE[p] for _, p, _ in consumed[key]}
        if pstages != cstages:
            errs.append("C5 cross-stage artifact identity %r: produced in %s, consumed in %s "
                        "(output_dir 文件约定不得跨阶段)" % (key, sorted(pstages), sorted(cstages)))
    if "HiPS" not in (cc.get("statement") or ""):
        errs.append("C5 carrier_contract.statement must name the HiPS product tree as the "
                    "cross-stage carrier")
    # C6 非退化
    if len(modules) < MIN_MODULES:
        errs.append("C6 only %d modules (min %d)" % (len(modules), MIN_MODULES))
    if n_ports < MIN_PORTS:
        errs.append("C6 only %d ports (min %d)" % (n_ports, MIN_PORTS))
    n_edges = len(set(produced) & set(consumed))
    if n_edges < MIN_EDGES:
        errs.append("C6 only %d producer→consumer artifact edges (min %d)" % (n_edges, MIN_EDGES))
    n_code = sum(len(code_side_tokens(code_lines, *bodies[f])) for f in bodies)
    if n_code < MIN_CODE_TOKENS:
        errs.append("C6 code-side token inventory only %d (min %d) -> 抽取退化"
                    % (n_code, MIN_CODE_TOKENS))
    return errs, {"modules": len(modules), "ports": n_ports, "edges": n_edges,
                  "code_tokens": n_code, "node_functions": len(bodies),
                  "c4_skipped_foreign": c4_skipped_foreign}


NODE_FN_BY_MODULE = {
    "astrocs.phase1.calibration": "p1_op_calibrate",
    "astrocs.phase1.cosmetic": "p1_op_cosmetic",
    "astrocs.phase1.star-psf": "p1_op_star_psf_impl",
    "astrocs.phase1.wcs-platesolve": "p1_op_wcs",
    "astrocs.phase1.photometry": "p1_op_photometry",
    "astrocs.phase1.noise-snr": "p1_op_noise",
    "astrocs.phase1.drizzle": "p1_op_drizzle",
    "astrocs.phase1.writer": "p1_op_writer",
    "astrocs.phase2.coverage": "p2_op_coverage",
    "astrocs.phase2.sample": "p2_op_sample",
    "astrocs.phase2.upm-fit": "p2_op_upm_fit",
    "astrocs.phase2.upm-apply": "p2_op_upm_apply",
    "astrocs.phase2.reject": "p2_op_reject",
    "astrocs.phase2.integrate": "p2_op_integrate",
    "astrocs.phase2.write": "p2_op_write",
    "astrocs.phase3.properties": "p3_op_properties",
    "astrocs.phase3.wcs": "p3_op_wcs",
    "astrocs.phase3.resample2": "p3_op_resample",
    "astrocs.phase3.writer": "p3_op_writer",
    "astrocs.phase3.verify": "p3_op_verify",
}


def node_fn_of(module_id):
    return NODE_FN_BY_MODULE.get(module_id)


# 每条自检用例的**种类**（单一事实源，逐条显式列举）。种类决定该用例失败时门给的是
# rc=1（被判对象不合规）还是 rc=3（门自身不可信），见 eng/ci/gate_trust.py 的三态口径。
# S1 是**内容断言**（真实注册表必须与代码一致），不是判别力断言——把两者混在一格里，
# 正是「门自检不绿」被误读成「门不可信」的根因。漏登记一条即判 CRASH。
CASE_KIND = {
    "S1-current-repo-green": KIND_CONTENT,
    "S2-empty-registry-red": KIND_DISCRIMINATING,
    "S3-declared-edge-without-implementation-red": KIND_DISCRIMINATING,
    "S4-undeclared-real-flow-red": KIND_DISCRIMINATING,
    "S4b-undeclared-hips-tree-red": KIND_DISCRIMINATING,
    "S5-direction-flipped-red": KIND_DISCRIMINATING,
    "S6-anchor-token-deleted-red": KIND_DISCRIMINATING,
    "S7-token-outside-symbol-red": KIND_DISCRIMINATING,
    "S8-unknown-symbol-red": KIND_DISCRIMINATING,
    "S9-cross-stage-edge-red": KIND_DISCRIMINATING,
    "S10-carrier-contract-missing-red": KIND_DISCRIMINATING,
    "S11-port-without-anchor-red": KIND_DISCRIMINATING,
    "S12-anchor-file-missing-red": KIND_DISCRIMINATING,
    "S13-node-function-renamed-red": KIND_DISCRIMINATING,
    # ↓ 落在本门**自身豁免分支**内的负例（见各用例处的注释）
    "X1-exempt-foreign-anchor-cannot-hide-direction-mismatch-red": KIND_EXEMPTION,
    "X2-exempt-nonartifact-grammar-token-still-anchor-checked-red": KIND_EXEMPTION,
}


# ── 自测（正例 + 负例；负例必须判红）────────────────────────────────────────
def _self_test(json_out=None):
    import tempfile
    cases = []
    details = {}
    reg_txt = io.open(os.path.join(REPO, REGISTRY), encoding="utf-8").read()
    code_txt = io.open(os.path.join(REPO, CODE), encoding="utf-8").read()

    def fixture(reg=None, code=None, extra_file=None):
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, os.path.dirname(REGISTRY)), exist_ok=True)
        os.makedirs(os.path.join(d, os.path.dirname(CODE)), exist_ok=True)
        io.open(os.path.join(d, REGISTRY), "w", encoding="utf-8").write(
            reg if reg is not None else reg_txt)
        io.open(os.path.join(d, CODE), "w", encoding="utf-8").write(
            code if code is not None else code_txt)
        # 注册表锚点引用的其它文件（如 HiPS 末端 sink）一并复制，使 fixture 与仓库同形
        for m in json.loads(reg if reg is not None else reg_txt).get("modules", []):
            for op in m.get("operations", []):
                for port in op.get("ports", []):
                    for a in port.get("code") or []:
                        rel = a.get("file")
                        if rel and rel != CODE:
                            src = os.path.join(REPO, rel)
                            dst = os.path.join(d, rel)
                            if os.path.isfile(src) and not os.path.isfile(dst):
                                os.makedirs(os.path.dirname(dst), exist_ok=True)
                                io.open(dst, "w", encoding="utf-8").write(
                                    io.open(src, encoding="utf-8").read())
        if extra_file:
            rel, txt = extra_file
            p = os.path.join(d, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            io.open(p, "w", encoding="utf-8").write(txt)
        return d

    def errs_of(reg=None, code=None, extra=None):
        d = fixture(reg, code, extra)
        try:
            return evaluate(d)[0]
        finally:
            import shutil
            shutil.rmtree(d, ignore_errors=True)

    # S1 正例：仓库现状全绿
    e = errs_of()
    cases.append(("S1-current-repo-green", e == []))
    details["S1-current-repo-green"] = "\n".join(e) or "(真实注册表与代码一致)"

    # S2 负例：空注册表 ⇒ C6 判红（不得把「没登记」当「一致」）
    doc = json.loads(reg_txt); doc["modules"] = []
    cases.append(("S2-empty-registry-red", any(x.startswith("C6") for x in errs_of(json.dumps(doc)))))

    # S3 负例：**声明了不存在的边**——给 photometry 加一个代码里没有的输入端口
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.photometry":
            m["operations"][0]["ports"].append({
                "name": "ghost_edge", "direction": "input", "carrier": "output_dir_file",
                "artifacts": ["p1_ghost.json"], "data_schema_id": "DATA-P1-GHOST",
                "unit": "ADU", "coordinate": "PIXEL", "scalar": "f32", "shape_hint": "[H,W]",
                "code": [{"file": CODE, "symbol": "p1_op_photometry", "token": "/p1_ghost.json"}]})
    cases.append(("S3-declared-edge-without-implementation-red",
                  any(x.startswith("C2") for x in errs_of(json.dumps(doc)))))

    # S4 负例：**漏声明**——把 drizzle 的 p1_sources 输入端口删掉（节点函数体仍读它）
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.drizzle":
            m["operations"][0]["ports"] = [p for p in m["operations"][0]["ports"]
                                           if p["name"] != "p1_sources"]
    cases.append(("S4-undeclared-real-flow-red",
                  any(x.startswith("C3 ") for x in errs_of(json.dumps(doc)))))

    # S4b 负例：漏声明 HiPS 产品树输出 ⇒ C3b 判红（载体面不得漏）
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.drizzle":
            m["operations"][0]["ports"] = [p for p in m["operations"][0]["ports"]
                                           if p["name"] != "frame_hips"]
    cases.append(("S4b-undeclared-hips-tree-red",
                  any(x.startswith("C3b") for x in errs_of(json.dumps(doc)))))

    # S5 负例：方向写反（把 p1_flux 从 output 改成 input）
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.photometry":
            for p in m["operations"][0]["ports"]:
                if p["name"] == "p1_flux":
                    p["direction"] = "input"
    cases.append(("S5-direction-flipped-red", any(x.startswith("C4") for x in errs_of(json.dumps(doc)))))

    # S6 负例：锚点 token 从代码里删除 ⇒ C2 判红（不是「文件存在即绿」）
    code_bad = code_txt.replace('"/p1_flux.json"', '"/p1_flux_moved.json"')
    cases.append(("S6-anchor-token-deleted-red",
                  any(x.startswith("C2") for x in errs_of(code=code_bad))))

    # S7 负例：token 被移出声明的符号 ⇒ C2 判红
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.photometry":
            for p in m["operations"][0]["ports"]:
                if p["name"] == "p1_flux":
                    p["code"][0]["symbol"] = "p1_op_calibrate"
    cases.append(("S7-token-outside-symbol-red",
                  any(x.startswith("C2") for x in errs_of(json.dumps(doc)))))

    # S8 负例：symbol 不存在（函数改名/删除）⇒ C2 判红
    doc = json.loads(reg_txt)
    doc["modules"][0]["operations"][0]["ports"][0]["code"][0]["symbol"] = "p1_op_no_such_fn"
    cases.append(("S8-unknown-symbol-red",
                  any(x.startswith("C2") for x in errs_of(json.dumps(doc)))))

    # S9 负例：跨阶段边（把 mosaic 的 p2_coverage 也声明给 export 节点消费）
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase3.properties":
            m["operations"][0]["ports"].insert(0, {
                "name": "p2_coverage", "direction": "input", "carrier": "output_dir_file",
                "artifacts": ["p2_coverage.json"], "data_schema_id": "DATA-P2-COV",
                "unit": "DIMENSIONLESS", "coordinate": "PIXEL", "scalar": "f32", "shape_hint": "[H,W]",
                "code": [{"file": CODE, "symbol": "p3_op_properties", "token": "/p2_coverage.json"}]})
    e9 = errs_of(json.dumps(doc))
    cases.append(("S9-cross-stage-edge-red", any(x.startswith("C5") for x in e9)))

    # S10 负例：载体合同缺失/未声明 HiPS ⇒ C1/C5 判红
    doc = json.loads(reg_txt); doc.pop("carrier_contract", None)
    e10 = errs_of(json.dumps(doc))
    cases.append(("S10-carrier-contract-missing-red",
                  any(x.startswith("C1") for x in e10) and any(x.startswith("C5") for x in e10)))

    # S11 负例：端口无 code 锚点（不可证伪）⇒ C1 判红
    doc = json.loads(reg_txt)
    doc["modules"][0]["operations"][0]["ports"][0].pop("code", None)
    cases.append(("S11-port-without-anchor-red",
                  any(x.startswith("C1") for x in errs_of(json.dumps(doc)))))

    # S12 负例：锚点指向不存在的文件 ⇒ C2 判红
    doc = json.loads(reg_txt)
    doc["modules"][0]["operations"][0]["ports"][0]["code"][0]["file"] = "no/such/file.cpp"
    cases.append(("S12-anchor-file-missing-red",
                  any(x.startswith("C2") for x in errs_of(json.dumps(doc)))))

    # S13 负例：代码侧抽取退化（节点函数被改名）⇒ C3 判红（不得静默跳过）
    code_bad = code_txt.replace("Result<void> p2_op_reject(", "Result<void> p2_op_reject_v2(")
    cases.append(("S13-node-function-renamed-red",
                  any(x.startswith("C3") for x in errs_of(code=code_bad))))

    # ── 落在本门**自身豁免分支**内的负例（GATE-TRUST-01）────────────────────
    # 分支①：C4 的守卫原本是「声明的锚点全不在本文件 ⇒ 整条 continue」。
    # 负例把违规喂进这条分支：把 photometry 的 p1_flux 锚点整体搬到**另一个文件**
    # （该文件内容与 CODE 逐字节相同，故 C2 锚点仍解析得到），同时把 direction 从
    # output 翻成 input —— 与代码事实相反。旧实现在此 `continue` 掉 ⇒ 全绿；
    # 修后代码侧有证据（节点函数确实碰了这个产物）⇒ 必须 C4 判红。
    doc = json.loads(reg_txt)
    foreign_rel = "lib/other/code_copy.cpp"
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.photometry":
            for p in m["operations"][0]["ports"]:
                if p["name"] == "p1_flux":
                    p["direction"] = "input"
                    for a in p["code"]:
                        a["file"] = foreign_rel
    errs_x1 = errs_of(json.dumps(doc), extra=(foreign_rel, code_txt))
    cases.append(("X1-exempt-foreign-anchor-cannot-hide-direction-mismatch-red",
                  any(x.startswith("C4") for x in errs_x1)))
    details["X1-exempt-foreign-anchor-cannot-hide-direction-mismatch-red"] = \
        "\n".join(errs_x1)

    # 分支②：`token_key(...) is None ⇒ continue`（非产物文法 token，如配置键/树动词）。
    # 负例把违规喂进这条分支：声明一个**非产物文法**的 token "/p1_op_photometry"
    # （故 C4 走豁免），但把它的 code 锚点 symbol 换成一个不存在的函数名 ⇒
    # 豁免只免掉 C4 的**角色**核对，**不得**连锚点可解析性一起免掉，必须 C2 判红。
    doc = json.loads(reg_txt)
    for m in doc["modules"]:
        if m["module_id"] == "astrocs.phase1.photometry":
            m["operations"][0]["ports"].append({
                "name": "ghost_grammar", "direction": "input", "carrier": "output_dir_file",
                "artifacts": ["p1_ghost"], "data_schema_id": "DATA-P1-GHOST",
                "unit": "ADU", "coordinate": "PIXEL", "scalar": "f32",
                "shape_hint": "[H,W]",
                "code": [{"file": CODE, "symbol": "p1_op_no_such_function",
                          "token": "/p1_op_photometry"}]})
    errs_x2 = errs_of(json.dumps(doc))
    cases.append(("X2-exempt-nonartifact-grammar-token-still-anchor-checked-red",
                  any(x.startswith("C2") for x in errs_x2)))
    details["X2-exempt-nonartifact-grammar-token-still-anchor-checked-red"] = \
        "\n".join(errs_x2)

    # 种类表覆盖性自检：漏登记一条用例的种类 ⇒ 三态会失真 ⇒ 本身就是门不可信。
    uncovered = [n for n, _g in cases if n not in CASE_KIND]
    if uncovered:
        cases.append(("S0-case-kind-table-must-cover-all-cases", False))

    items = [(n, g, CASE_KIND.get(n, KIND_DISCRIMINATING), details.get(n, ""))
             for n, g in cases]
    return emit(items, tool="check_block_flow_ports_vs_code", json_out=json_out,
                extra={"case_kind_table_covers_all": not uncovered,
                       "uncovered_cases": uncovered})


def main(argv=None):
    ap = argparse.ArgumentParser(description="块流端口 ↔ 代码双向一致门")
    ap.add_argument("--repo", default=REPO)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test(json_out=args.json_out)
    try:
        errs, extra = evaluate(args.repo)
    except GateError as exc:
        print("BLOCK_FLOW_PORTS_VS_CODE_FAIL: %s" % exc, file=sys.stderr)
        return 2
    verdict = "PASS" if not errs else "FAIL"
    print("BLOCK_FLOW_PORTS_VS_CODE_%s: modules=%d ports=%d edges=%d code_tokens=%d errors=%d"
          % (verdict, extra["modules"], extra["ports"], extra["edges"],
             extra["code_tokens"], len(errs)))
    # 判词逐条全量打印，**不截断**（旧实现 `errs[:60]` 会隐藏其余 finding）。
    for e in errs:
        print("  " + e)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
            {"tool": "check_block_flow_ports_vs_code", "verdict": verdict,
             "errors": errs, **extra}, ensure_ascii=False, indent=1) + "\n")
    return 0 if not errs else 1


if __name__ == "__main__":
    raise SystemExit(main())
