r"""WCS 拟合合理性拒绝组（S5 + S5b）—— **同一批、同一个文件**。

## 为什么两条判据必须同批同文件

审核包-R2 `T02-门禁退役与判据清单.md` §2.1 表 **S5b 行逐字**：

> 「**必须与 S5 同批迁移**（三者构成不可分的拒绝组）」

「三者」= 判据 (a) 尺度比可行域、判据 (c) 残差 `rms_px`、判据 (d) 内点 `n_pairs`
——即 `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:731-769` 的**同一个代码块**。
该代码块的拒绝语义是三者**合取**：`accept ⟺ (a) ∧ (c) ∧ (d)`。

拆开迁移会得到一条**在物理上不可能单独成立**的判据：本文件
`wcs.s5b.reject_group_conjunction` 用正本注释里的实证给出反证——

- 894 帧干净数据 ratio ∈ [0.969, 1.02] ⇒ 判据 (a) 对干净解**零判别力**；
- 4 帧镜像帧中 idx342 的 **ratio = 1.146 落在 (a) 的域内** ⇒ 它不是被 (a) 拦下的，
  而是被 (c)/(d) 拦下的（`ipv_wcs.cpp:749-751` 记 M42_M4 误配解
  `n_pairs∈[6,21]`、`rms_px∈[0.94,2.17]`）；
- 其余镜像帧 ratio 1.35–2.55 才是被 (a) 拦下的。

⇒ (a) 单独不能识别镜像解，(c)(d) 单独不覆盖错误尺度解，**只有合取才是这一组拒绝判据**。
本文件末尾两条守卫把这一点钉死：
`wcs.s5b.paired_migration_guard`（结构面）与
`wcs.s5b.reject_group_inseparable_guard`（行为面）。

## 正本与期望值的来源纪律

`expected` 一律取自下列三处，**绝不**由 `wcs_ref.py` 的输出反推：

1. `ipv_wcs.cpp:738/:753/:761` 的**字面阈值**（0.8 / 1.25 / 0.5 / 12）——这些是
   判据的布尔分支位置，属精确档（`wcs_ref.py` 模块头「阈值为什么是精确档」一节）；
2. `ipv_wcs.cpp:720-728` 注释里的**实测读数**（ratio 域、rms_px 域、n_pairs 域、
   镜像帧 ratio 1.35–2.55、idx342 ratio 1.146）——这些是**输入夹具值**；
3. **独立科学 Oracle `astropy`**：`astropy.wcs.utils.proj_plane_pixel_area`
   （`sqrt(|det(CD)|)·3600` 的 FITS 标准口径），登记在
   `docs/engineering/testing/TEST.md` §13 白名单「FITS 与天体测量的独立科学
   Oracle（测试侧，不进产品）」。

`wcs_ref.py` 是**被测口径**（按 `ipv_wcs.cpp` 逐行转写），不是期望值的来源。
"""

from __future__ import annotations

import math

from astropy.wcs import WCS
from astropy.wcs.utils import proj_plane_pixel_area

from . import harness
from . import wcs_ref as ref


# ---------------------------------------------------------------------------
# 夹具常数（全部来自 ipv_wcs.cpp 正本，逐条标注）
# ---------------------------------------------------------------------------

#: 初值光学尺度（角秒/像素）。`ipv_wcs.cpp:720` 逐字「与**初值**光学尺度 s0 之比」。
#: 取一个不在任何判据边界上的值，使 `ratio` 只由 `CD` 决定。
S0_ARCSEC = 1.2

#: 端点外推步长。取 `1e-6`：比 `ratio` 量级（~1）的 1 ulp（~2.2e-16）大 10 个数量级，
#: 远大于浮点噪声，远小于 0.8→1.25 的域宽（0.45），故「越界」判定不受舍入支配。
EPS = 1e-6

#: `ipv_wcs.cpp:721-722` 逐字实测：「894 干净帧实测 ratio∈[0.969,1.02]」
RATIO_CLEAN_LO, RATIO_CLEAN_HI = 0.969, 1.02
#: `ipv_wcs.cpp:722` 逐字实测：「4 镜像帧 ratio 1.35–2.55，其中 idx342 ratio 1.146」
RATIO_MIRROR_OUT_LO, RATIO_MIRROR_OUT_HI = 1.35, 2.55
RATIO_IDX342 = 1.146

#: `ipv_wcs.cpp:750-751` 逐字实测：「真实 874 有头 WCS 干净帧实测 n_pairs∈[21,56]、
#: rms_px∈[0.03,0.322]」
RMS_CLEAN_LO, RMS_CLEAN_HI = 0.03, 0.322
N_PAIRS_CLEAN_LO, N_PAIRS_CLEAN_HI = 21, 56
#: `ipv_wcs.cpp:751` 逐字实测：「而 M42_M4 误配解 n_pairs∈[6,21]、rms_px∈[0.94,2.17]」
RMS_MISPAIR_LO, RMS_MISPAIR_HI = 0.94, 2.17
N_PAIRS_MISPAIR_LO, N_PAIRS_MISPAIR_HI = 6, 21

#: 一对「除判据 (a) 外全部通过」的基准残差与内点。
RMS_OK = 0.2
N_PAIRS_OK = 30

_SRC_RATIO = "ipv_wcs.cpp:720-722,735-738（S5b：尺度比判据 (a)）"
_SRC_RMS = "ipv_wcs.cpp:749-751,753（S5：残差判据 (c)）"
_SRC_PAIRS = "ipv_wcs.cpp:749-751,761（S5：内点判据 (d)）"
_SRC_SEMANTICS = "ipv_wcs.cpp:729,739-745,753-768（fail-closed 失败语义）"


# ---------------------------------------------------------------------------
# 断言助手：把「正本要求 / 未注入 / 注入后」三态一次性表达清楚
# ---------------------------------------------------------------------------

def _gate(cd, s0=S0_ARCSEC, rms=RMS_OK, n_pairs=N_PAIRS_OK, **kw):
    """对 `wcs_ref` 的薄封装，参数序与 `ipv_wcs.cpp:731-769` 的输入一致。"""
    return ref.apply_plausibility_gate(*cd, s0, rms, n_pairs, **kw)


def _expect_verdict(spec_accept, faithful, injected, what: str) -> None:
    """**成对双向**断言（`CRITERION_QUALITY` E4/E7：`expected` 独立 / 双向）。

    - `spec_accept` = 正本逐字要求的接受/拒绝（不来自任何实现）；
    - `faithful`  = 未注入缺陷时被测口径的判定，**必须**等于 `spec_accept`；
    - `injected`  = 注入具名缺陷后的判定，**必须不等于** `spec_accept`
      ——否则这条判据恒绿，是一条没有牙齿的判据
      （`TEST.md` §5 逐字「负例必须可红：正确实现给绿，注入缺陷给红」）。
    """
    if faithful.success != spec_accept:
        raise harness.CheckFailure(
            f"{what}: 正本要求 success={spec_accept}，未注入时被测口径给 "
            f"success={faithful.success}（error={faithful.error!r}）")
    if injected.success == spec_accept:
        raise harness.CheckFailure(
            f"{what}: 注入缺陷后判定仍为 success={spec_accept} ⇒ **判据没有牙齿**，"
            f"这是一条无效负例（注入未生效，或该缺陷不在判据覆盖面内）")


def _expect_bound(measured, limit, injected_measured, what: str, unit: str = "") -> None:
    """数值档的双向断言：未注入时在门限内，注入后超界。"""
    if not (measured <= limit):
        raise harness.CheckFailure(
            f"{what}: 未注入时读数 {measured!r} {unit} 已超门限 {limit!r}")
    if not (injected_measured > limit):
        raise harness.CheckFailure(
            f"{what}: 注入缺陷后读数 {injected_measured!r} {unit} 仍在门限 "
            f"{limit!r} 内 ⇒ **判据没有牙齿**")


def _expect_reject_semantics(faithful, what: str) -> None:
    """正本 `ipv_wcs.cpp:739-745,753-768` 的失败语义，逐项判。"""
    harness.is_false(faithful.success, f"{what}: 正本要求失败时 success=false")
    harness.is_true(faithful.error != "", f"{what}: 正本要求 error 非空")
    harness.is_true(ref.ERROR_PREFIX in faithful.error,
                    f"{what}: error 缺正本前缀 {ref.ERROR_PREFIX!r}")
    harness.is_true(ref.ERROR_TAG_RATIO in faithful.error
                    or ref.ERROR_TAG_DISP in faithful.error,
                    f"{what}: error 缺溯源标记 {ref.ERROR_TAG_RATIO!r} / "
                    f"{ref.ERROR_TAG_DISP!r}")
    harness.is_true(faithful.rejected_by != "",
                    f"{what}: 正本在拒绝处立即 return，不得给下游留任何可用解")


# ---------------------------------------------------------------------------
# S5b —— 判据 (a) 尺度比可行域
# ---------------------------------------------------------------------------

#: 正向边界表。`ratio` 为**按目标值反解得到的 CD** 的实际尺度比，
#: `accept` 为 `ipv_wcs.cpp:738` 逐字不等号直接给出的判定。
_RATIO_TABLE = (
    # (标签, 目标 ratio, accept, 依据)
    ("下端点 0.8 本身", ref.RATIO_LO, True,
     ":738 `ratio < 0.8` 严格 `<` ⇒ 端点不触发 ⇒ 接受"),
    ("下端点外 0.8−ε", ref.RATIO_LO - EPS, False,
     ":738 `ratio < 0.8` ⇒ 越下界即拒"),
    ("干净帧实测下端 0.969", RATIO_CLEAN_LO, True,
     ":721 「894 干净帧实测 ratio∈[0.969,1.02]」，域内 ⇒ 接受"),
    ("域内 1.0", 1.0, True, "域内"),
    ("干净帧实测上端 1.02", RATIO_CLEAN_HI, True,
     ":721 同上，域内上端 ⇒ 接受"),
    ("镜像帧 idx342 ratio 1.146", RATIO_IDX342, True,
     ":722 「4 镜像帧 ratio 1.35–2.55，其中 idx342 ratio 1.146」⇒ 1.146 **落在域内**；"
     "它是被 (c)/(d) 拦下的，不是被 (a) 拦下的"),
    ("上端点 1.25 本身", ref.RATIO_HI, True,
     ":738 `ratio > 1.25` 严格 `>` ⇒ 端点不触发 ⇒ 接受"),
    ("上端点外 1.25+ε", ref.RATIO_HI + EPS, False,
     ":738 `ratio > 1.25` ⇒ 越上界即拒"),
    ("镜像帧域外下端 1.35", RATIO_MIRROR_OUT_LO, False,
     ":722 「4 镜像帧 ratio 1.35–2.55」⇒ 域外 ⇒ 拒"),
    ("镜像帧域外上端 2.55", RATIO_MIRROR_OUT_HI, False,
     ":722 同上 ⇒ 域外 ⇒ 拒"),
)


@harness.test(
    "wcs.s5b.scale_ratio_boundary_table",
    intent="判据 (a) 的接受域是**闭区间** [0.8, 1.25]：两端点本身接受，"
           "端点外一步即拒；并逐点覆盖正本注释里的实测 ratio 域与镜像帧 ratio。",
    inputs=f"s0={S0_ARCSEC}\"/px；按目标 ratio 反解 CD；表 {len(_RATIO_TABLE)} 行",
    expected="逐行 accept/reject 与 ipv_wcs.cpp:738 的严格不等号逐字一致；"
             "端点 0.8/1.25 接受，端点外一步拒绝；idx342 的 1.146 接受",
    source=_SRC_RATIO,
    criteria=("S5b", "S5"),
)
def _case_ratio_table() -> None:
    with harness.evidence() as ev:
        for label, target, accept, why in _RATIO_TABLE:
            cd = ref.cd_for_ratio(target, S0_ARCSEC)
            _, scale, ratio = ref.solve_scale_ratio(*cd, S0_ARCSEC)
            got = _gate(cd)
            if got.success != accept:
                raise harness.CheckFailure(
                    f"尺度比 {label}: 正本要求 accept={accept}，实测 {got.success}。"
                    f"（ratio={ratio!r} scale={scale!r}s/px，依据 {why}）")
            if accept:
                harness.exact(got.error, "", f"{label}: 接受时不得留拒绝文本")
                harness.exact(got.rejected_by, "", f"{label}: 接受时不得标记拒绝分支")
            else:
                harness.exact(got.rejected_by, ref.GATE_RATIO,
                              f"{label}: 拒绝必须由判据 (a) 触发")
                _expect_reject_semantics(got, f"尺度比 {label}")
            ev.record(f"ratio[{label}]", ratio, unit="", note=why)
        # 端点逐位精确性：判据端点不许被浮点舍入挪走
        for endpoint in (ref.RATIO_LO, ref.RATIO_HI):
            cd = ref.cd_for_ratio(endpoint, S0_ARCSEC)
            _, _, ratio = ref.solve_scale_ratio(*cd, S0_ARCSEC)
            harness.exact(ratio, endpoint,
                          f"端点 {endpoint}: 反解出的尺度比必须逐位等于端点本身，"
                          f"否则端点接受/拒绝的断言只是在测浮点噪声")
            ev.record(f"端点逐位相等[{endpoint}]", ratio)


@harness.test(
    "wcs.s5b.scale_ratio_astropy_oracle",
    intent="被测口径的像素尺度 `sqrt(|det(CD)|)·3600` 必须等于 FITS 标准的独立 "
           "口径 `astropy.wcs.utils.proj_plane_pixel_area`（`TEST.md` §13 白名单）。",
    inputs="含 det>0 / det<0 / 非对称(带 shear) CD 的 9 个夹具，覆盖 1e0–1e1 \"/px",
    expected="逐夹具 |被测 − astropy| ≤ atol + rtol·|astropy|，落 f64 非归约档；"
             "两侧读数逐条记入 evidence",
    source=_SRC_RATIO + "；独立 Oracle = astropy.wcs.utils.proj_plane_pixel_area"
                        "（TEST.md §13「FITS 与天体测量的独立科学 Oracle」）",
    criteria=("S5b",),
)
def _case_astropy_oracle() -> None:
    cases = []
    for ratio in (0.8, 0.969, 1.0, 1.146, 1.25, 1.35, 2.55):
        for sign in (1, -1):
            cases.append((f"r={ratio}/det{sign:+d}", ratio, sign, 0.0))
    cases.append(("r=1.0/det+1/shear0.3", 1.0, 1, 0.3))
    cases.append(("r=1.25/det-1/shear-0.3", 1.25, -1, -0.3))

    with harness.evidence() as ev:
        for label, ratio, sign, shear in cases:
            cd = ref.cd_for_ratio(ratio, S0_ARCSEC, det_sign=sign, shear=shear)
            _, scale, _ = ref.solve_scale_ratio(*cd, S0_ARCSEC)
            w = WCS(naxis=2)
            w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
            w.wcs.crval = [0.0, 0.0]
            w.wcs.crpix = [512.0, 512.0]
            w.wcs.cd = [[cd[0], cd[1]], [cd[2], cd[3]]]
            oracle = math.sqrt(proj_plane_pixel_area(w)) * 3600.0
            harness.close(scale, oracle, rtol=1e-12, atol=1e-13 * abs(oracle),
                          what=f"像素尺度 {label} vs astropy", scale=abs(oracle))
            ev.record(f"scale_arcsec[{label}]", scale, oracle, unit="arcsec/px",
                      note="门限=astropy 独立口径")
        # 伪实现证伪：|det| 必须是真行列式，不能写成 |cd11·cd22|
        cd = ref.cd_for_ratio(1.0, S0_ARCSEC, shear=0.3)
        naive = abs(cd[0] * cd[3]) * 3600.0
        _, scale, _ = ref.solve_scale_ratio(*cd, S0_ARCSEC)
        harness.is_true(naive != scale,
                        "带 shear 的 CD 上 |cd11·cd22| 与真行列式必须不同；"
                        "若相同则本 Oracle 无判别力")
        ev.record("伪实现 |cd11·cd22|·3600", naive, unit="arcsec/px",
                  note="与真行列式口径之差即为该 Oracle 的判别力")


@harness.test(
    "wcs.s5b.det_sign_independence",
    intent="正本 `ipv_wcs.cpp:723-726` 逐字「**不以 det 符号做全局判据**：det 手性随"
           "调用方 U 约定而变」⇒ det>0 与 det<0 但 |det| 相同的两个 CD，接受判定与"
           "拒绝文本必须完全一致。",
    inputs="同 |det| 的 CD 三对：diag 对角对角、diag 对角反对角、带 shear 的上三角",
    expected="每一对在 4 个残差/内点组合上的 (success, error, rejected_by) 逐项相等",
    source=_SRC_RATIO + " + :723-726（手性无关的限定条款）",
    criteria=("S5b",),
)
def _case_det_sign() -> None:
    with harness.evidence() as ev:
        for rms, n_pairs in ((RMS_OK, N_PAIRS_OK), (0.5, 12),
                             (RMS_MISPAIR_HI, N_PAIRS_OK),
                             (RMS_OK, N_PAIRS_MISPAIR_LO)):
            for label, ratio, shear in (("对角", 1.0, 0.0), ("带剪切", 1.02, 0.3)):
                pos = ref.cd_for_ratio(ratio, S0_ARCSEC, det_sign=+1, shear=shear)
                neg = ref.cd_for_ratio(ratio, S0_ARCSEC, det_sign=-1, shear=shear)
                harness.exact(ref.cd_det(*pos), -ref.cd_det(*neg),
                              f"{label}: 两个 CD 的 det 必须逐位互为相反数")
                harness.exact(abs(ref.cd_det(*pos)), abs(ref.cd_det(*neg)),
                              f"{label}: 两个 CD 的 |det| 必须逐位相同")
                g_pos = _gate(pos, rms=rms, n_pairs=n_pairs)
                g_neg = _gate(neg, rms=rms, n_pairs=n_pairs)
                harness.exact(g_pos.success, g_neg.success,
                              f"{label} rms={rms} n_pairs={n_pairs}: det 符号不得改变接受判定")
                harness.exact(g_pos.error, g_neg.error,
                              f"{label} rms={rms} n_pairs={n_pairs}: det 符号不得改变拒绝文本")
                harness.exact(g_pos.rejected_by, g_neg.rejected_by,
                              f"{label} rms={rms} n_pairs={n_pairs}: det 符号不得改变拒绝分支")
                ev.record(f"det符号判定[{label}/rms={rms}/np={n_pairs}]",
                          (g_pos.success, g_neg.success))


@harness.test(
    "wcs.s5b.applicability_s0_guard",
    intent="正本 `ipv_wcs.cpp:734` 逐字适用域守卫 `if (isfinite(s0) && s0 > 0.0)`："
           "`s0` 非有限或 ≤ 0 时判据 (a) **不适用**（该 CD 的尺度比不被检查）。"
           "这条按正本写成**正例**，不是缺陷。",
    inputs="s0 ∈ {NaN, +Inf, −Inf, 0.0, −1.0}，CD 取 ratio=2.55（域外），"
           "rms_px=0.2、n_pairs=30 使 (c)(d) 通过",
    expected="五个 s0 下均 success=true：判据 (a) 被守卫跳过，(c)(d) 通过即接受",
    source="ipv_wcs.cpp:734（S5b 适用域守卫）",
    criteria=("S5b",),
)
def _case_s0_applicability() -> None:
    cd = ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC)
    with harness.evidence() as ev:
        # 先证守卫真的存在：s0 合法时同一个 CD 必被拒
        harness.is_false(_gate(cd, s0=S0_ARCSEC).success,
                         "适用域内 ratio=2.55 必须被 (a) 拒绝")
        for label, s0 in (("NaN", math.nan), ("+Inf", math.inf),
                          ("−Inf", -math.inf), ("0", 0.0), ("负", -1.0)):
            got = _gate(cd, s0=s0)
            harness.is_true(got.success,
                            f"s0={label}: 判据 (a) 不适用时应由 (c)(d) 判定并接受；"
                            f"实测 success={got.success} error={got.error!r}")
            harness.exact(got.error, "", f"s0={label}: 接受时 error 必须为空")
            ev.record(f"s0={label} 时判据 (a) 是否**不**适用",
                      not ref._scale_gate_applies(s0),
                      note=f"守卫 :734 `isfinite(s0) && s0>0` 不成立 ⇒ (a) 被跳过；"
                           f"判定 success={got.success}")
        # 守卫下的反向：s0 合法时三道门仍照判，且顺序是 (a) → (c) → (d)
        #（`ipv_wcs.cpp:731-769` 是顺序 if + 立即 return）
        in_domain = ref.cd_for_ratio(1.0, S0_ARCSEC)
        harness.exact(_gate(in_domain, s0=S0_ARCSEC, rms=RMS_MISPAIR_LO,
                            n_pairs=N_PAIRS_OK).rejected_by,
                      ref.GATE_RMS,
                      "s0 合法且 ratio 在域内时，判据 (c) 仍照常生效")
        harness.exact(_gate(cd, s0=S0_ARCSEC, rms=RMS_MISPAIR_LO,
                            n_pairs=N_PAIRS_MISPAIR_LO).rejected_by,
                      ref.GATE_RATIO,
                      "三门同时触发时判据 (a) 优先（:737 在 :753 之前）")
        harness.exact(_gate(in_domain, s0=S0_ARCSEC, rms=RMS_MISPAIR_LO,
                            n_pairs=N_PAIRS_MISPAIR_LO).rejected_by,
                      ref.GATE_RMS,
                      "(a) 放行时判据 (c) 优先于 (d)")


@harness.test(
    "wcs.s5b.det_nonfinite_rejects",
    intent="正本 `ipv_wcs.cpp:737` `!std::isfinite(det)`：在判据 (a) 的**适用域内**，"
           "非有限 det 一律拒绝（fail-closed），并留确定性错误文本。",
    inputs="CD11 ∈ {NaN, +Inf, −Inf}，s0=1.2（适用域），rms_px=0.2、n_pairs=30",
    expected="三例均 success=false、error 非空含 RESCUE F-9、rejected_by='ratio'",
    source="ipv_wcs.cpp:737（S5b 非有限 det 分支）",
    criteria=("S5b",),
)
def _case_det_nonfinite() -> None:
    k = S0_ARCSEC / 3600.0
    with harness.evidence() as ev:
        for label, bad in (("NaN", math.nan), ("+Inf", math.inf), ("−Inf", -math.inf)):
            got = _gate((bad, 0.0, 0.0, k), s0=S0_ARCSEC)
            _expect_reject_semantics(got, f"det={label}")
            harness.exact(got.rejected_by, ref.GATE_RATIO,
                          f"det={label}: 必须由判据 (a) 拒绝")
            harness.is_true(str(got.error).count("nan") >= 1
                            or str(got.error).count("inf") >= 1,
                            f"det={label}: error 应回显非有限读数，实际 {got.error!r}")
            ev.record(f"det={label} 的 ratio", ref.solve_scale_ratio(
                bad, 0.0, 0.0, k, S0_ARCSEC)[2], note="非有限 ⇒ 不可判，fail-closed 拒绝")


# ---------------------------------------------------------------------------
# S5 —— 判据 (c) 残差门 与 判据 (d) 内点门
# ---------------------------------------------------------------------------

_RMS_TABLE = (
    ("干净帧实测下端 0.03", RMS_CLEAN_LO, True),
    ("干净帧实测上端 0.322", RMS_CLEAN_HI, True),
    ("门限 0.5 本身", ref.RMS_MAX_PX, True),
    ("门限外 0.5+ε", ref.RMS_MAX_PX + EPS, False),
    ("M42 误配解下端 0.94", RMS_MISPAIR_LO, False),
    ("M42 误配解上端 2.17", RMS_MISPAIR_HI, False),
    ("非有限 NaN", math.nan, False),
)

_NPAIRS_TABLE = (
    ("干净帧实测上端 56", N_PAIRS_CLEAN_HI, True),
    # ⚠ 正本 :750-751 的两个域在 21 处**重叠**：干净帧 n_pairs∈[21,56]、
    # M42 误配解 n_pairs∈[6,21]。21 既在干净域也在误配域 ⇒ 单看 n_pairs
    # 区分不了二者，必须靠合取（误配解的 rms_px∈[0.94,2.17] 全部越 0.5 门）。
    ("干净帧下端 / M42 误配解上端 21（两域重叠点）", N_PAIRS_CLEAN_LO, True),
    ("门限 12 本身", ref.N_PAIRS_MIN, True),
    ("门限外 11", ref.N_PAIRS_MIN - 1, False),
    ("M42 误配解域内 20（仍 ≥12）", N_PAIRS_CLEAN_LO - 1, True),
    ("M42 误配解下端 6", N_PAIRS_MISPAIR_LO, False),
)


@harness.test(
    "wcs.s5.rms_gate_boundary_table",
    intent="判据 (c)：`ipv_wcs.cpp:753` 逐字 `!isfinite(rms_px) || rms_px > 0.5`。"
           "`>` 严格 ⇒ `rms_px = 0.5` **接受**；`!isfinite` 分支 ⇒ NaN 拒绝。",
    inputs=f"CD 取 ratio=1.0（域内）、n_pairs=30；rms_px 表 {len(_RMS_TABLE)} 行",
    expected="逐行 accept/reject 与 :753 一致；门限 0.5 接受、0.5+ε 与 NaN 拒绝",
    source=_SRC_RMS,
    criteria=("S5",),
)
def _case_rms_table() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        for label, rms, accept in _RMS_TABLE:
            got = _gate(cd, rms=rms)
            if got.success != accept:
                raise harness.CheckFailure(
                    f"rms_px={label}: 正本要求 accept={accept}，实测 {got.success} "
                    f"（error={got.error!r}）")
            if not accept:
                harness.exact(got.rejected_by, ref.GATE_RMS,
                              f"rms_px={label}: 必须由判据 (c) 拒绝")
                _expect_reject_semantics(got, f"rms_px={label}")
            ev.record(f"rms_px[{label}]", rms, unit="px",
                      note=f"accept={accept}（:753 严格 >）")


@harness.test(
    "wcs.s5.n_pairs_gate_boundary_table",
    intent="判据 (d)：`ipv_wcs.cpp:761` 逐字 `n_pairs < 12`。`<` 严格 ⇒ "
           "`n_pairs = 12` **接受**；11 与 6 拒绝。",
    inputs=f"CD 取 ratio=1.0（域内）、rms_px=0.2；n_pairs 表 {len(_NPAIRS_TABLE)} 行",
    expected="逐行 accept/reject 与 :761 一致；门限 12 接受、11 与 6 拒绝",
    source=_SRC_PAIRS,
    criteria=("S5",),
)
def _case_pairs_table() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        for label, n_pairs, accept in _NPAIRS_TABLE:
            got = _gate(cd, n_pairs=n_pairs)
            if got.success != accept:
                raise harness.CheckFailure(
                    f"n_pairs={label}: 正本要求 accept={accept}，实测 {got.success} "
                    f"（error={got.error!r}）")
            if not accept:
                harness.exact(got.rejected_by, ref.GATE_PAIRS,
                              f"n_pairs={label}: 必须由判据 (d) 拒绝")
                _expect_reject_semantics(got, f"n_pairs={label}")
            ev.record(f"n_pairs[{label}]", n_pairs, note=f"accept={accept}（:761 严格 <）")
        # 域重叠登记：n_pairs 单门无法区分干净帧与 M42 误配解（见 _NPAIRS_TABLE 注释）
        ev.record("干净帧 n_pairs 域", f"[{N_PAIRS_CLEAN_LO}, {N_PAIRS_CLEAN_HI}]",
                  note="ipv_wcs.cpp:750")
        ev.record("M42 误配解 n_pairs 域", f"[{N_PAIRS_MISPAIR_LO}, {N_PAIRS_MISPAIR_HI}]",
                  note="ipv_wcs.cpp:751")
        ev.record("两域重叠点 n_pairs", N_PAIRS_CLEAN_LO,
                  note="重叠 ⇒ n_pairs 单门零判别力；分离靠 rms_px（误配解全部 >0.5）")
        overlap = _gate(cd, rms=RMS_MISPAIR_LO, n_pairs=N_PAIRS_CLEAN_LO)
        harness.is_false(overlap.success,
                         "重叠点 21 配上误配解的残差 0.94 必须被拒（合取才有判别力）")
        harness.exact(overlap.rejected_by, ref.GATE_RMS,
                      "重叠点上真正起作用的是判据 (c)")


# ---------------------------------------------------------------------------
# 失败语义（三个分支合起来判）
# ---------------------------------------------------------------------------

@harness.test(
    "wcs.s5.reject_failure_semantics",
    intent="正本 `ipv_wcs.cpp:729` 逐字「失败返回确定性 fail-closed 且 error 非空"
           "（FD-05 F4 语义）」与 `:745/:759/:767` 的立即 return：拒绝时"
           "success=false、error 非空且含确定性文本，**不静默改算法、不返哨兵解**。",
    inputs="三个拒绝分支各一例：(a) ratio=2.55；(c) rms_px=2.17；(d) n_pairs=6",
    expected="逐例 success=false、error 非空且含 'WCS 拒绝' 与溯源标记；"
             "接受路径 error 恒为空串",
    source=_SRC_SEMANTICS,
    criteria=("S5", "S5b"),
)
def _case_failure_semantics() -> None:
    ok_cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    mirror_cd = ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC)
    with harness.evidence() as ev:
        rejects = (
            ("判据 (a)", _gate(mirror_cd), ref.GATE_RATIO),
            ("判据 (c)", _gate(ok_cd, rms=RMS_MISPAIR_HI), ref.GATE_RMS),
            ("判据 (d)", _gate(ok_cd, n_pairs=N_PAIRS_MISPAIR_LO), ref.GATE_PAIRS),
        )
        for label, got, expect_branch in rejects:
            _expect_reject_semantics(got, label)
            harness.exact(got.rejected_by, expect_branch, f"{label}: 拒绝分支")
            ev.record(f"{label} 的 error 文本", got.error)
        accepted = _gate(ok_cd, rms=RMS_OK, n_pairs=N_PAIRS_OK)
        harness.exact(accepted.error, "", "接受路径 error 必须为空串（不得预填告警）")
        harness.exact(accepted.rejected_by, "", "接受路径不得标记任何拒绝分支")
        ev.record("接受路径 error", accepted.error)


# ---------------------------------------------------------------------------
# 成对守卫 1：合取与「不可拆」的行为面
# ---------------------------------------------------------------------------

#: 三道门各自的**专属证人**：只有该门能拦下它（其余两道门放行）。
_GATE_WITNESSES = (
    # (门, 证人标签, CD, rms, n_pairs, 正本依据)
    (ref.GATE_RATIO, "镜像帧 ratio=2.55",
     ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC), RMS_OK, N_PAIRS_OK,
     "ipv_wcs.cpp:722「4 镜像帧 ratio 1.35–2.55」"),
    (ref.GATE_RMS, "M42 误配解 rms_px=2.17",
     ref.cd_for_ratio(1.0, S0_ARCSEC), RMS_MISPAIR_HI, N_PAIRS_OK,
     "ipv_wcs.cpp:751「M42_M4 误配解 … rms_px∈[0.94,2.17]」"),
    (ref.GATE_PAIRS, "M42 误配解 n_pairs=6、rms_px=0.2",
     ref.cd_for_ratio(1.0, S0_ARCSEC), RMS_OK, N_PAIRS_MISPAIR_LO,
     "ipv_wcs.cpp:751「M42_M4 误配解 n_pairs∈[6,21]」"),
)


@harness.test(
    "wcs.s5b.reject_group_conjunction",
    intent="判据组 = {尺度比域, rms 门, n_pairs 门} 三者**合取**："
           "`accept ⟺ (a) ∧ (c) ∧ (d)`。逐个专属证人断言「只有对应那一道门拦下它」，"
           "从而证明三道门都不可缺；并用 idx342 证明 (a) 单独不覆盖镜像解。",
    inputs="三个专属证人（ratio=2.55 / rms_px=2.17 / n_pairs=6）"
           "与 idx342（ratio=1.146、rms_px=1.5、n_pairs=8）",
    expected="三证人各自 rejected_by 为对应门；idx342 的 rejected_by 是 (c) 或 (d)"
             "而**不是** (a)——实证 (a) 与 (c)(d) 不可拆",
    source=(_SRC_RATIO + " + " + _SRC_RMS + " + " + _SRC_PAIRS
            + "；合取语义见 ipv_wcs.cpp:727-729 与 :731-769 同一代码块"),
    criteria=("S5b", "S5"),
)
def _case_conjunction() -> None:
    with harness.evidence() as ev:
        for gate, label, cd, rms, n_pairs, why in _GATE_WITNESSES:
            got = _gate(cd, rms=rms, n_pairs=n_pairs)
            harness.is_false(got.success, f"证人「{label}」必须被拒绝（{why}）")
            harness.exact(got.rejected_by, gate, f"证人「{label}」的拒绝分支（{why}）")
            ev.record(f"证人「{label}」", got.rejected_by, note=why)

        # idx342：ratio=1.146 落在 (a) 的域内 ⇒ (a) 对它零判别力；
        # 它之所以被拦下，靠的是 (c)/(d)。这就是「不可拆」的实证。
        cd342 = ref.cd_for_ratio(RATIO_IDX342, S0_ARCSEC)
        _, _, ratio342 = ref.solve_scale_ratio(*cd342, S0_ARCSEC)
        harness.is_true(ref.RATIO_LO <= ratio342 <= ref.RATIO_HI,
                        "idx342 的 ratio 必须落在判据 (a) 域内（正本 :722）")
        got342 = _gate(cd342, rms=1.5, n_pairs=8)
        harness.is_false(got342.success, "idx342 型镜像解必须被拒绝")
        harness.is_true(got342.rejected_by in (ref.GATE_RMS, ref.GATE_PAIRS),
                        "idx342 型镜像解必须由 (c) 或 (d) 拦下；若判为 (a) 说明 "
                        "夹具与正本注释矛盾")
        harness.is_false(got342.rejected_by == ref.GATE_RATIO,
                        "idx342 的 ratio 在域内，判据 (a) 不可能拦下它")
        ev.record("idx342 ratio", ratio342, unit="", note="落在 [0.8,1.25] 域内")
        ev.record("idx342 实际拒绝分支", got342.rejected_by,
                  note="⇒ (a) 单独识别不了镜像解")


# ---------------------------------------------------------------------------
# 成对守卫 2：S5 与 S5b 不得被拆到不同文件（本条是硬约束）
# ---------------------------------------------------------------------------

@harness.test(
    "wcs.s5b.paired_migration_guard",
    intent="审核包-R2 T02 §2.1 S5b 行逐字「**必须与 S5 同批迁移**（三者构成不可分的"
           "拒绝组）」。结构守卫：S5 与 S5b 必须都出现在**本模块**内，且仓内任何"
           "其他模块都不得单独承载其中之一——一旦有人把 S5 挪走或把 S5b 挪走，此条判红。",
    inputs="harness.registered() 全量用例元数据（criteria + func.__module__）",
    expected="S5 与 S5b 在本模块各有 ≥1 条用例；其他模块 0 条；本模块不夹带其他判据",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.1 表 S5 / S5b 行",
    criteria=("S5b", "S5"),
)
def _case_paired_guard() -> None:
    me = __name__
    all_cases = harness.registered()
    mine = [c for c in all_cases if c.func.__module__ == me]
    other = [c for c in all_cases if c.func.__module__ != me]

    with harness.evidence() as ev:
        s5_here = [c.id for c in mine if "S5" in c.criteria]
        s5b_here = [c.id for c in mine if "S5b" in c.criteria]
        harness.is_true(s5_here, f"S5 必须与 S5b 同批：本模块 0 条 S5 用例")
        harness.is_true(s5b_here, f"S5b 必须与 S5 同批：本模块 0 条 S5b 用例")

        s5_else = [c.id for c in other if "S5" in c.criteria]
        s5b_else = [c.id for c in other if "S5b" in c.criteria]
        harness.is_false(s5_else,
                         "S5 被迁移到本模块之外的用例，违反 S5b 行「必须与 S5 同批迁移」："
                         f"{s5_else}")
        harness.is_false(s5b_else,
                         "S5b 被迁移到本模块之外的用例，违反 S5b 行「必须与 S5 同批迁移」："
                         f"{s5b_else}")

        stray = sorted({cr for c in mine for cr in c.criteria} - {"S5", "S5b"})
        harness.is_false(stray,
                         "本模块只承载 S5 + S5b 这一不可分拒绝组，不得夹带其他判据："
                         f"{stray}")

        ev.record("本模块 S5 用例数", len(s5_here))
        ev.record("本模块 S5b 用例数", len(s5b_here))
        ev.record("其他模块携带 S5 的用例", s5_else)
        ev.record("其他模块携带 S5b 的用例", s5b_else)


# ---------------------------------------------------------------------------
# 负例：每一个都必须真的能红
# ---------------------------------------------------------------------------

@harness.test(
    "wcs.s5b.negative.det_sign_used",
    intent="判据 (a) 必须手性无关。负例：把 `std::fabs(det)` 写成 `det`"
           "（`ipv_wcs.cpp:735`），det<0 的合法解会因 `sqrt(负数)=NaN` 被误拒。",
    inputs="det = −k² 的合法解（ratio=1.0、rms_px=0.2、n_pairs=30）与 det = +k² 的同值解",
    expected="正本：两者都接受（error 均为空）；注入后：det<0 的被拒、det>0 的仍接受",
    source="ipv_wcs.cpp:723-726（正本限定）+ :735（被注入的一行）",
    kind=harness.NEGATIVE,
    inject="`std::fabs(det)` → `det`（去掉绝对值）",
    defect_id="WCS-N-DETNOSIGN",
    criteria=("S5b",),
)
def _case_neg_det_sign() -> None:
    cd_pos = ref.cd_for_ratio(1.0, S0_ARCSEC, det_sign=+1)
    cd_neg = ref.cd_for_ratio(1.0, S0_ARCSEC, det_sign=-1)
    with harness.evidence() as ev:
        faithful_neg = _gate(cd_neg)
        injected_neg = _gate(cd_neg, use_abs_det=False)
        _expect_verdict(True, faithful_neg, injected_neg,
                        "det<0 的合法解（ratio=1.0）")
        harness.is_true(_gate(cd_pos, use_abs_det=False).success,
                        "det>0 的解不受 fabs 缺陷影响（证明该缺陷确实来自 fabs）")
        _expect_reject_semantics(injected_neg, "注入后的 det<0 解")
        ev.record("注入后 det<0 解的 scale_arcsec",
                  ref.solve_scale_ratio(*cd_neg, S0_ARCSEC, use_abs_det=False)[1],
                  unit="arcsec/px", note="NaN ⇒ ratio=NaN ⇒ 触发 !isfinite 分支")
        ev.record("注入后 det<0 解的 ratio",
                  ref.solve_scale_ratio(*cd_neg, S0_ARCSEC, use_abs_det=False)[2],
                  note="正本要求接受；注入后拒绝")


@harness.test(
    "wcs.s5b.negative.ratio_gate_removed",
    intent="判据 (a) 不可摘。负例：整块摘掉 `ipv_wcs.cpp:737-746`，ratio=2.55 的镜像解"
           "（正本 :722 实测域）被接受。",
    inputs="ratio=2.55、rms_px=0.2、n_pairs=30 的镜像帧型解",
    expected="正本：拒绝（rejected_by='ratio'）；注入后：接受 ⇒ 判据 (a) 有牙齿",
    source="ipv_wcs.cpp:722（证人）+ :737-746（被摘掉的判据 (a)）",
    kind=harness.NEGATIVE,
    inject="摘掉判据 (a) 整个代码块（`enable_ratio_gate=False`）",
    defect_id="WCS-N-NORATIO",
    criteria=("S5b", "S5"),
)
def _case_neg_ratio_gate_removed() -> None:
    cd = ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC)
    with harness.evidence() as ev:
        faithful = _gate(cd)
        injected = _gate(cd, enable_ratio_gate=False)
        _expect_verdict(False, faithful, injected, "镜像帧型解 ratio=2.55")
        harness.exact(faithful.rejected_by, ref.GATE_RATIO, "正本拒绝分支")
        _, _, ratio = ref.solve_scale_ratio(*cd, S0_ARCSEC)
        ev.record("镜像帧 ratio（摘门后被接受）", ratio,
                  note=f"正本域上限 {ref.RATIO_HI}，超界 "
                       f"{ratio / ref.RATIO_HI:.3f}×")


@harness.test(
    "wcs.s5.negative.rms_gate_removed",
    intent="判据 (c) 不可摘。负例：摘掉 `ipv_wcs.cpp:753-760`，rms_px=2.17 的 M42 误配解"
           "（正本 :751 实测域）被接受。",
    inputs="ratio=1.0（域内）、rms_px=2.17、n_pairs=30",
    expected="正本：拒绝（rejected_by='rms_px'）；注入后：接受 ⇒ 判据 (c) 有牙齿",
    source="ipv_wcs.cpp:751（证人）+ :753（被摘掉的判据 (c)）",
    kind=harness.NEGATIVE,
    inject="摘掉判据 (c) 整个代码块（`enable_rms_gate=False`）",
    defect_id="WCS-N-NORMS",
    criteria=("S5", "S5b"),
)
def _case_neg_rms_gate_removed() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        faithful = _gate(cd, rms=RMS_MISPAIR_HI)
        injected = _gate(cd, rms=RMS_MISPAIR_HI, enable_rms_gate=False)
        _expect_verdict(False, faithful, injected, "M42 误配解 rms_px=2.17")
        harness.exact(faithful.rejected_by, ref.GATE_RMS, "正本拒绝分支")
        ev.record("M42 误配解 rms_px（摘门后被接受）", RMS_MISPAIR_HI, ref.RMS_MAX_PX,
                  unit="px", note="超界倍率即摘门后漏过的倍数")


@harness.test(
    "wcs.s5.negative.n_pairs_gate_removed",
    intent="判据 (d) 不可摘 —— 这是 T02 §2.1 S5b 行「三者构成不可分的拒绝组」的点名"
           "证据：摘掉 `ipv_wcs.cpp:761-768` 后，M42 误配解（n_pairs=6、rms_px=0.2，"
           "两者的绝对值都在干净帧域内）会被接受。",
    inputs="ratio=1.0（域内）、rms_px=0.2（干净帧域内）、n_pairs=6（M42 域下端）",
    expected="正本：拒绝（rejected_by='n_pairs'）；注入后：接受 ⇒ 判据 (d) 有牙齿",
    source="ipv_wcs.cpp:751（证人）+ :761（被摘掉的判据 (d)）",
    kind=harness.NEGATIVE,
    inject="摘掉判据 (d) 整个代码块（`enable_pairs_gate=False`）",
    defect_id="WCS-N-NOPAIRS",
    criteria=("S5", "S5b"),
)
def _case_neg_pairs_gate_removed() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        faithful = _gate(cd, rms=RMS_OK, n_pairs=N_PAIRS_MISPAIR_LO)
        injected = _gate(cd, rms=RMS_OK, n_pairs=N_PAIRS_MISPAIR_LO,
                         enable_pairs_gate=False)
        _expect_verdict(False, faithful, injected,
                        "M42 误配解 n_pairs=6、rms_px=0.2")
        harness.exact(faithful.rejected_by, ref.GATE_PAIRS, "正本拒绝分支")
        ev.record("M42 误配解 n_pairs（摘门后被接受）", N_PAIRS_MISPAIR_LO,
                  ref.N_PAIRS_MIN, note="内点 6 被摘门后漏过 ⇒ 求解器返冒充解")


@harness.test(
    "wcs.s5b.negative.reject_success_true",
    intent="正本失败语义：`ipv_wcs.cpp:739/:754/:762` 逐字 `result->success = false`。"
           "负例：失败时返回 `success=true`（假阳性 WCS —— 正本 :717-718 逐字"
           "「比 fail-closed 更危险」）。",
    inputs="三个拒绝分支各一例",
    expected="正本：三例 success=false；注入后：三例 success=true ⇒ 失败语义有牙齿",
    source="ipv_wcs.cpp:717-718,729,739,754,762（fail-closed 语义）",
    kind=harness.NEGATIVE,
    inject="失败分支不写 `success = false`（`success_on_reject=True`）",
    defect_id="WCS-N-SUCCOK",
    criteria=("S5b", "S5"),
)
def _case_neg_success_true() -> None:
    ok_cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    mirror_cd = ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC)
    cases = (
        ("判据 (a)", mirror_cd, RMS_OK, N_PAIRS_OK),
        ("判据 (c)", ok_cd, RMS_MISPAIR_HI, N_PAIRS_OK),
        ("判据 (d)", ok_cd, RMS_OK, N_PAIRS_MISPAIR_LO),
    )
    with harness.evidence() as ev:
        for label, cd, rms, n_pairs in cases:
            faithful = _gate(cd, rms=rms, n_pairs=n_pairs)
            injected = _gate(cd, rms=rms, n_pairs=n_pairs, success_on_reject=True)
            _expect_verdict(False, faithful, injected, label)
            ev.record(f"注入后 {label} 的 success", injected.success,
                      note=f"error 仍非空（{len(injected.error)} 字符）⇒ "
                           "这是「假阳性成功」而非「静默」")
            harness.is_true(injected.error != "",
                            f"注入后 {label}: error 非空而 success=true —— "
                            "最危险形态（下游只看 success 就拿到冒充解）")


@harness.test(
    "wcs.s5b.negative.reject_error_blank",
    intent="正本失败语义：`ipv_wcs.cpp:740/:755/:763` 逐字写 `error` 且 `:729` 逐字"
           "「error 非空」。负例：拒绝时不写错误文本，调用方拿不到拒绝理由。",
    inputs="三个拒绝分支各一例",
    expected="正本：三例 error 非空且含确定性关键词；注入后：三例 error 为空串",
    source="ipv_wcs.cpp:729,740-743,755-757,763-765（确定性 fail-closed 文本）",
    kind=harness.NEGATIVE,
    inject="拒绝时把 `error` 写成空串（`blank_error=True`）",
    defect_id="WCS-N-NOERR",
    criteria=("S5b", "S5"),
)
def _case_neg_error_blank() -> None:
    ok_cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    mirror_cd = ref.cd_for_ratio(RATIO_MIRROR_OUT_HI, S0_ARCSEC)
    cases = (
        ("判据 (a)", mirror_cd, RMS_OK, N_PAIRS_OK),
        ("判据 (c)", ok_cd, RMS_MISPAIR_HI, N_PAIRS_OK),
        ("判据 (d)", ok_cd, RMS_OK, N_PAIRS_MISPAIR_LO),
    )
    with harness.evidence() as ev:
        for label, cd, rms, n_pairs in cases:
            faithful = _gate(cd, rms=rms, n_pairs=n_pairs)
            injected = _gate(cd, rms=rms, n_pairs=n_pairs, blank_error=True)
            _expect_reject_semantics(faithful, f"{label}（未注入）")
            harness.exact(injected.error, "",
                          f"{label}: 注入后 error 必须为空串（否则该缺陷不存在）")
            harness.is_false(injected.success, f"{label}: 注入不改变 success 字段")
            ev.record(f"注入后 {label} 的 error 长度", len(injected.error),
                      note="正本要求非空")


# ---------------------------------------------------------------------------
# 负组：阈值不得被放宽（ipv_wcs.cpp:729 逐字「不放宽任何既有阈值」）
# ---------------------------------------------------------------------------

@harness.test(
    "wcs.s5b.negative.ratio_bounds_widened",
    intent="判据 (a) 的两端是**闭区间**端点。负例：把 `:738` 的端点各向外挪 `ε`，"
           "`0.8−ε` 与 `1.25+ε` 就会被接受 —— 直接违反 `:729`「不放宽任何既有阈值」。",
    inputs=f"ratio = {ref.RATIO_LO}−ε 与 {ref.RATIO_HI}+ε（ε={EPS}），rms_px=0.2、n_pairs=30",
    expected="正本：两例均拒绝；注入后：两例均接受 ⇒ 端点语义有牙齿",
    source="ipv_wcs.cpp:738（闭区间端点）+ :729（不放宽任何既有阈值）",
    kind=harness.NEGATIVE,
    inject=f"端点放宽：下界 0.8→{ref.RATIO_LO - EPS:g}、上界 1.25→{ref.RATIO_HI + EPS:g}",
    defect_id="WCS-N-RATWIDEN",
    criteria=("S5b", "S5"),
)
def _case_neg_ratio_widened() -> None:
    with harness.evidence() as ev:
        for label, target in (("下界外 0.8−ε", ref.RATIO_LO - EPS),
                              ("上界外 1.25+ε", ref.RATIO_HI + EPS)):
            cd = ref.cd_for_ratio(target, S0_ARCSEC)
            faithful = _gate(cd)
            injected = _gate(cd, ratio_lo=ref.RATIO_LO - EPS,
                             ratio_hi=ref.RATIO_HI + EPS)
            _expect_verdict(False, faithful, injected, label)
            _, _, ratio = ref.solve_scale_ratio(*cd, S0_ARCSEC)
            ev.record(f"注入后被接受的 ratio[{label}]", ratio,
                      note=f"正本域 [{ref.RATIO_LO},{ref.RATIO_HI}]")
        # 端点本身在注入后仍必须接受（证明放宽没有把域整体推走）
        for endpoint in (ref.RATIO_LO, ref.RATIO_HI):
            cd = ref.cd_for_ratio(endpoint, S0_ARCSEC)
            harness.is_true(_gate(cd, ratio_lo=ref.RATIO_LO - EPS,
                                  ratio_hi=ref.RATIO_HI + EPS).success,
                            "放宽后端点本身仍应接受（否则测的是别的东西）")


@harness.test(
    "wcs.s5.negative.rms_threshold_widened",
    intent="判据 (c) 的阈值 `0.5` 不得放宽（`ipv_wcs.cpp:729` 逐字）。负例：把 `:753` "
           "的阈值挪到 `0.5+ε`，`rms_px = 0.5+ε` 的解就会被接受。",
    inputs=f"rms_px = {ref.RMS_MAX_PX}+ε（ε={EPS}）、ratio=1.0、n_pairs=30",
    expected="正本：拒绝；注入后：接受 ⇒ 判据 (c) 的阈值位置有牙齿",
    source="ipv_wcs.cpp:753 + :729（不放宽任何既有阈值）",
    kind=harness.NEGATIVE,
    inject=f"残差门阈值放宽 0.5→{ref.RMS_MAX_PX + EPS:g}",
    defect_id="WCS-N-RMSWIDEN",
    criteria=("S5", "S5b"),
)
def _case_neg_rms_widened() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        faithful = _gate(cd, rms=ref.RMS_MAX_PX + EPS)
        injected = _gate(cd, rms=ref.RMS_MAX_PX + EPS,
                         rms_max=ref.RMS_MAX_PX + EPS)
        _expect_verdict(False, faithful, injected, f"rms_px = {ref.RMS_MAX_PX}+ε")
        harness.exact(faithful.rejected_by, ref.GATE_RMS, "正本拒绝分支")
        ev.record("注入后被接受的 rms_px", ref.RMS_MAX_PX + EPS, ref.RMS_MAX_PX,
                  unit="px", note="正本门限 0.5（严格 >）")


@harness.test(
    "wcs.s5.negative.n_pairs_threshold_widened",
    intent="判据 (d) 的阈值 `12` 不得放宽（`ipv_wcs.cpp:729` 逐字）。负例：把 `:761` "
           "的阈值挪到 `11`，`n_pairs = 11` 的解就会被接受。",
    inputs="n_pairs=11（门限外一格）、ratio=1.0、rms_px=0.2",
    expected="正本：拒绝；注入后：接受 ⇒ 判据 (d) 的阈值位置有牙齿",
    source="ipv_wcs.cpp:761 + :729（不放宽任何既有阈值）",
    kind=harness.NEGATIVE,
    inject="内点门阈值放宽 12→11",
    defect_id="WCS-N-NPAIRSWIDEN",
    criteria=("S5", "S5b"),
)
def _case_neg_pairs_widened() -> None:
    cd = ref.cd_for_ratio(1.0, S0_ARCSEC)
    with harness.evidence() as ev:
        faithful = _gate(cd, n_pairs=ref.N_PAIRS_MIN - 1)
        injected = _gate(cd, n_pairs=ref.N_PAIRS_MIN - 1,
                         n_pairs_min=ref.N_PAIRS_MIN - 1)
        _expect_verdict(False, faithful, injected,
                        f"n_pairs = {ref.N_PAIRS_MIN - 1}")
        harness.exact(faithful.rejected_by, ref.GATE_PAIRS, "正本拒绝分支")
        ev.record("注入后被接受的 n_pairs", ref.N_PAIRS_MIN - 1, ref.N_PAIRS_MIN,
                  note="正本门限 12（严格 <）")


# ---------------------------------------------------------------------------
# 成对守卫 3：行为面 —— 任一门被摘掉都必红
# ---------------------------------------------------------------------------

_ABLATION = (
    ("enable_ratio_gate", ref.GATE_RATIO),
    ("enable_rms_gate", ref.GATE_RMS),
    ("enable_pairs_gate", ref.GATE_PAIRS),
)


@harness.test(
    "wcs.s5b.reject_group_inseparable_guard",
    intent="**成对回归守卫（行为面）**：判据组 = {尺度比域, rms 门, n_pairs 门}。"
           "逐门消融 —— 摘掉任一道门，它自己的专属证人就会被接受。"
           "任一门若摘掉后仍拒绝，说明该门没有牙齿（负例无效），本条判红。",
    inputs="三道门 × 各自专属证人（ratio=2.55 / rms_px=2.17 / n_pairs=6）共 3 次消融",
    expected="3 次消融全部使判定由「拒绝」翻成「接受」；三道门构成不可分的拒绝组",
    source="ipv_wcs.cpp:731-769（三门同块）+ 审核包-R2 T02 §2.1 S5b 行"
           "「三者构成不可分的拒绝组」",
    criteria=("S5b", "S5"),
)
def _case_inseparable_guard() -> None:
    with harness.evidence() as ev:
        for switch, gate in _ABLATION:
            gate, label, cd, rms, n_pairs, why = next(
                w for w in _GATE_WITNESSES if w[0] == gate)
            faithful = _gate(cd, rms=rms, n_pairs=n_pairs)
            harness.is_false(faithful.success,
                             f"消融前：证人「{label}」必须被拒绝（{why}）")
            injected = _gate(cd, rms=rms, n_pairs=n_pairs, **{switch: False})
            if not injected.success:
                raise harness.CheckFailure(
                    f"摘掉 {switch} 后证人「{label}」**仍被拒绝**（rejected_by="
                    f"{injected.rejected_by!r}）⇒ 门 {gate} 不是该证人的专属拒绝原因，"
                    "拒绝组可能已被拆开")
            ev.record(f"摘掉 {switch} 后证人「{label}」的判定", "接受",
                      note=f"{why} ⇒ 该门有牙齿")


# ---------------------------------------------------------------------------
# 登记项（不是缺陷是正本缺口，如实断言正本行为并把读数带出来）
# ---------------------------------------------------------------------------

@harness.test(
    "wcs.s5b.det_nonfinite_with_invalid_s0_accepted",
    intent="**缺口登记（非注入）**：`ipv_wcs.cpp:737` 的 `!isfinite(det)` 判据写在 "
           "`:734` 的 `if (isfinite(s0) && s0 > 0.0)` **守卫之内** ⇒ 当 `s0` 非法时，"
           "非有限 det 不被判据 (a) 拦下，闸门在该点 fail-open。本条如实断言正本行为"
           "并把读数带出来，供裁决；**不改**正本行为、不把它写成缺陷。",
    inputs="det = NaN，s0 = 0.0（守卫不成立），rms_px=0.2、n_pairs=30",
    expected="正本行为 = success=true（fail-open 缺口）；同一条 CD 在 s0 合法时必被拒",
    source="ipv_wcs.cpp:734（守卫）+ :737（非有限 det 判据在守卫内）",
    criteria=("S5b",),
)
def _case_det_nonfinite_hole() -> None:
    k = S0_ARCSEC / 3600.0
    cd = (math.nan, 0.0, 0.0, k)
    with harness.evidence() as ev:
        # 对照：适用域内同一个 CD 必被拒（这是 wcs.s5b.det_nonfinite_rejects 覆盖的）
        inside = _gate(cd, s0=S0_ARCSEC)
        harness.is_false(inside.success, "适用域内 det=NaN 必须拒绝（对照组）")
        # 缺口本体：守卫不成立时同一个 CD 被接受
        outside = _gate(cd, s0=0.0)
        harness.is_true(outside.success,
                        "正本逐字行为：s0 非法时守卫不进入，非有限 det 不被拦下"
                        "（本条断言的是正本现状，不是期望行为）")
        ev.record("det=NaN 且 s0=0 时的判定", "接受",
                  note="⚠ fail-open 缺口：判据 (a) 适用域守卫吞掉了非有限 det 检查")
        ev.record("det=NaN 且 s0=1.2（适用域内）时的判定", "拒绝",
                  note="对照：同一 CD 在适用域内被拒")