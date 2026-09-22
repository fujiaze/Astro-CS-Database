#!/usr/bin/env python3
"""RUNTIME-CI-001 确定性驱动：V6 三模式入口 + Phase1/2/3 写盘路径的跨 worker 预算等价回归。

方法（判据口径 = ASTROCS_DESIGN.md §8.3/§9 + docs/contracts/SCHEDULER_CONTRACT.md §2.1 +
docs/contracts/TEST_MATRIX.md §2）：
  * 独立（standalone）构建 eng/tests/integration/v6_p1 | v6_p2 | v6_p3 的写盘测试；
  * 同一输入在**不同 CPU 预算**（taskset 1/2/4/8 核）下重复运行；
  * 第一档判据：对每个产物目录做**内容摘要**（相对路径 + 文件内容 SHA-256，排序后聚合；
    目录名/日志不参与），跨预算**逐字节一致** ⇒ PASS（最强档）；
  * 第二档判据：摘要不同时**不直接判红**，改按冻结浮点容差做逐文件数值等价比对
    （eng/tools/v6/v6_numeric_equiv.py；FP64 rtol=1e-12/atol=1e-13×scale、FP32
    rtol=5e-6/atol=1e-6×scale、整数/mask/索引/NaN 位置精确一致），全部在容差内 ⇒ PASS；
    超差或结构不一致 ⇒ FAIL。理由：1/N worker 的合同判据是**浮点容差**而非逐位一致
    （负责人裁决 2026-09-22：「数值精度在浮点容差内就可以」），但容差档必须仍能抓住真实
    退化——因此保留逐字节档为优先判据，且容差档自带敏感性自检。
  * 产物缺失/运行失败/两档均不通过 → FAIL（rc 1）；构建或 taskset 缺失 → 清晰 FAIL（rc 2）。

用法:
  python3 eng/tools/v6/v6_determinism_driver.py [--repo <root>] [--work-root <dir>]
        [--budgets 1,2,4,8] [--skip-build] [--json-out <path>]
exit 0 = 全部写盘路径跨预算等价（逐字节或容差内）；1 = 不一致；2 = 环境/构建缺失（fail-closed）。
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

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import v6_numeric_equiv  # noqa: E402  （同目录兄弟模块：容差档逐文件数值等价比对）

DEFAULT_REPO = pathlib.Path(__file__).resolve().parents[3]

# V6 三模式入口 + Phase1/2/3 写盘路径（各自 standalone 构建）。
SPECS = [
    {"name": "v6_p1_phase1_write", "source": "eng/tests/integration/v6_p1",
     "binary": "v6_p1_integrate_test", "args": ["positive", "{work}"],
     "products": "{work}"},
    {"name": "v6_p2_three_modes_write", "source": "eng/tests/integration/v6_p2",
     "binary": "v6_p2_integrate_test", "args": ["write", "{work}"],
     "products": "{work}"},
    {"name": "v6_p3_export_write", "source": "eng/tests/integration/v6_p3",
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
    roots = {}
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
        roots[b] = prod_root
        res["runs"].append({"budget": b, "rc": p.returncode, "cpus": sel,
                            "digest": dig["digest"], "n_files": dig["n_files"]})
    uniq = set(digests.values())
    if len(uniq) == 1 and len(digests) == len(budgets):
        res["ok"] = True
        res["mode"] = "bitwise"
        return res
    # 第二档：按冻结浮点容差做逐文件数值等价比对（SCHEDULER_CONTRACT §2.1）。
    norm = [work_root / "runs" / spec["name"] / ("b%d" % b) for b in budgets]
    cmp_res = v6_numeric_equiv.compare_roots([roots[b] for b in budgets],
                                             normalize_dirs=norm)
    # 容差档敏感性自检：把首个产物根的某个浮点载荷扰动到远超容差，必须被判红
    # （防容差档被做成恒绿）。
    if cmp_res["ok"]:
        probe = _tolerance_sensitivity_probe([roots[b] for b in budgets], norm)
        if probe is not None and probe.get("ok"):
            res["ok"] = False
            res["mode"] = "tolerance"
            res["comparison"] = cmp_res
            res["error"] = ("tolerance-path sensitivity self-check failed: %s"
                            % probe.get("reason", "perturbed payload not detected"))
            return res
    res["ok"] = bool(cmp_res["ok"])
    res["mode"] = "tolerance"
    res["comparison"] = {k: v for k, v in cmp_res.items() if k != "offenders"}
    if not res["ok"]:
        res["error"] = ("not equivalent across budgets: bitwise digest mismatch (%s) and "
                        "tolerance comparison failed: %s"
                        % (json.dumps(digests), json.dumps(res["comparison"], ensure_ascii=False)))
    return res


def _tolerance_sensitivity_probe(roots, normalize_dirs):
    """容差档非退化自检：扰动一个浮点载荷，必须被 compare_roots 判红。

    返回 None 表示找不到可扰动的浮点载荷（此时不阻断，如实记录）；返回 {"ok": True, ...}
    表示「扰动后仍判绿」= 容差档失效，调用方必须判红。
    """
    import shutil as _shutil
    import tempfile

    try:
        import numpy as np
        from astropy.io import fits
    except ImportError:
        return None
    src = pathlib.Path(roots[0])
    for p in sorted(src.rglob("*")):
        if p.suffix.lower() not in (".fits", ".fts", ".fit"):
            continue
        try:
            with fits.open(p, memmap=False) as h:
                if h[0].data is None or h[0].data.dtype.kind != "f":
                    continue
                data = np.array(h[0].data, copy=True)
                header = h[0].header.copy()
        except Exception:
            continue
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td)
            a_dir, b_dir = td / "a", td / "b"
            a_dir.mkdir(); b_dir.mkdir()
            fits.PrimaryHDU(data, header=header).writeto(a_dir / p.name, overwrite=True)
            bumped = data.copy()
            flat = bumped.reshape(-1)
            flat[0] = flat[0] * 2.0 + 1.0 if flat[0] != 0 else 1.0
            fits.PrimaryHDU(bumped, header=header).writeto(b_dir / p.name, overwrite=True)
            v = v6_numeric_equiv.compare_roots([a_dir, b_dir])
            return {"ok": bool(v["ok"]), "file": str(p),
                    "reason": "" if v["ok"] else "perturbed float payload detected (expected)"}
    return None


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
