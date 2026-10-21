# 新核心架构独立设计审核记录

日期：2026-10-21（Asia/Shanghai）。工作起点：`adc7f33d58359ac3d65014f239d7f53fe92e4912`。交付为[架构规范](../../docs/engineering/rebuild/README.md)，本记录是过程与证据，不另定义科学或工程合同。

## 范围与执行身份

负责人要求以五份已验收科学规范开展全新架构设计，设计经过独立审核后再建立独立重构分支。本轮仅编辑规范、导航、索引及本记录；不改科学地基原文、生产算法和sources同步参考，不建重构分支。

作者通读最高设计、五份科学地基、工程/细节导航、文档治理和同主题旧架构/对象说明。旧工程仅用于确定适用边界，未把旧节点或schema继承为新设计约束。五份科学地基的旧候选标记保留，按负责人本次已验收指示使用；历史证据不改写。

独立审核者为本聊天的只读子Agent `/root/architecture_review`，未参与六份架构初稿编写，未编辑文件。其实际阅读范围为五份完整科学地基及六份架构规范；修订后再次阅读API、OBJECTS、EXECUTION、VERIFICATION和SYSTEM调用关系。审核只检查设计合同，不执行历史实验或现有生产调用，不构成外部负责人验收。

科学输入的SHA-256（原文件字节，便于锁定本次依据）：

| 文件 | SHA-256 |
|---|---|
| OBSERVATION_COORDINATES.md | ab45645688663be9c53061bcde37b7931adc4633845a1ec78851cfd9cd1a1c07 |
| GAIA_RESPONSE.md | 4acfd8d998654b3846c4fbe85cf6e3508e2489e417dab9ef4e9b67bae7d00111 |
| SPHERICAL_DRIZZLE.md | d197fd200f0a4d69bab44c865ce9fc670bb82069957a02aceb41093765ee827b |
| STATISTICS_SKY_COADD.md | 724faa978cc6804686b4c91d2c0aa07da38ab6a1362e238ea122f6d41b9df492 |
| INDEPENDENT_ORACLES.md | 1285ca452fbf23a78d3e7c058d5b255805d79b615404a486837600dbca4b807a |

## 发现、修订与复核

| 项 | 原缺口与失效例 | 修订归属 | 最终复核 |
|---|---|---|---|
| P1：LOO前置 | 只要求N≥4/可信Γ，可能对Hα/Red或异PSF的不同真实均值排异 | SYSTEM、API规定共同可比较均值、外部固定Γ、Gaussian适用域；否则skip，保留合法混合。VERIFICATION加入混响应/同数据Γ负例 | 关闭 |
| P1：反馈标准操作 | 下游要求几何细化，但只读provider无公开refine和替换版本合同；可能新R拼旧K/权 | API新增refine_geometry/RefinableGeometryProvider及完整快照事务；OBJECTS定义GeometryRevision，共同A/Ω的全源失效、T/H/K/P/Γ/选择/权/Export重建，累计预算不重置；EXECUTION计新旧快照峰值，VERIFICATION列反例 | 关闭 |
| P2：天测物理误差 | astrometry声明参数影响，API却只返WcsModel，参数根/同数据误差无明确传递边 | API返回AstrometricSolution，OBJECTS给参数cov/root/influence/共享身份/模型偏差；固定WCS数值界与真实误差分开，分支稳定Jacobian、拓扑改变联合MC，缺失明确unknown/conditional | 关闭 |

独立最终结论：三项发现全部关闭；当前未发现仍阻碍按科学合同独立实施的实质架构缺口。架构合同的独立设计审核通过，具备交负责人接受的条件。该结论不代表合同驱动器已经运行、实现已验证、生产吞吐或真实数据产品验收，也不自行授权创建重构分支。

## 文档核验与未执行范围

对六份新规范及四份被修改Markdown导航/最高设计检查140个本地文件链接，未发现失效路径；新文件均进入DOCUMENT_INDEX，无重复path。`git diff --check`通过。新增审核记录的相对入口链接另行核验。检查只是文档结构核验，不证明科学或实现正确。

新核心尚无实现；未编译、未运行科学测试、未跑真实FITS、未进行新合成实验或性能测量。VERIFICATION里的矩阵是后续独立实现任务，不能记成已执行证据。待负责人接受本架构后，才按所锁设计提交建立独立重构分支，实施标准接口及独立真值，再直接重写；选择性复用算法也必须经新合同和独立验证。
