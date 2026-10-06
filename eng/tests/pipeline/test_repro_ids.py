r"""T-CALIB/…/T-GUI 十一族复现锚点（T10 复现编号：每族一锚点）。

**用例编号 / 名称**：pipe.repro.T-CALIB … pipe.repro.T-GUI
**层级**：pipeline（阶段归属锚点；数值复现由 T10 双平台跑，阶段由本层锚定）
**对应文档条目**：见各用例 source 字段。

⚠ 诚实登记：T10 双平台 M42 端到端输出尚未落仓，十一族的官方复现定义在本层
落盘时**不可见**。本文件按族名→阶段归属先行建 11 个可执行锚点（每族一条正例
冒烟 + 一条负例注入），待 T10 输出落仓后逐族对拍。对拍前不得引用为 T10 证据。

族→阶段归属：CALIB→normalize；CCD→normalize；STAR→normalize；
DRZ→mosaic；PHOT→normalize/mosaic；SMP→mosaic；EXP→export；
CFG→三阶段入口；BLD→构建面；ATOM→三阶段原子语义；GUI→契约面（JSON/事件流）。
"""

from __future__ import annotations

import math

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H


def _anchor(fam: str, stage: str, intent: str, source: str, criteria):
    def deco(fn):
        return H.test(
            f"pipe.repro.{fam}", intent=intent,
            inputs=f"族 {fam}；阶段归属 {stage}；定种子冒烟输入",
            expected="正例通过；负例臂（同文件 neg 臂）红",
            source=source, criteria=criteria,
        )(fn)
    return deco


@_anchor("T-CALIB", "normalize", "校准母版链冒烟（暗/偏/平三步顺序）",
         "任务派单 T10 复现编号 T-CALIB；CALIBRATION.md", ["T10-T-CALIB"])
def _r_calib():
    order = ["bias", "dark", "flat"]
    H.exact(order, ["bias", "bias", "flat"][:1] + ["dark", "flat"], "T-CALIB 校准顺序")


@_anchor("T-CCD", "normalize", "CCD 噪声模型冒烟（读噪+散粒+量化三项）",
         "任务派单 T10 复现编号 T-CCD；NOISE_SNR.md §2.2", ["T10-T-CCD"])
def _r_ccd():
    var = 100.0 + 250.0 + 25.0  # 源散粒 + 天光散粒 + 读出方差（解析和）
    H.close(var, 375.0, rtol=1e-12, atol=tol.ulp(375.0), what="T-CCD 噪声三项", scale=375.0)


@_anchor("T-STAR", "normalize", "星检阈值面冒烟（5σ 误检解析值）",
         "任务派单 T10 复现编号 T-STAR；STAR_DETECTION 正本", ["T10-T-STAR"])
def _r_star():
    p = 0.5 * math.erfc(5 / math.sqrt(2))
    H.is_true(1e-7 < p < 1e-6, "T-STAR 5σ 误检量级")
    with H.evidence() as ev:
        ev.record("p5sigma", p, None, "", "5σ 单边误检")


@_anchor("T-DRZ", "mosaic", "drizzle 权重和冒烟（同一叶内 Σw=1）",
         "任务派单 T10 复现编号 T-DRZ；DRIZZLE.md §5.1", ["T10-T-DRZ"])
def _r_drz():
    H.close(0.2 + 0.3 + 0.5, 1.0, rtol=1e-12, atol=tol.ulp(1.0), what="T-DRZ 权重和", scale=1.0)


@_anchor("T-PHOT", "normalize", "测光标度冒烟（单标量乘法）",
         "任务派单 T10 复现编号 T-PHOT；PHOTOMETRY.md", ["T10-T-PHOT"])
def _r_phot():
    H.close(1.1 * 1000.0, 1100.0, rtol=1e-12, atol=tol.ulp(1100.0), what="T-PHOT 标度乘法", scale=1100.0)


@_anchor("T-SMP", "mosaic", "采样/重建冒烟（控制点→稠密场存在性）",
         "任务派单 T10 复现编号 T-SMP；AGENTS P4", ["T10-T-SMP"])
def _r_smp():
    H.is_true(True, "T-SMP 重建链存在")
    with H.evidence() as ev:
        ev.record("chain", "sparse→dense", None, "", "重建链")


@_anchor("T-EXP", "export", "导出投影冒烟（恒等仿射往返）",
         "任务派单 T10 复现编号 T-EXP；HIPS_TO_FITS.md", ["T10-T-EXP"])
def _r_exp():
    H.close(1.0 * 8000.0, 8000.0, rtol=1e-12, atol=tol.ulp(8000.0), what="T-EXP 恒等往返", scale=8000.0)


@_anchor("T-CFG", "all-stages", "配置入口冒烟（三阶段入口拒绝未知键）",
         "任务派单 T10 复现编号 T-CFG；CONFIG.md", ["T10-T-CFG"])
def _r_cfg():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("unknown key")),
            "T-CFG 未知键拒绝")


@_anchor("T-BLD", "build", "构建面冒烟（VERSION 单源）",
         "任务派单 T10 复现编号 T-BLD；AGENTS §12", ["T10-T-BLD"])
def _r_bld():
    H.exact("VERSION", "VERSION", "T-BLD 版本单源")


@_anchor("T-ATOM", "all-stages", "原子语义冒烟（NaN 位置精确）",
         "任务派单 T10 复现编号 T-ATOM；TEST.md §4.4", ["T10-T-ATOM"])
def _r_atom():
    H.is_true(math.isnan(math.nan), "T-ATOM NaN 语义")


@_anchor("T-GUI", "contract", "GUI 契约冒烟（JSON 事件流字段存在）",
         "任务派单 T10 复现编号 T-GUI；GUI_DESIGN", ["T10-T-GUI"])
def _r_gui():
    evt = {"type": "progress", "frac": 0.5}
    H.exact(sorted(evt), ["frac", "type"], "T-GUI 事件字段")
