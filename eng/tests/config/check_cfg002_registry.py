#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-002（W5-CFG-002）配置登记类机器门 —— 五项遗留的可执行闭合面。

单项职责（每项都能红能绿，负例入口 = --self-test）：
  CFG002-01  docs/plugins/** 配置表 <-> eng/packaging/config/config_registry.json 一一对应（缺登记/多登记/默认值漂移）
  CFG002-02  登记目标可解析（defaults.json 键 / phase_config 指针 / cpu_profile 指针 / 文档行号）与分类闭包
  CFG002-03  defaults.json 的 enum_target/enum_token 必须落进目标 phase_config 字段的 enum（默认值 -> 字段值域）
  CFG002-04  滤镜名匹配语义（exact + 无别名）与正反例（登记负例 + 现行库派生近失配 + ASTROCS_DESIGN §4.3 示例块）
  CFG002-05  cpu_profile.host.os_abi 值域 = 生产者字面量集合（fail-closed 枚举）
  CFG002-06  lib/**/module.yaml 键闭包（旋钮声明字段出现即判红）
  CFG002-07  索引归属唯一（eng/packaging/config/** vs eng/contracts/config/**；DOCUMENT_INDEX；eng/tests/test_index.csv）
  CFG002-08  docs/contracts/CONFIG_CONTRACT.md 的 CFG002-ANCHOR 标记行存活（文档承诺与登记一致）

用法：
  python3 eng/tests/config/check_cfg002_registry.py                # 跑真实仓库，rc=0 全绿
  python3 eng/tests/config/check_cfg002_registry.py --self-test     # 真实仓库全绿 + 每类故障注入必红
  python3 eng/tests/config/check_cfg002_registry.py --json-out X    # 机器输出
退出码：0 PASS；1 FAIL（含输入缺失的 fail-closed）；2 用法错误。
"""
import argparse
import json
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_DEFAULT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))

REGISTRY = "eng/packaging/config/config_registry.json"
DEFAULTS = "eng/packaging/config/defaults.json"
FILTERS = "eng/packaging/config/filters.json"
CPU_SCHEMA = "eng/contracts/schemas/cpu_profile.schema.json"
PHASE_SCHEMAS = {
    "normalize": "eng/contracts/schemas/phase_config_normalize.schema.json",
    "mosaic": "eng/contracts/schemas/phase_config_mosaic.schema.json",
    "export": "eng/contracts/schemas/phase_config_export.schema.json",
}
TEMPLATES = {
    "normalize": "eng/packaging/config/templates/normalize.phase_config.json",
    "mosaic": "eng/packaging/config/templates/mosaic.phase_config.json",
    "export": "eng/packaging/config/templates/export.phase_config.json",
}
CPU_NEG = "eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json"
CONTRACT_DOC = "docs/contracts/CONFIG_CONTRACT.md"
TEST_INDEX = "eng/tests/test_index.csv"
DOC_INDEX = "docs/DOCUMENT_INDEX.yaml"
HW_CPP = "lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp"
PROFILE_CPP = "lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp"
STAGE1_TPL = "lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json"
OWNER_CLASSES = {"science_param", "runtime_policy", "cli_surface", "resource_binding"}
REGISTRATIONS = {"defaults_json", "phase_config", "inputs_block", "cpu_profile",
                 "plugin_doc", "contracts_doc", "cli", "none"}
FINDINGS = {"none", "gap", "unregistered", "conflict"}
TICK = chr(96)


class Fail(Exception):
    """输入缺失/不可解析 -> fail-closed 判红（ENGINEERING_SPEC §8）。"""


def read_text(repo, rel):
    path = os.path.join(repo, rel)
    if not os.path.isfile(path):
        raise Fail("missing input file: %s" % rel)
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def load_json(repo, rel):
    try:
        return json.loads(read_text(repo, rel))
    except ValueError as exc:
        raise Fail("invalid JSON %s: %s" % (rel, exc))


def field_key(v):
    if v is None:
        return None
    v = v.replace(TICK, "")
    v = re.sub(r"\s+", " ", v).strip()
    return re.sub(r"\s*/\s*", " / ", v)


def cell_norm(v):
    if v is None:
        return None
    v = v.strip().strip(TICK)
    if v in ("——", "-", ""):
        return None
    return v


def parse_plugin_tables(repo):
    """docs/plugins/*/*.md 的配置项表 -> 行清单（field/doc/line/default/unit）。"""
    import glob
    rows = []
    for path in sorted(glob.glob(os.path.join(repo, "docs", "plugins", "*", "*.md"))):
        rel = os.path.relpath(path, repo).replace(os.sep, "/")
        with open(path, encoding="utf-8") as fh:
            lines = fh.read().split("\n")
        i = 0
        while i < len(lines):
            if re.match(r"^\|\s*字段\s*\|", lines[i]):
                hdr = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                j = i + 1
                if j < len(lines) and re.match(r"^\|[-\s|]+\|$", lines[j].strip()):
                    j += 1
                while j < len(lines) and lines[j].strip().startswith("|"):
                    cells = [c.strip() for c in lines[j].strip().strip("|").split("|")]
                    d = dict(zip(hdr, cells))
                    rows.append({"doc": rel, "line": j + 1, "module": os.path.basename(rel)[:-3],
                                 "field": field_key(d.get("字段")),
                                 "declared_default": cell_norm(d.get("默认")),
                                 "unit": cell_norm(d.get("单位"))})
                    j += 1
                i = j
            else:
                i += 1
    if not rows:
        raise Fail("no plugin config table rows found under docs/plugins/*/*.md")
    return rows


def json_pointer(node, pointer):
    """解析 '#'/'' 或 '#/a/b/0' 指针；失败返回 None。"""
    if pointer in ("", "#"):
        return node
    if not pointer.startswith("#"):
        return None
    cur = node
    for part in pointer[2:].split("/") if len(pointer) > 2 else []:
        part = part.replace("~1", "/").replace("~0", "~")
        if isinstance(cur, dict) and part in cur:
            cur = cur[part]
        elif isinstance(cur, list) and part.isdigit() and int(part) < len(cur):
            cur = cur[int(part)]
        else:
            return None
    return cur


def properties_of(node):
    return set(node.get("properties", {}).keys()) if isinstance(node, dict) else set()


def leaf_ok(node, registered_key):
    """registered_key 的末段是否为 node 的属性（node 可以是父对象）。"""
    if not isinstance(node, dict) or not isinstance(registered_key, str):
        return False
    leaf = registered_key.split(".")[-1].split("[")[0]
    return leaf in properties_of(node)


def check_01_registry_correspondence(repo):
    """docs 行 <-> 登记行 一一对应；默认值/单位/行号漂移即红。"""
    reg = load_json(repo, REGISTRY)
    rows = reg.get("plugin_knobs")
    if not isinstance(rows, list) or not rows:
        raise Fail("%s#plugin_knobs 缺少登记行" % REGISTRY)
    doc_rows = parse_plugin_tables(repo)
    doc_by_key = {}
    for r in doc_rows:
        k = (r["module"], r["field"])
        if k in doc_by_key:
            raise Fail("文档配置表出现重复行: %s" % (k,))
        doc_by_key[k] = r
    reg_by_key = {}
    for r in rows:
        for f in ("module", "doc", "line", "field", "declared_default", "unit",
                  "owner_class", "registration", "finding", "note"):
            if f not in r:
                raise Fail("登记行缺字段 %s: %r" % (f, r))
        if r["owner_class"] not in OWNER_CLASSES:
            raise Fail("未知 owner_class %r (%s/%s)" % (r["owner_class"], r["module"], r["field"]))
        if r["registration"] not in REGISTRATIONS:
            raise Fail("未知 registration %r (%s/%s)" % (r["registration"], r["module"], r["field"]))
        if r["finding"] not in FINDINGS:
            raise Fail("未知 finding %r (%s/%s)" % (r["finding"], r["module"], r["field"]))
        k = (r["module"], r["field"])
        if k in reg_by_key:
            raise Fail("登记册重复行: %s" % (k,))
        reg_by_key[k] = r
    missing = sorted(set(doc_by_key) - set(reg_by_key))
    if missing:
        raise Fail("缺登记 %d 行（文档有、登记册无）: %s" % (len(missing), missing[:8]))
    phantom = sorted(set(reg_by_key) - set(doc_by_key))
    if phantom:
        raise Fail("多登记 %d 行（登记册有、文档无）: %s" % (len(phantom), phantom[:8]))
    for k, d in sorted(doc_by_key.items()):
        r = reg_by_key[k]
        for f in ("doc", "line", "declared_default", "unit"):
            if r[f] != d[f]:
                raise Fail("%s/%s 的 %s 漂移: 文档=%r 登记=%r" % (k[0], k[1], f, d[f], r[f]))
    # finding 与登记点必须自洽（禁止用 none 掩盖未登记）
    for k, r in sorted(reg_by_key.items()):
        if r["registration"] == "none" and r["finding"] == "none":
            raise Fail("%s/%s 无登记点却标 finding=none" % k)
        if r["finding"] in ("gap", "unregistered", "conflict"):
            if not r["note"].strip():
                raise Fail("%s/%s finding=%s 但无 note" % (k[0], k[1], r["finding"]))
            c = r.get("conflict")
            if r["finding"] == "conflict":
                if not isinstance(c, dict) or not c.get("kind") or not c.get("evidence") or not c.get("owner"):
                    raise Fail("%s/%s finding=conflict 但 conflict 证据不完整" % k)
    # totals 必须与重算一致（禁止手写汇总糊弄）
    import collections
    t = reg.get("totals", {})
    if t.get("rows") != len(rows):
        raise Fail("totals.rows=%r 与实际 %d 不一致" % (t.get("rows"), len(rows)))
    for name, key in (("by_finding", "finding"), ("by_owner_class", "owner_class"),
                      ("by_registration", "registration")):
        got = dict(collections.Counter(r[key] for r in rows))
        if t.get(name) != got:
            raise Fail("totals.%s 与重算不一致: %r != %r" % (name, t.get(name), got))
    return "rows=%d documents=%d missing=0 phantom=0 drift=0" % (len(rows), len(doc_by_key))


# 单位归一别名表（登记册/文档用中文或符号写法，defaults 用 ASCII token）
UNIT_ALIASES = {"σ": "sigma", "sigma": "sigma", "1": "dimensionless",
                "度": "deg", "deg": "deg", "degree": "deg",
                "角秒": "arcsec", "arcsec": "arcsec", "arcsec2": "arcsec",
                "像素": "px", "px": "px", "pixel": "px",
                "秒": "s", "s": "s", "毫秒": "ms", "ms": "ms",
                "点/度²": "pt/deg2", "pt/deg2": "pt/deg2", "pt/deg^2": "pt/deg2"}
# 已知量纲间换算（登记点单位 -> 文档单位）；不在表内的跨单位比较判「不可比」而非放行
UNIT_SCALE = {("deg", "arcsec"): 3600.0, ("arcsec", "deg"): 1.0 / 3600.0,
              ("s", "ms"): 1000.0, ("ms", "s"): 1.0 / 1000.0}


def _unit_token(unit):
    """单位归一：去括注，别名折叠到规范 token；未声明返回 None。"""
    if unit is None:
        return None
    s = str(unit).strip()
    if s in ("", "——", "-", "None", "null"):
        return None
    s = re.sub(r"（[^）]*）|\([^)]*\)", "", s).strip()
    s = re.sub(r"\s+", "", s)
    return UNIT_ALIASES.get(s, s)


def _num(v):
    if isinstance(v, bool) or v is None:
        return None
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, str):
        try:
            return float(v.strip())
        except ValueError:
            return None
    return None


def _value_equal(declared, value, doc_unit, def_unit):
    """(equal|None, detail)：类型/单位/缩放域感知的相等判定。

    None 表示「不可比」（跨单位且无换算登记），由调用方计入未比对面而非放行。
    """
    du, vu = _unit_token(doc_unit), _unit_token(def_unit)
    scale = 1.0
    if du and vu and du != vu:
        scale = UNIT_SCALE.get((du, vu))
        if scale is None:
            return None, "单位不可比 doc=%r default=%r" % (doc_unit, def_unit)
    dn, vn = _num(declared), _num(value)
    if dn is not None and vn is not None:
        if abs(dn - vn * scale) <= 1e-9 * max(1.0, abs(dn)):
            return True, ""
        return False, "数值不等 文档=%r default=%r（单位 %r->%r ×%g）" % (
            declared, value, doc_unit, def_unit, scale)
    if str(declared).strip().strip('"').lower() == str(value).strip().strip('"').lower():
        return True, ""
    return False, "取值不等 文档=%r default=%r" % (declared, value)


def _resolve_registration(repo, r, schemas, defaults_keys):
    """返回（ok, detail）；登记点必须真实可解析。"""
    reg, at, key = r["registration"], r.get("registered_at"), r.get("registered_key")
    if reg == "defaults_json":
        if key not in defaults_keys:
            return False, "defaults.json 无键 %r" % key
        return True, "defaults.json#%s" % key
    if reg in ("phase_config", "inputs_block"):
        if not isinstance(at, str) or "#" not in at:
            return False, "%s 登记点缺 schema#pointer: %r" % (reg, at)
        path, pointer = at.split("#", 1)
        if path not in schemas:
            return False, "schema 路径不在 phase_config 三份之内: %s" % path
        schema = schemas[path]
        leaf = key.split(".")[-1] if isinstance(key, str) else None
        leaf = leaf.split("[")[0] if leaf else leaf
        if reg == "inputs_block":
            if not pointer.endswith("[]"):
                return False, "inputs_block 指针必须以 [] 结尾: %r" % at
            node = json_pointer(schema, "#" + pointer[:-2])
            for _ in range(3):  # 本地 $ref 展开（properties.blocks -> $defs.<name>）
                if isinstance(node, dict) and "$ref" in node and str(node["$ref"]).startswith("#/"):
                    node = json_pointer(schema, node["$ref"])
                else:
                    break
            if node is None:
                return False, "指针不可解析: %s#%s" % (path, pointer)
            if isinstance(node, dict) and isinstance(node.get("items"), dict):
                node = node["items"]  # 数组节点：属性面在 items 下
                for _ in range(3):    # items 亦可能是本地 $ref（blocks[] -> $defs.normalize_block）
                    if isinstance(node, dict) and "$ref" in node and str(node["$ref"]).startswith("#/"):
                        node = json_pointer(schema, node["$ref"])
                    else:
                        break
            if node is None:
                return False, "指针不可解析: %s#%s（items $ref 悬空）" % (path, pointer)
            if leaf and leaf not in properties_of(node):
                return False, "inputs 项无属性 %r: %s#%s" % (leaf, path, pointer)
            return True, "%s#%s" % (path, pointer)
        node = json_pointer(schema, "#" + pointer)
        if node is None:
            return False, "指针不可解析: %s#%s" % (path, pointer)
        if leaf and leaf not in properties_of(node) and pointer.rstrip("/").split("/")[-1] != leaf:
            return False, "指针节点无属性 %r: %s#%s" % (leaf, path, pointer)
        return True, "%s#%s" % (path, pointer)
    if reg == "cpu_profile":
        if not isinstance(at, str) or not at.startswith(CPU_SCHEMA + "#"):
            return False, "cpu_profile 登记点异常: %r" % at
        node = json_pointer(load_json(repo, CPU_SCHEMA), at[len(CPU_SCHEMA):])
        if node is None:
            return False, "cpu_profile 指针不可解析: %s" % at
        return True, at
    if reg in ("plugin_doc", "contracts_doc"):
        m = re.match(r"^(.+?):(\d+)(?:,\d+)*$", at or "")
        if not m:
            return False, "%s 登记点必须是 文件:行 形式: %r" % (reg, at)
        path, line = m.group(1), int(m.group(2))
        lines = read_text(repo, path).split("\n")
        if line < 1 or line > len(lines) or not lines[line - 1].strip():
            return False, "%s:%d 越界或空行" % (path, line)
        return True, at
    if reg == "cli":
        lines = read_text(repo, at.split(":")[0]).split("\n")
        return True, at
    return True, "none"


def _registration_value_problems(r, defaults_by_key):
    """登记点「值」判据（CFG002-02 值域面）。

    规则：
      1. registration=defaults_json 且登记点有值 ⇒ 必须与登记册 declared_default 相等
         （类型/单位/缩放域感知；跨单位不可比计入未比对面，不放行）；
         登记点有值而登记册未声明默认 ⇒ 判红（反向漂移）。
      2. registration=defaults_json 且登记点无值（null）⇒ 必须在**两处**显式挂起登记：
         defaults 条目的 authority_status=pending_authority + 非空 pending_task，
         且登记册行 note 显式写明 pending_authority。否则「finding=none + value=null」
         一律判红（禁止用 none 掩盖登记点没值）。
      3. phase_config/inputs_block 的 schema 叶子若声明 default ⇒ 必须与 declared_default 相等。
     """
    problems, checked, unchecked = [], 0, 0
    if r["registration"] in ("phase_config", "inputs_block"):
        # 覆盖诚实性：schema 面不承载运行值（实测 0/17 声明 default），
        # 这些行的值域不在本判据可判范围内，计数上报而非默认「已比对」。
        return problems, checked, unchecked + (1 if r["declared_default"] is not None else 0)
    if r["registration"] != "defaults_json":
        return problems, checked, unchecked
    entry = defaults_by_key.get(r["registered_key"])
    if entry is None:
        problems.append("%s/%s: defaults.json 无键 %r"
                        % (r["module"], r["field"], r["registered_key"]))
        return problems, checked, unchecked
    val = entry.get("value")
    pending = (entry.get("authority_status") == "pending_authority"
               and str(entry.get("pending_task") or "").strip())
    note = str(r.get("note") or "")
    if val is None:
        if not pending:
            problems.append("%s/%s: 登记点 %s 的 value=null 且缺 pending_authority/"
                            "pending_task 挂起登记（finding=%s）"
                            % (r["module"], r["field"], r["registered_key"], r["finding"]))
        elif "pending_authority" not in note:
            problems.append("%s/%s: 登记点 value=null 依赖 defaults 挂起，但登记册 note "
                            "未写明 pending_authority（finding=%s）"
                            % (r["module"], r["field"], r["finding"]))
        elif r["declared_default"] is not None:
            problems.append("%s/%s: 文档/登记册声明默认 %r 而登记点无值"
                            % (r["module"], r["field"], r["declared_default"]))
        return problems, checked, unchecked
    if r["declared_default"] is None:
        problems.append("%s/%s: 登记点有值 %r 但文档/登记册 declared_default 为 ——（反向漂移）"
                        % (r["module"], r["field"], val))
        return problems, checked, unchecked
    eq, why = _value_equal(r["declared_default"], val, r["unit"], entry.get("unit"))
    if eq is False:
        problems.append("%s/%s: 登记点值漂移 %s" % (r["module"], r["field"], why))
    elif eq is None:
        # 单位域不可比且无换算登记 ⇒ fail-closed 判红（不得以「不可比」放行）
        problems.append("%s/%s: 登记点单位域未登记换算 %s" % (r["module"], r["field"], why))
        unchecked += 1
    else:
        checked += 1
    return problems, checked, unchecked


def check_02_registration_targets(repo):
    """登记点可解析 + 登记点值与登记册 declared_default 一致 + 分类闭包
    （分离红线：runtime_policy/resource_binding 不得进科学配置）。"""
    reg = load_json(repo, REGISTRY)
    defaults_doc = load_json(repo, DEFAULTS)
    defaults_keys = {f["key"] for f in defaults_doc["fields"]}
    defaults_by_key = {f["key"]: f for f in defaults_doc["fields"]}
    schemas = {rel: load_json(repo, rel) for rel in PHASE_SCHEMAS.values()}
    phase_props = set()
    for s in schemas.values():
        # §9.68：normalize 的块定义名为 normalize_block（旧 normalize_config 已随
        # 逐帧形态退役）；mosaic/export 未变。
        for sub in (s.get("$defs", {}).get("normalize_block"),
                    s.get("$defs", {}).get("mosaic_config"),
                    s.get("$defs", {}).get("export_config")):
            phase_props |= properties_of(sub)
    problems = []
    value_checked = value_unchecked = 0
    for r in reg["plugin_knobs"]:
        ok, detail = _resolve_registration(repo, r, schemas, defaults_keys)
        if not ok:
            problems.append("%s/%s: %s" % (r["module"], r["field"], detail))
            continue
        vp, v_checked, v_unchecked = _registration_value_problems(r, defaults_by_key)
        problems.extend(vp)
        value_checked += v_checked
        value_unchecked += v_unchecked
        if r["owner_class"] in ("runtime_policy", "resource_binding") and \
                r["registration"] in ("defaults_json", "phase_config", "inputs_block"):
            problems.append("%s/%s: %s 不得登记进 %s（UNIFIED_MODEL §3 / CONFIG_CONTRACT §3）"
                            % (r["module"], r["field"], r["owner_class"], r["registration"]))
        if r["owner_class"] == "runtime_policy" and r["field"] in phase_props:
            problems.append("%s/%s: runtime_policy 旋钮出现在 phase_config 属性面" % (r["module"], r["field"]))
        if r["owner_class"] == "science_param" and r["registration"] == "phase_config" and \
                r["declared_default"] is not None:
            path, pointer = r["registered_at"].split("#", 1)
            node = json_pointer(schemas[path], "#" + pointer)
            if isinstance(node, dict) and "enum" not in node and leaf_ok(node, r["registered_key"]):
                node = node["properties"][r["registered_key"].split(".")[-1]]
            enum = node.get("enum") if isinstance(node, dict) else None
            if enum and r["declared_default"] not in [str(v) for v in enum]:
                problems.append("%s/%s: 默认 %r 不在目标 enum %r"
                                % (r["module"], r["field"], r["declared_default"], enum))
    if problems:
        raise Fail("登记点/分类问题 %d 条: %s" % (len(problems), problems[:6]))
    return ("resolved=%d value_checked=%d value_unchecked=%d separation_ok"
            % (len(reg["plugin_knobs"]), value_checked, value_unchecked))


def check_03_defaults_enum_mapping(repo):
    """defaults.json 里「默认值 -> 目标字段值域」的映射必须机器可解析且 token 合法。"""
    doc = load_json(repo, DEFAULTS)
    problems, mapped = [], 0
    for f in doc["fields"]:
        tgt, tok = f.get("enum_target"), f.get("enum_token")
        if (tgt is None) != (tok is None):
            problems.append("%s: enum_target 与 enum_token 必须同时出现" % f["key"])
            continue
        if tgt is None:
            continue
        mapped += 1
        if not isinstance(tgt, dict) or "schema" not in tgt or "pointer" not in tgt:
            problems.append("%s: enum_target 形态错误 %r" % (f["key"], tgt))
            continue
        try:
            schema = load_json(repo, tgt["schema"])
        except Fail as exc:
            problems.append("%s: %s" % (f["key"], exc))
            continue
        node = json_pointer(schema, tgt["pointer"])
        if not isinstance(node, dict) or "enum" not in node:
            problems.append("%s: 指针未落到含 enum 的节点 %s" % (f["key"], tgt["pointer"]))
            continue
        if tok not in [str(v) for v in node["enum"]]:
            problems.append("%s: enum_token=%r 不在 %s 的 enum %r" % (f["key"], tok, tgt["pointer"], node["enum"]))
    if not mapped:
        problems.append("没有任何 enum_target 登记（weight.default_mode 必须登记）")
    if problems:
        raise Fail("defaults 值域映射问题: %s" % problems[:6])
    return "mapped=%d" % mapped


def _validator(repo):
    import importlib.util
    path = os.path.join(repo, "eng", "tests", "common", "jsonschema_min.py")
    if not os.path.isfile(path):
        raise Fail("missing validator: eng/tests/common/jsonschema_min.py")
    spec = importlib.util.spec_from_file_location("cfg002_jsonschema_min", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _phase_filter_enum(schemas):
    enums = {}
    for phase, rel in PHASE_SCHEMAS.items():
        enums[phase] = schemas[rel]["$defs"]["filter_name"]["enum"]
    return enums


def check_04_filter_name_policy(repo):
    """滤镜名匹配语义：exact + 无别名；正反例必须能红能绿；库事实必须与登记一致。"""
    lib = load_json(repo, FILTERS)
    reg = load_json(repo, REGISTRY)
    look = lib["lookup"]
    problems = []
    if look.get("unknown_filter") != "error":
        problems.append("lookup.unknown_filter != error")
    if look.get("match") != "exact":
        problems.append("lookup.match != exact")
    if look.get("case_sensitive") is not True:
        problems.append("lookup.case_sensitive != true")
    if look.get("normalization") != "none":
        problems.append("lookup.normalization != none")
    if look.get("aliases") != {}:
        problems.append("lookup.aliases 非空（别名必须逐条登记，当前应为空）")
    keys = list(lib["filters"].keys())
    schemas = {rel: load_json(repo, rel) for rel in PHASE_SCHEMAS.values()}
    for phase, enum in _phase_filter_enum(schemas).items():
        if list(enum) != keys:
            problems.append("%s 的 filter enum 与库键不一致" % phase)
    facts = reg["filter_name_policy"]["library_facts"]
    if facts["keys"] != len(keys):
        problems.append("library_facts.keys=%r 实际=%d" % (facts["keys"], len(keys)))
    low = {}
    for k in keys:
        low.setdefault(k.lower(), []).append(k)
    coll = sum(1 for v in low.values() if len(v) > 1)
    if coll != facts["case_or_space_collisions"]:
        problems.append("库内大小写折叠重名数 %d != 登记 %r" % (coll, facts["case_or_space_collisions"]))
    channels = sorted({v["channel"] for v in lib["filters"].values()})
    if channels != sorted(facts["channel_domain"]):
        problems.append("channel 值域 %r != 登记 %r" % (channels, facts["channel_domain"]))
    empty = sorted(k for k, v in lib["filters"].items() if not v["channel"])
    if empty != sorted(facts["channel_empty_keys"]):
        problems.append("channel 空值键 %r != 登记 %r" % (empty, facts["channel_empty_keys"]))
    # 消费滤镜键的 phase 面必须与登记一致（dead $defs 不得静默复活/消失）
    consuming = []
    for phase, rel in PHASE_SCHEMAS.items():
        if '"#/$defs/filter_name"' in json.dumps(schemas[rel]):
            consuming.append(phase)
    if sorted(consuming) != sorted(reg["filter_name_policy"]["phases_consuming_filter"]):
        problems.append("消费滤镜键的 phase 面 %r != 登记 %r"
                        % (sorted(consuming), reg["filter_name_policy"]["phases_consuming_filter"]))
    dead = sorted(set(PHASE_SCHEMAS) - set(consuming))
    if dead != sorted(reg["filter_name_policy"]["dead_filter_enum_phases"]):
        problems.append("未消费 filter_name 的 phase %r != 登记 %r"
                        % (dead, reg["filter_name_policy"]["dead_filter_enum_phases"]))
    # non_key_examples：① 不得是库键；② 不得可被「大小写/空白折叠」解析回库键
    # （否则登记形同虚设：任何轻量归一化都会把它变成合法键）；③ where 必须是
    # 「文件:行」锚点，文件真实存在且该行真的含该串（fail-closed，锚点失效即判红）。
    # GATE-502：RELEASE-04 换版后最高设计 §3.3 的 "bader r" 反例行已删（原
    # ASTROCS_DESIGN.md:221 锚点失效）⇒ 登记已按现行合同重锚到
    # docs/contracts/CONFIG_CONTRACT.md §10 负例表；值与判据未变。
    reg_lits = {ex.get("literal") for ex in look.get("non_key_examples", [])}
    low_keys = {k.lower() for k in keys}
    fold_keys = {" ".join(k.split()).lower() for k in keys}
    for ex in look.get("non_key_examples", []):
        lit = ex.get("literal")
        if lit in keys:
            problems.append("non_key_examples 含库键 %r（示例不得是合法键）" % lit)
        if lit.lower() in low_keys or " ".join(lit.split()).lower() in fold_keys:
            problems.append("non_key_examples %r 可被大小写/空白折叠解析为库键（归一化必须保持关闭）" % lit)
        m = re.match(r"^(.+?):(\d+)$", ex.get("where", ""))
        if not m:
            problems.append("non_key_examples %r 的 where 不是「文件:行」锚点: %r"
                            % (lit, ex.get("where")))
            continue
        anchor_rel, anchor_ln = m.group(1), int(m.group(2))
        if not os.path.isfile(os.path.join(repo, anchor_rel)):
            problems.append("non_key_examples %r 的 where 文件不存在: %s" % (lit, anchor_rel))
            continue
        lines = read_text(repo, anchor_rel).split("\n")
        if anchor_ln > len(lines) or lit not in lines[anchor_ln - 1]:
            problems.append("non_key_examples %r 的 where 锚点不成立: %s" % (lit, ex.get("where")))
    # 现行库派生的近失配反例（不依赖任何历史文档示例；GATE-502 按任务书「改用现行
    # filters.json 中真实存在的失配反例」补）：真键的大小写/空白变体 + 一个「品牌在库、
    # 型号不在库」的缺号反例。它们必须既不等于库键，也不可被已登记归一化解析回库键。
    derived_negatives = []
    for k in ("Baader R", "Johnson V", "SDSS r"):
        derived_negatives += [k.lower(), k.upper(), k.replace(" ", "  "), " " + k]
    derived_negatives.append("Baader V")   # Baader 品牌在库（B/G/R/UV-IR/H-alpha/OIII），无 V 曲线
    # 注意口径差别：派生变体是**故意的近失配**（折叠后能落到真键上），因此只要求
    # 「字节精确下不是库键」；「折叠也不可解析」的要求只对 non_key_examples 生效
    # （它们是权威文本里明确要拒的拼写，例如 bader ≠ Baader 折叠加空白折叠都救不回）。
    for v in derived_negatives:
        if v in keys:
            problems.append("派生反例 %r 竟是库键（库事实变化，必须换反例）" % v)
    # 正反例（红绿双向）：必须覆盖**每一个**消费滤镜键的 phase —— normalize 的键位是
    # blocks[].filter_passband、mosaic 是 inputs[].filter。旧实现只认 inputs[] 形态，
    # normalize 被静默 continue 掉（恒真面）；现按形状分派 + 覆盖自证（不足即判红）。
    validator = _validator(repo)
    exercised = []
    for phase, rel in PHASE_SCHEMAS.items():
        schema = schemas[rel]
        tpl = load_json(repo, TEMPLATES[phase])
        probe = json.loads(json.dumps(tpl))
        field = None
        target = None
        if isinstance(probe.get("blocks"), list) and probe["blocks"]:
            target, field = probe["blocks"][0], "filter_passband"
            if field not in target:
                target, field = None, None
        if target is None:
            target = next((it for it in probe.get("inputs", []) if "filter" in it), None)
            field = "filter" if target is not None else None
        if target is None and phase == "mosaic":
            # mosaic 模板是块形态，而块面**不含** filter；承载 filter enum 的是 schema 的
            # 旧合同分支 inputs[]（合同留痕，§9.71 裁决 2 定案 4）。反例必须打到真正带
            # enum 的那个字段上，否则「未被拒绝」是假绿（字段根本不在被校验的文档里）。
            # 形状漂移不会静默：正例校验会先失败（正例被拒），且 exercised 覆盖自证兜底。
            probe = {"phase_name": "mosaic",
                     "config": {"output_dir": "path/to/out/mosaic", "precision": "fp64"},
                     "inputs": [{"product": "path/to/p1_hips_a", "filter": "Baader R"}]}
            target, field = probe["inputs"][0], "filter"
        if target is None:
            continue
        exercised.append(phase)
        for lit in reg["filter_name_policy"]["declared_negatives"]:
            target[field] = lit["literal"]
            if not validator.validate(probe, schema):
                problems.append("%s: 反例 %r 未被拒绝（匹配语义漏判）" % (phase, lit["literal"]))
        for v in derived_negatives:
            target[field] = v
            if not validator.validate(probe, schema):
                problems.append("%s: 现行库派生近失配反例 %r 未被拒绝" % (phase, v))
        for lit in reg["filter_name_policy"]["declared_positives"]:
            target[field] = lit
            errs = validator.validate(probe, schema)
            if errs:
                problems.append("%s: 正例 %r 被拒: %s" % (phase, lit, errs[:2]))
    want = sorted(reg["filter_name_policy"]["phases_consuming_filter"])
    if sorted(exercised) != want:
        problems.append("红绿双向只覆盖 %r，登记消费滤镜的 phase=%r（不得静默跳过）"
                        % (sorted(exercised), want))
    # ASTROCS_DESIGN §4.3 输入合同示例块（现行权威；旧 §3.3 锚点在 RELEASE-04 换版后
    # 已不含滤镜示例）：引号串必须是库键或已登记示例（近失配变体不得静默通过），
    # 且示例块必须至少含一个合法库键 —— 否则判据空转（假绿）。
    design = read_text(repo, "ASTROCS_DESIGN.md").split("\n")
    start = end = None
    for i, ln in enumerate(design):
        if ln.startswith("### 4.3"):
            start = i
        elif start is not None and ln.startswith("### ") and i > start:
            end = i
            break
    if start is None:
        problems.append("ASTROCS_DESIGN.md 缺 §4.3 段（示例锚点失效）")
    else:
        block = design[start:end or len(design)]
        positives = 0
        for ln in block:
            for s in re.findall(r'"([^"]+)"', ln):
                if s in keys:
                    positives += 1
                    continue
                if s in reg_lits:
                    continue
                if s.lower() in low_keys:
                    problems.append("§4.3 出现未登记的库键变体 %r（近失配必须显式登记）" % s)
        if positives == 0:
            problems.append("§4.3 示例块不含任何合法滤镜键 —— 判据空转（示例锚点失效）")
    if problems:
        raise Fail("滤镜名语义问题: %s" % problems[:6])
    return "keys=%d negatives=%d positives=%d derived_neg=%d phases=%s" % (
        len(keys), len(reg["filter_name_policy"]["declared_negatives"]),
        len(reg["filter_name_policy"]["declared_positives"]), len(derived_negatives),
        ",".join(sorted(exercised)))


def check_10_cpu_profile_kernel_link(repo):
    """cpu_profile 的 $defs.kernel_v1/kernel_v2 接线状态必须与登记一致（防静默接线/静默退化）。"""
    reg = load_json(repo, REGISTRY).get("cpu_profile_kernel_link")
    if not isinstance(reg, dict) or "refs" not in reg or "status" not in reg:
        raise Fail("config_registry.cpu_profile_kernel_link 缺失或形态错误")
    doc = read_text(repo, CPU_SCHEMA)
    problems = []
    for name, want in sorted(reg["refs"].items()):
        got = doc.count('"#/$defs/%s"' % name)
        if got != want:
            problems.append("$defs.%s 被 $ref 次数 %d != 登记 %d（接线状态变化必须同步登记与 handover）"
                            % (name, got, want))
    kernels = json_pointer(load_json(repo, CPU_SCHEMA), "#/$defs/profile_v2/properties/kernels")
    linked = isinstance(kernels, dict) and ("additionalProperties" in kernels or "$ref" in kernels)
    if linked == (reg["status"] == "UNLINKED"):
        problems.append("kernels 接线状态 %r 与登记 status=%r 不一致" % (linked, reg["status"]))
    if problems:
        raise Fail("cpu_profile kernel 接线登记问题: %s" % problems)
    return "kernel_v1_refs=%d kernel_v2_refs=%d status=%s" % (
        reg["refs"]["kernel_v1"], reg["refs"]["kernel_v2"], reg["status"])


def _anchor_hint_ok(txt, hint):
    """hint（source 里 path:line 后的括号文本）必须在该行留下可核token；CJK 允许前缀匹配。"""
    toks = [t for t in re.split(r"[^\w\u4e00-\u9fff./+\-]+", hint)
            if len(t) >= 2 and not re.fullmatch(r"§\d+", t)]
    for t in toks:
        if t in txt:
            return True
        if len(t) >= 3 and t[:2] in txt:
            return True
        if re.search(r"[\u4e00-\u9fff]", t) and t[:2] in txt:
            return True
    return False


def check_09_defaults_anchor_survival(repo):
    """defaults.json 的每个 source_ref 必须存活（行存在且非空）+ hint token 可核（ENGG_SPEC §8 锚存活）。

    例外的 paraphrase 清单必须显式登记且只减不增：新增 paraphrase 即判红。
    """
    doc = load_json(repo, DEFAULTS)
    reg = load_json(repo, REGISTRY)
    exceptions = set(reg.get("defaults_anchor_exceptions", {}).get("paraphrase", []))
    problems, checked, paraphrase = [], 0, []
    for f in doc["fields"]:
        ref = f.get("source_ref")
        if not ref:
            continue
        checked += 1
        path, line = ref.get("path"), ref.get("line")
        if not isinstance(path, str) or not isinstance(line, int):
            problems.append("%s: source_ref 形态错误 %r" % (f["key"], ref))
            continue
        try:
            lines = read_text(repo, path).split("\n")
        except Fail as exc:
            problems.append("%s: %s" % (f["key"], exc))
            continue
        if not (1 <= line <= len(lines)) or not lines[line - 1].strip():
            problems.append("%s: source_ref %s:%d 越界或空行" % (f["key"], path, line))
            continue
        txt = lines[line - 1]
        if f.get("authority_status") == "pending_authority":
            leaf = f["key"].split(".")[-1]
            if leaf not in txt:
                problems.append("%s: pending 字段的 source_ref 行不含字段名 %r" % (f["key"], leaf))
            continue
        m = re.search(r":\d+（([^）]*)）", f.get("source") or "")
        hint = m.group(1) if m else ""
        if not hint:
            problems.append("%s: sourced 字段的 source 缺 path:line（hint）" % f["key"])
            continue
        if not _anchor_hint_ok(txt, hint):
            paraphrase.append(f["key"])
            if f["key"] not in exceptions:
                problems.append("%s: source_ref %s:%d 的 hint %r 无 token 落在该行（锚漂移或需登记 paraphrase）"
                                % (f["key"], path, line, hint[:40]))
    if problems:
        raise Fail("defaults 锚存活问题 %d 条: %s" % (len(problems), problems[:6]))
    stale = sorted(exceptions - set(paraphrase))
    return "anchors=%d paraphrase=%d registered_exceptions=%d stale_exceptions=%s" % (
        checked, len(paraphrase), len(exceptions), stale)


SKIP_DIRS = (".git", "build", "run", "node_modules", "__pycache__", ".venv")


def _find_citation(repo, path):
    """引用路径解析：带目录前缀者要求原样存在；裸文件名（文档内简称）按全仓 basename 唯一解析。"""
    if os.path.isfile(os.path.join(repo, path)):
        return [path]
    if "/" in path:
        return []
    hits = []
    for root, dirs, files in os.walk(repo):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        if path in files:
            hits.append(os.path.relpath(os.path.join(root, path), repo))
    return sorted(hits)


def check_11_contract_doc_citations(repo):
    """CONFIG_CONTRACT 的 文件:行 引用结构存活 + 登记 token 锚不被文档重排冲掉。"""
    text = read_text(repo, CONTRACT_DOC)
    problems = []
    cites = set()
    for m in re.finditer(r"([A-Za-z0-9_./\u4e00-\u9fff\-]+\.(?:md|json|jsonc|yaml|csv|py|cpp|h)):(\d+)(?:-(\d+))?", text):
        path, a, b = m.group(1), int(m.group(2)), m.group(3)
        cites.add((path, a, int(b) if b else None))
    if len(cites) < 10:
        problems.append("引用数量异常（%d < 10），正则或文档结构可能失效" % len(cites))
    for path, a, b in sorted(cites):
        found = _find_citation(repo, path)
        if not found:
            problems.append("%s:%d 引用无法解析（无此文件）" % (path, a))
            continue
        try:
            lines = read_text(repo, found[0]).split("\n")
        except Fail as exc:
            problems.append("%s: %s" % (path, exc))
            continue
        if not (1 <= a <= len(lines)):
            problems.append("%s:%d 越界（文件 %d 行）" % (path, a, len(lines)))
            continue
        if not lines[a - 1].strip():
            problems.append("%s:%d 指向空行" % (path, a))
        if b is not None and (b < a or b > len(lines)):
            problems.append("%s:%d-%d 范围非法（文件 %d 行）" % (path, a, b, len(lines)))
    reg = load_json(repo, REGISTRY).get("contract_doc_citations")
    if not isinstance(reg, dict) or not reg.get("token_anchors"):
        problems.append("config_registry.contract_doc_citations.token_anchors 缺失")
    else:
        for item in reg["token_anchors"]:
            ref, token = item["ref"], item["token"]
            if ref not in text:
                problems.append("正文已不含引用 %s（引用被删/改写需同步登记）" % ref)
            path, n = ref.rsplit(":", 1)
            n = n.split("-")[0]
            lines = read_text(repo, path).split("\n")
            ln = int(item.get("token_line", n))
            found = _find_citation(repo, path)
            if found:
                lines = read_text(repo, found[0]).split("\n")
            if not found or ln > len(lines) or token not in lines[ln - 1]:
                problems.append("%s 被引行不含 token %r（锚漂移）" % (ref, token))
    if problems:
        raise Fail("CONFIG_CONTRACT 引用问题 %d 条: %s" % (len(problems), problems[:6]))
    return "citations=%d token_anchors=%d" % (len(cites), len(reg["token_anchors"]))


def _os_name_literals(repo):
    lines = read_text(repo, HW_CPP).split("\n")
    start = None
    for i, ln in enumerate(lines):
        if 'nlohmann::json os = {{"name",' in ln:
            start = i
            break
    if start is None:
        raise Fail("%s 未找到 os.name 生产者锚点" % HW_CPP)
    lits = []
    for ln in lines[start:start + 12]:
        lits += re.findall(r'"([^"]*)"', ln)
        if "}};" in ln:
            break
    return [x for x in lits if x != "name"]


def check_05_os_abi_domain(repo):
    """host.os_abi 值域 = 生产者字面量集合；schema 枚举 + 负例双向。"""
    reg = load_json(repo, REGISTRY)
    schema = load_json(repo, CPU_SCHEMA)
    node = json_pointer(schema, "#/$defs/profile_v2/properties/host/properties/os_abi")
    if not isinstance(node, dict) or "enum" not in node:
        raise Fail("cpu_profile v2 host.os_abi 未冻结为 enum（仍是自由字符串）")
    enum = [str(v) for v in node["enum"]]
    prod = _os_name_literals(repo)
    problems = []
    if sorted(set(prod)) != sorted(set(enum)):
        problems.append("schema enum %r != 生产者字面量 %r" % (enum, sorted(set(prod))))
    if sorted(set(reg["os_abi"]["enum"])) != sorted(set(enum)):
        problems.append("登记册 enum %r != schema enum %r" % (reg["os_abi"]["enum"], enum))
    plines = read_text(repo, PROFILE_CPP).split("\n")
    fallback = None
    for i, ln in enumerate(plines):
        if '"os_abi"' in ln:
            found = re.findall(r'"([^"]+)"', ln)
            fallback = found[-1] if found else None
            break
    if fallback is None:
        problems.append("%s 未找到 os_abi 回落字面量" % PROFILE_CPP)
    elif fallback not in enum:
        problems.append("profile_gen_v2 回落字面量 %r 不在 enum %r" % (fallback, enum))
    pend = schema.get("x-astrocs-pending-authority", {})
    if "host.os_abi" in pend:
        problems.append("x-astrocs-pending-authority 仍登记 host.os_abi（未撤销 pending）")
    if "host.os_abi" not in schema.get("x-astrocs-frozen-domains", {}):
        problems.append("x-astrocs-frozen-domains 未登记 host.os_abi 冻结事实")
    validator = _validator(repo)
    neg = load_json(repo, CPU_NEG)
    errs = validator.validate(neg, schema)
    if not errs:
        problems.append("负例 %s 未被拒（os_abi=%r）" % (CPU_NEG, neg["host"]["os_abi"]))
    pos = load_json(repo, "eng/tests/config/fixtures/positive/cpu_profile_v2.json")
    if validator.validate(pos, schema):
        problems.append("正例 cpu_profile_v2.json 未通过")
    if problems:
        raise Fail("os_abi 值域问题: %s" % problems)
    return "enum=%r producer=%r fallback=%r neg_fixture=REJECTED" % (enum, sorted(set(prod)), fallback)


def check_06_module_manifest_keys(repo):
    """lib/**/module.yaml 键闭包：出现旋钮声明键即红（新增旋钮必须先登记归属）。"""
    import glob
    reg = load_json(repo, REGISTRY)["module_manifest"]
    allowed = set(reg["registered_keys"])
    banned = set(reg["knob_declaring_keys"])
    paths = sorted(glob.glob(os.path.join(repo, "lib", "*", "module.yaml")) +
                   glob.glob(os.path.join(repo, "lib", "*", "*", "module.yaml")))
    if len(paths) < 15:
        raise Fail("module.yaml 数量异常（%d < 15），路径锚可能失效" % len(paths))
    problems = []
    for path in paths:
        rel = os.path.relpath(path, repo).replace(os.sep, "/")
        with open(path, encoding="utf-8") as fh:
            keys = re.findall(r"(?m)^([a-z_]+):", fh.read())
        bad = sorted(set(keys) & banned)
        if bad:
            problems.append("%s 出现未登记的旋钮声明键 %s" % (rel, bad))
        unknown = sorted(set(keys) - allowed)
        if unknown:
            problems.append("%s 出现未登记键 %s" % (rel, unknown))
    if problems:
        raise Fail("module.yaml 键闭包问题: %s" % problems[:6])
    return "manifests=%d keys_closed banned=%d" % (len(paths), len(banned))


def _tests_dirs_with_sources(repo):
    base = os.path.join(repo, "eng", "tests")
    if not os.path.isdir(base):
        raise Fail("eng/tests/ 不存在")
    out = []
    for name in sorted(os.listdir(base)):
        d = os.path.join(base, name)
        if not os.path.isdir(d) or name == "__pycache__":
            continue
        hit = False
        for root, dirs, files in os.walk(d):
            dirs[:] = [x for x in dirs if x != "__pycache__"]
            if any(f.endswith((".py", ".cpp", ".c", ".hpp")) or f == "CMakeLists.txt" for f in files):
                hit = True
                break
        if hit:
            out.append("eng/tests/" + name)
    if not out:
        raise Fail("eng/tests/ 下未发现任何测试目录（锚失效）")
    return out


def check_07_index_ownership(repo):
    """eng/packaging/config/** vs eng/contracts/config/** 归属唯一 + 三处索引登记归属。"""
    import csv
    problems = []
    cfg_files = []
    for root, dirs, files in os.walk(os.path.join(repo, "eng", "packaging", "config")):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for f in files:
            rel = os.path.relpath(os.path.join(root, f), repo).replace(os.sep, "/")
            cfg_files.append(rel)
            if f.endswith(".schema.json"):
                problems.append("eng/packaging/config/** 出现 schema 文件（第二事实源）: %s" % rel)
    cc_dir = os.path.join(repo, "eng", "contracts", "config")
    if not os.path.isdir(cc_dir):
        raise Fail("eng/contracts/config/ 不存在（锚失效）")
    cc_files = sorted(os.listdir(cc_dir))
    for f in cc_files:
        if not f.endswith(".schema.json"):
            problems.append("eng/contracts/config/** 出现非 schema 文件: %s" % f)
    inter = sorted({os.path.basename(p) for p in cfg_files} & set(cc_files))
    if inter:
        problems.append("eng/packaging/config/** 与 eng/contracts/config/** 同名文件（归属不唯一）: %s" % inter)
    # DOCUMENT_INDEX：CONFIG_CONTRACT 恰一次且 ACTIVE_NORMATIVE
    doc_lines = read_text(repo, DOC_INDEX).split("\n")
    # DOC-403（2026-09-21）：索引 v2 条目含 duty/upstream/downstream 字段，"下游被引用处"
    # 可以合法出现同一路径；归属唯一判据只针对**登记条目行**（"- path:"），既保留
    # "恰一次登记 + ACTIVE_NORMATIVE" 的判据强度，又不把下游引用误判为重复登记。
    # 判据强度由负例 index_entry_duplicated（重复登记行）守住。
    hits = [i for i, ln in enumerate(doc_lines)
            if ln.strip().startswith("- path:") and CONTRACT_DOC in ln]
    if len(hits) != 1:
        problems.append("%s 中 %s 出现 %d 次（应恰 1 次）" % (DOC_INDEX, CONTRACT_DOC, len(hits)))
    else:
        status = next((ln.strip() for ln in doc_lines[hits[0]:hits[0] + 3] if ln.strip().startswith("status:")), "")
        if "ACTIVE_NORMATIVE" not in status:
            problems.append("%s 中 %s 的状态非 ACTIVE_NORMATIVE: %r" % (DOC_INDEX, CONTRACT_DOC, status))
    # eng/tests/test_index.csv：登记 eng/tests/config + 已知未登记清单闭合
    rows = list(csv.DictReader(read_text(repo, TEST_INDEX).splitlines()))
    paths = [r["path"].strip() for r in rows if r.get("path")]
    if len(paths) != len(set(paths)):
        problems.append("%s 存在重复登记行" % TEST_INDEX)
    if "eng/tests/config" not in paths:
        problems.append("%s 未登记 eng/tests/config" % TEST_INDEX)
    known = set(load_json(repo, REGISTRY)["index_ownership"].get("test_index_known_unregistered", []))
    dirs = set(_tests_dirs_with_sources(repo))
    reg_ok = {d for d in dirs if d in paths or any(p.startswith(d + "/") for p in paths)}
    missing = sorted(dirs - reg_ok)
    if not set(missing) <= known:
        problems.append("eng/tests/** 未登记目录 %r 不在已登记缺口清单 %r 内（新增目录必须先登记或登记为缺口）"
                        % (sorted(set(missing) - known), sorted(known)))
    stale = sorted(known & reg_ok)  # 清单只减不增；已登记项残留只提示，不判红（避免与并线登记互相抖动）
    if problems:
        raise Fail("索引归属问题: %s" % problems[:6])
    return "config_files=%d contracts_config=%d test_index=%d gaps=%d stale_gaps=%s" % (
        len(cfg_files), len(cc_files), len(paths), len(known), stale)


ANCHORS = {
    "item1-plugin-defaults": REGISTRY,
    "item2-knob-ownership": REGISTRY,
    "item3-filter-name-policy": REGISTRY,
    "item4-os-abi-enum": REGISTRY,
    "item5-index-ownership": REGISTRY,
}


def check_08_contract_doc_anchors(repo):
    """CONFIG_CONTRACT 的 CFG002-ANCHOR 标记行必须存活且指向登记册。"""
    text = read_text(repo, CONTRACT_DOC)
    lines = text.split("\n")
    problems = []
    for name, target in sorted(ANCHORS.items()):
        # 约定：锚标记可写在条目行尾，但整篇必须恰出现一次，且同行必须指向登记册
        matched = [ln for ln in lines if "CFG002-ANCHOR:" in ln and name in ln]
        if len(matched) != 1:
            problems.append("锚 %s 出现 %d 次（应恰 1 次）" % (name, len(matched)))
            continue
        if target not in matched[0]:
            problems.append("锚 %s 未指向 %s" % (name, target))
    if problems:
        raise Fail("合同文档锚问题: %s" % problems)
    return "anchors=%d" % len(ANCHORS)


CHECKS = [
    ("CFG002-01", check_01_registry_correspondence),
    ("CFG002-02", check_02_registration_targets),
    ("CFG002-03", check_03_defaults_enum_mapping),
    ("CFG002-04", check_04_filter_name_policy),
    ("CFG002-05", check_05_os_abi_domain),
    ("CFG002-06", check_06_module_manifest_keys),
    ("CFG002-07", check_07_index_ownership),
    ("CFG002-08", check_08_contract_doc_anchors),
    ("CFG002-09", check_09_defaults_anchor_survival),
    ("CFG002-10", check_10_cpu_profile_kernel_link),
    ("CFG002-11", check_11_contract_doc_citations),
]


def run_checks(repo):
    out = []
    for cid, fn in CHECKS:
        try:
            out.append({"id": cid, "ok": True, "detail": fn(repo)})
        except Fail as exc:
            out.append({"id": cid, "ok": False, "detail": str(exc)})
        except Exception as exc:  # noqa: BLE001 —— fail-closed：任何异常都判红，不静默通过
            out.append({"id": cid, "ok": False, "detail": "%s: %s" % (type(exc).__name__, exc)})
    return out


SANDBOX_FILES = [
    "docs/DOCUMENT_INDEX.yaml", "docs/contracts/CONFIG_CONTRACT.md",
    "ASTROCS_DESIGN.md", "eng/tests/test_index.csv", HW_CPP, PROFILE_CPP, STAGE1_TPL,
    "ENGINEERING_SPEC.md", "docs/development/CONFIG_SCHEMA.md",
    "eng/tests/backend/test_cpu_profile.py", "eng/tests/unit/cpu007_profile_store_test.cpp",
    "eng/tools/validate_cpu_profile.py",
]
SANDBOX_DIRS = ["config", "eng/contracts/schemas", "eng/contracts/config", "docs/plugins",
                "eng/tests/config/fixtures"]
# 顶层科学/算法文档（defaults 的 source_ref 与 contracts_doc 登记点指向它们；v6 子树门不读，不入沙箱）
SANDBOX_GLOBS = ["docs/science/*.md", "docs/algorithms/*.md", "docs/design/UNIFIED_MODEL.md",
                 "lib/infrastructure/benchmark/backend_host/*.cpp", "ENGINEERING_SPEC.md"]


def build_sandbox(src, dst):
    """把门依赖的最小输入复制到 dst（自检用；不动真实仓库）。"""
    import glob
    os.makedirs(dst, exist_ok=True)
    for rel in SANDBOX_FILES:
        s = os.path.join(src, rel)
        if not os.path.isfile(s):
            raise Fail("sandbox source missing: %s" % rel)
        d = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy2(s, d)
    for rel in SANDBOX_DIRS:
        s = os.path.join(src, rel)
        if not os.path.isdir(s):
            raise Fail("sandbox source missing dir: %s" % rel)
        shutil.copytree(s, os.path.join(dst, rel), dirs_exist_ok=True)
    for pat in ("lib/*/module.yaml", "lib/*/*/module.yaml") + tuple(SANDBOX_GLOBS):
        hits = glob.glob(os.path.join(src, pat))
        if not hits:
            raise Fail("sandbox source pattern matched nothing: %s" % pat)
        for s in hits:
            rel = os.path.relpath(s, src)
            d = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copy2(s, d)
    v = os.path.join(src, "tests", "common", "jsonschema_min.py")
    if not os.path.isfile(v):
        raise Fail("sandbox source missing: eng/tests/common/jsonschema_min.py")
    d = os.path.join(dst, "tests", "common", "jsonschema_min.py")
    os.makedirs(os.path.dirname(d), exist_ok=True)
    shutil.copy2(v, d)
    return dst


def _edit_json(root, rel, mutate):
    path = os.path.join(root, rel)
    with open(path, encoding="utf-8") as fh:
        doc = json.load(fh)
    mutate(doc)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2)


def _edit_text(root, rel, mutate):
    path = os.path.join(root, rel)
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(mutate(text))


def _write_text(root, rel, text):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _row(doc, module, field):
    for r in doc["plugin_knobs"]:
        if r["module"] == module and r["field"] == field:
            return r
    raise Fail("registry row not found: %s/%s" % (module, field))


INJECTIONS = [
    ("plugin_table_row_added", "CFG002-01",
     lambda root: _edit_text(root, "docs/plugins/algorithms_phase1/03_star_detection.md",
                             lambda t: t.replace(
                                 "| " + chr(96) + "selection_function" + chr(96) + " | true | —— | 是否输出 selection function |",
                                 "| " + chr(96) + "selection_function" + chr(96) + " | true | —— | 是否输出 selection function |\n"
                                 "| " + chr(96) + "injected_knob" + chr(96) + " | 1 | px | 注入负例（未登记） |"))),
    ("registry_default_drift", "CFG002-01",
     lambda root: _edit_json(root, REGISTRY,
                             lambda d: _row(d, "03_star_detection", "min_area").__setitem__(
                                 "declared_default", "DRIFT"))),
    ("registry_phantom_row", "CFG002-01",
     lambda root: _edit_json(root, REGISTRY, lambda d: d["plugin_knobs"].append(
         {"module": "99_fake", "doc": "docs/plugins/algorithms_phase1/99_fake.md", "line": 1,
          "field": "ghost", "declared_default": None, "unit": None, "owner_class": "science_param",
          "registration": "none", "registered_at": None, "registered_key": None,
          "finding": "unregistered", "note": "注入负例", "conflict": None}))),
    ("registration_target_missing", "CFG002-02",
     lambda root: _edit_json(root, REGISTRY,
                             lambda d: _row(d, "11_upm", "gauge").__setitem__(
                                 "registered_at", PHASE_SCHEMAS["mosaic"] + "#/$defs/no_such_node"))),
    ("runtime_knob_into_science_config", "CFG002-02",
     lambda root: _edit_json(root, REGISTRY,
                             lambda d: _row(d, "16_fits_output", "band_height").update(
                                 {"registration": "phase_config",
                                  "registered_at": PHASE_SCHEMAS["export"] + "#/$defs/export_config",
                                  "registered_key": "export.config.band_height"}))),
    # §9.73 裁决 A44（2026-09-20）：weight.default_mode 组已注销（config 面收口），
    # 注入靶点改为现存唯一 enum_target 登记（snr.path → phase_config_mosaic#snr_path）。
    ("defaults_enum_token_illegal", "CFG002-03",
     lambda root: _edit_json(root, DEFAULTS, lambda d: [
         f.__setitem__("enum_token", "not_a_token") for f in d["fields"]
         if f["key"] == "snr.path"])),
    ("filter_match_folded", "CFG002-04",
     lambda root: _edit_json(root, FILTERS,
                             lambda d: d["lookup"].__setitem__("match", "casefold"))),
    ("filter_alias_smuggled", "CFG002-04",
     lambda root: _edit_json(root, FILTERS, lambda d: d["lookup"].__setitem__(
         "aliases", {"bader r": "Baader R"}))),
    ("os_abi_extra_value", "CFG002-05",
     lambda root: _edit_json(root, CPU_SCHEMA, lambda d: d["$defs"]["profile_v2"]["properties"]
                             ["host"]["properties"]["os_abi"]["enum"].append("darwin"))),
    ("os_abi_still_pending", "CFG002-05",
     lambda root: _edit_json(root, CPU_SCHEMA, lambda d: d["x-astrocs-pending-authority"].__setitem__(
         "host.os_abi", "注入负例：pending 未撤销"))),
    ("module_yaml_knobs_key", "CFG002-06",
     lambda root: _edit_text(root, "lib/algorithms/psf/module.yaml",
                             lambda t: t + "\nknobs:\n  - injected\n")),
    ("test_index_row_removed", "CFG002-07",
     lambda root: _edit_text(root, TEST_INDEX,
                             lambda t: "".join(ln for ln in t.splitlines(True)
                                               if not ln.startswith("eng/tests/config,")))),
    ("config_schema_second_source", "CFG002-07",
     lambda root: _write_text(root, "eng/packaging/config/injected.schema.json", "{}\n")),
    ("contracts_config_same_basename", "CFG002-07",
     lambda root: shutil.copy2(os.path.join(root, DEFAULTS),
                               os.path.join(root, "contracts", "config", "defaults.json"))),
    # DOC-403：登记条目行重复（下游引用不算重复登记；本注入保证判据仍有牙）
    ("index_entry_duplicated", "CFG002-07",
     lambda root: _edit_text(root, DOC_INDEX,
                             lambda t: t.replace('    - path: "' + CONTRACT_DOC + '"',
                                                 '    - path: "' + CONTRACT_DOC + '"' + chr(10)
                                                 + '    - path: "' + CONTRACT_DOC + '"', 1))),
    ("contract_doc_citation_drift", "CFG002-11",
     lambda root: _edit_text(root, CONTRACT_DOC, lambda t: t.replace(
         "docs/science/PHASE3_HIPS_TO_FITS.md:39", "docs/science/PHASE3_HIPS_TO_FITS.md:31"))),
    ("defaults_anchor_drift", "CFG002-09",
     lambda root: _edit_json(root, DEFAULTS, lambda d: [
         f.__setitem__("source_ref", {"path": "docs/science/STAR_DETECTION.md", "line": 1})
         for f in d["fields"] if f["key"] == "detection.threshold_sigma"])),
    # W5-CPU-001 接线后本注入改为「接线状态变化未登记」的退化方向：拆掉 kernel_v1 的
    # $ref 而不改登记册（原变异对已接线状态是无操作 → 负例面失效，故随接线同步更新）。
    ("kernel_link_degraded_without_registration", "CFG002-10",
     lambda root: _edit_json(root, CPU_SCHEMA, lambda d: d["$defs"]["legacy_v1"]["properties"]
                             ["kernels"].pop("items"))),
    # CFG002-02 值域面（DOC-SCI-001 A4）：登记点值必须与登记册 declared_default 一致
    ("registry_value_mismatch", "CFG002-02",
     lambda root: _edit_json(root, DEFAULTS, lambda d: [
         f.__setitem__("value", 0.5) for f in d["fields"]
         if f["key"] == "drizzle.pixfrac"])),
    ("registry_null_value_finding_none", "CFG002-02",
     lambda root: (_edit_json(root, DEFAULTS, lambda d: [
         f.__setitem__("value", None) for f in d["fields"]
         if f["key"] == "drizzle.pixfrac"]),
                   _edit_json(root, REGISTRY, lambda d: _row(d, "08_drizzle", "pixfrac").__setitem__(
                       "finding", "none")))),
    ("registry_unit_domain_mismatch", "CFG002-02",
     lambda root: _edit_json(root, DEFAULTS, lambda d: [
         f.__setitem__("unit", "deg") for f in d["fields"]
         if f["key"] == "detection.threshold_sigma"])),
    ("contract_anchor_removed", "CFG002-08",
     lambda root: _edit_text(root, CONTRACT_DOC,
                             lambda t: "".join(ln for ln in t.splitlines(True)
                                               if not ln.strip().startswith("CFG002-ANCHOR:")))),
];


def self_test(repo, verbose=True):
    """--self-test：真实仓库全绿 + 每类故障注入在沙箱中必红。"""
    real = run_checks(repo)
    bad = [c for c in real if not c["ok"]]
    if bad:
        return False, ["真实仓库未全绿: %s" % bad], real
    problems = []
    with tempfile.TemporaryDirectory(prefix="cfg002_selftest_") as tmp:
        base = build_sandbox(repo, os.path.join(tmp, "base"))
        baseline = run_checks(base)
        if [c for c in baseline if not c["ok"]]:
            return False, ["沙箱基线未全绿（沙箱不忠实）: %s"
                           % [c for c in baseline if not c["ok"]]], real
        for i, (name, expect, mutate) in enumerate(INJECTIONS):
            root = build_sandbox(repo, os.path.join(tmp, "inj%02d" % i))
            try:
                mutate(root)
            except Exception as exc:  # noqa: BLE001 —— 注入失败必须显式报告，不得静默算过
                problems.append("注入 %s 执行失败（负例面不可用）: %s: %s" % (name, type(exc).__name__, exc))
                continue
            res = {c["id"]: c for c in run_checks(root)}
            if res[expect]["ok"]:
                problems.append("注入 %s 未被 %s 检出（expected red）: %s"
                                % (name, expect, res[expect]["detail"]))
            elif verbose:
                print("  inject %-34s -> %s RED  (%s)" % (name, expect, res[expect]["detail"][:64]))
    return (not problems), problems, real


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=REPO_DEFAULT)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    repo = os.path.abspath(args.repo)
    if not os.path.isdir(repo):
        print("USAGE_ERROR repo not found: %s" % repo)
        return 2
    results = run_checks(repo)
    failures = [c for c in results if not c["ok"]]
    if not args.quiet:
        for c in results:
            print("%-10s %s  %s" % (c["id"], "PASS" if c["ok"] else "FAIL", c["detail"]))
    selftest = None
    rc = 1 if failures else 0
    if args.self_test:
        ok, problems, _real = self_test(repo, verbose=not args.quiet)
        selftest = {"ok": ok, "problems": problems}
        if not args.quiet:
            print("SELF_TEST %s injections=%d problems=%d"
                  % ("PASS" if ok else "FAIL", len(INJECTIONS), len(problems)))
            for p in problems:
                print("  - %s" % p)
        rc = 0 if (ok and not failures) else 1
    if not args.quiet:
        print("CFG002_SUMMARY checks=%d pass=%d fail=%d selftest=%s"
              % (len(results), len(results) - len(failures), len(failures),
                 "n/a" if selftest is None else ("PASS" if selftest["ok"] else "FAIL")))
    if args.json_out:
        path = args.json_out
        if not os.path.isabs(path):
            path = os.path.join(repo, path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({"tool": "check_cfg002_registry", "repo": repo, "checks": results,
                       "self_test": selftest, "passed": rc == 0}, fh, ensure_ascii=False, indent=2)
    return rc


if __name__ == "__main__":
    sys.exit(main())
