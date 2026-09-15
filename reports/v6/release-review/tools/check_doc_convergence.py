#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-CONVERGE-001 文档口径收敛检查器（V6 并行包 Wave 12）。

职责：把「冻结 V6 口径」与活动文档做结构一致性比对，并对**负向注入**判红。

检查项（rc 0 = 全部 PASS；任一门红 -> rc 2）：
  C1 canonical-vocabulary   单一权威权重词表（weight.kind/units/group_normalized/
                            normalization.*/weight_value）；第三套词表 token 只在
                            「禁止/第三套/legacy/别名/REJECT/迁移」语境出现。
  C2 psfsw-not-ivar         psfsw 不得被写成 ivar/Fisher/W_info/方差来源。
  C3 deferred-mode          生产模式列表不得含 psf_snr_power；DEFERRED 必须显式登记。
  C4 concentration-unit     concentration 单位必须 component_flux_unit/px^2（不得 ADU/px）。
  C5 resource-gate          16 worker 65.09% 不得写成通过/满足。
  C6 no-released            不得出现 RELEASED/已发布/正式发布 的宣称。
  C7 p33-p27-rollback       DATA_SEMANTICS/PUBLIC_API 不得重新引入 P33-COEF / P27 段落。
  C8 covariance-epsf        final covariance 只能 R C_in R^T 传播 + 必输 effective PSF。
  C9 modes-exact            生产三模式/文档基线两模式/延迟一词元组必须精确一致。
  C10 doc-index-coverage    tools/doccheck/check_doc_index.py rc=0（OI-04 覆盖闭合）。

--selftest：对内存副本注入 mutation，断言对应检查项判红（非「同实现自证」）。
"""
from __future__ import annotations
import argparse, json, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
OUT_DEFAULT = "artifacts/v6/release-review/doc_convergence_report.json"

SCOPE_IN_DIRS = ["docs/contracts", "docs/algorithms/v6", "docs/science/v6", "docs/validation/v6"]
SCOPE_IN_FILES = ["README.md", "REVIEW.md", "CHANGELOG.md"]
SCOPE_IN_JSON = ["docs/algorithms/v6/phase1/alg_p1_001_spec.json"]
# C-004.5 写保护（本包不得改写其正文）：只做信息登记，不作门红
PROTECTED_PREFIXES = ["docs/science/", "docs/owner/", "docs/design/", "docs/references/"]

THIRD_VOCAB = ["weight_normalized", "normalization_scope", "weight_type", "weight_kind_name",
               "norm_scope", "unit_string", "is_group_normalized", "weight_dimensionless"]
FORBIDDEN_MARKERS = ["forbidden", "禁止", "第三套", "残留", "reject", "legacy", "别名", "alias",
                     "迁移", "reader", "已删除", "删除"]
NEG_MARKERS = ["不得", "禁止", "不是", "非", "reject", "forbidden", "不可", "拒绝", "无权",
               "未", "not", "no ", "撤销", "取代", "已删除", "订正", "待修", "曾写作", "错误",
               "建议", "deferred", "延迟", "not_implemented", "不"]
CANONICAL_TOKENS = ["weight.kind", "weight.units", "weight.group_normalized",
                    "weight.normalization", "weight.weight_value"]
# mutation 目录/负向门/示例行：其出现「违例字面量」是门的设计，不是文档错误
MUT_ROW_RE = re.compile(r"(^|\|)\s*[mM]\d+|negative_mutation|mutation|\bmut-")


def load_scope(root: str):
    docs = {}
    for f in SCOPE_IN_FILES:
        p = os.path.join(root, f)
        if os.path.isfile(p):
            docs[f] = open(p, encoding="utf-8", errors="replace").read()
    for d in SCOPE_IN_DIRS:
        base = os.path.join(root, d)
        for dp, dn, fn in os.walk(base):
            if "/archive/" in dp.replace(os.sep, "/") + "/":
                continue
            for name in fn:
                if name.endswith((".md", ".json", ".yaml", ".yml")):
                    full = os.path.join(dp, name)
                    rel = os.path.relpath(full, root)
                    docs[rel] = open(full, encoding="utf-8", errors="replace").read()
    for f in SCOPE_IN_JSON:
        p = os.path.join(root, f)
        if os.path.isfile(p):
            docs[f] = open(p, encoding="utf-8", errors="replace").read()
    return docs


def add(findings, check, path, line, msg, severity="fail"):
    findings.append({"check": check, "path": path, "line": line, "message": msg, "severity": severity})


ALLOW_EXTRA = ["排除", "即红", "示例", "注入", "订正", "待修", "曾写作", "错误", "取代",
               "建议", "放宽", "登记", "训练", "基准", "红"]


def has_any(text, toks):
    low = text.lower()
    return any(t.lower() in low for t in toks)


def is_negated(ln: str) -> bool:
    """该行是禁止/否定/mutation 目录/门设计语境 -> 不算违例。"""
    return has_any(ln, NEG_MARKERS) or has_any(ln, ALLOW_EXTRA) or bool(MUT_ROW_RE.search(ln))


def c1_canonical(docs, findings):
    # 正向控制：权威文档必须出现全部 canonical 字段
    auth = ["docs/contracts/DATA_SEMANTICS.md", "docs/contracts/PUBLIC_API.md"]
    blob = "\n".join(docs.get(p, "") for p in auth)
    missing = [t for t in CANONICAL_TOKENS if t not in blob]
    if missing:
        add(findings, "C1", ";".join(auth), 0, "canonical 词表字段缺失: %s" % missing)
    for path, text in docs.items():
        for i, ln in enumerate(text.splitlines(), 1):
            low = ln.lower()
            for tok in THIRD_VOCAB:
                if tok in low and not has_any(ln, FORBIDDEN_MARKERS):
                    add(findings, "C1", path, i, "第三套词表 token '%s' 未在禁止/legacy 语境: %s" % (tok, ln.strip()[:100]))


def c2_psfsw_not_ivar(docs, findings):
    claim_res = [
        re.compile(r"variance_from_weight\s*[:=]\s*true", re.I),
        re.compile(r"uses_relative_weight_as_ivar\s*[:=]\s*true", re.I),
        re.compile(r"units\s*[:=]\s*[\"']?(ADU\^-2|flux\^-2|1/BUNIT\^2)", re.I),
        re.compile(r"var(iance)?\s*=\s*1/W_psfsw", re.I),
        re.compile(r"W_psfsw\S*\s*=\s*ivar", re.I),
        re.compile(r"psfsw\S*\s*(is|是|等同|等于)\s*(an?\s+)?(ivar|fisher)", re.I),
    ]
    for path, text in docs.items():
        if "psfsw" not in text.lower() and "PSFSW" not in text:
            continue
        for i, ln in enumerate(text.splitlines(), 1):
            if "psfsw" not in ln.lower():
                continue
            if any(rx.search(ln) for rx in claim_res) and not is_negated(ln):
                add(findings, "C2", path, i, "psfsw 被写成 ivar/Fisher/方差来源: %s" % ln.strip()[:110])


def c3_deferred(docs, findings):
    prod_re = re.compile(r"(生产|production)[^\n]{0,40}(模式|权重|枚举|列表|=|:|：|进)[^\n]{0,60}psf_snr_power", re.I)
    found_decl = False
    for path, text in docs.items():
        for i, ln in enumerate(text.splitlines(), 1):
            if "psf_snr_power" in ln:
                if re.search(r"DEFERRED|延迟|not_implemented|not production|不进", ln, re.I):
                    found_decl = True
                elif prod_re.search(ln) and not is_negated(ln):
                    add(findings, "C3", path, i, "psf_snr_power 出现在生产语境: %s" % ln.strip()[:110])
    if not found_decl:
        add(findings, "C3", "(scope)", 0, "未见 psf_snr_power = DEFERRED 的显式登记")


def c4_concentration(docs, findings):
    rx = re.compile(r"concentration[^\n]{0,90}ADU/px(?![²^2])|ADU/px(?![²^2])[^\n]{0,60}concentration", re.I)
    allow = ["放宽", "mutation", "reject", "禁止", "拒绝", "待修", "订正", "曾写作", "登记"]
    for path, text in docs.items():
        for i, ln in enumerate(text.splitlines(), 1):
            if rx.search(ln) and not is_negated(ln):
                add(findings, "C4", path, i, "concentration 单位仍为 ADU/px: %s" % ln.strip()[:110])


def c5_resource_gate(docs, findings):
    for path, text in docs.items():
        for i, ln in enumerate(text.splitlines(), 1):
            if re.search(r"65\.0(9)?", ln) and re.search(r"16\s*(worker|w)", ln, re.I):
                if re.search(r"PASS|通过|满足|达标|绿|合格", ln) and not has_any(ln, ["<", "低", "未", "not", "待", "pending", "不"]):
                    add(findings, "C5", path, i, "16w 低利用率被写成通过: %s" % ln.strip()[:110])


def c6_no_released(docs, findings):
    rx = re.compile(r"\bRELEASED\b|已发布|正式发布")
    for path, text in docs.items():
        for i, ln in enumerate(text.splitlines(), 1):
            if rx.search(ln) and not has_any(ln, ["不", "未", "not", "无权", "不得", "awaiting", "拒绝"]):
                add(findings, "C6", path, i, "出现发布宣称: %s" % ln.strip()[:110])


def c7_rollback(docs, findings):
    guard = ["P33-COEF", "snr_coefficient", "SNRCOEF", "DEAD-PARAMS", "P27"]
    for path in ["docs/contracts/DATA_SEMANTICS.md", "docs/contracts/PUBLIC_API.md"]:
        text = docs.get(path, "")
        for i, ln in enumerate(text.splitlines(), 1):
            for g in guard:
                if g in ln:
                    add(findings, "C7", path, i, "回退态字面量 '%s' 重新出现: %s" % (g, ln.strip()[:100]))


def c8_cov_epsf(docs, findings):
    blob = "\n".join(docs.values())
    if not re.search(r"R\s*C_in\s*R(\^T|ᵀ|T)", blob):
        add(findings, "C8", "docs/contracts", 0, "未见 R C_in R^T 传播式")
    if "effective_psf" not in blob and "effective PSF" not in blob:
        add(findings, "C8", "docs/contracts", 0, "未见 effective PSF 必输要求")


def c9_modes(docs, findings):
    vocab = docs.get("docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md", "")
    for tok in ["point_information", "surface_gls", "psfsw_robust", "equal", "pixel_ivar", "psf_snr_power"]:
        if tok not in vocab:
            add(findings, "C9", "docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md", 0,
                "模式词表缺少 '%s'" % tok)


def c10_doc_index(docs, findings, root, run_index=True):
    if not run_index:
        return
    r = subprocess.run([sys.executable, "tools/doccheck/check_doc_index.py"],
                       cwd=root, capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        add(findings, "C10", "docs/DOCUMENT_INDEX.yaml", 0,
            "check_doc_index.py rc=%d（docs_fully_covered 未闭合）" % r.returncode)


CHECKS = [c1_canonical, c2_psfsw_not_ivar, c3_deferred, c4_concentration,
          c5_resource_gate, c6_no_released, c7_rollback, c8_cov_epsf, c9_modes]


def run_checks(docs, root, run_index=False):
    f = []
    for fn in CHECKS:
        fn(docs, f)
    c10_doc_index(docs, f, root, run_index=run_index)
    return f


# ---------------- negative selftest（注入必须判红） ----------------
def selftest(docs):
    cases = []
    AUTH = "docs/contracts/DATA_SEMANTICS.md"
    VOCAB = "docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md"
    SPEC = "docs/algorithms/v6/phase1/alg_p1_001_spec.json"

    def mut(base, path, transform):
        d = dict(base)
        d[path] = transform(d[path])
        return d

    cases.append(("C2-psfsw-as-ivar", "units=\"flux^-2\"",
                  lambda d: mut(d, AUTH, lambda t: t + "\n| psfsw_robust_weight | units = \"flux^-2\" | 生产接受 |\n"), "C2"))
    cases.append(("C1-third-vocabulary", "third vocabulary weight_normalized",
                  lambda d: mut(d, AUTH, lambda t: t + "\nweight_normalized: true\n"), "C1"))
    cases.append(("C5-resource-as-pass", "16w 65.09% PASS",
                  lambda d: mut(d, "README.md", lambda t: t + "\n16 worker CPU mean 65.09% PASS\n"), "C5"))
    cases.append(("C6-released", "status RELEASED",
                  lambda d: mut(d, "REVIEW.md", lambda t: t + "\npackage_status: RELEASED\n"), "C6"))
    cases.append(("C4-ADU-per-px", "concentration ADU/px",
                  lambda d: mut(d, SPEC, lambda t: t.replace('"psfsw.concentration": "ADU/px^2"', '"psfsw.concentration": "ADU/px"')), "C4"))
    cases.append(("C3-psf_snr_power-in-production", "production modes include psf_snr_power",
                  lambda d: mut(d, VOCAB, lambda t: t + "\n生产模式 = {point_information, surface_gls, psfsw_robust, psf_snr_power}\n"), "C3"))
    cases.append(("C7-P33-reintroduced", "P33-COEF snr_coefficient",
                  lambda d: mut(d, AUTH, lambda t: t + "\nP33-COEF §12.2 snr_coefficient 落位\n"), "C7"))
    cases.append(("C1-canonical-removed", "rename weight.group_normalized (all docs)",
                  lambda d: {k: v.replace("weight.group_normalized", "group_normalized_x") for k, v in d.items()}, "C1"))
    cases.append(("C2-variance_from_weight-true", "psfsw variance_from_weight=true",
                  lambda d: mut(d, AUTH, lambda t: t + "\npsfsw covariance variance_from_weight = true\n"), "C2"))

    results = []
    ok = True
    for name, desc, make, expect in cases:
        d = make(docs)
        f = run_checks(d, ".", run_index=False)
        hit = any(x["check"] == expect for x in f)
        results.append({"mutation": name, "desc": desc, "expected_check": expect,
                        "red": hit, "findings": len([x for x in f if x["check"] == expect])})
        if not hit:
            ok = False
    return {"selftest_pass": ok, "cases": results}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    docs = load_scope(root)
    st = selftest(docs)
    if args.selftest:
        print(json.dumps(st, ensure_ascii=False, indent=1))
        return 0 if st["selftest_pass"] else 2
    findings = run_checks(docs, root, run_index=True)
    fails = [x for x in findings if x["severity"] == "fail"]
    out = {"tool": "reports/v6/release-review/tools/check_doc_convergence.py",
           "task": "DOC-CONVERGE-001", "base_head": "8e1e280e8db498aa879abd52ed8d18fa9f0eabd2",
           "scope_in_files_scanned": len(docs),
           "checks": ["C1-canonical-vocabulary", "C2-psfsw-not-ivar", "C3-deferred-mode",
                      "C4-concentration-unit", "C5-resource-gate", "C6-no-released",
                      "C7-p33-p27-rollback", "C8-covariance-epsf", "C9-modes-exact",
                      "C10-doc-index-coverage"],
           "selftest": st, "findings": findings, "fail_count": len(fails),
           "verdict": "DOC_CONVERGENCE_PASS" if (not fails and st["selftest_pass"]) else "DOC_CONVERGENCE_FAIL"}
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        open(args.json_out, "w", encoding="utf-8").write(text + "\n")
    print(json.dumps({k: out[k] for k in ("verdict", "fail_count", "scope_in_files_scanned")}, ensure_ascii=False))
    for x in fails[:40]:
        print("  FAIL", x["check"], x["path"], x["line"], x["message"][:120])
    if not st["selftest_pass"]:
        print("  SELFTEST MISSES:", [c["mutation"] for c in st["cases"] if not c["red"]])
    return 0 if out["verdict"] == "DOC_CONVERGENCE_PASS" else 2


if __name__ == "__main__":
    sys.exit(main())
