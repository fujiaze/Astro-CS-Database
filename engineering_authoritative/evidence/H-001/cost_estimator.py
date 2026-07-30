"""
H-001 阶段成本模型与动态成本估算器

为 Stage1 的 7 个阶段 (READ_FITS, CALIBRATE, PLATESOLVE, PSF, PHOTOMETRIC, SNR, DRIZZLE)
建立内存峰值/时长/CPU强度/IO强度的参数化成本模型, 基于 B-002 三帧统计校准。

规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
  FrameCostEstimator: 按图像尺寸、T1–T4、星点、Gaia数量、Nside估算阶段峰值

契约: engineering_authoritative/contracts/resource_profile.schema.json
  输出字段: frame_id, stage, predicted_peak_bytes, uncertainty_bytes,
           actual_peak_bytes, cpu_intensity, io_intensity
"""

from __future__ import annotations

import json
import os
import math
from dataclasses import dataclass, field, asdict
from typing import Optional


# ============================================================================
# Stage1 阶段名称常量
# ============================================================================

STAGE_READ_FITS = "READ_FITS"
STAGE_CALIBRATE = "CALIBRATE"
STAGE_PLATESOLVE = "PLATESOLVE"
STAGE_PSF = "PSF"
STAGE_PHOTOMETRIC = "PHOTOMETRIC"
STAGE_SNR = "SNR"
STAGE_DRIZZLE = "DRIZZLE"

ALL_STAGES = [
    STAGE_READ_FITS,
    STAGE_CALIBRATE,
    STAGE_PLATESOLVE,
    STAGE_PSF,
    STAGE_PHOTOMETRIC,
    STAGE_SNR,
    STAGE_DRIZZLE,
]

# 高内存阶段 (需预约, H-002 使用)
HIGH_MEMORY_STAGES = {STAGE_PLATESOLVE, STAGE_DRIZZLE}

# 高 CPU 阶段
HIGH_CPU_STAGES = {STAGE_CALIBRATE, STAGE_PLATESOLVE, STAGE_DRIZZLE}

# 高 I/O 阶段
HIGH_IO_STAGES = {STAGE_READ_FITS, STAGE_DRIZZLE}


# ============================================================================
# 帧参数 (成本估算输入)
# ============================================================================

@dataclass
class FrameParams:
    """帧参数: 成本估算器的输入"""
    frame_id: str
    image_w: int                      # 图像宽度 (像素)
    image_h: int                      # 图像高度 (像素)
    n_stars: int = 0                  # 预估星点数 (0=未知, 用默认值)
    n_gaia: int = 0                   # Gaia 星表帧内星数 (0=未知)
    nside: int = 0                    # DRIZZLE 输出 nside (0=自适应)
    pixel_scale_arcsec: float = 1.0   # 像素尺度 (角秒/像素)
    is_wide_field: bool = False       # 是否宽场 (pixel_scale > 3.0)

    @property
    def n_pixels(self) -> int:
        return self.image_w * self.image_h

    @property
    def image_buffer_bytes(self) -> int:
        """单张 float32 图像缓冲区大小"""
        return self.n_pixels * 4


# ============================================================================
# 阶段成本预测 (成本估算输出)
# ============================================================================

@dataclass
class StageCost:
    """单阶段成本预测结果"""
    stage: str
    predicted_peak_bytes: int       # 预测峰值内存 (字节)
    uncertainty_bytes: int          # 不确定度 (字节)
    predicted_duration_sec: float   # 预测耗时 (秒)
    cpu_intensity: float            # CPU 强度 (0.0-1.0)
    io_intensity: float             # I/O 强度 (0.0-1.0)
    is_high_memory: bool            # 是否高内存阶段
    model_notes: str = ""           # 模型说明

    def to_dict(self) -> dict:
        return asdict(self)

    def to_profile(self, frame_id: str) -> dict:
        """转换为 resource_profile.schema.json 格式"""
        return {
            "frame_id": frame_id,
            "stage": self.stage,
            "predicted_peak_bytes": self.predicted_peak_bytes,
            "uncertainty_bytes": self.uncertainty_bytes,
            "actual_peak_bytes": None,
            "cpu_intensity": self.cpu_intensity,
            "io_intensity": self.io_intensity,
        }


@dataclass
class FrameCostEstimate:
    """整帧成本预测 (所有阶段)"""
    frame_id: str
    stages: dict  # stage_name -> StageCost
    total_predicted_peak_bytes: int       # 全帧峰值内存 (各阶段最大值)
    total_predicted_duration_sec: float   # 全帧预测总耗时
    worst_stage: str                      # 峰值内存最大的阶段
    uncertainty_bytes: int                # 全帧不确定度 (worst stage)

    def to_dict(self) -> dict:
        return {
            "frame_id": self.frame_id,
            "stages": {k: v.to_dict() for k, v in self.stages.items()},
            "total_predicted_peak_bytes": self.total_predicted_peak_bytes,
            "total_predicted_duration_sec": self.total_predicted_duration_sec,
            "worst_stage": self.worst_stage,
            "uncertainty_bytes": self.uncertainty_bytes,
        }


# ============================================================================
# 成本模型常量 (从 B-002 三帧校准)
# ============================================================================

class CostModelConstants:
    """
    B-002 校准的成本模型常量

    校准数据:
      T2: 4096x4096, 1949 stars, 1210 gaia, nside=2048, total=26.2s
      T3: 4096x4096,  981 stars,  311 gaia, nside=2048, total=24.4s
      T4: 4500x3600, 1984 stars, 6021 gaia, nside= 512, total=25.6s
    """

    # --- 内存模型 ---
    # 图像缓冲区: W*H*4 (float32)
    # HEALPix 地图: 12 * nside^2 * 4 (float32) per map, DRIZZLE 需 3 maps (data/weight/snr)

    # 各阶段内存乘数 (相对 image_buffer_bytes)
    MEM_READ_FITS_MULTIPLIER = 1.0        # 1x image
    MEM_READ_FITS_OVERHEAD = 65536        # header 等
    MEM_CALIBRATE_MULTIPLIER = 3.0        # input + output + master
    MEM_PLATESOLVE_MULTIPLIER = 1.0       # image
    MEM_PLATESOLVE_SOLVER_BUF = 50 * 1024 * 1024  # solver 内部 ~50MB
    MEM_PSF_MULTIPLIER = 1.0              # image
    MEM_PSF_RESULT_PER_STAR = 72          # 9 doubles per star
    MEM_PHOTOMETRIC_MULTIPLIER = 1.0      # image
    MEM_PHOTOMETRIC_SPECTRUM_BUF = 8 * 1024 * 1024  # spectrum ~8MB
    MEM_SNR_BASE = 2 * 1024 * 1024        # ~2MB
    MEM_DRIZZLE_NUM_MAPS = 3              # data/weight/snr
    MEM_DRIZZLE_IMAGE_MULTIPLIER = 1.0    # + image buffer

    # --- 时长模型 ---
    # READ_FITS: ~5 ns/px (含磁盘 I/O)
    DURATION_READ_FITS_NS_PER_PX = 5.0e-9
    # CALIBRATE: ~45 ns/px
    DURATION_CALIBRATE_NS_PER_PX = 45.0e-9
    # PLATESOLVE: base + b * n_gaia + wide_field_penalty
    DURATION_PLATESOLVE_BASE_SEC = 0.760
    DURATION_PLATESOLVE_PER_GAIA_SEC = 1.457e-4  # 0.146 ms/gaia
    DURATION_PLATESOLVE_WIDE_FIELD_PENALTY = 0.8  # 宽场额外耗时 (s)
    # PSF: ~0.15 ms/star
    DURATION_PSF_MS_PER_STAR = 0.15e-3
    # PHOTOMETRIC: ~0.16 ms/matched star (n_matched ≈ n_gaia * 0.9)
    DURATION_PHOTOMETRIC_MS_PER_MATCHED = 0.16e-3
    DURATION_PHOTOMETRIC_MATCH_RATIO = 0.9  # n_matched / n_gaia
    # SNR: ~2ms (常数)
    DURATION_SNR_SEC = 0.002
    # DRIZZLE: c_drizzle * n_pixels + c_hp * 12 * nside^2
    DURATION_DRIZZLE_NS_PER_SRC_PX = 1.312e-6    # 1.312 ns/源像素
    DURATION_DRIZZLE_NS_PER_HP_PX = 2.67e-8      # 0.0267 ns/HEALPix像素

    # --- 不确定度 ---
    # 各阶段预测的不确定度比例 (相对 predicted_peak_bytes)
    UNCERTAINTY_READ_FITS = 0.10       # 10%
    UNCERTAINTY_CALIBRATE = 0.15       # 15%
    UNCERTAINTY_PLATESOLVE = 0.30      # 30% (Gaia catalog 大小变化大)
    UNCERTAINTY_PSF = 0.15             # 15%
    UNCERTAINTY_PHOTOMETRIC = 0.20     # 20%
    UNCERTAINTY_SNR = 0.50             # 50% (但绝对值小)
    UNCERTAINTY_DRIZZLE = 0.20         # 20%

    # --- 强度指标 (0.0-1.0) ---
    INTENSITY = {
        STAGE_READ_FITS:    {"cpu": 0.2, "io": 0.9},
        STAGE_CALIBRATE:    {"cpu": 0.8, "io": 0.3},
        STAGE_PLATESOLVE:   {"cpu": 0.7, "io": 0.4},
        STAGE_PSF:          {"cpu": 0.6, "io": 0.1},
        STAGE_PHOTOMETRIC:  {"cpu": 0.5, "io": 0.2},
        STAGE_SNR:          {"cpu": 0.3, "io": 0.1},
        STAGE_DRIZZLE:      {"cpu": 0.9, "io": 0.8},
    }

    # --- 默认值 (帧参数缺失时) ---
    DEFAULT_N_STARS = 1500
    DEFAULT_NSIDE = 2048
    DEFAULT_NSIDE_WIDE_FIELD = 512


# ============================================================================
# FrameCostEstimator 核心类
# ============================================================================

class FrameCostEstimator:
    """
    动态成本估算器: 输入帧参数, 预测各阶段资源需求。

    用法:
        estimator = FrameCostEstimator()
        params = FrameParams(frame_id="T5", image_w=4096, image_h=4096,
                             n_stars=2000, n_gaia=1500, nside=2048)
        estimate = estimator.estimate(params)
        print(estimate.total_predicted_peak_bytes)
    """

    def __init__(self, constants: type = CostModelConstants):
        self.C = constants

    # ------------------------------------------------------------------
    # 单阶段成本估算
    # ------------------------------------------------------------------

    def estimate_read_fits(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        peak = int(img + self.C.MEM_READ_FITS_OVERHEAD)
        unc = int(peak * self.C.UNCERTAINTY_READ_FITS)
        dur = self.C.DURATION_READ_FITS_NS_PER_PX * p.n_pixels
        inten = self.C.INTENSITY[STAGE_READ_FITS]
        return StageCost(
            stage=STAGE_READ_FITS,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_READ_FITS in HIGH_MEMORY_STAGES,
            model_notes=f"W*H*4 + overhead; ~{dur:.3f}s",
        )

    def estimate_calibrate(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        peak = int(img * self.C.MEM_CALIBRATE_MULTIPLIER)
        unc = int(peak * self.C.UNCERTAINTY_CALIBRATE)
        dur = self.C.DURATION_CALIBRATE_NS_PER_PX * p.n_pixels
        inten = self.C.INTENSITY[STAGE_CALIBRATE]
        return StageCost(
            stage=STAGE_CALIBRATE,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_CALIBRATE in HIGH_MEMORY_STAGES,
            model_notes=f"3x image (input+output+master); ~{dur:.3f}s",
        )

    def estimate_platesolve(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        n_stars = p.n_stars or self.C.DEFAULT_N_STARS
        n_gaia = p.n_gaia or 0
        star_buf = n_stars * 16  # star_det: x,y,flux,mag (float32*4)
        gaia_buf = n_gaia * 24   # gaia_cat: ra,dec,mag (float64+float64+float32)
        peak = int(img + star_buf + gaia_buf + self.C.MEM_PLATESOLVE_SOLVER_BUF)
        unc = int(peak * self.C.UNCERTAINTY_PLATESOLVE)
        dur = self.C.DURATION_PLATESOLVE_BASE_SEC
        dur += self.C.DURATION_PLATESOLVE_PER_GAIA_SEC * n_gaia
        if p.is_wide_field:
            dur += self.C.DURATION_PLATESOLVE_WIDE_FIELD_PENALTY
        inten = self.C.INTENSITY[STAGE_PLATESOLVE]
        return StageCost(
            stage=STAGE_PLATESOLVE,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_PLATESOLVE in HIGH_MEMORY_STAGES,
            model_notes=f"image + {n_stars} stars + {n_gaia} gaia + solver; ~{dur:.3f}s",
        )

    def estimate_psf(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        n_stars = p.n_stars or self.C.DEFAULT_N_STARS
        result_buf = n_stars * self.C.MEM_PSF_RESULT_PER_STAR
        peak = int(img + result_buf)
        unc = int(peak * self.C.UNCERTAINTY_PSF)
        dur = self.C.DURATION_PSF_MS_PER_STAR * n_stars
        inten = self.C.INTENSITY[STAGE_PSF]
        return StageCost(
            stage=STAGE_PSF,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_PSF in HIGH_MEMORY_STAGES,
            model_notes=f"image + {n_stars} PSF results; ~{dur:.3f}s",
        )

    def estimate_photometric(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        n_gaia = p.n_gaia or 0
        n_matched = int(n_gaia * self.C.DURATION_PHOTOMETRIC_MATCH_RATIO)
        gaia_buf = n_gaia * 24
        spectrum_buf = self.C.MEM_PHOTOMETRIC_SPECTRUM_BUF
        peak = int(img + gaia_buf + spectrum_buf)
        unc = int(peak * self.C.UNCERTAINTY_PHOTOMETRIC)
        dur = self.C.DURATION_PHOTOMETRIC_MS_PER_MATCHED * n_matched
        inten = self.C.INTENSITY[STAGE_PHOTOMETRIC]
        return StageCost(
            stage=STAGE_PHOTOMETRIC,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_PHOTOMETRIC in HIGH_MEMORY_STAGES,
            model_notes=f"image + {n_gaia} gaia + spectrum; ~{dur:.3f}s",
        )

    def estimate_snr(self, p: FrameParams) -> StageCost:
        n_stars = p.n_stars or self.C.DEFAULT_N_STARS
        psf_buf = n_stars * self.C.MEM_PSF_RESULT_PER_STAR
        peak = int(self.C.MEM_SNR_BASE + psf_buf)
        unc = int(peak * self.C.UNCERTAINTY_SNR)
        dur = self.C.DURATION_SNR_SEC
        inten = self.C.INTENSITY[STAGE_SNR]
        return StageCost(
            stage=STAGE_SNR,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_SNR in HIGH_MEMORY_STAGES,
            model_notes=f"psf + control points; ~{dur:.3f}s",
        )

    def estimate_drizzle(self, p: FrameParams) -> StageCost:
        img = p.image_buffer_bytes
        nside = p.nside or (self.C.DEFAULT_NSIDE_WIDE_FIELD if p.is_wide_field else self.C.DEFAULT_NSIDE)
        n_hp = 12 * nside * nside
        hp_maps = n_hp * 4 * self.C.MEM_DRIZZLE_NUM_MAPS  # 3 maps * float32
        peak = int(hp_maps + img * self.C.MEM_DRIZZLE_IMAGE_MULTIPLIER)
        unc = int(peak * self.C.UNCERTAINTY_DRIZZLE)
        # 时长: 源像素扫描 + HEALPix 写入
        dur = (self.C.DURATION_DRIZZLE_NS_PER_SRC_PX * p.n_pixels
               + self.C.DURATION_DRIZZLE_NS_PER_HP_PX * n_hp)
        inten = self.C.INTENSITY[STAGE_DRIZZLE]
        return StageCost(
            stage=STAGE_DRIZZLE,
            predicted_peak_bytes=peak,
            uncertainty_bytes=unc,
            predicted_duration_sec=dur,
            cpu_intensity=inten["cpu"],
            io_intensity=inten["io"],
            is_high_memory=STAGE_DRIZZLE in HIGH_MEMORY_STAGES,
            model_notes=f"nside={nside}, {n_hp} HP px, 3 maps + image; ~{dur:.3f}s",
        )

    # ------------------------------------------------------------------
    # 全帧成本估算
    # ------------------------------------------------------------------

    def estimate(self, p: FrameParams) -> FrameCostEstimate:
        """估算帧的全部 7 个阶段成本"""
        stages = {}
        estimators = {
            STAGE_READ_FITS: self.estimate_read_fits,
            STAGE_CALIBRATE: self.estimate_calibrate,
            STAGE_PLATESOLVE: self.estimate_platesolve,
            STAGE_PSF: self.estimate_psf,
            STAGE_PHOTOMETRIC: self.estimate_photometric,
            STAGE_SNR: self.estimate_snr,
            STAGE_DRIZZLE: self.estimate_drizzle,
        }
        for stage_name in ALL_STAGES:
            stages[stage_name] = estimators[stage_name](p)

        # 全帧峰值 = 各阶段最大值 (串行执行, 不叠加)
        total_peak = max(s.predicted_peak_bytes for s in stages.values())
        worst_stage = max(stages, key=lambda s: stages[s].predicted_peak_bytes)
        total_dur = sum(s.predicted_duration_sec for s in stages.values())
        unc = stages[worst_stage].uncertainty_bytes

        return FrameCostEstimate(
            frame_id=p.frame_id,
            stages=stages,
            total_predicted_peak_bytes=total_peak,
            total_predicted_duration_sec=total_dur,
            worst_stage=worst_stage,
            uncertainty_bytes=unc,
        )

    # ------------------------------------------------------------------
    # 最坏下一帧预算 (H-002 准入控制使用)
    # ------------------------------------------------------------------

    def estimate_worst_next_frame(self, p: FrameParams) -> int:
        """
        估算最坏情况下一帧的内存需求 (用于准入控制的安全余量)。
        取 DRIZZLE 阶段 (通常最高内存) 的预测峰值 + 不确定度。
        """
        drizzle_cost = self.estimate_drizzle(p)
        return drizzle_cost.predicted_peak_bytes + drizzle_cost.uncertainty_bytes

    # ------------------------------------------------------------------
    # 基线验证 (用 B-002 数据验证模型)
    # ------------------------------------------------------------------

    def validate_against_baseline(self, baseline_path: str) -> dict:
        """
        用 B-002 基线数据验证成本模型, 输出各帧各阶段的预测 vs 实际对比。

        返回:
          {
            "frames": [
              {
                "frame_id": ...,
                "stages": [
                  {"stage":..., "predicted_sec":..., "actual_sec":..., "error_pct":...}
                ],
                "total_predicted_sec":..., "total_actual_sec":..., "total_error_pct":...
              }
            ],
            "max_stage_error_pct": ...,
            "max_total_error_pct": ...,
            "verdict": "PASS" | "FAIL"
          }
        """
        with open(baseline_path, "r", encoding="utf-8") as f:
            baseline = json.load(f)

        results = {"frames": [], "max_stage_error_pct": 0.0, "max_total_error_pct": 0.0}

        for frame_data in baseline["frames"]:
            w, h = frame_data["image_wxh"]
            nside = frame_data["drizzle_nside"]
            pixel_scale = frame_data["pixel_scale_arcsec_per_px"]
            is_wide = pixel_scale > 3.0

            params = FrameParams(
                frame_id=frame_data["frame_id"],
                image_w=w,
                image_h=h,
                n_stars=frame_data["n_stars_detected"],
                n_gaia=frame_data["n_gaia_in_frame"],
                nside=nside,
                pixel_scale_arcsec=pixel_scale,
                is_wide_field=is_wide,
            )

            estimate = self.estimate(params)
            actual_timings = frame_data["stage_timings_sec"]

            frame_result = {
                "frame_id": params.frame_id,
                "stages": [],
                "total_predicted_sec": 0.0,
                "total_actual_sec": 0.0,
            }

            for stage_name in ALL_STAGES:
                pred_sec = estimate.stages[stage_name].predicted_duration_sec
                actual_sec = actual_timings.get(stage_name, 0.0)
                if actual_sec > 0.001:
                    error_pct = abs(pred_sec - actual_sec) / actual_sec * 100
                else:
                    error_pct = 0.0
                frame_result["stages"].append({
                    "stage": stage_name,
                    "predicted_sec": round(pred_sec, 3),
                    "actual_sec": actual_sec,
                    "error_pct": round(error_pct, 1),
                })
                frame_result["total_predicted_sec"] += pred_sec
                frame_result["total_actual_sec"] += actual_sec
                results["max_stage_error_pct"] = max(results["max_stage_error_pct"], error_pct)

            total_error = abs(frame_result["total_predicted_sec"] - frame_result["total_actual_sec"]) / frame_result["total_actual_sec"] * 100
            frame_result["total_error_pct"] = round(total_error, 1)
            results["max_total_error_pct"] = max(results["max_total_error_pct"], total_error)
            frame_result["total_predicted_sec"] = round(frame_result["total_predicted_sec"], 3)
            frame_result["total_actual_sec"] = round(frame_result["total_actual_sec"], 3)
            results["frames"].append(frame_result)

        # 判定: 总耗时误差 < 15%, 单阶段误差 < 50% (SNR 除外, 因绝对值小)
        results["verdict"] = "PASS" if results["max_total_error_pct"] < 15.0 else "FAIL"
        return results


# ============================================================================
# 便捷函数
# ============================================================================

def estimate_frame(
    frame_id: str,
    image_w: int,
    image_h: int,
    n_stars: int = 0,
    n_gaia: int = 0,
    nside: int = 0,
    pixel_scale_arcsec: float = 1.0,
) -> FrameCostEstimate:
    """便捷函数: 一行调用估算帧成本"""
    is_wide = pixel_scale_arcsec > 3.0
    params = FrameParams(
        frame_id=frame_id,
        image_w=image_w,
        image_h=image_h,
        n_stars=n_stars,
        n_gaia=n_gaia,
        nside=nside,
        pixel_scale_arcsec=pixel_scale_arcsec,
        is_wide_field=is_wide,
    )
    return FrameCostEstimator().estimate(params)
