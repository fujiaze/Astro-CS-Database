# SCI-RES-01 — 科学课题研究包（服务于逐层重构）

状态：READY_FOR_OWNER_EXECUTION_ORDER（已编制，未执行）
依据：负责人指示「这些是待研究课题，研究清楚后落到科学文档里」；以及重构包 GAP_AUDIT.md 的 GAP-024（权威数值缺口）。
上位权威：ASTROCS_DESIGN.md；研究纪律见 RESEARCH_PROTOCOL.md。

## 1. 本包要回答什么

最新权威文档里**点名要存在、但没有数值或定义**的科学量。它们挡住的不只是 config/defaults.json，还包括后续所有「按新设计重建」的任务——没有定义，实现就只能猜。

| 课题 | 问题 | 现状 |
|---|---|---|
| R-001 | 稀疏帧内 SNR 层的**控制点密度**（点/度²）取多少 | docs/plugins/algorithms_phase1/07_noise_snr.md 只有字段名与单位，无值 |
| R-002 | **标量门**的判据与阈值（何时标量模型够用、何时必须升级空间模型） | 同文件只说「标量门失败 → 自动升级为空间模型」，无判据 |
| R-003 | **暗场-亮场曝光容差**的定义式与适用域（负责人已给值 5 s，待落文档） | docs/science/CALIBRATION.md 只有 K=t_light/t_dark 语义，无容差 |
| R-004 | 已裁决值落文档（5 s 与 R-003 定义写进 docs/science/CALIBRATION.md） | 待执行 |
| R-005 | drizzle pixfrac 默认值的权威出处（语义权威已知，数值默认仅有实现出处 0.8） | docs/science/DRIZZLE.md 有语义无默认值 |

## 2. 执行方式

1. 每个课题一张任务卡，由研究 Agent 独立执行；**必须给出可复跑脚本与反例**；
2. 结论就绪（CONCLUSION_READY）后交负责人批准；
3. 批准后：由重构包把数值写进权威文档与 config/defaults.json；
4. 研究期间**不得**改 docs/science/**、docs/algorithms/** 与任何产品代码。

## 3. 与重构包的边界

- 研究包**不**做工程重构；重构包**不**自行裁定科学数值（缺值就标 pending_authority 并指向本包课题）；
- 两边共用 工程控制/ 目录规范与 AGENTS.md 硬禁令；
- 本包以 CONCLUSION_READY 为交付，不以「改完文档」为交付（文档修订需批准）。
