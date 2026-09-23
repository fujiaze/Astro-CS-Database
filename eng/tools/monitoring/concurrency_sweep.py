#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""concurrency_sweep.py — 帧并发度受控扫描 + 产品逐字节比对（P1-CONCURRENCY-CALIB-01）。

问题（一手证据 run/PERF-501/**，本工具要修的可复现性缺口）：
  Phase1 的**帧级并发度不是配置项**，它由
      frame_workers = min(__workers(lease), p1_memory_cap)
      p1_memory_cap = floor(MemAvailable * 0.75 / (W*H*kP1FrameBytesPerPixel))
  派生（lib/infrastructure/scheduler/src/module_adapters.cpp）。
  lease 来自 Runtime 线程预算 = **CPU affinity 掩码的 CPU 数**（实测 taskset -c 0→1、
  0-1→2、0-3→4、0-7→8）。⇒ **钉住并发度的可复现手段 = taskset 钉 CPU 掩码**，
  配合 kP1FrameBytesPerPixel 使 memory_cap ≥ 目标值（见 --min-cap 自检）。

本工具做三件事：
  1) run    ：以给定 CPU 掩码跑一次 normalize，**外部**采样资源时序
              （/proc/<pid>/status 的 VmHWM/VmRSS、/proc/<pid>/task/*/stat 的全线程 CPU），
              墙钟/CPU 均值/p50/峰值 RSS 全部来自外部采样，不采信被测进程自报；
              超阈值（--rss-kill-gb）即杀，核对 pid 后登记；
              跑完对 output_dir **全量**逐文件 sha256 → manifest JSON（可删产物省盘）。
  2) compare：对两份 manifest 逐文件比对，给出「FITS 差异数 / 必同 JSON 差异数 /
              遥测类差异数」三分类计数（口径见 MUST_IDENTICAL）。
  3) --self-test：合成夹具正/负例（能红能绿），不触碰真实 run/。

用法：
  python3 eng/tools/monitoring/concurrency_sweep.py run --tag L8 --cpuset 0-7 \
      --binary build/astrocs --cfg run/.../cfg.json --outdir run/.../out/L8 \
      --stderr run/.../logs/L8.stderr --summary run/.../summary_L8.json \
      --manifest run/.../manifest_L8.json [--rss-kill-gb 20] [--keep-products]
  python3 eng/tools/monitoring/concurrency_sweep.py compare \
      --manifest run/.../manifest_L2.json --manifest run/.../manifest_L8.json
  python3 eng/tools/monitoring/concurrency_sweep.py --self-test

退出码：0 = 正常；1 = 一般错误（含 compare 发现必同文件有差异）；2 = 用法/锚失效。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time

CLK_TCK = os.sysconf("SC_CLK_TCK")
SAMPLE_INTERVAL_S = 0.2

# 判据分类（事前冻结，不由结果反推）：默认 'must'，仅白名单降级为 'telem'。
# 遥测/路径/时间戳类（允许不同）—— 显式白名单：
TELEM_EXACT = {
    # 资源遥测（每次运行都不同）
    "alloc_report.json", "alloc_samples.csv", "resource_summary.json",
    "resource_timeseries.csv", "worker_balance.csv", "run_context.json",
    # 调度图/追踪（含时间戳与 run_id）
    "graph_sidecar.json", "observed_trace.json", "static_graph.json",
    # 逐帧终判（含 output_dir 路径串）
    "p1_final.json",
}
# 含 output_dir 绝对路径串、但**同时**含科学量 ⇒ 不能整体降级为遥测（会静默容忍科学差异）。
# 口径：对这些文件比对**路径掩码后的规范化 JSON**（掩码只替换本 run 的 output_dir 前缀，
# 其余字段逐字参与比较）⇒ 科学字段仍然严格比对。
MASKED_JSON = {"p1_phot.json", "p1_products.json", "p1_final.json"}
TELEM_PREFIX = ("graph/",)
TELEM_SUFFIX = ("/properties",)          # HiPS properties: hips_creation_date 墙钟
TELEM_RUN_RE = re.compile(r"^astrocs_run_[0-9a-f]+\.json$")   # run-scoped 遥测


def classify(relpath: str) -> str:
    """'must' = 必须逐字节相同；'telem' = 遥测/路径/时间戳类，允许不同。

    **fail-closed 口径（P1-CONCURRENCY-CALIB-01 修正）**：默认 'must'，
    只有**显式列入**遥测白名单的才降级为 'telem'。
    旧口径（默认 telem）曾把 `calibrated_*.fts` / `photoapplied_*.fts` 判成遥测 ——
    那是**科学产品**，差异会被静默容忍（判据静默退化，AGENTS §9）。
    新增产品类型时无需改本表即自动按 'must' 判（宁可误报，不可漏报）。
    白名单来源：run/PERF-501 同二进制重复运行实测的 23 个差异文件 +
    HiPS `properties`（`hips_creation_date` 墙钟，IVOA 必需）+ `graph/*` + run-scoped 遥测。
    """
    base = os.path.basename(relpath)
    if base in TELEM_EXACT:
        return "telem"
    if any(relpath.startswith(p) for p in TELEM_PREFIX):
        return "telem"
    if any(relpath.endswith(s) for s in TELEM_SUFFIX):
        return "telem"
    if TELEM_RUN_RE.match(base):
        return "telem"
    return "must"


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _mask_strings(obj, mask: str):
    """递归把字符串里的 run 输出目录前缀换成 <OUT>（只动这一处，其余逐字保留）。"""
    if isinstance(obj, dict):
        return {k: _mask_strings(v, mask) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_mask_strings(v, mask) for v in obj]
    if isinstance(obj, str):
        return obj.replace(mask, "<OUT>")
    return obj


def masked_json_sha256(path: str, mask: str) -> str:
    """路径掩码后的规范化 JSON 摘要（科学字段仍逐字参与）。解析失败返回 None。"""
    try:
        with open(path, "r", encoding="utf-8") as f:
            obj = json.load(f)
    except Exception:
        return None
    canon = json.dumps(_mask_strings(obj, mask), sort_keys=True,
                       separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(canon.encode("utf-8")).hexdigest()


def build_manifest(root: str, mask: str = "") -> dict:
    out = {}
    root_abs = os.path.abspath(root)
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            p = os.path.join(dirpath, fn)
            if os.path.islink(p) or not os.path.isfile(p):
                continue
            rel = os.path.relpath(p, root)
            rec = {"sha256": sha256_file(p), "size": os.path.getsize(p),
                   "class": classify(rel)}
            if mask and os.path.basename(p) in MASKED_JSON:
                m = masked_json_sha256(p, mask)
                if m:
                    rec["sha256_masked"] = m
            out[rel] = rec
    return out


def _proc_cpu_ticks(pid: int) -> int:
    """全线程 utime+stime（/proc/<pid>/task/*/stat）。组长单条会低估多线程进程。"""
    total = 0
    tdir = "/proc/%d/task" % pid
    try:
        tids = os.listdir(tdir)
    except OSError:
        return -1
    for tid in tids:
        try:
            with open("%s/%s/stat" % (tdir, tid), "rb") as f:
                data = f.read().decode("ascii", "replace")
        except OSError:
            continue
        rp = data.rfind(")")
        if rp < 0:
            continue
        parts = data[rp + 2:].split()
        if len(parts) < 13:
            continue
        total += int(parts[11]) + int(parts[12])
    return total


def _proc_rss(pid: int):
    try:
        with open("/proc/%d/status" % pid, "rb") as f:
            txt = f.read().decode("ascii", "replace")
    except OSError:
        return None, None
    vmhwm = vmrss = None
    for line in txt.splitlines():
        if line.startswith("VmHWM:"):
            vmhwm = int(line.split()[1]) * 1024
        elif line.startswith("VmRSS:"):
            vmrss = int(line.split()[1]) * 1024
    return vmhwm, vmrss


def grep_trace(stderr_path: str, prefixes) -> dict:
    """从 stderr 提取 [lease] / [p1cap] 行（生效并发度的唯一落盘证据）。"""
    hits = {p: [] for p in prefixes}
    if not os.path.exists(stderr_path):
        return hits
    with open(stderr_path, "rb") as f:
        for raw in f:
            line = raw.decode("utf-8", "replace")
            for p in prefixes:
                if p in line:
                    hits[p].append(line.rstrip("\n"))
    return hits


def cmd_run(a) -> int:
    os.makedirs(a.outdir, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(a.stderr)), exist_ok=True)
    cfg = json.load(open(a.cfg))
    cfg["blocks"][0]["output_dir"] = os.path.abspath(a.outdir)
    cfg_path = os.path.abspath(a.outdir.rstrip("/") + ".cfg.json")
    with open(cfg_path, "w") as f:
        json.dump(cfg, f, indent=2)

    argv = ["taskset", "-c", a.cpuset, os.path.abspath(a.binary),
            "normalize", "--json", cfg_path, "-y"]
    env = dict(os.environ)
    env["ASTROCS_P1CAP_TRACE"] = "1"
    env["ASTROCS_LEASE_TRACE"] = "1"
    t0 = time.monotonic()
    with open(a.stderr, "wb") as errf:
        proc = subprocess.Popen(argv, stdout=subprocess.DEVNULL, stderr=errf, env=env)
    pid = proc.pid
    # 核对 pid 身份：taskset 用 execvp，pid 即 astrocs 本体
    comm = ""
    try:
        comm = open("/proc/%d/comm" % pid).read().strip()
    except OSError:
        pass

    samples, cpu_pct = [], []
    prev_ticks, prev_t = None, None
    n_neg_clamped = 0
    killed = False
    while proc.poll() is None:
        hwm, rss = _proc_rss(pid)
        ticks = _proc_cpu_ticks(pid)
        now = time.monotonic()
        if hwm is not None:
            samples.append({"t": round(now - t0, 3), "vmhwm": hwm, "vmrss": rss})
        if prev_ticks is not None and ticks >= 0 and now > prev_t:
            delta = ticks - prev_ticks
            if delta < 0:
                # 线程池退出使 /proc/<pid>/task/*/stat 求和**下降** ⇒ 负增量是记账假象，
                # 不是"负 CPU"。夹到 0 后 mean 才可用；p50 本来就不受影响。
                # 计数登记在 n_cpu_neg_clamped，供读者判断该档 mean 的可信度。
                n_neg_clamped += 1
                delta = 0
            cpu_pct.append(100.0 * delta / CLK_TCK / (now - prev_t))
        if ticks >= 0:
            prev_ticks, prev_t = ticks, now
        if a.rss_kill_gb and rss is not None and rss > a.rss_kill_gb * (1 << 30):
            proc.send_signal(signal.SIGKILL)
            killed = True
            break
        time.sleep(SAMPLE_INTERVAL_S)
    # 身份复核：exec 完成后 comm 必须是 astrocs（证明采样对象就是被测进程本体）
    comm_after = ""
    try:
        comm_after = open("/proc/%d/comm" % pid).read().strip()
    except OSError:
        comm_after = "(exited)"
    rc = proc.wait()
    wall = time.monotonic() - t0

    peak = max((s["vmhwm"] for s in samples), default=None)
    summary = {
        "tag": a.tag, "cpuset": a.cpuset, "cfg": cfg_path,
        "binary": os.path.abspath(a.binary),
        "binary_sha256": sha256_file(os.path.abspath(a.binary)),
        "pid": pid, "pid_comm": comm, "pid_comm_after": comm_after,
        "exit_code": rc, "rss_kill_fired": killed,
        "n_cpu_neg_clamped": n_neg_clamped,
        "wall_s": round(wall, 3),
        "cpu_mean_pct": round(statistics.fmean(cpu_pct), 3) if cpu_pct else None,
        "cpu_p50_pct": round(statistics.median(cpu_pct), 3) if cpu_pct else None,
        "cpu_max_pct": round(max(cpu_pct), 3) if cpu_pct else None,
        "peak_rss_bytes": peak,
        "peak_rss_gb": round(peak / 1e9, 3) if peak else None,
        "n_samples": len(samples),
        "n_cpu_samples": len(cpu_pct),
    }
    summary.update(grep_trace(a.stderr, ["[lease]", "[p1cap]"]))
    if a.build_stamp:
        summary["build_stamp"] = json.load(open(a.build_stamp))
    with open(a.summary, "w") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)

    if rc == 0 and os.path.isdir(a.outdir):
        man = build_manifest(a.outdir, mask=os.path.abspath(a.outdir))
        with open(a.manifest, "w") as f:
            json.dump(man, f, indent=1, sort_keys=True)
        summary["n_files"] = len(man)
        summary["n_fits"] = sum(1 for k in man if k.endswith(".fits"))
        summary["n_must"] = sum(1 for v in man.values() if v["class"] == "must")
        with open(a.summary, "w") as f:
            json.dump(summary, f, indent=2, ensure_ascii=False)
        if not a.keep_products:
            shutil.rmtree(a.outdir, ignore_errors=True)
    print(json.dumps({k: v for k, v in summary.items()
                      if k not in ("[lease]", "[p1cap]")}, ensure_ascii=False))
    return 0 if rc == 0 else 1


def compare_manifests(base: dict, other: dict):
    common = set(base) & set(other)
    res = {"base_only": sorted(set(base) - set(other)),
           "other_only": sorted(set(other) - set(base)),
           "n_common": len(common), "must_diff": [], "telem_diff": [],
           "n_path_only": []}
    # P1-PARALLEL-AXIS-REDESIGN-01 判别力修正（**恒真门**）:
    #   旧口径只在**共同文件**上比对 ⇒ ① 两份 manifest 文件集完全不相交（common=0）、
    #   ② 另档产物为空目录、③ 另档多/少一个科学产品 —— 三种情形都必须判红，
    #   旧实现一律返回 0（绿），即「没有可比的差异」被当成「没有差异」。
    #   对 A/B 对照而言文件集必须逐项一致（同帧集 ⇒ 同产物集），故 fail-closed 判红。
    res["vacuous"] = (len(common) == 0)
    # 文件集不一致只在 **must 类**上判红：run-scoped 遥测（如 astrocs_run_<hash>.json）
    # 的文件名本身含 run 哈希 ⇒ 两次运行必然一有一无，那不是产品差异。
    # 口径：只统计 classify()=='must' 的单侧文件（宁可误报，不可漏报科学产品缺失）。
    res["base_only_must"] = [r for r in res["base_only"] if classify(r) == "must"]
    res["other_only_must"] = [r for r in res["other_only"] if classify(r) == "must"]
    res["set_mismatch"] = bool(res["base_only_must"] or res["other_only_must"])
    for rel in sorted(common):
        a, b = base[rel], other[rel]
        same = (a["sha256"] == b["sha256"])
        if not same and "sha256_masked" in a and "sha256_masked" in b:
            # 路径掩码后相同 ⇒ 差异仅来自 output_dir 串（科学字段逐字相同）
            same = (a["sha256_masked"] == b["sha256_masked"])
            if same:
                res["n_path_only"].append(rel)
        if not same:
            # 按**当前**判据重算 class（不信任 manifest 里存的历史 class）
            res["must_diff" if classify(rel) == "must" else "telem_diff"].append(rel)
    res["n_fits_diff"] = sum(1 for r in res["must_diff"]
                           if r.endswith((".fits", ".fts", ".fit")))
    res["n_must_diff"] = len(res["must_diff"])
    res["n_telem_diff"] = len(res["telem_diff"])
    return res


def cmd_compare(a) -> int:
    mans = [json.load(open(p)) for p in a.manifest]
    base_name = os.path.basename(a.manifest[0])
    rows = []
    worst = 0
    for i in range(1, len(mans)):
        res = compare_manifests(mans[0], mans[i])
        print("== %s vs %s ==" % (base_name, os.path.basename(a.manifest[i])))
        print("  common=%d  FITS差异=%d  必同JSON/其他差异=%d  遥测类差异=%d  "
              "仅差路径串=%d  仅基准有=%d 仅本档有=%d"
              % (res["n_common"], res["n_fits_diff"], res["n_must_diff"],
                 res["n_telem_diff"], len(res["n_path_only"]),
                 len(res["base_only"]), len(res["other_only"])))
        if res["must_diff"]:
            print("  !! 必同文件差异（前 20）:")
            for r in res["must_diff"][:20]:
                print("     -", r)
        # P1-PARALLEL-AXIS-REDESIGN-01 fail-closed: 无可比对象 / 文件集不一致 ⇒ 判红。
        # 旧口径只看**共同文件**的差异 ⇒ common=0 时恒返回 0（绿），是恒真门。
        if res["vacuous"]:
            print("  !! 空对照：共同文件 0 ⇒ 判红（无判别力，不得当绿）")
        if res["set_mismatch"]:
            print("  !! 文件集不一致（must 类）⇒ 判红（仅基准有 %d / 仅本档有 %d）"
                  % (len(res["base_only_must"]), len(res["other_only_must"])))
            for r in (res["base_only_must"] + res["other_only_must"])[:10]:
                print("     -", r)
        worst = max(worst, res["n_must_diff"])
        rows.append({"base": a.manifest[0], "other": a.manifest[i], **res})
    if a.out:
        with open(a.out, "w") as f:
            json.dump(rows, f, indent=1, ensure_ascii=False)
    bad = worst or any(r["vacuous"] or r["set_mismatch"] for r in rows)
    return 1 if bad else 0


def cmd_manifest(a) -> int:
    """从既有产物目录重建 manifest（含路径掩码哈希），供事后比对/补算。"""
    man = build_manifest(a.root, mask=os.path.abspath(a.root) if a.mask else "")
    with open(a.out, "w") as f:
        json.dump(man, f, indent=1, sort_keys=True)
    n_fits = sum(1 for k in man if k.endswith((".fits", ".fts")))
    n_masked = sum(1 for v in man.values() if "sha256_masked" in v)
    print(json.dumps({"root": a.root, "out": a.out, "n_files": len(man),
                      "n_fits_family": n_fits, "n_masked": n_masked},
                     ensure_ascii=False))
    return 0


def self_test() -> int:
    """正例（同内容 ⇒ 0 差异）+ 负例（改 1 字节 ⇒ 必须判红，且分类正确）。"""
    ok = True
    tmp = tempfile.mkdtemp(prefix="p1cc_selftest_")
    try:
        d1, d2 = os.path.join(tmp, "a"), os.path.join(tmp, "b")
        for d in (d1, d2):
            os.makedirs(os.path.join(d, "frame", "signal"), exist_ok=True)
            open(os.path.join(d, "frame", "signal", "Norder3.fits"), "wb").write(b"FITS" * 100)
            open(os.path.join(d, "frame", "p1_phot.json"), "w").write('{"k":1}')
            open(os.path.join(d, "resource_summary.json"), "w").write('{"t":1}')
        m1, m2 = build_manifest(d1), build_manifest(d2)
        r = compare_manifests(m1, m2)
        print("SELFCHECK phase1 同内容: must_diff=%d telem_diff=%d (期望 0/0)"
              % (r["n_must_diff"], r["n_telem_diff"]))
        ok &= (r["n_must_diff"] == 0 and r["n_telem_diff"] == 0)
        # 负例 1：科学产品改 1 字节 ⇒ 必须计入 must_diff 且是 FITS
        open(os.path.join(d2, "frame", "signal", "Norder3.fits"), "r+b").write(b"X")
        m2 = build_manifest(d2)
        r = compare_manifests(m1, m2)
        print("SELFCHECK phase2 FITS 改 1 字节: fits_diff=%d (期望 1)" % r["n_fits_diff"])
        ok &= (r["n_fits_diff"] == 1)
        # 负例 2：遥测类改 1 字节 ⇒ 必须计入 telem_diff，且**不**污染 must 计数
        open(os.path.join(d2, "resource_summary.json"), "w").write('{"t":2}')
        m2 = build_manifest(d2)
        r = compare_manifests(m1, m2)
        print("SELFCHECK phase3 遥测类改 1 字节: must_diff=%d telem_diff=%d (期望 1/1)"
              % (r["n_must_diff"], r["n_telem_diff"]))
        ok &= (r["n_must_diff"] == 1 and r["n_telem_diff"] == 1)
        # 负例 3：p1_phot.json（必同 JSON）改 1 字节 ⇒ must 计数 +1
        open(os.path.join(d2, "frame", "p1_phot.json"), "w").write('{"k":2}')
        m2 = build_manifest(d2)
        r = compare_manifests(m1, m2)
        print("SELFCHECK phase4 p1_phot.json 改 1 字节: must_diff=%d (期望 2)" % r["n_must_diff"])
        ok &= (r["n_must_diff"] == 2)
        # 采样面：对自身进程采一次，必须拿到 RSS
        hwm, rss = _proc_rss(os.getpid())
        print("SELFCHECK phase5 RSS 采样: vmhwm=%s vm_rss=%s" % (hwm, rss))
        ok &= (rss is not None and rss > 0)
        # 判据分类（fail-closed）：科学产品一律 'must'，仅白名单降级为 'telem'
        cases = [
            ("f/signal/Norder3.fits", "must"),
            ("calibrated_M42_x.fts", "must"),                 # 旧口径曾误判为 telem
            ("photoapplied_M42_x.fts", "must"),
            ("f/manifest.json", "must"),                      # 未知 JSON 默认 must
            ("f/p1_unknown_product.dat", "must"),             # 未知扩展名默认 must
            ("f/p1_stack.json", "must"),
            ("f/p1_final.json", "telem"),                     # 含 output_dir 路径串
            ("f/signal/properties", "telem"),                 # hips_creation_date
            ("graph/observed_graph.dot", "telem"),
            ("astrocs_run_ec3dea28e982.json", "telem"),
            ("resource_summary.json", "telem"),
        ]
        bad = [(p, classify(p), e) for p, e in cases if classify(p) != e]
        print("SELFCHECK phase6 判据分类: %d/%d 正确%s"
              % (len(cases) - len(bad), len(cases),
                 "" if not bad else "  错误=" + repr(bad)))
        ok &= not bad
        # 负例 4：.fts 科学产品改 1 字节 ⇒ 必须计入 must_diff（旧口径会漏报）
        open(os.path.join(d2, "frame", "p1_phot.json"), "w").write('{"k":1}')  # 复原
        open(os.path.join(d2, "frame", "signal", "Norder3.fits"), "wb").write(b"FITS" * 100)
        open(os.path.join(d2, "calibrated_M42_x.fts"), "wb").write(b"CAL" * 10)
        open(os.path.join(d1, "calibrated_M42_x.fts"), "wb").write(b"CAL" * 10)
        m1, m2 = build_manifest(d1), build_manifest(d2)
        open(os.path.join(d2, "calibrated_M42_x.fts"), "r+b").write(b"Z")
        m2 = build_manifest(d2)
        r = compare_manifests(m1, m2)
        print("SELFCHECK phase7 .fts 改 1 字节: must_diff=%d (期望 1)" % r["n_must_diff"])
        ok &= (r["n_must_diff"] == 1)
        # 路径掩码：p1_phot.json 只差 output_dir 前缀 ⇒ 掩码后相同（不计 must_diff）；
        # 科学字段改一个数 ⇒ 掩码后仍不同（必须计入 must_diff）。
        d3 = os.path.join(tmp, "m1"); d4 = os.path.join(tmp, "m2")
        os.makedirs(d3, exist_ok=True); os.makedirs(d4, exist_ok=True)
        open(os.path.join(d3, "p1_phot.json"), "w").write(
            json.dumps({"out": d3 + "/x.fts", "k_photo": 1.5e-17}))
        open(os.path.join(d4, "p1_phot.json"), "w").write(
            json.dumps({"out": d4 + "/x.fts", "k_photo": 1.5e-17}))
        r = compare_manifests(build_manifest(d3, mask=d3), build_manifest(d4, mask=d4))
        print("SELFCHECK phase8 只差 output_dir: must_diff=%d path_only=%d (期望 0/1)"
              % (r["n_must_diff"], len(r["n_path_only"])))
        ok &= (r["n_must_diff"] == 0 and len(r["n_path_only"]) == 1)
        open(os.path.join(d4, "p1_phot.json"), "w").write(
            json.dumps({"out": d4 + "/x.fts", "k_photo": 1.6e-17}))
        r = compare_manifests(build_manifest(d3, mask=d3), build_manifest(d4, mask=d4))
        print("SELFCHECK phase9 科学字段改 1 位: must_diff=%d (期望 1)" % r["n_must_diff"])
        ok &= (r["n_must_diff"] == 1)
        # 负例 5-7（P1-PARALLEL-AXIS-REDESIGN-01 **恒真门**修正）：旧口径只比**共同文件**，
        # common=0 时恒判绿 —— 「另档产物整体缺失/错位」会被当成「没有差异」。
        g1 = os.path.join(tmp, "g1"); g2 = os.path.join(tmp, "g2")
        os.makedirs(os.path.join(g1, "frame"), exist_ok=True)
        os.makedirs(os.path.join(g2, "other"), exist_ok=True)
        open(os.path.join(g1, "frame", "signal.fits"), "wb").write(b"A" * 32)
        open(os.path.join(g2, "other", "signal.fits"), "wb").write(b"A" * 32)
        r = compare_manifests(build_manifest(g1), build_manifest(g2))
        print("SELFCHECK phase10 文件集不相交: vacuous=%s set_mismatch=%s (期望 True/True)"
              % (r["vacuous"], r["set_mismatch"]))
        ok &= (r["vacuous"] and r["set_mismatch"])
        g3 = os.path.join(tmp, "g3"); os.makedirs(g3, exist_ok=True)
        r = compare_manifests(build_manifest(g1), build_manifest(g3))
        print("SELFCHECK phase11 另档为空目录: vacuous=%s (期望 True)" % r["vacuous"])
        ok &= bool(r["vacuous"])
        g4 = os.path.join(tmp, "g4"); os.makedirs(os.path.join(g4, "frame"), exist_ok=True)
        open(os.path.join(g4, "frame", "signal.fits"), "wb").write(b"A" * 32)
        open(os.path.join(g4, "frame", "extra.fits"), "wb").write(b"B" * 32)
        r = compare_manifests(build_manifest(g1), build_manifest(g4))
        print("SELFCHECK phase12 另档多一个产品: set_mismatch=%s must_diff=%d (期望 True/0)"
              % (r["set_mismatch"], r["n_must_diff"]))
        ok &= (r["set_mismatch"] and r["n_must_diff"] == 0)
        # 正例：单侧只差 **run-scoped 遥测**（文件名含 run 哈希）⇒ 不得判红。
        g5 = os.path.join(tmp, "g5"); g6 = os.path.join(tmp, "g6")
        for d in (g5, g6):
            os.makedirs(os.path.join(d, "frame"), exist_ok=True)
            open(os.path.join(d, "frame", "signal.fits"), "wb").write(b"A" * 32)
        open(os.path.join(g5, "astrocs_run_aaaaaaaaaaaa.json"), "w").write('{"t":1}')
        open(os.path.join(g6, "astrocs_run_bbbbbbbbbbbb.json"), "w").write('{"t":2}')
        r = compare_manifests(build_manifest(g5), build_manifest(g6))
        print("SELFCHECK phase13 单侧只差 run-scoped 遥测: set_mismatch=%s must_diff=%d (期望 False/0)"
              % (r["set_mismatch"], r["n_must_diff"]))
        ok &= (not r["set_mismatch"] and r["n_must_diff"] == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("CONCURRENCY_SWEEP SELFCHECK %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--self-test", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    p = sub.add_parser("run")
    p.add_argument("--tag", required=True)
    p.add_argument("--cpuset", required=True, help="taskset CPU 掩码，如 0-7（钉 lease）")
    p.add_argument("--binary", required=True)
    p.add_argument("--cfg", required=True)
    p.add_argument("--outdir", required=True)
    p.add_argument("--stderr", required=True)
    p.add_argument("--summary", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--build-stamp", default=None)
    p.add_argument("--rss-kill-gb", type=float, default=0.0)
    p.add_argument("--keep-products", action="store_true")
    p.set_defaults(func=cmd_run)
    r = sub.add_parser("manifest")
    r.add_argument("--root", required=True)
    r.add_argument("--out", required=True)
    r.add_argument("--mask", action="store_true",
                   help="对 MASKED_JSON 记路径掩码后的规范化摘要")
    r.set_defaults(func=cmd_manifest)
    q = sub.add_parser("compare")
    q.add_argument("--manifest", action="append", required=True)
    q.add_argument("--out", default=None)
    q.set_defaults(func=cmd_compare, json_out=[])
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not getattr(a, "cmd", None):
        ap.print_help()
        return 2
    return a.func(a)


if __name__ == "__main__":
    sys.exit(main())
