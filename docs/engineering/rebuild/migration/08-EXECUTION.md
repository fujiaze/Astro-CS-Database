# 08｜Agent 自主迁移总任务书：优先复用main、禁止性能/科学退化、完整196帧收敛

**唯一执行主线**：[MIGRATION](../MIGRATION.md)。**当前实施入口**：[#126 main→V2功能回归](https://github.com/fujiaze/Astro-CS-Database/issues/126)；**后续全数据**：[ #125 M42 S2 ](https://github.com/fujiaze/Astro-CS-Database/issues/125)，必须等待本轮高阶/跨面板功能真实PASS。工作分支只在 `rebuild/acsd-v2`，读取远端最新HEAD，main固定只读对照；禁止直接改main、强推、独立PR或重复开第二条开发主线。**本文件既是施工顺序，也是模块交付任务**，不是状态PASS清单。

## 1. 主Agent工作权责与每一任务的共同交付格式

主Agent负责规划/实施/复核/纠偏/推送，必要时并行委派：旧代码调查（只读）、科学/数学、性能profiler、IO/FITS/HEALPix、测试/观测QA等。多代理不同worktree可并行调查/开发互不冲突文件，最终合并与固定HEAD的唯一责任在主Agent。独立审核者不得审核自己写的生产实现。

任何任务必须输出以下六项：
1. main生产调用证据：入口、相关函数/文件/被调用路径、旧算法可保留部分、旧已知问题与旧历史证据的SHA/前置条件；
2. V2实际修改文件/公开操作、SCI/units/index/random roots/PSF/WCS依赖图，旧胶水删除/避免重复的说明；
3. 可重复的独立oracle或同输入main对照（包括破坏性负例），真实M42最小输入的观测值/解释，而非只有测试数；
4. **同算法同质量性能表**：main vs V2或迁入前后，CPU/wall/RSS/IO/nnz/实际worker、perf热点/缓存命中；不等价数学额外成本另列；
5. 本任务影响的缓存producer/消费者、实际重算的最小范围、输出SHA、孤立scratch清理和空间余量；
6. 独立Reviewer的PASS_SCOPED/FAIL/NOT_RUN及仍有何scientific limitations。出现可修Bug应直接修并重复**最小实验**，而非停在“建议修复”。

**代码规则**：一个科学操作只有一个V2正式实现，一份public header模型、一套Host资源/Storage；旧C++核正确时优先迁主体，改C ABI/JSON/全局path/线程/长期静态缓存。每次重构不得以“新设计更优雅”丢自动NSIDE、旧PSF/光度、multi-panel、sky、rejection、shared-LRU、stream/性能；若旧算法被真实反例证伪，保留新V2数学模型和main性能策略。

## 2. 详细施工依赖任务（每项都应可由Agent直接执行）

| 编号 | 要修改/审查的源码与准确范围 | 交付和先后依赖 | 最小验证与性能任务 |
|---|---|---|---|
| A00 | main `lib/infrastructure/cli/commands.cpp`、`scheduler/src/module_adapters.cpp`；V2 `apps/cli/commands.cpp`、`src/workflows/*.cpp` | 对22 Process /旧P1/P2/P3生产符号逐项做“实际调用→V2唯一consumer”映射，freeze两个SHA；先于所有迁移 | 找出inactive precise/UPM旧分支、实际E2E SKIP、真实49帧历史报告与回归；输出符号/可编译依赖清单 |
| A01 | main `CMakeLists.txt`、`eng/tools/e2e/run_e2e_chain.py`与V2根CMake/contract CLI；主旧工具仅只读 | 尝试可重复构建旧main与V2、统一bench/输入/源SHA/worker与科学参数；无法旧构建要记录确切依赖缺口 | 生成独立基线小ROI（不是另一个V2科学引擎）、外部FITS/astropy、CPU/RSS/IO；固定旧E2E历史真值边界 |
| A02 | V2 `src/runtime/{host,pool,memory,pipe,storage}.cpp`、`src/workflows/product*.cpp` | 确立Task+Storage的块/对象/manifest接口、scientific recipe hash、immutable ref/lease/scope；给后继迁入核唯一Host | 真实故障注入：超限、cancel、late reader、缺child、坏hash/非法ACSUNIT，全部fail-closed；内存和IO上界 |
| B01 | main `drizzle_engine.cpp::compute_auto_nside_ex`、`hp_drizzle_api.cpp`；V2 `src/coordinates/*` + `src/workflows/normalize.cpp` + `apps/cli/commands.cpp` | **P0优先**抽取动态NSIDE，建立全帧ResolutionProbe/Plan并实际接入CLI，按全部参与帧共同leaf阶；依赖A00/A02 | 解析WCS/SIP/Jacobian与0.967″ M42、强SIP与畸变极值；冻结order/tile order，验证不再order13→细FITS伪保真；成本仅轻量预flight |
| B02 | main `shared/healpix/healpix_core.cpp`、`spherical_overlap/poly_clip.cpp`；V2 `src/coordinates/healpix.cpp`、`src/geometry/{overlap,refine,containment}.cpp` | 复用NESTED/候选空间索引/几何cache，但保持V2真实曲边、区间和支持语义；依赖B01 | RAwrap、极/face seam、sliver独立几何oracle；candidate miss=0，RSS/CPU/valid leaf/几何命中 |
| B03 | main `drizzle_engine/astro_sphere_sink.cpp`；V2 `src/geometry/{drizzle,drizzle_core,drizzle_process}.cpp`、`src/workflows/normalize.cpp`、`product.cpp` | 恢复stripe+touched tile+threadlocal geometry LRU+realR block streaming；只存真实受支持tile；依赖B01/B02/A02 | 32/128/256/512ROI→完整4096²；星点PSF/MTF/flux、缓存失效、完整闭包；不能全空Norder目录占6GiB |
| B04 | main `calibration/src`与旧AIO；V2 `src/observation/{calibration_kernel,calibrate,stream_map}.cpp` | 按[01 N01/N02](01-NORMALIZE.md)移旧校准/坏点并只接V2reader；可与B01/B02并行 | 真T2 masters逐像元、T3缺master、DQ、母版shared root；整图复制数/pixels/s |
| B05 | main `platesolve/cpp/ipv/src`、`photometry/wcs_transform.cpp`；V2 `src/catalogue`、`src/sources/astrometry_*`、`src/coordinates/tan_sip.cpp` | 保留可证IPV成熟盲解与真实SIP逆解，统一WCS provider；依赖A02，输出供B01 | #92六帧旧误杀、M42无头CD的30帧、留出Gaia、candidate预算与Gaia索引一次载入 |
| B06 | main `star_detection/src`、`psf/src`；V2 `src/sources/{psf*,measure*}.cpp` | 迁PSF/LM和空间模型有效核心，局部星云背景、混叠/DQ、误差与实际work；依赖B04/B05 | 旧负299万通量与2星反例；独立PSF/flux/FWHM/留出真实源；星窗口cache和CPU |
| B07 | main `photometry/cpp/src`、`noise_snr/cpp/src`；V2 `src/{response,photometry,information,uncertainty,operators}` | 谱积分/测光施加/信号噪声统一身份与R共享，复用稀疏信息算法并修Fisher误用；依赖B06/B03 | 旧Antlia/QE错配，#9真实比例/二次variance、shared root作用、依赖闭包、数值rank与临时内存 |
| C01 | main `coverage/src/{coverage,sampler,stage2_common}.cpp`；V2 `src/sky/sky_controls_process.cpp`、`src/workflows/mosaic.cpp` | **至少两个真正不同面板有重叠**，支持并集/交、全局控制组与高阶区域计划；依赖B03/B07 | 真球面footprint/连接图；不得用同M5三帧作跨panel证据；Tile reads/control复用 |
| C02 | main `coverage/src/sky_plane.cpp`（勿连入退役UPM）；V2 `src/sky/sky.cpp` | 移Schur/IRLS/固定分片装配等正确优化，保留V2PCPᵀ和真实乘性flat/加性Sky区别；依赖C01 | 真实主带8.43%旧接缝、跨面板signed seam、rank/gauge/低频保真与blockedCholesky perf |
| C03 | main `coverage/src/rejection.cpp`与`integrate.cpp`；V2 `src/coadd/*` | 移有效RCR/ESD/MAD和逐像素集成，更新N小样本/scene权重，统一Sky后selection与weights；依赖C02 | 模拟坏点/卫星注入TPR/FPR、N2/3/4/16、共享根与合法负像素、N_eff、stack吞吐 |
| D01 | main `resample/p3_resample.cpp`、`projection/p3_wcs.cpp`；V2 `src/projection/*`、`src/workflows/export_workflow.cpp` | 移层级选择、SharedTileCache/negative cache、可证采样核与真实目标WCS；依赖B03/C03 | order18 Norder9一致性、目标面板全覆盖、额外插值MTF、worker/共享cache容量 |
| D02 | main `fits_output/p3_output.cpp`、`scheduler/src/export_stream.cpp`；V2 `src/workflows/{fits_writer,product}.cpp` + Storage | 移FITS流式、metadata、strict verify与事务；依赖D01/A02 | 大FITS失败注入、new-process reopen/数值/headers、block/queue/IO peak；不占两份full arrays |
| E01 | main `eng/tools/e2e/`/旧真实实验、V2 `validation/m42` | **只在B01–D02独立通过后** 执行真实高分辨率单帧→不同面板→六面板科学对照，含Sky/SNR/排异/强星云低频；性能和数学统一reviewer | 真4096²细节保真、空间signed seam、NaN、有条件noise与测试判别力；未通过自动回到producer |
| E02 | `#125`、真实196帧输入清单/已冻结缓存与四通道FITS | 完成全帧质量筛选及Hα50/R49/G49/B48，全M1–M6共同targetWCS四套正式FITS；依赖E01 **PASS** | 全196有状态/参与帧说明；真实分设备响应/校准；全域质量和资源审查，用户最后看FITS |

**实施优先顺序**：A00/A01/A02 → B01（P0立即解决）+并行B04/B05 → B02/B03/B06/B07 → C01/C02/C03 → D01/D02 → E01 Reviewer → E02。需要修性能时应在对应B/C/D任务内完成，不把“性能优化”全部放在结尾。不能因为B01的高阶资源压力回退固定order13并继续后续步骤。

## 3. 子任务并行、代码所有权与文档边界

- **可并行只读审计**：main autoNSIDE/Drizzle、IPV/响应、Sky/Rejection、Runtime/IO、测试与性能历史，子代理提交调用图+实际证据；不同时改统一main或V2顶层接口。
- **可并行实现**：B04校准、B05目录天测、D02 Writer若契约已经固定，但共享`include/acsd/contracts.hpp`、`apps/cli/commands.cpp`、`src/workflows/*.cpp`、`src/runtime/*`的修改由主代理串行merge并在最新HEAD重验。不要两个代理分别实现WCS和光度内的SIP副本。
- **独立科学/数值Reviewer**：对每个包验证估计量、单位、原先主库真实能力、随机相关、旧证伪问题与输入条件；并查统计判据是否能在已知注入错误后判红。
- **独立工程/性能Reviewer**：读生产源码/实际运行、内存bytes对象归属、CPU/IOprofile、流式重开/错误，明确性能相对main归因；任何“旧功能回退P0”应阻断完整产品发布。
- **唯一最终裁决**：科学源模型与公式归Foundation和Process；Workflows仅拥有调度数据边，性能数值归06，验证任务归07，Issue只引用本文件，避免三四份“独立完全任务书”互相矛盾。
- main原研究和testdata全只读，历史`run/`未跟踪的不存在时如实登记，不用本次V2产物替代历史事实。每次新ISSUE用于下一阶段独立执行与回收；旧ISSUE有历史结论不因新文件改写。

## 4. 每轮自治闭环：发现缺陷就继续修，不停在报告

固定决策循环：
1. **核实**：按A00旧main正式调用、当前V2 HEAD、Input/Recipe/CachedProduct和资源约束；列最短可判别故障。
2. **定位**：从失败产品溯源到实际Producer与旧同输入对照，诊断是模型/接口/单位/科学算法/资源/数据可用性哪一类。
3. **修复**：优先移植已有正确旧核/旧优化，而不是新写500行近似；旧不可信时以独立数学改进。
4. **最小复证**：解析或真实ROI、独立oracle+旧反例、同质量性能profile；变动只失效科学依赖子图的下游块。
5. **扩展实测**：单4096²→不同面板→六面板，仅在前置质量/资源没有硬错误后扩大批次；成功产物按hash复用。
6. **独立审核**：审源码、实际产物、错误负例、科学/性能与磁盘；FAIL自主回到第2步修缺陷而不是停止等待下一轮。
7. **交接**：#126真正完成（自动选阶/保真/跨面板+没有重大性能回归）后，独立Reviewer PASS后主Agent自行解锁已有#125并执行全部196帧。不要提前在旧order13产品上生成196帧缓存。

## 5. 空间、失败和误差协议（执行时必须用）

- 全196个原始M42 FITS只读；`validation/m42/inputs_manifest.json`的sha在变化时按文件重算，不重复全面hash。Calibration/PlateSolve/Response与Native缓存可独立于GeometryRecipe保留；改变autoNSIDE/order/R必须强制重算几何及下游，Sky/weights变更不得重做校准。
- 限额12GiB进程RSS和真实本机空盘安全余量；GPU不是首期。内存估算错误不允许自动重试更大工作集、无界加循环次数、强制降物理分辨率；先缩chunk/降低并发/Spill并记录实际效果。
- 不把失败staging目录无限积累，删除只能针对本次任务独占、manifest可证无引用目录，不碰source/未知目录和已有成功final。模型出错不准通过宽容FITS reader、lower output resolution或修改阈值重新计算“PASS”。
- 结束必须提供实际二进制/配置SHA、成品FITS路径与文件SHA、每通道的196帧归属、共同WCS/真实细节/每面板Signed seam/extended flux/SNR与noise/unknown、独立审核及完整性能报告。最终用户仅做FITS视觉接缝验收，科学/质量Agent自己做完。

## 6. 一轮结束报告模板

主代理在Issue中一次性回复：
- 基线与结束source_sha，实际生产算法入口和迁入函数/文件、为何没复用/为何改科学；
- 数学/物理变化、独立oracle输入/期望/观测及失败例，main↔V2同输入性能和来源；
- 真M42输入hash/used/excluded、真实order/targetWCS、单帧/两面板/全部状态；
- CPU/peakRSS/disk/IO/caches/work、被复用的对象与清理项目/现有空间；
- 输出文件、科学质量与尚不可辨识项、独立Reviewer签核以及是否可以进入下一步；
- 遇FAIL则给**已经修复/下一次定向动作**和继续执行事实，而不是空泛“下一轮建议”。

**本规范只授权执行此前已经指定的工作**。真正无法推进的外部阻断（权限、缺物理不可替代校准、磁盘永远不足）如实报告最小用户动作；不伪造成功，不在没有执行环境时声称已计算完成。
