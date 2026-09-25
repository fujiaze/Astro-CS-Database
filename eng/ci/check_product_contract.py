#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PRODUCT-CONTRACT：产品级 schema 符合性门（DATA-001/DATA-002 + V6 幂次冻结），fail-closed。

权威依据（沿索引下钻所得，逐条可回指）
  * 交换对象结构 / 角色绑定 / 拒绝条件：
    `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §1（role↔type↔schema_version
    强绑定、最小平面集）、§2/§2a（product_content 是 units 载体；planes 每项
    plane_id/units/dtype/invalid_policy）、§2b（最小平面集）、§4（R-NO-NAME-BINDING：
    输入资格、角色识别、单位/坐标/**平面语义**绝不根据文件名/目录名/路径猜测）、
    §5（X-NO-MANIFEST / X-NO-HASH / X-NO-SCHEMA / X-NO-UNITS）；
  * 机器真源：`eng/contracts/data/phase_product_exchange.schema.json`（$defs.plane 的
    plane_id 枚举 = signal/support/variance/ivar/mask）、
    `eng/contracts/data/artifact_types.registry.json`（type_id ↔ schema_version）、
    `eng/contracts/data/phase_product_exchange_matrix.json`（roles[].min_planes）；
  * 幂次冻结：`eng/contracts/schemas/product_family_field_constraints.schema.json`
    #/$defs/units.bunit_semantics（freeze_id FZ-BUNIT-SEMANTICS + FZ-P3-BUNIT-QUADRATIC）：
    variance = signal^2、ivar = 1/variance，且
    `pixel_area_power_defaults` = {signal_sb: -2, sb_variance_out: -4, sb_ivar_out: 4, flux: 0}；
  * 产品侧现状语义（只作对被检对象的事实比对，不作判据来源）：
    `docs/contracts/DATA_SEMANTICS.md` §12.2（DATA-P1-HIPS 根级 manifest 键）、
    §30.4（Phase3 VARIANCE/IVAR HDU 的 BUNIT = signal BUNIT 的平方 / 其倒数）。

判据（三条判据线，任一判红即 rc=1；fail-closed：**缺件不豁免**）
  T1 EXCHANGE-CONFORMANCE：语料中每一份交换对象文档必须过
     `phase_product_exchange.schema.json` 的结构校验，且 role↔type_id↔schema_version
     与注册表/矩阵一致（缺键、未知 type、枚举越界都在此判红）。
  T2 PLANE-SEMANTICS：`product_content.planes` 必须**显式声明语义面** ——
     每个平面自带 plane_id/units/dtype/invalid_policy；平面集必须覆盖该 role 的
     `min_planes`；当被检对象是**落盘产品树**时，磁盘上实际存在的 science 平面
     （signal/support/variance/ivar/mask）**必须逐个被声明**。未声明 ⇒ 判红
     （「语义面未声明哪个是 variance／逆方差／掩膜」= 本判据的判红面）。
  T3 PLANE-POWER：逐平面把 `units` 解析为立体角幂次 ⇒ `pixel_area_power`，
     必须等于该平面在该 `pixel_semantics` 下的冻结默认值；且
     variance.units ≡ signal.units²、ivar.units ≡ 1/variance.units（指数级精确）。
     同件内若另有 legacy 单标量 `pixel_area_power`，它必须与 signal 平面一致、
     且**不得**与任一其它已声明平面的幂次矛盾。不一致 ⇒ 判红（幂次不符）。

用法
  python3 eng/ci/check_product_contract.py [--json-out <path>]
  python3 eng/ci/check_product_contract.py --corpus-dir <dir> [--corpus-dir ...] [--no-default-corpus]
  python3 eng/ci/check_product_contract.py --self-test
exit 0 = 全过；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
EXCHANGE_SCHEMA = REPO / "eng/contracts/data/phase_product_exchange.schema.json"
ARTIFACT_SCHEMA = REPO / "eng/contracts/data/artifact_manifest.schema.json"
REGISTRY = REPO / "eng/contracts/data/artifact_types.registry.json"
MATRIX = REPO / "eng/contracts/data/phase_product_exchange_matrix.json"
PF_SCHEMA = REPO / "eng/contracts/schemas/product_family_field_constraints.schema.json"
VALIDATOR = REPO / "eng/tests/common/jsonschema_min.py"
FIXTURES = REPO / "eng/ci/fixtures/product_contract"

EXCHANGE_SCHEMA_ID = "astrocs.phase-product-exchange/v1"
ARTIFACT_SCHEMA_ID = "astrocs.artifact-manifest/v1"

PLANE_IDS = ("signal", "support", "variance", "ivar", "mask")
DTYPES = ("float32", "float64", "u8")
INVALID_POLICIES = ("nan_or_support_le_0",)
# DATA-002 §2a：units 非空且取实义字符串（占位/空串/首尾空白一律拒绝）。
PLACEHOLDER_UNITS = {"", "-", "n/a", "na", "none", "null", "tbd", "todo", "placeholder", "?"}

# ── 幂次表：既有落盘事实面，又必须与 V6 冻结值自洽（见 power_defaults_match_v6_frozen 用例） ──
# pixel_area_power 的定义（本项目单位串约定）：BUNIT ∝ A_pix^(p/2)，A_pix ∝ sr
#   ⇒ p = 2 × (units 中 sr 的指数)。
# 例：ADU/sr → -2（面亮度 signal）；ADU^2/sr^2 → -4（面亮度 variance）；
#     sr^2/ADU^2 → +4（面亮度 ivar）；ADU → 0（积分流量及其方差/逆方差）。
POWER_DEFAULTS = {
    "surface_brightness": {"signal": -2, "variance": -4, "ivar": 4},
    "integrated_flux": {"signal": 0, "variance": 0, "ivar": 0},
}
NO_POWER_PLANES = ("support", "mask")  # 无量纲 / 位掩码，不参与立体角幂次判据

_UNIT_TERM = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)(?:\^(-?\d+))?")


def load_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))


def _load_mod(name: str, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _rel(p) -> str:
    try:
        return str(pathlib.Path(p).relative_to(REPO))
    except ValueError:
        return str(p)


def unit_exponents(units: str) -> dict:
    """'ADU^2/sr^2' -> {'ADU': 2, 'sr': -2}；不支持复合分母（产品串不用）。"""
    out: dict = {}

    def acc(part: str, sign: int) -> None:
        for m in _UNIT_TERM.finditer(part):
            base = m.group(1)
            exp = int(m.group(2)) if m.group(2) is not None else 1
            out[base] = out.get(base, 0) + sign * exp

    num, _, den = str(units).partition("/")
    acc(num, 1)
    if den:
        acc(den, -1)
    return {b: e for b, e in out.items() if e != 0}


def pixel_area_power_of(units: str):
    exps = unit_exponents(units)
    if "sr" not in exps:
        return None
    return 2 * exps["sr"]


def _is_placeholder(units) -> bool:
    if not isinstance(units, str):
        return True
    return units.strip().lower() in PLACEHOLDER_UNITS or units != units.strip()


# ── 语料发现（按**内容**判定，不按文件名/目录名 —— DATA-002 §4 R-NO-NAME-BINDING） ──
LEGACY_MANIFEST_KEYS = ("format_version", "hips_version", "products", "nside", "tile_width")


def _classify_doc(doc) -> str:
    if not isinstance(doc, dict):
        return "other"
    if doc.get("exchange_schema") == EXCHANGE_SCHEMA_ID:
        return "exchange"
    if doc.get("manifest_schema") == ARTIFACT_SCHEMA_ID:
        return "artifact_manifest"
    return "other"


def is_legacy_product_descriptor(doc) -> bool:
    """内容判据：DATA-P1/P2-HIPS 根级 manifest（§12.2）或 DATA-P3-* 落盘 descriptor。"""
    if not isinstance(doc, dict):
        return False
    if all(k in doc for k in LEGACY_MANIFEST_KEYS):
        return True
    sch = doc.get("schema")
    return isinstance(sch, str) and sch.startswith("DATA-P") and ("HIPS" in sch or "P3" in sch)


def discover_docs(roots) -> list:
    out = []
    for root in roots:
        p = pathlib.Path(root)
        if p.is_file() and p.suffix == ".json":
            out.append(p)
        elif p.is_dir():
            out.extend(sorted(p.rglob("*.json")))
    return sorted(set(out))


def discover_products(roots) -> list:
    """产品 = 内容判据命中的目录：携带交换对象文档、DATA-001 manifest 或 legacy 产品 descriptor。"""
    found: dict = {}
    for root in roots:
        p = pathlib.Path(root)
        if not p.is_dir():
            continue
        for f in sorted(p.rglob("*.json")):
            try:
                doc = load_json(f)
            except (OSError, ValueError):
                continue
            kind = _classify_doc(doc)
            if kind == "other" and is_legacy_product_descriptor(doc):
                kind = "legacy"
            if kind == "other":
                continue
            found.setdefault(f.parent, {}).setdefault(kind, []).append(f)
    return sorted(found.items())


def on_disk_planes(product_dir) -> dict:
    """落盘 science 平面载体计数（只看是否存在载体文件，不解析内容）。"""
    product_dir = pathlib.Path(product_dir)
    got = {}
    for pid in PLANE_IDS:
        sub = product_dir / pid
        if not sub.is_dir():
            continue
        n = sum(1 for f in sub.rglob("*") if f.is_file())
        if n:
            got[pid] = n
    return got


# ── T1 / T2 / T3 ─────────────────────────────────────────────────────────────
def _registry_types() -> dict:
    return {t["type_id"]: t for t in load_json(REGISTRY)["types"]}


def _matrix_roles() -> dict:
    return {r["role"]: r for r in load_json(MATRIX)["roles"]}


def check_exchange_doc(doc: dict, jm, exchange_schema) -> list:
    """T1：结构校验 + role↔type↔schema_version 绑定。"""
    problems = []
    for path, msg in jm.validate(doc, exchange_schema):
        problems.append("T1 schema: %s: %s" % ("/".join(str(x) for x in path) or "<root>", msg))
    role = doc.get("product_role")
    roles = _matrix_roles()
    if role not in roles:
        problems.append("T1 X-NO-SCHEMA: product_role %r 不在 phase_product_exchange_matrix.roles"
                        % (role,))
        return problems
    bind = roles[role]
    types = _registry_types()
    tid = doc.get("type_id")
    if tid not in types:
        problems.append("T1 X-NO-SCHEMA: type_id %r 未登记于 artifact_types.registry.json" % (tid,))
    else:
        if types[tid].get("schema_version") != doc.get("schema_version"):
            problems.append("T1 X-NO-SCHEMA: schema_version %r != registry %r（type_id=%s）"
                            % (doc.get("schema_version"), types[tid].get("schema_version"), tid))
    if bind.get("type_id") and tid != bind["type_id"]:
        problems.append("T1 X-NO-SCHEMA: role %s 必须绑定 type_id %s，实为 %r"
                        % (role, bind["type_id"], tid))
    return problems


def check_plane_semantics(doc: dict, min_planes, disk_planes=None):
    """T2：语义面显式声明。返回 (problems, declared)。"""
    problems = []
    content = doc.get("product_content")
    if not isinstance(content, dict):
        return ["T2 X-NO-UNITS: product_content 缺失（units/planes 的唯一载体）"], {}
    planes = content.get("planes")
    if not isinstance(planes, list) or not planes:
        return ["T2 PLANE-SEMANTICS: product_content.planes 缺失或为空"], {}
    declared = {}
    for i, pl in enumerate(planes):
        if not isinstance(pl, dict):
            problems.append("T2 PLANE-SEMANTICS: planes[%d] 不是对象" % i)
            continue
        pid = pl.get("plane_id")
        if pid not in PLANE_IDS:
            problems.append("T2 PLANE-SEMANTICS: planes[%d].plane_id %r 不在 %s"
                            % (i, pid, list(PLANE_IDS)))
            continue
        if pid in declared:
            problems.append("T2 PLANE-SEMANTICS: plane_id %r 重复声明" % pid)
            continue
        declared[pid] = pl
        for k in ("units", "dtype", "invalid_policy"):
            if k not in pl:
                problems.append("T2 PLANE-SEMANTICS: planes[%s] 缺必需键 %s（未声明语义面）"
                                % (pid, k))
        if _is_placeholder(pl.get("units")):
            problems.append("T2 X-NO-UNITS: planes[%s].units=%r 缺失/空白/占位（DATA-002 §5）"
                            % (pid, pl.get("units")))
        if "dtype" in pl and pl.get("dtype") not in DTYPES:
            problems.append("T2 PLANE-SEMANTICS: planes[%s].dtype=%r 不在 %s"
                            % (pid, pl.get("dtype"), list(DTYPES)))
        if "invalid_policy" in pl and pl.get("invalid_policy") not in INVALID_POLICIES:
            problems.append("T2 PLANE-SEMANTICS: planes[%s].invalid_policy=%r 不在 %s"
                            % (pid, pl.get("invalid_policy"), list(INVALID_POLICIES)))
    missing_min = sorted(set(min_planes or []) - set(declared))
    if missing_min:
        problems.append("T2 PLANE-SEMANTICS: 该 role 的最小平面集未声明 %s（DATA-002 §2b）"
                        % missing_min)
    for pid, n in sorted((disk_planes or {}).items()):
        if pid not in declared:
            problems.append("T2 PLANE-SEMANTICS: 落盘平面 %r（%d 个载体）未被 "
                            "product_content.planes 声明 —— 语义面未声明即判红"
                            "（DATA-002 §5 X-NO-UNITS）" % (pid, n))
    if content.get("invalid_policy") not in INVALID_POLICIES:
        problems.append("T2 PLANE-SEMANTICS: product_content.invalid_policy=%r 不在 %s"
                        % (content.get("invalid_policy"), list(INVALID_POLICIES)))
    return problems, declared


def check_plane_power(declared: dict, legacy_scalars) -> list:
    """T3：逐平面幂次 + 平方/倒数关系 + legacy 单标量一致性。"""
    problems = []
    sem = None
    if "signal" in declared:
        sig_units = declared["signal"].get("units")
        if isinstance(sig_units, str):
            exps = unit_exponents(sig_units)
            if exps.get("sr") == -1:
                sem = "surface_brightness"
            elif "sr" not in exps:
                sem = "integrated_flux"
    if sem is None:
        problems.append("T3 PLANE-POWER: 无法由 signal 平面 units 判定 pixel_semantics"
                        "（须为面亮度 ADU/sr 或积分流量 ADU）")
        return problems
    defaults = POWER_DEFAULTS[sem]
    for pid, pl in sorted(declared.items()):
        if pid in NO_POWER_PLANES:
            continue
        units = pl.get("units")
        if not isinstance(units, str) or _is_placeholder(units):
            continue
        got = pixel_area_power_of(units)
        if got is None:
            # 单位串里没有立体角 ⇒ A_pix 指数为 0（积分流量族：ADU / ADU^2 / 1/ADU^2）。
            got = 0
        want = defaults.get(pid)
        if want is None:
            continue
        if got != want:
            problems.append("T3 PLANE-POWER: planes[%s].units=%r ⇒ pixel_area_power=%s，"
                            "冻结默认（%s）要求 %s" % (pid, units, got, sem, want))
    if "signal" in declared and "variance" in declared:
        s, v = declared["signal"].get("units"), declared["variance"].get("units")
        if isinstance(s, str) and isinstance(v, str) and not (_is_placeholder(s) or _is_placeholder(v)):
            sq = {b: e * 2 for b, e in unit_exponents(s).items()}
            if unit_exponents(v) != sq:
                problems.append("T3 PLANE-POWER: variance.units=%r 不是 signal.units=%r 的平方"
                                "（FZ-P3-BUNIT-QUADRATIC）" % (v, s))
    if "variance" in declared and "ivar" in declared:
        v, iv = declared["variance"].get("units"), declared["ivar"].get("units")
        if isinstance(v, str) and isinstance(iv, str) and not (_is_placeholder(v) or _is_placeholder(iv)):
            inv = {b: -e for b, e in unit_exponents(v).items()}
            if unit_exponents(iv) != inv:
                problems.append("T3 PLANE-POWER: ivar.units=%r 不是 variance.units=%r 的倒数"
                                "（ivar = 1/variance）" % (iv, v))
    sig_units = declared.get("signal", {}).get("units")
    sig_power = pixel_area_power_of(sig_units) if isinstance(sig_units, str) else None
    for where, value, _units in legacy_scalars:
        if not isinstance(value, int) or isinstance(value, bool):
            continue
        if sig_power is not None and value != sig_power:
            problems.append("T3 PLANE-POWER: %s 声明 pixel_area_power=%s 与 signal 平面 units=%r "
                            "的幂次 %s 不符" % (where, value, sig_units, sig_power))
        for pid, pl in sorted(declared.items()):
            if pid in NO_POWER_PLANES or pid == "signal":
                continue
            u = pl.get("units")
            if not isinstance(u, str) or _is_placeholder(u):
                continue
            p = pixel_area_power_of(u)
            if p is not None and value != p:
                problems.append("T3 PLANE-POWER: %s 的单标量 pixel_area_power=%s 与 %s 平面 units=%r "
                                "的幂次 %s 矛盾（单标量不能同时描述全部语义面）"
                                % (where, value, pid, u, p))
    return problems


def legacy_plane_declarations(legacy_files) -> list:
    """产品自陈的 planes 声明（legacy 形态）：[(rel_path, value)]。

    只作**对被检对象的取证**，不作判据来源 —— DATA-002 §4 R-NO-NAME-BINDING。
    """
    out = []
    for f in legacy_files:
        try:
            doc = load_json(f)
        except (OSError, ValueError):
            continue
        if isinstance(doc, dict) and "planes" in doc:
            out.append((_rel(f), doc.get("planes")))
    return out


def check_legacy_self_consistency(legacy_files) -> list:
    """T2/T3-legacy：**不依赖交换对象文档**也能判的自相矛盾（幂次单标量 / 平面语义不完整）。

    这是「在册产品过不了自家合同」的直接判红面：产品自己写下的单位键与幂次键
    互相矛盾，或平面语义只有名字没有量纲。
    """
    problems = []
    for f in legacy_files:
        try:
            doc = load_json(f)
        except (OSError, ValueError):
            continue
        if not isinstance(doc, dict):
            continue
        rel = _rel(f)
        scalar = doc.get("pixel_area_power")
        units_map = doc.get("units") if isinstance(doc.get("units"), dict) else {}
        if scalar is None and "pixel_area_power" in units_map:
            scalar = units_map.get("pixel_area_power")
        named = []
        for k, v in list(doc.items()) + list(units_map.items()):
            if k.endswith("_bunit") and isinstance(v, str):
                p = pixel_area_power_of(v)
                if p is not None:
                    named.append((k, v, p))
        if scalar is not None and named:
            mismatch = [(k, v, p) for k, v, p in named if p != scalar]
            if mismatch:
                problems.append(
                    "T3 PLANE-POWER(legacy): %s 只用单标量 pixel_area_power=%s 表达像素幂次，"
                    "与同件声明的 %s 不符 —— 单标量无法同时描述 signal/variance/ivar 语义面"
                    % (rel, scalar,
                       "、".join("%s=%r（幂次 %s）" % (k, v, p) for k, v, p in mismatch)))
        for rel2, planes in legacy_plane_declarations([f]):
            if not isinstance(planes, list):
                problems.append("T2 PLANE-SEMANTICS(legacy): %s#planes 不是数组" % rel2)
                continue
            bad = [p for p in planes if p not in PLANE_IDS]
            if bad:
                problems.append(
                    "T2 PLANE-SEMANTICS(legacy): %s#planes 含 %s，不在合同 plane_id 枚举 %s"
                    "（DATA-002 §2a；`coverage` 是 FITS EXTNAME，不是合同 plane_id）"
                    % (rel2, bad, list(PLANE_IDS)))
            if scalar is not None and any(p in ("variance", "ivar") for p in planes):
                want = {"variance": -4, "ivar": 4}
                wrong = [(p, want[p]) for p in planes if p in want and want[p] != scalar]
                if wrong:
                    problems.append(
                        "T3 PLANE-POWER(legacy): %s#planes 声明了 %s，但同件单标量 "
                        "pixel_area_power=%s 与之矛盾（面亮度下应为 %s）"
                        % (rel2, [p for p, _ in wrong], scalar,
                           "、".join("%s=%s" % (p, w) for p, w in wrong)))
            if planes and all(isinstance(p, str) for p in planes):
                problems.append(
                    "T2 PLANE-SEMANTICS(legacy): %s#planes 是裸字符串列表，未按 DATA-002 §2 "
                    "逐平面声明 units/dtype/invalid_policy（X-NO-UNITS：语义面只有名字、无量纲）"
                    % rel2)
    return problems


def _legacy_scalars(legacy_files) -> list:
    out = []
    for f in legacy_files:
        try:
            doc = load_json(f)
        except (OSError, ValueError):
            continue
        if not isinstance(doc, dict):
            continue
        rel = _rel(f)
        if "pixel_area_power" in doc:
            out.append((rel, doc.get("pixel_area_power"), doc.get("units")))
        u = doc.get("units")
        if isinstance(u, dict) and "pixel_area_power" in u:
            out.append((rel + "#units", u.get("pixel_area_power"), u))
    return out


def check_product_tree(product_dir, kinds: dict, jm, exchange_schema) -> list:
    """T1–T3 全链：一个落盘产品。缺交换对象文档 ⇒ 红（fail-closed，不按目录名补语义）。"""
    product_dir = pathlib.Path(product_dir)
    disk = on_disk_planes(product_dir)
    rel = _rel(product_dir)
    exchanges = kinds.get("exchange") or []
    legacy_files = kinds.get("legacy") or []
    legacy_self = check_legacy_self_consistency(legacy_files)
    if not exchanges:
        visible = sorted(disk) or (["<legacy descriptor only>"] if legacy_files else [])
        return ["T1 X-NO-MANIFEST: 产品 %s 未携带任何交换对象文档（exchange_schema=%s）—— "
                "无法建立 product_role/type_id/artifact_manifest/product_content.planes；"
                "落盘可见面=%s。DATA-002 §4 R-NO-NAME-BINDING 禁止据目录名/文件名反推平面语义。"
                % (rel, EXCHANGE_SCHEMA_ID, visible)] + legacy_self
    problems = list(legacy_self)
    legacy = _legacy_scalars(legacy_files)
    for f in exchanges:
        try:
            doc = load_json(f)
        except (OSError, ValueError) as exc:
            problems.append("T1 交换对象 %s 不可解析：%s" % (_rel(f), exc))
            continue
        tag = "@" + f.name
        problems += ["%s %s" % (p, tag) for p in check_exchange_doc(doc, jm, exchange_schema)]
        role = doc.get("product_role")
        min_planes = (_matrix_roles().get(role) or {}).get("min_planes") or []
        ps, declared = check_plane_semantics(doc, min_planes, disk)
        problems += ["%s %s" % (p, tag) for p in ps]
        problems += ["%s %s" % (p, tag) for p in check_plane_power(declared, legacy)]
    return problems


# ── 运行 ─────────────────────────────────────────────────────────────────────
def run(json_out: str = "", corpus_dirs=None, use_default_corpus: bool = True) -> int:
    jm = _load_mod("pc_jsonschema_min", VALIDATOR)
    exchange_schema = load_json(EXCHANGE_SCHEMA)
    artifact_schema = load_json(ARTIFACT_SCHEMA)

    roots = []
    if use_default_corpus:
        roots += [REPO / "eng/contracts/data/examples", FIXTURES]
        # 落盘产品树：只扫 run/<轮次>/out/<产品根> 这一层（有界；run/ 全量 rglob 会拖慢门禁，
        # 且 run/ 是可再生的临时面）。--corpus-dir 可显式追加任意深度。
        for out_root in sorted((REPO / "run").glob("*/out")):
            roots.append(out_root)
            roots.extend(sorted(p for p in out_root.iterdir() if p.is_dir()))
    for d in corpus_dirs or []:
        roots.append(pathlib.Path(d))

    docs = discover_docs(roots)
    if not docs:
        print("PRODUCT-CONTRACT_FAIL: 无任何语料（fail-closed）", file=sys.stderr)
        return 2

    t1_problems, n_exchange, n_artifact = [], 0, 0
    for f in docs:
        try:
            doc = load_json(f)
        except (OSError, ValueError):
            continue
        kind = _classify_doc(doc)
        if kind == "exchange":
            n_exchange += 1
            t1_problems += ["%s: %s" % (_rel(f), p)
                            for p in check_exchange_doc(doc, jm, exchange_schema)]
        elif kind == "artifact_manifest":
            n_artifact += 1
            t1_problems += ["%s: T1 schema: %s: %s"
                            % (_rel(f), "/".join(str(x) for x in path), msg)
                            for path, msg in jm.validate(doc, artifact_schema)]
    if n_exchange == 0:
        t1_problems.append("T1 X-NO-SCHEMA: 语料中不存在任何交换对象文档 —— "
                           "「在册产品」未以合同形态落盘")

    products = discover_products(roots)
    t_problems = []
    for pdir, kinds in products:
        t_problems += ["%s: %s" % (_rel(pdir), p)
                       for p in check_product_tree(pdir, kinds, jm, exchange_schema)]

    ok = not (t1_problems or t_problems)
    report = {
        "schema": "astrocs.product-contract-check/v1",
        "corpus_roots": [_rel(r) for r in roots],
        "n_json_docs": len(docs),
        "n_exchange_docs": n_exchange,
        "n_artifact_manifests": n_artifact,
        "n_products": len(products),
        "t1_conformance": {"ok": not t1_problems, "n_problems": len(t1_problems),
                           "problems": t1_problems[:200]},
        "t2t3_products": {"ok": not t_problems, "n_problems": len(t_problems),
                          "problems": t_problems[:400]},
        "verdict": "PASS" if ok else "FAIL",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("product-contract: docs=%d exchange=%d artifact_manifest=%d products=%d  "
          "T1(交换对象符合性)=%s  T2/T3(语义面+幂次)=%s"
          % (len(docs), n_exchange, n_artifact, len(products),
             "PASS" if not t1_problems else "FAIL", "PASS" if not t_problems else "FAIL"))
    for p in t1_problems[:8]:
        print("  T1 " + p)
    for p in t_problems[:12]:
        print("  T2/T3 " + p)
    print("PRODUCT-CONTRACT_%s" % report["verdict"])
    return 0 if ok else 1


# ── 自检 ─────────────────────────────────────────────────────────────────────
def _conforming_doc() -> dict:
    return {
        "exchange_schema": EXCHANGE_SCHEMA_ID,
        "exchange_version": 1,
        "product_role": "phase1_product_v1",
        "type_id": "astrocs.phase1.frame_hips.v1",
        "schema_version": 1,
        "origin": "astrocs",
        "artifact_manifest": {
            "manifest_schema": ARTIFACT_SCHEMA_ID,
            "manifest_version": 1,
            "artifact_id": "fixture-phase1-frame",
            "type_id": "astrocs.phase1.frame_hips.v1",
            "schema_version": 1,
            "storage_uri": "run:fixture/phase1/frame",
            "content_digest": {"algorithm": "sha256", "hex": "a" * 64},
            "size": 1,
            "producer": {"module_id": "astrocs.phase1.frame_hips"},
            "run": {"run_id": "fixture", "phase": "phase1"},
            "node": {"node_id": "frame_hips"},
            "input_digests": [],
            "config_digest": {"algorithm": "sha256", "hex": "b" * 64},
            "status": "COMPLETE",
            "created_utc": "2026-09-25T00:00:00Z",
        },
        "product_content": {
            "content_schema": "astrocs.phase-product-content/v1",
            "coordinate": {"frame": "icrs", "ra_unit": "deg", "dec_unit": "deg"},
            "geometry": {"format": "hips", "hips": {"ordering": "nested", "tile_width": 512}},
            "planes": [
                {"plane_id": "signal", "units": "ADU/sr", "dtype": "float32",
                 "invalid_policy": "nan_or_support_le_0"},
                {"plane_id": "support", "units": "dimensionless", "dtype": "float32",
                 "invalid_policy": "nan_or_support_le_0"},
                {"plane_id": "variance", "units": "ADU^2/sr^2", "dtype": "float32",
                 "invalid_policy": "nan_or_support_le_0"},
                {"plane_id": "ivar", "units": "sr^2/ADU^2", "dtype": "float32",
                 "invalid_policy": "nan_or_support_le_0"},
                {"plane_id": "mask", "units": "bitmask", "dtype": "u8",
                 "invalid_policy": "nan_or_support_le_0"},
            ],
            "invalid_policy": "nan_or_support_le_0",
        },
    }


def _write_tree(root, doc):
    root = pathlib.Path(root)
    root.mkdir(parents=True, exist_ok=True)
    (root / "phase_product_exchange.json").write_text(
        json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for pid in PLANE_IDS:
        d = root / pid
        d.mkdir(exist_ok=True)
        (d / "Npix0.fits").write_bytes(b"")
    return root


def self_test(json_out: str = "") -> int:
    import copy
    import tempfile
    jm = _load_mod("pc_jsonschema_min", VALIDATOR)
    exchange_schema = load_json(EXCHANGE_SCHEMA)
    roles = _matrix_roles()
    cases = []

    def rec(name, expect_ok, problems, note=""):
        cases.append({"case": name, "expect_ok": expect_ok, "ok": not problems,
                      "problems": [str(p)[:220] for p in problems[:4]], "note": note})

    base = _conforming_doc()
    min_planes = roles["phase1_product_v1"]["min_planes"]

    frozen = load_json(PF_SCHEMA)["$defs"]["units"]["properties"]["bunit_semantics"][
        "properties"]["pixel_area_power_defaults"]["properties"]
    drift = []
    if POWER_DEFAULTS["surface_brightness"]["signal"] != frozen["signal_sb"]["const"]:
        drift.append("signal_sb")
    if POWER_DEFAULTS["surface_brightness"]["variance"] != frozen["sb_variance_out"]["const"]:
        drift.append("sb_variance_out")
    if POWER_DEFAULTS["surface_brightness"]["ivar"] != frozen["sb_ivar_out"]["const"]:
        drift.append("sb_ivar_out")
    if POWER_DEFAULTS["integrated_flux"]["signal"] != frozen["flux"]["const"]:
        drift.append("flux")
    rec("power_defaults_match_v6_frozen", True, ["drift: %s" % drift] if drift else [],
        "本门幂次表 == product_family_field_constraints#/$defs/units.bunit_semantics")

    rec("exchange_conforming_clean", True, check_exchange_doc(base, jm, exchange_schema))
    rec("plane_semantics_conforming_clean", True,
        check_plane_semantics(base, min_planes, {p: 1 for p in PLANE_IDS})[0])
    rec("plane_power_conforming_clean", True,
        check_plane_power(check_plane_semantics(base, min_planes, {})[1], []))

    m = copy.deepcopy(base)
    m.pop("product_role")
    rec("neg1_missing_required_key_product_role", False,
        check_exchange_doc(m, jm, exchange_schema), "缺键：product_role")
    m = copy.deepcopy(base)
    m["product_content"]["planes"][2].pop("units")
    rec("neg1b_missing_plane_units", False, check_plane_semantics(m, min_planes, {})[0],
        "缺键：planes[variance].units")

    m = copy.deepcopy(base)
    m["product_content"]["planes"][2]["plane_id"] = "ivar"
    m["product_content"]["planes"][3]["plane_id"] = "mask"
    rec("neg2_wrong_plane_semantics", False, check_plane_semantics(m, min_planes, {})[0],
        "语义面声明改错：variance→ivar、ivar→mask")
    m = copy.deepcopy(base)
    m["product_content"]["planes"] = [p for p in m["product_content"]["planes"]
                                      if p["plane_id"] != "variance"]
    rec("neg2b_disk_plane_undeclared", False,
        check_plane_semantics(m, min_planes, {"signal": 1, "support": 1, "variance": 1})[0],
        "落盘 variance 面未在 planes 中声明")

    m = copy.deepcopy(base)
    m["product_content"]["planes"][2]["units"] = "ADU/sr"
    rec("neg3_power_mismatch_variance", False,
        check_plane_power(check_plane_semantics(m, min_planes, {})[1], []),
        "variance 幂次不符（ADU/sr 的幂次 -2 ≠ 冻结 -4）")
    m = copy.deepcopy(base)
    m["product_content"]["planes"][3]["units"] = "sr^4/ADU^4"
    rec("neg3b_ivar_not_reciprocal", False,
        check_plane_power(check_plane_semantics(m, min_planes, {})[1], []),
        "ivar 不是 variance 的倒数")
    rec("neg3c_legacy_scalar_contradicts_planes", False,
        check_plane_power(check_plane_semantics(base, min_planes, {})[1],
                          [("manifest.json", -2, None)]),
        "legacy 单标量 pixel_area_power=-2 与 variance/ivar 平面矛盾")

    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        good = _write_tree(td / "good", base)
        # legacy 根级 manifest 不带单标量 pixel_area_power（幂次由 planes 逐面声明承载）。
        (good / "manifest.json").write_text(json.dumps(
            {"format_version": 1, "hips_version": "1.4", "products": list(PLANE_IDS),
             "nside": 262144, "tile_width": 512,
             "units": {"bunit": "ADU/sr", "variance_bunit": "ADU^2/sr^2",
                       "ivar_bunit": "sr^2/ADU^2"}}) + "\n",
            encoding="utf-8")
        kinds = {"exchange": [good / "phase_product_exchange.json"],
                 "legacy": [good / "manifest.json"]}
        rec("tree_conforming_clean", True, check_product_tree(good, kinds, jm, exchange_schema))
        bad_scalar = _write_tree(td / "bad_scalar", base)
        (bad_scalar / "manifest.json").write_text(json.dumps(
            {"format_version": 1, "hips_version": "1.4", "products": list(PLANE_IDS),
             "nside": 262144, "tile_width": 512, "pixel_area_power": -2}) + "\n",
            encoding="utf-8")
        rec("neg_tree_legacy_scalar_power_conflict", False,
            check_product_tree(bad_scalar,
                               {"exchange": [bad_scalar / "phase_product_exchange.json"],
                                "legacy": [bad_scalar / "manifest.json"]}, jm, exchange_schema),
            "legacy 单标量 pixel_area_power=-2 与逐面声明的 variance(-4)/ivar(+4) 矛盾")
        bare = td / "bare"
        for pid in PLANE_IDS:
            (bare / pid).mkdir(parents=True)
            (bare / pid / "Npix0.fits").write_bytes(b"")
        rec("neg_tree_missing_exchange_doc", False,
            check_product_tree(bare, {"legacy": []}, jm, exchange_schema),
            "落盘产品树无交换对象文档 ⇒ X-NO-MANIFEST（目录名不得充当语义来源）")

    ok = all(c["ok"] == c["expect_ok"] for c in cases)
    report = {"schema": "astrocs.product-contract-selftest/v1", "n_cases": len(cases),
              "cases": cases, "verdict": "PASS" if ok else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %-42s expect_ok=%-5s %s"
              % ("PASS" if c["ok"] == c["expect_ok"] else "FAIL", c["case"], c["expect_ok"],
                 c["problems"]))
    print("PRODUCT-CONTRACT-SELFTEST_%s cases=%d" % (report["verdict"], len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="")
    ap.add_argument("--corpus-dir", action="append", default=[])
    ap.add_argument("--no-default-corpus", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args.json_out)
    return run(args.json_out, args.corpus_dir, not args.no_default_corpus)


if __name__ == "__main__":
    raise SystemExit(main())
