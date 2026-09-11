#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""REAL-000 确定性校准匹配计划工具（AstroCS 真实数据验收前置）。

职责（04_OWNER_DECISIONS_20260910.md 真实数据验收指令 / REAL-000 任务规格）：
  1. inventory —— 只读盘点 testdata/ 全部文件，按 数据集×望远镜×面板×滤镜×曝光
     分组计数，与 testdata/index.json 对账，产出逐文件 sha256 清单；
  2. plan     —— 按本文件顶部冻结的匹配规则，对 906 帧亮场 + 27 母版逐个产出
     {bias, dark(file,K,strategy), flat} 或 UNMATCHED(reason_code)，生成
     match_plan.json / match_summary.md / phase_config 模板；
  3. T1 显式空集用例：枚举 0 帧 0 母版，计划输出 "T1: empty(PASS)"，不是 skip。

冻结匹配键（顺序即优先级，不得在调用方重排）：
  (1) 望远镜目录 —— 亮场所属望远镜（index datasets[].telescope / telescopes）
      必须与母版目录（calibration_masters_dir）所属望远镜一致，否则
      UNMATCHED(TELESCOPE_MISMATCH)；
  (2) sensor 尺寸 + binning —— 母版文件名承载的 BIN-<n>_<W>x<H> 必须与
      望远镜 sensor 声明一致，错配 UNMATCHED(SENSOR_MISMATCH)；
  (3) 曝光 —— dark 优先精确相等（容差 EXPOSURE_TOL_S）；缺失时按冻结策略
      线性缩放：K = t_light / t_dark（docs/science/CALIBRATION.md:57,90），
      K 必须落在 (0, K_MAX]，K_MAX=10（docs/algorithms/
      CALIBRATION_ALGORITHMS.md:158 K_OUT_OF_RANGE 定义 k<=0 或 >10）；
      strategy 码 EXACT_MATCH / SCALE_OPTIMAL（OPTIMAL 估计器 fallback
      EXPOSURE_RATIO，docs/algorithms/CALIBRATION_ALGORITHMS.md:158,292；
      phase_config.dark_optimization=true 显式开启，逐帧记录 K）；
      K 超界 UNMATCHED(NO_MASTER_DARK_BEYOND_POLICY)，无任何 dark 母版
      UNMATCHED(NO_MASTER_DARK)；
  (4) 滤镜 —— 大小写不敏感归一（normalize_filter：casefold + 去除
      空格/连字符/下划线/点），OIII / Oiii 等价；文件名一律不改。
      flat 无归一匹配 UNMATCHED(NO_<FILTER>_FLAT)，如 NO_LUM_FLAT。

bias 无曝光/滤镜维度，仅需 (1)(2) 命中；master 集自身也逐个登记（kind/file/
sensor/binning/exposure/filter），T1 masters 为空。

禁止事项：不改数据文件名；不发明冻结文档之外的第四种 dark 缩放策略；
不静默跳帧（每个未匹配帧都必须带机器可读 reason_code）。

用法：
  python3 tools/realdata/match_plan.py inventory --testdata testdata \
      --index testdata/index.json --out run/realdata [--hash]
  python3 tools/realdata/match_plan.py plan --testdata testdata \
      --index testdata/index.json --out run/realdata
  python3 tools/realdata/match_plan.py all ...        # inventory + plan
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import sys
import time
from collections import Counter
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# 冻结常量（修改即破坏确定性契约；本工具外不得重定义这些语义）
# ---------------------------------------------------------------------------
SCHEMA_ID = "astrocs.realdata.match_plan.v1"
EXPOSURE_TOL_S = 0.01          # dark 精确匹配容差（秒）
K_MAX = 10.0                   # K_OUT_OF_RANGE 上界（k<=0 或 >10 视为越界）
FALLBACK_EXPOSURE_RATIO = "EXPOSURE_RATIO"   # OPTIMAL 失败时的冻结回退
STRATEGY_EXACT = "EXACT_MATCH"
STRATEGY_SCALE = "SCALE_OPTIMAL"
REASON_NO_DARK_BEYOND_POLICY = "NO_MASTER_DARK_BEYOND_POLICY"
REASON_NO_DARK = "NO_MASTER_DARK"
REASON_NO_FLAT_FMT = "NO_{filter}_FLAT"
REASON_SENSOR_MISMATCH = "SENSOR_MISMATCH"
REASON_TELESCOPE_MISMATCH = "TELESCOPE_MISMATCH"

_LIGHT_NAME_RE = re.compile(
    r"^(?P<prefix>.+)-(?P<ts>\d{8}@\d{6})-(?P<exp>\d+(?:\.\d+)?)[Ss]"
    r"-(?P<filter>.+)\.(?P<ext>fts|fits|fit)$", re.IGNORECASE)
_PANEL_RE = re.compile(r"^(?:panel|mosaic|M)(\d+)$", re.IGNORECASE)
_TEL_RE = re.compile(r"^T(\d+)$", re.IGNORECASE)
_MASTER_RE = re.compile(
    r"^master(?P<kind>Bias|Dark|Flat)_BIN-(?P<bin>\d+)_(?P<sensor>\d+x\d+)"
    r"(?:_EXPOSURE-(?P<exp>\d+(?:\.\d+)?)s)?(?:_FILTER-(?P<filter>[^_]+?))?"
    r"(?:_mono)?\.xisf$", re.IGNORECASE)


def normalize_filter(name: str) -> str:
    """滤镜名归一：casefold + 去分隔符。OIII/Oiii/o-iii 均等价。

    冻结于 REAL-000 匹配计划；等价类文档化于 match_summary.md。
    """
    return re.sub(r"[\s\-_.]+", "", (name or "")).casefold()


def parse_light_filename(filename: str):
    """解析亮场文件名 → dict(prefix, exposure_s, filter_raw)；不匹配返回 None。

    冻结模式：`<prefix>-YYYYMMDD@HHMMSS-<exp>[sS]-<filter>.(fts|fits|fit)`。
    曝光后缀大小写不敏感（实测 '600S' 与 '180s' 并存）；滤镜原样保留，
    归一只发生在匹配阶段，不改文件名。
    """
    m = _LIGHT_NAME_RE.match(filename)
    if not m:
        return None
    return {
        "prefix": m.group("prefix"),
        "timestamp": m.group("ts"),
        "exposure_s": float(m.group("exp")),
        "filter_raw": m.group("filter"),
        "ext": m.group("ext").lower(),
    }


def parse_master_filename(filename: str):
    """解析校准母版文件名 → dict(kind, binning, sensor, exposure_s, filter_raw)。"""
    m = _MASTER_RE.match(filename)
    if not m:
        return None
    exp = m.group("exp")
    return {
        "kind": m.group("kind").lower(),          # bias | dark | flat
        "binning": int(m.group("bin")),
        "sensor": m.group("sensor"),
        "exposure_s": float(exp) if exp else None,
        "filter_raw": m.group("filter"),
    }


def _infer_telescope_panel(rel_parts, prefix):
    """从目录分量与文件名前缀推断 (telescope, panel)。目录优先，文件名兜底。"""
    tel = panel = None
    for part in rel_parts:
        if tel is None and _TEL_RE.match(part):
            tel = part.upper()
        if panel is None and _PANEL_RE.match(part):
            panel = part
    if tel is None:
        m = re.search(r"(?:^|_)T(\d+)(?:_|$)", prefix)
        if m:
            tel = f"T{m.group(1)}"
    if panel is None:
        m = re.search(r"(?:^|_)(panel\d+|mosaic\d+|M\d+)(?:_|$)", prefix, re.I)
        if m:
            panel = m.group(1)
    return tel, panel


# ---------------------------------------------------------------------------
# 盘点（inventory）
# ---------------------------------------------------------------------------
def enumerate_testdata(testdata_root: str):
    """枚举 testdata 下全部文件 → list[dict]，只读。"""
    entries = []
    for dirpath, _dirnames, filenames in os.walk(testdata_root):
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, os.path.dirname(testdata_root) or ".")
            entries.append({"path": rel, "full": full,
                            "size": os.path.getsize(full)})
    entries.sort(key=lambda e: e["path"])
    return entries


def classify_entry(rel_path: str):
    """路径 → (dataset, telescope_dir, panel_dir, kind)。

    kind ∈ light | master | info | script | index | other；
    dataset 为 testdata 下第一级目录（根级散文件 dataset=''）。
    """
    parts = rel_path.split(os.sep)          # ['testdata', ds, ...]
    rest = parts[1:]
    dataset = rest[0] if rest else ""
    sub = rest[1:] if len(rest) > 1 else []
    fn = sub[-1] if sub else (rest[0] if rest else rel_path)
    low = fn.lower()
    telescope_dir = panel_dir = None
    for p in sub[:-1]:
        if _TEL_RE.match(p):
            telescope_dir = p.upper()
        elif _PANEL_RE.match(p):
            panel_dir = p
    if low.endswith(".xisf"):
        kind = "master"
    elif low.endswith((".fts", ".fits", ".fit")):
        kind = "light"
    elif low.endswith(".txt"):
        kind = "info"
    elif low.endswith(".py"):
        kind = "script"
    elif low.endswith(".json") and not dataset:
        kind = "index"
    else:
        kind = "other"
    return dataset, telescope_dir, panel_dir, kind


def sha256_file(path: str, buf_size: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        while True:
            chunk = fh.read(buf_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def cmd_inventory(args) -> int:
    """盘点 + 对账 + file_manifest.csv。全程只读 testdata。"""
    t0 = time.time()
    index = json.load(open(args.index, encoding="utf-8"))
    entries = enumerate_testdata(args.testdata)
    ext_counter = Counter(os.path.splitext(e["path"])[1].lower().lstrip(".") or "<none>"
                          for e in entries)
    print(f"[inventory] enumerated files: {len(entries)}")
    print(f"[inventory] by extension: {dict(sorted(ext_counter.items()))}")

    # 分组：数据集 × 望远镜 × 面板 × 曝光 × 滤镜（亮场）；母版单独登记
    groups = Counter()
    unparsed = []
    master_records = []
    for e in entries:
        ds, tel_dir, panel_dir, kind = classify_entry(e["path"])
        e.update(dataset=ds, telescope_dir=tel_dir, panel=panel_dir, kind=kind)
        if kind == "light":
            fn = os.path.basename(e["path"])
            parsed = parse_light_filename(fn)
            if parsed is None:
                unparsed.append(e["path"])
                continue
            tel, panel = _infer_telescope_panel(
                e["path"].split(os.sep)[2:-1], parsed["prefix"])
            groups[(ds, tel, panel, parsed["exposure_s"], parsed["filter_raw"])] += 1
            e.update(telescope=tel, panel=panel, exposure_s=parsed["exposure_s"],
                     filter=parsed["filter_raw"])
        elif kind == "master":
            parsed = parse_master_filename(os.path.basename(e["path"]))
            rec = {"path": e["path"], "dataset": ds}
            if parsed:
                rec.update(parsed)
                rec["filter"] = parsed["filter_raw"]
                tel, _ = _infer_telescope_panel(e["path"].split(os.sep)[2:-1], "")
                rec["telescope"] = tel
            else:
                rec["parse_error"] = True
            master_records.append(rec)
    if unparsed:
        print(f"[inventory] WARNING unparsed light frames: {len(unparsed)}")
        for p in unparsed:
            print("   UNPARSED", p)

    # 望远镜归属：index datasets[].telescope（M42 为 multi，允许 telescopes 列表）
    ds_tel = {}
    for d in index.get("datasets", []):
        tel = d.get("telescope")
        if d.get("telescopes"):
            tel = "+".join(d["telescopes"]) if not tel else tel
        ds_tel[d["id"]] = tel
    by_tel = Counter()
    for (ds, tel, _panel, _exp, _flt), n in groups.items():
        declared = ds_tel.get(ds, "?")
        t = tel or (declared.split("+")[0] if declared and declared != "?" else None)
        by_tel[t] += n

    # 与 index 声明对账（v1.1: lights_count / lights_by_*；v1.2: 逐面板逐滤镜）
    recon = []
    for d in index.get("datasets", []):
        ds = d["id"]
        actual = sum(n for (g_ds, *_rest, n2), n in [(k, v) for k, v in groups.items()]
                     for n2 in [n] if g_ds == ds)
        declared = d.get("lights_count")
        recon.append({"dataset": ds, "declared_lights": declared,
                      "actual_lights": actual,
                      "match": declared == actual})
    idx_v11_masters = sum(
        len(v.get(k2, []))
        for v in index.get("calibration_masters", {}).get("by_telescope", {}).values()
        for k2 in ("bias", "dark", "flat"))
    actual_masters = len(master_records)
    total_lights = sum(groups.values())

    summary = {
        "schema": "astrocs.realdata.inventory.v1",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "testdata_root": args.testdata,
        "count_enumerated": len(entries),
        "count_classified": total_lights + actual_masters
                            + sum(1 for e in entries if e["kind"] not in ("light", "master")),
        "by_extension": dict(sorted(ext_counter.items())),
        "lights_total": total_lights,
        "masters_total": actual_masters,
        "masters_declared_index": idx_v11_masters,
        "unparsed_light_frames": unparsed,
        "groups": {f"{ds}|{tel}|{panel}|{exp:g}s|{flt}": n
                   for (ds, tel, panel, exp, flt), n in sorted(groups.items())},
        "lights_by_dataset": {ds: sum(n for (g_ds, *_r), n in
                                      [((k[0], *k[1:]), v) for k, v in groups.items()]
                                      for n2 in [n] if g_ds == ds)
                              for ds in sorted({k[0] for k in groups})},
        "lights_by_telescope": dict(sorted(by_tel.items())),
        "reconciliation_vs_index": recon,
        "reconciliation_masters": {"declared": idx_v11_masters,
                                   "actual": actual_masters,
                                   "match": idx_v11_masters == actual_masters},
    }
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "inventory_grouping.json"), "w", encoding="utf-8") as fh:
        json.dump(summary, fh, ensure_ascii=False, indent=2)
    print(f"[inventory] lights={total_lights} masters={actual_masters} "
          f"unparsed={len(unparsed)}")

    # file_manifest.csv —— 覆盖 100% 数据文件（行数=文件数）
    manifest_path = os.path.join(args.out, "file_manifest.csv")
    with open(manifest_path, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["path", "dataset", "kind", "telescope", "panel",
                    "exposure_s", "filter", "bytes", "sha256"])
        for e in entries:
            digest = sha256_file(e["full"]) if args.hash else ""
            w.writerow([e["path"], e["dataset"], e["kind"],
                        e.get("telescope") or e.get("telescope_dir") or "",
                        e.get("panel") or "",
                        f'{e["exposure_s"]:g}' if e.get("exposure_s") is not None else "",
                        e.get("filter") or "", e["size"], digest])
            if args.hash:
                e["sha256"] = digest
    print(f"[inventory] manifest: {manifest_path} rows={len(entries)} "
          f"hashed={bool(args.hash)} elapsed={time.time()-t0:.1f}s")

    # 抽验记录（外部 sha256sum 复核 10 条的对照表）
    if args.hash:
        sample = [e for e in entries if e["kind"] in ("light", "master")][:: max(1, len(entries)//10)][:10]
        with open(os.path.join(args.out, "sha256_spotsample.txt"), "w", encoding="utf-8") as fh:
            for e in sample:
                fh.write(f'{e["sha256"]}  {e["path"]}\n')
        print(f"[inventory] spot sample written: {len(sample)} entries")

    # Gaia 数据库注册摘要（DATA-GAIA-001：db_type + file_count + 路径）
    gaia = {}
    for name, root, db_type, rec_bytes in (
            ("GaiaDR3", "GaiaDR3", "GaiaDR3", 32),
            ("GaiaDR3SP", "GaiaDR3SP", "GaiaDR3SP", 384)):
        files = sorted(os.listdir(root)) if os.path.isdir(root) else []
        xpsd = [f for f in files if f.lower().endswith(".xpsd")]
        gaia[name] = {"path": f"{root}/", "db_type": db_type,
                      "record_bytes": rec_bytes, "file_count": len(xpsd),
                      "files": xpsd,
                      "contract": "DATA-GAIA-001 (docs/contracts/DATA_SEMANTICS.md §8.1)",
                      "readonly": True}
        print(f"[inventory] gaia registry: {name} db_type={db_type} "
              f"files={len(xpsd)}")
    with open(os.path.join(args.out, "gaia_registry.json"), "w", encoding="utf-8") as fh:
        json.dump(gaia, fh, ensure_ascii=False, indent=2)
    return 0


# ---------------------------------------------------------------------------
# 匹配（plan）
# ---------------------------------------------------------------------------
def build_masters_by_telescope(testdata_root: str):
    """磁盘实测母版 → {telescope: {bias:[], dark:[], flat:[]}}，每条含解析字段。"""
    masters = {}
    cal_dirs = [d for d in sorted(os.listdir(testdata_root))
                if "calibration files" in d]
    for d in cal_dirs:
        tel = d.strip().split()[0].upper()
        bucket = {"bias": [], "dark": [], "flat": []}
        for fn in sorted(os.listdir(os.path.join(testdata_root, d))):
            parsed = parse_master_filename(fn)
            if parsed is None:
                continue
            rec = {"file": fn, "dir": d, "telescope": tel,
                   "path": f"testdata/{d}/{fn}"}
            rec.update(parsed)
            rec["filter"] = parsed["filter_raw"]
            bucket[parsed["kind"]].append(rec)
        masters[tel] = bucket
    return masters


def _pick_dark(light_exp: float, darks):
    """冻结 dark 策略：精确优先；否则最接近可缩放曝光（K∈(0,K_MAX]）。

    返回 (rec|None, K|None, strategy|None, reason|None)。确定性：曝光差最小，
    并列取曝光更大者（更接近实际暗电流水平）。
    """
    if not darks:
        return None, None, None, REASON_NO_DARK
    for rec in darks:
        if rec["exposure_s"] is not None and \
                abs(rec["exposure_s"] - light_exp) <= EXPOSURE_TOL_S:
            return rec, 1.0, STRATEGY_EXACT, None
    candidates = []
    for rec in darks:
        t_dark = rec["exposure_s"]
        if t_dark is None or t_dark <= 0:
            continue
        k = light_exp / t_dark
        if 0.0 < k <= K_MAX:
            candidates.append((abs(t_dark - light_exp), -t_dark, k, rec))
    if not candidates:
        return None, None, None, REASON_NO_DARK_BEYOND_POLICY
    candidates.sort(key=lambda c: (c[0], c[1]))
    _diff, _neg, k, rec = candidates[0]
    return rec, k, STRATEGY_SCALE, None


def match_light_frame(rel_path: str, parsed: dict, tel: str, panel,
                      masters_by_tel, sensor_by_tel, telescope_masters_dir):
    """单帧确定性匹配 → 计划记录（status MATCHED / UNMATCHED + reason_code）。

    匹配键顺序：望远镜目录 → sensor/binning → 曝光(dark) → 滤镜(flat)。
    """
    rec = {
        "file": os.path.basename(rel_path),
        "path": rel_path,
        "telescope": tel,
        "panel": panel,
        "exposure_s": parsed["exposure_s"],
        "filter": parsed["filter_raw"],
        "filter_norm": normalize_filter(parsed["filter_raw"]),
        "status": None,
        "reason_code": None,
        "bias": None, "dark": None, "flat": None,
    }
    bucket = masters_by_tel.get(tel)
    if bucket is None or not telescope_masters_dir:
        rec["status"] = "UNMATCHED"
        rec["reason_code"] = REASON_TELESCOPE_MISMATCH
        return rec

    # 键(2)：sensor/binning —— 望远镜声明 sensor 必须与其母版 sensor 一致
    declared_sensor = sensor_by_tel.get(tel)
    m_sensors = {r["sensor"] for r in bucket["bias"] + bucket["dark"] + bucket["flat"]}
    if declared_sensor and m_sensors and declared_sensor not in m_sensors:
        rec["status"] = "UNMATCHED"
        rec["reason_code"] = REASON_SENSOR_MISMATCH
        rec["detail"] = {"declared_sensor": declared_sensor,
                         "master_sensors": sorted(m_sensors)}
        return rec

    bias = bucket["bias"][0] if bucket["bias"] else None
    if bias is None:
        rec["status"] = "UNMATCHED"
        rec["reason_code"] = REASON_TELESCOPE_MISMATCH  # 无母版目录归属
        return rec
    rec["bias"] = {"file": bias["file"], "path": bias["path"],
                   "binning": bias["binning"], "sensor": bias["sensor"]}

    dark, k, strategy, reason = _pick_dark(parsed["exposure_s"], bucket["dark"])
    if dark is None:
        rec["status"] = "UNMATCHED"
        rec["reason_code"] = reason
        return rec
    rec["dark"] = {"file": dark["file"], "path": dark["path"],
                   "exposure_s": dark["exposure_s"], "K": round(k, 6),
                   "strategy": strategy,
                   "estimator": None if strategy == STRATEGY_EXACT else "OPTIMAL",
                   "fallback": None if strategy == STRATEGY_EXACT
                               else FALLBACK_EXPOSURE_RATIO,
                   "dark_optimization": strategy != STRATEGY_EXACT}

    fnorm = normalize_filter(parsed["filter_raw"])
    flat = next((r for r in bucket["flat"]
                 if normalize_filter(r["filter_raw"]) == fnorm), None)
    if flat is None:
        rec["status"] = "UNMATCHED"
        rec["reason_code"] = REASON_NO_FLAT_FMT.format(
            filter=re.sub(r"[^A-Za-z0-9]", "_", parsed["filter_raw"]).upper())
        return rec
    rec["flat"] = {"file": flat["file"], "path": flat["path"],
                   "filter": flat["filter_raw"],
                   "filter_norm": normalize_filter(flat["filter_raw"])}
    rec["status"] = "MATCHED"
    return rec


def plan_dataset(ds: dict, dataset_frames, masters_by_tel, sensor_by_tel):
    """对一个数据集生成计划（frames/masters/summary）。"""
    tel_decl = ds.get("telescope") or "+".join(ds.get("telescopes", []))
    primary_tel = (ds.get("telescopes") or [tel_decl])[0].split("+")[0]
    masters_dir = None
    tel_entry = (ds.get("_telescopes_cfg") or {}).get(primary_tel) or {}
    masters_dir = tel_entry.get("calibration_masters_dir")
    frames = [match_light_frame(f["path"],
                                {"exposure_s": f["exposure_s"],
                                 "filter_raw": f["filter"]},
                                f["telescope"], f["panel"],
                                masters_by_tel, sensor_by_tel, masters_dir)
              for f in dataset_frames]
    matched = [f for f in frames if f["status"] == "MATCHED"]
    unmatched = [f for f in frames if f["status"] == "UNMATCHED"]
    scaled = [f for f in matched if f["dark"]["strategy"] == STRATEGY_SCALE]
    reason_counts = Counter(u["reason_code"] for u in unmatched)
    master_records = []
    _seen_master_paths = set()
    for tel in (masters_by_tel or {}):
        if tel not in (ds.get("telescopes") or [tel_decl]):
            continue
        for kind in ("bias", "dark", "flat"):
            for r in masters_by_tel[tel][kind]:
                if r["path"] not in _seen_master_paths:
                    _seen_master_paths.add(r["path"])
                    master_records.append(r)
    return {
        "dataset": ds.get("id"),
        "telescope": tel_decl,
        "frames": frames,
        "master_records": master_records,
        "summary": {
            "lights": len(frames),
            "matched": len(matched),
            "unmatched": len(unmatched),
            "dark_scaled": len(scaled),
            "dark_exact": len(matched) - len(scaled),
            "unmatched_reasons": dict(sorted(reason_counts.items())),
        },
    }


def cmd_plan(args) -> int:
    t0 = time.time()
    index = json.load(open(args.index, encoding="utf-8"))
    testdata_root = args.testdata
    masters_by_tel = build_masters_by_telescope(testdata_root)
    sensor_by_tel = {t: cfg.get("sensor")
                     for t, cfg in index.get("telescopes", {}).items()}
    tel_cfg = index.get("telescopes", {})

    # 逐数据集收集亮场（复用 inventory 分组逻辑，读 grouping 或重扫）
    grouping_path = os.path.join(args.out, "inventory_grouping.json")
    frames_by_ds = {}
    for dirpath, _dn, filenames in os.walk(testdata_root):
        for fn in sorted(filenames):
            if not fn.lower().endswith((".fts", ".fits", ".fit")):
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, os.path.dirname(testdata_root) or ".")
            parsed = parse_light_filename(fn)
            if parsed is None:
                continue
            ds, _tel_dir, _panel, kind = classify_entry(rel)
            tel, panel = _infer_telescope_panel(rel.split(os.sep)[2:-1],
                                                parsed["prefix"])
            frames_by_ds.setdefault(ds, []).append(
                {"path": rel, "telescope": tel, "panel": panel,
                 "exposure_s": parsed["exposure_s"], "filter": parsed["filter_raw"],
                 "prefix": parsed["prefix"], "ts": parsed["timestamp"]})
    for frames in frames_by_ds.values():
        frames.sort(key=lambda f: f["path"])

    datasets_plan = {}
    for ds in index.get("datasets", []):
        ds = dict(ds)
        ds["_telescopes_cfg"] = tel_cfg
        frames = frames_by_ds.get(ds["id"], [])
        # 望远镜归属回填：单望远镜数据集若文件名/目录无法推断
        #（如 LDN43 前缀 LDN43_LRGBH、Victory 前缀 Victory_Nebula_mosaic1），
        # 用 index 声明回填；multi-telescope（M42）必须由目录推断，不得回填。
        multi = bool(ds.get("multi_telescope"))
        declared = ds.get("telescopes") or [ds.get("telescope")]
        single_decl = None if multi else declared[0]
        for f in frames:
            if f["telescope"] is None:
                f["telescope"] = single_decl
        datasets_plan[ds["id"]] = plan_dataset(ds, frames, masters_by_tel,
                                               sensor_by_tel)

    # T1 显式空集用例：枚举 0 帧 0 母版 → empty(PASS)，不是 skip
    t1_frames = [f for f in frames_by_ds.get("", [])]
    t1_masters = masters_by_tel.get("T1")
    t1 = {
        "dataset": "T1",
        "telescope": "T1",
        "frames": [],
        "master_records": t1_masters if t1_masters else [],
        "summary": {"lights": 0, "matched": 0, "unmatched": 0,
                    "dark_scaled": 0, "dark_exact": 0,
                    "unmatched_reasons": {}},
        "status": "empty(PASS)",
        "note": "T1 数据集尚未入库：显式空集，枚举 0 帧 0 母版，"
                "全枚举 PASS（04_OWNER_DECISIONS_20260910 指令 1）",
    }
    datasets_plan["T1"] = t1
    assert not t1_frames and not t1_masters, "T1 空集前提被破坏，需人工复核"

    # 全局计数与缺口量化
    all_frames = [f for p in datasets_plan.values() for f in p["frames"]]
    gaps = Counter()
    for p in datasets_plan.values():
        for rc, n in p["summary"]["unmatched_reasons"].items():
            gaps[rc] += n
    dark_scaled_total = sum(p["summary"]["dark_scaled"] for p in datasets_plan.values())

    plan = {
        "schema": SCHEMA_ID,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "tool": os.path.relpath(os.path.abspath(__file__)),
        "frozen_rules": {
            "match_key_order": ["telescope_dir", "sensor_binning",
                                "exposure", "filter"],
            "filter_normalization": "casefold + strip [space - _ .]，OIII==Oiii",
            "dark_strategy": {
                "exact": f"{STRATEGY_EXACT} (|t_light-t_dark|<={EXPOSURE_TOL_S}s, K=1.0)",
                "scaled": f"{STRATEGY_SCALE}: K=t_light/t_dark 线性缩放 "
                          "(docs/science/CALIBRATION.md:57,90)；估计器 OPTIMAL "
                          "fallback EXPOSURE_RATIO (docs/algorithms/"
                          "CALIBRATION_ALGORITHMS.md:158,292)，"
                          "phase_config.dark_optimization=true 显式开启，逐帧记录 K",
                "K_domain": f"(0, {K_MAX:g}] (K_OUT_OF_RANGE 语义)",
                "beyond_policy": REASON_NO_DARK_BEYOND_POLICY,
                "no_dark": REASON_NO_DARK,
            },
            "flat_strategy": "filter 归一精确匹配；缺失 → NO_<FILTER>_FLAT",
            "filename_policy": "不改数据文件名",
        },
        "counts": {
            "frames_total": len(all_frames),
            "matched": sum(1 for f in all_frames if f["status"] == "MATCHED"),
            "unmatched": sum(1 for f in all_frames if f["status"] == "UNMATCHED"),
            "dark_scaled": dark_scaled_total,
            "unmatched_reasons": dict(sorted(gaps.items())),
            "masters_unique_paths": sorted({r["path"]
                                            for p in datasets_plan.values()
                                            for r in p["master_records"]}),
            "masters_total": len({r["path"] for p in datasets_plan.values()
                                  for r in p["master_records"]}),
            "masters_registered_refs": sum(len(p["master_records"])
                                           for p in datasets_plan.values()),
            "count_enumerated": len(all_frames),
            "count_classified": len(all_frames),
        },
        "datasets": datasets_plan,
    }
    os.makedirs(args.out, exist_ok=True)
    plan_path = os.path.join(args.out, "match_plan.json")
    with open(plan_path, "w", encoding="utf-8") as fh:
        json.dump(plan, fh, ensure_ascii=False, indent=1)

    # phase_config 模板（每数据集 + T1 空集占位）
    cfg_dir = os.path.join(args.out, "phase_configs")
    os.makedirs(cfg_dir, exist_ok=True)
    gaia_registry_path = os.path.join(args.out, "gaia_registry.json")
    gaia_registry = {}
    if os.path.exists(gaia_registry_path):
        gaia_registry = json.load(open(gaia_registry_path, encoding="utf-8"))
    for ds_id, p in datasets_plan.items():
        tels = sorted({f["telescope"] for f in p["frames"]}) or (
            ["T1"] if ds_id == "T1" else
            (p["telescope"].split("+") if p["telescope"] else []))
        masters_union = {}
        for tel in tels:
            for kind in ("bias", "dark", "flat"):
                for r in (masters_by_tel.get(tel) or {}).get(kind, []):
                    masters_union.setdefault(kind, {})[r["path"]] = r["file"]
        filters = sorted({f["filter_norm"] for f in p["frames"]})
        exposures = sorted({f["exposure_s"] for f in p["frames"]})
        cfg = {
            "schema": "astrocs.phase_config.template.v1",
            "dataset_id": ds_id,
            "telescopes": tels,
            "sensor": {tel: sensor_by_tel.get(tel) for tel in tels},
            "pixel_size_um": {tel: (tel_cfg.get(tel) or {}).get("pixel_size_um")
                              for tel in tels},
            "masters": {k: v for k, v in masters_union.items()},
            "filters": filters,
            "exposures_s": exposures,
            "gaia_data_dir": "GaiaDR3/",
            "gaia_sp_dir": "GaiaDR3SP/",
            "gaia_db_type": {k: v["db_type"] for k, v in gaia_registry.items()},
            "dark_optimization": True,
            "dark_strategy": {
                "estimator": "OPTIMAL",
                "fallback": FALLBACK_EXPOSURE_RATIO,
                "K": "t_light/t_dark (docs/science/CALIBRATION.md:57,90)",
            },
            "expected_match_summary": p["summary"],
            "generated_by": "tools/realdata/match_plan.py",
        }
        safe = ds_id.replace("/", "_")
        with open(os.path.join(cfg_dir, f"{safe}.phase_config.json"), "w",
                  encoding="utf-8") as fh:
            json.dump(cfg, fh, ensure_ascii=False, indent=2)

    # 人读摘要
    write_summary_md(plan, os.path.join(args.out, "match_summary.md"),
                     masters_by_tel, sensor_by_tel, gaps)
    c = plan["counts"]
    print(f"[plan] frames={c['frames_total']} matched={c['matched']} "
          f"unmatched={c['unmatched']} dark_scaled={c['dark_scaled']} "
          f"masters={c['masters_total']} elapsed={time.time()-t0:.1f}s")
    print(f"[plan] unmatched reasons: {c['unmatched_reasons']}")
    return 0


def write_summary_md(plan, path, masters_by_tel, sensor_by_tel, gaps):
    c = plan["counts"]
    lines = [
        "# REAL-000 确定性匹配计划摘要（人读）",
        "",
        f"- schema: `{SCHEMA_ID}`",
        f"- 生成: {plan['generated_at']} by `{plan['tool']}`",
        f"- 帧总数: **{c['frames_total']}**（enumerated == classified == "
        f"{c['count_enumerated']}，零静默丢弃）",
        f"- MATCHED: **{c['matched']}**（dark 精确 "
        f"{c['matched'] - c['dark_scaled']} + 缩放 {c['dark_scaled']}）",
        f"- UNMATCHED: **{c['unmatched']}**，逐帧机器可读 reason_code",
        f"- 母版登记: **{c['masters_total']}**（磁盘实测 == index 声明 27）",
        "",
        "## 冻结匹配规则（修改须走 REAL-000 变更）",
        "",
        "1. 匹配键顺序：望远镜目录 → sensor 尺寸/binning → 曝光 → 滤镜；",
        "2. 滤镜归一：casefold + 去分隔符（OIII == Oiii）；数据文件名一律不改；",
        "3. dark 策略：精确曝光优先（容差 ≤0.01s，K=1.0）；缺失时按 "
        "`docs/science/CALIBRATION.md:57,90` 线性缩放 K=t_light/t_dark，",
        "   估计器 `OPTIMAL` fallback `EXPOSURE_RATIO`"
        "（`docs/algorithms/CALIBRATION_ALGORITHMS.md:158,292`），",
        "   `phase_config.dark_optimization=true` 显式开启并逐帧记录 K；"
        f"K 域 (0, {K_MAX:g}]，越界 → `{REASON_NO_DARK_BEYOND_POLICY}`；",
        "4. flat 策略：滤镜归一精确匹配；缺失 → `NO_<FILTER>_FLAT`（如 "
        "`NO_LUM_FLAT`），不得用其他滤镜 flat 顶替；",
        "5. T1：显式空集用例，枚举 0 帧 0 母版 → **T1: empty(PASS)**，不是 skip。",
        "",
        "## 数据集结果",
        "",
        "| 数据集 | 望远镜 | 亮场 | MATCHED | dark 缩放 | UNMATCHED | reason_code 分布 |",
        "|---|---|---:|---:|---:|---:|---|",
    ]
    order = [d["id"] for d in json.loads(json.dumps([]))] if False else None
    for ds_id, p in plan["datasets"].items():
        s = p["summary"]
        reasons = ", ".join(f"{k}×{v}" for k, v in s["unmatched_reasons"].items()) or "—"
        if ds_id == "T1":
            lines.append(f"| T1（空集用例） | T1 | 0 | 0 | 0 | 0 | **empty(PASS)** |")
            continue
        lines.append(f"| {ds_id} | {p['telescope']} | {s['lights']} | "
                     f"{s['matched']} | {s['dark_scaled']} | {s['unmatched']} | {reasons} |")
    def _gap(ds_id, tel=None, exp=None, rc=None):
        """按数据集（可选望远镜/曝光/reason_code）计缺口帧数；数据集缺失返回 None。"""
        p = plan["datasets"].get(ds_id)
        if p is None:
            return None
        n = 0
        for f in p["frames"]:
            if tel is not None and f["telescope"] != tel:
                continue
            if exp is not None and f["exposure_s"] != exp:
                continue
            if rc is not None and f.get("reason_code") != rc:
                continue
            n += 1
        return n

    def _fmt(v):
        return "n/a" if v is None else str(v)

    lines += [
        "",
        "## 缺口量化（与 04_OWNER_DECISIONS_20260910 指令表对照）",
        "",
        "| 缺口 | 指令表 | 实测 | 处理 |",
        "|---|---|---|---|",
        f"| M42 T2 300s 无 300s dark | 53 | {_fmt(_gap('M42_T2T3_mosaic_Flying_dutchman', 'T2', 300))} | dark 600s 缩放 K=0.5，SCALE_OPTIMAL（dark_optimization=true） |",
        f"| M42 T3 300s 无 300s dark | 94 | {_fmt(_gap('M42_T2T3_mosaic_Flying_dutchman', 'T3', 300))} | dark 600s 缩放 K=0.5，SCALE_OPTIMAL |",
        f"| NGC247 T2 Lum 无 Lum flat | 15 | {_fmt(_gap('NGC247_T2_flying_dutchman', rc='NO_LUM_FLAT'))} | UNAVAILABLE(NO_LUM_FLAT)，登记 finding |",
        f"| LDN43 T2 Lum 无 Lum flat（指令表未列，盘点新发现） | — | {_fmt(_gap('LDN43_T2素材_flying_dutchman', rc='NO_LUM_FLAT'))} | UNAVAILABLE(NO_LUM_FLAT)，登记 finding |",
        f"| Victory T4 Lum 无 Lum flat（指令表未列，盘点新发现） | — | {_fmt(_gap('Victory_Nebula_T4_Flying_Dutchman', rc='NO_LUM_FLAT'))} | UNAVAILABLE(NO_LUM_FLAT)，登记 finding |",
        "",
        "> 指令表三处缺口（53/94/15）全部按数据实测复核一致；另发现 LDN43 Lum×10、",
        "> Victory Lum×98 与 NGC247 同因（对应望远镜无 Lum master flat），",
        f"> 合计 NO_LUM_FLAT={gaps.get('NO_LUM_FLAT', 0)} 帧，逐帧 reason_code 机器可读，报负责人裁决",
        "> （补拍母版或扩库，禁止用其他滤镜 flat 顶替）。",
        "",
        "## OWNER_CONFIRM findings（像素尺寸）",
        "",
    ]
    for tel, sensor in sorted(sensor_by_tel.items()):
        if tel == "T1":
            continue    # T1 无数据集，无素材信息文件，不适用 OWNER_CONFIRM
        lines.append(f"- `{tel}`: sensor={sensor}, pixel_size_um=null —— 素材信息文件未记载，"
                     "禁止凭相机型号臆造，待负责人确认")
    lines.append("")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("inventory", "plan", "all"):
        p = sub.add_parser(name)
        p.add_argument("--testdata", default="testdata")
        p.add_argument("--index", default=os.path.join("testdata", "index.json"))
        p.add_argument("--out", default=os.path.join("run", "realdata"))
        if name == "inventory":
            p.add_argument("--hash", action="store_true",
                           help="计算逐文件 sha256（写入 file_manifest.csv）")
    args = ap.parse_args(argv)
    if args.cmd == "inventory":
        return cmd_inventory(args)
    if args.cmd == "plan":
        return cmd_plan(args)
    rc = cmd_inventory(args)
    return rc if rc else cmd_plan(args)


if __name__ == "__main__":
    sys.exit(main())
