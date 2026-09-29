#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_isa_same_source.py —— ISA 三侧同源门。

要修的缺陷（审查节点原话）
  禁硬编码 ISA 的门**只读根 CMake**，而 -march=native 散在多个模块 Makefile；
  能力声明只覆盖一个子集而对象按多子集编译，同仓另一处注释已指出不能只看该子集
  ⇒ 声明＝编译＝检测三者不同源。

判据（S2-C「判完成」）
  S1 站点完备：**全量构建输入**（git ls-files 里的 CMakeLists.txt / *.cmake /
     Makefile / GNUmakefile / *.mk）里的每一个 ISA 旗标站点都必须在
     eng/tools/quality/isa_sites.json 有登记；未登记 ⇒ 红（缺位时红而非绿）。
  S2 旗标一致：登记站点声明的旗标集合必须与该构建输入里**实际**出现的旗标集合相等。
  S3 三侧同源：基线以上（高于 -msse2）的 product_graph 站点必须满足
     编译旗标 ⊆ 声明需求位（declaration 宏）⊆ 检测位面（detection 宏），
     且每一个被要求的 feature 位都必须在 feature_bit_header 里实有定义。
     任一侧缺位/不一致 ⇒ 红。
  S4 宿主 ISA 一律红：构建输入里出现 -march=native / 裸 -march= 即红；
     只有 eng/ci/exemptions.json 的显式豁免能解除（当前豁免面为空）。
  S5 状态词：站点 status 必须落在 docs/ASTROCS_DESIGN.md §12.5 阶梯内（越词即红；
     阶梯由 eng/tools/quality/check_conclusion_truth.py 的解析器读出，口径唯一实现）。
  S6 正向状态需证据：status 为正向（CONTRACT_READY/IMPLEMENTED/INSTALLED/VERIFIED/
     READY_FOR_OWNER_REVIEW）的站点必须给出 declaration 与 detection，否则红。

用法
  python3 eng/tools/quality/check_isa_same_source.py [--root DIR] [--json-out PATH] [--quiet]
  python3 eng/tools/quality/check_isa_same_source.py --self-test
  python3 eng/tools/quality/check_isa_same_source.py --fault-inject <unregistered|flag-drift|declaration|march|all>
退出码 0 = 全绿；1 = 至少一条判红；2 = 用法/夹具错误。
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

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
try:  # 状态阶梯解析器唯一实现点（口径唯一源）
    from check_conclusion_truth import parse_ladder  # noqa: E402
except Exception:  # noqa: BLE001
    parse_ladder = None

REG = "eng/tools/quality/isa_sites.json"
DESIGN = "docs/ASTROCS_DESIGN.md"
ISA_FLAG = re.compile(r"(?<![\w-])-m(arch|tune|sse[0-9a-z._]*|avx[0-9a-z]*|fma|bmi[12]|f16c|popcnt)(=\S+)?")
# R-60: 非 GCC 工具链（MSVC / clang-cl）的 ISA 开关不是 -m* 形式，而是 /arch:<档>。
# 不认它 ⇒ MSVC 分支整体成为**扫描盲区**（站点不可见 ⇒ 旗标一致与三侧同源判据全部空转），
# 等于对「Windows 变体与基线同码」零把关。本处只**扩识别面**（收紧），不放宽任何既有判据。
MSVC_ARCH_FLAG = re.compile(r"/arch:(?:AVX10\.[0-9]+|AVX512|AVX2|AVX|SSE4\.2|SSE2|IA32|SSE)(?![\w.])")


def _isa_flags(text):
    """一个文本片段里的全部 ISA 旗标（GCC/Clang 的 -m* 与 MSVC/clang-cl 的 /arch: 并集）。"""
    return (set(m.group(0) for m in ISA_FLAG.finditer(text))
            | set(m.group(0) for m in MSVC_ARCH_FLAG.finditer(text)))
HOST_ISA = re.compile(r"(?<![\w-])-march(=\S+)?")
# 两套命名同源：backend_host 装载面用 ACS_FEAT_*，CPU-001 探测面用 ACS_CAP_FEAT_*，
# 由 registry 的 feature_aliases 归一，判据不承认"两套各自成立"。
FEATURE = re.compile(r"\bACS_(?:CAP_)?(?:FEAT|GROUP)_[A-Z0-9_]+\b")
# 位定义两种形态都要认：#define NAME (1ull << n)（无等号）与 enum 成员 NAME = 1u << n；
# 判据是"该名字与一个移位表达式同行"（见 _header_groups 的 defs）。
POSITIVE = ("CONTRACT_READY", "IMPLEMENTED", "INSTALLED", "VERIFIED", "READY_FOR_OWNER_REVIEW")


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


def _is_build_input(rel):
    base = os.path.basename(rel)
    return (base in ("CMakeLists.txt", "Makefile", "GNUmakefile")
            or rel.endswith(".cmake") or rel.endswith(".mk"))


def collect_build_inputs(root):
    """全量构建输入来自一条 git ls-files（唯一语料口径）。失败即抛（fail-closed）。"""
    r = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True)
    if r.returncode != 0:
        raise RuntimeError("git ls-files 失败 rc=%d（无法枚举全量构建输入）" % r.returncode)
    files = [p for p in r.stdout.decode("utf-8", "surrogateescape").split("\0") if p]
    return sorted(p for p in files if _is_build_input(p))


def _strip_comments_and_join(text):
    """去整行注释；把以反斜杠续行的逻辑行拼成一行（CMake 与 Make 同规则）。"""
    out = []
    for raw in text.splitlines():
        if raw.lstrip().startswith("#"):
            continue
        if out and out[-1].endswith("\\"):
            out[-1] = out[-1][:-1] + " " + raw
        else:
            out.append(raw)
    return out


CMAKE_TARGET_OPTS = re.compile(r"target_compile_options\(\s*([A-Za-z0-9_:.$-]+)([^)]*)\)", re.S)
CMAKE_GLOBAL_OPTS = re.compile(
    r"(add_compile_options\s*\(([^)]*)\)|set\s*\(\s*CMAKE_[A-Z_]*FLAGS([^)]*)\))", re.S)


def scan_sites(root, build_inputs):
    """扫描站点：{(build_input, target): set(flags)}；target 为空串表示文件级。

    CMake 的 target_compile_options( 与其旗标可以跨行（无需反斜杠续行），故按
    **整份文件**匹配命令跨度，而不是逐行 —— 逐行会把跨行旗标误记成文件级站点。
    """
    sites = {}
    for rel in build_inputs:
        p = os.path.join(root, rel)
        if not os.path.isfile(p):
            continue
        text = "\n".join(_strip_comments_and_join(_read(p)))
        is_cmake = os.path.basename(rel) == "CMakeLists.txt" or rel.endswith(".cmake")
        if is_cmake:
            hit = False
            for m in CMAKE_TARGET_OPTS.finditer(text):
                flags = _isa_flags(m.group(2))
                if flags:
                    sites.setdefault((rel, m.group(1)), set()).update(flags)
                    hit = True
            for m in CMAKE_GLOBAL_OPTS.finditer(text):
                flags = _isa_flags((m.group(2) or "") + (m.group(3) or ""))
                if flags:
                    sites.setdefault((rel, "<global>"), set()).update(flags)
                    hit = True
            if not hit:
                flags = _isa_flags(text)
                if flags:
                    sites.setdefault((rel, ""), set()).update(flags)
        else:
            flags = _isa_flags(text)
            if flags:
                sites.setdefault((rel, ""), set()).update(flags)
    return sites


def _macro_body(root, rel, macro):
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        return None
    text = _read(p)
    m = re.search(r"#define\s+%s\b" % re.escape(macro), text)
    if not m:
        return None
    tail = text[m.end():]
    lines = []
    for raw in tail.splitlines():
        if raw.strip() == "" and not lines:
            continue
        lines.append(raw)
        if not raw.rstrip().endswith("\\"):
            break
    return "\n".join(lines)


def _features_of(body):
    return set(FEATURE.findall(body or ""))


def _header_groups(root, rel):
    """从检测位面读出宏表 name -> body，并支持把组宏展开到**位**名。

    位定义（body 无 ACS_FEAT_* 记号）自成一位；组宏（body 是若干记号之或）递归展开。
    这样「声明写成组宏」与「声明写成显式或」在判据里等价（唯一拼写点允许）。"""
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        return {}, set()
    text = _read(p)
    raw = {}
    for m in re.finditer(r"#define\s+([A-Z][A-Z0-9_]*)\s+(.*?)(?=\n#|\Z)", text, re.S):
        raw[m.group(1)] = m.group(2)
    # CPU-001 的位与组都在 enum 里（不是 #define）⇒ 只认 #define 会把整张检测面读成空。
    for m in re.finditer(r"enum\b[^{]*\{([^}]*)\}", text, re.S):
        body = re.sub(r"/\*.*?\*/", " ", m.group(1), flags=re.S)
        body = re.sub(r"//[^\n]*", " ", body)
        for chunk in body.split(","):
            mm = re.match(r"\s*(ACS_[A-Z0-9_]+)\s*=\s*(.+)$", chunk.strip(), re.S)
            if mm:
                raw.setdefault(mm.group(1), mm.group(2))
    # 位定义 = 带移位的那一行里的名字（#define 与 enum 成员两种形态都覆盖）
    defs = set()
    for line in text.splitlines():
        if "<<" in line:
            defs.update(re.findall(r"ACS_FEAT_[A-Z0-9_]+", line))
    return raw, defs


def _canon(tokens, aliases):
    return {aliases.get(t, t) for t in tokens}


def _header_bits(header_raw, name, depth=0):
    """把组宏展开为位名集合（位定义 body 无 ACS_FEAT_* 记号 ⇒ 自身即一位）。"""
    toks = set(FEATURE.findall(header_raw.get(name, "")))
    toks.discard(name)
    if not toks or depth >= 6:
        return {name}
    out = set()
    for t in toks:
        out |= _header_bits(header_raw, t, depth + 1) if t in header_raw else {t}
    return out


def run(root, build_inputs=None):
    fails = []
    reg, rf = _load(root, REG)
    fails += rf
    if reg is None:
        return {"tool": "check_isa_same_source", "pass": False, "build_inputs": 0,
                "sites": 0, "fails": fails}
    if build_inputs is None:
        try:
            build_inputs = collect_build_inputs(root)
        except RuntimeError as exc:
            fails.append(str(exc))
            build_inputs = []
    ladder = []
    if parse_ladder is None:
        fails.append("状态阶梯解析器不可用（check_conclusion_truth.parse_ladder 缺位）")
    else:
        ladder, lf = parse_ladder(root)
        fails += lf
    fmap = reg.get("flag_feature_map") or {}
    aliases = reg.get("feature_aliases") or {}
    baseline = set(reg.get("baseline_flags") or [])
    header_rel = reg.get("feature_bit_header") or ""
    header_raw, header_defs = _header_groups(root, header_rel)
    # 组宏可以在别的头里定义（如 CPU-001 探测面的 ACS_CAP_GROUP_AVX512_SUBSET），
    # 故把 macro_sources 里所有头的宏表合并，供各侧展开使用。
    macro_raw = dict(header_raw)
    for rel in (reg.get("macro_sources") or []):
        macro_raw.update(_header_groups(root, rel)[0])
    if not header_defs:
        fails.append("检测位面缺位: %s 无任何 ACS_FEAT_* 定义（缺位时红而非绿）" % header_rel)
    registered = {}
    for s in reg.get("sites", []):
        key = (s["build_input"], s.get("target", ""))
        registered[key] = s
    sites = scan_sites(root, build_inputs)
    # S1 站点完备
    for key in sorted(sites):
        if key not in registered:
            fails.append("ISA 站点未登记: %s target=%s flags=%s（全量构建输入必须逐个登记）"
                         % (key[0], key[1] or "<file>", sorted(sites[key])))
    # S2 旗标一致
    for key, s in sorted(registered.items()):
        actual = sites.get(key)
        want = set(s.get("flags") or [])
        if actual is None:
            fails.append("登记站点在构建输入里找不到对应的 ISA 旗标行: %s target=%s"
                         % (key[0], key[1] or "<file>"))
            continue
        if actual != want:
            fails.append("旗标不一致: %s target=%s 实际=%s 登记=%s"
                         % (key[0], key[1] or "<file>", sorted(actual), sorted(want)))
    # S3 三侧同源
    for s in reg.get("sites", []):
        flags = set(s.get("flags") or [])
        above = flags - baseline
        decl = s.get("declaration")
        det = s.get("detection")
        if s.get("posture") == "product_graph":
            if above and not decl:
                fails.append("%s: 基线以上旗标 %s 无 declaration（声明缺位）"
                             % (s["id"], sorted(above)))
            if above and not det:
                fails.append("%s: 基线以上旗标 %s 无 detection（检测缺位）"
                             % (s["id"], sorted(above)))
        sides = [("declaration", decl), ("detection", det)]
        for extra in (s.get("extra_sides") or []):
            sides.append((extra.get("role", "side"), extra))
        need = set()
        for f in flags:
            bit = fmap.get(f)
            if bit is None:
                fails.append("%s: 旗标 %s 未登记到 feature 位（flag_feature_map 缺项）"
                             % (s["id"], f))
                continue
            # 一个旗标可同时许可多个位（R-60: MSVC /arch:AVX2 的官方口径是同时开 AVX2 与
            # FMA 指令面）⇒ 映射值允许是「位名列表」或「组宏名」，逐项展开为位名。
            for name in ([bit] if isinstance(bit, str) else list(bit)):
                if name in macro_raw:
                    need |= _canon(_header_bits(macro_raw, name), aliases)
                else:
                    need |= _canon({name}, aliases)
        for label, node in sides:
            if not node:
                continue
            usage = os.path.join(root, node["file"])
            if not os.path.isfile(usage):
                fails.append("%s: %s 使用点 %s 不存在" % (s["id"], label, node["file"]))
                continue
            if node["macro"] not in _read(usage):
                fails.append("%s: %s 使用点 %s 未出现宏 %s（该侧已旁路）"
                             % (s["id"], label, node["file"], node["macro"]))
                continue
            body = _macro_body(root, node["file"], node["macro"])
            if body is None:
                body = _macro_body(root, node.get("definition_file") or header_rel,
                                   node["macro"])
            if body is None:
                fails.append("%s: %s 宏 %s 无定义可解析（三侧不同源）"
                             % (s["id"], label, node["macro"]))
                continue
            raw = dict(macro_raw)
            raw.update(_header_groups(root, node["file"])[0])
            if node.get("definition_file"):
                raw.update(_header_groups(root, node["definition_file"])[0])
            toks = _features_of(body)
            have = set()
            for t in toks:
                have |= _canon(_header_bits(raw, t), aliases) if t in raw \
                    else _canon({t}, aliases)
            missing = sorted(need - have)
            if missing:
                fails.append("%s: %s(%s) 未覆盖编译旗标的位 %s —— 声明/编译不同源"
                             % (s["id"], label, node["macro"], missing))
            # 注（R-60）: 反方向（"声明了却没用"）**不在此处把关** —— 声明位可以合理地
            # 大于旗标位（例: GCC 的 -mavx2 隐含 -mavx，第二族 provider 头据此把
            # ACS_FEAT_AVX 一并声明；而第一族 backend 变体的声明只有 AVX2|FMA，二者
            # 在"隐含位"上的口径本就允许不同，Linux 侧声明位不得因此改动）。
            # 该方向由**产物级**判据把关（eng/tools/quality/check_variant_isa_disasm.py
            # --require-feature: 每一条声明位都必须在产物里找到使用证据），
            # 因为 R-60 约束 4 说的正是"制品声明 ↔ 产物实际指令集"的一致性。
            unaware = sorted(t for t in have if aliases.get(t, t) not in header_defs
                             and t not in header_defs)
            if unaware:
                fails.append("%s: %s 声明了检测位面不存在的位 %s" % (s["id"], label, unaware))
    # S8 平台分支许可位（R-60 约束 4 的正向半边）: 某些工具链没有子集档位旗标，一个旗标会
    # **多**许可若干位（MSVC /arch:AVX512 的许可面比 GCC 四子集旗标多 CD）。这类位不进
    # flag_feature_map 的通用映射（否则检测/路由侧也被迫逐个覆盖它，而它们各按平台写、
    # 且加载前预检以 DSO 自陈声明为准），但**必须**出现在声明侧 —— 缺了就是"用了却没声明"。
    for s in reg.get("sites", []):
        pex = s.get("platform_extra_bits") or {}
        if not pex:
            continue
        decl = s.get("declaration")
        if not decl:
            fails.append("%s: 登记了 platform_extra_bits 却无 declaration（无从把关）" % s["id"])
            continue
        raw = dict(macro_raw)
        raw.update(_header_groups(root, decl["file"])[0])
        if decl.get("definition_file"):
            raw.update(_header_groups(root, decl["definition_file"])[0])
        body = _macro_body(root, decl["file"], decl["macro"])
        if body is None:
            body = _macro_body(root, decl.get("definition_file") or header_rel, decl["macro"])
        if body is None:
            fails.append("%s: platform_extra_bits 的声明宏 %s 无定义可解析" % (s["id"], decl["macro"]))
            continue
        have = set()
        for t in _features_of(body):
            have |= _canon(_header_bits(raw, t), aliases) if t in raw else _canon({t}, aliases)
        for plat in sorted(pex):
            miss = sorted({aliases.get(b, b) for b in pex[plat]} - have)
            if miss:
                fails.append("%s: 平台 %s 的许可位 %s 未出现在声明 %s 里 —— 用了却没声明"
                             % (s["id"], plat, miss, decl["macro"]))
    # S7 TU 级隔离（R-60 硬约束 1）: 变体 DSO 的**门面 TU**（握手/自检/注册表）不得带任何
    # ISA 旗标。非 GCC 工具链没有函数级指令集覆盖（GCC 侧靠函数级属性），门面 TU 一旦与高 ISA
    # 旗标同 TU，自检入口就可能被生成高指令集指令 ⇒「不支持该 ISA 的机器上干净拒绝」会退化成
    # 「加载期撞非法指令」。本判据在**构建输入**层把关；产物层由
    # eng/tools/quality/check_variant_isa_disasm.py（符号级反汇编断言）复核。
    for iso in (reg.get("tu_isolation") or []):
        key_k = (iso.get("build_input", ""), iso.get("kernels_target", ""))
        key_f = (iso.get("build_input", ""), iso.get("face_target", ""))
        if key_k not in sites:
            fails.append("%s: 计算面 target %s 无 ISA 旗标 —— 变体产物将与基线同码"
                         % (iso.get("id"), iso.get("kernels_target")))
        if key_f in sites:
            fails.append("%s: 门面 target %s 带了 ISA 旗标 %s —— 自检/握手入口与高指令集同 TU"
                         "（R-60 硬约束 1 违反）"
                         % (iso.get("id"), iso.get("face_target"), sorted(sites[key_f])))
    # S4 宿主 ISA
    exemptions = set(reg.get("exemptions") or [])
    for rel in build_inputs:
        p = os.path.join(root, rel)
        if not os.path.isfile(p):
            continue
        for raw in _read(p).splitlines():
            if raw.lstrip().startswith("#"):
                continue
            m = HOST_ISA.search(raw)
            if m and rel not in exemptions:
                fails.append("宿主 ISA 不得硬编码: %s 出现 %s（AGENTS §6；豁免面为空）"
                             % (rel, m.group(0)))
    # S5 状态词
    for s in reg.get("sites", []):
        if parse_ladder is not None and s.get("status") not in ladder:
            fails.append("%s: status=%r 越出 §12.5 阶梯 %s" % (s["id"], s.get("status"), ladder))
        # S6 正向状态需证据
        if s.get("status") in POSITIVE and not (s.get("declaration") and s.get("detection")):
            fails.append("%s: status=%s（正向）却缺 declaration/detection 证据"
                         % (s["id"], s.get("status")))
    return {"tool": "check_isa_same_source", "root": os.path.abspath(root),
            "build_inputs": len(build_inputs), "sites": len(sites),
            "pass": not fails, "fails": fails}


# ── 自证 ────────────────────────────────────────────────────────────────────
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


def _mini(root, *, decl_body="(ACS_FEAT_AVX2 | ACS_FEAT_FMA)", flags=("-mavx2", "-mfma"),
          reg_flags=None, posture="product_graph", status="IMPLEMENTED", extra_input=None,
          fmap=None, cmake_extra="", extra_reg=None):
    os.makedirs(os.path.join(root, "eng/tools/quality"), exist_ok=True)
    os.makedirs(os.path.join(root, "lib/x"), exist_ok=True)
    # DESIGN 位于 docs/ 子目录：父目录必须先建，否则自证夹具写入即 FileNotFoundError（门崩）。
    os.makedirs(os.path.join(root, os.path.dirname(DESIGN)), exist_ok=True)
    with open(os.path.join(root, DESIGN), "w", encoding="utf-8") as f:
        f.write(MINI_DESIGN)
    with open(os.path.join(root, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write("# c\nadd_library(t x.cpp)\ntarget_compile_options(t PRIVATE %s)\n%s"
                % (" ".join(flags), cmake_extra))
    with open(os.path.join(root, "lib/x/backend.cpp"), "w", encoding="utf-8") as f:
        f.write("#define ASTROCS_BACKEND_REQUIRED_FEATURES %s\n" % decl_body)
    with open(os.path.join(root, "lib/x/provider.h"), "w", encoding="utf-8") as f:
        f.write("#define ACS_CPU_X_REQUIRED_FEATURES (ACS_FEAT_AVX2 | ACS_FEAT_FMA)\n")
    with open(os.path.join(root, "lib/x/capability_v1.h"), "w", encoding="utf-8") as f:
        f.write("enum { ACS_FEAT_AVX2 = 1u << 4, ACS_FEAT_FMA = 1u << 5 };\n")
    reg = {"schema_version": 1, "baseline_flags": ["-msse2"],
           "flag_feature_map": (fmap if fmap is not None else
                                {"-mavx2": "ACS_FEAT_AVX2", "-mfma": "ACS_FEAT_FMA"}),
           "feature_bit_header": "lib/x/capability_v1.h", "exemptions": [],
           "sites": [{"id": "s", "build_input": "CMakeLists.txt", "posture": posture,
                      "target": "t", "flags": list(reg_flags if reg_flags is not None else flags),
                      "declaration": {"file": "lib/x/backend.cpp",
                                      "macro": "ASTROCS_BACKEND_REQUIRED_FEATURES"},
                      "detection": {"file": "lib/x/provider.h",
                                    "macro": "ACS_CPU_X_REQUIRED_FEATURES"},
                      "status": status}]}
    if extra_input:
        reg["sites"].append(extra_input)
    for k, v in (extra_reg or {}).items():
        reg[k] = v
    with open(os.path.join(root, REG), "w", encoding="utf-8") as f:
        json.dump(reg, f)
    return ["CMakeLists.txt"] + ([extra_input["build_input"]] if extra_input else [])


def self_test():
    cases = []
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "p")
        bi = _mini(p)
        cases.append(("pos-same-source", run(p, bi)["pass"], True))
        n1 = os.path.join(td, "n1")
        bi = _mini(n1, decl_body="(ACS_FEAT_AVX2)")
        cases.append(("neg-declaration-subset", run(n1, bi)["pass"], False))
        n2 = os.path.join(td, "n2")
        bi = _mini(n2, flags=("-mavx2", "-mfma"), reg_flags=("-mavx2",))
        cases.append(("neg-flag-drift", run(n2, bi)["pass"], False))
        n3 = os.path.join(td, "n3")
        bi = _mini(n3)
        with open(os.path.join(n3, "lib/x/Makefile"), "w", encoding="utf-8") as f:
            f.write("CFLAGS = -O2 -march=native\n")
        cases.append(("neg-unregistered-site", run(n3, bi + ["lib/x/Makefile"])["pass"], False))
        n4 = os.path.join(td, "n4")
        bi = _mini(n4)
        with open(os.path.join(n4, "lib/x/Makefile"), "w", encoding="utf-8") as f:
            f.write("CFLAGS = -O2 -march=native\n")
        cases.append(("neg-host-isa", run(n4, bi + ["lib/x/Makefile"])["pass"], False))
        n5 = os.path.join(td, "n5")
        bi = _mini(n5, status="SHIPPED")
        cases.append(("neg-status-out-of-ladder", run(n5, bi)["pass"], False))
        # R-60: MSVC /arch: 识别面（不认它 = 整条 MSVC 分支成为扫描盲区）
        m1 = os.path.join(td, "m1")
        bi = _mini(m1, flags=("/arch:AVX2",),
                   fmap={"/arch:AVX2": ["ACS_FEAT_AVX2", "ACS_FEAT_FMA"]})
        cases.append(("pos-msvc-arch-flag", run(m1, bi)["pass"], True))
        m2 = os.path.join(td, "m2")
        bi = _mini(m2, flags=("/arch:AVX2",))  # /arch: 未登记到 feature 位 ⇒ 必须红
        cases.append(("neg-msvc-arch-unmapped", run(m2, bi)["pass"], False))
        # R-60: S7 TU 级隔离（门面 TU 不得带 ISA 旗标）
        t1 = os.path.join(td, "t1")
        bi = _mini(t1, cmake_extra="add_library(t_face x.cpp)\n",
                   extra_reg={"tu_isolation": [{"id": "iso", "build_input": "CMakeLists.txt",
                                                "face_target": "t_face", "kernels_target": "t"}]})
        cases.append(("pos-tu-isolation", run(t1, bi)["pass"], True))
        t2 = os.path.join(td, "t2")
        bi = _mini(t2, cmake_extra="add_library(t_face x.cpp)\n",
                   extra_reg={"tu_isolation": [{"id": "iso", "build_input": "CMakeLists.txt",
                                                "face_target": "t", "kernels_target": "t"}]})
        cases.append(("neg-tu-isolation-face-flagged", run(t2, bi)["pass"], False))
        # R-60: S8 平台分支许可位（声明侧必须含该位）
        p1 = os.path.join(td, "p1")
        bi = _mini(p1, extra_reg={"platform_extra_bits": {}})
        with open(os.path.join(p1, REG), encoding="utf-8") as f:
            reg_p1 = json.load(f)
        reg_p1["sites"][0]["platform_extra_bits"] = {"MSVC/clang-cl": ["ACS_FEAT_FMA"]}
        with open(os.path.join(p1, REG), "w", encoding="utf-8") as f:
            json.dump(reg_p1, f)
        cases.append(("pos-platform-extra-bits", run(p1, bi)["pass"], True))
        p2 = os.path.join(td, "p2")
        bi = _mini(p2, decl_body="(ACS_FEAT_AVX2)", extra_reg={"platform_extra_bits": {}})
        with open(os.path.join(p2, REG), encoding="utf-8") as f:
            reg_p2 = json.load(f)
        reg_p2["sites"][0]["platform_extra_bits"] = {"MSVC/clang-cl": ["ACS_FEAT_AVX512CD"]}
        with open(os.path.join(p2, REG), "w", encoding="utf-8") as f:
            json.dump(reg_p2, f)
        cases.append(("neg-platform-extra-bits-missing", run(p2, bi)["pass"], False))
    ok = all(got is want for _, got, want in cases)
    for name, got, want in cases:
        print("SELFTEST_%s %s (pass=%s want=%s)" % ("PASS" if got is want else "FAIL",
                                                    name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


MIRROR_KEEP = (REG, DESIGN)


def fault_inject(name, root):
    """真仓构建输入复制到临时树后注入违规，目标判据必须判红。"""
    # 第二族（CPU provider 变体族）的同型注入：S1/S2/S7/S8 对该族同样必须能红，
    # 否则「判据只覆盖第一族」会退化成第二族无人把关。
    names = ("unregistered", "flag-drift", "declaration", "march", "tu-isolation",
             "arch-flag-drift", "cpuprov-tu-isolation", "cpuprov-kernels-unflagged",
             "cpuprov-arch-flag-drift", "cpuprov-declaration-cd")
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
        bi = collect_build_inputs(root)          # 判据口径：只有构建输入参与站点扫描
        copy_rels = list(bi)
        reg = json.loads(_read(os.path.join(root, REG)))
        for s in reg.get("sites", []):
            for key in ("declaration", "detection"):
                if s.get(key):
                    copy_rels.append(s[key]["file"])
            for extra in (s.get("extra_sides") or []):
                copy_rels.append(extra["file"])
                if extra.get("definition_file"):
                    copy_rels.append(extra["definition_file"])
        copy_rels += list(MIRROR_KEEP) + list(reg.get("macro_sources") or [])
        for rel in sorted(set(copy_rels)):
            src = os.path.join(root, rel)
            if os.path.isfile(src):
                d = os.path.join(dst, rel)
                os.makedirs(os.path.dirname(d) or dst, exist_ok=True)
                shutil.copy(src, d)
        if name == "unregistered":
            os.makedirs(os.path.join(dst, "lib/x_inj"), exist_ok=True)
            with open(os.path.join(dst, "lib/x_inj/Makefile"), "w", encoding="utf-8") as f:
                f.write("CFLAGS = -O2 -mavx2\n")
            bi.append("lib/x_inj/Makefile")
        elif name == "flag-drift":
            reg["sites"][1]["flags"] = [f for f in reg["sites"][1]["flags"]
                                        if f != "-mavx512dq"]
            with open(os.path.join(dst, REG), "w", encoding="utf-8") as f:
                json.dump(reg, f)
        elif name == "declaration":
            p = os.path.join(dst, reg["sites"][1]["declaration"]["file"])
            t = _read(p)
            # 注入 = 把声明退回"只看一个子集"的原缺陷形态（F-only）
            t = re.sub(r"ACS_FEAT_AVX512_PROVIDER_REQUIRED", "ACS_FEAT_AVX512F", t)
            t = re.sub(r"\(ACS_FEAT_AVX512F(?:\s*\|[^)]*)?\)", "(ACS_FEAT_AVX512F)", t)
            with open(p, "w", encoding="utf-8") as f:
                f.write(t)
        elif name == "tu-isolation":
            # R-60: 把 ISA 旗标挂到**门面** target 上（自检/握手入口与高指令集同 TU）⇒ 必须红
            p = os.path.join(dst, "CMakeLists.txt")
            with open(p, "a", encoding="utf-8") as f:
                f.write("\ntarget_compile_options(astrocs_cpu_avx2 PRIVATE /arch:AVX2)\n")
        elif name == "arch-flag-drift":
            # R-60: MSVC 侧旗标从登记表里掉一个（/arch:AVX512 被删）⇒ 旗标不一致必须红
            reg["sites"][1]["flags"] = [f for f in reg["sites"][1]["flags"]
                                        if f != "/arch:AVX512"]
            with open(os.path.join(dst, REG), "w", encoding="utf-8") as f:
                json.dump(reg, f)
        elif name == "cpuprov-tu-isolation":
            # R-60 第二族: 把 ISA 旗标挂回 **cpuprov 门面** target
            # （改前形态: astrocs_cpuprov_avx2 整 TU 带 -mavx2，握手入口与高指令集同 TU）
            # ⇒ S7 必须红。
            p = os.path.join(dst, "CMakeLists.txt")
            with open(p, "a", encoding="utf-8") as f:
                f.write(chr(10) + "target_compile_options(astrocs_cpuprov_avx2 PRIVATE -mavx2 -mfma)" + chr(10))
        elif name == "cpuprov-kernels-unflagged":
            # 第二族: 把计算面 target 的旗标整条摘掉（变体与基线同码 = 假变体）⇒ S7 必须红
            # （"kernels_target 无 ISA 旗标"分支，与"门面带旗标"分支互为反面）。
            # 两平台分支的旗标行都注掉（CMake 注释行不参与站点扫描），否则只剩
            # /arch:AVX2 时站点仍有旗标、只触发 S2 而碰不到 S7 这一分支。
            p = os.path.join(dst, "CMakeLists.txt")
            t = _read(p)
            for line in ("  target_compile_options(astrocs_cpuprov_avx2_kernels PRIVATE /arch:AVX2)",
                         "  target_compile_options(astrocs_cpuprov_avx2_kernels PRIVATE -mavx2 -mfma)"):
                t = t.replace(line, "# INJ " + line.strip())
            with open(p, "w", encoding="utf-8") as f:
                f.write(t)
        elif name == "cpuprov-arch-flag-drift":
            # 第二族: MSVC 侧 /arch:AVX512 从登记表掉一个 ⇒ S2 旗标不一致必须红
            # （改前该族的 MSVC 腿被 if(NOT MSVC) 门控、**连旗标都没有**，
            #   登记表补上 /arch: 后若构建输入没同步，这里立刻判红）。
            site = next(s for s in reg["sites"] if s["id"] == "product-cpuprov-avx512")
            site["flags"] = [f for f in site["flags"] if f != "/arch:AVX512"]
            with open(os.path.join(dst, REG), "w", encoding="utf-8") as f:
                json.dump(reg, f)
        elif name == "cpuprov-declaration-cd":
            # 第二族声明面: 把 provider 的 AVX-512 声明退回"只看一个子集"的原缺陷形态
            # （ACS_CAP_GROUP_AVX512_SUBSET 去掉 CD）⇒ S3/S8 必须红
            # （该族确实带 -mavx512cd 编译，声明少一位 = "用了却没声明"）。
            site = next(s for s in reg["sites"] if s["id"] == "product-cpuprov-avx512")
            # 组宏 ACS_CAP_GROUP_AVX512_SUBSET 定义在 macro_sources 的 capability_v1.h,
            # 必须一并改；只改声明头不碰组宏定义 = 注入不生效（实测踩过: red=0）。
            for rel in {site["declaration"]["file"], site["detection"]["file"],
                        site["declaration"].get("definition_file") or "",
                        *(reg.get("macro_sources") or [])}:
                if not rel:
                    continue
                fp = os.path.join(dst, rel)
                if not os.path.isfile(fp):
                    continue
                t = _read(fp)
                t2 = re.sub(r"(ACS_CAP_FEAT_AVX512CD\s*\|\s*)", "", t)
                t2 = re.sub(r"(\|\s*ACS_CAP_FEAT_AVX512CD\b)", "", t2)
                if t2 != t:
                    with open(fp, "w", encoding="utf-8") as f:
                        f.write(t2)
        elif name == "march":
            p = os.path.join(dst, "lib/algorithms/psf/Makefile")
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as f:
                f.write("CXXFLAGS = -O2 -march=native\n")
            bi.append("lib/algorithms/psf/Makefile")
        res = run(dst, sorted(set(bi)))
        print("FAULT_INJECT %s red=%d" % (name, len(res["fails"])))
        for m in res["fails"][:3]:
            print("    %s" % m)
        return 0 if res["fails"] else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="ISA 三侧同源门（S2-C）")
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
        print("ISA_SAME_SOURCE %s build_inputs=%d sites=%d fails=%d"
              % ("PASS" if res["pass"] else "FAIL", res.get("build_inputs", 0),
                 res.get("sites", 0), len(res["fails"])))
        for m in res["fails"][:8]:
            print("  - %s" % m)
    else:
        print(json.dumps(res, ensure_ascii=False, indent=1, sort_keys=True))
    return 0 if res["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
