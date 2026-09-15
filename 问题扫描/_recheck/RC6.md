# RC6 复验档案（第 6 片 / 共 8 片）

- 时点 HEAD: a3a343a44080d917089e1f8d548ed2d0400b0c61
- 范围: 问题扫描/_cache/recheck_round1.json 的 verify[5::8]，共 62 条；逐条按 文件::符号 重定位复验（纯静态）
- 状态口径: STILL / FIXED(须给修在哪证据+他站残留判定) / MOVED / CANNOT_STATIC
- 免重报已读: 问题扫描/SUMMARY.md、问题扫描/INDEX.md；基线对照 git --no-optional-locks diff --name-only 521095b8..a3a343a4（35 个代码/合同文件）
- 建议标记（账本不回写，由前台统一执行）：FIXED 4 条建议 fix_state=FIXED 并记 fix 证据；其余维持 OPEN；M7-A-129 建议改级注记。

## 结论表（逐条）

| # | id | 优先 | 状态 | 新锚/证据(简) |
|---|----|------|------|-----------|
| 1 | FD-G-004 | P1 | STILL | checks.json::UT-CPU-AVX512 waivable=False(4项全false) vs evidence/ciqa/CIQA_REPORT.json:177/:276 SKIPPED(waivable)+V8-CI-010 计数在位；tests/unit/CMakeLists.txt::cpu001_selftest_avx512 SKIP_RETURN_CODE 77 新锚:412；无 waivable↔证据一致性门 |
| 2 | M1a-A-002 | P0 | STILL | PLATESOLVE.md:185-186 F1 仍只 rms_arcsec≤0.5″内部锚；registry:80 rms≤0.5″；memory.md:55 median=0.897px 反证仍在；DISP-WCS 表无天测象限偏置条目 |
| 3 | M1a-A-010 | P1 | STILL | p3_wcs.cpp:163 r>=M_PI/2 与 :187-189 denom<=0 两界原样；DATA_SEMANTICS.md:2258 与 PHASE3_PROJ_IMPL.md:255 「数学等价」句原样 |
| 4 | M1a-C-008 | P1 | STILL | 会话面 p3_session.cpp:365-366 manifest_hash=nullptr+hips_id 占位；p3_output.cpp:224-227 HISTORY；节点面 module_adapters.cpp:6024 hips_id 占位(manifest 已真值)；FITS_IMPL.md:344 仍「B2-A9/A10 已闭合」+ :31/:164/:172 引不存在「SCI-P3 §96」(SCI 节号至 §15) |
| 5 | M1a-F-003 | P1 | STILL | 0.1431 tests/ 0 命中(实场锚无 fixture)；test_phase1_inprocess.py:113-114 存在性断言；B2-A17 段现有合成 oracle 数值交叉(p1001_real_nodes_test.cpp:1623) 但 ALG:186 实场锚仍未钉 |
| 6 | M1a-I-002 | P2 | STILL | p3_wcs.cpp:18-21 #ifndef ASTROCS_P3_MAX_SIDE 原样；构建面定义 0 命中；DATA_SEMANTICS:2210「编译期覆盖如实冻结」句在；SCI §15:143「收窄不扩大」在 |
| 7 | M2a-B-4 | P1 | STILL | 三机制全在: fio_write_hdu:1151-1155 恒写全零 CHECKSUM 占位卡+end_v1:1261-1270 恒预留; acs_fio_verify_file_v1:1651 豁免仍判**计算值 sum** 侧(sum!=0xFFFFFFFF&&sum!=0); IO_001:139 默认关闭句; REG:223 §6 CONFORMANT/偏差无; tests/io checksum=0 默认仅:286 一例 |
| 8 | M2a-C-3 | P1 | STILL | 代码已不掩膜(drizzle_engine.cpp:1889-1894 注释+无 isfinite-continue)但六登记面仍写「静默 continue(等效掩膜)」: DRIZZLE_GEOMETRY:118/:230, DATA_SEMANTICS:245, lib/drizzle/README:89/:166, registry:43, STANDARDS_REGISTRY:163(DISP-DRZ-004) |
| 9 | M2a-D-2 | P2 | STILL | drizzle_engine.h:62-63「只保留 sumFlux/sumArea/nContrib、20B→12B」vs :65-70 4 字段(含 sumVarNum) 互斥原样；module_entry.cpp:551「6 个 f64×8B」与 FP32 实际互斥原样 |
| 10 | M2a-F-2 | P1 | STILL | fixture nodeCount="1" 三处(gaia_xpsd_fixture_gen.c:353/:486+mag_bounds:80)无多叶 fixture；search_recursive 递归分支新锚 :1644-1653/:1784-1793/:1921-1930；深树 0 用例；GAIA_QUERY §5 深树设计未落实 |
| 11 | M2a-H-3 | P2 | STILL | aio_hips_writer.cpp::write_tile_core :569-572 sig=flux/area,sup=area/A_cell,sup>1→1 钳制原样；hierarchy 回乘 :642-643 flux=sig_n*sup_n*A_cell；二站 :1087；clamp 计数 0 命中(无暴露)；DATA_SEMANTICS §12.2 语义缺口未动 |
| 12 | M2b-B-02 | P0 | STILL | ::write_moc_fits 新锚 :293-331 仍只写 MOCORDER/PIXCOUNT，无 ORDERING=NUNIQ/COORDSYS=C(:242-243 属 tile 头)；读侧 hips_parse_moc(runtime/io/hips_core.c:395)无必需键校验；REG:97 §4.4.1 仍 CONFORMANT |
| 13 | M2b-B-10 | P1 | STILL | REG :41/:43/:55/:86/:126/:152/:186/:213 六域 CLAUSES 错挂字面串原样；check_standards_registry.py:242-243 仍只查「含§字符」；:298 C3 自述仍形门 |
| 14 | M2b-G-03 | P1 | STILL | docs/standards/CODE_STANDARD.md:3「权威来源=V19R2 MASTER_CONTROL_SPEC §5」+:7「正式 toolchain=MSYS2 MinGW64」原样；TEST_STANDARD:6/:8、COMMENT_STANDARD:45、DOCUMENTATION_STANDARD 历史材料句在；与宪章 §1.1/§15.2 冲突未订正 |
| 15 | M3-C-001 | P0 | FIXED | 修在 lib/photometric_calib/cpp/src/star_matcher.cpp::cleanAndScale :514-535 |r_consistent|<3→NO_DATA 退化(diag fit_used=0/scale=1.0)+ :613-623 |r_inliers|>=2 才估 sigma(注释直引 B4-2(M3-C-001))；回归锁 lib/photometric_calib/tests/p1phot/p1phot_fixgates.cpp(3星过/2星退化用例)已注册 CMakeLists:141；他站: 同类 SCI 门缺失逐条另立(M3-C-009 等)不属本条 |
| 16 | M3-C-009 | P1 | STILL | orchestrator.cpp:957-958 dark_opt=0/dark_k=1.0 仅初始化零推导(赋值点=初始化)；:961-975 mode 只影响 master 文件校验；:948/:976 fallback 与 :949-950/:977-978 exposure 只进日志无校验；CONFIG_SCHEMA.md 仍无 calibration.mode/exposure 字段；json_config.cpp:686-688 仍强制解析 |
| 17 | M3-F-004 | P1 | STILL | docs/TRACEABILITY.csv:23-37 十五行 SCI-NOISE 仍 test_files=noise_model_science_test.cpp+VERIFIED，该文件仍不被任何 CMake 编译(0 命中,头注自宣 g++ 手编)且自述 SNR-011/012/015 不在本文件覆盖；tests/unit/CMakeLists.txt:515-528 EXISTS 条件注册+ :1152 V7 残留注释原样 |
| 18 | M3b-A-02 | P0 | STILL | 两链两量原样: sdet_api.cpp:2304 rec.flux=fit A(新锚) vs lib/phase1/stars/star_detector.cpp:151 s.flux=m00；DATA_SEMANTICS §17.2:704 flux 名下单栏口径(振幅A)；phase1/stars/README:11 flux(ADU)无 m00 注；p1_op_star_psf 消费点 module_adapters.cpp:1662-1664/:1710 |
| 19 | M3b-C-03 | P1 | STILL | 六子机制全在(行锚漂移):①主路径 rec.cx=fit :2302 vs SCI STAR_DETECTION §1「平台中心=edge-walking」②:1979 整数除法 vs :703 浮点除法 ③生产 fit 失败丢弃:2289(仅日志计数,交付无哨兵/状态;哨兵:1612 在 debug 路径) ④:863「丢弃饱和星」误导注释 vs :883 实为保饱和 ⑤:2306 A>dynrange 覆盖句 ⑥§17.3:721 d²≤1.0 vs SCI/实现 :899 <4.0 |
| 20 | M3b-G-01 | P0 | STILL | KNOWN_LIMITATIONS.md grep PSF/质心 0 命中；RELEASE_STATUS 0 命中；STAR_PSF_ALGORITHMS §11.3 :182-187 DISP-PSF-001..006 无一覆盖质心系统偏差；BLOCKER 字面量唯一宿主 gate2_psf_oracle.py:328(漂移)；ci/checks.json 无 gate2 登记；归档 docs/archive/history/memory_V18R2…:2051 仍是唯一记载 |
| 21 | M4-C-03 | P0 | FIXED | 修在 lib/phase2/tools/stage2.cpp: 产品级 :565-576 ivar_product_missing&&!legacy_allow_weight_fallback→return 7；tile 级并行 :1109-1117 读失败 fail=2(→:1324/:1326 rc=7)、串行 :1384-1389 return 7；ivar_tile_fallback_px 计数 :607/:1118/:1389；开关默认 false(stage2_common.h:94)。判完全: 两站(并行+串行)+产品级三层同治，无他站残留；文档面 PHASE2_MOSAIC_WRITE 与实现现已同向 |
| 22 | M4-F-02 | P0 | STILL | PHASE2_UPM.md:108 仍宣称「NumPy Huber IRLS+control_ivar 复算 θ rtol 1e-9」；tests/ 无独立 NumPy Huber 实现(仅向生产传 huber_delta 参数: p2002:72/78/178)；control_ivar 权重/k_corr 1.4/N_eff/弱零锚全式无独立复算；负例零 |
| 23 | M5a-C-001 | P1 | STILL | parser.cpp:52/:56/:60 phaseN run 仍放行 --cpu-profile；commands.cpp 唯一消费 cmd_show_effective:163-178；run 路径 budget=cli_affinity_cpu_count() :1102/:1308/:1693(漂移)与 profile 无关；module_adapters 改后仍 0 处 cpu_profile 消费 |
| 24 | M5a-G-003 | P0 | STILL | checks.json 解析 --gate-required/--gate-workers 全仓 0 命中(run.py:109-119 判据恒 false)；run.py:71-77 注释自认「CI 注册表当前零旗标，应用面=REAL-001/本地 heavy」；RELEASE_STATUS.md:110 仍写「缺失即 fail-closed」；evaluate_frozen_gate 唯一调用仍 run_monitored.py:726 自身 CLI+自测 test_ci001_failclosed.py:430 |
| 25 | M5b-C-01 | P0 | STILL | 422/422 VERIFIED、test_ids=TST-GEN-001 396 行(git grep: 仅 fixture 副本,无注册定义)原样；status/test_ids 两列仍不被门消费(两 checker 文件 0 处 test_ids/status 语义)；唯一变化=本门已新增 AST 全签名比对(:53-135 API-SIG-TEXT/ARITY/NOREF)——第③子面部分缓解，标题面(占位 VERIFIED)仍在 |
| 26 | M5b-E-02 | P1 | STILL | docs/architecture/PHASE3_MODULE_ARCH.md:1/:6/:8-11 仍写(lib/phase3 不存在,实际 phase3_rsmp/phase3_session)+四单元图 vs module_adapters.cpp::p3_nodes :6491-6498 五节点；FROZEN :3；TRACE MATRIX:26 resample2 VERIFIED；原引 lib/phase3_rsmp/src/p3_resample.cpp 现为 lib/phase3_session/p3_resample.cpp(位置漂移本体未修) |
| 27 | M5b-G-04 | P0 | STILL | 四份人抄副本仍在且现已裂: VERSION=alpha.2/checks.json:16 expected alpha.2/21 个 module.yaml=alpha.2，而 packaging/astrocs.product.json:3、install-tree.contract.json:5-6、dependency-lock.json:5、schemas(install-tree:12 const、product:10 pattern)仍 alpha.1；check_version_consistency.py:17-20 扫描面不含 packaging/与 lib/*/module.yaml；ci/check_version.py:59-60 豁免机器字段 |
| 28 | M5b-G-12 | P1 | STILL | check_serial_heavy.py:58-59 主判据仍「四关键词全缺才报」(任一命中即过,注释命中=过)；:92-93 Phase3 同型；:28 ALLOWLIST 在；宣称覆盖口径(6 面)≠实扫文件集；门 id NO-SERIAL-HEAVY 三 profile 在册 |
| 29 | M5b-G-20 | P1 | STILL | tools/check_module_readmes.py:12-18 仍硬编码 5 目录(:6 docstring 同)；lib/*/module.yaml 21、registry 26 页，集合对账零；checks.json::MODULE-READMES 三 profile 在册 |
| 30 | M6a-C-003 | P1 | STILL | module_adapters.cpp 改后三处现在时宣称仍在: 文件头 :27-28「AIO-002 原子发布内建」、:3242-3243、:4789；aio_publish.cpp 仍仅 lib/hips/CMakeLists.txt:30 引用(根 CMake astrocs_hips STATIC 源集 :351-353 不含)；writer 直写路径 std::remove :232/:301/:1030；lib/hips/README.md:192-195 自认「无原子发布(remove+create 直写)」——互斥对原样 |
| 31 | M6a-D-007 | P2 | STILL | snr_estimator.h:412 SNR_QF_PSF_OK 注「status==0 或 3」vs :82 fit_status「0=ok,1=rejected(status!=0)」同头互斥原样(锚漂移 :337→:412)；消费侧 orchestrator.cpp:4392 仍 (==0||==3)；snr_estimator.cpp:183 逐星仍 status!=0.0 剔除；无新注释订正 |
| 32 | M6a-D-015 | P2 | STILL | lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.h:10 头注仍「FITS 行主序 [y*512+x]」；cpp:9/:220 实现官方映射 nested_local_to_fits_index=(511-x)*512+y(healpix_core.h:46)——两式仍不等,弃用私约仍被当现行契约 |
| 33 | M6b-C-001 | P1 | STILL | KNOWN_LIMITATIONS.md:13-14「9. UPM ivar 回退: …回退 support(几何可靠性,无量纲,非逆方差)」仍现行口径；TROUBLESHOOTING.md:16「或接受 support 回退」；实现默认已 fail-closed(M4-C-03 修复面 stage2.cpp:565-576)+宪章 §6.3:191 禁 support 冒充——文档口径与现实/宪章三处仍冲突未订正 |
| 34 | M6b-E-006 | P2 | STILL | 主锚原样: TRACEABILITY_SPEC.md:6-7 权威声明仍锚废止控制包 16 号文+ARCHIVED 约束文件(其 :1-5 自宣 superseded)；§5 行号处现无「$F.2」句——git grep 该引用文本除问题扫描自身外 0 宿主(base 版本亦 0,判该子引自源头不实,建议前台订正证据摘录)；宪章 §1.1 未被该 SPEC 引用 |
| 35 | M6b-I-001 | P1 | STILL | ARCHITECTURE.md:16/:18/:51/:52 orchestrator.exe/astrocs-stage2 现在时数据流+模块表原样；API_REFERENCE.md:112/:127、modules/phase2.md:6「生产入口 astrocs-stage2」、TROUBLESHOOTING.md:7 在；tests/arch/test_single_cli.py:14-16 仍两字面黑名单+声明唯一性检查 |
| 36 | M7-A-121 | P1 | STILL | UPM SCI:20/:29/:46-47 quality_factor 进唯一冻结式仍无映射表(SCI 侧)；映射现仅存 lib/phase2/src/upm.cpp:177-185 私有 inline 且 (void)mode——mode 合法域仍未定义；CW SCI:38/:60/:102 frame quality=uint32 位掩码与 UPM [0,1] 因子同名两名仍未收敛；锚 SCI-UPM-WEIGHT-001 有定义(:45)。判弱化残余=SCI 无映射+mode 未实现+两文档口径未对账 |
| 37 | M7-A-129 | P1 | STILL-降级 | PHASE3_RSMP_IMPL.md:193「FP64 计算,Σw=1 精确成立」措辞缺陷原样(SCI :77 只称权重和=1,无 ULP/容差口径;累加 dtype 未声明)；但主事实①复算不成立: p3_resample.cpp:189 跳象限后 :214-218 nearest_pt 填充,Σw 恒代数=1,文件 base..HEAD 未改⇒报告时点亦无「只累加保留象限」形态⇒建议前台按②③(表述级)降级或并入 M7-F-201 族 |
| 38 | M7-A-139 | P1 | STILL | 分配面原样: RSMP_IMPL.md:263-264「cache 上限 max_tiles·512²·4B；输出平面 2×W·H×4B」；p3_output.cpp:469/:487/:513 vector<float>(nelem) 整帧缓冲×3；W,H≤20000 ⇒ GB 级无绝对上界/拒绝路径；§10 无登记；宪章 §5.3/§17.6 关切未动 |
| 39 | M7-G-105 | P1 | STILL | 例1: STANDARDS_REGISTRY.md:136「order ≤ 29…CONFORMANT 无(checked 收口)」原样(Górski 2005 无该条,引自 M2b-B-07 已证)；例2: module_adapters.cpp:1903「FITS paper IV §2.1: U=dx+A(dx,dy)」桥注释在**改后文件**中仍充当 :1915/:2154 实现依据；例3 为问题扫描自身账面挂账(M6b/M9 定档) |
| 40 | M8-C-002 | P1 | STILL | upm.cpp:515 注释仍「D1(worker 数无关,同一 worker 数下位精确)」；synthetic_gate.cpp:303 跨 worker 数仍 1e-6、:318 同 worker 重复仍 1e-12(均非位精确断言)；PHASE2_UPM.md:78/:94 等价门口径(0/1e-12)未动——三口径互斥原样 |
| 41 | M8-F-008 | P1 | STILL | 全仓仅 p1star_test_main.hpp 含 unknown-group 防护(1 命中)；其余 10 份同构仍 fail-open: 复验 p1cal_test_main.hpp::run_all_groups :82-121 结构未变(不匹配→0 组执行→total_fail==0→PASS rc=0)；aio_abi/p2hips 同型 |
| 42 | M8-G-003 | P1 | STILL | 40 探针复数一致；恒真 4 例原锚号原样: :996 F-032「… or True」、:1002 F-033、:1028 F-036、:1034 F-037 全 add(…,True,…)；F-022 :913-916 仍单文件字面量「CHECK(true)」判定 |
| 43 | M8a-E-001 | P1 | STILL | 两未注册 ID 引用面原样: ALG-P2-COV-001 docs 全 0 命中(连字符放宽亦 0)而 lib/phase2/README.md:174、module.yaml:32、module_adapters.cpp:874(改后文件在)三处仍引；lib/hips_p2/README.md:160-161 仍以「SCI-F3-001 更新(2026-09-12)」句式把控制包任务号当 SCI 修订源；无未注册 ID 门 |
| 44 | M8a-G-005 | P1 | STILL | nanoflann.hpp(lib/healpix_db/healpix_drizzle/,NANOFLANN_VERSION 0x190)仍无 license/哈希/版本采集: dependency-lock.json/ci/checks.json/DEPENDENCIES.md 0 命中；third_party/ 仍仅 nlohmann；THIRD_PARTY_NOTICE.md(31 行)只 healpix 一块；根 CMakeLists.txt:426 QA-001 注记在,交付经 astrocs_ahpx(编译面静态可证,实际产出需构建=运行期但缺陷本体为登记面缺失,判 STILL) |
| 45 | M8a-I-005 | P2 | STILL | 21 个 lib/*/module.yaml 仍注「遵循 11_MODULE_SOURCE_TEST_STANDARD.md」；modules/conformance/noop|echo module.yaml:3+README:62、providers/cpu/common/README.md:4/:36/:59(15 号文+旧约束§C6)、packaging/README.md:37/:39(03_TARGET/10_LINUX_CONTROL_NODE)、runtime/core/phase_lifecycle.README.md:13-15、spill_manager.h/resource_monitor.h 头注均原样；无 ACTIVE 镜像标注 |
| 46 | M9-C-2 | P1 | STILL | astro_image_io.h:125 aio_wcs_pixel_scale 仍零单位声明(同排 :126 rotation_deg 有名)；API_CONTRACTS.csv:105 该行 units 仍「ADU/pixel/deg per header」且全表 382 行同串(样板)；PUBLIC_API.md grep aio_wcs 0 命中；§17.3/§23.6/§28.4 类比缺位在 |
| 47 | M9-G-3 | P1 | STILL | lib/gaia_xpsd_client/src/module_entry.c::json_get_f64_array :191-194 计数循环仍「if(数字/负号){count++;strtod(q,&q);}」无推进守卫——q 不前进时死循环+count 自增原样(文件路径修正: 非 gaia_client.c)；调用 :319-320；同型他站 lib/drizzle/src/module_entry.cpp:199-208(q=end 仍不防 end==q) |
| 48 | M9-H-5 | P1 | STILL | aio_ahpx_reader.cpp::readSnr :571-574 与 ::readWeight :632-635 三行式(floatCount=N/4→resize→memcpy N 字节)原样无长度校验；正对照 readPixels :546-551 几何校验在；readHeader 版本校验在——新边界未继承老纪律形态不变 |
| 49 | V1-N-11 | P2 | FIXED | 修在: 逐源聚合已并行 lib/phase1/noise/snr_frame_science.cpp:112-114 #pragma omp parallel for schedule(dynamic,1)(确定性 compact :127 设计保留)+根图接线 CMakeLists.txt:506-508 astrocs_phase1_noise 链 OpenMP::OpenMP_CXX(P10-UTIL2-007 注释自明)；原锚 lib/snr_estimator/cpp/src/snr_frame_science.cpp 已迁至 lib/phase1/noise/。判: 本条主claim「单线程长计算」消失=修到那一站；sum_p2 无记忆化+逐源全轮廓重建残留但属性能项(数值需实测,不另立) |
| 50 | V10-N-08 | P2 | STILL | ①hips_browser_backend.cpp:157/:162/:168-169 atoi 无界→:200/:216 uint32_t(1)<<leaf_order_ 移位 UB 原样；②orchestrator.cpp:3983 atoi hips_order→:3985 1u<<(hips_order+9) 未夹、:4450 A_ORDER 越界静默跳过 SIP、:1531 第三种写法——三写法并存原样；两文件仍在构建图外(现产不可达判词不变) |
| 51 | V11-N-07 | P1 | STILL | tools/check_abi_boundary.py HEADERS 仍 common_abi_v1.h+backend_host/*.h(排除 impl/inc) 固定清单,:13-19；两正则仍要求 tag 名(:28 typedef\s+struct\s+(\w+)、:36 同型)——消费侧 8 头(ipv_api.h 等,匿名 typedef struct 为主)仍全在门外；无语料↔消费者驱动对账 |
| 52 | V12-N-05 | P1 | STILL | compute_initial_mag_cut 仍零调用者(git grep 全仓=定义 :336+声明 h:64)却占 API_CONTRACTS.csv:329 VERIFIED；活路径 :437-438 内联同式+offset、:423 min(clamp_hi,13.0)；ipv_select.h 注释矛盾现仍在 :108(初值夹到 13)vs :112(m 全程夹 [clamp_lo,clamp_hi])；13.0 无 params 字段;两路径 4.000mag 差未测 |
| 53 | V12-N-13 | P2 | STILL | ipv_select.cpp:317-318 夹 [50,60] 裸常数、:312 注释「宽窄 FOV 统一为 60」原样；ipv_types.h:233 img_n_target=20+:232「自适应 20→40→60」(PLATESOLVE §5 该句现已不在 docs,口径宿主移入头注)——三口径互斥保持；无 DISP |
| 54 | V13-N-04 | P1 | STILL | docs/TRACEABILITY.csv 复算 63 行 status 全 VERIFIED；两主表消费检查器(tools/quality/check_traceability.py、contracts/check_traceability.py)grep VERIFIED=0 命中⇒status 列零门；生成器 v19r3_traceability.py ci/checks.json 0 登记；SCI-NOISE 15:1 TEST-SNR-001 单锚面另由 M3-F-004 承载 |
| 55 | V14-N-07 | P1 | STILL | 四导出仍真实存在 snr_estimator.h:240/:245/:250/:256(锚号未漂)而 lib/snr_estimator/module.yaml exports(:69)/source_symbols(:77) 0 登记，:9 头注仍自述「源码核对清单…禁止手抄他版」；三面(header/yaml/PUBLIC_API)无一致性门 |
| 56 | V18-N-11 | P1 | STILL | p1drz_tests_selfcheck.cpp:49 argc>=1&&getenv(空值即污染)原样、:72 child_env[2]={env_buf,nullptr}(:84 execve→_exit(127))、唯一 127 判在 :95；p1cal/p1psf 等 child_env[]={fbuf,nullptr} 单条目环境原样(:48/:49)；父侧仍「非零即通过」 |
| 57 | V2-N-10 | P0 | FIXED | 修在 ipv_select.cpp(改后文件,P14-N-10 注记直引 V2-N-10): ①交付面 StarSelection.m_lim_converged/m_lim_query_failed(:559-560, ipv_types.h:102-104)+触顶 converged=false(:491)+:536 不收敛日志；②弃 fmod——可靠判据 n_ret>=cap_unit(:487-488)+break 先于 alpha 差分(:489-500)；③NaN/非法入参 :397/:402/:407/:413 isfinite 显式拒、step NaN 兜底 :528；回归锁 ipv/test/test_mag_iter.cpp(CMake:38)+test_mag_iter_delivery.cpp(:53, 交付面三态断言)——三残余全消,判完全；他站: 无其他 fmod-cap 启发式(grep 0) |
| 58 | V6-N-04 | P1 | STILL | CONTROL_WEIGHT_SNR.md:51 推导权威仍指 run/release-rescue/science-phot/…(run/* gitignored, tracked 仅 .gitkeep)；tracked 面 run/ 权威引用现仍成片: ipv_types.h×2、ipv_select.cpp、snr_frame_science.cpp:11(oracle 锚值来源)、gaia_client.c:39/:2113、ipv_api.h:309、PUBLIC_API.md:988/:1062、CLI_PROTOCOL_V1.md:76、p1psf_tests_core.cpp:852、known_failures.json:78-79——改后文件未清；无「tracked 引 run/ 即红」门 |
| 59 | V7-N-06 | P1 | STILL | module_adapters.cpp:1391-1392/:1398/:1429/:1494-1495: 非 K 分支交付 dark_scale=k_fixed(默认1.0)与 K 分支 actual_k=1.0 逐字段不可分,manifest 不记 dark_optimization/k_branch 来源(无 k_source 字段)；K 分支 fail-closed 正例未动 |
| 60 | V8-N-06 | P2 | STILL | gaia_client.h(改后文件)第 3 行仍「契约见 GAIA_QUERY.md Postconditions/Invariants/并行模型」——三节名 grep GAIA_QUERY.md 现 0 命中(实为「并发模型」「不变量」)；CACHE_POLICY.md 半为真未变；其余自报量(64/307200000/60s/8192/4GiB)与代码同式 |
| 61 | V9-N-06 | P1 | STILL | 扇出结构原样: CON-FULL-INTEGRATION(tools/quality/contracts/check_full_integration.py subprocess 聚合 generate_contract_report.py 的 10 checker)+KNOWN-FAILURES-BASELINE-CHECK(waivable=False,未登记新红即红)；基线现仍 1 条(p1_noise_adapter, conditional, 2026-12-31)——复数与报告判词一致 |
| 62 | V9-N-15 | P1 | STILL | ci/checks.json(改后文件)UT-CLI 仍 waivable=false+dirty_ignore_prefixes=[astrocs_run_, run/]+dirty_ignore_exact=[resource_samples.csv,resource_summary.json,worker_balance.csv] 裸根名；根目录 astrocs_run_*.json 实测 56 个未跟踪；AGENTS.md「落 run/cli_runs/」禁令与 ignore 名单对立原样；无裸根名即红门 |

## 统计（RC6 · verify[5::8] · 62 条 · HEAD a3a343a4）

| 状态 | 条数 |
|---|---|
| STILL | **58** |
| FIXED | **4**（M3-C-001、M4-C-03、V1-N-11、V2-N-10） |
| MOVED | 0 |
| CANNOT_STATIC | 0 |

- 优先级×状态：P0 11 条（FIXED 3：M3-C-001/M4-C-03/V2-N-10；STILL 8）；P1 40 条全 STILL；P2 11 条（FIXED 1：V1-N-11；STILL 10，其中 M7-A-129 建议降级）。
- FIXED 四条全部有改动文件内证据（B4-2/B4-2、P14-N-10/RQS-V2-N-10、P10-UTIL2-007/P8-SNR-LINUX），并复算了机制消失+回归锁注册；除 V2-N-10 外无同类他站残留。
- 特殊说明：
  - **M7-A-129**：判词按「主事实①跳象限欠归一」在现代码与 base 码均不成立（nearest 填充、Σw 恒代数 1、文件未改）——建议前台把该条降级为「Σw=1 精确成立」FP64 表述缺陷（并入 M7-F-201 族）或订正证据。
  - **M6b-E-006**：主锚 :6-7 原样仍 STILL；但账本证据「SPEC:135-136 $F.2」句在 HEAD 与 base 的 TRACEABILITY_SPEC.md 中均 0 命中，git grep 全仓无宿主——建议前台订正该子引。
  - **M2a-F-2**：证据文件 gaia_xpsd_fixture_sp_gen.c 已并入 gaia_xpsd_fixture_gen.c（:486），属锚更新非 MOVED。
  - **V12-N-13**：三口径第三项（20→40→60）现宿主在 ipv_types.h:232 注释（PLATESOLVE.md 中该句已不可寻），互斥实质未变。
  - **M9-G-3**：账本原记文件面按现树修正为 lib/gaia_xpsd_client/src/module_entry.c（非 gaia_client.c）；drizzle 同型站一并登记。
  - 改动文件（35）中命中本片的 14 个均按符号重定位复算，无「因文件改过就判 FIXED」情形（ipv_select.cpp、module_adapters.cpp、stage2.cpp、snr 组等改后缺陷面多在——逐条见上）。

