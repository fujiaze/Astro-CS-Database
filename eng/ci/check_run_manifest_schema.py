#!/usr/bin/env python3
"""run manifest 双合同符合性门（RULING-DOC-01 / 裁决 D）。

背景（两份**同名**冻结合同，必须分开判）:
  * CLI-003 = docs/engineering/MANIFEST_VERIFY_V1.md（API-MANIFEST-001, FROZEN）§2/§2.1：
    真实产出物 `<output_dir>/astrocs_run_<run_id>.json` 的字段合同（写入者 =
    lib/infrastructure/cli/commands.cpp::write_run_manifest）。
  * CFG-001 = eng/contracts/schemas/run_manifest.schema.json（配置分离锚点
    docs/contracts/config_separation_anchors.json 的 run_manifest 类）：描述「本次运行冻结：
    源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本」的字段名族。

判据（两档，都能红）:
  T1 CLI-003 符合性（严格）：每个真实 manifest 必须带齐 §2/§2.1 的必填字段与类型，且顶层
     不得出现未登记的新键（已登记的加性扩展见 REGISTERED_ADDITIVE）。缺字段/类型错/新键 ⇒ 判红。
  T2 CFG-001 偏差棘轮（登记制）：把每个 manifest 对 eng/contracts/schemas/run_manifest.schema.json
     的偏差（缺必填 / 多余属性）与 eng/ci/ledgers/run_manifest_schema_deviations.json 逐项比对；
     **未登记的新偏差 ⇒ 恒判红**；**登记项已消失（陈旧）⇒ 只在产物语料在场时判红**：
     干净检出里 run/**、artifacts/** 没有产物语料，唯一的真实 manifest 是入库 fixture，它天然
     缺 uncertainty_available 这类只在真实运行里出现的键 ⇒ 按「陈旧」判红是假红（同一提交在
     CI 与开发机上结论相反）。自测接缝 --assume-product-corpus 只把判红面放大（strictness-only）。
     登记集是「两份冻结合同对同名对象约定不同」的冲突台账，每条带冲突权威与处置状态；处置需要
     改 CFG-001 冻结 schema（方案见 run/RULING-DOC-01/REPORT.md），不得自行改冻结合同。
  语料面：run/**、artifacts/**、eng/ci/fixtures/run_manifest/**（外加 --corpus-dir）。其中
     取证/自测用的**整棵工作树快照**里的 astrocs_run_*.json 只反映快照当时的仓库状态（可含
     .gitignore 的陈旧残留），不是本仓运行产物 ⇒ 语料剔除。快照目录**名会变**
     （run/<轮次>/wsrc/** 是现约定，见 eng/tools/audit_intake.py；run/FINAL-07/
     doc-migration/selftest/** 是同一种东西换了名字），所以剔除**按属性判定**、不按目录名
     枚举（否则换名即复现漏判：同一 run_id 3577873f85f6 的同一份物理文件落在 wsrc/ 下被剔、
     落在 selftest/ 下未被剔）。剔除条数**可见登记**（stdout + 报告），不得静默。

用法:
  python3 eng/ci/check_run_manifest_schema.py [--json-out <path>] [--self-test]
    [--corpus-dir <dir>] [--no-default-corpus] [--assume-product-corpus]
自测面（--self-test，隔离语料，不依赖本机 run/** 现状）: T1 正/负例 + T2 正/负例 +
  源树复制体命中被剔除判绿 / **快照目录改名（非 wsrc）仍被剔除**（判据不靠名字） /
  **目录名不构成豁免**（叫 wsrc 但无源树属性仍照常判定）/ 同一 manifest 落产物目录仍判红 /
  无产物语料陈旧不致命但可见登记 / 加严接缝下陈旧致命 / 新偏差恒致命 / 登记项在场不再陈旧。
exit 0 = T1 与 T2 全过；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import contextlib
import importlib.util
import io
import json
import pathlib
import re
import shutil
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
SCHEMA = REPO / "eng/contracts/schemas/run_manifest.schema.json"
LEDGER = REPO / "eng/ci/ledgers/run_manifest_schema_deviations.json"
FIXTURES = REPO / "eng/ci/fixtures/run_manifest"
VALIDATOR = REPO / "eng/tests/common/jsonschema_min.py"

HEX40 = re.compile(r"^[0-9a-f]{40}$")

# ── 非产物语料（源树复制体）的剔除：**按属性判定**，不按目录名字面量枚举 ──
# 快照是整棵工作树的只读拷贝，里面的 manifest 命名文件只反映快照当时的仓库状态（可含
# .gitignore 的陈旧残留），不是本仓运行产物 ⇒ 不参与合同判定（否则门不 hermetic：同一提交
# 在「本机有旧快照」与「CI 干净检出」上结论不同）。
# 旧口径 SNAPSHOT_COMPONENTS = ("wsrc",) 只认一个字面目录名：快照换个名（selftest/ 等）就漏判，
# 于是同一个 run_id 3577873f85f6 的**同一份物理文件**在 4 处 wsrc/ 下被剔、在 1 处
# selftest/ 下未被剔而判红 ⇒ 判据缺陷，不是流程缺陷。
# 改为按**与目录名无关的可判定目录属性**判定「是否落在源树复制体里」，三条签名任一命中即剔：
#   S1 检出根属性：祖先目录直接含 `.git`（git 检出/克隆根）
#   S2 构建根属性：祖先目录直接含 `CMakeLists.txt`（本仓唯一根构建文件）
#   S3 源树布局属性：祖先目录同时含 `docs/` 与 `eng/`（本仓文档面与工具面并置）
# 祖先链在**语料根**处截止（语料根本身不算复制体），否则 run/ 下每个 manifest 都会被剔。
# 剔除仍**可见登记**（stdout + 报告的 n_pruned_* / pruned_* / pruned_reasons），不得静默。
SOURCE_COPY_SIGNATURES = (
    (".git", lambda a: (a / ".git").exists()),
    ("CMakeLists.txt", lambda a: (a / "CMakeLists.txt").is_file()),
    ("docs+eng", lambda a: (a / "docs").is_dir() and (a / "eng").is_dir()),
)


def _source_copy_signatures(anc: pathlib.Path) -> list:
    """祖先目录上命中的「源树/构建根」属性名（空列表 = 不像复制体）。

    每一项是「目录属性」判定式而不是目录名 ⇒ 判据与快照目录叫什么无关；新增属性只改
    SOURCE_COPY_SIGNATURES 一处（登记式）。
    """
    return [name for name, hit in SOURCE_COPY_SIGNATURES if hit(anc)]


# CLI-003 §2 必填（含类型/值域）。值 = 人类可读判据说明（判定在 _check_cli003 内实现）。
CLI003_REQUIRED = {
    "schema_version": "字符串（CLI-003 §2）",
    "kind": "常量 astrocs_run_manifest（CLI-003 §2）",
    "run_id": "非空字符串（CLI-003 §2）",
    "astrocs_version": "字符串（CLI-003 §2）",
    "platform": "对象且含 os/arch（CLI-003 §2）",
    "config_path": "字符串（CLI-003 §2）",
    "cpu_profile_path": "字符串或 null（CLI-003 §2）",
    "config_sha256": "字符串（CLI-003 §2；输入文件字节 hash）",
    "cpu_profile_sha256": "字符串或 null（CLI-003 §2）",
    "phases": "整数数组（CLI-003 §2）",
    "artifacts": "对象数组，每项含 role/path/sha256/size_bytes（CLI-003 §2 + §4 artifact 词表）",
    "status": "complete | incomplete（CLI-003 §2）",
    "started_utc": "字符串（CLI-003 §2）",
    "finished_utc": "字符串（CLI-003 §2）",
    "provenance": "对象（CLI-003 §2.1 加性扩展，见 PROVENANCE_REQUIRED）",
}
PROVENANCE_REQUIRED = {
    "source_sha": "40hex（CLI-003 §2.1；ASTROCS_COMMIT_SHA）",
    "source_version": "字符串（CLI-003 §2.1）",
    "algorithm_ids": "数组（CLI-003 §2.1）",
    "module_build_ids": "数组（CLI-003 §2.1）",
    "providers": "数组（CLI-003 §2.1）",
    "units": "数组（CLI-003 §2.1）",
    "coordinate_frames": "数组（CLI-003 §2.1）",
    "input_product_hashes": "数组（CLI-003 §2.1）",
    "output_product_hashes": "数组（CLI-003 §2.1）",
}
# 已登记的加性顶层键（各自有冻结合同依据；未登记的新键一律判红）。
REGISTERED_ADDITIVE = {
    "summary": "CLI-003 §4 final 事件同源摘要（write_run_manifest 写入）",
    "error": "SMOKE-001 D7：失败/取消 manifest 自带失败原因",
    "uncertainty_available": "FIX-E2E B1-A5/A10：节点级科学事实并入 manifest",
    "log_artifacts": "docs/engineering/LOG_AND_ERROR_CONTRACT.md §4（每次运行必填）",
    "budget_alloc": "docs/engineering/THREAD_BUDGET_ARCH.md（分配快照）",
    "storage": "R-42/P-181：docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md §10.3（运行级形态事实；"
               "机器事实源 hips_storage_form.schema.json#/$defs.manifest_storage）",
}


def _load_validator():
    spec = importlib.util.spec_from_file_location("run_manifest_jsonschema_min", VALIDATOR)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _check_cli003(man: dict) -> list:
    """返回问题列表（空 = 通过）。"""
    problems = []
    for key in CLI003_REQUIRED:
        if key not in man:
            problems.append("missing required (CLI-003 §2): %s" % key)
    for key in man:
        if key not in CLI003_REQUIRED and key not in REGISTERED_ADDITIVE:
            problems.append("unregistered top-level key: %s" % key)
    if man.get("kind") != "astrocs_run_manifest":
        problems.append("kind must be astrocs_run_manifest, got %r" % (man.get("kind"),))
    if man.get("status") not in ("complete", "incomplete"):
        problems.append("status must be complete|incomplete, got %r" % (man.get("status"),))
    if not isinstance(man.get("run_id"), str) or not man.get("run_id"):
        problems.append("run_id must be a non-empty string")
    for key in ("schema_version", "astrocs_version", "config_path", "config_sha256",
                "started_utc", "finished_utc"):
        if key in man and not isinstance(man[key], str):
            problems.append("%s must be a string" % key)
    for key in ("cpu_profile_path", "cpu_profile_sha256"):
        if key in man and not (man[key] is None or isinstance(man[key], str)):
            problems.append("%s must be string|null" % key)
    plat = man.get("platform")
    if not isinstance(plat, dict) or "os" not in plat or "arch" not in plat:
        problems.append("platform must be an object with os/arch")
    ph = man.get("phases")
    if not isinstance(ph, list) or any(not isinstance(x, int) or isinstance(x, bool) for x in ph):
        problems.append("phases must be a list of integers")
    arts = man.get("artifacts")
    if not isinstance(arts, list):
        problems.append("artifacts must be a list")
    else:
        for i, a in enumerate(arts):
            if not isinstance(a, dict):
                problems.append("artifacts[%d] must be an object" % i)
                continue
            # CLI-003 §3 verify 实际消费 path/sha256/size；role 出现在 §2 的示例里，
            # 但实测覆盖极低（见报告 artifacts_role_coverage），故按「有则必合法」
            # 处理并把它作为可见统计量报出，不静默放过。
            for f in ("path", "sha256"):
                if not isinstance(a.get(f), str) or not a.get(f):
                    problems.append("artifacts[%d].%s must be a non-empty string" % (i, f))
            if "role" in a and (not isinstance(a["role"], str) or not a["role"]):
                problems.append("artifacts[%d].role must be a non-empty string when present" % i)
            if "size_bytes" in a and not isinstance(a["size_bytes"], int):
                problems.append("artifacts[%d].size_bytes must be an integer when present" % i)
    prov = man.get("provenance")
    if not isinstance(prov, dict):
        problems.append("provenance must be an object")
    else:
        for key in PROVENANCE_REQUIRED:
            if key not in prov:
                problems.append("provenance missing required (CLI-003 §2.1): %s" % key)
        if "source_sha" in prov and not (isinstance(prov["source_sha"], str)
                                         and HEX40.match(prov["source_sha"])):
            problems.append("provenance.source_sha must be 40-hex")
        for key in ("algorithm_ids", "module_build_ids", "providers", "units",
                    "coordinate_frames", "input_product_hashes", "output_product_hashes"):
            if key in prov and not isinstance(prov[key], list):
                problems.append("provenance.%s must be a list" % key)
    return problems


def _cfg001_deviation(man: dict, schema: dict, validator) -> dict:
    """返回 {missing_required:[...], extra_properties:[...]}（仅顶层）。"""
    # jsonschema_min 的签名是 validate(instance, schema)（与 cfg_common.validate 相反）
    errs = validator.validate(man, schema)
    missing, extra = [], []
    for path, msg in errs:
        quoted = re.findall(r"'([^']+)'", msg)
        if not path and msg.startswith("required") and quoted:
            missing.append(quoted[0])
        elif len(path) == 1 and msg.startswith("additionalProperties") and quoted:
            extra.append(quoted[0])
    return {"missing_required": sorted(set(missing)), "extra_properties": sorted(set(extra))}


def _nonproduct_reason(p: pathlib.Path, corpus_roots: set) -> str:
    """该 manifest 是否落在**源树复制体**里 -> 剔除理由（空串 = 产物语料，参与 T1/T2）。

    按属性判定（见 SOURCE_COPY_SIGNATURES 处的说明）：自文件所在目录向上走，
    在语料根处截止；任一祖先目录命中源树/构建根签名即判非产物语料。目录名不参与判定。
    """
    anc = p.parent
    while True:
        if anc in corpus_roots:
            return ""
        hits = _source_copy_signatures(anc)
        if hits:
            return "SOURCE_COPY_ROOT ancestor=%s signatures=%s" % (anc, "+".join(hits))
        nxt = anc.parent
        if nxt == anc:
            return ""
        anc = nxt


def _discover(extra_dirs=None, use_default=True) -> tuple:
    """返回 (语料, 被剔除的快照命中, 产物语料条数)。

    产物语料 = 来自 run/**、artifacts/** 的**未剔除**命中（fixture 与 --corpus-dir 不算）；
    它是「陈旧偏差是否构成证据」的判据面：没有产物语料就没有可陈旧的载体。
    """
    cands, product_cands, roots = [], set(), set()
    if use_default:
        for base in ("run", "artifacts"):
            root = REPO / base
            if root.is_dir():
                hits = sorted(root.rglob("astrocs_run_*.json"))
                cands += hits
                product_cands |= {str(h) for h in hits}
                roots.add(root)
        if FIXTURES.is_dir():
            cands += sorted(FIXTURES.glob("*.json"))
            roots.add(FIXTURES)
    for d in extra_dirs or []:
        p = pathlib.Path(d)
        if p.is_dir():
            cands += sorted(p.rglob("astrocs_run_*.json")) + sorted(p.rglob("*.json"))
            roots.add(p)
    kept, pruned = [], []
    for f in sorted(set(cands)):
        why = _nonproduct_reason(f, roots)
        (pruned.append((f, why)) if why else kept.append(f))
    n_products = sum(1 for f in kept if str(f) in product_cands)
    return kept, pruned, n_products


def run(json_out: str = "", corpus_dirs=None, use_default_corpus=True,
        assume_product_corpus: bool = False) -> int:
    """assume_product_corpus：**只加严**的自测接缝（强制「产物语料在场」语义，使陈旧即致命），
    用来自证棘轮在产物语料缺席时不会退化成永不判红；发布/CI 调用不得置位。"""
    validator = _load_validator()
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
    reg_missing = set(ledger["registered_missing_required"])
    reg_extra = set(ledger["registered_extra_properties"])

    files, pruned_snapshots, n_products = _discover(corpus_dirs, use_default_corpus)
    if not files:
        print("RUN-MANIFEST-SCHEMA_FAIL: no manifest corpus found (run/**, artifacts/**, fixtures)",
              file=sys.stderr)
        return 2

    t1_bad, measured_missing, measured_extra = {}, set(), set()
    skipped = {}
    n_manifest = 0
    n_artifacts = 0
    n_artifacts_with_role = 0
    for f in files:
        try:
            man = json.loads(f.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            skipped[str(f)] = "unparsable: %s" % exc
            continue
        if not isinstance(man, dict):
            skipped[str(f)] = "top-level JSON is not an object"
            continue
        # 非 manifest 文件（如测试残留的空 {} 占位）不计入 T1/T2，但如实登记；
        # 「至少一个真实 manifest」由 git-tracked fixture 语料保证（见 --self-test）。
        if man.get("kind") != "astrocs_run_manifest":
            skipped[str(f)] = "not a run manifest (kind=%r)" % (man.get("kind"),)
            continue
        n_manifest += 1
        for a in man.get("artifacts") or []:
            if isinstance(a, dict):
                n_artifacts += 1
                if a.get("role"):
                    n_artifacts_with_role += 1
        p = _check_cli003(man)
        if p:
            t1_bad[str(f)] = p
        dev = _cfg001_deviation(man, schema, validator)
        measured_missing |= set(dev["missing_required"])
        measured_extra |= set(dev["extra_properties"])

    t2_new_missing = sorted(measured_missing - reg_missing)
    t2_new_extra = sorted(measured_extra - reg_extra)
    t2_stale_missing = sorted(reg_missing - measured_missing)
    t2_stale_extra = sorted(reg_extra - measured_extra)
    # new_* 永远致命；stale_*（登记项已消失）只在**产物语料在场**时致命——干净检出没有
    # run/**、artifacts/** 产物，唯一的真实 manifest 是入库 fixture，它天然不含
    # uncertainty_available 这类只在真实运行里出现的键；把「fixture 里没有」当陈旧证据，
    # 会让同一提交在 CI 与开发机上得出相反结论（非 hermetic 的假红）。缩窄语料（--corpus-dir
    # 负例注入）同样不构成陈旧证据。
    stale_fatal = bool(n_products) or bool(assume_product_corpus)
    t2_ok = not (t2_new_missing or t2_new_extra
                 or (stale_fatal and (t2_stale_missing or t2_stale_extra)))
    t1_ok = not t1_bad and n_manifest >= 1

    report = {
        "schema": "astrocs.run-manifest-schema-check/v1",
        "n_files_scanned": len(files),
        "n_pruned_worktree_snapshot_hits": len(pruned_snapshots),
        "pruned_worktree_snapshot_hits": [str(p) for p, _ in pruned_snapshots[:20]],
        "pruned_reasons": [{"path": str(p), "rule": "source_copy_root", "why": why}
                           for p, why in pruned_snapshots[:20]],
        "prune_rule": "按属性判定（祖先目录的源树/构建根签名 .git / CMakeLists.txt / docs+eng），"
                      "不按目录名字面量枚举；语料根处截止",
        "n_product_corpus_files": n_products,
        "assume_product_corpus": bool(assume_product_corpus),
        "n_manifests": n_manifest,
        "skipped_non_manifest": skipped,
        "artifacts_role_coverage": {"n_artifacts": n_artifacts,
                                    "n_with_role": n_artifacts_with_role,
                                    "note": "CLI-003 §2 示例含 role；实测覆盖极低 ⇒ 作为可见统计量登记，不静默放过"},
        "t1_cli003": {"ok": t1_ok, "offenders": t1_bad,
                      "registered_additive": sorted(REGISTERED_ADDITIVE)},
        "t2_cfg001": {
            "ok": t2_ok,
            "measured_missing_required": sorted(measured_missing),
            "measured_extra_properties": sorted(measured_extra),
            "registered_missing_required": sorted(reg_missing),
            "registered_extra_properties": sorted(reg_extra),
            "new_missing_required": t2_new_missing,
            "new_extra_properties": t2_new_extra,
            "stale_missing_required": t2_stale_missing,
            "stale_extra_properties": t2_stale_extra,
            "stale_fatal": stale_fatal,
            "conflict_note": ledger.get("conflict_note", ""),
        },
        "verdict": "PASS" if (t1_ok and t2_ok) else "FAIL",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("scanned=%d manifests=%d skipped_non_manifest=%d  T1(CLI-003)=%s  T2(CFG-001 ratchet)=%s"
          " pruned_snapshot_hits=%d product_corpus=%d stale_fatal=%s"
          % (len(files), n_manifest, len(skipped), "PASS" if t1_ok else "FAIL",
             "PASS" if t2_ok else "FAIL", len(pruned_snapshots), n_products, stale_fatal))
    for p, why in pruned_snapshots[:5]:
        print("  PRUNED 非产物语料（源树复制体属性命中）: %s  [%s]" % (p, why))
    for f, ps in list(t1_bad.items())[:6]:
        for msg in ps[:6]:
            print("  T1 %s: %s" % (f, msg))
    if not t2_ok:
        for k in ("new_missing_required", "new_extra_properties",
                  "stale_missing_required", "stale_extra_properties"):
            if report["t2_cfg001"][k]:
                print("  T2 %s: %s" % (k, report["t2_cfg001"][k]))
    print("RUN-MANIFEST-SCHEMA_%s" % report["verdict"])
    return 0 if (t1_ok and t2_ok) else 1


def self_test(json_out: str = "") -> int:
    """正例/负例自检（能红能绿）。"""
    validator = _load_validator()
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    fixture = json.loads((FIXTURES / "real_manifest_sample.json").read_text(encoding="utf-8"))
    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))

    cases = []

    def rec(name, expect_ok, problems):
        cases.append({"case": name, "expect_ok": expect_ok, "ok": not problems,
                      "problems": problems[:5]})

    rec("cli003_fixture_clean", True, _check_cli003(fixture))
    m = dict(fixture); m.pop("config_sha256")
    rec("cli003_missing_required", False, _check_cli003(m))
    m = dict(fixture); m["phases"] = "1"
    rec("cli003_wrong_type", False, _check_cli003(m))
    m = dict(fixture); m["brand_new_key"] = 1
    rec("cli003_unregistered_key", False, _check_cli003(m))
    m = dict(fixture); m["kind"] = "astrocs_other"
    rec("cli003_wrong_kind", False, _check_cli003(m))
    m = dict(fixture)
    m["provenance"] = {k: v for k, v in fixture["provenance"].items() if k != "source_sha"}
    rec("cli003_provenance_missing", False, _check_cli003(m))
    m = dict(fixture)
    m["provenance"] = dict(fixture["provenance"]); m["provenance"]["source_sha"] = "zz"
    rec("cli003_provenance_bad_sha", False, _check_cli003(m))

    dev = _cfg001_deviation(fixture, schema, validator)
    # fixture 是单份真实产物；登记集是全语料并集 ⇒ 不变量是「fixture 偏差 ⊆ 登记集」。
    subset = (set(dev["missing_required"]) <= set(ledger["registered_missing_required"])
              and set(dev["extra_properties"]) <= set(ledger["registered_extra_properties"]))
    rec("cfg001_fixture_deviation_subset_of_registered", True,
        [] if subset else ["deviation not covered by ledger: %s" % json.dumps(dev, ensure_ascii=False)])

    conforming = {"manifest_schema": "astrocs.run-manifest/v1", "run_id": "abc",
                  "software_sha": "a" * 40, "config_hash": "b" * 64,
                  "manifest_input_hashes": [{"path": "in.fits", "sha256": "c" * 64}],
                  "manifest_output_hashes": [{"path": "out.fits", "sha256": "d" * 64}],
                  "toolchain_version": "gcc 13.2", "created_utc": "2026-09-22T00:00:00Z"}
    d0 = _cfg001_deviation(conforming, schema, validator)
    rec("cfg001_conforming_instance_clean", True,
        [] if not (d0["missing_required"] or d0["extra_properties"]) else [json.dumps(d0)])
    # 检出类用例：ok 语义 = 「被检出」，expect_ok 恒为 True（判据必须能红）。
    bad = dict(conforming); bad["extra"] = 1
    det = _cfg001_deviation(bad, schema, validator)["extra_properties"]
    rec("cfg001_extra_property_detected", True,
        [] if det else ["extra property NOT detected (gate is blind)"])
    bad2 = dict(conforming); bad2.pop("config_hash")
    det2 = _cfg001_deviation(bad2, schema, validator)["missing_required"]
    rec("cfg001_missing_required_detected", True,
        [] if det2 else ["missing required NOT detected (gate is blind)"])

    # ---- 语料面与棘轮语义（能红能绿；用 --corpus-dir 隔离，不依赖本机 run/** 现状）----
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="run-manifest-selftest-"))
    try:
        def corpus(name, layout, snapshot_roots=()):
            """layout: {相对路径: 文档} ⇒ 建一个隔离语料目录。

            snapshot_roots: 这些相对路径额外建成**源树复制体**（写入 CMakeLists.txt 并建出
            docs/ 与 eng/），用来在**任意目录名**下模拟工作树拷贝——判据必须按属性识别它，
            不能靠名字。
            """
            root = tmp / name
            for rel, doc in layout.items():
                p = root / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
            for rel in snapshot_roots:
                (root / rel / "CMakeLists.txt").write_text("# copied worktree root\n",
                                                           encoding="utf-8")
                (root / rel / "docs").mkdir(parents=True, exist_ok=True)
                (root / rel / "eng" / "tests" / "cli").mkdir(parents=True, exist_ok=True)
            return root

        def judge(name, root, *, assume=False):
            out = tmp / ("%s.report.json" % name)
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = run(str(out), [str(root)], False, assume_product_corpus=assume)
            rep = json.loads(out.read_text(encoding="utf-8")) if out.is_file() else {}
            return rc, rep

        t1_bad_man = dict(fixture)
        t1_bad_man.pop("provenance")          # 缺必填 ⇒ T1 必须红
        # CFG-001 必填齐备的 manifest（隔离 T2：missing_required 为空，只剩 extra 面）
        cfg_ok = dict(fixture, manifest_schema="astrocs.run-manifest/v1",
                      software_sha="a" * 40, config_hash="b" * 64,
                      manifest_input_hashes=[{"path": "in.fits", "sha256": "c" * 64}],
                      manifest_output_hashes=[{"path": "out.fits", "sha256": "d" * 64}],
                      toolchain_version="gcc 13.2", created_utc="2026-09-22T00:00:00Z")
        new_key_man = dict(cfg_ok, zzz_brand_new_prop=1)

        # 正例：源树复制体（run/<轮次>/wsrc/**，现约定名）里的命中被剔除 ⇒ 判绿（且剔除条数可见）
        root = corpus("prune", {"R1/wsrc/eng/tests/cli/astrocs_run_stale.json": t1_bad_man,
                                "R1/out/astrocs_run_ok.json": fixture},
                      snapshot_roots=("R1/wsrc",))
        rc, rep = judge("prune", root)
        problems = []
        if rc != 0:
            problems.append("rc=%s want 0（快照命中应被剔除）" % rc)
        if rep.get("n_pruned_worktree_snapshot_hits") != 1:
            problems.append("n_pruned_worktree_snapshot_hits=%r want 1"
                            % rep.get("n_pruned_worktree_snapshot_hits"))
        if not (rep.get("t1_cli003") or {}).get("ok"):
            problems.append("T1 未绿：%s" % list((rep.get("t1_cli003") or {}).get("offenders", {}))[:1])
        rec("wsrc_snapshot_hits_pruned_green", True, problems)

        # 负例（判据自证的核心）：快照目录**改名**——不叫 wsrc、叫别的名字，源树属性齐备 ⇒
        # 仍必须照样剔掉。这条红了就说明剔除规则又退化成「字面目录名枚举」，换个目录名即复现。
        rc, rep = judge("prune_renamed", corpus(
            "prune_renamed",
            {"R9/evidence_tree_7f3a/eng/tests/cli/astrocs_run_stale.json": t1_bad_man,
             "R9/out/astrocs_run_ok.json": fixture},
            snapshot_roots=("R9/evidence_tree_7f3a",)))
        problems = []
        if rc != 0:
            problems.append("rc=%s want 0（改名后的源树复制体仍应被剔除）" % rc)
        if rep.get("n_pruned_worktree_snapshot_hits") != 1:
            problems.append("n_pruned_worktree_snapshot_hits=%r want 1（剔除规则依赖目录名）"
                            % rep.get("n_pruned_worktree_snapshot_hits"))
        if not any("SOURCE_COPY_ROOT" in (e or {}).get("why", "")
                   for e in rep.get("pruned_reasons") or []):
            problems.append("剔除理由未按属性登记: %s" % rep.get("pruned_reasons"))
        rec("renamed_snapshot_dir_still_pruned", True, problems)

        # 负例：目录名叫 wsrc 但**没有**任何源树属性 ⇒ 名字不构成豁免，必须照常判定
        rc, rep = judge("nameonly", corpus(
            "nameonly", {"R1/wsrc/eng/tests/cli/astrocs_run_bad.json": t1_bad_man}))
        problems = []
        if rc != 1:
            problems.append("rc=%s want 1（目录名不构成豁免）" % rc)
        if rep.get("n_pruned_worktree_snapshot_hits") != 0:
            problems.append("n_pruned_worktree_snapshot_hits=%r want 0（无源树属性却被剔）"
                            % rep.get("n_pruned_worktree_snapshot_hits"))
        rec("snapshot_dir_name_alone_is_not_exemption", True, problems)

        # 负例: 同一份不合规 manifest 落在**产物**目录 ⇒ 必须红（剔除不是普适豁免）
        rc, rep = judge("product", corpus("product",
                                         {"R1/out/astrocs_run_bad.json": t1_bad_man}))
        problems = []
        if rc != 1:
            problems.append("rc=%s want 1（产物面 T1 必须红）" % rc)
        if (rep.get("t1_cli003") or {}).get("ok") is not False:
            problems.append("T1 未红：offenders=%s"
                            % list((rep.get("t1_cli003") or {}).get("offenders", {}))[:1])
        rec("product_manifest_still_red", True, problems)

        # 正例: 无产物语料（干净检出形态）时登记项「陈旧」不致命，但如实登记
        rc, rep = judge("stale", corpus("stale", {"fx/real_manifest_sample.json": fixture}))
        t2 = rep.get("t2_cfg001") or {}
        problems = []
        if rc != 0:
            problems.append("rc=%s want 0（无产物语料不应按陈旧判红）" % rc)
        if t2.get("stale_fatal") is not False:
            problems.append("stale_fatal=%r want False" % t2.get("stale_fatal"))
        if "uncertainty_available" not in (t2.get("stale_extra_properties") or []):
            problems.append("陈旧项未可见登记: %s" % t2.get("stale_extra_properties"))
        rec("stale_nonfatal_without_product_corpus", True, problems)

        # 负例: 同一语料 + 只加严接缝 ⇒ 陈旧必须致命（证明接缝有效、棘轮未退化）
        rc, rep = judge("stale_strict",
                        corpus("stale_strict", {"fx/real_manifest_sample.json": fixture}),
                        assume=True)
        t2 = rep.get("t2_cfg001") or {}
        problems = []
        if rc != 1 or t2.get("stale_fatal") is not True or t2.get("ok") is not False:
            problems.append("rc=%s stale_fatal=%r t2.ok=%r（want 1/True/False）"
                            % (rc, t2.get("stale_fatal"), t2.get("ok")))
        rec("stale_fatal_with_assume_seam", True, problems)

        # 负例: 未登记的新偏差即使无产物语料也恒致命
        rc, rep = judge("newdev", corpus("newdev", {"fx/new.json": new_key_man}))
        t2 = rep.get("t2_cfg001") or {}
        problems = []
        if t2.get("new_extra_properties") != ["zzz_brand_new_prop"]:
            problems.append("new_extra_properties=%r" % t2.get("new_extra_properties"))
        if t2.get("ok") is not False:
            problems.append("T2 未红（新偏差必须致命），stale_fatal=%r" % t2.get("stale_fatal"))
        rec("new_deviation_fatal_without_product_corpus", True, problems)

        # 正例: 登记项真的出现在语料里 ⇒ 不再是陈旧项，判绿
        rc, rep = judge("registered", corpus(
            "registered", {"fx/unc.json": dict(fixture, uncertainty_available=True)}))
        t2 = rep.get("t2_cfg001") or {}
        problems = []
        if rc != 0 or t2.get("ok") is not True:
            problems.append("rc=%s t2.ok=%r（want 0/True）" % (rc, t2.get("ok")))
        if "uncertainty_available" in (t2.get("stale_extra_properties") or []):
            problems.append("登记项已在场却仍记陈旧: %s" % t2.get("stale_extra_properties"))
        rec("registered_deviation_present_green", True, problems)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    ok = all(c["ok"] == c["expect_ok"] for c in cases)
    report = {"schema": "astrocs.run-manifest-schema-selftest/v1", "cases": cases,
              "n_cases": len(cases), "verdict": "PASS" if ok else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %s (expect_ok=%s) %s" % ("PASS" if c["ok"] == c["expect_ok"] else "FAIL",
                                             c["case"], c["expect_ok"], str(c["problems"])[:100]))
    print("RUN-MANIFEST-SCHEMA-SELFTEST_%s cases=%d" % (report["verdict"], len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--corpus-dir", action="append", default=[],
                    help="追加语料目录（负例注入用；可重复）")
    ap.add_argument("--no-default-corpus", action="store_true",
                    help="只用语料目录（负例隔离；正例/发布态不得使用）")
    ap.add_argument("--assume-product-corpus", action="store_true",
                    help="只加严的自测接缝：强制「产物语料在场」（陈旧即致命）；发布态不得使用")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args.json_out)
    return run(args.json_out, args.corpus_dir, not args.no_default_corpus,
               args.assume_product_corpus)


if __name__ == "__main__":
    raise SystemExit(main())
