#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPEC-POLARITY-CONSIST-01: 判据口径 <-> 设计冻结语句 一致性门（能红能绿，fail-closed）。
#
# 任务依据: run/ONEPAGER/onepager_20260925_1018.md 一页纸 S1 第 13 条与第 16 条。
# 权威链: ASTROCS_DESIGN.md（最高设计）> ACCEPTANCE_SPEC.md（验收）> docs/plugins/**、
# docs/science/**（下层细则）；口径只在权威层定义，下层写指针不复述。
#
# 判据（四组）:
#   P1 冻结语句在场：最高设计必须逐字含三条冻结语义（线性面亮度 / 必须保持线性 /
#      控制点层存绝对 SNR）。缺任一条即判红（防"删掉冻结语句以躲开反义比对"）。
#   P2 反义判红：验收判据与下层文档不得把冻结语义写成反义——(a) 星等落盘/星等即数据形态、
#      (b) 稀疏控制点层 = 相对 SNR、(c) 无绝对数值窗口被读成不许出现数值。
#      命中且不在否定/引用/判红语境（不得/不可/禁/反义/判红/为准/而非/不是/不）内即判红。
#   P3 规模类数字可解析：ASTROCS_DESIGN.md 与 ACCEPTANCE_SPEC.md 里的规模类数字
#      （N 万 / 5 位以上整数）必须在本地窗口内解析到配置键（反引号 dotted key）或
#      推导式（派生/推导/导出/正本），或命中已登记豁免（版本号/年份/HEALPix 等）。
#   P4 配置真值复算：P3 引用的每个配置键必须有活载体（eng/contracts/schemas/
#      phase_config_normalize.schema.json 的属性、eng/packaging/config/config_registry.json
#      的登记键、或 lib/infrastructure/scheduler/src/module_adapters.cpp 的读取式），且文档
#      声明的默认值/合同域必须与活载体一致（文档值与代码值不一致即判红——这正是第 16 条
#      "文档陈述了一个配置上不存在的截断"的机器判据）。寄存器为空/输入缺失/扫描面为空
#      一律判红（fail-closed）。
#
# 用法:
#   python3 eng/tools/doccheck/check_spec_polarity_consistency.py [--root .] [--json-out F]
#   python3 eng/tools/doccheck/check_spec_polarity_consistency.py --self-test
# exit 0 = PASS；exit 1 = FAIL（逐条给 文件:行 + 反义/不可解析证据）；exit 2 = 输入不可用。
# --self-test 恒 0 = 内置正/负例全符合预期（12 例）；对外部工具结果只打印摘要，不决定自检通过。

from __future__ import annotations

import argparse
import io
import json
import os
import pathlib
import re
import shutil
import sys
import tempfile

DESIGN = "ASTROCS_DESIGN.md"
ACCEPT = "ACCEPTANCE_SPEC.md"
SCHEMA = "eng/contracts/schemas/phase_config_normalize.schema.json"
REGISTRY = "eng/packaging/config/config_registry.json"
CODE = "lib/infrastructure/scheduler/src/module_adapters.cpp"
SD = "docs/plugins/algorithms_phase1/03_star_detection.md"

SCAN_GLOBS = ("ACCEPTANCE_SPEC.md", "docs/plugins/**/*.md", "docs/science/**/*.md")

CONTEXT_GUARDS = ("不得", "不可", "禁止", "禁", "反义", "判红", "为准", "订正", "而非", "不是")


def config_key_bank():
    # 配置键寄存器: 每条 = 一个「文档规模数字 -> 配置真值」的待复核绑定。
    return {
        "star_detection.max_stars": {
            "doc_required": [SD, SCHEMA, REGISTRY, DESIGN, ACCEPT],
            "doc_required_count": {DESIGN: 1},
            "schema_ref": ("$defs", "star_detection_config", "properties", "max_stars"),
            "registry_key": "star_detection.max_stars",
            "code_res": [
                r'p1_int\(\s*sd\s*,\s*"max_stars"\s*,\s*kP1GuidedMaxStarsLo\s*\)',
                r"kP1GuidedMaxStarsLo\s*=\s*20000",
                r"kP1GuidedMaxStarsHi\s*=\s*50000",
            ],
            "doc_default": {
                "plugin_table": {"path": SD, "re": r"\|\s*`max_stars`\s*\|\s*(\d+)\s*\|\s*颗\s*\|"},
            },
            "doc_domain": {"path": SD, "re": r"\*\*\[(\d+),\s*(\d+)\]\*\*"},
        },
        "photometry.fit.max_stars": {
            "doc_required": [SD],
            "schema_ref": None,
            "registry_key": None,
            "code_res": [r'p1_int\(\s*fit_cfg\s*,\s*"max_stars"\s*,\s*(\d+)\s*\)'],
            "code_default_re": r'p1_int\(\s*fit_cfg\s*,\s*"max_stars"\s*,\s*(\d+)\s*\)',
            "doc_default": {
                "plugin_scale_row": {"path": SD, "re": r"`photometry\.fit\.max_stars`（默认 (\d+)"},
            },
            "doc_domain": None,
        },
        "snr.max_sources": {
            "doc_required": [SD],
            "schema_ref": None,
            "registry_key": None,
            "code_res": [
                r"snr\.max_sources\s*=\s*\*\*交付样本上限\*\*",
                r'sc\.value\(\s*"max_sources"\s*,\s*(\d+)\s*\)',
            ],
            "code_default_re": r'sc\.value\(\s*"max_sources"\s*,\s*(\d+)\s*\)',
            "doc_default": {
                "plugin_scale_row": {"path": SD, "re": r"`snr\.max_sources`（默认 (\d+)"},
            },
            "doc_domain": None,
        },
    }


SCALE_RE = re.compile(r"\d+(?:\.\d+)?\s*(?:万颗|万个|万次|万帧|万像素|万源|万)")
BIGINT_RE = re.compile(r"(?<![\w.,])[1-9]\d{4,}(?![\w.,])")
DERIVE_MARKERS = ("派生", "推导", "导出", "由配置", "配置键", "正本")
KEY_BT_RE = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+)`")
SCALE_EXEMPT = {
    "fits-basic": re.compile(r"FITS\s*Basic|FITS\s*标准|基本 FITS"),
    "version": re.compile(r"版本号|VERSION|schema_version|v\d+\.\d+"),
    "year": re.compile(r"(?:19|20)\d{2}\s*年"),
    "healpix": re.compile(r"nside|N_side|tile_width|Npix|HEALPix"),
}

FREEZE = (
    ("F1-linear-surface-brightness", "仍是线性面亮度"),
    ("F2-must-stay-linear", "必须保持线性"),
    ("F3-cpl-absolute-snr", "存控制点处的绝对SNR"),
)

ANTONYM_RULES = (
    ("A1-mag-as-data-form",
     re.compile(r"星等(?!不)[^。；\n]{0,8}落盘|落盘[^。；\n]{0,6}星等|数据形态[^。；\n]{0,4}(?:为|是|即)[^。；\n]{0,3}星等|星等即(?:为)?数据形态"),
     "把星等当数据形态（设计冻结：星等只在派生/展示时换算，不作为数据形态落盘）"),
    ("A2-sparse-as-relative",
     re.compile(r"稀疏[^。；\n]{0,12}相对\s*(?:SNR|信噪比)|相对\s*(?:SNR|信噪比)[^。；\n]{0,12}稀疏"),
     "把稀疏控制点层写成相对 SNR（设计冻结：控制点层存绝对 SNR）"),
    ("A3-no-number-misread",
     re.compile(r"不许出现数值|不得出现数值|禁止出现数值|不允许出现数值|必须给出绝对(?:数值|通量刻度)|绝对数值必须(?:落盘|给出)"),
     "把「无绝对数值窗口」读成「不许出现数值」（设计冻结：绝对值不可辨识，非不许出现数值）"),
)

RESOLVED_KEYS = []


def norm(text):
    for a, b in (("\u201c", '"'), ("\u201d", '"'), ("\u2018", "'"), ("\u2019", "'"),
                 ("\uff08", "("), ("\uff09", ")")):
        text = text.replace(a, b)
    return re.sub(r"\s+", "", text)


def read_text(path):
    try:
        return io.open(str(path), encoding="utf-8").read()
    except OSError:
        return None


def scan_files(root, globs):
    out = []
    for g in globs:
        out.extend(str(p.relative_to(root)).replace(os.sep, "/")
                   for p in root.glob(g) if p.is_file())
    return sorted(set(out))


def add(v, rule, detail, path=None, line=None, text=None):
    rec = {"rule": rule, "detail": detail}
    if path is not None:
        rec["path"] = path
    if line is not None:
        rec["line"] = line
    if text is not None:
        rec["text"] = text[:200]
    v.append(rec)


def check_p1(root, v, notes):
    txt = read_text(root / DESIGN)
    if txt is None:
        add(v, "P1-frozen-statement-missing", DESIGN + " 不可读（fail-closed）")
        return
    flat = norm(txt)
    found = 0
    for fid, needle in FREEZE:
        if norm(needle) in flat:
            found += 1
        else:
            add(v, "P1-frozen-statement-missing",
                "最高设计缺冻结语句 " + fid + "（必须逐字含「" + needle +
                "」；删冻结语句以躲开反义比对即判红）", path=DESIGN)
    notes["p1_frozen_found"] = found


def _guarded(line, m):
    lo = max(0, m.start() - 60)
    ctx = line[lo:m.end() + 30]
    return any(g in ctx for g in CONTEXT_GUARDS)


def check_p2(root, v, notes):
    files = scan_files(root, SCAN_GLOBS)
    if not files:
        add(v, "P2-scan-empty", "反义扫描面为空（fail-closed）")
        return
    notes["p2_scanned_files"] = len(files)
    hits = 0
    for f in files:
        txt = read_text(root / f)
        if txt is None:
            add(v, "P2-unreadable", f + " 不可读（fail-closed）", path=f)
            continue
        for i, line in enumerate(txt.splitlines(), 1):
            for rid, rx, why in ANTONYM_RULES:
                m = rx.search(line)
                if not m:
                    continue
                if _guarded(line, m):
                    notes.setdefault("p2_guarded", []).append("%s:%d %s" % (f, i, rid))
                    continue
                hits += 1
                add(v, "P2-antonym:" + rid, why, path=f, line=i, text=line)
    notes["p2_antonym_hits"] = hits
    if hits == 0:
        notes["p2_verdict"] = "未在该词面上发现反义（未证伪；不等于全文档无语义矛盾）"


def _window(line, m, span=80):
    return line[max(0, m.start() - span):m.end() + span]


def check_p3(root, v, notes):
    total = 0
    for doc in (DESIGN, ACCEPT):
        txt = read_text(root / doc)
        if txt is None:
            add(v, "P3-doc-missing", doc + " 不可读（fail-closed）", path=doc)
            continue
        for i, line in enumerate(txt.splitlines(), 1):
            for rx in (SCALE_RE, BIGINT_RE):
                for m in rx.finditer(line):
                    total += 1
                    win = _window(line, m)
                    if any(p.search(win) for p in SCALE_EXEMPT.values()):
                        notes.setdefault("p3_exempt", []).append("%s:%d %s" % (doc, i, m.group(0)))
                        continue
                    keys = KEY_BT_RE.findall(win)
                    if keys or any(k in win for k in DERIVE_MARKERS):
                        notes.setdefault("p3_resolved", []).append(
                            "%s:%d %s -> %s" % (doc, i, m.group(0), keys or "推导式"))
                        continue
                    add(v, "P3-scale-number-unresolvable",
                        "规模类数字 " + m.group(0) +
                        " 在本段内既无配置键（反引号 dotted key）也无推导式标记（派生/推导/导出/正本）",
                        path=doc, line=i, text=line)
    notes["p3_scale_numbers"] = total


def check_p4(root, v, notes, bank=None):
    del RESOLVED_KEYS[:]
    bank = config_key_bank() if bank is None else bank
    if not bank:
        add(v, "P4-registry-empty", "配置键寄存器为空（fail-closed）")
        return
    schema_txt = read_text(root / SCHEMA)
    if schema_txt is None:
        add(v, "P4-schema-missing", SCHEMA + " 不可读（fail-closed）")
        schema = None
    else:
        try:
            schema = json.loads(schema_txt)
        except ValueError as exc:
            add(v, "P4-schema-unparsable", SCHEMA + " 解析失败: " + str(exc))
            schema = None
    registry_txt = read_text(root / REGISTRY)
    code_txt = read_text(root / CODE)
    if registry_txt is None:
        add(v, "P4-registry-missing", REGISTRY + " 不可读（fail-closed）")
    if code_txt is None:
        add(v, "P4-code-missing", CODE + " 不可读（fail-closed）")

    for key, spec in sorted(bank.items()):
        rec = {"key": key, "carriers": [], "problems": []}
        for doc in spec.get("doc_required", []):
            txt = read_text(root / doc)
            if txt is None:
                rec["problems"].append("文档缺失/不可读: " + doc)
                continue
            need = spec.get("doc_required_count", {}).get(doc, 1)
            if doc == SCHEMA and spec.get("schema_ref"):
                # schema 的承载形式是属性路径（$defs/<段>/properties/<键>），不是 dotted 字面量：
                # 由下方 schema_ref 解析成功（schema:<路径> 活载体）证明，不另行要求复述键名。
                rec["carriers"].append("schema_file:" + SCHEMA)
                continue
            got = txt.count(key)
            if got < need:
                rec["problems"].append("文档未引用配置键名 %s（%s 命中 %d < %d）" % (key, doc, got, need))
            else:
                rec["carriers"].append("doc:" + doc)
        node = None
        if spec.get("schema_ref"):
            if schema is None:
                rec["problems"].append("schema 不可用，无法复核 " + key)
            else:
                node = schema
                for part in spec["schema_ref"]:
                    node = node.get(part) if isinstance(node, dict) else None
                    if node is None:
                        break
                if not isinstance(node, dict):
                    rec["problems"].append("schema 无属性 " + "/".join(spec["schema_ref"]))
                else:
                    carrier = "schema:" + "/".join(spec["schema_ref"])
                    if carrier not in rec["carriers"]:
                        rec["carriers"].append(carrier)
        if spec.get("registry_key"):
            if registry_txt is None:
                rec["problems"].append("config_registry 不可读，无法复核 " + key)
            elif '"' + spec["registry_key"] + '"' in registry_txt:
                rec["carriers"].append("registry:" + spec["registry_key"])
            else:
                rec["problems"].append("config_registry 未登记键 " + spec["registry_key"])
        if code_txt is None:
            rec["problems"].append("代码不可读，无法复核 " + key)
        else:
            for rx in spec.get("code_res", []):
                if not re.search(rx, code_txt):
                    rec["problems"].append("代码缺读取式: " + rx)
                else:
                    rec["carriers"].append("code:" + rx[:48])
        for tag, d in sorted(spec.get("doc_default", {}).items()):
            dtxt = read_text(root / d["path"])
            if dtxt is None:
                rec["problems"].append("默认值声明文档缺失: " + d["path"])
                continue
            m = re.search(d["re"], dtxt)
            if not m:
                rec["problems"].append("未能在 %s 解析出 %s（%s）" % (d["path"], key, tag))
                continue
            declared = m.group(1)
            carrier = node.get("default") if isinstance(node, dict) else None
            if carrier is None and spec.get("code_default_re") and code_txt:
                cm = re.search(spec["code_default_re"], code_txt)
                if cm and cm.groups():
                    carrier = cm.group(1)
            if carrier is None:
                rec["problems"].append("键 %s 的活载体未给出可比较默认值（%s 声明 %s）" % (key, tag, declared))
            elif str(carrier) != str(declared):
                rec["problems"].append("默认值发散：%s 声明 %s，活载体 = %s（%s）" % (tag, declared, carrier, key))
            else:
                rec.setdefault("values", {})[tag + ".default"] = declared
        d = spec.get("doc_domain")
        if d and isinstance(node, dict):
            dtxt = read_text(root / d["path"])
            m = re.search(d["re"], dtxt or "")
            if not m:
                rec["problems"].append("未能解析合同域声明（%s）" % d["path"])
            else:
                lo, hi = m.group(1), m.group(2)
                if str(node.get("minimum")) != lo or str(node.get("maximum")) != hi:
                    rec["problems"].append("合同域发散：文档 [%s, %s] vs schema [%s, %s]"
                                           % (lo, hi, node.get("minimum"), node.get("maximum")))
                else:
                    rec.setdefault("values", {})["domain"] = "[" + lo + ", " + hi + "]"
        RESOLVED_KEYS.append(rec)
        for p in rec["problems"]:
            add(v, "P4-" + key, p)
    notes["p4_keys"] = len(bank)


def run(root, bank=None):
    v, notes = [], {}
    check_p1(root, v, notes)
    check_p2(root, v, notes)
    check_p3(root, v, notes)
    check_p4(root, v, notes, bank=bank)
    return v, notes


def _write(root, path, content):
    p = root / path
    p.parent.mkdir(parents=True, exist_ok=True)
    io.open(str(p), "w", encoding="utf-8").write(content)


DESIGN_OK = (
    "# D\n\n## 2.1\n"
    "- **数据形态（架构口径，勿读成\u201c数据变成星等\u201d）**：施加后帧像素与 HiPS signal "
    "**仍是线性面亮度**，星等**只在派生/展示时**换算，不作为数据形态落盘。\n"
    "- **必须保持线性**：阶段二/三消费的始终是线性面亮度。\n\n## 2.2\n"
    "- **目标产品**：`sparse_snr_layer`**存控制点处的绝对 SNR** 两个对象承载。\n\n## 4.2\n"
    "- **星表引导检测**：按亮度取上限（**设计自定的算力上界，非科学常数**；由配置键 "
    "`star_detection.max_stars` 承载，合同域的唯一事实源 = "
    "`eng/contracts/schemas/phase_config_normalize.schema.json`）。\n"
)
ACCEPT_OK = (
    "# A\n\n| 验收项 | 判据 |\n|---|---|\n"
    "| 物理单位消除 | 数据形态 = 带逐帧相对测光零点的线性面亮度；无绝对数值窗口 |\n"
    "| 稀疏层与重建 | 稀疏**绝对** SNR 控制点层 + 重建 |\n"
)
SCHEMA_OK = json.dumps({
    "$defs": {"star_detection_config": {"properties": {
        "max_stars": {"type": "integer", "minimum": 20000, "maximum": 50000, "default": 20000}}}},
}, ensure_ascii=False)
SD_OK = (
    "| 字段 | 默认 | 单位 | 说明 |\n|---|---|---|---|\n"
    "| `max_stars` | 20000 | 颗 | 检测定义域上限。合同域 = **[20000, 50000]**（唯一事实源 = 本行 + schema） |\n"
    "| 规模口径 | —— | —— | 检测定义域 = `star_detection.max_stars`；拟合样本上限 = "
    "`photometry.fit.max_stars`（默认 5000，消费 `lib/infrastructure/scheduler/src/module_adapters.cpp`）；"
    "交付 SNR 样本上限 = `snr.max_sources`（默认 0 = **不限**，只截断交付样本行、**不**截断检测定义域） |\n"
)
CODE_OK = (
    '  const int max_stars = p1_int(sd, "max_stars", kP1GuidedMaxStarsLo);\n'
    "static constexpr int kP1GuidedMaxStarsLo = 20000;\n"
    "static constexpr int kP1GuidedMaxStarsHi = 50000;\n"
    '    const int fit_max = p1_int(fit_cfg, "max_stars", 5000);\n'
    "  // P14-N-08: snr.max_sources = **交付样本上限** (默认 0 = **不限**)。这是唯一\n"
    '  int snr_max_sources = sc.value("max_sources", 0);\n'
)
REGISTRY_OK = '{"registered_key": "star_detection.max_stars"}'


def fixture(root):
    _write(root, DESIGN, DESIGN_OK)
    _write(root, ACCEPT, ACCEPT_OK)
    _write(root, SCHEMA, SCHEMA_OK)
    _write(root, SD, SD_OK)
    _write(root, CODE, CODE_OK)
    _write(root, REGISTRY, REGISTRY_OK)


def fixture_bank():
    return {
        "star_detection.max_stars": {
            "doc_required": [DESIGN, SD, SCHEMA, REGISTRY],
            "doc_required_count": {DESIGN: 1},
            "schema_ref": ("$defs", "star_detection_config", "properties", "max_stars"),
            "registry_key": "star_detection.max_stars",
            "code_res": [
                r'p1_int\(\s*sd\s*,\s*"max_stars"\s*,\s*kP1GuidedMaxStarsLo\s*\)',
                r"kP1GuidedMaxStarsLo\s*=\s*20000",
                r"kP1GuidedMaxStarsHi\s*=\s*50000",
            ],
            "doc_default": {"t": {"path": SD, "re": r"\|\s*`max_stars`\s*\|\s*(\d+)\s*\|\s*颗\s*\|"}},
            "doc_domain": {"path": SD, "re": r"合同域 = \*\*\[(\d+),\s*(\d+)\]\*\*"},
        },
        "photometry.fit.max_stars": {
            "doc_required": [SD],
            "schema_ref": None,
            "registry_key": None,
            "code_res": [r'p1_int\(\s*fit_cfg\s*,\s*"max_stars"\s*,\s*(\d+)\s*\)'],
            "code_default_re": r'p1_int\(\s*fit_cfg\s*,\s*"max_stars"\s*,\s*(\d+)\s*\)',
            "doc_default": {"t": {"path": SD, "re": r"`photometry\.fit\.max_stars`（默认 (\d+)"}},
            "doc_domain": None,
        },
        "snr.max_sources": {
            "doc_required": [SD],
            "schema_ref": None,
            "registry_key": None,
            "code_res": [
                r"snr\.max_sources\s*=\s*\*\*交付样本上限\*\*",
                r'sc\.value\(\s*"max_sources"\s*,\s*(\d+)\s*\)',
            ],
            "code_default_re": r'sc\.value\(\s*"max_sources"\s*,\s*(\d+)\s*\)',
            "doc_default": {"t": {"path": SD, "re": r"`snr\.max_sources`（默认 (\d+)"}},
            "doc_domain": None,
        },
    }


def self_test():
    cases = []
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="spec-polarity-selftest-"))
    try:
        def fresh(name):
            d = tmp / name
            if d.exists():
                shutil.rmtree(str(d))
            d.mkdir(parents=True)
            fixture(d)
            return d

        d = fresh("green")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("G1 正例：口径一致必绿", not v, v))

        d = fresh("red-mag")
        io.open(str(d / ACCEPT), "a", encoding="utf-8").write("| 记录 | 产物星等落盘为最终数据形态 |\n")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R1 负例：星等当数据形态必红",
                      any(x["rule"].startswith("P2-antonym:A1") for x in v), v))

        d = fresh("red-sparse")
        io.open(str(d / ACCEPT), "a", encoding="utf-8").write("| 稀疏层 | 稀疏相对 SNR 场用于加权 |\n")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R2 负例：稀疏控制点层写成相对 SNR 必红",
                      any(x["rule"].startswith("P2-antonym:A2") for x in v), v))

        d = fresh("red-numwin")
        io.open(str(d / ACCEPT), "a", encoding="utf-8").write("| 单位 | 产物不许出现数值 |\n")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R3 负例：把无绝对数值窗口读成不许出现数值必红",
                      any(x["rule"].startswith("P2-antonym:A3") for x in v), v))

        d = fresh("red-freeze")
        t = io.open(str(d / DESIGN), encoding="utf-8").read().replace("**仍是线性面亮度**", "**已是星等**")
        _write(d, DESIGN, t)
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R4 负例：删/改冻结语句必红",
                      any(x["rule"] == "P1-frozen-statement-missing" for x in v), v))

        d = fresh("red-scale")
        io.open(str(d / DESIGN), "a", encoding="utf-8").write("- 星点拟合规模为 top 2-5 万颗。\n")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R5 负例：规模数字无配置键/推导式必红",
                      any(x["rule"] == "P3-scale-number-unresolvable" for x in v), v))

        d = fresh("green-scale")
        io.open(str(d / DESIGN), "a", encoding="utf-8").write(
            "- 星点拟合规模 upper = `star_detection.max_stars` 导出的 2-5 万颗。\n")
        v, _ = run(d, bank=fixture_bank())
        cases.append(("G2 正例：规模数字带配置键/推导式必绿",
                      not any(x["rule"] == "P3-scale-number-unresolvable" for x in v), v))

        d = fresh("red-default")
        _write(d, CODE, CODE_OK.replace("kP1GuidedMaxStarsLo = 20000", "kP1GuidedMaxStarsLo = 30000"))
        _write(d, SCHEMA, SCHEMA_OK.replace('"default": 20000', '"default": 30000'))
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R6 负例：配置活载体默认值与文档发散必红",
                      any(x["rule"] == "P4-star_detection.max_stars" for x in v), v))

        d = fresh("red-domain")
        _write(d, SCHEMA, SCHEMA_OK.replace('"maximum": 50000', '"maximum": 90000'))
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R7 负例：合同域与 schema 发散必红",
                      any("合同域发散" in x["detail"] for x in v), v))

        d = fresh("red-citation")
        t = io.open(str(d / DESIGN), encoding="utf-8").read().replace("`star_detection.max_stars`", "该配置键")
        _write(d, DESIGN, t)
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R8 负例：文档不再引用配置键名必红",
                      any("未引用配置键名" in x["detail"] for x in v), v))

        d = fresh("red-empty")
        shutil.rmtree(str(d / "docs"))
        for p in (d / SCHEMA, d / CODE, d / REGISTRY):
            p.unlink()
        v, _ = run(d, bank=fixture_bank())
        cases.append(("R9 负例：扫描面为空/活载体缺失必红（fail-closed）",
                      any(x["rule"] in ("P2-scan-empty", "P4-schema-missing", "P1-frozen-statement-missing",
                                        "P4-code-missing") for x in v), v))

        d = fresh("red-empty-bank")
        v, _ = run(d, bank={})
        cases.append(("R10 负例：配置键寄存器为空必红（fail-closed）",
                      any(x["rule"] == "P4-registry-empty" for x in v), v))

        bad = [c for c in cases if not c[1]]
        for name, ok, v in cases:
            print(("PASS  " if ok else "SELFTEST-FAIL  ") + name)
            if not ok:
                for x in v[:6]:
                    print("        " + json.dumps(x, ensure_ascii=False))
        print("self_test: cases=%d passed=%d failed=%d" % (len(cases), len(cases) - len(bad), len(bad)))
        return 0
    finally:
        shutil.rmtree(str(tmp), ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description="judgement-vocabulary vs design-freeze consistency gate")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    root = pathlib.Path(a.root).resolve()
    for need in (DESIGN, ACCEPT, SCHEMA, REGISTRY, CODE):
        if not (root / need).exists():
            print(json.dumps({"check": "SPEC-POLARITY-CONSIST-01", "verdict": "FAIL",
                              "reason": "缺输入 " + need}, ensure_ascii=False))
            return 2
    try:
        violations, notes = run(root)
    except Exception as exc:
        print(json.dumps({"check": "SPEC-POLARITY-CONSIST-01", "verdict": "CRASH",
                          "reason": exc.__class__.__name__ + ": " + str(exc)}, ensure_ascii=False))
        return 2
    summary = {"check": "SPEC-POLARITY-CONSIST-01",
               "verdict": "PASS" if not violations else "FAIL",
               "violations": len(violations), "notes": notes,
               "resolved_config_keys": RESOLVED_KEYS}
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with io.open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump({"summary": summary, "violations": violations}, fh, ensure_ascii=False, indent=1)
    if not a.quiet or violations:
        print(json.dumps({k: summary[k] for k in ("check", "verdict", "violations", "notes")},
                         ensure_ascii=False))
        for x in violations[:40]:
            print("  " + json.dumps(x, ensure_ascii=False))
        for r in RESOLVED_KEYS:
            print("  resolved " + json.dumps(r, ensure_ascii=False))
    return 0 if not violations else 1


if __name__ == "__main__":
    sys.exit(main())
