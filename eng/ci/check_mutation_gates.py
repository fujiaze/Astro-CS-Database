#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-MUTATION-GATES｜eng/ci/mutation_gates.json 的「登记项必须仍有对象」门。

权威依据
  - ENGINEERING_SPEC.md §10「检查器在输入缺失、路径不存在、依赖不可用时判红；
    『文件不存在』按『无违规』通过视为假绿」；
  - docs/ci/01_CHECKS.md §1「锚存活」（硬编码引用的仓库路径必须存在，失效时显式
    失败并点名）与「fail-closed」（scanned == 0 ⇒ rc != 0）；
  - AGENTS.md §9「门禁/判据本身不合理时改进门禁本身」。

为什么另立本门（根因，一手实测）
  eng/ci/mutation_gates.json 是「近似 ↔ mutation 证明门」的人工登记册，其
  open_items 第 2 条自述「建立『近似 ↔ mutation 门』双向登记并由机器门校验
  （本文件是人工登记，尚无 checker）」。实测（run/DEFECT-REPRO-01）：
    * 全仓**零机器消费方**（grep 命中只在 run/ 与历史审计件里）⇒ 孤儿登记；
    * design_claim 引的 "ASTROCS_DESIGN.md §11.1:565「每个近似有 mutation 证明门
      能红」" 在现行设计文档里**整条不存在**：ASTROCS_DESIGN.md 共 770 行，§11 是
      「双平台发行与安装」（:651），全文零 "mutation" ⇒ 锚已死；
    * 21 条路径形态 token 中 10 条 GONE（run/v6/** 已被 eng/tools/run_gc.py 回收；
      eng/tests/contracts/product_family/evidence/mutations.json 从未入库）。
  孤儿登记 + 失效登记项无人知 ⇒ 本门把「登记项必须仍有对象」变成判据。

判据（任一 A 违规 ⇒ exit 1）
  A1 结构     顶层 schema_version/purpose/gates 齐备；gates 为非空数组；每 gate 的
              id 非空且唯一、approximation 非空、in_ctest/default_on 为布尔。
  A2 锚存活   design_claim 必须是可解析、可复核的引用：
              <doc>[ §<sec>][:<line>]「<claim>」
                - doc 必须存在；§<sec> 必须是 doc 里真实存在的标题号；:<line> 必须在
                  行数内且非空；「<claim>」的文本必须在该 doc 中出现。
              任一不成立 ⇒ MUTATION_GATE_CLAIM_STALE 点名（不得静默降级）。
  A3 登记项仍有对象  每条 gate 的结构化产物字段（driver / registration / evidence）
              里的路径形态 token 必须存在；不存在时必须在同一 gate 的
              gone_artifacts 映射里显式登记（path -> 非空 reason）。
              棘轮（只减不增）：gone_artifacts 的每个 key 必须①被该 gate 的
              driver/registration/evidence 引用，且②确实不存在；产物回来了 ⇒ 判红
              （GONE_ARTIFACT_OBSOLETE，应删除该登记）。与
              eng/ci/path_domain_reservations.json（A3）、
              eng/ci/ledgers/run_manifest_schema_deviations.json 同款。
  A4 扫描面   gates 条目数为 0 ⇒ 判红（fail-closed：空扫描不得判绿）。
  A5 输入     登记册缺失 / 不可解析 ⇒ exit 2 并点名（MUTATION_GATES_INPUT_UNAVAILABLE），
              不得 traceback。

消费面（报告项，不判红）
  JSON 输出带 consumers：仓库内引用本登记册的文件清单。当前为 0（孤儿登记）；
  本门经 eng/ci/checks.json 注册后即成为其机器消费方。

只读；仅 stdlib；无网络。
用法:
  python3 eng/ci/check_mutation_gates.py [--registry eng/ci/mutation_gates.json] [--json-out F]
  python3 eng/ci/check_mutation_gates.py --self-test
exit 0 = 全绿；1 = 判据命中；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_REGISTRY = os.path.join("eng", "ci", "mutation_gates.json")

# 结构化产物字段（只判这三处；reason/note 等散文里的路径不作判据，见 A3 说明）
ARTIFACT_FIELDS = ("driver", "registration", "evidence")
# 路径形态 token（首段必须是仓库顶层目录名，避免把 URL/命令当路径）
PATH_RE = re.compile(
    r"(?:^|[\s(（\[])((?:eng|lib|docs|run|artifacts|testdata|实验|工程控制)"
    r"/[A-Za-z0-9_./\u4e00-\u9fff-]+)")
# design_claim 形态: <doc>[ §<sec>][:<line>]「<claim>」
CLAIM_RE = re.compile(
    r"^\s*(?P<doc>[^\s§:]+)"
    r"(?:\s*§\s*(?P<sec>[^\s:]+))?"
    r"(?::(?P<line>\d+))?\s*"
    r"「(?P<claim>[^」]+)」\s*$")


def _paths_in(value: str) -> list[str]:
    out = []
    for m in PATH_RE.finditer(value or ""):
        out.append(m.group(1).rstrip(".,;，。、"))
    return out


def _headings(text: str) -> list[str]:
    return [m.group(1).strip() for m in
            re.finditer(r"^\s{0,3}#{1,6}\s+(.+?)\s*$", text, re.M)]


def check_claim(repo: str, claim: str) -> list[str]:
    """A2：design_claim 锚存活。返回点名列表（空 = 存活）。"""
    m = CLAIM_RE.match(claim or "")
    if not m:
        return ["MUTATION_GATE_CLAIM_STALE design_claim 形态不可解析（期望 "
                "<doc>[ §<sec>][:<line>]「<claim>」）: %r" % (claim,)]
    doc_rel = m.group("doc")
    sec = m.group("sec")
    line_no = m.group("line")
    want = m.group("claim")
    problems: list[str] = []
    full = os.path.join(repo, doc_rel)
    if not os.path.isfile(full):
        return ["MUTATION_GATE_CLAIM_STALE design_claim 引用的文档不存在: %s" % doc_rel]
    try:
        with open(full, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as exc:
        return ["MUTATION_GATE_CLAIM_STALE design_claim 文档不可读 %s: %s" % (doc_rel, exc)]
    lines = text.splitlines()
    if sec:
        # 取每个标题的前导节号（如 "## 10. 机器一致性检查" -> "10"、"### 11.1 变异" -> "11.1"）
        # 与 §<sec> 精确比较；不能用"前缀包含"，否则 §10 会被 §10.5 蒙混过关。
        nums = set()
        for ln in lines:
            m2 = re.match(r"^\s{0,3}#{1,6}\s*([0-9]+(?:\.[0-9]+)*)", ln)
            if m2:
                nums.add(m2.group(1))
        if sec not in nums:
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的章节不存在: "
                            "%s §%s（该文档标题里没有此节号）" % (doc_rel, sec))
    if line_no:
        n = int(line_no)
        if n < 1 or n > len(lines):
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的行号越界: "
                            "%s:%d（文档共 %d 行）" % (doc_rel, n, len(lines)))
        elif not lines[n - 1].strip():
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的行是空行: "
                            "%s:%d" % (doc_rel, n))
    if want and want not in text:
        problems.append("MUTATION_GATE_CLAIM_STALE design_claim 的引文在该文档中不存在: "
                        "%s 内找不到「%s」" % (doc_rel, want[:60]))
    return problems


def check_registry(repo: str, doc: dict) -> tuple[list[str], dict]:
    """A1/A2/A3/A4。返回 (problems, info)。"""
    problems: list[str] = []
    info: dict = {"gates": 0, "artifacts_checked": 0, "gone_registered": 0}
    if not isinstance(doc, dict):
        return ["A1 登记册顶层必须是对象"], info
    for key in ("schema_version", "purpose", "gates"):
        if key not in doc:
            problems.append("A1 顶层缺必需键: %s" % key)
    gates = doc.get("gates")
    if not isinstance(gates, list):
        return problems + ["A1 gates 必须是数组"], info
    if not gates:
        return problems + ["A4 gates 为空（fail-closed：空扫描不得判绿）"], info
    info["gates"] = len(gates)

    if "design_claim" in doc:
        problems += check_claim(repo, str(doc.get("design_claim")))

    seen_ids: set = set()
    for idx, g in enumerate(gates):
        where = "gates[%d]" % idx
        if not isinstance(g, dict):
            problems.append("A1 %s 必须是对象" % where)
            continue
        gid = str(g.get("id") or "").strip()
        if not gid:
            problems.append("A1 %s 缺 id" % where)
        elif gid in seen_ids:
            problems.append("A1 gate id 重复: %s" % gid)
        else:
            seen_ids.add(gid)
        where = "gates[%d](%s)" % (idx, gid or "?")
        if not str(g.get("approximation") or "").strip():
            problems.append("A1 %s 缺 approximation（该门守什么近似）" % where)
        for bk in ("in_ctest", "default_on"):
            if not isinstance(g.get(bk), bool):
                problems.append("A1 %s 的 %s 必须是布尔" % (where, bk))
        gone = g.get("gone_artifacts") or {}
        if not isinstance(gone, dict):
            problems.append("A1 %s 的 gone_artifacts 必须是对象（path -> reason）" % where)
            gone = {}
        referenced: set = set()
        for field in ARTIFACT_FIELDS:
            val = g.get(field)
            if not isinstance(val, str):
                continue
            for tok in _paths_in(val):
                referenced.add(tok)
                info["artifacts_checked"] += 1
                if os.path.exists(os.path.join(repo, tok)):
                    continue
                if tok in gone and str(gone.get(tok) or "").strip():
                    info["gone_registered"] += 1
                    continue
                problems.append(
                    "MUTATION_GATE_ARTIFACT_GONE %s 的 %s 指向不存在的产物 %s"
                    "（登记项已无对象：删除该登记，或在 gone_artifacts 里显式登记"
                    " path -> 原因）" % (where, field, tok))
        # 棘轮：gone_artifacts 的 key 必须仍被引用且确实不存在
        for tok, reason in gone.items():
            if not str(reason or "").strip():
                problems.append("A1 %s 的 gone_artifacts[%s] 缺原因" % (where, tok))
            if tok not in referenced:
                problems.append(
                    "MUTATION_GATE_GONE_UNREFERENCED %s 的 gone_artifacts[%s] 不再被"
                    " driver/registration/evidence 引用（登记项失效，须删除）" % (where, tok))
            elif os.path.exists(os.path.join(repo, tok)):
                problems.append(
                    "MUTATION_GATE_GONE_OBSOLETE %s 的 gone_artifacts[%s] 指向的产物已"
                    "重新存在（棘轮只减不增，须删除该登记）" % (where, tok))
    return problems, info


# 消费方扫描面：只扫工程/文档/源码面，避开只读数据集（gaia/、testdata/）与产物区，
# 否则 walk 会拖到分钟级（实测 gaia/ 体量足以让 300s 超时）。
CONSUMER_SCAN_ROOTS = ("eng", "docs", "lib")
CONSUMER_MAX_BYTES = 2 * 1024 * 1024

# Windows 保留设备名（NUL/CON/PRN/AUX/COM1-9/LPT1-9）。工作树里存在这类条目时，
# ntpath.relpath 会把路径解析成设备名并抛 ValueError
# （path is on mount ...nul, start on mount 'F:'）⇒ 检查器抛 Traceback 而
# **没有 verdict**。门崩与判红必须可区分（GATE-TRIAGE-01）。
RESERVED_DEVICE_RE = re.compile(
    r"^(?:nul|con|prn|aux|com[1-9]|lpt[1-9])(?:\..*)?$", re.IGNORECASE)


def rel_path(repo: str, full: str):
    """相对仓库根的 POSIX 风格路径；不可解析（设备名/跨卷/符号环）返回 None。

    不静默丢弃：None 由调用方登记进 CONSUMER_SCAN_UNRESOLVABLE 报告项。
    """
    try:
        return os.path.relpath(full, repo).replace(os.sep, "/")
    except (ValueError, OSError):
        return None


def find_consumers(repo: str):
    """(消费方, 不可解析路径) 二元组；后者非空即 fail-closed 判红。

    消费方 = 仓库内引用本登记册的文件（限 CONSUMER_SCAN_ROOTS）。
    """
    hits: list[str] = []
    unresolvable: list[str] = []
    skip = {".git", "build", "run", "artifacts", "__pycache__", ".dsh-code-index",
            "gaia", "testdata", "实验", "工程控制"}
    for root in CONSUMER_SCAN_ROOTS:
        base = os.path.join(repo, root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in skip and not RESERVED_DEVICE_RE.match(d)]
            for fn in filenames:
                if RESERVED_DEVICE_RE.match(fn):
                    unresolvable.append(os.path.join(dirpath, fn))
                    continue
                full = os.path.join(dirpath, fn)
                rel = rel_path(repo, full)
                if rel is None:
                    unresolvable.append(full)
                    continue
                if rel == "eng/ci/mutation_gates.json":
                    continue
                try:
                    if os.path.getsize(full) > CONSUMER_MAX_BYTES:
                        continue
                    with open(full, encoding="utf-8", errors="ignore") as fh:
                        if "mutation_gates.json" in fh.read():
                            hits.append(rel)
                except OSError:
                    continue
    return sorted(hits), sorted(unresolvable)


def _self_test() -> int:
    """可执行正/负例面（A1/A2/A3/A4/A5 逐条）。"""
    import shutil
    import tempfile

    problems: list[str] = []
    tmp = tempfile.mkdtemp(prefix="astrocs_mg_")
    try:
        os.makedirs(os.path.join(tmp, "docs"))
        os.makedirs(os.path.join(tmp, "eng", "tests", "x"))
        with open(os.path.join(tmp, "docs", "DESIGN.md"), "w", encoding="utf-8") as fh:
            fh.write("# D\n\n## 11.1 变异\n\n每个近似有 mutation 证明门能红\n")
        with open(os.path.join(tmp, "eng", "tests", "x", "run.py"), "w",
                  encoding="utf-8") as fh:
            fh.write("print(1)\n")

        def doc(claim="docs/DESIGN.md §11.1:5「每个近似有 mutation 证明门能红」",
                driver="eng/tests/x/run.py", gone=None, gid="g1", dup=False,
                gates_empty=False):
            g = {"id": gid, "approximation": "x", "in_ctest": False,
                 "default_on": True, "driver": driver}
            if gone:
                g["gone_artifacts"] = gone
            gs = [g]
            if dup:
                gs.append(dict(g))
            return {"schema_version": 1, "purpose": "p",
                    "design_claim": claim, "gates": [] if gates_empty else gs}

        def case(name, d, want_rc, token=None, raw=None):
            if raw is None:
                p, _info = check_registry(tmp, d)
            else:
                p = raw
            rc = 1 if p else 0
            blob = " ".join(p)
            ok = (rc == want_rc) and (token is None or token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (name, rc, want_rc, "OK" if ok else "MISMATCH", blob[:120]))

        case("pos_live", doc(), 0)
        case("neg_claim_text_absent",
             doc(claim="docs/DESIGN.md §11.1:5「这句话不存在」"), 1,
             "MUTATION_GATE_CLAIM_STALE")
        case("neg_claim_section_absent",
             doc(claim="docs/DESIGN.md §99.9:5「每个近似有 mutation 证明门能红」"), 1,
             "章节不存在")
        case("neg_claim_line_oob",
             doc(claim="docs/DESIGN.md §11.1:9999「每个近似有 mutation 证明门能红」"), 1,
             "行号越界")
        case("neg_claim_doc_absent",
             doc(claim="docs/NOPE.md「x」"), 1, "文档不存在")
        case("neg_artifact_gone_unregistered",
             doc(driver="eng/tests/x/gone.py"), 1, "MUTATION_GATE_ARTIFACT_GONE")
        case("pos_artifact_gone_registered",
             doc(driver="eng/tests/x/gone.py",
                 gone={"eng/tests/x/gone.py": "run/ 一次性脚本，已随 run_gc 回收"}), 0)
        case("neg_gone_obsolete",
             doc(driver="eng/tests/x/run.py",
                 gone={"eng/tests/x/run.py": "误登记"}), 1, "MUTATION_GATE_GONE_OBSOLETE")
        case("neg_gone_unreferenced",
             doc(gone={"eng/tests/x/other.py": "无关"}), 1,
             "MUTATION_GATE_GONE_UNREFERENCED")
        case("neg_dup_id", doc(dup=True), 1, "id 重复")
        case("neg_empty_gates", doc(gates_empty=True), 1, "gates 为空")
        case("neg_missing_required_key",
             {"schema_version": 1, "gates": []}, 1, "顶层缺必需键")

        # A5：登记册缺失/不可解析
        for name, path, want_rc, token in (
                ("neg_input_missing", os.path.join(tmp, "nope.json"), 2, "INPUT_UNAVAILABLE"),
                ("neg_input_bad_json", None, 2, "INPUT_UNAVAILABLE")):
            if path is None:
                path = os.path.join(tmp, "bad.json")
                with open(path, "w", encoding="utf-8") as fh:
                    fh.write("{not json")
            rc, blob = _load_and_check(tmp, path)
            ok = (rc == want_rc) and (token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (name, rc, want_rc, "OK" if ok else "MISMATCH", blob[:120]))

        # A6（GATE-TRIAGE-01 负例）：Windows 保留设备名/跨卷路径不得让门崩。
        # 复现原缺陷：os.walk 在工作树里遇到设备名条目时 ntpath.relpath 抛
        #   ValueError: path is on mount '<device>', start on mount 'F:'
        # 原实现直接 Traceback（rc=1 但**没有 verdict**，与「判红」不可区分）。
        # 本负例在任意平台可跑：monkeypatch relpath 对探针路径抛同样的异常，
        # 断言 ①find_consumers 不抛 ②该路径进 unresolvable（不静默丢弃）。
        probe_dir = os.path.join(tmp, "eng", "probe")
        os.makedirs(probe_dir, exist_ok=True)
        probe = os.path.join(probe_dir, "consumer.py")
        with open(probe, "w", encoding="utf-8") as fh:
            fh.write("# mutation_gates.json\n")
        real_relpath = os.path.relpath
        real_rel_path = globals()["rel_path"]

        def boom(path, start=None):
            if os.path.abspath(str(path)) == os.path.abspath(probe):
                raise ValueError("path is on mount '<device>', start on mount 'F:'")
            return real_relpath(path, start) if start is not None else real_relpath(path)

        os.path.relpath = boom
        try:
            hits, unres = find_consumers(tmp)
            crashed = False
        except Exception:  # noqa: BLE001 - 崩溃即负例失败
            hits, unres, crashed = [], [], True
        finally:
            os.path.relpath = real_relpath
        ok = (not crashed) and any("consumer.py" in u for u in unres)
        problems.append("neg_device_path_no_crash %s | unresolvable=%s"
                        % ("OK" if ok else "MISMATCH", unres[:3]))

        # A6b（判别力自证）：把 rel_path 换回修复前的裸 os.path.relpath，
        # 同一输入必须抛 ValueError —— 证明 A6 不是恒真门。
        def raw_rel_path(repo_, full_):
            return os.path.relpath(full_, repo_).replace(os.sep, "/")

        globals()["rel_path"] = raw_rel_path
        os.path.relpath = boom
        try:
            find_consumers(tmp)
            pre_fix_raised = False
        except ValueError:
            pre_fix_raised = True
        except Exception:  # noqa: BLE001
            pre_fix_raised = False
        finally:
            os.path.relpath = real_relpath
            globals()["rel_path"] = real_rel_path
        problems.append("neg_device_path_pre_fix_raises %s"
                        % ("OK" if pre_fix_raised else "MISMATCH"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    bad = [p for p in problems if "OK" not in p]
    for p in problems:
        print("[selftest] %s" % p)
    print("[selftest] %d cases, %s" % (len(problems), "ALL OK" if not bad else "FAILED"))
    return 0 if not bad else 1


def _load_and_check(repo: str, path: str) -> tuple[int, str]:
    if not os.path.isfile(path):
        return 2, "MUTATION_GATES_INPUT_UNAVAILABLE 登记册缺失: %s" % path
    try:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        return 2, "MUTATION_GATES_INPUT_UNAVAILABLE 登记册不可解析 %s: %s" % (path, exc)
    problems, _info = check_registry(repo, doc)
    return (1 if problems else 0), " ".join(problems)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--registry", default=None,
                    help="登记册路径（默认 eng/ci/mutation_gates.json）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    path = args.registry or os.path.join(REPO, DEFAULT_REGISTRY)
    if not os.path.isfile(path):
        print("MUTATION_GATES_INPUT_UNAVAILABLE 登记册缺失: %s" % path, file=sys.stderr)
        return 2
    try:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        print("MUTATION_GATES_INPUT_UNAVAILABLE 登记册不可解析 %s: %s" % (path, exc),
              file=sys.stderr)
        return 2
    problems, info = check_registry(REPO, doc)
    consumers, unresolvable = find_consumers(REPO)
    if unresolvable:
        # fail-closed：路径归不到仓库根（Windows 保留设备名/跨卷/符号环）时
        # 不静默丢弃，具名判红 —— 否则扫描面被悄悄缩小而门仍报绿。
        problems = problems + [
            "CONSUMER_SCAN_UNRESOLVABLE 消费方扫描面有 %d 条路径无法归到仓库根：%s"
            % (len(unresolvable), unresolvable[:5])]
    out = {
        "checker": "eng/ci/check_mutation_gates.py",
        "registry": os.path.relpath(path, REPO).replace(os.sep, "/"),
        "gates": info["gates"],
        "artifacts_checked": info["artifacts_checked"],
        "gone_registered": info["gone_registered"],
        "consumers": consumers,
        "consumers_unresolvable": unresolvable,
        "problems": problems,
        "verdict": "MUTATION_GATES_RED" if problems else "MUTATION_GATES_OK",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        parent = os.path.dirname(os.path.abspath(args.json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    if problems:
        print("MUTATION_GATES_RED problems=%d" % len(problems))
        for p in problems:
            print("  [%s] %s" % (p.split(" ", 1)[0], p.split(" ", 1)[1] if " " in p else ""))
        print("  消费方（报告项）: %s" % (consumers or "无（孤儿登记）"))
        return 1
    print("MUTATION_GATES_OK gates=%d artifacts=%d gone_registered=%d consumers=%d"
          % (info["gates"], info["artifacts_checked"], info["gone_registered"],
             len(consumers)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
