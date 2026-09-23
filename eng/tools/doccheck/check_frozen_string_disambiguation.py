#!/usr/bin/env python3
"""冻结串读法消歧门（FZ-P3-BUNIT-QUADRATIC）。

背景：冻结串 "variance = signal^2; ivar = 1/variance; Phase3 variance BUNIT =
(main HDU signal BUNIT)^2" 的首分句与**数值物理律**同形，易被误读为「方差数值 = 信号数值的平方」。
正确读法是**单位传播**（BUNIT(variance) = BUNIT(signal)^2）。该串是冻结契约值（schema const 逐字规定），
**不得改写**；因此本门不改契约，只断言「**文档层已写明该串按单位读**」+「**四处登记值互不漂移**」。

断言（全部必须成立）：
  D1 冻结串在 schema $defs/units 的 const、registry quadratic_law.statement、
     units.example.json quadratic_law.statement 三处**逐字相同**（不得漂移）；
  D2 schema const **仍以** "variance = signal^2" 开头（即未被改写成单位传播式——改写=重新冻结，须走变更流程）；
  D3 docs/contracts/DATA_SEMANTICS.md §31.1 含**消歧说明**：
     (a) 冻结串逐字在文；(b) 单位读法 BUNIT(variance) = BUNIT(signal)^2 在文；
     (c) 数值层反例在文（探测器偏置项与信号无关 ⇒ 信号趋零时方差不趋零）；
  D4 文档仍含 "variance = signal^2"（field_constraints_oracle O04b 依赖它，删了会连带判红）。

用法: check_frozen_string_disambiguation.py [--self-test] [--repo-root DIR]
退出码 0 = 全绿；1 = 至少一条判红；2 = 用法/夹具错误。
"""
import argparse, json, os, sys

FROZEN = "variance = signal^2; ivar = 1/variance; Phase3 variance BUNIT = (main HDU signal BUNIT)^2"
UNIT_FORM = "BUNIT(variance) = BUNIT(signal)^2"
SCHEMA_REL = "eng/contracts/schemas/product_family_field_constraints.schema.json"
REG_REL = "eng/contracts/data/v6_clause_registry_v1.json"
EX_REL = "eng/contracts/data/examples/v6/units.example.json"
DS_REL = "docs/contracts/DATA_SEMANTICS.md"
# 数值层反例的关键片段（三者任一在文即可，容忍措辞微调）
COUNTEREXAMPLE_MARKERS = ["信号趋零时方差不趋零", "与信号无关", "1/12 DN^2"]


DISAMB_HEAD = "- **冻结串读法消歧（"
def _extract_bullet(text, head):
    """切出以 head 起头、到下一个顶层 "- **" 为止的 bullet 正文；找不到返回 None。"""
    i = text.find(head)
    if i < 0:
        return None
    j = text.find("\n- **", i)
    return text[i:] if j < 0 else text[i:j]


def _read(root, rel, overrides):
    p = overrides.get(rel)
    if p:
        with open(p, encoding="utf-8") as f:
            return f.read()
    with open(os.path.join(root, rel), encoding="utf-8") as f:
        return f.read()


def run(root, overrides=None):
    overrides = overrides or {}
    fails = []
    checks = 0

    def ck(cond, tag, msg):
        nonlocal checks
        checks += 1
        if not cond:
            fails.append("%s: %s" % (tag, msg))

    try:
        schema = json.loads(_read(root, SCHEMA_REL, overrides))
        reg = json.loads(_read(root, REG_REL, overrides))
        ex = json.loads(_read(root, EX_REL, overrides))
        ds = _read(root, DS_REL, overrides)
    except Exception as e:  # 夹具/解析错误
        return 2, ["FIXTURE: %s" % e], 0

    try:
        const = schema["$defs"]["units"]["properties"]["quadratic_law"]["properties"]["statement"]["const"]
        regst = reg["quadratic_law"]["statement"]
        exst = ex["quadratic_law"]["statement"]
    except (KeyError, TypeError) as e:
        return 2, ["FIXTURE: 冻结串路径缺失: %s" % e], 0

    # D1 三处逐字相同
    ck(const == regst == exst, "D1-frozen-string-drift",
       "schema const / registry statement / example statement 必须逐字相同；"
       "const=%r reg=%r ex=%r" % (const, regst, exst))
    # D2 const 未被改写
    ck(const.startswith("variance = signal^2"), "D2-const-not-refrozen",
       "schema const 必须仍以 \"variance = signal^2\" 开头（改写=重新冻结，须走变更流程）；实为 %r" % const)
    # D3 消歧说明在位。**注意判据必须限定在「消歧 bullet 内」**——
    # 若只在全文里找短语，则这些短语在别处（§31.1 单位层 bullet）也存在，
    # 判据会退化成恒真（删掉整个消歧 bullet 仍绿）。故先切出 bullet 再逐条断言。
    bullet = _extract_bullet(ds, DISAMB_HEAD)
    ck(bullet is not None, "D3a-disambiguation-bullet-present",
       "DATA_SEMANTICS 必须含以 %r 起头的消歧 bullet" % DISAMB_HEAD)
    body = bullet or ""
    ck(FROZEN in body, "D3b-frozen-string-in-bullet",
       "消歧 bullet 内必须逐字引用冻结串")
    ck(UNIT_FORM in body, "D3c-unit-reading-in-bullet",
       "消歧 bullet 内必须写明单位读法 %r" % UNIT_FORM)
    ck(any(m in body for m in COUNTEREXAMPLE_MARKERS), "D3d-counterexample-in-bullet",
       "消歧 bullet 内必须写明数值层反例（%s 任一）" % " / ".join(COUNTEREXAMPLE_MARKERS))
    ck("不得改写" in body or "不得改" in body, "D3e-no-rewrite-clause-in-bullet",
       "消歧 bullet 内必须写明该串不得改写（冻结契约值）")
    # D5 规范位（非消歧 bullet）也逐字引用冻结串 ⇒ 契约值在正文规范位可见
    ck(FROZEN in ds.replace(body, ""), "D5-frozen-string-at-normative-site",
       "冻结串必须同时在规范位（消歧 bullet 之外）逐字出现")
    # D4 oracle O04b 依赖的串仍在文档
    ck("variance = signal^2" in ds, "D4-oracle-o04b-dependency",
       "文档仍须含 \"variance = signal^2\"（field_constraints_oracle O04b 断言依赖）")
    return (0 if not fails else 1), fails, checks


def _self_test(root):
    import tempfile, shutil
    cases = []
    rc, fails, n = run(root)
    cases.append(("green_clean", rc, 0, fails))

    tmp = tempfile.mkdtemp(prefix="fzq-selftest-")
    try:
        # 红 1（单变量隔离）：**只删「冻结串读法消歧」那一条 bullet**，其余逐字保留
        # （冻结串原文、数值层原句、单位层原句都不动）⇒ 只有消歧断言该红。
        ds = open(os.path.join(root, DS_REL), encoding="utf-8").read()
        head = "- **冻结串读法消歧（"
        i = ds.find(head)
        assert i > 0, "self-test 夹具失效：文档里找不到消歧 bullet 起点"
        j = ds.find("\n- **EMVA 1288 的引用口径", i)
        assert j > i, "self-test 夹具失效：找不到消歧 bullet 终点"
        mutated = ds[:i] + ds[j + 1:]
        assert FROZEN in mutated, "self-test 夹具失效：误删了冻结串原文"
        assert "1/12 DN^2" in mutated, "self-test 夹具失效：误删了数值层反例"
        p1 = os.path.join(tmp, "ds_no_disamb.md")
        open(p1, "w", encoding="utf-8").write(mutated)
        rc, fails, _ = run(root, {DS_REL: p1})
        cases.append(("red_disambiguation_bullet_removed", rc, 1, fails))

        # 红 2：把 schema const 改写成单位传播式（= 重新冻结）
        sc = json.loads(open(os.path.join(root, SCHEMA_REL), encoding="utf-8").read())
        sc["$defs"]["units"]["properties"]["quadratic_law"]["properties"]["statement"]["const"] = UNIT_FORM
        p2 = os.path.join(tmp, "schema_refrozen.json")
        json.dump(sc, open(p2, "w", encoding="utf-8"), ensure_ascii=False)
        rc, fails, _ = run(root, {SCHEMA_REL: p2})
        cases.append(("red_const_refrozen", rc, 1, fails))

        # 红 3：registry statement 与 schema const 漂移
        rg = json.loads(open(os.path.join(root, REG_REL), encoding="utf-8").read())
        rg["quadratic_law"]["statement"] = FROZEN + " "
        p3 = os.path.join(tmp, "reg_drift.json")
        json.dump(rg, open(p3, "w", encoding="utf-8"), ensure_ascii=False)
        rc, fails, _ = run(root, {REG_REL: p3})
        cases.append(("red_statement_drift", rc, 1, fails))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    ok = all(got == want for _, got, want, _ in cases)
    for name, got, want, fails in cases:
        print("SELFTEST_%s %s (rc=%d want=%d)%s" % (
            "PASS" if got == want else "FAIL", name, got, want,
            "" if got == want else "  fails=%r" % (fails[:3],)))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--repo-root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
    a = ap.parse_args()
    root = a.repo_root
    if a.self_test:
        return _self_test(root)
    rc, fails, n = run(root)
    if rc == 2:
        print("FIXTURE_ERROR: %s" % fails[0])
        return 2
    if rc == 0:
        print("FROZEN_STRING_DISAMBIGUATION_PASS checks=%d" % n)
    else:
        print("FROZEN_STRING_DISAMBIGUATION_FAIL (%d):" % len(fails))
        for f in fails:
            print("  " + f)
    return rc


if __name__ == "__main__":
    sys.exit(main())
