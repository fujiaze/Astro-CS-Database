> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# ALG-P2-SURF-REJECTION — rejection reason / probability 规格

> 上游：ASTROCS_DESIGN.md §4（normalize）、§5（mosaic）、§6（export）

- 文档 ID：`ALG-P2-SURF-REJECTION`（`ALG-P2-SURF-001` 子文档）
- 上位：`DESIGN-P2-001 §5`；`UNIFIED §3/§6`；`PROJECT_SPEC §7/§8`；宪章 §4.1/§6.3（排异版本化）
- 裁决锚：`ADJ-GEN-02`（量不混名）；`FZ-GATE-SUPPORT-COVERAGE`；`ADJ-GEN-04`（标量降级）
- 既有实现级合同：`docs/algorithms/REJECTION_ALGORITHMS.md`（`ALG-REJ-001`）、`docs/algorithms/PHASE2_REJECTION.md`（`ALG-P2-REJ-IMPL-001`）——阈值/方法**原样继承**，本文件扩展 reason 分类与 probability，不重定义容差。
- 本任务条款：`ALG-P2S-REJ.1..7`
- 文献锚：Gruen, Seitz & Bernstein 2014 PASP 126,158（clipped mean stacking 与 PSF 差异下伪影控制）；Mosteller & Tukey 1977（robust biweight）

## 1 原则（`ALG-P2S-REJ.1`）

排异是**潜在污染状态的估计**，不是把异常值变成零（`DESIGN-P2-001 §5`）。必须输出 mask/count/**reason**/**probability**，且版本化（宪章 §6.3"排异算法及自动选择策略必须在 SCI/ALG 中版本化，有合成污染实验、边界条件和确定性验证"）。`rejection` 是独立合同对象，**不得并入 coverage**（宪章 §4.1）；不得作 coverage 使用（`UNIFIED §3`）。

## 2 reason 分类（`ALG-P2S-REJ.2`）

### 2.1 继承的判定面（原样，来自 `ALG-REJ-001`）

| reason | 语义 |
|---|---|
| `accepted` | 未被拒（含 UNDERDETERMINED 白名单外的保留样本） |
| `rejected_low` | 低于 lower threshold（禁以原始值正负号判向，`rejection.h:20-21` 冻结） |
| `rejected_high` | 高于 upper threshold |
| `underdetermined` | 样本数不足，未做拒绝判定（全接受语义，recall=0 显式） |

### 2.2 V6 污染类型分类（`DESIGN-P2-001 §5` 清单展开）

| 类别 id | 语义 | 证据特征（判据域） |
|---|---|---|
| `cosmic_ray` | 宇宙线/热像素 | 单帧紧凑高残差，compact 不生长（`large_scale` 不扩张） |
| `satellite_trail` | 卫星线/拖线 | 跨连通高残差结构，`large_scale` 生长（trail 扩张，`ALG-REJ-001 F13`） |
| `bad_column` | 坏列/坏像素 | 固定列/像素坐标跨帧一致异常 |
| `moving_source` | 移动源（小行星/卫星目标） | 跨帧位置单调移动的**科学信号**，默认不删 |
| `cloud_gradient` | 云/梯度 | 低频/大尺度残差与 UPM 背景模型失配 |
| `defocus_trail` | 失焦/拖线 | PSF 形状/集中度与模型失配（大 FWHM/椭圆度异常） |

- 一类一样本：`reason_class` 与 `rejected_low/high` 正交（拒绝方向）与 `reason_class`（污染机制）**分别落字段**，不得用一个整数承载两义（`ADJ-GEN-02`）。
- `moving_source` 可保留到**独立层**，不默认当缺陷删除（`DESIGN-P2 §5` 末段）。

## 3 probability（`ALG-P2S-REJ.3`）

```text
p_k(p) = P( 样本 k,p 属于污染状态 | 数据, Phase1 噪声, UPM 模型 )   ∈ [0,1]
```

- 逐样本输出 `reason` + `reason_class` + `probability`；方法/阈值/迭代上限**版本化**（`reject_profile_version`）。
- **阈值使用预测残差方差**（`DESIGN-P2 §5`）：`sigma_eff^2 = sigma_phase1^2 + J C_theta J^T`，其中 `J C_theta J^T` 是 UPM 参数不确定度（`ALG-P2-SURF-UPM.md §5`）；**禁止**用裸原始残差（未含 UPM 不确定度）作阈值。
- 小样本规则：`n <= 2` → `underdetermined` 全接受（recall=0 显式，不做伪剔除）；迭代上限 8（sigma/winsorized/averaged）、ESD max 10（**继承 `ALG-REJ-001`，不重定义**）。
- `probability` 仅门/推断输出，**禁止**进入 `weight.sources`/`weight_value`/`variance_from`（`FZ-GATE-SUPPORT-COVERAGE` 同构扩展；`UNIFIED §3`"rejection 门/概率，不是 coverage"）。

## 4 校准门（`ALG-P2S-REJ.4`）

在**预注册**污染注入集上（训练/验收样本不同，`SCI-PSFW-001 §3` 同构纪律）：

```text
可靠性: 按 p 分位分箱（每箱 >= FZ-AP2S-REJ-CALIB-BINMIN = 50 样本）
         |观测拒绝率 - mean(p)| <= FZ-AP2S-REJ-CALIB-ABS = 0.10
技巧分:  Brier skill score BSS = 1 - BS/BS_ref > FZ-AP2S-REJ-BSS-MIN = 0.10
```

- `BS = mean((p - y)^2)`（`y∈{0,1}` 真值），`BS_ref` = 基础率常数预测的 Brier 分。
- 任一门失败 → 该 probability **不得作为科学门**，须回退为显式阈值判定并登记 `unavailable`/校准失败原因（禁止声称概率校准）。
- 分箱样本不足（`<50`）→ 该箱不参与判定，但其覆盖必须登记（不得用不足样本冒充校准）。

## 5 fail-closed（`ALG-P2S-REJ.5`）

| 条件 | 行为 |
|---|---|
| 任一候选 values 非 finite | `invalid_input`（`ALG-REJ-001` F7） |
| `method=AUTO` 进 kernel | `invalid_method` |
| 空栈 / 资格数 < min_samples | `no_candidates` / `min_samples` |
| `n<=2` | `underdetermined` 全接受 |
| PERCENTILE×norm≠MEDIAN_CENTER 或 RCR×norm≠NONE | `invalid_configuration` |
| 非有限 weights/support | hard fail（不静默） |
| `sigma_eff` 未含 UPM 项 | REJECT（本任务新增；`DESIGN-P2 §5`） |
| probability 进权重面 | REJECT |

## 6 输出与 provenance（`ALG-P2S-REJ.6`）

每样本：`reason`（§2.1）、`reason_class`（§2.2，可多标签时取后验最大类 + 全类别概率）、`probability`、`sigma_eff` 组成（`sigma_phase1` 与 UPM 项）。
栈级：`status`（8 态，继承）、counts（accepted/rejected_low/rejected_high）、`iterations`、`method`、`profile_version`。
provenance：`reject_profile_version`、`sigma_eff_formula`、`calibration_status`（含 BSS 与可靠性表摘要）、`moving_source_layer` 指针（若保留）（`FZ-PROV-MINIMAL-SET`；`ADJ-GEN-03`）。

## 7 验证门（`ALG-P2S-REJ.7`）

1. 合成污染注入（宇宙线/卫星线/坏列/移动源/云梯度/拖线）分类 recall 与误拒率；
2. probability 校准：可靠性表 + BSS；把 probability 强制为 0/1 或未校准 → 门红；
3. 小样本 `n<=2` 全接受、recall=0 显式；
4. `large_scale` 只增不减：trail 扩张、compact cosmic 不生长；
5. 把 `support/coverage` 或 `rejection_probability` 当权重的 mutation → 门红；
6. 迭代上限/确定性：同输入同 reason/probability（固定序归约）。

阈值来源登记：`sigma 4.0/3.0/8iter`、`linear_fit 5.0/3.5/8`、`percentile 0.2/0.1`、`ESD alpha 0.05 max 10`、`large_scale min_structure 8/low 2/high 2` **全部继承** `ALG-REJ-001`，本任务不修改（"不得改冻结门/容差"）。
