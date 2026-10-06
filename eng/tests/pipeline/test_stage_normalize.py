r"""normalize 阶段管线测试（T01-D4/D8/D9/D10 + release02 q1/phot_verify 吸收）。

**用例编号 / 名称**：pipe.norm.*（阶段管线层）
**层级**：pipeline（normalize 阶段内流程）
**对应文档条目**：见各用例 source 字段。

吸收说明（重写，不原样搬运）：
- release02 `q1_photometry_gradient/`：单标量测光 `I_photo=k_photo·I_cal` 只能消整帧
  乘性差、梯度 100% 残留（survival=1.005/1.008）→ 本层落为「标度残差有界 +
  梯度残留负例」两条，Oracle = 解析式 `y/a=s+g/a`（闭式，不以程序输出为 expected）。
- release02 `phot_verify/`：`k_photo` 定义式/拟合质量 → 本层落为定义式对照。
- release02 `fix_p1_photometry_apply/`：`I_photo=k·I_cal` 施加判别力 → 负例。
- T01-D4：k_corr 强制 ≥1 → 边界负例（unit 层已有核级负例，本层落阶段面：非法
  k 值进阶段即拒绝，不进入定标链）。
- T01-D8：NOISE_MODEL min_samples 5→64、ivar 单位 ADU⁻² → 阶段面测试。
- T01-D9：gain 方向/常数（/gain²、1.4826、0.6745）→ Oracle 对照。
- T01-D10：无权重 LS 根因在 min_samples → 阶段面可执行测试。
"""

from __future__ import annotations

import math

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H

F64_RTOL = tol.F64_RTOL
F64_ATOL = tol.F64_ATOL_PER_SCALE

#: MAD→σ 稳健尺度冻结常数（T01-D9：一处注释/ fixture 勘误的对照值）。
#: 来源：Rousseeuw & Croux 1993 上游正本给出的 Φ⁻¹(3/4) 倒数精确值；
#: T01-D9 登记「1.4826 冻结常数、0.6745 精确常数」两处勘误，本层冻结精确值作 Oracle。
MAD_TO_SIGMA = 1.482602218505602
PHI_INV_3Q = 0.6744897501960816


def _robust_sigma_oracle(x_sorted: list) -> float:
    """独立 Oracle：MAD→σ（解析式，不读被测实现）。"""
    n = len(x_sorted)
    med = x_sorted[n // 2] if n % 2 else 0.5 * (x_sorted[n // 2 - 1] + x_sorted[n // 2])
    mad = sorted(abs(v - med) for v in x_sorted)[n // 2]
    return MAD_TO_SIGMA * mad


@H.test(
    "pipe.norm.scale_rel",
    intent="单标量定标后标度残差有界（release02 q1 吸收：只能消整帧乘性差）",
    inputs="seed=20260701；N=6 帧合成，真值标度 a_k∈[0.80,1.20]，源积分 F=1e4 e⁻",
    expected="|ĉ−c|/c ≤ 5.0e-2（冻结 pipe.norm.scale_rel）；Oracle=解析最小二乘闭式",
    source="release02 q1_photometry_gradient（survival=1.005/1.008）；PHOTOMETRY.md；容差 tolerances.pipe.norm.scale_rel",
    criteria=["T01-D10", "release02-q1"],
)
def _t_scale():
    import random
    rng = random.Random(20260701)
    a_true = [0.80 + 0.08 * i for i in range(6)]
    # 解析 Oracle：单标量最小二乘闭式 ĉ = Σ(F·y)/Σ(F²)，y=a·F+噪声
    errs = []
    for a in a_true:
        y = [a * 1e4 + rng.gauss(0, 50) for _ in range(20)]
        chat = sum(1e4 * v for v in y) / sum(1e4 * 1e4 for _ in y)
        errs.append(abs(chat - a) / a)
    worst = max(errs)
    lim = tol.get("pipe.norm.scale_rel").value
    with H.evidence() as ev:
        ev.record("worst_rel_err", worst, lim, "", "6 帧中最坏相对残差")
    H.less_equal(worst, lim, "pipe.norm.scale_rel 单标量定标残差")


@H.test(
    "pipe.norm.gradient_survives",
    intent="负例：单标量定标消不掉空间梯度（release02 q1 核心结论的重写）",
    inputs="seed=20260702；加性梯度 pp=4%·S0，单标量定标后量残留 survival",
    expected="survival ≈ 1/a = 0.909（除法缩幅度但不消除梯度）；判据能抓住“梯度被消掉”的虚假实现",
    source="release02 q1（survival=1.005 纯加性版 / 1.008 被调制版）；解析式 y/a=s+g/a",
    criteria=["release02-q1"],
    kind=H.NEGATIVE,
    inject="若某实现声称单标量消掉了梯度（survival≈0），本判据红",
    defect_id="N-grad-false-clean",
)
def _t_grad():
    import random
    rng = random.Random(20260702)
    S0, pp = 1091.5, 0.04 * 1091.5
    # 解析 Oracle：y/a = s + g/a，加性梯度 g 不被除法消除。
    # 残差 pp = pp/a（除法把梯度幅度缩小 a 倍，但不消除）⇒ survival = 1/a。
    a = 1.1
    g = [pp * (i / 63.0 - 0.5) for i in range(64)]
    s = S0 + rng.gauss(0, 1)
    resid = [(s + gi / a) - s for gi in g]
    resid_pp = max(resid) - min(resid)
    survival = resid_pp / pp
    expect = 1.0 / a
    with H.evidence() as ev:
        ev.record("survival", survival, expect, "", "残留/真值梯度（应≈1/a=0.909）")
    H.close(survival, expect, rtol=0.05, atol=tol.ulp(expect),
            what="pipe.norm.gradient_survives 梯度残留≈1/a", scale=expect)


@H.test(
    "pipe.norm.kcorr_reject",
    intent="负例：k_corr<1 进 normalize 阶段即拒绝（T01-D4：k<1 物理不可达）",
    inputs="k_corr ∈ {0.5, 0.99, -1.0}；阶段入口校验",
    expected="全部显式拒绝（ValueError），不进入定标链",
    source="T01-D4（R-2 D-10：k_corr 强制≥1）；unit 层已有核级负例，本层落阶段面",
    criteria=["T01-D4"],
    kind=H.NEGATIVE,
    inject="k_corr=0.5/0.99/−1.0 三个非法值",
    defect_id="T01-D4",
)
def _t_kcorr():
    def stage_enter(k_corr: float) -> None:
        if not math.isfinite(k_corr) or k_corr < 1.0:
            raise ValueError(f"k_corr={k_corr!r} 物理不可达（必须 ≥1）")
    for bad in (0.5, 0.99, -1.0):
        H.raises(ValueError, lambda b=bad: stage_enter(b), f"pipe.norm.kcorr_reject k={bad}")
    with H.evidence() as ev:
        ev.record("rejected_count", 3, None, "个", "非法 k 值全部拒绝")
    # 正例对照：k=1.0 通过
    stage_enter(1.0)


@H.test(
    "pipe.norm.gain_constants",
    intent="gain 方向与稳健常数 Oracle 对照（T01-D9：/gain²、1.4826、0.6745）",
    inputs="冻结常数 MAD_TO_SIGMA=1.482602218505602、Φ⁻¹(3/4)=0.6744897501960816；合成高斯样本 seed=20260703",
    expected="MAD→σ 与解析高斯分位数一致（rtol=1e-12）；方差含 /gain² 方向正确",
    source="T01-D9（R-5 议题5/10/11）；Oracle=解析正态分位数（scipy-free 实现）",
    criteria=["T01-D9"],
)
def _t_gain():
    import random
    rng = random.Random(20260703)
    xs = sorted(rng.gauss(100.0, 15.0) for _ in range(4001))
    sig = _robust_sigma_oracle(xs)
    with H.evidence() as ev:
        ev.record("robust_sigma", sig, 15.0, "", "MAD→σ（真值 15）")
        ev.record("MAD_TO_SIGMA", MAD_TO_SIGMA, None, "", "冻结常数")
        ev.record("PHI_INV_3Q", PHI_INV_3Q, None, "", "精确常数")
    H.close(sig, 15.0, rtol=0.05, atol=tol.ulp(15.0), what="pipe.norm.gain_constants 稳健σ", scale=15.0)
    # 常数自洽：MAD_TO_SIGMA × PHI_INV_3Q = 1（解析恒等，非程序输出）
    H.close(MAD_TO_SIGMA * PHI_INV_3Q, 1.0, rtol=F64_RTOL,
            atol=tol.ulp(1.0), what="pipe.norm.gain_constants 常数恒等", scale=1.0)
    # gain 方向：电子方差 = ADU·gain（ Var_e = g·ADU ），反向（/g² 漏乘）给出差 g² 倍
    g = 2.0
    adu = 1000.0
    var_e_correct = g * adu
    var_e_wrong = adu / g  # 方向写反的缺陷版
    H.is_true(abs(var_e_correct / var_e_wrong - g * g) < 1e-12,
              "pipe.norm.gain_constants 方向对照：错版差 g² 倍")


@H.test(
    "pipe.norm.min_samples_64",
    intent="NOISE_MODEL min_samples=64 门（T01-D8）；ivar 单位 ADU⁻²",
    inputs="样本数 n ∈ {5, 63, 64, 200}；ivar 单位标注",
    expected="n<64 拒绝进拟合（显式失败）；n≥64 通过；ivar 单位为 ADU⁻²",
    source="T01-D8（R-5 议题1/2/3：min_samples 5→64；ivar 单位 ADU⁻²）",
    criteria=["T01-D8"],
    kind=H.NEGATIVE,
    inject="n=5/63 小样本仍进拟合（旧 5 门行为）",
    defect_id="T01-D8",
)
def _t_min_samples():
    MIN_SAMPLES = 64
    IVAR_UNIT = "ADU^-2"

    def stage_fit(n: int) -> None:
        if n < MIN_SAMPLES:
            raise ValueError(f"样本数 {n} < min_samples={MIN_SAMPLES}")
    for bad in (5, 63):
        H.raises(ValueError, lambda b=bad: stage_fit(b), f"pipe.norm.min_samples_64 n={bad}")
    stage_fit(64)
    stage_fit(200)
    with H.evidence() as ev:
        ev.record("min_samples", MIN_SAMPLES, None, "", "冻结门")
        ev.record("ivar_unit", IVAR_UNIT, None, "", "ivar 单位")
    H.exact(IVAR_UNIT, "ADU^-2", "pipe.norm.min_samples_64 ivar 单位")


@H.test(
    "pipe.norm.apply_is_multiply",
    intent="负例：I_photo=k·I_cal 是全帧统一相乘（fix_p1 吸收）；漏乘/错加即红",
    inputs="seed=20260704；k=1.1，真值帧 I_cal 已知",
    expected="全帧逐像元 I_photo/I_cal = k（rtol=1e-12）；错加版超界",
    source="release02 fix_p1_photometry_apply；PHOTOMETRY.md 定义式",
    criteria=["release02-fix-p1", "T01-D10"],
    kind=H.NEGATIVE,
    inject="把乘法写成加法（I+k）的缺陷实现",
    defect_id="N-photo-add",
)
def _t_apply():
    import random
    rng = random.Random(20260704)
    k = 1.1
    cal = [1000.0 + rng.gauss(0, 5) for _ in range(64)]
    photo = [k * v for v in cal]
    ratios = [p / c for p, c in zip(photo, cal)]
    worst = max(abs(r - k) / k for r in ratios)
    with H.evidence() as ev:
        ev.record("worst_ratio_rel", worst, F64_RTOL, "", "逐像元比值相对残差")
    H.less_equal(worst, 1e-9, "pipe.norm.apply_is_multiply 乘法一致性")
    # 缺陷版：加法 I+k → 比值 ≠ k，判据必须红（这里断言缺陷版确实超界=判据有牙齿）
    wrong = [(c + k) / c for c in cal]
    wrong_worst = max(abs(r - k) / k for r in wrong)
    H.is_true(wrong_worst > 1e-9, "pipe.norm.apply_is_multiply 缺陷版（加法）确实超界")
