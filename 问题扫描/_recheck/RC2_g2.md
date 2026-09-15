# RC2 复验档案 · 第 2 片（g2，verify[1::8]，62 条）

- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（工作树与 HEAD 在全部真源区一致；脏文件 49 个均在 artifacts/evidence/reports/设计大纲，不影响本片判据；`docs/TRACEABILITY.csv` 工作树内容与 HEAD blob 逐字节一致，本片该文件一律按 `git show HEAD:` 读取）
- 分片来源：`问题扫描/_cache/recheck_round1.json` → verify[1::8]（62 条），ID 快照存 `问题扫描/_recheck/_g2_ids.json`，条目原文摘录存 `问题扫描/_recheck/_work/<ID>.md`
- 方法：每条按 文件::符号 重定位（不依赖旧行号）；复算原缺陷机制是否消失；FIXED 一律给出修在哪（commit + 文件::符号 + 机制证据）并判是否修全；缺失判定走三级复核（多口径 grep + 解释器解析 + 缓存/宿主档案）
- 统计：**STILL 54 ／ FIXED 7（其中部分修复 3）／ MOVED 0 ／ 不可复核 1（V12-N-17，建议前台按"锚失效待原轴补证"处理，四态表内暂记 CANNOT_STATIC 位）**

## 一、结论速览表（建议标记仅供前台回写账本，本档未改任何账本）

| # | ID | P | 四态 | 新锚（文件::符号/行） | 一句话依据 |
|---|----|---|------|----------------------|-----------|
| 0 | FD-F-002 | P1 | FIXED(部分) | cli/resource_recorder.h::(resource_summary 段):246-252；tests/unit/mon001_recorder_test.cpp:100-101 | adaeb531 把硬写字面量 true→false 并加 cpu_pct_units 键，谎报声明面已修；「子串 find(字面量)」守卫形态仍在，同族 FD-F-001（cpu_provider_test.cpp:73）原样 |
| 1 | L28e-E-001 | P1 | STILL | docs/TRACEABILITY.csv:39-64(HEAD)；tools/quality/v19r3_traceability.py:294/:311 | 16 枚 ID 仍 f-string 铸造+VERIFIED 登记，lib/tests/ci/tools 字面命中 0；TEST-UPMW-005 test_ids 列仍填诊断号 |
| 2 | M1a-A-006 | P1 | STILL | docs/science/ASTROMETRY.md::§3a:33/§6:103；STANDARDS_REGISTRY.md::D.catalog:45/:193；lib/gaia_xpsd_client/src/gaia_client.c:2072 | J2016.0 vs "J2000 同系"混用依旧；全链无 epoch 传播代码（grep 0）；ipv_api.h 无历元入参；pm/parallax 契约置 0 |
| 3 | M1a-C-004 | P0 | STILL | lib/phase3_session/p3_wcs.cpp::p3_wcs_make:83-140；module_adapters.cpp::p3_op_resample worker:5786-5789；PHASE3_PROJ_IMPL.md:382-384 | FOV≤20° 生产链仍零强制点（全仓源码 grep -i fov 仅 archive 注释）；ALG 现文反称"FOV 裁决属会话层合同"而会话层同样零命中 |
| 4 | M1a-E-001 | P1 | STILL | lib/plate_solve/module.yaml:3-8/:29-31；CMakeLists.txt:528；PLATESOLVE.md 符号表:121 区；ASTROMETRY.md:19 | module.yaml 仍断言"根 CMake 无 ipv 目标"而根图 :528 实有 astrocs_p1_ipv；12 导出锚/ALG/SCI 行锚全部仍错 |
| 5 | M1a-G-002 | P1 | STILL | docs/api/PHASE3_API_V1.md:3/:16/:24/:39；contracts/schemas/（仍缺）；tests/api/test_p3_api.py::test_01..06 | schema 文件仍不存在；门测试仍全是文档文本 assertIn；"仅 TAN"仍与宪章 §18.1 四投影(:221)冲突 |
| 6 | M2a-A-3 | P1 | STILL | gaia_client.c::query_spectrum_by_coords:2525；GAIA_QUERY.md:21-22；DATA_SEMANTICS.md:113；gaia_adapter_test.c::D5:577 | 平手仍"严格小于先见胜"；ALG 仍无任何 by_coords 判据小节；D5 仍 find_star 预筛全命中不测判据 |
| 7 | M2a-C-12 | P1 | STILL(残余面，原报即残余条) | DATA_SEMANTICS.md:121-123；lib/gaia_xpsd_client/README.md:40/:142；module_entry.c:802 | "未初始化"文本仍在；行 schema 仍把恒零 parallax/pmra/pmdec 作产品列下发；calloc 置 0 面(已修部分)保持 |
| 8 | M2a-C-7 | P1 | FIXED | lib/gaia_xpsd_client/src/gaia_client.c::block_budget_try_reserve:446 + 接线:2019（bd4bc23b，G3b） | 块缓存改客户端级共享总预算：insert 全路径经 try_reserve 判 client 总量；文档"全局≤4GB"口径转真；残 NULL=独立文件/测试路径（注释已声明） |
| 9 | M2a-E-2 | P1 | STILL | GAIA_QUERY.md:63(锚 :622-646 vs 实际 unproject@:901)/:197；gaia_client.h 头注（余 1 数值锚仍错）；README.md:149"2255 行" vs 实测 2700 行 | 系统性行锚漂移未订正且随并发提交继续放大（+128→+279）；"逐函数核对"自述仍不可复核 |
| 10 | M2a-G-2 | P2 | STILL | tools/check_data_artifacts.py:16-17；DATA_ARTIFACTS.md §3:102-103 | DATA_RE 仍只收 -NNN 后缀；DATA-P1-CAL 等无后缀登记项仍零覆盖；合同仍自称「全部登记、无重复」 |
| 11 | M2a-I-3 | P2 | STILL | docs/interfaces/data/DATA-003:16；DATA-002:6；DATA-004:6/:32 | 三份现行合同仍以 ARCHIVED_NON_NORMATIVE 的 AstroCS_ENGINEERING_CONSTRAINTS.md 为"约束来源/上游权威" |
| 12 | M2b-B-06 | P1 | STILL | aio_hips_writer.cpp::finalize_image_product:958/:983（+第二遍 properties :1200）；registry:96/:108/:246 | hips_status 仍两组词缺 clonable 组；hips_hierarchy 仍 "true"；registry 仍挂"可选/推荐键"判 PARTIAL、DISP-HIPS-003 TRACKED |
| 13 | M2b-F-02 | P1 | STILL | tests/unit/p1_hips_writer_test.cpp:79-80/:89/:94-95；aio_hips_writer.cpp:1129-1130（variance 分支仅判 flags） | "|| true"、两处"||"析取、同文件读两次当 hash 逐字在位；variance/ivar present 守卫仍不对称（diag 通道判据模式未扩散） |
| 14 | M3-A-003 | P1 | STILL | calibrator.cpp::calibrate:122/::calibrate_d:165（仍 max(flat,0.1)）；::normalize_flat:80（零调用方）；CALIBRATION.md §5:44-53；README.md:75 | SCI 公式 vs 直调语义差 median(flat) 倍依旧；前提仍只在 ALG"约定"；两条通道均不校验 median≈1（p1_master_flat_valid 只判 >0） |
| 15 | M3-C-005 | P1 | STILL(残余面) | lib/phase1_session/p1_session.cpp::(calibrate 段):306-345 仅尺寸校验；对照 module_adapters.cpp::p1_master_flat_valid:1304-1332 + 调用:1385 | IR 通道 fail-closed 保持；会话通道仍无退化校验直传 ac_calibrate_frame；ac_calibrate_frame 边界仍零判据；注：原报 cosmetic 路径 real_ingest.cpp 不存在（PATH_MISMATCH，建议订正锚而非撤条） |
| 16 | M3-E-001 | P0 | FIXED(部分) | 权威面已修：docs/traceability/TRACEABILITY_MATRIX.json::MOD-astrocs-phase1-calibration test_id/test_path=p1cal+test_calibration_oracle.py+master_flat_median；src_path 补 ac_correct_frame（c3452d48） | 但 6 实例中 4 个未动：docs/TRACEABILITY.csv:20 仍指 test_photometry_apply.cpp 且无 SUPERSEDED 牌；TEST_MATRIX.md:32 原样；CALIBRATION/PHOTOMETRY/NOISE §13 TST-* 非法 ID 3/1/2 处在位；matrix CSV 仅 2/30 行带 SUPERSEDED；flux_calibrator/02_FROZEN/SNR_* 幻影引用在位 ⇒ 判"未修全" |
| 17 | M3-I-002 | P1 | FIXED(机制在 base 前已不存在) | docs/api/PHASE1_API_V1.md:27（TB-ARCH-004 登记）；CALIBRATION_ALGORITHMS.md:260/:397（DISP-CAL-002） | 原报文本「不在本契约」在 HEAD 与 base 均 0 命中、docs/science/PHASE1_API.md 从未入库（锚不可复现）；现树符号已在 API 合同表+偏差登记在位；建议前台按"锚失效+机制已消"处理 |
| 18 | M3b-A-06 | P1 | STILL | PSF.md::§6:59；sdet_api.cpp:1879(x+=r)/:1825(0.2R)/:899/:931(d²<4)；STAR_DETECTION.md §1 | 块状共享假设 vs 逐星独立实现反向冲突原样；间距/密度条件仍缺；饱和列仍不消费 |
| 19 | M3b-E-01 | P1 | STILL | ALG:8"2373 行实测"vs 现 2555 行；:85/:190 锚 :1599-2353 vs sdet_detect_impl@:1749；dpsf_psf.cpp:2 旧前缀自引；ALG:40 DISP-STAR-006 悬空 | 实例①(csv 失真行)已被 c3452d48 删除，其余 ②③④ 全部保持且漂移扩大 |
| 20 | M4-A-01 | P0 | STILL | lib/phase2/src/rejection.cpp:1835-1854；REJECTION.md:79/:86/:88 | n≤4 全接受 UNDERDETERMINED 分支原样；FROZEN SCI 未订正；116446/tests 0 命中、该 fallback 仍零测试（UNDERDETERMINED 测试均为 n≤2/integrate 域） |
| 21 | M4-C-07 | P1 | STILL | PHASE2_MOSAIC_WRITE.md:9 vs :497-499；PUBLIC_API.md:54 vs :1174；stage2.cpp:594；module_adapters p2_op_reject:4189/integrate:4427/write:4797 | 双份实现分叉与"权威面锚定退出的 stage2"三处矛盾逐字在位（1174 行锚自 :1116 漂移） |
| 22 | M4-F-06 | P1 | FIXED(部分) | RESCUE-FD-08：module_adapters.cpp:3711/3859 在 IR 通道真值打印 stage coverage/sample ok；cli/commands.cpp:1107 打印 budget workers | 断言依赖的三条日志串现均在生产路由真实产出（p2006 test_01 已改判 stage+registry）；残：5/7 节点逐节点执行证据仍无载体（测试注释自认 IMPL/INT），红绿终态仍需 CI 执行面（CI-BINDING 不收 backend，另条） |
| 23 | M5a-E-001 | P1 | STILL | THREADING_MODEL.md:20-22；THREAD_BUDGET_ARCH.md:32；EXECUTION_MODEL.md:9；execution_options_contract.md（工程控制/schemas 悬空）；PIPELINE_OVERVIEW.md:131；anchor_contract.json:5-8 | 「全部有效」四锚全错且再漂（drizzle reduction 实在 :2098 区，s_target_cache_gen 已移至 :1780）；锚门 doc_globs 仍只有 science/algorithms 两目录 |
| 24 | M5a-G-007 | P1 | STILL | CMakeLists.txt:70-71(if(UNIX) find_package)；:345/:383/:434/:506/:519；:555/:568 dpsf/sdet 无条件 PUBLIC OpenMP；tests/unit/CMakeLists.txt:124/:423/:829；packaging/dependency-lock.json:69 | Windows 探测缺失原样，且新增 astrocs_phase1_noise 等块复制同一 UNIX-only 模式；「生产不依赖」登记口径未变 |
| 25 | M5b-C-05 | P1 | STILL | ERROR_MODEL.md:22-44；exit_codes.h:17(RESOURCE=10)；runtime_client.cpp:398(→5)；check_api_docs.py:27(6=EXEC)；ci/checks.json docs_machine_consistency=0 | 三处码表互不覆盖与"已全量校验"假声明原样；RESOURCE 5/10 双头未修 |
| 26 | M5b-F-01 | P1 | STILL | ci/checks.json:2399(-p test_ci001b_*.py)；ci/tests/ 21 个 test_*.py | pattern 仍只匹配 2 文件；test_ci001_failclosed.py 仍不被任何 profile 采集 |
| 27 | M5b-G-08 | P1 | STILL | tools/check_agents_gov.py:15(REVIEW_PENDING needle)/:18(FORBIDDEN=[])；validate_task_ledger.py:42-43；ci/checks.json validate_task_ledger=0 | 关键词全含判据未变；REVIEW_PENDING 反向锁定与台账验证器零注册依旧 |
| 28 | M5b-G-16 | P1 | STILL(部分缓解) | cli/commands.cpp kConfigTemplate:100-106(output_dir ".")/回退:866/:1050/:1076/:1148/:1216/:1267；bench 默认:2349；checks.json UT-CLI dirty_ignore:1412-1414 | B1-A8 已禁"隐式"缺省（parser.cpp:385-397 拒缺/空 output_dir）且 UT-CLI 挂账已移除（removals），但 config init 模板仍下发 "."、多处 ".": 回退与 CWD 默认与豁免面原样 |
| 29 | M5b-I-07 | P2 | STILL(部分) | DOC-004 三面占用：gen_module_readmes.py:3 vs TEST_MATRIX.md:62 vs API-001.md:61；check_api_contracts.py:49 required=4 项 | ②API_STANDARD 字段清单面与③VERSION_NAMESPACES known_limits 面已重写（gen_version.py 消费表在位）；DOC-004 一名三义仍在 |
| 30 | M6a-D-003 | P1 | STILL | ipv_wcs.cpp:17/:238(NB_GRID=7) vs :411-412(41/order5) | 头注释与实现互斥原样（行号微漂） |
| 31 | M6a-D-011 | P2 | STILL | cosmetic_corrector.cpp:159("8邻居") vs :204(dirs[4]) | 三说并存未动 |
| 32 | M6a-I-002 | P1 | STILL | p3_output.cpp:374(常量 1) vs :538(真值)；h:33；DATA_SEMANTICS.md:2046；PHASE3_FITS_IMPL.md:142；p3_output_test.cpp:75/:85、p3_assembly_test.cpp:121、test_p3_output.py:160、probe:78 | 写路径无条件置 1 与四处常量断言逐字在位（DATA 行锚 :1978→:2046 漂移） |
| 33 | M6b-E-002 | P1 | STILL | check_doc_line_anchors.py git_ls:58-62(无 timeout、非 0→None)+C1/C2 的 tracked_set is not None 跳过；anchor_contract.json doc_globs 2 目录/bindings 41；RELEASE_STATUS.md:70/:71/:94 锚全错；CALIBRATION_ALGORITHMS.md:20(:321-333=astrocs_aio，calibration 实在 :373) | 结构性三缺口+fail-open 全保持，L0 抽点复算仍 4/4 错且漂移再放大 |
| 34 | M6b-G-003 | P0 | STILL | ARCHITECTURE.md:3(机检 9/9 声明)+§7:114-125(本版实测/PASS 9/9/mismatches=[] 无 SHA/命令/时间)；ci/checks.json 命中 0；README-DOCS.md:22；DOCUMENTATION_STANDARD.md:14 | 无据"本版实测"与判定器不在册两事实原样；工具基面仍指 orchestrator |
| 35 | M7-A-002 | P0 | STILL | PHASE2_INTEGRATION.md:125-127(定义式回退) vs PHASE2_UPM.md:54(禁) vs CONTROL_WEIGHT_SNR.md:20/:50/:71 | 三层三语义未收敛，ALG 定义式越权句式原样（待 A-02 裁决） |
| 36 | M7-A-125 | P1 | STILL | 同 35（本条为 ALG 侧独立事实的指针条目） | 随宿主条目判定 |
| 37 | M7-A-135 | P1 | STILL | STAR_DETECTION_ALGORITHMS.md:93(§3 非极大 x+=5) vs :148(§6 命中后跳步)；实现 sdet_api.cpp:1879 x+=r | 文档互斥逐字在位；实现取哪侧属运行期问题（原条即标无法判定，不重复计） |
| 38 | M7-G-101 | P1 | STILL | PHASE3_PROJ_IMPL.md:360(「不互改」)；SCI-P3:20/:100(仍拒 SIN/CAR)；registry:64(四投影 CONFORMANT) | 「唯 SCI 未升级」单文档漏改格局未变 |
| 39 | M7-I-204 | P2 | STILL(收窄) | DATA_SEMANTICS.md:1314(reasons 0..3/P2RejectReason) vs §23.3:1504-1513(sampler reason 0..5 全定义，自称"权威值域") | 同名 u8 两值域并存仍在；订正：原报「4..5 无定义」对 sampler 通道不成立（e654d4c2 起 6 值齐全，该表在 base 前已存在）⇒ 请前台按"事实部分失准的 STILL"处理 |
| 40 | M8-F-004 | P0 | STILL | check_standards_registry.py:209/:249(path_exists 判据、条款维丢弃)；ci/checks.json 0 命中；registry:135/:154/:159/:162 仍指零执行测试源；4 个 basename 于 CMake/ci 采集 0 命中 | 证据链三断点全在：判定器未在册、CONFORMANT 建立在从不编译的测试源上 |
| 41 | M8-F-012 | P1 | STILL | test_drizzle_parallel.py:17/:45/:48/:93 | 被测仍是 provider 演示内核、1/997 抽样和、"逐位一致"文案原样 |
| 42 | M8a-C-003 | P1 | STILL | lib/astro_image_io/README.md:3/:5/:7/:9-12/:28/:44/:52/:129/:180/:192 | 七月快照未动：零外部依赖/GitHub 当前版本/dll 构建说明/OpenMP16 文案与根图 astrocs_cfitsio(:275)+third_party/cfitsio(181 文件) 矛盾原样 |
| 43 | M8a-F-001 | P2 | STILL | ci/checks.json rcr/rejection 0 命中；tests 2\.4\.7 0 命中；该串现散写 10+ 文件；rcr_oracle_compare.py 无守卫 | 两处并发修复仍零回归断言 |
| 44 | M8a-I-001 | P2 | STILL | 宪章:152/:235/:242(lib/algorithms 不存在)；DOCUMENTATION_STANDARD.md 14 行未扩；cli/include/contracts/tests/cmake/ci/providers/modules README 计数=0 | 条文缺口与 35 单元空白面复算原样 |
| 45 | M9-A-2 | P1 | STILL | sampler.cpp::kcorr_lookup:90/:96/:554；PHASE2_SAMPLER.md:188-193/:358；hp_drizzle_api.cpp:1149-1151；aio_hips_writer.cpp:1275 | 300-600″ 标定轴与亚弧秒生产尺度仍脱节两个数量级，clamp 恒饱和无声明无告警 |
| 46 | M9-F-1 | P0 | STILL | 8 孤儿 TU build 声明命中 0（逐名复算）；PRODUCTION_EXECUTION_INVENTORY.csv:270/:276 仍 production,yes；DEVELOPER_GUIDE.md:27 仍列测试入口 | 恶意输入回归件仍不进任何构建/CI 而被 docs 当证据 |
| 47 | M9-H-1 | P0 | FIXED | gaia module_entry.c::json_append_escaped:391(+end 入参)/::gaia_execute 链:786-830/::gaia_inspect 同型:992-1010；回归 tests/unit/gaia_module_manifest_bounds_test.c + tests/unit/CMakeLists.txt:926（07eb229b） | 双站点全部改为带界 append 链+触边 PARAM/BUFFER_TOO_SMALL 且尺寸查询同拒、不回读越界长度；越界机制逐项复算已消失；同族 S1-001(lib/hips) 属另条 |
| 48 | V1-N-04 | P1 | STILL | CONTROL_WEIGHT_SNR.md:36/:44-45(不产出 σ_F) vs module_adapters.cpp:2772/:2808-2809(snr_f/sigma_f_adu 逐源交付)+snr_science.cpp:180 | 文档否定句与交付字段仍直接相反（锚 :2544→:2808 漂移） |
| 49 | V10-N-04 | P1 | STILL | aio_fits.cpp:161-162(BSCALE/BZERO catch(...))+:389+:490-491 新增同型；fits_reader.cpp D 归一头戴式差异仍在；drizzle_engine.cpp:1147/:2273 GAIN 同型 | 吞错/无 endptr/无 isfinite/不吃 D 记法四要素原样，消费式仍直进交付像素 |
| 50 | V11-N-03 | P2 | STILL | diag_gaia_psf_projection.py:78-81(GaiaSpectrumStar 3 字段/24B) vs gaia_client.h:57-66(5 字段/32B) | ctypes 旧镜像未同步，静默错位面原样（V2-N-01 的镜像工具未覆盖此结构） |
| 51 | V12-N-01 | P1 | STILL | include/ 无任何 constants 头（三级复核 0 命中）；15 位字面量生产裸写 8 文件 27 处；唯一具名点 lib/phase1/noise/noise_model.h:15(消费者仅 .cpp:189)；NOISE_MODEL.md:91「差<1e-12」仍实差 5.602e-12 | 常数无承载层的架构事实原样（本条为 A-12/S-1 总述根因条） |
| 52 | V12-N-09 | P2 | STILL | resource_gate.h:164/:233(>)/:370(>=)/:423/:452；memory_report.h:48/:53 | 同一 32 三义、边界两侧结论相反、覆盖通道只作用一条，全部在位 |
| 53 | V12-N-17 | P2 | CANNOT_STATIC(实为宿主不可复现) | 宿主指针「V12.md §2.17」在现档案不存在；22.5/20.6/20.48 于 lib/tools/cli/docs 三级检索 0 命中（0.0 兜底唯一近邻 snr_science.cpp:226） | 无法在当前树静态确认"四套零点"缺陷存在；不得判 STILL 亦无修复证据——建议前台标记"锚失效，请 V12 轴补交宿主"后重派 |
| 54 | V13-N-08 | P2 | STILL(且扩散) | 根目录 p8/p9/p10/p11-files.patch 在位 + 新增 p15a-files.patch | 归批提醒件原样，同族站点 +1 |
| 55 | V18-N-02 | P1 | STILL(缓解) | check_prod_reachability.py::main:95-108 vs ::_selftest:207-232（复制扫描算法，仅共享 BANNED_* 常量:33/:41，无共享判定函数） | "自检重写主判据而非调用"结构原样；列表漂移风险已由共享常量消除，程序性漂移（源集合/匹配法两处各写）仍在 |
| 56 | V2-N-01 | P0 | FIXED | ipv_api.h:76-81(struct_size/abi_version@0/4, IPV_PARAMS_ABI_VERSION=2)；ipv_entry.cpp::validate_params_abi:212 + 5 入口:464/:507/:655/:701/:748 fail-closed；lib/plate_solve/tools/ipv_abi_mirror.py 单一事实源；diag 脚本:165-170 两侧比对；ctest 锁注册 ci/checks.json:3811-3849（fbfcfac0） | 越界写机制消失：自描述头 + 运行期拒旧调用方 + 布局锁三件套在册；同类他站=GaiaSpectrumStar 镜像（即 V11-N-03，已另判 STILL，不属本条修复义务） |
| 57 | V4-N-04 | P1 | STILL | ci/checks.json validate_registry=0；唯一调用 ci/tests/test_workflow_lock.py:624 而 CI-BINDING pattern(test_ci001b_*.py)不收 | R11 strict 规则仍零 CI 执行面，"条目会红"未闭合 |
| 58 | V7-N-02 | P2 | STILL | module_adapters.cpp::p1_parse_sip:1859/:1880(允许缺省全零→present=true)/:3206/:3232 | order>0 缺数组仍放行、判别位仍记声明值；无该分叉测试 |
| 59 | V7-N-10 | P2 | STILL | lib/drizzle/src/module_entry.cpp:355(「API-DRZ-001 默认 1.0」逐字在位)；module_adapters.cpp:2967 仍取 1.0；PUBLIC_API.md §API-DRZ-001:207+ 仍无默认条款 | 杜撰锚未删未改指，1.0 缺省仍无合同依据（待 A-37/DISP 登记面） |
| 60 | V9-N-02 | P1 | STILL | tools/traceability/check_traceability_matrix.py:387-390(表头不等 err+return) | 表头遮蔽行级发散的结构性缺陷原样（同文件锚位与报告一致，未修） |
| 61 | V9-N-11 | P2 | STILL | ci/run.py:103-106(EMPTY_OUTPUT_SILENCE_EXEMPT={API-DOCS,UNIT-CLOSURE})+:170 自述"设计即静默" | 空输出即 PASS 制度仍在名单内 |

## 二、统计与前台建议（本档不代写账本）

- 四态计数：STILL 54；FIXED 7（M2a-C-7、M9-H-1、V2-N-01 判"修全"；FD-F-002、M3-E-001、M4-F-06 判"部分修复，残面已在表内列明"；M3-I-002 为"锚失效+机制在 base 前已消"）；MOVED 0；不可复核 1（V12-N-17）。
- 建议账本标记：
  - FIXED-全：M2a-C-7（建议 CLOSED，修于 bd4bc23b）、M9-H-1（CLOSED，07eb229b，回归 gaia_module_manifest_bounds 已注册）、V2-N-01（CLOSED，fbfcfac0，三锁在册）。
  - FIXED-部分：M3-E-001（账本现标 FIXED，建议改注 PARTIAL 并保留 OPEN 残面：TRACE csv 行/TEST_MATRIX/TST-*/幻影引用四项，或按实例拆行）；M4-F-06、FD-F-002（建议 FIXED-PARTIAL 注记，勿径直 CLOSED）；M3-I-002（建议 CLOSED-锚失效注记）。
  - 锚订正件（只改锚不撤条）：M1a-A-006（calloc :1829→:2072）、M2a-A-3（:2266→:2525）、M2a-E-2、M3b-E-01、M5a-E-001、M6a-I-002（DATA :1978→:2046）、V1-N-04（:2544→:2808）、M3-C-005/M2a-C-12 的 cosmetic real_ingest.cpp 路径=不存在（疑混 lib/calibration 通道）。
  - 事实订正件：M7-I-204「4..5 无定义」对 sampler 通道不成立（§23.3 自 e654d4c2 起 6 值齐全）；V18-N-02 的"列表可各自漂移"已因共享 BANNED_* 常量部分缓解。
- 本片全程只读：read/glob/grep + 问题扫描内 python3 + git --no-optional-locks（show/log/diff/ls-files/merge-base），无 git 写、无账本写、无构建/测试执行。

## 三、逐条证据附录（关键命令 → 输出摘录；均为本会话在 HEAD 实测）

### A. FIXED 件（修在哪 + 是否修全）

**[0] FD-F-002 = FIXED(部分)**
- `grep -n normalized_cpu_100pct_all_allocated_cores cli/resource_recorder.h` → `252:"\"normalized_cpu_100pct_all_allocated_cores\":false,"`（前随注释 :246-248「M5a-G-002: cpu_pct 的单位是 percent_of_one_core…旧字段无条件自证 true 与采集事实相反。保留旧键名但置 false…并给出真实单位键」+ 新键 `"cpu_pct_units":"percent_of_one_core"`）
- `grep -n … tests/unit/mon001_recorder_test.cpp` → `100:CHECK(js.find("\"…\":false") …)`、`:101` 同型断言 units 键
- 修全判定：谎报声明已消除；但守卫仍是 find(字面量) 形态（机制名「子串断言守卫」在两处仍在用），同族他站 FD-F-001：`grep -n 'backend_table.inc' tests/unit/cpu_provider_test.cpp` → `73: CHECK(s.find("backend_table.inc") != std::string::npos);` 原样。修复 commit=adaeb531（merge-base --is-ancestor 证在 base 内 ⇒ 报告写作时点早于该修）。

**[8] M2a-C-7 = FIXED**
- `grep -n 'BlockCacheBudget\|block_budget' lib/gaia_xpsd_client/src/gaia_client.c` → `274: BlockCacheBudget block_budget; /* G3b: 客户端级…所有文件共享 */`、`2019: client->files[f].budget = &client->block_budget;`、`1966: block_budget_init(...)`
- 判据复算：`block_cache_insert_locked`（:558-580）budget!=NULL 分支 `while (block_budget_used(budget)+data_size > budget->max_memory) evict…` + `:579 if (!block_budget_try_reserve(budget, data_size)) return NULL;` —— 资源上限按客户端总量生效（原「每文件 4GB×32」消失）
- `git log -1 --oneline -S 'BlockCacheBudget' -- …/gaia_client.c` → `bd4bc23b perf(P1): …解块缓存客户端总预算`；文档侧 gaia_client.h:3 已改述「客户端级总预算 4GB…所有 XPSD 文件间共享」；GAIA_QUERY.md:157/232「≤4GB」全局口径转真。残余：NULL 预算回退 per-file 语义，注释明示「独立文件/测试路径」。

**[16] M3-E-001 = FIXED(部分)**（账本已标 FIXED=c3452d48）
- 修在哪：`python3` 解析 docs/traceability/TRACEABILITY_MATRIX.json → MOD-astrocs-phase1-calibration：`test_id=TEST-CAL-001;TEST-CAL-DESIGN-001`、`test_path=lib/calibration/tests/p1cal (ctest p1cal_units/…); tests/backend/test_calibration_oracle.py; tests/unit/CMakeLists.txt:441 master_flat_median+…`、src_path 含 ac_correct_frame/ac_set_num_threads 全 14 符号 ⇒ 权威面 VERIFIED 行锚指真实载体，机制（测试锚不覆盖被验门）在权威面消失。
- 未修全（逐条实测）：`git show HEAD:docs/TRACEABILITY.csv | sed -n 20p` → 仍 `TEST-CAL-001,lib/calibration/tests/test_photometry_apply.cpp,…,VERIFIED` 且 public_api 列无 ac_correct_frame；`grep -n SUPERSEDED docs/TRACEABILITY.csv` → 0；TEST_MATRIX.md:32 原样；`grep -c TST- docs/science/{CALIBRATION,PHOTOMETRY,NOISE_MODEL}.md` → 3/1/2；matrix CSV `grep -c SUPERSEDED`=2 而 `grep -c '^MOD'`=30；`find docs -name '02_FROZEN*'`=0 且 CALIBRATION_ALGORITHMS.md:172/:199/:406 仍引；PHOTOMETRY.md:3/TRACE csv:22 仍引 flux_calibrator（ls 该目录失败）。

**[17] M3-I-002 = FIXED(机制在 base 前已不存在/锚失效)**
- `git show HEAD:docs/science/PHASE1_API.md` → 「路径不在 HEAD 中」；`git log --all --follow -- 该路径` → 空（从未入库）；`git grep 不在本契约 521095b8 -- docs` 与 HEAD 全域 grep → 0 命中 ⇒ 原报文本在 base 与 HEAD 都不存在。
- 现树合规形态：`grep -n ac_set_num_threads docs/api/PHASE1_API_V1.md` → `:27 | … | TB-ARCH-004(checker 管控; V5 迁移整改点: 由 p1 budget 注入取代, ABI-001 收编)`；`grep -n DISP-CAL-002 docs/algorithms/CALIBRATION_ALGORITHMS.md` → `:260/:397` 偏差登记在位；头声明 astro_calibration.h:158。建议前台按 CLOSED(锚失效注记) 处理。

**[22] M4-F-06 = FIXED(部分)**
- `grep -rn 'stage coverage' lib/` → `module_adapters.cpp:3711 std::fprintf(stderr, "stage coverage ok: cells=%llu\n", …)`（前随 RESCUE-FD-08 注释「IR 通道的 coverage 节点把实测 union cell 数写 stderr…数值来自真实 p2_coverage_build 结果, 不造假」）+ `:3859 stage sample ok: obs=…`；`grep -rn 'session run: budget' lib/ cli/` → `cli/commands.cpp:1107` 新点在 cmd_phase2_run 生产路径。
- 测试侧：test_p2006 test_01 现文只断 returncode/final ok/两 stage frag/registry；p2007 `_session_budget_workers` 正则现可由 :1107 满足 ⇒ 「断言依赖生产链不发的日志」机制消除。
- 未修全：5/7 节点逐节点证据仍缺（测试注释自认「session stderr 仅对起手节点打 stage 日志, 逐节点 status 落盘无载体 — IMPL/INT」）；红/绿终态属 CI 执行面（维持原条 §6 注）。

**[47] M9-H-1 = FIXED(修全)**（账本 FIXED=07eb229b）
- 机制复算（原三要素逐项查）：①无容量转义器 → 现签名 `json_append_escaped(char** w, const char* end, const char* s)`（module_entry.c:391）返回 int 溢出位；②snprintf 返回值推进 → 现整段改带界 append 链 `head_end = head + sizeof(head) - 1; int head_ok = json_append_ch(&hw, head_end, '{'); …`（:786-825）每步失败短路、`:40 if (!head_ok)` PARAM+BUFFER_TOO_SMALL（:784 注释「整次失败…尺寸查询阶段同样拒绝」⇒ 原⑤触发前移面同修）；③第二站点 gaia_inspect 同型（:992 注释「M9-H-1: 同型第二站点——容量显式传入」）。
- 回归在册：`grep -rn gaia_module_manifest_bounds tests/ ci/` → tests/unit/gaia_module_manifest_bounds_test.c + tests/unit/CMakeLists.txt:919/:926 add_executable。同族他站：lib/hips/src/module_entry.cpp（S1-001）属另条，不影响本条「修全」判定（本条缺陷面=gaia 两站点）。

**[56] V2-N-01 = FIXED(修全)**（账本 FIXED=fbfcfac0）
- 结构自描述：ipv_api.h:76-81 `#define IPV_PARAMS_ABI_VERSION 2u` + `uint32_t struct_size;`@0/abi@4；5 公共入口 `if (!validate_params_abi(params, result, __func__)) return 0;`（ipv_entry.cpp:464/:507/:655/:701/:748，函数体 :212）。
- 镜像单一事实源：`lib/plate_solve/tools/ipv_abi_mirror.py` 在位；diag 脚本改为两侧比对（:165-170 mismatch 即报「请同步 ipv_abi_mirror.py 并重编 DLL」）。
- 锁在册：`grep -n 'ipv_abi_layout_lock' ci/checks.json` → :3811/:3829 采集 + :3845 selfcheck；ipv_params_abi_failclosed_test.cpp 存在于 cpp/test/。越界写机制复算消失（旧 struct_size 值会被拒）。「同类他站」=GaiaSpectrumStar 旧镜像未同步 → 已单列 [50] V11-N-03=STILL，不并入本条。

### B. 特殊判定件

**[53] V12-N-17 = 不可复核（暂记 CANNOT_STATIC 位）**
- 三级复核：①宿主指针失效：`grep '2.17' 问题扫描/findings/A_SCI_DEF/p1/V12.md`=0、该档案小节只有 V12-N-01/03/05/06；②值检索：`grep -rn '22\.5|20\.6|20\.48' lib cli tools providers modules docs(合同/科学) --include 源码/文档` 无任何 zp/零点语境命中（唯一 20.48 在 browser_qt 合成星表 gen_ref_source.py:88，与光度零点无关）；③缓存面：_v21_corpus.txt/DESIGN_check_numeric_constants.md/设计大纲 检索 0 命中。⇒ 不得判 STILL，亦无修复证据；建议前台标记「锚失效待原轴补证」重派或撤条。

**[55] V18-N-02 = STILL(缓解注记)**
- `sed -n '95,108p;207,232p' tools/quality/check_prod_reachability.py` → main 扫描 `cli_sources=[cli/main.cpp]`+BANNED 双循环；_selftest 以 fake_cli 重写同一 `re.findall/endswith` 算法。两处共享常量（:33/:41）⇒ 列表漂移面关闭，程序性漂移（源集合、匹配步骤两份实现）仍在，未抽出单一判定函数。

**[57] V4-N-04 = STILL**
- `grep -c validate_registry ci/checks.json` → 0；`grep -n validate_registry ci/tests/test_workflow_lock.py` → 624 唯一调用者；CI-BINDING-TESTS `-p test_ci001b_*.py`（checks.json:2399）不收该文件 ⇒ R11 strict 采集面仍零执行。

**[28] M5b-G-16 = STILL(部分缓解) 关键证据**
- `sed -n '100,106p' cli/commands.cpp` → kConfigTemplate 仍 `"output_dir": "."`；bench 默认 `:2349 : "cpu_profile.json"`；豁免 `checks.json:1412-1414 astrocs_run_/run/`；缓解：`grep -n 'FIX-E2E B1-A8' cli/parser.cpp` → :389-397 平铺会话缺/空 output_dir 拒（exit 2），known_failures.json UT-CLI 已入 removals（2026-09-14）。残留：显式 "."（模板默认值）仍合法直通 ⇒ 合规调用仍写根。
