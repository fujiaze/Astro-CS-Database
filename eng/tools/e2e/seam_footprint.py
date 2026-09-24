#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L4 接缝机器门：沿**真实帧足迹边界**的电平阶跃判据（能红能绿，内建正/负例）。

依据（逐条可核）：
  - ACCEPTANCE_SPEC.md §6.2「无接缝：**帧间**、块间无亮度/灰度阶跃」（帧间在前）；
  - ACCEPTANCE_SPEC.md §6.1（L4 产品生成链 = 整幅平面 FITS + 帧清单，两者都在手上）；
  - ASTROCS_DESIGN.md §2（SCI-C：接缝 = 帧集变化处的亮度阶跃）；
  - 实验/additive-sky-seamless/README.md:225（R5「相对接缝度量」< 1% = 1e-2）——
    本门阈值 1e-2 的唯一数值来源，不是另拍的；
  - AGENTS.md §9（判据必须能红能绿、配可执行正例/负例，而不是放松判据）。

为什么不是方差比（旧 V4 的盲区，必须写在工具里而不是只写在报告里）：
  eng/tools/e2e/render_vis.py::seam_metric（判据 V4）在**渲染脚本自切的 512 分块网格**上比
  「接缝处方差 / 块内方差」。电平阶跃 I → I + Δ **不改变方差** ⇒ 方差比对电平接缝**原理性失明**；
  且分块网格与帧足迹无关，真实帧间阶跃不落在 512 的整数倍行列上时完全不可见。
  实测（M42 整幅产品）：V4 = 0.9919 **判绿**，同一产品上本判据 |rel| max = 1.084e-1 **判红**。
  ⇒ V4 只作「渲染分块伪影」粗筛，不得再被引用为帧间接缝的证据
    （run/M42-E2E-01/pending-rulings.md R-2、run/VIS-P0FIX-01/REPORT.md 发现 4）。

判据（本工具 v2 = **纯电平阶跃**；v1 的 excess 口径已降级为诊断量）：
  seam(e)      = 沿第 e 条帧足迹边界、法向 ±d 的一阶差分（d 默认 2 px，**带符号**）
  ctrl(e)      = 同一条边界**法向平移 ctrl_shift 像素的平行线**上的同款差分（取有效样本多的一侧；
                 与 SCI-C c7 的 off-locus 对照同义）—— v2 里只用于**诊断**与适用域判定
  step(e)      = median(seam(e))                 **有符号**台阶（ADU）—— 判据分子
  bg(e)        = 边界自身 ±d 采样上 |电平| 的中位数（**局部**背景电平）
  rel_step(e)  = step(e) / bg(e)                 判据量（相对口径，无量纲，可跨产品比）
  ctrl_step(e) = median(ctrl(e))                 200 px 平行对照线上的同款有符号台阶
  step_net(e)  = step(e) − ctrl_step(e)          扣掉对照线后的"净台阶"——**诊断量**，不判红
  门：max_e |rel_step(e)| <= max_rel_excess（默认 1e-2）⇒ 绿（exit 0）；否则红（exit 1）。

为什么判据落在**边界自身的**有符号台阶上，而不是「台阶 − 200 px 外对照线」的净量：
  净量把「200 px 之外的结构」混进了判据。实测 M42 产品判据量最大的那条边
  （M2_T3_043524/bottom）：本边有符号台阶 = +7.69e-6（= 背景的 0.69%，**在门内**），
  而 200 px 外对照线自己的有符号台阶 = −1.13e-5（= 背景的 1.0%）⇒ 相减把它抬到 1.71e-2 判红。
  沿法向的中位电平剖面证明该处是**背景斜坡**而不是台阶：「台阶」随采样半距 d 近似线性增长
  （d = 2/4/8/16/32 px ⇒ 52/73/119/165/243 ×1e-6），而真正的电平跃变在 d 扫描下应当守恒。
  ⇒ 净量会把星云梯度/曲率误判成接缝，故降级为诊断量（run/SEAM-GATE-FIX-01/REPORT.md §2）。
  诚实边界：本判据仍含 2d·∂L/∂n 的梯度项（d 是采样半距）——背景梯度越陡，判据量越大；
  M42 产品实测最陡处已达门的 78%，更陡的星云/银道面场里需与 step_net、d 扫描一起判读。

为什么判据必须落在**有符号**台阶上（v1 假阳性模式的成因；实测证据见 run/VIS-E2E02-01/REPORT.md）：
  v1 判据 excess = median|seam| − median|ctrl| 对**噪声差**同样敏感（中位数绝对值随噪声尺度线性
  增长），所以它根本不是纯电平判据。实测 M42 整幅产品最差那条边（M6_T2_043619/top）：
  excess = 1.13e-5，而**有符号**台阶只有 +5.388e-6（占 48%）；同一条边 seam 侧 MAD = 2.273e-5，
  是对照线 8.193e-6 的 **2.77 倍** ⇒ excess 里一大半是噪声对比。全幅 196 条边的有符号台阶最大
  只有 0.219 个 8 位灰阶（交付 PNG 的量化步长 = 1 灰阶）⇒ v1 报的是图上**原理性看不见**的"缺陷"。
  v2 因此把判据落在**有符号**的 step 上（v1 的 excess、对照线净量、噪声差全部降级为
  **诊断量**：只报告、不判红），并逐边落盘供复核。

为什么本判据**没有** v1 的「周期即盲区」（独立审查 P0-7；本工具的负例自检 S8/S9 把它钉住）：
  v1 的判据量 excess = median|seam| − median|ctrl| **把对照线从判据里减掉了**，所以当场里存在
  与某条帧边界平行、间距整除 ctrl_shift 的同幅阶跃时，对照线自己踩在同幅阶跃上 ⇒ 两项精确
  相消 ⇒ 接缝判绿。解析条件：若 I(p + k·ctrl_shift·n) = I(p) + Δ（k 整数），则 ctrl 差分 ≡
  seam 差分 ⇒ excess ≡ 0，**与 Δ 大小无关**。
  实测（run/SEAM-PERIOD-BLIND-01：v1 用自写朴素实现复算、v2 逐字调用本工具）：
    P = 50 / 100 / 200（均整除 200）+ Δ = 200 ADU（= 背景 100 ADU 的 **200% 电平阶跃**）
    ⇒ v1 excess ≡ 0、整幅判绿；同式只把 P 改成 400 ⇒ v1 正常判红（Δ=5 ⇒ rel 5.0e-2 FAIL）。
  v2 的判据量 rel_step = median(img[+d] − img[−d]) / bg **不含对照线**（对照线只剩诊断量与
  适用域判定）⇒ 该退化族在结构上不存在。同一批夹具实测：P = 50/100/200/400 四档周期的
  rel_step **逐位相同**（200% 阶跃档 rel_step = ±1.0/±2.0，四档一字不差）⇒ 判据量与阶跃
  周期无关。
  ⇒ 由此也可见：审查给的处方「把 ctrl_shift 改成由输入几何自适应导出」**不是**这条缺陷的充分
  修法 —— 任何自适应偏移仍是一个具体的数，总存在整除它的周期 ⇒ 换个周期照样相消；根治只能
  把对照线移出判据（v2 已做）。故本工具**保留** ctrl_shift 常量（理由见「适用域」一节）。

判据的可检出下限（**诚实边界**；同时写进 --help 与 JSON 的 criterion.detection_floor）：
  判据是相对口径 ⇒ 允许的最大绝对台阶 = max_rel_excess × bg，bg = 边界两侧 |电平| 的中位数。
  对「下侧电平 L、阶跃 Δ」的边：step = Δ、bg = L + Δ/2 ⇒
      rel_step = Δ / (L + Δ/2) > max_rel_excess  ⟺  Δ/L > max_rel_excess / (1 − max_rel_excess/2)
  ⇒ 默认门 1e-2 时**可检出下限 = Δ/L 的 1.005%**（L = 边界**下侧**电平；≈ 0.0109 mag），
     更小的电平阶跃被放过。
  实测（run/SEAM-PERIOD-BLIND-01 门限扫描；合成夹具、无噪声）：Δ/L = 1.00% ⇒ PASS；
  1.01% ⇒ FAIL；5% / 20% / 200% ⇒ FAIL。**周期 200（旧盲区几何）与周期 400 的下限一致**。

判据的适用域（前置约束；与既有 min_samples 同类的工程处理，**不是**放松阈值）：
  只有「**两侧都在数据内部**」的帧边界才计入判据 —— 要求法向 ±ctrl_shift 处的平行线**两侧都能
  放置**、且各有 >= min_samples 个有效样本。两条理由：
    ① 物理定义：帧间电平接缝要求边界两侧都有数据；一侧没有数据 = 画幅/足迹**外缘**，那里的
       "台阶"是数据边缘本身，不是帧间接缝（实测 20 条超门边里 **15 条**贴着数据边界）；
    ② 对照线可放置域：一侧无数据时对照线只能放在另一侧，等于拿「边缘余量的高噪声」减「内部
       噪声」（实测最差那条距最近非有限像素仅 **1 px**、2181 px 只采到 65 个有效样本）。
  约束量 N = ctrl_shift（对照平行线的法向平移量，本工具**既有**的输入参数，默认 200 px）——
  **从输入导出**，不是另拍的整数常数；等价的逐边报告量是 margin_px（两面都放得下对照线的最大
  法向余量，阶梯扫描给出；margin_px >= ctrl_shift ⟺ 计入判据）。被排除的边界仍逐条落盘
  （exclude 字段 + 全部度量），只是不进判据 —— 不静默丢弃。

  为什么 v2 **不**把 ctrl_shift 改成「由输入几何自适应导出」（独立审查 P0-7 的处方；此处如实
  说明为何不采纳）：在 v2 里对照线已**不参与判红**，它只剩两个用途 ——
    ① 适用域：要求边界两侧各能放下一条 ±ctrl_shift 的平行线（"两侧都在数据内部"的可判定表述）；
    ② 诊断量：ctrl_step / step_net / noise_ratio / bg_ctrl。
  这两项都不与任何"周期"相消，所以 v1 的盲区机制在 v2 上无从复现（S8/S9 实测，见下）。
  反过来，把 ctrl_shift 变成随输入几何浮动的量会**引入**两个无依据的风险：
    ① 判据覆盖集（哪些边界计入）会随输入几何变化 ⇒ 同一产品在不同输入下判绿/判红的边界集
       不同，跨产品、跨版本不可比（适用域是"判据定义的一部分"，不是可调参数）；
    ② 自适应值只要等于某个周期 ⇒ 又落回 v1 的相消族（见「周期即盲区」一节），治不了病。
  故**保留常量 200 px**，并把"它的用途已降级"写在这里，而不是为了"看起来更自适应"引入浮动量。
  诚实边界：诊断量在"对照线自己踩在同幅阶跃上"时**不可读**（step_net 会 ≈0 而判据仍判红）；
  S8/S9 的夹具正是这种情形 —— 判读以 rel_step 为准，不看 step_net。

fail-closed（「无法判定」不得当「无接缝」）：
    · FITS / 帧清单 / 依赖不可用                ⇒ exit 2；
    · 有效帧边界数 == 0，或 rel_step 全部不可算 ⇒ **红**（exit 1）。

正/负例（--inject-frame k:amp，判别力自检；不依赖真值，靠已知注入）：
  把 amp 加进第 k 帧足迹多边形内部 = 沿该帧真实边界造一条**已知**电平阶跃，然后：
    · amp == 0 ⇒ 全部逐边度量必须与基线**一致**（真值无效应 ⇒ 必须回落到基线）；
    · amp != 0 ⇒ 该帧边界 max|有符号台阶| 必须 >= inject_detect_frac × |amp|（判据必须看得见），
                 同时打印旧 V4 方差比作对照（旧门对同一输入必须**不动** = 盲区复现）。
  任一不满足 ⇒ 该注入用例判红并计入 exit code。

--self-test（机器门常驻入口，不依赖 run/ 大产品）：合成夹具跑**同一代码路径**九组用例——
  S1 无台阶 ⇒ 绿；S2 注入已知台阶 ⇒ 红；S3 旧 V4 在 S2 输入上 ⇒ 绿（盲区复现）；
  S4 帧足迹落在画幅外 ⇒ 红（fail-closed）；S5 注入 0 ⇒ 与基线逐条一致且判绿（负例）；
  S6 **两侧噪声差 57× 但无电平台阶**的合成边 ⇒ 绿（v1 假阳性模式的负例；同一夹具上 v1 判据必须红）；
  S7 贴着数据边界（边缘余量高噪声）的边 ⇒ 被适用域排除、门判绿（同一夹具上 v1 判据必须红）；
  S8 **平行同幅阶跃、间距 = ctrl_shift（=200 px）、幅度 = 200% 局部电平**，且第一条阶跃正压在
     帧 0 的 top 边上 ⇒ **必须判红**，且同一夹具上 v1 口径必须**整幅判绿**（P0-7「周期即盲区」负例）；
  S9 同 S8 但间距 = ctrl_shift 的**因子**（=100 px）⇒ 同样必须判红、v1 必须判绿；
     两例都带对照臂 P = 2·ctrl_shift（=400，200 不整除它）⇒ v1 在该臂上必须**不**判绿（夹具非平凡），
     且三档周期的 rel_step 必须一致（判据量与周期无关）。
  REQUIRED_SELFTEST_CASES 硬校验用例名单：**缺任一条即自检失败**。

用法：
  python3 eng/tools/e2e/seam_footprint.py --fits <整幅 FITS> --p1-dirs <dir...> \
      --json-out run/ci/seam-footprint/l4.json [--max-rel-excess 1e-2] \
      [--inject-frame 0:0.05] [--inject-frame 0:0]
  python3 eng/tools/e2e/seam_footprint.py --self-test \
      --json-out run/ci/seam-footprint/selftest.json
exit 0 = 绿；1 = 红（超门接缝 / 正负例未命中）；2 = 输入/环境错误（fail-closed）。
"""
from __future__ import annotations

import argparse
import glob
import io
import json
import os
import shutil
import sys

import numpy as np
from astropy.io import fits
from astropy.wcs import Sip, WCS

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_WORK_DIR = os.path.join(REPO, "run/ci/seam-footprint")
EDGE_NAMES = ("bottom", "top", "left", "right")
FIXTURE_NPIX = 1200      # 自检夹具画幅边长（px）
FIXTURE_FRAME_N = 360    # 自检夹具帧像素边长
# 「对照线可放置域」阶梯（px，仅用于报告 margin_px；顶阶 = ctrl_shift，见 edge_metric）
MARGIN_LADDER = (0.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0)
# S6/S7 负例夹具（噪声差 / 边界余量）：画幅与帧边长翻倍 ⇒ 边上独立样本数翻倍，
# 中位数统计噪声减半，使「噪声差大但无台阶」的判绿有 >=2x 余量（确定性 seed，不靠运气）
FIXTURE_NPIX_BIG = 2400
FIXTURE_FRAME_N_BIG = 960
# S6/S7 负例夹具的噪声尺度（同一均值面、无电平台阶）：高/低噪声侧 σ。
# 取值使「v1 假阳性」系统性超出 1e-2 门（≈1.4e-2）而 v2 的判据量只剩采样噪声（≈1e-3 量级），
# 两边都有 >=2x 余量；夹具固定 seed ⇒ 逐位可复现，不靠运气。
SELF_SIGMA_HI = 4.0e-2
SELF_SIGMA_LO = 5e-4
# 自检必需用例名单（硬校验：缺任一条即自检失败，防止静默删用例）
REQUIRED_SELFTEST_CASES = (
    "S1-synthetic-no-step-green",
    "S2-injected-step-red",
    "S3-legacy-v4-blind-on-injected",
    "S4-degenerate-footprint-red",
    "S5-zero-injection-identity",
    "S6-noise-difference-no-step-green",
    "S7-boundary-margin-edge-excluded",
    "S8-periodic-step-on-edge-red",
    "S9-periodic-step-factor-on-edge-red",
)


# --------------------------------------------------------------------------- #
# 帧 WCS / 足迹
# --------------------------------------------------------------------------- #
def load_wcs_from_p1(path):
    """读 p1 阶段的帧 WCS 旁车（p1_wcs.json 的 wcs 块）。"""
    d = json.load(io.open(path, encoding="utf-8"))["wcs"]
    w = WCS(naxis=2)
    w.wcs.ctype = [d.get("ctype1", "RA---TAN"), d.get("ctype2", "DEC--TAN")]
    w.wcs.crval = [d["crval1"], d["crval2"]]
    w.wcs.crpix = [d["crpix1"], d["crpix2"]]
    w.wcs.cd = np.array([[d["cd11"], d["cd12"]], [d["cd21"], d["cd22"]]], dtype=float)
    sip = d.get("sip")
    if sip:
        def mat(key, order):
            return np.asarray(sip[key], dtype=float).reshape(6, 6)[:order + 1, :order + 1]
        o = int(sip.get("order", 3))
        ao = int(sip.get("ap_order", 5))
        w.sip = Sip(mat("a", o), mat("b", o), mat("ap", ao), mat("bp", ao),
                    [d["crpix1"], d["crpix2"]])
    return w, d


def frame_edges_p3(wframe, wp3, naxis, n_pts=256):
    """把帧的四条边界（帧像素坐标）投到产品网格，返回 [(name, Nx2 数组), ...]。

    顺序固定为 bottom(frame y=0) / top(y=ny-1) / left(x=0) / right(x=nx-1)，
    与 polygon_mask 的闭合顺序一致。
    """
    nx, ny = naxis
    t = np.linspace(0.0, 1.0, n_pts)
    specs = [(t * (nx - 1), np.zeros_like(t)), (t * (nx - 1), np.full_like(t, ny - 1)),
             (np.zeros_like(t), t * (ny - 1)), (np.full_like(t, nx - 1), t * (ny - 1))]
    out = []
    for nm, (xs, ys) in zip(EDGE_NAMES, specs):
        ra, dec = wframe.all_pix2world(xs, ys, 0)
        x3, y3 = wp3.all_world2pix(ra, dec, 0)
        out.append((nm, np.stack([x3, y3], axis=1)))
    return out


def wcs_from_header(hdr):
    """产品网格 WCS（从已打开的 FITS 头构造；供调用方复用已读入内存的产品数组）。"""
    w = WCS(naxis=2)
    w.wcs.ctype = [hdr["CTYPE1"], hdr["CTYPE2"]]
    w.wcs.crval = [hdr["CRVAL1"], hdr["CRVAL2"]]
    w.wcs.crpix = [hdr["CRPIX1"], hdr["CRPIX2"]]
    w.wcs.cd = np.array([[hdr["CD1_1"], hdr["CD1_2"]],
                         [hdr["CD2_1"], hdr["CD2_2"]]], float)
    return w


def product_wcs(path):
    with fits.open(path, memmap=True) as h:
        img = np.asarray(h[0].data, dtype=np.float32)
        hdr = h[0].header
    return img, wcs_from_header(hdr)


def collect_frames(p1_dirs):
    """按 --p1-dirs 顺序收集 <dir>/<frame>/p1_wcs.json，返回排序后的路径表。"""
    frames = []
    for d in p1_dirs:
        frames += sorted(glob.glob(os.path.join(d, "*", "p1_wcs.json")))
    return frames


# --------------------------------------------------------------------------- #
# 采样与度量
# --------------------------------------------------------------------------- #
def bilinear(img, x, y):
    h, w = img.shape
    ok = np.isfinite(x) & np.isfinite(y) & (x >= 1) & (y >= 1) & (x <= w - 2) & (y <= h - 2)
    xi = np.clip(x, 0, w - 2)
    yi = np.clip(y, 0, h - 2)
    x0 = np.floor(xi).astype(int)
    y0 = np.floor(yi).astype(int)
    fx = xi - x0
    fy = yi - y0
    v = (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy) +
         img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)
    return np.where(ok, v, np.nan)


def med_abs(x):
    x = np.asarray(x, float)
    x = x[np.isfinite(x)]
    return float(np.median(np.abs(x))) if x.size else None


def mad(x):
    x = np.asarray(x, float)
    x = x[np.isfinite(x)]
    if x.size < 3:
        return None
    m = np.median(x)
    return float(1.4826 * np.median(np.abs(x - m)))


def _ctrl_at(img, ex, ey, nxv, nyv, s, off, d):
    """法向 s·off 处、半距 ±d 的差分（= 该偏移上的一条平行对照线）。"""
    cp = bilinear(img, ex + nxv * (s * off + d), ey + nyv * (s * off + d))
    cm = bilinear(img, ex + nxv * (s * off - d), ey + nyv * (s * off - d))
    return cp - cm


def _placeable(seam_ok, c):
    """该偏移上「差分可算」的样本数（seam 与对照线都要有限）。"""
    return int((seam_ok & np.isfinite(c)).sum())


def _side_margin(img, ex, ey, nxv, nyv, seam_ok, s, ctrl_shift, d, min_samples):
    """一侧的「对照线可放置域」：满足 >= min_samples 个有效样本的最大法向偏移（px）。

    阶梯扫描（MARGIN_LADDER + 顶阶 ctrl_shift）；顶阶恰好是判据要求的偏移，因此
    margin_px >= ctrl_shift ⟺ 该侧计入判据（与 edge_metric 的 interior 同源同量）。
    """
    ladder = tuple(v for v in MARGIN_LADDER if v < ctrl_shift) + (float(ctrl_shift),)
    best = None
    for off in ladder:
        if _placeable(seam_ok, _ctrl_at(img, ex, ey, nxv, nyv, s, off, d)) >= min_samples:
            best = float(off)
    return best


def edge_metric(img, e, d=2.0, ctrl_shift=200.0, min_samples=20):
    """单条帧边界的电平阶跃判据量 + 噪声诊断 + 适用域判定（法向平移平行线对照）。

    判据量 = rel_step = median(seam) / bg（**有符号**电平台阶 / 边界处局部背景电平）；
    诊断量（**只报告，不判红**）= noise_ratio = MAD(seam)/MAD(ctrl)、noise_diff、
      ctrl_step / step_net（200 px 平行对照线口径）、step_d4x（d 扫描）、
      v1 的 excess / rel_excess（median|·| 之差，历史对照）；
    适用域 = interior（法向 ±ctrl_shift 两侧都能放对照线）—— 只有 interior 的边界进判据。
    """
    ex, ey = e[:, 0], e[:, 1]
    tx = np.gradient(ex)
    ty = np.gradient(ey)
    tn = np.hypot(tx, ty)
    tn[tn == 0] = 1.0
    tx, ty = tx / tn, ty / tn
    nxv, nyv = -ty, tx
    seam = bilinear(img, ex + nxv * d, ey + nyv * d) - bilinear(img, ex - nxv * d, ey - nyv * d)
    seam_ok = np.isfinite(seam)
    # 对照：法向 ±ctrl_shift 的平行线；选「有效样本多」的一侧（两侧同效时取 + 侧，确定性）
    ctrl_by_side, n_ok = {}, {}
    for s in (+1.0, -1.0):
        ctrl_by_side[s] = _ctrl_at(img, ex, ey, nxv, nyv, s, ctrl_shift, d)
        n_ok[s] = _placeable(seam_ok, ctrl_by_side[s])
    k = +1.0 if n_ok[+1.0] >= n_ok[-1.0] else -1.0
    ctrl = ctrl_by_side[k]
    m = seam_ok & np.isfinite(ctrl)
    n = int(m.sum())
    # 适用域（前置约束）：**两侧都在数据内部**才计入判据；被排除的边仍逐条落盘全部度量
    interior = (n_ok[+1.0] >= min_samples) and (n_ok[-1.0] >= min_samples)
    mg = [_side_margin(img, ex, ey, nxv, nyv, seam_ok, s, ctrl_shift, d, min_samples)
          for s in (+1.0, -1.0)]
    margin = None if any(v is None for v in mg) else float(min(mg))
    sv, cv = seam[m], ctrl[m]
    step = float(np.median(sv)) if sv.size else None              # 判据分子：有符号台阶
    ctrl_step = float(np.median(cv)) if cv.size else None         # 对照线上的同款量（诊断）
    step_net = (step - ctrl_step) if (step is not None and ctrl_step is not None) else None
    # 相对口径（SCI-C R5 同款）：台阶 / **局部背景电平**。
    # bg 取边界自身 ±d 采样的 |电平| 中位数（判据是纯局部量，不依赖 200 px 外的对照线）；
    # bg_ctrl 是 v1 的口径（对照线上的 |电平| 中位数），保留作诊断与历史对照。
    lvl = np.concatenate([np.abs(bilinear(img, ex + nxv * d, ey + nyv * d))[m],
                          np.abs(bilinear(img, ex - nxv * d, ey - nyv * d))[m]]) \
        if n else np.array([])
    lvl = lvl[np.isfinite(lvl)]
    bg = float(np.median(lvl)) if lvl.size else None
    bg_vals = np.abs(bilinear(img, ex + nxv * (k * ctrl_shift), ey + nyv * (k * ctrl_shift)))
    bg_vals = bg_vals[np.isfinite(bg_vals)]
    bg_ctrl = float(np.median(bg_vals)) if bg_vals.size else None
    # 诊断：d 扫描（真正的电平跃变在 d 扫描下守恒；背景梯度项 2d·∂L/∂n 随 d 线性增长）
    seam4 = (bilinear(img, ex + nxv * (4.0 * d), ey + nyv * (4.0 * d)) -
             bilinear(img, ex - nxv * (4.0 * d), ey - nyv * (4.0 * d)))
    m4 = m & np.isfinite(seam4)
    step_d4x = float(np.median(seam4[m4])) if m4.sum() else None
    seam_mad, ctrl_mad = mad(sv), mad(cv)
    ex_ = (med_abs(sv) - med_abs(cv)) if sv.size else None       # v1 口径（诊断，非判据）
    return dict(n=n, ctrl_side=("+" if k > 0 else "-"),
                n_ctrl_plus=n_ok[+1.0], n_ctrl_minus=n_ok[-1.0],
                interior=bool(interior), margin_px=margin,
                # exclude 语义：few_samples = 这条边自身就测不了（退化/出画幅）；
                #               not_interior = 能测，但有一侧在数据外（足迹外缘，不是帧间接缝）
                exclude=(None if interior
                         else ("few_samples" if n < min_samples else "not_interior")),
                # 判据量：**有符号电平台阶** / 局部背景电平
                step=step, bg=bg,
                rel_step=(step / bg) if (step is not None and bg) else None,
                # 诊断量（都不判红）：对照线口径、噪声差、v1 口径
                ctrl_step=ctrl_step, step_net=step_net, bg_ctrl=bg_ctrl,
                rel_step_net=(step_net / bg) if (step_net is not None and bg) else None,
                rel_ctrl_step=(ctrl_step / bg) if (ctrl_step is not None and bg) else None,
                step_d4x=step_d4x,
                rel_step_d4x=(step_d4x / bg) if (step_d4x is not None and bg) else None,
                seam_mad=seam_mad, ctrl_mad=ctrl_mad,
                noise_ratio=(seam_mad / ctrl_mad) if (seam_mad is not None and ctrl_mad) else None,
                noise_diff=(seam_mad - ctrl_mad) if (seam_mad is not None and ctrl_mad is not None)
                else None,
                seam_p90=float(np.percentile(np.abs(sv), 90)) if sv.size else None,
                ctrl_p90=float(np.percentile(np.abs(cv), 90)) if cv.size else None,
                seam_med=med_abs(sv), ctrl_med=med_abs(cv), excess=ex_,
                rel_excess=((ex_ / bg_ctrl) if (ex_ is not None and bg_ctrl) else None))


def v4_tile_ratio(img, tile=512):
    """复刻 eng/tools/e2e/render_vis.py::seam_metric（同口径，逐项对照用）。

    这是**方差比**：分子分母都是 |一阶差分| 的中位数（局部尺度量），与均值/电平无关。
    电平阶跃不改变它 ⇒ 本函数存在的意义就是「旧门在同一输入上不动」的对照证据。
    """
    a = np.nan_to_num(np.asarray(img, dtype=np.float64))
    vr = np.where(np.abs(a).sum(axis=1) > 0)[0]
    vc = np.where(np.abs(a).sum(axis=0) > 0)[0]
    if vr.size < 4 or vc.size < 4:
        return None, None
    r0, r1 = int(vr[0]), int(vr[-1])
    c0, c1 = int(vc[0]), int(vc[-1])

    def m(v):
        v = v[np.isfinite(v)]
        return float(np.median(np.abs(v))) if v.size else 0.0

    cols = list(range(c0 + tile, c1, tile))
    rows = list(range(r0 + tile, r1, tile))
    sv = m(np.concatenate([a[r0:r1 + 1, c] - a[r0:r1 + 1, c - 1] for c in cols])) if cols else 0.0
    iv = m(np.concatenate([a[r0:r1 + 1, c - 3] - a[r0:r1 + 1, c - 4] for c in cols])) if cols else 0.0
    sh = m(np.concatenate([a[r, c0:c1 + 1] - a[r - 1, c0:c1 + 1] for r in rows])) if rows else 0.0
    ih = m(np.concatenate([a[r - 3, c0:c1 + 1] - a[r - 4, c0:c1 + 1] for r in rows])) if rows else 0.0
    num, den = max(sv, sh), max(iv, ih)
    return ((num / den) if den > 0 else (float("inf") if num > 0 else 1.0),
            dict(seam_v=sv, inner_v=iv, seam_h=sh, inner_h=ih,
                 n_col=len(cols), n_row=len(rows)))


def polygon_mask(edges, shape):
    """帧足迹多边形掩模（与 edge_metric 采样的边界曲线同源）。"""
    from PIL import Image, ImageDraw
    h, w = shape
    pts = ([tuple(p) for p in edges[0][1]] + [tuple(p) for p in edges[3][1]] +
           [tuple(p) for p in edges[1][1]][::-1] + [tuple(p) for p in edges[2][1]][::-1])
    im = Image.new("L", (w, h), 0)
    ImageDraw.Draw(im).polygon([(float(x), float(y)) for x, y in pts], fill=1)
    return np.asarray(im, dtype=bool)


# --------------------------------------------------------------------------- #
# 判据（可单独调用：门判定与正负例断言都走这里，自检与生产同一代码路径）
# --------------------------------------------------------------------------- #
def summarize(per_edge, min_samples=20):
    """逐边度量 → 门统计量。

    判据集 = **适用域内**（exclude is None）的边界（rel_* / step_* 是新判据的统计量）；
    同时给出 v1 口径（无符号 excess）在**旧选择规则**（n >= min_samples，不设适用域）下的
    统计量（legacy_* / ex_*），供「修前 ↔ 修后」逐项对照 —— 它们**不参与判红**。
    噪声统计量（noise_ratio_* / noise_diff_*）同样是诊断量，**不参与判红**。
    """
    sel = [p for p in per_edge if p.get("exclude") is None]
    legacy = [p for p in per_edge
              if p.get("excess") is not None and int(p.get("n") or 0) >= min_samples]
    out = {"n_edges_total": len(per_edge), "n_edges_valid": len(sel),
           "n_edges_rel": int(sum(1 for p in sel if p.get("rel_step") is not None)),
           "n_edges_excluded_not_interior":
               int(sum(1 for p in per_edge if p.get("exclude") == "not_interior")),
           "n_edges_excluded_few_samples":
               int(sum(1 for p in per_edge if p.get("exclude") == "few_samples")),
           "n_edges_legacy_valid": len(legacy)}
    rel = np.array([abs(p["rel_step"]) for p in sel if p.get("rel_step") is not None], dtype=float)
    stp = np.array([abs(p["step"]) for p in sel if p.get("step") is not None], dtype=float)
    net = np.array([abs(p["rel_step_net"]) for p in sel if p.get("rel_step_net") is not None],
                   dtype=float)
    nr = np.array([p["noise_ratio"] for p in sel if p.get("noise_ratio") is not None], dtype=float)
    nd = np.array([p["noise_diff"] for p in sel if p.get("noise_diff") is not None], dtype=float)
    if rel.size:
        out.update(rel_med=float(np.median(rel)),
                   rel_p90=float(np.percentile(rel, 90)),
                   rel_max=float(rel.max()))
    if stp.size:
        out.update(step_med_abs=float(np.median(stp)),
                   step_p90_abs=float(np.percentile(stp, 90)),
                   step_max_abs=float(stp.max()))
    if net.size:
        out.update(rel_step_net_max=float(net.max()))
    d4 = np.array([abs(p["rel_step_d4x"]) for p in sel if p.get("rel_step_d4x") is not None],
                  dtype=float)
    if d4.size:
        out.update(rel_step_d4x_max=float(d4.max()))
    if nr.size:
        out.update(noise_ratio_med=float(np.median(nr)), noise_ratio_max=float(nr.max()))
    if nd.size:
        out.update(noise_diff_med=float(np.median(nd)))
    lex = np.array([p["excess"] for p in legacy], dtype=float)
    lrel = np.array([abs(p["rel_excess"]) for p in legacy if p.get("rel_excess") is not None],
                    dtype=float)
    if lex.size:
        out.update(ex_med=float(np.median(np.abs(lex))),
                   ex_p90=float(np.percentile(np.abs(lex), 90)),
                   ex_max=float(np.abs(lex).max()))
    if lrel.size:
        out.update(legacy_rel_med=float(np.median(lrel)),
                   legacy_rel_p90=float(np.percentile(lrel, 90)),
                   legacy_rel_max=float(lrel.max()))
    return out


def gate_decision(per_edge, max_rel_excess, min_samples=20):
    """门判定（fail-closed）：返回 dict(verdict, reason, max_abs_rel, n_exceed, stats)。

    判据 = max_e |rel_step(e)|（**有符号电平台阶** / 局部背景电平）在适用域内的边界上取最大；
    噪声差、v1 的 excess 口径只作为诊断量随 stats 落盘（n_exceed_legacy 供修前/修后对照）。
    """
    stats = summarize(per_edge, min_samples)
    cov = ("（判据覆盖 %d/%d 条边界；未计入 %d 条两侧并非都在数据内部、%d 条样本不足）"
           % (stats["n_edges_valid"], stats["n_edges_total"],
              stats["n_edges_excluded_not_interior"], stats["n_edges_excluded_few_samples"]))
    if not stats.get("n_edges_valid"):
        return dict(verdict="FAIL", max_abs_rel=None, n_exceed=None, n_exceed_legacy=None,
                    stats=stats,
                    reason=("适用域内有效帧边界数为 0（无法判定 ≠ 无接缝；fail-closed 判红）" + cov))
    if not stats.get("n_edges_rel"):
        return dict(verdict="FAIL", max_abs_rel=None, n_exceed=None, n_exceed_legacy=None,
                    stats=stats,
                    reason="全部有效边界的 rel_step 不可计算（背景电平为 0？fail-closed 判红）" + cov)
    sel = [p for p in per_edge if p.get("exclude") is None and p.get("rel_step") is not None]
    rel = np.array([abs(p["rel_step"]) for p in sel], dtype=float)
    n_exceed = int((rel > max_rel_excess).sum())
    mx = float(rel.max())
    lsel = [p for p in per_edge
            if p.get("rel_excess") is not None and int(p.get("n") or 0) >= min_samples]
    lrel = np.array([abs(p["rel_excess"]) for p in lsel], dtype=float)
    n_exc_legacy = int((lrel > max_rel_excess).sum()) if lrel.size else None
    if n_exceed:
        worst = max(sel, key=lambda p: abs(p["rel_step"]))
        return dict(verdict="FAIL", max_abs_rel=mx, n_exceed=n_exceed,
                    n_exceed_legacy=n_exc_legacy, stats=stats,
                    reason=("max|rel_step| = %.6g > %.6g（超门边界 %d/%d 条；最差 %s/%s；"
                            "SCI-C R5 相对接缝门）%s"
                            % (mx, max_rel_excess, n_exceed, rel.size,
                               worst.get("frame"), worst.get("edge"), cov)))
    return dict(verdict="PASS", max_abs_rel=mx, n_exceed=0, n_exceed_legacy=n_exc_legacy,
                stats=stats,
                reason="max|rel_step| = %.6g <= %.6g（门 %d 条有效边界全过）%s"
                       % (mx, max_rel_excess, rel.size, cov))


def _close(a, b, tol=1e-12):
    if a is None or b is None:
        return a is b
    return abs(float(a) - float(b)) <= tol * max(1.0, abs(float(a)), abs(float(b)))


def injection_assert(base_per, inj_per, frame, amp, detect_frac=0.5, min_samples=20):
    """正/负例断言：amp==0 ⇒ 与基线逐条一致；amp!=0 ⇒ 该帧边界必须看得见 |amp|。

    判据量是**有符号台阶** step_net，且只认「适用域内（exclude is None）」的边界 ——
    若注入的已知台阶落在适用域外，等于判据看不见它，本断言必须判失败（fail-closed）。
    返回 (ok, note, detail)。
    """
    if len(base_per) != len(inj_per):
        return False, "注入前后逐边条目数不一致（%d vs %d）" % (len(base_per), len(inj_per)), {}
    if amp == 0.0:
        bad = []
        for a, b in zip(base_per, inj_per):
            for k in ("n", "step", "ctrl_step", "step_net", "rel_step",
                      "seam_med", "ctrl_med", "excess", "rel_excess"):
                if not _close(a.get(k), b.get(k)):
                    bad.append("%s/%s.%s: %r -> %r" % (a.get("frame"), a.get("edge"), k,
                                                       a.get(k), b.get(k)))
            if a.get("exclude") != b.get("exclude"):
                bad.append("%s/%s.exclude: %r -> %r" % (a.get("frame"), a.get("edge"),
                                                        a.get("exclude"), b.get("exclude")))
        if bad:
            return (False, "注入 0 后度量未回落基线（真值无效应却有差异）：" + "; ".join(bad[:4]),
                    {"n_diff": len(bad)})
        return True, "注入 0 ⇒ %d 条边界度量与基线逐条一致（真值无效应 ⇒ 回落基线）" % len(base_per), \
            {"n_identical": len(base_per)}
    sel = [p for p in inj_per
           if p.get("frame") == frame and p.get("exclude") is None
           and p.get("step") is not None]
    if not sel:
        return False, ("注入 %s += %g 后该帧没有任何**计入判据**的边界"
                       "（判据看不见：适用域排除或有效样本不足）" % (frame, amp)), {}
    mx = max(abs(p["step"]) for p in sel)
    ok = mx >= detect_frac * abs(amp)
    note = ("注入 %s += %g ⇒ 该帧 %d 条边界（适用域内）max|有符号台阶| = %.6g，"
            "需 >= %.6g（= %.2f×|amp|）⇒ %s"
            % (frame, amp, len(sel), mx, detect_frac * abs(amp), detect_frac,
               "可见" if ok else "**不可见**（判据对该已知阶跃失明）"))
    return ok, note, {"frame": frame, "amp": amp, "max_abs_step": mx,
                      "required": detect_frac * abs(amp), "n_edges": len(sel)}


# --------------------------------------------------------------------------- #
# 单次评估（读产品 + 帧清单 → 逐边度量 + V4 对照 + 注入）
# --------------------------------------------------------------------------- #
def evaluate_product(fits_path, p1_dirs, frame_naxis=(4096, 4096), tile=512, d=2.0,
                     ctrl_shift=200.0, min_samples=20, max_rel_excess=1e-2,
                     injections=(), detect_frac=0.5, img=None, wp3=None, n_pts=256):
    """跑一次完整判定。img/wp3 可预置（调用方已把产品读进内存时避免二次 I/O）。"""
    frames = collect_frames(p1_dirs)
    if not frames:
        raise RuntimeError("--p1-dirs 下没有找到任何 <frame>/p1_wcs.json：%s" % list(p1_dirs))
    if img is None or wp3 is None:
        img2, wp32 = product_wcs(fits_path)
        img = img2 if img is None else img
        wp3 = wp32 if wp3 is None else wp3  # 调用方预置 img 时必须同时预置 wp3
    edges_by_frame = []
    for wj in frames:
        wf, _ = load_wcs_from_p1(wj)
        edges_by_frame.append((os.path.basename(os.path.dirname(wj)),
                               frame_edges_p3(wf, wp3, frame_naxis, n_pts=n_pts)))

    def run(im):
        per = []
        for name, edges in edges_by_frame:
            for nm, e in edges:
                r = edge_metric(im, e, d=d, ctrl_shift=ctrl_shift, min_samples=min_samples)
                r["frame"] = name
                r["edge"] = nm
                per.append(r)
        return per

    base_v4, base_v4d = v4_tile_ratio(img, tile)
    base_per = run(img)
    base_gate = gate_decision(base_per, max_rel_excess, min_samples)
    rec = {"fits": (os.path.abspath(fits_path) if fits_path else None),
           "shape": [int(img.shape[0]), int(img.shape[1])],
           "n_frames": len(frames), "frame_naxis": list(frame_naxis), "tile": tile,
           "norm_d": d, "ctrl_shift": ctrl_shift, "min_samples": min_samples,
           "n_pts": n_pts,
           "max_rel_excess": max_rel_excess,
           "criterion": {
               "name": "signed_level_step",
               "statistic": ("rel_step = median(img[+d] − img[−d]) / bg "
                             "（有符号电平台阶 / 边界处的局部背景电平）"),
               "gate": "max_e |rel_step(e)| <= max_rel_excess",
               "domain": ("只对两侧都在数据内部的边界计入：法向 ±ctrl_shift 两侧都能放对照线"
                          "且各有 >= min_samples 个有效样本（N = ctrl_shift，从输入导出）"),
               "diagnostics": ["step_d4x / rel_step_d4x（4d 采样半距的 d 扫描；判据的梯度敏感度）",
                               "step_net / rel_step_net（扣 200 px 平行对照线后的净台阶；不判红）",
                               "noise_ratio = MAD(seam)/MAD(ctrl)（噪声差；不判红）",
                               "noise_diff = MAD(seam) − MAD(ctrl)（不判红）",
                               "excess / rel_excess（v1 无符号口径，历史对照；不判红）"],
               "detection_floor": ("可检出下限 Δ/L > max_rel_excess/(1 − max_rel_excess/2)；"
                                   "默认 1e-2 ⇒ Δ/L > 1.005%（L = 边界下侧电平；≈0.0109 mag），"
                                   "更小的电平阶跃被放过（run/SEAM-PERIOD-BLIND-01 门限扫描）"),
               "period_blindness": ("v1 的 excess 口径在「平行同幅阶跃、间距整除 ctrl_shift」时"
                                    "精确相消（P0-7）；v2 判据不含对照线 ⇒ 与阶跃周期无关"
                                    "（--self-test 的 S8/S9 钉住）"),
               "v1_note": ("v1 的 excess = median|seam| − median|ctrl| 对噪声差敏感，"
                           "已降级为诊断量（run/VIS-E2E02-01/REPORT.md §1.1/§9）")},
           "v4": {"ratio": base_v4, "detail": base_v4d, "note": "方差比粗筛（对电平阶跃原理性失明）"},
           "baseline": {"gate": base_gate, "per_edge": base_per},
           "inject": []}
    for spec in injections:
        if ":" not in spec:
            raise RuntimeError("--inject-frame 需为 <k>:<amp>，实得 %r" % spec)
        ks, amps = spec.split(":", 1)
        k, amp = int(ks), float(amps)
        if not (0 <= k < len(edges_by_frame)):
            raise RuntimeError("--inject-frame 帧号 %d 越界（共 %d 帧）" % (k, len(edges_by_frame)))
        fname = edges_by_frame[k][0]
        mask = polygon_mask(edges_by_frame[k][1], img.shape)
        a2 = img.copy()
        if amp:
            a2[mask] += amp
        v4r, _ = v4_tile_ratio(a2, tile)
        per2 = run(a2)
        g2 = gate_decision(per2, max_rel_excess, min_samples)
        ok, note, detail = injection_assert(base_per, per2, fname, amp, detect_frac, min_samples)
        rec["inject"].append({
            "spec": spec, "frame_index": k, "frame": fname, "amp": amp,
            "mask_px": int(mask.sum()), "v4_ratio": v4r,
            "gate": g2, "assert_ok": bool(ok), "assert_note": note, "assert_detail": detail,
            "per_edge": per2})
        del a2, mask
    return rec


# --------------------------------------------------------------------------- #
# 合成夹具 + 自检（机器门常驻入口；不依赖 run/ 下的大产品）
# --------------------------------------------------------------------------- #
def _rot_cd(theta_deg, scale=5e-4):
    """帧 CD 矩阵 = 旋转 theta 的 diag(-s, s)（与产品 CD 同量级，足迹落在产品网格里）。"""
    t = np.deg2rad(theta_deg)
    r = np.array([[np.cos(t), -np.sin(t)], [np.sin(t), np.cos(t)]])
    return r @ np.array([[-scale, 0.0], [0.0, scale]])


def make_fixture(work_dir, seed=20260924, npix=FIXTURE_NPIX, frame_n=FIXTURE_FRAME_N,
                 theta_deg=0.7, bg=1.0, noise=1e-4, centers=None):
    """合成夹具：一张平滑产品 + N 帧帧清单（帧足迹为略倾斜的方块，四边两侧都有数据）。

    返回 (fits_path, [p1_dir], frame_naxis)。
    """
    from PIL import Image  # noqa: F401  （与 polygon_mask 同依赖，缺失时立刻报错）
    os.makedirs(work_dir, exist_ok=True)
    fits_path = os.path.join(work_dir, "synthetic_product.fits")
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:npix, 0:npix]
    img = (bg + 0.02 * xx / float(npix) + noise * rng.standard_normal((npix, npix)))
    hdr = fits.Header()
    hdr["CTYPE1"] = "RA---TAN"
    hdr["CTYPE2"] = "DEC--TAN"
    hdr["CRVAL1"] = 84.0
    hdr["CRVAL2"] = -5.4
    hdr["CRPIX1"] = npix / 2.0 + 0.5
    hdr["CRPIX2"] = npix / 2.0 + 0.5
    hdr["CD1_1"] = -5e-4
    hdr["CD1_2"] = 0.0
    hdr["CD2_1"] = 0.0
    hdr["CD2_2"] = 5e-4
    fits.PrimaryHDU(data=img.astype(np.float32), header=hdr).writeto(fits_path, overwrite=True)

    if centers is None:
        q = npix // 3
        centers = [(q, q), (2 * q, q), (q, 2 * q), (2 * q, 2 * q)]
    p1_dir = os.path.join(work_dir, "p1")
    if os.path.isdir(p1_dir):
        shutil.rmtree(p1_dir)
    cd = _rot_cd(theta_deg)
    for i, (cx, cy) in enumerate(centers):
        d = os.path.join(p1_dir, "synth_frame_%02d" % i)
        os.makedirs(d, exist_ok=True)
        wcs = {"cd11": float(cd[0, 0]), "cd12": float(cd[0, 1]),
               "cd21": float(cd[1, 0]), "cd22": float(cd[1, 1]),
               "crpix1": frame_n / 2.0 + 0.5, "crpix2": frame_n / 2.0 + 0.5,
               "crval1": 84.0, "crval2": -5.4,
               "ctype1": "RA---TAN", "ctype2": "DEC--TAN"}
        # 帧中心落在产品像素 (cx, cy)：由 crval 反解（产品 CD 为 diag(-5e-4, 5e-4)）
        dra = -5e-4 * (cx - (npix / 2.0 + 0.5))
        ddec = 5e-4 * (cy - (npix / 2.0 + 0.5))
        wcs["crval1"] = 84.0 + dra
        wcs["crval2"] = -5.4 + ddec
        io.open(os.path.join(d, "p1_wcs.json"), "w", encoding="utf-8").write(
            json.dumps({"schema": "astrocs.p1-wcs/synthetic", "wcs": wcs},
                       ensure_ascii=False, indent=1) + "\n")
    return fits_path, [p1_dir], (frame_n, frame_n)


def _edge_row(fits_path, p1_dirs, frame_naxis, frame_index=0, edge="top"):
    """夹具里某条帧边在产品网格上的行（theta=0 时该边严格水平 ⇒ 行是常数）。

    用于把「噪声台阶 / 数据边界」对齐到那条边 —— 负例夹具（S6/S7）靠它构造，
    不靠手算常数。
    """
    _, wp3 = product_wcs(fits_path)
    frames = collect_frames(p1_dirs)
    wf, _ = load_wcs_from_p1(frames[frame_index])
    ed = dict(frame_edges_p3(wf, wp3, frame_naxis))
    return float(np.median(ed[edge][:, 1]))


def rewrite_fixture_noise(src_fits, dst_fits, sigma_fn, nan_fn=None, bg=1.0, grad=0.02, seed=7):
    """在**同一均值面**上重造噪声（空间变化 σ / NaN 掩模）——判据适用域的负例夹具。

    均值面与原夹具逐像素相同（bg + grad·x/npix）⇒ 合成边**没有电平台阶**，变的只有噪声
    尺度与有限性（正是 v1 假阳性模式的成因）。WCS 头与帧清单沿用原夹具。
    """
    os.makedirs(os.path.dirname(os.path.abspath(dst_fits)), exist_ok=True)
    with fits.open(src_fits, memmap=True) as h:
        hdr = h[0].header.copy()
        shape = tuple(int(v) for v in h[0].data.shape)
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    mean = bg + grad * xx / float(shape[1])
    sig = np.asarray(sigma_fn(xx, yy), dtype=np.float64)
    data = mean + sig * np.random.default_rng(seed).standard_normal(shape)
    if nan_fn is not None:
        data = np.where(np.asarray(nan_fn(xx, yy), dtype=bool), np.nan, data)
    fits.PrimaryHDU(data=data.astype(np.float32), header=hdr).writeto(dst_fits, overwrite=True)
    return dst_fits


def write_periodic_step_fixture(src_fits, dst_fits, period_px, delta, edge_y, d=2.0, bg=1.0):
    """「平行同幅阶跃」负例夹具（独立审查 P0-7「周期即盲区」）：沿 y 每 period_px 一条
    **同向同幅**阶跃，第一条正好压在 edge_y（某条帧边界的 y 采样）所代表的那条帧边界上。

    阶跃行吸附到最近的**半整数**行（本工具的像素中心取整数坐标）⇒ 边界法向 ±d 的采样点分别
    落在阶跃两侧、各自距阶跃 >= d − 0.5 px；不满足即抛错（防夹具静默退化成"没有台阶"）。

    用途：period_px 整除 ctrl_shift 时，v1 的对照线（法向平移 ctrl_shift 的平行线）自己踩在
    同幅阶跃上 ⇒ excess = median|seam| − median|ctrl| 精确相消 ⇒ v1 判绿；v2 的判据量不含
    对照线 ⇒ 必须判红。返回 (dst_fits, step_row)。
    """
    os.makedirs(os.path.dirname(os.path.abspath(dst_fits)), exist_ok=True)
    with fits.open(src_fits, memmap=True) as h:
        hdr = h[0].header.copy()
        shape = tuple(int(v) for v in h[0].data.shape)
    ey = np.asarray(edge_y, dtype=float)
    ey = ey[np.isfinite(ey)]
    ys = float(np.floor(float(np.median(ey))) + 0.5)
    lo, hi = float(ey.min()), float(ey.max())
    if not (hi - d < ys < lo + d):
        raise RuntimeError("周期盲区夹具退化：阶跃行 %.4f 落不进全部边界采样点的 ±%g px 内"
                           "（y ∈ [%.4f, %.4f]）" % (ys, d, lo, hi))
    yy = np.arange(shape[0], dtype=float)[:, None]
    n = np.clip(np.floor((yy - ys) / float(period_px)) + 1.0, 0.0, None)
    data = float(bg) + float(delta) * n
    fits.PrimaryHDU(data=np.broadcast_to(data, shape).astype(np.float32),
                    header=hdr).writeto(dst_fits, overwrite=True)
    return dst_fits, ys


def _f(x):
    return "%.4e" % x if isinstance(x, float) else repr(x)


def self_test(work_dir, max_rel_excess=1e-2):
    """七组用例，全部走 evaluate_product / gate_decision 同一代码路径。

    REQUIRED_SELFTEST_CASES 硬校验用例名单：**缺任一条即自检失败**（不允许静默删用例）。
    """
    cases = []

    def add(name, ok, note):
        cases.append({"name": name, "ok": bool(ok), "note": note})

    fits_path, p1_dirs, fnax = make_fixture(os.path.join(work_dir, "fixture"))
    kw = dict(frame_naxis=fnax, tile=512, min_samples=20, max_rel_excess=max_rel_excess)

    # S1：无台阶 ⇒ 必须绿
    r1 = evaluate_product(fits_path, p1_dirs, **kw)
    g1 = r1["baseline"]["gate"]
    add("S1-synthetic-no-step-green", g1["verdict"] == "PASS",
        "无注入基线：%s（%s）" % (g1["verdict"], g1["reason"]))

    # S2：注入已知台阶 ⇒ **注入后**的门必须红，且该帧边界必须看得见
    amp = 0.05
    r2 = evaluate_product(fits_path, p1_dirs, injections=["0:%g" % amp], **kw)
    g2 = r2["inject"][0]["gate"]
    inj2 = r2["inject"][0]
    add("S2-injected-step-red",
        g2["verdict"] == "FAIL" and inj2["assert_ok"],
        "注入 %g：注入后门 %s（max|rel|=%.4e，基线 %.4e）；%s"
        % (amp, g2["verdict"], g2["max_abs_rel"] if g2["max_abs_rel"] is not None else float("nan"),
           r2["baseline"]["gate"]["max_abs_rel"], inj2["assert_note"]))

    # S3：旧 V4 方差比在同一输入上必须**不动**（盲区复现 = 新判据非冗余）
    v4b, v4i = r1["v4"]["ratio"], r2["inject"][0]["v4_ratio"]
    add("S3-legacy-v4-blind-on-injected",
        v4i is not None and v4i <= 1.5,
        "旧 V4 方差比：基线 %.6f → 注入已知台阶 %.6f（门 1.5）⇒ %s"
        % (v4b if v4b is not None else float("nan"), v4i if v4i is not None else float("nan"),
           "判绿 = 对电平阶跃失明" if (v4i is not None and v4i <= 1.5) else "意外判红"))

    # S4：帧足迹落在画幅外 ⇒ 有效边界 0 ⇒ 必须红（fail-closed）
    off = 20 * FIXTURE_NPIX
    f2, p2, fn2 = make_fixture(os.path.join(work_dir, "fixture_off"),
                               centers=[(off, off), (off + 1, off), (off, off + 1), (off + 1, off + 1)])
    r4 = evaluate_product(f2, p2, **kw)
    g4 = r4["baseline"]["gate"]
    add("S4-degenerate-footprint-red", g4["verdict"] == "FAIL",
        "帧足迹全在画幅外：%s（%s）" % (g4["verdict"], g4["reason"]))

    # S5：注入 0 ⇒ 与基线逐条一致且判绿（负例：真值无效应必须回落）
    r5 = evaluate_product(fits_path, p1_dirs, injections=["0:0"], **kw)
    inj5 = r5["inject"][0]
    add("S5-zero-injection-identity",
        inj5["assert_ok"] and r5["baseline"]["gate"]["verdict"] == "PASS",
        "注入 0：%s；门 %s" % (inj5["assert_note"], r5["baseline"]["gate"]["verdict"]))

    # ---- S6/S7：判据适用域的负例夹具（**同一均值面**，只改噪声尺度与有限性）----
    # theta=0 ⇒ 帧边在产品网格上严格水平/竖直，噪声台阶与数据边界可与该边逐像素对齐。
    f_big, p_big, fn_big = make_fixture(
        os.path.join(work_dir, "fixture_big"), npix=FIXTURE_NPIX_BIG,
        frame_n=FIXTURE_FRAME_N_BIG, theta_deg=0.0,
        centers=[(700, 700), (1700, 700), (700, 1700), (1700, 1700)])
    kw_big = dict(frame_naxis=fn_big, tile=512, min_samples=20,
                  max_rel_excess=max_rel_excess, n_pts=512)
    row_top = _edge_row(f_big, p_big, fn_big, 0, "top")
    fr0 = os.path.basename(os.path.dirname(collect_frames(p_big)[0]))

    def _st(rec):
        return rec["baseline"]["gate"]["stats"]

    def _legacy_red(rec):
        v = _st(rec).get("legacy_rel_max")
        return (v is not None and v > max_rel_excess), v

    # S6：噪声台阶正好落在帧 0/1 的 top 边上（边之上 σ_hi、之下 σ_lo）——**无电平台阶**。
    # 对照线只能落在低噪声一侧 ⇒ v1 口径（median|seam| − median|ctrl|）必然假阳；v2 必须判绿。
    s6 = rewrite_fixture_noise(
        f_big, os.path.join(work_dir, "fixture_noisestep", "product.fits"),
        lambda xx, yy: np.where(yy < row_top, SELF_SIGMA_HI, SELF_SIGMA_LO), seed=7)
    r6 = evaluate_product(s6, p_big, **kw_big)
    g6 = r6["baseline"]["gate"]
    red6, lg6 = _legacy_red(r6)
    nr6 = _st(r6).get("noise_ratio_max")
    add("S6-noise-difference-no-step-green",
        g6["verdict"] == "PASS" and red6 and (nr6 or 0.0) > 2.0,
        "帧 0/1 的 top 边两侧 σ = %.1e / %.1e（无电平台阶；噪声比 max = %.1f×）："
        "v2 判据 %s（max|rel_step| = %s，门 %.1e）；v1 口径 max|rel_excess| = %s ⇒ %s"
        % (SELF_SIGMA_HI, SELF_SIGMA_LO, nr6 if nr6 is not None else float("nan"),
           g6["verdict"], _f(g6["max_abs_rel"]), max_rel_excess, _f(lg6),
           "同一夹具上 v1 判红（假阳性模式复现）" if red6 else "**v1 未判红 ⇒ 夹具没复现假阳性模式**"))

    # S7：贴数据边界的边（外侧 30 px 就是 NaN）+ 边缘余量高噪声 ⇒ 适用域必须排除它、门判绿；
    # 同一夹具上 v1 口径仍判红（它拿「边缘余量的高噪声」减「内部噪声」）。
    nan_from = row_top + 30.0
    band_from = row_top - 10.0
    s7 = rewrite_fixture_noise(
        f_big, os.path.join(work_dir, "fixture_margin", "product.fits"),
        lambda xx, yy: np.where((yy >= band_from) & (yy < nan_from), SELF_SIGMA_HI, SELF_SIGMA_LO),
        nan_fn=lambda xx, yy: yy >= nan_from, seed=11)
    r7 = evaluate_product(s7, p_big, **kw_big)
    g7 = r7["baseline"]["gate"]
    e7 = [p for p in r7["baseline"]["per_edge"]
          if p.get("frame") == fr0 and p.get("edge") == "top"]
    ex7 = e7[0] if e7 else {}
    red7, lg7 = _legacy_red(r7)
    add("S7-boundary-margin-edge-excluded",
        g7["verdict"] == "PASS" and ex7.get("exclude") == "not_interior" and red7,
        "帧 0 的 top 边距 NaN 仅 %.0f px（外侧对照线落在 NaN 上）⇒ 该边 exclude=%r、"
        "margin_px=%s、n_ctrl_plus=%s；门 %s（max|rel_step| = %s；适用域排除 %s 条）；"
        "同一夹具 v1 口径 max|rel_excess| = %s ⇒ %s"
        % (nan_from - row_top, ex7.get("exclude"), _f(ex7.get("margin_px")),
           ex7.get("n_ctrl_plus"), g7["verdict"], _f(g7["max_abs_rel"]),
           _st(r7).get("n_edges_excluded_not_interior"), _f(lg7),
           "v1 判红（边缘余量假阳性复现）" if red7 else "**v1 未判红**"))

    # ---- S8/S9：P0-7「周期即盲区」负例（平行同幅阶跃、间距 = ctrl_shift 及其因子）----
    # 场 = 沿 y 每 P 一条**同向同幅**阶跃（幅度 delta_p = 2.0 = 200% 局部电平，bg = 1.0），
    # 第一条阶跃正压在帧 0 的 top 边上。P 取 ctrl_shift（=200）与其**因子**（=100）时，v1 的
    # 对照线（法向平移 ctrl_shift）自己踩在同幅阶跃上 ⇒ excess 精确相消 ⇒ v1 **整幅判绿**；
    # v2 的判据量不含对照线 ⇒ 必须判红。对照臂 P = 2·ctrl_shift（=400，200 不整除它）⇒
    # v1 必须**不**判绿（证明夹具非平凡：不是"什么都看不见"）。三档周期的 rel_step 必须一致。
    ctrl_shift_p, delta_p = 200.0, 2.0
    f_ax, p_ax, fn_ax = make_fixture(os.path.join(work_dir, "fixture_axis"), theta_deg=0.0)
    _, wp3_ax = product_wcs(f_ax)
    frames_ax = collect_frames(p_ax)
    wf_ax, _ = load_wcs_from_p1(frames_ax[0])
    top_ax = dict(frame_edges_p3(wf_ax, wp3_ax, fn_ax, n_pts=256))["top"]
    fr_ax = os.path.basename(os.path.dirname(frames_ax[0]))
    kw_p = dict(frame_naxis=fn_ax, tile=512, min_samples=20,
                max_rel_excess=max_rel_excess, ctrl_shift=ctrl_shift_p)
    periods_p = (ctrl_shift_p, ctrl_shift_p / 2.0, ctrl_shift_p * 2.0)
    rows_p = {}
    for period in periods_p:
        dst_p, step_row = write_periodic_step_fixture(
            f_ax, os.path.join(work_dir, "fixture_axis", "periodic", "product.fits"),
            period, delta_p, top_ax[:, 1], d=2.0, bg=1.0)
        r_p = evaluate_product(dst_p, p_ax, **kw_p)
        g_p = r_p["baseline"]["gate"]
        e_p = next((q for q in r_p["baseline"]["per_edge"]
                    if q.get("frame") == fr_ax and q.get("edge") == "top"), {})
        rows_p[period] = dict(step_row=step_row, gate=g_p["verdict"],
                              rel_step=e_p.get("rel_step"), rel_excess=e_p.get("rel_excess"),
                              legacy=g_p["stats"].get("legacy_rel_max"),
                              margin_px=e_p.get("margin_px"), exclude=e_p.get("exclude"))

    def _p_label(period):
        return ("ctrl_shift 本身" if period == ctrl_shift_p
                else ("ctrl_shift 的因子" if period < ctrl_shift_p else "ctrl_shift 的 2 倍（对照臂）"))

    def _p_note(period):
        r = rows_p[period]
        return ("P=%g（%s）：v2 门 %s（该边 rel_step=%s），v1 口径 max|rel_excess|=%s ⇒ %s"
                % (period, _p_label(period), r["gate"], _f(r["rel_step"]), _f(r["legacy"]),
                   "v1 判绿 = 旧盲区复现" if (r["legacy"] is not None
                                              and abs(r["legacy"]) <= max_rel_excess)
                   else "v1 判红（非退化臂）"))

    def _p_spread():
        v = [rows_p[p]["rel_step"] for p in periods_p if rows_p[p]["rel_step"] is not None]
        return (max(v) - min(v)) if len(v) == len(periods_p) else None

    def _p_ok(period):
        r, ctrl = rows_p[period], rows_p[ctrl_shift_p * 2.0]
        sp = _p_spread()
        return bool(r["gate"] == "FAIL" and r["rel_step"] is not None
                    and abs(r["rel_step"]) > max_rel_excess
                    and r["legacy"] is not None and abs(r["legacy"]) <= max_rel_excess
                    and ctrl["gate"] == "FAIL"
                    and ctrl["legacy"] is not None and abs(ctrl["legacy"]) > max_rel_excess
                    and sp is not None and sp <= 1e-12)

    add("S8-periodic-step-on-edge-red", _p_ok(ctrl_shift_p),
        "平行同幅阶跃、间距 = ctrl_shift（=%g px）、幅度 = 200%% 电平（Δ=%g / bg=1.0）压在帧 0 的 "
        "top 边上（阶跃行 %s；边界 y∈[%.3f, %.3f]，margin_px=%s）：%s；对照臂 %s；三档周期 "
        "rel_step 极差 = %s（判据量与周期无关）"
        % (ctrl_shift_p, delta_p, _f(rows_p[ctrl_shift_p]["step_row"]),
           float(top_ax[:, 1].min()), float(top_ax[:, 1].max()),
           _f(rows_p[ctrl_shift_p]["margin_px"]), _p_note(ctrl_shift_p),
           _p_note(ctrl_shift_p * 2.0), _f(_p_spread())))
    add("S9-periodic-step-factor-on-edge-red", _p_ok(ctrl_shift_p / 2.0),
        "同 S8 但间距 = ctrl_shift 的因子（=%g px）：%s"
        % (ctrl_shift_p / 2.0, _p_note(ctrl_shift_p / 2.0)))

    bad = [c["name"] for c in cases if not c["ok"]]
    have = {c["name"] for c in cases}
    missing = [n for n in REQUIRED_SELFTEST_CASES if n not in have]
    if missing:
        cases.append({"name": "S0-required-cases-present", "ok": False,
                      "note": "缺少必需用例（缺任一条即自检失败）：%s" % ", ".join(missing)})
        bad = [c["name"] for c in cases if not c["ok"]]
    return {"cases": cases, "n_pass": len(cases) - len(bad), "n_total": len(cases),
            "failed": bad, "verdict": "PASS" if not bad else "FAIL",
            "required_cases": list(REQUIRED_SELFTEST_CASES),
            "n_required_missing": len(missing)}


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def _write_json(path, rec):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    io.open(path, "w", encoding="utf-8").write(
        json.dumps(rec, ensure_ascii=False, indent=1) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description="L4 接缝机器门（沿真实帧足迹边界的电平阶跃判据）")
    ap.add_argument("--fits", default=None, help="整幅平面产品 FITS（L4 导出产品）")
    ap.add_argument("--p1-dirs", nargs="+", default=None,
                    help="P1 阶段帧目录（内含 <frame>/p1_wcs.json），可给多个夜次目录")
    ap.add_argument("--json-out", "--out", dest="json_out", default=None,
                    help="机器可读证据落盘路径")
    ap.add_argument("--max-rel-excess", type=float, default=1e-2,
                    help="门：max|rel_step| 上限（默认 1e-2 = SCI-C R5「相对接缝度量 < 1%%」）。"
                         "判据是**相对**口径 ⇒ 可检出下限 = 本值/(1 − 本值/2) 的**局部电平**占比："
                         "1e-2 时 Δ/L = 1.005%%（L = 边界下侧电平；≈0.0109 mag），更小的电平阶跃被放过（诚实边界，"
                         "见模块 docstring「判据的可检出下限」）")
    ap.add_argument("--min-samples", type=int, default=20,
                    help="一条边界计入判据所需的最少有效采样点数（默认 20）")
    ap.add_argument("--tile", type=int, default=512, help="V4 方差比对照用的分块边长")
    ap.add_argument("--frame-naxis", type=int, default=4096, help="帧像素边长（默认 4096）")
    ap.add_argument("--norm-d", type=float, default=2.0, help="法向差分半距（px，默认 2）")
    ap.add_argument("--ctrl-shift", type=float, default=200.0,
                    help="对照平行线的法向平移量（px，默认 200）。v2 里对照线**不参与判红**，"
                         "只用于适用域（两侧都在数据内部）与诊断量 ⇒ 保留常量，"
                         "不由输入几何自适应导出（理由见模块 docstring「适用域」一节）")
    ap.add_argument("--inject-frame", action="append", default=[],
                    help="正/负例：<k>:<amp>，把 amp 加进第 k 帧足迹内部（amp=0 为负例）")
    ap.add_argument("--inject-detect-frac", type=float, default=0.5,
                    help="注入 amp 后该帧边界 max|有符号台阶 step| 至少需达 |amp| 的该比例"
                         "（默认 0.5）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="跑合成夹具九组用例（机器门常驻入口，不需要 --fits/--p1-dirs；"
                         "缺任一条必需用例即失败；含「平行同幅阶跃、间距 = ctrl_shift 及其因子 "
                         "⇒ 必须判红」的 P0-7 负例 S8/S9）")
    ap.add_argument("--work-dir", default=DEFAULT_WORK_DIR,
                    help="自检夹具与默认证据落盘根（默认 run/ci/seam-footprint）")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    out = args.json_out or os.path.join(args.work_dir, "seam_footprint.json")

    if args.self_test:
        try:
            st = self_test(args.work_dir, args.max_rel_excess)
        except Exception as exc:  # noqa: BLE001 - 夹具不可用 = 环境错误（fail-closed）
            print("SEAM_FOOTPRINT_SELFTEST_ERROR: %s: %s" % (type(exc).__name__, exc),
                  file=sys.stderr)
            return 2
        rec = {"schema": "astrocs.seam-footprint/v1", "tool": "seam_footprint",
               "mode": "self-test", "max_rel_excess": args.max_rel_excess,
               "work_dir": os.path.abspath(args.work_dir), **st}
        _write_json(out, rec)
        for c in st["cases"]:
            print("SELFTEST %s %s  %s" % ("PASS" if c["ok"] else "FAIL", c["name"], c["note"]))
        if st.get("n_required_missing"):
            print("SEAM_FOOTPRINT_SELFTEST_REQUIRED_MISSING: %d 条必需用例缺失"
                  % st["n_required_missing"])
        print("SEAM_FOOTPRINT_SELFTEST_%s: %d/%d（必需用例 %d 条齐备）-> %s"
              % (st["verdict"], st["n_pass"], st["n_total"],
                 len(st.get("required_cases", ())), out))
        return 0 if st["verdict"] == "PASS" else 1

    if not args.fits or not args.p1_dirs:
        print("SEAM_FOOTPRINT_ERROR: 需 --fits 与 --p1-dirs（或 --self-test）", file=sys.stderr)
        return 2
    if not os.path.isfile(args.fits):
        print("SEAM_FOOTPRINT_ERROR: FITS 不存在: %s（fail-closed）" % args.fits, file=sys.stderr)
        return 2
    missing = [d for d in args.p1_dirs if not os.path.isdir(d)]
    if missing:
        print("SEAM_FOOTPRINT_ERROR: --p1-dirs 不存在: %s（fail-closed）" % missing, file=sys.stderr)
        return 2

    try:
        rec = evaluate_product(args.fits, args.p1_dirs,
                               frame_naxis=(args.frame_naxis, args.frame_naxis),
                               tile=args.tile, d=args.norm_d, ctrl_shift=args.ctrl_shift,
                               min_samples=args.min_samples,
                               max_rel_excess=args.max_rel_excess,
                               injections=args.inject_frame,
                               detect_frac=args.inject_detect_frac)
    except Exception as exc:  # noqa: BLE001 - 读不动/参数非法 = 输入错误（fail-closed）
        print("SEAM_FOOTPRINT_ERROR: %s: %s" % (type(exc).__name__, exc), file=sys.stderr)
        return 2

    gate = rec["baseline"]["gate"]
    rec["schema"] = "astrocs.seam-footprint/v1"
    rec["tool"] = "seam_footprint"
    rec["mode"] = "product"
    findings = []
    if gate["verdict"] == "FAIL":
        findings.append("SEAM %s" % gate["reason"])
    for inj in rec["inject"]:
        if not inj["assert_ok"]:
            findings.append("SEAM-INJECT[%s] %s" % (inj["spec"], inj["assert_note"]))
        if inj["amp"] != 0.0 and inj["v4_ratio"] is not None and inj["v4_ratio"] <= 1.5:
            inj["v4_blind_confirmed"] = True
    rec["findings"] = findings
    rec["verdict"] = "PASS" if not findings else "FAIL"
    _write_json(out, rec)

    if not args.quiet:
        print("n_frames=%d  edges=%d  V4(base)=%s（方差比，粗筛）"
              % (rec["n_frames"], rec["baseline"]["gate"]["stats"]["n_edges_total"],
                 ("%.6f" % rec["v4"]["ratio"]) if rec["v4"]["ratio"] is not None else "n/a"))
        st = rec["baseline"]["gate"]["stats"]
        print("baseline : %s | 判据 max|rel_step| med=%.4e p90=%.4e max=%.4e | 门 %.1e"
              % (gate["verdict"], st.get("rel_med", float("nan")),
                 st.get("rel_p90", float("nan")), st.get("rel_max", float("nan")),
                 args.max_rel_excess))
        print("  domain : 计入 %d/%d 条边界（两侧并非都在数据内部 %d / 样本不足 %d）| "
              "|有符号台阶| med=%.4e max=%.4e（ADU）| 噪声比 med=%.3g max=%.3g"
              % (st.get("n_edges_valid", 0), st.get("n_edges_total", 0),
                 st.get("n_edges_excluded_not_interior", 0),
                 st.get("n_edges_excluded_few_samples", 0),
                 st.get("step_med_abs", float("nan")), st.get("step_max_abs", float("nan")),
                 st.get("noise_ratio_med", float("nan")),
                 st.get("noise_ratio_max", float("nan"))))
        print("  legacy : v1 口径（无符号 excess）max|rel_excess| = %.4e，超门 %s/%s 条"
              "（只作历史对照，**不判红**）"
              % (st.get("legacy_rel_max", float("nan")), gate.get("n_exceed_legacy"),
                 st.get("n_edges_legacy_valid")))
        print("  diag   : 扣对照线的净台阶 max|rel_step_net| = %.4e；d 扫描 max|rel_step_d4x| = %.4e"
              "（**都不判红**，仅供判读：真台阶在 d 扫描下守恒，背景梯度按 d 增长）"
              % (st.get("rel_step_net_max", float("nan")), st.get("rel_step_d4x_max", float("nan"))))
        for inj in rec["inject"]:
            print("%-28s V4=%-10s 门=%-4s %s"
                  % ("inject " + inj["spec"],
                     ("%.6f" % inj["v4_ratio"]) if inj["v4_ratio"] is not None else "n/a",
                     inj["gate"]["verdict"], inj["assert_note"]))
        for f in findings:
            print("  " + f)
    print("SEAM_FOOTPRINT_%s: findings=%d -> %s" % (rec["verdict"], len(findings), out))
    return 0 if not findings else 1


if __name__ == "__main__":
    sys.exit(main())
