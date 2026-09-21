#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_module_map.py — 23 插件模块一致性映射门（CHK-MODULE-MANIFEST，P0）。

权威依据
  - docs/plugins/00_INDEX.md §1（模块归属）/§2（23 篇模块总表）/§3（每篇 8 节模板）/§5（维护规则）
  - ENGINEERING_SPEC.md §4（每模块必备 7 项）/§8（manifest/注册表/target/产品清单一致）
  - ASTROCS_DESIGN.md §7.1（顶层结构唯一）/§7.3（模块与 DLL/SO 边界，单一 entrypoint，
    不隐藏整阶段 Session；每个生产 DAG 节点映射唯一真实 module/导出入口）/§11.3（状态阶梯，唯一口径）
  - docs/ci/01_CHECKS.md §2（CHK-MODULE-MANIFEST = manifest/注册表/构建 target/产品清单一致，P0）
  - eng/contracts/config/module_dll_contract.schema.json（entrypoint_abi 统一 astrocs_module_query_v1）

输入
  docs/modules/MODULE_MAP.yaml（期望映射，23 行；本表不含 status 字段——状态一律现场算）

判据（fail-closed；任一 FAIL 级 finding → exit 1；GAP = 已登记缺口的可见计数）
  M1 映射表结构：23 行、ID 唯一、ID 集合 == docs/plugins/00_INDEX.md §2 机器解析结果；
     注册键 (module_id, entrypoint) 全局唯一（重复 = 两个 DLL 抢同一 entry）。
  M2 必备 7 项（ENGINEERING_SPEC §4）：目标目录 / README.md / module.yaml /
     公开头（含 abi_version 或 struct_size）/ 实现（单一 entrypoint）/
     CMake target（独立 DLL/SO）/ 共址可复用测试。
  M3 module.yaml 字段：ID / 版本 / ABI / 端口 / schema 链接 / entrypoint 与映射表一致。
  M4 合同引用有效：DATA 合同能在 docs/contracts/INDEX.yaml 解析；
     schema 链接能在 eng/contracts/schemas/ 找到（§4 第 7 项：eng/contracts/schemas/ 唯一事实源）；
     glob 形式的 schema 引用无法逐条证明 → 只记 NOTE，不得当通过证据。
  M5 facade/no-op 识别（DESIGN §7.3 / 任务卡步骤 3）：导出符号存在但无可执行路径
     （函数体内零调用）、或 entrypoint 直接转发到整阶段 Session → 判 NOT_IMPLEMENTED，
     绝不因为「符号存在」算 IMPLEMENTED。
  M6 产品清单一致（ENGINEERING_SPEC §8）：eng/packaging/astrocs.product.json 中存在
     对应 unit 且状态非 SKELETON；00_INDEX §2 明示「不进产品 manifest」的模块
     （hips_browser）以豁免理由登记（required=false + 权威引用）。
  M7 状态词只取 ASTROCS_DESIGN §11.3 词表；VERIFIED 必须有证据文件在仓库内，
     禁止表内自证：「合成测试或历史可用节点不等于真实数据/Windows VERIFIED」。

  M8 声明↔文件↔CMake target↔产品清单四方一致（FIX-404 / GAP_AUDIT G3-8）：
     每条路径声明（target_dir/readme/module_yaml/target_file/co_located_tests/schema_links）
     与 target 声明，要么在树中真实存在/被 CMake 定义，要么在 MODULE_MAP 的
     declared_absent_paths 中**逐条登记缺口**（owner + reason，owner 必须命中 gap_owners，
     kind=task 的 owner 必须有任务书文件）。
       * 缺失且未登记        → FAIL(fake_path / fake_target)   「假路径」= 本判据的打击对象；
       * 缺口登记的路径已存在 → FAIL(stale_declared_absent)     棘轮反向自证；
       * owner 未登记/无任务书→ FAIL(gap_owner_unknown)         fail-closed；
       * 已登记缺口          → GAP(declared_absent_registered)  逐条出机器表，不静默、不判绿。
  M9 块名词表（FIX-404 / GAP_AUDIT G1-5）：aio_pipeline.h「标准块定义表」是帧内命名块的
     唯一登记处，且必须与 aio_pipeline.cpp 的 kStandardBlockNames 逐名一致（声明↔实现）；
     生产源（非 eng/tests/fixture）里的 aio_frame_add_block / add_block_move / kv_set 调用点
     只准使用标准名或同文件 aio_block_name_register 显式注册的扩展名。
       * 表解析失败/行数不足 → FAIL(block_table_parse_failed)   fail-closed；
       * 表↔实现名集不一致   → FAIL(block_table_code_drift)；
       * 生产调用点用未注册名→ FAIL(block_name_unregistered)。
  M10 悬空台账收口（与 DOC-403 的 doc-index 门共用扫描数据）：
     消费 eng/tools/doccheck/dangling_ledger.json（缺文件/不可解析/超冻结上限 → FAIL，fail-closed）；
     FIX-404 声明文件域（lib/**）内的悬空条目必须清零；域外条目必须逐条给出
     classification + handoff（to + reason），owner 命中 gap_owners。
       * 台账不可用/超上限       → FAIL(dangling_ledger_unavailable)；
       * lib/** 仍有悬空条目     → FAIL(fix404_domain_dangling_open)；
       * 域外条目未路由/缺分类   → FAIL(dangling_ledger_entry_unrouted)。

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
  - ARCH-001 已把 35 个旧 lib/ 目录迁入 target_dir（清单=eng/cmake/ARCH-001-migration-manifest.md
    §1；MOD-002 已核对 23/23 条 legacy_paths 与该清单一致）；迁移未落地时仓库必然红灯，
    这是本门应有的诚实结论，不得放宽判据换绿。

用法
  python3 eng/tools/quality/check_module_map.py                      # 真实仓库 + 真实映射表
  python3 eng/tools/quality/check_module_map.py --json-out run/.../map.json
  python3 eng/tools/quality/check_module_map.py --repo-root <dir> --map <path>   # fixture
  python3 eng/tools/quality/check_module_map.py --selftest           # 合成正例/负例自检（零副作用）

只读（除 --json-out 显式请求）；仅 stdlib + PyYAML。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import json
import pathlib
import re
import subprocess
import sys
import tempfile

try:
    import yaml
except ImportError:  # pragma: no cover - 环境缺 PyYAML
    print("check_module_map: 需要 PyYAML（eng/ci/validate_workflow_binding.py 同依赖）", file=sys.stderr)
    raise SystemExit(2)

REPO = pathlib.Path(__file__).resolve().parents[3]
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
    "entrypoint_vtable_resolved": "NOTE",
    # --- M8 四方一致 / 缺口登记（FIX-404） ---
    "fake_path": "FAIL", "fake_target": "FAIL", "stale_declared_absent": "FAIL",
    "gap_owner_unknown": "FAIL", "gap_registry_missing": "FAIL",
    "gap_registry_count_mismatch": "FAIL", "stale_declared_absent_capability": "FAIL",
    "declared_absent_registered": "GAP", "declared_absent_capability_registered": "GAP",
    # --- M9 块名词表（FIX-404 / G1-5） ---
    "block_table_parse_failed": "FAIL", "block_table_code_drift": "FAIL",
    "block_name_unregistered": "FAIL", "block_name_registered": "NOTE",
    # --- M10 悬空台账收口（与 DOC-403 doc-index 门共用扫描数据） ---
    "dangling_ledger_unavailable": "FAIL", "fix404_domain_dangling_open": "FAIL",
    "dangling_ledger_entry_unrouted": "FAIL", "dangling_ledger_ok": "NOTE",
}

# 严重级：FAIL 判红；GAP = 已登记缺口（可见计数，不判绿也不判红）；NOTE = 信息。
SEVERITY_FAIL = "FAIL"
SEVERITY_GAP = "GAP"

# M8: 声明路径字段（缺一即缺口）+ target 声明。
DECLARED_PATH_FIELDS = ("target_dir", "readme", "module_yaml", "target_file",
                        "co_located_tests", "schema_links")
# M9: 标准块定义表所在文件与其实现（表↔实现逐名一致）。
BLOCK_TABLE_HEADER = "lib/infrastructure/aio/include/aio_pipeline.h"
BLOCK_TABLE_IMPL = "lib/infrastructure/aio/src/aio_pipeline.cpp"
BLOCK_TABLE_BEGIN = "标准块定义表"
BLOCK_TABLE_END = "块生命周期管理"
# M10: 与 DOC-403 doc-index 门共用的悬空台账 + 冻结上限（只减不增）。
DANGLING_LEDGER = "eng/tools/doccheck/dangling_ledger.json"
LEDGER_FROZEN_MAX = 104
# FIX-404 声明文件域（本任务收口后，台账里不得再有该域的悬空条目）。
FIX404_DOMAIN_PREFIXES = ("lib/",)
LEDGER_CLASSIFICATIONS = ("fixture_or_pattern", "stale_pointer", "retired_namespace",
                          "handoff_required")

# M5 强化判据阈值：query 型 entrypoint 交出的操作 vtable 至少要有这么多个成员
# 能在同文件解析出函数定义（DESIGN §7.3 的九操作 vtable；取 3 为下限）。
VTABLE_MIN_OPS = 3


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


def find_initializer_body(text: str, symbol: str):
    """找 symbol 的聚合初始化体（`symbol ... = { ... }`），返回花括号内文本；找不到返回 None。"""
    clean = strip_comments(text)
    for m in re.finditer(r"\b%s\b" % re.escape(symbol), clean):
        tail = clean[m.end():]
        brace = tail.find("{")
        if brace < 0:
            continue
        semi = tail.find(";")
        if 0 <= semi < brace:
            continue
        eq = tail.find("=")
        if eq < 0 or eq > brace:
            continue
        body = _match_brace(clean, m.end() + brace)
        if body is not None:
            return body
    return None


def resolve_entrypoint_vtable(text: str, body: str):
    """M5 强化：entrypoint 体内零调用时，解析它交出的**操作 vtable**。

    背景（口径订正，2026-09-20）：query 型 entrypoint 的合法形态是
    `*out_api = &g_<x>_api;` —— 只赋值、不调用。旧判据「函数体内零调用 ⇒ noop」
    把 calibration/cosmetic/drizzle/noise_snr/gaia_xpsd_client 五个**真实**
    九操作 adapter 误判为 no-op（假阳）。强化判据要求三条**同时**满足：
      ① entrypoint 体内出现 `= &<vtable>`，且该 vtable 能在同文件解析出聚合初始化体；
      ② 初始化体内至少 VTABLE_MIN_OPS 个成员能在同文件解析出函数定义（九操作 vtable）；
      ③ 至少 1 个成员函数体含真实调用（非空壳）。
    任一不满足 → 返回 None，维持 noop_entrypoint 判定。判别力**只增不减**：
    旧路径（体内有调用即算实现）保持不变，新路径比「符号存在」要求更多证据。
    """
    m = re.search(r"=\s*&\s*([A-Za-z_]\w*)", body)
    if not m:
        return None
    vtable = m.group(1)
    init = find_initializer_body(text, vtable)
    if init is None:
        return None
    ops = []
    for name in dict.fromkeys(re.findall(r"[A-Za-z_]\w*", init)):
        if name == vtable:
            continue
        op_body = find_function_body(text, name)
        if op_body is None:
            continue
        op_calls = [c for c in CALL_RE.findall(op_body)
                    if c.lower() not in CALL_KEYWORDS and c != name]
        ops.append((name, bool(op_calls)))
    live = [n for n, has_call in ops if has_call]
    if len(ops) >= VTABLE_MIN_OPS and live:
        return {"vtable": vtable, "ops": [n for n, _ in ops], "live": live}
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


REGISTERED_COUNT_FILE = "docs/modules/registry/module_id_migration_baseline.json"


def registered_index_count(repo: pathlib.Path):
    """文档集规模 = 显式登记值（烧毁式基线范式，附录 H.6），不再是代码里的 magic number。

    登记值在 docs/modules/registry/module_id_migration_baseline.json#index_module_count，
    只许在落地批内手改并写明依据。未登记时退化为「索引规模必须等于映射表模块数」，
    由 set(map_ids) == set(index_ids) 强一致约束兜底。
    """
    p = repo / REGISTERED_COUNT_FILE
    if not p.is_file():
        return None
    try:
        return int((json.loads(p.read_text(encoding="utf-8")) or {}).get("index_module_count"))
    except Exception:
        return None


class Ctx:
    def __init__(self, repo: pathlib.Path, doc: dict):
        self.repo = repo
        self.doc = doc
        conv = doc.get("conventions") or {}
        self.schema_dir = conv.get("schema_dir", "eng/contracts/schemas")
        self.product_manifest_file = conv.get("product_manifest_file", "eng/packaging/astrocs.product.json")
        self.contract_index_file = conv.get("contract_index_file", "docs/contracts/INDEX.yaml")
        self.module_yaml_keys = tuple(conv.get("module_yaml_required_keys") or MODULE_YAML_DEFAULT_KEYS)
        self.contract_ids = self._load_contract_ids()
        self.product_units = self._load_product_units()
        # M8: 缺口登记（owner 路由 + 已登记缺口键集），fail-closed 由 run_check 判定。
        self.gap_owners, self.gap_items, self.gap_findings = self._load_gap_registry()
        self.gap_keys = {(str(i.get("module")), str(i.get("key")), str(i.get("path")))
                         for i in self.gap_items}
        self.cap_gaps, cap_findings = self._load_capability_gaps()
        self.gap_findings.extend(cap_findings)
        self.cap_hits = set()

    def _load_gap_registry(self):
        """读 MODULE_MAP 的 gap_owners / declared_absent_paths。

        返回 (owners, items, findings)。缺登记表 / owner 未登记 / 任务书缺失 /
        计数不符一律产出 FAIL（fail-closed，不放行）。
        """
        findings = []
        owners = {}
        for ent in (self.doc.get("gap_owners") or []):
            if not isinstance(ent, dict) or not ent.get("id"):
                findings.append(Finding("<map>", "gap_registry_missing",
                                        "gap_owners 条目缺 id：%r" % (ent,)))
                continue
            oid = str(ent["id"])
            owners[oid] = ent
            if ent.get("kind") == "task":
                auth = str(ent.get("authority") or "")
                if not auth or not (self.repo / auth).is_file():
                    findings.append(Finding(
                        oid, "gap_owner_unknown",
                        "gap_owners 登记的任务书不存在：%s（fail-closed）" % (auth or "<空>")))
        reg = self.doc.get("declared_absent_paths")
        if not isinstance(reg, dict) or not isinstance(reg.get("items"), list):
            findings.append(Finding(
                "<map>", "gap_registry_missing",
                "declared_absent_paths 缺失/非表：声明缺失必须逐条登记缺口（禁删条目粉饰）；"
                "确无缺口时也要显式写 items: [] + count: 0"))
            return owners, [], findings
        items = []
        for it in reg["items"]:
            if not isinstance(it, dict) or not all(it.get(k) for k in ("module", "key", "path", "owner", "reason")):
                findings.append(Finding("<map>", "gap_registry_missing",
                                        "缺口条目缺 module/key/path/owner/reason：%r" % (it,)))
                continue
            if str(it["owner"]) not in owners:
                findings.append(Finding(
                    str(it.get("module")), "gap_owner_unknown",
                    "缺口 %s:%s 的 owner %r 未在 gap_owners 登记（fail-closed）"
                    % (it["key"], it["path"], it["owner"])))
            items.append(it)
        path_n = sum(1 for i in items if i["key"] != "target")
        tgt_n = sum(1 for i in items if i["key"] == "target")
        if int(reg.get("path_gap_count", -1)) != path_n:
            findings.append(Finding("<map>", "gap_registry_count_mismatch",
                                    "path_gap_count=%r 与实测 %d 不符"
                                    % (reg.get("path_gap_count"), path_n)))
        if int(reg.get("target_gap_count", -1)) != tgt_n:
            findings.append(Finding("<map>", "gap_registry_count_mismatch",
                                    "target_gap_count=%r 与实测 %d 不符"
                                    % (reg.get("target_gap_count"), tgt_n)))
        return owners, items, findings

    def _load_capability_gaps(self):
        """读 declared_absent_capabilities：已登记的实现缺口（module, code）→ owner。

        与路径缺口同一纪律：登记 ⇒ GAP（可见计数）；未登记 ⇒ 原样 FAIL；
        登记了却不再出现 ⇒ FAIL(stale_declared_absent_capability)（棘轮反向自证）。
        """
        findings = []
        reg = self.doc.get("declared_absent_capabilities")
        if not isinstance(reg, dict) or not isinstance(reg.get("items"), list):
            findings.append(Finding(
                "<map>", "gap_registry_missing",
                "declared_absent_capabilities 缺失/非表（fail-closed）；"
                "确无缺口时也要显式写 items: [] + capability_gap_count: 0"))
            return {}, findings
        caps = {}
        for it in reg["items"]:
            if not isinstance(it, dict) or not all(it.get(k) for k in ("module", "code", "owner", "reason")):
                findings.append(Finding("<map>", "gap_registry_missing",
                                        "能力缺口条目缺 module/code/owner/reason：%r" % (it,)))
                continue
            if str(it["owner"]) not in self.gap_owners:
                findings.append(Finding(
                    str(it.get("module")), "gap_owner_unknown",
                    "能力缺口 %s 的 owner %r 未在 gap_owners 登记（fail-closed）"
                    % (it.get("code"), it.get("owner"))))
            caps[(str(it["module"]), str(it["code"]))] = it
        if int(reg.get("capability_gap_count", -1)) != len(caps):
            findings.append(Finding("<map>", "gap_registry_count_mismatch",
                                    "capability_gap_count=%r 与实测 %d 不符"
                                    % (reg.get("capability_gap_count"), len(caps))))
        return caps, findings

    def _load_contract_ids(self):
        """合同 ID 集合 = 「- id:」条目 ∪ legacy_contract_id_map 中已裁决的 legacy ID。

        DATA-001 裁决（docs/contracts/INDEX.yaml#legacy_contract_id_map 与
        docs/contracts/unified_object_registry.json#legacy_contract_ids）已把一批旧 ID
        判为「legacy -> 统一对象 canonical」。这些 ID 是已登记的，门必须读同一权威；
        只有真正未知的 ID 才判 dangling_data_contract（fail-closed，不放宽）。
        """
        p = self.repo / self.contract_index_file
        if not p.is_file():
            return None
        text = p.read_text(encoding="utf-8", errors="ignore")
        ids = set(re.findall(r"^\s*-\s*id:\s*([A-Za-z0-9_.-]+)\s*$", text, flags=re.M))
        try:
            doc = yaml.safe_load(text) or {}
        except Exception:
            doc = {}
        for ent in (doc.get("legacy_contract_id_map") or []):
            if not isinstance(ent, dict):
                continue
            lid = ent.get("legacy_id")
            if not lid:
                continue
            # mapped 的必须给出非空 canonical_objects；否则视为登记未完成，不放行
            if ent.get("decision") == "mapped" and not (ent.get("canonical_objects") or []):
                continue
            ids.add(str(lid))
        return ids

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


def _missing_declaration_findings(ctx: Ctx, mid: str, key: str, path: str) -> list:
    """M8：一条缺失声明的判定 —— 已登记缺口 ⇒ GAP（可见）；未登记 ⇒ FAIL(fake_path)。

    反向自证：登记为缺口的路径若**已存在**，由 _present_declaration_findings 判红。
    """
    if (mid, key, path) in ctx.gap_keys:
        own = next((i.get("owner") for i in ctx.gap_items
                    if (str(i.get("module")), str(i.get("key")), str(i.get("path")))
                    == (mid, key, path)), "?")
        return [Finding(mid, "declared_absent_registered",
                        "%s 声明缺失但已登记缺口（owner=%s）：%s" % (key, own, path),
                        severity=SEVERITY_GAP)]
    code = "fake_target" if key == "target" else "fake_path"
    return [Finding(mid, code,
                    "%s 声明指向不存在的 %s：%s（未登记缺口 = 假路径；"
                    "不得删声明，须登记 owner）" % (key, "target" if key == "target" else "路径", path))]


def _present_declaration_findings(ctx: Ctx, mid: str, key: str, path: str) -> list:
    """M8 反向棘轮：登记为缺口的声明若已存在/已定义 ⇒ FAIL(stale_declared_absent)。"""
    if (mid, key, path) in ctx.gap_keys:
        return [Finding(mid, "stale_declared_absent",
                        "%s 已在 declared_absent_paths 登记为缺口，但该 %s 现已存在：%s"
                        "（缺口登记必须随落地同步删除）"
                        % (key, "target" if key == "target" else "路径", path))]
    return []


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
            findings.extend(_missing_declaration_findings(ctx, mid, "schema_links", rel))
            contracts_ok = False
        else:
            findings.extend(_present_declaration_findings(ctx, mid, "schema_links", rel))
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
        findings.extend(_missing_declaration_findings(ctx, mid, "target_dir",
                                                      str(m.get("target_dir"))))
    else:
        findings.extend(_present_declaration_findings(ctx, mid, "target_dir",
                                                      str(m.get("target_dir"))))
        readme = ctx.repo / str(m.get("readme", ""))
        if readme.is_file():
            items["readme"] = True
            if mid not in readme.read_text(encoding="utf-8", errors="ignore"):
                findings.append(Finding(mid, "readme_missing_module_id",
                                        "README 未出现模块 ID %s" % mid))
        else:
            findings.extend(_missing_declaration_findings(ctx, mid, "readme", str(m.get("readme"))))

        my_path = ctx.repo / str(m.get("module_yaml", ""))
        if not my_path.is_file():
            findings.extend(_missing_declaration_findings(ctx, mid, "module_yaml",
                                                          str(m.get("module_yaml"))))
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

        header_glob = (ctx.doc.get("conventions") or {}).get("public_header_glob", "lib/include/**/*.h")
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
        body_text = None
        for p in src_files:
            ptext = p.read_text(encoding="utf-8", errors="ignore")
            found = find_function_body(ptext, entrypoint)
            if found is not None:
                body = found
                body_text = ptext
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
                vt = resolve_entrypoint_vtable(body_text, body) if body_text else None
                if vt is not None:
                    items["implementation"] = True
                    findings.append(Finding(
                        mid, "entrypoint_vtable_resolved",
                        "query 型 entrypoint %s 交出操作 vtable %s：%d 个操作有定义、%d 个含真实调用（%s）"
                        % (entrypoint, vt["vtable"], len(vt["ops"]), len(vt["live"]),
                           ", ".join(vt["live"][:4])),
                        severity="NOTE"))
                else:
                    findings.append(Finding(
                        mid, "noop_entrypoint",
                        "entrypoint %s 函数体内零调用且未交出可解析的操作 vtable："
                        "导出符号存在但无可执行路径（判 NOT_IMPLEMENTED）" % entrypoint))
            else:
                items["implementation"] = True

        tf = ctx.repo / str(m.get("target_file", ""))
        target = str(m.get("target", ""))
        if tf.is_file():
            findings.extend(_present_declaration_findings(ctx, mid, "target_file",
                                                          str(m.get("target_file"))))
            text = tf.read_text(encoding="utf-8", errors="ignore")
            if re.search(r"add_(?:library|executable)\s*[(]\s*%s(?![A-Za-z0-9_.-])" % re.escape(target), text):
                items["cmake_target"] = True
                findings.extend(_present_declaration_findings(ctx, mid, "target", target))
            else:
                # M8 四方一致：target_file 在位但 target 未定义 —— 全仓找定义，找不到即缺口。
                if _target_defined_elsewhere(ctx, target, tf):
                    items["cmake_target"] = True
                    findings.append(Finding(
                        mid, "target_defined_outside_declared_file",
                        "target %s 未在声明的 %s 定义，但全仓其它 CMake 有定义（四方一致的定位腿）"
                        % (target, m.get("target_file")), severity="NOTE"))
                else:
                    findings.extend(_missing_declaration_findings(ctx, mid, "target", target))
        else:
            findings.extend(_missing_declaration_findings(ctx, mid, "target_file",
                                                          str(m.get("target_file"))))
            if _target_defined_elsewhere(ctx, target, tf):
                items["cmake_target"] = True
                findings.append(Finding(
                    mid, "target_defined_outside_declared_file",
                    "target %s 定义在全仓其它 CMake（声明 target_file %s 缺失；四方一致的定位腿）"
                    % (target, m.get("target_file")), severity="NOTE"))
            else:
                findings.extend(_missing_declaration_findings(ctx, mid, "target", target))

        tests_dir = ctx.repo / str(m.get("co_located_tests", ""))
        test_files = [p for p in tests_dir.rglob("*") if p.is_file()] if tests_dir.is_dir() else []
        if test_files and any(TEST_NAME_RE.search(str(p)) for p in test_files):
            items["co_located_tests"] = True
            findings.extend(_present_declaration_findings(ctx, mid, "co_located_tests",
                                                          str(m.get("co_located_tests"))))
        elif tests_dir.is_dir():
            # 路径在位、但没有可识别的测试文件：这是「测试缺失」判定，不是假路径。
            findings.extend(_present_declaration_findings(ctx, mid, "co_located_tests",
                                                          str(m.get("co_located_tests"))))
            findings.append(Finding(mid, "missing_co_located_tests",
                                    "共址测试目录在位但无可识别测试文件：%s"
                                    "（ENGINEERING_SPEC §4 第 6 项）" % m.get("co_located_tests")))
        else:
            findings.extend(_missing_declaration_findings(ctx, mid, "co_located_tests",
                                                          str(m.get("co_located_tests"))))

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
    # M8b: 已登记的实现缺口（module, code）⇒ 降级为 GAP（可见计数 + owner），
    # 未登记 ⇒ 原样 FAIL。降级只改严重级与说明，不改任何判定结论（status 现场照算）。
    out = []
    for f in findings:
        it = ctx.cap_gaps.get((mid, str(f["code"])))
        if it is not None and f["severity"] == SEVERITY_FAIL:
            ctx.cap_hits.add((mid, str(f["code"])))
            out.append(Finding(mid, str(f["code"]),
                               "%s（已登记实现缺口 owner=%s：%s）"
                               % (f["detail"], it.get("owner"), it.get("reason")),
                               severity=SEVERITY_GAP))
        else:
            out.append(f)
    return status, out, items



_CMAKE_CACHE = {}


def _all_cmake_texts(repo: pathlib.Path) -> dict:
    """全仓 CMake 文本（缓存）；跳过 build/ 与 third_party/（生成物/vendored）。"""
    key = str(repo)
    if key in _CMAKE_CACHE:
        return _CMAKE_CACHE[key]
    out = {}
    for p in repo.rglob("CMakeLists.txt"):
        rel = p.relative_to(repo).as_posix()
        if rel.startswith("build/") or "/third_party/" in rel or rel.startswith("third_party/"):
            continue
        if p.is_file():
            out[rel] = p.read_text(encoding="utf-8", errors="ignore")
    for p in repo.rglob("*.cmake"):
        rel = p.relative_to(repo).as_posix()
        if rel.startswith("build/") or "/third_party/" in rel or rel.startswith("third_party/"):
            continue
        if p.is_file():
            out.setdefault(rel, p.read_text(encoding="utf-8", errors="ignore"))
    _CMAKE_CACHE[key] = out
    return out


def _target_defined_elsewhere(ctx: Ctx, target: str, declared_file: pathlib.Path) -> bool:
    """target 是否在全仓任一 CMake 文件里被 add_library/add_executable 定义。"""
    if not target:
        return False
    pat = re.compile(r"add_(?:library|executable)\s*[(]\s*%s(?![A-Za-z0-9_.-])" % re.escape(target))
    for rel, text in _all_cmake_texts(ctx.repo).items():
        if (ctx.repo / rel) == declared_file:
            continue
        if pat.search(text):
            return True
    return False


def parse_standard_block_table(header_text: str):
    """M9：解析 aio_pipeline.h「标准块定义表」的块名集合（fail-closed：解析失败返回 None）。"""
    if BLOCK_TABLE_BEGIN not in header_text or BLOCK_TABLE_END not in header_text:
        return None
    seg = header_text.split(BLOCK_TABLE_BEGIN, 1)[1].split(BLOCK_TABLE_END, 1)[0]
    names = []
    for line in seg.splitlines():
        m = re.match(r"^\s*\*\s*\|\s*([A-Za-z_][A-Za-z0-9_]*)\s*\|", line)
        if m:
            names.append(m.group(1))
    return names or None


def parse_impl_block_names(impl_text: str):
    """M9：解析 aio_pipeline.cpp 的 kStandardBlockNames 数组（声明↔实现一致性）。"""
    m = re.search(r"kStandardBlockNames\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;", impl_text, re.S)
    if not m:
        return None
    return re.findall(r'"([^"]+)"', m.group(1))


def check_block_name_vocabulary(repo: pathlib.Path, tracked: list):
    """M9：块名词表 ↔ 生产调用点一致（fail-closed + 可红）。"""
    findings = []
    header = repo / BLOCK_TABLE_HEADER
    impl = repo / BLOCK_TABLE_IMPL
    if not header.is_file() or not impl.is_file():
        findings.append(Finding("<map>", "block_table_parse_failed",
                                "标准块定义表或其实现缺失：%s / %s（fail-closed）"
                                % (BLOCK_TABLE_HEADER, BLOCK_TABLE_IMPL)))
        return findings, {"standard": [], "call_sites": 0}
    names = parse_standard_block_table(header.read_text(encoding="utf-8", errors="ignore"))
    if not names or len(names) < 5:
        findings.append(Finding("<map>", "block_table_parse_failed",
                                "%s 的标准块定义表解析失败或行数不足（fail-closed）" % BLOCK_TABLE_HEADER))
        return findings, {"standard": [], "call_sites": 0}
    impl_names = parse_impl_block_names(impl.read_text(encoding="utf-8", errors="ignore"))
    if impl_names is None:
        findings.append(Finding("<map>", "block_table_code_drift",
                                "%s 中 kStandardBlockNames 解析失败（声明↔实现必须一致）" % BLOCK_TABLE_IMPL))
    elif sorted(set(impl_names)) != sorted(set(names)):
        findings.append(Finding(
            "<map>", "block_table_code_drift",
            "标准表↔实现名集不一致：仅表=%s / 仅实现=%s"
            % (sorted(set(names) - set(impl_names)), sorted(set(impl_names) - set(names)))))
    standard = set(names) | set(impl_names or [])
    # 生产调用点两种形态：直接 aio_frame_* 调用，或编排层 dlsym 得到的函数指针别名
    # （fn_add_block / fn_add_block_move / fn_kv_set ...）——后者同样必须受词表约束。
    call_re = re.compile(
        r"(?:aio_frame_|fn_)(?:add_block|add_block_move|kv_set|kv_set_double)"
        r"\s*\(\s*[^,]+,\s*\"([^\"]+)\"")
    reg_re = re.compile(r"aio_block_name_register\s*\(\s*\"([^\"]+)\"")
    sites = 0
    for rel in tracked:
        if not rel.endswith((".c", ".cc", ".cpp", ".cxx")):
            continue
        parts = rel.split("/")
        # 生产面判定：测试/夹具里的自定义名是**负例**（专测拒绝路径），不入本判据。
        if "tests" in parts or "test" in parts or "fixtures" in parts \
                or re.search(r"(^|[/_])test[_A-Za-z0-9.-]*\.", rel) or "_test." in rel:
            continue
        fp = repo / rel
        if not fp.is_file():
            continue          # git 索引里有、工作树已删（跨域在飞）——不由本判据裁决
        text = fp.read_text(encoding="utf-8", errors="ignore")
        used = set(call_re.findall(text))
        if not used:
            continue
        registered = set(reg_re.findall(text))
        for name in sorted(used):
            sites += 1
            if name in standard:
                continue
            if name in registered:
                findings.append(Finding(rel, "block_name_registered",
                                        "扩展块名 %s 经同文件显式注册（authority 在注册调用内）" % name,
                                        severity="NOTE"))
                continue
            findings.append(Finding(
                rel, "block_name_unregistered",
                "生产调用点使用非标准块名 %r：不在 %s 标准块定义表，且同文件无 "
                "aio_block_name_register 显式注册（FIX-404 / G1-5）" % (name, BLOCK_TABLE_HEADER)))
    return findings, {"standard": sorted(standard), "call_sites": sites}


def check_dangling_ledger(repo: pathlib.Path, gap_owners: dict):
    """M10：消费 DOC-403 doc-index 门的悬空台账（共用扫描数据），收口 FIX-404 域。"""
    findings = []
    p = repo / DANGLING_LEDGER
    if not p.is_file():
        findings.append(Finding("<map>", "dangling_ledger_unavailable",
                                "悬空台账缺失：%s（fail-closed）" % DANGLING_LEDGER))
        return findings, {}
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        findings.append(Finding("<map>", "dangling_ledger_unavailable",
                                "悬空台账不可解析：%s（fail-closed）" % exc))
        return findings, {}
    entries = data.get("entries")
    if not isinstance(entries, list):
        findings.append(Finding("<map>", "dangling_ledger_unavailable",
                                "悬空台账 entries 非列表（fail-closed）"))
        return findings, {}
    if int(data.get("max_entries", -1)) > LEDGER_FROZEN_MAX:
        findings.append(Finding("<map>", "dangling_ledger_unavailable",
                                "台账 max_entries=%r 超过冻结上限 %d（只减不增）"
                                % (data.get("max_entries"), LEDGER_FROZEN_MAX)))
    for e in entries:
        rel = str(e.get("file", ""))
        if rel.startswith(FIX404_DOMAIN_PREFIXES):
            findings.append(Finding(
                rel, "fix404_domain_dangling_open",
                "FIX-404 声明文件域（lib/**）内仍有未收口悬空条目：%r（token=%r）"
                % (rel, e.get("token"))))
            continue
        cls = str(e.get("classification") or "")
        handoff = e.get("handoff") or {}
        if cls not in LEDGER_CLASSIFICATIONS or not handoff.get("to") or not handoff.get("reason"):
            findings.append(Finding(
                rel, "dangling_ledger_entry_unrouted",
                "域外台账条目缺 classification/handoff(to+reason)，未路由即未收口：%r"
                % (e.get("token"),)))
        if str(handoff.get("to") or "") not in gap_owners:
            findings.append(Finding(
                rel, "dangling_ledger_entry_unrouted",
                "台账条目 handoff.to=%r 未在 gap_owners 登记（fail-closed）" % handoff.get("to")))
    counts = {}
    for e in entries:
        counts[str(e.get("owner"))] = counts.get(str(e.get("owner")), 0) + 1
    findings.append(Finding("<map>", "dangling_ledger_ok",
                            "台账 %d 条；FIX-404 域内 0 条；owner 分布=%s" % (len(entries), counts),
                            severity="NOTE"))
    return findings, {"entries": len(entries), "owner_counts": counts}


def run_check(repo: pathlib.Path, map_path: pathlib.Path):
    repo = pathlib.Path(repo).resolve()
    map_path = pathlib.Path(map_path)
    result = {
        "schema_version": 1,
        "tool": "eng/tools/quality/check_module_map.py",
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
    reg_count = registered_index_count(repo)
    if reg_count is None:
        reg_count = len(mods)   # 未登记时退化为「索引规模 == 映射表规模」
    if len(index_ids) != reg_count:
        findings.append(Finding("<map>", "index_parse_failed",
                                "%s §2 机器解析出 %d 个模块 ID（登记值 %d）" % (INDEX_REL, len(index_ids), reg_count)))
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
    # --- M8/M9/M10：缺口登记、块名词表、悬空台账（fail-closed） ---
    findings.extend(ctx.gap_findings)
    stale_caps = sorted(set(ctx.cap_gaps) - ctx.cap_hits)
    for mid_c, code in stale_caps:
        findings.append(Finding(
            mid_c, "stale_declared_absent_capability",
            "能力缺口 (%s, %s) 已登记但实测不再出现：缺口登记必须随落地同步删除" % (mid_c, code)))
    tracked = _git_tracked(repo)
    block_findings, block_detail = check_block_name_vocabulary(repo, tracked)
    ledger_findings, ledger_detail = check_dangling_ledger(repo, ctx.gap_owners)
    findings.extend(block_findings)
    findings.extend(ledger_findings)
    result["gaps"] = {
        "registered": len(ctx.gap_items),
        "path_gaps": sum(1 for i in ctx.gap_items if i.get("key") != "target"),
        "target_gaps": sum(1 for i in ctx.gap_items if i.get("key") == "target"),
        "owners": sorted(ctx.gap_owners),
        "fake_paths": len([f for f in findings if f["code"] in ("fake_path", "fake_target")]),
        "items": [{"module": i.get("module"), "key": i.get("key"), "path": i.get("path"),
                   "owner": i.get("owner")} for i in ctx.gap_items],
    }
    result["block_vocabulary"] = block_detail
    result["dangling_ledger"] = ledger_detail
    result["findings"] = [dict(f) for f in findings]
    fail = [f for f in findings if f["severity"] == SEVERITY_FAIL]
    gaps = [f for f in findings if f["severity"] == SEVERITY_GAP]
    counts = {}
    for r in result["modules"]:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    result["summary"] = {
        "modules_total": len(result["modules"]),
        "index_ids_total": len(index_ids),
        "ids_unique": len(set(map_ids)),
        "status_counts": counts,
        "fail_findings": len(fail),
        "gap_findings": len(gaps),
        "note_findings": len(findings) - len(fail) - len(gaps),
        "registered_gaps": len(ctx.gap_items),
        "fake_paths": result["gaps"]["fake_paths"],
        "verdict": "FAIL" if fail else "PASS",
    }
    rc = 1 if fail else 0
    if len(result["modules"]) != reg_count or len(index_ids) != reg_count:
        rc = 1
    return rc, result


_SKIP_WALK = ("build/", ".git/", "third_party/", "run/", "问题扫描/", "reports/",
              "artifacts/", "reverse_verify/", "工程控制/", "logs/")


def _git_tracked(repo: pathlib.Path) -> list:
    """git tracked 文件清单；git 不可用（如自检 fixture 的合成仓库）时退化为树遍历。

    退化不是放宽：遍历面同样排除 build/.git/third_party 等生成/外部面，
    且块名词表判据在两种面上都只认「标准表 ∪ 同文件显式注册」。
    """
    try:
        r = subprocess.run(["git", "-c", "core.quotepath=false", "ls-files"],
                           cwd=str(repo), capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            return [x for x in r.stdout.splitlines() if x]
    except OSError:
        pass
    out = []
    for p in repo.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(repo).as_posix()
        if any(rel.startswith(s) or ("/" + s) in rel for s in _SKIP_WALK):
            continue
        out.append(rel)
    return out


def _print_human(result: dict, quiet: bool) -> None:
    s = result.get("summary") or {}
    g = result.get("gaps") or {}
    print("CHK-MODULE-MANIFEST: %s  modules=%s/%s unique=%s fail=%s gap=%s note=%s "
          "fake_paths=%s registered_gaps=%s"
          % (s.get("verdict"), s.get("modules_total"), s.get("index_ids_total"),
             s.get("ids_unique"), s.get("fail_findings"), s.get("gap_findings"),
             s.get("note_findings"), s.get("fake_paths"), s.get("registered_gaps")))
    if not quiet:
        for r in result.get("modules", []):
            codes = sorted({f["code"] for f in r["findings"] if f["severity"] == "FAIL"})
            print("  %-18s %-16s %-34s %s"
                  % (r["id"], r["status"], r.get("target_dir"), " ".join(codes)))
        for it in (g.get("items") or []):
            print("  GAP  %-18s %-14s %-52s owner=%s"
                  % (it.get("module"), it.get("key"), it.get("path"), it.get("owner")))
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
    # 正例两条：plain（无变异）与 legacy_contract_ok（引用 legacy 旧 ID 必须绿，证明门读了
    # legacy_contract_id_map 这份权威）；负例见 MUTATIONS（含 dangling_contract_ref = 未知 ID 必须红）。
    # 正例：plain / legacy 合同 ID 映射 / query 型 entrypoint 交出真实操作 vtable（M5 强化判据）。
    cases = ([(name, None if name == "positive" else name, 0) for name in fx.POSITIVES]
             + [(name, name, 1) for name in fx.MUTATIONS])
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

