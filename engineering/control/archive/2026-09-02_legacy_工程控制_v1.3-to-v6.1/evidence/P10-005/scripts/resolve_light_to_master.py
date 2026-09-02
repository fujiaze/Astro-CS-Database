"""P10-005: Light 到 Master 唯一解析 resolver.

依据:
- tasks/P10-005.md
- docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md

匹配键 (按规范):
- 设备 ID
- 传感器尺寸 (image_size)
- Bin
- Gain/Offset (FLI 相机 Header 未写入, 全部为空, 视为通配)
- 曝光 (Dark)
- 温度范围 (全部 -20.0°C, 视为通配)
- 滤镜规范名 (Flat)
- Master 类型 (Bias/Dark/Flat)

匹配规则:
- Bias: 匹配设备 ID + Bin + image_size (传感器模式), 输出唯一
- Dark: 匹配设备 ID + Bin + image_size, 曝光规则:
  - 优先精确匹配 (Light exposure == Dark exposure)
  - 否则选择 >= Light exposure 的最接近 dark (防止过暗)
  - 若全部 dark < Light exposure, 选择最长的 dark (兜底)
- Flat: 匹配设备 ID + Bin + image_size + 滤镜规范名, 输出唯一
  - 缺失时标记 UNRESOLVED (reason=missing_flat_for_<canonical>)

禁止捷径:
- 不得 first-match (必须按规则明确选择)
- 不得静默选择歧义 (必须输出选择理由 + 歧义标记)

输出:
- LIGHT_TO_MASTER_RESOLUTION.csv (710 行, 每行一个 Light)
- UNRESOLVED_CALIBRATION_REPORT.md (unresolved 列表)
- RESOLUTION_SUMMARY.json (汇总统计)
"""
from __future__ import annotations

import csv
import json
import re
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_001_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-001"
P10_002_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-002"
P10_003_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-003"
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"
P10_005_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-005"
TESTDATA_DIR = REPO_ROOT / "testdata"


def iso_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def load_filter_alias_map() -> dict:
    with open(P10_004_DIR / "FILTER_ALIAS_MAP.json", "r", encoding="utf-8") as f:
        return json.load(f)


def normalize_filter(fmap: dict, alias: str) -> str | None:
    """使用 P10-004 的归一化函数."""
    if not alias:
        return None
    key = alias.strip()
    if not key:
        return None
    a2c = fmap.get("alias_to_canonical", {})
    if key in a2c:
        return a2c[key]
    upper = key.upper()
    if upper in a2c:
        return a2c[upper]
    return None


def load_calibration_masters(fmap: dict) -> list[dict]:
    """加载 P10-003 的 27 个 master, 添加 canonical_filter + 统一 image_size 字段.

    统一字段策略 (header 优先, filename 兜底):
    - image_size: image_size_from_header or image_size_from_filename
      (XISF header 中 NAXIS1/NAXIS2 缺失时, 文件名解析的尺寸是可靠回退)
    - filter_raw: filter_from_header or filter_from_filename
    - exposure_s: float(exposure_from_header or exposure_from_filename or 0)
    - bin_int: int(bin_from_header or bin_from_filename or 1)
    """
    masters = []
    csv_path = P10_003_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            # 规范化滤镜 (header 优先, filename 兜底)
            row["canonical_filter"] = normalize_filter(
                fmap,
                row.get("filter_from_header") or row.get("filter_from_filename") or ""
            )
            # 统一 exposure (header 优先, filename 兜底)
            exp_raw = row.get("exposure_from_header") or row.get("exposure_from_filename") or "0"
            try:
                row["exposure_s"] = float(exp_raw)
            except (ValueError, TypeError):
                row["exposure_s"] = 0.0
            # 统一 bin (header 优先, filename 兜底)
            bin_raw = row.get("bin_from_header") or row.get("bin_from_filename") or "1"
            try:
                row["bin_int"] = int(bin_raw)
            except (ValueError, TypeError):
                row["bin_int"] = 1
            # 统一 image_size (header 优先, filename 兜底)
            # 注: P10-003 中 XISF header 未提取 NAXIS1/NAXIS2 (存于 Image 元素属性,
            # 而非 FITSKeyword), 故 image_size_from_header 全为空.
            # 文件名格式 masterXxx_BIN-1_<W>x<H>_... 已包含可靠尺寸, 可作兜底.
            row["image_size"] = (
                row.get("image_size_from_header")
                or row.get("image_size_from_filename")
                or ""
            )
            masters.append(row)
    return masters


def load_fits_header_samples() -> dict:
    """加载 P10-001 的 49 个 FITS Header 采样 (key: 'target/device/panel/filter')."""
    with open(P10_001_DIR / "raw_logs" / "header_samples.json", "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("fits_headers", {})


def derive_device_id(light_path: Path) -> str | None:
    """从 light 文件路径推导设备 ID (T2/T3/T4)."""
    parts = light_path.parts
    for p in parts:
        m = re.match(r"^T([1-4])(?:[^a-zA-Z0-9]|$)", p)
        if m:
            return f"T{m.group(1)}"
        # 处理 "LDN43_T2素材_flying_dutchman" 形式
        m2 = re.search(r"_T([1-4])(?:[^a-zA-Z0-9]|$)", p)
        if m2:
            return f"T{m2.group(1)}"
    return None


def derive_target_panel_filter(light_path: Path) -> tuple[str, str, str]:
    """从路径推导 target/panel/filter, 用于 Header 查找.
    返回 (target, panel, filter) 字符串, 用于匹配 header_samples.json 的 key."""
    parts = light_path.relative_to(TESTDATA_DIR).parts
    # parts[0] = top dir (如 "Galaxy_Center_T4", "LDN43_T2素材_flying_dutchman")
    # parts[1] = "lights"
    # parts[2] = (optional) panel subdir (如 "panel1")
    # parts[-1] = filename
    top = parts[0]
    # target: 提取首段标识 (Galaxy_Center, LDN43, NGC1727, NGC247, NGC55, NGC83_cluster, Victory_Nebula)
    target = top.split("_T")[0] if "_T" in top else top.split("_flying")[0]
    if target == "NGC83":
        target = "NGC83_cluster"
    # panel: 若 parts[2] = "lights" 且 parts[3] 存在且以 "panel" 开头
    panel = ""
    if len(parts) >= 4 and parts[2] == "lights" and parts[3].startswith("panel"):
        panel = parts[3]
    # filter: 从文件名提取 (最后一个 -<Filter>.fts)
    fname = light_path.stem
    m = re.search(r"-([A-Za-z\-]+)$", fname)
    filter_raw = m.group(1) if m else ""
    return target, panel, filter_raw


def find_header_for_light(header_samples: dict, target: str, panel: str, filter_raw: str) -> dict | None:
    """查找 Light 对应的 Header 采样."""
    # header_samples key 格式: "Galaxy_Center_T4/panel1/Red" 或 "LDN43_T2素材_flying_dutchman/Blue"
    # 先尝试 target/panel/filter
    candidates = []
    for key, hdr in header_samples.items():
        kt = key.split("/")[0]
        # target 匹配
        if not kt.startswith(target):
            continue
        # panel 匹配
        if panel and panel not in key:
            continue
        # filter 匹配 (canonical)
        hdr_filter = hdr.get("filter", "")
        if hdr_filter == filter_raw:
            return hdr
        candidates.append((key, hdr))
    # 若精确匹配失败, 用 filter canonical 匹配
    if candidates:
        return candidates[0][1]
    return None


def match_bias(masters: list[dict], device_id: str, bin_int: int, image_size: str) -> tuple[dict | None, str, str]:
    """匹配 Bias Master: 设备 ID + Bin + image_size (传感器模式).

    使用统一 image_size 字段 (header 优先, filename 兜底).
    """
    matches = [m for m in masters
               if m["master_type"] == "Bias"
               and m["device_id"] == device_id
               and m["bin_int"] == bin_int
               and m["image_size"] == image_size]
    if len(matches) == 1:
        return matches[0], "unique", f"device={device_id} bin={bin_int} size={image_size} -> {matches[0]['file_name']}"
    elif len(matches) == 0:
        return None, "missing", f"no Bias master for device={device_id} bin={bin_int} size={image_size}"
    else:
        # 歧义: 多个匹配 (不应发生, 文件名 + header 一致性已校验)
        return matches[0], "ambiguous", f"multiple Bias matches ({len(matches)}), selected first: {matches[0]['file_name']}"


def match_dark(masters: list[dict], device_id: str, bin_int: int, image_size: str, light_exposure: float) -> tuple[dict | None, str, str]:
    """匹配 Dark Master: 设备 ID + Bin + image_size + 曝光规则.

    使用统一 image_size 字段 (header 优先, filename 兜底).
    曝光规则:
    1. 优先精确匹配 (Light exposure == Dark exposure, 容差 0.01s)
    2. 否则选择 >= Light exposure 的最接近 dark (防止过暗)
    3. 若全部 dark < Light exposure, 选择最长的 dark (兜底, 显式标记)
    """
    candidates = [m for m in masters
                  if m["master_type"] == "Dark"
                  and m["device_id"] == device_id
                  and m["bin_int"] == bin_int
                  and m["image_size"] == image_size]
    if not candidates:
        return None, "missing", f"no Dark master for device={device_id} bin={bin_int} size={image_size}"

    # 1. 精确匹配
    exact = [m for m in candidates if abs(m["exposure_s"] - light_exposure) < 0.01]
    if len(exact) == 1:
        return exact[0], "exact", f"exact exposure match: Light={light_exposure}s == Dark={exact[0]['exposure_s']}s -> {exact[0]['file_name']}"
    elif len(exact) > 1:
        # 歧义 (理论上每设备每曝光只有一个 dark, 但若发生则选第一个并标记)
        return exact[0], "ambiguous_exact", f"multiple exact dark matches ({len(exact)}), selected first: {exact[0]['file_name']}"

    # 2. >= Light exposure 的最接近 dark
    longer = [m for m in candidates if m["exposure_s"] >= light_exposure]
    if longer:
        longer.sort(key=lambda m: m["exposure_s"])
        best = longer[0]
        return best, "closest_longer", f"closest dark >= Light exposure: Light={light_exposure}s, Dark={best['exposure_s']}s -> {best['file_name']}"

    # 3. 兜底: 最长的 dark (全部 dark < Light exposure, 显式标记为 fallback)
    candidates.sort(key=lambda m: m["exposure_s"], reverse=True)
    best = candidates[0]
    return best, "fallback_longest", f"fallback: all darks < Light exposure ({light_exposure}s), selected longest dark={best['exposure_s']}s -> {best['file_name']}"


def match_flat(masters: list[dict], device_id: str, bin_int: int, image_size: str, canonical_filter: str) -> tuple[dict | None, str, str]:
    """匹配 Flat Master: 设备 ID + Bin + image_size + 滤镜规范名.

    使用统一 image_size 字段 (header 优先, filename 兜底).
    缺失时标记 missing_<filter>_flat (不静默选择其他滤镜 flat).
    """
    if not canonical_filter:
        return None, "missing", f"no canonical filter for Light"
    matches = [m for m in masters
               if m["master_type"] == "Flat"
               and m["device_id"] == device_id
               and m["bin_int"] == bin_int
               and m["image_size"] == image_size
               and m["canonical_filter"] == canonical_filter]
    if len(matches) == 1:
        return matches[0], "unique", f"device={device_id} bin={bin_int} size={image_size} filter={canonical_filter} -> {matches[0]['file_name']}"
    elif len(matches) == 0:
        return None, "missing", f"no Flat master for device={device_id} bin={bin_int} size={image_size} filter={canonical_filter} (missing_{canonical_filter.lower()}_flat)"
    else:
        return matches[0], "ambiguous", f"multiple Flat matches ({len(matches)}), selected first: {matches[0]['file_name']}"


def main() -> int:
    print("=" * 70, flush=True)
    print("P10-005 resolve_light_to_master.py 启动", flush=True)
    print("=" * 70, flush=True)

    # 加载数据
    print("\n[阶段 1] 加载数据...", flush=True)
    fmap = load_filter_alias_map()
    masters = load_calibration_masters(fmap)
    print(f"  Master 数: {len(masters)} (T2:{sum(1 for m in masters if m['device_id']=='T2')} T3:{sum(1 for m in masters if m['device_id']=='T3')} T4:{sum(1 for m in masters if m['device_id']=='T4')})", flush=True)

    header_samples = load_fits_header_samples()
    print(f"  Header 采样: {len(header_samples)} 个 (FITS)", flush=True)

    # 枚举所有 Light 文件
    print("\n[阶段 2] 枚举 Light 文件...", flush=True)
    light_files = sorted(TESTDATA_DIR.rglob("*.fts"))
    print(f"  Light 文件总数: {len(light_files)}", flush=True)

    # 阶段 3: 为每个 Light 解析 Bias/Dark/Flat
    print("\n[阶段 3] 解析每张 Light...", flush=True)
    rows = []
    unresolved = []
    by_device = defaultdict(int)
    by_filter = defaultdict(int)
    by_status = defaultdict(int)

    for i, lf in enumerate(light_files, 1):
        rel_path = lf.relative_to(REPO_ROOT).as_posix()
        device_id = derive_device_id(lf)
        target, panel, filter_raw = derive_target_panel_filter(lf)
        hdr = find_header_for_light(header_samples, target, panel, filter_raw) or {}

        filter_header = hdr.get("filter", "") or filter_raw
        canonical = normalize_filter(fmap, filter_header) or "UNKNOWN"
        exposure = float(hdr.get("exposure") or 0.0)
        bin_int = int(hdr.get("bin") or 1)
        image_size = hdr.get("image_size") or ""
        temp = hdr.get("temp") or ""

        # 匹配 Bias
        bias_m, bias_status, bias_reason = match_bias(masters, device_id, bin_int, image_size)
        # 匹配 Dark
        dark_m, dark_status, dark_reason = match_dark(masters, device_id, bin_int, image_size, exposure)
        # 匹配 Flat
        flat_m, flat_status, flat_reason = match_flat(masters, device_id, bin_int, image_size, canonical)

        # 判断 resolved
        bias_ok = bias_m is not None and bias_status == "unique"
        dark_ok = dark_m is not None and dark_status in ("exact", "closest_longer", "fallback_longest")
        flat_ok = flat_m is not None and flat_status == "unique"
        resolved = bias_ok and dark_ok and flat_ok

        # 歧义标记
        ambiguity = "NONE"
        if not bias_ok:
            ambiguity = f"BIAS_{bias_status.upper()}"
        elif not dark_ok:
            ambiguity = f"DARK_{dark_status.upper()}"
        elif not flat_ok:
            ambiguity = f"FLAT_{flat_status.upper()}"

        # 统计
        by_device[device_id] += 1
        by_filter[canonical] += 1
        by_status["resolved" if resolved else "unresolved"] += 1

        if not resolved:
            unresolved.append({
                "light_path": rel_path,
                "device_id": device_id,
                "filter_canonical": canonical,
                "exposure_s": exposure,
                "bias_status": bias_status,
                "dark_status": dark_status,
                "flat_status": flat_status,
                "ambiguity": ambiguity,
                "bias_reason": bias_reason,
                "dark_reason": dark_reason,
                "flat_reason": flat_reason,
            })

        row = {
            "light_path": rel_path,
            "light_file": lf.name,
            "device_id": device_id,
            "target": target,
            "panel": panel,
            "filter_raw": filter_header,
            "filter_canonical": canonical,
            "exposure_s": exposure,
            "bin": bin_int,
            "image_size": image_size,
            "temp_c": temp,
            "bias_master": bias_m["file_path"] if bias_m else "",
            "bias_status": bias_status,
            "bias_match_reason": bias_reason,
            "dark_master": dark_m["file_path"] if dark_m else "",
            "dark_status": dark_status,
            "dark_match_reason": dark_reason,
            "flat_master": flat_m["file_path"] if flat_m else "",
            "flat_status": flat_status,
            "flat_match_reason": flat_reason,
            "resolved": "YES" if resolved else "NO",
            "ambiguity": ambiguity,
        }
        rows.append(row)

        if i % 100 == 0 or i == len(light_files):
            print(f"  [{i}/{len(light_files)}] 解析中... (resolved={by_status['resolved']}, unresolved={by_status['unresolved']})", flush=True)

    # 阶段 4: 写入 CSV
    print("\n[阶段 4] 写入 LIGHT_TO_MASTER_RESOLUTION.csv...", flush=True)
    out_csv = P10_005_DIR / "LIGHT_TO_MASTER_RESOLUTION.csv"
    fields = ["light_path", "light_file", "device_id", "target", "panel",
              "filter_raw", "filter_canonical", "exposure_s", "bin", "image_size", "temp_c",
              "bias_master", "bias_status", "bias_match_reason",
              "dark_master", "dark_status", "dark_match_reason",
              "flat_master", "flat_status", "flat_match_reason",
              "resolved", "ambiguity"]
    with open(out_csv, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    print(f"  [写入] {out_csv} ({len(rows)} 行)", flush=True)

    # 阶段 5: 写入 UNRESOLVED_CALIBRATION_REPORT.md
    print("\n[阶段 5] 写入 UNRESOLVED_CALIBRATION_REPORT.md...", flush=True)
    out_md = P10_005_DIR / "UNRESOLVED_CALIBRATION_REPORT.md"
    with open(out_md, "w", encoding="utf-8") as f:
        f.write("# Unresolved Calibration Report\n\n")
        f.write(f"- Task: P10-005\n")
        f.write(f"- Generated: {iso_now()}\n")
        f.write(f"- Total Light frames: {len(rows)}\n")
        f.write(f"- Resolved: {by_status['resolved']}\n")
        f.write(f"- Unresolved: {by_status['unresolved']}\n\n")
        f.write("## Unresolved Details\n\n")
        if not unresolved:
            f.write("No unresolved Light frames.\n")
        else:
            # 按歧义类型分组
            by_amb = defaultdict(list)
            for u in unresolved:
                by_amb[u["ambiguity"]].append(u)
            for amb, items in sorted(by_amb.items()):
                f.write(f"### {amb} ({len(items)} frames)\n\n")
                f.write("| Light | Device | Filter | Exposure | Reason |\n")
                f.write("|-------|--------|--------|----------|--------|\n")
                for u in items[:20]:  # 前 20 个示例
                    f.write(f"| {u['light_path']} | {u['device_id']} | {u['filter_canonical']} | {u['exposure_s']}s | {u['flat_reason'] or u['dark_reason'] or u['bias_reason']} |\n")
                if len(items) > 20:
                    f.write(f"\n... and {len(items) - 20} more.\n")
                f.write("\n")
    print(f"  [写入] {out_md}", flush=True)

    # 阶段 6: 写入 RESOLUTION_SUMMARY.json
    print("\n[阶段 6] 写入 RESOLUTION_SUMMARY.json...", flush=True)
    summary = {
        "_description": "P10-005 Light-to-Master Resolution Summary",
        "generated_at": iso_now(),
        "total_lights": len(rows),
        "resolved": by_status["resolved"],
        "unresolved": by_status["unresolved"],
        "by_device": dict(by_device),
        "by_filter_canonical": dict(by_filter),
        "by_status": dict(by_status),
        "ambiguity_breakdown": dict(defaultdict(int, {u["ambiguity"]: sum(1 for x in unresolved if x["ambiguity"] == u["ambiguity"]) for u in unresolved})),
        "bias_status_breakdown": dict(defaultdict(int, {r["bias_status"]: 0 for r in rows})),
        "dark_status_breakdown": dict(defaultdict(int, {r["dark_status"]: 0 for r in rows})),
        "flat_status_breakdown": dict(defaultdict(int, {r["flat_status"]: 0 for r in rows})),
    }
    # 重新统计 breakdown
    for r in rows:
        summary["bias_status_breakdown"][r["bias_status"]] = summary["bias_status_breakdown"].get(r["bias_status"], 0) + 1
        summary["dark_status_breakdown"][r["dark_status"]] = summary["dark_status_breakdown"].get(r["dark_status"], 0) + 1
        summary["flat_status_breakdown"][r["flat_status"]] = summary["flat_status_breakdown"].get(r["flat_status"], 0) + 1
    summary["ambiguity_breakdown"] = dict(summary["ambiguity_breakdown"])
    # 按 ambiguity 重新统计
    amb_count = defaultdict(int)
    for r in rows:
        amb_count[r["ambiguity"]] += 1
    summary["ambiguity_breakdown"] = dict(amb_count)

    out_json = P10_005_DIR / "RESOLUTION_SUMMARY.json"
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    print(f"  [写入] {out_json}", flush=True)

    # 阶段 7: 打印汇总
    print("\n" + "=" * 70, flush=True)
    print("汇总:", flush=True)
    print(f"  Total lights: {len(rows)}", flush=True)
    print(f"  Resolved: {by_status['resolved']} ({100*by_status['resolved']/len(rows):.1f}%)", flush=True)
    print(f"  Unresolved: {by_status['unresolved']} ({100*by_status['unresolved']/len(rows):.1f}%)", flush=True)
    print(f"\n  按设备:", flush=True)
    for d, c in sorted(by_device.items()):
        print(f"    {d}: {c}", flush=True)
    print(f"\n  按滤镜规范名:", flush=True)
    for f_, c in sorted(by_filter.items()):
        print(f"    {f_}: {c}", flush=True)
    print(f"\n  Bias 状态:", flush=True)
    for s, c in sorted(summary["bias_status_breakdown"].items()):
        print(f"    {s}: {c}", flush=True)
    print(f"\n  Dark 状态:", flush=True)
    for s, c in sorted(summary["dark_status_breakdown"].items()):
        print(f"    {s}: {c}", flush=True)
    print(f"\n  Flat 状态:", flush=True)
    for s, c in sorted(summary["flat_status_breakdown"].items()):
        print(f"    {s}: {c}", flush=True)
    print(f"\n  歧义分类:", flush=True)
    for a, c in sorted(summary["ambiguity_breakdown"].items()):
        print(f"    {a}: {c}", flush=True)
    print("=" * 70, flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
