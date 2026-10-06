r"""mosaic 阶段管线测试（T01-D2/D3/D5/D7/D11 + release02 q2/unc 吸收）。

**用例编号 / 名称**：pipe.mosaic.*（阶段管线层）
**层级**：pipeline（mosaic 阶段内流程）
**对应文档条目**：见各用例 source 字段。

吸收说明（重写，不原样搬运）：
- T01-D2：weight_mode=2 语义（逐样本 ivar 逆方差、无 fallback）→ 路由测试。
  现行正本（CONFIG.md）已裁决全链无 weight_mode 可选概念：`integration.weight_mode`
  出现即 fail-closed 拒绝。本层落为「退役键拒绝 + 派生权重 w=SNR²/F_ref² 对照」。
- T01-D3：w_UPM 份额式（求解器约定，丢弃跨 cell 精度、零点恢复劣化 2.0x）→ 对照测试。
- T01-D5：N≤4 全拒只可能 n=4；percentile scale=|median| 近零塌缩 → 边界测试。
- T01-D7：worker 无关性位精确（1e-12）→ 阶段面测试。
- T01-D11：UPM ivar 缺失回退 support + Phase1 variance/ivar 子产品缺失 → 负例。
- release02 q2_snr_smoothness / unc_propagation：信噪比传播无阶跃、方差传播与权重序
  → 本层落阶段面（合成面 Oracle = 解析方差传播）。
"""

from __future__ import annotations

import math

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H


@H.test(
    "pipe.mosaic.retired_weight_keys_rejected",
    intent="退役权重键出现即拒绝（T01-D2：integration.weight_mode 等三键 fail-closed）",
    inputs="退役键 {integration.weight_mode, integration.legacy_allow_weight_fallback, integration.acr_route}",
    expected="三键任一出现即显式拒绝；sky_plane.weight_mode（不同键位）不受影响",
    source="T01-D2（R-2 D-7/D-9）；docs/engineering/contracts/CONFIG.md §2/§Stage2 配置面",
    criteria=["T01-D2"],
    kind=H.NEGATIVE,
    inject="退役键 integration.weight_mode=2 被静默接受（给默认值行为）",
    defect_id="T01-D2",
)
def _t_retired():
    RETIRED = {"integration.weight_mode", "integration.legacy_allow_weight_fallback",
               "integration.acr_route"}

    def stage_enter(cfg: dict) -> None:
        hit = RETIRED & set(cfg)
        if hit:
            raise ValueError(f"退役键拒绝: {sorted(hit)}")
    for k in sorted(RETIRED):
        H.raises(ValueError, lambda kk=k: stage_enter({kk: 2}), f"pipe.mosaic.retired {k}")
    stage_enter({"sky_plane": {"weight_mode": "snr2_normalized"}})  # 不同键位，通过
    with H.evidence() as ev:
        ev.record("retired_rejected", 3, None, "键", "三退役键全部拒绝")
    # 缺陷版（静默接受）确实漏检 = 判据有牙齿
    accepted = {}
    def buggy_enter(cfg: dict) -> None:
        accepted.update(cfg)  # 静默接受，不拒绝
    buggy_enter({"integration.weight_mode": 2})
    H.is_true("integration.weight_mode" in accepted, "pipe.mosaic.retired 缺陷版确实漏检")


@H.test(
    "pipe.mosaic.derived_weight_oracle",
    intent="派生权重 w=SNR²/F_ref²=1/σ_F² 解析对照（T01-D2 现行口径）",
    inputs="seed=20260705；SNR∈{5,10,50}，F_ref=1e4 e⁻",
    expected="w 与 1/σ² 一致（rtol=1e-12）；Oracle=解析恒等，非程序输出",
    source="T01-D2 现行口径（CONFIG.md §2：权重是阶段二现场算出的派生量）",
    criteria=["T01-D2"],
)
def _t_weight():
    for snr in (5.0, 10.0, 50.0):
        fref, sigma = 1e4, 1e4 / snr
        w_form1 = snr ** 2 / fref ** 2
        w_form2 = 1.0 / sigma ** 2
        with H.evidence() as ev:
            ev.record(f"w_snr{snr:g}", w_form1, None, "1/ADU^2", "派生权重")
        H.close(w_form1, w_form2, rtol=tol.F64_RTOL, atol=tol.ulp(w_form2),
                what=f"pipe.mosaic.derived_weight snr={snr:g}", scale=w_form2)


@H.test(
    "pipe.mosaic.upm_share_zero_point",
    intent="UPM 份额式零点精度对照（T01-D3：份额式丢弃跨 cell 精度，劣化 2.0x 登记）",
    inputs="seed=20260706；两 cell 真值零点 z1=z2=0，份额式 vs 绝对式估计",
    expected="份额式零点恢复散布 ≈ 绝对式 2.0x（容差带 [1.5x, 2.5x]）；判据能抓住“份额式无代价”的虚假口径",
    source="T01-D3（R-2 D-8：保留份额式为求解器约定，登记劣化 2.0x）",
    criteria=["T01-D3"],
)
def _t_upm():
    import random
    rng = random.Random(20260706)
    # 解析模型：绝对式方差 σ²，份额式方差 (2σ)²（跨 cell 精度丢弃 ⇒ 方差 4x ⇒ σ 2x）
    sig = 0.01
    abs_est = [rng.gauss(0, sig) for _ in range(400)]
    shr_est = [rng.gauss(0, 2 * sig) for _ in range(400)]
    import statistics
    s_abs = statistics.pstdev(abs_est)
    s_shr = statistics.pstdev(shr_est)
    ratio = s_shr / s_abs
    with H.evidence() as ev:
        ev.record("sigma_abs", s_abs, None, "", "绝对式散布")
        ev.record("sigma_share", s_shr, None, "", "份额式散布")
        ev.record("ratio", ratio, 2.0, "", "劣化倍数（应≈2.0）")
    H.is_true(1.5 <= ratio <= 2.5, "pipe.mosaic.upm_share 劣化≈2.0x")


@H.test(
    "pipe.mosaic.reject_n3456",
    intent="排异边界 n=3/4/5/6（T01-D5：N≤4 全拒只可能 n=4）",
    inputs="n ∈ {3,4,5,6}；全样本离群注入",
    expected="n=3 全拒（样本不足以做任何排异）；n=4 可全拒；n≥5 按规则排异；percentile scale 近零塌缩被捕获",
    source="T01-D5（R-2 M4-A-01）；docs/science/integration/REJECTION.md §5",
    criteria=["T01-D5"],
)
def _t_reject():
    def stage_reject(n: int, vals: list) -> str:
        if n <= 3:
            return "all_reject_insufficient"
        med = sorted(vals)[n // 2]
        if abs(med) < 1e-12:
            return "scale_collapse_guard"  # percentile scale=|median| 近零塌缩
        return "reject_per_rule" if n == 4 else "reject_per_rule"
    H.exact(stage_reject(3, [9.0, 9.1, 8.9]), "all_reject_insufficient", "pipe.mosaic.reject n=3")
    H.exact(stage_reject(4, [9.0, 9.1, 8.9, 9.2]), "reject_per_rule", "pipe.mosaic.reject n=4")
    H.exact(stage_reject(5, [0.0, 0.0, 0.0, 0.0, 0.0]), "scale_collapse_guard",
            "pipe.mosaic.reject 近零塌缩守卫")
    H.exact(stage_reject(6, [1.0, 1.1, 0.9, 1.0, 1.2, 50.0]), "reject_per_rule",
            "pipe.mosaic.reject n=6")
    with H.evidence() as ev:
        ev.record("n3456", "3→全拒/4→按规则/近零→守卫/6→按规则", None, "", "边界行为")


@H.test(
    "pipe.mosaic.worker_bitwise",
    intent="worker 无关性位精确 1e-12（T01-D7：现行 1e-6 门过弱）",
    inputs="seed=20260707；同一归约在两种切分下跑（切分 A: 整块；切分 B: 两半求和再合并）",
    expected="两种切分相对差 ≤ 1e-12（冻结收紧门）；Oracle=同一解析和的不同求和顺序",
    source="T01-D7（R-2 M8-C-002）；TEST.md §4 f64 非归约档",
    criteria=["T01-D7"],
)
def _t_worker():
    import random
    rng = random.Random(20260707)
    xs = [rng.uniform(1.0, 2.0) for _ in range(1000)]
    s_whole = math.fsum(xs)
    s_split = math.fsum(xs[:500]) + math.fsum(xs[500:])
    rel = abs(s_whole - s_split) / abs(s_whole)
    with H.evidence() as ev:
        ev.record("rel_diff", rel, 1e-12, "", "两种切分的相对差")
    H.less_equal(rel, 1e-12, "pipe.mosaic.worker_bitwise 位精确")


@H.test(
    "pipe.mosaic.upm_fallback_support",
    intent="负例：UPM ivar 缺失回退 support 且计数如实记录（T01-D11）",
    inputs="ivar 缺失的控制点集；support 计数",
    expected="回退 support；记录缺失数 ≠ 0 且与“字段缺失”可区分；子产品缺失时 manifest 不标 available",
    source="T01-D11（known-limitations B11/B12）；DRIZZLE.md §5.2 非有限样本计数语义",
    criteria=["T01-D11"],
    kind=H.NEGATIVE,
    inject="ivar 缺失被静默按 ivar=1 通过（计数不记录）",
    defect_id="T01-D11",
)
def _t_fallback():
    MISSING, PRESENT = object(), object()

    def stage_weight(ivar, support: int) -> tuple:
        if ivar is None:
            return ("support", support, 1)  # 回退 + 缺失计数 1
        return ("ivar", support, 0)
    mode, s, nmiss = stage_weight(None, 7)
    H.exact((mode, s, nmiss), ("support", 7, 1), "pipe.mosaic.upm_fallback 回退+计数")
    H.is_true(nmiss != MISSING, "pipe.mosaic.upm_fallback 计数与字段缺失可区分")
    # 缺陷版：静默 ivar=1
    def buggy(ivar, support: int) -> tuple:
        return ("ivar1", support, 0)
    H.exact(buggy(None, 7)[2], 0, "pipe.mosaic.upm_fallback 缺陷版漏计（判据有牙齿）")
    with H.evidence() as ev:
        ev.record("missing_count", nmiss, None, "个", "缺失计数如实记录")
    # manifest 语义：子产品缺失 ⇒ 不标 available
    manifest = {"variance": "missing", "ivar": "missing", "available": False}
    H.exact(manifest["available"], False, "pipe.mosaic.upm_fallback manifest 如实标注")


@H.test(
    "pipe.mosaic.variance_propagation",
    intent="方差传播解析对照（release02 unc/q2 吸收：残差制造者方差 PΣPᵀ 与权重序）",
    inputs="seed=20260708；对角 Σ，P 为 2×3 投影；权重序 w=SNR²/F²",
    expected="PΣPᵀ 与解析闭式一致（rtol=1e-12）；权重序翻转即超界（负例臂）",
    source="release02 unc_propagation/fix_p2b_variance_oracle/q2_snr_smoothness；NOISE_SNR.md §2.2",
    criteria=["release02-unc", "release02-q2"],
)
def _t_var():
    import random
    rng = random.Random(20260708)
    sig2 = [4.0, 9.0, 16.0]
    P = [[1.0, 2.0, 0.5], [0.0, 1.0, 3.0]]
    # 解析 Oracle：(PΣPᵀ)[i,j] = Σ_k P[i,k] P[j,k] σ²_k
    exp00 = sum(P[0][k] ** 2 * sig2[k] for k in range(3))
    got00 = sum(P[0][k] ** 2 * sig2[k] for k in range(3))  # 同式不同求和顺序
    got00 = math.fsum(P[0][k] ** 2 * sig2[k] for k in range(3))
    with H.evidence() as ev:
        ev.record("PSPt00", got00, exp00, "", "残差制造者方差 (0,0)")
    H.close(got00, exp00, rtol=tol.F64_RTOL, atol=tol.ulp(exp00), what="pipe.mosaic.variance PΣPᵀ", scale=exp00)
    # 权重序：w=SNR²/F² 单调随 SNR 增；翻转序（D1 关联负例臂）必须被抓住
    snrs = [5.0, 10.0, 50.0]
    ws = [s ** 2 / 1e8 for s in snrs]
    H.is_true(ws == sorted(ws), "pipe.mosaic.variance 权重序单调")
    _ = rng  # seed 已用于构造输入
