#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_manifest_isa_artifact.py — 清单声明面 ↔ **产物**双向判据（ISA 声明收口 R-60/R-61）。

为什么需要它（与既有判据的分工，一层一层不多不少）:
  · check_isa_same_source.py      = 构建输入层（旗标站点 ↔ 声明 ↔ 检测三侧同源）
  · gen_provider_manifests.py    = 清单**生成**层（声明位必须由构建口径推导，生成期拒出）
  · check_variant_isa_disasm.py  = 产物的**符号级**ISA 面（某 TU 真/假变体、门面零宽指令）
  · 本脚本                      = 清单 ↔ 产物的**逐条目交叉**：清单里写了什么，产物里必须真的有。
    前面三条都不看「清单与被它描述的那个 .so/.dll 是否对得上」—— 生成期清单与构建期产物
    可以各跑各的（改了旗标没重生成清单、换了产物没重跑生成器），只有交叉判据能抓住。

判据（全红才算红；无豁免面）:
  M1 清单可解析, kind ∈ {astrocs_backends_manifest, astrocs_providers_manifest}, backends 非空。
  M2 条目声明位与 **cpu_features.h 唯一事实源**逐位一致（required_features_names 重算
     required_features_bits 必须相等）—— 清单里出现位表里没有的位名即红。
  M3 条目 file 在 --artifacts-dir 实存（缺 ⇒ 红）；产物容器必须可识别（判不出 ⇒ 红，
     不猜"大概是 PE"）。容器支持 ELF 与 PE/COFF 两族（见 check_variant_isa_disasm.detect_objfmt）。
  M4 **档位证据（本脚本的主判据）**: 清单声明的档位必须在产物里有「该档才可能发射」的证据。
       · 声明含 avx512f ⇒ 产物须有 ≥1 条 EVEX(首字节 0x62) 编码指令，或 zmm 寄存器
       · 声明含 avx2   ⇒ 产物须有 ≥1 条 VEX/EVEX(0xC4/0xC5/0x62) 编码指令，或 ymm 寄存器
       · 声明为空（基线）⇒ 产物必须**零** VEX/EVEX
     「清单声明了 AVX-512 但产物里没有」= 虚假能力声明/变体与基线同码 ⇒ 判红。
     喂不匹配的产物（拿 AVX2 的 .so 顶 AVX-512 条目，或拿门面 .o 顶变体条目）必红。
  M5 **反向安全方向**: 产物里**实测用到**的位（按 FEATURE_EVIDENCE 表逐位取证，且 AVX-512
     档的证据额外要求 EVEX 编码）必须都在清单声明里。用了却没声明 = 加载预检
     (required ⊆ detected) 放行后首调撞非法指令 ⇒ 判红。
  M6 build.flags（逐变体构建旗标）必须等于旗标站点口径表推导出的适用旗标串 ——
     该字段此前是「写进 JSON 但没人读」的死字段；本判据把它接上真实口径（站点 = 站点 id，
     清单必须写 isa_site_map）。站点未登记 / 清单未登记站点映射 ⇒ 红。

用法:
  python3 eng/tools/quality/check_manifest_isa_artifact.py \
      --manifest build/providers/backends.manifest.json --artifacts-dir build/providers
  python3 eng/tools/quality/check_manifest_isa_artifact.py --self-test
退出码: 0 = 全绿；1 = 至少一条判红；2 = 用法/输入错误。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)                       # eng/tools
for _p in (HERE, TOOLS):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import check_variant_isa_disasm as cv  # noqa: E402  产物证据表/反汇编读取（唯一实现点）
import isa_feature_bits               # noqa: E402
import isa_sites                      # noqa: E402

MANIFEST_KINDS = ("astrocs_backends_manifest", "astrocs_providers_manifest")
# 档位 → 判据口径。键是"该档的特征位"，值是"产物里必须能证明的编码/寄存器"。
TIER_PROBES = (
    ("avx512f", "AVX-512 档"),
    ("avx2", "AVX2 档"),
)
# 清单条目 backend_id → isa_sites.json 站点 id。清单里没有 backend_id → 站点映射
# ⇒ M6 无法核对旗标，判红（不得"看着像口径"却无从核对）。
DEFAULT_SITE_MAP = {
    "avx2": "product-avx2",
    "avx512": "product-avx512",
}


class _A:
    """极简参数容器（read_text 只用 binary/text/objdump/objfmt/dumpbin 五个字段）。"""

    def __init__(self, binary=None, text=None, objdump="", objfmt="auto", dumpbin=""):
        self.binary, self.text = binary, text
        self.objdump, self.objfmt, self.dumpbin = objdump, objfmt, dumpbin


def _disasm(path, objdump="", objfmt="auto", dumpbin=""):
    """产物 → 反汇编文本。走产物面判据的读取口（同一套容器识别与反汇编器选择）。"""
    return cv.read_text(_A(binary=pathlib.Path(path), objdump=objdump,
                           objfmt=objfmt, dumpbin=dumpbin))


def _encoding_counts(text):
    n = {"evex": 0, "vex": 0, "legacy": 0, "unknown": 0}
    for ln in text.splitlines():
        if ln.strip():
            n[cv.encoding_of(ln)] += 1
    return n


def _has_width(text, width):
    """该宽度的向量寄存器出现行数（%ymm / %zmm，含/不含 % 两种反汇编写法）。"""
    rx = re.compile(r"%" + width + r"mm\d+|\b" + width + r"mm\d+")
    return sum(1 for ln in text.splitlines() if rx.search(ln))


def declared_tier(names):
    """清单声明集 → 最高档位键（'avx512f' / 'avx2' / None=基线）。按档位从高到低取第一个。"""
    s = set(names)
    for key, _label in TIER_PROBES:
        if key in s:
            return key
    return None


def check_entry(entry, artifacts_dir, fb, reg, objdump, objfmt, dumpbin, site_map):
    """单条目 → (fails, logs)。"""
    fails, logs = [], []
    tag = str(entry.get("backend_id") or entry.get("file") or "?")
    names = [str(n).strip().lower() for n in (entry.get("required_features_names") or [])]
    bits = entry.get("required_features_bits")

    # M2 位值唯一事实源
    calc = 0
    for n in names:
        try:
            calc |= fb.mask(n)
        except isa_feature_bits.UnknownFeature as exc:
            fails.append("M2 %s: 声明位 %r 不在 cpu_features.h（%s）—— 位表漂移" % (tag, n, exc))
    if not isinstance(bits, int):
        fails.append("M2 %s: required_features_bits 缺失或非整数（%r）" % (tag, bits))
    elif calc != bits:
        fails.append("M2 %s: 声明位重算 %d != 清单 %d（位表漂移）" % (tag, calc, bits))
    if not names:
        fails.append("M4 %s: 条目没有任何声明位（required_features_names 为空）" % tag)

    # M3 产物存在 + 容器可识别
    fname = str(entry.get("file") or "")
    path = os.path.join(artifacts_dir, fname)
    if not fname or not os.path.isfile(path):
        fails.append("M3 %s: 产物不存在 %s（清单声明的条目必须有实物可核）" % (tag, path))
        return fails, logs
    try:
        fmt = cv.detect_objfmt(path) if objfmt == "auto" else objfmt
    except (OSError, ValueError) as exc:
        fails.append("M3 %s: %s" % (tag, exc))
        return fails, logs
    try:
        text = _disasm(path, objdump, objfmt, dumpbin)
    except SystemExit as exc:                      # read_text 内部判红后退出
        fails.append("M3 %s: 反汇编读取失败 rc=%s（产物 %s）" % (tag, exc.code, path))
        return fails, logs
    if not text.strip():
        fails.append("M3 %s: 反汇编文本为空（断言不得空转）" % tag)
        return fails, logs
    enc = _encoding_counts(text)
    logs.append("%s: 容器=%s 指令行 evex=%d vex=%d legacy=%d unknown=%d"
                % (tag, fmt, enc["evex"], enc["vex"], enc["legacy"], enc["unknown"]))
    if enc["unknown"] and (enc["evex"] + enc["vex"]) == 0 and names:
        fails.append("M4 %s: 反汇编文本**无指令字节列**（编码层判据不可判）—— fail-closed；"
                     "请用 objdump/llvm-objdump/dumpbin /disasm 的带字节输出" % tag)

    # M4 档位证据（声明了但产物里没有 ⇒ 红）
    tier = declared_tier(names)
    if tier == "avx512f":
        if enc["evex"] == 0 and _has_width(text, "z") == 0:
            fails.append("M4 %s: 清单声明 AVX-512（%s），产物里零 EVEX(0x62) 指令且零 zmm —— "
                         "「声明了但产物里没有」（变体与基线同码 / 虚假能力声明）" % (tag, names))
        else:
            logs.append("%s: AVX-512 档证据成立（EVEX %d 条 / zmm %d 行）"
                        % (tag, enc["evex"], _has_width(text, "z")))
    elif tier == "avx2":
        if enc["evex"] == 0 and enc["vex"] == 0 and _has_width(text, "y") == 0:
            fails.append("M4 %s: 清单声明 AVX2（%s），产物里零 VEX/EVEX 编码指令且零 ymm —— "
                         "「声明了但产物里没有」（变体与基线同码 / 虚假能力声明）" % (tag, names))
        else:
            logs.append("%s: AVX2 档证据成立（VEX %d + EVEX %d 条 / ymm %d 行）"
                        % (tag, enc["vex"], enc["evex"], _has_width(text, "y")))
    else:
        ops = [ln for ln in text.splitlines() if cv.encoding_of(ln) in ("vex", "evex")]
        if ops:
            fails.append("M4 %s: 清单未声明任何宽指令档位（%s），产物却含 VEX/EVEX 指令 %d 条，"
                         "例: %s" % (tag, names, len(ops), ops[0].strip()[:96]))
        else:
            logs.append("%s: 基线面成立（零 VEX/EVEX）" % tag)

    # M5 反向：产物实测用到的位必须都在清单声明里
    used, unused = [], []
    for feat in sorted(cv.FEATURE_EVIDENCE):
        n = cv.feature_evidence_count(text, feat)
        if n is None:
            fails.append("M5 %s: 证据表无此位 %r（fail-closed）" % (tag, feat))
            continue
        if n:
            (used if feat in names else unused).append("%s(%d)" % (feat, n))
    if unused:
        fails.append("M5 %s: 产物实测用到但清单未声明的位 %s —— 加载预检(required ⊆ detected)"
                     "会放行到不支持的机器，首调撞非法指令" % (tag, unused))
    if used:
        logs.append("%s: 声明位实测有使用证据 %s" % (tag, used))
    return fails, logs


def check_flags_field(doc, reg, site_map):
    """M6: build.flags 逐变体旗标 == 站点口径表推导值（死字段接上真实口径）。"""
    fails, logs = [], []
    build = doc.get("build") or {}
    flags = build.get("flags")
    if not isinstance(flags, dict) or not flags:
        fails.append("M6: build.flags 缺失或非对象（逐变体构建旗标无从核对）")
        return fails, logs
    smap = site_map if site_map is not None else build.get("isa_site_map")
    for bid in sorted(flags):
        if bid in (None, "", "baseline"):
            continue                        # 基线本身无旗标站点（它就是基线 ISA 面）
        site_id = (smap or {}).get(bid) or DEFAULT_SITE_MAP.get(bid)
        if not site_id:
            fails.append("M6 %s: 清单未给出 isa_site 映射且不在默认表内 —— build.flags 无从核对"
                         % bid)
            continue
        site = isa_sites.site_of(reg, site_id)
        if site is None:
            fails.append("M6 %s: 站点 %s 未登记在 isa_sites.json" % (bid, site_id))
            continue
        want = " ".join(isa_sites.flags_for(site, isa_sites.GNU_PLATFORM))
        for plat in (isa_sites.GNU_PLATFORM, isa_sites.MSVC_PLATFORM):
            p_flags = " ".join(isa_sites.flags_for(site, plat))
            if plat == reg.get("platform_of_manifest"):
                want = p_flags
        if str(flags[bid]).strip() != want.strip():
            fails.append("M6 %s: build.flags=%r != 站点 %s 推导的适用旗标 %r —— "
                         "清单记录的构建口径与旗标站点表漂移"
                         % (bid, flags[bid], site_id, want))
        else:
            logs.append("M6 %s: build.flags == 站点 %s 推导值 %r" % (bid, site_id, want))
    return fails, logs


INJECTIONS = ("declared-avx512-artifact-avx2", "unknown-feature-name", "bits-drift")


def apply_injection(doc, name):
    """负例注入：把清单指向**不匹配**的产物/位表，证明判据能红（不是恒真门）。

    这是负例自证的机器可执行面，与 --self-test 的合成夹具互为独立两路证据：
    一路在夹具上证明"该红时红"，一路在**真实清单**上证明"换个产物就红"。
    注入只改内存中的 doc，不落盘。
    """
    if name not in INJECTIONS:
        raise ValueError("未知注入名 %r（允许 %s）" % (name, "/".join(INJECTIONS)))
    entries = doc.get("backends") or []
    if name == "declared-avx512-artifact-avx2":
        # 取档位**最低**的那条产物，把所有更高档位的条目都指过去 ——
        # 这正是原事故形态（清单声明 AVX-512，产物其实是低档变体 / 与基线同码）。
        rank = {"avx512f": 2, "avx2": 1}

        def _r(e):
            return rank.get(declared_tier(e.get("required_features_names") or []) or "", 0)
        if not entries or _r(entries[0]) == _r(entries[-1]):
            raise ValueError("注入 %s 需要清单里存在**两个不同档位**的条目" % name)
        lowest = min(entries, key=_r)
        for e in entries:
            if _r(e) > _r(lowest):
                e["file"] = lowest["file"]          # 故意指错产物
    elif name == "unknown-feature-name":
        entries[0]["required_features_names"] = ["avx2", "avx2048f"]
    elif name == "bits-drift":
        entries[0]["required_features_bits"] = 7
    return doc


def run(manifest, artifacts_dir, repo=None, objdump="", objfmt="auto", dumpbin="",
        site_map=None, quiet=False, inject=""):
    fails, logs = [], []
    try:
        doc = json.load(open(manifest, encoding="utf-8"))
    except (OSError, ValueError) as exc:
        return ["M1: 清单读不出/非 JSON: %s (%s)" % (manifest, exc)], logs
    if inject:
        try:
            doc = apply_injection(doc, inject)
        except ValueError as exc:
            return ["M1: 负例注入失败: %s" % exc], logs
        logs.append("负例注入已施加: %s（判据必须因此判红）" % inject)
    kind = doc.get("kind")
    if kind not in MANIFEST_KINDS:
        fails.append("M1: kind=%r 不在 %s" % (kind, "/".join(MANIFEST_KINDS)))
    entries = doc.get("backends") or []
    if not entries:
        fails.append("M1: backends 为空（无条目可核 = 断言空转）")
    fb = isa_feature_bits.FeatureBits.load(repo_root=repo)
    reg = isa_sites.load()
    if site_map is None:
        plat = reg.get("platform_of_manifest")
        if plat:
            site_map = None          # 由 check_flags_field 读 doc.build.isa_site_map
    for e in entries:
        ef, el = check_entry(e, artifacts_dir, fb, reg, objdump, objfmt, dumpbin, site_map)
        fails += ef
        logs += el
    ff, fl = check_flags_field(doc, reg, site_map)
    fails += ff
    logs += fl
    if not quiet:
        for m in logs:
            print("  ok  %s" % m)
    for m in fails:
        print("FAIL %s" % m)
    print("MANIFEST_ISA_ARTIFACT %s entries=%d fails=%d"
          % ("PASS" if not fails else "FAIL", len(entries), len(fails)))
    return fails, logs


# ── 自证夹具 ────────────────────────────────────────────────────────────────
# 用真 ELF/COFF 容器（首部魔数 + 最小可反汇编体）承载**手写**反汇编语义的最小产物：
# 判据要证明的是"清单↔产物"这条交叉，故夹具必须同时变动两侧。
def _elf(text_bytes=b"\x7fELF\x02\x01\x01\x00" + b"\x00" * 60):
    return text_bytes


def _coff(text_bytes=b"\x64\x86\x05\x00" + b"\x00" * 60):
    return text_bytes


DIS_AVX512 = """0000000000000000 <astrocs_variant_kernel_dispatch_v1>:
       0: 62 f1 7c 48 10 03              \tvmovups\t(%rbx), %zmm0
       7: 62 f1 7e 08 7b c6              \tvcvtusi2ss\t%esi, %xmm12, %xmm0
       d: c3                             \tretq
"""
DIS_AVX2 = """0000000000000000 <astrocs_variant_kernel_dispatch_v1>:
       0: c5 fc 10 03                    \tvmovups\t(%rbx), %ymm0
       7: c4 e2 75 a8 c2                 \tvfmadd213ss\t%xmm2, %xmm1, %xmm0
       c: c3                             \tretq
"""
DIS_AVX2_WITH_VL_LOOKALIKE = DIS_AVX2 + """0000000000000020 <other>:
    20: c5 fd d4 c1                    \tvpaddq\t%ymm1, %ymm0, %ymm0
    25: c3                             \tretq
"""
DIS_BASELINE = """0000000000000000 <astrocs_backend_get_api_v1>:
       0: 48 89 5c 24 08                \tmov\t%rbx, 8(%rsp)
       5: c3                             \tretq
"""


FIXTURE_DIS = (("a512.elf", DIS_AVX512, "elf"), ("a2.elf", DIS_AVX2, "elf"),
                 ("a2vl.elf", DIS_AVX2_WITH_VL_LOOKALIKE, "elf"),
                 ("base.elf", DIS_BASELINE, "elf"),
                 ("a512.obj", DIS_AVX512, "pe-coff"), ("a2.obj", DIS_AVX2, "pe-coff"))


def _make_fixtures(td):
    """落全部夹具产物（真容器魔数 + 假内容），并返回 路径→反汇编文本 的映射表。"""
    table = {}
    for name, dis, _fmt in FIXTURE_DIS:
        p = pathlib.Path(td) / name
        p.write_bytes(_elf() if name.endswith(".elf") else _coff())
        table[str(p)] = dis
    return table


def _fake_objdump(table):
    """假 objdump：按产物路径回放自证反汇编文本（只服务 --self-test）。"""
    def run(cmd, *_a, **_k):
        class _R:
            returncode, stderr = 0, ""
            stdout = table.get(str(cmd[-1]), "")
        return _R()
    return run


def _manifest(path, names, bits, fname, build_flags=None, isa_site_map=None):
    doc = {"schema_version": "1", "kind": "astrocs_backends_manifest",
           "features_defined": sorted(isa_feature_bits.FeatureBits.load().names),
           "backends": [{"file": fname, "backend_id": fname.split(".")[0].replace("a", "")
                         or "avx2", "abi_version": 1,
                         "required_features_bits": bits,
                         "required_features_names": names}],
           "build": {"flags": build_flags if build_flags is not None
                     else {"avx2": "-mavx2 -mfma"}, "abi_version": 1, "compiler": "GNU-14.2.0"}}
    if isa_site_map is not None:
        doc["build"]["isa_site_map"] = isa_site_map
    json.dump(doc, open(path, "w", encoding="utf-8"), indent=2)
    return doc


def self_test():
    """红/绿双向自证：同一批夹具上每条判据都必须给出预期结果。"""
    import isa_feature_bits as ifb
    fb = ifb.FeatureBits.load()
    cases = []
    real_run = cv.subprocess.run
    with tempfile.TemporaryDirectory() as td:
        table = _make_fixtures(td)
        cv.subprocess.run = _fake_objdump(table)
        try:
            a512_bits = fb.bits_of(["avx512f", "avx512bw", "avx512dq", "avx512vl"])
            a2_bits = fb.bits_of(["avx2", "fma"])
            mf512 = _manifest(os.path.join(td, "m512.json"),
                              ["avx512f", "avx512bw", "avx512dq", "avx512vl"], a512_bits,
                              "a512.elf", build_flags={"avx512": "-mavx512f -mavx512bw -mavx512vl "
                                                                "-mavx512dq"})
            mf2 = _manifest(os.path.join(td, "m2.json"), ["avx2", "fma"], a2_bits,
                            "a2.elf", build_flags={"avx2": "-mavx2 -mfma"})
            mf2vl = _manifest(os.path.join(td, "m2vl.json"), ["avx2", "fma"], a2_bits,
                              "a2vl.elf", build_flags={"avx2": "-mavx2 -mfma"})
            mf2_512 = _manifest(os.path.join(td, "m2_512.json"),
                                ["avx512f", "avx512bw", "avx512dq", "avx512vl"], a512_bits,
                                "a2.elf", build_flags={"avx2": "-mavx2 -mfma"})
            mfbase = _manifest(os.path.join(td, "mbase.json"), [], 0, "base.elf",
                               build_flags={"baseline": "(none; amd64 SSE2 基线)"})
            mfwrong = _manifest(os.path.join(td, "mwrong.json"),
                                ["avx512f", "avx512bw", "avx512dq", "avx512vl"], a512_bits,
                                "a512.elf",
                                build_flags={"avx512": "-mavx512f"})   # M6 旗标漂移

            def go(mf, fn=None):
                fails, _ = run(mf, td, quiet=True, **({"objdump": "fake"} if not fn else fn))
                return 0 if not fails else 1

            cases.append(("pos-avx512-matches", go(os.path.join(td, "m512.json")), 0))
            cases.append(("pos-avx2-matches", go(os.path.join(td, "m2.json")), 0))
            # 「清单声明了但产物里没有」: AVX2 产物顶 AVX-512 条目
            cases.append(("neg-declared-avx512-artifact-avx2",
                          go(os.path.join(td, "m2_512.json")), 1))
            # 同名位在 AVX-512 产物上不得被 VEX 外观指令蹭绿（avx2vl 夹具只有 VEX）
            cases.append(("pos-avx2-no-vl-lookalike", go(os.path.join(td, "m2vl.json")), 0))
            # 位表漂移（M2）
            bad = json.load(open(os.path.join(td, "m2.json"), encoding="utf-8"))
            bad["backends"][0]["required_features_bits"] = 7
            json.dump(bad, open(os.path.join(td, "mbits.json"), "w"))
            cases.append(("neg-bits-drift", go(os.path.join(td, "mbits.json")), 1))
            # 声明位不在 cpu_features.h（M2 fail-closed）
            bad2 = json.load(open(os.path.join(td, "m2.json"), encoding="utf-8"))
            bad2["backends"][0]["required_features_names"] = ["avx2", "avx1024f"]
            json.dump(bad2, open(os.path.join(td, "mname.json"), "w"))
            cases.append(("neg-unknown-bit-name", go(os.path.join(td, "mname.json")), 1))
            # 产物缺失（M3）
            gone = _manifest(os.path.join(td, "mgone.json"), ["avx2", "fma"], a2_bits,
                             "nosuch.elf")
            cases.append(("neg-artifact-missing", go(os.path.join(td, "mgone.json")), 1))
            # 基线条目遇到带 VEX 的产物（M4 反向）
            cases.append(("neg-baseline-declares-nothing-but-artifact-has-vex",
                          go(os.path.join(td, "mbase.json")), 1))
            # 旗标口径漂移（M6）
            cases.append(("neg-flags-drift", go(os.path.join(td, "mwrong.json")), 1))
            # COFF 容器走同一套断言
            mf512o = _manifest(os.path.join(td, "m512o.json"),
                               ["avx512f", "avx512bw", "avx512dq", "avx512vl"], a512_bits,
                               "a512.obj",
                               build_flags={"avx512": "-mavx512f -mavx512bw -mavx512vl "
                                                     "-mavx512dq"})
            cases.append(("pos-coff-avx512-matches", go(os.path.join(td, "m512o.json")), 0))
            mf2o = _manifest(os.path.join(td, "m2o.json"),
                             ["avx512f", "avx512bw", "avx512dq", "avx512vl"], a512_bits,
                             "a2.obj", build_flags={"avx2": "-mavx2 -mfma"})
            cases.append(("neg-coff-declared-avx512-artifact-avx2",
                          go(os.path.join(td, "m2o.json")), 1))
            # M5: 产物实测用到但清单未声明（去掉 fma 声明，产物里有 vfmadd）
            mf5 = _manifest(os.path.join(td, "m5.json"), ["avx2"], fb.bits_of(["avx2"]),
                            "a2.elf", build_flags={"avx2": "-mavx2"})
            cases.append(("neg-used-but-not-declared", go(os.path.join(td, "m5.json")), 1))
        finally:
            cv.subprocess.run = real_run
    ok = all(got == want for _, got, want in cases)
    for name, got, want in cases:
        print("SELFTEST_%s %s (rc=%d want=%d)"
              % ("PASS" if got == want else "FAIL", name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="清单声明面 ↔ 产物双向判据")
    ap.add_argument("--manifest", default="")
    ap.add_argument("--artifacts-dir", default="")
    ap.add_argument("--repo", default="")
    ap.add_argument("--objdump", default="", help="反汇编器可执行名（默认按容器自动选）")
    ap.add_argument("--objfmt", default="auto", choices=cv.OBJFMT)
    ap.add_argument("--dumpbin", default="", help="Windows dumpbin 可执行（走 /disasm）")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--inject", default="", choices=[""] + list(INJECTIONS),
                    help="负例注入：把清单指向不匹配的产物/位表，判据**必须**因此判红")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not a.manifest or not a.artifacts_dir:
        print("用法: --manifest <json> --artifacts-dir <dir>（或 --self-test）", file=sys.stderr)
        return 2
    fails, _ = run(a.manifest, a.artifacts_dir, repo=a.repo or None, objdump=a.objdump,
                   objfmt=a.objfmt, dumpbin=a.dumpbin, quiet=a.quiet, inject=a.inject)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
