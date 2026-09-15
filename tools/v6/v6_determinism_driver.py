#!/usr/bin/env python3
"""RUNTIME-CI-001 确定性驱动：V6 三模式入口 + Phase1/2/3 写盘路径的逐字节一致回归。

方法：
  * 独立（standalone）构建 tests/integration/v6_p1 | v6_p2 | v6_p3 的写盘测试；
  * 同一输入在**不同 CPU 预算**（taskset 1/2/4/8 核）下重复运行；
  * 对每个产物目录做**内容摘要**（相对路径 + 文件内容 SHA-256，排序后聚合；
    目录名/日志不参与），要求跨预算逐字节一致；
  * 产物缺失/运行失败/摘要不一致 → FAIL（rc 1）；构建或 taskset 缺失 → 清晰 FAIL（rc 2）。

用法:
  python3 tools/v6/v6_determinism_driver.py [--repo <root>] [--work-root <dir>]
        [--budgets 1,2,4,8] [--skip-build] [--json-out <path>]
exit 0 = 全部写盘路径跨预算逐字节一致；1 = 不一致；2 = 环境/构建缺失（fail-closed）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

DEFAULT_REPO = pathlib.Path(__file__).resolve().parents[2]

# V6 三模式入口 + Phase1/2/3 写盘路径（各自 standalone 构建）。
SPECS = [
    {"name": "v6_p1_phase1_write", "source": "tests/integration/v6_p1",
     "binary": "v6_p1_integrate_test", "args": ["positive", "{work}"],
     "products": "{work}"},
    {"name": "v6_p2_three_modes_write", "source": "tests/integration/v6_p2",
     "binary": "v6_p2_integrate_test", "args": ["write", "{work}"],
     "products": "{work}"},
    {"name": "v6_p3_export_write", "source": "tests/integration/v6_p3",
     "binary": "v6_p3_export_test", "args": ["positive", "{art}"],
     "products": "{art}"},
]


def _sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


TEXT_SUFFIXES = {".json", ".txt", ".csv", ".md", ".yaml", ".yml"}


def content_digest(root: pathlib.Path, normalize_dirs=None) -> dict:
    """相对路径 + 文件内容 的聚合摘要（不含目录名/日志）。

    normalize_dirs: 把文本/JSON 产物中出现的这些**运行沙盒绝对路径**替换为 @RUNDIR@
    后再哈希。理由与 p1drz_taskset_invariance.sh 同口径：产物中记录的输入绝对路径
    是「运行落点」元数据，随每次运行的沙盒目录而变，不属于科学载荷；科学载荷
    （FITS 科学数组与数值字段）仍逐字节比较。FITS 等二进制不受影响。
    """
    tokens = []
    if normalize_dirs:
        for d in normalize_dirs:
            for cand in (str(d), str(pathlib.Path(d).resolve())):
                b = cand.encode("utf-8")
                if b not in tokens:
                    tokens.append(b)
        tokens.sort(key=len, reverse=True)
    files = []
    for p in sorted(root.rglob("*")):
        if not p.is_file():
            continue
        if p.suffix == ".log":
            continue
        if tokens and p.suffix.lower() in TEXT_SUFFIXES:
            data = p.read_bytes()
            for t in tokens:
                data = data.replace(t, b"@RUNDIR@")
            h = hashlib.sha256(data).hexdigest()
        else:
            h = _sha256_file(p)
        files.append((str(p.relative_to(root)).replace(os.sep, "/"), h))
    agg = hashlib.sha256()
    for rel, h in files:
        agg.update(rel.encode("utf-8"))
        agg.update(b"\x00")
        agg.update(h.encode("ascii"))
        agg.update(b"\n")
    return {"n_files": len(files), "digest": agg.hexdigest(),
            "files": {rel: h for rel, h in files}}


def cpu_list() -> list:
    try:
        out = subprocess.run(["taskset", "-pc", str(os.getpid())],
                             capture_output=True, text=True, timeout=20)
        if out.returncode != 0:
            return []
        spec = out.stdout.split(":")[-1].strip()
    except (OSError, subprocess.SubprocessError):
        return []
    cpus = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-")[:2]
            cpus += list(range(int(a), int(b) + 1))
        else:
            cpus.append(int(part))
    return cpus


def build_spec(repo: pathlib.Path, work_root: pathlib.Path, spec: dict) -> tuple:
    src = repo / spec["source"]
    bdir = work_root / "build" / spec["name"]
    bdir.mkdir(parents=True, exist_ok=True)
    cm = subprocess.run(["cmake", "-S", str(src), "-B", str(bdir),
                         "-DCMAKE_BUILD_TYPE=Release"],
                        capture_output=True, text=True, timeout=900)
    if cm.returncode != 0:
        return False, "cmake configure failed: " + cm.stderr[-800:]
    bd = subprocess.run(["cmake", "--build", str(bdir), "-j",
                         str(min(8, os.cpu_count() or 4))],
                        capture_output=True, text=True, timeout=1800)
    if bd.returncode != 0:
        return False, "build failed: " + bd.stderr[-800:]
    return True, str(bdir)


def run_spec(repo: pathlib.Path, work_root: pathlib.Path, spec: dict,
             budgets: list, cpus: list, skip_build: bool) -> dict:
    res = {"name": spec["name"], "source": spec["source"], "runs": [],
           "ok": False, "error": ""}
    bdir = work_root / "build" / spec["name"]
    if not skip_build or not bdir.exists():
        ok, msg = build_spec(repo, work_root, spec)
        if not ok:
            res["error"] = msg
            return res
    binary = bdir / spec["binary"]
    if not binary.exists():
        res["error"] = "binary missing: %s" % binary
        return res
    digests = {}
    for b in budgets:
        run_dir = work_root / "runs" / spec["name"] / ("b%d" % b)
        if run_dir.exists():
            shutil.rmtree(run_dir)
        run_dir.mkdir(parents=True, exist_ok=True)
        art = run_dir / "artifacts"
        art.mkdir(parents=True, exist_ok=True)
        argv = [str(binary)]
        for a in spec["args"]:
            argv.append(a.format(work=str(run_dir), art=str(art)))
        sel = ",".join(str(c) for c in cpus[:b])
        cmd = ["taskset", "-c", sel] + argv
        log = run_dir / "run.log"
        with open(log, "w", encoding="utf-8") as lf:
            p = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, timeout=1800)
        prod_root = pathlib.Path(spec["products"].format(work=str(run_dir), art=str(art)))
        if p.returncode != 0:
            res["runs"].append({"budget": b, "rc": p.returncode, "cpus": sel,
                                "digest": None, "n_files": 0})
            res["error"] = "run failed at budget %d (rc=%d); see %s" % (b, p.returncode, log)
            return res
        if not prod_root.exists():
            res["error"] = "product root missing: %s" % prod_root
            return res
        dig = content_digest(prod_root, normalize_dirs=[run_dir])
        if dig["n_files"] == 0:
            res["error"] = "no product files under %s (zero-product run cannot PASS)" % prod_root
            return res
        # 敏感性自检: 载荷改 1 字节必须改变摘要（防归一化/摘要实现把门做成恒绿）。
        pivot = None
        for q in sorted(prod_root.rglob("*")):
            if q.is_file() and q.suffix.lower() in ("", ".fits", ".dat", ".bin", ".txt", ".json"):
                if q.suffix.lower() == ".json":
                    continue
                pivot = q
                break
        if pivot is not None:
            tmp = run_dir / "_sensitivity"
            tmp.mkdir(parents=True, exist_ok=True)
            target = tmp / (pivot.name + ".probe")
            data = bytearray(pivot.read_bytes())
            if data:
                data[0] ^= 0x01
            target.write_bytes(bytes(data))
            probe_root = tmp
            pdig = content_digest(probe_root, normalize_dirs=[run_dir])
            if pdig["digest"] == dig["digest"]:
                res["error"] = ("sensitivity self-check failed: payload byte flip did not "
                                "change digest (determinism gate is blind)")
                return res
        digests[b] = dig["digest"]
        res["runs"].append({"budget": b, "rc": p.returncode, "cpus": sel,
                            "digest": dig["digest"], "n_files": dig["n_files"]})
    uniq = set(digests.values())
    res["ok"] = (len(uniq) == 1 and len(digests) == len(budgets))
    if not res["ok"]:
        res["error"] = "digest mismatch across budgets: %s" % json.dumps(digests)
    return res


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=str(DEFAULT_REPO))
    ap.add_argument("--work-root", default="")
    ap.add_argument("--budgets", default="1,2,4,8")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--json-out", default="")
    ap.add_argument("--only", default="", help="只跑匹配的子集（name 子串，逗号分隔）")
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo).resolve()
    if not sys.platform.startswith("linux"):
        print("V6_DETERMINISM_FAIL: platform not linux (%s); taskset budget variation "
              "unavailable -> fail-closed" % sys.platform, file=sys.stderr)
        return 2
    if shutil.which("taskset") is None:
        print("V6_DETERMINISM_FAIL: taskset unavailable -> cannot vary CPU budget "
              "(fail-closed; skip-only is not PASS)", file=sys.stderr)
        return 2
    cpus = cpu_list()
    if len(cpus) < 2:
        print("V6_DETERMINISM_FAIL: <2 usable CPUs -> cannot vary budget (fail-closed)",
              file=sys.stderr)
        return 2
    budgets = [int(x) for x in args.budgets.split(",") if x.strip()]
    budgets = [b for b in budgets if b <= len(cpus)]
    if len(budgets) < 2:
        print("V6_DETERMINISM_FAIL: <2 budgets within available CPUs", file=sys.stderr)
        return 2
    work_root = pathlib.Path(args.work_root) if args.work_root else (repo / "run" / "v6" / "RUNTIME-CI-001" / "determinism")
    work_root.mkdir(parents=True, exist_ok=True)
    only = [s for s in args.only.split(",") if s.strip()]
    specs = [s for s in SPECS if (not only or any(o in s["name"] for o in only))]

    results = []
    all_ok = True
    env_fail = False
    for spec in specs:
        r = run_spec(repo, work_root, spec, budgets, cpus, args.skip_build)
        results.append(r)
        if not r["ok"]:
            all_ok = False
            if "build failed" in r["error"] or "cmake configure" in r["error"]:
                env_fail = True
        print("[%s] %s budgets=%s files=%s" % (
            "PASS" if r["ok"] else "FAIL", r["name"], budgets,
            [x["n_files"] for x in r["runs"]] if r["runs"] else "[]"))
        if not r["ok"]:
            print("   -> %s" % r["error"])
    out = {"schema": "astrocs.v6.determinism/v1", "budgets": budgets,
           "results": results, "verdict": "PASS" if all_ok else "FAIL"}
    if args.json_out:
        p = pathlib.Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if all_ok:
        print("V6_DETERMINISM_PASS specs=%d budgets=%s" % (len(specs), budgets))
        return 0
    print("V6_DETERMINISM_FAIL")
    return 2 if env_fail else 1


if __name__ == "__main__":
    raise SystemExit(main())
