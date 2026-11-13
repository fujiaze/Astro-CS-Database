# 00｜main 生产资产与旧文档处置——先取证后迁移

> **范围**：只读比较 main `c5fee09fedb97ccbe39cb47ea0985e0d8a339db2`，V2 `1e2f8f8d29ec88c2703a49adea90ecfab801eb14`。本清单是静态源码调用审查与历史证据索引，**没有重新编译 main、跑全量M42、测峰值RSS**。迁移Agent必须补做02–07要求的真实调用确认；目录存在、函数有名字 ≠ 生效。

## 1. main 是真实科学工程，不是只有设计文档

- 根 `README.md`、`CMakeLists.txt` 和 `lib/infrastructure/cli/commands.cpp` 定义正式 `acsd normalize/mosaic/export --json`；独立命令通过 published HiPS/manifest 连接。正式 `run_phase1_block` 位于 commands.cpp 约2243行；`run_phase2_block`约1461；`run_phase3_block`约1706。
- `lib/infrastructure/scheduler/src/module_adapters.cpp` 约18577行，是**真实生产适配器，不是应重用的数学大类**。Normalize 在同文件的 `p1_op_{calibrate,cosmetic,wcs,star_psf,photometry,noise,drizzle,writer}`；Mosaic 的七个 `p2_op_{coverage,sample,upm_fit,upm_apply,reject,integrate,write}`；Export的 `p3_op_{properties,wcs,resample,writer,verify}`。
- 主要算法源目录 `lib/algorithms/` 包括校准、IPV天测、星检测、PSF、光度、噪声、HEALPix Drizzle、Coverage/Sampler/Sky/Rejection/Integration、球面→平面重采样和FITS。部分 module.yaml 和封装是占位/兼容层，不等于调用成功。
- main Runtime已有 `block_frame.cpp`、`block_flow.cpp`、`artifact_store.cpp`、`runtime.cpp`、`scheduler.cpp`、`executor.cpp`、`memory_budget.cpp`、`export_stream.cpp`、`cpu_budget.cpp` 等。主要缺陷是**正式节点间仍通过 p1_*.json/图像临时FITS/p2_*.bin 工作文件**，以及大规模 `module_adapters.cpp` 耦合。V2的统一typed对象、Host/Storage/Pipe应该取代这些胶水，不要重新发明旧数学核。
- 不能因 main 有 `eng/tests/e2e/test_pipeline_e2e.py` 就写全套E2E PASS：其中Normalize/Mosaic/Export B类测试为显式 SKIP；但 main `eng/tools/e2e/run_e2e_chain.py` 有真实链驱动与 `--verify-only`，且旧 `实验/m42-realdata` 记载真实运行。分清登记测试、真实历史运行和当前重跑。

## 2. 旧源码按资产身份分类

| main 源路径（可复核） | 正式生产引用及使用方式 | V2 接受者 | 策略/风险 |
|---|---|---|---|
| `calibration/src/{calibrator,dark_optimizer,master_generator}.cpp`，`calibration/cpp/cosmetic_corrector.cpp` | p1_calibrate/p1_cosmetic | observation.calibrate,regroup | 抽校准与DQ，主版/暗偏差、T2/T3尺寸/时间要复核；不带旧全局I/O |
| `platesolve/cpp/ipv/src/ipv_{solver,ransac,robust_refine,sip,distortion}.cpp` | p1_wcs调用IPV | catalogue.access,sources.astrometry,coordinates.wcs | 复用盲解与真实星匹配；旧 #92 AP/BP迭代反例保留，V2只有单一坐标基 |
| `star_detection/src/{sdet_image,nls_lm}.cpp`，`psf/src/{pixel_integrated_psf,spatial_psf}.cpp` | p1_star_psf_impl | sources.psf,measure | 抽独立计算核；局部M42星云背景/混叠和饱和是新需求，不能照搬旧误选 |
| `photometry/cpp/src/{spectrum_integrator,frame_photometry_fit,spatial_gain,image_corrector,star_matcher,wcs_transform}.cpp` | p1_photometry | response.integrate,photometry.fit/apply | 主体可改造；旧通带/QE错误是**硬反例**，Shared Gaia模型不拆成独立噪声 |
| `drizzle/healpix_drizzle/{drizzle_engine,hp_drizzle_api,hp_drizzle_hips_api,astro_sphere_sink}.cpp` | p1_drizzle，真实 `compute_auto_nside_ex` / `hp_drizzle_compute_auto_nside` | coordinates、geometry.drizzle、Normalize预flight | **必须复原动态选阶+确定性条带归约+ touched叶稀疏Tile+几何LRU**；旧平面四角近似须科学更正 |
| `drizzle/healpix_drizzle/{spherical_overlap,poly_clip,wcs_sip}.cpp` | Drizzle候选及球面交叠 | geometry.overlap,coordinates.wcs | 旧曲边近似不能直接替换V2真实连续曲边/区间模型；保留候选搜索优化 |
| `shared/healpix/healpix_core.cpp` | NESTED indexing，tile/leaf层级 | coordinates,geometry,projection | 复用经过外部库检验的坐标转换与位操作；仅保留一份实现 |
| `noise_snr/cpp/src/{sparse_reference,noise_model,information_weight}.cpp`，`integration/normalize_science/` | p1_drizzle后球面reference completion | information.*,uncertainty.covariance | 科学估计量和稀疏核保留；Fisher→scene混用/共享母版误差必须修正 |
| `coverage/src/{coverage,sampler,stage2_common}.cpp` | p2_coverage / p2_sample | sky.controls,workflow/coadd | **真多面板区域并集/重叠、逐tile采样**；旧target_order=min保护但会丢细度，需预规划共同高阶 |
| `coverage/src/sky_plane.cpp`；`upm.cpp` | p2_upm_fit调用sky_plane_build_joint，后者为主生产路径 | sky.fit | 复用联合Schur/IRLS/连通gauge等正确核；不把UPM退役旧核复制成第二求解器 |
| `coverage/src/rejection.cpp`、`integrate.cpp`；`integration/phase2_integrate/` | p2_reject→p2_integrate | coadd.select/weights/integrate | RCR/ESD/MAD等候选方法与逐像素加权归约可复用；小N/FP背景与场景权重科学条件须复核 |
| `coverage/src/block.cpp`、`coverage/include/astro/phase2/async_io.h` | P2动态分块、header-only BoundedAsyncQueue；检查实际正式入口 | V2 Task/Pipe/Memory | **性能合同复用**，旧队列/线程池本体不应重复建立 |
| `projection/{p3_projection,p3_wcs}.cpp`、`resample/p3_{resample,rsmp_operator,rsmp_units}.cpp`、`fits_output/p3_output.cpp` | p3_wcs/resample/writer/verify | projection.export, Storage | 导出 WCS、双线性/nearest、共享tile缓存、失败tile负缓存、流式FITS发布复用；不得凭旧文声称真正面积核生产已支持 |
| `infrastructure/scheduler/src/{export_stream,plan_estimator,cpu_budget}.cpp` | 生产资源/子块调度，参见真正CLI/IR路径 | Runtime/Workflow | 学习不变量和算账，不保留第二池；旧预算估算对高阶真实压力必须实测 |
| `infrastructure/aio/` | FITS/HiPS read/write/manifest | V2 Storage/Product IO | 复用标准格式/逻辑、事务与优化，改为V2唯一I/O；旧archive/legacy分支不回迁 |
| `phase1_session/`、`phase3_session/`、旧AIO/archive/legacy/healpix_stack | 兼容或历史非全链路径 | 无，历史参照 | **不迁到生产**，除非用实证证明拥有正式路径缺失的独立能力 |
| `infrastructure/hips_browser/` Qt/OpenGL | 非CLI算法核 | GUI后续 | 可复用渲染/HiPS浏览算法，UI生命周期另按V2服务接口规划 |

## 3. 五类文档处理：裁决，不复制全部

1. **最高目标及科学论证（保留功能与公式作比对）**：main `docs/ACSD_DESIGN.md`、`docs/science/unified/`、`docs/science/{normalize,drizzle,sky,noise_snr,photometry,projection,resample,formats,integration}/`；原 `docs/science/algorithms/{normalize,mosaic,export}/` 特别有真实kernel和优化选择。与V2五份 Foundation 发生冲突时以独立物理/数学核验裁决，不按文档时间自动取胜。
2. **正式实际接线（强烈保留为源码索引）**：main `docs/engineering/architecture/{SCIENCE_IMPLEMENTATION_DELTA,NORMALIZE_PIPELINE,DATA_FLOW}.md`、`docs/detail/{normalize,mosaic,export}/`、`docs/engineering/contracts/`。它们说明“生产入口使用谁/哪里仍取工作文件”；迁到V2后不复制旧数据合同。
3. **性能/资源（保留算法与对照）**：main `docs/engineering/{resources,architecture,testing,build}/`、`eng/tools/e2e/`、旧线程/缓存/stream源码、具体报告中有效负例。旧硬件速度不可未经复测直接定成新指标。
4. **独立实验（只读历史证据）**：main `实验/m42-realdata/`、`实验/issue9-p0-b1-production-fix/`、`实验/issue11-passband-identity/`、`实验/issue80-p5-a1-production-fix/`、`实验/issue92-wcs-gaia-match/`、`eng/tests/pipeline/`、`eng/tests/unit/`。应筛“被测SHA/真值/命令/局限”，迁有效反例而不是复制报告到多个V2目录。
5. **历史/不再维护**：旧实现状态台账、旧临时目录、已被否决的Fisher/权重假设、旧巨型Runtime写法、历史假设为PASS的测试装饰，保留Git版本，可在本目录提供索引；不要把陈旧版本复制到 V2 工作树或用批量搜索替代读懂消费者。

## 4. 真实历史证据纠偏

- `实验/m42-realdata/REPORT_paper.md` 记录 **RELEASE-05 的49帧真实M42三阶段运行**（T2 16+T3 33，523 tiles、137101312 leaf、4096² FITS），不代表本次已恢复产物；Git上未跟踪`run/RELEASE-05`，必须到既有机器路径查找或按计划少量重验，不能用不存在产物作实时 oracle。
- 同报告指出约 **2.78% covered_but_nonfinite**、部分通带/QE配置导致巨大测光散差（旧修正后的0.04505mag是历史报告，非本次重算），T3噪声系统项及sky假阳性判据。**不可只拿“49帧已跑”宣布科学成功。**
- `实验/issue9-p0-b1-production-fix/` 有真实同输入 Gaia光度尺度施加、variance/ivar/单位的验证及**T3/M1 Red双帧P2/P3**；`issue92...` 有真实六帧WCS逆变换先坏后修到入帧约1083–1090星；这些比纯空E2E登记更能指导V2。
- V2 S1过关的三帧T2/M5 `order13` 约25.8″等效叶，原图约0.967″/px，输出4096²不恢复损失细节；内部数学自洽PASS仍可保留，**成品精度/多面板PASS不能据此成立**。
- 旧 main `compute_auto_nside_ex` 自适应采样推断约`nside=2^18`于0.967″案例只是规模推算，不代表它具有严格全域下界证书或全196帧同一计划；V2必须补此系统级能力。

## 5. 旧源码的性能证据索引（只存来源，实际测量看06）

- `drizzle_engine.cpp`：target_ipix线程本地 bounded LRU geometry cache，run generation隔离；`t_candidates` scratch复用；按stripe唯一作者累加、按条带固定归约；touched leaf 合并；`astro_sphere_sink.cpp`只写有效tile。
- `resample/p3_resample.cpp`：**跨worker共享**有界 tile LRU，512² float32 ≈1MiB/tile；失败读负缓存；本地小热工作集减少全局锁，I/O在全局锁外。
- `coverage/src/block.cpp`：按 `memory_limit × safety_factor` 动态 micro-chunk，固定负担超限明确拒绝；`include/.../async_io.h` bounded queue有背压/close/cancel/error。
- `sky_plane.cpp`：Schur、IRLS、按帧分片再固定归约、64宽 blocked Cholesky；其 `std::thread` 和 `OMP_NUM_THREADS` 来源是**需适配**的旧机制，不可复制进V2重复消耗CPU租约。
- `scheduler/src/module_adapters.cpp`：P1帧间/帧内预算曾修复OpenMP ICV遗留和N×N超售；`export_stream.cpp` read/compute/write三段子块线程及有界队列。**恢复有效并行度，但不得违反V2唯一Pool/Task资源账本**。

如需冻结详细文件SHA，可由执行Agent在该基线对所选**正式生产调用文件**做`git ls-tree`清单，记录文件hash，变更时只重新查受影响模块；不要把旧库每个目录和实验重算成一次巨大hash工程。
