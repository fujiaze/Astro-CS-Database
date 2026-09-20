# CHANGE CLAIM — WEIGHT-FREF-PERFRAME-001

## 1. 一句话

把 `weight_mode=2`（逆方差加权）的**组间公共 `F_ref` 硬闸门**改为**逐帧用自己的 `F_ref,k`**，
组间一致性降级为**报告字段**。依据 = 负责人 `GAP_AUDIT.md` **§9.49 定案 2**（帧间独立）。

## 2. 权威依据（逐字）

> 「极度异常值拒绝，并抛出错误…**这玩意应该是帧间独立的，为啥要组间对比**」
> 「**不同光学系统的帧混装不得报错**」
> 「门只有一个：**单帧标定是否可信**…与其它帧无关」

同族先例：`PHOT-GATE-DROP-001`（同一轮删除组间 k 散度门）。本条是它在 **Phase2 权重链**上的对应物。

## 3. 缺陷（实测）

`run/RELEASE-02/e2e/` 的 6 帧跨 **t2_m1 / t2_m2 两块指向**：

```
astrocs mosaic --weight_mode 2
rc=2  phase2 failed: node integrate failed:
  weight_mode=2 requires per-frame ivar products; 6/6 frames missing ivar;
  frame-SNR weight chain NOT closed (unclosed_invalid_reference_flux):
  ASTROCS_REFERENCE_FLUX 逐帧不一致（组内公共通量标度要求）
```

两块各自的组公共锚：t2_m1 `F0 = 7.829149539147795e-09`、t2_m2 `F0 = 7.749508598e-09`（差 1.017%）。

**为什么这是缺陷而不是保护**：`FREF-BASELINE-001` 的
`scope="frame_independent_fixed_magnitude"` 下

```
F_ref,k = 10^(−0.4·(m_ref − ZP_k))
```

`ZP_k` 依赖**该帧自己的**光学系统/滤镜 ⇒ **不同指向或不同光学系统的帧合法地有不同的 `F_ref,k`**。
旧闸门等价于「不同光学系统的帧混装即报错」，与 §9.49 定案 2 **直接冲突**，
并使 `weight_mode=2` 在**任何多指向拼接**上完全不可用（链路永不闭合）。

## 4. 科学论证：配对性定理**不**要求跨帧相等

`WEIGHT-SCI-001` 配对性定理（旧注释自己写明）：

```
SNR_f = a_f·F_ref/σ_f  ⇒  SNR_f²/F_ref² = a_f²/σ_f² = w_f  ⇔  分子分母同源
```

**「同源」= 同一帧的 SNR 与该帧的 F_ref 同源**，是**逐帧内**的约束，
**不是**跨帧约束。逐帧用 `F_ref,k` 时 `w_k = SNR_k²/F_ref,k²` **仍然精确成立**（同源保持）。
旧实现用组标量反而在跨指向时**破坏**了同源性。

**独立佐证**：`PhotometricMosaic`（PMM）源码级研究 —— 其 `scale` 允许任意量级
（注释显式支持 12bit vs 16bit、2× 尺度差），**无任何跨帧一致性门**。

## 5. 改动面（最小）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h` | `FrameWeightInput` 加 `double ref_flux = 0.0`（逐帧 `F_ref,k`，0=回退组标量）；`WeightChainResult` 加 `std::vector<double> reference_flux_k` |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | `w` 用 `f_ref_k = (f.ref_flux>0 ? f.ref_flux : reference_flux)`；非有限/非正 → `kUnclosedInvalidReferenceFlux`（**判据保留，只换作用域**） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | 逐帧 `in.ref_flux = fref`；删 `ref_err` 硬失败分支；新增报告字段 `reference_flux_spread_rel` / `_frame` / `_noncommon` / `reference_flux_gate`；manifest 补 `photscale_spread_gate` |
| `tests/unit/p1001_real_nodes_test.cpp` | 原 `RED-3`/`RED-5` **语义反转**为 `SPREAD-REPORT-1`/`-2`（详见 §6） |

**未改**：任何容差、任何阈值、`P1_PHOT_MAX_SIGMA_DEX`/`P1_PHOT_MIN_FIT_STARS` 等**帧内**判据、
`weight_from_snr` 公式、`g_k²` 处理、稀疏层路径。

## 6. 测试处置（**不是删测试，是反转断言**）

`p1001_real_nodes_test.cpp` 的两个负例原本**锁的就是这条门**，必须同步反转 —— 否则测试与裁决自相矛盾：

| 原 | 新 | 断言变化 |
|---|---|---|
| `P1PHOTBROKEN RED-3`（16 dex 散度） | `SPREAD-REPORT-1` | `applied==false` → **`applied==true`**；无产物 → **有产物**；新增：`spread_dex>0`、`spread_warn==true`、`spread_gate` 含 `none`、**无** `degraded_reason`/`photscale_error` |
| `F-INSTR RED-5`（0.107 mag 散度） | `SPREAD-REPORT-2` | 同上 |

**判别力保持（关键）**：两个用例仍会在**把组间门加回来**时立即转红（`applied` 会变 `false`）。
**帧内门未被削弱**：`RED-2`（`n_matched=0` ⇒ 无拟合证据）等帧内负例原样保留。

## 7. 验证（实测，可复现）

```
# 1) 单测
ninja -C build p1001_real_nodes_test && ./build/tests/unit/p1001_real_nodes_test
  ⇒ rc=0，CHECK failed 计数 = 0

# 2) 全量回归
ctest --test-dir build
  ⇒ 100% tests passed, 0 tests failed out of 467

# 3) 生产端到端（6 帧跨 2 块指向）
astrocs mosaic --json <w2 cfg> -y   ⇒ rc=0
```

`p2_integrated.json` 实测：

```json
{"weight_mode": 2,
 "weight_basis": "frame_snr_ivar",
 "weight_source": "frame_snr",
 "snr_chain_closure": "closed",          ← 修复前不可达
 "snr_chain_used": true,
 "reference_flux_spread_rel": 0.010172361787007554,   ← 报告，非门
 "reference_flux_noncommon": true,                    ← 报告，非门
 "reference_flux_gate": "none (owner ruling 9.49: frame-independent; pairing is per-frame SNR_k^2/F_ref,k^2)"}
```

## 8. 影响面

- **科学结论**：`weight_mode=2` 从「**从未在生产数据上跑通**」变为**已跑通且权重链闭合**。
  这解除了论文「逆方差加权叠加」主张的**唯一阻塞**。
- **向后兼容**：`ref_flux=0`（未设置）时行为与旧实现**逐位一致**（回退组标量）。
  仅当生产写侧提供逐帧 `F_ref,k` 时才有新行为。
- **`weight_mode=1`（等权/方差加权）不受影响**。
- **风险**：逐帧 `F_ref,k` 若写错，权重会错 —— 但该风险**已被帧内判据与 `F_ref,k>0` 有限性检查覆盖**，
  且旧闸门**并不能**防住它（旧闸门只查跨帧一致性，不查单帧正确性）。

## 9. 版本

本包**不碰版本号**（负责人令）。