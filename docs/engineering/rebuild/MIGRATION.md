# main → V2 无损科学能力迁移：唯一执行入口

**基线与状态**：本轮是文档/源码静态审查与迁移任务编排，非运行验收。main 冻结 `c5fee09fedb97ccbe39cb47ea0985e0d8a339db2`，V2 规划基线 `1e2f8f8d29ec88c2703a49adea90ecfab801eb14`；执行前按最新 `rebuild/acsd-v2` HEAD 重新定位、检查实际差异。只改 V2；main 保持只读历史基线。

## 0. 重构目标与不变边界

**目标不是替换旧科学系统，而是让 main 已生产可达的能力在 V2 单一标准工程中继续成立，并修正已证伪问题。** 保留 Normalize→Mosaic→Export 三命令、真实 Gaia/WCS/PSF/测光、动态 NSIDE、原生像元→球面 Drizzle、全区域多面板 Mosaic、Sky/排异/权重、HiPS 与平面 FITS、Windows/Linux、诊断与界面合同。原 `main` 已有 49 帧 M42 三命令历史实验；不能把 V2 的三帧固定 order13 数值闭包称为原功能等价。

**不要求兼容旧 ABI、JSON、临时文件目录/内部对象布局**，但不等于拒绝复用旧 C++ 算法或运行优化。V2 只允许一套 Host/ProcessRegistry/Task/Pipe/BlockStore/Storage、统一类型与单位、一条正式科学计算路径。旧算法核按正确性与性能经审查后 **优先提取和适配**；新严格几何/协方差/共享根优于旧科学近似时，保留新实现并继承旧有的候选搜索与性能方法。禁止把旧 `module_adapters.cpp` 的 18k 行胶水整体搬回来。

## 1. 文档权威与最短阅读路径

| 需求 | 唯一权威 | 其它材料身份 |
|---|---|---|
| 科学估计量、前向模型、几何与误差 | `docs/science/foundation/` 五篇 | main `docs/science/` 与历史论文是对照/待合并依据；矛盾须推导裁决 |
| V2 工程骨架、typed模型与持久化 | `docs/engineering/rebuild/{SYSTEM,API,OBJECTS,EXECUTION}.md`、实际 `include/acsd/` | main runtime 只作为优化与行为证据，旧接口不兼容 |
| 22 个 Process 算法与验收前置 | `docs/engineering/rebuild/processes/<id>.md` | 迁移施工单只指定源函数、适配方法、性能与测法，不复写数学证明 |
| 三命令调度数据边与缓存失效 | `standards/WORKFLOWS.md` | 不在科学Process内定义另一条CLI工作流 |
| 旧能力、旧文档状态与已知坑 | [00-BASELINE-ASSETS](migration/00-BASELINE-ASSETS.md) | 历史证据永久留Git，不批量复制过期文档 |
| 精确施工任务 | [01-NORMALIZE](migration/01-NORMALIZE.md)；[02-GEOMETRY](migration/02-GEOMETRY.md)；[03-MOSAIC](migration/03-MOSAIC.md)；[04-EXPORT](migration/04-EXPORT.md)；[05-RUNTIME](migration/05-RUNTIME.md) | 按模块任务编号归属、可交子代理；禁止另造同主题平行任务书 |
| 性能基线/硬约束与回归 | [06-PERFORMANCE](migration/06-PERFORMANCE.md) | 历史速度不是当前机器实测值 |
| 独立实验证据与门槛 | [07-VERIFICATION](migration/07-VERIFICATION.md) | 单次 exit0/哈希/内部恒等式不等于质量PASS |
| 任务依赖、负责人、交付及循环 | [08-EXECUTION](migration/08-EXECUTION.md) | GitHub #126 是首轮实施入口，#125 是全196帧后续任务 |

重构代码修改必须先用 00 定位生产旧路径，再按所属 01–05 任务施工；所有迁移都交 06 性能和 07 科学对照；批次组织与 FAIL 自主回修按 08。对重复文段直接加链接，不复制同一验收标准到22份Process。

## 2. 按能力的迁移原则

- **COPY/ADAPT**：旧核算法可被证明正确且实际用于生产时，移动源码到 V2 模块，剥离 C ABI/JSON/全局路径，把核心算子对接现有 `Ref/ArrayRef/Context`。优先复用算法而不是重新写数学。
- **REVISE**：旧核含已知科学偏差（旧曲边四角近似、参考Fisher权当场景variance、旧Sky不可辨识、低N排异、通带/QE错配），用 V2 科学正本修正；旧代码作速度/行为参照，保留不会改变正确性的优化。
- **REPLACE INFRA**：旧 `module_adapters.cpp`、多套线程池、跨节点 `p1_*.json`、重复 tile reader 与旧 AIO 路径不复制。只迁外部CLI合同、调度行为、缓存策略和经过证明的核心实现。
- **ARCHIVE**：旧 `phase*_session` 兼容外壳、归档 `healpix_stack/`、退役 UPM 内核、占位 `module.yaml` 等仅作历史，不误连生产。
- **NOT CERTIFIED**：旧源码存在、静态连接、旧测试文件存在、历史产物记录分别是不同证据等级；同输入独立运行前不能把任何候选写成PASS。

## 3. 用户可见质量底线

- 动态 leaf order 由 **实际最终 WCS/SIP 和原始角尺度**推导；统一全局 order 再 Normalize。约0.967″输入不能用约25.8″的 order13 产品导出0.967″ FITS后宣称保真。tile order 与 leaf order 不混淆。
- 跨面板是真的不同天空足迹 `M1…M6` 的 sky-overlap + Coadd，不能拿一块 M5 内三张曝光当 mosaic 科学验收。
- 背景与乘性平场为不同估计量，强星云低频不得强行 flatten；有明显接缝/菱形星点/假纹理时**不得用 science=conditional 直接标成最终成品**。
- M42 六面板×T2/T3×Hα/R/G/B：196 候选（Hα 50、R49、G49、B48）逐帧去向和最终共同WCS四通道 FITS 是 #125 真正交付。由于原始校准/响应未知，拒绝有根据的坏帧是允许的；不能把不兼容响应无条件混合或只处理三帧替代。
- 有界资源/性能：科学精度相等时保留 main 的 tile/几何缓存、分块、并行优化和I/O复用；需要优化V2，不许通过粗 order、减少参与曝光、关闭Sky/排异或降低验证精度换速度。

## 4. 两个独立完成层次

**迁移包 PASS**：正式V2同一生产入口含迁入算法，旧main+V2同输入/同估计量对拍，独立 oracle、新旧正常例/反例、12GiB资源约束和性能误差预算有可复核证据。旧 main 错误应有负例差异，不能要求错误逐位一致。

**完整成品 READY_FOR_OWNER_REVIEW**：高阶分辨率、至少两个不同面板、再完整196帧四通道/六面板，空间接缝/扩展结构与点源保真、N_eff/可辨识SNR/排异统计、外部FITS读回和性能均已由Agent独立确认，最终才交用户视觉验收。内部产品闭包PASS不能替代成品PASS。

规范不能替代代码：所有条目最初状态为 **DOCUMENTED_NOT_IMPLEMENTED/UNVERIFIED**，只有目标分支上相应函数/测试/实际结果才能逐条升级，历史S1 PASS范围仍保留。
