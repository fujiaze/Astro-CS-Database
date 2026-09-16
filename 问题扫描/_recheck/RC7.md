# RC7 复核档案 · recheck_round1 verify[6::8]（61 条）

- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（BASE `521095b8`）
- 分片：`问题扫描/_cache/recheck_round1.json` → `verify[6::8]`（i=6），实测 61 条（494 条取模第 7 组）；不触碰他人区间
- 方法：一律按 文件::符号 重新定位（glob/grep/read + 只读 python3 统计 + `git --no-optional-locks diff/show/log` 只读），复算原缺陷机制是否消失；缺失判定走三级复核（①工作树 ②HEAD `git ls-files`/`git show HEAD:<path>` ③改名/移动检索）
- 工作树状态：脏改含 `docs/TRACEABILITY.csv`、`artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv`、`reports/v19r2|v19r3/*`、`设计大纲/**`；涉及时单独注明，结论以 HEAD 入库态为准
- 四态口径：STILL（缺陷仍在，给新锚）/ FIXED（给"修在哪"证据：文件::符号 + 关键 diff 或新逻辑，并判是否修全/同类他站是否仍在）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）
- 本档只输出**建议标记**，不回写账本、不 commit（账本由前台统一处理）

## 一、结论总表（61 条，全量回填）

| # | ID | 优先 | 类别 | 四态 | 新锚（文件::符号） | 一句话判据 |
|---|----|------|------|------|--------------------|------------|
| 0 | L28b-D-004 | P2 | D_COMMENT | **STILL** | drizzle_engine.cpp:370/:591（原 369/590，+1 漂） | 匿名常量 1.25 的注释仍无《依据+DISP号》，DRIZZLE_GEOMETRY.md:65-66/:237 未变 |
| 1 | M1a-A-003 | P0 | A_SCI_DEF | **STILL** | p3_projection.cpp:198/:204/:216-217/:243/:255 + :23-24 | CRVAL2 仍不进映射、零守卫；PHASE3_PROJ_IMPL §15.2/§15.5 未变；本窗口零改动 |
| 2 | M1a-C-001 | P0 | C_DOC_CODE_GAP | **STILL** | ASTROMETRY.md:39/57/109/144-145 + PLATESOLVE.md:7/17/42 + ipv_wcs.cpp:409-412/535-536 | SCI/ALG 仍 7×7+「Oracle 全过」而实码 NB_GRID=41/81、AP 阶 5/7；门适用路径待 A-09 |
| 3 | M1a-D-001 | P1 | D_COMMENT | **STILL** | ipv_api.h:239-249（:240-241） | 接口仍写「Y 轴向上」而实现(:12/:1779)与 PLATESOLVE.md:128、README.md:58 全为向下；本窗口只加 ABI 版本字段 |
| 4 | M1a-F-004 | P1 | F_TEST_GAP | **STILL** | test_p3004_spherical_oracle.py:7 ↔ :157/:134 | 文档头称 1e-3 而断言 1e-2；合同要求的常数场 max_abs=0 无测试执行；tools/validation 不存在 |
| 5 | M1a-I-003 | P2 | I_DOC_HYGIENE | **STILL** | PHASE3_FITS_IMPL.md:54/:90/:191/:248/:344 | 整改声明「已闭合」而 §9 复杂度段(:248)仍写 fdatasum；代码面全仓无 fdatasum；与 modules/phase3_fits.md:133 互斥 |
| 6 | M2a-C-1 | P0 | C_DOC_CODE_GAP | **FIXED** | module_entry.c::build_result_json:777/:936-938/:794 | 已按 n_coords 定长 + JSON 发 n_coords，DATA_SEMANTICS:113 订正，锁 gaia_adapter_test.c::D6 已注册（残留：仅尾 miss 形态） |
| 7 | M2a-C-4 | P1 | C_DOC_CODE_GAP | **STILL** | lib/drizzle/README.md:9/:21/:24-25/§9 + module.yaml:21-26/:33-35 | 仍称「仅合同文件无源码/entrypoint MISSING/node_operations []」而源与 2 个 operation 俱在且被 core 注册 |
| 8 | M2a-D-3 | P2 | D_COMMENT | **STILL** | drizzle_engine.cpp:1890-1895；spherical_overlap.cpp:35/:1599；aio_hips_writer.cpp:172/:952/:1006/:1032 | 任务ID 注释仍无 DISP/条目号锚，DRIZZLE_GEOMETRY §8 与 DATA_SEMANTICS:247-249/:266 未改 |
| 9 | M2a-F-3 | P1 | F_TEST_GAP | **STILL** | lib/gaia_xpsd_client/test/test_gaia_race.c + tests 面 parallax/pmra 0 命中 | 孤儿测试仍不被 CMake/ctest/CI 任一引用；schema 发布的 parallax/pmra/pmdec 仍无任何测试期望非默认值 |
| 10 | M2a-H-4 | P2 | H_NUMERIC | **STILL** | hp_drizzle_api.cpp:1012-1041（:1024-1031） | FLOAT64 variance 仍被静默 (float) 降写且引擎无 f64 通道；§11.1 与 DATA-002:78 仍记 float32；信号侧已 fail-closed 使不对称更刺眼 |
| 11 | M2b-B-03 | P0 | B_STD_MISMATCH | **STILL** | aio_hips_writer.cpp::finalize_image_product:943→:984 | CD 等积换算仍乘 3600 写 hips_pixel_scale(arcsec) 违 §6.3.1 度口径，而 STANDARDS_REGISTRY:98 仍记 CONFORMANT |
| 12 | M2b-C-01 | P0 | C_DOC_CODE_GAP | **STILL** | lib/astro_image_io/src/hips/（rename/fsync/staging 0 命中）+ module_adapters.cpp:28/:3242-3243/:4789 | HIPS 发布仍无原子发布/目录 fsync（合同已诚实登记 DISP-HIPS-004），但 core 三处「原子/temp+rename」假注释仍在（A-22 待裁） |
| 13 | M2b-G-04 | P1 | G_GOV_GATE | **STILL** | lib/common/healpix/THIRD_PARTY_NOTICE.md:19/:22 ↔ healpix_core.cpp:337-346/:417-422 | 告示仍称未迁移 RING/邻居且未复制任何 GPL，而实码自述移植 GPL-2+ 与 astrometry.net；DEPENDENCIES.md 零登记 |
| 14 | M3-C-002 | P0 | C_DOC_CODE_GAP | **STILL** | pc_api.cpp:137-138/396-397/524-525/767-768/1046-1047 + orchestrator:2649-2659 | 库层钩子与 fixgates 锁已入库（原报「零命中」失实），但生产五站第三参一律 nullptr、六导出无质量位 ⇒ 机制未消 |
| 15 | M3-C-010 | P1 | C_DOC_CODE_GAP | **FIXED** | docs/algorithms/PHOTOMETRIC_FIT.md::§1:9（b0353303） | 无字段的 zero_point 声明已撤；未修全=production_call_paths_stage1.csv:5 仍留该诊断字段 + GLOSSARY 未区分 |
| 16 | M3-F-005 | P1 | F_TEST_GAP | **STILL** | TEST_MATRIX.md:41 + TRACEABILITY(SCI-CW 0 行) + INDEX.yaml:106-113 + CONTROL_WEIGHT_SNR §4/§8 + synthetic_gate.cpp::UPMW-001:3974 | TEST-CW 仍占位、SCI-CW 从未登记、六要素缺 4 节；且 UPMW-001 验的恰是被 CW 口径否掉的一侧 |
| 17 | M3b-A-03 | P0 | A_SCI_DEF | **STILL** | orchestrator:2470/:2506/:2474 + DATA §15.2:584-591/§18:766 + dpsf_psf.cpp::compute_trimmed_mad:222-253 | 截尾残差仍被定名 flux_uncertainty 并当 SNR 分母，无 N_eff/雅可比传播；P5-SNR 反使「SNR 使用 A/B/mad」成冲突残留 |
| 18 | M3b-C-04 | P1 | C_DOC_CODE_GAP | **STILL** | STAR_PSF_ALGORITHMS §1.1:12-14 ↔ DATA §15.2:549/:566 ↔ dynamic_psf.h:57/:17-31 | 布局 A/B 标签两 L1 互反、ALG 幽灵列 q_psf 全域 0 命中、flux 混入 ADU 域、注释列序与结构体不符；仅加消歧声明 |
| 19 | M3b-H-02 | P1 | H_NUMERIC | **STILL** | dpsf_psf.cpp:89-92/:107-110/:170-173/:200-207/:445-457 + orchestrator:2493-2495 | 非法参数域仍回写 1e10、λ 无界无发散判据、耗尽后 12 字段原样回填；编排仍认 status=3 有效，与 DATA §15.2（测光只收 0）相反 |
| 20 | M4-C-04 | P1 | C_DOC_CODE_GAP | **STILL** | p2_session.cpp:204/:183/:201-202 + upm.cpp:225/:246/:290-309/:505 + module_adapters.cpp:3752 | SCI 无条件断言「单帧区 continuation 填」，而两条生产装配 λs 默认恒 0 且不传 nodes ⇒ 延拓不可发生；「λ0=0」一支已失效 |
| 21 | M4-F-03 | P1 | F_TEST_GAP | **STILL** | tests/unit/CMakeLists.txt:669-671 + p2_upm_synthetic_test.cpp:125/:129 | 该 ctest 仍只链 astrocs_contracts、零 p2_upm_* 调用、CHECK(true) 冒名不变量门并打印 PASS；原报 TEST-UPM-003/004 论据作废 |
| 22 | M5a-C-002 | P1 | C_DOC_CODE_GAP | **STILL** | EXECUTION_MODEL:9/:11/:14/:22-24 + THREADING_MODEL:21 + sampler.cpp:883-892 + phase2/CMakeLists:28 + pc_api:340/717/996 | 「OpenMP 16」证据行仍指向只有裸 parallel for 的 calibrator；sampler/upm 实为 std::thread 池而文档称 OpenMP/串行；HiPS/ACR 互斥原样 |
| 23 | M5a-G-004 | P1 | G_GOV_GATE | **STILL** | commands.cpp:774/:789-793/:962 + resource_recorder.h:277-280 + resource_gate.h:254-255 + context.cpp:132-144 | set_workers 两形参同源⇒利用率恒 50%；set_queue/progress 零调用⇒队列列恒 0 而前提恒真；peak_active 只升不降⇒p50 门语义变形 |
| 24 | M5b-C-02 | P0 | C_DOC_CODE_GAP | **STILL** | cmake/astrocs.product.windows.json.in:9-10 ↔ packaging/astrocs.product.json:9-10 | 同一 commit 的 Windows 模板对两单源骨架 unit 记 IMPLEMENTED、Linux 记 SKELETON 并自述不冒认；Win 侧 secure_loader.c:407 仍 UNSUPPORTED |
| 25 | M5b-E-03 | P1 | E_TRACE_BREAK | **STILL** | CLI_PROTOCOL_V1:25/:39/:51 + cli/exit_codes.h:1 + cli/protocol.h:2 + check_api_docs.py:150-158/:343-344 | 唯一源头文件与顶层 schemas/ 均不存在（三级复核），检查器三候选回退吸收⇒门恒绿；原报 module_adapters 三站与 API-001:19 系锚误 |
| 26 | M5b-G-05 | P0 | G_GOV_GATE | **STILL** | check_build_graph.py::main:25-33/:36-42 + BUILD_GRAPH.md:9-17 + ci/checks.json:747-771 | 门仍只做子串在场判断、不读根 CMakeLists/install_layout/File API ⇒ 文档越准越红；产品目标面命中 0 |
| 27 | M5b-G-13 | P1 | G_GOV_GATE | **STILL** | check_abi_boundary.py:13-19/:30-33/:46-47/:58 | 收集 11 头只扫 1 头而 PASS 文案按 len(HEADERS) 虚报；struct_size 死分支恒不触发；模块化 ABI 头族全不在扫描集 |
| 28 | M5b-I-04 | P2 | I_DOC_HYGIENE | **STILL** | CHANGELOG.md:3-6 + VERSION + product.json:3 + packaging/README.md:37 + doccheck/check_version_namespaces.py:48/:131-132 | VERSION 与 CI 门已到 alpha.2，CHANGELOG 仍把上一轮标「Current Alpha」并自述 VERSION=alpha.1；日志驻留点只警告不 FAIL |
| 29 | M6a-C-004 | P2 | C_DOC_CODE_GAP | **STILL** | types.h::DRZ_CFG_KEY_NESTED:55 + module_entry.cpp:417-418/:536-544/:927 | 键注释仍写「0=RING 1=NESTED」而 README 已改「仅 1 可用」；缺键即 0 ⇒ validate 放行、execute 必被引擎拒（:769/:894/:1666） |
| 30 | M6a-D-008 | P2 | D_COMMENT | **STILL** | module_adapters.cpp:2613 + snr_estimator/README.md:5-6 + memory.md:35-36 + phase1/noise/README.md:28 + CMakeLists.txt:495-498 | 三处「唯一」宣称互斥；生产库编译的是 217 行 median+MAD 实现，snr_estimator 的 475 行 patch 级实现不在源集（md5 不同、行数已漂） |
| 31 | M6a-F-001 | P2 | F_TEST_GAP | **STILL** | healpix_core.h:6-8 + STANDARDS_REGISTRY:135 + tests/unit/CMakeLists.txt:612-614 | CONFORMANT 证据指向的 test_healpix_oracle.cpp 未挂 ctest/CI（只注册 neighbors 测试）且需仓外 oracle.jsonl ⇒ 百万点宣称无可重跑通路 |
| 32 | M6b-C-002 | P1 | C_DOC_CODE_GAP | **STILL** | PHASE2_INTEGRATION.md:42/:64/:113/:170/§11.3:234-243 ↔ integrate.cpp:44-50 + p2_output_semantics_test.cpp:85/:109 | B2-A7 已在代码修好并有正/反双向回归门，冻结 ALG 仍登记为「现状缺陷语义」且逐符号锚表整体错位（DISP-P2INT-002 亦过期） |
| 33 | M6b-E-007 | P2 | E_TRACE_BREAK | **STILL** | IO_001 §3:27/:30（对照 RELEASE_STATUS.md:79、INDEX.yaml:763） | 合同列的 io 骨架四件逐路径 cat-file 全 NOT_IN_HEAD（比原报 3 件更重），L0 仍记 IMPLEMENTED；DATA-001 子站成立但换宿主（contracts/data 两处 doc_ref 悬空，见 §4.1）|
| 34 | M6b-I-002 | P1 | I_DOC_HYGIENE | **STILL** | SCIENCE_FREEZE.md:3-5 + BASELINE.md:12/:14-21 ↔ RELEASE_STATUS.md:31/:70-72 | 仍以现在时宣告「ACCEPTANCE_GATES G1-G10 全部 PASS、FREEZE=PASS」而该文件不存在、evidence/performance 跟踪 0 文件；PASS 口径已被 owner 页废止 |
| 35 | M7-A-122 | P1 | A_SCI_DEF | **STILL** | NOISE_MODEL.md §2:16 ↔ §3:29/:76/:99 + DATA_SEMANTICS:413-414/:326 | ivar 单位仍记 pixel⁻²·ADU⁻²，与同篇量纲不变量及全部合同(ADU⁻²)互斥，多出像元面积平方因子；实现按 ADU⁻² ⇒ 属文档错 |
| 36 | M7-A-130 | P1 | A_SCI_DEF | **STILL** | STAR_PSF_ALGORITHMS.md:75/:151 ↔ PSF.md:20 + dpsf_psf.cpp:393-394 | 「FWHM=1.230310·sx/sy」仍是除法串记法，与同篇 F2 推导及 SCI 的轴向乘法自反；:151 代码锚已漂（:339-340→:393-394） |
| 37 | M7-C-001 | P0 | C_DOC_CODE_GAP | **FIXED（verified 维持 PARTIAL）** | upm.cpp:245/:260-262/:1064-1065/:1354 + stage2_common.cpp:466 + synthetic_gate.cpp:221 | dce8abd4 采强制 G==8 + 显式 rc=3：build/open 双侧校验、实际 G 进几何与 hash、三入口传播、双向往返测试；残留=配置面仍宣称 1..64 |
| 38 | M7-I-201 | P2 | I_DOC_HYGIENE | **STILL** | SCIENCE_SCOPE.md:53 ↔ CALIBRATION.md §9:82 | 入口页仍把 FP32 限为「显式等价路径 + UPM 专属门」，下游冻结校准页仍定 FP32 为核心默认；分层精度表未做（全 docs 0 命中） |
| 39 | M8-F-001 | P0 | F_TEST_GAP | **FIXED** | tests/abi 五站 TestCase（静态清点 22 例）+ validate_registry.py::R11:88-138/:227 | 0 用例假门已闭；但 BASE 即已如此⇒原报定性属定稿锚误。残留：README:39 的 36/36 未订正；R11 自身在 135 项检查中零 CI 载体 |
| 40 | M8-F-009 | P1 | F_TEST_GAP | **STILL→建议 PARTIAL** | resource_gate.h:97-98/:213/:217（已修）↔ p2_seam_gate_test.cpp:71 + test_ci001_failclosed.py:128/137/152（未修） | 三事实 1 修 2 存：0.80 已改 85 并补边界锁；自证式通过输入未改；反向钉死未改且该文件不被 CI 采集、全仓 --gate-required 命中 0 |
| 41 | M8-H-001 | P2 | H_NUMERIC | **FIXED（建议按误报撤竞态本体）** | gaia_client.c:1541-1594/1670-1722/1811-1862（18/18 受 #pragma omp atomic） | 逐行判定 HEAD 与 BASE 均「写点 18/受保护 18/裸写 0」，保护自 348ca1e8（BASE 祖先）⇒ 原报两点在基线即不成立；残余转 M2a-F-3 |
| 42 | M8a-E-002 | P1 | E_TRACE_BREAK | **STILL** | healpix_browser_qt/README.md:125-129 | 三条设计文档引用经工作树/HEAD/改名三级复核全不存在（036a3bb5 批量删后无重建），实存替代页未被指向；同族他站 31 文件 |
| 43 | M8a-G-006 | P1 | G_GOV_GATE | **STILL** | check_module_readmes.py::MODULES:12-17 + main:29/:31-44/:45-46 + checks.json::MODULE-READMES | 门仍只检硬编码 5 份（tracked module 级 README 实测 41 份）、判据到字样级；静态重放 errors=NONE 而被检 5 份自身 6 个 Pd-ddd id 全部不可解析；21 份 manifest 无门 |
| 44 | M8a-I-006 | P2 | I_DOC_HYGIENE | **STILL** | docs/GLOSSARY.md（25 行，entrypoint 0 命中）+ calibration README:163 ↔ module.yaml:21 | 术语仍无权威定义、两义混用且同模块文档与 manifest 直接冲突；check_glossary 的 REQUIRED 不含该词⇒结构上不可能变红；A-32 待裁 |
| 45 | M9-C-3 | P1 | C_DOC_CODE_GAP | **STILL** | ipv_polygon.h:42/:98/:114 + polygon.cpp:475/:633/:658 ↔ ipv_types.h:91 fov_diag_deg | 同名 fov_diag 在两函数按角秒解释（靠裸 3.0*3600.0 钉口径）而主链按度传递；无生产调用方⇒维持「一接线即 3600× 复发」 |
| 46 | M9-G-4 | P1 | G_GOV_GATE | **STILL** | runtime/io/fits_core.c:1077-1078/:1095-1100/:1228-1231/:1480 + fits_stream_v1.h:33 | target 与 tmp 同为 512 且两处 snprintf 均不查截断、begin 无长度校验、探针用未截断路径⇒长路径下 tmp==target 原地覆薄；同库正对照未采纳 |
| 47 | M9-H-6 | P1 | H_NUMERIC | **STILL** | aio_ahpx_reader.cpp:350/:356/:419-432/:441/:461/:473/:491 | double→uint64 直转 offset/size、(long) 偏移无 _fseeki64 分支、全文件无长度对照、分配先于读；收窄：readRaw 短读拒是部分缓解 |
| 48 | V10-N-01 | P1 | C_ALG_IMPL | **STILL** | fits_reader.cpp:425-431/:514-516/:530 + drizzle_engine.cpp:1896-1911 | 短读仅警告并按实读收缩 pixels，宽高仍用声明值且 return true；消费侧三重循环按声明尺寸索引⇒越界读机制完整 |
| 49 | V10-N-09 | P2 | C_ALG_IMPL | **STILL** | commands.cpp:204/:1081/:1286/:1672（对照 :288-294）+ 三处 *_log.cpp:22 | 同 TU 内 4 处 strtol(nullptr) 无界无诊断，而超时解析已 endptr+值域双挡；订正：日志站实为 (v>=0&&v<=3)?v:INFO，「负值→静默 0」不成立 |
| 50 | V11-N-08 | P1 | G_GOV_GATE | **STILL** | aio_abi_v1.h:70 ↔ aio_pipeline.h:84-85 + status_codes.h:147-148 + orchestrator.cpp:749-762 | 门检结构与交付结构仍是字段顺序相反的两份同名概念类型并被冻成断言；握手五项独不比 struct_size 却写进通过日志；fingerprint 算而不比 |
| 51 | V12-N-06 | P1 | A_SCI_DEF | **STILL** | ipv_types.h:238 + ipv_select.cpp:364/:1002/:1299/:1568/:1894 | 六处「已登记于 docs/algorithms/IPV_PIPELINE.md」的注释仍指向不存在路径（真实文件在 lib/plate_solve/，其内 DISP 计数 0）⇒ 冻结前提失效 |
| 52 | V12-N-14 | P2 | A_SCI_DEF | **缺失确认→建议 REJECT** | 无——任何 文件::符号 与全史面均不存在 | 三套 β 先验在 HEAD/BASE/全史/影子树零载体；-S 唯一命中是该 finding 自身提交；现行 β=4 且两份文档同口径、PSF.md:92 禁 β≠4 |
| 53 | V13-N-05 | P2 | G_GOV_GATE | **STILL** | v19r3_traceability.py:284-292/:299 + check_traceability_matrix.py:12/:213-222 + synthetic_gate.cpp:5214/:5260 | 42 枚铸造行实测 27 枚零宿主（+1 仅 legacy 档案文案）；pr_tests 仍 10 行实 8 实体；C5 只管三键⇒「行↔实体」无门；CSV 脏改系纯 CRLF |
| 54 | V14-N-08 | P1 | G_GOV_GATE | **STILL** | tools/quality/gen_module_readmes.py::main:133-143（覆盖点 :141） | 生成器仍无条件 open(w) 以 descriptor 覆写 registry 页，与 10 张事实修订页求交 10/10 命中、7 页 front-matter 与 descriptor 互斥；零接线 |
| 55 | V18-N-13 | P1 | G_GOV_GATE | **STILL** | check_full_integration.py:39/:57/:62/:70/:84 | 豁免额度 10 与其判据集合、DELIVERED 结论、exit 0 四者全部出自同一文件同一表达式链，全仓无机器可读源⇒自证额度机制未变 |
| 56 | V3-N-01 | P2 | C_DOC_CODE_GAP | **STILL** | PUBLIC_API.md:238-244 ↔ hp_drizzle_api.cpp:1011 | 帧通道清单仍止于 -13、无集中枚举；fail-closed 整改新产出的 -14 已在码却在合同零登记（全仓词界检索 0 宿主）；-10/-11 同族 |
| 57 | V6-N-05 | P1 | G_GOV_GATE | **FIXED** | lib/phase1/noise/README.md:70-73（3d8e04db，窗口内） | BASE 命中 1→HEAD 0：五处失实断言整删并改写为可核对接线说明；未修全=新文本又虚构两个 json 直调钩子（全树零定义），建议③机器门未建 |
| 58 | V7-N-07 | P1 | C_ALG_IMPL | **STILL** | module_adapters.cpp:2966（对照 :2934-2953/:2977） | nested 仍 p1_int(...,1)：字符串错型被吞成 NESTED（用户填 "0" 得 1，tile ipix 全重排且不可发现），同批两键均已收紧；无回显对照 |
| 59 | V8-N-07 | P1 | D_COMMENT | **STILL** | DATA_SEMANTICS.md:820-824（:823）+ module_adapters.h:30 等 6 文件 | 援引「负责人裁决 2026-09-14」实测 40 行，而 memory.md 对该日期 0 命中、DISP-PSF-FAST 全域 0 命中（连号都未进注册表）、A-44a/b 无条目⇒自指未解 |
| 60 | V9-N-08 | P1 | G_GOV_GATE | **STILL** | check_comments.py:34/:37-42 + ci/checks.json::CON-COMMENTS:773-782 | 提取式仍带 re.S⇒首条含「冻结」的巨块注释（实测 synthetic_gate.cpp 匹配 222679 字符=整文件）抑制全文件版本号检查；门口径 0 红而逐行确有违规 |

## 二、逐条复核记录

### A 组（我自己复核：L28b/M1a/M2a/M2b 簇，索引 0–13）

#### [0] L28b-D-004 — STILL（行锚 +1 漂移）
- 命令：`git --no-optional-locks show HEAD:lib/healpix_db/healpix_drizzle/drizzle_engine.cpp | grep -n 'ADAPTIVE_RATIO_THRESH'`
- 输出：`370:static const double ADAPTIVE_RATIO_THRESH = 1.25;  // 尺度比阈值 (max/min > 1.25 触发细分) []` / `591: bool uniform = (!no_data && local_max / local_min <= ADAPTIVE_RATIO_THRESH);`
- 新锚：lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::ADAPTIVE_RATIO_THRESH (:370) ／ ::sample_quadtree 唯一读取点 (:591)；被误读对象 docs/algorithms/DRIZZLE_GEOMETRY.md:65-66（HP_CIRCUMRADIUS_FACTOR=1.25）/:237 原样
- 判据：行尾空溯源括号 `[]` 仍在；该阈仍无任何 SCI/ALG 出处；两个 1.25（细分阈 vs 外接圆半径因子）之间仍无区分句。机制未变，仅 369→370、590→591 漂移。
- 建议标记：STILL(重锚 :370/:591)

#### [1] M1a-A-003 — STILL
- 命令：`grep -n 'theta = -yd\|dec_deg = theta\|theta = dec_deg' lib/phase3_proj/p3_projection.cpp`；`git --no-optional-locks diff --stat 521095b8 HEAD -- lib/phase3_proj/ docs/algorithms/PHASE3_PROJ_IMPL.md`（空＝两文件自 BASE 未动）
- 输出：:198 `const double theta = -yd * kRad; // θ = −Y`；:204 `*dec_deg = theta * kDeg;`；:216-217 `theta = dec_deg*kRad … plane_to_pix(d, phi*kDeg, -theta*kDeg, x, y)`；:243/:255 AIT 同型；:23-24 文件头仍写「CAR/AIT … CRVAL2 仅记录于 header 不进入映射」；:328 `if (std::fabs(centre_dec_deg) > kMaxAbsDec)`（无 CRVAL2==0 强制）
- 新锚：lib/phase3_proj/p3_projection.cpp::car_pix2world(:192-206)/::car_world2pix(:208-218)/::ait_pix2world(:224-245)/::ait_world2pix(:247-261)/::g1_build_cd(:56-67)/::p3_projection_make(:315-340)；docs/algorithms/PHASE3_PROJ_IMPL.md::§15.2(:398-401)/::§15.5(:445)
- 判据：建议处置 ④（make 对 CAR/AIT 强制 CRVAL2==0）未落地（`grep -rn 'crval_dec_deg == 0' lib/phase3_proj/` → 0 命中）；native 纬度仍当 celestial 纬度用，dec(CRPIX)=CRVAL2 仍不成立；四角守卫对 CAR 仍不可能触发（只在 |θ|>90° 才 PARAM）。ALG 侧「FITS 实践一致」表述原样。
- 建议标记：STILL(重锚)

#### [2] M1a-C-001 — STILL（四段判据逐条复算，全部仍成立）
- (1) 口径割裂：`grep -n 'NB_GRID\|7×7' docs/science/ASTROMETRY.md docs/algorithms/PLATESOLVE.md` → SCI :39/:57/:109/:125/:145 仍钉 NB_GRID=7 / 7×7；ALG :7/:17/:42/:74/:124/:167 同；而 `grep -n 'NB_GRID\|AP_FIT_ORDER\|APX_ORDER' lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` → :411 `const int NB_GRID = 41;`、:412 `AP_FIT_ORDER = 5`、:535 `NB_GRID_X = 81`、:536 `APX_ORDER = 7`。仓内已无任何 7×7 网格实现。
- (2) 门与路径错配：SCI §11:172 仍宣称「Oracle 全过（astropy 交叉 <1e-4 px）」；ipv_wcs.cpp:409 注释仍自证「冻结 1e-4 px 在该 fixture 下数学不可达, 需负责人裁决 (finding)」，达标者是 :840 `wcs_sky_to_pixel_iterative`（迭代反演），非导出的一步 AP/BP。
- (3) 导出能力缺：ipv_types.h:64-72 仍 `double AP[36]` / `APx[100]; int apx_order;  //上限 9`；lib/photometric_calib/cpp/src/wcs_transform.cpp:46 `if (sip_order < 0 || sip_order > 5) throw` ⇒ 阶 7 的 APx 在下游不可消费。
- (4) 一步残余：tests/unit/p1wcs/p1wcs_tests_apbp.cpp:256-257 仍 `// 布局扩展防退化观察线 (非验收线)` + `CHECK(cs, ap36_onestep_max[f] < 50.0)`；bridge 交叉测试 :304-305 仍用 `build_sip_matrix(fx["APx"], fx["apx_order"], 10)`。
- 新锚：docs/science/ASTROMETRY.md::§5(:57)/§7(:109)/§9(:125)/§11(:144-145)/:172；docs/algorithms/PLATESOLVE.md::§4(:17)/符号表(:124)；lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip(:241，网格段 :402-540)/::wcs_sky_to_pixel_iterative(:840)；tests/unit/p1wcs/p1wcs_tests_apbp.cpp::f2x 段(:251-257)；tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py::§2(:302-305)
- 判据：该 P0 的四个可验段落均无变化；ipv_wcs.cpp 在 BASE..HEAD 零改动（`git --no-optional-locks diff --stat 521095b8 HEAD -- lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` 空），被标 CHANGED 系宿主文件 ipv_api.h（P18 struct_size）连带，与本条机制无关。
- 建议标记：STILL(重锚)

#### [3] M1a-D-001 — STILL
- 命令：`git --no-optional-locks show HEAD:lib/plate_solve/cpp/ipv/include/ipv_api.h | sed -n '239,249p'`；`grep -n '+0.5 契约\|center=index+0.5' docs/algorithms/PLATESOLVE.md lib/plate_solve/README.md`；`grep -n '图像中心原点, Y 轴向上' lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`
- 输出：ipv_api.h:240-241 `// [0] det_x_px - 检测器 x (像素, 图像中心原点, Y 轴向上)`；实现侧注释 ipv_solver.cpp:12 与 :1779 同措辞；PLATESOLVE.md:128/:145-148「消费方对 IpvWcsResult/inlier 缓冲坐标必须按 +0.5 契约解读」；lib/plate_solve/README.md:58/:65-66「IPV 接口契约 center=index+0.5」
- 新锚：lib/plate_solve/cpp/ipv/include/ipv_api.h::ipv_get_last_inliers 字段约定(:239-249，声明 :261)；lib/plate_solve/cpp/ipv/src/ipv_solver.cpp::get_last_inliers(:1739-1782)；docs/algorithms/PLATESOLVE.md::§5(:128)/§9(:145-148)；lib/plate_solve/README.md(:58,:65-66)
- 判据：三处措辞互斥原样存在（中心原点+Y-up vs 数组下标+0.5+Y-down），实现处未加统一裁决注释、也未加建议②的值域断言（`grep -rn 'det_x_px' lib/plate_solve/cpp/ipv/test/` → 无值域断言；测试只测缓存重置/返回 0）。BASE..HEAD 对 ipv_api.h 的改动仅在 IpvParams struct_size 段（:61-83），未触碰字段表。
- 建议标记：STILL(重锚 :239-249)

#### [4] M1a-F-004 — STILL
- 命令：`grep -n '1e-3\|1e-2\|assertLess' tests/backend/test_p3004_spherical_oracle.py`；`git --no-optional-locks diff --stat 521095b8 HEAD -- tests/backend/`（空）
- 输出：:7 头注「B) 解析球面函数…vs 独立 reference(**1e-3** 相对容差)」；:157 `self.assertLess(worst_rel, 1e-2, …)`；:158 `assertLess(worst_abs, 5e-3)`；:134 `self.assertLess(float(fin.std()), 1e-3, "常数场输出应恒定")`
- 新锚：tests/backend/test_p3004_spherical_oracle.py::test_01(:134)/::test_02_analytic_field_matches_reference(:136-158)；docs/algorithms/PHASE3_RESAMPLE.md::§9 容差行(:100)；docs/science/PHASE3_HIPS_TO_FITS.md::§12 第 12 项(:109)
- 判据：声明 1e-3 ↔ 断言 1e-2 十倍差未订正；常数场仍是 `std()<1e-3` 而非合同（PHASE3_RESAMPLE.md:100）要求的 `max_abs=0(nearest)`；SYN-007 数值表仍无宿主——三级复核：`ls tools/validation/` → 目录不存在，`git --no-optional-locks ls-files tools/validation` → 0，仅 17 处引用型出现（docs 内），无冻结数值表 ⇒ 「容差由 SYN-007 预冻结」仍无源。
- 建议标记：STILL(重锚)

#### [5] M1a-I-003 — STILL
- 命令：`git --no-optional-locks show HEAD:docs/algorithms/PHASE3_FITS_IMPL.md | grep -n 'fdatasum'`；`git --no-optional-locks diff --stat 521095b8 HEAD -- docs/algorithms/PHASE3_FITS_IMPL.md`（空）
- 输出：:54 删除线「~~fdatasum（:59-67）~~」、:90「旧 fdatasum」、:191「删除自算 fdatasum（B2-A9 修正）」、**:248 §9 复杂度仍写「时间 O(W·H)（fits_write_pix 两次全帧 + sha256 全文件 + fdatasum O(4·W·H) 字节）」**；代码侧 `grep -rn 'fdatasum' lib/` → 仅 lib/phase3_fits/{README.md:53,memory.md:63,76} 文档残留，无生产实现
- 新锚：docs/algorithms/PHASE3_FITS_IMPL.md::§3(:54-58)/§6(:88-91)/§9(:248)/§14(:344-350)；docs/modules/phase3_fits.md::已知限制(:131-136，仍写「整改项：manifest_hash 恒 nullptr」) 对 PHASE3_FITS_IMPL.md:344「整改项（B2-A9/A10 已闭合）」
- 判据：三口径并存与"复杂度式含已删项"原样；README↔ALG 对同一整改项的"现状 vs 已闭合"互斥陈述仍在（模块页 :133 未随 B2-A9/A10 闭合订正）。
- 建议标记：STILL(重锚)

#### [6] M2a-C-1 — FIXED（修在哪已核，回归锁在线；测试面留一处残余形态）
- 修在哪：commit `dce8abd4 fix(RQS/B3): Phase2/3 六项 —— 坐标序…`（`git --no-optional-logs` 笔误校正：`git --no-optional-locks log -S 'M2a-C-1' -- lib/gaia_xpsd_client/ docs/contracts/DATA_SEMANTICS.md tests/` → 唯一命中 dce8abd4）
  - lib/gaia_xpsd_client/src/module_entry.c::build_result_json：:777 `b64_idx_cap = match_idx ? b64_encoded_len((uint64_t)c.n_coords * sizeof(int)) : 3`（旧为 count）；:936 `b64_encode((const uint8_t*)match_idx, (uint64_t)c.n_coords * sizeof(int), w)` + :937-938 显式引用「M2a-C-1: match_idx 载荷长度 = n_coords … 见 DATA_SEMANTICS §8.2」；:794/:797 结果 JSON 新增 `"n_coords":%d`
  - docs/contracts/DATA_SEMANTICS.md::§8.2 :113 现文「out_match_idx | int32 | **坐标序；长度 = n_coords（= 结果 JSON n_coords 字段）** | −1 = 该坐标未匹配」
  - 回归锁：tests/unit/gaia_adapter_test.c::D6(:648-727) 构造 4 坐标 2 命中（对跖 RA+180°/−dec 必未命中），断言 `dn==2`、`didx=={0,1,-1,-1}`、`n_coords==NC`、`count==dn`、载荷字节数 `== NC*sizeof(int)`、且与直连 API `memcmp==0`；该文件在册（tests/unit/CMakeLists.txt:877 add_executable / :888 add_test(NAME gaia_cat_adapter)）
- 判据（是否修全）：原报机制（截断为 matched_count）已消失，合同/实现/载荷三面对齐"坐标序 + −1 + 长度 n_coords"；同类他站：`grep -n 'match_idx' lib/.../gaia_client.c` 显示 C API 侧分配/回填本就是 n_coords（:2554/:2571/:2574），未被本条指控，无残余；`git --no-optional-locks grep -n 'match_idx' HEAD -- cli lib/core runtime tools` → 0 命中，仓内无第二消费站被截断语义影响。
- 残余（不改判）：建议③要求的三例只落了一例——D6 覆盖"尾部两个未命中"，**"首坐标未命中"与"全未命中"两形态仍无断言**；合同 §8.2 shape 段（:117-119）仍只给 out_stars/out_spectra 的 out_count 口径、未同段重申 match_idx 长度（长度信息在表格行内，可接受）。
- 建议标记：FIXED(dce8abd4) + 附注「回归锁缺 2 形态（首坐标未命中/全未命中）」

#### [7] M2a-C-4 — STILL
- 命令：`git --no-optional-locks show HEAD:lib/drizzle/README.md | sed -n '1,30p'`；`git --no-optional-locks show HEAD:lib/drizzle/module.yaml | sed -n '20,40p'`；`git --no-optional-locks ls-files lib/drizzle`
- 输出：README:9 「…由 P1-DRZ-IMPL 建立，当前本目录仅合同文件、无源码」；README:21 「现状实现编入 CMake 静态库 astrocs_drizzle（CMakeLists.txt:356-366，无独立 DLL 产物）」；README:24-25 「文档状态 CONTRACT_READY…构建随 CMakeLists.txt:356-366」；module.yaml:21-24 「entrypoint=MISSING：registry descriptor…未接节点…astrocs_p1_drizzle.dll 未建」、:34 `entrypoint: MISSING`、:35 `node_operations: []`
  实存事实：`git ls-files lib/drizzle` → src/module_entry.cpp、include/astrocs/drizzle/types.h、src/astrocs_p1_drizzle.def、src/module_exports.map 均在库；`grep -n 'add_subdirectory(lib/drizzle)' CMakeLists.txt` → :184；`grep -n add_library lib/drizzle/CMakeLists.txt` → :61 `add_library(astrocs_p1_drizzle SHARED`；`git show HEAD:lib/core/src/module_adapters.cpp` → :816 `ModuleDescriptor p1_drizzle_descriptor()`、:6446 `{p1_drizzle_descriptor(), {P1NodeOp::Drizzle, "drizzle_stack", …}}`（节点已接）
- 新锚：lib/drizzle/README.md::§1(:9,:21,:24-25)、::§9(:155-157)；lib/drizzle/module.yaml::头注(:21-26)/字段(:33-35)；对照 CMakeLists.txt:184、lib/drizzle/CMakeLists.txt:61、lib/core/src/module_adapters.cpp::p1_drizzle_descriptor(:816)/节点表(:6446)
- 判据：README/module.yaml 的"当前事实"仍与构建接线相反，且锚号（CMakeLists.txt:356-366）已漂（静态库现在 :408-436）。
- 建议标记：STILL(重锚 README:9/21/24-25；module.yaml:21-26/33-35)

#### [8] M2a-D-3 — STILL
- 命令：`grep -n 'P1-DRZ-NONFINITE' lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`；`grep -n 'ORACLE_HARDENING' lib/healpix_db/healpix_drizzle/spherical_overlap.cpp`；`grep -n 'B2-A8\|RESCUE-FD' lib/astro_image_io/src/hips/aio_hips_writer.cpp`；`grep -n 'B2-A1[247]' docs/algorithms/DRIZZLE_GEOMETRY.md docs/contracts/DATA_SEMANTICS.md`
- 输出：drizzle_engine.cpp:1890-1895「(P1-DRZ-NONFINITE) 冻结合同 docs/science/DRIZZLE.md §8 :96: …旧 isfinite(...)+continue 静默吞像素已删除」；spherical_overlap.cpp:35「签字修正 (ORACLE_HARDENING)…」、:1599「ORACLE_HARDENING: 赤道带…」；aio_hips_writer.cpp:952/:1006「B2-A8: …」、:172「RESCUE-FD-03: …实测 1-4 次」、:1032「RESCUE-FD-04」；hp_drizzle_api.cpp:965/:1002「RESCUE-FD-02」「M2a-H-1: …」；DRIZZLE_GEOMETRY.md:165/:173/:175/:184 B2-A12/A14/A17 引用簇；DATA_SEMANTICS.md:247-249/:266 同簇
- 新锚：lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::drizzleTiled(:1890-1895)；lib/healpix_db/healpix_drizzle/spherical_overlap.cpp(:35,:1599)；lib/astro_image_io/src/hips/aio_hips_writer.cpp::finalize_image_product(:952)/::obs_filter 段(:1006)/::write_chksum_deterministic(:172)；docs/algorithms/DRIZZLE_GEOMETRY.md::§8(:165-184)；docs/contracts/DATA_SEMANTICS.md::§11.1(:247-249)
- 判据：§12.2 白名单禁止的"任务编号/审计流水/带行号注释锚"仍成堆在场（:1890 一条同时含任务号 + 文档行号锚 §8 :96）。drizzle_engine.cpp 的 BASE..HEAD 改动（P22 性能解耦）未清理任何此类注释。
- 建议标记：STILL(重锚)

#### [9] M2a-F-3 — STILL（两实例均无在线保护）
- 实例①：三级复核 `git --no-optional-locks grep -l 'test_gaia_race' HEAD` → 仅命中该文件自身（+`设计大纲`/`问题扫描` 归档）；`grep -rn 'gaia_race' lib/gaia_xpsd_client/CMakeLists.txt lib/gaia_xpsd_client/Makefile ci/ctest_baseline.json` → 0；`ci/checks.json::UT-GAIA-ZLIB` 命令是 `python3 -m unittest discover -s lib/gaia_xpsd_client/tests`（**另一个目录**，非 test/），无 TSAN/race 项
  → 竞态验证件 `lib/gaia_xpsd_client/test/test_gaia_race.c` 仍不编译、不进 ctest、不进 CI；被守护的修复仍在（gaia_client.c:260/:262/:348/:356-360 BcLock/bc_lock_* 封装）
- 实例②：`git --no-optional-locks grep -n 'parallax' HEAD -- tests` → **0 命中**；`git --no-optional-locks grep -n 'pmra' HEAD -- tests` → 0。而装配侧仍显式置零并对外发布四列：gaia_client.c:2072 `calloc(...) /* CAT-GAIA-IMPL: 契约要求 parallax/pmra/pmdec 显式置 0 */`、module_entry.c:800-803 cone schema 仍含 `"parallax":"f32","pmra":"f32","pmdec":"f32","source_id":"i64"`
- 新锚：lib/gaia_xpsd_client/test/test_gaia_race.c（未注册）／lib/gaia_xpsd_client/CMakeLists.txt（无 target）／ci/checks.json::UT-GAIA-ZLIB；lib/gaia_xpsd_client/src/gaia_client.c::cone 装配(:2072) 与 ::GaiaSpectrumStar(gaia_client.h:66)／::module_entry.c cone schema(:800-803)；tests/unit/gaia_cat_test.c、::gaia_adapter_test.c、::gaia_integration_test.c（parallax 零引用）
- 判据：两条"改过没人守"的状态均未变化。附带发现（不改本条判定，建议并档 M2a-C-12 域）：DATA_SEMANTICS §8.2:121-123 现文仍称 parallax/pmra/pmdec「输出结构体中**未初始化**，调用方不得使用」，与代码已 calloc 置 0 相反。
- 建议标记：STILL(重锚)

#### [10] M2a-H-4 — STILL
- 命令：`grep -n 'FLOAT64' lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp | sed -n '1,12p'`；`grep -n 'variance' lib/healpix_db/healpix_drizzle/drizzle_engine.h`；`git show HEAD:docs/contracts/DATA_SEMANTICS.md | sed -n '254p'`
- 输出：hp_drizzle_api.cpp:1024-1031 `else if (vblk->type == AIO_BLOCK_FLOAT64) { … varianceConv[i] = (float)vd[i]; … fprintf(stderr, "… variance 块 FLOAT64 → FLOAT32 转换") }`（仅日志，不拒绝、不报错）；drizzle_engine.h:166/:187/:204/:219/:308 variance 形参恒 `const float*`、:280/:295 `float varianceValue`（无 f64 通道）；DATA_SEMANTICS §11.1:254 variance 行仍只写「float32，随 data 布局」，未登记 f64 输入的窄化/拒绝路径
- 新锚：lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::variance 块装配(:1012-1041)；lib/healpix_db/healpix_drizzle/drizzle_engine.h::processPixel(:280)/drizzleTiled(_f64) 形参(:166/:187/:204/:219)；docs/contracts/DATA_SEMANTICS.md::§11.1(:254)；docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md::planes 表(:78 variance dtype=float32)
- 判据：signal 侧本轮新增了 fail-closed（hp_drizzle_api.cpp:1002-1010 M2a-H-1：precision_mode=FP64 而 data 为 FLOAT32 → return -14），**variance 侧仍静默降位**，不对称不但存在且被对照放大。合同仍无该窄化声明、无 provenance 键。
- 建议标记：STILL(重锚 hp_drizzle_api.cpp:1012-1041)

#### [11] M2b-B-03 — STILL
- 命令：`grep -n 'hips_pixel_scale' lib/astro_image_io/src/hips/aio_hips_writer.cpp`；`sed -n '932,945p' ...`；`grep -n 'hips_pixel_scale' docs/algorithms/HIPS_WRITER.md docs/standards/STANDARDS_REGISTRY.md docs/contracts/DATA_SEMANTICS.md`
- 输出：aio_hips_writer.cpp:943 `std::snprintf(buf, sizeof(buf), "%.6f", 3600.0 * 180.0 / kPi() * std::sqrt(kPi() / 3.0) / (double)ps->nside);`、:984 `kv.push_back({"hips_pixel_scale", buf});`；HIPS_WRITER.md:186「hips_pixel_scale = 3600·180/π·√(π/3)/nside arcsec」；DATA_SEMANTICS.md:350-351 同 arcsec 口径；STANDARDS_REGISTRY.md:98 该行仍挂 §6.3.1 并判 CONFORMANT（偏差列只填 DISP-HIPS-012）
- 新锚：lib/astro_image_io/src/hips/aio_hips_writer.cpp::finalize_image_product(:943,:984)；docs/algorithms/HIPS_WRITER.md::ALG-HIPS-005 (5b)(:170-187)；docs/standards/STANDARDS_REGISTRY.md::D.hips 清单第 6 行(:98)；docs/contracts/DATA_SEMANTICS.md::DATA-P1-HIPS §12.3(:350-351)（原报 §12.1 properties 键表口径已并入 12.3 行）
- 判据：3600 因子仍在（= 角秒），标准 IVOA HiPS 1.0 §4.4.1 同名键单位是度；ALG 与 DATA 仍按实现口径登记，注册表条款映射仍错挂 §6.3.1。文件自 BASE 未改。建议②③（订正文档 + 改挂 §4.4.1）与 ④（外部独立复算 Oracle）均无落地痕迹。
- 建议标记：STILL(重锚 :943/:984)

#### [12] M2b-C-01 — STILL（"部分修复"的其余半边状态原样，三处失实注释仍在）
- 命令：`grep -rn 'rename\|fsync\|atomic_replace\|\.partial' lib/astro_image_io/src/hips/ | head`（限定作用域）；`git --no-optional-locks show HEAD:lib/core/src/module_adapters.cpp | grep -n 'AIO-002' | head`
- 输出：HiPS 写出目录内 `rename/fsync/staging/COMPLETE/.tmp/atomic` 命中 **0**（同命令在 `lib/astro_image_io/src/` 根层的 aio_pipeline.cpp/hiss_stream_writer.cpp 仍有 rename/atomic_replace 正对照）；writer 直写站点现在 :232（write_fits_image 起首 std::remove）、:301（write_moc_fits）、:337（write_properties fopen "wb"）、:1030（manifest 段）、:1175/:1244、:1468（aio_hips_finalize manifest fopen）、make_dirs :111（无 fsync）、aio_hips_abort :1557（只 delete）；module_adapters 三处失实注释：:28「write_variance_tile/finalize (AIO-002 原子发布内建;」、:3242-3243（p1_op_writer 头，:3251 函数体）「AIO-002 原子发布原语内建于 aio_hips 落盘路径」、:4789（p2_op_write 头，:4797）同措辞
- 新锚：lib/astro_image_io/src/hips/aio_hips_writer.cpp::write_fits_image(:218/:232)/::write_moc_fits(:293/:301)/::write_properties(:335/:337)/::make_dirs(:111)/::finalize_image_product(:932)/::aio_hips_finalize(:1361/:1468)/::aio_hips_abort(:1557)；lib/core/src/module_adapters.cpp::文件头节点地图(:28)/::p1_op_writer(:3242-3251)/::p2_op_write(:4789-4797)；docs/contracts/DATA_SEMANTICS.md::§12.5(:369-385)；docs/standards/STANDARDS_REGISTRY.md::DISP-HIPS-004(:109/:247)；docs/algorithms/HIPS_WRITER.md::§10 DISP 表(:313)
- 判据：合同/注册表现在如实登记"非原子、覆盖式、无回滚"（该半边此前已修），但**生产写出仍直写、失败清理仍只 delete 句柄、编排层注释仍把 IO-003 的原子语义记到 writer 账上**——原报机制（三处注释口径 + 未采用同库原子原语）逐站复算全部仍在。
- 建议标记：STILL(重锚；三站注释 :28/:3242/:4789 为施工点)

#### [13] M2b-G-04 — STILL（四面冲突均原样）
- 命令：`sed -n '17,23p' lib/common/healpix/THIRD_PARTY_NOTICE.md`；`sed -n '1,10p' lib/common/healpix/healpix_core.h`；`sed -n '1,10p;337,341p;417,421p' lib/common/healpix/healpix_core.cpp`
- 输出：NOTICE:19「仅迁移 NESTED 排序所需路径, **未迁移 RING 排序与邻居查询**」vs healpix_core.h:108 `std::vector<uint64_t> neighbors(uint32_t nside, uint64_t ipix);` 与 healpix_core.cpp:337「neighbors / query_disc helpers (B4-01 精选迁移…)」、:374 实现；NOTICE:22「**未复制任何 GPL (Healpix_cxx / RELION) 代码进入生产树**」vs healpix_core.cpp:338-339「邻居算法移植自官方 HEALPix C++ (Healpix_3.83 healpix_base.cc / healpix_tables.cc, **GPL-2+ 参考**)」、:417-418「query_disc … **移植自 Healpix_3.83 healpix_base.cc query_disc_internal**」；healpix_core.h:5-6「依据公开 HEALPix 算法…的**独立实现**」vs healpix_core.cpp:4-7「ang2pix/pix2ang 算法来源: **astrometry.net healpix.c**」；断链 URL 仍在 healpix_core.cpp:6「https:// github.com/astrometry/astrometry.net healpix.c」；:274-281 shift 收口 `if (shift >= 32) shift = 31;` 原样；healpix_core.h:29-30「ipix 越界时 ra=dec=0」原样
- 新锚：lib/common/healpix/THIRD_PARTY_NOTICE.md::迁移范围(:17-22)+::许可声明(:3-15)；lib/common/healpix/healpix_core.h::文件头来源与 Oracle 声明(:1-10)/::neighbors(:108)/::pix2ang_nest 越界语义(:29-30)；lib/common/healpix/healpix_core.cpp::文件头(:1-10)/::neighbors 段出处(:337-346)/::官方 face/swap 表(:347)/::query_disc 段(:417-422)/::nested_local_to_xy(:274-277)/::xy_to_nested_local(:279-282)
- 判据：三对互斥陈述逐字复算全部仍在，DEVIATION 面 `DEPENDENCIES.md` 对 healpix **零命中**（依赖登记缺口同样原样）。
- 建议标记：STILL(重锚)


### C 组（子代理复验 + 我抽核：M5a/M5b/M6a 簇，索引 22–29）

> 时点注：本组复验时工作树 HEAD 已前进到 `a20a1db8`（= a3a343a4 +1，增量仅动 `问题扫描/` 存档 145 文件），本组全部涉事对象零交集；我对其中 M5a-G-004 / M6a-C-004 的关键锚点做了独立抽核，与结论一致。

#### [22] M5a-C-002 — STILL
- 命令：`git --no-optional-locks diff --stat 521095b8 HEAD -- <涉事 8 文件>`（输出空）；`grep -n 'omp\|num_threads' lib/calibration/src/calibrator.cpp`；`grep -n 'std::thread' lib/phase2/src/sampler.cpp`
- 关键输出：EXECUTION_MODEL.md:9 仍称「OpenMP 16 / calibrator.cpp: OpenMP 16」，实为裸 `#pragma omp parallel for schedule(static)`（:89/:119/:128/:162/:171，无 16 无 num_threads，:25 注释「固定 16 线程」且违 CONCURRENCY_STANDARD.md:19）；:11 称「OpenMP or serial、CMakeLists.txt:18」，实为 sampler.cpp:883-892 `std::vector<std::thread> pool`（:882 注释「OpenMP 条件已移除」），option 实在 lib/phase2/CMakeLists.txt:28；THREADING_MODEL.md:21 仍称 sampler 串行；:22-24 与 ASYNC_IO_CONTRACT.md:89「待接入点」互斥原样；pc_api.cpp「OpenMP 16 线程」注释漂至 :340/:717/:996，与 :62 等 `omp_get_max_threads()` 打印自相矛盾
- 建议标记：STILL(重锚 EXECUTION_MODEL:9/11/14/22-24；THREADING:21；sampler:883-892；phase2/CMakeLists:28；pc_api:340/717/996)

#### [23] M5a-G-004 — STILL（我抽核：两形参同源与恒 50% 公式已亲验）
- 命令：`grep -n 'set_workers' cli/commands.cpp`；`sed -n '277,282p' cli/resource_recorder.h`；`git --no-optional-locks grep -n 'set_queue(|set_progress(' HEAD -- '*.cpp' '*.h' '*.hpp'`
- 我的抽核输出：commands.cpp:774 `recorder.set_workers(planned_start, planned_start)`、:793 `recorder.set_workers(eff, eff)`（两形参同源）；resource_recorder.h:279-280 `denom = active + runnable; util = active*100.0/denom` ⇒ utilization_pct 恒 50.00（或 0.00）；set_queue/set_progress 全仓仅定义零调用；gate 判据漂至 resource_gate.h:254-255；context.cpp::_note_acquire(:132-144) 峰值只上不下 ⇒ p50 判据失义
- 建议标记：STILL(重锚 commands:774/789-793/962；recorder:84-94/277-280；gate:254-255；context:132-144；module_adapters:461)

#### [24] M5b-C-02 — STILL
- 命令：`cat cmake/astrocs.product.windows.json.in`；`grep -n 'PLATFORM-RUNTIME\|PLATFORM-IO' packaging/astrocs.product.json`；`grep -n 'add_library(astrocs_runtime\|add_library(astrocs_io' CMakeLists.txt`；`grep -n 'ACS_ERR_UNSUPPORTED' runtime/module_loader/secure_loader.c`
- 关键输出：Windows 模板 :9-10 仍 IMPLEMENTED，Linux 清单 :9-10 仍 SKELETON（其 note 自述「不冒认实现完成」）；install_layout.cmake:129-133 无条件 configure_file；骨架判据可核（CMakeLists.txt:133 单源 artifact_abi_v1.c、:146 单源 fits_core.c）；secure_loader.c:407/:414-415 Windows 仍 ACS_ERR_UNSUPPORTED
- 建议标记：STILL(重锚 两清单 :9-10；CMakeLists:133/146/631；install_layout:129-133；secure_loader:407)

#### [25] M5b-E-03 — STILL（含两处锚误订正）
- 命令：三级复核 `ls include/astrocs/exit_codes.h`、`git ls-files include/astrocs/ | grep -i exit`、`git ls-files 'schemas/*'`、`git ls-files contracts/schemas/ | grep jsonl`、`git --no-optional-locks grep -n 'schemas/phase3_config_v1' HEAD`、`sed -n '150,158p' tools/check_api_docs.py`
- 关键输出：CLI_PROTOCOL_V1.md:25/:51 仍宣告不存在的唯一源 include/astrocs/exit_codes.h，:39 仍指不存在的顶层 schemas/（真实在 contracts/schemas/jsonl_event_v1.schema.json）；check_exit_codes 三候选回退（:150-158）吸收悬空、门恒绿；check_schema_files(:343-344) 不含声明面；**新发现同族站** cli/protocol.h:2 亦写不存在的 schemas/ 路径；**锚误订正**：原报「module_adapters 三处硬写 schemas/phase3_config_v1」与「API-001.md:19 同指 schemas/」在 BASE 与 HEAD 均零命中（API-001:19 现指正确的 cli/exit_codes.h），该两子站不成立
- 建议标记：STILL(重锚 CLI_PROTOCOL:25/39/51；checker:150-158/343-344；补站 cli/protocol.h:2；删两误站)

#### [26] M5b-G-05 — STILL
- 命令：`git --no-optional-locks diff --stat 521095b8 HEAD -- tools/quality/contracts/check_build_graph.py docs/architecture/BUILD_GRAPH.md`（空）；`git --no-optional-locks diff 521095b8 HEAD -- ci/checks.json | grep -c 'BUILD-GRAPH'`（0）
- 关键输出：判据仍是子串在场（:25-26 `tgt in text`、:29 P2_ENABLE_OPENMP、:36-42 只扫 lib/phase2/CMakeLists.txt），从不读根 CMakeLists/install_layout/File API ⇒ 订正文档反而变红；BUILD_GRAPH.md:15-17 仍列 orchestrator.exe/astro_image_io.dll/hepix_drizzle，:11 仍列 acr_kernels/cuda_bridge_loader；产品目标名在 BUILD_GRAPH.md 命中数 0；细节订正：astrocs-stage2 等仍定义在 lib/phase2/CMakeLists.txt:123/:136/:141 但被根图 `if(ASTROCS_BUILD_TESTS)`（CMakeLists.txt:692→:701）挡在产品面外 ⇒「产品面无覆盖」机制原样
- 建议标记：STILL(重锚 checker:25-33/36-42；BUILD_GRAPH:9-17；checks.json:747-771)

#### [27] M5b-G-13 — STILL
- 命令：`git --no-optional-locks diff --stat 521095b8 HEAD -- tools/check_abi_boundary.py`（空）；`ls lib/backend_host/*.h | grep -vc 'impl\|inc'`（10）
- 关键输出：HEADERS=11 但扫描循环只跑 BOUNDARY_ONLY=[common_abi_v1.h]（:46-47）⇒ 10 个公开头收集后从未被扫；:58 PASS 文案仍按 len(HEADERS) 虚报；:30-33 死分支（整文件文本判 struct_size，恒不触发）原样；include/astrocs/abi/*、contracts/artifact_abi_v1.h、io/aio_abi_v1.h 仍不在扫描集
- 建议标记：STILL(重锚 :13-19/:30-33/:46-47/:58；checks.json:161-181)

#### [28] M5b-I-04 — STILL
- 命令：`cat VERSION`；`sed -n '1,12p' CHANGELOG.md`；`grep -n '0.11.0-alpha' ci/checks.json`；三级复核 checker 路径 → `tools/doccheck/check_version_namespaces.py`（原报 tools/ 根路径已迁移，非删除）
- 关键输出：VERSION=0.11.0-alpha.2 且 CI 门期望 alpha.2，CHANGELOG.md:3 仍「[0.11.0-alpha.1] … Current Alpha」、:5 自述「根 VERSION = 0.11.0-alpha.1」；packaging/astrocs.product.json:3 仍 alpha.1；packaging/README.md:37（原 :17 漂）仍 alpha.1 目录名；豁免链原样：checker :48 LOG_FILES 含 CHANGELOG、:131-132「日志驻留点漂移只警告不 FAIL」
- 建议标记：STILL(重锚 CHANGELOG:3-6；checker 路径订正 tools/doccheck/)

#### [29] M6a-C-004 — STILL（我抽核通过）
- 命令：`git --no-optional-locks diff --stat 521095b8 HEAD -- lib/drizzle/`（空）；`sed -n '53,57p' lib/drizzle/include/astrocs/drizzle/types.h`；`sed -n '414,420p' lib/drizzle/src/module_entry.cpp`；`grep -n 'requires NESTED' lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
- 我的抽核输出：types.h:55 `#define DRZ_CFG_KEY_NESTED "nested"  /* 0=RING 1=NESTED */`（并列口径未订正）；module_entry.cpp:417-418 `c->nested = (found && nested != 0.0) ? 1 : 0;`（缺键即 RING 语义 0）；引擎三处硬拒实测 :769/:894/:1666「HISS requires NESTED ordering, RING not supported」（子代理报 :768/:893/:1665 为其 if 行，同站点）⇒ validate(:536-544) 放行、execute(:927) 必被拒，fail-closed 通道仍不可达；core 侧缺省 1 现漂至 module_adapters.cpp:2966-2967
- 建议标记：STILL(重锚 types.h:55、module_entry.cpp:417-418/536-544/927、engine:769/894/1666、adapters:2966)


### F 组（子代理复验 + 我抽核：运行时内存安全/数值/登记面簇，索引 46–53）

> 时点注：本组复验时工作树已前进到 `a20a1db8`/`89c4f85d`；我独立核过 `git --no-optional-locks diff --name-only -z a3a343a4..HEAD` = 194 文件、**非 问题扫描/ 路径 0 个** ⇒ 全部前进提交均为档案，本组判定对 a3a343a4 等价成立。

#### [46] M9-G-4 — STILL
- 命令：`grep -rn 'define ACS_FIO_PATH_MAX' modules/`；`sed -n '1075,1100p;1178,1262p' runtime/io/fits_core.c`；`git --no-optional-locks log --oneline 521095b8..HEAD -- runtime/io/fits_core.c`（空）
- 关键输出：fits_stream_v1.h:33 `#define ACS_FIO_PATH_MAX 512`；acs_fio_writer_v1_s :1077-1078 target/tmp 同为 512；:1228 `snprintf(wr->target,sizeof,…,"%s",path_utf8)` 不查截断；:1099 `snprintf(out,cap,"%s.tmp.%ld.%lu",…)` 不查返回值；begin 校验面 :1187-1214 无 path 长度校验（全文件唯一 strlen 在 :1705 无关）；:1216 overwrite 探针用未截断路径、实写用截断 tmp；:1231 "w+b"；:1480 `rename(wr->tmp, wr->target)`
- 判据：两个 512 同长 + 无校验 ⇒ 长路径下 tmp 与 target 塌缩为同一串、原子提交退化为原地覆薄的机制未变；同库正对照 hips_core.c::hips_join_path(:533 溢出即报错) 仍未被采纳；lib/ cli/ 零调用 ⇒ 生产不可达、P1 不上调
- 建议标记：STILL(重锚 :1077/:1095-1100/:1228-1231/:1480)

#### [47] M9-H-6 — STILL
- 命令：`grep -n 'parseHeader\|::readRaw\|extractNumber\|fseek\|ftell\|fstat' lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp`；`git --no-optional-locks log --oneline 521095b8..HEAD -- <该文件>`（空）
- 关键输出：:350/:356 `blk.offset/size = (uint64_t)extractNumber(…)`（:108 返回 double）；:441 `std::fseek(m_fp,(long)offset,SEEK_SET)` 无 _fseeki64/fseeko 分支；文件长度对照（ftell/fstat/SEEK_END）0 命中；:461 `std::vector<uint8_t> compData(blk->size)` 分配先于读；:473/:491 `estSize = blk->size*8`；:419/:424/:432 `(int)extractNumber` 赋 w/h/c 无范围校验
- 判据：LLP64 (long) 折叠命中合法偏移时 fseek/fread 双双"成功"而读错块的机制完整；措辞收窄一处——readRaw 的 `bytesRead != size` 短读拒(:448-452) 使"越过 EOF"被读面兜住，故残余是"错块 + 巨额预分配 + 无 fsize 对照"三件
- 建议标记：STILL(原锚即现锚；注短读拒为部分缓解)

#### [48] V10-N-01 — STILL（我亲读复核通过）
- 命令：`sed -n '418,435p;505,532p' lib/healpix_db/healpix_drizzle/fits_reader.cpp`；`git --no-optional-locks log --oneline 521095b8..HEAD -- lib/healpix_db/healpix_drizzle/fits_reader.cpp`（空）；`grep -n 'pixels\.size()' lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
- 我的复核输出：:424-427 `if (got < data_size) { fprintf(警告…); n_pixels = got / bytes_per_pixel; // 按实际读取量处理 }`、:431 `img.pixels.resize(n_pixels);`、:514-516 `img.width = width; img.height = height;`（头声明值）、:530 `return true;` ⇒ 短读后仍成功返回；消费侧 drizzleTiled 现行 :1896/:1900/:1905/:1911 索引 `pixels[(size_t)y*(size_t)img.width+(size_t)x]`、循环界 :1868-1871/:1889 全用声明宽高，输入侧零 size() 校验（现存的 846/964/1622/2360 均为输出侧）
- 判据：越界读机制原样；P22 两提交（8c977118/0765064c）只重排归约顺序、未增输入校验（git log 对 fits_reader.cpp 为空）；正对照 aio_xisf 硬拒(:507) 与同文件 RGB 挡(:503) 仍在，说明是"该站漏做"而非口径缺失；截断 FITS 负例在 drizzle 测试面 0 命中
- 建议标记：STILL(重锚 fits_reader.cpp:425-431/514-515/530；消费侧 drizzle_engine.cpp:1896-1911)

#### [49] V10-N-09 — STILL
- 命令：`grep -n 'strtol(.*nullptr, 10)' cli/commands.cpp`；`sed -n '20,23p' lib/astro_image_io/src/aio_log.cpp lib/dynamic_psf/src/dpsf_log.cpp lib/star_detector/src/sdet_log.cpp`；五文件 `git log 521095b8..HEAD`（空）
- 关键输出：commands.cpp:204/:1081/:1286/:1672 四处 `std::strtol(sleep_ms, nullptr, 10)`（行号与档案逐字吻合，:205 直入 deadline 可负无界）；同 TU 正对照 :288-294 `strtoul(tmo,&end,10)` + 全串消费 + v∈(0,86400]；三 log 站 `int v = std::atoi(env);` 原样；sampler.cpp:128-129 atof + (0,1] 挡原样
- 内容订正（我复核认同）：三 log 站实为 `(v>=0&&v<=3)?v:INFO` ⇒ "非数字→atoi=0→静默最低级"成立，但"负值→静默 0"不成立（负值落 INFO 回退）；主判据（同 TU 双纪律未推广）不变
- 建议标记：STILL(重锚 :204/:1081/:1286/:1672；订正负值档描述)

#### [50] V11-N-08 — STILL
- 命令：`sed -n '69,78p' include/astrocs/io/aio_abi_v1.h`；`sed -n '83,93p' lib/astro_image_io/include/aio_pipeline.h`；`sed -n '736,765p' lib/orchestrator/cpp/src/orchestrator.cpp`；`grep -rn 'enum_fingerprint' lib cli runtime include modules tests`；五文件 git log（空）
- 关键输出：甲 struct_size 在前(:70)、乙 abi_version 在前(:84)/struct_size 第二(:85)；include/astrocs/abi/status_codes.h:147-148 仍把"first/second"两种口径冻成断言；aio_abi.cpp:101-103 只对甲严校；orchestrator :749-754 五项判据**独不比 struct_size**，:760-762 却把 struct_size 写进"握手通过"日志；enum_fingerprint 全仓仅 2 命中（声明 :87 + 常量 0x5A1C0001 :166），零比较点
- 判据：门检结构与交付结构仍是两份字段顺序相反的同名概念类型；"被算被记不参与执行"纹面原封
- 建议标记：STILL(原锚即现锚)

#### [51] V12-N-06 — STILL
- 命令（三级）：`git --no-optional-locks grep -n 'docs/algorithms/IPV_PIPELINE' HEAD -- 'lib'`；`git ls-files | grep -i ipv_pipeline`；`git --no-optional-locks show HEAD:docs/algorithms/IPV_PIPELINE.md`；`git --no-optional-locks log --diff-filter=D --all -- '*IPV_PIPELINE*'`
- 关键输出：L1 恰 6 命中且全是注释自身，现行号 ipv_types.h:238（原 225，+13 漂移）、ipv_select.cpp:364（精确）/:1002/:1299/:1568/:1894（原 946/1244/1514/1841，+56 漂移）；L2 唯一实存文件是 `lib/plate_solve/IPV_PIPELINE.md`（不在 docs/）；L3 工作树/HEAD 无该路径、无删除记录；真实文档 `grep -c DISP` = 0，:118/:123 仍写 \`ρ(G)=5×10^(1.3×(G-10))\` 作现行步骤；BASE 与 HEAD 站点数同为 5+1 ⇒ 3d8e04db 只致行漂未修复；依据 run/perf-fix/P4-magiter/REPORT.md 本地存在但 gitignored（`git ls-files run/` 仅 .gitkeep）
- 判据：「已登记故不动冻结文档」的登记锚仍指向不存在路径 ⇒ 论证前提失效原样成立
- 建议标记：STILL(重锚 ipv_types.h:238、ipv_select.cpp:364/1002/1299/1568/1894)

#### [52] V12-N-14 — 缺失确认（四态之外；建议撤案 REJECT，勿记 FIXED）
- 命令（三级 + 全史）：L1 `grep -rn 'kMoffatBeta\|k_beta_prior\|2\.5531628' lib docs include run/`（0）；L2 `git --no-optional-locks show 521095b8:lib/dynamic_psf/src/dpsf_psf.cpp | grep -c 'kMoffatBeta'`（0）、`git ls-files | grep -i psf_fitting`（空）；L3 我另跑 `git --no-optional-locks log --all --oneline -S 'kMoffatBetaLower'` 与 `-S '2.5531628'`
- 关键输出（我的独立复核）：两条 -S 查询的**唯一命中都是 17fa5ff7**，即该 finding 自身文本的提交；`git grep '2\.553' HEAD -- lib docs include tests tools cli runtime modules contracts` 仅命中 gate4 证据 CSV 的浮点串（2.5531272872211268 等），非源码；现行 dpsf_psf.cpp 为固定 β=4 的 Moffat4（:25 `MOFFAT4_FWHM_FACTOR = 1.230310`、:393-394 `fwhm_x = FACTOR*sx`/`fwhm_y = FACTOR*sy`），全文无 2.5 字面量；STAR_PSF_ALGORITHMS.md 通篇 β=4（:7/:55/:140/:192）、PSF.md:92 明令禁 β≠4；`docs/algorithms/PSF_FITTING_VARIANTS.md` 工作树/HEAD/BASE/全史均不存在
- 判据：无修复 commit（非 FIXED）、他站零命中（非 MOVED）、纯静态即可判且判空（非 CANNOT_STATIC）⇒ 所报「三套 β 先验互斥 + 文档自称 2.553 实回退 2.5」的机制在 a3a343a4 与全部历史均无载体，代码与两份文档口径一致（β=4）
- 建议标记：REJECT(锚点全史不存在；请核该 finding 产出快照来源；账本如需四态请单列"缺失确认"，不得计 FIXED)

#### [53] V13-N-05 — STILL（28 枚口径复算为 27 纯零 + 1 枚仅 legacy 档案文案）
- 命令：`git -c core.quotepath=off grep -l -z <ID> HEAD` 逐 42 枚分类（剔除登记面 docs/TRACEABILITY.csv、docs/traceability/、tools/quality/v19r3_traceability.py、reports/v19r3/ 与扫描自引用档案）；`sed -n '284,292p' tools/quality/v19r3_traceability.py`；`sed -n '12p;213,222p' tools/traceability/check_traceability_matrix.py`
- 关键输出：纯零 27/42 = SCI 13（SCI-UPM-002..010 九枚 + SCI-NOISE-004/006/009/010 四枚）+ TEST 14（TEST-PR-UPM-002..010 九枚 + TEST-UPMW-002/003/005/006/007 五枚），与档案逐枚吻合；差的 1 枚 SCI-NOISE-012 仅存在于 legacy 控制包档案 QA-V19R7/R8 文案 ⇒ 若不计档案文案宿主则 28 枚原样复现；生成器 pr_tests 现 :284-292 仍 index4/7 同指 `UpmPersistRoundtripChainNoDrift`、5/8 同指 `InsertionOrderIndependent` ⇒ 10 行实 8 实体；C5 原文(:12) 只管三键唯一，实现 :213-222 无"行↔测试实体"维度 ⇒ 无机器门成立；`docs/TRACEABILITY.csv` 的脏改经我核为**纯 CRLF 行尾差异**（`git diff HEAD` 空、cmp 首差在第 200 字节 \\r）⇒ HEAD 判定与工作树内容一致
- 新锚：tools/quality/v19r3_traceability.py::_gen_range_rows(:284-292、pr_tests[i-1] :299)；tools/traceability/check_traceability_matrix.py(:12/:213-222)；lib/phase2/tests/synthetic_gate.cpp(:5214/:5260)
- 建议标记：STILL(重锚 :284-292；注记 012 仅档案文案)


### B 组（子代理复验 + 我对两处矛盾点独立裁决：Phase1/PSF/测光簇，索引 14–21）

> 时点注：本组复验时工作树为 `a20a1db8`/`89c4f85d`（a3a343a4 之后的纯档案提交）；我另核 `git --no-optional-locks diff --name-only -z a3a343a4..HEAD` 的非 问题扫描/ 路径数为 0 ⇒ 判定对 a3a343a4 等价。本组子代理两条交付对 M4-F-03 结论互斥（前一条报 FIXED、后一条报 STILL），我以实测取证裁决，见 [21]。

#### [14] M3-C-002 — STILL（账本原记 PARTIAL，本次确认残余仍是主机制）
- 命令：`git --no-optional-locks grep -cE 'quality_flags|SATURATED|saturat' a3a343a4 -- lib/photometric_calib/cpp/src`；`awk 'NR>=137&&NR<=142' lib/photometric_calib/cpp/src/pc_api.cpp`；`grep -n 'quality_flags' lib/photometric_calib/cpp/include/photometric_calib.h`（**0 命中，我实测**）；`grep -n 'satur\|qf\|quality' lib/orchestrator/cpp/src/orchestrator.cpp`（PHOTOMETRIC 段零命中）
- 关键输出：库层钩子真实存在（`star_matcher.cpp:395 const uint32_t* quality_flags`、:419-428 quality_pass 过滤、reason=7、rejected_quality 计数），回归锁 `lib/photometric_calib/tests/p1phot/p1phot_fixgates.cpp` 已 tracked 且登记为门 `CTEST-P1PHOT-FIXGATES`（ctest_targets=`p1phot_fixgates`、waivable=false，我实测）；**但五个生产导出站的 cleanAndScale 第三实参一律 nullptr**（pc_api.cpp:138/397/525/768/1047），六个 C 导出（photometric_calib.h:106/156/189/205/231/251）签名无质量位入参，编排层仍只把 `psf_status=row[0]` 送进定标
- 订正原档案：①「cpp/src 全文 grep 零命中」失实（HEAD 与 BASE 各 21 行命中）；②钩子落地早于 BASE（c3452d48 系 521095b8 祖先），本窗口该模块零改动
- 判据：SCI §4 第二判据（饱和星不参与定标）在**接口层**仍无处可施 ⇒ 原缺陷机制未消失，只是同域多了内部钩子与锁；负责人裁决 2026-09-14「不新增/不修改任何 C 导出 ABI」是该项停在 PARTIAL 的根因
- 建议标记：STILL(重锚 pc_api.cpp:137-138/396-397/524-525/767-768/1046-1047 五站 nullptr + orchestrator:2649-2659；PHOTOMETRY.md:37/:84 的锚 `star_matcher.cpp:35-40` 仍错指 percentileOf)

#### [15] M3-C-010 — FIXED（本组唯一改判，含 PARTIAL 残留）
- 修复在哪：`docs/algorithms/PHOTOMETRIC_FIT.md::§1 输出行(:9)` 由 `PhotometricCalibrationQuality (sigma_mag, sigma_cal_rel, zero_point) + scale` 改为 `(sigma_mag, sigma_cal_rel) + scale<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4：删除无定义式、结构体无字段的 zero_point) -->`（我实测 HEAD :9 即为该行）；提交 `b0353303`（science(P5)，2026-09-14，系 BASE 祖先 ⇒ 本窗口 diff 为空但缺陷已闭）
- 机制复算：原缺陷是「文档声明一个两结构体都没有的字段」；现行 `lib/snr_estimator/cpp/include/.../snr_estimator.h:41-52` 只有 sigma_*/n_matches/fit_status、`photometric_calib.h:40-45` 只有 scale_factor/sigma_residual，声明侧已撤 ⇒ 机制消失
- 是否修全：**未全**。同类他站 `docs/architecture/production_call_paths_stage1.csv:5` 仍把 `photometric.zero_point` 写成 `run_stage_photometric` 的诊断字段（我实测该行原样，且 lib 全树无该 dotted key）；建议处置第三项「GLOSSARY 与天测侧术语区分」未做（docs/GLOSSARY.md 零命中）。另三级复核：原档案所引天测符号 `apply_zero_point_correction` 在 HEAD 不存在
- 建议标记：FIXED(b0353303::PHASE2… 实为 docs/algorithms/PHOTOMETRIC_FIT.md::§1) + 残留登记 production_call_paths_stage1.csv:5

#### [16] M3-F-005 — STILL
- 命令：`git --no-optional-locks grep -n 'TEST-CW' HEAD`；`git show HEAD:docs/TRACEABILITY.csv | grep -c 'SCI-CW'`（0）；`git show HEAD:docs/contracts/INDEX.yaml | sed -n '106,113p'`；`git show HEAD:docs/science/CONTROL_WEIGHT_SNR.md | grep '^## '`；`wc -l`（现 119 行，原 88）
- 关键输出：TEST-CW 全仓仅 1 命中 = TEST_MATRIX.md:41 的 `lib/phase2/tests/...` 占位；SCI-CW 在 TRACEABILITY 从未登记（HEAD 0、工作树 0、`log -S 'SCI-CW' -- TRACEABILITY` 无删除记录），HEAD 只有 TEST-UPMW-001..007；INDEX.yaml:106-113 的 ALG-CW-001 与 :90-97 的 SCI-CW-001 仍指同一 CONTROL_WEIGHT_SNR.md；该文档章节仅 §1/2/2a/3..8，§9/§11/§14/§15 依旧缺失（宪章 §7.3 六要素）；§8(:112-119) 仍声称「公开 API 见 TRACEABILITY、测试见 synthetic_gate(UPMW-*)」两映射
- 方向冲突复算：§4:71 要求 `weights=support×snr_v²`，TEST_MATRIX:41 属性写「snr 扰动不变」，而真实同形门 `lib/phase2/tests/synthetic_gate.cpp::UPMW-001`（现 :3974-4003，漂移 +59）验的是「science 权重必须与 obs.snr 无关 / support 不得乘入」——即门验的是被 SCI-CW 否掉的那一侧
- 建议标记：STILL(重锚 TEST_MATRIX:41 / INDEX:106-113 / CONTROL_WEIGHT_SNR §4:66-71、§8:112-119 / UPMW-001:3974)

#### [17] M3b-A-03 — STILL
- 命令：`awk 'NR>=2467&&NR<=2516' lib/orchestrator/cpp/src/orchestrator.cpp`；`git --no-optional-locks grep -n 'flux_uncertainty' HEAD -- docs/contracts/DATA_SEMANTICS.md`；`awk 'NR>=222&&NR<=253' lib/dynamic_psf/src/dpsf_psf.cpp`；`git --no-optional-locks grep -rn 'sigma_flux|flux_sigma' HEAD -- lib`（0）
- 关键输出：:2470 注释「[4]=flux_uncertainty (PSF mad proxy)」、:2506 `row[4] = prow[7];  // mad (uncertainty proxy)`、:2474「SNR 使用 A/B/mad」三处原样；DATA §15.2 现 :584-591、§18 表行现 :766（漂移）仍作同名定名；`compute_trimmed_mad`(:222-253) 仍返回截尾均值|残差|（ADU/像元），无 √N_eff、无 A/sx/sy 雅可比传播；DISP-PSF-005（ALG §11.3:186/:208「无参数协方差输出」「禁止宣称已实现」）与 PSF.md §9a:88 零漂移
- 旁证与反向：P5-SNR 已把消费侧列 7 改称 residual_scale 并宣布旧 (A-B)/mad 退休（noise_model.cpp:318、snr_estimator.h:201/:362、PSF.md §1:8）⇒ 反而使 DATA §15.2:589-590 的「SNR 使用 A/B/mad」成为与 SCI/ALG 冲突的**残留措辞**；该列在生产码内目前无读者（PLATESOLVE/SNR 只读 col 1/2/3/6/7/12/13/14），风险面为合同语义
- 建议标记：STILL(重锚 orchestrator:2470/2506/2474 + DATA §15.2:587 与 §18:766)

#### [18] M3b-C-04 — STILL
- 命令：`git --no-optional-locks grep -n 'q_psf' HEAD -- lib/dynamic_psf`（0）；`grep -n 'out_row\[7\]\|out_row\[8\]' lib/dynamic_psf/src/dpsf_psf.cpp`；`git show HEAD:docs/contracts/DATA_SEMANTICS.md | sed -n '549p;566p'`
- 关键输出：ALG §1.1(:12-14) 定义「布局 A = oracle/表面亮度参数序 (B,A,x0,y0,sx,sy,θ,residual_scale,q_psf)」、§1.2(:25-27)「布局 B = 生产 PSF 块序」；DATA §15.2:549 却是「布局 A：编排层 psf 块」、:566「布局 B：批 API out_psf_params」⇒ 同名标签两 L1 文档互反，module.yaml:154-162 复述 ALG 版；ALG §1.1 后两列无任何缓冲对应（批缓冲 :932-933/:1100-1101 写 fwhm_x/fwhm_y，编排块 :2450-2451 写 mad/eccentricity，q_psf 零命中，LM 参数向量 NPARAMS=7）；ALG:19「B/A/flux/ADU 域」仍混入不存在的 flux 列；dynamic_psf.h:57 对 `_f` 通道的列注与 DPSFFitResult 实际字段序(:17-31) 及批缓冲列序三者互异
- 判据：四小项全在；新增的只是「消歧声明」（ALG:44-47、DATA:582「跨层传递禁止直接复用同一缓冲」），字母 A/B 仍互反 ⇒ 宪章 §12.3 字面一致未达成
- 建议标记：STILL(重锚 ALG §1.1:12-14 / DATA §15.2:549,566 / dynamic_psf.h:57 与 :17-31)

#### [19] M3b-H-02 — STILL
- 命令：`awk 'NR>=84&&NR<=118' lib/dynamic_psf/src/dpsf_psf.cpp`；`awk 'NR>=120&&NR<=208' … | grep -nE 'lambda|return|continue'`；`awk 'NR>=445&&NR<=457'`；`awk 'NR>=2493&&NR<=2495' lib/orchestrator/cpp/src/orchestrator.cpp`
- 关键输出：非法参数域仍写人造巨残差（:89-92 全样本 1e10、:107-110 逐样本 1e10，未加权未剔除）；λ 仍只有 ×0.1(:200)/×10(:202)，奇异时 `lambda*=10; continue;`(:170-173)，无上下界、无发散判据、非有限残差不出码；出口仅「收敛」(:183-186) 与「耗尽」(:206-207) 两支，耗尽后 12 字段原样回填(:445-457)；编排 `psf_valid=(prow[0]==0.0||prow[0]==3.0)`(:2493-2495) 原样，PLATESOLVE 侧同判据(:1817-1825，sat/fwhm/edge 仅统计不剔)；DATA §15.2 现 :554 仍规定「PHOTOMETRIC 仅 status=0 行入匹配」而 star_matcher.cpp:236 确为 status==0 ⇒ 同颗星「天测可用 / 测光被剔」的口径相反仍在
- 判据：dpsf 行号整体漂移 +19（B4-P1-1 NaN-safe 比较器插入），机制逐点原样；批 [N,9] 通道已把 ITERATION_LIMIT 判非 OK(:917/:923-941、:1086-1108)，但编排走结构体通道(:2372 `_d` / :2395 `_f`) ⇒ 残余未被覆盖
- 建议标记：STILL(重锚 dpsf_psf.cpp:89-92/107-110/170-173/195-197/206-207/445-457 + orchestrator:2493-2495)

#### [20] M4-C-04 — STILL
- 命令：`grep -n 'p2_upm_build\|nodes\|smoothing_lambda' lib/phase2_session/p2_session.cpp`；`sed -n '223,227p;244,247p' lib/phase2/src/upm.cpp`；`grep -n 'smoothing_lambda' lib/core/src/module_adapters.cpp`；`git --no-optional-locks grep -rn 'control_source' HEAD`（0）
- 我的实测输出：PHASE2_UPM.md:88「单帧区 | harmonic continuation 填」与 :134「退化路径：无 ≥2 帧 clean 覆盖 → harmonic continuation 填单帧区」措辞未变；p2_session.cpp:204 `rc = p2_upm_build(obs.data(), n_obs, &uc, &model);`（:165 备好的 nodes 向量从不传入），`uc` 于 :183 零初始化、仅 :201-202 当 config 显式给 `upm.smoothing_lambda` 才覆盖；upm.cpp:225 `cfg.smoothing_lambda = 0.0;`、:246 `if (cfg.smoothing_lambda < 0.0) cfg.smoothing_lambda = 0.0;`、lambda_s=max(0,λs)(:505)，nodes==nullptr 时 control 只从 obs 生成(:290-309) ⇒ 会话路径既无零数据自由度也无拉普拉斯耦合，延拓不可能发生；IR 链 fit_upm(:3679) 走 p2_upm_build_geo(:3759-3761) 有 nodes，但 λs 仍只在显式给键时 >0(:3729/:3752-3753)，八份 stage2 配置全为 `"smoothing": 0.0`
- 附带发现：upm.cpp:244 `if (cfg.grid != 8) return 3;   // M7-C-001: UPM 网格常数不一致 → 显式拒绝` —— 同文件的 M7-C-001 修复以注释自证，属**他条**证据，不影响本条判据
- 锚订正：原报「λ0 又被置 0 加剧」一支**已失效**（λ0 现为显式 1e-3：p2_session.cpp:191、upm.cpp:226/:244 `zero_anchor_weight=1e-3`）；ALG §12 仍在 :343/:444-445 自记「可执行 TEST-P2-UPM-001/002 登记为 MISSING」，T7(:356) 现状列只指代码锚
- 建议标记：STILL(重锚 p2_session.cpp:204/:183/:201-202 + upm.cpp:225/246/290-309/505 + module_adapters.cpp:3752；删「λ0=0 加剧」子站)

#### [21] M4-F-03 — STILL（子代理两版互斥，我实测裁决）
- 矛盾与裁决：该子代理前版交付称本条已由 `c3452d48` 修复（新建 `tests/CMakeLists.txt:507 astrocs_p2_upm_synthetic_test`、门 `CTEST-PHASE2-UPM-GATE`、`install(TARGETS phase2 EXPORT)`），后版改报 STILL。我独立取证：`git --no-optional-locks grep -n 'astrocs_p2_upm_synthetic_test' HEAD` → **0 命中**；`grep 'CTEST-PHASE2-UPM-GATE'` → **0 命中**；`git grep 'install(TARGETS phase2'` → 0；`git ls-files | grep registration` 只有 `tools/quality/check_ctest_registration.py`。⇒ 前版所述工件不存在，判 STILL
- 实测锚：`tests/unit/CMakeLists.txt:669-671` 仍 `add_executable(p2_upm_synthetic_test p2_upm_synthetic_test.cpp)` + `target_link_libraries(... PRIVATE astrocs_contracts)` + `add_test(NAME p2_upm_synthetic …)`；同段 :666-667 的 `p2_workers_test` 却链 `astrocs_phase2` ⇒ 该站是孤例；`tests/unit/p2_upm_synthetic_test.cpp:125` 仍 `CHECK(true);  // 语义: 合成框架无单覆盖污染`、:129 仍打印「P2-003 TESTS PASS（…加性非乘性）」
- 证据链更正：TRACEABILITY 无 `TEST-UPM-003/004` 行（BASE 与 HEAD 均 0），现行 UPM 登记是 `TEST-UPMW-001..007`，其载体 `lib/phase2/tests/synthetic_gate.cpp` 真链 phase2 生产库并跑 p2_upm_build/p2_upm_raw_weight（`lib/phase2/CMakeLists.txt:77-78`、:105 gtest_discover_tests 我实测）⇒「登记面对生产代码零覆盖」的旧论据不成立，影响面应收缩为「本 ctest 自身对生产码零贡献且名不副实」
- 建议标记：STILL(重锚 tests/unit/CMakeLists.txt:669-671 + p2_upm_synthetic_test.cpp:121-126/:129；附：原档案 TEST-UPM-003/004 论据作废)

### E 组预录（我亲测两条，先于子代理交付落档；如与子代理回报冲突以本节实测为准）

#### [39] M8-F-001 — FIXED（修未全，两处残留）
- 时间线取证：修复提交 `adaeb531`（2026-09-14 15:10:33）经 `git --no-optional-locks merge-base --is-ancestor adaeb531 521095b8` 判定**系 BASE 的祖先**；`git --no-optional-locks diff --stat 521095b8 HEAD -- tests/abi ci/validate_registry.py` **输出为空**；「R11」字样在 BASE 与 HEAD 同为 6 处命中 ⇒ 本窗口零变更，修复在扫描时点即已入库。
- 机制复算（原「0 用例假门」是否消失）：`tests/abi/test_secure_loader.py` 尾部现行 `class TestSecureLoaderAcceptance(unittest.TestCase)` + `def test_acceptance_script_passes(self): self.assertEqual(main(), 0, ...)`，类 docstring 自述「M8-F-001：原为 main() 直跑验收脚本，unittest discover 采集 0 用例 ⇒ UT-ABI 门空转」；五站静态清点用例数 = test_abi002_lifecycle 18、test_abi005_echo 1、test_mod001_install_load_check 1、test_module_registry 1、test_secure_loader 1，合计 **22**（`grep -h 'def test_' tests/abi/*.py | wc -l` = 22）⇒ `discover -s tests/abi` 现真采真红，原机制消失。
- 但**非本窗口修复**：`git --no-optional-locks show 521095b8:tests/abi/test_secure_loader.py | tail -6` 显示 BASE 版已含同一 TestCase 包装 ⇒ 原 finding 断言「全文件 TestCase 零命中」在扫描基线上即不成立，属定稿锚误（其 :216/:122/:160/:186 行号确指 `def main()`，但漏看文件尾部的包装类；另 `mod001_install_load_check.py` 本身至今 0 用例，采集靠同名 `test_` 前缀文件代理）。
- 残留①（文档站未订正）：`runtime/module_loader/README.md:39` 仍为 `python3 tests/abi/test_secure_loader.py   # 退出码 0 = 36/36 PASS`（建议处置③「标注为脚本直跑、非 CI 采集」未做；grep '36/36' 全仓命中 1）。
- 残留②（**R11 自身无 CI 载体**）：扫 `ci/checks.json` 全部 135 项检查的 command，**无一项引用 validate_registry**；`.github/workflows/ci-{linux,windows}.yml` 不引用（其 run: 体只允许 `ci/run.py`/`ci/wf_step.py`/`ci/bootstrap.py`）；`ci/run.py` 只调 `validate_against_schema`（结构校验，不含 R11 语义判据）。tracked 面引用 validate_registry 的只有 `ci/INVENTORY_REPORT.md` 与 `ci/tests/{test_deep_profiles,test_negative_guards,test_windows_ci,test_workflow_lock}.py`，而唯一覆盖 ci/tests 的门 `CI-BINDING-TESTS` 为 `unittest discover -s ci/tests -t ci/tests -p test_ci001b_*.py` —— 我以 fnmatch 实测该 pattern 仅收 `test_ci001b_binding.py`、`test_ci001b_wf_step.py` ⇒ 上述四文件（含 `test_registry_strict_still_passes` 这个唯一真调用 validate_registry 的用例）在 CI 上零采集。⇒ R11 是**有判据无载体**的门，M8-F-001 的同类根因（登记面与执行面脱节）在整改件自身复现。
- 建议标记：FIXED(adaeb531::tests/abi 五站 TestCase 包装 + ci/validate_registry.py::R11)；残留两条建议另立 finding（README:39 的 36/36 口径；R11/validate_registry 无 CI 载体，可并入 M8-G-001 族），不因此把本条改判 STILL

#### [41] M8-H-001 — FIXED（原报「裸 ++」在 BASE 即不成立）
- 机制复算：用 python 逐行扫描 `lib/gaia_xpsd_client/src/gaia_client.c`，对每个 `trace->(nodes|blocks|bytes|decomp|polar_nodes|eq_nodes)` 的 `++`/`+=` 写点要求其前一条非空行为 `#pragma omp atomic`。实测：**HEAD 写点 18 / 受保护 18 / 裸写 0；BASE(521095b8) 写点 18 / 受保护 18 / 裸写 0**。保护由提交 `348ca1e8`（2026-08-15「…checked public allocation + per-query trace」）引入，且 `merge-base --is-ancestor 348ca1e8 521095b8` 成立 ⇒ 早于扫描基线。
- 三点订正：①原报写点 :1313/:1360/:1362 等与现行三族写点 :1542-:1594 / :1670-:1722 / :1811-:1862 不对应，且 HEAD 的 :1313/:1360 处为无关代码；②「结构成员非 `_Atomic` ⇒ 更新非原子」混淆类型标注与更新语义：`#pragma omp atomic` 正是对普通标量做原子更新的标准手段，本模块 `lib/gaia_xpsd_client/CMakeLists.txt:112-114` `find_package(OpenMP QUIET)` + `OpenMP::OpenMP_C`，:116 明示无 OpenMP 时串行退化（串行亦无竞态）；③注释 :56-59 与实现一致：`enabled` 在并行区外单点赋值（:2059/:2293/:2602），四个并行入口 :2103/:2314/:2478/:2618 `#pragma omp parallel for … num_threads(gaia_omp_team_size())` 内所有计数更新均在 `if (trace && trace->enabled)` 下受 atomic 保护，读取点 :2205-:2207/:2416-:2418 在并行区之后 ⇒「注释为假、trace-on 即 UB」不成立。
- 残余（不属本条主机制，转记他条）：trace-on 路径仍零回归面——`git --no-optional-locks grep -l 'ASTROCS_GAIA_TRACE' HEAD` 只命中 GAIA_QUERY.md、模块 README/memory、gaia_client.c 自身与扫描档案；`lib/gaia_xpsd_client/test/test_gaia_race.c` 仍零注册（该站已记于 M2a-F-3，不重复计）。
- 建议标记：FIXED(348ca1e8::GaiaTraceCtx 全部写点 omp atomic 化，早于 BASE) + 证据链更正（原报「裸 ++/+=」「注释与实现互斥」两点在 BASE 即不成立；残余仅「trace-on 无回归面」，属 F_TEST_GAP 族）

### G 组（子代理复验 + 我抽核：数值/常量/机器门真实性簇，索引 54–60）

#### [54] V14-N-08 — STILL
- 命令：`git --no-optional-locks show 521095b8:tools/quality/gen_module_readmes.py | md5sum`（HEAD 同法）；`git --no-optional-locks grep -ln gen_module_readmes HEAD -- ci .github`；只读 python 复刻 parse_descriptors 与页面前言比对
- 关键输出：两版 md5 同为 `a361af81`（零改动）；覆盖写仍在 :141 `with open(p, "w", encoding="utf-8")`；ci/.github 引用为空（零接线）；生成器 22 页输出集 ∩ 10 张「事实修订」页 = **10/10 全在覆盖集内**；7 页 front-matter 与 descriptor 现值不一致（noise-snr 页 `SCI-NOISE-001/ALG-NOISE-001/API-NOISE-001` vs descriptor `SCI-P1-SNR-001/ALG-004/API-P1-006`）
- 判据：三要件（无条件覆盖 / 修订只落派生页 / 生成器零接线）逐条复算仍在；`tools/check_module_readmes.py`（MODULE-READMES 门）只校验 lib 侧 L2 README、不校验 registry 页 ⇒ 不构成保护
- 建议标记：STILL(重锚 :133-143，覆盖点 :141)

#### [55] V18-N-13 — STILL
- 命令：两版 md5 比对（同 `6332460e`）；`git --no-optional-locks grep -ln 'T500' HEAD`；只读 python 复刻 T407 判据
- 我的独立核对：`grep -rn 'HARDCODE_THREAD_DEBT' tools/` → `tools/quality/contracts/check_full_integration.py:39 HARDCODE_THREAD_DEBT_CAP = 10`、:60 超额判 FAIL、:62 `debt_count = HARDCODE_THREAD_DEBT_CAP`（截回上限）、:64 DEBT 文案自述「waiver scope: FORBID-HARDCODE-THREADS only, cap 10」；额度数值/判据集合/豁免结论/退出码四者全部出自同一文件同一表达式链，全仓 T411/T500 无机器可读源
- 附带：现行 lib 侧 FORBID-HARDCODE-THREADS 静态复刻为 0 命中 ⇒ 豁免额度暂无实体可挂，但机制本体（自证额度）未变
- 建议标记：STILL(锚点未漂移；消费门 ci/checks.json::CON-FULL-INTEGRATION :902)

#### [56] V3-N-01 — STILL（我抽核通过）
- 命令：`git --no-optional-locks show HEAD:docs/contracts/PUBLIC_API.md | sed -n '226,250p'`；`grep -n -- 'return -14' lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp`；全仓词界检索 `-14`
- 我的实测输出：合同 drizzle 返回码段现 :238-244 仍止于「-9=无 WCS（:541-545）、-12=HiPS dir 空（:1044-1048）、-13=直写失败（:1066-1070）——正负两套并存无集中枚举（登记缺陷，迁移整改点）」，**无 -14**；生产者仍在 `hp_drizzle_api.cpp:1011 return -14;`（:1006 `if (config.precision_mode == 1 && !data_is_f64)`）⇒ 该负值码系他条整改（M2a-H-1 fail-closed）新增，未回带合同
- 判据：机制未消失仅行号漂移；同族 -10/-11 在生产者侧 7 处 return 亦零登记 ⇒ 缺的是集中枚举面而非单行
- 建议标记：STILL(重锚 PUBLIC_API.md:238-244 / hp_drizzle_api.cpp:1011)

#### [57] V6-N-05 — FIXED（未修全）
- 修复在哪：`lib/phase1/noise/README.md`（现 :70-73），提交 `3d8e04db`（fix(P14)）一次删掉五处失实断言（原文「本批**未改仓库树中的 lib/core**」「仓库文件逐字节未变，见 REPORT §6」「run/perf-fix/…lib-core-snr-wiring.patch」），改写为「**lib/core 接线（P8 越域补丁 → P14 落树）**：p1_op_star_psf_impl / p1_op_noise 位于 lib/core/src/module_adapters.cpp」
- 机制复算：原缺陷是「README 宣称零改动」与同提交 `module_adapters.cpp` +162 行互斥；现两符号实测确在库内（module_adapters.cpp:1606 p1_op_star_psf_impl）⇒ 断言与 diff 不再冲突；`git --no-optional-locks grep '未改仓库树\|逐字节未变' HEAD` 非审计目录 0 命中（三级复核通过）
- 是否修全：**未全**。新文本 :71-73 又写入「新增两个测试专用直调钩子 `p1_op_star_psf_json` / `p1_op_noise_json`」，三级复核（工作树全量 grep → HEAD grep → `log --diff-filter=D -S`）显示两名**全树零定义**，实测 `tests/unit/p1snr/p1snr_frame_parity_test.cpp:156` 是经 `register_phase_modules` 走生产注册表 ⇒ 同族「自报量不可复算」在新文本复现；建议③的机器门（README 宣称 vs commit-stat）未入库
- 建议标记：FIXED(3d8e04db) + PARTIAL(残留=新文本虚构两钩子名 + 机器门未建)

#### [58] V7-N-07 — STILL
- 命令：`grep -n 'const int nested = p1_int' lib/core/src/module_adapters.cpp`；读 `p1_int` 实现；`git show 521095b8:… | sed -n '2825,2850p'`；`grep -rnE 'nested_request|nested_source' 同文件`
- 关键输出：现行 :2966 `const int nested = p1_int(dj, "nested", 1);`（BASE 在 :2831，纯漂移）；`p1_int` 对字符串（非 boolean/integer/float）直接 `return dflt` ⇒ `"0"` 被吞成 1=NESTED；对浮点 `static_cast<int>` 截断；同文件 nside 有 :2946 `float rejected (no truncation)`、precision_mode 有 :2977 `is_number_integer()` 前置门，nested 只有事后 `if (nested != 0 && nested != 1)`（错型已被吞，永不触发）；`nested_request/nested_source` 0 命中 ⇒ 产物仍只回显 :3198 `{"nested", res.nested}`；tests 无 nested 错型断言
- 判据：该批纪律的另两键均已收紧，nested 仍是唯一漏网键 ⇒ 机制原样
- 建议标记：STILL(重锚 :2966；对照门 :2934-2953/:2977)

#### [59] V8-N-07 — STILL（待负责人裁，非我可闭合）
- 命令：`grep -c '2026-09-14' memory.md`（**0**）；`git --no-optional-locks grep -n '2026-09-14' HEAD` + 负责人词过滤；`git grep -c 'PSF-FAST'`；`grep -nE 'A-44[ab]' 问题扫描/40_OWNER_DECISIONS.md`
- 关键输出：CHANGELOG.md 仅登记 P5-SNR 一族（:9/:11）；PSF-FAST 现 16 处 / 6 文件（module_adapters.h:30、module_adapters.cpp 10 处、dynamic_psf/README.md:204、module.yaml:157、phase1_session/README.md:49、tests/unit/p1001:2182/:2228）；帧头 WCS 17 处 / 5 文件；`DATA_SEMANTICS.md:823` 仍自带「负责人裁定原文：『我从来没有允许过使用…』」，而两引文的仓内宿主现仅剩审计底档镜像（`设计大纲/_evidence/commits/index.jsonl:1957/1980`），规范登记面 A-44a/A-44b 检索 0 命中；账本 V8-N-07 fix_state=OPEN
- 我的独立量测：`git --no-optional-locks grep -c '负责人裁决 2026-09-14|裁决（2026-09-14|2026-09-14 裁决|负责人授权' a3a343a4 -- docs lib contracts` 合计 **40 行**（任意 2026-09-14 命中 68 行）；`git --no-optional-locks grep -n 'DISP-PSF-FAST' a3a343a4` → **0 命中**、`grep -rn 'DISP-PSF-FAST-00[1-4]' docs/` → 0 ⇒ PSF-FAST 族连 DISP 号本身都未进 STANDARDS_REGISTRY，比子代理口径更硬
- 判据：权威登记面仍零条目、援引与裁定原文仍由合同/头文件自证（自指未解）；准绳①（负责人设计意图）无法在仓内取证 ⇒ 按判据③属未闭合，A-44 待负责人裁
- 建议标记：STILL(登记面仍 0 条目；重锚 DATA_SEMANTICS.md:820-824、module_adapters.h:30)

#### [60] V9-N-08 — STILL（我抽核通过）
- 命令：两版 md5 比对（同 `3cdf38fb`，零改动）；只读 python 双跑「门口径」与「逐行口径」
- 我的独立核对：`grep -n 're\.S' tools/quality/contracts/check_comments.py` → **:34 `comments = re.findall(r'//.*|/\*.*?\*/', text, re.S)`** 原样 ⇒ 块注释匹配可跨整文件；抑制条件 :39 `if "冻结" not in c and "history" not in …`
- 关键输出：门口径下红文件数 **0 / verdict PASS**，而逐行口径实测 `lib/phase2/tests/synthetic_gate.cpp:5488` 的 `// F-V19R2-UPM-002：…` 确为违规；该文件首条「注释」匹配长度 = 222679 字符 = 整文件全长（「冻结」在偏移 8283）；含「冻结」的 lib 源文件 181/639；含 V19R2/V19R3 的 2 个文件 **2/2 被巨块注释抑制**；无「整文件被单条注释抑制」告警
- 判据：门仍是假绿（删掉那行注释它依旧永不红）⇒ 机制与量化数字与原报一致
- 建议标记：STILL(锚点未漂移 :34/:39；门位 ci/checks.json::CON-COMMENTS :773-782)

#### [37] M7-C-001 — FIXED（我亲测；D 组子代理回报如不同以本节实测为准）
- 时间线：账本记 FIXED(`dce8abd4`，verified_state=PARTIAL)。`git --no-optional-locks diff --stat 521095b8 HEAD -- lib/phase2/src/upm.cpp` 为空、`dce8abd4` 系 BASE 祖先 ⇒ 修复在扫描基线即已入库（非本窗口修复）。
- 机制复算（原「两侧无一致性门 ⇒ 静默错格架」是否消失）：`lib/phase2/src/upm.cpp:245` `if (cfg.grid != 8) return 3;   // M7-C-001: UPM 网格常数不一致 → 显式拒绝`（我实测）；:260-262 `m->grid = cfg.grid;`（注释「实际 G 进入几何/模型（已校验 ==8）」）+ `m->cell_side = 512 / m->grid;`；打开侧 :1064-1065 `m->grid = (int)j.value("grid", 8); if (m->grid != 8) { delete m; return 1; }   // M7-C-001 打开处网格校验`；几何 hash :1354 `payload += "grid=" + std::to_string(m->grid) + ";"` ⇒ 原报「hash 只写自家常数、不含实际 G」的站点已改；传播侧 `lib/phase2/src/stage2_common.cpp:466` `mcfg.grid = cfg.control_grid_per_tile;   // M7-C-001: UPM G 必须 == 采样器 G（UPM 侧校验）`，`lib/phase2_session/p2_session.cpp:191` 与 `lib/core/src/module_adapters.cpp:3738` 同样把采样器 G 传入 ⇒ 一致性门成立（fail-closed）。
- 能红的证据：回归测试 `lib/phase2/tests/synthetic_gate.cpp:221 TEST(Phase2Upm, ControlGridMismatchRejected)` 体实测 `bad.grid = 4; EXPECT_NE(p2_upm_build(...), 0); EXPECT_EQ(model, nullptr);` + 默认 grid=8 分支 `ASSERT_EQ(..., 0)` ⇒ 双向往返；载体已注册：`lib/phase2/CMakeLists.txt:77-78` `add_executable(phase2_synthetic_gate tests/synthetic_gate.cpp)` 链 `phase2`、:105 `gtest_discover_tests(phase2_synthetic_gate …)`。头文件口径同步：`lib/phase2/include/astro/phase2/upm.h:92-95` 注释「必须 == 采样器 control_grid_per_tile 且 == UPM 网格常数 8；不等时 p2_upm_build* 返回 3」。
- 是否修全（两处残留，均不改判）：①配置面仍宣称可配 1..64（`stage2_common.cpp:40-45` 只校验 1..64；`docs/development/CONFIG_SCHEMA.md:11` 与 `docs/contracts/DATA_SEMANTICS.md:1444` 仅写「默认 8」而未标「实际仅 8 可用，其余值在 build 期 rc=3 拒绝」）⇒ 与 M6a-C-004 同型的「键面口径未随硬拒订正」；②失败点在 build 期而非配置校验期，`astrocs-stage2` 配 G=4 会跑完采样才在 UPM 建模型处失败（原报所列 `reports/REAUDIT_V3/scripts/stage2_*.json` 的 G=4 配置现变为显式失败而非静默错位，风险性质从「数值污染」降为「可用性」）。
- 建议标记：FIXED(dce8abd4::upm.cpp 三站校验 + geometry_hash 纳入实际 G + stage2_common/p2_session/adapters 传播 + ControlGridMismatchRejected 双向往返)；残留②条建议登记为口径订正类子项（CONFIG_SCHEMA.md:11 / DATA_SEMANTICS.md:1444）

### D 组（索引 30–36）— 子代理未交付结论，本组七条由我本人实测落档

#### [30] M6a-D-008 — STILL
- 命令：module_adapters 唯一真实入口 grep；snr_estimator README/memory 与 phase1/noise README 唯一宣称 grep；根 CMakeLists 488-500 行读取；两实现 wc -l + md5 比对；p1noise CMakeLists 36-56 行读取
- 关键输出：三处「唯一」宣称并存互斥 —— module_adapters.cpp:2613「op: estimate_snr（唯一真实入口 NoiseModel::estimate; SCI-NOISE-001 公式）」（原锚 :2142 漂移 +471）、lib/snr_estimator/README.md:5-6 与 memory.md:35-36「现状唯一生产实现 lib/snr_estimator/cpp/src/noise_model.cpp（466 行）」（实测现 475 行，行数亦失实）、lib/phase1/noise/README.md:28「snr_science.cpp 唯一权威科学实现（已编入 astrocs_phase1_noise）」
- 构建面反证：CMakeLists.txt:495-498 的 astrocs_phase1_noise 源集 = lib/phase1/noise/noise_model.cpp + lib/phase1/noise/snr_frame_science.cpp + lib/snr_estimator/cpp/src/snr_science.cpp ⇒ 生产静态库编译的是 217 行的全帧 median+MAD 实现（:153 注释），而 snr_estimator 的 475 行 patch 级三层实现（:54 稳健尺度 1.4826×median(|x−median|)、:70 patch sky 收集）**不在生产源集**；两文件 md5 不同（c8c73d0b… / b0737595…）⇒「两数值不同实现各挂唯一」原样
- 附带：tests/p1noise/CMakeLists.txt:37 仍自述「被测面: 现状唯一生产实现 …noise_model.cpp」，:40 注释目标名 astrocs_p1_noise_prod 而 :55 实名 p1noise_under_test ⇒ 目标名不符仍在
- 时间线：BASE..HEAD 对 CMakeLists 该 target 零改动 ⇒ 非本窗口整改
- 建议标记：STILL(重锚 module_adapters.cpp:2613 + snr_estimator/README.md:5-6 + memory.md:35-36 + phase1/noise/README.md:28 + CMakeLists.txt:495-498 + tests/p1noise/CMakeLists.txt:37/:40/:55)

#### [31] M6a-F-001 — STILL
- 命令：healpix_core.h/THIRD_PARTY_NOTICE 的 mismatch=0 宣称 grep；tests/unit/CMakeLists.txt 与 ci/ctest_baseline.json 的 oracle 测试注册检索；python 扫 135 项 checks 是否引用 common/healpix；git ls-files 检索 oracle jsonl
- 关键输出：healpix_core.h:6-8 仍称「由 astropy-healpix 作为外部 Oracle …(order 0..22) 交叉验证, mismatch=0」，STANDARDS_REGISTRY.md:135 的 §5.3 CONFORMANT 证据行列 test_healpix_oracle.cpp + snr_hips_spatial_oracle.py；实测 tests/unit/CMakeLists.txt:612-614 只注册 r9b_healpix_neighbors（源为 test_healpix_neighbors.cpp），ci/ctest_baseline.json:173 亦只有该项 ⇒ test_healpix_oracle.cpp **未挂 ctest**；其宿主仅 lib/common/Makefile:26-27 与 lib/astro_image_io/tests/sanitize_wsl_v4/v5.sh（手工 ASan 脚本）；135 项注册检查引用 lib/common/healpix 的门 **0 项**
- 仓外输入依赖：test_healpix_oracle.cpp:2 用法行要求外部 oracle.jsonl；生成器 gen_spatial_fuzz.py:2 标 NON_PRODUCTION_TOOL_ONLY；HEAD 面唯一同名资产在归档 engineering/control/archive/…/P13-003/sanitizer/（非活动输入且只 order7）⇒「百万点 0..22 mismatch=0」在仓内无可重跑通路
- 建议标记：STILL(重锚 healpix_core.h:6-8 / STANDARDS_REGISTRY.md:135 / tests/unit/CMakeLists.txt:612-614)

#### [32] M6b-C-002 — STILL（代码已修、冻结文档仍记「现状缺陷」）
- 命令：PHASE2_INTEGRATION.md 的 sup_max/DISP-P2INT grep；integrate.cpp:40-58；p2_output_semantics_test.cpp 的 B2-A7 grep；BASE..HEAD diff（三文件均空）
- 关键输出：代码真源 integrate.cpp:44-50 已按「B2-A7: canonical reducer = max(accepted support)…故 sup_max 必须在权重分支之前更新」实装（:49-50 在权重分支 :51-57 之前，:56 才是 w==0 continue），回归门在位：p2_output_semantics_test.cpp:85「4b) B2-A7 回归门」、:109「4c) B2-A7 反向边界」、:140 PASS 横幅已含该语义
- 文档仍按修前状态登记：PHASE2_INTEGRATION.md:42「sup_max | max(accepted support)（现状=R3-A 缺陷语义，§11.3）| integrate.cpp:61-61」（实际更新点 :49-50，:61 另有所指）、:64「sup_max 更新 | :54-55 | …位于 :49 continue 之后（DISP-P2INT-001）」（现 :54-55 是权重 finite/负值门，:49 是 sup_max 自身）、:113 伪码注「现状位置→DISP-P2INT-001 :54-55」、:170「实现现状（sup_max 在 w==0 continue 之后，:54-55）」、§11.3 :234-243 仍列「现状缺陷清单…整改: sup_max 更新移至 w==0 continue 之前」——该整改已完成；DISP-P2INT-002 亦已过期（SCI:58 确为 max over valid,W>0，header:17 为 max(accepted support)，代码现走 header 口径）
- 建议标记：STILL(重锚 PHASE2_INTEGRATION.md:42/:64/:113/:170/§11.3:234-243；真源 integrate.cpp:44-50 + 门 :85/:109)

#### [33] M6b-E-007 — STILL
- 命令（三级）：IO_001 §3 表 24-33 行；逐路径 git cat-file -e；git ls-files modules/services/io；ls docs/interfaces/data；INDEX.yaml DATA-001 检索
- 关键输出：合同 :27 仍列「io 服务模块骨架（README/module.yaml/CMake/占位入口）| modules/services/io/」、:30 仍列 io_module_api_v1.h；实测四者全 NOT_IN_HEAD（README.md、module.yaml、CMakeLists.txt、io_module_api_v1.h），git ls-files 该目录只有 4 个文件（两公开头 + 两自检）⇒ 比原报「4 中 3 缺」更重（骨架三件套 + 占位 ABI 头全缺）
- 连带：docs/owner/RELEASE_STATUS.md:79 仍以 IMPLEMENTED 记「FITS 流式接口 | IO-001（接口 + 实现 + 契约测试）」；docs/interfaces/data/ 实存 DATA-002/003/004，无 DATA-001_ARTIFACT_CONTRACT.md，而 INDEX.yaml:763 注释仍称「DATA-001 冻结」⇒ **锚型订正 + 换宿主**：`INDEX.yaml` 确无 id: DATA-001 条目（只 :763 注释提及），但悬空引用真实存在于机器合同 `contracts/data/artifact_types.registry.json:6` 的 doc_ref（另 :7 note、`contracts/data/artifact_manifest.schema.json:5` description），目标文档 `git cat-file -e HEAD:docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md` = NOT_IN_HEAD，且 HEAD 面 136 项注册检查无任何门校验 doc_ref 可达性 ⇒ 子站成立、主站（§3 清单与实存不符）不变；D 组「全树 grep=0、该子句不可复现」系把检索限定在 INDEX/docs/interfaces 所致，已在 §4.1 推翻
- 建议标记：STILL(重锚 IO_001 §3:27/:30 + contracts/data/{artifact_types.registry.json:6-7, artifact_manifest.schema.json:5} 三处悬空 doc_ref；DATA-001 子站换宿主后仍成立)

#### [34] M6b-I-002 — STILL
- 命令：SCIENCE_FREEZE.md:1-8；ls docs/validation/ACCEPTANCE_GATES.md；git ls-files evidence/performance 计数；BASELINE.md:10-22；RELEASE_STATUS.md:28-32/:70-72
- 关键输出：SCIENCE_FREEZE.md:3-5 仍以现在时宣称「V17 Round0-6 clean-tree 终验已完成…ACCEPTANCE_GATES.md G1-G10 全部 PASS、known P0/P1 = 0，ASTROCS_FOUNDATION_FINAL_FREEZE = PASS」，而 docs/validation/ACCEPTANCE_GATES.md **不存在**、evidence/performance 跟踪文件数 **0**（BASELINE.md:12 仍写「完整数值见 evidence/performance/*.json（V14 交付）」并紧接 :14-21 现在时给 V14 秒数）；owner/RELEASE_STATUS.md:31 明文「历史口径 PASS…自 DOC-CONV-001 起由本表取代」⇒ 复活已废止口径且无定位性（无 SHA/rc=0/当轮命令）
- 正对照（证明是纪律未贯彻）：RELEASE_STATUS.md:70-72 的 IMPLEMENTED 行均自带命令 + rc=0 + commit 短码（如 ctest rt001_unique_executor PASS（RT-001 91440c16）），冻结页未采同标准
- 建议标记：STILL(重锚 SCIENCE_FREEZE.md:3-5 + BASELINE.md:12/:14-21；对照 RELEASE_STATUS.md:31/:70-72)

#### [35] M7-A-122 — STILL
- 命令：NOISE_MODEL.md 的 ivar/pixel 单位 grep；DATA_SEMANTICS.md 的 ivar 单位 grep
- 关键输出：NOISE_MODEL.md:16 符号表仍「ivar | 1/variance (pixel⁻²·ADU⁻²)」，与同篇 :29「ivar: ADU⁻²」、:76「variance [ADU²] → ivar [ADU⁻²] 倒数关系精确」、:99 互斥；DATA_SEMANTICS.md:413-414 亦记「variance: ADU²；ivar: ADU⁻²」、:326「1/ADU²（=1/variance）」⇒ 多出的 pixel⁻² 因子与全部合同不符，且该符号表行被 §7 量纲不变量引用
- 影响复算：现行实现按 ADU⁻² 直接用 ivar 作权重（integrate.cpp 权重分支），故属文档错而非已发生的数值事故 ⇒ 定级不上调
- 建议标记：STILL(锚点未漂移 :16 ↔ :29/:76/:99 + DATA_SEMANTICS:413-414/:326)

#### [36] M7-A-130 — STILL
- 命令：STAR_PSF_ALGORITHMS.md 的 1.230310 grep；MOFFAT4_FWHM_FACTOR 全域 grep；dpsf_psf.cpp:391-395；PSF.md 的 FWHM grep
- 关键输出：ALG:75 仍「post: FWHM=1.230310·sx/sy」、:151 仍「FWHM=1.230310·sx/sy（F2 系数 MOFFAT4_FWHM_FACTOR :24）| :339-340」——把两轴宽度写成除法串，与同篇 :54 各向同性推导「FWHM=2√2σ·√(2^(1/4)−1)=1.230310σ」及 :7 不自洽；实现 dpsf_psf.cpp:393-394 为 fwhm_x = FACTOR*sx、fwhm_y = FACTOR*sy（逐轴**乘法**），SCI PSF.md:20 记「fwhm_x/y | 轴向 FWHM 1.230310·s」正确 ⇒ 错只在 ALG 两处
- 锚漂移：ALG:151 所指代码行现漂至 :393-394，且注内常量行号写 :24 而实测 :25 ⇒ 逐符号锚表随之错位（与 M6b-C-002 同族）
- 建议标记：STILL(重锚 ALG:75/:151；正对照 PSF.md:20 与 dpsf_psf.cpp:393-394)

### E 组其余（子代理交付，我逐条抽核取证）

#### [38] M7-I-201 — STILL（我抽核：两锚逐字复现）
- 命令：`sed -n '51,53p' docs/science/SCIENCE_SCOPE.md`；`sed -n '80,82p' docs/science/CALIBRATION.md`；`git --no-optional-locks diff --stat 521095b8 HEAD -- docs/science`（空）
- 我实测：SCOPE:53 仍「默认 FP64 科学计算；FP32 仅显式等价路径（SparseEqualsDense 1e-12 门）」；CALIBRATION.md:80「## 9 精度策略」/:82 仍「母版算术与校准核心为 FP32（float）」；建议的「SCOPE 改分层精度表」未做（全 docs 检索「分层精度」0 命中）；SparseEqualsDense 的权威页是 PHASE2_UPM.md（UPM 域专属门），不能当校准路径的等价性证明 ⇒ 两 SCI 页入口级互斥原样
- 建议标记：STILL(锚点未漂移 SCOPE:53 ↔ CALIBRATION §9:82)

#### [40] M8-F-009 — 部分修（1 修 2 存），建议账本 PARTIAL
- ①已修（adaeb531）：`cli/resource_gate.h:97-98` 现为 `kCpuP50MinPercent = 90.0;` / `kCpuMeanMinPercent = 85.0;   // §18.2 冻结均值下限 85%`、:213 注「旧 D.6 的 0.80 已被 §18.2 明文废止」、:217 判据改用 `kCpuMeanMinPercent / 100.0` ⇒ 原报「把废止系数 0.80 钉成期望值」消失；期望值同步（cpu_monitor_test 用 1.7/0.85 组合）并新增绝对值边界锁（p2_workers_test 的 3.4/3.39 双向、tests/cli/test_resource_gate.py 的 0.85 文案断言）
- ②未修（我实测）：`tests/unit/p2_seam_gate_test.cpp:71` 仍 `g.avg_equivalent_cores = compute_cores_threshold(g) * 1.2;  // 超阈值` ⇒ 通过输入由被测函数自算，阈值系数怎么改都恒过；该文件 BASE..HEAD diff 空且仍在 ctest 基线
- ③未修且多一面（我实测）：`ci/tests/test_ci001_failclosed.py` 的 `test_registry_gate_flags_removed_by_owner_ruling` 仍断言「全部 requires_monitor 检查命令均不含 --gate-required」（把「注册表不含判定请求」钉成期望），而 `grep -c 'gate-required' ci/checks.json` = **0** ⇒ 该断言永真；且该文件不被 CI 采集（CI-BINDING-TESTS pattern 只收 test_ci001b_*.py，我实测）⇒ 「退不改假绿」与「元门永不运行」同在；§10.5/§18.2 判定在 CI 内仍是 heavy 只采样、零判定
- 附带文字面：0.80 旧口径注释残留 13 处（tests/backend 与 tests/unit/mon001_gate_test.cpp），属 §5.3 一致性非断言错误
- 建议标记：PARTIAL(①FIXED(adaeb531) + 残留②p2_seam_gate_test.cpp:71 + 残留③test_ci001_failclosed.py:128/137/152 与 CI-BINDING-TESTS 采集缺口)

#### [42] M8a-E-002 — STILL（我抽核：路径不存在已三级复核）
- 命令：`grep -n '设计文档' -A6 lib/healpix_db/healpix_browser_qt/README.md`；对 README 内每个 docs 路径逐个 `[ -f ]` 测存；`git --no-optional-locks ls-files | grep -iE 'cpp-qt-browser|superpowers'`（0）；`ls docs/superpowers`（不存在）；`git --no-optional-locks log --diff-filter=D --all -- '*superpowers*'`
- 我实测：:127/:128/:129 三条引用（specs 两份 + plans 一份）全部 MISSING，:130 的 memory.md 实存；全仓 tracked 面无该三份文件、无改名搬迁记录；活动树内的实际替代页 `docs/browser/HIPS_BROWSER.md` 与 `docs/modules/healpix_browser_qt.md` 存在但 README 不指它们 ⇒ 悬空引用原样
- 建议标记：STILL(重锚 README::设计文档:127-129；同族他站一并清)

#### [43] M8a-G-006 — STILL（覆盖面我另测为 5/41）
- 命令：`git --no-optional-locks diff --stat 521095b8 HEAD -- tools/check_module_readmes.py`（空）；`grep -c '^    ("lib/' tools/check_module_readmes.py`（**5**）；python 统计 tracked 的 module 级 README.md 数
- 我实测：门仍只检硬编码 5 份（lib/phase1/{stars,wcs,photometry,noise} + lib/phase3_session），判据为「合同 ID 字样正则 + 引用文件名字面子串 + 存在性」三项；tracked 的 module 级 README.md 实测 **41** 份（另 101 份含各级），即覆盖比 ~5/41 ⇒ 与子代理按免报区算出的 5/43 同量级，结论不变：确数覆盖极低、要素核验 0 项、门自身静态重放 errors NONE（放行）
- 建议标记：STILL(重锚 MODULES:12-17 / main:29,31-44,45-46 + ci/checks.json::MODULE-READMES)

#### [44] M8a-I-006 — STILL（我抽核：GLOSSARY 确无该词条）
- 命令：`git --no-optional-locks show a3a343a4:docs/GLOSSARY.md | wc -l`（**25**）、`| grep -ci entrypoint`（**0**）；`grep -n 'entrypoint' docs/contracts/MODULE_MAP.md docs/governance/*.md`（0）
- 关键输出：术语缺口未裁；同词两义全域混用（manifest 字段值 vs registry 入口措辞），我另测 lib/drizzle/module.yaml:34 与 lib/calibration 的 README↔module.yaml 冲突站仍在（见 A 组 [10] 与我此前对 M2a-C-4 的实测：module.yaml 记 entrypoint: MISSING 而源码/注册表俱在）
- 建议标记：STILL(重锚 GLOSSARY.md 全文 0 词条；A-32 裁决前不定档)

#### [45] M9-C-3 — STILL（我抽核：三套口径同时可见）
- 命令：`grep -n 'arcsec|_as|fov_' lib/plate_solve/cpp/ipv/src/ipv_polygon.cpp`；`grep -n 'fov_diag' lib/plate_solve/cpp/ipv/include/ipv_polygon.h lib/plate_solve/cpp/ipv/include/ipv_robust_refine.h`
- 我实测：polygon.h:42/:98/:114 三处形参注释仍「FOV 对角线 (角秒…)」，而 `ipv_robust_refine.h:110` 同概念写 `fov_diag_deg - FOV 对角线 (度)`、`ipv_types.h:108/126` 另有 `s0 (arcsec/pixel)`、`rms_arcsec` ⇒ 同库内角秒/度双口径未收敛；生产选择器（ipv_select）传度（见同组 ipv_wcs 侧证据），而 polygon 侧 `is_wide_fov = (fov_diag > 3.0 * 3600.0)` 一类裸 3600 换算（子代理实测 :633）仍是 3600 族复发点；两函数无生产调用方 ⇒ 维持「一旦接线即错档三个数量级」的潜伏定级
- 建议标记：STILL(重锚 ipv_polygon.h:42/:98/:114 + polygon.cpp 的 3600 换算站 + 对照 ipv_robust_refine.h:110)

<!-- 待回填：结论总表 + 统计 -->

## 三、统计与交付要点（已回填）

### 四态统计（61 条）
- **STILL: 54**（总表口径；其中 [40] M8-F-009 属「三事实 1 修 2 存」，账本建议记 **PARTIAL** 而非纯 OPEN ⇒ 纯 STILL 53 条；[14] M3-C-002 账本原已 PARTIAL，本片维持不升档，判语仍为 STILL）
- 合计校验：STILL 54 + FIXED 6 + 缺失确认 1 = **61**（= 分片实测条数；MOVED 0、CANNOT_STATIC 0）
- **FIXED: 6** — [6] M2a-C-1(`dce8abd4`)、[15] M3-C-010(`b0353303`)、[37] M7-C-001(`dce8abd4`)、[39] M8-F-001(`adaeb531`)、[41] M8-H-001(`348ca1e8`)、[57] V6-N-05(`3d8e04db`)
  - 6 条中 **5 条的修复提交都是扫描基线 `521095b8` 的祖先**（`merge-base --is-ancestor` 逐个实测：dce8abd4/b0353303/adaeb531/348ca1e8 均 YES，仅 `3d8e04db` 为 NO ⇒ 落在本窗口内 ⇒ 唯一「本轮真修复」= V6-N-05；[41] 另建议按**误报**撤竞态本体）。
- **MOVED: 0**（本片无「位置变缺陷原样」型：漂移站点一律按 STILL + 重锚处理）
- **CANNOT_STATIC: 0**（61 条全部可静态判定，无一需要运行期证据）
- **四态外（缺失确认 / 建议 REJECT）: 1** — [52] V12-N-14（锚点在 HEAD/BASE/全史/影子树四面无载体，唯一 `git log -S` 命中是该 finding 自身提交 17fa5ff7）

### 待负责人裁决项（本片涉及，不影响四态判定）
- [2] M1a-C-001 与 [1] M1a-A-003：A-07（CAR/AIT CRVAL2 语义）、A-09（SIP 1e-4 门适用路径）
- [12] M2b-C-01：A-22（DISP-HIPS-004 二选一，负责人已注「不是能力缺口」）
- [43]/[50]/[54]/[55]/[60] 的机器门类：A-44（`check_comment_anchor_exists` 等门计划）
- [44] M8a-I-006：A-32（entrypoint 术语权威定义）
- [59] V8-N-07：A-44a/b 无条目 ⇒ 「裁决 2026-09-14」自指未解

### 建议账本标记（仅建议，不回写；逐条见第一节总表末列）
1. 维持 FIXED：M2a-C-1、M8-F-001（但补两条残留：`runtime/module_loader/README.md:39` 的「36/36 PASS」口径未订正；`ci/validate_registry.py::R11` 在 135 项注册检查与两个 workflow 中零载体 ⇒ 防复发门本身永不执行）。
2. M7-C-001 判 FIXED，但 `verified_state` **维持 PARTIAL**（收工前收回我先前的升档建议；同类他站 5 处在位：配置面仍校验 1..64（`stage2_common.cpp:40-45`、`sampler.cpp:499/:594-595`）、新 `rc=3` 未进 `PHASE2_UPM_IMPL.md` 的 rc 表（实测 `grep -c 'rc=3'` = 0）与 `PUBLIC_API.md:1784-1790` 枚举、`CONFIG_SCHEMA.md:11`/`DATA_SEMANTICS §23.1:1444` 仍宣 G 可配、`module_adapters.cpp` upm-fit 缺键静默按 8（=V3-N-03）、IR 通道 `p2_sampler_default_config()` 忽略用户 G；且 `lib/phase2/tools/stage2.cpp:434-435` 只报 "UPM build failed (no detail)" ⇒ 8 份 G=4 历史配置现变为无诊断硬失败。原句保留如下：（build/open 双侧校验 + 实际 G 进几何与 hash + 双向往返回归测试均已入库注册），残留另立口径类子项（`stage2_common.cpp:40-45` 仍只校验 1..64、`sampler.cpp:499`、`CONFIG_SCHEMA.md:11`、`DATA_SEMANTICS.md:1444`、`PHASE2_SAMPLER.md:46`）。
3. M3-C-002 **不升档**，但证据列「cpp/src 全文 grep 零命中」须订正为「21 行命中（BASE 同数）」——原证据失实，判定不变。
4. 由 OPEN 改判 FIXED 两条：M3-C-010（`b0353303` 删声明；残留 `production_call_paths_stage1.csv:5`）、M8-H-001（`348ca1e8` 起 18/18 写点受 `#pragma omp atomic`，裸写 0；竞态本体建议按误报撤，残余转 M2a-F-3）。
5. 建议 REJECT/撤案一条：V12-N-14。
6. 需重锚的高频漂移站（供前台批量改锚）：`module_adapters.cpp`（如 :2142→:2613）、`drizzle_engine.cpp`（+364，:745/:870/:1578→:769/:894/:1666）、`dpsf_psf.cpp`（+19）、`ipv_select.cpp`（+56）、`ipv_types.h`（+13）、`pc_api.cpp`（:331/695/969→:340/717/990）、`synthetic_gate.cpp`（+59）、`DATA_SEMANTICS` §15.2/§18（+34/+36）、`packaging/README.md`（:17→:37）、`gaia_client.c`（原 :1313/:1360/:1362→:1541/:1670/:1811 区）。
7. 原 finding 文本需「证据链更正」（不改四态）7 处：M3-C-002 的零命中失实；M4-C-04 的「λ0=0 加剧」已失效（现 1e-3）；M4-F-03 的 TEST-UPM-003/004 登记面论据作废（真门在 TEST-UPMW-001..007）；M5b-E-03 的 module_adapters 三站 + API-001:19 两子站自基线即不成立；V10-N-09 的「负值→静默 0」（回退实为 (v>=0&&v<=3)?v:INFO）；V13-N-05 的 28→实测 27 纯零 + 1 档案文案；V6-N-05 修复时点（基线即已…→ 3d8e04db 在窗口内，此条反向我修正了子代理）。
8. 建议**新增登记**的复验期新发现：①`ci/validate_registry.py` 无 CI 载体（可并入 M8-G-001/V4-N-04/V9-N-14 的「有判据无载体」门族）；②`ci/tests` 21 个 test_*.py 因 `CI-BINDING-TESTS` pattern 只采 2 个（同根于 M8-F-009③）；③M6b-E-007 的 io 骨架实测四件全缺（比原报三件更重）；④V3-N-01 的 `-14` 与 M2a-H-4 的 f64 降级同源，属 fail-closed 整改「新码未回带合同」。
9. 窗口归因结论：61 条涉事文件在 `521095b8..a3a343a4` 内被改动的仅 8 个（module_adapters.cpp、gaia_client.c、drizzle_engine.{h,cpp}、hp_drizzle_api.{h,cpp}、ipv_api.h/ipv_types.h/ipv_select.cpp/ipv_select.h、lib/phase1/noise/README.md、DATA_SEMANTICS.md、ci/checks.json、tests/unit/CMakeLists.txt），逐条复算后**仅 V6-N-05 一条因窗口内提交闭合**，其余改动均属他域（P17 性能、P22 归约解耦、P27 dead-params、ABI 版本字段、ctest 登记），未触及任何原缺陷机制。
10. 时点一致性：任务书时点 `a3a343a4` 之后 HEAD 持续推进（本片复核期间至 `d500babf`），但 `git --no-optional-locks diff --name-only -z a3a343a4..HEAD` 的 203 个文件中非 `问题扫描/` 档案面为 **0** ⇒ 61 条在 a3a343a4 的结论对当前 HEAD 等价（组 E 亦独立以路径限定检索复现 0 命中）。


## 四、D 组交付对表 + 时点订正（收工前）

### 4.1 D 组（fae9bf7c）交付与我实测的对表
- 结论一致：D 组 8 条 = 7 STILL + 1 FIXED(M7-C-001)，与我本人实测逐条同判 ⇒ 本片 61 条无跨组冲突。
- D 的**新增证据经我复核成立并已采纳**：①「唯一」宣称站由 2 增殖到 4 —— 实测 HEAD 面 `lib/snr_estimator/cpp/src/snr_science.cpp:9`「本 TU 是 SNR 科学量的**唯一权威实现**」与 `lib/phase1/noise/snr_frame_science.h:6`「唯一权威实现: …snr_science.cpp (P5-SNR, 负责人授权)」逐字命中 ⇒ [30] 判据加强；②权威文档自身互斥 —— `docs/algorithms/NOISE_ESTIMATION.md:226`「### 13.5 遗留通道与迁移旧符号（非生产）」与 :231/:127「随 `astrocs_phase1_noise` 静态库编译…并进主程序」并存，而 `module_adapters.cpp:2613` 仍称 `NoiseModel::estimate` 为唯一真实入口 ⇒ [30] 三向互斥成立；③`lib/phase2/src/integrate.cpp` 现 **81 行**而 `PHASE2_INTEGRATION.md` 有 3 处仍称「76 行」(:4/:49/:183) ⇒ [32] 行数失实计入判据；④`healpix_fullsky_oracle.py` 从未存在（tracked 面 0 命中，HEAD 内唯一引用站是 `test_healpix_oracle.cpp:4` 的注释）⇒ [31] 判据加强（连生成器本体都是虚构）。
- **我推翻 D 的一处结论**：D 称「DATA-001 子句已不可复现（全树 grep=0）」系**漏站**——实测 HEAD 面 `git grep -n 'DATA-001_ARTIFACT_CONTRACT' HEAD -- contracts` 命中 3 处：`contracts/data/artifact_types.registry.json:6`（`"doc_ref": "docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md"`）、`:7` note、`contracts/data/artifact_manifest.schema.json:5` description，而该文档 `git cat-file -e HEAD:…` 为 NOT_IN_HEAD ⇒ **断链事实成立**，只是宿主是机器合同 JSON 而非 `INDEX.yaml`（D 与我的初测都把检索限定在 INDEX/docs/interfaces，属同一盲区）。补强点：全仓 136 项注册检查中**无任何门校验 doc_ref 可达性**（python 扫 registry 类命令 0 命中）。⇒ [33] 维持 STILL 并把该站并入证据。
- D 的 [37] 残留 5 处我复算：②「新 rc=3 未进权威 rc 表」实测 `git show HEAD:docs/algorithms/PHASE2_UPM_IMPL.md | grep -c 'rc=3'` = **0**（表内只有 rc=1/rc=2 站）；`lib/phase2/tools/stage2.cpp:434-435` 实测 `log("UPM build failed"); fprintf(… "(no detail)")` ⇒ 采纳。⇒ **[37] 的 verified_state 建议由「升 VERIFIED」收回为「维持 PARTIAL」**（同类他站仍在：配置面 1..64、rc=3 未登记、upm-fit 缺键静默按 8、IR 通道忽略用户 G、文档仍宣 G 可配）。
- 其余细节：`lib/gaia_xpsd_client/test/test_gaia_race.c` 仍在树且仍零注册；E 组所报 0.80 残留站名与计数经我逐条核验为真（`tests/backend/{fixture_common:124,test_p3006:*2,test_p2007:*3,test_p2003:162}` + `tests/unit/mon001_gate_test.cpp` 7 命中 = 14 处，E 报 13 处属计数口径差）。

### 4.2 时点订正（推翻本档开头的时点注）
- 本档先前写「a3a343a4..HEAD 的 203 个漂移文件全部在 `问题扫描/` 内 ⇒ 零风险」。**该判断已被后续提交推翻**：收工时 HEAD=`6d202dc8`，`git --no-optional-locks diff --name-only a3a343a4..HEAD` 的非 `问题扫描/` 文件数 = **180**（含 P23 HiPS 重构把 `lib/core/src/module_adapters.cpp` 由 6509 行改到 6322 行、`hp_drizzle_api.cpp` ±31 行、`tests/unit/CMakeLists.txt` +29 行、`ci/checks.json`、`docs/contracts/{PUBLIC_API.md,DATA_SEMANTICS.md}`）。
- 对策：对全部「宿主已变」条目在 **HEAD blob** 上复测，逐条结果 —— [8] 任务号注释簇仍在（`hp_drizzle_api.cpp:946/:967/:971/:1005`，`drizzle_engine.cpp:1890` 未漂）；[10] `hp_drizzle_api.cpp:1030` 仍 `varianceConv[i] = (float)vd[i];`、:1011 仍 `return -14`；[11] `aio_hips_writer.cpp:943` 仍 `3600.0 * 180.0 / kPi() * std::sqrt(kPi()/3.0) / nside` → `:984 hips_pixel_scale`；[12] `git grep -cE 'rename|fsync' HEAD -- lib/astro_image_io/src/hips/` 仍 0 命中（tmp 命中仅在 reader 的局部缓冲）；[21] `tests/unit/CMakeLists.txt:679-681` 仍 `add_executable(p2_upm_synthetic_test …)` + 只链 `astrocs_contracts`；[30] 两站漂 1 行（blob :2614/:2618 → 工作树 :2613/:2617）；[56] `PUBLIC_API.md:238-244` 帧通道清单仍止于 `-13`，`grep -n 'rc=-14|返回 -14'` 0 命中；[58] `module_adapters.cpp:2966` 仍 `const int nested = p1_int(dj, "nested", 1);` ⇒ **61 条在 `a3a343a4` 与 `6d202dc8` 同判，无一翻案**，仅需按新行号更新锚。
- 计数订正：`ci/checks.json` 在 HEAD 面为 **136** 项检查（我先前报的 135 是**工作树脏读**）。工作树未提交地把 3 道非豁免 `linux-main` 门（`CTEST-MON004-ENFORCEMENT`、`CTEST-RESOURCE-MONITOR-QUALITY`、`CTEST-COMPARE-PRODUCTS-QUALITY`）连同 `tests/unit/mon004_enforcement_test.cpp` 一并删除，并新增 2 道未入库门 `CTEST-IPV-DEAD-PARAMS-LOCK{,-SELFCHECK}`；四件套（测试源 + CMake 注册 + 门 + `ci/ctest_baseline.json`）在工作树删除是**自洽**的，但属在途变更 ⇒ 本片判定一律以 HEAD blob 为准。
- 本档中所有「135 项注册检查」的表述按 HEAD 面读作 136 项，不影响任何一条判据（相关命中数均为 0）。

### 本片取证纪律自查
- 全程静态：`read`/`glob`/`grep` + 只读 `python3`（文本/JSON/ast 统计，不执行仓内脚本）+ `git --no-optional-locks` 只读；未跑编译/cmake/ctest/tests/二进制/CI/仓库脚本，未做任何写入式 git 操作，未回写账本。
- 每条结论均给命令与关键输出；缺失判定一律三级复核（工作树 glob/grep → HEAD `git ls-files`/`git cat-file -e` → 改名/移动与全史 `--diff-filter=D`/`-S`）。
- 子代理交付的 6 组结论中，凡缺「机制复算 + 载体存在性」双证者由我实测覆盖（含对 D 组「DATA-001 子句不可复现」的推翻，见第四节）：M4-F-03（组 B 自相矛盾，按实测判 STILL，并否掉其「TEST-UPM-003/004 不存在」的伪证据）、M8-F-001（否掉组 E 的 `tools/quality/check_ci_registration.py` 与 `_ABI_EXPECT` 两项不存在的工件）、M8-H-001（改为按行级保护复算）、V12-N-14（改撤案）、V6-N-05（修复时点归因更正）、M6a-C-004（引擎行号 off-by-one 更正）。D 组子代理交付晚于我的实测，其 8 条与我同判（其 DATA-001「不可复现」一说被我 HEAD 面复测推翻，见 §4.1）。
