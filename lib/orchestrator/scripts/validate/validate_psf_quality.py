# -*- coding: utf-8 -*-
"""
validate_psf_quality.py - PSF 拟合质量验证脚本
功能: 验证 PSF 块的拟合质量
用途: 全链路调试阶段验证 PSF 拟合结果
调用: python validate_psf_quality.py <psf_json> [--output <output_json>]

PSF 块格式 (FLOAT64[N,9]):
    每行: [status, B, flux, cx, cy, fwhm, A, mad, eccentricity]
    - status: 0=成功, 非0=失败 (1=NO_CONVERGENCE, 2=INVALID_PARAMS, 3=ITERATION_LIMIT)
    - B: 背景值
    - flux: 流量
    - cx, cy: 星中心坐标
    - fwhm: 半高全宽 (像素) = (fwhm_x + fwhm_y) / 2
    - A: Moffat 振幅 (供 SNR 模块 §14 使用)
    - mad: 拟合残差 MAD (供 SNR 模块 §14 使用)
    - eccentricity: 椭率 (供 SNR 模块 §14 使用)

输入 JSON 格式 (由 run_pipeline_debug.py 导出):
    {
        "psf": [
            [0, 1.2, 12345.6, 100.5, 200.3, 3.5],
            ...
        ]
    }

验证项:
    1. 成功率 > 50%
    2. fwhm 中位数在 1-10 像素范围
    3. fwhm 标准差 < 中位数的 50%
    4. status=0 的星 flux > 0
    5. cx/cy 在合理范围 (>= 0 且有限)
    6. B (背景) 为有限值
    7. 无异常值 (fwhm > 20 或 flux < 0)

输出: JSON 到 output/pipeline_debug/<frame_name>/validate_psf.json
退出码: 0=全部通过, 非0=存在失败项
"""

from __future__ import annotations

import os
import sys
import json
import logging
import argparse
from datetime import datetime

import numpy as np


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "logs", "validate"
)
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "validate_psf_quality_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("validate_psf_quality")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("日志系统初始化完成，日志文件: %s", log_file)
    return lg


logger = _init_logger()


# ============================ PSF 状态码映射 ============================

PSF_STATUS_NAMES = {
    0: "OK",
    1: "NO_CONVERGENCE",
    2: "INVALID_PARAMS",
    3: "ITERATION_LIMIT",
}


# ============================ 核心验证逻辑 ============================

def load_psf_json(psf_json_path: str) -> np.ndarray:
    """加载 PSF JSON 文件并返回 [N,6] 数组

    参数:
        psf_json_path: PSF JSON 文件路径

    返回:
        np.ndarray: shape=(N,6), dtype=float64, 列=[status,B,flux,cx,cy,fwhm]

    异常:
        FileNotFoundError: 文件不存在
        ValueError: JSON 格式错误或 psf 字段缺失
    """
    logger.info("加载 PSF JSON: %s", psf_json_path)
    if not os.path.isfile(psf_json_path):
        raise FileNotFoundError(f"PSF JSON 文件不存在: {psf_json_path}")

    with open(psf_json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    if not isinstance(data, dict) or "psf" not in data:
        raise ValueError("JSON 缺少 'psf' 字段或顶层不是对象")

    psf_list = data["psf"]
    if not isinstance(psf_list, list):
        raise ValueError("'psf' 字段必须是列表")

    if len(psf_list) == 0:
        logger.warning("PSF 列表为空")
        return np.zeros((0, 9), dtype=np.float64)

    psf_array = np.asarray(psf_list, dtype=np.float64)
    if psf_array.ndim != 2 or psf_array.shape[1] != 9:
        raise ValueError(
            f"PSF 数组形状错误，期望 [N,9]（status,B,flux,cx,cy,fwhm,A,mad,eccentricity），实际 {psf_array.shape}"
        )

    logger.info("PSF 数据加载完成: %d 行, 形状=%s", psf_array.shape[0], psf_array.shape)
    return psf_array


def validate_psf(psf_array: np.ndarray) -> dict:
    """执行 PSF 质量验证

    参数:
        psf_array: shape=(N,9), 列=[status,B,flux,cx,cy,fwhm,A,mad,eccentricity]

    返回:
        dict: 验证结果（与输出 JSON 结构一致）
    """
    checks = []
    n_total = psf_array.shape[0]

    logger.info("=" * 60)
    logger.info("开始 PSF 质量验证 (共 %d 颗星)", n_total)
    logger.info("=" * 60)

    # 处理空数据特殊情况
    if n_total == 0:
        logger.error("PSF 数据为空，验证失败")
        checks.append({
            "name": "non_empty",
            "pass": False,
            "detail": "PSF 数据为空",
        })
        return {
            "psf_json": "",
            "overall_pass": False,
            "n_total": 0,
            "n_success": 0,
            "n_failed": 0,
            "success_rate": 0.0,
            "checks": checks,
            "statistics": {},
        }

    # 提取各列
    status_col = psf_array[:, 0]
    B_col = psf_array[:, 1]
    flux_col = psf_array[:, 2]
    cx_col = psf_array[:, 3]
    cy_col = psf_array[:, 4]
    fwhm_col = psf_array[:, 5]

    # ---- 基本统计 ----
    n_success = int(np.sum(status_col == 0))
    n_failed = n_total - n_success
    success_rate = n_success / n_total if n_total > 0 else 0.0

    logger.info("总数: %d, 成功: %d, 失败: %d, 成功率: %.2f%%",
                n_total, n_success, n_failed, success_rate * 100)

    # 状态分布日志
    status_counts = {}
    for s in np.unique(status_col.astype(int)):
        cnt = int(np.sum(status_col.astype(int) == s))
        name = PSF_STATUS_NAMES.get(s, f"UNKNOWN({s})")
        status_counts[int(s)] = cnt
        logger.info("  status=%d (%s): %d 颗", s, name, cnt)

    # ---- 仅对成功星 (status=0) 计算统计量 ----
    success_mask = (status_col == 0)
    fwhm_ok = fwhm_col[success_mask]
    flux_ok = flux_col[success_mask]
    B_ok = B_col[success_mask]

    statistics = {}
    if fwhm_ok.size > 0:
        statistics["fwhm_median"] = float(np.median(fwhm_ok))
        statistics["fwhm_std"] = float(np.std(fwhm_ok))
        statistics["fwhm_min"] = float(np.min(fwhm_ok))
        statistics["fwhm_max"] = float(np.max(fwhm_ok))
        statistics["flux_median"] = float(np.median(flux_ok))
        statistics["flux_std"] = float(np.std(flux_ok))
        statistics["B_median"] = float(np.median(B_ok))
        logger.info("FWHM 统计: median=%.4f, std=%.4f, min=%.4f, max=%.4f",
                    statistics["fwhm_median"], statistics["fwhm_std"],
                    statistics["fwhm_min"], statistics["fwhm_max"])
        logger.info("Flux 统计: median=%.4f, std=%.4f",
                    statistics["flux_median"], statistics["flux_std"])
        logger.info("B    统计: median=%.4f", statistics["B_median"])
    else:
        logger.warning("无成功星可用于统计计算")

    # ==================== 验证项 1: 成功率 > 50% ====================
    check_success_rate = success_rate > 0.5
    checks.append({
        "name": "success_rate",
        "pass": bool(check_success_rate),
        "detail": f"{n_success}/{n_total} = {success_rate*100:.1f}% "
                  f"{'>' if check_success_rate else '<='} 50%",
    })
    logger.info("[检查1] success_rate: %s (%d/%d = %.1f%% %s 50%%)",
                "PASS" if check_success_rate else "FAIL",
                n_success, n_total, success_rate * 100,
                ">" if check_success_rate else "<=")

    # ==================== 验证项 2: fwhm 中位数在 1-10 像素范围 ====================
    if fwhm_ok.size > 0:
        fwhm_median = statistics["fwhm_median"]
        check_fwhm_median = 1.0 <= fwhm_median <= 10.0
        checks.append({
            "name": "fwhm_median",
            "pass": bool(check_fwhm_median),
            "detail": f"median={fwhm_median:.4f} px (1-10 range)",
        })
        logger.info("[检查2] fwhm_median: %s (median=%.4f px, 期望 1-10)",
                    "PASS" if check_fwhm_median else "FAIL", fwhm_median)
    else:
        checks.append({
            "name": "fwhm_median",
            "pass": False,
            "detail": "无成功星，无法计算 fwhm 中位数",
        })
        logger.error("[检查2] fwhm_median: FAIL (无成功星)")

    # ==================== 验证项 3: fwhm 标准差 < 中位数的 50% ====================
    if fwhm_ok.size > 0:
        fwhm_std = statistics["fwhm_std"]
        fwhm_median = statistics["fwhm_median"]
        threshold = fwhm_median * 0.5
        check_fwhm_std = fwhm_std < threshold
        checks.append({
            "name": "fwhm_std",
            "pass": bool(check_fwhm_std),
            "detail": f"std={fwhm_std:.4f} {'<' if check_fwhm_std else '>='} "
                      f"{threshold:.4f} (50% of median)",
        })
        logger.info("[检查3] fwhm_std: %s (std=%.4f %s %.4f = 50%% of median=%.4f)",
                    "PASS" if check_fwhm_std else "FAIL",
                    fwhm_std,
                    "<" if check_fwhm_std else ">=",
                    threshold, fwhm_median)
    else:
        checks.append({
            "name": "fwhm_std",
            "pass": False,
            "detail": "无成功星，无法计算 fwhm 标准差",
        })
        logger.error("[检查3] fwhm_std: FAIL (无成功星)")

    # ==================== 验证项 4: status=0 的星 flux > 0 ====================
    if flux_ok.size > 0:
        n_flux_nonpos = int(np.sum(flux_ok <= 0))
        check_flux_pos = n_flux_nonpos == 0
        if check_flux_pos:
            detail = f"all {flux_ok.size} successful stars have flux>0"
        else:
            detail = (f"{n_flux_nonpos}/{flux_ok.size} successful stars "
                      f"have flux<=0")
        checks.append({
            "name": "flux_positive",
            "pass": bool(check_flux_pos),
            "detail": detail,
        })
        logger.info("[检查4] flux_positive: %s (%s)",
                    "PASS" if check_flux_pos else "FAIL", detail)
    else:
        checks.append({
            "name": "flux_positive",
            "pass": False,
            "detail": "无成功星，无法验证 flux>0",
        })
        logger.error("[检查4] flux_positive: FAIL (无成功星)")

    # ==================== 验证项 5: cx/cy 在合理范围 ====================
    # 检查所有星（含失败）的 cx/cy >= 0 且有限
    cx_finite = np.all(np.isfinite(cx_col))
    cy_finite = np.all(np.isfinite(cy_col))
    cx_nonneg = np.all(cx_col >= 0)
    cy_nonneg = np.all(cy_col >= 0)
    check_pos_range = cx_finite and cy_finite and cx_nonneg and cy_nonneg

    cx_min = float(np.min(cx_col)) if n_total > 0 else 0.0
    cx_max = float(np.max(cx_col)) if n_total > 0 else 0.0
    cy_min = float(np.min(cy_col)) if n_total > 0 else 0.0
    cy_max = float(np.max(cy_col)) if n_total > 0 else 0.0

    pos_detail = (f"cx in [{cx_min:.2f}, {cx_max:.2f}], "
                  f"cy in [{cy_min:.2f}, {cy_max:.2f}]")
    checks.append({
        "name": "position_range",
        "pass": bool(check_pos_range),
        "detail": pos_detail,
    })
    logger.info("[检查5] position_range: %s (%s)",
                "PASS" if check_pos_range else "FAIL", pos_detail)
    if not check_pos_range:
        n_cx_inf = int(np.sum(~np.isfinite(cx_col)))
        n_cy_inf = int(np.sum(~np.isfinite(cy_col)))
        n_cx_neg = int(np.sum(cx_col < 0))
        n_cy_neg = int(np.sum(cy_col < 0))
        logger.error("  cx 非有限=%d, cy 非有限=%d, cx<0=%d, cy<0=%d",
                     n_cx_inf, n_cy_inf, n_cx_neg, n_cy_neg)

    # ==================== 验证项 6: B (背景) 为有限值 ====================
    B_finite = np.all(np.isfinite(B_col))
    checks.append({
        "name": "background_finite",
        "pass": bool(B_finite),
        "detail": "all B values finite" if B_finite
                  else f"{int(np.sum(~np.isfinite(B_col)))}/{n_total} B values non-finite",
    })
    logger.info("[检查6] background_finite: %s",
                "PASS" if B_finite else "FAIL")

    # ==================== 验证项 7: 异常值检查 ====================
    # 异常: fwhm > 20 或 flux < 0 （对所有星检查）
    n_fwhm_outlier = int(np.sum(fwhm_col > 20.0))
    n_flux_neg = int(np.sum(flux_col < 0.0))
    n_outliers = n_fwhm_outlier + n_flux_neg
    check_outliers = n_outliers == 0
    outlier_detail = (f"{n_outliers} outliers "
                      f"(fwhm>20: {n_fwhm_outlier}, flux<0: {n_flux_neg})")
    checks.append({
        "name": "outliers",
        "pass": bool(check_outliers),
        "detail": outlier_detail,
    })
    logger.info("[检查7] outliers: %s (%s)",
                "PASS" if check_outliers else "FAIL", outlier_detail)

    # ==================== 汇总 ====================
    overall_pass = all(c["pass"] for c in checks)
    logger.info("=" * 60)
    logger.info("验证汇总: overall_pass=%s", overall_pass)
    for c in checks:
        logger.info("  [%s] %s: %s",
                    "PASS" if c["pass"] else "FAIL", c["name"], c["detail"])
    logger.info("=" * 60)

    return {
        "psf_json": "",
        "overall_pass": bool(overall_pass),
        "n_total": int(n_total),
        "n_success": int(n_success),
        "n_failed": int(n_failed),
        "success_rate": float(success_rate),
        "checks": checks,
        "statistics": statistics,
    }


def derive_output_path(psf_json_path: str, output_arg: str | None = None) -> str:
    """推导输出 JSON 路径

    规则:
        - 若 output_arg 指定，使用 output_arg
        - 否则根据 psf_json 文件名推导 frame_name，输出到
          output/pipeline_debug/<frame_name>/validate_psf.json

    frame_name 推导:
        从 psf_json 文件名中去除扩展名，并去除常见后缀（如 _psf_fit、_psf）。
        例如: "3_psf_fit.json" -> frame_name="3"
              "frameA_psf.json" -> frame_name="frameA"
    """
    if output_arg:
        return output_arg

    psf_json_path = os.path.normpath(psf_json_path)
    fname = os.path.basename(psf_json_path)
    name_no_ext = os.path.splitext(fname)[0]

    # 去除常见后缀
    for suffix in ("_psf_fit", "_psf", ".psf"):
        if name_no_ext.endswith(suffix):
            name_no_ext = name_no_ext[: -len(suffix)]
            break

    frame_name = name_no_ext if name_no_ext else "unknown"

    project_root = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..")
    )
    output_path = os.path.join(
        project_root, "output", "pipeline_debug", frame_name, "validate_psf.json"
    )
    return output_path


def main(argv: list[str] | None = None) -> int:
    """主入口

    返回:
        0 = 全部验证通过
        1 = 存在验证失败项
        2 = 输入/运行时错误
    """
    parser = argparse.ArgumentParser(
        description="PSF 拟合质量验证脚本"
    )
    parser.add_argument(
        "psf_json",
        help="PSF 块 JSON 文件路径 (由 run_pipeline_debug.py 导出)"
    )
    parser.add_argument(
        "--output", "-o",
        default=None,
        help="输出 JSON 路径 (默认: output/pipeline_debug/<frame_name>/validate_psf.json)"
    )
    args = parser.parse_args(argv)

    try:
        # 加载数据
        psf_array = load_psf_json(args.psf_json)

        # 执行验证
        result = validate_psf(psf_array)
        result["psf_json"] = os.path.normpath(args.psf_json)

        # 推导输出路径并写入
        output_path = derive_output_path(args.psf_json, args.output)
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=4)
        logger.info("验证结果已写入: %s", output_path)

        # 打印关键结果到 stdout
        print(json.dumps({
            "output_path": output_path,
            "overall_pass": result["overall_pass"],
            "n_total": result["n_total"],
            "n_success": result["n_success"],
            "success_rate": result["success_rate"],
        }, ensure_ascii=False, indent=2))

        return 0 if result["overall_pass"] else 1

    except (FileNotFoundError, ValueError, json.JSONDecodeError) as e:
        logger.error("输入错误: %s", e, exc_info=True)
        return 2
    except Exception as e:
        logger.error("运行时错误: %s", e, exc_info=True)
        return 2


if __name__ == "__main__":
    sys.exit(main())
