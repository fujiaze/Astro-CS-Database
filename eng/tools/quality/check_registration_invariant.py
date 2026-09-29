#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""INV-REG-001 判据登记面不变式门（待前台安装到 eng/tools/quality/）

把「已登记 ⇒ 实现已入库」变成常设不变式，双向封口：

  INV-1  已登记 => 实现已入库
         checks.json 每条 check/step 的 command 里的仓内脚本路径，
         必须出现在 git 跟踪集（git ls-files）中。
         治：登记表指向尚未入库的脚本 -> 干净克隆必红（R-36 的机器化）。

  INV-2  已入库判据脚本 => 其夹具同批入库
         判据脚本若在源码里引用 <dir>/fixtures/<name>/，该子树全部文件
         必须都在跟踪集内。
         治：夹具漏提交 -> 他人克隆后自证报「夹具缺失」而恒红。

设计纪律（对齐本车道取证纪律）：
  · fail-closed：跟踪集取不到 / 注册表读不到 / 判据脚本集合为空 => exit 2。
  · 「工作区存在」不等于「已入库」：一律以跟踪集为准（git ls-files）。
  · 路径判定不用正则猜：以 argv 里的形态即路径为准，尾部带后缀才算脚本。
  · 自测面 --self-test 全部在临时目录 fixture 上跑，零副作用；
    注入负例必须判红（否则门退化）。

用法：
  python3 check_registration_invariant.py [--root <repo>] [--json-out <f>]
  python3 check_registration_invariant.py --self-test
退出码：0 通过；1 判红；2 输入不可用/门自身错误（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
import tempfile

REL_REGISTRY = "eng/ci/checks.json"
SCRIPT_SUFFIX = (".py", ".sh", ".ps1", ".bat")
INREPO_PREFIX = ("eng/", "lib/", "scripts/", "tools/", "docs/")


def _fail_closed(msg: str) -> None:
    print("[fail-closed] %s" % msg, file=sys.stderr)
    raise SystemExit(2)


def tracked_set(root: pathlib.Path):
    """git 跟踪集。取不到即 fail-closed（绝不退回「工作区存在即算已入库」）。"""
    try:
        p = subprocess.run(["git", "ls-files"], cwd=str(root),
                           capture_output=True, text=True, timeout=300)
    except Exception as e:  # noqa: BLE001
        _fail_closed("git ls-files 执行异常：%s" % e)
    if p.returncode != 0:
        _fail_closed("git ls-files rc=%d：%s" % (p.returncode, p.stderr.strip()[:200]))
    out = {ln.strip() for ln in p.stdout.splitlines() if ln.strip()}
    if not out:
        _fail_closed("git ls-files 返回空集（拒绝把空集当「全部已入库」）")
    return out


def load_registry(root: pathlib.Path):
    p = root / REL_REGISTRY
    if not p.is_file():
        _fail_closed("缺注册表：%s" % p)
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        _fail_closed("注册表不可解析：%s" % e)
    if not isinstance(data.get("checks"), list) or not data["checks"]:
        _fail_closed("注册表 checks 为空或缺失")
    return data["checks"]


def iter_commands(check):
    yield check.get("command") or []
    for s in check.get("steps") or []:
        yield s.get("command") or []


def repo_script_tokens(check):
    """从一条判据的命令行里取出仓内脚本路径 token（不做正则猜测，按后缀判定）。"""
    for cmd in iter_commands(check):
        for tok in cmd or []:
            if not isinstance(tok, str) or tok.startswith("-"):
                continue
            if tok.startswith(INREPO_PREFIX) and tok.endswith(SCRIPT_SUFFIX):
                yield tok


def fixture_dirs_for(script_rel: str, tracked, repo_root: pathlib.Path):
    """脚本源码里引用的夹具目录（<dir>/fixtures/<name> 的三种命名口径）。"""
    src = repo_root / script_rel
    if not src.is_file():
        return []
    try:
        txt = src.read_text(encoding="utf-8", errors="ignore")
    except Exception:  # noqa: BLE001
        return []
    if "fixtures" not in txt:
        return []
    base = os.path.basename(script_rel)
    if base.endswith(".py"):
        stem = base[:-3]
    else:
        stem = base
    names = [stem]
    for pre in ("check_", "verify_", "test_"):
        if stem.startswith(pre):
            names.append(stem[len(pre):])
    names.append(stem.replace("_", "-"))
    d = os.path.dirname(script_rel)
    out = []
    for n in dict.fromkeys(names):
        cand = "%s/fixtures/%s" % (d, n)
        if cand in tracked:
            out.append(cand)
    return out


def fixture_dirs_on_disk(script_rel: str, repo_root: pathlib.Path):
    """脚本源码引用 fixtures 时，其夹具目录在**工作区**的实际存在者。
    口径：<脚本目录>/fixtures/<名称>；名称取 stem、去 check_/verify_/test_ 前缀、
    连字符化三种形态逐一试。此处判「存在」看工作区（夹具是否漏提交要靠跟踪集另行比对）。"""
    src = repo_root / script_rel
    if not src.is_file():
        return []
    try:
        txt = src.read_text(encoding="utf-8", errors="ignore")
    except Exception:  # noqa: BLE001
        return []
    if "fixtures" not in txt:
        return []
    base = os.path.basename(script_rel)
    stem = base[:-3] if base.endswith(".py") else base
    names = [stem]
    for pre in ("check_", "verify_", "test_"):
        if stem.startswith(pre):
            names.append(stem[len(pre):])
    names.append(stem.replace("_", "-"))
    d = os.path.dirname(script_rel)
    out = []
    for n in dict.fromkeys(names):
        cand = "%s/fixtures/%s" % (d, n)
        if (repo_root / cand).is_dir():
            out.append(cand)
    return out


def walk_tracked(prefix, tracked):
    return sorted(p for p in tracked if p.startswith(prefix.rstrip("/") + "/"))


def check(root: pathlib.Path):
    errors, notes = [], []
    tracked = tracked_set(root)
    registry = load_registry(root)
    notes.append("跟踪集 %d 条；登记判据 %d 条" % (len(tracked), len(registry)))

    # ---------- INV-1 ----------
    inv1_total = 0
    inv1_bad = {}
    for c in registry:
        cid = c.get("id", "<无 id>")
        for tok in repo_script_tokens(c):
            inv1_total += 1
            if tok not in tracked:
                inv1_bad.setdefault(tok, set()).add(cid)
    for tok in sorted(inv1_bad):
        errors.append("INV-1 已登记但实现未入库：%s <- %s" % (tok, sorted(inv1_bad[tok])))
    notes.append("INV-1 登记引用的仓内脚本 %d 处；未入库 %d 个"
                 % (inv1_total, len(inv1_bad)))

    # ---------- INV-2 ----------
    # 分母：被登记判据引用、且已入库的 .py 判据脚本
    referenced = {t for c in registry for t in repo_script_tokens(c)}
    if not referenced:
        _fail_closed("注册表未引用任何仓内判据脚本（拒绝把空分母当「无夹具缺失」）")
    # 只对**已入库**的判据脚本核夹具；未入库的已由 INV-1 判红，不在此重复。
    # 分母口径 = 「已登记引用的」∪「跟踪集内形如判据的脚本」——
    # 纪律要求「已入库判据脚本 ⇒ 夹具同批入库」，**与是否已登记无关**
    # （未登记的判据脚本同样会被别人直接跑，夹具漏提交一样恒红）。
    import fnmatch as _fn
    _gate_cand = set()
    for p in tracked:
        if not p.endswith(".py"):
            continue
        b = os.path.basename(p)
        if b.startswith("test_") or b.endswith("_test.py"):
            continue
        if any(x in p for x in ("/tests/", "testdata/", "/examples/", "__pycache__/")):
            continue
        if _fn.fnmatch(b, "check_*.py") or _fn.fnmatch(b, "verify_*.py"):
            _gate_cand.add(p)
    _all_gate = sorted((({s for s in referenced if s.endswith(".py")} | _gate_cand)
                        & tracked))
    gate_scripts = _all_gate
    inv2_dirs = 0
    inv2_files = 0
    inv2_missing = []
    for s in gate_scripts:
        for d in fixture_dirs_on_disk(s, root):
            inv2_dirs += 1
            base = root / d
            for dirpath, dirnames, filenames in os.walk(base):
                dirnames[:] = [x for x in dirnames if x != "__pycache__"]
                for f in filenames:
                    if f.endswith(".pyc"):
                        continue
                    full = pathlib.Path(dirpath) / f
                    rel = str(full.relative_to(root)).replace(os.sep, "/")
                    inv2_files += 1
                    if rel not in tracked:
                        inv2_missing.append((s, rel))
    for s, f in sorted(set(inv2_missing)):
        errors.append("INV-2 夹具未入库（他人克隆后自证恒红）：%s <- %s" % (f, s))
    notes.append("INV-2 判据脚本 %d 个；夹具目录 %d 个；夹具文件 %d 个；未入库 %d 个"
                 % (len(gate_scripts), inv2_dirs, inv2_files, len(set(inv2_missing))))
    return errors, notes, inv2_files


# ── 自测面（临时目录 fixture，零副作用；含注入负例）────────────────────────
def _mk(root: pathlib.Path, files: dict, tracked: list):
    for rel, content in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")


FAKE_GIT = "eng/ci/checks.json"


def _selftest_case(name, files, tracked, expect_rc):
    """用 fixture + 假的 git ls-files（通过 PATH 注入）跑一遍 check()。"""
    with tempfile.TemporaryDirectory(prefix="invreg_selftest_") as td:
        root = pathlib.Path(td)
        _mk(root, files, tracked)
        # 注入一个假的 git：ls-files 输出 tracked 列表
        bindir = root / "_fakebin"
        bindir.mkdir()
        (bindir / "git").write_text(
            "#!/bin/sh\nif [ \"$1\" = ls-files ]; then cat <<'EOF'\n%s\nEOF\nexit 0; fi\nexit 1\n"
            % "\n".join(tracked), encoding="utf-8")
        (bindir / "git").chmod(0o755)
        old = os.environ.get("PATH", "")
        os.environ["PATH"] = str(bindir) + os.pathsep + old
        try:
            errors, notes, _ = check(root)
            rc = 1 if errors else 0
        except SystemExit as e:
            rc = int(e.code or 2)
        finally:
            os.environ["PATH"] = old
    ok = (rc == expect_rc)
    print("  %-46s expect_rc=%d got=%d  %s"
          % (name, expect_rc, rc, "PASS" if ok else "FAIL"))
    return ok


def self_test() -> int:
    print("INV-REG-001 self-test")
    reg_ok = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-OK", "command": ["python3", "eng/ci/good.py"], "steps": []},
    ]}, ensure_ascii=False)
    reg_bad = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-BAD", "command": ["python3", "eng/ci/missing.py"], "steps": []},
    ]}, ensure_ascii=False)
    reg_fix = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-BAD", "command": ["python3", "eng/ci/missing.py"], "steps": []},
    ]}, ensure_ascii=False)
    reg_fx = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-FX", "command": ["python3", "eng/ci/g.py"], "steps": []},
    ]}, ensure_ascii=False)
    reg_empty = json.dumps({"schema_version": 1, "checks": []}, ensure_ascii=False)

    cases = [
        # N0 正例：登记的脚本都已入库 => 绿
        ("N0 正例：登记⇒已入库", {FAKE_GIT: reg_ok,
          "eng/ci/good.py": "print(1)\n"}, ["eng/ci/checks.json", "eng/ci/good.py"], 0),
        # N1 注入负例：登记指向未入库脚本 => 判红
        ("N1 负例：登记指向未入库脚本", {FAKE_GIT: reg_bad,
          "eng/ci/missing.py": "print(1)\n"}, ["eng/ci/checks.json"], 1),
        # N2 负例：工作区有文件但未入库（先登记后入库的原型）=> 判红
        ("N2 负例：文件在盘但未入库", {FAKE_GIT: reg_fix,
          "eng/ci/missing.py": "print(1)\n"}, ["eng/ci/checks.json"], 1),
        # N3 负例：夹具文件未入库 => 判红
        ("N3 负例：夹具漏提交", {FAKE_GIT: reg_fx,
          "eng/ci/g.py": "FX='fixtures/g'\n", "eng/ci/fixtures/g/case.json": "{}"},
         ["eng/ci/checks.json", "eng/ci/g.py", "eng/ci/fixtures/g"], 1),
        # N4 负例：注册表空 => fail-closed exit 2
        ("N4 负例：注册表空 => fail-closed", {FAKE_GIT: reg_empty}, ["eng/ci/checks.json"], 2),
        # N5 负例：跟踪集空 => fail-closed exit 2
        ("N5 负例：跟踪集空 => fail-closed", {FAKE_GIT: reg_ok,
          "eng/ci/good.py": "print(1)\n"}, [], 2),
    ]
    bad = 0
    for name, files, tracked, exp in cases:
        if not _selftest_case(name, files, tracked, exp):
            bad += 1

    # T1 回归（本门自身的洞，FINAL-07）：前 6 例全部直接调 check()，**从不调 main()**，
    # 于是 argparse 的 --root 默认值零覆盖 —— 默认取 __file__.parent 时会拼出
    # eng/tools/quality/eng/ci/checks.json 而必然 exit 2，但 6 例照样全绿。
    # 这里补一条**按文档写的用法直接跑 main()** 的正例：不传 --root，断言它能在
    # 真实仓库里定位到注册表并给出结论（不要求 pass，只要求不是「缺注册表」误报）。
    rc = main([])
    if rc == 2:
        print("  T1 回归：默认形态 main() 返回 fail-closed(2)"
              "⇒ --root 默认值仍不能定位注册表")
        bad += 1
    else:
        print("  T1 回归：默认形态 main() 可用（rc=%d，非缺注册表误报）" % rc)

    print("SELFTEST_%s" % ("FAIL" if bad else "PASS"))
    return 1 if bad else 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="判据登记面不变式门（INV-REG-001）")
    # 默认为**仓库根**而不是脚本所在目录：注册表在 <root>/eng/ci/checks.json，
    # 取 __file__.parent 会拼出 eng/tools/quality/eng/ci/checks.json ⇒ 默认形态必然 exit 2
    # 「缺注册表」。FINAL-07 实测。判定面必须能在**按文档写的用法直接跑**时可用。
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    root = pathlib.Path(a.root)
    errors, notes, _ = check(root)
    if a.json_out:
        out = pathlib.Path(a.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"check": "INV-REG-001", "root": str(root),
                                   "notes": notes, "errors": errors,
                                   "pass": not errors}, ensure_ascii=False, indent=1),
                       encoding="utf-8")
    for n in notes:
        print("[ok] %s" % n)
    if errors:
        for e in errors:
            print("[FAIL] %s" % e, file=sys.stderr)
        print("INV_REG_FAIL errors=%d" % len(errors))
        return 1
    print("INV_REG_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
