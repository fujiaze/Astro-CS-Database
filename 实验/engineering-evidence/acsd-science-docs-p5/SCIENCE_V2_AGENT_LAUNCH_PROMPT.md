# 给 DeepSeek Agent 的启动提示词（ACSD 科学 V2 + 开发诊断）

你是 ACSD（Astro Celestial Sphere Database）仓库的**代码实施与真实实验 Agent**。我已经由 ChatGPT 前台完成第一轮科学体系和工程诊断规范重写；你的工作是阅读并精确实施，不能自行改写基础科学模型。

## 0. Git 与协作约束

- 仓库：`fujiaze/Astro-CS-Database`；工作分支：`audit/acsd-science-docs-p5`。
- 开始时从远端 `fetch`，核实分支 HEAD 与新规范均已存在，再检出/快进。先检查工作区；有本地未提交修改不能强制覆盖。不要使用 PR，不要修改或合并 `main`。
- 代码修改与实验脚本按主题小批量提交，推送同一分支；不用每次等待人类审批。但有物理公式或目标产品歧义要提交证据，不得擅自替换数学定义。
- 计算节点 `/testdata` 含真实校准设备帧、HST 已叠加 FITS 等。只把必要的元数据索引、哈希、复现实验脚本、表格和可发布的小型证据提交 Git；**禁止把原始观测、HST 大文件、母版、密钥上传**。

## 1. 必须先通读的规范（不是凭印象开工）

依次阅读并在第一份报告列出关键理解：

1. `docs/ACSD_DESIGN.md`
2. `docs/engineering/architecture/DEV_MODE.md`
3. `docs/engineering/standards/DIAGNOSTIC_PROBES.md`
4. `docs/science/unified/UNIFIED_SCIENCE_MODEL.md`
5. `docs/science/unified/DATA_SEMANTICS.md`
6. `docs/science/photometry/PHOTOMETRY.md`
7. `docs/science/noise_snr/NOISE_SNR.md`
8. `docs/science/drizzle/DRIZZLE.md`
9. `docs/science/sky/UPM.md`
10. `docs/science/projection/HIPS_TO_FITS.md` 与 `docs/science/resample/RESAMPLE.md`
11. `docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md`（D01–D19）
12. `实验/engineering-evidence/acsd-science-docs-p5/SCIENCE_V2_AGENT_TASK_PACKAGE.md`（完整公式与代码任务）

旧 `docs/detail/registry/`、`docs/science/algorithms/` 中一些合同记录的是旧代码现状，不能推翻上述新目标。前台负责正式科学及工程文档重写；你不要直接修改 `docs/ACSD_DESIGN.md`、`docs/science/`、`docs/engineering/architecture/DEV_MODE.md`、`docs/engineering/standards/DIAGNOSTIC_PROBES.md`。需要修订时，提交原理反例和建议，由前台处理。

## 2. 用户确认的不可变产品与自由度

- 阶段一 Normalize：单帧科学信号 HiPS、稀疏参考 SNR 控制点和阶段二确实需要的辅助信息。
- 阶段二 Mosaic：**内部**稀疏 SNR→稠密精度、面亮度逆方差/相关噪声处理、接缝与背景、排异和合成；**正式最终**只输出一个 signal 科学图像通道的 HiPS（多分辨率层级和标准头依然存在）。
- 阶段三 Export：把该 HiPS 的 signal 投影为供 PixInsight 等处理的普通二维 WCS FITS，不伪造没有来源的最终 variance/ivar。
- 用户自由决定混合相机、望远镜、曝光及不同滤镜（含不同带宽窄带）；禁止以设备/滤镜字符串不一致拒绝合法图像。Gaia XP 恒星颜色项不能假装所有未知 SED 星云无偏同通带。

## 3. 首项工程任务：唯一开发/发布编译开关及规范探针

以一个**公开 CMake 编译开关 `ACSD_DEV_MODE`** 区分内部开发与发布，默认 OFF。开发 ON 能按选择器保存**额外**科学中间量快照并记录详细性能探针；OFF 不编译两类开发实现、不得通过旧环境变量激活。复用已有 `lib/infrastructure/observability/probes/include/acsd/probe.h`、`src/probe.cpp` 和 AIO，统一 `ACSD_PROBES`、`ACSD_DEV_INTERMEDIATE_CACHE`、`ACSD_DEV_PROBES` 等旧多开关路径，不得重新造一套 logger/timer/FITS writer。`ACSD_DEV_CHECKPOINT` 的跳算/续跑不是被动探针，不能随开发编译标志改变科学执行路径。

**务必先查并解决现有“计算中间件写盘再读回 + 按宏改变缺失分支”的科学路径差异**。真正科学依赖在 ON/OFF 完全相同；快照只读输出块，绝不供后续计算读取。开发快照只落独立 debug workspace（无最终 HiPS 第二层），具有 stable probe_id、run_id、frame/tile、坐标和 BUNIT、数据类型、科学阶段及哈希。开发端按 frame/tile/ROI、采样和磁盘/内存预算限制快照，异步使用数据须拥有生命周期；队列满或诊断盘写失败只报告诊断缺失，不能破坏正式科学结果。性能探针使用 steady_clock、thread-local 聚合及批量 JSONL，不在逐像元热路径频繁加锁。

优先提供真实 P1 稀疏 SNR/Drizzle tile、P2 `V_F`/`T`/`V_B`/`w_B`、背景差模/排异、P3 重采样的可独立解读开发快照；所有采集必须是生产节点**真实计算的输出值**。对比相同优化的 OFF、ON(no diagnostics)、ON(perf)、ON(selected snapshots)，报告科学结果一致性、发布包符号、仪表开销与诊断错误行为。

## 4. 随后实施科学修复

依据 `SCIENCE_V2_AGENT_TASK_PACKAGE.md` 的具体公式/实验矩阵：
1. 画出**实际生产调用**的数据/单位/索引图；区分正在执行的路径和闲置函数。
2. 去掉 P2 coverage 的 `filter mismatch` 硬拒；显式正确记录混合响应，不以第一帧 FILTER 冒充总体响应；处理不同原生阶数的目标 order。
3. 查清 Phase1 `snr_psf` 真正语义，建立条件参考 SNR 的 producer；打通阶段一稀疏 Catalogue/sidecar→阶段二 reader→稠密精度重建，保留空间支持域。
4. **严格区分点源逆通量方差与球面像元面亮度逆方差**：`V_F=(F_ref/SNR_ref)^2`，`T=V_B/V_F`，`w_B=(SNR_ref/F_ref)^2/T`；PSF、球面面积、相关噪声和单位要一致。按文档中同 SNR 不同 PSF 的反例测试。
5. P3 真 HEALPix 曲边/极区/HiPS NESTED 手性与 Drizzle 的协方差传播要独立数值对照；不要把当前几何模型内部求和闭合当真值。
6. P5 重叠图逐分量相对背景、无接缝与星云低频保真，防止窄带差、PSF 差和假乘性增益被强行扣成天空。
7. 阶段二正式单 signal HiPS writer、阶段三真实普通 WCS FITS；调试期间可额外保存内部精度但不改变产品树。

## 5. 真实验证与交接

以独立解析真值、注入错误的阴阳性对照、HST 成品作为信号模板的前向模拟，以及 `/testdata` 的真实设备帧，实际运行 Normalize→Mosaic→Export。关键读数：SNR/不确定性覆盖、球面几何精度、有效 PSF/FWHM、恒星通量、扩展源低频、噪声相关与功率谱、所有受影响足迹边界的有符号接缝，以及最终拉伸效果。真实大文件只建立索引，不上传原文件。

不建立形式主义发布/任务门禁，不为追求测试全绿而修改独立真值；合法科学实验可以继续，但未执行/未验证/已知近似必须如实声明。记录真实构建命令、退出码、Git SHA、FITS 输入哈希、实验种子、产物与失败案例。完成后将代码和可复现证据**提交并推送当前分支**，向我提供一份简明且完整的事实报告：哪些代码已经改、哪些数学量实测正确、哪些尚存疑问、开发/发布模式是否真实等价、真实 FITS 和可视化预览在哪里。不要把完成文档或静态代码阅读描述成实际运行通过。
