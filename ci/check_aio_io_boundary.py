#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AIO-IO-BOUNDARY：「aio 是文件级唯一 I/O 边界」机器判据（FIX-201）。

权威依据
  - ASTROCS_DESIGN.md §9（I/O 与原子产品）逐字：
      「aio 是文件级唯一 I/O 边界：任何文件读写必须经 aio；不得有第二处 I/O 实现。」
      「机器判据：全仓文件打开 / 流式读写 / 文件系统写操作，除 aio 内部外应为 0。
        ⇒ 现存违规（HiPS 发布实现、FITS 输出里的直接文件打开、块↔文件导出/缓存
        接口）须登记并整改。」
      「块↔文件的导出/缓存接口不是生产接口：……必须删除或明确降级为非生产/
        诊断并登记；禁止任何阶段内节点用它们搬运数据。」
  - 工程控制/RELEASE-02/GAP_AUDIT.md §9.73【裁决 U5】负责人逐字：
      「全部走 aio……没有其他需要读写的地方了」。
  - 工程控制/RELEASE-03/GAP_AUDIT.md V03 / G02 / §4（前置裁决）；
    run/RELEASE-02/design-merge/CTR-01.md 红3；ARCH-01.md（aio_publish /
    fits_output 的 fopen）。
  - ENGINEERING_SPEC.md §8（每项检查必须有正例与负例、能红能绿、fail-closed）。

────────────────────────────────────────────────────────────────────────────
I/O 点穷举表（ASTROCS_DESIGN §9，此外无）
────────────────────────────────────────────────────────────────────────────
  #1 阶段一 normalize：读 原始 FITS + 校准帧（.fit/.fts/.fits 等） → 写 HiPS
  #2 阶段二 mosaic   ：读 HiPS                                → 写 天球 HiPS
  #3 阶段三 export   ：读 天球 HiPS                           → 写 FITS
  #4 运行面（§9 登记在册的固定产物，逐个登记名字/生产者/原子性归属）：
     resource_timeseries.csv / resource_summary.json / worker_balance.csv /
     astrocs_run_*.json（run manifest）/ run_context.json / graph/**（.dot/.svg）；
     天光面模型与稠密缓存、诊断文件、控制点接受记录。

  #1–#3 的全部文件读写实现只允许存在于 lib/infrastructure/aio/**；
  #4 的生产者见台账 ci/ledgers/aio_io_boundary_inventory.json（逐条登记）。

────────────────────────────────────────────────────────────────────────────
判据（确定性、可机器复跑；三层）
────────────────────────────────────────────────────────────────────────────
HARD（无白名单，命中即红）
  H1 lib/algorithms/drizzle/hips/**（HiPS 原子发布实现，含 aio_publish.cpp）
     与 lib/algorithms/fits_output/**（FITS 输出）内零文件系统原语。
  H2 六个「块↔文件」接口（aio_frame_save_cache / aio_frame_load_cache /
     aio_frame_export_block_fits / aio_frame_export_block_xml /
     aio_frame_export_all_xml / aio_pipeline_export_xml）在
     lib/infrastructure/aio/** 之外的任何调用点 = 生产越界 ⇒ 红。
     身份 = 非生产/诊断（ASTROCS_DESIGN §9 第二段）。

AIO-INTERNAL（设计允许）
  I1 lib/infrastructure/aio/** 内的文件系统原语 = aio 边界本体，允许。

INVENTORY（棘轮上限，fail-closed）
  N1 其余全部首方 C/C++ 源：每个命中文件必须在
     ci/ledgers/aio_io_boundary_inventory.json 登记（逐条给理由 + 归属）。
  N2 未登记文件出现命中 ⇒ 红。
  N3 已登记文件的命中数超过登记值 ⇒ 红（只减不增；登记不是豁免）。
  N4 登记项在树上已无命中（陈旧）⇒ 默认告警（STALE），
     --strict-inventory 下判红（供 BLD-201 收口时清零台账）。

A44（前台追加，同源违规；ASTROCS_DESIGN §2.1 + §9.73 裁决 A44）
  A1 活目标 HiPS provenance 不得再出现「权重模式」族键/取值来源：
     lib/infrastructure/aio/** 的 C/C++ 源码中零 ASTROCS_WEIGHT_MODE；
     HiPS provenance 只承载帧级 SNR 与稀疏相对 SNR 比值。
     （docs/** 面由 ci/check_no_weight_mode.py 覆盖；本规则补代码面。）

扫描面：仓库首方 C/C++ 源（lib/ tests/ include/ tools/ scripts/ ci/ cmake/ cli/ runtime/）。
排除：third_party / build / run / .git / archive / legacy / __pycache__（非首方或非活代码）。
注释行（以 // 或 * 或 /* 开头）不计入（避免把条款引用当实现）。

用法
  python3 ci/check_aio_io_boundary.py                    # 扫真实仓库；rc=0 全绿
  python3 ci/check_aio_io_boundary.py --self-test        # 合成树正例+负例（能红能绿）
  python3 ci/check_aio_io_boundary.py --report           # 附带完整台账清单
  python3 ci/check_aio_io_boundary.py --strict-inventory # 陈旧登记项也判红
  python3 ci/check_aio_io_boundary.py --update-inventory # 重算台账（显式；写 ci/ledgers/）

退出码：0 PASS；1 FAIL（含未登记/超限/HARD/A44 命中）；2 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile

# ── 扫描面 ──────────────────────────────────────────────────────────────────
SCAN_ROOTS = ("lib", "tests", "include", "tools", "scripts", "ci", "cmake", "cli", "runtime")
SRC_EXT = (".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".inl")
EXCLUDE_PARTS = ("/third_party/", "/build/", "/run/", "/.git/", "/archive/",
                 "/legacy/", "/__pycache__/", "/node_modules/", "/.venv/")

AIO_PREFIX = "lib/infrastructure/aio/"

# H1：FIX-201 步骤 2/3 的整改对象 —— 必须零文件系统原语（无白名单）。
HARD_ZERO_PREFIXES = (
    "lib/algorithms/drizzle/hips/",   # HiPS 原子发布实现（aio_publish.cpp）
    "lib/algorithms/fits_output/",    # FITS 输出（p3_output.cpp）
)

# H2：块↔文件接口符号（ASTROCS_DESIGN §9「不是生产接口」）。
DIAGNOSTIC_SYMBOLS = (
    "aio_frame_save_cache", "aio_frame_load_cache",
    "aio_frame_export_block_fits", "aio_frame_export_block_xml",
    "aio_frame_export_all_xml", "aio_pipeline_export_xml",
)

INVENTORY_REL = "ci/ledgers/aio_io_boundary_inventory.json"

# ── 规则集（与 FIX-201 验收门同口径） ────────────────────────────────────────
RULES = (
    ("fopen", re.compile(r"(?<![A-Za-z0-9_])(?:_?w?fopen|freopen)\s*\(")),
    # C++ 流族: fstream / ofstream / ifstream / iostream (含 std:: 前缀形态)。
    ("cxx-stream", re.compile(r"(?<![A-Za-z0-9_])(?:std::)?(?:[io]?fstream|iostream)\b")),
    ("open", re.compile(r"(?<![A-Za-z0-9_.>:])(?:::)?_?open\s*\(")),
    ("filesystem", re.compile(r"(?<![A-Za-z0-9_])std::filesystem\b|(?<![A-Za-z0-9_])fs::")),
    ("fs-mutate", re.compile(
        r"(?<![A-Za-z0-9_])(?:::)?_?(?:unlink|rmdir|mkdir|opendir|readdir|closedir|"
        r"fsync|ftruncate|truncate|symlink|_commit)\s*\(")),
    ("win32-fs", re.compile(
        r"(?<![A-Za-z0-9_])(?:MoveFileEx[AW]|CreateDirectory[AW]|FindFirstFile[AW]|"
        r"DeleteFile[AW]|RemoveDirectory[AW])\b")),
)

A44_TOKEN = "ASTROCS_WEIGHT_MODE"
A44_SCAN_PREFIXES = ("lib/infrastructure/aio/",)

# ── 台账分类（首条命中即生效；逐条给理由 + 归属） ────────────────────────────
CATEGORY_RULES = (
    ("lib/infrastructure/pipeline/orchestrator/", "RETIRED-PENDING",
     "ASTROCS_DESIGN §7.1 退役计划内（逐像素方差接线搬进 scheduler 后删除）；删除前不得新增 I/O",
     "§7.1 退役计划 / 后续控制包"),
    ("lib/infrastructure/pipeline/module_loader/", "BOOTSTRAP-TRUST-BOUNDARY",
     "模块装载与安全加载器是 aio 自身的装载前置（自举信任边界），不能依赖 aio 才启动",
     "后续控制包（AIO 装载面）"),
    ("lib/infrastructure/gaia_xpsd_client/", "LOCAL-CATALOG-READER",
     "本地星表（XPSD）解析：非 FITS/HiPS/manifest 面；§2.4 离线零网络",
     "后续控制包（星表面 I/O 收口）"),
    ("lib/infrastructure/acr/", "DORMANT",
     "ACR dormant，不进生产（§1.3 非目标 / §7.1）",
     "无（dormant）"),
    ("lib/infrastructure/hips_browser/", "NOT-IN-PRODUCT",
     "§7.1：未来 GUI 可视化组件，不进产品 manifest",
     "无（非产品）"),
    ("lib/infrastructure/cli/", "OBSERVABILITY-FIXED-ARTIFACTS",
     "§9 登记在册的固定产物（resource_timeseries.csv / resource_summary.json / "
     "worker_balance.csv / alloc_* / 日志）+ CLI 配置读取",
     "FIX-208 / 后续控制包（I/O 收口）"),
    ("lib/infrastructure/scheduler/", "PRODUCTION-RESIDUAL",
     "生产面（scheduler）仍直读配置/产品文件（ifstream / std::filesystem）",
     "后续控制包（I/O 收口）"),
    ("lib/phase1_session/", "PRODUCTION-RESIDUAL",
     "生产面（阶段一会话）仍用 std::filesystem 探路径",
     "后续控制包（I/O 收口）"),
    ("lib/phase3_session/", "PRODUCTION-RESIDUAL",
     "生产面（阶段三会话）仍自建目录（::_mkdir/::mkdir）",
     "后续控制包（I/O 收口）"),
    ("lib/algorithms/shared/dirent_win.h", "POSIX-SHIM",
     "Windows dirent 兼容 shim（复刻第三方 POSIX API），非产品数据面 I/O",
     "无（平台 shim）"),
    ("lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp", "DIAGNOSTIC-TRACE",
     "drizzle 内部 trace（ASTROCS_DRIZZLE_TRACE 环境变量显式开启才生效）的选择集"
     "读写与 lineage/leaf jsonl 落盘；非生产数据通路，属诊断面",
     "后续控制包（诊断面 I/O 收口）"),
    ("lib/algorithms/drizzle/healpix_drizzle/", "PRODUCTION-RESIDUAL",
     "drizzle 模块生产面残留 I/O（FIX-201 已收口 fits_reader.cpp 的 fopen 复制、"
     "astro_sphere_sink.cpp 的 p1_snr.json ifstream、hp_drizzle_api.cpp 的 "
     "operation_counts.json 直写）",
     "后续控制包（I/O 收口）"),
    ("lib/algorithms/coverage/tools/", "DEV-TOOL",
     "开发/诊断命令行工具（非三命令生产路径）", "无（工具面）"),
    ("lib/algorithms/", "PRODUCTION-RESIDUAL",
     "算法面仍有模块自持日志/配置/属性文件 I/O", "后续控制包（I/O 收口）"),
    ("tests/", "TEST-HARNESS",
     "测试夹具与负例构造（非生产数据通路）", "无（测试面）"),
    ("lib/", "PRODUCTION-RESIDUAL",
     "未分类生产面残留 I/O", "后续控制包（I/O 收口）"),
)


# ── 扫描 ────────────────────────────────────────────────────────────────────
def iter_sources(root):
    """确定性顺序产出首方 C/C++ 源文件（相对 POSIX 路径, 绝对路径）。"""
    out = []
    for scan_root in SCAN_ROOTS:
        base = os.path.join(root, scan_root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(dirnames)
            for name in sorted(filenames):
                if os.path.splitext(name)[1].lower() not in SRC_EXT:
                    continue
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                if any(part in "/" + rel for part in EXCLUDE_PARTS):
                    continue
                out.append((rel, full))
    out.sort()
    return out


def _is_comment(line):
    s = line.strip()
    return s.startswith("//") or s.startswith("*") or s.startswith("/*")


def mask_noncode(text):
    """把字符串/字符字面量与注释的内容替换为空格（保留换行与列位置）。

    目的：避免把**字面量里的文字**当成实现 —— 例如
    std::string("fsync(tmp): ") 里的 fsync( 不是文件系统调用。
    标识符本身（fopen/mkdir/... 的调用名）在字面量之外，不受影响。
    """
    out = []
    i = 0
    n = len(text)
    state = 0          # 0=code 1=line-comment 2=block-comment 3=string 4=char
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
                out.append(c)
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
            out.append(c if c == "\n" else " ")
            i += 1
            continue
        # state 3/4: 字面量
        if c == "\\":
            out.append("  ")
            i += 2
            continue
        if (state == 3 and c == '"') or (state == 4 and c == "'"):
            state = 0
            out.append(" ")
            i += 1
            continue
        out.append(c if c == "\n" else " ")
        i += 1
    return "".join(out)


def scan(root):
    """返回 {relpath: [(lineno, rule, snippet), ...]}（注释行不计）。"""
    hits = {}
    for rel, full in iter_sources(root):
        try:
            with open(full, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        masked = mask_noncode(text)
        lines = text.splitlines()
        found = []
        for rule, rx in RULES:
            for m in rx.finditer(masked):
                ln = text.count("\n", 0, m.start()) + 1
                src_line = lines[ln - 1] if 0 < ln <= len(lines) else ""
                if _is_comment(src_line):
                    continue
                found.append((ln, rule, src_line.strip()[:140]))
        if found:
            found.sort()
            hits[rel] = found
    return hits


def scan_a44(root):
    """A44 代码面：活目标 HiPS provenance 的「权重模式」键残留。"""
    hits = []
    for rel, full in iter_sources(root):
        if not any(rel.startswith(p) for p in A44_SCAN_PREFIXES):
            continue
        try:
            with open(full, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in re.finditer(re.escape(A44_TOKEN), text):
            ln = text.count("\n", 0, m.start()) + 1
            hits.append((rel, ln))
    return hits


def scan_diagnostic_calls(root):
    """H2：块↔文件接口在 aio 之外的调用点（生产越界）。"""
    pat = re.compile(r"(?<![A-Za-z0-9_])(%s)\s*\(" % "|".join(DIAGNOSTIC_SYMBOLS))
    hits = []
    for rel, full in iter_sources(root):
        if rel.startswith(AIO_PREFIX):
            continue
        try:
            with open(full, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        masked = mask_noncode(text)
        lines = text.splitlines()
        for m in pat.finditer(masked):
            ln = masked.count("\n", 0, m.start()) + 1
            src_line = lines[ln - 1] if 0 < ln <= len(lines) else ""
            hits.append((rel, ln, m.group(1), src_line.strip()[:140]))
    return hits


def classify(rel):
    for prefix, category, reason, owner in CATEGORY_RULES:
        if rel.startswith(prefix):
            return category, reason, owner
    return "PRODUCTION-RESIDUAL", "未分类首方源 I/O", "后续控制包（I/O 收口）"


# ── 台账 ────────────────────────────────────────────────────────────────────
def load_inventory(root):
    path = os.path.join(root, INVENTORY_REL)
    if not os.path.isfile(path):
        return None
    with open(path, "r", encoding="utf-8") as fh:
        doc = json.load(fh)
    return {e["path"]: e for e in doc.get("entries", [])}


def build_inventory(root, hits):
    entries = []
    for rel in sorted(hits):
        if rel.startswith(AIO_PREFIX) or rel.startswith(HARD_ZERO_PREFIXES):
            continue
        category, reason, owner = classify(rel)
        by_rule = {}
        for _ln, rule, _snip in hits[rel]:
            by_rule[rule] = by_rule.get(rule, 0) + 1
        entries.append({
            "path": rel,
            "hits": len(hits[rel]),
            "rules": by_rule,
            "category": category,
            "reason": reason,
            "owner": owner,
        })
    return {
        "schema": "astrocs-aio-io-boundary-inventory/v1",
        "authority": "ASTROCS_DESIGN.md §9 + 工程控制/RELEASE-02/GAP_AUDIT.md §9.73 裁决 U5",
        "note": ("棘轮上限台账：每个非 aio 命中文件逐条给出理由与归属。命中数只减不增"
                 "（超过登记值判红）；文件转干净后须删除对应条目。本台账不是 waiver —— "
                 "HARD 层（lib/algorithms/drizzle/hips/**、lib/algorithms/fits_output/**、"
                 "块↔文件接口越界调用）不受其约束。"),
        "entries": entries,
    }


# ── 主判据 ──────────────────────────────────────────────────────────────────
def evaluate(root, inventory, strict_inventory):
    hits = scan(root)
    hard, inventory_hits = [], {}
    for rel, found in hits.items():
        if rel.startswith(AIO_PREFIX):
            continue                      # AIO-INTERNAL
        if rel.startswith(HARD_ZERO_PREFIXES):
            hard.append((rel, found))
        else:
            inventory_hits[rel] = found

    unregistered, over, stale = [], [], []
    if inventory is None:
        return {
            "fail_closed": "台账缺失: " + INVENTORY_REL,
            "hard": hard, "unregistered": sorted(inventory_hits),
            "over": [], "stale": [], "diag": [], "a44": [],
            "hits": hits, "inventory_hits": inventory_hits,
        }
    for rel in sorted(inventory_hits):
        if rel not in inventory:
            unregistered.append(rel)
        elif len(inventory_hits[rel]) > int(inventory[rel].get("hits", 0)):
            over.append((rel, int(inventory[rel].get("hits", 0)),
                         len(inventory_hits[rel])))
    for rel in sorted(inventory):
        if rel not in inventory_hits:
            stale.append(rel)

    diag = scan_diagnostic_calls(root)
    a44 = scan_a44(root)
    return {
        "fail_closed": None,
        "hard": hard, "unregistered": unregistered, "over": over,
        "stale": stale, "diag": diag, "a44": a44,
        "hits": hits, "inventory_hits": inventory_hits,
    }


def render(res, report, strict_inventory):
    print("AIO-IO-BOUNDARY (ASTROCS_DESIGN §9「aio 是文件级唯一 I/O 边界」; "
          "§9.73 裁决 U5)")
    if res["fail_closed"]:
        print("  FAIL-CLOSED: " + res["fail_closed"])
        return 2
    bad = 0
    print("  HARD-H1 (HiPS 发布实现 / FITS 输出内文件系统原语 = 0):")
    if res["hard"]:
        bad += len(res["hard"])
        for rel, found in res["hard"]:
            for ln, rule, snip in found:
                print("    VIOLATION %s:%d [%s] %s" % (rel, ln, rule, snip))
    else:
        print("    PASS")
    print("  HARD-H2 (块↔文件接口在 aio 之外的调用点 = 0):")
    if res["diag"]:
        bad += len(res["diag"])
        for rel, ln, sym, snip in res["diag"]:
            print("    VIOLATION %s:%d %s() %s" % (rel, ln, sym, snip))
    else:
        print("    PASS")
    print("  A44 (活目标 HiPS provenance 的「权重模式」键残留 = 0):")
    if res["a44"]:
        bad += len(res["a44"])
        for rel, ln in res["a44"]:
            print("    VIOLATION %s:%d %s" % (rel, ln, A44_TOKEN))
    else:
        print("    PASS")
    print("  INVENTORY (棘轮上限台账 %s):" % INVENTORY_REL)
    print("    非 aio 命中文件 %d；已登记 %d；未登记 %d"
          % (len(res["inventory_hits"]),
             len(res["inventory_hits"]) - len(res["unregistered"]),
             len(res["unregistered"])))
    if res["unregistered"]:
        bad += len(res["unregistered"])
        for rel in res["unregistered"]:
            print("    UNREGISTERED %s (%d 处) — 新增越界 I/O，必须整改或登记理由"
                  % (rel, len(res["inventory_hits"][rel])))
    if res["over"]:
        bad += len(res["over"])
        for rel, was, now in res["over"]:
            print("    GROWTH %s: 登记 %d → 实际 %d（只减不增）" % (rel, was, now))
    if res["stale"]:
        tag = "VIOLATION" if strict_inventory else "STALE"
        if strict_inventory:
            bad += len(res["stale"])
        for rel in res["stale"]:
            print("    %s %s — 已无命中，请从台账删除该条" % (tag, rel))
    if report:
        print("  ── 台账明细（按类别） ──")
        for rel in sorted(res["inventory_hits"]):
            cat, _reason, owner = classify(rel)
            print("    %-24s %-58s %2d  %s"
                  % (cat, rel, len(res["inventory_hits"][rel]), owner))
    if bad:
        print("AIO-IO-BOUNDARY_FAIL: %d 项违规（HARD/A44/未登记/超限）" % bad)
        return 1
    print("AIO-IO-BOUNDARY_PASS: HARD 层 = 0；A44 = 0；台账无未登记/超限"
          + ("；台账无陈旧条目" if strict_inventory else ""))
    return 0


# ── 自检（正例 + 负例；能红能绿） ────────────────────────────────────────────
def _mk(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def self_test():
    """合成同构树上证明：干净树判绿；八类注入各自判红；aio 内部不误报。"""
    ok = True
    with tempfile.TemporaryDirectory(prefix="aio_io_boundary_selftest_") as td:
        aio = os.path.join(td, "lib/infrastructure/aio/src")
        alg = os.path.join(td, "lib/algorithms/clean_mod")
        hips = os.path.join(td, "lib/algorithms/drizzle/hips/src")
        fits = os.path.join(td, "lib/algorithms/fits_output")
        cli = os.path.join(td, "lib/infrastructure/cli")
        # 正例：aio 内部允许 I/O；算法面走 aio 原语（零命中）；cli 命中 1 处且已登记
        _mk(os.path.join(aio, "aio_atomic_file.h"),
            "#include <cstdio>\n"
            "static int f(void){ FILE* h = std::fopen(\"x\",\"rb\"); return h?0:1; }\n")
        _mk(os.path.join(alg, "clean.cpp"),
            "#include \"aio_atomic_file.h\"\n"
            "int g(void){ return aio_atomic::make_dirs(\"/tmp/x\"); }\n")
        _mk(os.path.join(cli, "obs.cpp"),
            "void w(void){ std::ofstream f(\"resource_timeseries.csv\"); }\n")
        inv = {"lib/infrastructure/cli/obs.cpp":
               {"path": "lib/infrastructure/cli/obs.cpp", "hits": 1}}
        res = evaluate(td, inv, strict_inventory=True)
        pos_ok = (not res["hard"] and not res["unregistered"] and not res["over"]
                  and not res["stale"] and not res["diag"] and not res["a44"])
        ok = ok and pos_ok
        print("  self-test 正例(干净树 + aio 内部不误报 + 登记面): [%s]"
              % ("PASS" if pos_ok else "FAIL"))

        # 负例 1：未登记的新文件出现非 aio 写 ⇒ 必红
        _mk(os.path.join(alg, "sneaky.cpp"),
            "void s(void){ FILE* f = std::fopen(\"out.bin\",\"wb\"); }\n")
        res = evaluate(td, inv, strict_inventory=True)
        n1 = "lib/algorithms/clean_mod/sneaky.cpp" in res["unregistered"]
        ok = ok and n1
        print("  self-test 负例1(未登记新越界写): flagged=%s [%s]"
              % (n1, "PASS" if n1 else "FAIL"))

        # 负例 2：HARD 层（fits_output）注入 ⇒ 必红（台账不可豁免）
        _mk(os.path.join(fits, "p3_output.cpp"),
            "void p(void){ std::FILE* f = std::fopen(\"o.fits\",\"rb\"); }\n")
        res = evaluate(td, inv, strict_inventory=True)
        n2 = any(r.startswith("lib/algorithms/fits_output/") for r, _ in res["hard"])
        ok = ok and n2
        print("  self-test 负例2(HARD 层 fits_output 注入): flagged=%s [%s]"
              % (n2, "PASS" if n2 else "FAIL"))
        os.remove(os.path.join(fits, "p3_output.cpp"))

        # 负例 3：HARD 层（HiPS 发布实现）注入 ⇒ 必红
        _mk(os.path.join(hips, "aio_publish.cpp"),
            "void q(void){ ::mkdir(\"/x\", 0755); }\n")
        res = evaluate(td, inv, strict_inventory=True)
        n3 = any(r.startswith("lib/algorithms/drizzle/hips/") for r, _ in res["hard"])
        ok = ok and n3
        print("  self-test 负例3(HARD 层 aio_publish 注入): flagged=%s [%s]"
              % (n3, "PASS" if n3 else "FAIL"))
        os.remove(os.path.join(hips, "aio_publish.cpp"))

        # 负例 4：块↔文件接口在 aio 之外被调用 ⇒ 必红
        _mk(os.path.join(alg, "uses_diag.cpp"),
            "int u(void* fr, const char* p){ return aio_frame_export_all_xml(fr, p); }\n")
        res = evaluate(td, inv, strict_inventory=True)
        n4 = any(r.endswith("uses_diag.cpp") for r, _l, _s, _x in res["diag"])
        ok = ok and n4
        print("  self-test 负例4(块↔文件接口越界调用): flagged=%s [%s]"
              % (n4, "PASS" if n4 else "FAIL"))
        os.remove(os.path.join(alg, "uses_diag.cpp"))

        # 负例 5：已登记文件命中数增长 ⇒ 必红（台账不是豁免）
        _mk(os.path.join(cli, "obs.cpp"),
            "void w(void){ std::ofstream f(\"a.csv\"); std::ofstream g(\"b.csv\"); }\n")
        res = evaluate(td, inv, strict_inventory=True)
        n5 = any(r == "lib/infrastructure/cli/obs.cpp" for r, _w, _n in res["over"])
        ok = ok and n5
        print("  self-test 负例5(登记文件命中数增长): flagged=%s [%s]"
              % (n5, "PASS" if n5 else "FAIL"))
        _mk(os.path.join(cli, "obs.cpp"),
            "void w(void){ std::ofstream f(\"resource_timeseries.csv\"); }\n")

        # 负例 6：A44 权重模式键残留 ⇒ 必红
        _mk(os.path.join(aio, "aio_hips_writer.cpp"),
            "static const char* k = \"%s\";\n" % A44_TOKEN)
        res = evaluate(td, inv, strict_inventory=True)
        n6 = any(r.endswith("aio_hips_writer.cpp") for r, _l in res["a44"])
        ok = ok and n6
        print("  self-test 负例6(A44 权重模式键残留): flagged=%s [%s]"
              % (n6, "PASS" if n6 else "FAIL"))
        os.remove(os.path.join(aio, "aio_hips_writer.cpp"))

        # 负例 7：台账陈旧 ⇒ strict 下判红、默认仅告警
        os.remove(os.path.join(cli, "obs.cpp"))
        res_s = evaluate(td, inv, strict_inventory=True)
        res_l = evaluate(td, inv, strict_inventory=False)
        n7s = any(r == "lib/infrastructure/cli/obs.cpp" for r in res_s["stale"])
        n7l = any(r == "lib/infrastructure/cli/obs.cpp" for r in res_l["stale"])
        n7 = n7s and n7l
        ok = ok and n7
        print("  self-test 负例7(台账陈旧: strict 红 / 默认告警): strict=%s default=%s [%s]"
              % (n7s, n7l, "PASS" if n7 else "FAIL"))

        # 负例 8：台账缺失 ⇒ fail-closed（不得把「文件不存在」当「无违规」）
        res = evaluate(td, None, strict_inventory=False)
        n8 = res["fail_closed"] is not None
        ok = ok and n8
        print("  self-test 负例8(台账缺失 fail-closed): flagged=%s [%s]"
              % (n8, "PASS" if n8 else "FAIL"))

    print("AIO-IO-BOUNDARY_SELF-TEST_%s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="aio 文件级唯一 I/O 边界机器判据 (FIX-201)")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--strict-inventory", action="store_true")
    ap.add_argument("--update-inventory", action="store_true")
    ap.add_argument("--root", default=None)
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    root = args.root or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if not os.path.isdir(os.path.join(root, "lib")):
        print("FAIL-CLOSED: %s 下无 lib/ —— 扫描面不可用" % root, file=sys.stderr)
        return 2

    if args.update_inventory:
        hits = scan(root)
        inv = build_inventory(root, hits)
        path = os.path.join(root, INVENTORY_REL)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        tmp = path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(inv, fh, ensure_ascii=False, indent=2, sort_keys=False)
            fh.write("\n")
        os.replace(tmp, path)
        print("INVENTORY-UPDATED: %s (%d entries)" % (INVENTORY_REL, len(inv["entries"])))
        return 0

    inventory = load_inventory(root)
    res = evaluate(root, inventory, args.strict_inventory)
    return render(res, args.report, args.strict_inventory)


if __name__ == "__main__":
    raise SystemExit(main())
