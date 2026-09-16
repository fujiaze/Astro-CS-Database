# SCIENCE_CORRECTNESS —— 科学正确性优先（负责人指令 2026-09-16）

## 一句话

**科学文档必须正确。** 有独立证据证明它错了，就改它——不需要为"它是冻结的"而保留错误，也不需要等负责人逐条批准。

依据：负责人原话「**我不要遵循旧制，我要求科学文档必须是正确的**」，与 `AGENTS.md §8`（科学歧义才问负责人）一致：
**证据能定的事，执行行自己定；证据不能定的，才上呈。**

## 判定规则（四条，取代旧的"一律不得修改"）

| 情形 | 处置 | 谁决定 |
|---|---|---|
| 文档与**外部标准/文献**不符（如 FITS WCS Paper II、IVOA 规范） | **改文档**（+ 改实现，若实现也随之错） | 执行行，凭标准原文 + 实验 |
| 文档与**可复现实验**不符（如蒙特卡洛证明某默认值统计上不成立） | **改文档**（或改实现，取决于哪边被证明错） | 执行行，凭实验（脚本 + 输出 + 样本量） |
| 文档**自相矛盾**（同文两处互斥） | 判定哪一处与标准/实验一致，改另一处 | 执行行，凭证据 |
| 两份权威文档互斥**且证据无法判定** | 上呈负责人 | 负责人 |

## 变更 claim（每次订正必填，落在本文件 §登记表）

订正科学文档时必须在同一提交里登记一条 claim：**改了什么 / 依据什么证据（可复跑）/ 影响面（哪些模块、合同、registry、测试）/ 版本递增**。

## 登记表

| # | 日期 | 文档 | 改什么 | 证据（可复跑） | 影响面 | 状态 |
|---|---|---|---|---|---|---|
| SC-001 | 2026-09-16 | `docs/science/PHASE3_HIPS_TO_FITS*.md`、`docs/algorithms/PHASE3_PROJ_IMPL*.md`、`docs/algorithms/PHASE3_RESAMPLE*.md`、`docs/contracts/DATA_SEMANTICS.md`、`docs/plugins/algorithms_phase3/14_projection.md` | ① CAR/AIT 按 FITS WCS Paper II 把 `CRVAL2`（含 LONPOLE 默认规则）纳入映射；② AIT 域界 `A<2` → `A≤1`；③ CAR native 极行（θ=±90°）fail-closed；④ 投影集合以 `ASTROCS_DESIGN §5.3` 的**八投影**为准，删除对**已废止宪章**条款的引用；⑤ registry 升 v3、v1 退场 | `reports/PROJECT-GOVERNANCE-01/research/R-1_投影WCS数学正确性.md`；`run/PROJECT-GOVERNANCE-01/R-1/`（astropy 7.0.1 逐点对拍 22 组，max 6.854e-13°；`probe_p3proj` vs `probe_fixed` 显示 CRPIX 处 dec 0→30；追加 dec0≠0 用例后现 Oracle 必红） | `lib/algorithms/projection/**`（公式+registry）、共址测试、`tests/unit/v6_p3_proj/**`、相关门 | 待执行 |
| SC-002 | 2026-09-16 | `docs/science/NOISE_MODEL.md`、`docs/algorithms/NOISE_ESTIMATION.md` | ① §4 `min_samples` 默认值 5 → **64**；② §2:16 ivar 单位 `pixel⁻²·ADU⁻²` → `ADU⁻²`；③ §5:51 兜底式记号拆为两条显式式；④ 删除「不改 SCI，以代码为准」的权威倒置表述 | `reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md`；`run/PROJECT-GOVERNANCE-01/R-5/exp1..exp9`（N=5 ⇒ 单 patch 偏差 −19.2%、SCI 自设 5% oracle 通过率 0.6%；N=64 ⇒ −1.25%、92.8%；权重场误差 5:0.0686 / 64:0.0105 / 最优 0.0104） | `lib/algorithms/noise_snr/**` 默认配置与测试、SCI §11 oracle、`config/**` 反射 | 待执行 |

## 与旧做法的差别（一句话对照）

- 旧：文档冻结 ⇒ 发现文档错也只能"登记、上呈、等批准"，甚至写出"以代码为准"来绕过；
- 新：**文档错就改文档**（凭标准或实验），改了登记 claim；只有"证据判不了"的才上呈。
