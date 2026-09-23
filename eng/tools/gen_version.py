#!/usr/bin/env python3
"""VER-001 唯一版本源生成接口。
读取仓库根 VERSION（唯一权威版本源）+ git 状态，输出 --version --json 合同对象。
规则见 docs/VERSIONING.md；版本格式合同见控制包 13_ALPHA_VERSION_AND_PHASE3.md §1。

git 面与 fail-closed（规范依据 docs/ci/01_CHECKS.md §1「不得 traceback」「fail-closed」、
ENGINEERING_SPEC.md §10/§11「降级必须显式」）：
  * 合同对象里的 "+g<commit12>" 分量**只能**来自 git HEAD —— 没有 git 就没有诚实的
    版本串。原实现用 subprocess.run(..., check=True)：非 git 工作树 ⇒
    CalledProcessError traceback；git 可执行文件缺失 ⇒ FileNotFoundError traceback
    （两条均在 run/DEFECT-REPRO-01 复现）。traceback 文本被调用方
    eng/tools/doccheck/check_version_namespaces.py 原样塞进检查明细，把「依赖不可用」
    误报成「版本链不符」—— 报错语义错位。
  * 现行为 = **显式降级 + 具名留痕 + fail-closed**，先例：
    - eng/ci/check_version.py::anchor_status（非 git 工作树 ⇒ 跳过跟踪复核并留痕）；
    - eng/tools/traceability/check_traceability_matrix.py（GIT_UNAVAILABLE ⇒ rc=2）。
    具体：
      - stderr 打印 GIT_UNAVAILABLE: <具名原因>；
      - --json 打印**降级记录**（git_available=false / version=null /
        base_version=<根 VERSION> / degraded_reason=<原因>），**不产出合同对象**
        —— 防止消费方把 base 串当合法版本串静默接受（那会是新的假绿）；
      - 非 --json 只打印根 VERSION 基础号；
      - 退出码 2（依赖不可用 ⇒ 不给结论，不判绿）。
退出码：0 = 合同对象已产出；2 = GIT_UNAVAILABLE（fail-closed）；
        1 = VERSION 源非法（SystemExit 文本）。
用法：
  python3 eng/tools/gen_version.py [--json]
  python3 eng/tools/gen_version.py --self-test   # 正例（真仓）+ 负例（临时非 git 树 / 无 git）
"""
import json, os, re, shutil, subprocess, sys, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SEMVER_ALPHA = re.compile(r"^(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$")
# ABI/schema 版本的唯一定义点: ABI-001/ API-002 冻结后置 1
ABI_VERSION = "0"
CLI_SCHEMA_VERSION = "0"
FORBIDDEN_PRERELEASE = ("stable", "rc", "beta", "release")
GIT_TIMEOUT_S = 30


class GitUnavailable(Exception):
    """git 不可用 / 非 git 工作树 —— fail-closed，rc=2，具名留痕 GIT_UNAVAILABLE。"""


def read_base_version(path=None):
    p = path or os.path.join(REPO, "VERSION")
    with open(p, encoding="utf-8") as f:
        raw = f.read().strip()
    if not SEMVER_ALPHA.match(raw):
        raise SystemExit(f"VERSION 源非法(必须 MAJOR.MINOR.PATCH-alpha.N, 拒绝 {FORBIDDEN_PRERELEASE}): {raw!r}")
    low = raw.lower()
    if any(t in low for t in FORBIDDEN_PRERELEASE):
        raise SystemExit(f"VERSION 源含禁止的 prerelease 标记: {raw}")
    return raw


def _git(args):
    """(ok, stdout, reason)。任何不可用形态都返回**具名 reason**，绝不抛异常。"""
    try:
        p = subprocess.run(["git", *args], cwd=REPO, capture_output=True,
                           text=True, timeout=GIT_TIMEOUT_S)
    except FileNotFoundError:
        return False, "", "git 可执行文件不存在（PATH 上找不到 git）"
    except subprocess.TimeoutExpired:
        return False, "", "git %s 超时（%ds）" % (" ".join(args), GIT_TIMEOUT_S)
    except OSError as exc:                                    # noqa: BLE001
        return False, "", "git %s 调用失败: %s" % (" ".join(args), exc)
    if p.returncode != 0:
        last = (p.stderr.strip().splitlines() or [""])[-1]
        return False, "", "git %s 失败 rc=%d（%s）" % (" ".join(args), p.returncode, last[:160])
    return True, p.stdout.strip(), ""


def git(args):
    """兼容旧签名：成功返回 stdout；不可用 ⇒ GitUnavailable（不再 traceback）。"""
    ok, out, why = _git(args)
    if not ok:
        raise GitUnavailable(why)
    return out


def degraded_report(reason):
    """显式降级记录。**不是**合同对象：version 为 null，消费方不得当版本串使用。"""
    try:
        base = read_base_version()
    except (SystemExit, OSError):
        base = None
    return {
        "version": None,
        "base_version": base,
        "prerelease": "alpha",
        "commit": None,
        "dirty": None,
        "build_id": None,
        "abi_version": ABI_VERSION,
        "cli_schema_version": CLI_SCHEMA_VERSION,
        "git_available": False,
        "degraded_reason": "GIT_UNAVAILABLE: %s" % reason,
    }


def build_report(base=None, commit=None, dirty=None):
    base = base or read_base_version()
    commit = commit or git(["rev-parse", "HEAD"])
    if dirty is None:
        dirty = bool(git(["status", "--porcelain"]))
    c12 = commit[:12]
    suffix = f"+g{c12}" + (".dirty" if dirty else "")
    return {
        "version": f"{base}{suffix}",
        "prerelease": "alpha",
        "commit": commit,
        "dirty": dirty,
        "build_id": f"g{c12}" + (".dirty" if dirty else ""),
        "abi_version": ABI_VERSION,
        "cli_schema_version": CLI_SCHEMA_VERSION,
    }


def _run(script, cwd, env=None):
    return subprocess.run([sys.executable, script, "--json"], cwd=cwd, env=env,
                          capture_output=True, text=True, timeout=120)


def _self_test():
    """正例（真仓合同对象）+ 负例（非 git 树 / git 缺失 ⇒ 具名 GIT_UNAVAILABLE，不 traceback）。"""
    problems = []
    me = os.path.abspath(__file__)
    # 正例：真仓库（git 可用）⇒ rc=0 且产出合同对象
    pos = _run(me, REPO)
    try:
        rep = json.loads(pos.stdout)
    except Exception:                                         # noqa: BLE001
        rep = {}
    ver = str(rep.get("version") or "")
    if pos.returncode != 0 or not ver.startswith(read_base_version() + "+g"):
        problems.append("正例未产出合同对象: rc=%d stdout=%r" % (pos.returncode, pos.stdout[:120]))
    # 负例 1：临时非 git 树（GIT_CEILING_DIRECTORIES 阻断向上寻找 .git）
    tmp = tempfile.mkdtemp(prefix="astrocs_genver_")
    try:
        os.makedirs(os.path.join(tmp, "eng", "tools"))
        shutil.copy2(me, os.path.join(tmp, "eng", "tools", "gen_version.py"))
        shutil.copy2(os.path.join(REPO, "VERSION"), os.path.join(tmp, "VERSION"))
        env = dict(os.environ, GIT_CEILING_DIRECTORIES=tmp)
        r = _run(os.path.join(tmp, "eng", "tools", "gen_version.py"), tmp, env)
        if r.returncode != 2:
            problems.append("负例(非 git 树) 期望 rc=2，实得 %d" % r.returncode)
        if "GIT_UNAVAILABLE" not in r.stderr:
            problems.append("负例(非 git 树) stderr 缺具名 GIT_UNAVAILABLE: %r" % r.stderr[:160])
        if "Traceback" in r.stderr or "Traceback" in r.stdout:
            problems.append("负例(非 git 树) 出现 traceback（§1 禁止）")
        try:
            deg = json.loads(r.stdout)
        except Exception:                                     # noqa: BLE001
            deg = None
        if not deg or deg.get("git_available") is not False or deg.get("version") is not None:
            problems.append("负例(非 git 树) 降级记录形态不符: %r" % (r.stdout[:160],))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    # 负例 2：git 可执行文件不在 PATH（FileNotFoundError 面）
    empty = tempfile.mkdtemp(prefix="astrocs_nopath_")
    try:
        env = dict(os.environ, PATH=empty)
        r = _run(me, REPO, env)
        if r.returncode != 2 or "GIT_UNAVAILABLE" not in r.stderr:
            problems.append("负例(无 git) 期望 rc=2 + GIT_UNAVAILABLE，实得 rc=%d stderr=%r"
                            % (r.returncode, r.stderr[:160]))
        if "Traceback" in r.stderr:
            problems.append("负例(无 git) 出现 traceback（§1 禁止）")
    finally:
        shutil.rmtree(empty, ignore_errors=True)
    for p in problems:
        print("  - %s" % p)
    print("SELF_TEST %s positives=1 negatives=2" % ("PASS" if not problems else "FAIL"))
    return 0 if not problems else 1


def main():
    if "--self-test" in sys.argv:
        return _self_test()
    try:
        rep = build_report()
    except GitUnavailable as exc:
        # 显式降级 + 具名留痕（check_version.py::anchor_status 的非 git 树先例）：
        # 版本串的 +g<sha> 分量不可得 ⇒ 不产出合同对象、不判绿，rc=2 fail-closed。
        print("GIT_UNAVAILABLE: %s" % exc, file=sys.stderr)
        if "--json" in sys.argv:
            print(json.dumps(degraded_report(str(exc)), ensure_ascii=False, indent=1))
        else:
            try:
                print(read_base_version())
            except (SystemExit, OSError):
                pass
        return 2
    if "--json" in sys.argv:
        print(json.dumps(rep, ensure_ascii=False, indent=1))
    else:
        print(rep["version"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
