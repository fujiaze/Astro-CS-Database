# 阶段一运行配置（规范副本）

本目录存放**可提交**的阶段一（`normalize`）运行配置的**规范副本**。

## 与 `run/**` 的关系

`run/**` 下的同名配置是**工作副本**，按设计被 `.gitignore` 整体忽略（见仓库根 `.gitignore`），
因此对它的修改**不会进入提交**、也不构成可追溯记录。**规范副本以本目录为准**：需要复现或审计
某次运行时，应以本目录的文件为据；`run/**` 下的文件只反映本机当时的实际状态。

## 文件

| 文件 | 说明 |
|---|---|
| `p1_rerun_config.json` | 阶段一重跑配置，含 t2 / t3 两个块；通带声明为 `Astrodon E-series R`（`filter_passband` 与 `photometry.fit.filter` 各块一处，共四处）。内容与 `run/PHOT-FIX/p1_rerun_config.json` 逐字节一致。 |

## 通带声明的口径

通带取值取自数据集记载的真实滤镜（`testdata/index.json` 的 `datasets[].filters`），该键**必填**
且取值必须是滤镜库的具名条目；**不得**依赖任何通用名自动映射（滤镜库语义与三条冻结条款见
`docs/engineering/contracts/CONFIG.md` 的滤镜库一节）。

声明值 `Astrodon E-series R` 的**来源为负责人指定**；其"该曲线物理上对应本设备那片滤镜"这一主张
**未经外部标准核对**，正确性由合同指定的「单帧测光一致性残差」判据承担。
