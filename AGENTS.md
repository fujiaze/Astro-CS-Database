# ACSD V2｜Agent 科学功能无损迁移协作规范

**先读** [架构入口](docs/engineering/rebuild/README.md) → **[main源码迁移总入口](docs/engineering/rebuild/MIGRATION.md)** → [旧真实调用/文档资产](docs/engineering/rebuild/migration/00-BASELINE-ASSETS.md) → [施工任务](docs/engineering/rebuild/migration/08-EXECUTION.md)。按本次模块阅读对应[01–05源码施工](docs/engineering/rebuild/MIGRATION.md)、[06性能合同](docs/engineering/rebuild/migration/06-PERFORMANCE.md)、[07独立实验](docs/engineering/rebuild/migration/07-VERIFICATION.md)，再读对应 Process、五篇 Foundation、SYSTEM/OBJECTS/API/EXECUTION。**先查main真正的源码/生产消费者，再写V2代码。**

## 1. 工程目标与不可让步的功能

V2是main已有科学处理系统的**模块化/有界资源/强类型重构**，不是“从零实现22个算子”的新产品。保留旧正常算法核及已经优化的CPU/IO路径，修正已被数学或真实数据证伪的旧错误；只保留一份V2科学算子、一份typed公开接口、一套Pool/Task/Pipe/Storage、一个正式Normalize→Mosaic→Export工作流。不兼容旧ABI/旧临时文件和旧内部产品可以，但**不能因此移除旧自动NSIDE、真实高分辨率Drizzle、Gaia/IPV/PSF/光度、多面板Sky/Mosaic/排异/Export或主分支的缓存/分块/线程优化**。

以`main@c5fee09fedb97ccbe39cb47ea0985e0d8a339db2`为冻结静态基线，当前V2按最新远端HEAD；在旧main只读条件下核可达算法与历史实验，不把旧测试文件中显式SKIP、旧函数存在或未在本机复跑的历史结果说成实时PASS。已有V2 order13三帧数值闭包也不是用户最终图像质量的PASS。

## 2. 每个迁移包的工作流程（不因发现Bug停工）

1. **调查main**：旧CLI→module_adapters→被调用科学kernel、输入/输出/单位/线程/缓存、已知负例、实际生产与test-only分开；在任务清单中记录旧源码及可复用函数。
2. **科学正确性**：对照Foundation/Process和独立论文/解析/真实输入；旧算法错误保留独立负例，新实现可保留更严谨数学，迁旧kernel不等于恢复旧错误。
3. **最小迁移**：改源码接V2已有公开操作，移走旧JSON/I/O、全局临时状态/自建线程池，科学对象用Ref/Storage/Pipe及同一资源租约；既有V2核心若正确则补不足，不另写一套。
4. **最小证伪→真实数据**：解析与已有main反例→小M42ROI→单帧4096²高阶→两个不同面板→全六面板/196帧。对跨源码改动只重算受影响依赖、复用有效WCS/校准/Normalize缓存。
5. **科学与性能复审**：独立数学/QA reviewer核输入/estimand/真实FITS、星点MTF/PSF/天光与多面板接缝；独立性能审查同科学策略下main↔V2的wall/CPU/RSS/IO/cache、资源/线程与故障注入。失败先定位producer、修、定向回测，继续执行，不只留下后续建议。
6. **一次性报告**：主分支旧算法资产、实际V2代码diff、实跑范围、收益与代价/失败、未解决科学项、可重开产品及Reviewer的限定判决。没有实际源码/数据验证就写 NOT_RUN 而不是PASS。

## 3. 性能与资源纪律

main 的性能优化必须继承**语义**：Drizzle每线程geometry LRU+有序stripe归约+touched-only Tile；P2动态micro-chunk、稀疏Sky Schur/IRLS和控点缓存；Export跨worker共享bounded LRU/negative cache/流式子块写入；旧嵌套OpenMP已修过的CPU budget问题不能复活。V2一套Host预算、12GiB进程上限和有界IO/pipe；不得对每帧保留一套共享Gaia索引/大tile缓存，不得生成全球空HiPS tiles，不得以降order、减曝光、隐藏Sky/Rejection或修改阈值冒充高性能。性能档位和工程回归警戒线见[06](docs/engineering/rebuild/migration/06-PERFORMANCE.md)。

原始`testdata/`/`gaia/`只读、旧实验及历史证据不删。产品缓存按input/recipe/source/科学依赖哈希重用；禁止反复全量SHA196帧、重复原始FITS复制、多次全量跑而只改Writer。失败仅安全清理由本任务独占且无引用的staging，成功final/他人数据不动。无资源能力时具名BLOCKED并保留可复用成果，不谎报已处理全量。

## 4. 文档与Git权威

数学只在五份Foundation；各22 Process只负责自身科学操作及错误/资源；SYSTEM/OBJECTS/API/EXECUTION分别唯一负责骨架、对象、协议和执行；WORKFLOWS只负责三命令的科学数据边与阶段屏障；迁移目录只负责main旧函数复用、性能和施工/测试任务，不复制科学推导。公开字段唯一以`include/acsd/`为准。

只改`rebuild/acsd-v2`，保留他人修改，先核远端HEAD、追加提交/推送；不动main、不强推、不做PR。允许有边界子代理并行查main/数学/性能、实施文件明确所有权，最终合并/验收必须固定同一个实际SHA。独立审核者不得验收自己开发的科学算法。

**用户既有裁决**：不以CI、自动文档脚本/覆盖率评分或自动PASS门禁替代科研证明。可用编译器、探针、有限实验和性能profile生成证据；真实科学公式由独立推导/反例/审查判定。Issue #126 实施独立功能回归，真正PASS后解锁 #125 完整M42 196帧四通道，用户最后只检查正式FITS的视觉接缝；代理自己承担SNR/排异/校准/天光与性能。
