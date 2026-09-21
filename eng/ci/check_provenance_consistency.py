#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PROVENANCE-CONSISTENCY —— 产品 provenance 自洽门。

防的复发缺口：产品声明 photometry_applied=false 却带非中性 photscal，或
BUNIT=ASTROCS_RELATIVE_FLUX（相对通量）却未应用测光 ⇒ 下游把 ADU 当相对通量
使用（或反之），静默量纲错。

判据（fail-closed）：
  P1 生产侧中性 provenance：module_adapters.cpp 的 p1_phot.json 构造
     photometry_applied=false 时 photscal 必须为 1.0；
  P2 消费侧守卫：hiss_writer.cpp 必须存在
     BUNIT=ASTROCS_RELATIVE_FLUX 且 PHOTAPPL=FALSE 的显式拒绝；
  P3 数据侧：扫描产品 JSON（--products-root，默认 eng/ci/fixtures/provenance +
     存在的 run/RELEASE-04/e2e/evidence），对每个含 provenance 键的记录断言
     photometry_applied=false ⇒ photscal==1.0 且 bunit != ASTROCS_RELATIVE_FLUX；
     photometry_applied=true ⇒ photscal 有限且 > 0；
  P3b 数据侧补盲（P1-PHOT-BROKEN）：schema==DATA-P1-PHOTPROV-001 且
      photometry_applied=true 的记录还必须**自带逐帧标度与拟合证据**：
        · photscales 为非空对象，键数 == n_frames（若给出），值均为有限正数；
        · photoapplied_artifacts 为非空数组，数量与 photscales 一致；
        · photscale_detail 与 photscales 键集一致，逐帧 fitted==true 且
          n_matched >= MIN_FIT_STARS（SCI-PHOT-001 §4 冻结门）。
      动机（实测 RELEASE-02 L4-rebuild/norm_phot）：11/12 板块声明
      applied=true + photscal=1.0，而该 1.0 是 star_matcher NO_DATA 分支返回的
      **占位值**（日志 "匹配+清洗完成: 0 颗, scale=1.000000e+00"）——
      provenance 结构完整但语义为假；原 P3 只查 finite&&>0 ⇒ 判绿（盲区）。
  P4 零记录扫描 / 锚点缺失 ⇒ rc=2（不得空扫描判绿）。

豁免：eng/ci/ledgers/provenance_exceptions.json 的 'provenance:<file>:<path>' 条目。

用法：
  python3 eng/ci/check_provenance_consistency.py [--repo ROOT] [--products-root DIR]... \
      [--json-out F] [--self-test]
exit 0 = 自洽或已登记；exit 1 = 有 finding；exit 2 = 锚点/台账不可用。
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-PROVENANCE-CONSISTENCY"
ADAPTERS = "lib/infrastructure/scheduler/src/module_adapters.cpp"
HISS_WRITER = "lib/infrastructure/aio/src/hiss_writer.cpp"
LEDGER = "eng/ci/ledgers/provenance_exceptions.json"
DEFAULT_ROOTS = ("eng/ci/fixtures/provenance", "run/RELEASE-04/e2e/evidence", "run/RELEASE-04")
PRODUCT_GLOBS = ("**/p1_phot.json", "**/manifest.json", "**/*product*.json",
                 "**/*provenance*.json")
MAX_FILES = 4000
RELATIVE_FLUX = "ASTROCS_RELATIVE_FLUX"
PHOTPROV_SCHEMA = "DATA-P1-PHOTPROV-001"
MIN_FIT_STARS = 3  # SCI-PHOT-001 §4 冻结门 |r_consistent| >= 3


def _neutral_photscal(adapters: str):
    """抽取「未应用测光」分支的 photscal 字面量；支持三种生产写法。"""
    # A: {"photometry_applied", false}, {"photscal", <lit>}
    m = re.search(r'"photometry_applied"\s*,\s*false\s*\}\s*,\s*\{?\s*'
                  r'"photscal"\s*,\s*([0-9.eE+\-]+)', adapters)
    if m:
        return m.group(1)
    # B: {"photometry_applied", X}, {"photscal", X ? Y : <lit>}
    m = re.search(r'"photometry_applied"\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\}\s*,\s*'
                  r'\{?\s*"photscal"\s*,\s*\1\s*\?\s*[^:\n,}]+\s*:\s*([0-9.eE+\-]+)',
                  adapters)
    if m:
        return m.group(2)
    # C: (*man)["photometry_applied"] = X; ... ["photscal"] = X ? Y : <lit>;
    m = re.search(r'\["photometry_applied"\]\s*=\s*([A-Za-z_][A-Za-z0-9_]*)\s*;'
                  r'[^\n]*\n[^\n]*\["photscal"\]\s*=\s*\1\s*\?\s*[^:\n]+\s*:\s*'
                  r'([0-9.eE+\-]+)', adapters)
    if m:
        return m.group(2)
    return None


def check_source_invariants(repo: pathlib.Path):
    findings = []
    adapters = gc.read_text(repo / ADAPTERS, ADAPTERS)
    neutral = _neutral_photscal(adapters)
    if neutral is None:
        findings.append("provenance_producer_anchor_missing:%s（未找到 photometry_applied "
                        "与 photscal 中性分支的配对；生产写法已变更，需重新登记锚点）" % ADAPTERS)
    elif abs(float(neutral) - 1.0) > 1e-12:
        findings.append("provenance_producer_neutral_photscal:%s（未应用测光分支却写 "
                        "photscal=%s）" % (neutral, neutral))
    writer = gc.read_text(repo / HISS_WRITER, HISS_WRITER)
    if not re.search(r"is_relative_flux\s*&&\s*!\s*photappl", writer):
        findings.append("provenance_writer_guard_missing:%s（缺 BUNIT=RELATIVE_FLUX 且 "
                        "PHOTAPPL=FALSE 的拒绝）" % HISS_WRITER)
    if "HISS_ERR_INVALID_STATE" not in writer:
        findings.append("provenance_writer_error_code_missing:%s" % HISS_WRITER)
    return findings


def _iter_products(root: pathlib.Path):
    seen = set()
    for pattern in PRODUCT_GLOBS:
        for path in root.glob(pattern):
            if path.is_file() and path not in seen:
                seen.add(path)
                yield path


def _walk_records(node, path, out):
    if isinstance(node, dict):
        keys = {"photometry_applied", "photappl", "photscal", "bunit"}
        if keys & set(node):
            out.append((path, node))
        for key, value in node.items():
            _walk_records(value, "%s/%s" % (path, key), out)
    elif isinstance(node, list):
        for i, value in enumerate(node):
            _walk_records(value, "%s[%d]" % (path, i), out)


def _applied(rec):
    if "photometry_applied" in rec:
        return bool(rec["photometry_applied"])
    if "photappl" in rec:
        try:
            return int(rec["photappl"]) != 0
        except (TypeError, ValueError):
            return None
    return None


def _is_finite_positive_number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        v = float(value)
    except (TypeError, ValueError):
        return False
    return math.isfinite(v) and v > 0.0


def _photprov_applied_problems(rec):
    """P3b：DATA-P1-PHOTPROV-001 且 applied=true 时的逐帧标度/拟合证据自洽判据。

    防的复发缺口（实测 RELEASE-02 L4-rebuild/norm_phot）：11/12 板块
    photometry_applied=true + photscal=1.0，而该 1.0 是 star_matcher 在
    NO_DATA 分支（无匹配 / |r_consistent|<3）返回的**占位值**，不是拟合标度。
    结构上 provenance 完整，语义上「已应用」为假 ⇒ 原 P3（只查 finite&&>0）
    判绿。本判据要求 applied=true 必须自带逐帧标度、逐帧产物与逐帧拟合证据。
    """
    problems = []
    scales = rec.get("photscales")
    if not isinstance(scales, dict) or not scales:
        problems.append("photometry_applied=true 但 photscales 缺失/为空"
                        "（逐帧标度是 applied 的必要条件）")
    else:
        n_frames = rec.get("n_frames")
        if isinstance(n_frames, int) and not isinstance(n_frames, bool) and n_frames > 0 \
                and len(scales) != n_frames:
            problems.append("photometry_applied=true 但 photscales 覆盖 %d/%d 帧"
                            % (len(scales), n_frames))
        for key, value in scales.items():
            if not _is_finite_positive_number(value):
                problems.append("photscales[%s]=%r 非有限正数" % (key, value))
    artifacts = rec.get("photoapplied_artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        problems.append("photometry_applied=true 但 photoapplied_artifacts 缺失/为空")
    elif isinstance(scales, dict) and len(artifacts) != len(scales):
        problems.append("photoapplied_artifacts(%d) 与 photscales(%d) 数量不一致"
                        % (len(artifacts), len(scales)))
    detail = rec.get("photscale_detail")
    if not isinstance(detail, dict) or not detail:
        problems.append("photometry_applied=true 但 photscale_detail 缺失/为空"
                        "（无法区分真实拟合标度与 NO_DATA 占位 1.0）")
    else:
        if isinstance(scales, dict) and set(detail) != set(scales):
            problems.append("photscale_detail 键集与 photscales 不一致")
        for key, item in detail.items():
            if not isinstance(item, dict):
                problems.append("photscale_detail[%s] 非对象" % key)
                continue
            if item.get("fitted") is not True:
                problems.append("photscale_detail[%s].fitted != true"
                                "（占位/无证据标度不得声明为已应用）" % key)
            n_matched = item.get("n_matched")
            if isinstance(n_matched, bool) or not isinstance(n_matched, int) \
                    or n_matched < MIN_FIT_STARS:
                problems.append("photscale_detail[%s].n_matched=%r < %d"
                                "（SCI-PHOT-001 §4 冻结门）" % (key, n_matched, MIN_FIT_STARS))
            if isinstance(scales, dict) and key in scales:
                k_detail = item.get("k_photo")
                if not _is_finite_positive_number(k_detail) or \
                        abs(float(k_detail) - float(scales[key])) > 0.0:
                    problems.append("photscale_detail[%s].k_photo=%r 与 photscales 不一致"
                                    % (key, k_detail))
    return problems


def scan_products(repo: pathlib.Path, roots):
    findings = []
    record_count = 0
    scanned_files = 0
    for root in roots:
        root = pathlib.Path(root)
        base = root if root.is_absolute() else repo / root
        if not base.is_dir():
            continue
        for path in sorted(_iter_products(base))[:MAX_FILES]:
            rel = path.relative_to(repo).as_posix() if path.is_relative_to(repo) else str(path)
            try:
                doc = json.loads(path.read_text(encoding="utf-8"))
            except Exception:  # noqa: BLE001 - 非 JSON 产品跳过（不计入 record）
                continue
            scanned_files += 1
            records = []
            _walk_records(doc, "", records)
            for rec_path, rec in records:
                record_count += 1
                applied = _applied(rec)
                photscal = rec.get("photscal")
                bunit = rec.get("bunit")
                problems = []
                if applied is False:
                    if photscal is not None:
                        try:
                            if abs(float(photscal) - 1.0) > 1e-12:
                                problems.append("photometry_applied=false 但 photscal=%s" % photscal)
                        except (TypeError, ValueError):
                            problems.append("photscal 非数值: %r" % (photscal,))
                    if bunit is not None and str(bunit).upper() == RELATIVE_FLUX:
                        problems.append("photometry_applied=false 但 bunit=%s" % bunit)
                elif applied is True:
                    if photscal is not None:
                        try:
                            value = float(photscal)
                            if not math.isfinite(value) or value <= 0.0:
                                problems.append("photometry_applied=true 但 photscal=%s" % photscal)
                        except (TypeError, ValueError):
                            problems.append("photscal 非数值: %r" % (photscal,))
                    if rec.get("schema") == PHOTPROV_SCHEMA:
                        problems.extend(_photprov_applied_problems(rec))
                if not problems:
                    continue
                for problem in problems:
                    findings.append("provenance:%s:%s %s" % (rel, rec_path or "/", problem))
    return findings, record_count, scanned_files


def evaluate(repo: pathlib.Path, roots=None):
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    findings = check_source_invariants(repo)
    roots = roots or DEFAULT_ROOTS
    data_findings, record_count, scanned_files = scan_products(repo, roots)
    if record_count == 0:
        raise gc.GateError("ANCHOR_STALE: 零 provenance 记录（扫描文件 %d，roots=%s）"
                           "（不得空扫描判绿）" % (scanned_files, list(roots)))
    for item in data_findings:
        parts = item.split(":", 2)
        key = "provenance:%s:%s" % (parts[1], parts[2].split(" ")[0]) if len(parts) >= 3 else None
        if key and key in ledger:
            continue
        findings.append(item)
    return findings, {"scanned_files": scanned_files, "record_count": record_count,
                      "roots": [str(r) for r in roots]}


# --------------------------------------------------------------------------- selftest ----
_GOOD_NEUTRAL = {"schema": "DATA-P1-PHOTPROV-001", "photometry_applied": False,
                 "photscal": 1.0, "bunit": "ADU"}
_GOOD_APPLIED = {"photometry_applied": True, "photscal": 1.23, "bunit": RELATIVE_FLUX}
_BAD_NEUTRAL = {"photometry_applied": False, "photscal": 2.5, "bunit": "ADU"}
_BAD_BUNIT = {"photometry_applied": False, "photscal": 1.0, "bunit": RELATIVE_FLUX}
# P3b 夹具（P1-PHOT-BROKEN）: 真实拟合标度（6.27e-17 量级, SCI-PHOT-001 §3 单位
# [F_syn 单位]/ADU）必须判绿; NO_DATA 占位 1.0 / 无拟合证据必须判红。
_GOOD_PHOTPROV = {
    "schema": PHOTPROV_SCHEMA, "photometry_applied": True, "photscal": 6.272202992543341e-17,
    "n_frames": 2,
    "photscales": {"a": 6.272202992543341e-17, "b": 5.685037392078662e-17},
    "photoapplied_artifacts": ["/p/a.fits", "/p/b.fits"],
    "photscale_detail": {
        "a": {"k_photo": 6.272202992543341e-17, "n_matched": 939, "fitted": True},
        "b": {"k_photo": 5.685037392078662e-17, "n_matched": 917, "fitted": True}},
}
_BAD_PHOTPROV_PLACEHOLDER = {
    "schema": PHOTPROV_SCHEMA, "photometry_applied": True, "photscal": 1.0,
    "n_frames": 2, "photscales": {"a": 1.0, "b": 1.0},
    "photoapplied_artifacts": ["/p/a.fits", "/p/b.fits"],
}
_BAD_PHOTPROV_NOFIT = {
    "schema": PHOTPROV_SCHEMA, "photometry_applied": True, "photscal": 1.0,
    "n_frames": 1, "photscales": {"a": 1.0}, "photoapplied_artifacts": ["/p/a.fits"],
    "photscale_detail": {"a": {"k_photo": 1.0, "n_matched": 0, "fitted": False}},
}


def _write_fixture(root: pathlib.Path, records, entries=None, source_ok=True):
    import json
    (root / "lib/infrastructure/scheduler/src").mkdir(parents=True, exist_ok=True)
    (root / "lib/infrastructure/aio/src").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/fixtures/provenance").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    adapters = ('{"photometry_applied", false}, {"photscal", 1.0},'
                if source_ok else '{"photometry_applied", false}, {"photscal", 2.5},')
    (root / ADAPTERS).write_text(adapters, encoding="utf-8")
    (root / HISS_WRITER).write_text("if (is_relative_flux && !photappl) return -2; "
                                    "// HISS_ERR_INVALID_STATE\n", encoding="utf-8")
    (root / "eng/ci/fixtures/provenance/prod_product.json").write_text(json.dumps(records), encoding="utf-8")
    (root / LEDGER).write_text(json.dumps(
        {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": list(entries or [])}),
        encoding="utf-8")


def _selftest() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok, [_GOOD_NEUTRAL, _GOOD_APPLIED])
        cases.append(("green_consistent", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, [_BAD_NEUTRAL, _BAD_BUNIT])
        cases.append(("red_photscal_and_bunit", True, d_bad))
        d_src = base / "badsrc"
        _write_fixture(d_src, [_GOOD_NEUTRAL], source_ok=False)
        cases.append(("red_producer_neutral_photscal", True, d_src))
        d_pb = base / "photprov"
        _write_fixture(d_pb, [_GOOD_PHOTPROV, _GOOD_NEUTRAL])
        cases.append(("green_photprov_fitted", False, d_pb))
        d_pb_red = base / "photprov_red"
        _write_fixture(d_pb_red, [_BAD_PHOTPROV_PLACEHOLDER])
        cases.append(("red_photprov_no_detail", True, d_pb_red))
        d_pb_red2 = base / "photprov_red2"
        _write_fixture(d_pb_red2, [_BAD_PHOTPROV_NOFIT])
        cases.append(("red_photprov_nofit", True, d_pb_red2))
        d_led = base / "ledgered"
        _write_fixture(d_led, [_BAD_NEUTRAL, _GOOD_APPLIED],
                       entries=[{"id": "provenance:eng/ci/fixtures/provenance/prod_product.json:[0]",
                                 "kind": "known_divergence", "reason": "夹具",
                                 "owner": "fixture", "exit_condition": "夹具"}])
        cases.append(("green_ledgered", False, d_led))
        rc = gc.selftest_main(cases, lambda repo: evaluate(repo)[0])
        d_empty = base / "empty"
        _write_fixture(d_empty, [])
        try:
            evaluate(d_empty)
            failures.append("zero_records_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS zero_records (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-PROVENANCE-CONSISTENCY 产品 provenance 自洽门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--products-root", action="append", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo, args.products_root)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
        return 1
    print("%s_PASS records=%d files=%d roots=%s"
          % (CHECK_ID, extra["record_count"], extra["scanned_files"], extra["roots"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())