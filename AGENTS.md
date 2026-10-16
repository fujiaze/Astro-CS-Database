# ACSD 协作约定

ACSD 的科学内核是 `normalize`、`mosaic`、`export` 三个独立命令。项目目标、产品和平台边界见[最高设计](docs/ACSD_DESIGN.md)。Windows Qt6 GUI 独立构建，通过 JSON、子进程与 JSONL 事件调用科学内核。

## 阅读与依据

开工先通读最高设计，再读本主题的[科学分册](docs/science/README.md)、[工程合同](docs/engineering/README.md)、[模块细节](docs/detail/00_INDEX.md)，最后核对实际代码与消费者。改文档也须读上层和同主题正本；脚本定位不能代替阅读全文。

Normalize 先读[科学链](docs/science/normalize/NORMALIZE_SCIENCE.md)、[生产架构](docs/engineering/architecture/NORMALIZE_PIPELINE.md)、[数据合同](docs/engineering/contracts/NORMALIZE_DATA.md)和[接口规格](docs/detail/normalize/contracts.md)。

最高设计定义目标；science 定义估计量、公式、单位和适用域；engineering 定义架构、数据与工程行为；detail 连接接口与实现。同一主题只有一份正本。索引、机器 schema、代码和实验记录不能反向改写科学定义。

## 当前工作与 Git

直接在 `main` 整理和重写文档，按主题提交，每次提交后立即推送到远端 `main`；远端有新提交时先保留并整合，推送失败如实记录并重试，不强推。科学与数学疑点集中写入[待推导问题](实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，记录原文、分歧、影响与所需证据，本轮不据疑点切换模型或算法。任务范围以用户当前指令为准，实验内的任务包不自动恢复执行效力。

保留他人的修改和未跟踪文件，并发修改同一文件先协调。不 force-push，不创建未经请求的 PR。发布由项目负责人决定；文档完成不代表科学产品已验收。

## 科学责任与证据

前台负责统一模型与正式科学、工程定义；执行者依明确规格实施。发现科学矛盾时提供原文、一手依据或可复核反例，统一订正正本后再改实现。派单须点名先读的上层、专题与 detail 文件；回报列实际阅读、关键口径、改动与验证范围。

类型声明、生产可达、编译成功、真实运行与独立科学验证分别报告。原始 FITS 缺 WCS 头只是输入属性，IPV 的成功与失败以实际求解为据；solver 候选与求解后科学源表职责不同。合法信号缺精度模型时保留能力状态，未知误差不得填零。

实验从运行清单枚举帧集，从产品取几何，核对运行身份与天区。缺几何逐帧登记，天区不符则分析无效。命令、输入身份、统计分母、失败尝试、脚本和轻量结果随做随存于所属单元，不覆盖过程记录；真实观测 FITS 不上传 Git。

## 工程纪律

模块通过声明的输入输出交换数据，Runtime 管理对象与大数组寿命。模块不读全局配置、不私建线程池、不直接退出、不绕过统一 I/O。共享算法只保留一份实现，通过公开 API 复用。接口说明单位、坐标、像元原点、所有权、错误、并发及下游影响。

诊断开关只有 `ACSD_DEV_MODE`，遵循[开发模式](docs/engineering/architecture/DEV_MODE.md)与[探针规范](docs/engineering/standards/DIAGNOSTIC_PROBES.md)。探针只观察科学对象；正式 cache/spill 不由开发开关控制。Mosaic 最终只发布 signal HiPS 科学通道。

测试依据正式定义，用独立真值和正负例暴露缺陷。项目不设研发审批门；损坏输入、非法几何、不可计算数值与写盘失败仍应明确报错。

## 文档维护

正式正文正向解释现行设计，避免任务流水、沿革和重复定义。科学文档保留符号、假设、必要推导、数值方法、误差与引用；工程文档说明组件、数据流、生命周期和接口；模块文档给真实入口与调用。历史读数与失败过程归实验记录。

移动或归并文档时保留有效内容，修订引用与[索引](docs/DOCUMENT_INDEX.yaml)。写法、协作与追溯统一见[文档维护规范](docs/engineering/governance/DOCUMENT_GOVERNANCE.md)。
