# docs —— P2 跨帧绝对 SNR 的支撑推导与分册报告

本目录只放**不能由单元入口六件套承载**的深度内容。六件套（`../README.md`、
`../REPORT_paper.md`、`../REPORT_experiment.md`、`../refs.md`、`../code/`、`../results/`）
回答假说、方法、数据、结果、诚实边界、复现命令与佐证来源；本目录回答"为什么这么算"。

## 推导与订正

| 文件 | 内容 |
|---|---|
| `DERIVATIONS_P2.md` | 推导腿汇总：各常数的解析闭式与结构论证，对应论文的 [推导] 标注 |
| `LEDGER_CORRECTIONS_P2.md` | 本单元对既有表述的订正台账（每条含现象 → 证据 → 处置） |

## 分册实验报告

EXP-01…EXP-06 是本单元各条结论的完整实验报告，含推导、负例、门禁与逐点读数；
`../REPORT_experiment.md` 与 `../REPORT_paper.md` 只承载结论层。

| 文件 | 主题 |
|---|---|
| `EXP-01-DELTA-AND-ESTIMATOR.md` | 天光语义容差 δ 与逐源 SNR 估计量错配 |
| `EXP-02-STRUCTURE-CONTAMINATION.md` | 天光噪声估计的"结构污染"缺陷与判据 |
| `EXP-03-REGIONAL-SIGMA.md` | 区域化 σ_sky 的绝对准确性与跨帧一致性 |
| `EXP-04-RECONSTRUCTION.md` | 稀疏重建算子选型与稀疏/稠密精度对比 |
| `EXP-05-ABSOLUTE-SNR.md` | 绝对 SNR 表示的推导与三口径适用域 |
| `EXP-06-SNR-PHYS.md` | 帧内 SNR 的物理建模与重建（完整报告） |
| `EXP-06-SUMMARY.md` | EXP-06 的上呈摘要（面向负责人阅读） |

## 定案与调研

| 文件 | 内容 |
|---|---|
| `f-instr-canon.md` | 星点通量口径定案（RELEASE-02 裁决 A6） |
| `frame-snr-canon.md` | 帧级 SNR 定案 |
| `snr-propagation-design.md` | SNR 在全流程中的传播方案设计（误差预算口径） |
| `surveys/f-instr-survey.md` | 星点通量口径的文献与开源实现调研记录 |
| `surveys/frame-snr-survey.md` | 帧级 SNR 的文献与开源实现调研记录 |
| `SNR_WEIGHT_RESEARCH_PACK.md` | 权重模式研究包。**其中的"权重模式 / 权重档位 / mode0·mode1·mode2"整套概念不存在**，本文件仅作历史留痕，不构成现行规范；权重是阶段二按天球像素对应帧集合现场算出的派生量。本文件另承担第三方工具版本锚的溯源证据职责，被仓内版本一致性门禁逐字校验，**不得删改其中第三方版本字面量** |

## 复现

本目录为文档，无独立执行入口。支撑脚本见 `../code/`（`exp01`…`exp06`、`audit/`、
`reverse_verify/`），一键复现命令见 `../README.md`。
