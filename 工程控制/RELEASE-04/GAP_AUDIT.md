# GAP_AUDIT — RELEASE-04 差距审计

差距来源：① 负责人 2026-09-21 裁决（三创新点、预检语义、工程治理清理）；② RELEASE-03 SUMMARY §5 遗留 18 项；③ 新最高设计与现状的文档规范差距。执行时先复核现状（文件/行以当时 main 为准），再开工。

## G0. 顶层设计差距（最高优先级）

| ID | 差距 | 现状 | 目标条款 | 任务 |
|---|---|---|---|---|
| G0-1 | 测光星等坐标系未作为完整创新点验证 | Phase1 有 photometry/apply 节点与文档，但"Gaia XP 光谱×QE×透过率积分拟合、整帧消除物理单位"缺少完整实验单元（HST 仿真 + 代数合成 + 真实数据三重佐证） | 最高设计 §2.1、§12.3 SCI-A | SCI-401 |
| G0-2 | 跨帧绝对 SNR 传递链未闭环 | 帧级 SNR、稀疏层、三口径已有 EXP-205 适用域结论，但"不受天光影响的真实信号 SNR"的物理仿真验证、ΣSNR² 集成对拍、PixInsight 方法学对标未形成完整实验单元 | §2.2、SCI-B | SCI-402 |
| G0-3 | 加性天光/无接缝猜想未证实 | UPM 有实现与 q3_additive_truth 合成实验，但"校准后阶跃纯加性"仍是猜想；乘性世界、非退化接缝判据、真实数据接缝闭环未完成 | §2.3、SCI-C | SCI-403 |
| G0-4 | 预检语义需最终对齐 | 检查页面三档（correct/warn/error）、correct 与 warn 都需 yes、-y/-yes 跳过确认、error 阻塞且 -y 不可越、-force 跳过整个检查 | §4.5 | E2E-401（语义对拍） |

## G1. 文档与索引差距

| ID | 差距 | 任务 |
|---|---|---|
| G1-1 | 五份根文档需替换为包内新版（最高设计重写：§2 三创新点、§12.3 实验单元、预检最终语义、排异五档表、三口径适用域、历史叙事清零） | DOC-401 |
| G1-2 | 下级文档残留日期、任务编号（EXP-/§9.x/RELEASE- 引用）、历史叙事；排异/control_ivar/面亮度/NaN 掩膜/噪声模型 A 等已定案口径需在 plugins/science/algorithms 全面一致 | DOC-402 |
| G1-3 | 双向层级索引未闭合：最高设计指针、下级文档抬头上游条款、DOCUMENT_INDEX.yaml、悬空索引机器门 | DOC-403 |
| G1-4 | 最高设计附录 B 引用的 PHOTOMETRY_RESEARCH_PACK 需存在；测光研究包（Gaia XP 合成测光、QE/透过率积分、星表引导检测文献与开源对照）需补齐 | DOC-404 |
| G1-5 | DATA_SEMANTICS §11.1 variance 未入标准块表；`aio_pipeline.h` 约 304 行"自定义块名也允许"仍在 | FIX-404 |

## G2. 工程治理清理差距

| ID | 差距 | 任务 |
|---|---|---|
| G2-1 | v6 家族（p3_v6_export.cpp、v6 SIN 内核等）与 lib/pipeline/orchestrator（约 16191 行）未接入生产命名块管线；最高设计 §8.2 要求阶段内 PipelineFrame 命名块内存管线——先接线再删旧目录 | CLEAN-401 |
| G2-2 | psfsw 统一对象后残留分支（coverage.cpp 约 376/425/430 行）无退役注释 | CLEAN-401 |
| G2-3 | 噪声模型 B（noise_model_b.cpp，实验证明偏差 +104%~231%）待退役处置；第三 σ 估计器（star_detector.cpp 约 30-70 行，增益 +71.8%）经实验裁决去留 | CLEAN-401 |
| G2-4 | 工程控制/ 下 RELEASE-01/02/03、PROJECT-GOVERNANCE-01/02、SCI-RES-01 六个历史包；reports/ 历史报告；docs/ 下 archive/backlog/governance/owner/review 等待甄别 | CLEAN-402 |
| G2-5 | aio 唯一 I/O 棘轮台账余 155 文件/1656 处：PRODUCTION-RESIDUAL 47 处必须收口，TEST-HARNESS 56 处显式白名单，DORMANT/RETIRED-PENDING 随 CLEAN-401 处置 | CLEAN-403 |

## G3. RELEASE-03 遗留代码差距（FIX/PERF）

| ID | 差距（RELEASE-03 SUMMARY §5） | 任务 |
|---|---|---|
| G3-1 | HiPS tile 非原子发布；Phase2 直写输出目录 | FIX-401 |
| G3-2 | Phase2 signal 无 BUNIT/像素语义 provenance；最高设计 §6.3 输入语义守卫未接线（p3_v6_export.cpp 未进构建） | FIX-402 |
| G3-3 | `aio_hips_writer.cpp` 约 495-499、776-800 行用 f32 hierarchy 累加（dk=9 偏差 2.5e-3/3.95e-4），改 f64 | FIX-403 |
| G3-4 | `module_adapters.cpp` 约 1040-1057 行阶段二写端口仍 UnitId::ADU（应 SURFACE_BRIGHTNESS） | FIX-402 |
| G3-5 | drizzle 非有限样本静默 continue，未暴露 n_rejected_nonfinite | FIX-405 |
| G3-6 | v6 SIN 内核往返 2.5e-5px，冻结容差 1e-6 偏紧 25×，经 SCI 复核后冻结 | FIX-406 |
| G3-7 | variance_floor 钳制 fail-open，改 fail-closed | FIX-405 |
| G3-8 | MODULE_MAP.yaml 164 条声明中 53 条路径不存在；lib/ 注释 40 条引用不存在路径（真 28/假 12） | FIX-404 |
| G3-9 | DISP-P2SMP-002 rejected_insufficient_retained 双计数 | FIX-405 |
| G3-10 | 事件流 10 类开放 kind 需登记 schema | FIX-405 |
| G3-11 | export FITS 恒含 RUNID/CHECKSUM 致严格字节判据恒红；astrocs verify 不在命令树 | FIX-405 |
| G3-12 | p2_final.json 仍出 weight_mode:2 | FIX-405 |
| G3-13 | compare_bitwise.py 未掩码 sampler_config.cpu_workers | FIX-405 |
| G3-14 | cfitsio 全局锁致 Phase2 并行区间均值 0.71 核（RELEASE-02 遗留，L2 性能门拦路） | PERF-401 |
| G3-15 | SIGTERM 全阶段 exit 9 路径未覆盖 | FIX-406 |

## G4. 验收差距

| ID | 差距 | 任务 |
|---|---|---|
| G4-1 | 非 fast 检查档、tests/cli 余 6 红（5 条构建树环境 + 1 条待复跑） | BLD-401 |
| G4-2 | 三命令小批量端到端 + 预检语义对拍 + 1/N worker 一致性 | E2E-401 |
| G4-3 | L4 完全未做：M42（49 帧）+ 银心（32 帧）R 通道全流程、两张平面 FITS、拉伸切块 PNG、接缝/黑洞/亮斑目检、分段计时热点优化 | VIS-401 |
| G4-4 | README、版本号 0.0.1alpha、发布包白名单/SBOM、控制包收口自清理 | FIN-401 |

## 未覆盖（本包不做，显式记录）

- Windows 腿（Linux 全绿后另包；双平台 CI 构建门在 BLD-401 保持）；
- GUI/HiPS Browser、GPU/ACR 生产化、ARM；
- R 通道以外的多通道成品帧（本版仅 R）；
- 安装器（自解压目录即可，发行前再做）。
