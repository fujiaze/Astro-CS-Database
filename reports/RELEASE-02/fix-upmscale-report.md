# RELEASE-02 FIX-UPMSCALE 报告：UPM per-control 归一化绝对阈值 → 尺度无关判据

> 任务：把 UPM per-control 权重归一化的绝对阈值 `1e-12` 改为尺度无关判据，使生产
> `control_ivar` 尺度下 C 场复活（接缝真根因）。
> 范围：仅 `lib/algorithms/coverage/src/upm.cpp` 一处判据 + 一个判别力回归；
> 不改任何科学公式/默认容差，不动 `sky_plane.*` 与 `g_k`，不改 `docs/**`。
> 执行者：FIX-UPMSCALE（SubAgent）；零 git 写权限；未跑 ninja/cmake/ctest。

---

## 1. 摘要

- **改动**：`lib/algorithms/coverage/src/upm.cpp:582-586`（原 `569`）把
  `if (sums[ck] > 1e-12)` 改为 `if (s > 0.0 && std::isfinite(s))`；归一化
  表达式 `raw_w / s * reliability` 逐字不变。
- **独立复现（非引用）**：4 次生产 L4 模型（`bitref_16w` / `bitref_1w` /
  `mosaic_out` / `mosaic_out_w1`）的 `p2_upm_model.bin` 内嵌 JSON 全部为
  `iterations=1, converged=1, objective=0.0, use_ivar_weight=1`，且 `C` 为 49 行
  全空（非零项 0）⇒ `p2_upm_fit` 生产空操作。
- **生产尺度**：`p2_samples.json` 实测 29336/29336 个 control 的
  `Σ control_ivar < 1e-12`（`control_ivar` 中位 5.595e-22；Σ 中位 5.101e-21，
  最大 1.637e-20）⇒ 旧门 100% 判假、权重整体清零。
- **判别力**：新回归 `Phase2Upm.ProductionScaleControlIvarKeepsCFieldAlive` 用
  生产量级 `control_ivar=5.6e-22`：旧判据下归一化份额 = 0（C=0/objective=0/
  iterations=1），新判据下份额 = 0.25（C1≈14.97/objective=7.09e-21/iterations=2）。
- **测试影响**：既有全部 UPM 用例的 `control_ivar` 使 Σ≈O(1)（≥0.25）⇒ 行为
  逐位不变；新增 1 个用例（gtest 自动发现，无需改 CMake）；fail-closed `rc=2`
  与「真零权重仍得 0」均保持。

---

## 2. 缺陷确认（独立复算，证据落盘）

### 2.1 生产模型统计（直接读 `p2_upm_model.bin` 内嵌 JSON）

| run | iterations | converged | objective | C 非零项 | use_ivar_weight |
|---|---|---|---|---|---|
| L4-rebuild/bitref_16w | 1 | 1 | 0.0 | 0 | 1 |
| L4-rebuild/bitref_1w | 1 | 1 | 0.0 | 0 | 1 |
| L4-rebuild/mosaic_out | 1 | 1 | 0.0 | 0 | 1 |
| L4-rebuild/mosaic_out_w1 | 1 | 1 | 0.0 | 0 | 1 |

证据：`run/RELEASE-02/fix-upmscale/prod_model_stats_evidence.log`、
`run/RELEASE-02/fix-upmscale/prod_cfield_evidence.log`。

### 2.2 生产 control_ivar 尺度（直接读 `p2_samples.json`）

| run | n_obs | n_controls | control_ivar 中位 | Σ 中位 | Σ 最大 | Σ<1e-12 占比 |
|---|---|---|---|---|---|---|
| bitref_16w | 277234 | 29336 | 5.595e-22 | 5.101e-21 | 1.637e-20 | 100.0% |
| bitref_1w | 277234 | 29336 | 5.595e-22 | 5.101e-21 | 1.637e-20 | 100.0% |
| mosaic_out | 277234 | 29336 | 5.595e-22 | 5.101e-21 | 1.637e-20 | 100.0% |
| mosaic_out_w1 | 277234 | 29336 | 5.595e-22 | 5.101e-21 | 1.637e-20 | 100.0% |

证据：`run/RELEASE-02/fix-upmscale/prod_ivar_scale_evidence.log`。
结论：旧绝对门 `sums[ck] > 1e-12` 在生产输入下对**每一个** control 都判假 ⇒
`raw_w[i] = 0.0` 全量执行 ⇒ C 场恒 0、objective 恒 0、iterations 恒 1。
合成测试用 `control_ivar=1.0`（Σ≈8 > 1e-12）故永远绿，尺度裂缝因此长期不可见。

---

## 3. 改动（file:line）

`lib/algorithms/coverage/src/upm.cpp:582-586`（`build_impl` 的 `compute_raw` lambda）：

    修改前（原 569）：
        if (sums[ck] > 1e-12)
            raw_w[i] = raw_w[i] / sums[ck] * m->controls[ck].reliability;
        else
            raw_w[i] = 0.0;

    修改后：
        const double s = sums[ck];
        if (s > 0.0 && std::isfinite(s))
            raw_w[i] = raw_w[i] / s * m->controls[ck].reliability;
        else
            raw_w[i] = 0.0;

只改「是否执行归一化」的判据；`raw_w / sums * reliability` 的表达式、控制流、
`sums` 的聚合顺序（1T/2T 归约）与所有默认容差均未变。

---

## 4. 判据理由：为何尺度无关，且为何不改变数学语义

1. **定义域就是「Σ > 0」**。冻结文档 `docs/science/PHASE2_UPM.md` §5 定义
   `w_cell = w_UPM / Σ_cell w_UPM × control_reliability`，`Σ_cell w_cell =
   control_reliability`。该分式有定义的充要条件是 `Σ_cell w_UPM > 0` 且有限；
   `1e-12` 从来不是科学阈值，只是历史实现里「防除零」的绝对魔数。
2. **量纲分析**。`w_UPM = quality × control_ivar`（ADU^-2，绝对值量）。任何
   「与绝对常数比较」的门都会随观测量的单位/尺度失效；`Σ > 0` 是无量纲不变式
   （对 `w_UPM → λ·w_UPM`，λ>0，判据不变）。
3. **数学语义不变**。归一化表达式逐字保留；改判据只决定该 control 的观测是否
   进入份额归一化。对任何旧判据为真的输入（Σ>1e-12），新判据也为真 ⇒ 结果
   逐位相同；仅对旧判据误杀的 `0 < Σ ≤ 1e-12` 区间恢复应有行为。
4. **`std::isfinite` 的作用**：仅拒绝 `+inf/NaN` 的退化求和（例如 raw 权重
   溢出）；有限正值一律归一化。这不是放宽 fail-closed（见 §5）。
5. **真零权重仍得 0**：若某 control 全部观测权重为 0（如 `quality_flags=16`
   ⇒ `quality_factor=0`），则 `Σ=0` ⇒ 判据为假 ⇒ `raw_w[i]=0.0`。真正的 0
   权重不会被当作「有效观测」而生成值。

---

## 5. fail-closed 保持（未放宽）

- `p2_upm_raw_weight`（定义 `upm.cpp:1342`；非法 ivar 检查 `upm.cpp:1361`）对
  `use_ivar_weight=1` 且 `control_ivar` 非有限或 ≤0 **仍显式返回 `rc=2`**；
  本任务未触碰该函数。
- `compute_raw` 对 `rc!=0` 仍直接向上传播（串行 `upm.cpp:559-563`；并行
  `upm.cpp:542-550`）；`build_impl` 收到后 `p2_upm_close + return 2`
  （`upm.cpp:632-638`）。
- 新回归内显式断言：`control_ivar=0` 的 build 仍 `rc=2` 且 `*out_model` 保持 null。
- 新回归内显式断言：全 `quality_flags=16`（qf=0）时 build `rc=0`、C=0 且有限，
  无 NaN/Inf（真零权重不生成值）。

---

## 6. 判别力证明（关键）

### 6.1 回归用例

`lib/algorithms/coverage/tests/synthetic_gate.cpp:534` 新增
`TEST(Phase2Upm, ProductionScaleControlIvarKeepsCFieldAlive)`：

- 输入：2 帧 × 同一 control × 每帧 2 个观测（10/20 与 25/35），
  `control_ivar=5.6e-22`、`uncertainty=4.2e10`、`quality_flags=1`。
- (a) 复算 `Σ w_UPM`，断言 `0 < Σ < 1e-12`（证明旧门必然判假）与
  `Σ>0`（证明权重合法）；对同一 raw 分别套用旧/新判据，断言
  `old_norm == 0.0` 且 `new_norm ≈ 0.25`。
- (b) 端到端：`p2_upm_build` rc=0；gauge 帧 C=0；待解帧 `|C|>1e-6`；
  `iterations>1`；`objective>0`；`converged==1`；校准后 frame1 的 raw 25 落回
  frame0 参考面（|out-10|<0.5）。
- (c) fail-closed：`control_ivar=0` ⇒ build rc=2。
- (d) 真零权重：qf=0 ⇒ C=0 且有限。

### 6.2 独立数值复算（旧 vs 新）

`run/RELEASE-02/fix-upmscale/discriminative_proof.py` 忠实复算 `build_impl` 的
权重/ M 更新 / C 的 CG 求解 / objective / 收敛判据（同一 double 算术）：

    per-control Σ w_UPM = 2.240000e-21   (旧门 >1e-12 ? False)
    quantity        OLD sums>1e-12   NEW sums>0 & finite
    norm            0.0              0.25
    M               0.0              15.0
    C1              0.0              14.970059880239521
    iters           1                2
    conv            1                1
    objective       0.0              7.086294842796382e-21

旧判据下复算签名与 §2.1 生产实测完全一致（iterations=1, converged=1,
objective=0.0, C=0）⇒ 该用例对旧阈值**必然失败**（判别力成立）；新判据下
C 场复活。证据：`run/RELEASE-02/fix-upmscale/discriminative_proof.log`。

### 6.3 为什么既有测试抓不到

`tests/unit/.../synthetic_gate.cpp` 的 `make_obs` 统一 `control_ivar=1.0`
（原注释「方差 1 → ivar=1 等价等权」，现 `synthetic_gate.cpp:71-73/84-87`）。
Σ≈O(1) 远大于 1e-12 ⇒ 旧门为真 ⇒ 测试永远绿。全仓测试中 `control_ivar`
取值仅 1.0 / 4.0 / 0.5 / 100.0 / 2500.0 等 O(1) 量级，无一进入生产尺度区间，
故本回归填补了判别力空白。

---

## 7. 同类绝对阈值清单（本任务只修 569；其余交前台裁决）

对全仓 `lib/**` 中「对权重/方差/ivar 用绝对常数做判据」的位置逐一评估：

| # | file:line | 代码 | 是否误杀生产量级 | 处置 |
|---|---|---|---|---|
| 1 | upm.cpp:582-586（原 569） | `sums[ck] > 1e-12` | **是（100%）** | **本任务已修** |
| 2 | upm.cpp:1391 | `p2_upm_normalized_weights`: `(s > 1e-12) ? raw[i]/s*rel : 0.0` | **是（同一缺陷）** | 同类，未修（超范围）；建议前台裁决 |
| 3 | upm.cpp:708/724/733/758/775/784 | `den > 1e-12` / `den <= 1e-12`，den=Σ 归一化份额×huber | 否（den 无量纲、O(reliability)） | 低风险，不改 |
| 4 | upm.cpp:614/622 | CG 退化 break：`pAp <= 1e-30`、`rs_new < 1e-24` | 否（归一化后 obs_w=O(1)） | 数值保护，不改 |
| 5 | sky_plane.cpp:633 | `support_floor = 1e-9 * max(max_node_weight,1e-300)` | 否（已相对化，尺度不变） | 安全；且 sky_plane 禁改 |
| 6 | sky_plane.cpp:324/945/1030 | σ/SNR 分母 `1e-12` | 否（仅 ≤0 时兜底/分母下限） | 不同类；sky_plane 禁改 |
| 7 | sampler.cpp:864 | `sigma = (s0>0)?s0:1e-12` | 否（仅 ≤0 兜底） | 不改 |
| 8 | noise_snr/cpp/src/noise_model.cpp:632；noise_snr/wrapper_phase1/noise_model.h:16 | `variance_floor = 1e-12` | 否（对 control_variance≈1e21 无关；仅 σ<1e-6 时封顶权重） | 冻结配置默认，禁改；登记知悉 |
| 9 | platesolve/.../ipv_robust_refine.cpp:592/702/807 | σ/denom `1e-12`/`1e-9` | 否（像素位置 σ 域，非 control ivar） | 不同域，不改 |
| 10 | calibration/src/dark_optimizer.cpp:251 | `denom <= 1e-12` | 否（曝光/计数回归方差，非 ivar） | 不同域，不改 |
| 11 | drizzle/healpix_drizzle/drizzle_engine.cpp:443 | `fabs(denom) < 1e-15` | 否（gnomonic 投影分母 cos 角） | 不同域，不改 |
| 12 | acr/tests/classic/e05_convolution.cpp:125/152/280 | `sum > 1e-12` 核归一化 | 否（测试内，核和 O(1)） | 测试代码，不改 |

**重点上呈**：#2 `p2_upm_normalized_weights` 是与 569 **完全同一**的缺陷
（同一表达式、同一 `1e-12`），只是当前 solve 路径不调用它（`build_impl` 自带
`compute_raw`）。它是导出 API（`P2_API`，`upm/include/astro/phase2/upm.h:155`），
任何直接调用方在生产 `control_ivar` 下都会得到全 0 权重。按任务约束「只修 569」
本任务未改，**请前台裁决是否同批修复**。

> 订正（2026-09-20）：#2 已随 CONFORM-FIX-B 003 修复 —— lib/algorithms/coverage/src/upm.cpp:1654 现为 out_norm[i] = (s > 0.0 && std::isfinite(s)) ? raw[i] / s * rel : 0.0;（绝对门 1e-12 已去；2026-09-20 复核）；「请前台裁决是否同批修复」不再需要（旧文 =「请前台裁决是否同批修复」）。

---

## 8. 测试影响

- **既有测试**：全部 UPM 用例的 `control_ivar` 使 `Σ w_UPM` 为 O(1)（最小 0.25），
  旧门与新门在这些输入上同为真 ⇒ 结果**逐位不变**，无既有断言需改。
- **新增**：`synthetic_gate.cpp:534` 一个 TEST；`gtest_discover_tests`
  （`lib/algorithms/coverage/CMakeLists.txt:153`）自动发现，**无需改 CMake**。
- **语法门**：以生产编译参数 `g++ -fsyntax-only` 通过：
  - `upm.cpp`（`-std=gnu++17 -Wall -Wextra -Wpedantic -Wconversion`，exit 0）
  - `synthetic_gate.cpp`（`-std=gnu++20 -DGTEST_HAS_PTHREAD=1`，exit 0）
  未跑 ninja/cmake/ctest（按任务约束，构建/测试由前台执行）。
- **待前台执行**：`ctest -R phase2_synthetic_gate`（至少
  `phase2_synthetic_gate.Phase2Upm.ProductionScaleControlIvarKeepsCFieldAlive`）。

---

## 9. 预期对接缝的效果

UPM 加性 C 场的设计目的（`docs/science/PHASE2_UPM.md:7`）是「使校准后样本
`calibrated = raw - C_f(p)` 在全域可比」——把所有帧归一化到公共面。C 场恒 0
时 `calibrated ≡ raw`，逐帧零点差完全保留 ⇒ 帧间不可比 ⇒ 接缝。

修复后（预期，需前台 L4 重建验证）：

1. `p2_upm_fit` 不再空操作：`iterations>1`、`objective>0`、`C` 非零项 > 0；
2. `p2_upm_calibrate_block` 真正扣除 C ⇒ `p2_corrected` 逐帧零点被拉到公共面
   ⇒ 跨帧重叠区（seam）残差下降；
3. 因 `C` 进入 `model_hash`，生产 `model_hash` / `p2_corrected.*` 必然变化，
   与旧生产 run 的逐位对比预期**不同**（这是修复的预期结果，不是回归）；
4. 迭代从 1 次升到收敛（≤ `max_iterations=100`），`p2_upm_fit` 运行时间会上升，
   请前台关注 perf 门（既有合成用例同样跑满迭代，故数值行为有先例）。

注：接缝可能仍有其他贡献项（天光面、g_k 由 FIX-GK 负责）；本修复只保证 UPM
加性 C 场这一项不再被清零。

---

## 10. 复现命令

    export TMPDIR=/dev/shm/astrocs_upmscale
    python3 run/RELEASE-02/fix-upmscale/discriminative_proof.py
    # 生产证据（只读）：
    python3 - <<'PY'  # 见 run/RELEASE-02/fix-upmscale/prod_*.log 的脚本
    PY

---

## 11. 边界与未做

- 未跑 ninja/cmake/ctest；未提交 git（零写权限）；未改 `docs/**`、`sky_plane.*`、`g_k`。
- 未改 #2 `p2_upm_normalized_weights`（超范围，已在 §7 上呈）。
- `corrected` 口径按 SD-22（方案 B）保持，由 FIX-GK 负责。 订正（2026-09-20）：口径已由 §9.67 定案 1（工程控制/RELEASE-02/GAP_AUDIT.md:2548-2555）定为 corrected = raw − δ_k（保留 B_ref；additive_mode 默认 delta，module_adapters.cpp:6129-6130）；raw − C_k 全减不再是默认。B 报告要求写「生产默认 raw − C」（依据 §9.54/§9.55 S8）已被更晚的 §9.67 定案 1 取代，不采纳。旧文 =「按 SD-22（方案 B）保持」。
- 生产 L4 重建与接缝度量复跑属前台验收步骤，本任务仅交付实现 + 判别力测试 + 证据。
