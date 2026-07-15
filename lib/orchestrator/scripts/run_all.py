# -*- coding: utf-8 -*-
"""
Run_all 全链路整合测试编排器 (Galaxy_Center_T4)
功能: 串联 step1~step4 四个阶段，对 3 panel × 5 filter 共 15 帧马赛克测试数据
      执行「校准 -> plate solving -> 光谱积分 -> 梯度估算」全链路流水线，
      汇总每帧每阶段的成功/失败、耗时与关键指标，输出 summary.json。
用途: 一键回归测试整条 CS 归一化链路，定位失败帧与失败阶段，便于分析。
接口:
    python run_all.py --config test_config.json [--output-root <dir>] [--frame <id>]
依赖: Python 标准库 (os/sys/json/time/argparse/logging/subprocess/datetime);
      step1_calibrate.run (import 调用);
      step2_solve.run (import 调用);
      step3_integrate.py / step4_estimate.py (subprocess 调用)
调用示例:
    # 跑全部 15 帧
    python run_all.py --config test_config.json
    # 只跑单帧调试
    python run_all.py --config test_config.json --frame panel1_Red
    # 覆盖输出根目录
    python run_all.py --config test_config.json --output-root D:/results
注意:
    - step1/step2 通过 import 直接调用 run()；step3/step4 通过 subprocess 调用 CLI
    - step2 每次调用都会重新初始化 Gaia/StarDetector/IPVSolver 环境
    - 某阶段失败则跳过该帧后续阶段，并在 summary 中标记 skipped
    - 所有相对路径均基于 config.project_root 拼接为绝对路径
    - 日志同时输出到文件(UTF-8)与控制台，格式: [时间][级别][帧ID][阶段] 消息
"""

from __future__ import annotations

import os
import sys
import json
import time
import logging
import argparse
import subprocess
from datetime import datetime

# ============================ 路径常量 ============================

# 本脚本所在目录: .../lib/integration_test/python
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# 日志目录: .../lib/integration_test/logs
_LOG_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", "logs"))

# 确保 step1/step2 可被 import（脚本自身目录加入 sys.path）
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)


# ============================ 日志配置 ============================

_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] [%(frame_id)s] [%(stage)s] %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


class _ContextFilter(logging.Filter):
    """为日志记录注入默认 frame_id / stage 字段，保证格式字符串始终可用"""

    def filter(self, record):
        if not hasattr(record, "frame_id"):
            record.frame_id = "-"
        if not hasattr(record, "stage"):
            record.stage = "-"
        return True


def _init_logger() -> logging.Logger:
    """初始化编排器日志，同时输出到文件(UTF-8)和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "run_all_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)
    ctx = _ContextFilter()

    lg = logging.getLogger("run_all")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        fh.addFilter(ctx)
        lg.addHandler(fh)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        ch.addFilter(ctx)
        lg.addHandler(ch)

    lg.info("日志系统初始化完成，日志文件: %s", log_file)
    return lg


logger = _init_logger()


# ============================ 工具函数 ============================

def _resolve_path(project_root: str, p: str) -> str:
    """将相对路径基于 project_root 解析为绝对路径；绝对路径原样返回"""
    if not p:
        return p
    if os.path.isabs(p):
        return os.path.normpath(p)
    return os.path.normpath(os.path.join(project_root, p))


def _log(frame_id: str, stage: str, level: int, msg: str, *args, exc_info: bool = False):
    """带帧ID/阶段上下文的日志输出"""
    logger.log(level, msg, *args, exc_info=exc_info,
               extra={"frame_id": frame_id, "stage": stage})


def _parse_last_json(stdout: str):
    """解析 stdout 最后一行 JSON
    Returns:
        (dict, None) 或 (None, err_hint)
    """
    if not stdout:
        return None, "stdout 为空"
    lines = [ln.strip() for ln in stdout.splitlines() if ln.strip()]
    if not lines:
        return None, "stdout 无有效行"
    last = lines[-1]
    try:
        return json.loads(last), None
    except json.JSONDecodeError as e:
        return None, f"stdout 最后一行非合法 JSON: {e}; 行内容: {last[:200]}"


def _empty_stage(error: str) -> dict:
    """构造被跳过阶段的结果结构"""
    return {"success": False, "duration_sec": 0.0, "error": error, "metrics": {}}


# ============================ 阶段执行 ============================

def _stage_step1(frame, frame_dir, light_abs, calib_abs, frame_id):
    """阶段1: import 调用 step1_calibrate.run -> 01_calibrated.fits"""
    _log(frame_id, "step1", logging.INFO, "阶段1(校准)开始: light=%s", light_abs)
    t0 = time.time()
    try:
        from step1_calibrate import run as run_step1
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step1", logging.ERROR, "导入 step1_calibrate 失败: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"导入失败: {e}", "metrics": {}}

    try:
        result = run_step1(light_abs, frame_dir, calib_abs)
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step1", logging.ERROR, "run_step1 异常: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"run 异常: {e}", "metrics": {}}

    dur = time.time() - t0
    success = bool(result.get("success"))
    err = result.get("error", "") or ""
    metrics = result.get("stats", {}) or {}
    if success:
        _log(frame_id, "step1", logging.INFO,
             "阶段1(校准)成功: 耗时=%.2fs, 输出=%s", dur, result.get("output_path"))
    else:
        _log(frame_id, "step1", logging.ERROR, "阶段1(校准)失败: %s (耗时=%.2fs)", err, dur)
    return {"success": success, "duration_sec": round(dur, 2),
            "error": err, "metrics": metrics}


def _stage_step2(frame, frame_dir, calibrated_fits, cfg, frame_id):
    """阶段2: import 调用 step2_solve.run -> 02_wcs.json + 02_detected_stars.json"""
    wcs_path = os.path.join(frame_dir, "02_wcs.json")
    detected_path = os.path.join(frame_dir, "02_detected_stars.json")
    _log(frame_id, "step2", logging.INFO, "阶段2(解析)开始: image=%s", calibrated_fits)
    t0 = time.time()
    try:
        from step2_solve import run as run_step2
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step2", logging.ERROR, "导入 step2_solve 失败: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"导入失败: {e}", "metrics": {}}

    try:
        result = run_step2(
            image_path=calibrated_fits,
            ra0=float(frame.get("ra0", 0.0)),
            dec0=float(frame.get("dec0", 0.0)),
            focal_length=float(cfg.get("focal_length_mm", 200.0)),
            pixel_size=float(cfg.get("pixel_size_um", 6.0)),
            output_path=wcs_path,
            output_detected_path=detected_path,
        )
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step2", logging.ERROR, "run_step2 异常: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"run 异常: {e}", "metrics": {}}

    dur = time.time() - t0
    success = bool(result.get("success"))
    err = result.get("error", "") or ""
    metrics = {
        "rms_px": result.get("rms_px", 0.0),
        "rms_arcsec": result.get("rms_arcsec", 0.0),
        "n_pairs": result.get("n_pairs", 0),
        "fov_diag_deg": result.get("fov_diag_deg", 0.0),
        "n_detected": result.get("n_detected", 0),
        "n_selected": result.get("n_selected", 0),
    }
    if success:
        _log(frame_id, "step2", logging.INFO,
             "阶段2(解析)成功: 耗时=%.2fs, rms_px=%.4f, n_pairs=%d, fov=%.4f°",
             dur, metrics["rms_px"], metrics["n_pairs"], metrics["fov_diag_deg"])
    else:
        _log(frame_id, "step2", logging.ERROR, "阶段2(解析)失败: %s (耗时=%.2fs)", err, dur)
    return {"success": success, "duration_sec": round(dur, 2),
            "error": err, "metrics": metrics}


def _stage_step3(frame, frame_dir, detected_stars_path, cfg, frame_id):
    """阶段3: subprocess 调用 step3_integrate.py -> 03_fsyn.json"""
    fsyn_path = os.path.join(frame_dir, "03_fsyn.json")
    narrowband = frame.get("narrowband")

    cmd = [
        sys.executable, "step3_integrate.py",
        "--detected-stars", detected_stars_path,
        "--qe", str(cfg.get("qe_name", "")),
        "--gaia-data", cfg["_gaia_data_abs"],
        "--mag-low", str(cfg.get("mag_low", 8.0)),
        "--mag-high", str(cfg.get("mag_high", 16.0)),
        "--output", fsyn_path,
    ]
    if narrowband:
        cmd += [
            "--narrowband-center", str(narrowband["center_nm"]),
            "--narrowband-bw", str(narrowband["bandwidth_nm"]),
            "--narrowband-trans", str(narrowband["transmittance"]),
        ]
        mode = f"窄带(center={narrowband['center_nm']}nm, bw={narrowband['bandwidth_nm']}nm)"
    else:
        cmd += ["--filter-name", str(frame.get("filter_curve_name", ""))]
        mode = f"宽带(filter={frame.get('filter_curve_name')})"

    _log(frame_id, "step3", logging.INFO, "阶段3(积分)开始: %s, 输出=%s", mode, fsyn_path)
    _log(frame_id, "step3", logging.DEBUG, "subprocess 命令: %s", " ".join(cmd))
    t0 = time.time()
    try:
        proc = subprocess.run(
            cmd, cwd=_SCRIPT_DIR, capture_output=True,
            text=True, encoding="utf-8", errors="replace",
        )
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step3", logging.ERROR, "subprocess 启动异常: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"subprocess 异常: {e}", "metrics": {}}

    dur = time.time() - t0
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    data, parse_err = _parse_last_json(stdout)
    if data is None:
        err = parse_err + ("; stderr: " + stderr[-500:] if stderr else "")
        _log(frame_id, "step3", logging.ERROR, "阶段3(积分)输出解析失败: %s", err)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": err, "metrics": {}}

    success = bool(data.get("success"))
    err = data.get("error", "") or ""
    metrics = {
        "n_stars": data.get("n_stars", 0),
        "filter_name": data.get("filter_name", ""),
    }
    if success:
        _log(frame_id, "step3", logging.INFO,
             "阶段3(积分)成功: 耗时=%.2fs, n_stars=%d, filter=%s",
             dur, metrics["n_stars"], metrics["filter_name"])
    else:
        _log(frame_id, "step3", logging.ERROR, "阶段3(积分)失败: %s (耗时=%.2fs)", err, dur)
        if stderr:
            _log(frame_id, "step3", logging.DEBUG, "stderr 末尾: %s", stderr[-500:])
    return {"success": success, "duration_sec": round(dur, 2),
            "error": err, "metrics": metrics}


def _stage_step4(frame, frame_dir, calibrated_fits, fsyn_path, wcs_path, cfg, frame_id):
    """阶段4: subprocess 调用 step4_estimate.py -> 04_calibrated_final.fits + 质量报告"""
    final_fits = os.path.join(frame_dir, "04_calibrated_final.fits")
    report_path = os.path.join(frame_dir, "04_quality_report.json")

    cmd = [
        sys.executable, "step4_estimate.py",
        "--image", calibrated_fits,
        "--fsyn", fsyn_path,
        "--wcs", wcs_path,
        "--output", final_fits,
        "--report", report_path,
        "--match-radius", str(cfg.get("match_radius_px", 3.0)),
        "--outlier-sigma", str(cfg.get("outlier_sigma", 3.0)),
        "--max-order", str(cfg.get("max_order", 5)),
    ]

    _log(frame_id, "step4", logging.INFO, "阶段4(梯度估算)开始: 输出=%s", final_fits)
    _log(frame_id, "step4", logging.DEBUG, "subprocess 命令: %s", " ".join(cmd))
    t0 = time.time()
    try:
        proc = subprocess.run(
            cmd, cwd=_SCRIPT_DIR, capture_output=True,
            text=True, encoding="utf-8", errors="replace",
        )
    except Exception as e:
        dur = time.time() - t0
        _log(frame_id, "step4", logging.ERROR, "subprocess 启动异常: %s", e, exc_info=True)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": f"subprocess 异常: {e}", "metrics": {}}

    dur = time.time() - t0
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    data, parse_err = _parse_last_json(stdout)
    if data is None:
        err = parse_err + ("; stderr: " + stderr[-500:] if stderr else "")
        _log(frame_id, "step4", logging.ERROR, "阶段4(梯度估算)输出解析失败: %s", err)
        return {"success": False, "duration_sec": round(dur, 2),
                "error": err, "metrics": {}}

    success = bool(data.get("success"))
    err = data.get("error", "") or ""
    metrics = {
        "n_matched": data.get("n_matched", 0),
        "scale_factor": data.get("scale_factor", 0.0),
    }
    if success:
        _log(frame_id, "step4", logging.INFO,
             "阶段4(梯度估算)成功: 耗时=%.2fs, n_matched=%d, scale=%.6e",
             dur, metrics["n_matched"], metrics["scale_factor"])
    else:
        _log(frame_id, "step4", logging.ERROR, "阶段4(梯度估算)失败: %s (耗时=%.2fs)", err, dur)
        if stderr:
            _log(frame_id, "step4", logging.DEBUG, "stderr 末尾: %s", stderr[-500:])
    return {"success": success, "duration_sec": round(dur, 2),
            "error": err, "metrics": metrics}


# ============================ 单帧处理 ============================

def process_frame(frame, cfg, output_root, frame_id):
    """处理单帧: 依次执行 step1~step4，失败则跳过后续阶段

    Returns:
        dict: {step1_calibrate, step2_solve, step3_integrate, step4_estimate} 各阶段结果
    """
    _log(frame_id, "-", logging.INFO, "=" * 60)
    _log(frame_id, "-", logging.INFO, "开始处理帧: panel=%s, filter=%s",
         frame.get("panel"), frame.get("filter"))

    frame_dir = os.path.join(output_root, frame_id)
    os.makedirs(frame_dir, exist_ok=True)
    _log(frame_id, "-", logging.INFO, "帧输出目录: %s", frame_dir)

    light_abs = _resolve_path(cfg["_project_root"], frame.get("light_path", ""))
    calib_abs = cfg["_calib_abs"]

    stages = {}

    # ---- 阶段1: 校准 ----
    s1 = _stage_step1(frame, frame_dir, light_abs, calib_abs, frame_id)
    stages["step1_calibrate"] = s1
    if not s1["success"]:
        for name in ("step2_solve", "step3_integrate", "step4_estimate"):
            stages[name] = _empty_stage("skipped due to step1 failure")
        _log(frame_id, "-", logging.ERROR, "帧处理中止(step1 失败)")
        return stages

    calibrated_fits = os.path.join(frame_dir, "01_calibrated.fits")

    # ---- 阶段2: plate solving ----
    s2 = _stage_step2(frame, frame_dir, calibrated_fits, cfg, frame_id)
    stages["step2_solve"] = s2
    if not s2["success"]:
        for name in ("step3_integrate", "step4_estimate"):
            stages[name] = _empty_stage("skipped due to step2 failure")
        _log(frame_id, "-", logging.ERROR, "帧处理中止(step2 失败)")
        return stages

    wcs_path = os.path.join(frame_dir, "02_wcs.json")
    detected_path = os.path.join(frame_dir, "02_detected_stars.json")

    # ---- 阶段3: 光谱积分 ----
    s3 = _stage_step3(frame, frame_dir, detected_path, cfg, frame_id)
    stages["step3_integrate"] = s3
    if not s3["success"]:
        stages["step4_estimate"] = _empty_stage("skipped due to step3 failure")
        _log(frame_id, "-", logging.ERROR, "帧处理中止(step3 失败)")
        return stages

    # ---- 阶段4: 梯度估算 ----
    fsyn_path = os.path.join(frame_dir, "03_fsyn.json")
    s4 = _stage_step4(frame, frame_dir, calibrated_fits, fsyn_path, wcs_path, cfg, frame_id)
    stages["step4_estimate"] = s4

    _log(frame_id, "-", logging.INFO, "帧处理完成: step4 %s",
         "成功" if s4["success"] else "失败")
    return stages


# ============================ 主入口 ============================

def main():
    """命令行入口"""
    parser = argparse.ArgumentParser(
        description="Run_all 全链路整合测试编排器: 串联 step1~step4，输出 summary.json",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--config", required=True,
                        help="test_config.json 配置文件路径(必填)")
    parser.add_argument("--output-root", default=None,
                        help="输出根目录(可选, 覆盖 config 中的 output_root)")
    parser.add_argument("--frame", default=None,
                        help="只跑指定 id 的单帧(可选, 用于调试)")
    args = parser.parse_args()

    # Windows 控制台默认 GBK，强制 stdout UTF-8，避免中文日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    # 1. 读取配置
    _log("-", "-", logging.INFO, "读取配置文件: %s", args.config)
    if not os.path.isfile(args.config):
        _log("-", "-", logging.ERROR, "配置文件不存在: %s", args.config)
        return 1
    with open(args.config, "r", encoding="utf-8") as f:
        cfg = json.load(f)

    project_root = cfg.get("project_root", "")
    if not project_root or not os.path.isdir(project_root):
        _log("-", "-", logging.ERROR, "project_root 无效或不存在: %s", project_root)
        return 1

    # 预解析公共绝对路径，存入 cfg 供各阶段复用
    cfg["_project_root"] = project_root
    cfg["_gaia_data_abs"] = _resolve_path(project_root, cfg.get("gaia_data_dir", ""))
    cfg["_calib_abs"] = _resolve_path(project_root, cfg.get("calibration_dir", ""))

    # 输出根目录: CLI 覆盖 > config.output_root(基于 project_root)
    if args.output_root:
        output_root = os.path.abspath(args.output_root)
    else:
        output_root = _resolve_path(project_root, cfg.get("output_root", ""))
    os.makedirs(output_root, exist_ok=True)

    _log("-", "-", logging.INFO, "project_root=%s", project_root)
    _log("-", "-", logging.INFO, "gaia_data=%s", cfg["_gaia_data_abs"])
    _log("-", "-", logging.INFO, "calibration_dir=%s", cfg["_calib_abs"])
    _log("-", "-", logging.INFO, "output_root=%s", output_root)

    # 2. 筛选要处理的帧
    frames = cfg.get("frames", [])
    if args.frame:
        frames = [fr for fr in frames if fr.get("id") == args.frame]
        if not frames:
            _log("-", "-", logging.ERROR, "未找到匹配 id 的帧: %s", args.frame)
            return 1
    _log("-", "-", logging.INFO, "待处理帧数: %d", len(frames))

    # 3. 逐帧串行执行
    started_at = datetime.now()
    t_start = time.time()
    frame_results = {}
    success_count = 0

    for idx, frame in enumerate(frames, 1):
        frame_id = frame.get("id", f"frame_{idx}")
        _log(frame_id, "-", logging.INFO, "进度: %d/%d", idx, len(frames))
        try:
            stages = process_frame(frame, cfg, output_root, frame_id)
        except Exception as e:
            _log(frame_id, "-", logging.ERROR, "帧处理未捕获异常: %s", e, exc_info=True)
            stages = {
                "step1_calibrate": _empty_stage(f"未捕获异常: {e}"),
                "step2_solve": _empty_stage("skipped due to step1 failure"),
                "step3_integrate": _empty_stage("skipped due to step1 failure"),
                "step4_estimate": _empty_stage("skipped due to step1 failure"),
            }
        frame_results[frame_id] = stages
        if stages.get("step4_estimate", {}).get("success"):
            success_count += 1

    finished_at = datetime.now()
    total_dur = time.time() - t_start
    failed_count = len(frames) - success_count

    # 4. 输出 summary.json
    summary = {
        "started_at": started_at.strftime("%Y-%m-%d %H:%M:%S"),
        "finished_at": finished_at.strftime("%Y-%m-%d %H:%M:%S"),
        "total_duration_sec": round(total_dur, 2),
        "total_frames": len(frames),
        "success_frames": success_count,
        "failed_frames": failed_count,
        "frames": frame_results,
    }
    summary_path = os.path.join(output_root, "summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    _log("-", "-", logging.INFO, "=" * 60)
    _log("-", "-", logging.INFO,
         "全部帧处理完成: 成功=%d, 失败=%d, 总耗时=%.2fs",
         success_count, failed_count, total_dur)
    _log("-", "-", logging.INFO, "summary.json 已写入: %s", summary_path)

    return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
