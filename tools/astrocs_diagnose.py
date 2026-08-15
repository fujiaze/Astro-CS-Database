#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""astrocs_diagnose.py — AstroCS 轻量故障定位工具 (V19)

用法:
  py -3.12 tools/astrocs_diagnose.py <run_dir> [--json out.json] [--timeout 30]

功能:
  - 扫描 run_dir 下的日志 (*.log)、stage1/stage2 配置与产物清单
  - 按统一 stage ID + error taxonomy 归类 symptom → stage → evidence →
    likely cause → command → fix path
  - 输出人类可读报告 + 机器可读 JSON
  - 默认不读取大 FITS/HiPS; 外部命令 (如 ctest) 带 timeout

错误分类 (与 docs/ERROR_TAXONOMY.md 一致):
  E100 DLL_LOAD_FAILED      E200 BLOCK_MISSING       E300 CALIBRATE_FAILED
  E400 PLATESOLVE_FAILED    E500 PSF_FAILED          E600 PHOTOMETRIC_FAILED
  E700 SNR_FAILED           E800 DRIZZLE_FAILED      E900 HIPS_WRITE_FAILED
  E910 HIPS_VERIFY_FAILED   E920 STAGE2_FAILED       E950 CONFIG_ERROR
  E960 FILE_IO_ERROR        E970 GENERIC_ERROR       E980 TIMEOUT
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time


STAGE_IDS = [
    "READ_FITS", "CALIBRATE", "PLATESOLVE", "PSF", "PHOTOMETRIC",
    "SNR", "NSIDE", "DRIZZLE", "HIPS_WRITE", "HIPS_VERIFY",
    "STAGE2_DISCOVER", "STAGE2_COVERAGE", "STAGE2_UPM", "STAGE2_REJECT",
    "STAGE2_INTEGRATE", "STAGE2_HIPS",
]

ERROR_MAP = [
    (r"exit_code[\"' :=]+2|DLL 未加载|DLL_LOAD_FAILED", "E100", "DLL_LOAD_FAILED",
     "动态库未加载 (mingw64 PATH / DLL 依赖缺失)", "Set-Item env:Path 'C:\\msys64\\mingw64\\bin;'+$env:Path; .\\toolchain.ps1 check"),
    (r"块不存在|BLOCK_MISSING|block missing", "E200", "BLOCK_MISSING",
     "上游阶段未产出必需命名块", "检查前一阶段日志; 确认 stage 顺序与 stop_after 配置"),
    (r"CALIBRATE_FAILED|校准失败|calibrat.*fail", "E300", "CALIBRATE_FAILED",
     "校准阶段失败 (master 缺失/尺寸不匹配)", "核对 calibration masters 路径与 BINNING/FILTER"),
    (r"PLATESOLVE_FAILED|platesolve.*fail|WCS 求解失败", "E400", "PLATESOLVE_FAILED",
     "星表匹配/WCS 求解失败", "检查 OBJCTRA/DEC 初值、GaiaDR3SP 目录、sip order"),
    (r"PSF_FAILED|psf.*fail|PSF 拟合失败", "E500", "PSF_FAILED",
     "PSF 拟合失败 (星点不足/饱和)", "检查 star_det 块与拟合半径参数"),
    (r"PHOTOMETRIC_FAILED|photometric.*fail|测光定标失败", "E600", "PHOTOMETRIC_FAILED",
     "测光定标失败 (Gaia 匹配不足)", "检查 filters/QE 曲线与 Gaia 检索半径"),
    (r"SNR_FAILED|snr.*fail|NoiseWeightModelV1 失败|NOISE_MODEL_STATUS", "E700", "SNR_FAILED",
     "SNR/噪声模型失败 (退化帧)", "检查 psf 块、sigma_residual、gain/readnoise 元数据"),
    (r"DRIZZLE_FAILED|drizzle.{0,120}(fail|error)|Drizzle 失败|pixfrac.{0,80}(非法|invalid|拒绝)|nside.{0,80}(非法|invalid|拒绝)", "E800", "DRIZZLE_FAILED",
     "Drizzle 失败 (WCS/SIP 非法或几何错误)", "检查 WCS 关键字、pixfrac∈(0,1]、nside 2 的幂"),
    (r"HIPS_WRITE_FAILED|hips.*write failed|aio_hips", "E900", "HIPS_WRITE_FAILED",
     "HiPS 直写失败 (CFITSIO/产品结构)", "检查输出目录可写、tile_depth=9、nside>=512"),
    (r"HIPS_VERIFY_FAILED|verify failed|hips verify.*fail", "E910", "HIPS_VERIFY_FAILED",
     "HiPS 验证失败 (tile/MOC/properties 语义)", "用 aio_hips_reader 检查 signal/support/variance 产品"),
    (r"STAGE2 FAILED|stage2.{0,120}fail|p2_|REJECTION FAILED", "E920", "STAGE2_FAILED",
     "Phase2 失败 (coverage/UPM/rejection/integrate)", "检查 stage2 JSON 与 gate 日志; 用 rejection_cli 复现"),
    (r"CONFIG_ERROR|非法.*配置|unsupported.*weight_mode|config.*invalid", "E950", "CONFIG_ERROR",
     "配置非法 (schema/枚举/组合)", "对照 docs/CONFIG_REFERENCE.md 与 configs/ 模板"),
    (r"FILE_IO_ERROR|无法.*文件|failed to open|cannot open|file not found", "E960", "FILE_IO_ERROR",
     "文件 IO 失败 (路径/权限/损坏)", "检查路径大小写、磁盘空间、文件完整性 (sha256)"),
    (r"timed out|timeout|超时", "E980", "TIMEOUT",
     "阶段超时", "检查硬门耗时; 用 ASTROCS_DRIZZLE_FINE_PROFILE=1 定位热点"),
]


def scan_logs(run_dir: str, timeout: int) -> list[dict]:
    """扫描 *.log 文件, 提取错误 evidence (每文件限前 N 行, 不读大文件)。"""
    findings: list[dict] = []
    n_files = 0
    for root, _dirs, files in os.walk(run_dir):
        for fn in files:
            if not fn.endswith(".log"):
                continue
            if n_files >= 400:
                findings.append({"file": "(limit)", "note": "已达 400 个日志文件上限, 其余跳过"})
                return findings
            n_files += 1
            path = os.path.join(root, fn)
            try:
                size = os.path.getsize(path)
                if size > 2 * 1024 * 1024:
                    findings.append({
                        "file": path, "size": size,
                        "note": "log > 2MiB, 只扫尾部 2000 行"})
                    with open(path, "r", encoding="utf-8", errors="replace") as f:
                        tail = f.readlines()[-2000:]
                    hits = []
                    for i, line in enumerate(tail):
                        low = line.lower()
                        if "passed" in low or "pass " in low or " ok " in low:
                            continue
                        for pat, code, name, cause, fix in ERROR_MAP:
                            if re.search(pat, low):
                                hits.append({
                                    "line": i + 1, "text": line.strip()[:240],
                                    "code": code, "name": name,
                                    "likely_cause": cause, "command": fix})
                                break
                        if len(hits) >= 5:
                            break
                    if hits:
                        findings.append({"file": path, "hits": hits})
                    continue
                with open(path, "r", encoding="utf-8", errors="replace") as f:
                    lines = f.readlines()
            except OSError as e:
                findings.append({"file": path, "error": str(e)})
                continue
            hits = []
            for i, line in enumerate(lines):
                low = line.lower()
                if "passed" in low or "pass " in low or " ok " in low:
                    continue
                for pat, code, name, cause, fix in ERROR_MAP:
                    if re.search(pat, low):
                        hits.append({
                            "line": i + 1, "text": line.strip()[:240],
                            "code": code, "name": name,
                            "likely_cause": cause, "command": fix})
                        break
                if len(hits) >= 5:
                    break
            if hits:
                findings.append({"file": path, "hits": hits})
    # 全局上限: 最多 200 个文件命中, 避免大仓库扫描噪音
    if len(findings) > 200:
        findings = findings[:200]
    return findings


def scan_artifacts(run_dir: str) -> dict:
    """扫描 stage 配置/产物清单 (只列文件, 不读 FITS/HiPS 大文件)。"""
    artifacts = {"configs": [], "products": []}
    for root, _dirs, files in os.walk(run_dir):
        for fn in files:
            low = fn.lower()
            p = os.path.join(root, fn)
            if low.endswith((".json", ".yaml", ".yml")):
                artifacts["configs"].append(p)
            elif low.endswith((".hips", ".fits", ".fz", ".xisf", ".fts")):
                size = os.path.getsize(p) if os.path.exists(p) else -1
                artifacts["products"].append({"path": p, "size": size})
    return artifacts


def run_command(cmd: list[str], timeout: int) -> str:
    """带 timeout 的外部命令执行 (默认不调用)。"""
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=timeout)
        return r.stdout[-4000:] + r.stderr[-4000:]
    except subprocess.TimeoutExpired:
        return f"command timed out after {timeout}s"
    except OSError as e:
        return f"cannot run: {e}"


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="AstroCS 轻量故障定位")
    ap.add_argument("run_dir", help="run/ 目录 (或任意日志目录)")
    ap.add_argument("--json", help="输出机器可读 JSON 路径")
    ap.add_argument("--timeout", type=int, default=30,
                    help="外部命令超时秒数 (默认 30)")
    ap.add_argument("--exec", help="可选: 额外执行的外部命令 (空格分隔)")
    args = ap.parse_args()

    if not os.path.isdir(args.run_dir):
        print(f"错误: 目录不存在: {args.run_dir}", file=sys.stderr)
        return 2

    t0 = time.time()
    findings = scan_logs(args.run_dir, args.timeout)
    artifacts = scan_artifacts(args.run_dir)
    report = {
        "tool": "astrocs-diagnose",
        "version": "1.0.0",
        "run_dir": os.path.abspath(args.run_dir),
        "stage_ids": STAGE_IDS,
        "scan_elapsed_sec": round(time.time() - t0, 3),
        "findings": findings,
        "artifacts": artifacts,
        "n_errors": sum(len(f.get("hits", [])) for f in findings),
    }
    if args.exec:
        report["exec_output"] = run_command(args.exec.split(), args.timeout)

    # 人类可读输出
    print(f"=== AstroCS Diagnose: {args.run_dir} ===")
    print(f"stage ids: {', '.join(STAGE_IDS)}")
    n_err = 0
    for f in findings:
        if "error" in f:
            print(f"[IO] {f['file']}: {f['error']}")
            continue
        if "note" in f:
            print(f"[SKIP] {f['file']}: {f['note']}")
            continue
        print(f"\n[{f['file']}]")
        for h in f.get("hits", []):
            n_err += 1
            print(f"  {h['code']} {h['name']} @line {h['line']}: {h['text']}")
            print(f"    cause: {h['likely_cause']}")
            print(f"    fix:   {h['command']}")
    print(f"\n总计: {n_err} 处错误信号, 扫描耗时 {report['scan_elapsed_sec']}s")
    print("提示: 默认不读大 FITS/HiPS; 详细排查见 docs/TROUBLESHOOTING.md")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
        print(f"JSON 报告: {args.json}")
    return 0 if n_err == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
