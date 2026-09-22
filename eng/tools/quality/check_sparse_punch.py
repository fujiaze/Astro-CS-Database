#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-SPARSE-PUNCH / CHK-SPARSE-PUNCH-PROBE：裸形态「文件系统打洞」判据。

权威依据
  - ASTROCS_DESIGN.md §10（裸形态体积削减两种机制的分工、生效面、失败语义）；
  - docs/design/PRODUCT_STORAGE_FORM.md §9.1（打洞：何时 / 对谁 / 失败怎么办 / 如何验证）；
  - docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7 表 T1（冻结规则 + 五条判据）
    与表 T2（包围盒 TRIM：显式 opt-in、默认不启用）；
  - ACCEPTANCE_SPEC.md §3.2（裸形态体积削减验收）；
  - ENGINEERING_SPEC.md §8（每项检查必须有可执行正例与负例、能红能绿、fail-closed）、
    §11（打洞在 fsync 之后、算哈希与原子发布之前完成，不得改变文件字节）。

判据分三层
  T1 静态不变量（对**生产源**断言，不依赖运行环境）
     S1  生产头存在（缺失即红，不静默跳过）
     S2  打洞路径**零浮点**（类型/字面量/比较）——IEEE -0.0 在浮点比较下等于 0.0
         但其位型含非零字节；用浮点判定会把 -0.0 区打成洞并改变文件字节
     S3  可打洞谓词是**字节级**（逐字节 != 0）
     S4  打洞系统调用**唯一实现**：一次 fallocate，且带 PUNCH_HOLE | KEEP_SIZE
     S5  Windows 分支存在（FSCTL_SET_SPARSE + FSCTL_SET_ZERO_DATA）
     S6  块粒度：Linux 4 KiB / Windows 64 KiB（合同 §7 T1 冻结值）
     S7  降级先于接触：能力探测在**任何打洞调用之前**（不支持 ⇒ 一个字节都不动）
     S8  读回复算存在：打洞前后整文件 sha256 比对，不一致 ⇒ PUNCH_VERIFY_MISMATCH
     S9  全仓首方 C/C++ 中，aio 之外**零**打洞原语（aio 是文件级唯一 I/O 边界）
     S10 写端接线：打洞在 fsync 之后、原子 rename 之前；读回不一致 ⇒ 不发布
     S11 打洞入口在 aio 之外零调用点；归档/容器路径零打洞
     TA  包围盒 TRIM **默认不开**：全仓首方源零 TRIM 关键字写入者
     TB  半开禁止：若出现 TRIM1/TRIM2 写入者，同文件必须同时写 ONAXIS1/ONAXIS2
     TC  文档登记存在：合同 §7 T2 行与设计 §9.2 明写「默认不启用」与四个关键字
  T2 谓词模型 + 判据判别力（纯 Python，可跨平台跑）
     - 与 T1 同源的字节谓词模型；对确定性语料逐块断言
     - **负例注入**：把谓词换成浮点等值判定的「变异体」，必须在 -0.0 语料上判错
       ⇒ 证明判据不是恒真门
  T3 真实系统调用（--probe-run，Linux；编译生产头为探针后运行）
     - 差分：探针的 C++ 谓词 vs 本文件的 Python 模型，逐块必须一致
     - 合成 FITS（NaN 边距 + 连续全零带 + DATASUM/CHECKSUM）：打洞后
       整文件 sha256 / st_size / st_blocks / astropy（memmap 与非 memmap）/
       cfitsio 交叉读器（header 卡片与数据区原始字节）/ 跨洞边界 pread 全等
     - 真实 AstroCS 瓦片（signal 与 support）：同上；signal 的 NaN 边距不得被打洞
     - 负例红：对 NaN 区强制打洞 ⇒ 必须判红；对数据区强制打洞 ⇒ 必须判红
     - 降级：卷不支持（注入等价于 EOPNOTSUPP）⇒ 跳过、文件逐字节不变、不阻断
     - 幂等语义：对已打洞文件再打一次 ⇒ 无新释放（NO_RELEASE），字节仍不变

用法
  python3 eng/tools/quality/check_sparse_punch.py --self-test [--json-out P]
  python3 eng/tools/quality/check_sparse_punch.py --probe-run [--json-out P] [--workdir D]
退出码：0 = PASS；1 = FAIL（含任一 case 判红）；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# ── 锚（缺失即红，不静默跳过）────────────────────────────────────────────────
ANCHOR_PUNCH_HEADER = "lib/infrastructure/aio/src/aio_sparse_punch.h"
ANCHOR_WRITER = "lib/infrastructure/aio/src/hips/aio_hips_writer.cpp"
ANCHOR_PROBE_SRC = "eng/tools/quality/sparse_punch_probe.cpp"
ANCHOR_CROSS_READER = "eng/tools/quality/fits_cross_reader.c"
ANCHOR_CONTRACT = "docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md"
ANCHOR_DESIGN = "docs/design/PRODUCT_STORAGE_FORM.md"
ANCHOR_ACCEPT = "ACCEPTANCE_SPEC.md"
ANCHOR_ENGINEERING = "ENGINEERING_SPEC.md"

AIO_PREFIX = "lib/infrastructure/aio/"
# 打洞入口的**判据工具**（非生产调用点）：探针由生产头编译，必须能调这两个入口。
JUDGMENT_TOOLS = ("eng/tools/quality/sparse_punch_probe.cpp",)
SRC_EXT = (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".inl")
SCAN_ROOTS = ("lib", "eng/tests", "lib/include", "eng/tools", "eng/ci", "eng/cli", "runtime")
EXCLUDE_PARTS = ("/third_party/", "/build/", "/run/", "/.git/", "/archive/", "/legacy/",
                 "/__pycache__/", "/node_modules/", "/.venv/")

TRIM_KEYWORDS = ("TRIM1", "TRIM2", "ONAXIS1", "ONAXIS2")

# 打洞粒度（合同 §7 T1：Linux 4 KiB；Windows 释放粒度 64 KiB 且须 64 KiB 对齐）
BLOCK_LINUX = 4096
BLOCK_WINDOWS = 65536

# 探针返回码（与 aio_sparse_punch.h 的 PunchRc 逐值一致）
RC_OK = 0
RC_UNSUPPORTED = 1
RC_VERIFY_MISMATCH = 2
RC_IO_ERROR = 3
RC_NO_RELEASE = 4


# ── 文本处理 ────────────────────────────────────────────────────────────────
def mask_noncode(text: str) -> str:
    """把注释与字符串/字符字面量的内容替换为空格（保留换行与列位置）。

    目的：静态断言只针对**实现**，不针对注释里引用的条款文字与错误消息文本。
    """
    out = []
    i = 0
    n = len(text)
    state = 0  # 0=code 1=line-comment 2=block-comment 3=string 4=char
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == 0:
            if c == "/" and nxt == "/":
                state = 1
                out.append("  ")
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = 2
                out.append("  ")
                i += 2
                continue
            if c == '"':
                state = 3
                out.append(" ")
                i += 1
                continue
            if c == "'":
                state = 4
                out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
            continue
        if state == 1:
            if c == "\n":
                state = 0
                out.append("\n")
            else:
                out.append(" ")
            i += 1
            continue
        if state == 2:
            if c == "*" and nxt == "/":
                state = 0
                out.append("  ")
                i += 2
                continue
            out.append("\n" if c == "\n" else " ")
            i += 1
            continue
        # string / char
        if c == "\\":
            out.append("  ")
            i += 2
            continue
        if (state == 3 and c == '"') or (state == 4 and c == "'"):
            state = 0
            out.append(" ")
            i += 1
            continue
        out.append("\n" if c == "\n" else " ")
        i += 1
    return "".join(out)


def iter_sources(repo: Path):
    for root in SCAN_ROOTS:
        base = repo / root
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(dirnames)
            for name in sorted(filenames):
                if os.path.splitext(name)[1].lower() not in SRC_EXT:
                    continue
                full = Path(dirpath) / name
                rel = full.relative_to(repo).as_posix()
                if any(part in "/" + rel for part in EXCLUDE_PARTS):
                    continue
                yield rel, full


def find_function_body(masked: str, name: str):
    """返回 (start, end) 偏移：函数名后第一个 '{' 到配对的 '}'。找不到返回 None。"""
    m = re.search(r"\b" + re.escape(name) + r"\s*\(", masked)
    if not m:
        return None
    i = masked.find("{", m.end())
    if i < 0:
        return None
    depth = 0
    for j in range(i, len(masked)):
        if masked[j] == "{":
            depth += 1
        elif masked[j] == "}":
            depth -= 1
            if depth == 0:
                return (i, j)
    return None


# ── case 记录 ───────────────────────────────────────────────────────────────
class Cases:
    def __init__(self):
        self.items = []

    def add(self, cid, ok, detail, evidence=None):
        self.items.append({"id": cid, "ok": bool(ok), "detail": detail,
                           "evidence": evidence if evidence is not None else {}})
        return bool(ok)

    def skip(self, cid, reason):
        self.items.append({"id": cid, "ok": None, "detail": "SKIPPED: " + reason,
                           "evidence": {"skipped": reason}})

    def failed(self):
        return [c for c in self.items if c["ok"] is False]

    def skipped(self):
        return [c for c in self.items if c["ok"] is None]


# ── T1：静态不变量 ──────────────────────────────────────────────────────────
def static_checks(repo: Path, cases: Cases):
    header = repo / ANCHOR_PUNCH_HEADER
    if not header.is_file():
        cases.add("S1_source_present", False, "缺少生产头 " + ANCHOR_PUNCH_HEADER)
        return
    raw = header.read_text(encoding="utf-8", errors="replace")
    code = mask_noncode(raw)
    cases.add("S1_source_present", True, "生产头存在", {"path": ANCHOR_PUNCH_HEADER,
                                                       "bytes": len(raw)})

    # S2 打洞路径零浮点
    fp_hits = []
    for pat in (r"\bfloat\b", r"\bdouble\b", r"\blong\s+double\b",
                r"\b\d+\.\d+(?:[eE][-+]?\d+)?[fFlL]?\b",
                r"\b(?:isnan|isinf|fabs|fabsf|signbit|fpclassify)\b"):
        for m in re.finditer(pat, code):
            line = code[:m.start()].count("\n") + 1
            fp_hits.append("line %d: %s" % (line, m.group(0)))
    cases.add("S2_no_floating_point", not fp_hits,
              "打洞路径零浮点（-0.0 的位型含非零字节，浮点等值判定会误判）" if not fp_hits
              else "打洞路径出现浮点：%s" % fp_hits[:6],
              {"hits": fp_hits[:20]})

    # S3 谓词字节级
    body = find_function_body(code, "block_is_literal_zero")
    ok3 = False
    detail3 = "找不到 block_is_literal_zero 函数体"
    if body:
        text = code[body[0]:body[1]]
        ok3 = bool(re.search(r"p\s*\[\s*i\s*\]\s*!=\s*0", text)) and "==" not in text
        detail3 = ("谓词逐字节 != 0（无浮点/等值比较）" if ok3
                   else "谓词不是纯字节级：%s" % text.strip()[:200])
    cases.add("S3_predicate_bytewise", ok3, detail3)

    # S4 打洞系统调用唯一实现
    falloc = list(re.finditer(r"\bfallocate\s*\(", code))
    ok4 = (len(falloc) == 1
           and "FALLOC_FL_PUNCH_HOLE" in code
           and "FALLOC_FL_KEEP_SIZE" in code)
    cases.add("S4_single_punch_syscall", ok4,
              "fallocate 调用点 %d 处；PUNCH_HOLE=%s KEEP_SIZE=%s" % (
                  len(falloc), "FALLOC_FL_PUNCH_HOLE" in code,
                  "FALLOC_FL_KEEP_SIZE" in code),
              {"fallocate_calls": len(falloc)})

    # S5 Windows 分支
    ok5 = "FSCTL_SET_SPARSE" in code and "FSCTL_SET_ZERO_DATA" in code
    cases.add("S5_windows_branch", ok5,
              "Windows 分支 = FSCTL_SET_SPARSE + FSCTL_SET_ZERO_DATA" if ok5
              else "Windows 分支缺 API（平台等价实现缺失）")

    # S6 块粒度
    ok6 = (re.search(r"kPunchBlockBytes\s*=\s*65536ULL", code) is not None
           and re.search(r"kPunchBlockBytes\s*=\s*4096ULL", code) is not None)
    cases.add("S6_block_granularity", ok6,
              "Linux 4096 / Windows 65536（合同 §7 T1 冻结粒度 + Windows 64 KiB 对齐）"
              if ok6 else "块粒度常量与冻结值不符")

    # S7 降级先于接触
    ok7 = False
    detail7 = "找不到 punch_all_zero_blocks 函数体"
    mb = find_function_body(code, "punch_all_zero_blocks")
    if mb:
        ftext = code[mb[0]:mb[1]]
        probe_at = ftext.find("volume_supports_punch")
        punch_at = ftext.find("punch_range_at")
        ok7 = probe_at >= 0 and punch_at >= 0 and probe_at < punch_at
        detail7 = ("能力探测（第 %d 字符）先于任何打洞调用（第 %d 字符）"
                   % (probe_at, punch_at)) if ok7 else \
                  "探测/打洞次序错误：probe=%d punch=%d" % (probe_at, punch_at)
    cases.add("S7_degrade_before_touch", ok7, detail7)

    # S8 读回复算（**限定在 punch_all_zero_blocks 体内**：别处的同名比较不能顶替）
    ok8, detail8 = False, "找不到 punch_all_zero_blocks 函数体"
    if mb:
        f8 = code[mb[0]:mb[1]]
        ok8 = ("PUNCH_VERIFY_MISMATCH" in f8 and "sha256_before" in f8
               and "sha256_after" in f8
               and "r.sha256_after != r.sha256_before" in f8)
        detail8 = ("打洞后丢页缓存重读并比对整文件 sha256；不一致 ⇒ PUNCH_VERIFY_MISMATCH"
                   if ok8 else "读回复算缺失或未与打洞前比对（须在 punch_all_zero_blocks 体内）")
    cases.add("S8_readback_verify", ok8, detail8)

    # S9 / S11 / TA / TB / TD：全仓扫描
    outside_punch, outside_entry, trim_writers, archive_entry = [], [], {}, []
    scanned = 0
    for rel, full in iter_sources(repo):
        scanned += 1
        text = full.read_text(encoding="utf-8", errors="replace")
        mc = mask_noncode(text)
        if not rel.startswith(AIO_PREFIX):
            for tok in ("FALLOC_FL_PUNCH_HOLE", "FSCTL_SET_ZERO_DATA"):
                if tok in mc:
                    outside_punch.append("%s: %s" % (rel, tok))
            if ("punch_all_zero_blocks" in mc or "punch_range_forced" in mc) \
                    and rel not in JUDGMENT_TOOLS:
                outside_entry.append(rel)
        if "punch_all_zero_blocks" in mc and re.search(r"archive|zst|\.tar", rel):
            archive_entry.append(rel)
        lit = set()
        for kw in TRIM_KEYWORDS:
            if re.search(r'"\s*' + kw + r"\s*\"", text) or re.search(r"'%s'" % kw, text):
                lit.add(kw)
        if lit:
            trim_writers[rel] = sorted(lit)
    cases.add("S9_no_second_impl", not outside_punch,
              "全仓 %d 个首方源文件扫描：aio 之外零打洞原语" % scanned if not outside_punch
              else "aio 之外出现打洞原语：%s" % outside_punch[:6],
              {"scanned_files": scanned, "violations": outside_punch[:20]})
    cases.add("S11_punch_entry_scope", not outside_entry and not archive_entry,
              "打洞入口只在 aio 内被调用；归档/容器路径零调用" 
              if (not outside_entry and not archive_entry)
              else "越界调用：%s / 归档路径：%s" % (outside_entry[:5], archive_entry[:5]))

    # TA 包围盒 TRIM 默认不开
    cases.add("TA_trim_default_off", not trim_writers,
              "全仓首方源零 TRIM1/TRIM2 关键字写入者 ⇒ 无法开启 TRIM（默认不开）"
              if not trim_writers
              else "出现 TRIM 关键字写入者：%s" % json.dumps(trim_writers, ensure_ascii=False),
              {"trim_writers": trim_writers})
    # TB 半开禁止
    half_open = {k: v for k, v in trim_writers.items()
                 if not ({"ONAXIS1", "ONAXIS2"} <= set(v))}
    cases.add("TB_trim_keywords_complete", not half_open,
              "无 TRIM 写入者（空真）或写入者均带全 ONAXIS1/ONAXIS2"
              if not half_open else "半开状态（写了 TRIM 未写 ONAXIS）：%s" % half_open,
              {"half_open": half_open})

    # TC 文档登记（默认不开 + 四个关键字）
    contract = (repo / ANCHOR_CONTRACT)
    design = (repo / ANCHOR_DESIGN)
    if not contract.is_file() or not design.is_file():
        cases.add("TC_trim_registered_in_docs", False,
                  "缺文档锚：%s / %s" % (ANCHOR_CONTRACT, ANCHOR_DESIGN))
    else:
        ctext = contract.read_text(encoding="utf-8", errors="replace")
        dtext = design.read_text(encoding="utf-8", errors="replace")

        def section(text, start_pat):
            m = re.search(start_pat, text)
            if not m:
                return ""
            nxt = re.search(r"\n##", text[m.end():])
            return text[m.start(): m.end() + (nxt.start() if nxt else len(text))]

        sec7 = section(ctext, r"## 7. ")
        sec92 = section(dtext, r"### 9.2 ")
        rows = [ln for ln in sec7.splitlines() if "T2" in ln and "TRIM" in ln]
        checks = {
            "contract_s7_has_t2_row": bool(rows),
            "contract_s7_default_off": "默认不启用" in sec7,
            "contract_s7_four_keywords": all(kw in sec7 for kw in TRIM_KEYWORDS),
            "design_s92_default_off": "默认不启用" in sec92,
            "design_s92_four_keywords": all(kw in sec92 for kw in TRIM_KEYWORDS),
        }
        ok_tc = all(checks.values())
        cases.add("TC_trim_registered_in_docs", ok_tc,
                  "合同 §7 T2 行与设计 §9.2 登记「默认不启用」+ TRIM1/TRIM2/ONAXIS1/ONAXIS2"
                  if ok_tc else "TRIM 可选形态登记不完整：%s" % checks,
                  {"checks": checks})

    # S10 写端接线
    writer = repo / ANCHOR_WRITER
    if not writer.is_file():
        cases.add("S10_writer_wiring", False, "缺少写端锚 " + ANCHOR_WRITER)
        return
    wtext = mask_noncode(writer.read_text(encoding="utf-8", errors="replace"))
    wb = find_function_body(wtext, "write_fits_atomic")
    ok10, detail10 = False, "找不到 write_fits_atomic 函数体"
    if wb:
        f = wtext[wb[0]:wb[1]]
        i_fsync = f.find("fsync_path(tmp, 0)")
        i_punch = f.find("punch_all_zero_blocks")
        i_rename = f.find("atomic_replace(tmp, final_path)")
        i_mismatch = f.find("PUNCH_VERIFY_MISMATCH")
        ok10 = (0 <= i_fsync < i_punch < i_rename and 0 <= i_mismatch < i_rename)
        detail10 = ("打洞在 fsync 之后（%d）、原子 rename 之前（%d）；读回不一致分支在 rename 之前（%d）"
                    % (i_fsync, i_rename, i_mismatch)) if ok10 else \
                   "接线次序错误：fsync=%d punch=%d rename=%d mismatch=%d" % (
                       i_fsync, i_punch, i_rename, i_mismatch)
    cases.add("S10_writer_wiring", ok10, detail10)


# ── T2：谓词模型 + 判据判别力 ───────────────────────────────────────────────
def corpus():
    """与 sparse_punch_probe.cpp 的 make_corpus() 逐块同源（确定性）。"""
    blk = BLOCK_LINUX
    items = []
    items.append(("all_zero", bytes(blk)))
    items.append(("be_nan_7fc00000", struct.pack(">I", 0x7FC00000) * (blk // 4)))
    items.append(("be_snan_7f800001", struct.pack(">I", 0x7F800001) * (blk // 4)))
    items.append(("be_neg_nan_ffc00000", struct.pack(">I", 0xFFC00000) * (blk // 4)))
    items.append(("be_neg_zero_80000000", struct.pack(">I", 0x80000000) * (blk // 4)))
    items.append(("le_neg_zero_00000080", struct.pack("<I", 0x80000000) * (blk // 4)))
    b = bytearray(blk); b[0] = 1
    items.append(("one_nonzero_at_head", bytes(b)))
    b = bytearray(blk); b[blk - 1] = 1
    items.append(("one_nonzero_at_tail", bytes(b)))
    items.append(("be_one_3f800000", struct.pack(">I", 0x3F800000) * (blk // 4)))
    b = bytearray(blk)
    x = 0x12345678
    for i in range(blk):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        b[i] = (x >> 24) & 0xFF
    items.append(("lcg_random_seed_12345678", bytes(b)))
    b = bytearray(blk)
    for i in range(blk // 2, blk - 3, 4):
        b[i:i + 4] = struct.pack(">I", 0x7FC00000)
    items.append(("half_zero_half_nan", bytes(b)))
    return items


def py_literal_zero(bs: bytes) -> bool:
    """与生产头 block_is_literal_zero 同语义：逐字节比较。"""
    for v in bs:
        if v != 0:
            return False
    return True


def py_has_nan(bs: bytes) -> bool:
    exp, man = 0x7F800000, 0x007FFFFF
    for i in range(0, len(bs) - 3, 4):
        be = struct.unpack_from(">I", bs, i)[0]
        le = struct.unpack_from("<I", bs, i)[0]
        if (be & exp) == exp and (be & man) != 0:
            return True
        if (le & exp) == exp and (le & man) != 0:
            return True
    return False


def float_equality_mutant(bs: bytes) -> bool:
    """**变异体**（故意错的判据）：按 float32 数值判「可打洞」（任一字节序成立即可）。

    这是判据判别力的负例面：IEEE -0.0 与 0.0 数值比较相等，但位型含非零字节；
    FITS 是大端存储，只看数值的实现会把 -0.0 边距判成可打洞并改变文件字节。
    变异体必须在 -0.0 语料上与正确判据分歧，否则说明本判据是恒真门。
    """
    le_zero = all(struct.unpack_from("<f", bs, i)[0] == 0.0
                  for i in range(0, len(bs) - 3, 4))
    be_zero = all(struct.unpack_from(">f", bs, i)[0] == 0.0
                  for i in range(0, len(bs) - 3, 4))
    return le_zero or be_zero


def model_checks(cases: Cases, probe_json=None):
    cs = corpus()
    bad = []
    for kind, bs in cs:
        z = py_literal_zero(bs)
        nan = py_has_nan(bs)
        if kind == "all_zero" and not z:
            bad.append("%s: 全零块被判为不可打洞" % kind)
        if kind.startswith(("be_nan", "be_snan", "be_neg_nan", "half_zero_half_nan")):
            if z:
                bad.append("%s: NaN 块被判为可打洞（红线破坏）" % kind)
            if not nan:
                bad.append("%s: NaN 位型未被识别" % kind)
        if kind.endswith("neg_zero_80000000") or kind.endswith("neg_zero_00000080"):
            if z:
                bad.append("%s: -0.0 块被判为可打洞（会改变文件字节）" % kind)
    cases.add("T2_model_semantics", not bad,
              "语料 %d 块：全零可打洞；NaN 与 -0.0 一律不可打洞" % len(cs) if not bad
              else "模型语义错误：%s" % bad, {"corpus_size": len(cs)})

    # 判别力：变异体必须在 -0.0 语料上与正确判据分歧
    mutant_wrong = []
    for kind, bs in cs:
        if float_equality_mutant(bs) != py_literal_zero(bs):
            mutant_wrong.append(kind)
    ok = ("be_neg_zero_80000000" in mutant_wrong
          and "le_neg_zero_00000080" in mutant_wrong
          and "all_zero" not in mutant_wrong)
    cases.add("T2_criterion_discriminating", ok,
              "浮点等值变异体在 -0.0 语料上判错（%s）⇒ 字节判据是必要的，判据非恒真"
              % ",".join(mutant_wrong) if ok
              else "变异体未被检出 ⇒ 判据可能是恒真门（分歧=%s）" % mutant_wrong,
              {"mutant_divergence": mutant_wrong})

    if probe_json is None:
        cases.skip("T3_cxx_py_differential", "未提供探针输出（--probe-run 下必跑）")
        return
    got = {c["kind"]: c for c in probe_json.get("corpus", [])}
    diff = []
    for kind, bs in cs:
        g = got.get(kind)
        if g is None:
            diff.append("%s: 探针语料缺失" % kind)
            continue
        if bool(g["cxx_literal_zero"]) != py_literal_zero(bs):
            diff.append("%s: C++ 谓词=%s，Python 模型=%s"
                        % (kind, g["cxx_literal_zero"], py_literal_zero(bs)))
        if bool(g["cxx_has_nan"]) != py_has_nan(bs):
            diff.append("%s: NaN 识别不一致" % kind)
    cases.add("T3_cxx_py_differential", not diff,
              "探针（生产头编译）与独立 Python 模型在 %d 块语料上逐块一致" % len(cs)
              if not diff else "差分不一致：%s" % diff[:6],
              {"diff": diff[:20]})


# ── T3：真实系统调用 ────────────────────────────────────────────────────────
def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_dir(d: Path) -> dict:
    out = {}
    for p in sorted(d.glob("*")):
        if p.is_file():
            out[p.name] = sha256_file(p)
    return out


def which_compiler(names):
    for n in names:
        p = shutil.which(n)
        if p:
            return p
    return None


def build_probe(repo: Path, bindir: Path, log):
    cxx = which_compiler(("g++", "c++", "clang++"))
    cc = which_compiler(("gcc", "cc", "clang"))
    if not cxx or not cc:
        return None, None, "缺 C/C++ 编译器：cxx=%s cc=%s" % (cxx, cc)
    bindir.mkdir(parents=True, exist_ok=True)
    probe = bindir / "sparse_punch_probe"
    cmd = [cxx, "-std=c++17", "-O2", "-Wall", "-Wextra",
           "-I" + str(repo / "lib/infrastructure/aio/src"),
           "-I" + str(repo / "lib/algorithms/shared"),
           "-o", str(probe), str(repo / ANCHOR_PROBE_SRC),
           str(repo / "lib/infrastructure/aio/src/aio_log.cpp"),
           str(repo / "lib/algorithms/shared/crypto/sha256.cpp")]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    log.append({"step": "build_probe", "cmd": cmd, "rc": r.returncode,
                "stderr": r.stderr[-4000:]})
    if r.returncode != 0:
        return None, None, "探针编译失败 rc=%d: %s" % (r.returncode, r.stderr[-800:])
    reader = bindir / "fits_cross_reader"
    cmd2 = [cc, "-O2", "-Wall", "-Wextra", "-o", str(reader),
            str(repo / ANCHOR_CROSS_READER), "-lcfitsio"]
    r2 = subprocess.run(cmd2, capture_output=True, text=True, timeout=600)
    log.append({"step": "build_cross_reader", "cmd": cmd2, "rc": r2.returncode,
                "stderr": r2.stderr[-4000:]})
    if r2.returncode != 0:
        return probe, None, "cfitsio 交叉读器编译失败 rc=%d: %s" % (r2.returncode,
                                                              r2.stderr[-800:])
    return probe, reader, ""


def run_probe(probe: Path, args, env_extra=None, timeout=600):
    env = dict(os.environ)
    if env_extra:
        env.update(env_extra)
    r = subprocess.run([str(probe)] + [str(a) for a in args], capture_output=True,
                       text=True, timeout=timeout, env=env)
    out = r.stdout.strip().splitlines()
    js = json.loads(out[-1]) if out else {}
    return r.returncode, js, r.stderr[-2000:]


def cross_reader(reader: Path, fits: Path):
    """cfitsio 交叉读器：只经 cfitsio API 读、只向 stdout 写（无文件系统原语）。"""
    r = subprocess.run([str(reader), str(fits)], capture_output=True, text=True,
                       timeout=600)
    js = json.loads(r.stdout.strip().splitlines()[-1]) if r.stdout.strip() else {}
    return r.returncode, js, r.stderr[-2000:]


def data_extents(path: Path):
    """已分配区段图（SEEK_DATA/SEEK_HOLE）。洞 = [0, size) 内 data extent 的补集。"""
    size = os.path.getsize(path)
    out = []
    fd = os.open(str(path), os.O_RDONLY)
    try:
        pos = 0
        while pos < size:
            try:
                d = os.lseek(fd, pos, os.SEEK_DATA)
            except OSError:
                break
            try:
                h = os.lseek(fd, d, os.SEEK_HOLE)
            except OSError:
                break
            if h > d:
                out.append({"data_start": d, "data_end": h})
            pos = h
    finally:
        os.close(fd)
    return out


def region_sha256(path: Path, offset: int, length: int) -> str:
    with open(path, "rb") as f:
        f.seek(offset)
        return hashlib.sha256(f.read(length)).hexdigest()


def astropy_reads(path: Path):
    """astropy 两路（memmap / 非 memmap）逐 HDU 的 header 与数据字节摘要。"""
    from astropy.io import fits
    res = {}
    for mode in ("memmap", "nonmemmap"):
        per = []
        with fits.open(str(path), memmap=(mode == "memmap")) as hdul:
            for i, hdu in enumerate(hdul):
                hdr = hdu.header.tostring().encode("ascii", "replace")
                data = hdu.data
                raw = b"" if data is None else data.tobytes()
                per.append({"hdu": i, "header_sha256": hashlib.sha256(hdr).hexdigest(),
                            "data_sha256": hashlib.sha256(raw).hexdigest(),
                            "data_bytes": len(raw)})
        res[mode] = per
    return res


def astropy_checksums(path: Path):
    from astropy.io import fits
    out = []
    with fits.open(str(path), checksum=True) as hdul:
        for i, hdu in enumerate(hdul):
            d = hdu.verify_datasum()
            c = hdu.verify_checksum()
            out.append({"hdu": i, "datasum_ok": int(d) if d is not None else None,
                        "checksum_ok": int(c) if c is not None else None})
    return out


def pread_probes(path: Path, ref: Path, bounds):
    """跨洞边界随机访问探针：与打洞前参考副本逐字节比较。"""
    import random
    probes = []
    with open(path, "rb") as a, open(ref, "rb") as b:
        size = os.path.getsize(path)
        offsets = []
        for s, e in bounds:
            offsets += [max(0, s - 4096), s, min(size - 1, s + 1),
                        (s + e) // 2, max(0, e - 1), e, min(size - 1, e + 4096)]
        rnd = random.Random(20260922)
        offsets += [rnd.randrange(0, size) for _ in range(16)]
        offsets += [0, 2880, size - 1]
        for off in offsets:
            ln = min(65536, size - off)
            if ln <= 0:
                continue
            a.seek(off); b.seek(off)
            da, db = a.read(ln), b.read(ln)
            probes.append({"offset": off, "len": ln, "equal": da == db,
                           "a_bytes": len(da), "b_bytes": len(db)})
    return probes


def holes_from_extents(extents, size):
    """data extent 的补集 = 洞区间。"""
    holes = []
    pos = 0
    for e in sorted(extents, key=lambda x: x["data_start"]):
        if e["data_start"] > pos:
            holes.append((pos, e["data_start"]))
        pos = max(pos, e["data_end"])
    if pos < size:
        holes.append((pos, size))
    return holes


def align_up(v, a=BLOCK_LINUX):
    return ((v + a - 1) // a) * a


def align_down(v, a=BLOCK_LINUX):
    return (v // a) * a


def make_fixture(dest: Path, block=BLOCK_LINUX):
    """合成 FITS：行 0..99 全零（连续可打洞带）、行 100..411 数据、行 412..511 NaN。"""
    import numpy as np
    from astropy.io import fits
    n = 512
    arr = np.zeros((n, n), dtype=">f4")
    yy = np.arange(n, dtype=np.float32)[:, None]
    arr[100:412, :] = (yy[100:412, :] * 0.25 + 1.0).astype(">f4")
    arr[412:, :] = np.float32("nan")
    hdu = fits.PrimaryHDU(arr)
    hdu.header["BUNIT"] = "ADU"
    hdu.writeto(str(dest), overwrite=True)
    with fits.open(str(dest), mode="update") as h:
        h[0].add_checksum()
    # 关键区段（字节偏移；data 从第 2 个 2880 块起）
    data_off = 2880
    zero_band = (data_off, data_off + 100 * n * 4)
    nan_band = (data_off + 412 * n * 4, data_off + n * n * 4)
    return {"zero_band": zero_band, "nan_band": nan_band,
            "size": os.path.getsize(dest), "block": block}


def probe_run(repo: Path, workdir: Path, cases: Cases, log):
    workdir.mkdir(parents=True, exist_ok=True)
    bindir = workdir / "bin"
    probe, reader, err = build_probe(repo, bindir, log)
    if probe is None:
        cases.add("T3_build_probe", False, err)
        return
    cases.add("T3_build_probe", True, "探针由**生产头**编译（被测代码 = 上线代码）",
              {"probe": str(probe), "cross_reader": str(reader) if reader else None})
    if reader is None:
        cases.add("T3_build_cross_reader", False, err)
    else:
        cases.add("T3_build_cross_reader", True, "cfitsio 交叉读器编译成功")

    # 差分
    rc, pj, perr = run_probe(probe, ["predicate"])
    if rc != 0:
        cases.add("T3_probe_predicate", False, "predicate rc=%d %s" % (rc, perr))
    else:
        cases.add("T3_probe_predicate", True, "谓词语料由生产头求出", {"corpus": len(pj.get("corpus", []))})
        model_checks(cases, probe_json=pj)

    # 合成 FITS
    fx = workdir / "synth"
    fx.mkdir(parents=True, exist_ok=True)
    src = fx / "synth.fits"
    info = make_fixture(src)
    ref = fx / "synth_ref.fits"
    shutil.copy2(src, ref)
    target = fx / "synth_work.fits"
    shutil.copy2(src, target)
    st0 = os.stat(target)
    sha0 = sha256_file(target)
    astro0 = astropy_reads(target)
    astro_c0 = astropy_checksums(target)
    rcx, xj0, xerr = cross_reader(reader, target) if reader else (1, {}, "no reader")
    raw0 = region_sha256(target, 2880, 512 * 512 * 4)
    rc, pj, perr = run_probe(probe, ["punch", target])
    st1 = os.stat(target)
    sha1 = sha256_file(target)
    astro1 = astropy_reads(target)
    astro_c1 = astropy_checksums(target)
    rcx1, xj1, _ = cross_reader(reader, target) if reader else (1, {}, "")
    raw1 = region_sha256(target, 2880, 512 * 512 * 4)
    res = pj.get("result", {})
    holes = holes_from_extents(data_extents(target), st1.st_size)
    nan_hit = [h for h in holes if h[0] < info["nan_band"][1] and h[1] > info["nan_band"][0]]
    # 打洞粒度是 4 KiB：可打洞的只有全零带的**块对齐内区**。
    zb0, zb1 = align_up(info["zero_band"][0]), align_down(info["zero_band"][1])
    zero_cover = [h for h in holes if h[0] <= zb0 and h[1] >= zb1]
    # 洞起点必须块对齐；洞终点要么块对齐，要么 == EOF（末块补到块边界释放，
    # 内核的洞图在 EOF 处截断 —— st_size 不变，EOF 之外无字节）。
    unaligned = [h for h in holes
                 if h[0] % BLOCK_LINUX or (h[1] % BLOCK_LINUX and h[1] != st1.st_size)]
    bounds = [(s, e) for s, e in holes]
    probes = pread_probes(target, ref, bounds)

    cases.add("T3_synth_rc_ok", res.get("rc") == RC_OK and res.get("verified") is True,
              "打洞 rc=%s verified=%s reason=%s" % (res.get("rc"), res.get("verified"),
                                                 res.get("reason")),
              {"result": res})
    cases.add("T3_synth_bytes_identical",
              sha0 == sha1 and st0.st_size == st1.st_size,
              "整文件 sha256 与 st_size 打洞前后相同（sha=%s…）" % sha0[:16]
              if sha0 == sha1 and st0.st_size == st1.st_size
              else "字节/尺寸变化：sha %s→%s size %d→%d" % (sha0[:12], sha1[:12],
                                                        st0.st_size, st1.st_size),
              {"sha_before": sha0, "sha_after": sha1,
               "size_before": st0.st_size, "size_after": st1.st_size})
    cases.add("T3_synth_blocks_dropped",
              st1.st_blocks < st0.st_blocks and res.get("released_bytes", 0) > 0,
              "st_blocks %d→%d，释放 %d B" % (st0.st_blocks, st1.st_blocks,
                                            res.get("released_bytes", 0))
              if st1.st_blocks < st0.st_blocks else
              "st_blocks 未下降（%d→%d）⇒ 判红" % (st0.st_blocks, st1.st_blocks),
              {"blocks_before": st0.st_blocks, "blocks_after": st1.st_blocks,
               "released_bytes": res.get("released_bytes")})
    cases.add("T3_synth_astropy_identical", astro0 == astro1,
              "astropy memmap/非 memmap 两路逐 HDU header 与数据字节全等"
              if astro0 == astro1 else "astropy 读回不一致")
    cases.add("T3_synth_cfitsio_identical",
              bool(xj0) and xj0 == xj1,
              "cfitsio 交叉读器：逐 HDU header 卡片摘要与像素摘要全等（%d HDU）"
              % len(xj1.get("per_hdu", [])) if (xj0 and xj0 == xj1)
              else "cfitsio 读回不一致",
              {"before": xj0, "after": xj1})
    cases.add("T3_synth_raw_data_region_identical", raw0 == raw1,
              "数据区原始字节（%d B，文件层面直读）打洞前后 sha256 相同" % (512 * 512 * 4)
              if raw0 == raw1 else "数据区原始字节变化：%s → %s" % (raw0[:12], raw1[:12]),
              {"raw_sha_before": raw0, "raw_sha_after": raw1})
    cases.add("T3_synth_checksum_ok",
              astro_c0 == astro_c1 and all(
                  (c.get("datasum_ok") in (1, None)) and (c.get("checksum_ok") in (1, None))
                  for c in astro_c1) and bool(astro_c1),
              "DATASUM/CHECKSUM 打洞后仍自洽且与打洞前一致" if astro_c0 == astro_c1
              else "校验和不一致：%s → %s" % (astro_c0, astro_c1),
              {"before": astro_c0, "after": astro_c1})
    cases.add("T3_synth_pread_identical",
              all(p["equal"] for p in probes) and len(probes) >= 16,
              "跨洞边界随机访问 %d 个探针逐字节全等" % len(probes)
              if all(p["equal"] for p in probes) else
              "存在不等探针：%s" % [p for p in probes if not p["equal"]][:3],
              {"n_probes": len(probes), "bounds": bounds[:8]})
    cases.add("T3_synth_zero_band_punched", bool(zero_cover),
              "连续全零带的块对齐内区 [%d,%d) 被洞完整覆盖（原始带 %s，带首 2880..4096 属 header 尾块不可打）"
              % (zb0, zb1, info["zero_band"]) if zero_cover
              else "全零带未被覆盖（零块扫描失效）：holes=%s 期望覆盖 [%d,%d)"
              % (holes[:8], zb0, zb1), {"holes": holes[:8], "aligned_band": [zb0, zb1]})
    cases.add("T3_synth_holes_block_aligned", not unaligned,
              "洞起点均 %d B 块对齐；终点块对齐或 == EOF（末块补到块边界释放）" % BLOCK_LINUX
              if not unaligned else "出现非块对齐洞：%s（EOF=%d）" % (unaligned, st1.st_size),
              {"holes": holes[:8], "size": st1.st_size})
    cases.add("T3_synth_nan_band_untouched", not nan_hit,
              "NaN 带 %s 未被任何洞覆盖（红线：NaN 区绝不打洞）" % (info["nan_band"],)
              if not nan_hit else "NaN 带被洞覆盖：%s ⇒ 红线破坏" % nan_hit,
              {"nan_band": info["nan_band"], "holes": holes[:8]})

    # 负例 1：对 NaN 区强制打洞 ⇒ 必须判红
    neg = fx / "synth_nan_neg.fits"
    shutil.copy2(src, neg)
    nsha0 = sha256_file(neg)
    nrc, npj, _ = run_probe(probe, ["forced", neg, align_up(info["nan_band"][0]),
                                    BLOCK_LINUX])
    nsha1 = sha256_file(neg)
    nres = npj.get("result", {})
    cases.add("T3_neg_nan_region_red",
              nres.get("rc") == RC_VERIFY_MISMATCH and nsha0 != nsha1,
              "对 NaN 区强制打洞 ⇒ rc=%s（VERIFY_MISMATCH）且字节确实变化（判据能红）"
              % nres.get("rc") if nres.get("rc") == RC_VERIFY_MISMATCH
              else "对 NaN 区强制打洞未被判红：rc=%s" % nres.get("rc"),
              {"result": nres, "sha_before": nsha0, "sha_after": nsha1})

    # 负例 2：对数据区强制打洞 ⇒ 必须判红
    neg2 = fx / "synth_data_neg.fits"
    shutil.copy2(src, neg2)
    d0 = sha256_file(neg2)
    drc, dpj, _ = run_probe(probe, ["forced", neg2,
                                    align_up(2880 + 200 * 512 * 4), BLOCK_LINUX])
    d1 = sha256_file(neg2)
    dres = dpj.get("result", {})
    cases.add("T3_neg_data_region_red",
              dres.get("rc") == RC_VERIFY_MISMATCH and d0 != d1,
              "对非零数据区强制打洞 ⇒ rc=%s 且字节变化（判据能红）" % dres.get("rc")
              if dres.get("rc") == RC_VERIFY_MISMATCH else
              "对数据区强制打洞未被判红：rc=%s" % dres.get("rc"),
              {"result": dres})

    # 降级：注入「卷不支持」等价于 EOPNOTSUPP
    deg = fx / "synth_degrade.fits"
    shutil.copy2(src, deg)
    g0 = os.stat(deg)
    gsha0 = sha256_file(deg)
    grc, gpj, _ = run_probe(probe, ["punch", deg],
                            env_extra={"ASTROCS_SPARSE_PUNCH_FAULT": "unsupported"})
    g1 = os.stat(deg)
    gsha1 = sha256_file(deg)
    gres = gpj.get("result", {})
    cases.add("T3_degrade_not_failclosed",
              gres.get("rc") == RC_UNSUPPORTED and gsha0 == gsha1
              and g0.st_blocks == g1.st_blocks and g0.st_size == g1.st_size
              and gres.get("released_bytes", -1) == 0
              and gres.get("punched_bytes", -1) == 0,
              "卷不支持 ⇒ rc=%s reason=%s；文件逐字节不变、分配不变、released_bytes=0（跳过而非阻断）"
              % (gres.get("rc"), gres.get("reason"))
              if gres.get("rc") == RC_UNSUPPORTED else
              "降级路径错误：rc=%s" % gres.get("rc"),
              {"result": gres, "sha_before": gsha0, "sha_after": gsha1,
               "blocks_before": g0.st_blocks, "blocks_after": g1.st_blocks})

    # 幂等：再打一次 ⇒ 无新释放
    idem = fx / "synth_idem.fits"
    shutil.copy2(src, idem)
    rc1, pj1, _ = run_probe(probe, ["repunch", idem])
    r1 = pj1.get("result", {})
    r2 = pj1.get("second", {})
    a2 = pj1.get("after2", {})
    cases.add("T3_repunch_no_release",
              r1.get("rc") == RC_OK and r2.get("rc") == RC_NO_RELEASE
              and a2.get("alloc") == pj1.get("after", {}).get("alloc")
              and a2.get("sha256") == pj1.get("before", {}).get("sha256"),
              "首次打洞 rc=%s；再次打洞 rc=%s（NO_RELEASE，无新释放）且字节不变"
              % (r1.get("rc"), r2.get("rc")) if r2.get("rc") == RC_NO_RELEASE
              else "幂等语义不符：second rc=%s" % r2.get("rc"),
              {"first": r1, "second": r2})

    # 真实 AstroCS 瓦片（存在才跑；不存在显式登记）
    tiles = [("signal", repo / "run/PERF-401/out/real16_w1/signal/Norder9/Dir1450000"),
             ("support", repo / "run/PERF-401/out/real16_w1/support/Norder9/Dir1450000")]
    for name, d in tiles:
        cand = sorted(d.glob("*.fits")) if d.is_dir() else []
        if not cand:
            cases.skip("T3_real_tile_" + name,
                       "缺真实瓦片语料 %s（不静默通过）" % d.relative_to(repo))
            continue
        rt = fx / ("real_%s.fits" % name)
        shutil.copy2(cand[0], rt)
        s0 = os.stat(rt)
        h0 = sha256_file(rt)
        ra0 = astropy_reads(rt)
        rc0, rj0, _ = cross_reader(reader, rt) if reader else (1, {}, "")
        rc, pjr, _ = run_probe(probe, ["punch", rt])
        s1 = os.stat(rt)
        h1 = sha256_file(rt)
        ra1 = astropy_reads(rt)
        rc1, rj1, _ = cross_reader(reader, rt) if reader else (1, {}, "")
        rr = pjr.get("result", {})
        holes_r = holes_from_extents(data_extents(rt), s1.st_size)
        cases.add("T3_real_tile_%s_readers" % name,
                  ra0 == ra1 and bool(rj0) and rj0 == rj1,
                  "真实 %s 瓦片：astropy（memmap/非 memmap）与 cfitsio 交叉读器逐 HDU 全等"
                  % name if (ra0 == ra1 and rj0 and rj0 == rj1)
                  else "真实 %s 瓦片读器不一致" % name,
                  {"cfitsio": rj1})
        cases.add("T3_real_tile_" + name,
                  rr.get("rc") == RC_OK and rr.get("verified") is True and h0 == h1
                  and s0.st_size == s1.st_size,
                  "真实 %s 瓦片：rc=%s 释放 %d B（%.2f%%）sha 不变"
                  % (name, rr.get("rc"), rr.get("released_bytes", 0),
                     100.0 * rr.get("released_bytes", 0) / max(1, s0.st_size))
                  if rr.get("rc") == RC_OK else "真实瓦片打洞失败 rc=%s" % rr.get("rc"),
                  {"result": rr, "sha_before": h0, "sha_after": h1,
                   "holes": holes_r[:8], "size": s0.st_size})
        if name == "signal":
            # signal 边距是 NaN ⇒ 除 FITS 填充尾部外不得有洞
            big = [h for h in holes_r if (h[1] - h[0]) > BLOCK_LINUX]
            cases.add("T3_real_signal_nan_margin_untouched", not big,
                      "signal 瓦片仅有填充尾块洞（无 >4 KiB 洞 ⇒ NaN 边距未被打洞）"
                      if not big else "signal 瓦片出现大洞：%s ⇒ 可能打到了 NaN 边距" % big,
                      {"holes": holes_r[:8]})


# ── main ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true",
                    help="静态不变量 + 谓词模型 + 判据判别力（不编译、不跑系统调用）")
    ap.add_argument("--probe-run", action="store_true",
                    help="编译生产头为探针并跑真实系统调用（Linux）")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    if not (repo / ANCHOR_PUNCH_HEADER).is_file():
        print("SPARSE-PUNCH_FAIL: 锚缺失 %s（fail-closed）" % ANCHOR_PUNCH_HEADER)
        return 2
    cases = Cases()
    log = []
    static_checks(repo, cases)
    if args.probe_run:
        workdir = Path(args.workdir) if args.workdir else Path(
            tempfile.mkdtemp(prefix="sparse-punch-"))
        probe_run(repo, workdir, cases, log)
    else:
        model_checks(cases)

    failed = cases.failed()
    skipped = cases.skipped()
    verdict = "PASS" if not failed else "FAIL"
    out = {"check": "CHK-SPARSE-PUNCH-PROBE" if args.probe_run else "CHK-SPARSE-PUNCH",
           "mode": "probe-run" if args.probe_run else "self-test",
           "verdict": verdict, "cases": cases.items,
           "n_cases": len(cases.items), "n_failed": len(failed),
           "n_skipped": len(skipped), "build_log": log}
    if args.json_out:
        p = Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    for c in cases.items:
        mark = "PASS" if c["ok"] is True else ("SKIP" if c["ok"] is None else "FAIL")
        print("[%s] %s: %s" % (mark, c["id"], c["detail"]))
    print("SPARSE-PUNCH_%s cases=%d failed=%d skipped=%d" % (
        verdict, len(cases.items), len(failed), len(skipped)))
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
