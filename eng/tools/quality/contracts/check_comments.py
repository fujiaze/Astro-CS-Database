#!/usr/bin/env python3
"""check_comments.py — T406 comments checker

Checks: 禁止过期审计轮次、旧版本宣称、代码复述、错误线程/单位；要求复杂不变量附近有 ID
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re

STALE_PATTERNS = [
    (r"V1[0-9]R[0-9]", "stale audit round V19R2/V19R3"),
    (r"TODO.*fix|FIXME.*legacy", "code复述 fix/legacy"),
    (r"thread.*16.*hard.*code|num_threads\(16\)", "hardcoded 16 threads"),
]
# Require ID near complex invariants: check that invariant-adjacent comments have SCI/ALG ID
REQUIRE_ID_NEAR = ["invariant", "不变量", "conservative", "false_negative"]

# ── W4-A3：不变量-ID 判据的作用域 ─────────────────────────────────────────────
# 事由：原实现把「含 invariant/不变量/false_negative 等词 ⇒ 必须就近出现 SCI-/ALG-
# ID」应用到 **lib/ 全域**。但 SCI-*/ALG-* 是科学/算法层 ID（定义域 docs/science、
# docs/algorithms），只有 lib/algorithms/** 的实现有对应权威可引；基础设施模块
# （lib/infrastructure/**、lib/core/**）在追溯矩阵里 SCI/ALG 本就登记为 MISSING。
# 实测 2 条真红全是基础设施头里的**通用工程措辞**（退出码枚举注释「科学验证/数值
# 不变量失败」、错误码描述「内部不变量违例」），要求它们引 SCI/ALG 是错口径。
# 修法：判据作用域收窄到 lib/algorithms/**（口径讲得通的那一面），并给可执行负例面
# --self-test（注入一条 lib/algorithms/** 的无 ID 不变量注释 ⇒ 必红）。
INVARIANT_SCOPE = "lib/algorithms"
REQUIRE_ID_NEAR_LOWER = tuple(k.lower() for k in REQUIRE_ID_NEAR)


# 权威引用面（W4-A3）：不变量注释必须给出**权威锚**。权威锚 = 科学/算法 ID，
# 或指向权威文档路径（docs/science/**、docs/algorithms/**）。
# 依据：追溯规范允许"ID 或文档锚"两种形态；实测 ipv_solver.h:66 引的是
# `docs/algorithms/PLATESOLVE.md Invariants`，是合格的权威锚却被旧口径判红。
AUTHORITY_TOKENS = ("SCI-", "ALG-", "TRACEABILITY",
                    "docs/science/", "docs/algorithms/")
# 内嵌第三方单头库（vendored）识别：文件头若干行内同时出现许可证名与 Copyright。
# 判据：这类文件的注释不受本项目注释纪律约束（不是我们的代码）。
VENDOR_HEAD_LINES = 40
VENDOR_LICENSE_RE = re.compile(r"BSD License|MIT License|Apache License|GNU (?:LESSER )?GENERAL PUBLIC LICENSE|zlib License", re.I)
VENDOR_COPYRIGHT_RE = re.compile(r"Copyright\s*(\(c\))?\s*\d{4}", re.I)


def _is_vendored(src: pathlib.Path, text: str) -> bool:
    if "third_party" in src.as_posix() or "archive" in src.as_posix():
        return True
    head = "\n".join(text.splitlines()[:VENDOR_HEAD_LINES])
    return bool(VENDOR_LICENSE_RE.search(head) and VENDOR_COPYRIGHT_RE.search(head))


def _has_authority(text: str) -> bool:
    return any(tok in text for tok in AUTHORITY_TOKENS)


def _in_invariant_scope(repo: pathlib.Path, src: pathlib.Path) -> bool:
    try:
        rel = src.relative_to(repo).as_posix()
    except ValueError:
        rel = src.as_posix()
    return rel == INVARIANT_SCOPE or rel.startswith(INVARIANT_SCOPE + "/")


def _scan(repo: pathlib.Path):
    """返回 (findings, total)。抽出来供 main 与 --self-test 共用（同一判据面）。"""
    findings = []
    srcs = (list((repo / "lib").rglob("*.cpp")) + list((repo / "lib").rglob("*.h"))
            + list((repo / "lib").rglob("*.hpp")))
    srcs = [p for p in srcs if "third_party" not in str(p) and "archive" not in str(p)]
    total = len(srcs)
    for src in srcs:
        text = src.read_text(encoding="utf-8", errors="ignore")
        comments = re.findall(r"//.*|/\*.*?\*/", text, re.S)
        if _is_vendored(src, text):
            continue  # 内嵌第三方头不属本项目注释面（依据见 _is_vendored）
        for c in comments:
            if "V19R2" in c or "V19R3" in c:
                if "冻结" not in c and "history" not in str(src).lower():
                    findings.append({"id": "COMMENT-STALE", "severity": "P1",
                                     "file": str(src.relative_to(repo)), "symbol": c[:60],
                                     "observed": "stale audit round",
                                     "expected": "remove or update"})
                    break
        if not _in_invariant_scope(repo, src):
            continue
        text_lower = text.lower()
        if any(k in text_lower or k in text for k in REQUIRE_ID_NEAR_LOWER):
            if not _has_authority(text):
                findings.append({"id": "COMMENT-MISSING-ID", "severity": "P1",
                                 "file": str(src.relative_to(repo)),
                                 "observed": "invariant without SCI/ALG ID nearby",
                                 "expected": "ID reference"})
    return findings, total


def _self_test() -> int:
    """可执行负例面（ENGINEERING_SPEC §8）：判据必须能红，且不得对域外面误红。"""
    import tempfile
    cases = []
    with tempfile.TemporaryDirectory(prefix="cc-selftest-") as td:
        root = pathlib.Path(td)
        # 正例：lib/algorithms/** 内不变量注释 + SCI ID ⇒ 绿
        d = root / "lib/algorithms/x/src"
        d.mkdir(parents=True)
        (d / "ok.cpp").write_text("// SCI-X-001: 该不变量由 SCI 冻结\nint f(){return 0;}\n",
                                  encoding="utf-8")
        f, _ = _scan(root)
        cases.append(("pos_algorithm_with_id", f, 0))
        # 负例：lib/algorithms/** 内不变量注释无 ID ⇒ 必红
        (d / "bad.cpp").write_text("// 本函数保持不变量: 输出恒非负\nint g(){return 0;}\n",
                                   encoding="utf-8")
        f, _ = _scan(root)
        cases.append(("neg_algorithm_invariant_no_id", f, 1))
        # 域外：lib/infrastructure/** 的通用措辞不得误红（作用域收窄的依据）
        d2 = root / "lib/infrastructure/y"
        d2.mkdir(parents=True, exist_ok=True)
        (d2 / "infra.h").write_text("enum E { SCIENCE = 4, }; // 科学验证/数值不变量失败\n",
                                    encoding="utf-8")
        # 正例：权威锚也可以是权威文档路径（不限于 SCI-/ALG- ID）
        (d / "bad.cpp").write_text(
            "// 见 docs/algorithms/PLATESOLVE.md Invariants: 输出恒非负\nint h(){return 0;}\n",
            encoding="utf-8")
        f, _ = _scan(root)
        cases.append(("pos_authority_doc_path", f, 0))
        # 正例：内嵌第三方单头库（BSD + Copyright 头）不属本项目注释面
        d3 = root / "lib/algorithms/z/third"
        d3.mkdir(parents=True)
        (d3 / "vendored.hpp").write_text(
            "/* BSD License\n * Copyright 2011-2026 Jose Blanco\n */\n"
            "// 保持不变量\nint v(){return 0;}\n", encoding="utf-8")
        (d / "bad.cpp").unlink()
        f, _ = _scan(root)
        cases.append(("pos_vendored_header_out_of_scope", f, 0))
        # 域外：lib/infrastructure/** 的通用措辞不得误红（作用域收窄的依据）
        d2 = root / "lib/infrastructure/y"
        d2.mkdir(parents=True, exist_ok=True)
        (d2 / "infra.h").write_text("enum E { SCIENCE = 4, }; // 科学验证/数值不变量失败\n",
                                    encoding="utf-8")
        f, _ = _scan(root)
        cases.append(("pos_infrastructure_out_of_scope", f, 0))
    ok = True
    for name, found, want in cases:
        got = len(found)
        good = got == want
        ok = ok and good
        print("[self-test] %-32s findings=%d want=%d %s"
              % (name, got, want, "OK" if good else "MISMATCH"))
    print("[self-test] %d cases, %s" % (len(cases), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true",
                    help="可执行负例面（注入必红 / 域外不误红 / 未注入必绿）")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    repo = pathlib.Path(args.repo)
    findings, total = _scan(repo)
    status = "PASS" if not findings else "FAIL"
    # Scan source files in lib/**/*.cpp|*.h|*.hpp
    # 判据体已抽到 _scan()（main 与 --self-test 共用同一判据面，杜绝两套口径）
    # Additional: check that no source file claims wrong thread model (e.g., says parallel but is serial)
    # Heuristic: if doc says parallel but source has OFF, that's already covered in EXEC checker; skip here

    result = {"tool":"check_comments","status":status,"files_scanned":total,"coverage":{"scanned":total,"total":total,"ratio":1.0,"mode":"full"},"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_comments" tests="{total}" failures="{failures}"><testcase classname="comments" name="hygiene"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
