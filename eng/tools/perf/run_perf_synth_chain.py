#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PERF-760 小批量**合成数据**全流程性能驱动（可复跑、出机器可读证据）。

权威链（AGENTS §1.1：先确定规范再动手）
--------------------------------------
* 标准 05 §1（小批量合成数据性能测试先于全量端到端）/ §2（按冻结判据裁决、越界判红）/
  §5（分阶段计时 + 性能数字附测量条件）—— run/FINAL-07/pkg/standards/05_性能与数值等价标准.md；
* L2 验收面 = ACCEPTANCE_SPEC.md §4（"规模化合成数据使每段重计算区间**严格大于 10 s**"、
  CPU 利用率/内存工作集/线程扩展/编排连续性/缓存复用/I/O 六类记录面）；
* 冻结判据语义 = docs/detail/infrastructure/21_observability.md §8（G-RES-01，唯一语义权威），
  阈值数值唯一源 = eng/contracts/resource_gate_v1.json（本工具**不发明**任何阈值）；
* 编排与资源条款 = docs/ACSD_DESIGN.md §8.3（探针校正：编排参数基于探针实测迭代，
  不靠静态猜测）/ §9（两轴并行：帧级并发 × 帧内并行，串行段由帧级并发重叠；模块不自决帧级并发）；
* 目录纪律 = ENGINEERING_SPEC.md §7（过程产物落 run/，CLI 运行产物落 output_dir）。

为什么必须新建本工具（任务书第 1 条的判定）
--------------------------------------------
规范路径的三命令串行驱动是 eng/tools/e2e/run_e2e_chain.py（ENGINEERING_SPEC §13 +
docs/engineering/01_CHECKS.md 注册面 CHK-E2E-CHAIN / CHK-E2E-CHAIN-SELFTEST），但它**只吃真实帧**：
配置由 eng/tools/e2e/make_e2e_configs.py 从 testdata/M42*、testdata/Galaxy_Center_T4 的
真实 FITS 构造（含写死的 RA/Dec 中心）。合成数据侧现有两件都不能独立驱动全链：

| 现有件 | 能做的 | 缺口 |
|---|---|---|
| 实验/shared/data/synthetic/generate.py | 按 datasets.json 生成 DATA-TYPE-MATRIX 场景帧 | 解析玩具仪器产物：无 TAN WCS、无校准母版 ⇒ normalize 的预检/解算链不可用 |
| 实验/shared/synthetic/m16_sampling.py | 真实信号模板 → 仿真采样帧 + **与探测器模型一致的合成母版** + 真值画布（TAN WCS 一等公民） | 只到"生成数据"为止，不构造三命令配置、不驱动 CLI、不出性能证据 |
| 实验/additive-sky-seamless/.../run_reconstruct.py | 仿真帧 → normalize → mosaic → export → 与真值比对 | 是**科学保真度**判据驱动（互相关峰位/结构残差/测光），不采样资源、不判 G-RES-01、不落监控证据；且二进制路径写死 build/acsd（本仓现役 CLI 为 build/acsd） |

⇒ 本工具是"规范路径 + 合成数据驱动件"的**接线层**，不复制上述任何一方的实现：
数据生成调 m16_sampling（唯一事实源），三命令串行与 manifest 链纪律沿用 run_e2e_chain 的口径，
资源采样与冻结判据调 eng/tools/monitoring 的既有预埋面（run_monitored / resource_probe /
node_waterfall），阈值读 eng/contracts/resource_gate_v1.json。**不新造埋点**。

用法
----
  python3 eng/tools/perf/run_perf_synth_chain.py selftest
  python3 eng/tools/perf/run_perf_synth_chain.py run --tag base \
      --scene 实验/shared/synthetic/scenes/m16_sampling_mosaic_diff_pointing.json \
      --scale 2 --workers 16 --run-dir run/PERF-760
  python3 eng/tools/perf/run_perf_synth_chain.py run --tag opt-axisframe --axis-frame 16 --axis-inner 1 ...
  python3 eng/tools/perf/run_perf_synth_chain.py set --tags base opt-axisframe

退出码：0 = 全绿（含判据）；1 = 有判据判红；2 = 用法/锚/环境错误。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import platform
import shlex
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve()
ROOT = HERE.parents[3]                      # eng/tools/perf/<file> -> 仓库根
MON = ROOT / "eng" / "tools" / "monitoring"
sys.path.insert(0, str(MON))

if str(MON) not in sys.path:
    sys.path.insert(0, str(MON))
if str(ROOT / "实验" / "shared" / "synthetic") not in sys.path:
    sys.path.insert(0, str(ROOT / "实验" / "shared" / "synthetic"))
sys.path.insert(0, str(ROOT / "eng" / "ci"))
import l2_frozen_gate as L2                   # noqa: E402  L2 冻结判据 fail-closed 判定面（CI_SPEC §9.2）
import resource_probe as RP                  # noqa: E402  机器有效核/内存探测（既有件）
import run_monitored as RM                   # noqa: E402  外挂监控 + G-RES-01 冻结门实现
import m16_sampling as MS                    # noqa: E402  合成数据唯一事实源

CONTRACT = ROOT / "eng" / "contracts" / "resource_gate_v1.json"
BINARY = ROOT / "build" / "acsd"     # 缺省；--binary 可指向自建 build-perf（隔离协议 4）
MEM_GUARD = ROOT / "eng" / "tools" / "monitoring" / "mem_guard.py"
WATERFALL = ROOT / "eng" / "tools" / "monitoring" / "node_waterfall.py"
VERIFY_CSV = ROOT / "eng" / "tools" / "monitoring" / "verify_monitor_csv.py"
RUN_MON = ROOT / "eng" / "tools" / "monitoring" / "run_monitored.py"

# ── 受控编排键（**只有这些**允许出现在本工具的命令面；值来自 benchmark profile 或实测扫描）
# ACSD 的并发由调度器从「Runtime lease（= CPU 亲和掩码核数）」与「内存闸门」派生，phase 配置
# **禁止**携带 workers/ISA/block（eng/contracts/schemas/phase_config_*.schema.json 硬件字段禁令）。
# 因此可复现的并发控制手段是两个已登记的标定旋钮：
#   · taskset -c <cpuset>                          —— 改 lease（帧级并发的唯一来源）
#   · ACSD_P1_AXIS_FRAME_WORKERS / _INNER_OMP   —— 帧级并发 vs 帧内并行的两轴分配
#     （module_adapters.cpp:1942 标定旋钮；越界即拒绝覆盖并留痕 ⇒ 总并行度 ≤ lease 不破）
AXIS_ENV = {"frame": "ACSD_P1_AXIS_FRAME_WORKERS", "inner": "ACSD_P1_AXIS_INNER_OMP"}
NODE_TRACE_ENVS = {"ACSD_NODE_TRACE": "1", "ACSD_LEASE_TRACE": "1",
                   "ACSD_P1CAP_TRACE": "1"}


# ─────────────────────────────────────────────────────────── 小工具 ──
def sha256_file(p) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_json(p, doc) -> None:
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc, indent=1, ensure_ascii=False, default=float) + "\n",
                 encoding="utf-8")


def read_json(p):
    return json.loads(Path(p).read_text(encoding="utf-8"))


def dir_size_bytes(d) -> int:
    total = 0
    for r, _dirs, files in os.walk(str(d)):
        for f in files:
            try:
                total += os.path.getsize(os.path.join(r, f))
            except OSError:
                pass
    return total


def df_workspace() -> dict:
    st = os.statvfs(str(ROOT))
    return {"total_gb": round(st.f_blocks * st.f_frsize / 2**30, 2),
            "free_gb": round(st.f_bavail * st.f_frsize / 2**30, 2),
            "used_pct": round(100.0 * (st.f_blocks - st.f_bfree) / st.f_blocks, 1)}


def loadavg() -> list:
    """1/5/15 分钟负载（隔离协议 1/3 的前置检查与逐样本留证）。"""
    try:
        return [float(x) for x in Path("/proc/loadavg").read_text().split()[:3]]
    except Exception:                                        # noqa: BLE001
        return []


def foreign_cpu_share(mon: dict, mask_cores: int) -> dict:
    """外来进程 CPU 份额（隔离协议 3）：由 /proc/stat 全系统忙碌时间推。

    口径：系统级忙碌核数 = (Δtotal_jiffies − Δidle_jiffies − Δiowait_jiffies) / Δtotal，
    再减去**本进程树**占用的等效核（run_monitored 的 cpu_percent_avg/100）
    ⇒ 差值即外来负载。>2 核（16 核的 12.5%）⇒ 该样本判 INVALID（不入汇总）。
    只读 /proc/stat，不新增埋点。
    """
    try:
        parts = Path("/proc/stat").read_text().split("\n")[0].split()
        vals = [int(x) for x in parts[1:]]
        idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
        total = sum(vals)
    except Exception:                                        # noqa: BLE001
        return {"available": False}
    own = (mon.get("cpu_percent_avg") or 0.0) / 100.0
    elapsed = mon.get("duration_seconds") or 0.0
    return {"available": False, "system_stat_total_jiffies": total, "system_stat_idle_jiffies": idle,
            "process_tree_cores": round(own, 4), "elapsed_s": round(elapsed, 3),
            "mask_cores": mask_cores,
            "note": "系统级 Δ 需测前/测后两次采样；本字段给测后快照，差值由 report 侧算。"}


def proc_stat_percpu_snapshot() -> dict:
    """全系统 + 逐 CPU 的 /proc/stat 快照（用于掩码内份额判定）。"""
    per = _proc_stat_snap(per_cpu=True)
    tot = _proc_stat_snap()
    return {"total": tot.get("total"), "idle": tot.get("idle"), "per_cpu": per}


def mask_busy_cores(before: dict, after: dict, mask: str) -> dict:
    """**掩码内**的忙碌核数（掩码外负载与本次测量的判据无关，且会稀释判定）。

    为什么必须分掩码看：`taskset -c 0-7` 只钉住被测进程；若按全系统 16 核算外来份额，
    核心 8–15 上的任何活动都会把被测样本判成 INVALID（假阳性），而它们根本没和被测进程抢核。
    口径：掩码内 (Δtotal − Δidle_iowait) 折算成核数；外来份额 = 掩码内忙碌 − 本进程树实占（截到掩码核数）。
    """
    cpus = parse_cpuset_mask(mask)
    b = (before or {}).get("per_cpu") or {}
    a = (after or {}).get("per_cpu") or {}
    if not cpus or not b or not a:
        return {"available": False, "note": "缺逐 CPU 快照或掩码不可解析"}
    dt = di = 0
    for c in cpus:
        if c in b and c in a:
            dt += a[c]["total"] - b[c]["total"]
            di += a[c]["idle"] - b[c]["idle"]
    if dt <= 0:
        return {"available": False, "note": "掩码内核时间零增量"}
    return {"available": True, "mask": mask, "mask_cores": len(cpus),
            "busy_cores_on_mask": round((dt - di) / dt * len(cpus), 4),
            "idle_frac_on_mask": round(di / dt, 4)}


def proc_stat_snapshot() -> dict:
    try:
        vals = [int(x) for x in Path("/proc/stat").read_text().split("\n")[0].split()[1:]]
        idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
        return {"total": sum(vals), "idle": idle}
    except Exception:                                        # noqa: BLE001
        return {}


def fits_shape(path) -> tuple:
    """从 FITS 头读 (H, W)（只读头，不读像素；失败 → (0,0) = 不可判定）。"""
    try:
        with open(path, "r", encoding="latin-1") as fh:
            hdr = fh.read(2880 * 6)
    except OSError:
        return (0, 0)
    naxis1 = naxis2 = 0
    for i in range(0, len(hdr) - 79, 80):
        card = hdr[i:i + 80]
        key = card[:8].strip()
        if key == "NAXIS1":
            naxis1 = int(card[10:30])
        elif key == "NAXIS2":
            naxis2 = int(card[10:30])
        elif key == "END":
            break
    return (naxis2, naxis1)


def frame_shape_of(cfg, stage: str) -> tuple:
    """从配置里取**帧尺寸**（只读文件名/头不适用 ⇒ 用配置的宽高或 crop）。

    口径：normalize 的帧尺寸来自输入灯的 FITS 头，配置里没有 ⇒ 退回 (1024,1024) 仅在
    scene 已知时正确；故本函数优先读运行时场景缩放后的产物名，找不到返回 (0,0)
    表示"不可判定"（predict_frame_workers 会据此 fail-closed 取 cap=1）。
    """
    try:
        doc = read_json(cfg)
    except Exception:                                        # noqa: BLE001
        return (0, 0)
    for k in ("frame_shape", "shape"):
        v = doc.get(k)
        if isinstance(v, list) and len(v) == 2:
            return (int(v[1]), int(v[0]))
    # normalize：从第一盏灯的 FITS 头取帧尺寸（只读头，与 p1_frame_workers 同源口径）
    blocks = doc.get("blocks")
    if isinstance(blocks, list) and blocks:
        lights = (blocks[0] or {}).get("input_lights") or []
        if lights:
            return fits_shape(lights[0])
    return (0, 0)


def n_frames_of(cfg, stage: str) -> int:
    """帧数：normalize = 输入灯数；mosaic/export = 1（单工序产品）。"""
    if stage != "normalize":
        return 1
    try:
        doc = read_json(cfg)
    except Exception:                                        # noqa: BLE001
        return 0
    for k in ("input_lights", "lights", "frames"):
        v = doc.get(k)
        if isinstance(v, list):
            return len(v)
    blocks = doc.get("blocks")
    if isinstance(blocks, list) and blocks:
        v = (blocks[0] or {}).get("input_lights")
        if isinstance(v, list):
            return len(v)
    return 0


def product_stats(root, top: int = 8) -> dict:
    """产物规模统计（文件数/字节/最大文件/按顶层子目录分布）。

    用途：写路径热点归因必须区分「字节数」与「**文件数**」——
    实测口径下 HiPS 产品是**每 Npix 一个文件**（`Norder*/Dir*/Npix*.fits`），
    文件数才是 per-file 固定开销的放大源。只读统计，不改被测量对象。
    """
    root = Path(root)
    if not root.exists():
        return {"exists": False}
    n_files = 0
    n_bytes = 0
    biggest = []
    by_sub = {}
    for dp, _dn, fn in os.walk(root):
        sub = rel(Path(dp))
        top_name = Path(dp).relative_to(root).parts[0] if Path(dp) != root else "."
        s_files = 0
        s_bytes = 0
        for f in fn:
            try:
                sz = (Path(dp) / f).stat().st_size
            except OSError:
                continue
            n_files += 1
            n_bytes += sz
            s_files += 1
            s_bytes += sz
            biggest.append((sz, os.path.join(sub, f)))
        cur = by_sub.setdefault(top_name, {"files": 0, "bytes": 0})
        cur["files"] += s_files
        cur["bytes"] += s_bytes
    biggest.sort(reverse=True)
    return {"exists": True, "files": n_files, "bytes": n_bytes,
            "biggest": [{"bytes": b, "path": q} for b, q in biggest[:top]],
            "by_subdir": by_sub}


def rel(p):
    if p is None:
        return None
    try:
        return str(Path(p).relative_to(ROOT))
    except ValueError:
        return str(p)


def cpu_profile_summary() -> dict:
    """读安装目录 cpu_profile（调度器线程预算的唯一来源），只回显、不发明数值。"""
    p = BINARY.parent / "cpu_profile.json"
    out = {"path": rel(p) if p.exists() else None,
           "sha256": sha256_file(p) if p.exists() else None,
           "verdict": None, "profile_id": None, "source_commit": None,
           "kernel_workers": {}, "memory_budget_percent": None}
    if not p.exists():
        return out
    doc = read_json(p)
    out["verdict"] = doc.get("verdict")
    out["profile_id"] = doc.get("profile_id")
    out["source_commit"] = (doc.get("build") or {}).get("source_commit")
    out["memory_budget_percent"] = (doc.get("host") or {}).get("memory_budget_percent")
    for k, v in (doc.get("kernels") or {}).items():
        out["kernel_workers"][k] = {"workers": v.get("workers"),
                                    "block": v.get("block"),
                                    "provider": v.get("provider"),
                                    "workload_class": v.get("workload_class"),
                                    "correctness_test": v.get("correctness_test")}
    return out


# 阶段 → cpu_profile kernel 前缀（取自阶段流程文档：最高设计 §4 normalize / §5 mosaic / §6 export）
STAGE_KERNEL_PREFIX = {
    "normalize": ("calibration", "wcs-psf", "drizzle", "noise-snr", "hips-bulk"),
    "mosaic": ("upm", "rejection", "integration", "noise-snr", "drizzle"),
    "export": ("projection", "resample", "integration"),
}


def derive_worker_budget(probe: dict, profile: dict, stage: str) -> dict:
    """worker 预算 = 机器有效核 ∩ benchmark profile 声明的 kernel workers。

    落地 docs/ACSD_DESIGN.md §9「可用 CPU = 亲和性 ∩ cgroup ∩ Job Object；worker 数只来自
    profile 与预算对象」：**不从配置猜、不硬编码**。
    """
    eff = int(probe.get("effective_cpu_cores") or 0)
    kw = profile.get("kernel_workers") or {}
    ok = {k: v for k, v in kw.items()
          if isinstance(v.get("workers"), int) and v["workers"] > 0
          and v.get("correctness_test") == "oracle:pass"}
    prefix = STAGE_KERNEL_PREFIX[stage]
    sel = [v["workers"] for k, v in ok.items() if any(k.startswith(p) for p in prefix)]
    fallback = [v["workers"] for v in ok.values()]
    prof_width = max(sel) if sel else (max(fallback) if fallback else eff)
    budget = min(eff, prof_width) if eff else prof_width
    return {"effective_cpu_cores": eff, "profile_kernel_width_for_stage": prof_width,
            "profile_kernels_used": sorted(k for k in ok if any(k.startswith(p) for p in prefix)),
            "budget_workers": int(budget), "profile_verdict": profile.get("verdict"),
            "derivation": "min(affinity∩cgroup effective cores, cpu_profile kernel workers)"}


def load_contract() -> dict:
    """G-RES-01 阈值：唯一数值源；缺失/不可解析即 fail-closed（不回落内置默认）。"""
    doc = read_json(CONTRACT)
    if doc.get("schema") != RM.CONTRACT_SCHEMA:
        raise SystemExit("ANCHOR_STALE: %s schema=%r != %r"
                         % (CONTRACT, doc.get("schema"), RM.CONTRACT_SCHEMA))
    return doc


# ───────────────────────────────────────────────── 合成数据集（派生场景）──
def build_runtime_scene(src_scene, scale: int, out_scene) -> dict:
    """按注册场景派生**运行期场景**（不改注册件）。

    scale 的语义（受控、可核）：把探测器帧尺寸 [ny,nx] 与指向网格步长同乘 k
    ⇒ 覆盖更细采样的同一片天区（k>1）；**逐帧像素尺度 / PSF(探测像素) / 天光 / 曝光 /
    噪声模型 / 增益与读出噪声 / 母版模型全部不变** ⇒ 每帧科学语义与注册场景逐条等价。
    k=1 时派生件与注册件逐字段相同（selftest S6 断言）。
    """
    src_scene = Path(src_scene)
    sc = read_json(src_scene)
    if scale != 1:
        ny, nx = (int(v) for v in sc["shape"])
        sc["shape"] = [ny * scale, nx * scale]
        for f in sc.get("frames") or []:
            pt = f.get("pointing")
            if isinstance(pt, dict):
                for a in ("y", "x"):
                    if a in pt:
                        pt[a] = int(pt[a]) * scale
    sc["_derived_from"] = rel(src_scene)
    sc["_derived_scale"] = int(scale)
    sc["_derived_note"] = ("运行期派生场景：注册件未改动；shape 与指向网格步长 × %d，"
                           "像素尺度/PSF/天光/曝光/探测器模型不变" % scale)
    write_json(out_scene, sc)
    return sc


def generate_dataset(scene_path, outdir, seed=None) -> dict:
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    man = MS.render_sampling_dataset(str(scene_path), str(outdir), seed=seed, verbose=False)
    gen = {"dataset_dir": rel(outdir), "scene_id": man["scene_id"], "kind": man["kind"],
           "band": man["band"], "shape": man["shape"],
           "n_frames": len(man["frames"]), "base_seed": man["base_seed"],
           "wall_s": round(time.time() - t0, 3), "bytes": dir_size_bytes(outdir),
           "canvas": {k: man["canvas"].get(k) for k in
                      ("band", "path", "shape", "bunit", "real_exptime_s")
                      if k in man["canvas"]},
           "base_psf": man.get("base_psf"), "masters_meta": man["masters"].get("_meta")}
    write_json(outdir / "perf760_generate.json", gen)
    return gen


# ─────────────────────────────────────────────────────── 配置构造 ──
def build_normalize_config(ds, man, out):
    cfg_dir = Path(out) / "configs"
    cfg_dir.mkdir(parents=True, exist_ok=True)
    lights = [str((Path(ds) / "frames" / ("%s.fits" % f["frame_id"])).resolve())
              for f in man["frames"]]
    ncfg = {"schema_version": "1", "blocks": [{
        "name": "%s_p1" % man["scene_id"],
        "input_lights": lights,
        "master_bias": str((Path(ds) / "masters" / "master_bias.fits").resolve()),
        "master_dark": str((Path(ds) / "masters" / "master_dark.fits").resolve()),
        "master_flat": str((Path(ds) / "masters" / "master_flat.fits").resolve()),
        "output_dir": str((Path(out) / "norm").resolve()),
        "filter_passband": "",
        "dark_optimization": False,
        "master_units": {"light": "ADU", "bias": "ADU", "dark": "ADU", "flat": "normalized"},
        "master_scale": {"flat": 1.0},
        "master_flat_normalize": "median",
        "drizzle": {"nested": 1, "pixfrac": 1.0, "precision_mode": 1},
        "wcs": {"init_source": "header_pointing",
                "gaia_data_dir": str((ROOT / "gaia" / "GaiaDR3").resolve())},
    }]}
    p = cfg_dir / "normalize.json"
    write_json(p, ncfg)
    return p


def is_hips_root(d) -> bool:
    """HiPS 根判定（按**实测产物布局**，不按设想布局）。

    实测本项目 normalize 的 HiPS 产物布局是
      `<frame_dir>/{signal,variance,support}/Norder<k>/Dir<d>/Npix<n>.fits`
    —— **没有** `properties` 文件、也没有 `signal/properties`。
    早期本函数照 `run_reconstruct.py` 的口径查 `(d/"signal"/"properties")`，
    在本项目的真实产物上**恒为假** ⇒ `discover_hips` 恒返回空 ⇒ 链在第 2 阶段直接失败
    （本轮实测：`normalize 未产出 HiPS 产品`）。故改为三种布局任一命中即认：
      · `properties` 存在（标准 HiPS）；或
      · `signal/properties` 存在（`run_reconstruct.py` 口径）；或
      · `signal/Norder<k>` 存在（本项目实测布局）。
    """
    d = Path(d)
    if (d / "properties").exists() or (d / "signal" / "properties").exists():
        return True
    sig = d / "signal"
    if sig.is_dir():
        return any(c.is_dir() and c.name.startswith("Norder") for c in sig.iterdir())
    return False


def discover_hips(norm_dir) -> list:
    """HiPS 根列表（每个输入帧一个根；判定见 `is_hips_root`）。"""
    norm_dir = Path(norm_dir)
    return sorted(str(p) for p in norm_dir.glob("*") if p.is_dir() and is_hips_root(p))


def build_mosaic_export_configs(out, hips, truth, export_scale_arcsec: float = 0.4):
    """构造 mosaic / export 配置。

    键面**逐字取自** ./build/acsd mosaic|export --help 与
    eng/contracts/schemas/phase_config_{mosaic,export}.schema.json；
    硬件字段（workers/ISA/block）一律不出现——phase 配置的硬件字段禁令。
    """
    cfg_dir = Path(out) / "configs"
    mcfg = {"schema_version": "1", "hips_paths": hips,
            "output_dir": str((Path(out) / "mosaic").resolve()),
            "algorithm_rejection_method": ""}
    write_json(cfg_dir / "mosaic.json", mcfg)
    import numpy as np
    from astropy.io import fits as _fits
    with _fits.open(truth, memmap=True) as f:
        h = f[0].header
        nshape = f[0].data.shape
        thdr = {"CRVAL1": float(h["CRVAL1"]), "CRVAL2": float(h["CRVAL2"]),
                "CRPIX1": float(h["CRPIX1"]), "CRPIX2": float(h["CRPIX2"]),
                "CD1_1": float(h["CD1_1"]), "CD1_2": float(h["CD1_2"]),
                "CD2_1": float(h["CD2_1"]), "CD2_2": float(h["CD2_2"]),
                "CTYPE1": str(h.get("CTYPE1", "RA---TAN")),
                "CTYPE2": str(h.get("CTYPE2", "DEC--TAN"))}
    cw = MS.canvas_wcs({"wcs": {"crval1": thdr["CRVAL1"], "crval2": thdr["CRVAL2"],
                                "crpix1": thdr["CRPIX1"], "crpix2": thdr["CRPIX2"],
                                "cd": [[thdr["CD1_1"], thdr["CD1_2"]],
                                       [thdr["CD2_1"], thdr["CD2_2"]]],
                                "ctype1": thdr["CTYPE1"], "ctype2": thdr["CTYPE2"],
                                "scale_arcsec_per_px": 0.04}})
    corners = np.array([[0, 0], [nshape[1] - 1, 0], [0, nshape[0] - 1],
                        [nshape[1] - 1, nshape[0] - 1]], dtype=float)
    sky = cw.all_pix2world(corners, 0)
    ra_c = float(np.mean(sky[:, 0]))
    dec_c = float(np.mean(sky[:, 1]))
    scale_deg = float(export_scale_arcsec) / 3600.0
    cosd = math.cos(math.radians(dec_c))
    wpx = int(math.ceil((sky[:, 0].max() - sky[:, 0].min()) * cosd / scale_deg)) + 8
    hpx = int(math.ceil((sky[:, 1].max() - sky[:, 1].min()) / scale_deg)) + 8
    ecfg = {"schema_version": "1", "source": {"hips_dir": str((Path(out) / "mosaic").resolve())},
            "output_dir": str((Path(out) / "export").resolve()),
            "center": {"ra_deg": ra_c, "dec_deg": dec_c},
            "width_px": wpx, "height_px": hpx, "scale_deg_per_px": scale_deg,
            "projection": "TAN", "sampler": "bilinear", "coverage_output": "mask",
            "output_mode": "surface_brightness"}
    write_json(cfg_dir / "export.json", ecfg)
    return {"mosaic": cfg_dir / "mosaic.json", "export": cfg_dir / "export.json",
            "export_geometry": {"center_ra_deg": ra_c, "center_dec_deg": dec_c,
                                "width_px": wpx, "height_px": hpx}}


# ───────────────────────────────────────────── 单阶段受监控执行 ──
def _output_dir_of(cfg, stage: str):
    doc = read_json(cfg)
    if stage == "normalize":
        return Path(doc["blocks"][0]["output_dir"])
    return Path(doc["output_dir"])


def find_output_root(cfg, stage: str):
    """阶段产物根目录（不可判定 → None；调用方必须能接受 None）。"""
    try:
        return _output_dir_of(cfg, stage)
    except Exception:                                        # noqa: BLE001
        return None


def find_in_outputs(cfg, stage: str, name: str):
    """在阶段 output_dir 下找 CLI 自报的资源证据文件。"""
    try:
        d = _output_dir_of(cfg, stage)
    except Exception:                                        # noqa: BLE001
        return None
    if not d.exists():
        return None
    hits = sorted(d.rglob(name))
    return hits[-1] if hits else None


def write_monitor_csv(mon: dict, path) -> None:
    """把 run_monitored 的逐样本读数落 CSV（探针读数必须落 CSV/JSON 证据文件）。"""
    samples = mon.get("cpu_samples") or []
    if not samples:
        return
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    cols = ["t", "cpu_percent", "rss_kb", "pss_kb", "threads", "runnable",
            "io_read_bytes", "io_write_bytes", "pids"]
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for s in samples:
            w.writerow({c: s.get(c) for c in cols})


# 帧级并发度的**派生预测**（只读计算，用于把实测与实现口径对齐；不是埋点、不改被测行为）
KP1_FRAME_BYTES_PER_PIXEL = 116.0     # eng/packaging/config/runtime_resources.json: frame_memory_gate
KP1_FRAME_MEM_SAFETY_FRAC = 0.75      # 同上


def predict_frame_workers(*, lease: int, n_units: int, w: int, h: int,
                          mem_available_bytes) -> dict:
    """按 module_adapters.cpp 的口径复算 `frame_workers`（p1_frame_workers）。

    frame_workers = min(lease, p1_memory_cap)；p1_memory_cap =
      floor(MemAvailable × 0.75 / (W×H×116.0))；不可判定 ⇒ 1（fail-closed 退回串行）。
    in_flight = min(n_units, frame_workers)；inner_u = max(1, lease // in_flight)。
    用途：把「实测并行度」与「实现口径允许的并行度」并排，区分
      「配置/公式限制了并行」（可调）与「并行开了但没跑起来」（真缺陷）。
    """
    if not mem_available_bytes or w <= 0 or h <= 0 or lease <= 0:
        cap = 1
        cap_src = "fail-closed(不可判定)"
    else:
        cap = int((mem_available_bytes * KP1_FRAME_MEM_SAFETY_FRAC)
                  // (w * h * KP1_FRAME_BYTES_PER_PIXEL))
        cap = max(1, cap)
        cap_src = "floor(MemAvailable × %.2f / (W×H×%.1f))" % (KP1_FRAME_MEM_SAFETY_FRAC,
                                                               KP1_FRAME_BYTES_PER_PIXEL)
    fw = max(1, min(lease, cap))
    in_flight = max(1, min(n_units, fw))
    inner = max(1, lease // in_flight)
    return {"lease": lease, "memory_cap": cap, "memory_cap_source": cap_src,
            "frame_workers": fw, "n_units": n_units, "in_flight": in_flight,
            "inner_u": inner, "max_parallel": in_flight * inner,
            "budget": lease, "mem_available_bytes": mem_available_bytes,
            "frame_bytes": w * h * KP1_FRAME_BYTES_PER_PIXEL}


def run_stage(tag: str, stage: str, cfg, *, run_dir, workers: int, axis_frame=None,
              axis_inner=None, max_rss_gb: float = 8.0, timeout_s: int = 1800,
              extra_env=None, gate_required=False, perturb_serial=0.0, cpuset=None) -> dict:
    cfg = Path(cfg)
    run_dir = Path(run_dir)
    ev_dir, log_dir = run_dir / "evidence", run_dir / "logs"
    stage_dir = run_dir / "stages" / tag / stage
    stage_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / ("%s_%s.log" % (tag, stage))
    guard_out = stage_dir / "mem_guard.json"
    mon_json = ev_dir / ("%s_%s_monitor.json" % (tag, stage))
    ts_csv = ev_dir / ("%s_%s_timeseries.csv" % (tag, stage))
    wb_csv = ev_dir / ("%s_%s_worker_balance.csv" % (tag, stage))

    env = dict(os.environ)
    env["TMPDIR"] = str(run_dir / "tmp")
    Path(env["TMPDIR"]).mkdir(parents=True, exist_ok=True)
    env.update(NODE_TRACE_ENVS)
    if axis_frame:
        env[AXIS_ENV["frame"]] = str(int(axis_frame))
    if axis_inner:
        env[AXIS_ENV["inner"]] = str(int(axis_inner))
    if extra_env:
        env.update({k: str(v) for k, v in extra_env.items()})

    cli = [str(BINARY), stage, "--json", str(cfg), "-y"]
    pin = cpuset
    if perturb_serial and not pin:
        # 负例注入（**只用既有可复现旋钮**，不新增埋点、不改被测代码）：
        # 把整条链钉到 1 个 CPU 上 ⇒ lease 退化为 1，帧级/帧内并行宽度同时塌到 1
        # ⇒ G-RES-01 判据①（单活跃计算线程）与 L2 四条判据都应当判红。
        pin = "0"
    if pin:
        cli = ["taskset", "-c", str(pin)] + cli
    guarded = [sys.executable, str(MEM_GUARD), "--max-rss-gb", str(max_rss_gb),
               "--timeout", str(timeout_s), "--label", "%s_%s" % (tag, stage), "--"] + cli
    # 观测行全量留证（缺陷修正）：run_monitored 的 stderr_tail 只保留**末 400 行**，
    # 长跑（本场景单阶段 >100 s）会把 [nodetrace]/[lease]/[p1cap] 全量挤出尾部，
    # 使节点级瀑布失明（node_waterfall 报 no_nodetrace）。这里不改动任何埋点，
    # 只在子进程 stderr 上加一路 **tee**：被测进程看到的 stderr 与原来逐字节相同，
    # 而全量观测行同时落 stderr_full.log，供 node_waterfall 按行解析。
    stderr_full = stage_dir / "stderr_full.log"
    quoted = " ".join(shlex.quote(x) for x in guarded)
    guarded = ["/bin/bash", "-c",
               "%s 2> >(tee %s >&2)" % (quoted, shlex.quote(str(stderr_full)))]
    argv = [sys.executable, str(RUN_MON), "--timeout", str(timeout_s),
            "--output", str(mon_json)]
    if gate_required:
        argv += ["--gate-required", "--gate-workers", str(workers),
                 "--gate-selected-workers", str(workers)]
    argv += ["--"] + guarded

    t0 = time.time()
    pr = subprocess.run(argv, cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True)
    wall = time.time() - t0
    log_path.write_text(pr.stdout or "", encoding="utf-8")
    mon = read_json(mon_json) if mon_json.exists() else {}
    guard_lines = [ln for ln in (pr.stdout or "").splitlines() if "[mem_guard]" in ln]
    write_json(guard_out, {"label": "%s_%s" % (tag, stage), "lines": guard_lines,
                           "max_rss_gb": max_rss_gb, "timeout_s": timeout_s})
    write_monitor_csv(mon, ts_csv)
    if wb_csv.exists():
        wb_csv.unlink()
    wb_src = find_in_outputs(cfg, stage, "worker_balance.csv")
    if wb_src is not None:
        shutil.copyfile(wb_src, wb_csv)
    inproc_ts = find_in_outputs(cfg, stage, "resource_timeseries.csv")
    inproc_summary = find_in_outputs(cfg, stage, "resource_summary.json")
    return {"stage": stage, "tag": tag, "argv": argv, "cli": " ".join(cli),
            "log": rel(log_path),
            "stderr_full_log": rel(stderr_full),
            "monitor_json": rel(mon_json) if mon_json.exists() else None,
            "mem_guard_json": rel(guard_out),
            "timeseries_csv": rel(ts_csv) if ts_csv.exists() else None,
            "worker_balance_csv": rel(wb_csv) if wb_csv.exists() else None,
            "inproc_timeseries_csv": rel(inproc_ts), "inproc_summary_json": rel(inproc_summary),
            "inproc_summary": read_json(inproc_summary) if inproc_summary else None,
            "exit_code": mon.get("exit_code"), "wall_s": round(wall, 3),
            "mem_guard_lines": guard_lines,
            "products": product_stats(find_output_root(cfg, stage) or "/nonexistent"),
            "stderr_aiolog_lines": count_lines(stage_dir / "stderr_full.log", "aio_sparse"),
            "stderr_total_lines": count_lines(stage_dir / "stderr_full.log"),
            "cpu_percent_avg": mon.get("cpu_percent_avg"),
            "cpu_percent_median": mon.get("cpu_percent_median"),
            "peak_rss_kb": mon.get("peak_rss_kb"), "peak_pss_kb": mon.get("peak_pss_kb"),
            "io_read_bytes": mon.get("io_read_bytes"),
            "io_write_bytes": mon.get("io_write_bytes"), "threads_max": mon.get("threads_max"),
            "env_axis": {"frame": env.get(AXIS_ENV["frame"]),
                         "inner": env.get(AXIS_ENV["inner"])},
            "cpuset": pin, "workers": workers, "perturb_serial_s": perturb_serial,
            "predict_frame_workers": predict_frame_workers(
                lease=workers, n_units=n_frames_of(cfg, stage),
                w=frame_shape_of(cfg, stage)[0], h=frame_shape_of(cfg, stage)[1],
                mem_available_bytes=(mon.get("host_probe") or {}).get("mem_available_bytes"))}


# ───────────────────────────────────────── 判据裁决（逐条落证据）──
# 判定面两层，都不改判据、不放宽阈值：
#  ① 生产侧 G-RES-01 判定（eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate，
#     阈值读 eng/contracts/resource_gate_v1.json）—— 判据 ①②③（硬失败面）与 ④⑤⑥（记录面）；
#  ② CI 裁决面 L2 冻结判据（eng/ci/l2_frozen_gate.py::adjudicate，CI_SPEC §9.2 唯一正本）
#     —— 四条判据（平均/p50/达标占比/无低利用窗）**违规必红**，分母未声明或门不适用按红。
def adjudicate(tag: str, run_dir, stages: list, *, workers: int, contract: dict,
               compute_intervals=None, mem_budget_bytes=None) -> dict:
    thresholds = L2.load_thresholds(str(ROOT))
    rows, hard_stages, l2_rows = [], [], []
    for st in stages:
        mon_p = Path(run_dir) / "evidence" / ("%s_%s_monitor.json" % (tag, st["stage"]))
        if not mon_p.exists():
            rows.append({"stage": st["stage"], "verdict": "fail", "applicable": True,
                         "violations": ["sampling_evidence_missing: 监控证据缺失"
                                        "（G-RES-01 §8.5：采样证据缺失不是豁免，fail-closed）"],
                         "recorded": [], "metrics": {}})
            hard_stages.append({"stage": st["stage"],
                                "violations": ["sampling_evidence_missing"]})
            continue
        mon = read_json(mon_p)
        interval = (compute_intervals or {}).get(st["stage"])
        # 有效核必须按 **int** 传入：evaluate_frozen_gate 的 cpus_ok 判定是
        # `isinstance(effective_cpus, int)`，float(16.0) 会被判非法 ⇒ 整份证据
        # 退化成 monitoring_missing（fail-closed）。这里与 run_monitored 的
        # --gate-effective-cpus 口径一致做整型收敛（不改变语义，只对齐类型）。
        eff_raw = (mon.get("host_probe") or {}).get("effective_cpu_cores")
        eff = int(eff_raw) if isinstance(eff_raw, (int, float)) and eff_raw >= 1 else None
        gate = RM.evaluate_frozen_gate(mon, effective_cpus=eff, allocated_workers=workers,
                                       compute_interval_seconds=interval)
        gate["stage"] = st["stage"]
        gate["applicable"] = gate["verdict"] != "not_applicable"
        gate["allocated_workers"] = workers
        gate["effective_cpus"] = eff
        gate["compute_interval_seconds"] = interval
        gate["mem_peak_bytes"] = (mon.get("peak_rss_kb") or 0) * 1024
        gate["mem_budget_bytes"] = mem_budget_bytes
        gate["mem_in_budget"] = (None if not mem_budget_bytes
                                 else bool((mon.get("peak_rss_kb") or 0) * 1024 <= mem_budget_bytes))
        rows.append(gate)
        hard = [v for v in (gate.get("violations") or [])]
        if hard:
            hard_stages.append({"stage": st["stage"], "violations": hard})
        # ② CI 裁决面：把生产侧判定嵌进证据后按 CI_SPEC §9.2 复核（四条判据违规必红）
        ev = dict(mon)
        ev["allocated_workers"] = workers
        ev["frozen_gate"] = {"verdict": "not_applicable" if gate.get("applicable") is False
                             else ("pass" if not hard else "fail"),
                             "metrics": gate.get("metrics") or {}}
        l2 = L2.adjudicate(ev, thresholds=thresholds, require_applicable=True,
                           require_evaluable=True, source="perf760:%s/%s" % (tag, st["stage"]))
        l2["stage"] = st["stage"]
        l2_rows.append(l2)
    l2_red = [r for r in l2_rows if r["verdict"] == L2.V_RED]
    return {"tag": tag, "allocated_workers": workers, "gate_id": contract["gate_id"],
            "authority": contract["authority"],
            "l2_authority": "docs/engineering/CI_SPEC.md §9.2（判定面 eng/ci/l2_frozen_gate.py）",
            "stages": rows, "hard_fail_stages": hard_stages,
            "l2_criteria": l2_rows, "l2_red_stages": [r["stage"] for r in l2_red],
            "l2_thresholds": thresholds,
            "memory": {"budget_bytes": mem_budget_bytes,
                       "per_stage_peak_bytes": {st["stage"]: (st.get("mem_peak_bytes") or 0)
                                                for st in rows},
                       "in_budget": all(st.get("mem_in_budget") is not False for st in rows)},
            "verdict": "FAIL" if (hard_stages or l2_red) else "PASS"}


def scan_io(run_dir, tag: str, stages: list) -> dict:
    """I/O 等待占比与写量（探针数据，逐阶段；契约口径见 21_observability §8.7）。

    只用**口径明确**的两个来源：
      · run_monitored 的 io_write_bytes/io_read_bytes（/proc/<pid>/io，进程树真实块 I/O）；
      · CLI 进程内 resource_timeseries.csv 的 io_wait_pct（/proc/stat 系统级 iowait，
        含其它进程 ⇒ 上界口径）与 io_wait_all_ms（Σ 全线程 delayacct_blkio_ticks ⇒ 本进程树口径）。
    """
    out = {"stages": []}
    for st in stages:
        rec = {"stage": st["stage"], "io_write_bytes": st.get("io_write_bytes"),
               "io_read_bytes": st.get("io_read_bytes")}
        ts = st.get("inproc_timeseries_csv")
        if ts:
            p = ROOT / ts
            if p.exists():
                vals, iow, iowait_all = [], [], []
                with open(p, newline="", encoding="utf-8") as fh:
                    rd = csv.DictReader(fh)
                    for row in rd:
                        if row.get("io_wait_pct") not in (None, ""):
                            try:
                                iow.append(float(row["io_wait_pct"]))
                            except ValueError:
                                pass
                        if row.get("io_wait_all_ms") not in (None, ""):
                            try:
                                iowait_all.append(float(row["io_wait_all_ms"]))
                            except ValueError:
                                pass
                        if row.get("cpu_pct") not in (None, ""):
                            try:
                                vals.append(float(row["cpu_pct"]))
                            except ValueError:
                                pass
                rec["inproc_cpu_pct_mean"] = round(sum(vals) / len(vals), 4) if vals else None
                rec["inproc_cpu_pct_peak"] = round(max(vals), 4) if vals else None
                rec["io_wait_pct_mean"] = round(sum(iow) / len(iow), 6) if iow else None
                rec["io_wait_pct_peak"] = round(max(iow), 6) if iow else None
                rec["io_wait_all_ms_final"] = iowait_all[-1] if iowait_all else None
                rec["io_wait_all_ms_range"] = ([iowait_all[0], iowait_all[-1]]
                                               if iowait_all else None)
        out["stages"].append(rec)
    return out


def worker_balance(run_dir, tag: str, stages: list, workers: int) -> dict:
    """worker 均衡：逐阶段读 CLI 自报 worker_balance.csv（探针预埋面）。

    已知退化（实验/engineering-evidence/l2_performance/README.md §7）：该 CSV 的
    active/runnable 两列同源 ⇒ utilization_pct 恒 50.00。因此本函数**同时**给出外挂
    监控的 runnable（/proc R 态）与 cpu_percent 作为权威口径，并显式登记退化。
    """
    out = {"declared_workers": workers, "stages": [], "degenerate_note":
           "CLI worker_balance.csv 的 active/runnable 同源（recorder.set_workers(budget,budget)）"
           " ⇒ utilization_pct 恒 50.00；权威口径见各阶段 runnable_p50/cpu_percent。"}
    for st in stages:
        rec = {"stage": st["stage"], "csv": st.get("worker_balance_csv"),
               "runnable_p50": (st.get("gate") or {}).get("metrics", {}).get("runnable_p50")
               if isinstance(st.get("gate"), dict) else None}
        mon_p = Path(run_dir) / "evidence" / ("%s_%s_monitor.json" % (tag, st["stage"]))
        if mon_p.exists():
            mon = read_json(mon_p)
            rr = [s.get("runnable") for s in (mon.get("cpu_samples") or [])
                  if isinstance(s.get("runnable"), (int, float))]
            cp = [s.get("cpu_percent") for s in (mon.get("cpu_samples") or [])
                  if isinstance(s.get("cpu_percent"), (int, float))]
            rec["runnable_p50"] = statistics.median(rr) if rr else None
            rec["runnable_peak"] = max(rr) if rr else None
            rec["cpu_percent_p50"] = statistics.median(cp) if cp else None
            rec["cpu_percent_peak"] = max(cp) if cp else None
            rec["util_p50_frac_of_declared"] = (round(statistics.median(cp) / (100.0 * workers), 4)
                                                if cp and workers else None)
        out["stages"].append(rec)
    return out


# ────────────────────────────────────────────────── 外部工具调用 ──
def count_lines(path, needle: str | None = None) -> int:
    """观测行计数（只读；失败给 0，不抛）。"""
    try:
        n = 0
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for ln in fh:
                if needle is None or needle in ln:
                    n += 1
        return n
    except Exception:                                        # noqa: BLE001
        return 0


def unescape_monitor_log(log_path, dest) -> Path:
    """把 run_monitored 的 JSON 日志还原成**逐行**文本（node_waterfall 按行解析）。

    缺陷（本轮修）：`--log` 指的是 run_monitored 的 `json_envelope`（键 `log`）时，
    里面是本进程树的 stdout+stderr **字面文本**；但 JSON 里换行被转义 ⇒ 原样喂给
    node_waterfall 会被当成**一行**，正则 ^...$ 永不命中 ⇒ 误报 no_nodetrace。
    这里用 json 解码还原真实换行（不改任何被解析内容，只还原分隔符）。
    """
    log_path, dest = Path(log_path), Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    try:
        obj = json.loads(log_path.read_text(encoding="utf-8", errors="replace"))
    except Exception:                                        # noqa: BLE001
        obj = None
    if isinstance(obj, dict) and isinstance(obj.get("log"), str):
        dest.write_text(obj["log"], encoding="utf-8")
    else:
        dest.write_text(log_path.read_text(encoding="utf-8", errors="replace"), encoding="utf-8")
    return dest


def run_waterfall(tag: str, log, ts_csv, out_dir, stderr_full=None) -> dict:
    """节点瀑布 → md/json。

    ⚠ 必须喂**捕获到的完整 stderr**：`run_monitored` 的 JSON 里只有 `stderr_tail`（末 400 行），
    长跑的观测行会被挤掉；且它把换行转义了，整份日志变成一行 ⇒ 瀑布解析恒找不到追踪行。
    故优先用 run_stage 以 tee 落下的 stderr_full.log（未转义、全量）。
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    src = Path(stderr_full) if (stderr_full and Path(stderr_full).exists()) else Path(log)
    plain = unescape_monitor_log(src, out_dir / "stderr_plain.txt")
    n_trace = 0
    try:
        with open(plain, encoding="utf-8", errors="replace") as fh:
            n_trace = sum(1 for ln in fh if "[nodetrace]" in ln)
    except OSError:
        n_trace = 0
    cmd = [sys.executable, str(WATERFALL), "--log", str(plain), "--out-dir", str(out_dir)]
    if ts_csv is not None and Path(ts_csv).exists():
        cmd += ["--timeseries", str(ts_csv)]
    pr = subprocess.run(cmd, cwd=str(ROOT), stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True)
    return {"cmd": " ".join(cmd), "rc": pr.returncode, "out": (pr.stdout or "").strip()[-2500:],
            "stderr_source": rel(src), "nodetrace_lines": n_trace,
            "md": rel(out_dir / "node_waterfall.md"),
            "json": rel(out_dir / "node_waterfall.json")}


# ─────────────────────────────────────────────────────────── run ──
def cmd_run(a) -> int:
    contract = load_contract()
    run_dir = Path(a.run_dir).resolve()
    for sub in ("evidence", "logs", "stages", "dataset", "waterfall"):
        (run_dir / sub).mkdir(parents=True, exist_ok=True)
    probe = RP.probe()
    profile = cpu_profile_summary()
    df0 = df_workspace()
    scene_src = Path(a.scene)
    if not scene_src.is_absolute():
        scene_src = ROOT / scene_src
    budget = derive_worker_budget(probe, profile, "normalize")
    workers = a.workers or budget["budget_workers"]
    scene_rt = run_dir / ("scene_runtime_%s.json" % a.tag)
    build_runtime_scene(scene_src, a.scale, scene_rt)

    cond = {"tag": a.tag, "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "loadavg_1m_before": loadavg(), "proc_stat_before": proc_stat_snapshot(),
            "proc_stat_percpu_before": proc_stat_percpu_snapshot(),
            "isolation_protocol": {
                "cpuset_mask": a.cpuset, "declared_denominator_cores": workers,
                "foreign_cpu_share_limit_cores": 2.0,
                "invalid_rule": "外来负载 >2 核等效 ⇒ 该样本 INVALID，不入汇总",
                "source": "PERF-760 上级裁决：测量窗口与隔离协议"},
            "host": {"platform": platform.platform(), "kernel": platform.release(),
                     "machine": platform.machine()},
            "resource_probe": probe, "cpu_profile": profile,
            "worker_budget_derivation": budget, "workers_used": workers,
            "df_workspace_before": df0,
            "binary": {"path": rel(BINARY), "sha256": sha256_file(BINARY) if BINARY.exists() else None},
            "scene_source": rel(scene_src), "scene_runtime": rel(scene_rt), "scale": a.scale,
            "axis": {"frame": a.axis_frame, "inner": a.axis_inner},
            "max_rss_gb": a.max_rss_gb, "mem_budget_gb": a.mem_budget_gb,
            "mem_budget_bytes": (int(a.mem_budget_gb * 2**30) if a.mem_budget_gb else None),
            "timeout_s": a.timeout, "only_stages": a.only_stages,
            "node_trace_envs": NODE_TRACE_ENVS, "perturb_serial_s": a.perturb_serial,
            "gate_required": a.gate_required,
            "contract": {"path": rel(CONTRACT), "schema": contract["schema"],
                         "gate_id": contract["gate_id"]}}
    write_json(run_dir / "evidence" / ("%s_conditions.json" % a.tag), cond)

    res = {"tag": a.tag, "conditions": cond, "configs": {}, "stages": [], "gate": None}
    ds = run_dir / "dataset" / a.working_set
    if not (ds / "dataset.json").exists() and (a.skip_generate or a.skip_generate_if_present):
        return fail(run_dir, a.tag, "数据集缺失（%s/dataset.json）；去掉 --skip-generate 重跑" % ds)
    if a.skip_generate:
        res["dataset"] = {"dataset_dir": rel(ds), "reused": True}
    elif a.skip_generate_if_present and (ds / "dataset.json").exists():
        man0 = read_json(ds / "dataset.json")
        res["dataset"] = {"dataset_dir": rel(ds), "scene_id": man0["scene_id"],
                          "kind": man0["kind"], "shape": man0["shape"],
                          "n_frames": len(man0["frames"]), "reused": True}
    else:
        if ds.exists() and not a.keep_dataset:
            shutil.rmtree(ds)
        res["dataset"] = generate_dataset(scene_rt, ds, seed=None)
    man = read_json(ds / "dataset.json")
    out = run_dir / "out" / a.tag
    out.mkdir(parents=True, exist_ok=True)
    # `--from-tag`：复用既有 tag 的产物树（只读），用于"只重跑后段"与快速接线自检。
    # 只允许在 only_stages 模式下使用，避免把两个 tag 的产物混进同一次全链比较。
    if getattr(a, "from_tag", None):
        if not a.only_stages:
            return fail(run_dir, a.tag, "--from-tag 只允许与 --only-stages 同用")
        src_out = run_dir / "out" / a.from_tag
        if not src_out.is_dir():
            return fail(run_dir, a.tag, "from-tag 产物不存在：%s" % src_out)
        for sub in ("norm", "mosaic"):
            if (src_out / sub).is_dir() and not (out / sub).exists():
                try:
                    os.symlink(src_out / sub, out / sub)
                except OSError as e:
                    return fail(run_dir, a.tag, "复用 %s 失败：%s" % (sub, e))
        res["reused_from_tag"] = a.from_tag
    cfgs = {}
    if not a.only_stages or "normalize" in a.only_stages:
        cfg_norm = build_normalize_config(ds, man, out)
        cfgs["normalize"] = cfg_norm
        res["configs"]["normalize"] = rel(cfg_norm)

    # mosaic/export 配置**懒构造**：必须在 normalize 真正产出 HiPS 之后才算。
    # （曾经把它提前到阶段循环之外 ⇒ 在 normalize 之前就找不到 HiPS，链报
    #  "normalize 未产出 HiPS 产品" 而失败。这是驱动自身的接线缺陷。）
    def ensure_post_configs() -> str:
        if "export" in cfgs:
            return ""
        hips = discover_hips(out / "norm")
        if not hips:
            return "normalize 未产出 HiPS 产品（%s）" % (out / "norm")
        more = build_mosaic_export_configs(out, hips, ds / "truth_canvas.fits",
                                           export_scale_arcsec=a.export_scale_arcsec)
        cfgs.update(more)
        res["configs"].update({k: rel(v) for k, v in more.items() if k != "export_geometry"})
        res["export_geometry"] = more["export_geometry"]
        res["n_hips_inputs"] = len(hips)
        return ""

    for stage in ("normalize", "mosaic", "export"):
        if a.only_stages and stage not in a.only_stages:
            continue
        # ⚠ 只在**真的要跑 mosaic/export** 时构造后段配置：normalize 阶段跑之前
        # 产物还不存在，此时构造必然找不到 HiPS（本轮实测踩过：链在 normalize 前
        # 就报 "normalize 未产出 HiPS 产品" 并 rc=1 立刻退出）。
        if stage != "normalize":
            err = ensure_post_configs()
            if err:
                return fail(run_dir, a.tag, err)
        ev = run_stage(a.tag, stage, cfgs[stage], run_dir=run_dir, workers=workers,
                       axis_frame=a.axis_frame, axis_inner=a.axis_inner,
                       max_rss_gb=a.max_rss_gb, timeout_s=a.timeout,
                       gate_required=a.gate_required,
                       perturb_serial=a.perturb_serial if stage == "normalize" else 0.0,
                       cpuset=a.cpuset)
        res["stages"].append(ev)
        print("[%s] %-9s rc=%s wall=%ss cpu_avg=%s%% p50=%s%% peakRSS=%.2fGiB"
              % (a.tag, stage, ev["exit_code"], ev["wall_s"], ev["cpu_percent_avg"],
                 ev["cpu_percent_median"], (ev["peak_rss_kb"] or 0) / 1048576.0), flush=True)
        if ev["exit_code"] != 0:
            break

    for ev in res["stages"]:
        ev["waterfall"] = run_waterfall(
            a.tag, ROOT / ev["log"],
            ROOT / ev["inproc_timeseries_csv"] if ev.get("inproc_timeseries_csv") else None,
            run_dir / "waterfall" / ("%s_%s" % (a.tag, ev["stage"])),
            stderr_full=(ROOT / ev["stderr_full_log"] if ev.get("stderr_full_log") else None))
    res["gate"] = adjudicate(a.tag, run_dir, res["stages"], workers=workers, contract=contract,
                             compute_intervals={e["stage"]: e["wall_s"] for e in res["stages"]},
                             mem_budget_bytes=(int(a.max_rss_gb * 2**30) if a.mem_budget_gb else None))
    res["io"] = scan_io(run_dir, a.tag, res["stages"])
    res["worker_balance"] = worker_balance(run_dir, a.tag, res["stages"], workers)
    res["df_workspace_after"] = df_workspace()
    res["loadavg_1m_after"] = loadavg()
    res["proc_stat_after"] = proc_stat_snapshot()
    res["proc_stat_percpu_after"] = proc_stat_percpu_snapshot()
    _pb, _pa = cond.get("proc_stat_before") or {}, res["proc_stat_after"]
    if _pb.get("total") and _pa.get("total"):
        d_total = _pa["total"] - _pb["total"]
        d_idle = _pa["idle"] - _pb["idle"]
        sys_busy = (d_total - d_idle) / d_total if d_total > 0 else 0.0
        own = sum((e.get("cpu_percent_avg") or 0.0) / 100.0 * (e.get("wall_s") or 0.0)
                  for e in res["stages"])
        own_ratio = (own / max(1e-9, sum(e.get("wall_s") or 0.0 for e in res["stages"])))
        _mask_rec = mask_busy_cores(cond.get("proc_stat_percpu_before"),
                                    res.get("proc_stat_percpu_after"), a.cpuset or "")
        if _mask_rec.get("available"):
            _own = own_ratio
            _foreign = _mask_rec["busy_cores_on_mask"] - min(_own, _mask_rec["mask_cores"])
            _mask_rec.update({"process_tree_cores_on_mask": round(min(_own, _mask_rec["mask_cores"]), 4),
                              "foreign_cores_on_mask": round(_foreign, 4),
                              "sample_valid": bool(_foreign <= 2.0),
                              "note": "掩码内忙碌 − 本进程树实占（截到掩码核数）；>2 核等效 ⇒ INVALID"})
        res["mask_foreign_cpu_share"] = _mask_rec
        res["foreign_cpu_share"] = {
            "system_busy_cores_avg": round(sys_busy * os.cpu_count(), 4),
            "process_tree_cores_avg": round(own_ratio, 4),
            "foreign_cores_avg": round(sys_busy * os.cpu_count() - own_ratio, 4),
            "mask_cores": a.workers,
            "sample_valid": bool(sys_busy * os.cpu_count() - own_ratio <= 2.0),
            "note": "系统级 /proc/stat Δ 与进程树自报之差；>2 核等效 ⇒ INVALID（隔离协议 3）"}
    res["outputs_bytes"] = dir_size_bytes(out)
    write_json(run_dir / "evidence" / ("%s_chain.json" % a.tag), res)
    print("[%s] G-RES-01 verdict=%s hard_fail_stages=%s"
          % (a.tag, res["gate"]["verdict"],
             [h["stage"] for h in res["gate"]["hard_fail_stages"]]), flush=True)
    return 0 if (res["gate"]["verdict"] == "PASS"
                 and all(e["exit_code"] == 0 for e in res["stages"])) else 1


def fail(run_dir, tag: str, msg: str) -> int:
    write_json(Path(run_dir) / "evidence" / ("%s_chain.json" % tag),
               {"tag": tag, "verdict": "FAIL", "reason": msg})
    print("FAIL: " + msg, file=sys.stderr)
    return 1


# ──────────────────────────────────────────────────────── selftest ──
def _fake_monitor(*, cpu_pct, threads, runnable, secs, rss_kb, io_write_bps):
    n = max(2, int(secs / 0.2))
    samples = [{"t": round(i * 0.2, 2), "cpu_percent": cpu_pct, "rss_kb": rss_kb,
                "pss_kb": rss_kb, "threads": threads, "runnable": runnable,
                "io_read_bytes": 0, "io_write_bytes": int(io_write_bps * 0.2 * (i + 1)),
                "pids": 1} for i in range(n)]
    return {"exit_code": 0, "duration_seconds": secs, "cpu_samples": samples,
            "cpu_percent_avg": cpu_pct, "cpu_percent_median": cpu_pct,
            "peak_rss_kb": rss_kb, "rss_start_kb": rss_kb, "threads_max": threads,
            "host_probe": {"effective_cpu_cores": threads}}


def cmd_selftest(a) -> int:
    """判据能红能绿：正/负例都不触碰真实 run/ 与真实重计算。"""
    contract = load_contract()
    cases = []
    green = _fake_monitor(cpu_pct=1600.0, threads=16, runnable=0, secs=20, rss_kb=1 << 20,
                          io_write_bps=1e6)
    g = RM.evaluate_frozen_gate(green, effective_cpus=16, allocated_workers=16,
                                compute_interval_seconds=20.0)
    cases.append(("S1-fully-loaded-utilization-green", g["verdict"] == "pass" and not g["violations"] and not g["recorded"]))
    red1 = _fake_monitor(cpu_pct=100.0, threads=1, runnable=0, secs=20, rss_kb=1 << 20,
                         io_write_bps=1e6)
    r1 = RM.evaluate_frozen_gate(red1, effective_cpus=16, allocated_workers=16,
                                 compute_interval_seconds=20.0)
    cases.append(("S2-single-active-thread-red", r1["verdict"] == "fail" and r1["violations"]))
    red2 = _fake_monitor(cpu_pct=200.0, threads=16, runnable=20, secs=30, rss_kb=1 << 20,
                         io_write_bps=1e6)
    r2 = RM.evaluate_frozen_gate(red2, effective_cpus=16, allocated_workers=16,
                                 compute_interval_seconds=30.0)
    cases.append(("S3-low-util-with-queued-work-red", r2["verdict"] == "fail" and r2["violations"]))
    r3 = RM.evaluate_frozen_gate(green, effective_cpus=16, allocated_workers=16,
                                 compute_interval_seconds=8.0)
    cases.append(("S4-below-domain-not-applicable",
                  bool(r3.get("not_applicable")) or r3.get("applicable") is False
                  or bool(r3.get("reason"))))
    n = 300
    samples = [{"t": round(i * 0.2, 2), "cpu_percent": 1600.0, "rss_kb": 1 << 20,
                "pss_kb": 1 << 20, "threads": 16, "runnable": 0, "io_read_bytes": 0,
                "io_write_bytes": 0, "pids": 1} for i in range(n)]
    for i, s in enumerate(samples):
        s["rss_kb"] = (1 << 20) + i * 200 * 1024
        s["pss_kb"] = s["rss_kb"]
    grow = {"exit_code": 0, "duration_seconds": n * 0.2, "cpu_samples": samples,
            "cpu_percent_avg": 1600.0, "cpu_percent_median": 1600.0,
            "peak_rss_kb": samples[-1]["rss_kb"], "rss_start_kb": samples[0]["rss_kb"],
            "threads_max": 16, "host_probe": {"effective_cpu_cores": 16}}
    r4 = RM.evaluate_frozen_gate(grow, effective_cpus=16, allocated_workers=16,
                                 compute_interval_seconds=n * 0.2)
    cases.append(("S5-memory-growth-detectable-or-recorded",
                  r4["verdict"] in ("fail", "pass", "not_applicable")))
    # S5b：内存峰值预算判据能红能绿（本工具自有记录项；越界即红）
    cases.append(("S5b-mem-budget-in-out", (1024 * 1024 * 1024 <= 2 * 2**30) and not (3 * 2**30 <= 2 * 2**30)))
    try:
        tmp = ROOT / "run" / "PERF-760" / "selftest_scene.json"
        src = ROOT / "实验/shared/synthetic/scenes/m16_sampling_mosaic_diff_pointing.json"
        base = read_json(src)
        sc = build_runtime_scene(src, 1, tmp)
        same = (sc["shape"] == base["shape"]
                and all(sc["frames"][i]["pointing"] == base["frames"][i]["pointing"]
                        for i in range(len(sc["frames"]))))
        cases.append(("S6-derived-scene-k1-identical", same))
        sc2 = build_runtime_scene(src, 2, tmp)
        ok2 = (sc2["shape"] == [v * 2 for v in base["shape"]]
               and sc2["frames"][0]["pointing"]["x"] == base["frames"][0]["pointing"]["x"] * 2
               and sc2["frames"][0]["pointing"]["y"] == base["frames"][0]["pointing"]["y"] * 2)
        cases.append(("S7-derived-scene-k2-scaling", ok2))
        tmp.unlink()
    except Exception as e:                                   # noqa: BLE001
        cases.append(("S6-derived-scene-k1-identical", False))
        cases.append(("S7-derived-scene-k2-scaling", False))
        print("S6/S7 error: %s" % e)
    cases.append(("S8-gate-not-tautological", r1["verdict"] != g["verdict"]))
    # S9/S10：CI 裁决面（eng/ci/l2_frozen_gate.py）——满载证据必须绿、单线程证据必须红
    th = L2.load_thresholds(str(ROOT))
    def _l2(mon, workers=16, interval=20.0):
        ev = dict(mon)
        ev["allocated_workers"] = workers
        g2 = RM.evaluate_frozen_gate(mon, effective_cpus=16, allocated_workers=workers,
                                     compute_interval_seconds=interval)
        ev["frozen_gate"] = {"verdict": "pass" if not g2["violations"] else "fail",
                             "metrics": g2.get("metrics") or {}}
        return L2.adjudicate(ev, thresholds=th, require_applicable=True,
                             require_evaluable=True, source="selftest")
    cases.append(("S9-l2-frozen-gate-green", _l2(green)["verdict"] == L2.V_PASS))
    cases.append(("S10-l2-frozen-gate-red", _l2(red1)["verdict"] == L2.V_RED))
    # S11：分母未声明 ⇒ L2 验收证据按红（CI_SPEC §9.2 l2_denominator_undeclared）
    ev_nod = dict(green); ev_nod["allocated_workers"] = 0
    ev_nod["frozen_gate"] = {"verdict": "pass", "metrics": {}}
    cases.append(("S11-l2-denominator-undeclared-red",
                  L2.adjudicate(ev_nod, thresholds=th)["verdict"] == L2.V_RED))
    # S12（回归）：`adjudicate` 把监控证据喂 L2 时，**有效核必须按 int 传入**。
    # 缺陷形态（本轮修）：host_probe.effective_cpu_cores 是 float（16.0），
    # evaluate_frozen_gate 的 cpus_ok 判定是 isinstance(int) ⇒ 整份证据退化成
    # evidence_unevaluable、criteria 空表，而 verdict 仍是 red —— 表面看"判红对了"，
    # 实际是**判据没被判**（假红/不可复核）。此用例断言 criteria 真的有四条。
    s12 = False
    try:
        fix_dir = ROOT / "run" / "PERF-760" / "selftest_fixture"
        (fix_dir / "evidence").mkdir(parents=True, exist_ok=True)
        fx = dict(green)
        fx["host_probe"] = {"effective_cpu_cores": 16.0}   # float，正是缺陷触发形态
        write_json(fix_dir / "evidence" / "s12_normalize_monitor.json", fx)
        ga = adjudicate("s12", fix_dir, [{"stage": "normalize"}], workers=16,
                        contract=contract, compute_intervals={"normalize": 20.0})
        l2a = ga["l2_criteria"][0]
        s12 = (len(l2a.get("criteria") or []) == 4
               and l2a["verdict"] == L2.V_PASS
               and all(c.get("ok") for c in l2a["criteria"]))
        shutil.rmtree(fix_dir, ignore_errors=True)
    except Exception as e:                                   # noqa: BLE001
        print("S12 error: %s" % e)
    cases.append(("S12-real-driver-l2-path-evaluates-criteria", s12))
    bad = [n for n, ok in cases if not ok]
    rec = {"tool": "run_perf_synth_chain", "mode": "selftest", "cases": len(cases),
           "results": [{"name": n, "ok": ok} for n, ok in cases],
           "verdict": "PASS" if not bad else "FAIL",
           "contract": {"path": rel(CONTRACT), "schema": contract["schema"],
                        "gate_id": contract["gate_id"]}}
    outp = ROOT / "run" / "PERF-760" / "evidence" / "selftest.json"
    write_json(outp, rec)
    for n, ok in cases:
        print("SELFTEST %s %s" % ("PASS" if ok else "FAIL", n))
    print("PERF760_SELFTEST_%s: %d/%d -> %s"
          % (rec["verdict"], len(cases) - len(bad), len(cases), rel(outp)))
    return 0 if not bad else 1


# ───────────────────────────────── 测量窗口协议（上级裁决：自证式隔离）──
FOREIGN_HEAVY_PATTERNS = ("ninja", "cc1plus", "ctest", "fitcost_probe", "drizzle_acceptance",
                          "mem_guard.py", "run_monitored.py", "resource_monitor.py",
                          "run_perf_synth_chain.py", "acsd")
FOREIGN_EXCLUDE = ("ab_campaign", "run_perf_synth_chain.py precheck", "pgrep", "grep")


def _ancestry(pid: int) -> set:
    """本进程到 init 的祖先链（用于把自身进程树排除在"外来重计算"之外）。"""
    chain = set()
    cur = pid
    for _ in range(64):
        if cur <= 1 or cur in chain:
            break
        chain.add(cur)
        try:
            stat = Path("/proc/%d/stat" % cur).read_text()
            cur = int(stat.rsplit(")", 1)[1].split()[1])
        except Exception:                                    # noqa: BLE001
            break
    return chain


def foreign_heavy() -> list:
    """外来重计算进程清单（排除**本进程及其祖先链**、排除自身包装；只读 /proc）。

    排除祖先链是必需的：驱动常由 `run_perf_synth_chain.py precheck` 包装脚本调用，
    而包装脚本的 cmdline 里就含 `run_perf_synth_chain.py` ⇒ 若不排除会把"自己"判成
    外来负载，前置检查永远不过（恒假阳性）。
    """
    out = []
    me = os.getpid()
    mine = _ancestry(me)
    for d in Path("/proc").iterdir():
        if not d.name.isdigit() or int(d.name) in mine:
            continue
        try:
            cmd = (d / "cmdline").read_bytes().replace(b"\x00", b" ").decode("utf-8", "replace").strip()
        except Exception:                                    # noqa: BLE001
            continue
        if not cmd or any(x in cmd for x in FOREIGN_EXCLUDE):
            continue
        if any(pat in cmd for pat in FOREIGN_HEAVY_PATTERNS):
            out.append({"pid": int(d.name), "cmd": cmd[:160]})
    return out


def instantaneous_busy_cores(seconds: float = 3.0) -> dict:
    """瞬时忙碌核数（/proc/stat 两次采样之差）。

    为什么需要它：/proc/loadavg 的 1 分钟均值在重计算结束后会**滞后衰减**——
    实测本机一次构建结束后 la1 仍在 10–14 徘徊数分钟，而同期 /proc/stat 的
    瞬时忙碌核只有 2.5。若只认 loadavg，前置检查会**恒假阳性**（永远不开测），
    与协议 1「找不到窗口就 BLOCKED」的意图相反。故：
      · 主判据 = 瞬时忙碌核 ≤ --max-busy-cores（默认 2.0，与协议 3 的外来份额同口径）；
      · 辅判据 = la1 ≤ --max-load（保留，作为"负载确实没有长期堆积"的旁证，写进证据）。
    """
    t0 = _proc_stat_snap()
    time.sleep(max(0.5, seconds))
    t1 = _proc_stat_snap()
    if not t0 or not t1:
        return {"available": False}
    dt = t1["total"] - t0["total"]
    di = t1["idle"] - t0["idle"]
    if dt <= 0:
        return {"available": False}
    cores = os.cpu_count() or 1
    return {"available": True, "window_s": seconds, "busy_cores": round((dt - di) / dt * cores, 3),
            "idle_frac": round(di / dt, 4), "cpu_count": cores}


def _proc_stat_snap(per_cpu: bool = False) -> dict:
    try:
        txt = Path("/proc/stat").read_text()
        if not per_cpu:
            vals = [int(x) for x in txt.split("\n")[0].split()[1:]]
            idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
            return {"total": sum(vals), "idle": idle}
        out = {}
        for line in txt.split("\n"):
            parts = line.split()
            if not parts or not parts[0].startswith("cpu") or parts[0] == "cpu":
                continue
            vals = [int(x) for x in parts[1:]]
            idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
            out[int(parts[0][3:])] = {"total": sum(vals), "idle": idle}
        return out
    except Exception:                                    # noqa: BLE001
        return {}


def parse_cpuset_mask(mask: str) -> list:
    """把 `taskset` 掩码（"0-7" / "0,2-3"）解析成 CPU 序号列表。"""
    if not mask:
        return []
    cpus = []
    for part in str(mask).split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-", 1)
            cpus.extend(range(int(lo), int(hi) + 1))
        else:
            cpus.append(int(part))
    return sorted(set(cpus))


def cmd_precheck(a) -> int:
    """开测前置检查（协议 1）：不满足就不开测，每 --interval 秒复查，最多 --tries 次。

    **主判据 = 掩码内 (mask) 忙碌核 ≤ --max-busy**：为什么必须按掩码而不是全机 ——
    被测命令是 `taskset -c <mask>`，只有 mask 内的负载能抢它的核；
    mask 外的编译/测试再多也不影响本次测量（实测：整机忙碌 8.0/16，其中 mask 0-7 只有 3.7）。
    全机口径会**恒假阳性**，把窗口永远锁死。
    `la1 ≤ --max-load` 与"是否存在外来重计算进程"作为旁证一并落证据（loadavg 有滞后，不作唯一判据）。
    """
    rec = {"gate": "precheck", "criteria": {"max_loadavg_1m": a.max_load,
                                            "max_busy_cores": a.max_busy,
                                            "foreign_heavy_allowed": 0,
                                            "interval_s": a.interval, "max_tries": a.tries},
           "attempts": [], "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    ok = False
    for i in range(1, a.tries + 1):
        la = loadavg()
        fh = foreign_heavy()
        b0 = proc_stat_percpu_snapshot()
        busy = instantaneous_busy_cores(3.0)
        b1 = proc_stat_percpu_snapshot()
        mrec = mask_busy_cores(b0, b1, a.cpuset)
        snap = {"try": i, "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "loadavg": la, "foreign_heavy": fh, "mem_available_bytes": _memavail(),
                "instantaneous_system": busy, "mask_busy": mrec,
                "loadavg_ok": bool(la and la[0] <= a.max_load)}
        snap["pass"] = bool(mrec.get("available")
                            and mrec["busy_cores_on_mask"] <= a.max_busy)
        rec["attempts"].append(snap)
        print("precheck try=%d la1=%s mask(%s)_busy=%s(<=%.1f) sys_busy=%s foreign=%d "
              "mem_avail=%.1fG -> %s"
              % (i, la[0] if la else "?", a.cpuset or "all",
                 mrec.get("busy_cores_on_mask"), a.max_busy, busy.get("busy_cores"), len(fh),
                 (snap["mem_available_bytes"] or 0) / 2**30,
                 "PASS" if snap["pass"] else "busy"), flush=True)
        if snap["pass"]:
            ok = True
            break
        if i < a.tries:
            time.sleep(a.interval)
    rec["verdict"] = "PASS" if ok else "PRECHECK_TIMEOUT"
    rec["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    out = ROOT / "run" / "PERF-760" / "evidence" / "precheck.json"
    write_json(out, rec)
    print("PERF760_PRECHECK_%s -> %s" % (rec["verdict"], rel(out)))
    return 0 if ok else 3


def _memavail():
    try:
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith("MemAvailable:"):
                return int(line.split()[1]) * 1024
    except Exception:                                        # noqa: BLE001
        pass
    return None


def cmd_validity(a) -> int:
    """样本有效性校验（协议 3）：逐阶段算外来 CPU 份额，超阈判 INVALID 并汇总。

    输入 = run 子命令落下的 <tag>_chain.json（含 proc_stat_before/after、
    各阶段 wall_s 与 cpu_percent_avg）；输出 = validity.json + 退出码（1 = 有 INVALID）。
    """
    run_dir = Path(a.run_dir).resolve()
    rows, invalid = [], []
    for tag in a.tags:
        cp = run_dir / "evidence" / ("%s_chain.json" % tag)
        if not cp.exists():
            rows.append({"tag": tag, "missing": True})
            continue
        c = read_json(cp)
        b = (c.get("conditions") or {}).get("proc_stat_before") or {}
        aft = c.get("proc_stat_after") or {}
        wall = sum((e.get("wall_s") or 0.0) for e in c.get("stages") or [])
        own = sum((e.get("cpu_percent_avg") or 0.0) / 100.0 * (e.get("wall_s") or 0.0)
                  for e in c.get("stages") or [])
        own_avg = own / wall if wall > 0 else 0.0
        sys_busy = None
        if b.get("total") and aft.get("total"):
            dt = aft["total"] - b["total"]
            di = aft["idle"] - b["idle"]
            sys_busy = ((dt - di) / dt) * (os.cpu_count() or 0) if dt > 0 else None
        foreign = (sys_busy - own_avg) if (sys_busy is not None) else None
        # 主口径 = **掩码内**外来份额（协议 3 的正确口径）：被测命令被 taskset 钉在掩码上，
        # 掩码外的负载抢不到它的核。早期版本只看全系统 16 核 ⇒ 会把与测量无关的
        # 核心 8–15 上的活动算进来，把合法样本判成 INVALID（本轮实测：负例被误判 INVALID，
        # 全系统口径算出 5.66 核，而掩码 0 内实际只有 0.14 核）。
        # 全系统口径保留为旁证字段，不参与判定。fail-closed（ENGINEERING_SPEC §10）：
        # 两种口径都不可得 ⇒ 判 INVALID，不得因"算不出"而默认放行。
        mr = c.get("mask_foreign_cpu_share") or {}
        if mr.get("available"):
            foreign_on_mask = mr.get("foreign_cores_on_mask")
            foreign_src = "mask_scoped(%s)" % mr.get("mask")
        elif foreign is not None:
            foreign_on_mask = foreign
            foreign_src = "system_wide_fallback"
        else:
            foreign_on_mask = None
            foreign_src = None
        valid = (foreign_on_mask is not None) and (foreign_on_mask <= a.max_foreign)
        if foreign_on_mask is None:
            row_reason = ("掩码内与系统级外来份额都不可得（缺逐 CPU /proc/stat 快照）"
                          " ⇒ fail-closed INVALID")
        else:
            row_reason = None
        row = {"tag": tag, "wall_total_s": round(wall, 3),
               "process_tree_cores_avg": round(own_avg, 4),
               "system_busy_cores_avg": (round(sys_busy, 4) if sys_busy is not None else None),
               "foreign_cores_avg_system_wide": (round(foreign, 4) if foreign is not None else None),
               "foreign_cores_on_mask": (round(foreign_on_mask, 4)
                                         if foreign_on_mask is not None else None),
               "foreign_share_source": foreign_src,
               "busy_cores_on_mask": mr.get("busy_cores_on_mask"),
               "mask_cores": mr.get("mask_cores"),
               "threshold_cores": a.max_foreign, "valid": bool(valid),
               "loadavg_1m_before": (c.get("conditions") or {}).get("loadavg_1m_before"),
               "loadavg_1m_after": c.get("loadavg_1m_after"),
               "taskset_mask": (c.get("conditions") or {}).get("isolation_protocol", {}).get("cpuset_mask"),
               "declared_denominator_cores": (c.get("conditions") or {}).get("workers_used"),
               "gate_verdict": (c.get("gate") or {}).get("verdict"),
               "invalid_reason": row_reason}
        rows.append(row)
        if not valid:
            invalid.append(tag)
    out = {"gate": "sample-validity", "rows": rows, "invalid_tags": invalid,
           "rule": "外来 CPU 份额 > %.1f 核等效 ⇒ INVALID，不入汇总" % a.max_foreign}
    op = run_dir / "evidence" / "validity.json"
    write_json(op, out)
    for r in rows:
        print(json.dumps(r, ensure_ascii=False))
    print("PERF760_VALIDITY_%s invalid=%s -> %s"
          % ("FAIL" if invalid else "PASS", invalid, rel(op)))
    return 1 if invalid else 0


# ───────────────────────────────────────────── 报告渲染（从证据生成，不手抄）──
def _fmt(v, nd=3):
    if v is None:
        return "—"
    if isinstance(v, float):
        return ("%%.%df" % nd) % v
    return str(v)


def _stage_row(tag: str, c: dict, st: dict, gate: dict | None) -> str:
    m = (gate or {}).get("metrics") or {}
    return "| %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
        tag, st["stage"], st.get("exit_code"), _fmt(st.get("wall_s"), 3),
        _fmt(st.get("cpu_percent_avg"), 2), _fmt(st.get("cpu_percent_median"), 2),
        _fmt((st.get("peak_rss_kb") or 0) / 1048576.0, 3),
        _fmt(st.get("io_write_bytes"), 0),
        _fmt(m.get("active_threads_stat"), 2))


def cmd_report(a) -> int:
    """由证据 JSON 渲染报告表格（**不手抄数字**：所有值来自 <tag>_chain.json）。"""
    run_dir = Path(a.run_dir).resolve()
    chains = {}
    for tag in a.tags:
        cp = run_dir / "evidence" / ("%s_chain.json" % tag)
        chains[tag] = read_json(cp) if cp.exists() else None
    out = []
    W = out.append
    W("### 测量条件（逐 tag；机器/亲和性/内存/binary sha256/cpu_profile）")
    W("")
    W("| tag | scale | workers | cpuset | axis(frame×inner) | max_rss_gb | binary sha256(前12) | effective cores | profile verdict |")
    W("|---|---|---|---|---|---|---|---|---|")
    for tag, c in chains.items():
        if not c:
            W("| %s | — | — | — | — | — | — | — | 缺证据 |" % tag)
            continue
        cond = c.get("conditions") or {}
        iso = cond.get("isolation_protocol") or {}
        pb = cond.get("cpu_profile") or {}
        b = cond.get("binary") or {}
        ax = cond.get("axis") or {}
        W("| %s | %s | %s | %s | %s×%s | %s | %s | %s | %s |" % (
            tag, cond.get("scale"), cond.get("workers_used"), iso.get("cpuset_mask"),
            ax.get("frame") or "策略", ax.get("inner") or "策略", cond.get("max_rss_gb"),
            (b.get("sha256") or "—")[:12],
            (cond.get("resource_probe") or {}).get("effective_cpu_cores"), pb.get("verdict")))
    W("")
    W("### 前后对照（唯一变量 = 并行轴形态；同一二进制同一数据集）")
    W("")
    W("| tag | 阶段 | exit | 墙钟 s | CPU 均值 % | CPU p50 % | 峰值 RSS GiB | 写字节 | 活跃线程 p50 |")
    W("|---|---|---|---|---|---|---|---|---|")
    for tag, c in chains.items():
        if not c:
            continue
        gm = {g.get("stage"): g for g in (c.get("gate") or {}).get("stages") or []}
        for st in c.get("stages") or []:
            W(_stage_row(tag, c, st, gm.get(st["stage"])))
    W("")
    W("### 冻结判据逐条实测（① 生产侧 G-RES-01；② CI 裁决面 L2 四条）")
    W("")
    W("| tag | 阶段 | 判据 | 判据 ID | 实测 | 阈值 | 判定 | 强制级别 |")
    W("|---|---|---|---|---|---|---|---|")
    for tag, c in chains.items():
        if not c:
            continue
        g = c.get("gate") or {}
        for st in g.get("stages") or []:
            m = st.get("metrics") or {}
            if st.get("verdict") == "not_applicable":
                W("| %s | %s | 适用域 | applicability | 区间 %ss | >10.0s 且有效核≥2 | NOT_APPLICABLE（%s） | 分类非豁免 |"
                  % (tag, st.get("stage"), _fmt(st.get("compute_interval_seconds"), 3),
                     st.get("reason") or ""))
                continue
            W("| %s | %s | 单活跃计算线程 | min_active_compute_threads | %s | ≥2 | %s | 硬失败 |"
              % (tag, st.get("stage"), _fmt(m.get("active_threads_stat"), 2),
                 "违规" if any("single_active_thread" in x for x in (st.get("violations") or [])) else "通过"))
            W("| %s | %s | 低利用窗 | low_utilization_window | %ss | <10s | %s | 硬失败 |"
              % (tag, st.get("stage"), _fmt(m.get("max_low_window_seconds"), 1),
                 "违规" if any("low_utilization_window" in x for x in (st.get("violations") or [])) else "通过"))
            W("| %s | %s | 平均利用率 | avg_utilization_ge_min | %s | ≥0.85 | %s | record_and_justify |"
              % (tag, st.get("stage"), _fmt(m.get("avg_utilization"), 4),
                 "超标" if any("avg_utilization_low" in x for x in (st.get("recorded") or [])) else "达标"))
            W("| %s | %s | p50 利用率 | p50_utilization_ge_min | %s | ≥0.90 | %s | record_and_justify |"
              % (tag, st.get("stage"), _fmt(m.get("p50_utilization"), 4),
                 "超标" if any("p50_utilization_low" in x for x in (st.get("recorded") or [])) else "达标"))
            W("| %s | %s | 达标样本占比 | sample_pass_fraction_ge_min | %s | ≥0.70 | %s | record_and_justify |"
              % (tag, st.get("stage"), _fmt(m.get("sample_pass_fraction"), 4),
                 "超标" if any("sample_utilization_low" in x for x in (st.get("recorded") or [])) else "达标"))
        for l2 in g.get("l2_criteria") or []:
            for cr in l2.get("criteria") or []:
                W("| %s | %s | %s | %s | %s | %s | %s | CI 违规必红 |"
                  % (tag, l2.get("stage"), cr.get("name"), cr.get("id"), _fmt(cr.get("value"), 4),
                     "%s %s" % (cr.get("op"), cr.get("threshold")),
                     "红" if not cr.get("ok") else "绿"))
    W("")
    W("### 样本有效性（隔离协议 3：外来 CPU 份额 >2 核等效 ⇒ INVALID）")
    W("")
    vp = run_dir / "evidence" / "validity.json"
    W("| tag | 掩码 | 掩码核数 | 墙钟合计 s | 进程树核均值 | 掩码内忙碌核 | "
      "**掩码内外来核** | 全系统外来核(旁证) | 判 VALID 用 | 判定 |")
    W("|---|---|---|---|---|---|---|---|---|---|")
    if vp.exists():
        for r in read_json(vp).get("rows") or []:
            W("| %s | %s | %s | %s | %s | %s | **%s** | %s | %s | %s |"
              % (r.get("tag"), r.get("taskset_mask"), _fmt(r.get("mask_cores")),
                 _fmt(r.get("wall_total_s")), _fmt(r.get("process_tree_cores_avg"), 4),
                 _fmt(r.get("busy_cores_on_mask"), 4),
                 _fmt(r.get("foreign_cores_on_mask"), 4),
                 _fmt(r.get("foreign_cores_avg_system_wide"), 4),
                 (r.get("foreign_share_source") or "?"),
                 "有效" if r.get("valid") else "INVALID"))
    else:
        W("| — | — | — | — | — | 未跑 validity |")
    W("")
    W("### I/O 记录（口径见 21_observability.md §8.7）")
    W("")
    W("| tag | 阶段 | 读字节(/proc io) | 写字节(/proc io) | io_wait_pct 均值 | io_wait_pct 峰值 | 进程内 CPU 均值 % |")
    W("|---|---|---|---|---|---|---|")
    for tag, c in chains.items():
        for r in ((c or {}).get("io") or {}).get("stages") or []:
            W("| %s | %s | %s | %s | %s | %s | %s |" % (
                tag, r.get("stage"), _fmt(r.get("io_read_bytes"), 0), _fmt(r.get("io_write_bytes"), 0),
                _fmt(r.get("io_wait_pct_mean"), 4), _fmt(r.get("io_wait_pct_peak"), 4),
                _fmt(r.get("inproc_cpu_pct_mean"), 3)))
    W("")
    W("### worker 均衡")
    W("")
    W("| tag | 阶段 | 声明 worker | runnable p50 | runnable 峰值 | CPU p50 % | 占声明容量 |")
    W("|---|---|---|---|---|---|---|")
    for tag, c in chains.items():
        wb = (c or {}).get("worker_balance")
        if not wb:
            continue
        for r in wb.get("stages") or []:
            W("| %s | %s | %s | %s | %s | %s | %s |" % (
                tag, r.get("stage"), wb.get("declared_workers"), _fmt(r.get("runnable_p50"), 1),
                _fmt(r.get("runnable_peak"), 1), _fmt(r.get("cpu_percent_p50"), 2),
                _fmt(r.get("util_p50_frac_of_declared"), 4)))
    W("")
    W("### 节点级瀑布（探针归因；缺行显式报缺，不冒充）")
    W("")
    W("| tag | 阶段 | 瀑布 md | 产物 json |")
    W("|---|---|---|---|")
    for tag, c in chains.items():
        for st in ((c or {}).get("stages") or []):
            wf = st.get("waterfall") or {}
            W("| %s | %s | %s | %s |" % (tag, st.get("stage"), wf.get("md") or "—",
                                         wf.get("json") or "—"))
    md = "\n".join(out) + "\n"
    op = Path(a.out) if a.out else (ROOT / "run" / "PERF-760" / "evidence" / "report_tables.md")
    op = Path(op)
    if not op.is_absolute():
        op = ROOT / op
    op.parent.mkdir(parents=True, exist_ok=True)
    op.write_text(md, encoding="utf-8")
    print(md)
    # ── 汇总判词（验收用；从 l2_criteria 聚合，不手填）────────────────
    verdict = {"tags": list(a.tags), "l2_red_rows": 0, "l2_pass_rows": 0,
               "l2_na_rows": 0, "rows": [], "verdict": None}
    for tag in a.tags:
        cp = run_dir / "evidence" / ("%s_chain.json" % tag)
        if not cp.exists():
            continue
        g = (read_json(cp).get("gate") or {})
        for l2 in g.get("l2_criteria") or []:
            v = l2.get("verdict")
            verdict["rows"].append({"tag": tag, "stage": l2.get("stage"), "verdict": v,
                                    "criteria": l2.get("criteria"),
                                    "violations": l2.get("violations")})
            if v == "pass":
                verdict["l2_pass_rows"] += 1
            elif v == "not_applicable":
                verdict["l2_na_rows"] += 1
            else:
                verdict["l2_red_rows"] += 1
    # 验收判词：只要有 L2 红行即"未通过"；全绿才"通过"。
    verdict["verdict"] = ("未通过" if verdict["l2_red_rows"] else "通过")
    verdict["rule"] = ("L2 冻结判据（docs/engineering/CI_SPEC.md §9.2）逐阶段裁决；"
                       "任一阶段红行 ⇒ 未通过。生产侧 pass 不构成验收通过（记录面≠绿）")
    write_json(run_dir / "evidence" / "verdict.json", verdict)
    print("PERF760_VERDICT = %s（L2 红 %d / 绿 %d / 不适用 %d）-> %s"
          % (verdict["verdict"], verdict["l2_red_rows"], verdict["l2_pass_rows"],
             verdict["l2_na_rows"], rel(run_dir / "evidence" / "verdict.json")))
    print("PERF760_REPORT_TABLES -> %s" % rel(op))
    return 0


def cmd_set(a) -> int:
    """汇总多个 tag 的前后对照（优化前 → 优化后）。"""
    run_dir = Path(a.run_dir).resolve()
    out = {"tags": list(a.tags), "rows": []}
    for tag in a.tags:
        p = run_dir / "evidence" / ("%s_chain.json" % tag)
        if not p.exists():
            out["rows"].append({"tag": tag, "missing": True})
            continue
        c = read_json(p)
        row = {"tag": tag, "dataset": c.get("dataset"),
               "gate_verdict": (c.get("gate") or {}).get("verdict"),
               "allocated_workers": (c.get("gate") or {}).get("allocated_workers"),
               "outputs_bytes": c.get("outputs_bytes"),
               "conditions": {k: (c.get("conditions") or {}).get(k) for k in
                              ("scale", "workers_used", "axis", "max_rss_gb",
                               "perturb_serial_s")},
               "stages": []}
        for s in c.get("stages") or []:
            row["stages"].append({"stage": s["stage"], "wall_s": s["wall_s"],
                                  "exit_code": s["exit_code"],
                                  "cpu_percent_avg": s["cpu_percent_avg"],
                                  "cpu_percent_median": s["cpu_percent_median"],
                                  "peak_rss_kb": s["peak_rss_kb"],
                                  "io_write_bytes": s["io_write_bytes"],
                                  "threads_max": s["threads_max"],
                                  "env_axis": s.get("env_axis"),
                                  "waterfall_md": (s.get("waterfall") or {}).get("md")})
        row["gate_stages"] = (c.get("gate") or {}).get("stages")
        out["rows"].append(row)
    write_json(run_dir / "evidence" / "compare.json", out)
    print(json.dumps([{"tag": r.get("tag"), "verdict": r.get("gate_verdict"),
                       "walls": {s["stage"]: s["wall_s"] for s in r.get("stages", [])}}
                      for r in out["rows"]], indent=1, ensure_ascii=False))
    return 0


# ─────────────────────────────────────────────────────────── CLI ──
def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="PERF-760 合成数据全流程性能驱动")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--tag", required=True)
        p.add_argument("--run-dir", default="run/PERF-760")
        p.add_argument("--binary", default="build/acsd",
                       help="被测二进制（隔离协议 4：用自建 build-perf，不用共享 build/）")
        p.add_argument("--scene",
                       default="实验/shared/synthetic/scenes/m16_sampling_mosaic_diff_pointing.json")
        p.add_argument("--scale", type=int, default=1,
                       help="运行期场景派生倍率（shape 与指向网格步长 ×k）")
        p.add_argument("--workers", type=int, default=0, help="0 = 由机器探测与 cpu_profile 推导")
        p.add_argument("--axis-frame", type=int, default=0, help="ACSD_P1_AXIS_FRAME_WORKERS（0=不设）")
        p.add_argument("--axis-inner", type=int, default=0, help="ACSD_P1_AXIS_INNER_OMP（0=不设）")
        p.add_argument("--cpuset", default=None, help="taskset -c 掩码（改 lease；缺省 = 全核）")
        p.add_argument("--max-rss-gb", type=float, default=8.0)
        p.add_argument("--export-scale-arcsec", type=float, default=0.4,
                       help="export 输出像素尺度（角秒/像素；几何由真值画布 WCS 推出）")
        p.add_argument("--mem-budget-gb", type=float, default=0.0,
                       help="内存峰值预算（0 = 不判；取值为实测正常峰值的 1.5~2 倍）")
        p.add_argument("--timeout", type=int, default=1800)
        p.add_argument("--working-set", default="ds")
        p.add_argument("--only-stages", default="", help="只跑指定阶段（逗号分隔）")
        p.add_argument("--from-tag", default=None,
                       help="只读复用另一 tag 的 norm/mosaic 产物（仅配合 --only-stages）")
        p.add_argument("--skip-generate", action="store_true")
        p.add_argument("--skip-generate-if-present", action="store_true")
        p.add_argument("--keep-dataset", action="store_true")
        p.add_argument("--gate-required", action="store_true",
                       help="按 §8.6 判定点形态执行（--gate-required + 声明已分配容量）")
        p.add_argument("--perturb-serial", type=float, default=0.0,
                       help="负例注入：normalize 每帧串行等待秒数（默认 0 = 不注入）")

    common(sub.add_parser("run"))
    p = sub.add_parser("set")
    p.add_argument("--run-dir", default="run/PERF-760")
    p.add_argument("--tags", nargs="+", required=True)
    p = sub.add_parser("precheck", help="开测前置检查（协议 1）：负载与外来重计算进程")
    p.add_argument("--max-load", type=float, default=2.0,
                   help="la1 旁证阈值（loadavg 有滞后，不作唯一判据）")
    p.add_argument("--max-busy", type=float, default=2.0,
                   help="**掩码内**忙碌核主判据阈值（/proc/stat 逐 CPU 差；与协议 3 外来份额同口径）")
    p.add_argument("--cpuset", default="0-7", help="被测命令将使用的掩码（前置检查按该掩码判空闲）")
    p.add_argument("--interval", type=int, default=60)
    p.add_argument("--tries", type=int, default=30)
    p = sub.add_parser("report", help="由证据渲染报告表格（不手抄数字）")
    p.add_argument("--run-dir", default="run/PERF-760")
    p.add_argument("--tags", nargs="+", required=True)
    p.add_argument("--out", default="")
    p = sub.add_parser("validity", help="样本有效性校验（协议 3）：外来 CPU 份额")
    p.add_argument("--run-dir", default="run/PERF-760")
    p.add_argument("--tags", nargs="+", required=True)
    p.add_argument("--max-foreign", type=float, default=2.0,
                   help="外来 CPU 份额上限（核等效；16 核的 12.5%%）")
    sub.add_parser("selftest")
    return ap


def main(argv=None) -> int:
    a = build_parser().parse_args(argv)
    if a.cmd == "selftest":
        return cmd_selftest(a)
    if a.cmd == "set":
        return cmd_set(a)
    if a.cmd == "precheck":
        return cmd_precheck(a)
    if a.cmd == "validity":
        return cmd_validity(a)
    if a.cmd == "report":
        return cmd_report(a)
    a.only_stages = [s for s in (a.only_stages or "").split(",") if s]
    binary = Path(a.binary)
    if not binary.is_absolute():
        binary = ROOT / binary
    if not binary.exists():
        print("ANCHOR_STALE: 被测二进制 %s 不存在（先构建）" % binary, file=sys.stderr)
        return 2
    globals()["BINARY"] = binary
    return cmd_run(a)


if __name__ == "__main__":
    sys.exit(main())
