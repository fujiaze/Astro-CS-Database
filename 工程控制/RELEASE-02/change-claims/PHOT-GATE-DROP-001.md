# 变更 claim：PHOT-GATE-DROP-001 —— 删除 Phase1 测光「组间 k 散度门」

- **claim_id**: `PHOT-GATE-DROP-001`
- **日期**: 2026-09-20
- **状态**: **已实施**（代码侧），文档侧待同步
- **提出**: 前台（E2E 实测触发）
- **依据**: 负责人裁决 `工程控制/RELEASE-02/GAP_AUDIT.md §9.49 定案 2`（逐字见下）

---

## 1. 变更对象

`lib/infrastructure/scheduler/src/module_adapters.cpp` —— Phase1 测光节点（`astrocs.phase1.photometry`）中的
**组间（帧间）标度一致性门** `P1_PHOT_MAX_SPREAD_DEX`（原值 `0.02` dex ≈ 0.05 mag 峰峰）。

```cpp
// 变更前（fail-closed 分支）
if (scales_complete && kmin > 0.0) {
  const double spread_dex = std::log10(kmax / kmin);
  if (!std::isfinite(spread_dex) || spread_dex > P1_PHOT_MAX_SPREAD_DEX) {
    photscale_error = "photscale inconsistent across frames (...)";
    scales_complete = false;              // ← 整组拒绝施加
  }
}
```

## 2. 依据（负责人裁决，逐字）

> 「**极度异常值拒绝，并抛出错误**，其他的合理范围都可以接受。**这玩意应该是帧间独立的，为啥要组间对比**」
> 「**不同光学系统的帧混装不得报错**」
> 「门只有一个：**单帧标定是否可信**…**与其它帧无关**」

**独立佐证**（`reports/RELEASE-02/pmm-study.md` §Q-B，PMM 源码级研究）：
**PhotometricMosaic 亦没有任何「拒绝帧」的跨帧一致性门**。其模型本身是相对的（`scale` 允许任意量级，
源码注释**显式支持 12bit vs 16bit、高达 2× 尺度差**）；跨帧比例只用于**星点匹配预筛**，
超限的后果是「**这一对星不匹配**」而**不是「这一帧被拒绝」**。
PMM-STUDY 独立给出的**第一条建议**即为「删除 `module_adapters.cpp` 的组间 k 散度门」。

## 3. 触发证据（**E2E 实测，非推断**）

2026-09-20 E2E 首跑：`normalize` **rc=0**（无报错），但产物 `p1_phot.json`：

```json
{"photometry_applied": false,
 "photscal": 1.0,
 "degraded_reason": "photscale_incomplete",
 "photscale_error": "photscale inconsistent across frames (max/min=1.074356 = 0.031148 dex > 0.020000 dex); refusing to apply a mixed photometric system"}
```

⇒ **测光归一化在生产上完全不执行**。该门是 **fail-closed** 的（拒绝执行而非产出错值），
故此前 L4 产物**不是错的，是缺一步** —— 但**负责人「所有帧已归一化到同一测光体系」的前提在此前所有运行中都不成立**。

## 4. 变更内容（**门降级为报告字段，不是简单删掉**）

| 项 | 处置 |
|---|---|
| fail-closed 分支 | **删除** |
| `spread_dex` 计算 | **保留** |
| `photscale_spread_dex`（值） | **新增**，写入 `photometry_provenance` 与 manifest 两处 |
| `photscale_spread_warn`（超参考值仅提示） | **新增**（PMM warning 范式） |
| `photscale_spread_gate` | **新增**，值 `"none (owner ruling 9.49: frame-independent)"` —— **显式声明此处不是门** |
| **帧内**判据 `P1_PHOT_MIN_FIT_STARS=3` | **不变** |
| **帧内**判据 `P1_PHOT_MAX_SIGMA_DEX=1.0` | **不变** |

## 5. 影响面

**代码**：`module_adapters.cpp` 一处（含注释订正）。

**文档（待同步，走本 claim）**：
- `module_adapters.cpp` 原注释把 `P1_PHOT_MAX_SPREAD_DEX` 描述为「负责人判据 = 同组帧间 k 峰峰 ≤ 0.05 mag」，
  **该表述已被 §9.49 推翻** ⇒ 注释已就地订正；
- `docs/science/SCI-PHOT-001` 与 `docs/plugins/algorithms_phase1/` 中的对应表述须同步订正为
  「**组间一致性是语义目标与报告字段，不是门禁**」。
- **（2026-09-20 第 2 轮返修补登，对应审稿意见 B1）论文模块——本次已同步完成**：
  - `run/RELEASE-02/paper/modules/P3P5P6-phot-upm-mosaic.md`：**新增 §1.3a「证据基座变更登记」**；
    §1.3(d)、§1.4⑥、§2.2⑥、§7 边界 7、§8 次要缺口① **五处**均加「修复前实现 / 该门已于 2026-09-20 删除」限定；
    §8 增第 4 项待补实验（修复后 12 板块 E2E 重跑复核）。
  - `run/RELEASE-02/paper/modules/P4-snr-propagation.md`：§P4.4 ② 增 L4 数据来源声明（比值型量不受影响）。
  - `run/RELEASE-02/paper/modules/P9-real-data.md`：§1.2 增 L4 数据来源声明；§1.6⑥ 补注「after 0/12」为修复前状态。
  - **此前本 claim §5 的影响面清单未列论文模块** ⇒ 论文侧零登记，属登记缺口；本条即补登。

**产物语义**：`photometry_applied` 由恒 `false` 变为**按帧内判据真实决定**；
`photscal` 由恒 `1.0`（伪）变为**真实 `k_photo`**。

## 6. 变更前后实测（同一输入，E2E 2026-09-20）

| 输入 | `photometry_applied` | `photscal` | `photscale_spread_dex` | `warn` | `degraded_reason` |
|---|---|---|---|---|---|
| `p1_m42_t2_m1_red` 前 | **false** | 1.0 | （未落盘） | — | `photscale_incomplete` |
| `p1_m42_t2_m1_red` **后** | **True** | **5.647065217024146e-17** | 0.031148161841308315 | True | **None** |
| `p1_m42_t2_m2_red` **后** | **True** | **5.121220877410474e-17** | 0.0158799… | False | **None** |

## 7. 一致性回归

- 该变更**不改变任何科学公式、不改变任何帧内判据、不改变输出单位**；
- 只把「跨帧一致性」从**门禁**降级为**报告字段**，与负责人裁决和 PMM 的 warning 范式一致；
- **未放宽** `P1_PHOT_MIN_FIT_STARS` / `P1_PHOT_MAX_SIGMA_DEX` 两条帧内门。

## 8. 红线声明

- 本变更**有负责人裁决背书**（§9.49 定案 2），**不是**为让门变绿而放宽判据；
- **未删除任何检查项**、**未使用 waiver**；
- 变更方向是「**让被误阻断的科学步骤恢复执行**」，而不是「掩盖失败」。