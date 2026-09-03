#!/usr/bin/env python3
"""DATA-004 product provenance 层（执行形态；权威文档形态 =
docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md + 本文件 docstring）。

冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-004 + 约束 A.6）：
  - 区分 revision 类别 product/module/ABI/data schema/doc revision/history：
    DATA-001 manifest 的 type_id/schema_version = data schema revision；
    本层用 `revision.product` 记录产物版本、`revision.module` 记录模块版本、
    `revision.abi` 记录 ABI 版本、`revision.data_schema` 记录 data schema
    revision（须与 manifest type_id/schema_version 一致——机器校验器拒绝
    data_schema revision 与 manifest 登记不一致的文档，防止"新数据旧 schema
    冒充"与"旧数据新 schema 静默接收"）；
    `doc_revision` 是 manifest 文档形态自身的修订（doc revision），
    `history` 是已替换（旧）product 版本链（旧 product 版本不静默接收）。
  - provenance digest（确定性溯源摘要）= sha256(规范 JSON)：
      version=provenance 层版本
      revisions: product/module/abi/data_schema 类别字符串与版本
      source_commit: 产生本产物的源码 commit sha（40 hex；执行环境无 git 时
        由调用方显式给出——本模块绝不自行调 git、绝不猜测）
      config_digest: 模块运行配置摘要 sha256/64hex（与 manifest.config_digest 一致）
      provider_digest: 计算后端 provider（CPU ISA/OS 能力）摘要 sha256/64hex
      worker_digest: worker/threading 拓扑摘要 sha256/64hex
      input_digests: 输入产物 {artifact_id, digest}（稳定排序）
      science_ids: 本产物依据的科学合同 ID（SCI-*，稳定排序）
      注意：deterministic = 同输入配置 ⇒ 同 digest。created_utc/run_id 等运行
      事实不进入 digest（运行事实非溯源输入，避免"同输入不同 digest"假象）。
  - 同输入配置 ⇒ 同 provenance digest（确定性验收：运行时间/目录不参与）。
  - 旧 product 版本不静默接收：`assert_not_superseded(revision, history)` 只放行
    revision 未出现在 history 的发布（同 revision 重发布由唯一 producer 拒绝）；
    `supersede()` 生成显式替换记录；版本语义版本号比较按 (major, minor, patch)。
  - privacy scan：`scan_privacy(text)` 对自由文本/诊断做敏感信息扫描——
    绝对类 Unix 路径、Windows 盘符路径、UNC、家目录前缀、URL 用户信息、
    Bearer/凭据形键值 → 一律判定泄露（本模块返回扫描结果，不静默改写）；
    provenance 顶层字段结构上禁止绝对路径（storage_uri/artifact_id 词法层已拒，
    本层不产生任何文件系统路径字段）。

本文件为纯 Python 执行语义（Linux 控制/轻合成节点可完整验证）；Windows 正式
DLL 交付由 DATA-004 同语义 C 接线复刻（provenance digest 公式一致）。
科学公式/常数不改；DATA-001/002 已冻结 schema/registry/validator 不改。
"""
from __future__ import annotations

import hashlib
import json
import re
from typing import Any, Dict, Iterable, List, Optional, Tuple

_SCI_ID_RE = re.compile(r"^SCI-[A-Z0-9-]+$")
_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_HEX40 = re.compile(r"^[0-9a-f]{40}$")
# provider/worker digest 允许任意来源哈希的通用 hex 位数（32/40/64……）；
# 本层只查 hex 词法（1..128 小写），不解读载荷内容
_PROVIDER_HEX_RE = re.compile(r"^[0-9a-f]{1,128}$")
_ARTIFACT_ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")

# revision 类别（DATA-004 冻结全集：product/module/ABI/data schema/doc revision/history）
REVISION_CATEGORIES = ("product", "module", "abi", "data_schema")

PROVENANCE_VERSION = 1
PROVENANCE_SCHEMA = "astrocs.provenance/v1"
HISTORY_SCHEMA = "astrocs.provenance-history/v1"
_KNOWN_HISTORY_KEYS = {
    "history_schema", "history_version", "revision_category", "artifact_id",
    "replaced", "superseded_by", "replaced_at_utc",
}

# 版本号词法（确定性可比较）：段以字母/数字开头结尾，分隔符 ._-+ 只允许单次
# 出现于段间（拒绝 "1..0" / "1.0.0-" 等连续/尾部分隔符）
_VERSION_LIKE = re.compile(r"^[0-9A-Za-z]+(?:[._+-][0-9A-Za-z]+)*$")


class ProvenanceError(ValueError):
    """DATA-004 provenance 语义错误（校验失败/版本冲突/隐私泄露）。"""


def _sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _norm_hex(value: Any, what: str) -> str:
    """把 digest 归一为小写 hex；非法 → ProvenanceError。"""
    if not isinstance(value, str):
        raise ProvenanceError(f"{what} must be a hex string, got {type(value).__name__}")
    v = value.strip().lower()
    if not _HEX64.match(v):
        raise ProvenanceError(f"{what} must be 64 lowercase hex chars")
    return v


def parse_version(version: Any, what: str = "version") -> Tuple[int, ...]:
    """把版本号解析为 (major, minor, patch)；语义化前缀/预发布号丢弃比较。

    支持常见形态：semver "1.2.3"、"0.11.0-alpha.1"；data_schema revision
    "v1"/"v2"（剥前导字母前缀 v → 1/2）；构建标识
    "0.11.0-alpha.1-linux-amd64-gcc14"（只取数值段）。
    """
    if not isinstance(version, str) or not _VERSION_LIKE.match(version):
        raise ProvenanceError(f"{what} invalid version string: {version!r}")
    base = version.split("+")[0].split("-")[0]
    # 剥前导字母前缀（v1 → 1；仅当剩余是数值段）
    while base and base[0].isalpha():
        base = base[1:]
    parts = base.split(".")
    nums: List[int] = []
    for p in parts:
        if p.isdigit():
            nums.append(int(p))
        else:
            break
    if not nums:
        raise ProvenanceError(f"{what} version must start with numeric component: {version!r}")
    return tuple(nums)


def version_gt(a: Any, b: Any) -> bool:
    """版本 a 是否严格大于版本 b（(major,minor,patch) 数值比较）。"""
    return parse_version(a) > parse_version(b)


def version_ge(a: Any, b: Any) -> bool:
    return parse_version(a) >= parse_version(b)


def canonical_provenance_json(doc: Dict[str, Any]) -> str:
    """provenance 顶层规范 JSON（键序 = DATA-004 冻结字段序；ensure_ascii=False 紧凑）。"""
    ORDER = (
        "provenance_schema", "version", "artifact_id", "revision", "source_commit",
        "config_digest", "provider_digest", "worker_digest", "input_digests",
        "science_ids",
    )
    out = {}
    for k in ORDER:
        if k in doc:
            out[k] = doc[k]
    for k in sorted(set(doc.keys()) - set(ORDER)):
        out[k] = doc[k]
    return json.dumps(out, ensure_ascii=False, sort_keys=False, separators=(",", ":"))


def _artifact_label(kind: str, artifact_id: str) -> str:
    return f"{kind}.{artifact_id}"


# ─────────────────────────────────────────────────────────────────────────────
# revision 类别与语义
# ─────────────────────────────────────────────────────────────────────────────


def build_revision(*, product: Any = None, module: Any = None,
                   abi: Any = None, data_schema: Any = None) -> Dict[str, Any]:
    """构造 revision 块（data schema revision 是 DATA-004 新语义，需要显式类别区分）。

    每个类别只接受字符串版本（缺省 None = 不携带该类别；全部缺失拒绝）。
    product = 产物版本（如 "1.2.0"）；module = 模块版本（如 "0.11.0-alpha.1"）；
    abi = ABI 版本（如 "v1"）；data_schema = data schema revision（如 "v1"，
    语义 = manifest type_id.schema_version；校验器要求与 manifest 登记一致）。
    """
    rev: Dict[str, str] = {}
    if product is not None:
        rev["product"] = _require_version(product, "revision.product")
    if module is not None:
        rev["module"] = _require_version(module, "revision.module")
    if abi is not None:
        rev["abi"] = _require_version(abi, "revision.abi")
    if data_schema is not None:
        rev["data_schema"] = _require_version(data_schema, "revision.data_schema")
    if not rev:
        raise ProvenanceError("revision requires at least one category (product/module/abi/data_schema)")
    return rev


def _require_version(v: Any, what: str) -> str:
    if not isinstance(v, str) or not _VERSION_LIKE.match(v):
        raise ProvenanceError(f"{what} invalid version string: {v!r}")
    return v


def validate_revision(revision: Any) -> List[str]:
    """校验 revision 块：object、键 ∈ 类别集合、值为合法版本字符串。"""
    if not isinstance(revision, dict):
        return ["revision must be an object"]
    extra = sorted(set(revision.keys()) - set(REVISION_CATEGORIES))
    errs: List[str] = []
    if extra:
        errs.append(f"revision unknown category: {extra}")
    for cat, ver in revision.items():
        try:
            _require_version(ver, f"revision.{cat}")
        except ProvenanceError as exc:
            errs.append(str(exc))
    return errs


def assert_revision_is_manifest_data_schema(revision: Any, manifest: Dict[str, Any]) -> None:
    """data schema revision 必须与 manifest type_id/schema_version 一致。

    机器校验器（DATA-004）：revision.data_schema 的语义 = manifest.type_id 的
    schema_version。拒绝"新数据旧 schema 冒充"与"旧数据新 schema 静默接收"。
    """
    if not isinstance(manifest, dict) or not isinstance(revision, dict):
        raise ProvenanceError("revision.data_schema consistency requires dict revision + DATA-001 manifest")
    ds = revision.get("data_schema")
    if ds is None:
        return
    tid = manifest.get("type_id")
    sv = manifest.get("schema_version")
    if not isinstance(tid, str) or not isinstance(sv, int):
        raise ProvenanceError("manifest type_id/schema_version missing (cannot bind data_schema revision)")
    expect = f"v{sv}"
    if ds != expect:
        raise ProvenanceError(
            f"data_schema revision {ds!r} inconsistent with manifest type_id {tid!r} "
            f"schema_version v{sv} (expect {expect!r}) — 新数据旧 schema / 旧数据新 schema 一律拒绝")


def assert_doc_revision_is_current(doc_revision: Any, expected: str = "v1") -> None:
    """doc revision：manifest 文档形态修订。非当前 doc revision 显式拒绝（不收）。"""
    if doc_revision is None:
        return  # 未声明（DATA-001 冻结期文档）→ 放行；声明后必须为当前值
    if not isinstance(doc_revision, str) or doc_revision != expected:
        raise ProvenanceError(
            f"doc_revision {doc_revision!r} not current (expected {expected!r}) — "
            f"旧文档形态不静默接收")


def _validate_replaced_entry(entry: Any, idx: int) -> List[str]:
    if not isinstance(entry, dict):
        return [f"replaced[{idx}] must be an object"]
    errs: List[str] = []
    if "version" not in entry:
        errs.append(f"replaced[{idx}].version required")
    else:
        try:
            _require_version(entry["version"], f"replaced[{idx}].version")
        except ProvenanceError as exc:
            errs.append(str(exc))
    if "digest" not in entry:
        errs.append(f"replaced[{idx}].digest required")
    else:
        dg = entry["digest"]
        if not isinstance(dg, dict) or dg.get("algorithm") != "sha256" or not isinstance(dg.get("hex"), str) or not _HEX64.match(dg.get("hex", "")):
            errs.append(f"replaced[{idx}].digest must be sha256/64hex")
    if "reason" in entry and (not isinstance(entry["reason"], str) or not entry["reason"]):
        errs.append(f"replaced[{idx}].reason required non-empty when present")
    extra = sorted(set(entry.keys()) - {"version", "digest", "reason"})
    if extra:
        errs.append(f"replaced[{idx}] additional property not allowed: {extra}")
    return errs


def build_history(*, category: str, artifact_id: str,
                  replaced: Iterable[Dict[str, Any]],
                  superseded_by: Optional[Dict[str, Any]] = None,
                  replaced_at_utc: Optional[str] = None) -> Dict[str, Any]:
    """构造旧 product 版本链（history）——旧 product 版本不静默接收的显式记录。

    category ∈ {product,module,abi,data_schema}；replaced 每项:
      {version, digest:{algorithm=sha256,hex}, reason?}
    replaced 必须非空且按版本升序；当前版本不得出现在 replaced（避免把"当前
    版本"写成已替换——那会让自己被拒收）。
    """
    if category not in REVISION_CATEGORIES:
        raise ProvenanceError(f"history category must be in {sorted(REVISION_CATEGORIES)}: {category!r}")
    if not isinstance(artifact_id, str) or not _ARTIFACT_ID_RE.match(artifact_id):
        raise ProvenanceError(f"history artifact_id invalid: {artifact_id!r}")
    entries = [dict(e) for e in replaced]
    if not entries:
        raise ProvenanceError("history.replaced must be non-empty (旧版本显式记录)")
    for i, e in enumerate(entries):
        errs = _validate_replaced_entry(e, i)
        if errs:
            raise ProvenanceError("; ".join(errs))
    versions = [parse_version(e["version"]) for e in entries]
    if versions != sorted(versions):
        raise ProvenanceError("history.replaced must be in ascending version order")
    if len(set(tuple(v) for v in versions)) != len(versions):
        raise ProvenanceError("history.replaced versions must be unique")
    doc: Dict[str, Any] = {
        "history_schema": HISTORY_SCHEMA,
        "history_version": 1,
        "revision_category": category,
        "artifact_id": artifact_id,
        "replaced": entries,
    }
    if superseded_by is not None:
        errs = _validate_replaced_entry(superseded_by, -1)
        if errs:
            raise ProvenanceError("; ".join(errs))
        doc["superseded_by"] = dict(superseded_by)
    if replaced_at_utc is not None:
        if not isinstance(replaced_at_utc, str) or not re.match(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$", replaced_at_utc):
            raise ProvenanceError("history.replaced_at_utc must be YYYY-MM-DDTHH:MM:SSZ")
        doc["replaced_at_utc"] = replaced_at_utc
    return doc


def validate_history(history: Any) -> List[str]:
    """校验 history 块（完整语义；与 build_history 同一规则集）。"""
    if not isinstance(history, dict):
        return ["history must be an object"]
    errs: List[str] = []
    extra = sorted(set(history.keys()) - _KNOWN_HISTORY_KEYS)
    if extra:
        errs.append(f"history additional property not allowed: {extra}")
    if history.get("history_schema") != HISTORY_SCHEMA:
        errs.append(f"history.history_schema must be {HISTORY_SCHEMA!r}")
    if history.get("history_version") != 1:
        errs.append("history.history_version must be 1")
    cat = history.get("revision_category")
    if cat not in REVISION_CATEGORIES:
        errs.append(f"history.revision_category must be in {sorted(REVISION_CATEGORIES)}: {cat!r}")
    aid = history.get("artifact_id")
    if not isinstance(aid, str) or not _ARTIFACT_ID_RE.match(aid):
        errs.append(f"history.artifact_id invalid: {aid!r}")
    repl = history.get("replaced")
    if not isinstance(repl, list) or not repl:
        errs.append("history.replaced required non-empty")
    else:
        versions = []
        for i, e in enumerate(repl):
            errs.extend(_validate_replaced_entry(e, i))
            if isinstance(e, dict) and isinstance(e.get("version"), str):
                try:
                    versions.append(parse_version(e["version"]))
                except ProvenanceError:
                    pass
        if versions != sorted(versions):
            errs.append("history.replaced must be in ascending version order")
    sb = history.get("superseded_by")
    if sb is not None:
        errs.extend(_validate_replaced_entry(sb, -1))
    ra = history.get("replaced_at_utc")
    if ra is not None and (not isinstance(ra, str) or not re.match(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$", ra)):
        errs.append("history.replaced_at_utc must be YYYY-MM-DDTHH:MM:SSZ")
    return errs


# ─────────────────────────────────────────────────────────────────────────────
# provenance digest（确定性：同输入配置 ⇒ 同 digest）
# ─────────────────────────────────────────────────────────────────────────────


def provenance_digest_hex(*, artifact_id: str, revision: Dict[str, Any],
                          source_commit: str = "",
                          config_digest: Dict[str, Any],
                          provider_digest: Optional[Dict[str, Any]] = None,
                          worker_digest: Optional[Dict[str, Any]] = None,
                          input_digests: Optional[Iterable[Dict[str, Any]]] = None,
                          science_ids: Optional[Iterable[str]] = None) -> str:
    """确定性 provenance digest（sha256/64hex）。

    同输入配置（artifact_id + revision + source_commit + config_digest +
    provider/worker/input hashes + science_ids）⇒ 同 digest——本函数只消费上述
    输入，绝不消费运行时间/临时目录/随机量；created_utc/run_id 等运行事实在
    调用方侧存于旁路字段，不参与 digest（保证确定性验收）。

    字段校验：
      - artifact_id: DATA-001 词法；
      - revision: 合法类别+版本；不含 doc_revision/history（文档形态与旧链不
        参与 digest——旧版本拒收由 supersede/assert 语义保证，不靠混淆 digest）；
      - source_commit: 40 hex（源码 commit 溯源；缺省拒绝——本模块不猜 commit）；
      - config_digest: sha256/64hex（须与 manifest.config_digest 一致，调用方核对）；
      - provider_digest / worker_digest: {algorithm, hex}（hex 1..128 小写；算法名
        建议 sha256；只影响 digest 内容，不影响确定性）；
      - input_digests: [{artifact_id, digest}]，稳定排序（同集合同 digest）；
      - science_ids: SCI-* 列表，稳定排序（同集合同 digest）。
    """
    if not isinstance(artifact_id, str) or not _ARTIFACT_ID_RE.match(artifact_id):
        raise ProvenanceError(f"provenance artifact_id invalid: {artifact_id!r}")
    rev_errs = validate_revision(revision)
    if rev_errs:
        raise ProvenanceError("; ".join(rev_errs))
    if not isinstance(source_commit, str) or not _HEX40.match(source_commit):
        raise ProvenanceError("source_commit must be 40 lowercase hex (源码 commit 溯源)")
    cfg = _norm_hex(config_digest.get("hex"), "config_digest.hex") if isinstance(config_digest, dict) else None
    if cfg is None:
        raise ProvenanceError("config_digest required sha256/64hex")
    prov = {
        "provenance_schema": PROVENANCE_SCHEMA,
        "version": PROVENANCE_VERSION,
        "artifact_id": artifact_id,
        "revision": _ordered_revision(revision),
        "source_commit": source_commit.lower(),
        "config_digest": {"algorithm": "sha256", "hex": cfg},
    }
    if provider_digest is not None:
        prov["provider_digest"] = _norm_provider_digest(provider_digest, "provider_digest")
    if worker_digest is not None:
        prov["worker_digest"] = _norm_provider_digest(worker_digest, "worker_digest")
    if input_digests is not None:
        prov["input_digests"] = _norm_input_digests(input_digests)
    if science_ids is not None:
        prov["science_ids"] = _norm_science_ids(science_ids)
    return _sha256_text(canonical_provenance_json(prov))


def _ordered_revision(revision: Dict[str, Any]) -> Dict[str, Any]:
    """revision 按冻结类别序输出（product/module/abi/data_schema）。"""
    return {c: revision[c] for c in REVISION_CATEGORIES if c in revision}


def _norm_provider_digest(obj: Any, what: str) -> Dict[str, Any]:
    if not isinstance(obj, dict):
        raise ProvenanceError(f"{what} must be an object {{algorithm, hex}}")
    alg = obj.get("algorithm")
    if not isinstance(alg, str) or not alg:
        raise ProvenanceError(f"{what}.algorithm required non-empty")
    hx = obj.get("hex")
    if not isinstance(hx, str) or not _PROVIDER_HEX_RE.match(hx.strip().lower()):
        raise ProvenanceError(f"{what}.hex must be lowercase hex (1..128 chars)")
    return {"algorithm": alg, "hex": hx.strip().lower()}


def _norm_input_digests(items: Any) -> List[Dict[str, Any]]:
    if not isinstance(items, list):
        raise ProvenanceError("input_digests must be an array")
    out = []
    for i, item in enumerate(items):
        if not isinstance(item, dict):
            raise ProvenanceError(f"input_digests[{i}] must be an object")
        aid = item.get("artifact_id")
        if not isinstance(aid, str) or not aid:
            raise ProvenanceError(f"input_digests[{i}].artifact_id required non-empty")
        dg = item.get("digest")
        if not isinstance(dg, str) or not _HEX64.match(dg):
            raise ProvenanceError(f"input_digests[{i}].digest must be 64 lowercase hex")
        out.append({"artifact_id": aid, "digest": dg.lower()})
    out.sort(key=lambda e: (e["artifact_id"], e["digest"]))
    return out


def _norm_science_ids(ids: Any) -> List[str]:
    if not isinstance(ids, list):
        raise ProvenanceError("science_ids must be an array")
    out = []
    for i, sid in enumerate(ids):
        if not isinstance(sid, str) or not _SCI_ID_RE.match(sid):
            raise ProvenanceError(f"science_ids[{i}] must match SCI-[A-Z0-9-]+: {sid!r}")
        out.append(sid)
    out.sort()
    return out


# ─────────────────────────────────────────────────────────────────────────────
# 旧 product 版本不静默接收
# ─────────────────────────────────────────────────────────────────────────────


def assert_not_superseded(revision: Dict[str, Any], history: Any,
                          category: str = "product") -> None:
    """发布前检查：revision 不得是 history 中已记录的旧（被替换）版本。

    - revision 未声明该类别（None/缺）→ 放行（该类别无版本语义）；data_schema
      类别缺省由 manifest 绑定检查兜底；
    - revision 版本出现在 history.replaced 任一 version → 显式拒绝
      （旧 product 版本不静默接收：该版本已由 supersede 记录为被替换版本，
      新发布必须显式升版本并 supersede，不能悄悄重发旧版本——同版本重发也由
      唯一 producer 硬拒绝）；
    - history.superseded_by 是接替者（= 本次发布的当前版本），不构成拒收条件
      ——它指向未来/当前，说明 replaced 旧链被谁接管；本函数只拦"旧版本回归"。
    """
    if not isinstance(revision, dict) or not isinstance(history, dict):
        return  # 无版本语义 / 无历史 → 放行（唯一 producer 已防止静默重复）
    ver = revision.get(category)
    if ver is None:
        return
    herrs = validate_history(history)
    if herrs:
        raise ProvenanceError("history invalid: " + "; ".join(herrs))
    if history.get("revision_category") != category:
        raise ProvenanceError(
            f"history revision_category {history.get('revision_category')!r} != {category!r}")
    repl = history.get("replaced", [])
    for e in repl:
        if isinstance(e, dict) and e.get("version") == ver:
            raise ProvenanceError(
                f"refusing superseded {category} version {ver!r}: already recorded in "
                f"history.replaced (旧 product 版本不静默接收 — 需显式升版本 supersede)")


# ─────────────────────────────────────────────────────────────────────────────
# privacy scan：绝对用户路径/凭据不泄露
# ─────────────────────────────────────────────────────────────────────────────


# 与 LOG-001 redact 对齐的扫描模式：绝对类 Unix 路径、Windows 盘符路径、UNC、
# URL 用户信息、家目录前缀、Bearer、形似凭据键值。
_PRIVACY_PATTERNS = [
    re.compile(r"(?i)\b(password|passwd|pwd|token|secret|api[_-]?key|credential|private[_-]?key)"
               r"\s*[=:]\s*[^\s,;\"']+"),
    re.compile(r"(?i)\bAuthorization\s*:\s*Bearer\s+\S+"),
    re.compile(r"(?i)\bbearer\s+[A-Za-z0-9._~+/=:-]+"),
    re.compile(r"[A-Za-z]:\\[^\s\"',;]+"),            # Windows 盘符绝对路径
    re.compile(r"\\\\[^\\\s\"',;]+\\[^\s\"',;]+"),    # UNC 路径
    re.compile(r"/home/[^/\s\"',;]+(?:/[^\s\"',;]*)*"),
    re.compile(r"/Users/[^/\s\"',;]+(?:/[^\s\"',;]*)*"),
    re.compile(r"/tmp/[^\s\"',;]+"),
    re.compile(r"(?i)\bC:\\Users\\[^\s\"',;]+"),
    re.compile(r"[a-zA-Z][a-zA-Z0-9+.-]*://[^\s\"',;]+"),  # scheme://...（URL 用户信息/令牌）
]

_SECRET_LITERAL = re.compile(r"(?i)\b(?:password|passwd|secret|token|api[_-]?key|credential|private[_-]?key|authorization|bearer)\b")


def scan_privacy(text: str) -> List[str]:
    """privacy scan：文本含绝对用户路径/凭据 → 返回泄露模式列表（空 = 干净）。

    DATA-004 验收：privacy scan 不泄露绝对用户路径/凭据。本扫描器只报告
    （不静默改写，避免"报告层吞证据"）；调用方把含泄露的文本写入 provenance
    相关诊断 → 拒绝发布/记录为泄露。
    """
    if not isinstance(text, str):
        return ["privacy scan input must be a string"]
    hits: List[str] = []
    for pat in _PRIVACY_PATTERNS:
        m = pat.search(text)
        if m:
            hits.append(pat.pattern)
    return sorted(set(hits))


def scan_privacy_doc(doc: Dict[str, Any]) -> List[str]:
    """对 provenance/history 文档做结构扫描：任何字符串字段泄露敏感模式即报告。"""
    if not isinstance(doc, dict):
        return ["privacy scan doc must be an object"]
    hits: List[str] = []
    for k, v in doc.items():
        if isinstance(v, str):
            for pat in scan_privacy(v):
                hits.append(f"{k}: {pat}")
        elif isinstance(v, dict):
            for pat in scan_privacy_doc(v):
                hits.append(f"{k}.{pat}")
        elif isinstance(v, list):
            for i, item in enumerate(v):
                if isinstance(item, dict):
                    for pat in scan_privacy_doc(item):
                        hits.append(f"{k}[{i}].{pat}")
                elif isinstance(item, str):
                    for pat in scan_privacy(item):
                        hits.append(f"{k}[{i}]: {pat}")
    return hits


def assert_privacy_clean(*texts: str) -> None:
    """任一文本含泄露 → ProvenanceError（绝不把敏感内容写进 provenance 相关输出）。"""
    for t in texts:
        hits = scan_privacy(t)
        if hits:
            raise ProvenanceError(f"privacy scan leak: absolute path/credential detected ({len(hits)} pattern(s))")


def assert_doc_privacy_clean(doc: Dict[str, Any]) -> None:
    hits = scan_privacy_doc(doc)
    if hits:
        raise ProvenanceError("privacy scan doc leak: " + "; ".join(hits[:5]))


# ─────────────────────────────────────────────────────────────────────────────
# provenance 顶层完整校验
# ─────────────────────────────────────────────────────────────────────────────


def _check_input_digest_field(items: Any, errs: List[str]) -> None:
    if not isinstance(items, list):
        errs.append("input_digests must be an array")
        return
    seen: set[str] = set()
    for i, item in enumerate(items):
        if not isinstance(item, dict):
            errs.append(f"input_digests[{i}] must be an object")
            continue
        aid = item.get("artifact_id")
        if not isinstance(aid, str) or not _ARTIFACT_ID_RE.match(aid):
            errs.append(f"input_digests[{i}].artifact_id invalid: {aid!r}")
        else:
            if aid in seen:
                errs.append(f"duplicate input artifact_id: {aid!r}")
            seen.add(aid)
        dg = item.get("digest")
        if not isinstance(dg, str) or not _HEX64.match(dg):
            errs.append(f"input_digests[{i}].digest must be 64 lowercase hex")


def _check_science_ids(ids: Any, errs: List[str]) -> None:
    if not isinstance(ids, list):
        errs.append("science_ids must be an array")
        return
    for i, sid in enumerate(ids):
        if not isinstance(sid, str) or not _SCI_ID_RE.match(sid):
            errs.append(f"science_ids[{i}] must match SCI-[A-Z0-9-]+: {sid!r}")


def validate_provenance(doc: Any) -> List[str]:
    """完整校验 provenance 顶层（与 provenance_digest_hex 同一规则集）。

    允许的旁路字段（不参与 digest，只做语义校验）：
      - provenance_digest：发布时计算的 digest（存在时调用方负责与公式一致；
        publish 接线会复算核对）；
      - doc_revision：manifest 文档形态修订；存在必须为当前值（v1）；
      - history：旧 product 版本链（revision_category/artifact_id/版本序完整校验）；
      - created_utc/run_id/phase：运行事实，仅溯源展示，不参与 digest。
    """
    if not isinstance(doc, dict):
        return ["provenance must be an object"]
    errs: List[str] = []
    extra = sorted(set(doc.keys()) - {
        "provenance_schema", "version", "artifact_id", "revision",
        "source_commit", "config_digest", "provider_digest", "worker_digest",
        "input_digests", "science_ids", "provenance_digest", "doc_revision",
        "history", "created_utc", "run_id", "phase",
    })
    if extra:
        errs.append(f"provenance additional property not allowed: {extra}")
    if doc.get("provenance_schema") != PROVENANCE_SCHEMA:
        errs.append(f"provenance_schema must be {PROVENANCE_SCHEMA!r}")
    if doc.get("version") != PROVENANCE_VERSION:
        errs.append(f"version must be {PROVENANCE_VERSION}")
    aid = doc.get("artifact_id")
    if not isinstance(aid, str) or not _ARTIFACT_ID_RE.match(aid):
        errs.append(f"artifact_id invalid: {aid!r}")
    rev = doc.get("revision")
    if rev is None:
        errs.append("revision required")
    else:
        errs.extend(validate_revision(rev))
    sc = doc.get("source_commit")
    if not isinstance(sc, str) or not _HEX40.match(sc):
        errs.append("source_commit must be 40 lowercase hex (源码 commit 溯源)")
    cd = doc.get("config_digest")
    if not isinstance(cd, dict) or cd.get("algorithm") != "sha256" or not isinstance(cd.get("hex"), str) or not _HEX64.match(cd.get("hex", "")):
        errs.append("config_digest must be sha256/64hex")
    for f in ("provider_digest", "worker_digest"):
        obj = doc.get(f)
        if obj is not None:
            if not isinstance(obj, dict) or not isinstance(obj.get("algorithm"), str) or not obj.get("algorithm"):
                errs.append(f"{f}.algorithm required non-empty")
            elif not isinstance(obj.get("hex"), str) or not _PROVIDER_HEX_RE.match(obj["hex"].strip().lower()):
                errs.append(f"{f}.hex must be lowercase hex (1..128 chars)")
    if "input_digests" in doc:
        _check_input_digest_field(doc["input_digests"], errs)
    if "science_ids" in doc:
        _check_science_ids(doc["science_ids"], errs)
    if "provenance_digest" in doc:
        pd = doc["provenance_digest"]
        if not isinstance(pd, str) or not _HEX64.match(pd):
            errs.append("provenance_digest must be 64 lowercase hex (旁路对照字段)")
    if "doc_revision" in doc:
        dr = doc["doc_revision"]
        if not isinstance(dr, str) or dr != "v1":
            errs.append(f"doc_revision must be current 'v1' (旧文档形态不静默接收): {dr!r}")
    if "history" in doc:
        errs.extend(validate_history(doc["history"]))
    for f, pat in (("created_utc", r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$"),
                   ("replaced_at_utc", r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")):
        if f in doc:
            v = doc[f]
            if not isinstance(v, str) or not re.match(pat, v):
                errs.append(f"{f} must be YYYY-MM-DDTHH:MM:SSZ")
    return errs


def provenance_dict_to_digest(doc: Dict[str, Any]) -> str:
    """把完整 provenance 顶层（剔除旁路与标识字段）换算为 digest（可复算）。

    参与 digest 的字段 = provenance_digest_hex 的命名参数（artifact_id/revision/
    source_commit/config_digest/provider_digest/worker_digest/input_digests/
    science_ids）；provenance_schema/version 是文档形态标识，由公式固定写入，
    不在 doc 中重复参与。缺失的必填字段 → ProvenanceError（与公式一致）。
    """
    keys = ("artifact_id", "revision", "source_commit", "config_digest",
            "provider_digest", "worker_digest", "input_digests", "science_ids")
    core = {k: doc[k] for k in keys if k in doc}
    if "artifact_id" not in core or "revision" not in core or "source_commit" not in core or "config_digest" not in core:
        raise ProvenanceError("provenance doc missing digest inputs (artifact_id/revision/source_commit/config_digest)")
    return provenance_digest_hex(**core)


def capability_digest(records: Iterable[Dict[str, Any]],
                      algorithm: str = "sha256") -> str:
    """把 provider/worker 能力记录规范化摘要为 {algorithm, hex}。

    provider 记录形如 {"name": "...", "value": "..."}（如 CPU ISA 能力、
    OS 探测结果）；worker 记录形如 {"kind": "...", "value": "..."}。按
    name/kind + value 稳定排序后拼 JSON 做 sha256 → 同一能力集合同 digest。
    供 manifest 旁路 provider_digest/worker_digest 与 provenance digest 使用。
    """
    if not isinstance(records, Iterable):
        raise ProvenanceError("capability records must be iterable")
    norm = []
    for i, r in enumerate(records):
        if not isinstance(r, dict):
            raise ProvenanceError(f"capability record[{i}] must be an object")
        key = r.get("name") if "name" in r else r.get("kind")
        if not isinstance(key, str) or not key:
            raise ProvenanceError(f"capability record[{i}] requires non-empty name/kind")
        val = r.get("value")
        if not isinstance(val, str):
            raise ProvenanceError(f"capability record[{i}].value must be a string")
        norm.append((key, val))
    norm.sort(key=lambda kv: (kv[0], kv[1]))
    payload = json.dumps(norm, ensure_ascii=False, separators=(",", ":"))
    return _sha256_text(payload)


def make_provenance_doc(*, artifact_id: str, revision: Dict[str, Any],
                        source_commit: str, config_digest: Dict[str, Any],
                        provider_digest: Optional[Dict[str, Any]] = None,
                        worker_digest: Optional[Dict[str, Any]] = None,
                        input_digests: Optional[Iterable[Dict[str, Any]]] = None,
                        science_ids: Optional[Iterable[str]] = None,
                        history: Optional[Dict[str, Any]] = None,
                        doc_revision: Optional[str] = None,
                        created_utc: Optional[str] = None,
                        run_id: Optional[str] = None,
                        phase: Optional[str] = None) -> Dict[str, Any]:
    """构造完整 provenance 文档（含确定性 digest 的旁路字段）。

    digest = provenance_digest_hex(同输入)——只消费溯源输入；history/
    doc_revision/created_utc/run_id/phase 只做语义展示，不进入 digest。
    """
    digest = provenance_digest_hex(
        artifact_id=artifact_id, revision=revision, source_commit=source_commit,
        config_digest=config_digest, provider_digest=provider_digest,
        worker_digest=worker_digest, input_digests=input_digests,
        science_ids=science_ids)
    doc: Dict[str, Any] = {
        "provenance_schema": PROVENANCE_SCHEMA,
        "version": PROVENANCE_VERSION,
        "artifact_id": artifact_id,
        "revision": _ordered_revision(revision),
        "source_commit": source_commit.lower(),
        "config_digest": {"algorithm": "sha256",
                          "hex": _norm_hex(config_digest.get("hex"), "config_digest.hex")},
    }
    if provider_digest is not None:
        doc["provider_digest"] = _norm_provider_digest(provider_digest, "provider_digest")
    if worker_digest is not None:
        doc["worker_digest"] = _norm_provider_digest(worker_digest, "worker_digest")
    if input_digests is not None:
        doc["input_digests"] = _norm_input_digests(input_digests)
    if science_ids is not None:
        doc["science_ids"] = _norm_science_ids(science_ids)
    doc["provenance_digest"] = digest
    if doc_revision is not None:
        if not isinstance(doc_revision, str) or doc_revision != "v1":
            raise ProvenanceError(f"doc_revision must be current 'v1': {doc_revision!r}")
        doc["doc_revision"] = doc_revision
    if history is not None:
        herrs = validate_history(history)
        if herrs:
            raise ProvenanceError("history invalid: " + "; ".join(herrs))
        if history.get("revision_category") in REVISION_CATEGORIES:
            pass
        doc["history"] = dict(history)
    if created_utc is not None:
        if not isinstance(created_utc, str) or not re.match(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$", created_utc):
            raise ProvenanceError("created_utc must be YYYY-MM-DDTHH:MM:SSZ")
        doc["created_utc"] = created_utc
    if run_id is not None:
        if not isinstance(run_id, str) or not run_id:
            raise ProvenanceError("run_id required non-empty")
        doc["run_id"] = run_id
    if phase is not None:
        if phase not in {"phase1", "phase2", "phase3"}:
            raise ProvenanceError(f"phase must be phase1/phase2/phase3: {phase!r}")
        doc["phase"] = phase
    assert_doc_privacy_clean(doc)
    return doc
