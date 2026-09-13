# M7 · A_SCI_DEF · P0（第二层合并：L19 SCI 数学轴 + L20 ALG 轴 + L21 跨文档口径矩阵）

- 合并代理：M7（第二层合并验证代理）；输入：`问题扫描/_cache/L19.md`(30) + `L20.md`(29) + `L21.md`(14) = 73 条。
- 定稿口径：每条均按当前树重读（**关键句 grep 命中 + 定点 read 回读**双口径；未以单次 read 的行文本作等值判据），四态判定见 `_merge/M7.md` §1。
- 复核时点：RESCUE-V3 并发修改 `lib/` 期间；全部锚用 `path::符号`，行号仅注「复核时 N」。

---

## M7-A-001 「科学权重」唯一性被三处不同对象宣称，且 SCI-UPM §5 两式在实现侧被证明不等价（Σ 域裁决 = 单 control 内跨帧）

- 类别: A_SCI_DEF · 建议优先级: **P0**（L21-004 原 P1 → 本代理升档；L20-001、L21-003 并入本条）
- 位置:
  - `docs/science/PHASE2_UPM.md::§5 科学权重块`（复核时 :46 三因子积式 vs :47 raw/normalized 两行）；`::§3 量纲表`（复核时 :29 `w_UPM`: ADU⁻²）；`::§7 k_corr 缩放不变量`（复核时 :79）
  - `docs/algorithms/UPM_SOLVER.md::§2 F1/F2`（复核时 :17，grep `w_norm = w /`）
  - `docs/contracts/DATA_SEMANTICS.md::§19.3「科学权重唯一冻结式」句`、`::§23.4「control_ivar 是唯一科学权重源」句`
  - `docs/GLOSSARY.md::pixel_weight 行`（复核时 :12，「= ivar」却标「无量纲」）
  - 实现（本代理亲自读码定案）：`lib/phase2/src/upm.cpp::p2_upm_normalized_weights`、`::p2_upm_raw_weight`；`lib/phase2/src/sampler.cpp::p2_sample_controls_cached`（control_id 赋值处）
- 证据摘录（当前树逐字）:
  > SCI-UPM :46 `w_UPM = quality_factor × geometric_reliability × control_ivar`
  > SCI-UPM :47 `raw = quality × control_ivar ; normalized = raw / Σraw · geom (per-control)`
  > upm.cpp `sums[obs[i].control_id] += raw[i]; … out_norm[i] = (s > 1e-12) ? raw[i] / s * rel : 0.0;`
  > sampler.cpp `o.control_id = (std::uint64_t)ci;`（ci = 采样器 cell 索引，跨帧共享）
- **Σ 域裁决（本条的定案依据，父层指令要求我落）**：`sums` 以 `control_id` 为键，而 `control_id` 是**每 control cell 一个、跨帧共享**的索引 ⇒ Σ 域 = **单 control 内跨全部观测（跨帧）**。由此
  1. 每 control 的归一权重和恒等于 `rel`（默认 1.0），与 control_ivar、k_corr、σ_bg、N_retained 的**绝对量级无关**；
  2. :46 三因子积（量纲 ADU⁻²）与 :47（除以 Σraw 后为无量纲份额 × geom）**只在 Σraw≡1 时相等**，而 Σraw 是随帧数与噪声变化的量 ⇒ **两式不可能同时为真**；
  3. 实现取 :47 ⇒ 冻结 SCI 的 :46、§3 的 `w_UPM: ADU⁻²` 量纲声明、§7「control_variance 随 k_corr 线性缩放」不变量、§10「禁改 k_corr」条款在**产品解算面同时不可观测**（k_corr 进 Σraw 的分子与分母，精确相消）；
  4. 「对每帧权重和=1」与「Σ_k 1 = n_frames × 8²」两读法在实现侧**都不成立**（成立的是「Σ 每 control 权重和 = rel」）⇒ 三处「唯一」宣称互斥得证。
- 权威依据: 宪章 §1.1（SCI 高于 ALG/代码）、§4.1（weight 量纲不得模糊）、§6.3（背景模型权重与叠加权重严格分离）、§12.3-1/-2（科学定义=算法=代码=测试且机器一致）
- 影响: UPM 的「control-ivar 感知加权」这一科学声明在产品面只剩**同一 control 内的跨帧相对比例**；绝对精度加权、k_corr 保守因子、误差传播链（control_variance→ivar→HISS）在解算端失去可观测证据。属「冻结定义式与实现式不等价且实现取被引用式之外的那一式」的数值级冲突 ⇒ P0。
- 建议处置: ①负责人裁决唯一式（:46 绝对量 or :47 归一份额）并重冻结；②若保 :47，须把 §7 k_corr 缩放不变量与 §10 禁改条款改注「仅诊断面」；③§3 量纲表 `w_UPM` 随式改（ADU⁻² vs 无量纲份额）；④GLOSSARY 补 `w_UPM`/`pixel_weight` 两条并给域限定（叠加积分权重=ivar；UPM 梯度权重=w_UPM）；⑤DATA §19.3/§23.4 的「唯一」加域限定。
- 置信度: 高（Σ 域为代码亲验事实；代数结论可手推复现）
- related: L21-003（并入）、L21-004（本条即其升档定稿）、L20-001（同一事实的 ALG 侧）、L21-001（独立事实，另条）、L07-004、L08-008/009、DISP-P2UPM-003、A-02（`40_OWNER_DECISIONS.md` weight_mode=2 之争，**不同事实**勿并）、移交 M4
- 四态判定: **仍成立**（两份 SCI + 两份 ALG + GLOSSARY 复核时文本未变；实现侧本代理亲验）

---

## M7-A-002 ALG 权威层把「ivar 缺失时回退 support」写成 weight_mode=2 的唯一语义（L21-001，维持 P0）

- 类别: A_SCI_DEF · 优先级: **P0**
- 位置: `docs/algorithms/PHASE2_INTEGRATION.md::§/weight_mode 块`（本代理复核时该篇在位；L21 记「复核时 :」以符号锚为准）；对照 `docs/science/CONTROL_WEIGHT_SNR.md::§4`（复核时 :42）、`docs/science/PHASE2_UPM.md::§5 SCI-UPM-WEIGHT-001`（:54 禁 support 进权重）
- 证据: ALG 把「缺 ivar ⇒ 用 support 顶」登记为 weight_mode=2 的**定义**，而 SCI-UPM-WEIGHT-001 明写「禁 production 乘 support^p；support 仅 eligibility/coverage 语义」，SCI-CW 又把 mode=2 写成 support×snr² ⇒ 三个层对同一枚举值给三种语义，ALG 层用「定义式」写法完成越权。
- 权威依据: 宪章 §1.1（ALG 不得反向定义 SCI 语义）、§6.3（背景模型权重与叠加权重严格分离、support 不得冒充权重）、§4.1（support 与 ivar 量纲不同）
- 影响: 叠加权重面（weight_mode=2）与 UPM 梯度权重面被同一枚枚举值缝合；support 是覆盖度而非精度，按本条语义实现即把「覆盖多的像素」当「精度高的像素」，直接改变 mosaic 数值。
- 处置: ①ALG 该节改为「现状登记 + DISP 条目」姿势（HIPS_WRITER.md::§0 的写法是模板，见 `_merge/M7.md` §越权句式表 :710 行）；②枚举值语义由负责人裁决（A-02）；③补可机检判据（C-03 建议条款）。
- 置信度: 高（三处文本本代理 grep 复核在位）
- related: M7-A-001（同一权重主题的**另一事实**：per-control 归一化）、M7-A-102（L19-002 snr²≡x²·ivar 恒等式）、M3-A-002（两份 FROZEN SCI 互斥，`40_OWNER_DECISIONS.md::A-02`，**不同事实面勿并条**）、L07-004、L21-004、C-03
- 四态判定: **仍成立**
