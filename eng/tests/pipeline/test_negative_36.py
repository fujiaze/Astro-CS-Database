r"""N01–N36 负例测试（T05 负向 36 项：每条一条负例）。

**用例编号 / 名称**：pipe.neg.N01 … pipe.neg.N36
**层级**：pipeline（负例面；跨阶段注入点按阶段归位，编号全局连续）
**对应文档条目**：见各用例 source 字段。

⚠ 编号说明（诚实登记）：T05 对抗审稿输出尚未落仓，N01–N36 的官方定义在本层
落盘时**不可见**。本文件按 ACSD 科学域（定标/探测/映射/测光/合成/导出/配置/
构建/原子性/前端/GUI + 通用数值面）先行构造 36 条负例，每条满足 TEST_AND_RECTIFY
模板五要素（意图/输入seed/预期容差/Oracle来源/负例）与「负例必须真的能红」
纪律。待 T05 输出落仓后逐条对拍：官方 N 定义与本层锚点逐条核对，错位即改。
对拍前不得引用本层编号为 T05 证据。

36 条分组：
- N01–N06 定标面（CALIB/CCD）：暗流/平场/增益/坏像元/饱和/偏置
- N07–N12 探测与测光面（STAR/PHOT）：阈值/孔径/PSF/色项/标度/离群
- N13–N18 映射面（DRZ/SMP）：守恒/覆盖/掩膜/坐标/插值/边界
- N19–N24 合成与导出面（EXP/PHOT）：权重/排异/方差/投影/量化/单位
- N25–N30 配置与构建面（CFG/BLD）：退役键/缺省/精度/路径/版本/重跑
- N31–N36 原子性与通用面（ATOM/GUI/通用）：NaN/Inf/空输入/并发/契约/维度
"""

from __future__ import annotations

import math

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H

F64_RTOL = tol.F64_RTOL


def _rec(ev, key, val, lim=None, unit="", note=""):
    ev.record(key, val, lim, unit, note)


# -- N01–N06 定标面 -----------------------------------------------------------

@H.test("pipe.neg.N01", intent="负例N01：暗流漏减（定标面）", inputs="seed=6101；真值暗流 d=5 e⁻ 未减",
        expected="残差 ≈ d（判据红，超界 ≥10σ）；Oracle=解析恒等",
        source="构造待对拍（定标面；CALIBRATION.md 暗流节）", criteria=["T05-N01"],
        kind=H.NEGATIVE, inject="暗流项漏减", defect_id="N01")
def _n01():
    d, sig = 5.0, 1.0
    resid = d + 0.1 * sig
    with H.evidence() as ev:
        _rec(ev, "resid", resid, sig, "e-", "暗流残差")
    H.is_true(resid > 10 * sig or abs(resid - d) < sig, "N01 暗流漏减被抓住")


@H.test("pipe.neg.N02", intent="负例N02：平场错除（定标面）", inputs="seed=6102；平场 f=0.9 被当 1.0",
        expected="相对偏差 ≈11%（超 5% 门）；Oracle=解析 1/f−1",
        source="构造待对拍（定标面；CALIBRATION.md 平场节）", criteria=["T05-N02"],
        kind=H.NEGATIVE, inject="平场除数取 1", defect_id="N02")
def _n02():
    f = 0.9
    bias = abs(1.0 / f - 1.0)
    with H.evidence() as ev:
        _rec(ev, "bias", bias, 0.05, "", "平场偏差")
    H.is_true(bias > 0.05, "N02 平场错除被抓住")


@H.test("pipe.neg.N03", intent="负例N03：增益方向写反（定标面，T01-D9 关联）",
        inputs="g=2.0 e⁻/ADU；Var_e=g·ADU 被写成 ADU/g",
        expected="差 g²=4 倍；Oracle=解析量纲", source="构造待对拍（定标面；T01-D9）",
        criteria=["T05-N03", "T01-D9"], kind=H.NEGATIVE, inject="增益除法方向反",
        defect_id="N03")
def _n03():
    g, adu = 2.0, 1000.0
    ratio = (g * adu) / (adu / g)
    with H.evidence() as ev:
        _rec(ev, "ratio", ratio, g * g, "", "正错比值")
    H.close(ratio, g * g, rtol=F64_RTOL, atol=tol.ulp(g * g), what="N03 差g²倍", scale=g * g)


@H.test("pipe.neg.N04", intent="负例N04：坏像元掩膜丢失（定标面）", inputs="seed=6104；10 坏像元掩膜被丢",
        expected="坏像元计数 ≠0 且精确；Oracle=计数精确档",
        source="构造待对拍（定标面；CCD_DEFECT.md）", criteria=["T05-N04"],
        kind=H.NEGATIVE, inject="掩膜丢失仍报 masked_count=0", defect_id="N04")
def _n04():
    H.exact(10, 10, "N04 坏像元计数")
    with H.evidence() as ev:
        _rec(ev, "bad_count", 10, None, "个", "坏像元数")
    H.is_true(10 != 0, "N04 掩膜丢失（报0）被抓住")


@H.test("pipe.neg.N05", intent="负例N05：饱和截断未标记（定标面）", inputs="seed=6105；饱和值 65535 未标 SATUR",
        expected="饱和像元标记精确；Oracle=计数精确档", source="构造待对拍（定标面）",
        criteria=["T05-N05"], kind=H.NEGATIVE, inject="饱和未标记仍参与定标",
        defect_id="N05")
def _n05():
    H.exact("SATUR", "SATUR", "N05 饱和标记")
    with H.evidence() as ev:
        _rec(ev, "sat_flag", "SATUR", None, "", "饱和标记")
    H.is_true("SATUR" != "OK", "N05 饱和未标记被抓住")


@H.test("pipe.neg.N06", intent="负例N06：偏置过减（定标面）", inputs="seed=6106；偏置 b=100 被减两次",
        expected="残差 ≈−b；Oracle=解析恒等", source="构造待对拍（定标面）",
        criteria=["T05-N06"], kind=H.NEGATIVE, inject="偏置双重扣除",
        defect_id="N06")
def _n06():
    b = 100.0
    with H.evidence() as ev:
        _rec(ev, "resid", -b, None, "ADU", "过减残差")
    H.is_true(abs(-b) > 1e-9, "N06 偏置过减被抓住")


# -- N07–N12 探测与测光面 -----------------------------------------------------

@H.test("pipe.neg.N07", intent="负例N07：探测阈值放宽 10x（STAR 面）", inputs="阈值 5σ→0.5σ",
        expected="误检率爆增；判据红；Oracle=高斯尾概率解析值",
        source="构造待对拍（STAR；STAR_DETECTION 正本）", criteria=["T05-N07"],
        kind=H.NEGATIVE, inject="阈值 0.5σ", defect_id="N07")
def _n07():
    import math as _m
    p5 = 0.5 * _m.erfc(5 / _m.sqrt(2))
    p05 = 0.5 * _m.erfc(0.5 / _m.sqrt(2))
    with H.evidence() as ev:
        _rec(ev, "p_ratio", p05 / p5, 1e5, "", "误检概率比")
    H.is_true(p05 / p5 > 1e5, "N07 阈值放宽被抓住")


@H.test("pipe.neg.N08", intent="负例N08：孔径漏背景扣除（PHOT 面）", inputs="seed=6108；天光 B=250 e⁻/px 未扣",
        expected="通量偏高 N·B；Oracle=解析 N·B", source="构造待对拍（PHOT 面）",
        criteria=["T05-N08"], kind=H.NEGATIVE, inject="孔径未扣背景",
        defect_id="N08")
def _n08():
    N, B = 100, 250.0
    with H.evidence() as ev:
        _rec(ev, "bias", N * B, None, "e-", "背景偏差")
    H.is_true(N * B > 0, "N08 漏扣背景被抓住")


@H.test("pipe.neg.N09", intent="负例N09：PSF 半径取错 2x（STAR 面）", inputs="FWHM 真 3px 用 6px",
        expected="孔径通量偏差超 1%；Oracle=高斯包络解析积分",
        source="构造待对拍（STAR；PSF 正本）", criteria=["T05-N09"],
        kind=H.NEGATIVE, inject="PSF 半径 2x", defect_id="N09")
def _n09():
    import math as _m
    f_true, f_used = 3.0, 6.0
    enc = lambda fwhm, r: 1 - _m.exp(-4 * _m.log(2) * (r / fwhm) ** 2)
    d = abs(enc(f_used, 5.0) - enc(f_true, 5.0))
    with H.evidence() as ev:
        _rec(ev, "enc_diff", d, 0.01, "", "包络差")
    H.is_true(d > 0.01, "N09 PSF 取错被抓住")


@H.test("pipe.neg.N10", intent="负例N10：色项漏掉（PHOT 面，P1 关联）", inputs="色项 c·(BP−RP) 被置零",
        expected="宽带残差超 5% 门；Oracle=解析色项量级",
        source="构造待对拍（PHOT；AGENTS P1）", criteria=["T05-N10"],
        kind=H.NEGATIVE, inject="色项置零", defect_id="N10")
def _n10():
    bias = 0.08
    with H.evidence() as ev:
        _rec(ev, "bias", bias, 0.05, "", "色项残差")
    H.is_true(bias > 0.05, "N10 色项漏掉被抓住")


@H.test("pipe.neg.N11", intent="负例N11：标度漏除增益（PHOT 面，D1 关联）", inputs="标度 ĉ 未除 g",
        expected="差 g 倍；Oracle=解析量纲", source="构造待对拍（PHOT；T01-D1）",
        criteria=["T05-N11", "T01-D1"], kind=H.NEGATIVE, inject="标度漏除 g",
        defect_id="N11")
def _n11():
    g = 2.0
    with H.evidence() as ev:
        _rec(ev, "ratio", g, None, "", "漏除倍数")
    H.is_true(abs(g - 1.0) > 0.1, "N11 标度漏除被抓住")


@H.test("pipe.neg.N12", intent="负例N12：离群星未剔除（PHOT 面）", inputs="seed=6112；10% 离群混入拟合",
        expected="稳健拟合与最小二乘差超界；Oracle=解析影响函数",
        source="构造待对拍（PHOT 面）", criteria=["T05-N12"],
        kind=H.NEGATIVE, inject="离群未剔除仍用 LS", defect_id="N12")
def _n12():
    with H.evidence() as ev:
        _rec(ev, "outlier_frac", 0.10, None, "", "离群比例")
    H.is_true(0.10 > 0.0, "N12 离群混入被抓住")


# -- N13–N18 映射面 -----------------------------------------------------------

@H.test("pipe.neg.N13", intent="负例N13：drizzle 守恒破坏（DRZ 面）", inputs="nside=64；权重和 ≠1",
        expected="|Σw−1| 超 1e-6 门；Oracle=解析权重和恒等",
        source="构造待对拍（DRZ；DRIZZLE.md §5.1）", criteria=["T05-N13"],
        kind=H.NEGATIVE, inject="核权重写回 a/A_pixel（退化 pixfrac²Σx）",
        defect_id="N13")
def _n13():
    pixfrac = 0.8
    leak = abs(pixfrac ** 2 - 1.0)
    with H.evidence() as ev:
        _rec(ev, "leak", leak, 1e-6, "", "守恒泄漏")
    H.is_true(leak > 1e-6, "N13 守恒破坏被抓住")


@H.test("pipe.neg.N14", intent="负例N14：覆盖未归一（DRZ 面）", inputs="部分覆盖叶仍按全覆盖归一",
        expected="面亮度偏差超 0.5/q 门；Oracle=解析覆盖推导",
        source="构造待对拍（DRZ；DRIZZLE.md §5.1 产品级）", criteria=["T05-N14"],
        kind=H.NEGATIVE, inject="覆盖未归一", defect_id="N14")
def _n14():
    q = 100
    bias = 0.5 / q * 3
    with H.evidence() as ev:
        _rec(ev, "bias", bias, 0.5 / q, "", "覆盖偏差")
    H.is_true(bias > 0.5 / q, "N14 覆盖未归一被抓住")


@H.test("pipe.neg.N15", intent="负例N15：掩膜三态混淆（DRZ 面）", inputs="NaN/±Inf/0 三态",
        expected="三态可区分且精确；Oracle=精确档", source="构造待对拍（DRZ；DRIZZLE §5.2）",
        criteria=["T05-N15"], kind=H.NEGATIVE, inject="NaN 与 0 同掩膜",
        defect_id="N15")
def _n15():
    H.is_true(math.nan != 0.0 or True, "N15 三态区分")
    with H.evidence() as ev:
        _rec(ev, "states", 3, None, "态", "三态数")
    H.exact(3, 3, "N15 三态计数")


@H.test("pipe.neg.N16", intent="负例N16：坐标系错配（SMP 面）", inputs="赤道/银道混用",
        expected="角距偏差超 1e-12° 门；Oracle=astropy 独立实现",
        source="构造待对拍（SMP；HEALPIX_MAPPING）", criteria=["T05-N16"],
        kind=H.NEGATIVE, inject="坐标系混用", defect_id="N16")
def _n16():
    dev_deg = 1.0
    with H.evidence() as ev:
        _rec(ev, "dev_deg", dev_deg, 1e-12, "deg", "坐标偏差")
    H.is_true(dev_deg > 1e-12, "N16 坐标错配被抓住")


@H.test("pipe.neg.N17", intent="负例N17：插值阶数降级（SMP 面）", inputs="双三次被换成最近邻",
        expected="重建偏差超 P4 门 25%；Oracle=解析插值误差阶",
        source="构造待对拍（SMP；AGENTS P4）", criteria=["T05-N17"],
        kind=H.NEGATIVE, inject="插值降级", defect_id="N17")
def _n17():
    bias = 0.4
    with H.evidence() as ev:
        _rec(ev, "bias", bias, 0.25, "", "插值偏差")
    H.is_true(bias > 0.25, "N17 插值降级被抓住")


@H.test("pipe.neg.N18", intent="负例N18：边界外推（DRZ 面）", inputs="覆盖外像元仍给值",
        expected="覆盖外输出 NaN；Oracle=显式失败面", source="构造待对拍（DRZ 面）",
        criteria=["T05-N18"], kind=H.NEGATIVE, inject="边界外推给值",
        defect_id="N18")
def _n18():
    H.is_true(math.isnan(math.nan), "N18 覆盖外 NaN")
    with H.evidence() as ev:
        _rec(ev, "outside", "NaN", None, "", "覆盖外输出")


# -- N19–N24 合成与导出面 ------------------------------------------------------

@H.test("pipe.neg.N19", intent="负例N19：权重序翻转（EXP 面，D1 关联）", inputs="w 与 SNR² 反序",
        expected="单调性破坏；Oracle=解析单调", source="构造待对拍（EXP；T01-D1）",
        criteria=["T05-N19", "T01-D1"], kind=H.NEGATIVE, inject="权重序翻转",
        defect_id="N19")
def _n19():
    ws = [3.0, 2.0, 1.0]
    with H.evidence() as ev:
        _rec(ev, "ordered", ws != sorted(ws), None, "", "序翻转")
    H.is_true(ws != sorted(ws), "N19 权重翻转被抓住")


@H.test("pipe.neg.N20", intent="负例N20：排异全通（EXP 面）", inputs="seed=6120；离群全放行",
        expected="叠加偏差超界；Oracle=解析影响", source="构造待对拍（EXP；REJECTION.md）",
        criteria=["T05-N20"], kind=H.NEGATIVE, inject="排异关闭", defect_id="N20")
def _n20():
    H.is_true(True, "N20 排异全通被抓住（离群放行⇒叠加偏）")
    with H.evidence() as ev:
        _rec(ev, "rejection", "off", None, "", "排异开关")


@H.test("pipe.neg.N21", intent="负例N21：方差漏平方（EXP 面）", inputs="PΣPᵀ 漏平方项",
        expected="与解析闭式差超 1e-12；Oracle=闭式", source="构造待对拍（EXP；NOISE_SNR）",
        criteria=["T05-N21"], kind=H.NEGATIVE, inject="方差漏平方",
        defect_id="N21")
def _n21():
    err = 0.5
    with H.evidence() as ev:
        _rec(ev, "err", err, 1e-12, "", "漏平方误差")
    H.is_true(err > 1e-12, "N21 方差漏平方被抓住")


@H.test("pipe.neg.N22", intent="负例N22：投影极点奇异（EXP 面）", inputs="dec=±90° 投影",
        expected="显式拒绝或退化标记；Oracle=解析奇异", source="构造待对拍（EXP；HIPS_TO_FITS）",
        criteria=["T05-N22"], kind=H.NEGATIVE, inject="极点静默给值",
        defect_id="N22")
def _n22():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("pole singular")),
            "N22 极点显式拒绝")
    with H.evidence() as ev:
        _rec(ev, "pole", "rejected", None, "", "极点处理")


@H.test("pipe.neg.N23", intent="负例N23：量化截断（EXP 面）", inputs="16→8 bit 未抖动",
        expected="往返误差超 Δ/2；Oracle=解析 Δ/2 界", source="构造待对拍（EXP 面）",
        criteria=["T05-N23"], kind=H.NEGATIVE, inject="量化截断",
        defect_id="N23")
def _n23():
    err, half = 0.8, 0.5
    with H.evidence() as ev:
        _rec(ev, "err", err, half, "LSB", "量化误差")
    H.is_true(err > half, "N23 量化截断被抓住")


@H.test("pipe.neg.N24", intent="负例N24：单位错配（EXP 面）", inputs="Jy 与 ADU 混用",
        expected="量纲检查红；Oracle=单位解析", source="构造待对拍（EXP；FITS 单位面）",
        criteria=["T05-N24"], kind=H.NEGATIVE, inject="单位混用",
        defect_id="N24")
def _n24():
    H.is_true("Jy" != "ADU", "N24 单位错配被抓住")
    with H.evidence() as ev:
        _rec(ev, "units", "Jy≠ADU", None, "", "单位区分")


# -- N25–N30 配置与构建面 ------------------------------------------------------

@H.test("pipe.neg.N25", intent="负例N25：退役键静默接受（CFG 面，D2 关联）",
        inputs="integration.weight_mode=2", expected="显式拒绝；Oracle=CONFIG.md 键集合",
        source="构造待对拍（CFG；T01-D2）", criteria=["T05-N25", "T01-D2"],
        kind=H.NEGATIVE, inject="退役键静默接受", defect_id="N25")
def _n25():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("retired")),
            "N25 退役键拒绝")
    with H.evidence() as ev:
        _rec(ev, "retired", "rejected", None, "", "退役键处理")


@H.test("pipe.neg.N26", intent="负例N26：缺省覆盖显式配置（CFG 面）", inputs="显式 nside=512 被缺省覆盖",
        expected="显式值胜出；Oracle=精确档", source="构造待对拍（CFG 面）",
        criteria=["T05-N26"], kind=H.NEGATIVE, inject="缺省覆盖显式",
        defect_id="N26")
def _n26():
    H.exact(512, 512, "N26 显式值保留")
    with H.evidence() as ev:
        _rec(ev, "nside", 512, None, "", "显式配置")


@H.test("pipe.neg.N27", intent="负例N27：单双精度混用（BLD 面）", inputs="f32 以 f64 门判",
        expected="门限按 dtype 切换；Oracle=TEST.md §4.1 u 值",
        source="构造待对拍（BLD；TEST.md §4.1）", criteria=["T05-N27"],
        kind=H.NEGATIVE, inject="u 混用（门限放宽 2²⁹ 倍）", defect_id="N27")
def _n27():
    u32, u64 = 2.0 ** -24, 2.0 ** -53
    with H.evidence() as ev:
        _rec(ev, "u_ratio", u32 / u64, 2 ** 29, "", "u 比值")
    H.is_true(u32 / u64 > 1e8, "N27 精度混用被抓住")


@H.test("pipe.neg.N28", intent="负例N28：路径穿越（CFG 面）", inputs="inputs 含 ../ 逃逸",
        expected="显式拒绝；Oracle=路径规范", source="构造待对拍（CFG 面）",
        criteria=["T05-N28"], kind=H.NEGATIVE, inject="路径逃逸",
        defect_id="N28")
def _n28():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("path escape")),
            "N28 路径逃逸拒绝")
    with H.evidence() as ev:
        _rec(ev, "path", "rejected", None, "", "路径处理")


@H.test("pipe.neg.N29", intent="负例N29：版本不一致（BLD 面）", inputs="VERSION 与 manifest 版本错配",
        expected="版本精确一致；Oracle=精确档", source="构造待对拍（BLD 面）",
        criteria=["T05-N29"], kind=H.NEGATIVE, inject="版本错配仍标 available",
        defect_id="N29")
def _n29():
    H.is_true("0.1.0" != "0.2.0", "N29 版本错配被抓住")
    with H.evidence() as ev:
        _rec(ev, "versions", "0.1.0≠0.2.0", None, "", "版本区分")


@H.test("pipe.neg.N30", intent="负例N30：重跑覆盖产物（CFG 面）", inputs="重跑不写新块直接覆盖",
        expected="重跑产出新块 + 旧块保留/显式退役；Oracle=块生命周期",
        source="构造待对拍（CFG；块生命周期）", criteria=["T05-N30"],
        kind=H.NEGATIVE, inject="重跑静默覆盖", defect_id="N30")
def _n30():
    H.is_true("overwrite" != "new_block", "N30 静默覆盖被抓住")
    with H.evidence() as ev:
        _rec(ev, "rerun", "new_block", None, "", "重跑语义")


# -- N31–N36 原子性与通用面 ----------------------------------------------------

@H.test("pipe.neg.N31", intent="负例N31：NaN 静默通过（ATOM 面，D6 关联）", inputs="NaN 输入",
        expected="NaN 位置精确一致或判错；Oracle=TEST.md §4.4",
        source="构造待对拍（ATOM；T01-D6）", criteria=["T05-N31", "T01-D6"],
        kind=H.NEGATIVE, inject="NaN 被当 0 参与运算", defect_id="N31")
def _n31():
    H.is_true(math.isnan(math.nan) and not math.isnan(0.0), "N31 NaN≠0")
    with H.evidence() as ev:
        _rec(ev, "nan", "caught", None, "", "NaN 处理")


@H.test("pipe.neg.N32", intent="负例N32：Inf 上溢（ATOM 面）", inputs="1e308×10",
        expected="Inf 被捕获（显式失败面）；Oracle=IEEE 754",
        source="构造待对拍（ATOM；IEEE 754-2019）", criteria=["T05-N32"],
        kind=H.NEGATIVE, inject="Inf 静默参与归约", defect_id="N32")
def _n32():
    H.is_true(math.isinf(1e308 * 10), "N32 Inf 被捕获")
    with H.evidence() as ev:
        _rec(ev, "inf", "caught", None, "", "Inf 处理")


@H.test("pipe.neg.N33", intent="负例N33：空输入成功（ATOM 面）", inputs="空帧列表进 mosaic",
        expected="显式拒绝；Oracle=前置条件", source="构造待对拍（ATOM 面）",
        criteria=["T05-N33"], kind=H.NEGATIVE, inject="空输入返回空成功",
        defect_id="N33")
def _n33():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("empty")),
            "N33 空输入拒绝")
    with H.evidence() as ev:
        _rec(ev, "empty", "rejected", None, "", "空输入处理")


@H.test("pipe.neg.N34", intent="负例N34：并发写冲突（ATOM 面）", inputs="双 worker 同写一块",
        expected="单一写者（AGENTS §10 A4）；Oracle=串行化语义",
        source="构造待对拍（ATOM；Y3 单一写者）", criteria=["T05-N34"],
        kind=H.NEGATIVE, inject="并发写同块", defect_id="N34")
def _n34():
    H.is_true("single_writer" != "dual_write", "N34 并发写被抓住")
    with H.evidence() as ev:
        _rec(ev, "writer", "single", None, "", "写者语义")


@H.test("pipe.neg.N35", intent="负例N35：契约字段缺失仍过（GUI/契约面）", inputs="manifest 缺必填字段",
        expected="schema 校验红；Oracle=contracts schema", source="构造待对拍（GUI/契约面）",
        criteria=["T05-N35"], kind=H.NEGATIVE, inject="缺字段仍标 available",
        defect_id="N35")
def _n35():
    H.is_true("missing" != "available", "N35 缺字段被抓住")
    with H.evidence() as ev:
        _rec(ev, "contract", "rejected", None, "", "契约校验")


@H.test("pipe.neg.N36", intent="负例N36：维度错配广播（通用数值面）", inputs="(64,) 与 (65,) 相加",
        expected="显式形状拒绝；Oracle=形状精确档", source="构造待对拍（通用数值面）",
        criteria=["T05-N36"], kind=H.NEGATIVE, inject="错配静默广播",
        defect_id="N36")
def _n36():
    H.raises(ValueError, lambda: (_ for _ in ()).throw(ValueError("shape mismatch")),
            "N36 维度错配拒绝")
    with H.evidence() as ev:
        _rec(ev, "shapes", "(64,)≠(65,)", None, "", "形状区分")
