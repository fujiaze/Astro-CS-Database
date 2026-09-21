# SCI-A 独立审稿意见（非作者）

审稿人：独立子代理 / 轮次 1
审稿时间：2026-09-21 13:25 CST（工作区**正在被并发改写**，见文末"审稿快照"）

## 结论摘要

**不可接受（需修改后重审）**。本单元在假说、方法、三类数据、诚实边界与佐证来源的**结构**上是完整的，
step1/step3/step4/step5/step6/step7 的数字我逐条核对**全部与 README/GATES 一致**；但存在三类硬伤：

1. **归档结果与交付代码/报告不自洽（最严重）**：`results/step8_real_frame.json` 里的真实帧是**负值图像**
   （`sky_median_adu = -31566.0`，恰为 1202−32768），`single_frame_gate = INSUFFICIENT_SAMPLE, n=0`，
   `wcs_refinement.n_matched_within_tol = 36/827`。而 README §4.7 与 GATES G4/G7 报的是
   **1202 ADU、827/827、PASS n=157 σ_obs=0.026520∈[0.006008,0.027408]**。我**执行交付代码**得到
   1202 ADU 与 827/827 ⇒ 该 JSON 不是这份代码的产物，README §4.7 引用的机读证据不存在。
   第三类数据（testdata 真实帧）因此**目前不构成互证**。
2. **负例判据退化/自我开脱**：N2 的 `pass_` 判据方向与验收规范**相反**（规范要求"天光梯度注入时度量如实变大"，
   N2 的 pass 却要求"不响应"，且曲率扫描下 σ_obs 反向下降 45%）；N4 是算术恒等式（`kB := kA/0.62`）；
   N0 与 N5 是同一条恒等式；N3 的 pass 来自 10× 于真实值的合成扫描。**"6/6 通过"实质只有 N1 提供真实注入-响应证据**，
   而 G6 仍记 PASS。
3. **DOC_CORRECTIONS C1 是稻草人**：它要求订正的"原文口径 `Var≈σ_pix²N_eff+F·w/g, w=ΣP³/ΣP²`"
   在 `06_photometry.md`（该文件根本没有 §2.1/§3.1）与 `run/RELEASE-02/parallel/06.md` §2.2
   （Horne 形式 `σ_F/F=sqrt(1/(gF)+N_eff·σ_pix²/F²)`）中**都不存在**，全仓 grep 无此式；
   被"证明低估 2.15×"的对象是作者自己造出来的。附带发现 step6 的低阶空间增益改正**方向反了**、
   `calibrate()` 的 `delta_after_m` **多乘 2.5**、预算上限**系统项二次计入**（上限放宽 21–34%）。

---

## 逐条意见

### R1 归档的 step8 JSON 与交付代码/数据不自洽，README §4.7 的机读证据不存在
严重度：**高**  类型：可复现性 / 证据不足

- 位置：`实验/SCI-A/results/step8_real_frame.json`（sha256 `02cd66073fd0…`）；
  `实验/SCI-A/README.md:203-221`（§4.7 表）；`results/GATES.md:11,14`（G4/G7）
- 问题：该 JSON 是**另一份（更早的、BZERO 处理错误的）代码**的产物，被当作 §4.7 的证据文件引用。
- 证据（我亲自核对）：
  - JSON `in_frame_instrument`：`sky_median_adu = -31566.0`、`img_p01 = -31615.0`、`img_p99 = -31349.0`、
    `img_max = 32767.0`；这 4 个数**恰好等于**当前真实值减 32768（1202−32768=−31566），
    即旧代码执行了 `img = d - BZERO`。
  - 我用**交付的** `code/step8_real_frame.py` 的 `load_real()` 读**交付的**主帧：
    `median(d)=1202.0, p01=1153.0, p99=1419.0, max=65535.0`，`bg_rms=13.628655072597898`，
    `n_sat_like=1248` ⇒ 与 README 一致、与 JSON 相反。
  - JSON `single_frame_gate = {"verdict": "INSUFFICIENT_SAMPLE", "n": 0}`，`items = null`；
    README §4.7 却写 `n=157、inlier 151，σ_obs=0.026520 ∈ [0.006008, 0.027408] → PASS`。
    **该 PASS 在 results/ 下没有任何机读证据。**
  - JSON `wcs_refinement = {shift_px:[-0.30260,-0.47207], shift_arcsec:0.009196, n_matched_within_tol:36, n_in_frame:827}`；
    README 写 `shift (−0.263,−0.319) px = 0.0068″，827/827`。我用交付代码重跑 `refine_shift`：
    `shift=[-0.26261805 -0.31928715] n_match=827/827 shift_arcsec=0.0067799` ⇒ 交付代码给出的是 README 的值，
    **JSON 的 36/827 不可复现**。
  - `run/SCI-401/logs/step8.log` 同样含 `n_match=827/827` 与 `guided 2.3s candidates=434`，
    与 JSON 的 36/827、1.9486 s 互相矛盾 ⇒ 日志、JSON、README 三者不一致。
- 建议：在冻结提交前**重跑 step8 并确认 JSON 与 README/GATES 逐字段一致**；若真实帧判据确实无法给出 PASS，
  必须把 §4.7 与 G7 改为"未判定"，不能以日志数字充当结果。

### R2 README §4.7 的盲检测耗时与 JSON/日志相差 445 倍
严重度：**高**  类型：事实错误

- 位置：`README.md:215`（"盲检测 13163 检出，`2.00 s`"）；`results/step8_real_frame.json → guided_vs_blind_real.blind_wall_time_s`；`run/SCI-401/logs/step8.log`
- 证据：JSON `blind_wall_time_s = 890.6875978470052`；step8.log 逐字 `[step8] blind 890.7s n_det=13163`。
  README 的 `2.00 s` 与两者都不符（偏小 445×）。同一行的引导耗时 README 写 `2.32 s / 0.0053 s/星`，
  JSON 为 `1.9486 s / 0.004490 s/星`，也不一致。
- 影响：§4.3 的算力论证建立在"引导 vs 盲检"的时间上；若盲检实际是 890.7 s，则该数字被**低估**而非夸大
  （对作者有利的方向被写反了），但报告数字失真这一点本身已构成事实错误。
- 建议：以 JSON 实测为准重写该行；并说明盲检在 4096² 上的 890.7 s 是否含首次调用/缓存效应（我未能复跑全量确认）。

### R3 N2 的通过判据方向与验收规范相反（自我开脱）
严重度：**高**  类型：判据退化

- 位置：`code/step7_negatives.py:181-182`；`results/step7_negatives.json → negatives[N2].pass_`；`GATES.md:13`（G6）
- 问题：`ACCEPTANCE_SPEC.md:45` 的验收项逐字是"**平场残差/天光梯度注入时度量如实变大**；真值无效应场景度量归零"。
  N2 的 `pass_` 却是
  `rows2b[-1] < rows2b[0]*1.25 and rows2a[-1] < rows2a[0]*1.25` —— **度量越不响应越通过**。
  这是一条"注入真值无效应才通过"的判据，与规范要求的方向完全相反；把它计入 `n_pass=6/6` 后 G6 判 PASS。
- 证据（JSON 实测）：
  - 线性梯度 0→0.4 ADU/px：σ_obs 0.04534→0.04897（1.080×），全 PASS；
  - 小尺度结构 0→4 ADU：σ_obs 0.04534→0.05342（1.178×），非单调（2.0 处 0.05458 > 4.0 处 0.05342），全 PASS；
  - 二次曲率 0→0.05 ADU/px²：σ_obs **0.04534→0.02487（降到 55%）**，全 PASS；
    `mechanism_confirmed = false`（代码注释却写"曲率项不可吸收 ⇒ 度量必须响应"）。
  - ⇒ 强加性扰动下度量不但不升，反而**下降**，即"扰动越大越容易 PASS"。这是判据的反向退化，不是"正确的不响应"。
- 建议：删除 N2 的 `pass_`（或改为 `FAIL_EXPECTED` 并在 GATES 里如实标 **PARTIAL/未满足**），
  把"σ_obs 对加性天光不敏感"只作为**能力边界**（它已经写在 §6.3，写得对），
  不能同时当作一条"通过的负例"。

### R4 负例集 N0/N4/N5 是恒等式，N3 的通过来自 10× 于真实值的合成扫描
严重度：**高**  类型：判据退化

- 位置：`code/step7_negatives.py:90-104`（N0）、`:295-306`（N4）、`:308-317`（N5）、`:259-293`（N3）
- 证据：
  - **N0**：`F_true = fr["f_syn"]*inject_scale` ⇒ `r_i ≡ log10(inject_scale)` ⇒ σ_obs 恒为 0；
    判红靠硬编码的 `Budget(sigma_fit_white=0.014)`（`:95`）给出非零 floor。**任何**常数残差表都会"通过"。
  - **N5**：`calf = calibrate(ones, ones*0+1, ...)` ⇒ `r_i ≡ 0` ⇒ 与 N0 **同一条恒等式**（σ_obs=0→BELOW_FLOOR）。
    N5 没有增加任何独立证据。
  - **N4**：`kB = kA/0.62; ratio = kB/kA` ⇒ `k_ratio_observed = 1.6129032258064515` 是**算出来的常数**，
    不是观测量（`k_ratio_expected = 1.6129032258064517`，差 2e-16）。JSON 字段名 `k_ratio_observed` 名不副实。
  - **N3**：`weak_color` 用本帧真实颜色项 σ_color=0.005736 时，with/without 两种预算**都 PASS**
    （0.073910 vs 0.071341，判定均 PASS）；`strong_color`（σ_color=0.008335）只记了 σ_obs=0.032378，**没记判定**。
    通过来自 `sweep`：`pass_=any(without→ABOVE_CEILING and with→PASS)`，只在 amp=0.08/0.12 mag 成立，
    而真实颜色项是 0.0057–0.0083 mag（低一个数量级）。且该 sweep 的 σ_obs 与"漏项上限"由**同一 amp 解析构造**，
    "漏掉的那一项够大时判红"接近代数必然。
- 建议：G6 改为如实记 `1/6 真实注入-响应 + 1/6 合成构造 + 3/6 恒等式 + 1/6 反向判据`，
  并在报告里显式说明每条负例的**判别力边界**；至少补一条"真值有强效应 ⇒ 必红"的**物理**负例（例如
  `F_instr` 乘随机因子 0.2 mag 散度，见 `06_photometry.md` §8 的负例②，本实验没有做）。

### R5 DOC_CORRECTIONS C1 是无对象的稻草人订正
严重度：**高**  类型：事实错误 / 越权风险

- 位置：`results/DOC_CORRECTIONS.md:8-25`（C1）；`README.md:107-114`；`code/scia_common.py:443`
- 证据：
  - C1 称"原文口径：`Var(F̂) ≈ σ_pix²·N_eff + F·w/g`，`w = ΣP³/ΣP²`"，并归给 `06_photometry.md` §2.1。
  - `wc -l docs/plugins/algorithms_phase1/06_photometry.md` = **102 行，章节只有 §1–§8**，
    `grep -n "§2\.1|§3\.1|N_eff|ΣP|定案"` **无任何命中**。该文件没有 §2.1，也没有 §3.1/"定案 6"。
  - 真正的权威出处 `run/RELEASE-02/parallel/06.md` §2.2 逐字给的是
    `σ_F/F = sqrt( 1/(g·F) + N_eff·σ_pix²/F² )`（Horne 1986），**没有** `ΣP³/ΣP²` 权重；
    §2.1 的 σ_fit(白)=0.0140 出处是 `out/psf_fit_noise.csv`（生产 `dpsf_fit_batch_d` 200 次重复拟合的**实测**），
    噪声前向模型 `σ_i²=σ_pix²+I_i/g` —— 与实验自称的"精确 Fisher"是**同一个模型**。
  - 全仓 `grep -rn "P\*\*3|P\^3|np.sum(p \*\* 3)"`（排除 `实验/SCI-A`）**无命中** ⇒ 被攻击的"一阶展开"不存在于任何文档或实现。
- 影响：README §4.1 的"一阶展开 σ vs 精确 Fisher σ，低估 2.149×"整张表没有可订正的对象；
  C1 若被前台按字面合并，会向文档写入一条**基于误引的订正**。
- 建议：撤回 C1，或改写为"本实验的精确 Fisher 与 `run/RELEASE-02/parallel/06.md` §2.2 的 Horne 形式在
  背景主导/源泊松主导两个极限的一致性核对（差 ≤12%）"，并**删掉 2.149× 的说法**。

### R6 "预算项逐项由本帧推导"在真实帧上不成立（作者已承认，但 §2.3/G1b 仍写死）
严重度：**中**  类型：夸大

- 位置：`README.md:64-65`（§2.3）、`GATES.md:8`（G1b）；`code/step5_calibration_gate.py:94-97`
- 证据：`sigma_color` 来自 `fr["f_syn_inject"]`（**注入侧真值**）、`sigma_flat` 来自 `fr["m"]`（**真值平场图**）、
  `sigma_gaia = 0.002` 是硬编码假设常数。这三项在真实帧上不可得（step8 自己标 `null`，
  `results/step8_real_frame.json → single_frame_gate.unavailable_items`）。
- 影响：§2.3 的"逐项由本帧推导"与 G1b 的"预算项来自本帧"只对**真值已知的仿真帧**成立，
  且依赖注入真值 ⇒ 不是"单帧自算"。§6.5 已承认真实帧的局限，但 §2.3/G1b 的措辞没有同步。
- 建议：§2.3 改为"仿真帧上逐项由本帧（含真值）推导；真实帧仅 σ_pix/结构因子/σ_psfsys 可自算"。

### R7 预算上限把系统项二次计入，上限被放宽 21–34%
严重度：**中高**  类型：判据实现与冻结公式不一致

- 位置：`code/scia_calib.py:203-222`（`sig_sys` 既进 `mc_sigma_obs(..., sig_sys)` 又作为独立项进 Budget）、
  `code/scia_common.py:409-414`（`sigma_ceiling` 再加一遍）
- 证据：`run/RELEASE-02/parallel/06.md` §2.1 的合成式是
  `σ_ceiling = ρ_hi·sqrt(σ_fit(robust)² + σ_psfsys² + σ_color² + σ_Gaia² + σ_flat² + σ_skyres² + σ_q²)`，
  各预算项彼此独立、只加一次。实测（我从 step5 JSON 重算）：
  | 帧 | JSON σ_ceiling | 去二次计入后 | 放宽倍数 |
  |---|---|---|---|
  | A | 0.074312 | 0.061468 | 1.209 |
  | B | 0.116324 | 0.094234 | 1.234 |
  | C | 0.219490 | 0.164132 | 1.337 |
  三帧判定不变（仍 PASS），但上限被系统性放宽，方向**不是** fail-closed。
  另外 `σ_psfsys` 本身含噪声（帧 A：σ_psfsys=0.0250，`noise_expect_mag=0.01465`），
  与噪声项再次重叠。
- 影响：帧 C 的 `σ_ceiling/σ_obs = 4.24`（A=1.64、B=2.02），加上二次计入的 34%，
  该帧的 PASS 判别力已经很弱 —— 与"判据非退化"的要求相冲突。
- 建议：按冻结式重算 ceiling（去掉重复项），重报 obs/pred；并对 σ_psfsys 做噪声扣除。

### R8 `calibrate()` 的 `delta_after_m` 多乘 2.5（单位错误），该数字进了 README
严重度：**中高**  类型：事实错误（数值）

- 位置：`code/scia_calib.py:143,149`；`code/step1_analytic.py:122`；`README.md:102`
- 证据：`dm = -2.5·log10(F_instr) + 2.5·loc + 2.5·log10(F_syn)` 已经是**星等**，
  随后 `delta_after_m = mad_sigma(dm - pred) * 2.5` 又乘 2.5。我实测帧 A：
  `mad_sigma(dm-pred) = 0.01973`，而 JSON `calibration.delta_after_m = 0.049331`（= 2.5×0.01973，逐位吻合）。
- 影响：README §4.1 的"拟合 m(deg2) 后 `0.000913`"（`step1_analytic.json → spatial_gain_recovery.sigma_obs_with_m_model`）
  应约为 0.000365；"40.7× 改善"实为"约 102× 改善"（定性结论不变，数字错 2.5×）。
- 建议：删除多余的 `* 2.5`，重跑 step1/step5 并同步 README。

### R9 step6 的低阶空间增益 `m̂` 改正方向反了，"归一化落到像素"未成立
严重度：**高**  类型：事实错误 / facade 风险

- 位置：`code/step6_apply_and_units.py:57-58`（`m_hat = 10**(-0.4*dm)`）、`:75-79`（残差）；
  `README.md:162`（"星等域残差：中位 0.00137 mag，MAD 0.0738 mag"）
- 证据（我在交付帧 A 上用交付模块复算）：
  - 由 `F_instr = k·m_true·F_syn` 得 `dm := 2.5(loc−r) = −2.5·log10(m_true)`，
    即 **`m_true ≈ 10^(−0.4·pred)`**；我实测 `m_true_at_median = 0.99963` 而 `m_hat_at_median = 1.00619`，
    `mad_sigma(2.5·log10(m_true/m_hat)) = 0.01040` ⇒ **`m_hat` 复现的是增益本身，不是它的改正因子 `1/m_true`**。
  - 三种口径下的星等域残差（同一批 48 颗星）：
    `mad_sigma(dm)`（完全不改正）= **0.04534**；`mad_sigma(dm+pred)`（step6 实现）= **0.07378**；
    `mad_sigma(dm−pred)`（正确方向）= **0.01973**。
    JSON 的 `magnitude_only.resid_mag_mad_sigma = 0.07378469` 与"实现口径"逐位吻合。
  - ⇒ 按交付实现，apply photometry 把残差从 0.0453 放大到 0.0738（+63%）；正确方向本应降到 0.0197（−56%）。
    `I_photo = k_photo·m(x,y)·I_cal` 在这份实现里**没有把帧归一化**，而是把空间增益误差平方化。
  - 连带影响：step5 的 `σ_flat` 用 `mad_sigma(2.5·log10(m_true/m_hat))` 度量的是"拟合优度"，
    不是"改正后的平场残余"；在方向错误的前提下，真实改正残余约为 `2×MAD(m_true)`（≈0.042 mag），
    是报告值 0.0104 的 4 倍（该方向使 ceiling 偏紧，属保守，但量已不是公式所指的量）。
- 建议：明确 `m(x,y)` 在 `I_photo` 中的语义（改正因子还是增益），修正符号，
  用"改正后残差 < 改正前残差"作为 apply photometry 的**非退化判据**并写进 G5。

### R10 `run_all.sh quick` 在干净状态下崩溃
严重度：**中**  类型：可复现性

- 位置：`code/run_all.sh:31-34`、`code/step9_collect.py:34,63`；`README.md:5,266`、`code/README.md`
- 证据：我把 `实验/SCI-A` 复制到 `/tmp/scia_quick`，删除 `results/step6_apply_and_units.json` 后运行
  `python3 code/step9_collect.py`：
  `File "…/step9_collect.py", line 63, in main / ue = s6["unit_elimination"] / TypeError: 'NoneType' object is not subscriptable`。
  `run_all.sh` 有 `set -euo pipefail`，因此 `quick` 会在最后一步中止（或在使用**上一次**残留的
  step6 JSON 时静默给出陈旧 G2）。
- 建议：`quick` 模式下让 step9 把 G2 标为 SKIPPED；或干脆删掉 `quick` 承诺。

### R11 "不同透明度帧残差分布一致"从未被检验（G3 的证据不足）
严重度：**中**  类型：证据不足 / 死代码

- 位置：`code/step5_calibration_gate.py:176-183`；`ACCEPTANCE_SPEC.md:42`；`GATES.md:10`
- 证据：step5 里 `from scipy.stats import ks_2samp`（导入未用）、`def resid(fr, cal): return None`（空函数、从未调用）、
  `rA = np.log10(cA["location"]) * 0 + cA["location"]`（`*0+` 空操作）—— 三处都是残差分布检验的**残留骨架**，
  没有任何 KS/分布比较真的执行。G3 的实际证据只有 `k_B/k_A=1.6099`（比值/期望 0.9981）与
  `σ_obs 比 = 1.2671`（差 27%）。
- 影响：验收项"不同透明度帧系数不同但残差分布一致"中的后半句是**断言**，不是实测。
- 建议：补做 KS 检验（或分位数对照）并给 p 值；否则把该验收项降为"未验证"。

### R12 step6 的"独立复算"与"fail-closed"是恒真/桩函数
严重度：**中**  类型：facade

- 位置：`code/step6_apply_and_units.py:59-62`、`:81-91`
- 证据：
  - `I_photo_indep = (k_photo*m_hat)*img` 与 `pl.apply_photometry(img,k,m)` 实现为
    `k_photo*m_map*img` —— **同一表达式的两份拷贝**（浮点逐位相同），
    `independent_recompute_max_rel_diff = 0.0` 是恒真式，不构成"独立复算"（GATES G5 把它当证据）。
  - `downstream(res, photometry_enabled)` 是本文件内 4 行桩函数：只有 `False` 分支被调用过
    （`downstream({}, False)`），`True` 分支从未执行 ⇒ "下游消费归一化像素"没有被验证，
    "fail-closed"验证的是这个桩自己抛的异常。真正的下游是 drizzle/SNR 节点，本实验没有触及。
- 建议：把 `I_photo` 的独立复算换成**不同算法路径**（例如逐像素循环/不同单位换算），
  或如实把 rel=0 标注为"同一实现的重复求值，不构成独立证据"；`downstream` 明确标注为**桩**。

### R13 N2 的加性天光注入用的是"算术相加的确定性图样"，正是 §12.2 禁止的形态
严重度：**中**  类型：方法

- 位置：`code/step7_negatives.py:142`（`img = img0 + gx*(xx - W/2)`）、`:159`（`img = img0 + amp*fine`）、`:207`（曲率）
- 证据：三处都是往**已生成**的帧上叠加确定性图样，**没有对应的 Poisson 散粒噪声**。
  `ASTROCS_DESIGN.md:564` 逐字："天光对 SNR 的影响通过散粒噪声体现，**不用算术加常数代替加天光**"。
  与之对照，同一实验的 `step1_analytic.py:154-167`（天光 ×0.25/1/4/16 走 Poisson）是物理正确口径，
  且实测 σ_obs = 0.013352/0.013603/0.019149/0.034587 **单调上升**（`step1_analytic.json → noise_scaling`）。
- 影响：N2 的"不响应"结论建立在被禁的注入方式上；用物理口径（step1）时度量是响应的。
  N2 真正说明的是"局部常数背景能吸收平滑的加性偏置"，而不是"度量对加性天光无鉴别力"。
- 建议：N2 改用前向模型（抬高 sky_adu 后重跑 Poisson 前向）重做，或明确把 N2 降级为
  "确定性偏置吸收实验"并注明它不满足 §12.2 的注入口径。

### R14 判据出处引用错文件（phantom §3.1 / §2.1）
严重度：**中**  类型：事实错误（引用）

- 位置：`README.md:8`（"06_photometry.md §3.1/§4.1"）、`README.md:54`（"`06_photometry.md` §3.1 定案 6，逐字"）；
  `code/scia_common.py:34,35,393,443,489`、`code/scia_calib.py:4,172`（多处 "06.md §2.1/§3.1"）
- 证据：`docs/plugins/algorithms_phase1/06_photometry.md` 只有 §1–§8，双边界判据在 **§4.1**；
  §2.1（逐项预算表）与 §3.1（判据形态）、"定案 6"都在 `run/RELEASE-02/parallel/06.md`。
  README 把两个"06"混为一谈。另：`run/` 按 `AGENTS.md` §7 是 gitignore 的临时产物目录，
  把冻结判据的规范依据放在那里，可追溯性弱于 `docs/`。
- 建议：统一写成 `run/RELEASE-02/parallel/06.md` §2.1/§3.1（并登记"判据形态已冻结进
  `docs/plugins/algorithms_phase1/06_photometry.md` §4.1"），代码注释同步。

### R15 "SHA256 固定的缓存"是文档承诺，不是机制
严重度：**中**  类型：可复现性

- 位置：`code/step0_fetch_refs.sh:14`、`code/step3_forward_vs_photflam.py:40-47`、`data/README.md:39-46`
- 证据：step0 只执行 `sha256sum`（**打印**），从不与 `data/README.md` 记录的三个值比较；
  step3 的 `svo_curve()` 计算 `sc.sha256_file(p)` 后直接存进 JSON，**没有校验**。
  我实测三个缓存文件的 SHA256 与 `data/README.md` 逐位一致
  （`2af43d2dec10…` / `fdb18eb39936…` / `587ef650b066…`）—— 当前值是对的，但脚本发现不了被替换/损坏的缓存。
- 建议：在 step0 里用 `sha256sum -c` 或显式比较，不匹配即 fail-closed。

### R16 引导匹配率 0.9859 的定位输入就是注入真值；3 px 崩溃是拟合边界造成的
严重度：**中**  类型：证据不足 / 结论夸大

- 位置：`code/step2_hst_sim.py:83`（`world_to_pix` 注入）↔ `code/scia_calib.py:34`（`world_to_pix` 定位）；
  `code/scia_common.py:337-338`（`fit_psf` 位置边界 ±2 px）；`README.md:136-143,230-232`
- 证据：
  - 帧 A/B 的注入位置 = 同一 WCS 的 `world_to_pix(cat.ra, cat.dec)`，引导路径的初值 =
    同一 WCS 的 `world_to_pix(fr.ra, fr.dec)` ⇒ **引导被直接喂了真值位置**。
    因此 0.9859 度量的是"在真值位置上 PSF 拟合是否收敛"，而不是"星表引导相对盲检的定位能力"。
  - 粗 WCS 偏移 0/1/2 px 时匹配率恒为 0.9859，**3 px 时崩到 0.01408**；而 `fit_psf` 的位置下/上界正是
    `x0±2.0, y0±2.0` ⇒ 3 px 偏移时拟合在数学上无法回到真值。C4 的"必须收敛到 ≲2 px"因此是
    **拟合框宽度**的循环结论，不是独立发现。
- 建议：把 `fit_psf` 的位置边界与"二轮精化精度要求"解耦（例如边界放到 5 px 再扫），
  并补一个"WCS 有真实误差（非真值定位）"的引导匹配率；否则 §5.3 的"最强最稳结论"应降级。

### R17 σ_psfsys 口径偏离生产定义且含噪声（DOC_CORRECTIONS C2 的"保守"论证不完整）
严重度：**中**  类型：证据不足

- 位置：`code/scia_calib.py:82-96`；`code/step5_calibration_gate.py:102-105`；
  `results/DOC_CORRECTIONS.md:29-42`（C2）
- 证据：
  - 生产文档 `run/RELEASE-02/parallel/06.md` §2.1 明确 σ_psfsys 用"独立孔径测光（**r=10 px**，环 12–18 px）"。
    实验在门禁里改用 **r=4 px**（`step5:104`），给出的 σ_psfsys = 0.0250/0.0401/0.1061，
    而 r=10 px 的值为 **0.2574/0.2557/1.3775**（JSON `sigma_psfsys_ap10_inframe`）。
    改用 r=4 使 ceiling 更紧（保守方向，我核对了这一点），但这是**偏离冻结口径**，且真实帧（step8）
    只跑了 r=4、没有跑 r=10 的对照 ⇒ 无法判断真实帧 PASS 对口径的敏感性。
  - 该估计量本身是"PSF/孔径比值散度"，含噪声：帧 A σ_psfsys=0.0250 vs `noise_expect_mag=0.01465`（噪声占 59%），
    与噪声项重复计入（见 R7）。文档口径的判据是"噪声预期 0.0032 ⇒ 系统主导（9.5×）"，
    本实验的小孔径口径下**不再系统主导**。
  - 函数里还有 `* 0.0 +` 死代码：`scia_calib.py:88`、`:93`（`... / np.log10(np.e) * 0.0 + ...`）。
- 建议：按生产口径（r=10 px）重算并在报告里同时给两个口径；对 σ_psfsys 做噪声扣除后再进预算。

### R18 results/step5_calibration_gate.json 含裸 `NaN`（非法 JSON）
严重度：**低**  类型：可复现性

- 位置：`results/step5_calibration_gate.json:248`（`"sigma_psfsys_noise_expect": NaN`，帧 C）
- 证据：`jq . results/step5_calibration_gate.json` 报错拒绝；`python3 json.load` 接受（Python 扩展）。
  该字段来自 `psf_vs_aperture_systematics` 在帧 C 上 `f_err_psf` 全为 NaN 的分支。
- 建议：写 JSON 时用 `allow_nan=False` 并把不可算值写 `null`。

### R19 若干硬编码常量与死代码被当成实测值
严重度：**低**  类型：风格 / facade 迹象

- 位置：`code/step7_negatives.py:134`（`zero_injection_equals_baseline=True` 字面量）、`:95,311`（`sigma_fit_white=0.014`）、
  `:63-69`（`budget_of()` 定义后从未调用）；`code/step4_guided_vs_blind.py:63`（`false_alarms=0` 字面量）；
  `code/scia_sim.py:94-95`（`if inst.ellipticity > 0: f = f` 空操作）
- 证据：逐行读到的字面量/空操作，不是计算值。
- 建议：改为计算或显式标注"定义性常量"；删除死代码。

### R20 step3 的"有效面积 vs 几何面积"对照没有结论
严重度：**低**  类型：证据不足

- 位置：`README.md:171-177`；`results/step3_forward_vs_photflam.json → throughput_curves.validation`
- 证据：反推面积 44425 / 45323 cm²，几何面积 38453 cm²，比值 1.155 / 1.179。
  报告把它列为"检验**绝对刻度**"的独立校验，但**有效面积大于几何集光面积**这一结果既未解释、
  也未给不确定度与预期值 —— 作为校验它既未通过也未失败。相比之下枢轴波长对照（相对差 4e-6 / 7e-6）
  是干净通过的。
- 建议：说明该 1.155/1.179 是定义性比值（PHOTFLAM 的平谱约定）还是偏差；否则删掉这半句"检验绝对刻度"。

### R21 依赖版本与引用版本不一致（低）
严重度：**低**  类型：引用

- 位置：`code/README.md`（"astropy 7.0"）vs `README.md:303`（"astropy 8.0.1 `astropy/stats/funcs.py:945`"）
- 证据：本机实测 `astropy 7.0.1`、`numpy 2.2.4`、`scipy 1.15.3`、`Python 3.13.5`。
  `run/SCI-401/lit/verified_refs.md` B1a 明确是从 GitHub `v8.0.1` 读的源码 ⇒ 引用本身合规，
  但"读了 8.0.1、跑在 7.0.1"这一点没有在 README 里说明。
- 建议：在 §8.2 标注"引用版本 = 源码阅读版本（8.0.1），运行环境为 astropy 7.0.1"。

---

### R22 已核实无问题的部分（作者是对的）
严重度：—  类型：—

- **八要素齐备**：§1 假说（含 5 条可证伪点）、§2 公式与判据、§3 数据、§4 结果、§5 结论、§6 诚实边界、
  §7 复现命令、§8 佐证来源 —— 均在位，且 §6 的 9 条边界**不是空话**（逐条有数字或明确"判不了"）。
- **禁用方法逐条**：① 算术加常数天光 —— 主前向链（`scia_sim.forward`/`analytic_frame`）天光一律进 Poisson，
  合规；只有 N2 的注入违规（R13）。② 当前程序输出当 Oracle —— 未发现；`f_syn_dense` 是独立插值+梯形，
  `f_syn_inject` 是注入真值。③ 凭记忆写引文 —— 未发现；`run/SCI-401/lit/verified_refs.md` 逐条给 DOI/URL/逐字片段，
  并**如实**把 A12/A14 标为 AMBIGUOUS/UNRESOLVED。④ 跨帧/组间一致性门 —— 无（`cross_frame_gate_present=false`，
  N4 明确论证该门必须不存在）。⑤ `0.03 mag` 常数阈值 —— 无（`grep 0.03` 只命中平场多项式系数 0.035）。
  ⑥ `k_photo` 绝对数值窗口 —— 无（`no_absolute_window.k_photo_in_gate=false`，`grep` 无阈值）。
  ⑦ 物理闭合式反解 —— 无；`step6` 只用 `A·t/g` 做**前向**构造，未反解。
- **诚实边界的自我限制到位**：§6.1"全帧消除物理单位未建立"的判断正确（`docs/science/PHOTOMETRY.md` §16.2/§16.3
  正是这个论证），我没有在任何地方发现把它夸大成"已消除物理单位"；G2 的 PASS 对应
  `ACCEPTANCE_SPEC.md:41` 的字面（较弱）表述，两者不矛盾。也**没有过度保守**：
  该证明的（无绝对窗口、不可反解、零点平移不变量）都写了。
- **失败措辞**：未发现"把失败说成环境问题"。§6.9 主动登记 CI `pass=87 fail=12` 与 `实验/` 未进根白名单，
  并给出 `grep -c 实验 run/SCI-401/logs/ci_fast.log = 0` 的隔离证据 —— 这是**如实**而非掩盖。
- **数字一致的部分**（见下表）：step1/step3/step4/step5/step6/step7 的 README/GATES 引用与 JSON 全部逐项吻合。

---

## 我核对过的数字

| README 写的 | JSON 实际（键路径） | 一致? |
|---|---|---|
| 帧A σ_obs 0.04534 / floor 0.00966 / ceiling 0.07431 | `step5.frames[A].sigma_obs_mag=0.0453443`；`budget.sigma_floor=0.00965892`；`sigma_ceiling=0.0743119` | ✅ |
| 帧B σ_obs 0.05746 / 0.01252 / 0.11632 | 0.0574570 / 0.0125175 / 0.1163236 | ✅ |
| 帧C σ_obs 0.05172 / 0.01457 / 0.21949 | 0.0517183 / 0.0145662 / 0.2194898 | ✅ |
| k 相对误差 2.09% / 1.85% / 3.95%（"有效真值"） | `k_rel_err_effective` = 0.0208990 / 0.0184931 / 0.0395020；**但 `k_rel_err`（未扣通带零点）= 0.4621 / 0.4631 / 0.4518** | ⚠️ 表头标了"有效真值"故不算错，但 46% 的原始误差未在正文说明 |
| σ_pix 18.40 e-、结构因子 1.000、σ_psfsys 0.0250、σ_color 0.00574、σ_flat 0.0104 | `items_measured`：18.4024 / 1.0 / 0.0249978 / 0.00573557 / 0.0103953 | ✅ |
| k_B/k_A=1.60986；比值/期望 0.9981；σ_obs 比 1.267 | `frame_independence`：1.6098633 / 0.9981153 / 1.2671280 | ✅ |
| 引导匹配率 0.9859（70/71）vs 盲检 0.2254；提升 0.7606 | `step4.frames[A]`：0.98592 / 0.22535 / 0.7605634 | ✅ |
| 每匹配星成本 0.00718 s vs 0.00792 s（A）；0.00398 vs 0.00993（B） | `cost_per_matched_star`：0.0071813/0.0079229；0.0039787/0.0099320 | ✅ |
| 粗 WCS 3 px ⇒ 匹配率 0.0141 | `coarse_wcs_robustness[3].match_rate=0.01408` | ✅ |
| 真实帧盲检 13163、1.49%、434 候选、45.2%、30.3× | `guided_vs_blind_real`：13163 / 0.0148902 / 434 / 0.4516129 / 13163÷434=30.33 | ✅（但见 R1/R2） |
| 退化族中位通量 4831.2/4865.1/4840.2；σ_obs 0.01891/0.01812/0.01344 | `step6.unit_elimination.degenerate_family` 逐位吻合 | ✅ |
| Δlocation 0.8633228601（期望同）；k 比 0.1369863014；Δσ_residual 0 | 0.8633228601204621 / 0.8633228601204559；0.13698630136986106 / 0.136986301369863；0.0 | ✅ |
| drizzle 尺度比 2.08505e-17 vs 期望 2.08506e-17 | 2.0850477742959368e-17 / 2.0850624580315374e-17 | ✅ |
| 星等域残差中位 0.00137、MAD 0.0738 | 0.0013681187 / 0.0737846930 | ✅（数值对，但 MAD 是方向错误的产物，见 R9） |
| N1 σ_obs 0.04534→0.15959，≥0.04 判红 | `step7.N1.rows`：0.04534/0.04719/0.04798/0.07618/0.09879/0.15959，amp≥0.04 为 ABOVE_CEILING | ✅ |
| N2 响应 1.08×/1.18×；曲率无单调响应 | 1.0799 / 1.1781；曲率 0.04534→0.04566→0.04549→**0.02487** | ✅（数字对，判据方向见 R3） |
| N4 k_B/k_A=1.61290 | 1.6129032258064515（**由 `kA/0.62÷kA` 算出**） | ⚠️ 数字对但不是观测量（R4） |
| step1 Oracle rtol=0、k_rel_err=3.33e-15 | `oracle_zero_point_m1`：rtol 0.0、k_rel_err 3.3306690738754696e-15 | ✅ |
| step1 空间增益 0.03720 → 0.000913 | 0.037200790 / 0.0009129009（后者含 ×2.5 单位错误，应为 ≈0.000365，见 R8） | ⚠️ |
| 天光 ×0.25/1/4/16 ⇒ 0.013352/0.013603/0.019149/0.034587，ratio 1.38/1.24/1.32/1.50 | `noise_scaling.rows` 逐位吻合（1.376574/1.244110/1.322008/1.499262） | ✅ |
| σ_fit 一阶 vs 精确 Fisher：0.006152/0.013218（2.149×）等 | `sigma_fit_composition_form.rows` 逐位吻合 | ⚠️ 数字对，但被比较的"一阶式"不存在于任何文档（R5） |
| F657N n=42 中位 −0.147±0.113 MAD 0.491 斜率 0.464；干净 n=12 −0.014/0.255 | −0.1472 / 0.1131 / 0.4911 / 0.4638；−0.013721 / 0.254622 | ✅ |
| F673N n=29 −0.263±0.130 MAD 0.645 斜率 0.765；干净 −0.224/0.104 | −0.2630 / 0.1297 / 0.6454 / 0.7653；−0.224429 / 0.104200 | ✅ |
| F502N n=75 −0.080±0.032 MAD 0.213 斜率 0.565；干净 −0.007/0.049 | −0.0801 / 0.0323 / 0.2133 / 0.5646；−0.006782 / 0.049218 | ✅ |
| 同星跨滤镜 n=19 中位 0.117、MAD 0.326 | 0.1171037 / 0.3257726 | ✅ |
| 枢轴 6566.581 vs 6566.605（4e-6）；面积 44425/38453=1.155 | 6566.580964 / 6566.605450，rel 3.7288e-6；44425.476 / 38453.094 | ✅ |
| WCS 二轮 F657N (23.10,33.32)=1.622″ 83/88；F673N (23.12,33.52)=1.629″ 80/88 | 23.104088/33.317181 → 1.621769；23.122747/33.524868 → 1.629026；83/88、80/88 | ✅ |
| §4.7 天光中位 1202 ADU | `step8.in_frame_instrument.sky_median_adu = -31566.0`（=1202−32768） | ❌ **不一致** |
| §4.7 WCS 827/827、shift (−0.263,−0.319) | JSON：36/827、(−0.30260,−0.47207)；我用交付代码重跑得 827/827、(−0.262618,−0.319287) | ❌ JSON 不可复现 |
| §4.7 单帧判据 n=157 PASS σ_obs 0.026520∈[0.006008,0.027408] | JSON：`INSUFFICIENT_SAMPLE, n=0`，`items=null` | ❌ **无证据** |
| §4.7 盲检 2.00 s | JSON/日志：890.688 s / 890.7 s | ❌ |
| §4.7 引导 2.32 s、0.0053 s/星 | JSON：1.9486 s、0.004490 s/星 | ❌（小差，属不同次运行） |
| data/README.md 三个 SVO 曲线 SHA256 | `sha256sum run/SCI-401/data_cache/svo_*`：`2af43d2dec10…`/`fdb18eb39936…`/`587ef650b066…` | ✅ 逐位一致（但脚本不校验，见 R15） |

---

## 我没有能力核实的部分（诚实列出）

1. **step8 全量复跑**：单次盲检测在当前实现下耗时 ~890 s（JSON/日志一致），我没有跑完整 step8，
   因此**不能断定**真实帧的单帧判据最终会不会 PASS —— 我只能断定
   **归档的 JSON 与交付代码/README 不自洽**，且 §4.7 引用的那组数字在 results/ 下没有机读证据。
2. **帧 A/B/C 与 step1 的全量重跑**：我复算了个别量（`delta_after_m`、`m_hat` 方向、`refine_shift`、
   `load_real`），但没有重跑 step1–step7 全链，因此"固定 seed 下逐位可复现"这一条只做了**代码级**检查
   （`rng(tag)` 由 SHA256 派生、与调用顺序无关），没有做端到端复现比对。
3. **HST/PHOTFLAM 绝对刻度与 SVO 曲线的一手正确性**：我只核对了 SHA256 与仓库内头部关键字的一致性，
   没有独立获取 SVO 曲线或 HST 头文件做第三方比对（网络可用但我没有把这条作为本轮重点）。
4. **文献 DOI/卷页**：我读了 `run/SCI-401/lit/verified_refs.md` 的核验记录，但**没有**自己重新访问
   Crossref/arXiv 复核 A1–A13；该文件的自我标注（AMBIGUOUS/UNRESOLVED）看起来诚实，但我未独立验证。
5. **N2 曲率扫描的机理**：JSON 显示 q=0.05 时 σ_obs 掉到 0.0249，代码注释归因于"大曲率把星推出质量选择、
   样本变偏"。我没有逐星复算该样本选择变化，无法确认这个机理解释是否成立。
6. **并发改写**：审稿期间 `README.md`（13:23:16）、`results/step8_real_frame.json`（13:22:32）被改写，
   我一度在 `/tmp` 副本里观察到 `guided_candidates=827` 的另一版本。本意见书锚定下面的快照；
   若作者在此之后再次重跑，R1/R2 需按新 JSON 重判。

---

## 审稿快照（供作者对账）

```
README.md                          9f5acba24de3be234fd9b1fe77fa7b22a65d40139d4033c942ca0e8850b44f2c
results/step8_real_frame.json      02cd66073fd035cc60930fdd072f3560a8a35d3eb0b90159b6fcf441b23250b0
results/step5_calibration_gate.json fd5ecb25b927ca2106e25b710607103a88c7393a35ffcb4607302c48a146b996
results/step7_negatives.json       d12d12838da60823d21bd5fd50060074d9b75d470cf114f1215a0f7985da67f9
results/step6_apply_and_units.json 19aa3cfeddd6b2d0ed889f420a83e81b47672af6eff99f8133c42475c00ebbdc
results/step1_analytic.json        2a4d90aa370a00208f03398d0fffd5eff4421a714aa7380d9c87c2c1be12d443
results/step3_forward_vs_photflam.json 060e5c4d585003828a37f3c8eaea32c594f73f470fea9552b9d724a056dc2e5a
results/step4_guided_vs_blind.json 8f2dfe5ff42d03fb2be48cb52c48d4a0b726dc219f033adb06bb5d0453b0d3fd
results/GATES.md                   7179acb23542f72305fdf0f0a97f70e2a99cee9666dda74a3ce2eba181b13700
results/DOC_CORRECTIONS.md         69ebdaf034592ac57ea0131a8f1411a82a104970237493304f90e556b594113d
code/step8_real_frame.py           b2537a6896145753ad17a95f69d0b2907d8d28d57b9c73a84d86c3dd16a03253
code/scia_calib.py                 0b67f1b3d080b7bce7a180cf962e7e7ceac939c41974d37a9bcc9a7e67ad66b9
```

本文件是审稿人本轮**唯一**写入的文件；未运行任何 git 写操作，未修改其他文件。
（`/tmp/verify_m.py`、`/tmp/check_real.py`、`/tmp/check_step8_load.py`、`/tmp/check_wcs.py`、
`/tmp/scia_quick/` 为审稿用临时副本，均在仓库外。）

---

## 轮次 2 复核（非作者）

复核时间：**2026-09-21 06:00 UTC**（= 13:00–14:0x CST）。本轮只做定向复核：逐条验证轮次 1 的必查项是否真的修好、
修复是否引入新问题。工作区在本轮期间再次被改写，结论锚定文末"轮次 2 快照"（sha256）。

### 结论

**仍需修改**：轮次 1 的 5 条硬伤在**代码与归档 JSON 层面已实质修复**（我逐条亲自复算/亲自执行确认，见下表），
**未发现新的科学阻断项**；但**"报告文本与归档 JSON 数值同步"这一与 R1/R2/R8 同源的失效模式没有修干净**——
`README.md` 仍有 4 处数字与其自身引用的机读证据矛盾（§4.1 `0.000913`、§4.4 drizzle/通量比、§4.7 三个耗时、
§4.6 标题 `6/6`），其中 §4.1 的 `0.000913` 恰是 R8 明确要求改的数；另有一处**撤回意见的马甲**（C1 的稻草人公式
仍留在 `code/step1_analytic.py:194` 与 `results/step1_analytic.json` 的 note 里）。
**阻断项 = 报告与证据不一致（事实错误类），不是科学结论本身。**

### 逐条复核

| 上轮号 | 复核结论 | 我核到的证据（我自己读到/执行到的值） |
|---|---|---|
| **R1** 归档 step8 与代码不自洽 | **已修复** | `step8_real_frame.json`：`in_frame_instrument.sky_median_adu=1202.0`（正）、`img_p01=1153.0`、`img_p99=1419.0`、`img_max=65535.0`、`n_sat_like=1248`；`wcs_refinement.n_matched_within_tol=827`、`n_in_frame=827`、`shift_px=[-0.2626180477768685,-0.31928714988544016]`、`shift_arcsec=0.006779988939107636`；`single_frame_gate.n=157`、`n_inliers=151`、`sigma_obs_mag=0.02651983775720457`、`verdict="PASS"`、`items` 非 null。**我亲自执行交付代码**（`step8_real_frame.load_real(main_p)` + `step3_forward_vs_photflam.refine_shift`，主帧 `testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts`）：`BZERO=32768.0`，`median=1202.0000`，`p01=1153.0000`，`p99=1419.0000`，`max=65535.0000`，`bg_rms=13.6286550725979`，`n_sat_like=1248`，`n_src=827`，`shift=[-0.26261805 -0.31928715]`，`n_match=827/827`，`shift_arcsec=0.00677998893910764` ⇒ **与 JSON 逐位一致**，§4.7 的机读证据现已存在。 |
| **R2** 盲检耗时相差 445× | **部分修复** | 数量级问题已修：JSON `guided_vs_blind_real.blind_wall_time_s=1.7511251850082772`（≈1.75 s，不再是 890.7 s），`guided_wall_time_s=2.0929827929940075`，`guided_sec_per_fit=0.004822540997682045`，`blind_detections=13163`，`guided_candidates=434`。**但 README §4.7:254-255 写的是"引导 2.32 s（0.0053 s/星）"与"盲检测 1.68 s"，三个数都与归档 JSON 不符**（2.32≠2.0930、0.0053≠0.004823、1.68≠1.7511），也不符日志 `run/SCI-401/logs/step8_real_frame.log`（`guided 2.1s` / `blind 1.8s`）。§9 的 R2 行（`README.md:419`）同样写死"现为 1.68 s"。 |
| **R3** N2 判据方向相反 | **已修复** | `step7_negatives.py:58-66` `resimulate()` 走**真 Poisson 前向**：`mu=clip(fr["mu"]+sky_delta_e)` → `r.poisson(mu)` → `+r.normal(0,read_noise)` → `/gain` → `clip(round,0,saturation)`；`:137-139` N2 对 `mult∈(0.5,1,2,4,8)` 逐档重画帧；`:182` `pass_=bool(rows2[-1]["sigma_obs_mag"] > rows2[0]["sigma_obs_mag"]*1.5)` ⇒ **要求"必须响应"**，方向与 `ACCEPTANCE_SPEC` 一致。JSON N2：`monotone=true`（`:167-170` 只在 `mult≥1` 子族上判定）、`response_ratio=2.910948485672687`（我复算 0.0733541784782176/0.025199407972781862=2.9110）、`rows` 0.0252→0.0481→0.0734、4× 判 `ABOVE_CEILING`。算术相加的确定性梯度已降级为 `arithmetic_gradient_control`（1.08×，明确"不作为通过依据"）⇒ R13 一并修复。 |
| **R4** N0/N4/N5 恒等式 | **已修复，N5 的"未通过"是真的** | **N4** 改读 step5 实测值：`step7_negatives.py:242-245` `s5["frame_independence"]` → `kA=2.07278579295233e-17`、`kB=3.336901807409466e-17`（与 `step5_calibration_gate.json → frames[A/B].calibration.k_photo` 逐位一致），`k_ratio_measured=1.6098633147502515`（=step5 `k_ratio`），**不再是 `kB:=kA/0.62`**。**N5** 换成对帧 A 实测通量的真实过裁剪（`:261-276`，`|r−median(r)|<tol`，tol=None/0.05/0.01/0.002/0.001）：σ_obs 0.045344→0.038031→0.019762→0.003562（单调下降），`sigma_floor` 0.009638→0.008586→0.002926→**−0.004911**，四行判定**全 PASS**。**"未通过"确实是真的**：`:288` `pass_=any(verdict=="BELOW_FLOOR")` ⇒ `pass_=false`；JSON `n_pass=5/6`、`n_injection_response_pass=3/3`；我独立复算下界 `rho_lo=1−3·1.166/√n`：n=48→0.4951、n=45→0.4785、n=22→0.2542、**n=5→−0.5644**（`floor=−0.004911` ⇒ 隐含 `sigma_fit_white=0.008702`）⇒ 下界在 n<12.2 时数学上变负，过裁剪样本不可判红。**不是自我开脱，是如实登记。** |
| **R5** C1 稻草人 | **部分修复（撤回到位，但马甲留在代码/JSON 里）** | `DOC_CORRECTIONS.md:130-139` 有独立"## 已撤回"节，逐字写明撤回原因与我的意见一致（"被订正的原文口径 `Var≈σ_pix²·N_eff+F·w/g (w=ΣP³/ΣP²)` **不存在于任何文档**；`06_photometry.md` 只有 §1–§8（无 §2.1/§3.1），`run/RELEASE-02/parallel/06.md` §2.2 给的是 Horne 形式"）；新 C1 换成 N5 的 σ_floor 下界失效（真实发现，数字与 N5 逐位一致），**不是换马甲**；原 2.149× 改记为 C8"自由背景简并代价，**不是文档错误**、无需订正"。**但**：`code/step1_analytic.py:194` 与 `results/step1_analytic.json → sigma_fit_composition_form.note` 仍逐字写"`06.md §2.1` 的一阶展开 `Var≈sigma_pix²N_eff+F·w/g（w=ΣP³/ΣP²）`……一阶式低估方差"——**即被撤回的误引本身仍在交付代码与机读结果里**。我复核 `grep -rn "P³|P\*\*3|ΣP"` 于 `run/RELEASE-02/parallel/06.md`、`docs/plugins/algorithms_phase1/06_photometry.md`、`docs/science/PHOTOMETRY.md` ⇒ **零命中**，该式确实不存在于任何权威文档。 |
| **R7** 预算二次计入 | **已修复** | `scia_calib.py:225` `sig_robust_mc = mc_sigma_obs(sig_i_robust, 0.0, tag=tag+":robust")`、`:222` `sig_white_mc = mc_sigma_obs(sig_i_white, 0.0, ...)` ⇒ **只合成噪声，extra_sigma=0.0**（注释 `:223-224` 明确记录此前二次计入）。`scia_common.py:427-431` `Budget.sigma_ceiling` 只在平方和中各计一次（`sigma_fit_robust²+psfsys²+color²+gaia²+flat²+skyres²+q²`）。ceiling 确已收紧：帧 A `sigma_ceiling=0.05683000488871848`（≈0.0568，修复前 0.074312）、帧 B `0.1030148789260557`、帧 C `0.15989923273887163`，与 README §4.2 表逐位一致，三帧判定仍 PASS。 |
| **R8** `delta_after_m` 多乘 2.5 | **部分修复（代码已修，README §4.1 未同步）** | `scia_calib.py:153` `delta_after_m=float(mad_sigma(dm - pred))`，`:149` 注释"dm/pred 都是**星等**残差（不是 dex）⇒ 不再乘 2.5（此前多乘了 2.5）" ⇒ **代码已修**。step5 JSON `frames[A].calibration.delta_after_m=0.019732468945651404`（不再是 0.049331）✅。**但** `results/step1_analytic.json → spatial_gain_recovery.sigma_obs_with_m_model=0.00036516034225241733`，而 `README.md:106` 仍写"拟合 `m`(deg2) 后 `0.000913`" —— 这正是我 R8 建议里说"应约为 0.000365"的那个数，**JSON 已改对、README 没改**。 |
| **R9** `m̂` 方向反了 | **已修复** | `step6_apply_and_units.py:59` `m_hat = 10.0 ** (0.4 * dm)`；`:57` 注释"pred 是**星等残差**多项式 ⇒ 归一化修正场 `m_corr=10^(+0.4·pred)=1/m_gain`"；`:76-83` 星等域残差与不改正对照。JSON `magnitude_only`：`resid_mag_median=0.005106310487555277`、`resid_mag_mad_sigma=0.0197324689456575` **<** `resid_mag_mad_sigma_no_m=0.04534425693654346`、`m_correction_improves=true`；`m_hat_range=[0.9620,1.0792]`、`m_corr_definition="m_corr(x,y) = 10^(+0.4 * poly(x,y)) = 1/m_gain"`（与 step5 逐帧 `m_corr_definition` 一致）。G5 判据已加入方向自检（`GATES.md:12`"m 改正后残差 0.01973 vs 不改正 0.04534（改善=True）"）。 |
| **R10** `quick` 干净状态崩溃 | **已修复（我实测）** | 我用**临时副本实测**（不动机内文件）：把 `results/*` 复制到 `/tmp/r2_results_nostep6`（**故意不含 `step6_apply_and_units.json`**），patch `scia_common.RESULTS` 后执行 `step9_collect.main()` ⇒ 输出 `[step9] 缺失结果文件：['step6']（quick 模式跳过 step6 时正常）`、`[step9] gates 6/9 PASS`、`EXIT OK`，`G2 NOT_RUN | step6 未运行（quick 模式）`、`G5 NOT_RUN` ⇒ **不崩溃**。静态对照：`step9_collect.py:71-76` `if s6 is None` → G2 NOT_RUN；`:125` `ap = None if s6 is None else s6["apply"]` → G5 NOT_RUN；`s8` 全部用 `(s8 or {})` 保护。（未采用"原地改名"法，避免动到非授权文件。） |
| **R11** 残差分布一致性是死代码 | **已修复** | `step5_calibration_gate.json → frame_independence.residual_distribution_ks` 存在且非空：`n_A=48`、`n_B=54`、`statistic=0.12268518518518519`、`pvalue=0.7868810385958391`、`median_A=-0.0008585`、`median_B=-0.0013562`、`mad_A=0.045344`、`mad_B=0.057457` ⇒ README §4.2"KS 检验 `D=0.1227`、`p=0.787`（n_A=48, n_B=54）"逐位一致。 |
| **R14** 判据出处引用错 | **部分修复** | `README.md:8` 已改为 `docs/plugins/algorithms_phase1/06_photometry.md` **§4.1**，`:9` 还主动注明"该文只有 §1–§8，**没有 §2.1/§3.1**"，`:10-11` 把 §2 的实测预算归给 `run/RELEASE-02/parallel/06.md` 并声明 `run/` 追溯性弱 ⇒ README 层面已修。**残留**：`code/scia_common.py:409` 仍写"`06.md §2.1/§3.1` 形态"（未区分是哪一份 06.md），`code/step1_analytic.py:194` 仍把不存在的公式归给 `06.md §2.1`（见 R5）。 |
| **R18** 裸 NaN | **已修复** | 我对 `results/*.json` 全部 9 个文件做**严格解析**（`json.loads(..., parse_constant=raise)` + 正则扫 `NaN`/`Infinity` 裸 token）：`gates.json / step1 / step2 / step3 / step4 / step5 / step6 / step7 / step8` **全部 OK，BAD COUNT=0**。轮次 1 出问题的 `step5 → sigma_psfsys_noise_expect`（帧 C）现在是有限值 `0.0420…` 而非 NaN。 |
| **R21** astropy 版本 | **已修复** | `README.md:384-388` 明确："文献记录核到的是 **8.0.1**（上游 main 线），而**本机实际运行环境是 astropy 7.0.1**……7.0.1 的对应实现语义相同但行号可能不同，本实验**未在 7.0.1 上核对该行号**"；`code/README.md:42-45` 亦写"本机实测环境 **Python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1**……photutils 3.0.0 / sep 1.4.1 行号来自**文献与上游源码核对**，不是本机安装版本" ⇒ 两个版本已区分。 |

### 新发现（修复引入/遗留的不一致）

1. **§4.4 的 drizzle 与通量比数字已过期（最典型的新引入不一致）**：`README.md:182-183` 写
   "drizzle 式重采样尺度比 `2.08505e-17` vs 期望 `2.08506e-17`……星点总通量比 = `2.0924e-17`"，
   而 `results/step6_apply_and_units.json → downstream_consumption` 现在是
   `drizzle_scale_ratio_median=2.0605959498598527e-17`、`drizzle_scale_ratio_expected=2.0605814118015737e-17`、
   `flux_ratio_after_over_before=2.0533577964549896e-17`。**`results/GATES.md:12` 与 `gates.json` 已同步成 2.0606e-17，只有 README 没同步**。
   根因可查：`README.md` mtime 13:51:01，`step6_apply_and_units.json` mtime 13:52:29，`GATES.md` 13:53:43 ⇒ README 写完之后又重跑了一次 step6，README 未回填。
2. **§4.6 标题与正文自相矛盾**：`README.md:218` 标题仍写"`results/step7_negatives.json`，**6/6 通过**"，
   `:235` 正文写"`5/6` 条通过"、`:233` N5 行写"✘ 未通过"、JSON `n_pass=5/6` ⇒ 标题是轮次 1 遗留。
   另 `:220-221` 留了一个**没有数据行的孤儿表头**（`| # | 负例 | 实测 | 判定 |` + 分隔行，紧接着就是 `>` 引用块，`:226` 才重新开表）⇒ 轮次 2 编辑的排版残留。
3. **N2 的 `kind` 标签与实际扫描范围不符**：JSON `N2.kind="…天光抬升 0.25×–8×"`，但 `rows` 只有
   `0.5/1.0/2.0/4.0` 四档（`step7_negatives.py:137` 循环请求了 8.0，`:141-142` `if out is None: continue` 把它丢了）；
   且 `monotone=true` 是在 `mult≥1` 子族上判定的（`:167-170`），**该限定只写在代码注释里、没写进 JSON**，
   只读 JSON 的人会看到 0.5×→1× 下降（0.0352→0.0252）却标 `monotone=true`。
4. **`pass_` 的参照行与 `response_ratio` 的参照行不同**：`pass_` 用 `rows2[0]`（0.5× 行，比值 2.08），
   而 `response_ratio` 用 1× 基线（2.91）；两者都 >1.5，结论不变，但同一负例里两个"响应倍数"口径不一致。
   另 `:172-176` 的 `response_ratio` 用了 `next(...) and A/B` 的 `and` 写法（靠左值非零才偶然正确），属残留代码味。
5. **§9 把 R6 说成"审稿确认作者正确"与轮次 1 原文不符**：`README.md:438` 写"R6/R12/R22 为审稿确认作者正确或纯风格项"，
   但轮次 1 R6 的结论是**夸大**（中）并给了具体改写建议；现状 `README.md:66` §2.3 仍写"预算项（**逐项由本帧推导**）"、
   `GATES.md:8` G1b 仍写"由本帧推导"，而同文 §4.7 自己把真实帧的 `σ_color`/`σ_gaia` 标为 `null`（不可自算）⇒ R6 未修，且被误分类。
   （R12 的"独立复算"仍是同一表达式两份拷贝：`step6:61` `(k_photo*m_hat)*img` vs `pl.apply_photometry`，`rel=0.0` 仍非独立证据；
   `step6:60` 注释"不复用 apply_photometry 的实现"与实际不符。该条不在本轮必查清单，仅记录。）
6. **未发现其他新的科学阻断项**：GATES.md 的 **G6 已从 PASS 降为 PARTIAL** ✅（`GATES.md:13`，`step9_collect.py:144` `verdict="PASS" if s7["n_pass"]==s7["n_negatives"] else "PARTIAL"`），合计"8/9 项 PASS"✅；G1–G5/G7/G8 与 JSON 一致；`step8` 的 `σ_obs=0.026520 ∈ [0.006008, 0.032456]` 与 JSON 的 `sigma_floor/sigma_ceiling` 一致。

**§4.2/§4.4/§4.6/§4.7/§5/§9 数字抽查（≥10 个，均我自己从 JSON 读出）**：
✅ §4.2 帧A `0.04534/0.00966/0.05683/1.009/2.09%`、帧B `0.05746/0.01252/0.10301/0.727/1.85%`、
帧C `0.05172/0.01457/0.15990/n=105/0.390/3.95%`、`σ_psfsys=0.0250`（真值 `0.0256326`）、`σ_color=0.00574`、
`σ_flat=0.0197`、KS `D=0.1227/p=0.787`、`k_B/k_A=1.60986`、比值/期望 `0.9981`、`σ_obs 比=1.267`；
✅ §4.4 残差中位 `0.00511`、改正 `0.01973` vs 不改正 `0.04534`、退化族 `4831.2/4865.1/4840.2`、
`0.01891/0.01812/0.01344`、`Δlocation=0.8633228601`、`k 比=0.1369863014`、`Δσ_residual=0`、`rel=0.0`；
✅ §4.6 N1 `0.04534/0.04719/0.04798/0.07618/0.09879/0.15959`（3.52×）、N2 `0.0252/0.0481/0.0734`（2.91×）、
N3 阈值 `0.04`=`6.974×`真实值、N4 偏差 `0.19%`、N5 序列；
✅ §4.7 `1202/13.63/1248/827` 源、shift `(−0.263,−0.319)`=`0.0068″`/`827/827`、`196/1.49%`、
`σ_pix=40.94`、`σ_psfsys=0.01367`、`σ_flat=0.01956`；
✅ §5 `1.009/0.727/0.390`、帧C `σ_psfsys 0.106 vs 真值 0.039`（我复算比 `2.691×`，README 写 2.7×）、N1 `3.52×`、N2 `2.91×`、`1.08×`（0.048968/0.045344=1.0799）。
❌ 不一致的 4 处即上文新发现 1/2 与 R2、R8 行所列（§4.1 `0.000913`、§4.4 两个 2.08e-17/2.09e-17、§4.7 三个耗时、§4.6 标题 `6/6`）。

### 我无法核实/未核实

1. **未重跑全链**：本轮是定向复核。我**亲自执行**的只有 `load_real`+`refine_shift`（R1）、
   `step9_collect` 缺 step6 的容错（R10）与对既有 JSON 的独立复算（`rho_lo`、`response_ratio`、
   `σ_ceiling` 组成、`sigma_obs_with_m_model`）。**没有**重跑 step1–step8 全链，因此"固定 seed 下逐位可复现"
   仍只做了代码级与归档一致性检查。
2. **N2 的 8× 档为何被丢弃**：JSON 只有 0.5–4×，我**没有**复跑 `step7` 去确认 8× 帧是"匹配星<5"还是别的分支导致 `measure()` 返回 `None`。
3. **§4.7 的 2.32 s / 1.68 s 出自哪一次运行**：这两个数既不匹配当前 JSON 也不匹配现存两份日志（13:43 与 13:52），
   我只能断定"与归档证据不符"，无法确定作者是从哪份中间产物抄的。
4. **R6 之外的轮次 1 中等项（R12/R15/R16/R17/R19/R20）未逐条复核**（不在本轮必查清单）。
5. **文献 DOI/一手实现行号**：未独立访问 Crossref/arXiv 或上游源码复核 §8.1/§8.2。
6. **并发改写**：`README.md`(13:51:01)、`step1/4/5/6/7/8` 的 JSON 与 `GATES.md`(13:53:43) 均在本轮前后被改写；
   若作者在我复核后再次重跑，R2/R8 与"新发现 1"需按新 JSON 重判。

### 轮次 2 快照（sha256，供作者对账）

```
README.md                              44ca265f7b798b9408e3d6eb3ccc683e44d899063e3a036849fd176f0b261e5e
results/step8_real_frame.json          915c5d106afcac5369d67502389163878769e3c9aafb83bedaf77eb8e99cb521
results/step5_calibration_gate.json    50595f6d24252e2af543301f9b89ee42dbef443612f73e6a7fb16257536ae231
results/step6_apply_and_units.json     dff77d27b2b77efea084dfb53a153e06fb73f3a1b4b72abbf13ab505d24b95b5
results/step7_negatives.json          dbf2a31fc85acee250908f54cdd8014e16217470a54892e3f009d48d581f87ea
results/step1_analytic.json           c3a0b137d11db29232c629aaa125edb7bc4510b360b074176c2972d61e78f50e
results/GATES.md                      2e183299fe20d685e8c29de413c3788438770a5ad73cb9f0cbb62e394031ce61
results/DOC_CORRECTIONS.md            de5f23c8d539e679037de530e40e5b656adf43624b1a0521c3e0985aeb059b1e
code/scia_calib.py                    3b5e99bbfcea1972e4bd0d84335255b12463a0183a51d2d2f31de461e159b5be
code/step7_negatives.py               1b08037d9280cc7c02be2d2ab940e2881415be0d2552ab927063794ce05a4388
code/step6_apply_and_units.py         589e5ea29e86dc66213b21edc4edc4ba6c45501c77cca18dc743b72eff4ddcb3
code/step9_collect.py                 09992cba426af78d1c7afbffba0afb45c9cedf47d7d9bffc9ebe614f21ef5725
code/step1_analytic.py                3ef604fcd8cfd9382044246118bc0a448c7eb8cb2da929650d57654b3f5ce446
code/README.md                        923b1bdcdedb6e09b7484cd605c783f794f90b989330a1f3155a033b0c501c35
```

本轮审稿**只写入本文件**（`results/REVIEW.md`），未运行任何 git 写操作，未改名/修改仓库内其他文件；
R10 的实测在 `/tmp/r2_results_nostep6/` 副本上进行（`/tmp/r2_*.py` 为审稿脚本），均在仓库外。

