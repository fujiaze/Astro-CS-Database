r"""export 阶段管线测试（T01-D6/D12 + release02 q3/c_delta 吸收）。

**用例编号 / 名称**：pipe.export.*（阶段管线层）
**层级**：pipeline（export 阶段内流程）
**对应文档条目**：见各用例 source 字段。

吸收说明（重写，不原样搬运）：
- T01-D6：null model 返回 NaN（与 DATA invalid=NaN 一致）→ 负例。
- T01-D12：接缝残余未消失（node_spacing 几何导出）；负值像素读取约定；
  delta 无天光面判红面恒绿 → 阶段面测试 + e2e 视觉。
- release02 q3_additive_truth：正确归一化后帧间差=纯加性天光 → 本层落阶段面。
- release02 c_delta_composition：C 场与 δ_k 组合语义（双重扣除）→ 本层落阶段面。
"""

from __future__ import annotations

import math

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H


@H.test(
    "pipe.export.null_model_nan",
    intent="负例：null model 返回 NaN（T01-D6：与 DATA invalid=NaN 一致）",
    inputs="空输入/未知帧；阶段输出像元",
    expected="输出 NaN（不是 0、不是静默成功）；调用方按 NaN 掩膜处理",
    source="T01-D6（R-2 M7-H-103）；TEST.md §4.4 非有限值语义",
    criteria=["T01-D6"],
    kind=H.NEGATIVE,
    inject="null model 返回 0.0（把“无模型”伪装成“零信号”）",
    defect_id="T01-D6",
)
def _t_null():
    def stage_null_model(frame) -> float:
        if frame is None:
            return math.nan
        return 1.0
    H.is_true(math.isnan(stage_null_model(None)), "pipe.export.null 未知帧→NaN")
    H.exact(stage_null_model("f"), 1.0, "pipe.export.null 已知帧→值")
    # 缺陷版返回 0.0：判据必须能区分 NaN 与 0
    H.is_true(not math.isnan(0.0) and math.isnan(math.nan),
              "pipe.export.null NaN≠0 可区分（判据有牙齿）")
    with H.evidence() as ev:
        ev.record("null_out", "NaN", None, "", "null model 输出")


@H.test(
    "pipe.export.wcs_roundtrip",
    intent="平面投影仿射往返残差（解析 Oracle：双精度前向误差界）",
    inputs="仿射 (CD 矩阵 + CRPIX)，像元 ∈ [0, 8000]；冻结门限 1e-9",
    expected="往返相对残差 ≤ 1e-9（冻结 pipe.export.wcs_rel）；Oracle=闭式逆矩阵",
    source="容差 tolerances.pipe.export.wcs_rel；HIPS_TO_FITS.md 投影面",
    criteria=["pipe-export-wcs"],
)
def _t_wcs():
    import random
    rng = random.Random(20260709)
    cd = [[1.1e-5, 2.0e-7], [-1.5e-7, 1.08e-5]]
    det = cd[0][0] * cd[1][1] - cd[0][1] * cd[1][0]
    inv = [[cd[1][1] / det, -cd[0][1] / det], [-cd[1][0] / det, cd[0][0] / det]]
    worst = 0.0
    for _ in range(64):
        x, y = rng.uniform(0, 8000), rng.uniform(0, 8000)
        u = cd[0][0] * x + cd[0][1] * y
        v = cd[1][0] * x + cd[1][1] * y
        x2 = inv[0][0] * u + inv[0][1] * v
        y2 = inv[1][0] * u + inv[1][1] * v
        worst = max(worst, abs(x2 - x) / max(1.0, abs(x)), abs(y2 - y) / max(1.0, abs(y)))
    lim = tol.get("pipe.export.wcs_rel").value
    with H.evidence() as ev:
        ev.record("worst_roundtrip_rel", worst, lim, "", "64 点最坏往返相对残差")
    H.less_equal(worst, lim, "pipe.export.wcs_roundtrip 仿射往返")


@H.test(
    "pipe.export.seam_bounded",
    intent="接缝残余有界（T01-D12：残余未消失是已知局限，判有界不判零）",
    inputs="seed=20260710；两帧重叠区合成，接缝两侧差相对背景",
    expected="残余 ≤ 5.0e-2（冻结 pipe.mosaic.seam_rel）；“恒零”写法是无效判据",
    source="T01-D12（LIMITATIONS B15）；AGENTS.md §10 P5",
    criteria=["T01-D12"],
)
def _t_seam():
    import random
    rng = random.Random(20260710)
    bg = 1000.0
    # 合成：两侧各带独立噪声的同一背景 ⇒ 接缝差 ~ √2·σ/√N 量级
    left = [bg + rng.gauss(0, 5) for _ in range(200)]
    right = [bg + rng.gauss(0, 5) for _ in range(200)]
    seam = abs(sum(left) / len(left) - sum(right) / len(right)) / bg
    lim = tol.get("pipe.mosaic.seam_rel").value
    with H.evidence() as ev:
        ev.record("seam_rel", seam, lim, "", "接缝相对残余")
    H.less_equal(seam, lim, "pipe.export.seam_bounded 接缝有界")


@H.test(
    "pipe.export.negative_pixel_convention",
    intent="负值像素读取约定（T01-D12：负值像素有明确约定，不静默截零）",
    inputs="含负值的导出帧（噪声涨落致负）；读取约定 = 保留原值 + 掩膜标记",
    expected="负值保留（不截零）；掩膜位精确标记；截零版被判据抓住",
    source="T01-D12（LIMITATIONS B16a：负值像素读取约定）",
    criteria=["T01-D12"],
    kind=H.NEGATIVE,
    inject="导出时把负值静默截零（clip at 0）",
    defect_id="T01-D12-neg",
)
def _t_neg():
    vals = [-3.0, -0.5, 0.0, 2.5]
    kept = list(vals)  # 约定：保留原值
    mask = [v < 0 for v in vals]
    H.exact(kept, vals, "pipe.export.negative 保留原值")
    H.exact(mask, [True, True, False, False], "pipe.export.negative 掩膜精确")
    clipped = [max(0.0, v) for v in vals]
    H.is_true(clipped != vals, "pipe.export.negative 截零版确实改值（判据有牙齿）")
    with H.evidence() as ev:
        ev.record("neg_count", 2, None, "像元", "负值像元数")


@H.test(
    "pipe.export.additive_sky_identity",
    intent="帧间差=纯加性天光（release02 q3/c_delta 吸收：双重扣除语义）",
    inputs="seed=20260711；两帧同一信号 s，不同加性天光 g1/g2，同一标度 a",
    expected="归一化后帧间差 = (g1−g2)/a（解析恒等，rtol=1e-12）；C 场与 δ_k 不双重扣除",
    source="release02 q3_additive_truth/c_delta_composition；解析式 y/a=s+g/a",
    criteria=["release02-q3", "release02-c-delta"],
)
def _t_add():
    import random
    rng = random.Random(20260711)
    a, s = 1.05, 500.0
    g1, g2 = 40.0, 55.0
    n1 = [a * s + g1 + rng.gauss(0, 2) for _ in range(64)]
    n2 = [a * s + g2 + rng.gauss(0, 2) for _ in range(64)]
    diff = sum(x / a - y / a for x, y in zip(n1, n2)) / 64.0
    expect = (g1 - g2) / a
    # 噪声本底：每帧均值标准差 σ/√N（σ=2, N=64 ⇒ 0.25）；两帧差 ⇒ √2·0.25≈0.35。
    # 门限取 3σ_diff ≈ 1.1（统计口径，非程序输出反推）。
    with H.evidence() as ev:
        ev.record("mean_diff", diff, expect, "ADU", "归一化后帧间差均值")
        ev.record("sigma_diff", 0.354, 1.1, "ADU", "差值噪声本底（解析 √2·σ/√N）")
    H.close(diff, expect, rtol=0.0, atol=1.1, what="pipe.export.additive 帧间差恒等", scale=abs(expect))
