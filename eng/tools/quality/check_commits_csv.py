#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_commits_csv.py — GOV-001 COMMITS.csv 验证器（**已退役，非门禁**）。

退役判定依据
  1) 判据对象全仓已不存在：COMMITS.csv 在活动树零命中（仅 run/CLEAN-402/backup/
     reports/REAUDIT_V3/** 的历史备份里留有旧副本，属 run/ 临时面）。
  2) 判据面已被在册门承接：提交纪律由 AGENTS.md §8 与 eng/ci 的在册检查项
     （CHK-KNOWN-FAILURES-BASELINE / CHK-IMPACT-MAP 等）承担；本脚本在
     eng/ci/checks.json 与 docs/ci/01_CHECKS.md §2 中零引用。
  3) 负例入口自身已坏：原 --selftest 在生成器产不出行时直接 rows[0] ⇒
     IndexError 裸 traceback（实跑 rc=1），违反 docs/ci/01_CHECKS.md §1:14。
     本次一并订正：夹具不可构造时**显式具名**失败/跳过，不 traceback、不静默。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check --commits CSV 复跑（判据未改）；
  * 本检查器不写任何产物文件（--selftest 只在 tempfile 目录里造夹具）
    ⇒ 不存在「产物落仓库根」的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

def _deduce_root() -> str:
    """仓库根推导：向上找到同时含 VERSION 与 eng/tools 的目录（可移植）。

    注：本文件位于 eng/tools/quality/（深度 3），固定层数的 dirname 链会把根算成
    <repo>/eng —— 实测会打印 ANCHOR_STALE: SCHEMA_REL <repo>/eng/eng/contracts/...
    这类错锚路径。改为按标记文件上溯，移动文件不再算错根。
    """
    cur = os.path.dirname(os.path.abspath(__file__))
    for _ in range(6):
        if (os.path.isfile(os.path.join(cur, "VERSION"))
                and os.path.isdir(os.path.join(cur, "eng", "tools"))):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


REPO = _deduce_root()

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活） ────────────────────────────
RESULTS_DIR_REL = os.path.join("evidence", "v6_1_rework", "tasks")
GEN_TOOL_REL = os.path.join("eng", "tools", "quality", "gen_commits_csv.py")

REQUIRED = {"task_id", "parent_commit", "result_commit", "commit_subject",
            "committed_utc", "pushed_origin_main", "changed_paths_sha256",
            "task_result_sha256"}
SHA40 = set("0123456789abcdef")

RETIREMENT_MARKER = (
    "RETIRED: check_commits_csv.py（GOV-001）未注册进 eng/ci/checks.json；"
    "判据对象 COMMITS.csv 全仓已不存在（仅 run/ 历史备份留有副本）。"
)


def git(repo: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(repo), *args],
                          capture_output=True, text=True, timeout=60)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_40hex(value: str) -> bool:
    return len(value) == 40 and all(c in SHA40 for c in value)


def check(repo: Path, commits_csv: Path, results_dir: Path) -> list[str]:
    errors: list[str] = []
    with commits_csv.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or not REQUIRED.issubset(set(reader.fieldnames)):
            return ["COMMITS.csv columns mismatch: %s" % (reader.fieldnames,)]
        rows = list(reader)

    seen_tasks: set[str] = set()
    seen_commits: dict[str, list[str]] = {}
    for row in rows:
        task_id, result, parent = row["task_id"], row["result_commit"], row["parent_commit"]
        if task_id in seen_tasks:
            errors.append("duplicate task_id: %s" % task_id)
        seen_tasks.add(task_id)
        # 同一 commit 可由双任务合并提交共享(如 "CPU-001/CPU-002:")，但必须 subject 声明两者
        if result in seen_commits:
            owners = seen_commits[result]
            subj = row["commit_subject"]
            all_declared = all("%s:" % o in subj or "%s/" % o in subj for o in owners + [task_id])
            if not all_declared:
                errors.append("duplicate result_commit %s for %s（未声明为合并提交）"
                              % (result, task_id))
        seen_commits.setdefault(result, []).append(task_id)
        if not is_40hex(result) or not is_40hex(parent):
            errors.append("%s: bad sha (result=%s parent=%s)" % (task_id, result, parent))
        # subject 以 <TASK-ID>: 开头; 允许双任务合并前缀 "CPU-001/CPU-002:"
        subj_ok = row["commit_subject"].startswith(task_id + ":") or \
                  ("/" in row["commit_subject"].split(":")[0] and
                   task_id + "/" in row["commit_subject"].split(":")[0] + "/")
        if not subj_ok:
            errors.append("%s: subject does not start with task id" % task_id)
        # parent chain
        proc = git(repo, "rev-parse", result + "^")
        actual_parent = proc.stdout.strip() if proc.returncode == 0 else ""
        if actual_parent != parent:
            errors.append("%s: result parent %s != parent_commit %s"
                          % (task_id, actual_parent, parent))
        # ancestor of HEAD/main
        if git(repo, "merge-base", "--is-ancestor", result, "main").returncode != 0:
            errors.append("%s: result %s is not an ancestor of main" % (task_id, result))
        # pushed to origin/main
        pushed = row["pushed_origin_main"].lower() == "true"
        on_origin = git(repo, "merge-base", "--is-ancestor", result, "origin/main").returncode == 0
        if pushed and not on_origin:
            errors.append("%s: claims pushed but result %s not on origin/main" % (task_id, result))
        if not pushed and on_origin:
            errors.append("%s: claims unpushed but result %s IS on origin/main" % (task_id, result))
        # committed_utc
        actual_utc = git(repo, "log", "-1", "--format=%cI", result).stdout.strip()
        if row["committed_utc"] != actual_utc:
            errors.append("%s: committed_utc mismatch %s != %s"
                          % (task_id, row["committed_utc"], actual_utc))
        # changed_paths_sha256
        paths = git(repo, "show", "--name-only", "--format=", result).stdout
        h = hashlib.sha256()
        h.update(paths.encode("utf-8"))
        if row["changed_paths_sha256"] != h.hexdigest():
            errors.append("%s: changed_paths_sha256 mismatch" % task_id)
        # task_result_sha256
        result_path = results_dir / task_id / "TASK_RESULT.json"
        if not result_path.is_file():
            errors.append("%s: TASK_RESULT.json missing" % task_id)
        elif row["task_result_sha256"] != sha256(result_path):
            errors.append("%s: task_result_sha256 mismatch" % task_id)
    return errors


def legacy_main(repo: Path, commits_csv: Path | None, results_dir: Path) -> int:
    """退役前的原判定实现（判据未改；只把裸读改成具名锚检查）。"""
    if commits_csv is None:
        print("COMMITS_CHECK_FAIL: --commits required", file=sys.stderr)
        return 3
    if not commits_csv.is_file():
        print("ANCHOR_STALE: COMMITS_CSV %s" % commits_csv, file=sys.stderr)
        return 2
    if not results_dir.is_dir():
        print("ANCHOR_STALE: RESULTS_DIR_REL %s"
              % RESULTS_DIR_REL.replace(os.sep, "/"), file=sys.stderr)
        return 2
    errors = check(repo, commits_csv, results_dir)
    if errors:
        print("COMMITS_CHECK_FAIL (%d)" % len(errors))
        for err in errors[:40]:
            print("  - %s" % err, file=sys.stderr)
        return 1
    print("COMMITS_CHECK_PASS")
    return 0


def legacy_selftest(repo: Path, results_dir: Path | None = None,
                     gen: Path | None = None,
                     subjects: list | None = None) -> int:
    """原 --selftest 的三类篡改夹具（本次订正：夹具造不出时具名失败/跳过，不 IndexError）。

    返回 0 = 三类篡改全被抓 + real PASS；1 = 具名失败；2 = 前置不可用（显式 SKIP，
    **不计入 PASS**）。results_dir/gen/subjects 可注入，供 --self-test 直接验证
    「生成器失败」「行数不足」两条原本会 IndexError 的路径。
    """
    import re
    if results_dir is None:
        results_dir = (repo / RESULTS_DIR_REL).resolve()
    if gen is None:
        gen = (repo / GEN_TOOL_REL).resolve()
    if not Path(results_dir).is_dir():
        print("SELFTEST_SKIP legacy_fixtures: ANCHOR_STALE: RESULTS_DIR_REL %s"
              % RESULTS_DIR_REL.replace(os.sep, "/"))
        return 2
    if not Path(gen).is_file():
        print("SELFTEST_SKIP legacy_fixtures: ANCHOR_STALE: GEN_TOOL_REL %s"
              % GEN_TOOL_REL.replace(os.sep, "/"))
        return 2
    if subjects is None:
        proc = git(repo, "log", "--first-parent", "--format=%H %s", "main")
        subjects = []
        for line in proc.stdout.splitlines():
            m = re.match(r"^([0-9a-f]{40}) ([A-Z0-9]+-[0-9]{3}):", line)
            if m:
                subjects.append((m.group(1), m.group(2)))
    if len(subjects) < 2:
        print("SELFTEST_SKIP legacy_fixtures: need at least 2 task commits, got %d"
              % len(subjects))
        return 2

    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        real = tmpd / "COMMITS_real.csv"
        genproc = subprocess.run(
            [sys.executable, str(gen), "--repo", str(repo),
             "--results-dir", str(results_dir), "--out", str(real)],
            capture_output=True, text=True, timeout=120)
        if genproc.returncode != 0:
            # 订正点：原实现在这里继续往下走，随后 rows[0] ⇒ IndexError 裸 traceback。
            print("SELFTEST_FAIL legacy_fixtures: 生成器失败 rc=%d: %s"
                  % (genproc.returncode, genproc.stderr.strip()[:200]), file=sys.stderr)
            return 1
        with real.open(encoding="utf-8", newline="") as fh:
            rows = list(csv.DictReader(fh))
        if len(rows) < 2:
            # 订正点：行数不足 ⇒ 具名失败，不再 rows[0]/rows[1] 越界。
            print("SELFTEST_FAIL legacy_fixtures: 生成器只产出 %d 行（需 >= 2）"
                  % len(rows), file=sys.stderr)
            return 1

        def _write(rows_, path_):
            with path_.open("w", encoding="utf-8", newline="") as fh:
                w = csv.DictWriter(fh, fieldnames=list(rows_[0].keys()))
                w.writeheader()
                w.writerows(rows_)
            return path_

        rows_a = [dict(r) for r in rows]
        rows_a[0]["parent_commit"] = "0" * 40
        errs = check(repo, _write(rows_a, tmpd / "COMMITS_tamper.csv"), results_dir)
        if not any("parent" in e for e in errs):
            print("SELFTEST_FAIL: tampered parent not caught", file=sys.stderr)
            return 1

        rows_b = [dict(r) for r in rows]
        rows_b[0]["result_commit"], rows_b[1]["result_commit"] = \
            rows_b[1]["result_commit"], rows_b[0]["result_commit"]
        errs = check(repo, _write(rows_b, tmpd / "COMMITS_swap.csv"), results_dir)
        if not any("parent" in e or "subject" in e for e in errs):
            print("SELFTEST_FAIL: swapped commits not caught", file=sys.stderr)
            return 1

        rows_c = [dict(r) for r in rows]
        rows_c[0]["pushed_origin_main"] = "false"
        errs = check(repo, _write(rows_c, tmpd / "COMMITS_unpushed.csv"), results_dir)
        if not any("pushed" in e or "origin" in e for e in errs):
            print("SELFTEST_FAIL: unpushed not caught", file=sys.stderr)
            return 1

        errs = check(repo, real, results_dir)
        if errs:
            print("SELFTEST_FAIL: real COMMITS fails: %s" % errs[:5], file=sys.stderr)
            return 1
    print("SELFTEST_PASS legacy_fixtures: tamper/swap/unpushed all caught + real PASS")
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _run_cli(args: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=600)


def _self_test(repo: Path) -> int:
    """可执行正/负例面：退役契约（§1:10–11）+ 原夹具面（可构造时）。"""
    problems: list[str] = []
    cases = 0

    def check_case(name: str, rc: int, want_rc: int, blob: str, token: str) -> None:
        nonlocal cases
        cases += 1
        ok = rc == want_rc and token in blob
        print("  SELFTEST_%s %-34s rc=%d want_rc=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing" % (name, rc, want_rc, token))

    r = _run_cli([])
    check_case("P0_retired_noarg", r.returncode, 2, r.stdout + r.stderr, "RETIRED")

    # 显式入口缺 --commits ⇒ 用法错误 rc=3（具名，不 traceback）
    r = _run_cli(["--legacy-check"])
    check_case("N1_legacy_requires_commits", r.returncode, 3, r.stdout + r.stderr,
               "--commits required")

    # 锚缺失：--commits 指向不存在文件 ⇒ ANCHOR_STALE rc=2
    r = _run_cli(["--legacy-check", "--commits", os.path.join(str(repo), "no_such.csv")])
    check_case("N2_commits_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
               "ANCHOR_STALE: COMMITS_CSV")

    # 锚缺失：results-dir 不存在 ⇒ ANCHOR_STALE rc=2
    with tempfile.TemporaryDirectory() as tmp:
        csvp = Path(tmp) / "COMMITS.csv"
        csvp.write_text(",".join(sorted(REQUIRED)) + "\n", encoding="utf-8")
        r = _run_cli(["--legacy-check", "--commits", str(csvp),
                      "--results-dir", os.path.join(tmp, "no_such_dir")])
        check_case("N3_results_dir_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
                   "ANCHOR_STALE: RESULTS_DIR_REL")

    # 原夹具面（真仓可构造时）：本次订正后不得再 IndexError
    rc_legacy = legacy_selftest(repo)
    if rc_legacy == 2:
        print("  SELFTEST_SKIP %-34s（前置不可用，见上一行；不计入 PASS）"
              % "P4_legacy_fixtures_real")
    else:
        cases += 1
        if rc_legacy != 0:
            problems.append("legacy_selftest rc=%d" % rc_legacy)
        print("  SELFTEST_%s %-34s rc=%d" % ("PASS" if rc_legacy == 0 else "FAIL",
                                            "P4_legacy_fixtures_real", rc_legacy))

    # 订正实证：原实现在这两条路径上都会 rows[0] ⇒ IndexError 裸 traceback。
    # 直接注入假生成器复现两条路径，断言现在都是**具名失败**。
    import contextlib
    import io
    with tempfile.TemporaryDirectory() as tmp:
        fake_results = Path(tmp) / "results"
        (fake_results / "X-001").mkdir(parents=True)
        fake_gen_fail = Path(tmp) / "gen_fail.py"
        fake_gen_fail.write_text(
            "import sys" + chr(10) + "sys.stderr.write('boom'" + chr(10) +
            "sys.exit(1)" + chr(10), encoding="utf-8")
        fake_gen_one = Path(tmp) / "gen_one.py"
        fake_gen_one.write_text(
            "import sys, csv" + chr(10) +
            "w = csv.writer(open(sys.argv[sys.argv.index('--out') + 1], 'w', newline=''))" +
            chr(10) + "w.writerow(['changed_paths_sha256','commit_subject'," +
            "'committed_utc','parent_commit','pushed_origin_main','result_commit'," +
            "'task_id','task_result_sha256'])" + chr(10) +
            "w.writerow(['X-001'] + ['x'] * 7)" + chr(10),
            encoding="utf-8")
        for name, genp, token in (("N4_legacy_gen_failed_named", fake_gen_fail, "生成器失败"),
                                  ("N5_legacy_rows_insufficient_named", fake_gen_one,
                                   "只产出 1 行")):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
                rc = legacy_selftest(repo, results_dir=fake_results, gen=genp,
                                     subjects=[("a" * 40, "X-001"), ("b" * 40, "X-002")])
            check_case(name, rc, 1, buf.getvalue(), token)

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--legacy-check", action="store_true", dest="legacy_check",
                        help="复跑退役前的原判定实现（判据未改）")
    parser.add_argument("--repo", type=Path, default=Path(REPO))
    parser.add_argument("--commits", type=Path)
    parser.add_argument("--results-dir", type=Path, default=None)
    parser.add_argument("--self-test", "--selftest", action="store_true", dest="self_test")
    args = parser.parse_args(argv)

    repo: Path = args.repo.resolve()
    results_dir = args.results_dir or (repo / RESULTS_DIR_REL)

    if args.self_test:
        return _self_test(repo)
    if not args.legacy_check:
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check --commits CSV [--repo DIR] [--results-dir DIR]",
              file=sys.stderr)
        return 2
    return legacy_main(repo, args.commits.resolve() if args.commits else None, results_dir)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
