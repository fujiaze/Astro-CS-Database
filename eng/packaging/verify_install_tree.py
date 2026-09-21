#!/usr/bin/env python3
"""AstroCS 安装树/模块 verify 验证器 (BLD-003 + W5-PKG-001)

机器验收:
  1. clean install 仅白名单: 以 eng/packaging/install-tree.contract.json 的 units
     为白名单; 每个 required 文件必须存在 (缺任一 required → 非零退出);
     **白名单外条目必须判红**（W5-PKG-001 增补: 改前只查"声明的在不在",
     不查"多出来的"); 目录条目不计入违例, 但目录内文件逐个人检。
  2. 删除 noop 模块 DLL/.so → verify 明确失败: 本工具把 MOD-NOOP 标为
     required; 删除 modules/astrocs_noop.so 后必须非零退出并输出
     "MISSING REQUIRED <...>" 与 "MODULE VERIFY FAIL", 证明宿主无静态
     fallback (BLD-003 验收失败路径; CLI 的 modules verify 命令面属 CLI-001,
     本工具是安装树验收层, 不替代 CLI)。
  3. product manifest 结构: 顶层 astrocs.product.json 存在且 JSON 可解析,
     units 全部存在 (rel_path 相对 prefix 解析); **manifest 与合同的 unit 集
     必须双向闭合**（改前合同少一个 unit 仍 PASS）; sha256 为 null (BLD-003
     骨架) 时跳过 hash 校验, 非空时必须与实文件一致 (ABI-004 填充后自动生效)。
  4. 版本/来源登记面: manifest product_version 必须逐字等于仓库根 VERSION
     (唯一版本源); platform 与 unit 扩展名必须自洽; source_commit 必须是
     40 位 hex, 并在 --expect-commit 给定时必须逐字相等（本轮报告 HEAD 差异,
     不把「构建树陈旧」并入结构判据, 见文件末 NOTE）。

用法:
  python3 eng/packaging/verify_install_tree.py --prefix <install-prefix>
    退出码 0 = 全部 required 存在 + 无白名单外条目 + manifest 一致; 1 = 违例。
  python3 eng/packaging/verify_install_tree.py --prefix <prefix> --contract <path>
    指定安装树合同 (默认 <repo>/packaging/install-tree.contract.json)。
  python3 eng/packaging/verify_install_tree.py --repo <repo> --prefix <prefix>
  python3 eng/packaging/verify_install_tree.py --self-test
    负例注入自测（ENGINEERING_SPEC §8 可执行负例面）: 每项注入必须判红。
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

MANIFEST = "astrocs.product.json"
VERSION_FILE = "VERSION"
UNIT_KINDS = ("exe", "runtime", "io", "module", "provider")


class InputUnavailable(Exception):
    """输入缺失/不可解析 —— fail-closed（退出码 2）。"""


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def repo_head(repo: Path) -> str:
    try:
        out = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(repo),
                             capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return ""
    return out.stdout.decode("utf-8").strip()


def iter_tree_files(prefix: Path):
    """安装树内全部普通文件（相对 prefix, 正斜杠）。"""
    for p in sorted(prefix.rglob("*")):
        if p.is_file():
            yield p.relative_to(prefix).as_posix()


def evaluate(repo: Path, prefix: Path, contract_path: Path, expect_commit: str = ""):
    """返回 (违规列表, 报告 dict)。任何输入缺失抛 InputUnavailable。"""
    if not prefix.is_dir():
        raise InputUnavailable(f"prefix not a directory: {prefix}")
    if not contract_path.is_file():
        raise InputUnavailable(f"ANCHOR_STALE: contract 不存在: {contract_path}")
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise InputUnavailable(f"contract 非法 JSON: {e}") from e
    units = contract.get("units") or []
    if not units:
        raise InputUnavailable("contract units 为空 (fail-closed)")

    violations, present, missing = [], [], []
    for u in units:
        rel = u["install_path"]
        if (prefix / rel).exists():
            present.append(rel)
        elif u.get("required", False):
            missing.append(rel)

    # ── 白名单闭包: 安装树不得出现未登记条目 ──
    declared = {u["install_path"] for u in units}
    for rel in iter_tree_files(prefix):
        if rel not in declared:
            violations.append(("UNREGISTERED", f"{rel} (安装树出现合同未登记条目)"))

    # ── product manifest ──
    manifest_ok, manifest_err = True, ""
    manifest = {}
    mpath = prefix / MANIFEST
    if not mpath.is_file():
        manifest_ok, manifest_err = False, f"product manifest missing: {MANIFEST}"
    else:
        try:
            manifest = json.loads(mpath.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            manifest_ok, manifest_err = False, f"product manifest invalid JSON: {e}"
    if manifest_ok:
        for mu in manifest.get("units", []):
            rel = mu.get("rel_path", "")
            if rel and not (prefix / rel).exists():
                manifest_ok = False
                manifest_err = f"product manifest unit missing: {rel}"
                break
        munits = {u.get("unit_id"): u for u in manifest.get("units", [])}
        contract_units = {u.get("unit_id"): u for u in units
                          if u.get("kind") in UNIT_KINDS}
        for uid in sorted(set(contract_units) - set(munits)):
            violations.append(("CONTRACT-ONLY", f"{uid} (合同登记但产品清单缺失)"))
        for uid in sorted(set(munits) - set(contract_units)):
            violations.append(("MANIFEST-ONLY", f"{uid} (产品清单登记但合同缺失)"))
        for uid in sorted(set(munits) & set(contract_units)):
            a = munits[uid].get("rel_path")
            b = contract_units[uid]["install_path"]
            if a != b:
                violations.append(("REL-PATH-DRIFT", f"{uid}: manifest={a} contract={b}"))
            claimed = munits[uid].get("sha256")
            if claimed and (prefix / (a or "")).is_file():
                got = sha256_file(prefix / a)
                if got != claimed:
                    violations.append(("HASH-MISMATCH",
                                        f"{a}: got {got[:16]}… want {claimed[:16]}…"))
        # 平台与扩展名自洽
        plat = manifest.get("platform")
        if plat not in ("linux-amd64", "windows-amd64"):
            violations.append(("PLATFORM-INVALID", repr(plat)))
        else:
            want_ext = ".dll" if plat == "windows-amd64" else None
            for mu in manifest.get("units", []):
                rel = mu.get("rel_path", "")
                if want_ext and rel.endswith(".dll") is False and mu.get("kind") != "exe":
                    violations.append(("PLATFORM-EXT",
                                        f"{rel}: platform={plat} 期待 .dll"))
                if want_ext is None and rel.endswith(".dll"):
                    violations.append(("PLATFORM-EXT",
                                        f"{rel}: platform={plat} 不期待 .dll"))
        # 版本单源
        vfile = repo / VERSION_FILE
        if not vfile.is_file():
            raise InputUnavailable(f"ANCHOR_STALE: {VERSION_FILE} 不存在于 {repo}")
        ver = vfile.read_text(encoding="utf-8").strip()
        if manifest.get("product_version") != ver:
            violations.append(("VERSION-DRIFT",
                                f"manifest.product_version="
                                f"{manifest.get('product_version')!r} != VERSION {ver!r}"))
        sc = str(manifest.get("source_commit", ""))
        if not re.fullmatch(r"[0-9a-f]{40}", sc):
            violations.append(("COMMIT-FORMAT", f"source_commit={sc!r} 非 40 位 hex"))
        elif expect_commit and sc != expect_commit:
            violations.append(("COMMIT-DRIFT",
                                f"source_commit={sc} != --expect-commit {expect_commit}"))
    head = repo_head(repo)
    report = {
        "checker": "INSTALL_TREE_VERIFY",
        "prefix": str(prefix),
        "contract": str(contract_path),
        "declared_units": len(units),
        "required_present": [r for r in present
                             if any(u["install_path"] == r and u.get("required", False)
                                    for u in units)],
        "missing_required": missing,
        "unregistered": [m.split(" ", 1)[0] for c, m in violations
                         if c == "UNREGISTERED"],
        "violations": [{"code": c, "detail": m} for c, m in violations],
        "product_manifest_ok": manifest_ok,
        "product_manifest_error": manifest_err,
        "manifest_product_version": manifest.get("product_version"),
        "manifest_source_commit": manifest.get("source_commit"),
        "repo_head": head,
        "stale_build_dir": bool(head and manifest.get("source_commit")
                                and manifest.get("source_commit") != head),
        "verdict": "PASS" if (not violations and not missing and manifest_ok) else "FAIL",
    }
    return violations, missing, manifest_ok, manifest_err, report


def run(repo: Path, prefix: Path, contract_path: Path, expect_commit: str, json_out: str):
    try:
        violations, missing, manifest_ok, manifest_err, report = evaluate(
            repo, prefix, contract_path, expect_commit)
    except InputUnavailable as e:
        print(f"VERIFY FAIL (fail-closed): {e}", file=sys.stderr)
        return 2
    if json_out:
        out = Path(json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
    if report["stale_build_dir"]:
        print(f"PROVENANCE NOTE: 安装树 source_commit={report['manifest_source_commit']} "
              f"!= HEAD={report['repo_head']}（构建树陈旧: 重新 configure 后重装；"
              f"结构判据不受影响）", file=sys.stderr)
    if missing:
        for rel in missing:
            print(f"MISSING REQUIRED {rel}", file=sys.stderr)
    for code, detail in violations:
        print(f"VIOLATION {code} {detail}", file=sys.stderr)
    if missing:
        print("MODULE VERIFY FAIL: required install-tree unit(s) missing "
              "(no static fallback)", file=sys.stderr)
        return 1
    if not manifest_ok:
        print(f"PRODUCT MANIFEST FAIL: {manifest_err}", file=sys.stderr)
        return 1
    if violations:
        print(f"INSTALL_TREE_VERIFY FAIL: {len(violations)} 处违例", file=sys.stderr)
        return 1
    print(f"INSTALL_TREE_VERIFY PASS: {report['declared_units']} units 闭合, "
          f"product manifest OK")
    return 0


# ── 负例注入自测（ENGINEERING_SPEC §8 可执行负例面）────────────────────────
def build_fixture(base: Path, repo: Path):
    """最小自洽夹具: repo_root(含 eng/packaging/VERSION) + prefix(合同全集在位)。"""
    fx_repo = base / "repo"
    (fx_repo / "eng").mkdir(parents=True, exist_ok=True)
    shutil.copytree(repo / "eng" / "packaging", fx_repo / "eng" / "packaging",
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "launch"))
    shutil.copy2(repo / VERSION_FILE, fx_repo / VERSION_FILE)
    contract = json.loads((fx_repo / "eng" / "packaging" / "install-tree.contract.json")
                          .read_text(encoding="utf-8"))
    manifest = json.loads((fx_repo / "eng" / "packaging" / MANIFEST).read_text(encoding="utf-8"))
    manifest["source_commit"] = repo_head(repo) or "0" * 40
    manifest["platform"] = "linux-amd64"
    for mu in manifest["units"]:
        if mu["rel_path"].endswith(".dll"):
            mu["rel_path"] = mu["rel_path"][:-4] + ".so"
    (fx_repo / "eng" / "packaging" / MANIFEST).write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    prefix = base / "prefix"
    for u in contract["units"]:
        p = prefix / u["install_path"]
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("fixture\n", encoding="utf-8")
    (prefix / MANIFEST).write_text(json.dumps(manifest, ensure_ascii=False, indent=2),
                                   encoding="utf-8")
    return fx_repo, prefix, fx_repo / "eng" / "packaging" / "install-tree.contract.json"


def _drift_version(prefix: Path):
    p = prefix / MANIFEST
    d = json.loads(p.read_text(encoding="utf-8"))
    d["product_version"] = "9.9.9"
    p.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")


def _drop_manifest_unit(prefix: Path):
    p = prefix / MANIFEST
    d = json.loads(p.read_text(encoding="utf-8"))
    d["units"] = d["units"][:-1]
    p.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")


def _hash_mismatch(prefix: Path):
    p = prefix / MANIFEST
    d = json.loads(p.read_text(encoding="utf-8"))
    d["units"][0]["sha256"] = "0" * 64
    p.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")


def self_test(repo: Path) -> int:
    ok = True
    cases = (
        ("positive (未注入)", lambda p, r: None),
        ("缺 required 模块", lambda p, r: (p / "modules" / "astrocs_noop.so").unlink()),
        ("白名单外多列文件", lambda p, r: (p / "junk.so").write_text("x", encoding="utf-8")),
        ("白名单外多列目录内文件",
         lambda p, r: (p / "junkdir").mkdir() or
         (p / "junkdir" / "x.so").write_text("x", encoding="utf-8")),
        ("manifest 漏 unit", lambda p, r: _drop_manifest_unit(p)),
        ("manifest 版本漂移", lambda p, r: _drift_version(p)),
        ("manifest sha256 不符", lambda p, r: _hash_mismatch(p)),
        ("manifest 声明文件缺失",
         lambda p, r: (p / "modules" / "astrocs_p1_drizzle.so").unlink()),
        ("manifest 非法 JSON",
         lambda p, r: (p / MANIFEST).write_text("{not json", encoding="utf-8")),
    )
    with tempfile.TemporaryDirectory(prefix="astrocs-installtree-selftest-") as tmp:
        base = Path(tmp)
        for i, (case, mutate) in enumerate(cases):
            work = base / f"case{i}"
            work.mkdir()
            fx_repo, prefix, contract = build_fixture(work, repo)
            mutate(prefix, fx_repo)
            try:
                violations, missing, manifest_ok, err, _ = evaluate(
                    fx_repo, prefix, contract)
            except InputUnavailable as e:
                print(f"SELFTEST FAIL {case}: 注入后输入不可用 {e}", file=sys.stderr)
                ok = False
                continue
            red = bool(violations or missing or not manifest_ok)
            good = (not red) if case.startswith("positive") else red
            detail = ("应绿" if case.startswith("positive") else "应红") + \
                     f", 实得 {'红' if red else '绿'} {violations or missing or err}"
            print(("SELFTEST PASS " if good else "SELFTEST FAIL ") + f"{case}: {detail}")
            ok = ok and good
    print("SELFTEST " + ("PASS (全部注入判红, 正例判绿)" if ok else "FAIL"))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="AstroCS 安装树 verify (BLD-003/W5-PKG-001)")
    ap.add_argument("--prefix", default="", help="安装前缀 (install prefix)")
    ap.add_argument("--contract", default="",
                    help="安装树合同 JSON (默认 <repo>/eng/packaging/install-tree.contract.json)")
    ap.add_argument("--repo", "--root", dest="repo", default="",
                    help="仓库根 (默认由脚本位置推导)")
    ap.add_argument("--expect-commit", default="",
                    help="要求 manifest.source_commit 逐字等于该 SHA")
    ap.add_argument("--json-out", default="", help="可选 JSON 报告输出路径")
    ap.add_argument("--self-test", action="store_true",
                    help="负例注入自测（机器可执行负例面）")
    args = ap.parse_args()

    repo = Path(args.repo).resolve() if args.repo else Path(__file__).resolve().parent.parent.parent
    if args.self_test:
        return self_test(repo)
    if not args.prefix:
        print("VERIFY FAIL: --prefix 必填 (或使用 --self-test)", file=sys.stderr)
        return 2
    prefix = Path(args.prefix).resolve()
    contract_path = (Path(args.contract) if args.contract
                     else repo / "eng" / "packaging" / "install-tree.contract.json")
    return run(repo, prefix, contract_path, args.expect_commit, args.json_out)


if __name__ == "__main__":
    raise SystemExit(main())

# NOTE (W5-PKG-001 判据边界): 安装树 source_commit 与仓库 HEAD 不一致只在
# 报告中标注 stale_build_dir 并打印 PROVENANCE NOTE, 不并入结构违例 ——
# 复用旧 build dir 是合法工作流（eng/tests/abi/mod001 就用既有 --build-dir 安装）,
# 把「构建树陈旧」混进结构判据会让既有门随机变红; 需要强约束时显式传
# --expect-commit <sha>（CI linux-main 在同一 run 内 configure+build+install）。
