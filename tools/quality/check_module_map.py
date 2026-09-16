#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_module_map.py — 23 插件模块一致性映射门（CHK-MODULE-MANIFEST，P0）。

权威依据
  - docs/plugins/00_INDEX.md §1（模块归属）/§2（23 篇模块总表）/§3（每篇 8 节模板）/§5（维护规则）
  - ENGINEERING_SPEC.md §4（每模块必备 7 项）/§8（manifest/注册表/target/产品清单一致）
  - ASTROCS_DESIGN.md §7.1（顶层结构唯一）/§7.3（模块与 DLL/SO 边界，单一 entrypoint，
    不隐藏整阶段 Session；每个生产 DAG 节点映射唯一真实 module/导出入口）/§11.3（状态阶梯，唯一口径）
  - docs/ci/01_CHECKS.md §2（CHK-MODULE-MANIFEST = manifest/注册表/构建 target/产品清单一致，P0）
  - contracts/config/module_dll_contract.schema.json（entrypoint_abi 统一 astrocs_module_query_v1）

输入
  docs/modules/MODULE_MAP.yaml（期望映射，23 行；本表不含 status 字段——状态一律现场算）

判据（fail-closed；任一 FAIL 级 finding → exit 1）
  M1 映射表结构：23 行、ID 唯一、ID 集合 == docs/plugins/00_INDEX.md §2 机器解析结果；
     注册键 (module_id, entrypoint) 全局唯一（重复 = 两个 DLL 抢同一 entry）。
  M2 必备 7 项（ENGINEERING_SPEC §4）：目标目录 / README.md / module.yaml /
     公开头（含 abi_version 或 struct_size）/ 实现（单一 entrypoint）/
     CMake target（独立 DLL/SO）/ 共址可复用测试。
  M3 module.yaml 字段：ID / 版本 / ABI / 端口 / schema 链接 / entrypoint 与映射表一致。
  M4 合同引用有效：DATA 合同能在 docs/contracts/INDEX.yaml 解析；
     schema 链接能在 contracts/schemas/ 找到（§4 第 7 项：contracts/schemas/ 唯一事实源）；
     glob 形式的 schema 引用无法逐条证明 → 只记 NOTE，不得当通过证据。
  M5 facade/no-op 识别（DESIGN §7.3 / 任务卡步骤 3）：导出符号存在但无可执行路径
     （函数体内零调用）、或 entrypoint 直接转发到整阶段 Session → 判 NOT_IMPLEMENTED，
     绝不因为「符号存在」算 IMPLEMENTED。
  M6 产品清单一致（ENGINEERING_SPEC §8）：packaging/astrocs.product.json 中存在
     对应 unit 且状态非 SKELETON；00_INDEX §2 明示「不进产品 manifest」的模块
     （hips_browser）以豁免理由登记（required=false + 权威引用）。
  M7 状态词只取 ASTROCS_DESIGN §11.3 词表；VERIFIED 必须有证据文件在仓库内，
     禁止表内自证：「合成测试或历史可用节点不等于真实数据/Windows VERIFIED」。

状态判定（主状态 = 阶梯最高一级；主状态与 verification_status 都是 §11.3 词）
  FAIL            记录自相矛盾（注册键重复 / 合同悬空 / 行字段缺失 / 合同漂移）
  DORMANT/DEFERRED 仅当映射表登记 lifecycle 且带权威引用（当前 23 个模块均未登记）
  NOT_IMPLEMENTED 目标目录或实现缺失，或 entrypoint 为 facade/no-op
  CONTRACT_READY  实现缺失，但插件文档 + schema 链接 + DATA 合同全部在位
  IMPLEMENTED     非 facade 实现在位（7 项齐备但未进产品清单）
  INSTALLED       IMPLEMENTED + CMake target 在位 + 产品清单 unit 在位且非 SKELETON
  VERIFIED        INSTALLED + 映射表登记的证据文件存在且含 Windows x64 + 真实数据标记

诚实性红线（本门的存在意义）
  - 实现只认 target_dir（lib/algorithms/<id> / lib/infrastructure/<id>）。迁移前的旧命名
    目录（lib/star_detector、lib/plate_solve、lib/phase2_*、lib/phase3_*、lib/orchestrator…）
    只是 legacy_paths 历史定位线索，永不参与 IMPLEMENTED/INSTALLED 判定。
  - ARCH-001 已把 35 个旧 lib/ 目录迁入 target_dir（清单=cmake/ARCH-001-migration-manifest.md
    §1；MOD-002 已核对 23/23 条 legacy_paths 与该清单一致）；迁移未落地时仓库必然红灯，
    这是本门应有的诚实结论，不得放宽判据换绿。

用法
  python3 tools/quality/check_module_map.py                      # 真实仓库 + 真实映射表
  python3 tools/quality/check_module_map.py --json-out run/.../map.json
  python3 tools/quality/check_module_map.py --repo-root <dir> --map <path>   # fixture
  python3 tools/quality/check_module_map.py --selftest           # 合成正例/负例自检（零副作用）

只读（除 --json-out 显式请求）；仅 stdlib + PyYAML。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import json
import pathlib
import re
import sys
import tempfile

try:
    import yaml
except ImportError:  # pragma: no cover - 环境缺 PyYAML
    print("check_module_map: 需要 PyYAML（ci/validate_workflow_binding.py 同依赖）", file=sys.stderr)
    raise SystemExit(2)

REPO = pathlib.Path(__file__).resolve().parents[2]
CHECK_ID = "CHK-MODULE-MANIFEST"
MAP_REL = "docs/modules/MODULE_MAP.yaml"
INDEX_REL = "docs/plugins/00_INDEX.md"
STATUS_VOCABULARY = (
    "CONTRACT_READY", "IMPLEMENTED", "INSTALLED", "VERIFIED",
    "NOT_IMPLEMENTED", "NOT_VERIFIED", "DEFERRED", "DORMANT", "FAIL",
)
MODULE_YAML_DEFAULT_KEYS = (
    "id", "module_id", "module_version", "abi_version",
    "entrypoint", "input_ports", "output_ports", "data_contracts",
)
SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx")
HEADER_SUFFIXES = (".h", ".hpp", ".hh")
TEST_NAME_RE = re.compile(r"(?:^|[/\\])(?:test[_A-Za-z0-9.-]*|[A-Za-z0-9_.-]*_test)\.[A-Za-z0-9]+$")
CALL_RE = re.compile(r"([A-Za-z_]\w*)\s*\(")
CALL_KEYWORDS = frozenset({
    "if", "for", "while", "switch", "return", "sizeof", "static_cast",
    "reinterpret_cast", "const_cast", "dynamic_cast", "decltype", "catch",
    "defined", "alignof", "noexcept", "throw", "new", "delete",
})
SESSION_TOKEN_RE = re.compile(r"session", re.IGNORECASE)
FINDING_SEVERITY = {
    "map_missing": "FAIL", "map_unreadable": "FAIL", "map_schema_version": "FAIL",
    "map_modules_not_list": "FAIL", "index_parse_failed": "FAIL",
    "index_id_set_mismatch": "FAIL", "duplicate_module_id": "FAIL",
    "duplicate_entrypoint": "FAIL", "missing_map_field": "FAIL",
    "missing_target_dir": "FAIL", "missing_readme": "FAIL",
    "readme_missing_module_id": "FAIL", "missing_module_yaml": "FAIL",
    "module_yaml_unreadable": "FAIL", "module_yaml_missing_key": "FAIL",
    "module_yaml_value_mismatch": "FAIL", "module_yaml_ports_missing": "FAIL",
    "module_yaml_contract_drift": "FAIL", "missing_public_header": "FAIL",
    "header_missing_abi_version": "FAIL", "missing_implementation": "FAIL",
    "entrypoint_undefined": "FAIL", "facade_session_forward": "FAIL",
    "noop_entrypoint": "FAIL", "missing_cmake_target": "FAIL",
    "missing_co_located_tests": "FAIL", "no_contract_reference": "FAIL",
    "dangling_data_contract": "FAIL", "dangling_schema_link": "FAIL",
    "product_unit_missing": "FAIL", "product_unit_skeleton": "FAIL",
    "product_unit_module_id_mismatch": "FAIL",
    "product_manifest_exemption_missing": "FAIL",
    "product_manifest_unreadable": "FAIL", "contract_index_unreadable": "FAIL",
    "lifecycle_without_authority": "FAIL", "verification_evidence_missing": "FAIL",
    "schema_link_glob_unverifiable": "NOTE", "product_manifest_exempt": "NOTE",
    "not_verified": "NOTE", "legacy_paths_present": "NOTE",
}


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class Finding(dict):
    def __init__(self, module: str, code: str, detail: str, severity=None):
        super().__init__(module=module, code=code,
                         severity=severity or FINDING_SEVERITY.get(code, "FAIL"),
                         detail=detail)


# --------------------------------------------------------------------------- 工具 ----

def strip_comments(text: str) -> str:
    """去块注释与行注释（仅用于 facade 判定；字符串字面量内的 // 可能被误删，保守可接受）。"""
    text = re.sub(r"/[*].*?[*]/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def _match_brace(text: str, start: int):
    """从 text[start] == '{' 起做花括号配对，返回函数体（不含最外层花括号）。"""
    depth = 0
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None


def find_function_body(text: str, symbol: str):
    """在源码文本中找 symbol 的函数定义体；找不到返回 None。"""
    clean = strip_comments(text)
    for m in re.finditer(r"\b%s\s*[(]" % re.escape(symbol), clean):
        line_start = clean.rfind("\n", 0, m.start()) + 1
        head = clean[line_start:m.start()]
        if re.search(r"(^|[^\w])(?:return|=)\s*$", head):
            continue
        tail = clean[m.end():]
        semi = tail.find(";")
        brace = tail.find("{")
        if brace < 0:
            continue
        if 0 <= semi < brace:
            continue
        body = _match_brace(clean, m.end() + brace)
        if body is not None:
            return body
    return None


def parse_index_module_ids(repo: pathlib.Path):
    """从 docs/plugins/00_INDEX.md §2 的模块总表机器解析 23 个模块 ID（表第 2 列）。"""
    p = repo / INDEX_REL
    if not p.is_file():
        return []
    lines = p.read_text(encoding="utf-8", errors="ignore").splitlines()
    try:
        start = next(i for i, ln in enumerate(lines) if re.match(r"^##\s*2[.]", ln))
    except StopIteration:
        return []
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if re.match(r"^##\s*3[.]", lines[i]):
            end = i
            break
    ids = []
    for ln in lines[start:end]:
        m = re.match(r"^\|\s*[\x60]([0-9]{2}_[a-z0-9_]+[.]md)[\x60]\s*\|\s*([A-Za-z0-9_]+)\s*\|", ln)
        if m:
            ids.append(m.group(2))
    return ids


class Ctx:
    def __init__(self, repo: pathlib.Path, doc: dict):
        self.repo = repo
        self.doc = doc
        conv = doc.get("conventions") or {}
        self.schema_dir = conv.get("schema_dir", "contracts/schemas")
        self.product_manifest_file = conv.get("product_manifest_file", "packaging/astrocs.product.json")
        self.contract_index_file = conv.get("contract_index_file", "docs/contracts/INDEX.yaml")
        self.module_yaml_keys = tuple(conv.get("module_yaml_required_keys") or MODULE_YAML_DEFAULT_KEYS)
        self.contract_ids = self._load_contract_ids()
        self.product_units = self._load_product_units()

    def _load_contract_ids(self):
        p = self.repo / self.contract_index_file
        if not p.is_file():
            return None
        return set(re.findall(r"^\s*-\s*id:\s*([A-Za-z0-9_.-]+)\s*$",
                              p.read_text(encoding="utf-8", errors="ignore"), flags=re.M))

    def _load_product_units(self):
        p = self.repo / self.product_manifest_file
        if not p.is_file():
            return None
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
        except Exception:
            return []
        return data.get("units") or []



# ------------------------------------------------------------------- 模块判定 ----

def _source_files(module_dir: pathlib.Path, tests_dir_name: str = "tests"):
    out = []
    for p in sorted(module_dir.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if tests_dir_name in p.relative_to(module_dir).parts:
            continue
        out.append(p)
    return out


def evaluate_module(ctx: Ctx, m: dict, seen_keys: dict):
    mid = str(m.get("id", "?"))
    findings = []
    items = {
        "readme": False, "module_yaml": False, "public_header": False,
        "implementation": False, "cmake_target": False, "co_located_tests": False,
        "contract_refs": False, "product_manifest": False, "plugin_doc": False,
    }

    for key in ("id", "group", "plugin_doc", "target_dir", "readme", "module_yaml",
                "target", "target_file", "co_located_tests", "module_id",
                "module_version", "abi_version", "entrypoint", "data_contracts",
                "schema_links", "product_manifest"):
        if m.get(key) in (None, ""):
            findings.append(Finding(mid, "missing_map_field", "映射表缺少字段 %s" % key))

    registry_key = (str(m.get("module_id")), str(m.get("entrypoint")))
    if registry_key in seen_keys and seen_keys[registry_key] != mid:
        findings.append(Finding(
            mid, "duplicate_entrypoint",
            "注册键 (module_id=%s, entrypoint=%s) 已被模块 %s 占用"
            % (registry_key[0], registry_key[1], seen_keys[registry_key])))
    else:
        seen_keys[registry_key] = mid

    items["plugin_doc"] = (ctx.repo / str(m.get("plugin_doc", ""))).is_file()

    data_contracts = [str(x) for x in (m.get("data_contracts") or [])]
    schema_links = [str(x) for x in (m.get("schema_links") or [])]
    schema_link_globs = [str(x) for x in (m.get("schema_link_globs") or [])]
    contracts_ok = True
    if ctx.contract_ids is None:
        findings.append(Finding(mid, "contract_index_unreadable",
                                "合同索引缺失：%s" % ctx.contract_index_file))
        contracts_ok = False
    else:
        for cid in data_contracts:
            if cid not in ctx.contract_ids:
                findings.append(Finding(mid, "dangling_data_contract",
                                        "DATA 合同 %s 不在 %s 中" % (cid, ctx.contract_index_file)))
                contracts_ok = False
    for rel in schema_links:
        if not (ctx.repo / rel).is_file():
            findings.append(Finding(mid, "dangling_schema_link", "schema 链接不存在：%s" % rel))
            contracts_ok = False
    for pat in schema_link_globs:
        findings.append(Finding(mid, "schema_link_glob_unverifiable",
                                "glob 形式 schema 引用无法逐条证明，不作通过证据：%s" % pat))
    if not data_contracts and not schema_links and not schema_link_globs:
        findings.append(Finding(mid, "no_contract_reference",
                                "端口无任何 DATA 合同/schema 引用（ENGINEERING_SPEC §4 第 7 项）"))
        contracts_ok = False
    items["contract_refs"] = bool(contracts_ok and (data_contracts or schema_links))

    module_dir = ctx.repo / str(m.get("target_dir", ""))
    if not module_dir.is_dir():
        findings.append(Finding(mid, "missing_target_dir",
                                "目标目录不存在：%s（实现只认该目录；legacy 目录不算实现）"
                                % m.get("target_dir")))
    else:
        readme = ctx.repo / str(m.get("readme", ""))
        if readme.is_file():
            items["readme"] = True
            if mid not in readme.read_text(encoding="utf-8", errors="ignore"):
                findings.append(Finding(mid, "readme_missing_module_id",
                                        "README 未出现模块 ID %s" % mid))
        else:
            findings.append(Finding(mid, "missing_readme", "README 缺失：%s" % m.get("readme")))

        my_path = ctx.repo / str(m.get("module_yaml", ""))
        if not my_path.is_file():
            findings.append(Finding(mid, "missing_module_yaml",
                                    "module.yaml 缺失：%s" % m.get("module_yaml")))
        else:
            try:
                my = yaml.safe_load(my_path.read_text(encoding="utf-8", errors="ignore")) or {}
            except Exception as exc:
                my = None
                findings.append(Finding(mid, "module_yaml_unreadable", "module.yaml 解析失败：%s" % exc))
            if isinstance(my, dict):
                items["module_yaml"] = True
                for key in ctx.module_yaml_keys:
                    if key not in my:
                        findings.append(Finding(mid, "module_yaml_missing_key",
                                                "module.yaml 缺必需键 %s" % key))
                for key, expected in (("module_id", m.get("module_id")),
                                      ("abi_version", m.get("abi_version")),
                                      ("entrypoint", m.get("entrypoint")),
                                      ("module_version", m.get("module_version"))):
                    if key in my and my[key] != expected:
                        findings.append(Finding(
                            mid, "module_yaml_value_mismatch",
                            "module.yaml %s=%r 与映射表期望 %r 不一致" % (key, my[key], expected)))
                inp, outp = my.get("input_ports"), my.get("output_ports")
                if not isinstance(inp, list) or not isinstance(outp, list) or not (inp or outp):
                    findings.append(Finding(mid, "module_yaml_ports_missing",
                                            "module.yaml input_ports/output_ports 必须是非空列表"))
                my_contracts = {str(x) for x in (my.get("data_contracts") or [])}
                for cid in sorted(my_contracts - set(data_contracts)):
                    findings.append(Finding(mid, "module_yaml_contract_drift",
                                            "module.yaml data_contracts 多出映射表未登记项：%s" % cid))

        header_glob = (ctx.doc.get("conventions") or {}).get("public_header_glob", "include/**/*.h")
        headers = [p for p in module_dir.glob(header_glob) if p.is_file()]
        if not headers:
            headers = [p for p in module_dir.rglob("*")
                       if p.is_file() and p.suffix.lower() in HEADER_SUFFIXES
                       and "tests" not in p.relative_to(module_dir).parts]
        if headers:
            items["public_header"] = True
            if not any(re.search(r"abi_version|struct_size",
                                 p.read_text(encoding="utf-8", errors="ignore")) for p in headers):
                findings.append(Finding(mid, "header_missing_abi_version",
                                        "公开头缺 abi_version/struct_size（ENGINEERING_SPEC §4 第 3 项）"))
        else:
            findings.append(Finding(mid, "missing_public_header",
                                    "公开头缺失（%s/%s）" % (m.get("target_dir"), header_glob)))


        entrypoint = str(m.get("entrypoint", ""))
        src_files = _source_files(module_dir)
        body = None
        for p in src_files:
            found = find_function_body(p.read_text(encoding="utf-8", errors="ignore"), entrypoint)
            if found is not None:
                body = found
                break
        if body is None:
            declared = any(re.search(r"\b%s\b" % re.escape(entrypoint),
                                     p.read_text(encoding="utf-8", errors="ignore"))
                           for p in src_files) if src_files else False
            if declared:
                findings.append(Finding(mid, "entrypoint_undefined",
                                        "entrypoint %s 只有声明无定义（导出符号无可执行路径）" % entrypoint))
            else:
                findings.append(Finding(mid, "missing_implementation",
                                        "target_dir 下无 entrypoint %s 的生产实现（src/**）" % entrypoint))
        else:
            calls = [c for c in CALL_RE.findall(body)
                     if c.lower() not in CALL_KEYWORDS and c != entrypoint]
            session_calls = sorted({c for c in calls if SESSION_TOKEN_RE.search(c)})
            if session_calls:
                findings.append(Finding(
                    mid, "facade_session_forward",
                    "entrypoint %s 直接转发到整阶段 Session：%s（DESIGN §7.3 禁止；判 NOT_IMPLEMENTED）"
                    % (entrypoint, ", ".join(session_calls))))
            elif not calls:
                findings.append(Finding(
                    mid, "noop_entrypoint",
                    "entrypoint %s 函数体内零调用：导出符号存在但无可执行路径（判 NOT_IMPLEMENTED）" % entrypoint))
            else:
                items["implementation"] = True

        tf = ctx.repo / str(m.get("target_file", ""))
        target = str(m.get("target", ""))
        if tf.is_file():
            text = tf.read_text(encoding="utf-8", errors="ignore")
            if re.search(r"add_(?:library|executable)\s*[(]\s*%s(?![A-Za-z0-9_.-])" % re.escape(target), text):
                items["cmake_target"] = True
            else:
                findings.append(Finding(mid, "missing_cmake_target",
                                        "%s 中无 add_library/add_executable(%s)"
                                        % (m.get("target_file"), target)))
        else:
            findings.append(Finding(mid, "missing_cmake_target",
                                    "target 文件缺失：%s（target=%s）" % (m.get("target_file"), target)))

        tests_dir = ctx.repo / str(m.get("co_located_tests", ""))
        test_files = [p for p in tests_dir.rglob("*") if p.is_file()] if tests_dir.is_dir() else []
        if test_files and any(TEST_NAME_RE.search(str(p)) for p in test_files):
            items["co_located_tests"] = True
        else:
            findings.append(Finding(mid, "missing_co_located_tests",
                                    "共址测试缺失或为空：%s（ENGINEERING_SPEC §4 第 6 项）"
                                    % m.get("co_located_tests")))

    pm = m.get("product_manifest") or {}
    if not pm.get("required", True):
        reason = str(pm.get("exemption_reason") or "").strip()
        if not reason:
            findings.append(Finding(mid, "product_manifest_exemption_missing",
                                    "product_manifest.required=false 必须给出权威豁免理由"))
        else:
            items["product_manifest"] = True
            findings.append(Finding(mid, "product_manifest_exempt",
                                    "产品清单豁免（%s）" % reason))
    else:
        units = ctx.product_units
        if units is None:
            findings.append(Finding(mid, "product_manifest_unreadable",
                                    "产品清单缺失：%s" % ctx.product_manifest_file))
        else:
            match_mid, match_uid = pm.get("match_module_id"), pm.get("match_unit_id")
            hit = None
            for u in units:
                if match_uid and u.get("unit_id") == match_uid:
                    hit = u
                    break
                if match_mid and u.get("module_id") == match_mid:
                    hit = u
                    break
            if hit is None:
                findings.append(Finding(mid, "product_unit_missing",
                                        "产品清单无对应 unit（match_module_id=%s, match_unit_id=%s）"
                                        % (match_mid, match_uid)))
            else:
                ustatus = str(hit.get("status", ""))
                if match_mid and hit.get("module_id") not in (None, match_mid):
                    findings.append(Finding(mid, "product_unit_module_id_mismatch",
                                            "unit %s module_id=%r != 期望 %r"
                                            % (hit.get("unit_id"), hit.get("module_id"), match_mid)))
                if ustatus == "SKELETON":
                    findings.append(Finding(mid, "product_unit_skeleton",
                                            "unit %s 状态 SKELETON（非 §11.3 生产状态，判未安装）"
                                            % hit.get("unit_id")))
                elif ustatus not in ("IMPLEMENTED", "INSTALLED", "VERIFIED"):
                    findings.append(Finding(mid, "product_unit_missing",
                                            "unit %s 状态 %r 不在 IMPLEMENTED/INSTALLED/VERIFIED"
                                            % (hit.get("unit_id"), ustatus)))
                else:
                    items["product_manifest"] = True

    lifecycle = str(m.get("lifecycle") or "").upper()
    verified = False
    if lifecycle in ("DORMANT", "DEFERRED") and not str(m.get("lifecycle_authority") or "").strip():
        findings.append(Finding(mid, "lifecycle_without_authority",
                                "lifecycle=%s 必须带 lifecycle_authority 权威引用" % lifecycle))
    ver = m.get("verification") or {}
    if ver.get("claimed") in ("VERIFIED", True):
        ev_path = ctx.repo / str(ver.get("evidence_path") or "")
        ev_text = ev_path.read_text(encoding="utf-8", errors="ignore") if ev_path.is_file() else ""
        if re.search(r"windows|win64|x64", ev_text, re.I) and re.search(r"real[_-]?data|真实数据", ev_text, re.I):
            verified = True
        else:
            findings.append(Finding(mid, "verification_evidence_missing",
                                    "claimed VERIFIED 但证据缺失或未含 Windows x64 + 真实数据标记"))

    implemented = bool(items["implementation"])
    installed = bool(implemented and items["cmake_target"] and items["product_manifest"])
    contract_ready = bool(items["plugin_doc"] and schema_links and contracts_ok and items["contract_refs"])
    # FAIL 只留给「模块自身记录自相矛盾」；外部合同/schema 面未落地（DATA-001 域）
    # 只让门变红，不把模块进度词污染成 FAIL（进度仍如实记 NOT_IMPLEMENTED）。
    record_broken = any(f["code"] in ("duplicate_entrypoint", "missing_map_field",
                                      "module_yaml_contract_drift")
                        for f in findings)
    if lifecycle in ("DORMANT", "DEFERRED") and not any(
            f["code"] == "lifecycle_without_authority" for f in findings):
        status = lifecycle
    elif record_broken:
        status = "FAIL"
    elif installed:
        status = "VERIFIED" if verified else "INSTALLED"
    elif implemented:
        status = "IMPLEMENTED"
    elif contract_ready:
        status = "CONTRACT_READY"
    else:
        status = "NOT_IMPLEMENTED"
    if status not in STATUS_VOCABULARY:
        status = "FAIL"
        findings.append(Finding(mid, "missing_map_field", "状态词越出 §11.3 词表（内部错误）"))
    if installed and not verified:
        findings.append(Finding(mid, "not_verified",
                                "未取得真实数据 + Windows x64 验收：合成测试不等于 VERIFIED（§11.3）"))
    if m.get("legacy_paths"):
        existing = [p for p in m["legacy_paths"] if (ctx.repo / str(p)).exists()]
        if existing:
            findings.append(Finding(mid, "legacy_paths_present",
                                    "legacy 实现面存在（仅定位用，不参与实现判定）：%s" % ", ".join(existing)))
    return status, findings, items



def run_check(repo: pathlib.Path, map_path: pathlib.Path):
    repo = pathlib.Path(repo).resolve()
    map_path = pathlib.Path(map_path)
    result = {
        "schema_version": 1,
        "tool": "tools/quality/check_module_map.py",
        "check_id": CHECK_ID,
        "generated_utc": _utc_now(),
        "repo_root": str(repo),
        "map_path": str(map_path),
        "authority": [
            "docs/plugins/00_INDEX.md §1/§2/§3/§5",
            "ENGINEERING_SPEC.md §4/§8",
            "ASTROCS_DESIGN.md §7.1/§7.3/§11.3",
            "docs/ci/01_CHECKS.md §2",
        ],
        "status_vocabulary": list(STATUS_VOCABULARY),
        "status_vocabulary_authority": "ASTROCS_DESIGN.md §11.3",
        "modules": [],
        "findings": [],
    }
    findings = []
    doc = {}
    if not map_path.is_file():
        findings.append(Finding("<map>", "map_missing", "映射表缺失：%s" % map_path))
    else:
        try:
            doc = yaml.safe_load(map_path.read_text(encoding="utf-8")) or {}
        except Exception as exc:
            findings.append(Finding("<map>", "map_unreadable", "映射表解析失败：%s" % exc))
            doc = {}
    if not isinstance(doc, dict):
        findings.append(Finding("<map>", "map_unreadable", "映射表根必须是映射"))
        doc = {}
    if doc.get("schema_version") != 1:
        findings.append(Finding("<map>", "map_schema_version",
                                "schema_version 必须为 1，实际 %r" % doc.get("schema_version")))
    mods = doc.get("modules")
    if not isinstance(mods, list):
        findings.append(Finding("<map>", "map_modules_not_list", "modules 必须是列表"))
        mods = []

    index_ids = parse_index_module_ids(repo)
    if len(index_ids) != 23:
        findings.append(Finding("<map>", "index_parse_failed",
                                "%s §2 机器解析出 %d 个模块 ID（期望 23）" % (INDEX_REL, len(index_ids))))
    map_ids = [str(m.get("id")) for m in mods if isinstance(m, dict)]
    if len(set(map_ids)) != len(map_ids):
        dup = sorted({i for i in map_ids if map_ids.count(i) > 1})
        findings.append(Finding("<map>", "duplicate_module_id", "映射表重复模块 ID：%s" % ", ".join(dup)))
    if index_ids and set(map_ids) != set(index_ids):
        findings.append(Finding("<map>", "index_id_set_mismatch",
                                "映射表 ID 集合 != %s §2：缺 %s / 多 %s"
                                % (INDEX_REL, sorted(set(index_ids) - set(map_ids)),
                                   sorted(set(map_ids) - set(index_ids)))))

    ctx = Ctx(repo, doc)
    seen_keys = {}
    for m in mods:
        if not isinstance(m, dict):
            continue
        status, mfind, items = evaluate_module(ctx, m, seen_keys)
        findings.extend(mfind)
        result["modules"].append({
            "id": m.get("id"), "group": m.get("group"), "scope": m.get("scope"),
            "target_dir": m.get("target_dir"), "target": m.get("target"),
            "module_id": m.get("module_id"), "entrypoint": m.get("entrypoint"),
            "plugin_doc": m.get("plugin_doc"),
            "status": status,
            "verification_status": "VERIFIED" if status == "VERIFIED" else "NOT_VERIFIED",
            "contract_ready": bool(items["contract_refs"]) and bool(m.get("schema_links")),
            "implemented": bool(items["implementation"]),
            "installed": bool(items["implementation"] and items["cmake_target"]
                              and items["product_manifest"]),
            "items": items,
            "findings": [dict(f) for f in mfind],
        })
    result["findings"] = [dict(f) for f in findings]
    fail = [f for f in findings if f["severity"] == "FAIL"]
    counts = {}
    for r in result["modules"]:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    result["summary"] = {
        "modules_total": len(result["modules"]),
        "index_ids_total": len(index_ids),
        "ids_unique": len(set(map_ids)),
        "status_counts": counts,
        "fail_findings": len(fail),
        "note_findings": len(findings) - len(fail),
        "verdict": "FAIL" if fail else "PASS",
    }
    rc = 1 if fail else 0
    if len(result["modules"]) != 23 or len(index_ids) != 23:
        rc = 1
    return rc, result


def _print_human(result: dict, quiet: bool) -> None:
    s = result.get("summary") or {}
    print("CHK-MODULE-MANIFEST: %s  modules=%s/%s unique=%s fail=%s note=%s"
          % (s.get("verdict"), s.get("modules_total"), s.get("index_ids_total"),
             s.get("ids_unique"), s.get("fail_findings"), s.get("note_findings")))
    if not quiet:
        for r in result.get("modules", []):
            codes = sorted({f["code"] for f in r["findings"] if f["severity"] == "FAIL"})
            print("  %-18s %-16s %-34s %s"
                  % (r["id"], r["status"], r.get("target_dir"), " ".join(codes)))
    fails = [f for f in result.get("findings", []) if f["severity"] == "FAIL"]
    for f in fails[:40]:
        print("  FAIL %-18s %-32s %s" % (f["module"], f["code"], f["detail"]))
    if len(fails) > 40:
        print("  ... 其余 %d 条 FAIL 见 JSON" % (len(fails) - 40))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="23 插件模块一致性映射门（CHK-MODULE-MANIFEST）")
    ap.add_argument("--repo-root", default=None, help="仓库根（默认本工具上两级）")
    ap.add_argument("--map", default=None, help="映射表路径（默认 <repo>/docs/modules/MODULE_MAP.yaml）")
    ap.add_argument("--json-out", default=None, help="机器 JSON 输出路径")
    ap.add_argument("--quiet", action="store_true", help="只打印汇总与 FAIL 明细")
    ap.add_argument("--selftest", action="store_true", help="合成正例/负例自检（零仓库副作用）")
    args = ap.parse_args(argv)

    if args.selftest:
        return _selftest()

    repo = pathlib.Path(args.repo_root).resolve() if args.repo_root else REPO
    map_path = pathlib.Path(args.map) if args.map else repo / MAP_REL
    rc, result = run_check(repo, map_path)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    _print_human(result, args.quiet)
    return rc


def _selftest() -> int:
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "fixtures"))
    import module_map_fixture as fx
    failures = []
    cases = [("positive", None, 0)] + [(name, name, 1) for name in fx.MUTATIONS]
    with tempfile.TemporaryDirectory(prefix="mod001-selftest-") as td:
        tmp = pathlib.Path(td)
        for name, mutation, expect_rc in cases:
            root = fx.build_repo(tmp / name, mutation=mutation)
            rc, res = run_check(root, root / MAP_REL)
            ok = (rc == expect_rc)
            if name == "positive" and rc == 0 and res["summary"]["modules_total"] != 23:
                ok = False
            print("  selftest %-24s rc=%d expect=%d %s" % (name, rc, expect_rc, "OK" if ok else "MISMATCH"))
            if not ok:
                failures.append(name)
    print("SELFTEST %s (%d cases)" % ("PASS" if not failures else "FAIL: " + ",".join(failures), len(cases)))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())

