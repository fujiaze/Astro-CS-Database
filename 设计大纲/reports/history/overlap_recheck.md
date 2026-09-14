# 历史取证重叠区核对（overlap_recheck）

> 本文件为两位重叠区核对员分区间接力产物：**上篇**（本版新增）覆盖分片对 S02<-S01 … S13<-S12（重叠 seq 46–660，共 120 条）；**下篇**（既有内容，原样保留）覆盖分片对 S14<-S13 … S25<-S24（重叠 seq 706–1320）。两篇合并覆盖 S02–S25 全部 24 对；S26–S37 各区间的核对结论仍以各自分片报告为准。

## 上篇 · 分片对 S02<-S01 … S13<-S12

- 核对人：重叠区核对员（事后补核）；在全部 37 份分片报告落盘后执行。
- 任务：对分片对 S02<-S01 至 S13<-S12 共 12 对，逐条核对后片 manifest（`设计大纲/_evidence/commits/slices/Sxx/manifest.csv`，角色=重叠复核 的前 10 行）与两篇报告中对同一提交"做了什么/落点/内容性质"的判断。
- 判定口径：**一致** = 双方事实与定性无冲突（后片为"重叠待核"时，以其登记事实与前片主责正文逐项相符为限）；**口径差-可兼容** = 事实相同、分类/命名表述不同且可并存；**不一致** = 对同一可核对事实双方给出不同值。
- 处理规则：后片重叠区若已在分片时逐条判"一致"（本次涉及 S07、S11 两片），不再重复比对，登记"已完成核对"。

### 总计数

| 指标 | 数值 |
|---|---|
| 处理的分片对 | 12 对（S02<-S01 … S13<-S12） |
| 覆盖 seq 总数 | 120 条（12 对 × 10 条） |
| 其中"已完成核对"（分片时已逐条判一致，登记不重做） | 20 条（S07<-S06 十条、S11<-S10 十条，两片原判全为"一致"） |
| 本次逐条核对 seq 数 | **100 条** |
| 一致 | **97 条** |
| 口径差-可兼容 | **2 条**（seq 49、seq 660） |
| 不一致 | **1 条**（seq 161，tools/_* 文件计数 16 vs 19） |

### S02<-S01（重叠区 seq 46–55；S02 原判全部"重叠待核"，本次核）

| seq | sha8 | S01（主责）一句话 | S02（重叠区独立观察）一句话 | 判定 |
|---|---|---|---|---|
| 46 | 2767c66b | photometric_calib 残余 Python 4 文件纯删除 -1470（flux_calibrator/python/ 迁移后残留清理） | 删 lib/photometric_calib 4 个残留 Python 文件 -1470 行，消息自述 import 断裂/被 C++ 替代而归档，落点为归档 | 一致 |
| 47 | 312dd8ad | dynamic_psf 性能回归修复（自述 9.26s→0.26s）：LOG_INFO→LOG_WARN、去双 fflush、Makefile 回补 -fopenmp，2 文件 +12/-8，生产代码 | 同一修复三处改动逐一同名同值（自述 9.26s→0.26s），落点文件一致 | 一致 |
| 48 | 9d6b200a | astro_image_io .ahpx 头 JSON 读取改动态缓冲，3 文件 +40/-13，缺陷修复 | 同一动态缓冲改造（另附 16MB 上限、容量回报、Python 侧 65536→2x 扩容循环等卡载细节），无冲突 | 一致 |
| 49 | 9de3fa88 | photometric_calib README+memory 双修改刷新至 v2.0 C++ DLL 版（+221/-145）；49–55 以"文档定稿批组"统一定性"文档或规格+控制包或台账（模块级 memory.md 集中建档/刷新）" | 同文件同行数（+221/-145）同 v2.0 口径，但本条单独判"纯文档" | **口径差-可兼容**（事实全同；差异仅分组定性 vs 单条定性，见备注 R-1） |
| 50 | 9e366aed | dynamic_psf 新增模块 memory.md + README 记录性能修复（+39/-2） | 同 | 一致 |
| 51 | 675d351d | astro_image_io 新增 memory.md + README 补 GitHub 仓库信息（+39/-0） | 同（并注"seq 57 的父提交即此"，与 S01 谱系无冲突） | 一致 |
| 52 | 5c2a5e7f | calibration README/memory 更新 C++ DLL 切换状态（+49/-4） | 同 | 一致 |
| 53 | 39498757 | gaia_xpsd_client 新增 memory.md + README 补仓库信息（+35/-0） | 同 | 一致 |
| 54 | 65a52de3 | data_pipeline 新增 memory.md + README 用法示例（+123/-25） | 同 | 一致 |
| 55 | 75c6ab30 | healpix_db 新增 memory.md + README 补仓库信息（+41/-0） | 同 | 一致 |

### S03<-S02（重叠区 seq 101–110；S03 原判全部"重叠待核"，本次核）

| seq | sha8 | S02（主责）一句话 | S03（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 101 | b684ab25 | merge astro_image_io/main：终态 58 文件 +15883 导入 lib/astro_image_io/，结构整理（仓库合并），L | 同（58 文件 +15883/-0 全新增，L） | 一致 |
| 102 | 58daf22c | merge calibration/main：31 文件 +6831 导入 lib/calibration/，L | 同 | 一致 |
| 103 | b92fefe3 | merge data_pipeline/main：8 文件 +2627 导入 lib/data_pipeline/，L | 同 | 一致 |
| 104 | 2325bae9 | merge dynamic_psf/main：12 文件 +1617 导入 lib/dynamic_psf/，M | 同（分级 M 亦同） | 一致 |
| 105 | f727ab83 | merge gaia_xpsd_client/main：12 文件 +3711 导入，L | 同 | 一致 |
| 106 | 8c03d08e | merge healpix_db/main：35 文件 +7214 导入，L | 同 | 一致 |
| 107 | 6a2ce3d3 | merge orchestrator/main：37 文件 +12661 导入，L | 同 | 一致 |
| 108 | 3746a61f | merge photometric_calib/main：32 文件 +26956 导入，L | 同 | 一致 |
| 109 | dabf35e1 | merge plate_solve/main：55 文件 +19183 导入，L | 同 | 一致 |
| 110 | 98ec469c | merge snr_estimator/main：8 文件 +1113 导入，M；关联栏称"至此 10 个模块仓库全部并入" | 同（8 文件 +1113，M）；共同形态栏称导入序列至 seq 111（star_detector，本片主责）止、共 11 仓 | 一致（条目事实全同；"10 全部并入"系 S02 视野内界定语，S03 已在本片主责 111 补全第 11 环，非条目级冲突，见备注 R-2） |

### S04<-S03（重叠区 seq 156–165；S04 原判全部"重叠待核"，本次核）

| seq | sha8 | S03（主责）一句话 | S04（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 156 | c3865eb6 | P07-002 长批次与故障稳定性两拍之首：15 文件，evidence 10 文件 +2078、台账三件、tools/_p07_002_setup.json/.log，13/13 PASS，未改业务源码，L | 同（P07-002 VERDICT: PASS；evidence+control；另含留痕 2 文件），L | 一致 |
| 157 | 29cb2912 | P07-002 补证：44 文件 +26557 纯新增 logs/36+HISS 输出 7（-f 强添加），L | 同（44 文件 +26557 纯新增），L | 一致 |
| 158 | 889944ad | P08-001 CLI Core v1 发布包：dist/ 文本 6 文件入库+.gitignore 例外+evidence 5 文件+台账；卡面噪声区 6 项，M | 同（evidence+control+.gitignore；含噪声区文件 6 个），M | 一致 |
| 159 | dc774c6d | P08-002 v1.1 开发包最终独立复核与交接两拍之首：14 文件（evidence 10+台账三件+memory.md +56），M | 同（P08-002 PASS；memory.md +56），M | 一致 |
| 160 | a3b468dd | -f 强添加 4 份被 .gitignore 忽略的 logs 入 evidence/P08-002/logs/（4 文件 +334），M | 同（logs 证据 4 文件，git add -f 绕过），M | 一致 |
| 161 | ed145a70 | 81 文件 +15944/-0：根 README（+182）+ audit/AstroCS-v1.1-audit-pack 48 文件（bundle、AUDIT_MANIFEST.json）+ **tools/ 19 个运行中间件与 gen_audit_pack.py** + evidence 11 + engineering/engineering 1，L | 同提交 README/audit-pack 描述一致，但称"等 **16 个** tools/_* 运行留痕"，L | **不一致**（tools/_* 计数 16 vs 19，见"不一致条目"一节，以 S03 为准） |
| 162 | 0d1857de | P09-001 冻结 v1.1 事实基线+engineering_v1.2 部署：146 文件（tasks 50/docs 23/evidence 13 等），L | 同（146 文件，tasks 50/docs 23/evidence 13 等），L | 一致 |
| 163 | 06df865b | P09-002 统一共享检测命名 INTERNAL_DETECTION_SHARED_EXPORT：lib/plate_solve+lib/orchestrator 共 5 代码文件+evidence/control，仅命名/注释级，M | 同（5 代码文件，两模块），M | 一致 |
| 164 | 73a33b79 | P09-003 冻结测光失败帧与浏览器性能基线：v1.2 evidence 18 文件 +3346 纯新增+control 三件，无生产代码，L | 同（evidence 18 文件纯新增），L | 一致 |
| 165 | bb6963b4 | P10-001 读取全部 TestData 说明文档并建数据台账：evidence 14 文件 +3145+control 三件，L | 同（evidence 14 + control 3），L | 一致 |

### S05<-S04（重叠区 seq 211–220；S05 只登记消息首行+分级+日期，本次逐条对 S04 主责正文核）

| seq | sha8 | S04（主责）一句话 | S05（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 211 | 51d89849 | C-001 HISS v2 契约冻结（17 章、HI2S/version=2/CHUNK_SIZE=4096）+H-001~003 资源监测链完成+3 头文件骨架，L | "HISS v2契约冻结 + H链资源监测完成"，L | 一致 |
| 212 | 38b05b0e | C-002 HISS v2 读写器 hiss_v2.py（1006 行）实现完成、37/37 PASS、3 帧 V1→V2，M | "HISS v2读写器实现完成, 37/37测试PASS"，M | 一致 |
| 213 | af38cf1e | Gate C 收口：C-003 inspector 26/26、C-004 可视化、验收 8/8、进入 Gate D，L | "Gate C 全部通过 + 进入 Gate D"，L | 一致 |
| 214 | 5518d0bc | Gate D 收口+D-001 银心三片球面重合 6/6、F-001 由 D-001 覆盖、进入 Gate E，M | "Gate D全部通过 + F-001完成 + 进入Gate E"，M | 一致 |
| 215 | f9d0a14c | Gate E 收口：E-001~004 全局加性共识曲面链 4/4、验收 8/8、进入 Gate G，L | "Gate E全部通过 + 进入Gate G"，L | 一致 |
| 216 | aa210b93 | G-001~003 叠加链+F-002 三片集成 4/4+H-004 补验 DONE（H 链闭环），L | "叠加链完成 + H链全DONE"，L | 一致 |
| 217 | 5a41e285 | I-001 冻结格式/CLI/算法契约：CLI_CONTRACT+ALGORITHM_CONTRACT+GATE_G 改 30 帧口径+full_regression_allowed，M | "冻结格式、CLI和算法契约"，M | 一致 |
| 218 | d91acf10 | p13-001 中文路径缺陷正式修复：配置迁 ASCII 路径 lib/orchestrator/configs，T2 单帧验证 PASS，S | "修复 stage1_batch_runner 中文路径导致 orchestrator C++ 文件系统错误"，S | 一致 |
| 219 | caa6cb9d | I-002 阶段 A 收口（15 帧，1 FAIL 已知 DRIZZLE 栈溢出）+710 帧全量回归启动，S | "阶段A 15帧代表帧验证通过 + 启动710帧全量回归"，S | 一致 |
| 220 | cb1b20e0 | p13-001 EVIDENCE_DIR 支持环境变量覆盖+T2/T3/T4 三设备并行，S | "支持环境变量覆盖输出目录, 启用3设备并行回归"，S | 一致 |

### S06<-S05（重叠区 seq 266–275；S06 原判全部"重叠待核"，本次核）

| seq | sha8 | S05（主责）一句话 | S06（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 266 | 821a1856 | R05 Gate E FAST 对照实验 288 cases：benchmark_precise_fast.cpp 补 fast 链接，自述 12.37x 并发现极区异常，附 tools/_r05_fast_experiment_commit_msg.txt，实验 | 同（288 cases；两落点同名），experiment | 一致 |
| 267 | 4b6cb05f | R06 PRECISE 几何修复七项 B01/B02/B03/B04/B05/B11/B16（Eriksson 公式、边界细分、SVD Jacobian 等），三文件 | 同（七项代号名单与落点三文件逐项相同） | 一致 |
| 268 | 0f6239c4 | R06 Step7 六项跨模块修复 B13/B14/B18/B19/B20/B22：14 文件 +211/-77 跨五模块 | 同（六项代号、五模块、14 文件均相同） | 一致 |
| 269 | fb2dee21 | R06 Step8：4 个诊断程序（diag_candidate_missing/diag_overlap_detail/diag_overlap/diag_boundary_precision）作为修复证据入库 +900 | 同（4 个 diag 名单相同，测试证据定性相同） | 一致 |
| 270 | e841eb88 | R06 按正确 NSIDE 重做 FAST 对照实验（作废 266 的 12.37x）：benchmark_precise_fast.cpp+tools/_r06_analyze_fast.py | 同（重做实验、两落点文件相同） | 一致 |
| 271 | 32d6bcce | R06 交付工具入库：_r06_make_delivery.py+_r06_wiki_hashes.py（_r06_analyze_fast.py 与 270 有一文件归属交错，S05 已自注） | 同（两脚本点名相同，交付工具定性一致） | 一致 |
| 272 | f8d749e4 | ACR Phase A：lib/acr 17 文件（ADR-001..009、dependency-lock.json、path_guard.ps1、骨架）+工程控制/tasks 3 文档，L | 同（17 文件；另注 3 份 tasks 即本片 seq 293 删除的旧包——S06 区内补充，无冲突） | 一致 |
| 273 | 35a3843b | ACR Phase B 公共 API 全套+CPU baseline runtime（oneTBB 封装、33 测试、legacy_chunk_adapter），16 文件 | 同（16 文件、含同名两文件） | 一致 |
| 274 | 4bba43d0 | R07 PRECISE 通量闭合修复：三角形扇剖分替代直接 S-H 裁剪+2 个 diag，flux_err≤4.44e-16 | 同（扇剖分方案、2 diag；另注其归并消息标"不进 main 需 cherry-pick"——属 S06 区内处置信息） | 一致 |
| 275 | cdefda05 | R07-M09/M10/M11 自动 NSIDE 修复（3D 切向量 Jacobian、9 点保守采样、32/32 测试） | 同（三项编号、主题一致；S06 注其被 0299 标 SUPERSEDED——属其区内信息） | 一致 |

### S07<-S06（重叠区 seq 321–330）——**已完成核对**

S07 报告第一节在分片时即对照当时已存在的 H-S06.md 逐条核对（基线条目 G40..G49），10 条全判"一致"，另闭环两处跨片缺口（F-fix 10 落地于 seq 331/332/333；RealMixed 自述口径不变）。本次仅登记，不重做：

| seq | sha8 | S07 原判 | seq | sha8 | S07 原判 |
|---|---|---|---|---|---|
| 321 | 8c616f1c | 一致（nlohmann/json 替换手写解析，1 文件 +80/-129） | 326 | 45022dcf | 一致（F-fix 9 RecoverableGate+MemoryBudget，5 文件 +408/-51） |
| 322 | 8653bd54 | 一致（Python 生产层清理 89 文件 +100/-16079，多目录删除式整理认定相同） | 327 | bd036f3e | 一致（FP32/FP64 双精度 ABI，34 文件 +2942/-223） |
| 323 | 12fb99f3 | 一致（Fix Review 控制包同步，工程控制 10 文件） | 328 | c6efe310 | 一致（HISS_VERIFY 全 Tile+SNR 原因分类，3 文件） |
| 324 | a664f87a | 一致（F-fix 5 SharedWorkPool 并发安全，4 文件 +743/-275） | 329 | 67123a45 | 一致（CudaExecutor 首步，8 文件 +288/-12） |
| 325 | 56f95a87 | 一致（F-fix 6+7 DeviceExecutor+cost-driven claims，5 文件 +583/-0） | 330 | 248080f8 | 一致（F-fix 8 收尾 LNK2019 修复+真调 kernel，4 文件 +380/-17；父链 331→330 吻合） |

### S08<-S07（重叠区 seq 376–385；S08 原判全部"重叠待核"，本次核）

| seq | sha8 | S07（主责）一句话 | S08（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 376 | 4f68a1af | 24号计划 §4 基础：每设备自适应控制保留，利用率目标环降级为报告型并移出常规 ctest，10 文件 +225/-44，M | 同（"每设备自适应控制基础；利用率目标环降级为报告用"，lib/acr） | 一致 |
| 377 | 18e6dcdf | FP64 SNR 子块全链 dtype 感知（snr_dtype 0/1、读写分离、禁止静默转换），8 文件，编号 PREC-109/HISS-102，M | 同（含两个编号） | 一致 |
| 378 | 68266341 | StarDetector FP64 第一步：sdet_image 新增 double 原语 _d 变体（2 文件 +129/-0），PREC-108，S | 同（含编号） | 一致 |
| 379 | 086df45a | 24号计划 §1：BenchmarkDriver 真实 GPU 微基准+ProfileGenerator 聚合后映射（修 median 恒 0）+holdout 测试，8 文件，M | 同（"统一真实 benchmark 与 hardware-profile 管线"） | 一致 |
| 380 | 997cf4c6 | 24号计划 §7：MSVC ASan 覆盖扩至 device_executor/cpu_controller/system_metrics（2 文件 +118/-5），S | 同（"sanitizer 覆盖扩至 cpu executor/contracts/controller"） | 一致 |
| 381 | dcb8c197 | Drizzle 阶段 6：Tile 局部累加+流式写入（取消每 leaf unordered_map），4 文件 +790/-629，M | 同（括注同一取消点） | 一致 |
| 382 | d7601233 | lib/acr/memory.md 记 24号计划进度+holdout 门限改按量级（≤200%），2 文件，S | 同（"24-plan 完成进度与 holdout 容差记录"） | 一致 |
| 383 | a407392d | Drizzle 阶段 7：TileAccumulator 模板化真 Scalar 累计+spherical_overlap/wcs_sip 双实例，10 文件 +520/-310，M | 同（并注其为本片 PREC/DRIZ 链锚点父——其区内谱系信息） | 一致 |
| 384 | 3b7c545e | 25号计划 §1：qualification kernel 去竞态+workload 等价描述符，3 文件 +242/-63，M | 同 | 一致 |
| 385 | fd42381b | 25号计划 §2：CUDA 桥接独立容量记账+分块卷积修正+扩缩容测试，5 文件 +227/-22，S | 同（并注其为 ACR 链锚点父） | 一致 |

### S09<-S08（重叠区 seq 431–440；S09 原判全部"重叠待核"，本次核）

| seq | sha8 | S08（主责）一句话 | S09（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 431 | cc1d6923 | Auto 设备 worker 启动前先过资格门（focused_benchmark/dispatcher/mixed_route_planner+2 测试，6 文件），S | 同（6 文件名单逐项相同） | 一致 |
| 432 | 83291adf | 真实 CUDA device buffer 驻留持久化（residency_manager+dispatcher 桥接 persistent 上传，6 文件 +207/-23），S | 同（点名 residency_manager、acr_cuda_bridge_host 等 6 文件） | 一致 |
| 433 | 233359d0 | reduce/drizzle 私有 partial 与 merge（focused_operations 3 文件 +152/-16），S | 同 | 一致 |
| 434 | eba1c23d | pinned 内存预留台账：新增 utilization/pinned_ledger.{cpp,hpp}+dispatcher 挂接（5 文件 +174/-0），S | 同（新增两文件点名相同） | 一致 |
| 435 | c82013ee | lib/acr/memory.md 记 focused V2 修复进度（+27；关联注 429 V2 包回写），S | 同（并附卡内细节："聚焦版 v2（控制包 9）"审计 8 项阻断全修复、点名 40c9d21/3cbd998/cc1d692），与 S08 定性相容 | 一致 |
| 436 | e107061c | drizzle buffer 角色与 residency 输入角色修正+GPU 测试串行化（4 文件 +130/-95），S | 同 | 一致 |
| 437 | 0f451b28 | wiki 指针更新，消息记录"Phase1 未闭合, NOT_SYNCED"（gitlink +1/-1），S | 同（附 c199ee9→9c35212 具体指针值，出自其卡，无冲突） | 一致 |
| 438 | a36c4825 | 成本单位订正+qualification 与 gpu eligibility 分离（lib/acr 3 文件 +81/-46）+同提交 ACR_FOCUSED_CONTROL_PACKAGE_V3 全套 21 文档（+1731），M | 同（21 件、3 代码文件、V3 名单相同） | 一致 |
| 439 | c00266eb | 混合路由 makespan 主张核算修正+保留 recommended chunk（mixed_route_planner+测试），S | 同 | 一致 |
| 440 | a50a97a2 | CAND-001 候选安全修复：buffer_rad 1.0→1.1（实测上限 1.0438×hp_res）、极冠/边界回退球面查询、新增独立 Oracle（自述 5502/5502），M | 同（三项改动与 5502/5502 全同） | 一致 |

### S10<-S09（重叠区 seq 486–495；S10 原判全部"重叠待核"，本次核）

| seq | sha8 | S09（主责）一句话 | S10（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 486 | ee582449 | PSF-001 坐标契约修复：dpsf_psf.cpp +5/-2、orchestrator 星点链消费 PSF 拟合中心 +94/-19（合计 +99/-21），生产代码 | 同（PSF-001、两模块、+99/-21） | 一致 |
| 487 | 863a8490 | Phase A 坐标契约最终测试 gate1_psf_final_test.py（+333），G1-G6 全过 | 同（脚本名与 +333 相同） | 一致 |
| 488 | 70edb842 | XPSD PCL 官方解码进生产链（flux_min/flux_mul 绝对积分，8 文件 +602/-19） | 同（6+2 文件拆分与模块相同） | 一致 |
| 489 | f75d4eb4 | reverse_drizzle.cpp 增 WCS 物理范围校验（crval/CD 上限，+12），输入校验 | 同（+12） | 一致 |
| 490 | 732c3a62 | 删 lib/orchestrator/configs 6 个旧格式配置（-495），结构整理/入口唯一化 | 同（删 6 文件 -495，"纯删除整理"） | 一致 |
| 491 | 57112296 | 两模块 memory.md 记 XPSD 官方解码语义与 Phase B 结果（+24），台账 | 同（+24；并注"本片 0498 之父"——其区内谱系事实，无冲突） | 一致 |
| 492 | 4769979c | route_profile_v2 扩 scenario 级 qualification 与 probe/final 字段（+252/-3；关联注"父=477"） | 同（+252/-3；注"父 2a7fbe3b（ACR 分支起点）"——经查 seq 477 的 sha8 即 2a7fbe3b（commits_table.csv 可核），两表述同一事实） | 一致 |
| 493 | 91737ae1 | benchmark_route_estimator 加 2D chunk 插值/scenario-gated routing/replay mode（两文件 +273/-78） | 同（两文件） | 一致 |
| 494 | d61c7560 | route_profile_calibration.cpp 大改：fit/probe/final 数据集隔离+真实自适应细化+cold mixed 等（+966/-460） | 同（+966/-460） | 一致 |
| 495 | 23378a25 | weighted_integration_benchmark 从 profile 出 replay 报告、按 scenario 取 best、真实门（+163/-217） | 同（+163/-217） | 一致 |

### S11<-S10（重叠区 seq 541–550）——**已完成核对**

S11 报告第一节在分片时即对照当时已存在的 H-S10.md 逐条核对，10 条全判"一致"，并下游确认两分支合流（551 父=198d69e0）。本次仅登记，不重做：

| seq | sha8 | S11 原判 | seq | sha8 | S11 原判 |
|---|---|---|---|---|---|
| 541 | 86142ec4 | 一致（CFG-001 V6 归档清理，6 文件 +15/-10） | 546 | ea1d01c1 | 一致（VRAM 门+auto report+ready 聚合，3 文件） |
| 542 | f55e7846 | 一致（wiki 指针→a34a264，PHASE1=FROZEN 移除） | 547 | af7cbea7 | 一致（test_dispatcher_bdr.cpp +286 注入测试） |
| 543 | e7c58363 | 一致（memory.md 记 V6 审核包 39bed5eb，+28） | 548 | 303c38e8 | 一致（快照 100ms 缓存，+15/-1） |
| 544 | 2a009a05 | 一致（V6 审核包 SHA→73cdd6be，+1/-1） | 549 | e949b306 | 一致（lib/acr/memory.md 控制包 50127981 台账，+72） |
| 545 | 7caa7289 | 一致（ACR 基座三修，4 文件 +158/-16） | 550 | 198d69e0 | 一致（merge 352 文件 +60198 结构模式；551 谱系衔接确认） |

### S12<-S11（重叠区 seq 596–605；S12 原判全部"重叠待核"，本次核）

| seq | sha8 | S11（主责）一句话 | S12（重叠区登记）一句话 | 判定 |
|---|---|---|---|---|
| 596 | 594db03f | phase2-v3 R7：Linear Fit 改 Siril 1.4.3 定义语义（x 按值排序、σ=mean|residual|、N>3 守卫）+linear_fit_oracle.py，gate 37/37，S | 同（三要点与 gate 计数全同） | 一致 |
| 597 | 89ed5482 | R8 局部 SNR/fallback 生产语义：per-cell local_snr_map、回退整帧 median 计数、G7 runtime gate PASS，S | 同 | 一致 |
| 598 | 470b5c7c | R9 证据闭环：CUDA begin_offset 微-chunk 修复、CPU/CUDA Sigma 0 diff、死代码清理、MEMORY_ACR_COMPARE=PASS、schema 加 acr_route，M | 同（四点全同） | 一致 |
| 599 | 746f9981 | v4 R1 生产接线：stage2_common 共享 parse/build、raw/normalized 权重公共层、geometry hash、G1 PASS、gate 38/38，M | 同 | 一致 |
| 600 | cbf6918e | R2 payload 敏感帧身份：tile 原始字节+SNR 目录哈希、frame_id 缓存、DATASUM reader API，M | 同 | 一致 |
| 601 | 4d8a8666 | R3 完整空间 truth：跨 tile 边界节点合并（seam jump→0）、abs p95/max 硬门、per-component gauge API，M | 同（并附其区内补充：seq 607 撤销该合并的消长关系，不涉 601 本身定性） | 一致 |
| 602 | 777b175d | R4 完整 sequential RCR：官方 rcr 2.4.7 performRejection 链重写、SS_MEDIAN_DL 校准表、13/13 对齐，M | 同 | 一致 |
| 603 | 7a8a1378 | R5 真实 Siril 1.4.3 LinearFit 冻结参照：生产对齐官方 siril_fit_linear+N−r≤4 守卫，M | 同 | 一致 |
| 604 | 1e9498ea | R6 snr_available 显式化（废弃 1.0-as-unknown）、三区真实产物门、gate 41/41，S | 同 | 一致 |
| 605 | 71d41862 | config_smoke 子进程补 PATH（astro_image_io.dll），CONFIG_SMOKE=PASS，S | 同 | 一致 |

### S13<-S12（重叠区 seq 651–660；S13 原判全部"重叠待核"，本次核）

| seq | sha8 | S12（主责）一句话 | S13（重叠区摘要）一句话 | 判定 |
|---|---|---|---|---|
| 651 | f2d7f5a1 | v15 ACR 路径 underdetermined 计数改 resolved plan（stage2.cpp +3/-1）+docs×4/reports×9+浏览器 3 处小修，18 文件，M | 同（stage2.cpp+browser STF 面+文档/报告/round1 findings） | 一致 |
| 652 | 77879c72 | oracle 边界断言对齐 INVALID_INPUT 契约（rejection_oracle_compare.py +5/-1）+self_review round2-5 五件，S | 同 | 一致 |
| 653 | 8a772cae | round6_final_verification.md+docs/validation 订正：FINALIZATION_SELF_REVIEW=PASS（6 轮+清洁树自述），S | 同 | 一致 |
| 654 | 2e52c437 | 根 memory.md（+18）与 lib/phase2/memory.md（+19）追加 V15 最终语义收口条目，S | 同 | 一致 |
| 655 | ef6ab2fa | v16 修复：profile 拆分（wbpp_current vs astrocs_adaptive）、RejectionNormalizationPolicy（median_center 默认/percentile 负值安全/INVALID_CONFIGURATION）、MinMax 固定计数单遍、统一 eligibility 收集器，8 文件 +1078/-551，M | 同（五要点全同；"固定计数/定长"为一字之差的同义复述） | 一致 |
| 656 | fd145ee8 | ScratchVec 堆模式迁移修 n>64 崩溃+真实 16 曝光卫星门 V2 度量工具（2 文件 +234/-8），S | 同 | 一致 |
| 657 | 37bebb9b | WBPP Light 默认值配置化（linearFit 5/3.5、percentile 0.2/0.1）+astrocs_adaptive profile+schema 同步，S | 同（数值全同） | 一致 |
| 658 | 1145a28f | v16 报告批次收编（final_status/wbpp_feature_matrix/rejection_normalization/satellite_v2/full_e2e 等 17 文件 +510/-371）+自审 rounds 0-6 升级，M | 同 | 一致 |
| 659 | b72e3023 | 根 memory.md（+16）与 lib/phase2/memory.md（+22）追加 v16 进展条目，S | 同 | 一致 |
| 660 | ed7a544d | 称"phase2-v17 **收口**"——非有限 weight/support→INVALID_INPUT、显式状态 NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT、canonical support reducer、INVALID_* 硬契约、版本化 profile 名+删别名，M；但同条关联栏又称"v16→v17 边界""v17 campaign 完成状态由后续片核对" | 称"V17 **起点**"，内容五要项逐一同名同值 | **口径差-可兼容**（事实全同；阶段定位表述差，S13"起点"与 S12 关联栏口径一致，见备注 R-3） |

### 不一致条目（单列）

#### seq 161 | ed145a70 | 2026-07-27 | L（分片对 S04<-S03）

- **双方原文引述**
  - S04（后片重叠区第 161 行原文）：「docs: 首版项目 README（182 行）+ audit/AstroCS-v1.1-audit-pack（48 文件，含 git bundle 与 AUDIT_MANIFEST.json）+ **tools/_commit_audit.*、tools/_p06/p07_*.log 等 16 个 tools/_* 运行留痕（该 16 个属运行产物性质，非工程内容）**」
  - S03（前片主责 seq 161 条目原文）：「**(c) tools/ 下 19 个 _p06_002_*、_p07_001_*、_p07_002_*、_commit_audit_* JSON/log/txt 运行中间件与 tools/gen_audit_pack.py 生成脚本**（自述）」
- **分歧点**：同一提交（81 文件 +15944/-0）中 tools/_* 运行留痕的文件数——S04 记 16，S03 记 19（另有 1 个非下划线脚本 gen_audit_pack.py）。其余各分量（README +182、audit-pack 48 文件、总数 81 文件、性质定性）双方一致。
- **以哪边为准**：**以 S03 为准**。
- **依据（可核对材料）**：`设计大纲/_evidence/commits/index.jsonl` 中 seq 161（sha8 ed145a70）的 files 全清单：tools/ 前缀文件共 **20 个 = 19 个 tools/_* 运行中间件 + tools/gen_audit_pack.py**（19 件逐项为 _commit_audit.json、_commit_audit_msg.txt、_p06_002_commit_msg.txt、_p06_002_run_snr_debug.json/.log、_p06_002_run_tests.json/.log、_p07_001_git.log、_p07_001_git2.json/.log、_p07_002_commit_msg2.txt、_p07_002_git.json/.log、_p07_002_git2.json/.log、_p08_001_commit_msg.txt、_p08_001_release.json/.log、_p08_dll_deps.txt）；顶层分布 README.md 1 / audit 48 / engineering 12 / tools 20 与双方共同认可的"81 文件"合计吻合。S04 的 16 与该清单任何自然口径均不吻合（其引用的全卡 0161 样本区因卡片截断仅见 14 个 tools/_ 名），判为计数误差；S03 的 19+1 与清单逐项一致。
- **影响**：不改变双方对 seq 161 的整体定性（文档+审计包入库、运行留痕属产物性质），仅为条目内一处可证伪的事实计数差。

### 备注（口径差与跨片补充说明）

- **R-1（seq 49，S02<-S01）**：事实层面双方完全相同（两文件、+221/-145、v2.0 口径、无代码面）。差异仅在内容性质标签：S01 将 seq 49–55 作"文档定稿批组"统一标注"文档或规格 + 控制包或台账"（"控制包或台账"分量源自 50–55 的模块级 memory.md 集中建档）；S02 对 49 单条注"纯文档"（49 确为两既有文档修改、无新增文件）。分组批复合标签与单条纯文档标签不互相否证，判口径差-可兼容。
- **R-2（seq 110，S03<-S02）**：S02 在 110 关联栏称"至此 10 个模块仓库全部并入主仓"；S03 共同形态栏称导入序列为 seq 101..111 共 11 仓（第 11 个 star_detector 在 S03 主责 seq 111，manifest/S03 正文可查）。101–110 十条本身逐项一致；"10/11"之差由分片视野边界造成、S03 已在自己主责区补全，不构成条目级不一致。另记：S03 的 seq 111 关联栏"续接重叠区 seq 101..110 的十一个 merge"中"十一个"当为"十个（加本条共十一）"之笔误，属 S03 区内表述，不影响本比对结论。
- **R-3（seq 660，S13<-S12）**：五项内容改动（INVALID_INPUT、三个显式状态、canonical support reducer、INVALID_* 硬契约、版本化 profile 名+删别名）双方逐条同值同字面量；差异只在把 660 放在 v17 时间轴的哪一端——S12 正文起句"phase2-v17 收口"与其自身关联栏"与 658 构成 v16→v17 边界""v17 campaign 完成状态由后续片核对"内部有张力，S13"V17 起点"与 S12 关联栏口径一致。判口径差-可兼容。
- S05、S13 等片重叠区只登记消息首行+分级（或一句卡载摘要），其"逐条判一致"的含义即"后片登记锚点与前片主责正文逐项相符"；已按该口径逐条核过分级、日期、sha8 与主题。
- 各片重叠区行数与 manifest 角色列完全对应：S02:46–55、S03:101–110、S04:156–165、S05:211–220、S06:266–275、S07:321–330、S08:376–385、S09:431–440、S10:486–495、S11:541–550、S12:596–605、S13:651–660，无缺行多行。
- 上篇核对方法：只读 设计大纲/reports/history/slices/H-S01..S13.md、设计大纲/_evidence/commits/slices/S02..S13/manifest.csv、cards/compact 卡、index.jsonl、commits_table.csv；未执行 git show，未改动任何既有文件。

---

## 下篇 · 分片对 S14<-S13 … S25<-S24（另一核对员既有成果，原样保留）

# 历史取证重叠区核对（overlap_recheck）

- 任务：37 份分片报告早期并发派发导致部分片写"重叠待核"；本文件对分片对 S14<-S13 … S25<-S24（共 12 对、每对 10 条 seq，合计 120 条）逐条复核重叠区与上一片主责条目的一致性。
- 方法：(1) 从 `设计大纲/_evidence/commits/slices/<后片>/manifest.csv` 取角色=重叠复核 的 10 行 seq；(2) 在后片报告的"重叠区核对/重叠复核"段落取该侧一句话结论；(3) 到前片报告主责条目正文取该侧结论；(4) 按"做了什么／落点／内容性质"三要素比对，判定 一致 ／ 口径差-可兼容 ／ 不一致；后片报告若已逐条判"一致"（含 3.2/§4 终核段），登记"已完成核对"，不重复裁决。
- 材料：仅仓库既有文件（分片报告、manifest、紧凑卡/全卡、index.jsonl）；未改动任何证据文件。
- 判定统计见文末"总计"；唯一"不一致"条目单列一节。

## 一、总览

| 分片对 | 重叠 seq | 后片原判 | 本轮动作 | 一致 | 口径差 | 不一致 |
|---|---|---|---|---|---|---|
| S14<-S13 | 706–715 | 逐条已判"一致"(10/10) | 登记已完成核对 | 10(登记) | 0 | 0 |
| S15<-S14 | 761–770 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S16<-S15 | 816–825 | 重叠待核×10 | 逐条复核 | 9 | 1 | 0 |
| S17<-S16 | 871–880 | 逐条已判"一致"(10/10) | 登记已完成核对 | 10(登记) | 0 | 0 |
| S18<-S17 | 926–935 | 重叠待核×10 | 逐条复核 | 9 | 1 | 0 |
| S19<-S18 | 981–990 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S20<-S19 | 1036–1045 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S21<-S20 | 1091–1100 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S22<-S21 | 1146–1155 | 已有逐条结论(9一致/1不一致) | 逐条复核并对争议条裁决 | 9 | 0 | 1 |
| S23<-S22 | 1201–1210 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S24<-S23 | 1256–1265 | 重叠待核×10 | 逐条复核 | 10 | 0 | 0 |
| S25<-S24 | 1311–1320 | 逐条已判"一致"(10/10) | 登记已完成核对 | 10(登记) | 0 | 0 |

## 二、逐对逐条明细

### S14<-S13（seq 706–715）——已完成核对（跳过）

H-S14.md "二、重叠区复核（对照 H-S13.md）"与"3.2 重叠区核对（逐条）"已对 10 条逐一判"一致"（汇总语："全部 10 条一致，无不一致条目"；另附 3 处补充观察：713/714 模式复现、§12 假 PASS 双自述、紧凑卡+278/全卡+257 卡内口径差，后片自注均不构成冲突）。本轮登记，不重复裁决。

| seq | sha8 | 判定 |
|---|---|---|
| 706 | feb12ee0 | 已完成核对（后片逐条判一致） |
| 707 | 2871a517 | 已完成核对 |
| 708 | 2885bea5 | 已完成核对 |
| 709 | edfdd1d1 | 已完成核对 |
| 710 | 1752e8ca | 已完成核对 |
| 711 | ce8ec527 | 已完成核对 |
| 712 | a3506d64 | 已完成核对 |
| 713 | fb59df96 | 已完成核对 |
| 714 | c4131430 | 已完成核对 |
| 715 | 23c11c19 | 已完成核对 |

### S15<-S14（seq 761–770）

后片 H-S15.md 第二节标"重叠待核"并各附一句话自洽描述；前片 H-S14.md 主责正文逐条比对：

| seq | sha8 | S15（后片）一句话 | S14（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 761 | 7e889a9b | B2-07/DRZ-02：DRIZZLE_GEOMETRY.md 三层缓冲口径（1.25×/3.0×/1.25× hp_res，锚 spherical_overlap.cpp:40、HP_CIRCUMRADIUS_FACTOR） | B2-07 文档对齐：把"查询圆=包围圆+2.0×hp_res"旧单口径改写为与代码一致的三层缓冲口径，+7/-2，纯文档 | 一致 |
| 762 | 6f8b3bbf | B2-10/PH2-01：UPM_SOLVER.md +1 行（raw vs normalized weights） | B2-10 澄清文档 +1：补 raw=quality·control_ivar 与 normalized=raw/sum·geom 分层注释 | 一致 |
| 763 | b3bb379b | [QA-V19R7-B2]：台账 B2-07/09/10 TODO→DONE | 结构模式标记组：MASTER_TASK_REGISTER.csv 单文件翻转，763 行 B2-07/09/10（+3/-3） | 一致 |
| 764 | 2f0d6749 | B2-12/PH2-05：INTEGRATION_ALGORITHMS.md zero-weight 契约锚点 | B2-12 文档锚点 +1/-1：integrate.cpp:48、:65-66 零权重合同句写入算法文档 | 一致 |
| 765 | 78615aae | B2-11/PH2-04：REJECTION_ALGORITHMS.md large_scale trail 锚点 | B2-11 文档锚点 +1/-1：large_scale trail 不生长 compact cosmic 的代码锚（rejection.cpp:1501-1592） | 一致 |
| 766 | 6f946469 | B2-01/SC-07/08：CALIBRATION_ALGORITHMS.md dark_opt 双分支 + flat clamp 0.1（calibrator.cpp 锚点） | B2-01 文档对齐 +7/-3：Postconditions/伪代码双分支（K=t_light/t_dark、k→1.0 fallback）、flat clamp 0.1 与 calibrator.cpp 行号锚 | 一致 |
| 767 | 34f43ad4 | [QA-V19R7-B2]：台账 B2-01/11/12 TODO→DONE | 结构模式标记组：767 行 B2-01/11/12（+3/-3） | 一致 |
| 768 | 7ce23d24 | B2-05/NO-01：NOISE_ESTIMATION.md +2 行（gain 诊断隔离） | B2-05 文档隔离：per-file +2（聚合 +4/-0），新增"Gain/Readnoise 仅诊断、不入生产"节 | 一致（+2 为逐文件口径，与前片聚合口径同事实） |
| 769 | df37783b | B2-02/SC-09：PLATESOLVE.md CRPIX 0/1-based 换算 + A/B 前向 AP/BP 逆向 + 极区分支 | B2-02 文档对齐 +2/-2：CRPIX 换算、SIP 阶数上限 2–3、极区保守分支，并删除旧"A/B 双前向，V18R3 审计"表述 | 一致（S15 记 diff 新增行、S14 记删除行；卡 0769 证实为同一改写行的两面，互补非矛盾） |
| 770 | bfbaef24 | [QA-V19R7-B2]：台账 B2-02/05 TODO→DONE | 结构模式标记组：770 行 B2-02/05（+2/-2） | 一致 |

小计：10 一致 / 0 口径差 / 0 不一致。

### S16<-S15（seq 816–825）

后片 H-S16.md "2. 重叠复核条目"标"重叠待核"+面内事实；前片 H-S15.md 主责正文逐条比对：

| seq | sha8 | S16（后片）一句话 | S15（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 816 | 30da7fab | V19R8 规格草案 30_WIKI_TO_CODE_QUALITY_V19R8_SPEC.md 新增 +212（工程控制/docs，S 级） | 新建同文件 +212：V19R8 六阶段质量闭环草案（S0–S6、权威链、红线、约 100 commits 预算） | 一致 |
| 817 | b3aa1c77 | HEALPix 去重收敛 lib/common 权威（B4-01/DRZ-01，5 文件 +261/-762，M 级结构模式） | B4-01 去重落地：healpix_db +97/-762、common +164/-0（合计 +261/-762），净 -501，生产代码重构 | 一致 |
| 818 | 126223cd | V19R8 三件套补齐：QA_V19R8_QUALITY.md +185、QA-V19R8-QUALITY-OPTIMIZATION.md +421 | 同两文件新增（checklist + umbrella 任务分解，+606） | 一致 |
| 819 | 10f1a249 | 台账销账 B4-01 DONE [QA-V19R7-B4]，DRZ-01 | 台账 B4-01 TODO→DONE（+1/-1），自述 53/95、P0 cleared | 一致 |
| 820 | a7b15faf | V19R8 Spec 精化 +44/-1，自述 212→255 行，与变更面算术吻合 | Spec 第一次精化 +44/-1（增 4.3/4.4/5.9–5.11/9 节，212→255） | 一致 |
| 821 | f1cf87a6 | round03_status.json 新增，53/95，含 b4_todo_head 六项 | round03_status.json 新增 +16（done=53/todo=42），b4_todo_head 列 B4-10/11/13/14/15（五项） | 口径差-可兼容（见注①） |
| 822 | d2732520 | round07_status.json 新增，53/95 hold | round07_status.json 新增 +20，同口径 hold（b4_running B4-10/14） | 一致 |
| 823 | be617347 | B4-14：dynamic_psf Moffat4 数值契约文档注释，dpsf_psf.cpp/.h +20/-0 | 同：cpp +6 / h +14 新增 Moffat4 契约注释块（公式、守卫清单行锚），不改算法 | 一致 |
| 824 | 56a99645 | 销账 B4-14 DONE | 台账 B4-14 TODO→DONE，54/95 | 一致 |
| 825 | 3c131e81 | S2-03：STAR_PSF 文档数值守卫对齐 dpsf_psf 锚点 + platesolve SIP 距平（docs 2 文件 + ipv_sip.cpp） | S2-03 跨谱系收尾：STAR_PSF_ALGORITHMS.md 补锚、PLATESOLVE.md 措辞、ipv_sip.cpp 三常量回指注释（值不变） | 一致 |

小计：9 一致 / 1 口径差 / 0 不一致。

### S17<-S16（seq 871–880）——已完成核对（跳过）

H-S17.md "2. 重叠复核条目"对 10 条逐一给出"**一致**"判定（对照 H-S16.md 主责正文），"4. 重叠区核对"汇总"10 条全部一致、无'不一致'条目"；其中 871 的"+3/-0 vs +3/净+2"次级出入已由该片自判为行级口径差异、不影响结论。本轮登记，不重复裁决。

| seq | sha8 | 判定 |
|---|---|---|
| 871 | 9261ea66 | 已完成核对（后片逐条判一致） |
| 872 | 8b1e7b1b | 已完成核对 |
| 873 | b9e53c50 | 已完成核对 |
| 874 | 9ae70320 | 已完成核对 |
| 875 | 51a578a8 | 已完成核对 |
| 876 | c616ffd2 | 已完成核对 |
| 877 | 4e83a052 | 已完成核对 |
| 878 | 68aed09b | 已完成核对 |
| 879 | 3dbf4723 | 已完成核对 |
| 880 | ca9b077d | 已完成核对 |

### S18<-S17（seq 926–935）

后片 H-S18.md "2. 重叠复核条目"标"重叠待核"+卡片视角摘要；前片 H-S17.md 主责正文（926-927/928/929-930/931/932/933-934/935 各组）逐条比对：

| seq | sha8 | S18（后片）一句话 | S17（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 926 | 1d03dcc7 | 恢复 c04_phase2_gate.log 为 2123B vm-bj SKIP 证据版（替换 15344B 残缺 partial，1 文件 +30/-299），D-04 证据修复第一步，运行日志入库 | 926 把该 log 回退为 2123B SKIP 版（+30/-299；小节 +25/-276），自述 stray partial、回退后 22/22 OK；C-04 振荡链中段回摆 | 一致（S18 存疑的"SKIP 是否终态"由 S17 的 929 终态叙述回答，非矛盾） |
| 927 | 105dd7a9 | 重生成 v19r8_quality 的 manifest.csv+SHA256SUMS.txt（各 ±1，自述 22/22 OK） | 同两文件各 +1/-1，自述 "c04 restored 2123B now 22/22 OK" | 一致 |
| 928 | 50e0358b | MASTER_TASK_REGISTER.csv 标记 D-01 DONE（±1） | 销账 QA-V19R7-D-01 TODO→DONE，92/95，自述 Fresh Review round83 四检查 PASS | 一致 |
| 929 | 3526a297 | c04 证据"恢复真实执行版"（79 PASSED/10 SKIPPED/35724ms，+299/-30）；标注面内矛盾：消息自述含代码改动但变更面仅 1 个日志文件 | 929 再改写为真实执行版（总量 +299/-34；小节 +276/-25）；变更面仅含日志，消息点名的 synthetic_gate.cpp 不在本面（可移植修正实际载于 924） | 口径差-可兼容（见注②；"自述 vs 变更面"定性双方一致） |
| 930 | b26db31a | 再次重生成 manifest+SHA（[D-04 fix3]，2 文件 ±2） | 930 清单重生成：manifest c04 行 2123B/87d222→15344B/4e2aef37，SHA 同步，各 +1/-1 | 一致 |
| 931 | ba837501 | 台账标记 D-02 DONE | 销账 D-02（Repository Auditor 抽样）TODO→DONE，93/95，自述 5/5 抽样 | 一致 |
| 932 | 2edce846 | 台账标记 D-05 DONE | 销账 D-05（干净 HEAD 校验），自述 HEAD 57c0652==origin/main（片外 sha 已登记） | 一致 |
| 933 | af3ee37b | V19R8 final "94/95" 发布口径：CHANGELOG.md +4、RELEASE_STATUS.md ±1 [D-06] | 933 发布口径终版：CHANGELOG 新增 94/95 节（+4）、RELEASE_STATUS 90→94/95（+1/-1），维持"不宣称发布" | 一致 |
| 934 | dad9e227 | 台账标记 D-06 DONE | 销账 D-06，自述"V19R8 final → 95/95 DONE" | 一致 |
| 935 | 754abc99 | CFG-001：8 个 stage2 配置 json legacy→V17 typed（GC 3panel 对齐 wbpp_2_9_1 auto；未修则 p2_stage2_parse_config rc=7）；CONFIG_REFERENCE pixfrac 1.0→0.8；smoothing 保持 0.0 | P0 配置迁移 CFG-001：8 份 stage2 JSON 扁平键→typed 块（键 +60/-33）、gc_3panel 改动最深、pixfrac 0.8 冻结，9 文件 +94/-34 | 一致 |

小计：9 一致 / 1 口径差 / 0 不一致。

### S19<-S18（seq 981–990）

后片 H-S19.md 重叠表标"重叠待核（无对照物）"+本片独立读解；前片 H-S18.md 主责正文（980+981/982+983/984+985/986/987/988+989+990 各组）逐条比对：

| seq | sha8 | S19（后片）一句话 | S18（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 981 | 63178f51 | reports/README.md 新增 +16 版本归档说明，纯文档单文件 | 980+981 组：981 新增 reports/README.md（+16）说明版本归档规则；该组整体性质=结构整理+文档 | 一致 |
| 982 | 0ed8ed95 | CON-008：async_io.h/async_io_test.cpp +153（失败/队列/多 worker 测试）+ docs/architecture 线程安全合同 +22/-10 | 982：async_io.h 注释 reader 线程安全约束、测试 10 项（+153）、ASYNC_IO_CONTRACT.md 置 PASS+§9（+22/-10） | 一致 |
| 983 | cfb49bd6 | CON-008 PASS 收账：ledger/COMMITS/CON008_progress.md 三处台账文档 | 983：改 02_TASK_LEDGER.csv、COMMITS.csv，新增 CON008_progress.md（+22），纯台账销账 | 一致 |
| 984 | f86afc88 | CON-009：ivar_wiring_test/synthetic_gate +51（2T 重复确定性校验）+ CON009_complete.md | 984：两测试文件加重复 2T 建模（+51）、新增 CON009_complete.md（+42，TSan 工具链+矩阵论证） | 一致 |
| 985 | c2e5e24d | CON-009 PASS 收账：02_TASK_LEDGER 与 COMMITS 两行 | 985：台账销账 CON-009 PASS（两表行） | 一致 |
| 986 | 53a1cc04 | CON-010 开案 FAIL：生产 CLI 2T SIGSEGV（sampler 并行 AIO 读竞态），新增 CON010_runtime_gate.md +78，编号兼挂 CON-004 | 986：1T exit0/2T exit-11 实测+TSan 定位 cfitsio 并发读非线程安全，CON010_runtime_gate.md 新增、台账置 FAIL、CON009 限定补记；承接 969 CON-004 | 一致 |
| 987 | f343090a | 唯一代码修复：sampler.cpp +10/-2 并行采样 AIO 读串行化（g_aio_mu），消除 cfitsio 并发读崩溃 | 987：sampler.cpp 新增 file-scope g_aio_mu 串行化 aio_hips_open/read_tile_pair（+10/-2），计算仍并行，效率代价自述 | 一致 |
| 988 | 411d729e | CON010_runtime_gate.md 改写（+57/-64）：崩溃已修但并行效率门禁仍 FAIL | 988：更新实测 wall≈1.02、CPU≈107% ⇒ FAIL 维持，"禁止启动 32R" | 一致 |
| 989 | bb1b3ba6 | 同文件 +21/-6：补可执行技术阻塞与串行化源清单 | 989：补可执行技术阻塞与根因（串行化源清单） | 一致 |
| 990 | ee5126af | 同文件 +8：门禁失败与工作负载无关，Amdahl 上限 ~1.34x | 990：换真实 rejection 'sigma' 复测排除复杂度因素，Amdahl ~1.34x、门禁现行架构不可满足、记 G2 停止点（+8/-0） | 一致 |

小计：10 一致 / 0 口径差 / 0 不一致。

### S20<-S19（seq 1036–1045）

后片 H-S20.md 重叠表仅给消息首行节选+"重叠待核"；前片 H-S19.md 主责正文逐条比对（后片侧描述较浅，比对限于消息级，但无一处定性分歧）：

| seq | sha8 | S20（后片）一句话 | S19（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 1036 | 6a947f51 | feat(reaudit): 审核包生成 + 停止时点记账 | REAUDIT_V3 收束：REVIEWPACK md 新增（RUN-003/004/005 记账、TSan 5634 建议 TST-003 FAIL）、执行侧停止、控制包打回 | 一致 |
| 1037 | 020cdc99 | feat(v4): C0-001 V4控制包本体 | 新增自造 V4 控制包本体（REAUDIT_V4 21 文件 +1400，自述 CONTROL_PASS files=21） | 一致 |
| 1038 | ae643541 | feat(v4): C0-002..005 起点 SHA 冻结 | V4 包 C0-002..005 执行：起点 SHA 冻结/工具链盘点/债务登记/账本初始化/capsule 机制（3 文件） | 一致 |
| 1039 | b12305ed | chore(v5): 移除自造 V4 重审包归位原包（L 级） | L 级结构模式：REAUDIT_V4 21 文件全删（-1461）+ 用户 CONTROL_V4 原包 26 文件 +1102 归位 | 一致 |
| 1040 | 1c580eb9 | chore(v5): 前会话遗留工作树归位 | 前会话工作树归位：8 份 EVIDENCE_INDEX 改名迁移、self_review 8 件新增、traceability 重生成（17 文件） | 一致 |
| 1041 | 38951423 | chore(v5): 补全 self_review 旧路径删除侧 | 补 1040 遗漏的删除侧：根 self_review 8 文件全删（-283） | 一致 |
| 1042 | f99e80d8 | (BASE-001) 起点冻结与风险登记 | BASE-001：V5 控制包 37 文件入库 + artifacts/prerelease_v5 初始化（BASELINE 三 SHA、RISK_REGISTER 6 项）+ make_capsule.py | 一致 |
| 1043 | 15720f5e | (GOV-001) AGENTS 合并 V5 短块 | GOV-001：AGENTS.md +26/-78（状态机/节点/目录规范节）+ check_agents_gov.py（+42）+ 台账 PASS | 一致 |
| 1044 | 9fe142e0 | (VER-001) alpha 单一版本源 | VER-001：根 VERSION 单源 + gen_version/check_version_consistency/schema/VERSIONING.md/tests（10 文件） | 一致 |
| 1045 | 45e64640 | (TRACE-001) 六层追溯 schema | TRACE-001：check_traceability.py（+123，claim 正则+R1–R7）+ TRACEABILITY 种子 6 claim + 测试（10 文件） | 一致 |

小计：10 一致 / 0 口径差 / 0 不一致。

### S21<-S20（seq 1091–1100）

后片 H-S21.md 重叠段标"重叠待核"（H-S20.md 当时未落盘）+一行画像；前片 H-S20.md 主责正文（CLI-002/003、ABI-001..003、ISA-001 各组两提交）逐条比对：

| seq | sha8 | S21（后片）一句话 | S20（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 1091 | 2774a26d | (CLI-002) 表行登记 | 1091 表行登记 CAP-CLI-002（1090 的伴生登记） | 一致 |
| 1092 | 8b7cf3e0 | (CLI-003) config/manifest/verify 闭环（MANIFEST_VERIFY_V1 合同+白名单键/schema_version=2） | 1092 同合同落地：main.cpp +331/-27、docs/api +46、tests +159/-3，退出码 8/5/3 映射，自述 102/102 | 一致 |
| 1093 | 12d650c0 | (CLI-003) 表行登记 | 1093 表行登记 CAP-CLI-003 | 一致 |
| 1094 | 7718a2f8 | (ABI-001) C ABI v1 落地（C11/C++17 双编译+host services+12 条目） | 1094 common_abi_v1.h（+163）+ host_services/baseline_backend（+298，12 kernel 条目表）+ tests（+181），代码面 6 | 一致 |
| 1095 | 72fc2e17 | (ABI-001) 表行登记 | 1095 表行登记 CAP-ABI-001 | 一致 |
| 1096 | 0c49a370 | (ABI-002) backend 加载安全化（CPUID+OSXSAVE+XGETBV 实测+预检五查） | 1096 loader/cpu_features 4 文件 +348：同实测口径+预检五查+注入 7 形态全拒，tests +248 | 一致 |
| 1097 | 42d1eedd | (ABI-002) 表行登记 | 1097 表行登记 CAP-ABI-002 | 一致 |
| 1098 | 1761491b | (ABI-003) baseline 全生产 kernel（12 kernel+Oracle） | 1098 baseline_kernels 3 文件 +344/-25（12 kernel、budget 租借行带、DET）+ Python 独立 Oracle 测试 +328 + opcode 门 +42 | 一致 |
| 1099 | 904f73d2 | (ABI-003) 表行登记 | 1099 表行登记 CAP-ABI-003 | 一致 |
| 1100 | 1ae33eb4 | (ISA-001) ISA 热点测量与 AVX2 变体决策（backend_table.inc 抽参+hips+28%） | 1100 backend_table.inc/avx2 变体、tests +281、ISA_VARIANTS.md、MEASUREMENTS.csv（并登记消息自述百分比 vs CSV 实测量差异） | 一致（S21 的 hips+28% 与两侧引用的消息口径及 CSV +28.7 均相合） |

小计：10 一致 / 0 口径差 / 0 不一致。

### S22<-S21（seq 1146–1155）

后片 H-S22.md 已带逐条结论（9 一致/1 不一致）；本轮逐条对照 H-S21.md 组 23–27 主责正文复核其结论，并对争议条（1150）以证据面独立裁决：

| seq | sha8 | S22（后片）一句话 | S21（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 1146 | 50fedef8 | (MON-004) 内存增长分析：cli/memory_growth.h +136、test +122，242/242 | 组23 同：memory_growth.h +136（Theil-Sen/OOM 预警）、tests +122，自述 242/242 | 一致 |
| 1147 | df1539cf | 表行登记；细化：台账行为 IN_PROGRESS→PASS、所引 MON-004 胶囊 zip 未入库 | 组23：seq1147 表行登记+ledger PASS | 一致（细化与补充发现不与前片冲突） |
| 1148 | 866dfd1d | (ISO-001) ACR/GPU/Mixed 隔离验证：test_iso +185、exit 3 拒绝、248/248 | 组24 同：+185、符号扫描 0、发行包纯 CPU | 一致 |
| 1149 | f49808af | 表行登记；补充 zip 未入库 | 组24：表行登记+ledger PASS | 一致 |
| 1150 | 163e9e56 | (PAR-001) 有界队列验证；ASYNC_IO_CONTRACT.md 系**修改（M，19+/0-）非新增** | 组25：tests +162、"docs/architecture **新增 1 篇**（+19）"，254/254 | **不一致**（单列一节，以 S22 为准） |
| 1151 | 5ad37814 | 表行登记；补充 zip 未入库 | 组25：表行登记 | 一致 |
| 1152 | e7034892 | (PAR-002) BLOCKED：六条实证复现；+91/+71 确认为卡片抽取口径差 | 组26：阻断报告+台账 BLOCKED，已标注 +91 聚合/+71 逐文件两口径 | 一致 |
| 1153 | 77d96c35 | (PAR-002) 胶囊登记（自此携带二进制 zip，新增规律观察） | 组26：PAR-002_e703489.zip + 两表行 | 一致 |
| 1154 | 96ed7e89 | (PAR-003) UPM 并行验证 PASS：test +114、PAR003_verification +67，数值逐字对应 | 组27：同（257 tests、1.137e-13、+48KB、40.9→26.0ms） | 一致 |
| 1155 | 7f346b61 | (PAR-003) 胶囊登记：zip+两表行 | 组27：1155 胶囊登记 | 一致 |

小计：9 一致 / 0 口径差 / 1 不一致。

### S23<-S22（seq 1201–1210）

后片 H-S23.md 重叠段标"重叠待核"（H-S22.md 当时未落盘）+一行画像；前片 H-S22.md 组 23（WIN-002 MSVC 移植链逐条明细）比对：

| seq | sha8 | S23（后片）一句话 | S22（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 1201 | d2e00987 | WIN-002 NOMINMAX/__cpuidex/LoadLibraryExA | 1201（3 文件 +14/-5）：loader 受限搜索宏可见才用、否则 LoadLibraryA 回退（消息首行原文即"LoadLibraryExA 搜索flags防御化"）、文件级 NOMINMAX、__cpuidex 签名订正 | 一致 |
| 1202 | 57f90a0f | WIN-002 全局 NOMINMAX 编译定义 | 1202（+1/-1）：NOMINMAX 提升为 CMake 侧 MSVC 全局定义 | 一致 |
| 1203 | 66934677 | WIN-002 mingw64 zlib 链接试验 | 1203（+5/-2）：改链 mingw64 zlib.h+libz.a，引入 ACS_ZLIB_ROOT | 一致 |
| 1204 | 31cf3be2 | WIN-002 改 MSVC-built zlib（libz.lib） | 1204（+9/-3）：改 MSVC 自建 libz.lib/zlib.lib 三档回退 | 一致 |
| 1205 | c8cb1815 | WIN-002 p1_session 辅助函数移出 extern C | 1205（+18/-17）：四辅助函数移出 extern "C"（C2526） | 一致 |
| 1206 | 1016ce2f | WIN-002 _USE_MATH_DEFINES / POSIX→_前缀宏 / dirent_win | 1206（4 文件 +96/-1）：M_PI、下划线前缀宏、新增 dirent_win.h（+62） | 一致 |
| 1207 | 4c2e2b80 | WIN-002 gethostname→GetComputerNameA | 1207（+6/-0）：p3_output 换 GetComputerNameA | 一致 |
| 1208 | a7945efa | WIN-002 __try→try/catch(/EHa) 等 | 1208（5 文件 +28/-14）：sampler SEH 改写+/EHa、parse_props 出 extern C、补头 | 一致 |
| 1209 | 22344697 | WIN-002 catch(...) 去 _exception_code | 1209（+3/-3）：去 catch(...) 内 _exception_code 取值 | 一致 |
| 1210 | 5cdafc7b | WIN-002 cfitsio unistd.h win_compat shim | 1210（2 文件 +56/-0）：新增 win_compat/unistd.h（+52）+ CMake include 路径 | 一致 |

小计：10 一致 / 0 口径差 / 0 不一致。

### S24<-S23（seq 1256–1265）

后片 H-S24.md 重叠表标"重叠待核"（H-S23.md 当时未落盘）+一句话画像；前片 H-S23.md 主责正文逐条比对：

| seq | sha8 | S24（后片）一句话 | S23（前片主责）一句话 | 判定 |
|---|---|---|---|---|
| 1256 | fd4e11ed | V5 审核包尾：assemble_audit.py 更新 blockers+windows_32r_run_id=4f885b8b8fd4，重打审核包 33 文件（88 PASS 口径） | 1256（M）：同三事，落点 artifacts/prerelease_v5 7 文件+工具 +4/-3，validate 维持 RELEASE_NOT_READY_BLOCKED | 一致 |
| 1257 | 5d9061f4 | WIN-006/007 资源内存门控运行型 PASS：新增 WIN006_007_resource_memory_gate.md（phase2 --resource-detail、workers=16、run 8d2f39e326b7） | 1257（S，新增 +17）：同 run id/采样口径，注明 timeseries 缺采 | 一致 |
| 1258 | 774a7616 | 统一 SHA 状态登记：Linux 版本串 g5d9061f481a4 vs Windows gb842899eb8fb，WIN_SHA_unified 报告新增 | 1258：Linux 重建+doctor、HEAD 漂移判定、"不宣称 release"，报告新增 +21 | 一致 |
| 1259 | 587fe0e3 | 统一 SHA 续：Windows 重建确认版本串 configure 期 git describe 烘焙、build 不刷新；功能统一、SHA 差异 cosmetic；同报告 -21/+7 改写 | 1259：同机理断言与结论，报告改写 +7/-21 | 一致 |
| 1260 | ac2d230d | 新增 tools/pack_audit_package.py（+116）：基线代码+证据 zip（AUDIT_PACKAGE_587fe0e341a7.zip，4.69MB/1695 文件） | 1260：同工具 +116，zip 自述 4.69MB/1695 文件/1698 项校验，zip 本体不在变更面（双方同注） | 一致 |
| 1261 | 4b1b948e | L 级结构模式：BAS-001 导入 CONTROL_V6 46 文件 +3809 + evidence/refactor 4 文件 +458；CONTROL_PASS files=46 tasks=88；ff-push HEAD=ac2d230 | 1261：BAS-001 冻结仓库身份与 V6 控制包基线，50 文件全新增 +4267（46+4），CONTROL_PASS/TASK_GRAPH_PASS，ff-push，"外部修改 205 项" | 一致（3809+458=4267，两口径吻合） |
| 1262 | debeb0a3 | BAS-002 冻结构建目标与生产入口：evidence/refactor 5 文件（BUILD_TARGET_GRAPH.dot/.json、ENTRY_INVENTORY、台账、TASK_RESULT），零代码面 | 1262：同（8 targets/9 entries、nm 符号验证；tasks/BAS-002 4 新增+台账修改） | 一致 |
| 1263 | e4fa8a36 | BAS-003 静态调用图与线程入口清单：5 文件（GLOBAL_LOCKS/SCHEDULER_INVENTORY/SERIAL_HEAVY 等），零代码面 | 1263：同三 CSV（15/5/4 计数、grep 75 命中）+台账 | 一致 |
| 1264 | 8d89088e | BAS-004 P0 所有权与科学语义风险登记 + G0 门禁达成（gates/CHECKLIST、RISKS.csv），零代码面 | 1264：同（风险 7 项、G0 CHECKLIST 7/7、P0-003 NOT_RUN 工具链阻塞） | 一致 |
| 1265 | ecc805d4 | VER-001 唯一版本源 0.10.0-alpha.1：VERSION、cli/CMakeLists、VERSIONING.md、make_linux_release、tests 多处（11 文件 +109/-17） | 1265：同（file(READ VERSION)、9 处同步点、check_version_consistency 自述、TASK_RESULT 挂账） | 一致 |

小计：10 一致 / 0 口径差 / 0 不一致。

### S25<-S24（seq 1311–1320）——已完成核对（跳过）

H-S25.md "二、重叠区核对（逐条：一致／不一致）"已对 10 条逐一判"一致"（对照 H-S24.md 主责条目并逐卡实核，汇总"10 条一致、0 条不一致、0 条待核"；1316 为"一致+追加同源现象"）。本轮登记，不重复裁决。

| seq | sha8 | 判定 |
|---|---|---|
| 1311 | 957accff | 已完成核对（后片逐条判一致） |
| 1312 | ab52854f | 已完成核对 |
| 1313 | 3c66e64e | 已完成核对 |
| 1314 | ab45a63a | 已完成核对 |
| 1315 | b31a3780 | 已完成核对 |
| 1316 | f5e1177a | 已完成核对（后片在同意外追加卡级口径差观察） |
| 1317 | be2d6c0d | 已完成核对 |
| 1318 | a994fe1d | 已完成核对 |
| 1319 | e4275ecc | 已完成核对 |
| 1320 | 0b8117e2 | 已完成核对 |

## 三、总计

- 处理分片对：**12**（S14<-S13 … S25<-S24）
- 核对 seq 总数：**120**（12 对 × 10 条）
  - 其中"已完成核对"（后片报告已逐条判一致，本轮仅登记、不重复裁决）：**30**（S14、S17、S25 三对）
  - 本轮实际逐条裁决：**90**
- 判定分布（本轮 90 条）：**一致 87 ／ 口径差-可兼容 2 ／ 不一致 1**
- 若把 30 条"已完成核对"按其后片原判计入"一致"，则全 120 条口径为：一致 117 ／ 口径差 2 ／ 不一致 1。
- 注①（seq 821，S16<-S15）：b4_todo_head 项数——S15 主责条目列举 5 项（B4-10/11/13/14/15），S16 重叠行记"六项"。以证据卡为准：全卡 cards/0821-f1cf87a6.md 内容摘录列 6 项（含 QA-V19R7-B4-16）。双方对"新增 round03 快照、53/95"的定性判断一致，S15 侧系列举转录遗漏，属可兼容口径差，取 S16"六项"。
- 注②（seq 929，S18<-S17）：总量删行数——S18 记 +299/-30，S17 记"总量 +299/-34；小节 +276/-25"。以证据卡为准：紧凑卡 C0929 与全卡规模行均记 +299/-30（逐文件小节 +276/-25），"-34"在两卡中无对应。双方对"恢复真实 79/10 版、代码自述不在变更面（实际载于 924）"的定性完全一致，属可兼容口径差，取 S18/卡面数字；S17 的 -34 登记为转录误差。

## 四、不一致条目（单列）

### seq 1150 | 163e9e56（分片对 S22<-S21，PAR-001 有界队列与并行合同验证）

- **S21（前片主责，H-S21.md 组 25）原文**：
  > "seq1150 新增 tests/cli/test_parallel_queue.py（+162）：BoundedAsyncQueue 运行时把关的有界队列/backpressure/error/cancel、I/O 与 compute overlap、无全局串行锁；**docs/architecture 新增 1 篇（+19）**。消息自述 254/254。"
- **S22（后片，H-S22.md 二、重叠复核）原文**：
  > "上一片组 25 称'docs/architecture 新增 1 篇（+19）'；本片按 index.jsonl 与全卡逐文件操作位核对，**docs/architecture/ASYNC_IO_CONTRACT.md 是修改（M，19+/0-）而非新增**，本提交的新增文件只有 artifacts/prerelease_v5/PAR-001/LOG.md 与 tests/cli/test_parallel_queue.py（新增 2 修改 2）。其余（测试行数、254/254、台账 PASS）一致。"
- **差异实质**：同一提交内 docs/architecture 落点的文件操作性质（新增 A vs 修改 M）定性相反，属"落点/内容性质"层面的事实分歧，不能用聚合口径解释，故判**不一致**。
- **裁决：以 S22 为准**。依据（均为可核对材料）：
  1. `设计大纲/_evidence/commits/index.jsonl` seq 1150 的逐文件操作位记录为 `["M", "docs/architecture/ASYNC_IO_CONTRACT.md", 19, 0]`，操作 A 仅见于 PAR-001/LOG.md 与 test_parallel_queue.py、操作 M 另见 02_TASK_LEDGER.csv——"新增 2 修改 2"与 S22 所述逐项吻合。
  2. 全卡 cards/1150-163e9e56.md 的 PAR-001 LOG.md 内容摘录第 3 条自述即为"**更新** docs/architecture/ASYNC_IO_CONTRACT.md §10 记录 PAR-001 运行时结论"，且摘录中出现"ASYNC_IO_CONTRACT §3/§9"既有章节引用——被改文件是本提交之前已存在的合同文档。
  3. 该文件的既存在性另有上游旁证：H-S18.md 主责条目 seq 982（0ed8ed95）即"ASYNC_IO_CONTRACT.md 状态置 PASS + 第 9 节生产接入结论"的修改（docs/architecture +22/-10），早于 seq 1150。
- **影响面**：S21 对 1150 的"做了什么"（PAR-001 合同验证：测试+文档+登记、无生产代码）与其余数字（+162/+19、254/254、台账 PASS）均与 S22 一致；不一致仅在该文档"新增/修改"一个定性词。订正取 index.jsonl 操作位（M）。

## 五、附注

- S22<-S21 的 1147/1149/1151 三条，S22 判"一致"同时给出细化（台账前置状态 IN_PROGRESS、胶囊 zip 未入库）——细化属新增发现而非与前片冲突，本轮维持"一致"。
- 各对后片报告"重叠待核"段落中自附的一句话结论与 manifest/紧凑卡全部对得上，未发现任何一侧对"做了什么"的定性互为否定（除上述 1150 一处）。
- 本文件仅新建，未改动任何既有报告与证据文件。


---

# 第二部分：重叠区复核（S26<-S25 … S37<-S36）

> 本部分由另一核对任务追加，覆盖任务指定分片对：S26<-S25、S27<-S26、S28<-S27、S29<-S28、S30<-S29、S31<-S30、S32<-S31、S33<-S32、S34<-S33、S35<-S34、S36<-S35、S37<-S36。上文第一部分（S14..S25 区段）不改动。
> 判定标准与第一部分一致：一致＝双方对"做了什么/落点/内容性质"实质相同（数字引用差异能在同一张证据卡找到出处的，判一致并加口径注记）；口径差-可兼容＝实质不冲突但结论表述层（用词/标签/粒度）不同；不一致＝实质冲突。

## 总计（本部分）

| 项 | 数 |
|---|---|
| 处理分片对 | 12（其中 11 对新核对，1 对〔S35<-S34〕登记"已完成核对"） |
| 本次逐条核对 seq 数 | 110 |
| 一致 | 106 |
| 口径差-可兼容 | 4（seq 1371、1427、1429、1922） |
| 不一致 | 0 |
| 已完成核对对所含 seq（不计入 110） | 10（H-S35 原判 10/10 一致） |

通用口径注记：证据卡自带两种行数口径——"变更面（按目录聚合）"行与"每文件变更行数"行对含新增文件的目录常给出不同数字（例：卡 1375 聚合行 tests/unit 1 文件 +209 -12 vs 每文件行 +202 -12；卡 1538 聚合 +517 vs 每文件 +437）。两侧报告各引同卡不同行时数字不等但均有出处，不构成冲突。

## 一、S26<-S25（重叠 seq 1366..1375，主责属 H-S25）

后片段落：H-S26 "重叠区核对（seq 1366..1375）"表（当时称 H-S25.md 不存在，10 条登记"重叠待核"并附本片侧客观事实）。

| seq | S26 侧一句话 | S25 侧一句话 | 判定 |
|---|---|---|---|
| 1366 | GOV-002：代码面仅 validate_task_ledger.py(+42/-18)，台账 GOV-002→PASS，自检用例名含非法状态/双 IN_PROGRESS/依赖先过后消失/环/缺任务/任务数不符 | GOV-002：修复 gate 与依赖状态机，同函数同规模(+42/-18)，全部错误聚合+任务数校验，PLAN 七类负面 fixture，台账→PASS | 一致 |
| 1367 | VER-001：L 级 28 文件版本推进：VERSION+1/-1、docs/VERSIONING.md、docs/review 5 文件、check_version_consistency.py+14/-6（PRERELEASE_BAD/EXEMPT/SKIP_DIRS）、p3_session.cpp+7/-4、新增 include version_generated.h | VER-001：版本推进 0.10.0-alpha.2 落实 VERSION 单源，合计 28 文件 +278/-39，生产/测试/工具/文档/台账五面混合 | 一致 |
| 1368 | CHK-001：新增 tools/quality/check_prod_reachability.py 229 行；包面产出 PROD_REACHABILITY.dot/.json | CHK-001：生产可达调用图检查器（compile_commands+nm），+201/+229，JSON/DOT 图入 evidence 7 文件 +1451 | 一致 |
| 1369 | CHK-002：新增 check_pipeline_graph.py 283 行；台账依赖 CHK-001；判定列"node edge port artifact version backend worker 全比" | CHK-002：非空静态图/运行图检查器，--ir/--module-index/--trace 必填、7 负例，比较字段逐项列出 | 一致 |
| 1370 | CHK-003：新增 check_serial_heavy.py 161 行；台账依赖 CHK-002；验收列 reachability/compile defs/gate callers | CHK-003：serial-heavy 与资源接线检查器（+135/+161），allowlist 登记 | 一致 |
| 1371 | 编号列"（无编号）"：仅新增 evidence/v6_1_rework/gates/G1/CHECKLIST.md（7 项全 PASS）；另记文档内"创建时间"与提交时刻约 27 分钟偏差 | 标题记 G1-STATUS（V6.1 门）：登记 V6.1 的 G1 门 7/7 PASS，单文件纯登记提交，七项判据逐项回指 seq 1365–1370 | 口径差-可兼容（叫法口径：消息无任务号，S26 记"无编号"，S25 以门标签 G1-STATUS 命名；做了什么/落点/性质三项判断完全相同，S26 的时间偏差系补充观察、与 S25 无冲突） |
| 1372 | RT-001：13 文件：context.h/runtime.h/context.cpp/runtime.cpp＋新增 tests/unit/rt001_abi_test.cpp，docs/contracts+56 | RT-001：Runtime 与模块公共合同冻结：runtime.h+83、context.h+69/-2、RT-001.md+38、rt001_abi_test+100、CMake 纳入 astrocs_core | 一致 |
| 1373 | RT-002：context.h/.cpp 改动＋新增 rt002_budget_test.cpp（+171） | RT-002：acquire 增策略参数、_make_lease、RAII 归还；rt002_budget_test.cpp(+152) | 一致（注：+171＝卡目录聚合行"tests/unit 2 文件"，+152＝卡每文件行；同卡双口径，各有所本） |
| 1374 | RT-003：context/scheduler 改动＋新增 rt003_context_test.cpp、改 core_context_test.cpp；日志 c01..c03 | RT-003：RunContext 无锁改带锁、get_artifact 快照化（唯一 producer）、scheduler.cpp+4/-2 竞争修复、两测试 | 一致 |
| 1375 | RT-004：pipeline.h/.cpp 大改（lib/core +248/-180）＋core_pipeline_test.cpp(+209/-12) | RT-004：删手写 JSON 字符串解析（json_trim/top_key_value 等）改用 nlohmann::json；pipeline.h+23/-14、pipeline.cpp+233/-176、测试+202/-12 | 一致（注：+248/+209 引卡目录聚合行，+233/+202 引卡每文件行，卡自身双口径） |

小计：一致 9、口径差 1、不一致 0。

## 二、S27<-S26（重叠 seq 1421..1430，主责属 H-S26）

后片段落：H-S27 "三、重叠区条目"（当时称 H-S26.md 不存在，10 条"重叠待核"＋事实基线）。

| seq | S27 侧一句话 | S26 侧一句话 | 判定 |
|---|---|---|---|
| 1421 | P3-004 独立球面重采样 Oracle（常数/解析场 cos²dec vs 独立 TAN reference；RA wrap；旋转 TAN；无人工接缝）。落点 tests/backend 3 文件 +245/-2 + evidence 4 | P3-004 同主题，7 文件 +410/-3（test_p3004_spherical_oracle.py +186、phase2_fixture_main.cpp+36/-1、seam +2/-1，evidence/tasks/P3-004） | 一致（注：+245＝卡目录聚合行；卡每文件行合计 224；总数 410＝245+165 与卡聚合吻合——两侧各引卡不同行） |
| 1422 | P3-004 COMMITS.csv 绑定 41 任务（单文件 +1） | P3-004 绑定提交，COMMITS.csv +1 | 一致 |
| 1423 | P3-005 FITS 输出校验与 provenance（bitpix -32/-64 决定 buffer；关键字/coverage/provenance）。落点 lib/phase3_session 3 + tests/backend 2 + tests/unit 2 + evidence 4 | P3-005 同主题（原硬编码 -32 改正），11 文件 +263/-6 同一文件集合 | 一致 |
| 1424 | P3-005 COMMITS.csv 绑定 42 任务 | 绑定 42 任务，+1 | 一致 |
| 1425 | P3-006 Phase3 生产 Pipeline 与资源门（IR 5 节点链 properties→wcs→resample2→writer→verify；≥10s）。落点 runtime_client +32/-13、module_adapters +113、tests 4、evidence 4 | P3-006 同 IR 5 节点链，10 文件 +415/-17 同落点（五 descriptor 名与节点/端口常量成对可证） | 一致 |
| 1426 | P3-006 COMMITS.csv 绑定 43 任务 | 绑定 43 任务，+1 | 一致 |
| 1427 | "DOC-001 重建合同 front matter 与完整图（36 合同图/生产可达/源索引/注释卫生）。落点全在 evidence/v6_1_rework（四检查日志）"（沿用消息"重建"措辞） | DOC-001 判"复验与留证，不是重建"：7 文件 +188/-1 全在 evidence、代码面 0；PLAN 自述"本任务复验当前 SHA"；四日志与 S27 所记同名 | 口径差-可兼容（S27 段导语自限"仅登记事实基线、不作一致性判断"，"重建"系消息首行转述〔manifest S27 该行消息含"DOC-001: 重建…"〕；两片的落点登记逐字相同。实质无冲突；定性以可证的 S26"复验"为准，S27 未下相反结论） |
| 1428 | DOC-001 COMMITS.csv 绑定 44 任务 | 绑定 44 任务，+1 | 一致 |
| 1429 | "DOC-002 重建当前版本追溯矩阵（63 行追溯/66 claims/18 DATA artifact）。落点全在 evidence/v6_1_rework（三日志）"（沿用消息） | DOC-002 复验并归档证据：6 文件 +160/-1 全在 evidence；三日志同名；63/66/18 两侧同引消息/PLAN（S26 另标"未证实"） | 口径差-可兼容（同 1427 理由） |
| 1430 | DOC-002 COMMITS.csv 绑定 45 任务 | 绑定 45 任务，+1（S26 末条） | 一致 |

小计：一致 8、口径差 2、不一致 0。

## 三、S28<-S27（重叠 seq 1476..1485，主责属 H-S27）

后片段落：H-S28 "重叠复核条目"表（当时称 H-S27.md 不存在，10 条"重叠待核"，仅登编号＋消息首行摘）。

| seq | S28 侧一句话 | S27 侧一句话 | 判定 |
|---|---|---|---|
| 1476 | LOG-001 "feat(logging): 统一结构化日志接口" | LOG-001 冻结统一结构化日志合同：schema＋参考实现＋合同文档＋checker＋测试；明确不接生产 logger | 一致（S28 仅引消息首行，无与 S27 细结论抵触处） |
| 1477 | ARC-001 "docs(architecture): 冻结DLL产品边界" | ARC-001 DLL 边界冻结为机器可校验合同，落点仅 contracts/config 单 schema（文档正文按 note 留控制包区） | 一致（消息 docs 前缀与实际落点的差异系 S27 自行登记的"消息 vs 变更面"问题，非两报告冲突） |
| 1478 | BLD-001 "build(msvc): 冻结Windows发布工具链" | BLD-001 冻结工具链：cmake/toolchain、CMakePresets、packaging/windows+schemas、DEPENDENCIES.md | 一致 |
| 1479 | DATA-001 "feat(data): 冻结类型化产物合同" | DATA-001 artifact manifest schema＋C ABI 头/实现＋校验器＋负例测试 7 文件 | 一致 |
| 1480 | GOV-002（附带 LOG-001）"归档非当前工程文档"：L；885 文件、改名 881、engineering/control 855 文件 | GOV-002 批量归档搬家：改名 881＋新增 3＋修改 1，DOCUMENT_INDEX.yaml＋doccheck 门；编号字段含 LOG-001（归档时补登） | 一致（885/881/855 计数两侧同卡同源） |
| 1481 | ABI-001 "feat(abi): 定义模块C ABI v1" | ABI-001 include/astrocs/abi 四纯 C 头 +575，C11/C++17 双可编译、opaque handle、禁 STL/异常 | 一致 |
| 1482 | GOV-003 "build(version): 统一Alpha版本事实源" | GOV-003 VERSION→0.11.0-alpha.1、VERSION_NAMESPACES.md＋check_version_namespaces.py、REVIEW.md 同步 | 一致 |
| 1483 | RT-001 "feat(runtime): 实现类型化运行图" | RT-001 typed_dag.py+624 与 schema/合同头/registry/fixture＋正反测试（V7 期 RT-001，与 V6.1 期 1372 的 RT-001 同号异题；两侧均按各自片内语境处理，无混淆） | 一致 |
| 1484 | DATA-002 "feat(data): 定义三阶段产品交换合同" | DATA-002 phase_product_exchange schema＋矩阵＋4 示例＋校验器＋测试 9 文件 | 一致 |
| 1485 | BLD-002 "feat(build): 建立唯一根 CMake 构建图"（S；3 文件 +34/-3） | BLD-002 唯一 project/唯一 add_executable：根+9/-1、cli+10/-2、phase2+15/-0＝+34/-3 | 一致 |

小计：一致 10、口径差 0、不一致 0。

## 四、S29<-S28（重叠 seq 1531..1540，主责属 H-S28）

后片段落：H-S29 "二、重叠复核"逐条＋"五、重叠区核对"表（当时称 H-S28.md 不存在，10 条"重叠待核"）。

| seq | S29 侧一句话 | S28 侧一句话 | 判定 |
|---|---|---|---|
| 1531 | V8-CI-004 分发与写租约（evidence 2 文件 +30/-4：WRITE_LEASE.json、V8-CI-004.json） | 同（WRITE_LEASE.json＋新增 dispatch/V8-CI-004.json） | 一致 |
| 1532 | V8-CI-004 changed path→checks 映射：impact_map.json +58/-15、新增 test_impact_map.py +297、evidence 11 文件 fast 实测（+3238/-17） | 同主题；分域 impact_map+58/-15、ci/tests+297、evidence 11(+2883)；57 项 41 PASS/16 FAIL、16 项固有红移交 G-FIX | 一致（注：S29 括注 +3238/-17 为卡三域聚合合计〔2883+297+58／2+0+15〕，括注位置易误读为 evidence 单域数，但与 S28 分域行不冲突） |
| 1533 | V8-CI-005 分发与写租约（2 文件 +31/-3） | 同 | 一致 |
| 1534 | V8-CI-005 linux-deep 检查注册与 deep 构建驱动链：checks.json +291、checks.schema.json、resource_monitor.py、run.py、CMakeLists+61、tools/quality 3 文件、deep_ci_driver.py 等代码面 11 文件＋evidence 15 | 同主题 28 文件按注册表/驱动/构建/文档/证据五集合归纳（ci 4 改、tools/quality 3 新、CMake+cmake、evidence 15(+2975)） | 一致（卡证实 checks.json +291 与 ci/resource_monitor.py〔V8-CI-003 补登 shim〕均在本提交变更面；S28 未点名属粒度差，计数相容） |
| 1535 | V8-CI-006 分发与写租约（2 文件 +46/-3） | 同 | 一致 |
| 1536 | V8-CI-006 Windows 构建驱动与 candidate 打包链：ci_windows_driver.py、tools/quality +755、checks.json +145、tests 2、evidence 14 | 同：WIN-* 三项 77→80、driver+755 五阶段、tests、evidence 14(+5490) | 一致 |
| 1537 | V8-CI-007 分发与写租约（2 文件 +47/-3） | 同 | 一致 |
| 1538 | V8-CI-007 双平台 workflow、action 锁与 CI 辅助链：workflows 2、actions.lock.json、bootstrap.py +339、select_profile/validate_candidate、tests +517，代码面 8 文件 | 同主题 19 文件：ci-linux/ci-windows、actions.lock＋verifier、三辅助脚本、test_workflow_lock.py +38 用例（共 151 OK） | 一致（注：S29 的 +517/+339＝卡目录聚合行行数，S28 的 +38＝消息自述用例数——两问不同、均有卡面出处） |
| 1539 | V8-CI-008 分发与写租约（2 文件 +45/-3） | 同 | 一致 |
| 1540 | V8-CI-008 Fatduck 三 job workflow 与候选选择器：select_candidate.py +403、test_fatduck_workflow.py +539、workflow +190 | 同：fatduck.yml+190 三 job、select_candidate.py+403（+29 离线单测共 180 OK）、四档回归 57/71/7/61 不变 | 一致（+539＝卡 ci/tests 聚合行；用例数与行数两问兼容。S29 另以本片 1542"补登 V8-CI-008 CLOSED(9b3fe93e)"与 S28 交叉印证，成立） |

小计：一致 10、口径差 0、不一致 0。

## 五、S30<-S29（重叠 seq 1586..1595，主责属 H-S29）

后片段落：H-S30 "三、重叠复核条目"（当时称 H-S29.md 不存在，10 条"待核"＋基线）。

| seq | S30 侧一句话 | S29 侧一句话 | 判定 |
|---|---|---|---|
| 1586 | conformance 探针模块版本单源 alpha.1→alpha.2＋registry/bootstrap_utf8 断言动态化（11 文件 +31/-12） | 同，含 mutation 红→绿自证；落点 modules/conformance 7、ci/tests、contracts/config、tests/abi、台账 | 一致 |
| 1587 | Windows ctest 7 失败根因闭环＋monitor Win32 采样＋driver 全量日志 tee（13 文件） | 同（C1189 c11atomics、libz 变量化、/tmp 回退、io_adapter rename；13 文件清单一致） | 一致 |
| 1588 | wave4 三项：UT-API 断言同步／UT-IO numpy 环境依赖／UT-CPU-AVX512 fortified memset（4 文件） | 同三项同落点（tests/api、ci-linux.yml、avx512_provider.cpp、台账） | 一致 |
| 1589 | PRODUCTION_EXECUTION_INVENTORY.csv 重新生成同步（负责人确认；docs/architecture +35/-8） | 同（git archive 逐字节一致实验、test_05 定性"csv 与源树同步门"） | 一致 |
| 1590 | Windows 两修：MSVC C2065 EDQUOT＋monitor.h Win32 采样 API 修正（monitor.h +31/-7、fits_core.c +7） | 同（EDQUOT 宏守卫；Thread32/GetProcessIoCounters 替换） | 一致 |
| 1591 | MON-001 复验 PASS＋恢复 ResourceRecorder 生产接线（commands.cpp +25/-1、evidence +73） | 同（考古定位 CLI-002 迁移删接线根因、恢复 record/set_stage/write_all） | 一致 |
| 1592 | MON-002 复验：first-10s gate 接线＋阈值补齐＋豁免矛盾修正（resource_gate.h 等 6 代码文件、tests/unit +98/-7） | 同（8 文件 +379/-24、mon002_gate_test +98/-7、RESOURCE(10)/CANCELLED、旧测试反转） | 一致 |
| 1593 | QA-001..005 复验全 PASS——3 个 checker 环境适配（duplication/reproducible_build/warning_suppression＋evidence 5） | 同（构建目录/nm 路径/VERSION 单源三处适配；9 文件 +520/-316） | 一致 |
| 1594 | CPU-006/007/008＋LNX-003/004/005 复验 reverify 全 PASS（仅 evidence/refactor 6 文件＋台账） | 同（六项零修复、reverify 锚 observed_head=f0bc039a；7 文件 +160/-12；纯证据追加） | 一致 |
| 1595 | Windows 残余 3 ctest：ACP 第二层编码墙＋/tmp 遗留＋avx512 诊断（tests/unit 3 文件 +66/-7） | 同（utf8_path 助手、mon001 TMPDIR 回退链、cpu001 定性实现无 bug；"1596 之父"衔接与 S29 父指针核验一致） | 一致 |

小计：一致 10、口径差 0、不一致 0。

## 六、S31<-S30（重叠 seq 1641..1650，主责属 H-S30）

后片段落：H-S31 "片尾二"表（当时称 H-S30.md 不存在，10 条"重叠待核"＋本片独立取证结论）。前片为单条成组条目（1641–1650"台账补登追赶环收束组"：每提交仅改 COMMIT_LEDGER.jsonl 各 +1 行，被补 sha 逐一点名）。

| seq | S31 侧一句话 | S30 侧一句话（组内该行） | 判定 |
|---|---|---|---|
| 1641 | 台账补登 48e2f75f，单文件 COMMIT_LEDGER.jsonl +1/-0；性质＝控制包或台账 | 1641 补登 48e2f75f，+1 行 | 一致 |
| 1642 | 补登 21b325a6；被补对象即 1640（fix(core) P0 budget 注入链，属 S30 主责） | 1642 补登 21b325a6 | 一致 |
| 1643 | "缺登全量补齐"自述与变更面 1 行规模不符（自述强于变更面） | 同记张力："紧凑卡计 +1 行，与'全量'字样存在张力，未证实" | 一致 |
| 1644 | 补登 ed787ed6，+1 | 同 | 一致 |
| 1645 | 补登 9f031197，+1 | 同 | 一致 |
| 1646 | 补登 cb548e24＋守则固化（S31 注：本条无规范/工具落地文件，规则实体迟至 1668） | 补登 cb548e24 并固化守则（commit 前 append HEAD，杜绝追赶循环） | 一致 |
| 1647 | 补登 a3bf767b，"终收敛"自述其后 4 条仍追加（S31 判不成立） | 补登 a3bf767b（称"终收敛"） | 一致 |
| 1648 | 补登 ce8d4a5c，"稳态恢复：尾行=HEAD-1、HEAD 自参照豁免"（豁免规则实体见 1668） | 同型补登＋同口径字面量 | 一致 |
| 1649 | 补登 64902198，missing=={HEAD} 单元素豁免态（S31 注其登记行在 1655 被删、1656 重加） | 同型补登，注"收束后稳定形态允许 HEAD 自身一行在途" | 一致 |
| 1650 | 补登 f2e439a6，"豁免态（终）"（S31 注 35 秒后 1651 仍全量对齐，循环未由补登终结） | 同型补登＋稳定态说明；机制性收口寄于 1646 守则声明 | 一致 |

小计：一致 10、口径差 0、不一致 0。两侧同判"十连机械补登、收敛自述存疑"，互相印证。

## 七、S32<-S31（重叠 seq 1696..1705，主责属 H-S31）

后片段落：H-S32 "重叠区核对"表（当时称 H-S31.md 不存在，10 条"重叠待核"）。

| seq | S32 侧一句话 | S31 侧一句话 | 判定 |
|---|---|---|---|
| 1696 | R17 Windows 残余：p3_output/p3_assembly 测试目录 generic_string 化（TEMP 反斜杠被 path_is_safe 拒），tests/unit 2 文件 +24/-8 | 同（两测试文件 +10/-3、+14/-5；"lib 层 path_is_safe 语义不动"红线一致） | 一致 |
| 1697 | R18：fsync 前只读 open 在 MSVC _commit(FlushFileBuffers) 下必败，p3_output.cpp +7/-0 | 同（_WIN32 改 O_RDWR、POSIX 不变；引用 1696 诊断 status=2 errno=9） | 一致 |
| 1698 | P2-SESSION-DOC 装配层合同冻结（控制包 17/140，DISP-001..008 登记，12 文件 +1109/-3） | 同（PHASE2_SESSION.md+registry+三件套+trace 追加；17/140；DISP 登记；删除量仅 -3） | 一致 |
| 1699 | P2-SESSION-DOC 台账 CLOSED 同步（source_of_status=fg_verified_commit 而非 fg_machine_verified，锚 722b5737） | 同（CLOSED 80→81、closed_commit=722b5737、fg_verified_commit） | 一致（S32 重点回核项 (a) 之 fg_verified_commit 一侧与 S31 相符） |
| 1700 | R19 Linux 残余 3 项（SAMP-DOC 索引同步/执行台账 regen/oracle fixture version_generated.h），落点 workflow+docs/architecture | 同 3 项；落点 workflow 1+docs/architecture 1；登记"第 3 项索引同步在变更面为 0 文件——未证实" | 一致（S32 把"3 项"与"2 文件落点"分列，未主张第 3 项有文件落点，与 S31 缺口观察兼容；重点回核项 (c) 的 18/140→19/20/21 连号在 S31 侧亦成立） |
| 1701 | p2-batchO tools 质量面 R13：strip 族原子写（SIGKILL 验证 SHA256 不变）/zip-slip 预验证/serial_heavy UnboundLocal/检查器失效族 8 项，16 文件 +585/-147 | 同（strip 三件+zip-slip+UnboundLocal+失效族；16 文件 +585/-147；"非恒 PASS/非恒 FAIL/非崩溃"三验） | 一致 |
| 1702 | fatduck-admin 运维通道新建（workflow_dispatch 带外恢复通道） | 同（fatduck-admin.yml +22，带外管理入口＋用途约束注释） | 一致 |
| 1703 | fatduck-admin step 改 shell:cmd（服务账户 PATH 无用户级 pwsh，自托管首曝） | 同（pwsh→cmd 单行 +1/-1；S31 另注 1702 注释未订正） | 一致 |
| 1704 | P2-UPM-DOC UPM 域冻结合同（控制包 18/140，ALG-UPM-001 原位修订 std::thread 实测锚，DISP-P2UPM-001..004，15 文件 +1896/-68） | 同（原位修订＋实现级合同＋registry 手写页；18/140；五门 ALL PASS 自述） | 一致 |
| 1705 | P2-UPM-DOC 台账 CLOSED 同步（fg_machine_verified，锚 f506ef7f；其 CSV 行欠账由本片 1706/1707 补） | 同型差异登记（只改 1 文件 TASK_STATE.json、无 COMMIT_LEDGER/STATE_RECONCILIATION；fg_machine_verified 为 1668 换轨后首例） | 一致（"CSV 欠账由后片补"系 S32 后见之明，与 S31"是否漏登未证实"互补不冲突） |

小计：一致 10、口径差 0、不一致 0。S32 自列三处重点回核项均经对照证实。

## 八、S33<-S32（重叠 seq 1751..1760，主责属 H-S32）

后片段落：H-S33 "重叠复核条目"表（当时称 H-S32.md 不存在，10 条"重叠待核"）。

| seq | S33 侧一句话 | S32 侧一句话 | 判定 |
|---|---|---|---|
| 1751 | P1-DRZ-TEST 台账 CLOSED 同步（commit 902d2e20），evidence 2 文件 +7/-7 | 组内"1751 两文件 +7/-7"（drizzle 验证面 p1drz 的收口对） | 一致 |
| 1752 | CAT-GAIA-INT：gaia 集成 descriptor+typed ports+调用计数；lib/gaia_xpsd_client 2 +132/-2、tests/unit 2 +861、evidence 1 | 同（integration json+module.yaml +132/-2、gaia_integration_test.c +861；101 断言、FIPS180-4 实测、5 文件 +995/-2） | 一致 |
| 1753 | CAT-GAIA-INT 台账 CLOSED 同步（commit 054af3a3），2 文件 | 同组成对收口；94074a32 又被 1755 作 closed_commit 引用 | 一致 |
| 1754 | MON-002 内存增长检测：memory_report.h 双探针（mallinfo2+smaps_rollup）、Theil-Sen 全曲线斜率、reclaim 三分类、validate_alloc_report 复读重算（+487；resource_gate +47/-1；commands +48/-2；tests +337） | 同，数字逐项相同（peak 不参与判定、伪造必败、哨兵语义同构 p2007） | 一致 |
| 1755 | MON-002 COMMITS.csv 回填＋台账 CLOSED（消息锚 commit 94074a32），evidence 2 文件 +5/-5；与本片 1778 同为"台账滞后回填"型 | 同条并登记"消息锚（94074a32=1753，feat 前 base）与台账行内锚（acceab68）并存漂移——同 CPU 组模式" | 一致（S32 记锚点漂移、S33 记滞后回填形态，所指同一现象，互补不冲突） |
| 1756 | P1-DRZ-IMPL：drizzle 迁移独立模块 lib/drizzle 九操作 C ABI v1（DRZ-001）；5 文件 +1588、tests 2 +520、根 CMake +8/-1、evidence 1 | 同（module_entry vtable+租约+取消检查点、version-script 唯一导出、dlsym 三探针 NULL 实证） | 一致 |
| 1757 | P1-DRZ-IMPL 台账 CLOSED 同步（commit 2c065ace），evidence 2 文件 | "1757 为 evidence 2 文件 +7/-7" | 一致 |
| 1758 | P1-CAL-IMPL：calibration 迁移独立模块 astrocs_p1_calibration SHARED（PIC/C++17/OpenMP）legacy 4TU＋module_entry 1490 行九操作（CAL-004）；7 文件 +1733/-7、tests +819 | 同（导出面净化 nm 唯一/dlsym 12 NULL、direct-vs-plugin 10/10 BITWISE、T-1 同口径） | 一致 |
| 1759 | P1-CAL-IMPL 台账 CLOSED 同步（commit adf820ac），evidence 2 文件 | "1759 为 evidence 2 文件 +7/-7" | 一致 |
| 1760 | P1-CAL-INT：calibration 集成 descriptor+typed ports+调用计数（10-op 词表/5 端口/DEV-2 可选 NULL 恒等；RT-006/RT-05）；lib/calibration 2 +181、tests 2 +999、evidence 1 | 同（integration json+module.yaml +181/-0、calibration_integration_test +999、113 断言；与 1752 同模板第二实例） | 一致 |

小计：一致 10、口径差 0、不一致 0。

## 九、S34<-S33（重叠 seq 1806..1815，主责属 H-S33）

后片段落：H-S34 "重叠区核对"（当时经 glob 称 H-S33.md 不存在，10 条"重叠待核"，每条仅标题级一句话＋衔接线索）。

| seq | S34 侧一句话 | S33 侧一句话 | 判定 |
|---|---|---|---|
| 1806 | P3-001 投影 registry 与四投影冻结实现 | registry v1 冻结 TAN/SIN/CAR/AIT 恰四行＋统一操作面 make/pix2world/world2pix/fits_keywords，lib/phase3_proj 5 文件 +608/-8 | 一致 |
| 1807 | P3-002 不确定度传播与扩展 HDU 科学面 | 科学面：p3_uncertainty_open（variance 优先/次 ivar、皆无=NONE 显式、非法=产品损坏拒绝）＋_ex 采样接口 | 一致 |
| 1808 | P3-002 五子节点唯一 operation 委托 | 委托面：module_adapters.cpp +779/-7＋tests；与 1807 分工"科学面/委托面分离"变更面可证 | 一致（"五子节点"为消息口径，S33 未计数亦未否认） |
| 1809 | RT-001 唯一 Executor 与实测资源门 | 唯一 executor 生产接入（基线实证 executor.cpp 未编入生产 target、acquire(1,1) 恰租 1 槽）＋run_monitored/test_frozen_gate 门求值 | 一致 |
| 1810 | MOD-001 科学 DLL 安装加载验证与产品清单 | aio_abi 编入生产 target（F-AIO-003 闭环）＋install_layout＋六模块 SHARED＋产品清单 JSON | 一致 |
| 1811 | CLI-001 薄 CLI validate/plan/inspect 命令面 | kRules 登记 9 条命令＋cmd_phase_validate/plan/inspect（零 Runtime 零执行）＋golden/协议测试 | 一致 |
| 1812 | CI-001 收紧 CI 注册表与资源 fail-closed | WIN-* waivable→false、监控必须调用 evaluate、负向实测 rc=1；移交 F-CI-001-01 | 一致 |
| 1813 | F-CI-001-01 WIN-* 移出资源门（owner 裁决） | owner 裁决回调旗标（构建/打包/单测非重计算面，§10.5 冻结语义针对重计算区间），维持 waivable=false、回退仅限旗标 | 一致（S34 另记本片 1816 以该口径为矛盾源，属后片衔接线索，不与 S33 冲突） |
| 1814 | F-CI-002-01..03 Linux 构建断链与 TASK-RESULT-SCHEMA 修复 | 根 CMake 移除 add_subdirectory(lib/snr_estimator) 断链引用、RPATH 摘 p1_noise、schema 校验器口径（3 文件 +46/-14） | 一致 |
| 1815 | REAL-000 真实数据审计索引 v1.2 | testdata 944 文件逐文件 sha256 对账＋match_plan.py＋索引 v1.2 落 testdata；"为 1816+ REAL-001 提供输入" | 一致（S34"1816 为直接下游"与 S33 同判） |

小计：一致 10、口径差 0、不一致 0。

## 十、S35<-S34（重叠 seq 1861..1870）——已完成核对（登记，不重复比对）

H-S35 落盘时上一片 H-S34.md 已存在（437 行，主责含 1861..1870），其"重叠区核对（seq 1861..1870，对比上一片 H-S34.md）"一节已逐条判定 1861–1870 十条全部"一致"。H-S35 原文汇总："重叠区 10 条与 H-S34 全部主题、编号、性质、变更面判定一致；仅在 1861/1862 存在'_merge 增量数字/合并组落点粒度'层面的转述差异，经复核为多笔合计与单卡数字的正常差异，不构成结论级不一致。"按任务规则跳过重复比对，登记为已完成核对（10 seq：一致 10／口径差 0／不一致 0；两条转述差异的归类沿用 H-S35 原判，本次抽查未发现与 H-S34 证据相抵触之处）。

## 十一、S36<-S35（重叠 seq 1916..1925，主责属 H-S35）

后片段落：H-S36 "2. 重叠复核区"表（当时称 H-S35.md 不存在，10 条"重叠待核"）。前片 H-S35 按 RQS 主题分组组织（组条目内逐 seq 点名），比对取组内该 seq 专属句。

| seq | S36 侧一句话 | S35 侧一句话 | 判定 |
|---|---|---|---|
| 1916 | docs(audit/RQS)：问题扫描/INDEX.md 单文件 +9/-9，域名标签去重 MM4→M4 | RQS-G 组："1916（bed82382）INDEX 域名标签去重（MM4→M4）"，零代码面汇总层迭代 | 一致 |
| 1917 | RQS：F00-02 部分撤回（自述第三次同类错锚），_merge 2 文件＋_cache 1 文件 | RQS-E 组："1917（fc4ef16a）F00-02 部分撤回（tests/abi 下无 test_io_ownership_contract.py、错行号，第三次同类）"＋披露"limit:1 局部读被整文件写回致台账截断 160 行"事故与禁令 | 一致 |
| 1918 | RQS：簇1 补记 io_ownership 在册但门永不红；SUMMARY.md＋_merge | RQS-G 组："1918（7add91d6）簇1 补记 io_ownership 确已注册进基线（在册的门永不会红，C-02 三向门须加第四向）" | 一致 |
| 1919 | RQS：F00 全量机械自审，path::符号逐条回读真源；_merge 单文件 | RQS-E 组："1919（32b38b4b）F00 全量机械自审（逐条回读 path::符号 真源）" | 一致 |
| 1920 | RQS：F00-11 影子树 agent 指令文件与冻结宪章唯一入口冲突；_cache 单文件 +49 | RQS-K 组：1920 登记 run/reaudit_v3/run001/A/AGENTS.md 与宪章 §3.1/§8.1/§1.1 正面冲突、且位于 run/** 免报区对 CI 文档门永久不可见 | 一致 |
| 1921 | 同上主题续＋C-10/A-34：40_OWNER_DECISIONS.md＋_cache＋_merge | 1921 补充全仓清点（18 份 AGENTS.md 仅根 1 份 TRACKED、长度四种并存）＋新增 C-10 门与 A-34 请裁 | 一致 |
| 1922 | RQS：L28b 交档，27 轴＋14 域收工，INDEX v3；findings/40_OWNER/INDEX/_cache/_merge 共 7 文件 | 无独立条目；主题归纳第 2 条将 1922 登记入"RQS 收档与汇总层"线："INDEX/SUMMARY/40_OWNER_DECISIONS 的迭代，十域→十四域→27 轴全部收工里程碑" | 口径差-可兼容（条目粒度差：S35 以主题线覆盖 1922、未展开其文件构成，S36 逐条登记；两侧对 1922 的性质归类（RQS 收档/汇总层）与"27 轴收工＋14 域＋INDEX 迭代"事实相合，无事实冲突。取更细的 S36 逐条形为该 seq 登记基线，S35 未给出可抵触断言） |
| 1923 | fix(BACKEND/FD-08)：UT-BACKEND 13 项预存真红清零＋resume 误报修复；tests/backend 3＋cli/commands.cpp＋cli/monitor.h＋lib/core 1 | BACKEND/FD-08 组：1923 按根因分类清 13 项（p3006 resume 预检误报 rc=8、seam 夹具补 VARIANCE/IVAR、观测链恢复、monitor 哨兵）；同落点 | 一致 |
| 1924 | test(BACKEND/FD-08b)：p2007 10s 锚 workload 自标定；tests/backend 3 文件 +137/-40 | 组内"1924 三文件 tests/backend"、FD-08→FD-08b、1923 修复后 CI 复跑暴露的口径漂移补丁（p2007 在其共用夹具清单内） | 一致 |
| 1925 | RQS：负责人两裁定落地（B-14 授权入规程 10_PROTOCOL.md＋A-34 仅登记不改名）＋findings 新档 | RQS-J 组："1925（cb4405d8）负责人两裁定落地——B-14 授权（规程 §8 E 层边界：只读 ls-files/nm/objdump/ctest -N＋python 纯文本分析，输出限 问题扫描/** 与 run/审计执行层/**）、A-34 不改名仅登记（定稿 FD-G-001）"；落点含 10_PROTOCOL.md | 一致 |

小计：一致 9、口径差 1、不一致 0。

## 十二、S37<-S36（重叠 seq 1971..1980，主责属 H-S36）

后片段落：H-S37 "重叠复核条目"表（当时称 H-S35/H-S36 不存在，10 条"重叠待核"，附时间/父/规模锚点）。

| seq | S37 侧一句话 | S36 侧一句话 | 判定 |
|---|---|---|---|
| 1971 | fix(star_detector)：朝向角归一化无限循环致生产挂死（P11）；4 文件 +233/-7 | P11 件 1：sdet LM alpha=±inf 时 while(|angle|>90) 永不终止、timeout 900→rc=124 实测；新增 sdet_angle_guard.h 有界归一化＋fail-closed；同 4 文件 | 一致 |
| 1972 | perf(ipv)：triangle_match 枚举加搜索预算上界＋fail-closed 原因（P11）；5 文件 +200/-6 | P11 件 2：lib/plate_solve 五文件（ipv_triangle/ipv_solver/test_triangle_budget/CMake）；与 1971 同秒成批落链 | 一致 |
| 1973 | docs(audit/RQS)：V2 片1 立条 V2-N-01 P0（IpvParams ctypes 镜像未同步 72 字节越界写）；8 文件 +269/-21 | V2 片1：V2-N-01 P0（72 字节越界写、无 struct_size 版本位、零注册门不可见）＋V2-N-02；_verify/V2.md＋findings＋账本三视图 | 一致 |
| 1974 | docs(audit/RQS)：V2 片2 立 P1 两条 V2-N-03/N-04；4 文件 +24/-1 | 同（V2-N-03 算法文档把被修错口径钉成规范 RMSE、V2-N-04 参考星门只做库层一半） | 一致 |
| 1975 | V2 片2 P2 三条 V2-N-05/06/07；4 文件 +28/-1 | 同（MAD 可为 0 新失效模式、锁自述失实、PHOTOMETRY 锚漂移） | 一致 |
| 1976 | V2 收工立条 V2-N-08 升 P0（FAST 裁决前提被 35c85f53 推翻）；4 文件 +28/-1 | V2-N-08 点名 35c85f53=P8、findings/G_GOV_GATE/p0/V2-D2.md 新档＋账本 +3；并注"升 P0 前值起点不完整" | 一致 |
| 1977 | perf(P10)：利用率第二轮（noise 补 -fopenmp、逐源并行、按需预算）；6 文件 +330/-25 | 同（根因＝phase1_noise 编译行缺 -fopenmp、snr 2430.9→366.6ms、dynamic 调度＋确定性归约＋2048 封顶、按 plan 声明配预算） | 一致 |
| 1978 | docs(audit/RQS)：V6 片3 立 P1 两条 V6-N-04/N-05；9 文件 +395/-56 | 同（V6-N-04 冻结 SCI 推导权威指向 gitignored run/（50 处新引用/14 文件）、V6-N-05 README 称未改 lib/core 而 diff +162 行） | 一致 |
| 1979 | docs(audit/RQS)：前台自验脚本入库（回应 V6 提醒，核实 9777193d 是否 P? 补全非越权）；3 文件 +27/-3 | 同（_front_selfcheck.py 等三枚；被验对象＝1963 的账本列变更；"自验结论卡片未载"两侧均未证实） | 一致 |
| 1980 | fix(P9)：p1_op_wcs 移除帧头 WCS 初始指向，新增 header_pointing（OBJCTRA/OBJCTDEC＋焦距/像元）；4 文件 +275/-38 | 同（负责人指令原话、删 header_crval 分支、DATA_SEMANTICS §18.5 新增、906 帧可推导性统计；P9/P11 互为解算失败两型收口） | 一致 |

小计：一致 10、口径差 0、不一致 0。

## 十三、本部分"不一致"条目专列

无。本次核对的 110 条重叠 seq 中，未发现在"做了什么/落点/内容性质"上实质冲突、需指定以某边为准的条目。

4 条口径差-可兼容条目的双方原文引述与兼容依据（依据均为可核对材料——证据卡变更面聚合、manifest 消息首行、双方报告自身的限定语）：

1. seq 1371（S26 vs S25）：S26 表列原文："1371 | 545bd4d4 |（无编号）| 仅新增 evidence/v6_1_rework/gates/G1/CHECKLIST.md（7 项全 PASS）……存在约 27 分钟偏差"；S25 标题原文："### seq 1371 | 545bd4d4 | 2026-08-31 | S（G1-STATUS，V6.1 门）"。兼容依据：manifest 该条消息首行为"G1 门清单 7/7 PASS (治理/版本/检查器)"——不含任务编号样式；"G1-STATUS"是 S25 自加门标签，S26 的"无编号"指消息无任务号，两说法同真；落点（单 CHECKLIST 文件）与性质（纯门登记）两侧一致。
2. seq 1427（S27 vs S26）：S27 原文："DOC-001 重建合同 front matter 与完整图（36 合同图/生产可达/源索引/注释卫生）。落点全在 evidence/v6_1_rework（含 c01_contract_graph.log…）。重叠待核"；S26 原文："故本条的变更面性质是'复验与留证'，不是'重建'：消息自述用'重建'，变更面只给检查结果与 PLAN"。兼容依据：S27 段落导语自限"不作一致性判断"、"重建"系 manifest 消息首行原文；两片落点登记逐字相同（全在 evidence、代码面 0、四日志同名）。定性取可证的 S26"复验"为准（变更面证据），S27 未下相反结论。
3. seq 1429（S27 vs S26）：与 1427 同理——S27 转述消息"重建当前版本追溯矩阵（63/66/18）"，S26 依变更面（6 文件全在 evidence、三日志）判"复验并归档"；数字 63/66/18 两侧同引消息/PLAN，S26 另标"未证实"（补充而非相反）。兼容。
4. seq 1922（S36 vs S35）：S36 原文："1922 | f4f88d54 | …RQS：L28b 交档，27 轴＋14 域收工，INDEX v3；findings/40_OWNER/INDEX/_cache/_merge 共 7 文件"；S35 原文（主题归纳 2）："RQS 收档与汇总层（依据 …1915、1916、1922）：INDEX/SUMMARY/40_OWNER_DECISIONS 的迭代，十域→十四域→27 轴全部收工里程碑"。兼容依据：S35 未为 1922 立独立条目，其主题线对 1922 的定性与 S36 逐条登记同一；"十四域"与"14 域"同值；差异仅条目粒度。

另附两条互证记录（不构成判定差异）：seq 1540（S29 以本片 1542 补登自述与 S28 交叉印证，成立）；卡内双口径现象系统性存在（1373/1375/1421/1532/1538/1540 等"目录聚合行≠每文件行之和"），各分析师引用不同行所致数字差均非误读，建议汇总层统一注明行数口径以哪行为准。

## 十四、本部分复核方法与材料说明

1. seq 清单取自各后片 manifest.csv 角色列＝重叠复核 的前 10 行，与任务给定区间逐片核对一致（1366-1375／1421-1430／1476-1485／1531-1540／1586-1595／1641-1650／1696-1705／1751-1760／1806-1815／1861-1870／1916-1925／1971-1980）。
2. 前片主责条目为比对基准；前片普遍按"同编号连续提交成组/主题分组"组织（H-S26 将 1421–1422 并组、H-S30 将 1641–1650 并为组、H-S35 按 RQS 主题分组），核对时以组内该 seq 专属行/句为准。
3. 数字类差异一律回溯 设计大纲/_evidence/commits/cards/ 对应卡仲裁；卡内"目录聚合"与"每文件"两区块的固有口径差已核实（卡 1373、1375、1421、1532、1534、1538、1540），不计为两报告冲突。
4. 各后片重叠段附带的结构事实（父指针衔接、时间戳单调、H-S33 的"父＝前一 seq sha8"链核、H-S32 自列三处重点回核项）与前片核对均无矛盾，未单列成行。
5. 第一部分（S14..S25 区段）为另一核对任务既有产物，本部分仅追加、未改动其内容。

