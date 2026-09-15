# RC4 复验档案（recheck round 1 · verify[3::8] · 62 条）

> 时点 HEAD：`a3a343a44080d917089e1f8d548ed2d0400b0c61`（与派发时点一致；tracked 源码工作树与 HEAD 无差异，复验以工作树读取 = HEAD 内容，关键处以 git --no-optional-locks show/diff 双确认；docs/TRACEABILITY.csv 工作树有未提交改动，判定一律以 HEAD blob 为准）。
> 方法：一律按 文件::符号 重新定位（不依赖原行号）；FIXED 须给出「修在哪」的证据（文件::符号 + 关键 diff/新逻辑）并复核同类他站；缺失判定走三级复核（符号 grep → 文件 ls-files/find → git show）。纯静态：read/glob/grep + 只读 python3 + git 只读；未运行任何编译器/ctest/门脚本。
> 四态：STILL（缺陷机制复算仍在位，给新锚）/ FIXED（已修，给证据+完全性判定）/ MOVED（位置变、缺陷原样）/ CANNOT_STATIC（需运行期终判）。

## 一、四态结论表

| # | ID | 结论 | 新锚（文件::符号，截断展示） |
|---|----|------|------|
| 1 | FD-G-001 | **STILL** | `run/reaudit_v3/run001/A/AGENTS.md::「正式运行只有 orchestrator.exe」段(现:155)/「toolchain.ps1 run 唯一正式入口」(:179)；B 份同文；影子` |
| 2 | L28e-E-003 | **STILL** | `docs/science/PHASE2_UPM.md::ALG-UPM-002/003/004 绑定列表(现:112-115)；对照 lib/phase2/src/upm.cpp::文件头注释(:10-23)` |
| 3 | M1a-A-008 | **STILL** | `docs/science/PHASE3_HIPS_TO_FITS.md::§5(:66) vs §8(:86)；docs/contracts/DATA_SEMANTICS.md::§30.4(:2430/:2442)；d` |
| 4 | M1a-C-006 | **STILL** | `docs/algorithms/PLATESOLVE.md::§5:115；ipv_select.cpp::select_image_stars(:720-728)；ipv_types.h::IPVSolverParam` |
| 5 | M1a-E-003 | **STILL** | `docs/algorithms/PHASE3_PROJ_IMPL.md:::11-12；PHASE3_RSMP_IMPL.md:::13-14/§3:54` |
| 6 | M1a-H-001 | **STILL** | `lib/core/src/module_adapters.cpp::p3_op_resample(:5677) 内 worker(:5771-5773)、band_task(:5851-5864)、计数门(:5886)、` |
| 7 | M2a-B-2 | **STILL** | `docs/standards/STANDARDS_REGISTRY.md::D.catalog 判定表首行(:193)；CLAUSES(:186)；偏差表(:204)；检查器 docs/standards/checks/` |
| 8 | M2a-C-14 | **STILL** | `contracts/data/phase_product_exchange.schema.json::storage_uri(:54)；contracts/data/artifact_manifest.schema.js` |
| 9 | M2a-C-9 | **STILL** | `lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file(:1219 魔数唯一文件级校验、:1221-1222 header_len 读后即弃)、::parse_tre` |
| 10 | M2a-E-4 | **STILL** | `modules/services/io/include/astrocs/io/hips_input_v1.h::文件头角色行(:3)；docs/interfaces/io/IO_002_HIPS_INPUT_INTERF` |
| 11 | M2a-H-1 | **FIXED** | `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::hp_drizzle_run 精度段(:966-1008)；lib/core/src/module_adapters.` |
| 12 | M2b-A-02 | **STILL** | `lib/astro_image_io/src/hips/aio_hips_writer.cpp::finalize_hierarchy(:1069 nside_k=1u<<(k+9))/::aio_hips_produc` |
| 13 | M2b-B-08 | **STILL** | `docs/standards/STANDARDS_REGISTRY.md::D.spherical-projection 清单行(:62)/D.hips 行(:94/:97/:98/:99)/D.drizzle 行(:1` |
| 14 | M2b-G-01 | **STILL** | `ASTROCS_PROJECT_CONSTITUTION.md::§5.2(:152)/::§6.2(:184)；lib/hips/module.yaml(:33 module_id=astrocs.p1.hips_wr` |
| 15 | M3-A-005 | **STILL** | `lib/snr_estimator/cpp/src/noise_model.cpp::noise_model_build(:263-264 has_spatial_field=(enable&&n>=4))与 ::fil` |
| 16 | M3-C-007 | **STILL** | `docs/modules/registry/astrocs.phase1.calibration.md:::7 upstream 与 :56；对照 lib/calibration/module.yaml(:16/:20)` |
| 17 | M3-F-002 | **STILL** | `tests/backend/test_calibration_oracle.py::_approx_list(:224-227)/::test_01..test_06 容差(:232/:237/:254/:264/:27` |
| 18 | M3-I-004 | **STILL** | `lib/phase1/tests/p1phot/p1phot_fixtures.hpp::头注(:13-14「测试面共址 lib/phase1/tests/p1phot」)；对照 lib/photometric_cali` |
| 19 | M3b-C-01 | **STILL** | `lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py::门判定段(:304-335，blocker 字面量 :328-335)；ev` |
| 20 | M3b-F-02 | **STILL** | `docs/science/PSF.md::§11(:99)/§13(:113)/§15(:121-126)；docs/traceability/TRACEABILITY_MATRIX.json::MOD-astrocs-` |
| 21 | M4-C-01 | **FIXED** | `lib/phase2/src/sampler.cpp::kcorr_lookup(:88-110 两段分段线性)；seam:::1161 astrocs_phase2_kcorr_lookup_for_test；回归 l` |
| 22 | M4-E-02 | **STILL** | `docs/algorithms/PHASE2_INTEGRATION.md::§1(:4)/§3(:49/:64/:113)/§11.3(:234 起)/§11.4 F5(:266-269)；docs/algorithm` |
| 23 | M4-F-08 | **STILL** | `tests/backend/test_p2004_reject_integrate.py::docstring(:3-8)/driver 段(:43/:48/:111)/::test_02..05(:157-172)；l` |
| 24 | M5a-G-001 | **FIXED** | `cli/resource_gate.h::kCpuMeanMinPercent(:98)/kMon001UtilSampleMinPercent(:108)/kMon001QueueUtilMinPercent(:111` |
| 25 | M5a-G-009 | **STILL** | `tools/quality/check_isa_leak.py::docstring(:11)/::check provider 侧(:101/:110)；ci/checks.json::ISA-LEAK-SELFTES` |
| 26 | M5b-C-07 | **STILL** | `docs/algorithms/PHASE3_PROJ_IMPL.md::非目标(:20)/:::190（旧清单 SIN/ZEA/CAR/AIT）vs ::§15(:348/:355 正确四投影)；ASTROCS_PRO` |
| 27 | M5b-G-02 | **STILL** | `tools/quality/check_traceability.py::main(:159 无条件 return 0)；tools/check_traceability.py::DEFAULT_TABLES(:16)/` |
| 28 | M5b-G-10 | **STILL** | `tools/check_legacy_exit.py::check(:19-21 无 else、nm 无 timeout)/::main LEG-004(b)(:67-70 同型、nm 无 timeout)/::(c) ` |
| 29 | M5b-G-18 | **STILL** | `docs/modules/registry/astrocs.phase3.resample.md:::15/:35/:40/:52；lib/core/src/module_adapters.cpp::phase3_des` |
| 30 | M6a-C-001 | **STILL** | `lib/phase3_session/p3_session.cpp::p3_session_run(:198-199 计算、:358/:371/:405 仅记账)；lib/phase3_session/p3_resamp` |
| 31 | M6a-D-005 | **STILL** | `lib/snr_estimator/cpp/include/snr_estimator.h::snr_noise_model_v1_fill 声明注释(:168-170)；lib/snr_estimator/cpp/sr` |
| 32 | M6a-D-013 | **STILL** | `lib/phase3_session/p3_resample.cpp::p3_sample_bilinear_ex(:176 pix2ang_nest 死计算、:251 (void)y1;(void)c_ra;(void` |
| 33 | M6a-I-004 | **STILL** | `lib/calibration/cpp/cosmetic_corrector.cpp::文件头(:1-7 无 legacy 声明)/::修复循环(:139/:143/:196 无条件 stderr)；lib/calibr` |
| 34 | M6b-E-004 | **STILL** | `引用族（现势命中）：02_FROZEN_STAGE1_HISS_SPEC 14 处/13 文件、00_COMMON_CONTRACTS 26 处/17 文件、SNR_SCIENCE_DERIVATION 3、SNR_RE` |
| 35 | M6b-G-005 | **STILL** | `ci/checks.json::PRODUCTION-GRAPH(command=--selftest)/::TRACEABILITY-MATRIX(command 无 --strict)；tools/quality/c` |
| 36 | M7-A-119 | **STILL** | `docs/science/PSF.md::§2(:28 flux=ADU 含推导注,订正已在 base 前 b0353303)/::§5(:52 无单位标签)；docs/science/PHOTOMETRY.md::§2` |
| 37 | M7-A-127 | **STILL** | `docs/algorithms/REJECTION_ALGORITHMS.md:::18/:37/:99（ESD α=0.05/max10 被当冻结判据引用）` |
| 38 | M7-A-137 | **STILL** | `docs/algorithms/PHASE3_PROJ_IMPL.md::§15.3 CAR 段(:415-417「无投影奇点（δ 线性）」)；::zenithal θ 用法(:369-370 θ₀=CRVAL2/:41` |
| 39 | M7-G-103 | **STILL** | `docs/standards/SCIENTIFIC_CONSTANTS.md（仍不存在）；docs/science/NOISE_MODEL.md+algorithms（1.482602218505602/1.482602` |
| 40 | M8-B-001 | **STILL** | `lib/core/src/artifact.cpp::Provenance::science_hash(:62 mix(created_utc)/:63 mix(platform))；docs/interfaces/da` |
| 41 | M8-F-006 | **STILL** | `ci/checks.json::UT-*.command（18 个 discover -s，lib 面仅 lib/gaia_xpsd_client/tests）；lib/photometric_calib/cpp/tes` |
| 42 | M8-G-001 | **STILL** | `tools/quality/check_ctest_registration.py::collect_real(:202)/::ADD_TEST_NAME_RE(:65/:105 起点=add_test)；ci/vali` |
| 43 | M8a-C-005 | **STILL** | `lib/**/module.yaml(21) vs lib/core/src/module_adapters.cpp::d.module_id(22)；lib/snr_estimator/README.md:::1；li` |
| 44 | M8a-G-003 | **STILL** | `tools/quality/gen_source_index_v61.py::VENDORED_PARTS(:35)/::is_third_party(:95)；tools/gen_v19_evidence.py::is` |
| 45 | M8a-I-003 | **STILL** | `tools/README.md::全文(:1-3 astro_toolkit 自述/:24/:63 git_push 步/:75 一键 add→commit→push)` |
| 46 | M9-B-2 | **STILL** | `lib/healpix_db/healpix_drizzle/fits_reader.cpp::CD 构造块(:359-368 含独立 DEG2RAD 字面 :362)；lib/healpix_db/healpix_dr` |
| 47 | M9-G-1 | **STILL** | `lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file(:1194 memset 首行清零/:1219 魔数/长度校验裸 return -1 无回收，正对照 :121` |
| 48 | M9-H-3 | **STILL** | `lib/gaia_xpsd_client/src/gaia_client.c::read_leaf_block(:1115 自报偏移寻址/:1121 非压缩 memcpy/:1131 zlib uncompress)；:` |
| 49 | V1-N-06 | **STILL** | `lib/snr_estimator/cpp/src/snr_estimator.cpp::snr_extract_model(:574-578 仅初始化五旧字段；早退 :586/:594/无有效星支)；三字段赋值迟至 :` |
| 50 | V10-N-06 | **STILL** | `lib/healpix_db/healpix_drizzle/fits_reader.cpp::readFits(:315 仅判 <=0/:333-340 naxis3 归一无上限/:419-420 n_pixels=w` |
| 51 | V11-N-05 | **STILL** | `include/astrocs/abi/lifecycle_v1.h::六判定函数声明(:158/:163/:169/:177/:194/:234)；tests/abi/abi002_lifecycle_probe.c:` |
| 52 | V12-N-03 | **STILL** | `docs/science/NOISE_MODEL.md::§附注2(:135)；lib/photometric_calib/cpp/src/star_matcher.cpp::_MAD_SCALE(:21/:545)；l` |
| 53 | V12-N-11 | **STILL** | `lib/gaia_xpsd_client/src/gaia_client.c::MAX_STARS_RESULT(:32，原 :31 因 P19-gaia 下移 1)/::QUERY_CACHE_MAX_BYTES(:1` |
| 54 | V13-N-02 | **STILL** | `docs/traceability/TRACEABILITY_MATRIX.json::P1-CAL 行(:106)/P1-NOISE 行(:210)/photometry 同形行；判定器 tools/traceabil` |
| 55 | V14-N-05 | **STILL** | `cmake/install_layout.cmake::if(TARGET) 门卫族(:65/:71/:77…，:21 注 noise 摘出)；根 CMakeLists.txt:::210-214/:239(F-CI-0` |
| 56 | V18-N-09 | **STILL** | `lib/dynamic_psf/tests/p1psf/p1psf_tests_core.cpp::注入名变量(:77 F_REC="recovery"/:78 F_ABI=…，:986 同名局部量第二定义)；消费站点(` |
| 57 | V2-N-08 | **FIXED** | `lib/core/src/module_adapters.cpp::p1_op_noise(:2629 起，P14-N-08 snr.max_sources 唯一开关+truncated)；lib/phase1/nois` |
| 58 | V6-N-01 | **STILL** | `docs/TRACEABILITY.csv::全表(63 数据行，PSF/REJ 关键词零命中；SCI-PSF-001/SCI-REJ-001/SCI-INT-001/SCI-ACR-EQUIV-001 四行在 HEAD` |
| 59 | V7-N-04 | **STILL** | `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::WCS 解析段(:405-409 读 CD、:428 has_cd=(cd11!=0//cd22!=0)、:433-4` |
| 60 | V8-N-02 | **STILL** | `lib/phase1/noise/snr_frame_science.cpp:::3 宣称注释/::median_of(:30，用于 :142/:166/:170)/::2.5*dex→mag(:86)；lib/phas` |
| 61 | V9-N-04 | **STILL** | `docs/contracts/API_CONTRACTS.csv::API-estimate_mag_lim_by_density(:331，行锚自原 :371 漂移-40)；lib/plate_solve/cpp/ip` |
| 62 | V9-N-13 | **STILL** | `ci/checks.json::135 门中 104 门 changed_paths 不含自身 checker 脚本、131 门不含 ci/checks.json；例 CON-API-CONTRACTS paths=[d` |

统计：STILL 58 ／ FIXED 4 ／ MOVED 0 ／ CANNOT_STATIC 0

## 二、逐条证据（命令+输出摘要）

### 1. FD-G-001 — STILL
- 新锚：run/reaudit_v3/run001/A/AGENTS.md::「正式运行只有 orchestrator.exe」段(现:155)/「toolchain.ps1 run 唯一正式入口」(:179)；B 份同文；影子 57 份指令文件仅根 AGENTS.md tracked
- 复验证据：find+git ls-files+wc -l+grep 关键句。57 份指令文件（原报 18），tracked 仅根 AGENTS.md；207 行影子两句互斥原文逐字在位；长度 2×207/51×78/3×57/1×5 四种并存；无新增一致性门（checks.json 仅 AGENTS-GOV 检根文件，不比对影子）；负责人裁定"仅登记"，条目自证不撤。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 2. L28e-E-003 — STILL
- 新锚：docs/science/PHASE2_UPM.md::ALG-UPM-002/003/004 绑定列表(现:112-115)；对照 lib/phase2/src/upm.cpp::文件头注释(:10-23)
- 复验证据：grep -n "ALG-UPM-00" → 绑定行逐字在位；grep -rE "ALG-UPM-00[234]" lib/ tests/ tools/ ci/ providers/ runtime/ modules/ cli/ include/ → 命中 0；upm.cpp 头仍挂语义式 ID；同族 5 枚真源 grep 亦零命中 ⇒ 双命名空间并存原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 3. M1a-A-008 — STILL
- 新锚：docs/science/PHASE3_HIPS_TO_FITS.md::§5(:66) vs §8(:86)；docs/contracts/DATA_SEMANTICS.md::§30.4(:2430/:2442)；docs/algorithms/PHASE3_RSMP_IMPL.md::§6.6(:207-208)；lib/phase3_session/p3_resample.cpp::p3_sample_bilinear_ex(:243)
- 复验证据：grep：:66「有限 tile 像素」与 :86「NaN→C=1」字面冲突逐字在位（行号未漂）；RSMP §6.6 同抄两值；实现 :243 注「§4: NaN 参与仍 C=1」按 :86 走 ⇒ 矛盾原样、不变量仍不可测。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 4. M1a-C-006 — STILL
- 新锚：docs/algorithms/PLATESOLVE.md::§5:115；ipv_select.cpp::select_image_stars(:720-728)；ipv_types.h::IPVSolverParams(:219-222)；ipv_solver.cpp(:440-443/:849/:1514/:1590/:1661)
- 复验证据：三机制全在：①排序键互斥（文档 flux 降序 vs 实现 box-mag 升序+flux 注「当前未使用」）；②默认 20 被 60 覆盖（const 改为 for(:{60}) 单档循环，注释自证「不做自适应扩充」，types 注释仍吹 20→40→60）；③覆盖站点 3→5 处。diff 521095b8..HEAD 仅 P14-N-10 mag_iter 交付+ABI 锁，未触本条。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 5. M1a-E-003 — STILL
- 新锚：docs/algorithms/PHASE3_PROJ_IMPL.md:::11-12；PHASE3_RSMP_IMPL.md:::13-14/§3:54
- 复验证据：合同钉值仍 50/165/58 行（实测 2026-09-11/12 字样在位），wc -l 实测 63/230/116 ⇒ 偏差 +13/+65/+58 未更新，行锚未漂。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 6. M1a-H-001 — STILL
- 新锚：lib/core/src/module_adapters.cpp::p3_op_resample(:5677) 内 worker(:5771-5773)、band_task(:5851-5864)、计数门(:5886)、corrupt(-2 :5779)、注入(:5853-5857)
- 复验证据：四要素原样：signal open 失败裸 return；band_executed 无条件+1；门只比计数；仅 uncertainty 有 corrupt 通道；注入只演任务被吞。module_adapters +257/-25 未触此路径，无 open_fail 用例 ⇒ STILL（行号自 4824 区漂移至 5768 区）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 7. M2a-B-2 — STILL
- 新锚：docs/standards/STANDARDS_REGISTRY.md::D.catalog 判定表首行(:193)；CLAUSES(:186)；偏差表(:204)；检查器 docs/standards/checks/check_standards_registry.py::C3/LEGAL_STATUS
- 复验证据：:193 条款列含历元 J2016.0、标准要求列丢历元、仍 CONFORMANT+偏差"无"；DISP-GAIA-001 覆盖面不含历元；检查器无"判定式覆盖条款面各维"断言 ⇒ 登记方法缺陷原样，D.fits 同型亦在。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 8. M2a-C-14 — STILL
- 新锚：contracts/data/phase_product_exchange.schema.json::storage_uri(:54)；contracts/data/artifact_manifest.schema.json::storage_uri(pattern+maxLength1024)/::size.maximum(:71)；runtime/artifact_store/artifact_manifest_validator.py::TYPE_ID(:40)/::_is_nonneg_int(:72-73)；phase_product_exchange_validator.py::_TYPE_ID_RE(:54)/::min_planes(:267)
- 复验证据：python3 解析+grep：exchange schema storage_uri 仍仅 minLength:1（manifest 侧有 URI pattern），词法分叉在；validator::_is_nonneg_int 仍无 int64 上限（schema :71 maximum 在执行面失效，方向相反）；type_id 两正则字面分叉（\.v[0-9] vs \.[v][0-9]+）逐字在位；min_planes/role↔format 断言仍只存在于 validator 执行形态(:267)。contracts/ 与 runtime/ 不在 base→HEAD 变更清单。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 9. M2a-C-9 — STILL
- 新锚：lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file(:1219 魔数唯一文件级校验、:1221-1222 header_len 读后即弃)、::parse_tree(:1329-1337 root_pos/node_count 无区间校验、:1356-1358 child memcpy)、::search_recursive(:1644-1653)/::search_recursive_spectrum(:1784-1790)/::search_recursive_photometry(:1921) 下钻无界；tests/unit/gaia_cat_test.c::mode_negative(:661-662/:688/:696)
- 复验证据：grep 复算：header_len 全文件仅 :1221-1222 两命中（读出即弃无一致性校验）；:1333 仍仅 node_count>0&&root_pos>0 即 xf->mmap_data+root_pos 裸偏移，无 root_pos+node_count*stride≤mmap_size；child 索引直接 memcpy 自文件、下钻仅判非零不判 <node_count；测试仍以「坏魔数被加载 file_count=0（应静默跳过）」「只要部分树可加载即接受」钉成期望。本文件 base→HEAD 的 +102/-17 属 P19-gaia magnitudeRange 有界解析（V5-N-03），未触碰字段撒谎族。行锚自原报漂移 +185 左右。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 10. M2a-E-4 — STILL
- 新锚：modules/services/io/include/astrocs/io/hips_input_v1.h::文件头角色行(:3)；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md::§2 归属表(:43-44)；CMakeLists.txt::add_library(astrocs_io SHARED runtime/io/fits_core.c)(:146)
- 复验证据：复读：h:3 仍称「astrocs_io.dll 对外的 HiPS 输入读取合同」；IO_002:44 仍写「C 实现、私有、DLL 内」；astrocs_io target 源列表仍仅 fits_core.c；grep hips_core 于全部 CMakeLists/*.cmake 零命中；acs_hips_open_v1 仅 selftest/头声明/合同文档，无生产调用点。发布归属失实原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 11. M2a-H-1 — FIXED
- 新锚：lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::hp_drizzle_run 精度段(:966-1008)；lib/core/src/module_adapters.cpp::p1_op_drizzle(:3033-3067)；回归 tests/unit/drizzle_precision_default_test.cpp::T7/T8；drizzle_engine.cpp 累加域 TemplateT<double>(:871/:1553)
- 复验证据：修在哪：①p1_op_drizzle 注释点名 M2a-H-1——precision_mode==1 时输入上转 double、以 AIO_BLOCK_FLOAT64 提交（:3041-3056）并加块 dtype==precision_mode 显式自检（:3060-3067）；②hp_drizzle_run 点名 M2a-H-1/RESCUE-FD-02——precision_mode 参数优先、非法值/未知 PRECISION KV 显式拒绝(-1)，-1 且无 KV 缺省升 FP64（宪章 §5.3「不再静默 FP32」），且 config.precision_mode==1 && !data_is_f64 → return -14 fail-closed(:1002-1008)；③累加域随 use_f64：FP64 走 drizzleTiled<double>/processPixel_f64 double 累加，原「恒 FLOAT32 提交+事后加宽落盘」站点(原:2303)已不存在。修完全性：门在 hp_drizzle_run choke point，p1 节点(:3184)与 lib/drizzle DLL adapter(module_entry.cpp:929 传 precision_mode)两通道同受；回归 T8 显式测「请求 FP64 而块 FLOAT32 → 拒绝」、T7 推翻「FP32/FP64 数值等价」旧断言。同类他站：未见残留恒 FLOAT32 提交+FP64 声明通道。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=FIXED

### 12. M2b-A-02 — STILL
- 新锚：lib/astro_image_io/src/hips/aio_hips_writer.cpp::finalize_hierarchy(:1069 nside_k=1u<<(k+9))/::aio_hips_product_begin(:481 tile_width!=512 即拒)；docs/algorithms/HIPS_WRITER.md::ALG-HIPS-004(4c)(:142 与 :146 同节两式)；IO_002::§3.1(:67-68)/§3.3(:104)；runtime/io/hips_core.c::hips_parse_properties(:245)；hips_input_v1.h(:31/:33)；lib/phase3_session/hips_properties.h(:23/:25)；lib/hips/src/module_entry.cpp::hips_cfg_parse(:342/:351)
- 复验证据：逐站复读：写侧 2^(k+9) 与写死 512 在位；ALG 同节 :142「nside_k=2^(k+9)」与 :146「NSIDE=2^k」互斥两句在位；IO_002 三句（TW 2 的幂 1≤TW≤16384、0≤K≤29、NSIDE 必等 2^(K+9)）在位；读侧 :245 仍硬编 order+9、TW 不参与；四层入参域（v1.h 29/16384、phase3 20/512、lib/hips [512,2^24]/512、IO_002 29/16384 但读侧执行不实现 TW）无一变化 ⇒ 自产层级 tile 必被判 TILE_INVALID、TW≠512 合法外部产品系统性误拒两机制原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 13. M2b-B-08 — STILL
- 新锚：docs/standards/STANDARDS_REGISTRY.md::D.spherical-projection 清单行(:62)/D.hips 行(:94/:97/:98/:99)/D.drizzle 行(:160/:163)；§3 索引(:239 STD-F1 自造字面量/:243 STD-F4 尾巴)；docs/standards/checks/check_standards_registry.py::C4_checklist_deviation_column(:272-274)/::C7_deviation_index_rows_resolve(:316-333)
- 复验证据：程序化+定点复读：CONFORMANT 行挂开放 ID 的 7 行（:62/:94/:97/:98/:99/:160/:163）逐字在位；索引有而清单偏差列零挂的 8 个 ID（DISP-P3PROJ-001、DISP-HIPS-001/-004/-007/-010/-011、DISP-DRZ-006/-008）复算命中一致；§3 状态列 CLOSED=0、STD-F1 行仍为「CONFORMANT（导出边界…）」越域自造字面量、STD-F4 仍带自由文本尾巴；检查器无 C4′（CONFORMANT 行偏差列须空）/C7′（状态值域）判据，C7 对「条款列语义错挂」仍无判据。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 14. M2b-G-01 — STILL
- 新锚：ASTROCS_PROJECT_CONSTITUTION.md::§5.2(:152)/::§6.2(:184)；lib/hips/module.yaml(:33 module_id=astrocs.p1.hips_writer,:37 dll_name)；lib/hips/CMakeLists.txt(:43 add_library(astrocs_p1_hips_writer SHARED))；根 CMakeLists.txt(:208 add_subdirectory(lib/hips))；lib/hips_p2/module.yaml(:41/:48 CONTRACT_READY/MISSING)
- 复验证据：复算：宪章两句原文在位；git ls-files lib/infrastructure=0、目录不存在；lib/hips 顶层模块+SHARED 目标+add_subdirectory+packaging required_units 交付面在位；lib/hips_p2 仍为 3 文档壳(module.yaml/memory.md/README.md)且以 CONTRACT_READY 名义先行登记 target=astrocs_p2_hips_writer.dll；docs/architecture/ 与 STANDARDS_REGISTRY 零架构偏差登记。归属冲突原样、待负责人裁决状态不变。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 15. M3-A-005 — STILL
- 新锚：lib/snr_estimator/cpp/src/noise_model.cpp::noise_model_build(:263-264 has_spatial_field=(enable&&n>=4))与 ::fill_impl 平面 LS 段(:401-405 det 判据，原 :392-398 漂移 +10)
- 复验证据：复读：:402-403 仍 const double det=sxx*syy-sxy*sxy; if(fabs(det)>1e-24){b=...;c=...;} 无 else、无退化标记；:263-264 has_spatial_field 仍与 det 无关 ⇒ 退化静默 b=c=0 而 has_spatial_field=1 机制原样；docs/science/NOISE_MODEL.md 与 NOISE_ESTIMATION.md grep det/条件数/1e-24 零命中 ⇒ 判据仍无出处、量纲不闭合。注：工作树出现未跟踪 lib/snr_estimator/{CMakeLists.txt,include/,src/} 新面（非 HEAD 内容），不改变 HEAD 上缺陷在位判定。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 16. M3-C-007 — STILL
- 新锚：docs/modules/registry/astrocs.phase1.calibration.md:::7 upstream 与 :56；对照 lib/calibration/module.yaml(:16/:20)；lib/calibration/src/ac_api.cpp::ac_version(:136-137)
- 复验证据：grep：registry :7/:56 仍引 SCI-P1-CAL-001/ALG-P1-CAL-001/API-P1-001；全 docs/+contracts/ 树 SCI-P1-CAL-001 命中仅该 registry 页、INDEX.yaml 命中 0；registry 页 version:1.0.0/status:ACTIVE(:3-4) vs module.yaml 0.11.0-alpha.2/CONTRACT_READY vs ac_version() 返回 "v1.0.0" ⇒ 三口径原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 17. M3-F-002 — STILL
- 新锚：tests/backend/test_calibration_oracle.py::_approx_list(:224-227)/::test_01..test_06 容差(:232/:237/:254/:264/:274)；对照 docs/science/CALIBRATION.md::§11:107/§15:133
- 复验证据：复读：_approx_list 仍 range(0,len,max(1,len//40)) 约 40 点抽样、非逐像素；容差仍 1e-3/1e-3/3e-3/2e-3/2e-3；SCI §11 仍冻结「逐像素比对，FP32 rtol=1e-6, atol=1e-7」、§15 仍声明该界「全过」。近两提交 bc059562(×10 夹具纠偏)/607b37e3 未收紧容差/抽样。③ g++ 同源编译 Oracle 形态不变。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 18. M3-I-004 — STILL
- 新锚：lib/phase1/tests/p1phot/p1phot_fixtures.hpp::头注(:13-14「测试面共址 lib/phase1/tests/p1phot」)；对照 lib/photometric_calib/tests/p1phot/*(14 文件含 CMakeLists)、tests/unit/CMakeLists.txt:::511
- 复验证据：三级复核：① 该目录在 HEAD 与 base 均无跟踪（git ls-tree 空）——孤立副本属工作树未跟踪件；② 工作树四文件仍在（p1phot_fixtures.hpp 等），头注仍自称「共址于 lib/phase1/tests/p1phot」并引 CMakeLists.txt 头注决策（该文件实际不存在）；③ 行数对比孤立=264/133/322/657 vs 注册=252/175/461/(注册套件无同名 tests_core.cpp) ⇒ 分叉且孤立份自原报时点又增长（417→657）。无构建接入原样（目录无 CMakeLists，注册面 :511 指向 photometric_calib）。判 STILL，附注：载体在免报/git 盲区，机器门不可达恰为本条卫生问题之实证。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 19. M3b-C-01 — STILL
- 新锚：lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py::门判定段(:304-335，blocker 字面量 :328-335)；evidence/gate2_result.json::gates(:27-33)/::astrocs_production_path.centroid_median_px(:5)；docs/KNOWN_LIMITATIONS.md(psf/质心/centroid 零命中)；docs/owner/RELEASE_STATUS.md(PSF-001/0.5px 零命中)；docs/algorithms/STAR_DETECTION_ALGORITHMS.md:::169；docs/science/STAR_DETECTION.md:::13；lib/star_detector/src/sdet_api.cpp:::548 vs lib/dynamic_psf/src/dpsf_psf.cpp:::295/:434-436
- 复验证据：复算：BLOCKER 仍未闭合——evidence 生产门 centroid_p95_le_0.3px_production=false、production centroid_median_px=0.7701px，oracle :328 字面「BLOCKER (PSF-001, 未闭合)」在案；规范层 docs/KNOWN_LIMITATIONS.md 对 psf/质心零命中、docs/owner/RELEASE_STATUS.md 对 PSF-001/0.5px 零命中；SCI:13/ALG:169 仍正面宣称「不引入 0.5px 网格量化损失」；sdet x+0.5-cx 与 dpsf x-cx 两采样式差异仍在（dpsf :434 注释自述旧实现引 ~0.5px 系统偏差、归一 cx+x0）。与原判不同的一点：验收门定义已按姊妹条 M3b-F-01 整改为生产路径（gates 名 *_production、converged 降为旁证 _converged_not_acceptance），这使门「会红」，但本条三事实（未闭合 BLOCKER 不入规范层、规范层正面宣称与 0.77px 实测矛盾、归档层独活）逐字在位 ⇒ STILL；0.5px 偏差本体是否已被实现消解需重跑 oracle 运行期终判（静态不可），然 evidence 现值为假即登记面矛盾仍红。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 20. M3b-F-02 — STILL
- 新锚：docs/science/PSF.md::§11(:99)/§13(:113)/§15(:121-126)；docs/traceability/TRACEABILITY_MATRIX.json::MOD-astrocs-phase1-star-psf(:261-266)；docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv(:53-55/:73-75)；lib/dynamic_psf/tests/p1psf/p1psf_oracle.hpp::容差表注释(:110-127)
- 复验证据：分子事实复算：① scipy/curve_fit 复算全仓（tests/lib）零命中，§11 承诺原文在位 ⇒ 在；② 原报 TRACEABILITY.csv:65 五失真行在 base 与 HEAD 的 CSV（64 行）中均已不存在（PSF 行整体缺失，含 star-psf 零命中）⇒ 该行消失但非"改正"，CSV 面反而无任何 PSF TEST/EVIDENCE 登记；③ TST-PSF-INV-*/FAIL-* 仍仅 docs 两处、tests/lib 零命中 ⇒ 在；④ MATRIX TEST-PSF-DESIGN-001 test_status=VERIFIED 与 evidence_id=EVID-MISSING/evidence_status=MISSING 并存、notes 自述「EVIDENCE 待落」原文在位 ⇒ 在；⑤ p1psf/p1star 在 INVENTORY 仍标 tool/非发布目标、未进 TEST/EVIDENCE 行 ⇒ 在；⑥ p1psf_oracle.hpp:115-127 自述 cen_abs=1e-9/s_rel=1e-6 为「probe 无噪声回收实证」并明注「不是 SCI §11」⇒ 容差不同源原样。综合 STILL（残余主体成立；②随 CSV 行删除失去逐字锚，改锚至"CSV 无 PSF 登记行"新事实）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 21. M4-C-01 — FIXED
- 新锚：lib/phase2/src/sampler.cpp::kcorr_lookup(:88-110 两段分段线性)；seam:::1161 astrocs_phase2_kcorr_lookup_for_test；回归 lib/phase2/tests/kcorr_lookup_test.cpp::TEST(kcorr,corner_exact/piecewise_midpoints_rtol_1e_12)；注册 lib/phase2/CMakeLists.txt:::95-99 add_executable(phase2_sampler tests/kcorr_lookup_test.cpp)
- 复验证据：修在哪：dce8abd4「fix(RQS/B3)…kcorr 网格…」将均匀取列改为按真实网格两段归一（lo=(pf<=0.8); c0/c1∈{0,1}/{1,2}; fx 段内归一），注释点名「不得按 [0.5,1.0] 均匀网格取列（F2 角点须精确等于表值）」；pf=0.8 现命中节点 bitwise 返回 1.3925/2.8971，原 +1.52%/+2.12% 表外值消失；scale 轴 r0 权重式端点 exact 保持。F2 角点门落地为可执行测试（corner_exact 逐点 EXPECT_EQ）并挂入 phase2_sampler 目标（原「无（新建）」）。修完全性：消费链 :554 kcorr_lookup(pf,300.0) 走同一修复后函数；PHASE2_SAMPLER.md 同提交更新。同类他站：非均匀网格插值 grep 未见第二处。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=FIXED

### 22. M4-E-02 — STILL
- 新锚：docs/algorithms/PHASE2_INTEGRATION.md::§1(:4)/§3(:49/:64/:113)/§11.3(:234 起)/§11.4 F5(:266-269)；docs/algorithms/PHASE2_SESSION.md:::21/:31/:84-86/:222/:254
- 复验证据：复算：integrate.cpp 实测 81 行，合同仍钉 76 行并自称「实测锚定」；B2-A7 已落地（integrate.cpp:46-50 sup_max 移到权重分支前，注释自述旧缺陷），但 §3 锚表 :64/:113 仍描述「位于 :49 continue 之后（DISP-P2INT-001）」、§11.3 现状缺陷清单仍在册未关、§11.4 F5 仍写「现状实现该门 FAIL」（现应 PASS）；PHASE2_SESSION.md 钉 282 行实测 298、module_adapters.cpp 锚 :777-784/:283-300/:746-751 全部漂移（现 P2Api :1013、phase2_descriptor :581、注册 :6424）、§11.2 :254 仍描述「run 成功→complete」（现 p2_session.cpp:252 链不完整期 status=partial）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 23. M4-F-08 — STILL
- 新锚：tests/backend/test_p2004_reject_integrate.py::docstring(:3-8)/driver 段(:43/:48/:111)/::test_02..05(:157-172)；lib/phase2/include/astro/phase2/rejection.h::COMPAT 注(:299)；lib/phase2/src/rejection.cpp::compat 体(:1888-1889)
- 复验证据：复算：①名义「生产 Oracle」docstring 原文在位、driver 仍走 compat p2_reject_stack 且 sv.frame_ids=nullptr（frame identity 无载体）；②compat frame_ids 错绑缺陷原文在位：eligibility_core 剔除后仍 for i<m: fids[i]=in->frame_ids[i]（compact 下标直取原始数组，未经掩码映射）；③Python 层 test_02-05 仍只复读 returncode+「P2-004 DRIVER PASS」字符串（:154-172）无独立判定。strided 面回归略有补强（synthetic_gate.cpp:4501+ ex API permutation+frame_ids、p2002 frame_slots 注入断言，均已注册 ctest），但 compat 错映射本体与其无门事实未动，且本条登记的 test_p2004 名义失真全数在位。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 24. M5a-G-001 — FIXED
- 新锚：cli/resource_gate.h::kCpuMeanMinPercent(:98)/kMon001UtilSampleMinPercent(:108)/kMon001QueueUtilMinPercent(:111)/compute_cores_threshold(:214-217)/:161 注释；cli/commands.cpp::run_with_resource_gate 判定体(:946-968)；tests/unit/p2_workers_test.cpp(:89-90)
- 复验证据：修在哪：adaeb531「fix(RQS/B2): 门禁硬化——恢复冻结资源门」——compute_cores_threshold 改 (kCpuMeanMinPercent/100.0)*m=0.85·m（0.80 字面消失）；常量改 85.0/60.0 且被 commands.cpp:960/964 实际引用（原「除定义行外零引用」+0.75/0.50 字面量判据改为常量比较）；:153/:161 注释订正为「阈值按 §18.2 冻结为 85%/60%」；p2_workers_test 断言随判据改（CHECK(==85.0)/(==60.0)，原 CHECK(t1==0.80*2.0) 消失）；:280 均值比较 <kCpuMeanMinPercent 会红。修完全性：反面对照 run_monitored.py 本就 0.85/0.60 一致化。残余小疵：commands.cpp:939 段注释仍写「U>=0.75/…U<0.50」旧数（代码已用常量，注释滞后），属登记型残余，不影响四态判定 ⇒ FIXED。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=FIXED

### 25. M5a-G-009 — STILL
- 新锚：tools/quality/check_isa_leak.py::docstring(:11)/::check provider 侧(:101/:110)；ci/checks.json::ISA-LEAK-SELFTEST(:623,--selftest :633)；对照 docs/architecture/ISA_VARIANTS.md::§3(:77)
- 复验证据：复算：checks.json 内 ISA-LEAK 仅 SELFTEST 一项（真实扫描不入 profile）、.github/workflows 对该脚本零直接调用；docstring :11「验证 link map(nm)」仍在、实现体除 docstring 外无 nm/link-map 逻辑；--avx2-lib/--avx512-lib 缺省/文件不存在仍静默跳过(:101/:110 `if avx2_lib is not None and avx2_lib.is_file()`)；AVX512VL/EVEX-xmm 面与 %k 掩码兜底未补；test_abi_kernels 在 checks.json 仍零命中。四子事实原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 26. M5b-C-07 — STILL
- 新锚：docs/algorithms/PHASE3_PROJ_IMPL.md::非目标(:20)/:::190（旧清单 SIN/ZEA/CAR/AIT）vs ::§15(:348/:355 正确四投影)；ASTROCS_PROJECT_CONSTITUTION.md::§18.1(:655)；传播面 README.md:::33、docs/modules/phase3_proj.md:::41、docs/modules/registry/astrocs.phase3.wcs.md:::51/:135、docs/contracts/PUBLIC_API.md:::2024；实现 lib/phase3_proj/p3_projection.cpp:::10-23
- 复验证据：grep 对账：:20/:190 旧清单（含未冻结 ZEA、漏已冻结 AIT）逐字在位，与同文档 §15 及宪章 §18.1「TAN+SIN+CAR+AIT」互斥；README:33、phase3_proj.md:41、wcs registry :51/:135、PUBLIC_API:2024 五处复制传播全在位；实现按四投影含 AIT(:16/:23/:220) ⇒ 文档↔实现↔宪章三方不一致与无集合对账门原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 27. M5b-G-02 — STILL
- 新锚：tools/quality/check_traceability.py::main(:159 无条件 return 0)；tools/check_traceability.py::DEFAULT_TABLES(:16)/::main(:110)；ci/checks.json::TRACEABILITY/TRACEABILITY-CODE/TRACEABILITY-MATRIX
- 复验证据：python3 解析 checks.json 复算：TRACEABILITY 命令仍 tools/quality/check_traceability.py（该文件 return 1 零命中、:159 return 0 恒绿，waivable=false、changed_paths 含 lib/cli/tests）；TRACEABILITY-CODE 仍无参 ⇒ DEFAULT_TABLES 仍指 artifacts/prerelease_v5/tables 快照而触发面含 docs/**；TRACEABILITY-MATRIX changed_paths 仍 [schemas,docs/**,evidence/**] 不含 lib/cli/tests ⇒ 代码侧断链三门无一会红。机制整体原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 28. M5b-G-10 — STILL
- 新锚：tools/check_legacy_exit.py::check(:19-21 无 else、nm 无 timeout)/::main LEG-004(b)(:67-70 同型、nm 无 timeout)/::(c) 源码判据(:73-76 仅 glob lib/phase2/src、豁免 acr_kernels.cpp)；ci/steps/linux_build_root_graph.sh(:14/:32 构建树 linux-control+cp build/astrocs)；ci/checks.json::ACR-DORMANT(changed_paths=[CMakeLists.txt,lib/phase2/**,legacy/**])
- 复验证据：复算：①二进制判据路径写死 build/root-cmake/astrocs 且 if exists 无 else——CI 步骤从 build/linux-control cp 到 build/astrocs，不产 root-cmake ⇒ 恒走跳过分支输出 PASS；②源码判据仍单目录 glob+双字面量与判据+豁免文件，acr_kernels.cpp 注释仍把注册指向门不扫的 module_adapters.cpp；③两处 nm subprocess.run 无 timeout(§14.5)。三机制逐字在位。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 29. M5b-G-18 — STILL
- 新锚：docs/modules/registry/astrocs.phase3.resample.md:::15/:35/:40/:52；lib/core/src/module_adapters.cpp::phase3_descriptor(:600)/注册(:6428-6432)/占位自认注释(:6489-6490)；runtime/pipeline/module_ports.registry.json(:269 resample2)；cli/runtime_client.cpp(:220)；docs/traceability/TRACEABILITY_MATRIX.csv:::25
- 复验证据：复算：registry 页「Registry production 模块」「parallel_ok=True」「确定性=固定顺序输出(1/N 等价已验)」「TEST-P3-RES-001 对应测试」四句逐字在位；占位 descriptor 仍以 make_session_module<P3Api> 整阶段包装注册且注释自认「P2 模板复制残留…由 P3-RSMP-INT 处理」；产品链路（ports/CLI IR）仍用 resample2；矩阵行 MOD-astrocs-phase3-resample 的 SRC/TEST/EVIDENCE 记 MISSING——页面「已验」与矩阵 MISSING 并存原样；同族两页 registry 无 descriptor 自述仍在（star-detection:13-14、session:13）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 30. M6a-C-001 — STILL
- 新锚：lib/phase3_session/p3_session.cpp::p3_session_run(:198-199 计算、:358/:371/:405 仅记账)；lib/phase3_session/p3_resample.cpp::p3_sampler_open_ex(:135-136)/::open 他径(:301-302)/::read_leaf(:53 s->order)/::p3_order_select(:84-95)；lib/astro_image_io/src/hips/aio_hips_reader.cpp::tile 路径(:105/:155/:230/:234 均 d->hips_order)；lib/core/src/module_adapters.cpp::p3_op_resample(:5749-5753 计算、唯一后继为记账)；docs/science/PHASE3_HIPS_TO_FITS.md::§9a-5(:56-57/:61)/§9a-11(:108)
- 复验证据：grep 数据流复算：p3_order_select 的 out_order 在会话通道只进 prov.order_sel_used(:371) 与 result JSON(:405)；IR 通道(:5749 计算后)读路径仍 p3_sampler_open_ex→s->order=p.order→leaf_nside=kTileWidth<<p.order(:135-136/:301-302)，采样 leaf 由 ang2pix_nest(impl->leaf_nside) 定位(:154/:173)；reader 组路径仍全部按 d->hips_order。SCI「读层级 leaf_nside=2^(order_sel+9)/ipix=ang2pix(nside=2^(order_sel+9))」两句原文在位(:61/:91→现 §9a 编号 :56-57/:61)。两通道 order_sel 均不参与执行、ORDERSEL 记录非实际读取层级——机制原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 31. M6a-D-005 — STILL
- 新锚：lib/snr_estimator/cpp/include/snr_estimator.h::snr_noise_model_v1_fill 声明注释(:168-170)；lib/snr_estimator/cpp/src/noise_model.cpp::g_model_floor(:32/:126/:412-413/:442)；lib/snr_estimator/include/astrocs/noise/types.h::op 词表注释(:59-61)
- 复验证据：复读：公共头 fill 段仍只写「最小二乘平面…（负预测 clamp 到 variance_floor）」，grep 侧表/影子/进程级/不得拷贝 在公共头零命中＝零披露；实现侧表仍是进程级指针键 static unordered_map(:32)，build 写(:126)/free 删(:442)/fill 每像素哈希查找回退 1e-12(:411-413 位于像素双层循环内)；types.h「影子实例不在 g_model_floor 注册表→floor 回退 1e-12…如实登记不消化」原样(:59-61)。四机制（静默 1e-12 分支、拷贝/生命周期约束不披露、登记替代义务、热路径哈希）全部在位。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 32. M6a-D-013 — STILL
- 新锚：lib/phase3_session/p3_resample.cpp::p3_sample_bilinear_ex(:176 pix2ang_nest 死计算、:251 (void)y1;(void)c_ra;(void)c_dec、y1 实用于 :229/:232/:234)；docs/algorithms/PHASE3_RSMP_IMPL.md::§6.5(:194)
- 复验证据：grep：死计算与三条 (void) 原样在位（c_ra/c_dec 除 :251 无消费者；y1 实际参与 dy 与零点求解而 (void)y1 仍在）；ALG §6.5 :194 仍把「(void)y1 压 unused」当实测写进冻结叙述。P5-SNR 批注未清理此死代码（同文件 kcorr 修复只动了网格段）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 33. M6a-I-004 — STILL
- 新锚：lib/calibration/cpp/cosmetic_corrector.cpp::文件头(:1-7 无 legacy 声明)/::修复循环(:139/:143/:196 无条件 stderr)；lib/calibration/cpp/cosmetic_corrector.h::CC_EXPORT(:7/:9/:21/:35/:47 默认可见导出)；docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:::79(cpp 路径行 production=yes)；lib/cosmetic/README.md:::159 遗留通道自述
- 复验证据：三级复核：文件存在且 tracked；头注七行与生产版雷同、无 legacy/未编译/公式差异声明；CC_EXPORT 默认导出 cc_* 符号；inventory :79 行 location=lib/calibration/cpp/cosmetic_corrector.cpp、classification=production、production_reachable=yes，与 :81(src 生产版)并立；README :159 仍自述「遗留通道…未编译进 CMake 主构建…公式与 ac_* 不同」。三方互斥与无条件进度日志原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 34. M6b-E-004 — STILL
- 新锚：引用族（现势命中）：02_FROZEN_STAGE1_HISS_SPEC 14 处/13 文件、00_COMMON_CONTRACTS 26 处/17 文件、SNR_SCIENCE_DERIVATION 3、SNR_REDESIGN_CONTRACT 4、CPU_ADAPTIVE_V1 2（真实文件全部 0）；tools/doccheck/ 无引用可解析校验器
- 复验证据：逐族 grep -rln/-c 复算：五族不存在名仍在真源被当依据（命中量级低于原报 139/40——原口径含 8 类与更多前缀，本轮抽验 7 族中 6 族在位、docs/validation/ACCEPTANCE_GATES 一族已零命中消失）；find 确认零真实文件；include/astrocs/exit_codes.h 引用仍指空路径（真实件为 cli/exit_codes.h）。tools/doccheck 仅 index/version/namespaces 三器，§12.3-9（引用可解析）机器实现仍缺。机制在、规模略缩 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 35. M6b-G-005 — STILL
- 新锚：ci/checks.json::PRODUCTION-GRAPH(command=--selftest)/::TRACEABILITY-MATRIX(command 无 --strict)；tools/quality/check_pipeline_graph.py::compare(:40 module_index 形参体零用/::coordinate 比较不存在)；tools/quality/contracts/check_test_contracts.py::伞形豁免(:38-44)；tools/traceability/check_traceability_matrix.py::STATUS_OK(:55)
- 复验证据：python3 解析+grep 复算：PRODUCTION-GRAPH CI 仍只 --selftest（真实 compare 不上线）；compare 形参 module_index 在函数体零引用、docstring 宣称的 unit/coordinate 比较在实现中无 coordinate 比对逻辑；TRACEABILITY-MATRIX CI 命令仍无 --strict（C8 越界仅 WARN）；check_test_contracts 的「TST- in tids」+synthetic_gate 存在即 continue 伞形豁免原样；三条追溯门组合仍无法证伪 VERIFIED（M5b-G-02 交叉复算 return 0/快照输入/changed_paths 无代码面）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 36. M7-A-119 — STILL
- 新锚：docs/science/PSF.md::§2(:28 flux=ADU 含推导注,订正已在 base 前 b0353303)/::§5(:52 无单位标签)；docs/science/PHOTOMETRY.md::§2(:14 F_instr ADU) vs ::§3(:96 「ADU·px 或 e⁻ 同尺度」)；docs/contracts/DATA_SEMANTICS.md 无「星点流量=ADU」钉死行
- 复验证据：对账 base/HEAD：PSF 侧 px² 第三标签已在 base 前被 P5-SNR 订正消掉（:28/:87 现明写 ADU+推导）——该子事实 FIXED；但本条主断言「链上标签互斥」残余仍在：PHOTOMETRY:96 仍写 ADU·px（把 px 当量纲保留的第三标签之线性形态），与同文档 :14 ADU 及 PSF 订正注互斥且无抵消说明；建议处置的 DATA_SEMANTICS 钉死与「两文档同文订正」未完成（PHOTOMETRY 未订、DATA_SEMANTICS 零行）。综合 STILL（部分修复：三套→两套，残站 PHOTOMETRY:96）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 37. M7-A-127 — STILL
- 新锚：docs/algorithms/REJECTION_ALGORITHMS.md:::18/:37/:99（ESD α=0.05/max10 被当冻结判据引用）
- 复验证据：grep：ESD 块文字与 base 一致（文件不在变更清单）；n≥15/25 有效性域前置仍零命中（:14 的 n<6/6≤n≤15/n>15 是方法路由档位，非 NIST ESD 近似有效性域）；「未纳入 NIST 自述近似有效性域」与「α 被当冻结统计判据引用」两机制原样。发生率维仍属运行期（原报即注「无法判定（发生率）」），文档面可静态判 STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 38. M7-A-137 — STILL
- 新锚：docs/algorithms/PHASE3_PROJ_IMPL.md::§15.3 CAR 段(:415-417「无投影奇点（δ 线性）」)；::zenithal θ 用法(:369-370 θ₀=CRVAL2/:413 θ=acos(ρ) 极距) vs AIT θ 用法(:418-423 cosθ·cos(φ/2) 纬度)；AIT 退化点旧字面(φ=0/2,λ=0)随 §15 重写已不命中
- 复验证据：子事实复核：①「CAR 无投影奇点」字面逐字在位 :417，极点整行塌缩仍无排除条款（新增的只是 |θ|>90° fail-closed :416，对 |δ|=90 塌缩本身不作声明）；②θ 在两族投影承担两个互斥几何量（zenithal 极距 vs AIT 纬度）原样；③AIT 退化点错误字面随 §15.3 现文重写已消失（grep「退化点/0/2」零命中）——该子项 FIXED。主判据（奇点/有效域两向不闭合）仍在位 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 39. M7-G-103 — STILL
- 新锚：docs/standards/SCIENTIFIC_CONSTANTS.md（仍不存在）；docs/science/NOISE_MODEL.md+algorithms（1.482602218505602/1.4826022185/1.4826/1.4826f/1-0.6745 五写并存）；docs/algorithms/PHASE2_SAMPLER.md::常数权威块(:186-193)
- 复验证据：复算：四元表文件未新建、全仓 grep 值×因次×适用域×推导源 零命中 ⇒「缺表」面事实原样；1.4826 族四种口径+代码 1.4826f 仍并存在位（uniq -c 复算）；PHASE2_SAMPLER 常数块 300″/600″ 档仍未声明 scale 因次与 clamp/插值因次（唯一变化是插值描述随 M4-C-01 更新为「两段分段线性（非均匀网格）」，反而给了实现锚）。无任何 SCI/ALG 裸常数→表的机器门。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 40. M8-B-001 — STILL
- 新锚：lib/core/src/artifact.cpp::Provenance::science_hash(:62 mix(created_utc)/:63 mix(platform))；docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md::§2(:85)；runtime/artifact_store/provenance.py::provenance_digest_hex 文档串(:354)；tests/artifact/test_provenance.py::TestDeterministicDigest::test_run_facts_do_not_change_digest(:354)；tests/unit/core_artifact_test.cpp::test_science_hash_stable(:65，两对象同由 make_descriptor:18 构造)
- 复验证据：grep 复算：C++ 侧 :62-63 两行 mix 原文在位（违宪侧未动）；合同「运行事实不参与」条款 :85 在位；Python 合规侧 docstring 与其测试均在位；C++ 唯一稳定测试仍用同工厂构造两对象（零鉴别力）。双实现互斥+测试无鉴别力两机制原样。注：本条不在变更清单文件外——artifact.cpp/tests/unit 该文件未改。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 41. M8-F-006 — STILL
- 新锚：ci/checks.json::UT-*.command（18 个 discover -s，lib 面仅 lib/gaia_xpsd_client/tests）；lib/photometric_calib/cpp/test/test_photometric_calib.py::test_sip_wcs(:269 return t4_ok)；lib/photometric_calib/cpp/test/test_spectrum_integrator_golden.py(:302/:320/:345)；tools/quality/ci_coverage_runner.py::--tests default=tests
- 复验证据：python3 解析 checks.json 复算：discover -s 根 18 个、含 lib 面仅 gaia_xpsd_client/tests，lib/photometric_calib 与 tests/realdata 仍不被采（连 DEEP-COV-PY 根 tests 也不含）；return 布尔形态测试 :269 在位（pytest 只 warning 不红）；golden tolerance=0.01 无 SCI 出处、exe 未找到 return False 静默降级原样。三重缺陷全在。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 42. M8-G-001 — STILL
- 新锚：tools/quality/check_ctest_registration.py::collect_real(:202)/::ADD_TEST_NAME_RE(:65/:105 起点=add_test)；ci/validate_registry.py::R11(:89-138，discover 0 用例判红——方向②已于 adaeb531 窗口内修复)；docs/standards/checks/check_standards_registry.py:::209 _clauses 丢弃/:248 path_exists；ci/checks.json STD-REGISTRY 命中 0；ci/known_failures.json 现 1 条
- 复验证据：方向①：collect_real 仍从 add_test 出发、无反向枚举磁盘 *_test.cpp 规则（建议②未落地）⇒ 在；方向②：R11 已在位且真计数（「discover 目录采集 0 用例(门空转)」:138，adaeb531「假门转真门」引入，经 ci/tests/test_negative_guards.py 消费）⇒ 此向已闭；方向③：check_standards_registry 仍不注册（STD-REGISTRY 零命中）、C3 仍只验 path_exists、_clauses 仍丢弃 ⇒ 在。元结论「无门能证伪 VERIFIED/未执行证据」因 ①③ 空集仍在 ⇒ STILL，注明部分修复（②补上，本条三向之一）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 43. M8a-C-005 — STILL
- 新锚：lib/**/module.yaml(21) vs lib/core/src/module_adapters.cpp::d.module_id(22)；lib/snr_estimator/README.md:::1；lib/snr_estimator/module.yaml:::32；module_adapters.cpp::p1_noise_snr_descriptor(:799，原 :702 漂移)
- 复验证据：python3 值集合精确比对复算：manifest 21 值、descriptor 22 值、精确交集 = []（0/22，原报 0/21 口径一致且更强）；snr 模块三名仍各表（README astrocs.p1.noise-snr / yaml astrocs.p1.noise / descriptor astrocs.phase1.noise-snr）。命名体系整体分裂原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 44. M8a-G-003 — STILL
- 新锚：tools/quality/gen_source_index_v61.py::VENDORED_PARTS(:35)/::is_third_party(:95)；tools/gen_v19_evidence.py::is_excluded(:57)；docs/algorithms/anchors/check_doc_line_anchors.py::ARCHIVE_MARKERS(:45/:106)；tools/arch/check_thread_budget.py(:81)；等九处
- 复验证据：逐站 grep：全部判据仍为目录名字面（"third_party"/"archive"/"legacy" 字符串集合），无一改为内容/许可证驱动；派生代码真实落点（lib/plate_solve/cpp/ipv/src、nanoflann.hpp、healpix_core.cpp 头注）不在任何判据的命中面内——登记面与真实闭包互不覆盖机制原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 45. M8a-I-003 — STILL
- 新锚：tools/README.md::全文(:1-3 astro_toolkit 自述/:24/:63 git_push 步/:75 一键 add→commit→push)
- 复验证据：复读：README 仍只述 astro_toolkit、抬头「减少子 Agent 频繁触发沙箱确认」逐字在位；实体面反增：tools 文件 160→190、check/verify/audit 63→71，索引依旧为零（子目录 README 仍仅 tools/README.md 与 realdata 两份）；git_push 步骤与「SubAgent 不直接 commit」纪律冲突面未加边界声明。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 46. M9-B-2 — STILL
- 新锚：lib/healpix_db/healpix_drizzle/fits_reader.cpp::CD 构造块(:359-368 含独立 DEG2RAD 字面 :362)；lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::hp_drizzle_run 同型块(:432-441 含独立 DEG2RAD :435，行号自原 :564-571 漂移——文件重排)；口径面 docs/contracts/DATA_SEMANTICS.md:::246、docs/algorithms/DRIZZLE_GEOMETRY.md:::116
- 复验证据：grep+复读：两 TU 六行构造逐字重复仍在（cd[1]=-cdelt2·sinr/cd[2]=+cdelt1·sinr 两份，DEG2RAD 字面量两份）；tests/unit 与 p1drz 测试面 grep crota 零命中 ⇒ CROTA2≠0 对照测试仍无；DATA_SEMANTICS:246 仅描述机制、未固定旋转方向口径、D.fits 域无登记。原报撤回的「符号相反升 P0」钩子维持作废（本复验不重翻）。残余三事实原样 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 47. M9-G-1 — STILL
- 新锚：lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file(:1194 memset 首行清零/:1219 魔数/长度校验裸 return -1 无回收，正对照 :1216 MAP_FAILED 分支就地 close)；::gaia_client_create_ex 循环(:1978/:1998 仅 ==0 分支处理，-1 无补偿)
- 复验证据：复读：P19-gaia 改动未触碰该路径——:1219 校验失败仍不 munmap/不 close(fd)（mmap 已建立后第一个校验点）；调用方两分支（Win :1978、POSIX :1998）只在返回 0 时接管，返回 -1 时下次同下标被 :1194 memset 抹掉句柄 ⇒ 每损坏文件泄漏 1 映射+1 fd 机制原样；无损坏状态码区分。行锚自 :1009/:1034 漂移 +185。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 48. M9-H-3 — STILL
- 新锚：lib/gaia_xpsd_client/src/gaia_client.c::read_leaf_block(:1115 自报偏移寻址/:1121 非压缩 memcpy/:1131 zlib uncompress)；::find_max_block_size(:1086-1093→scratch 尺寸同源)；消费点 :1597 等四条查询路径
- 复验证据：复读函数体：全函数无 data_position+block_offset(+block_size/compressed_size) ≤ mmap_size 任何校验；非压缩分支 memcpy(scratch, comp, block_size) 源侧无界、zlib/lz4 把映射外指针交给解压函数；scratch 长度由自报 max_block_size 决定（同源）⇒ 双侧皆不可信；唯一校验 dest_len==block_size 为事后。新增的 B13-R13-5 注释是 shuffle OOM 处理，非区间校验 ⇒ 越界读机制原样。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 49. V1-N-06 — STILL
- 新锚：lib/snr_estimator/cpp/src/snr_estimator.cpp::snr_extract_model(:574-578 仅初始化五旧字段；早退 :586/:594/无有效星支)；三字段赋值迟至 :640-642；对照 include/snr_estimator.h:432/:450「无 ZP 时 NaN」承诺；N-06b: lib/phase2/tools/stage2.cpp::weight_mode==2 两站 final-else(:1120-1121/:1392-1393)
- 复验证据：复读：v1 三条早退路径上 median_source_snr/frame_depth_flux5_adu/frame_depth_m5_mag 仍保留调用方栈值（初始化块只五字段，赋值在 :640+）；v2/v3 memset(0) 与头承诺 NaN 及 SnrSourceResult 用 NaN 的三套哨兵不一原样。N-06b：tile 读失败支现已 ++ivar_tile_fallback_px（:1118/:1389，M4-C-03 整改），但「帧根本没有 ivar 产品」final-else 仍 support 冒充权重且不加计数（:1120-1121/:1392-1393）⇒ 两半事实均在 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 50. V10-N-06 — STILL
- 新锚：lib/healpix_db/healpix_drizzle/fits_reader.cpp::readFits(:315 仅判 <=0/:333-340 naxis3 归一无上限/:419-420 n_pixels=width·height·naxis3 直乘 data_size)；正对照 lib/astro_image_io/src/aio_fits.cpp::P0-4 上限(:171-176)/aio_xisf.cpp::XISF_MAX_DIM
- 复验证据：复读：第三个读面仍无 NAXISn 上限（同族两读面 65535 硬拒未跟进，aio_fits:176 在位而 fits_reader 零命中 65535）；声明尺寸直乘分配量原样；drizzle 侧无 65535 类锁测试；是否必崩属运行期的限定保留（bad_alloc/占坑为静态事实）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 51. V11-N-05 — STILL
- 新锚：include/astrocs/abi/lifecycle_v1.h::六判定函数声明(:158/:163/:169/:177/:194/:234)；tests/abi/abi002_lifecycle_probe.c::唯一定义；tests/abi/test_abi002_lifecycle.py::compile_probe(:67)/只编译注释(:94)
- 复验证据：三级复核：①六函数名在 lib/runtime/cli/modules/providers（含 .c/.cpp）逐名 grep 全部零命中；②全仓唯一定义仍在测试探针副本；③compile_probe 仍只编单 TU 不链生产对象、:94「避免未定义引用」自证在位 ⇒ 生产零实现+门自证结构原样。本窗新增 ipv ABI failclosed 测试属另一 ABI 域，未接线本头六函数。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 52. V12-N-03 — STILL
- 新锚：docs/science/NOISE_MODEL.md::§附注2(:135)；lib/photometric_calib/cpp/src/star_matcher.cpp::_MAD_SCALE(:21/:545)；lib/photometric_calib/tests/p1phot/p1phot_oracle.hpp::MAD_SCALE(:68，:19 冻结注)
- 复验证据：复读：:135 仍写「1.482602218505602=1/Φ⁻¹(3/4)…与 SCI-PHOT 的 0.6745 同源」（Φ⁻¹(3/4) 真值 0.6744897502，1/0.6745=1.4825796887，对冻结值 rel −1.52e-05，比 1.4826 一系偏差大 10.2 倍——倒数式「同源」陈述仍为假）；实装 _MAD_SCALE=0.6745 在位（:21，:545 mad/0.6745 产 sigma_residual 进 photometric_calib.h 交付面）；p1phot_oracle :68 用同一 0.6745 ⇒ 共源钉死失效仍在。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 53. V12-N-11 — STILL
- 新锚：lib/gaia_xpsd_client/src/gaia_client.c::MAX_STARS_RESULT(:32，原 :31 因 P19-gaia 下移 1)/::QUERY_CACHE_MAX_BYTES(:142-143)；lib/plate_solve/cpp/ipv/include/ipv_types.h::m_lim_gaia_cap_per_file(:260，原 :244)；lib/gaia_xpsd_client/src/module_entry.c::kQueryCacheCap(:546)；锁缺失面 tests/unit/gaia_cat_test.c(:907)
- 复验证据：复读：一具名（.c 私有宏）+三独立复制原样——200000 字面在 gaia_client.c:32/ipv_types.h:260/module_entry.c:546 三处各一份；「本模块只读该常量」注释与抄值实况仍不符；缓存乘积仍两 TU 各算（QUERY_CACHE_MAX_BYTES 随宏、kQueryCacheCap 裸算）⇒ MAX_STARS_RESULT 改动致 33% 高估+fmod 触顶检测失效的推导仍成立；tests 无 307200000/QUERY_CACHE_MAX_BYTES 锁（grep 零命中）、test_mag_iter 仍 stub 抄值。P19-gaia 与 ipv ABI 改动未收敛该常数。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 54. V13-N-02 — STILL
- 新锚：docs/traceability/TRACEABILITY_MATRIX.json::P1-CAL 行(:106)/P1-NOISE 行(:210)/photometry 同形行；判定器 tools/traceability/check_traceability_matrix.py::C7 _check_anchor(只认 path 或 path::symbol)
- 复验证据：复读：三行 test_path 仍为分号散文串（"lib/calibration/tests/p1cal (ctest p1cal_units/...); tests/backend/test_calibration_oracle.py; tests/unit/CMakeLists.txt:441 master_flat_median + ..."，含行号注记与中文括注），任何一段都不是合法 path::symbol ⇒ DANGLING_REF 事实链原样；regression_test 用不读 MATRIX 列的 checker 充当依据的循环性未改。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 55. V14-N-05 — STILL
- 新锚：cmake/install_layout.cmake::if(TARGET) 门卫族(:65/:71/:77…，:21 注 noise 摘出)；根 CMakeLists.txt:::210-214/:239(F-CI-002-01 注)；packaging/astrocs.product.json::note(:6)；tests/abi/mod001_install_load_check.py::S4(:255 len(munits)==10)/S7(:331-333 ==10)；packaging/install-tree.contract.json(无 MOD-P1-NOISE 行)
- 复验证据：双向机制原样：①「收编即装」——安装规则仍全部 if(TARGET) 条件式，两份白名单仍无 MOD-P1-NOISE 行，工作树未跟踪 lib/snr_estimator/CMakeLists.txt(SHARED) 使门卫「即将为真」的时点判断不变（HEAD 未入库⇒HEAD 时点无实际多出单元，属未爆状态而非修复）；②反向假红——S4/S7 双处仍把 units 字面钉 ==10（且注释自述「F-CI-002-01: noise 摘出」后仍 10），补登记即先红，惩罚合规的机制在位。建议①②③均未落地。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 56. V18-N-09 — STILL
- 新锚：lib/dynamic_psf/tests/p1psf/p1psf_tests_core.cpp::注入名变量(:77 F_REC="recovery"/:78 F_ABI=…，:986 同名局部量第二定义)；消费站点(:103/:113/:130 …)；perf/selfcheck 面同族
- 复验证据：复读：注入名仍全部经 const char* 变量传递、检查宏调用点只写 F_REC/F_ABI 符号（:103-131 抽样命中即证）；:77 与 :986 双定义仍在；ACS_FAULT 宏（建议改法）零命中；按字面量 grep "recovery" 仅命中两处定义行本身、站点零命中 ⇒ 「字面量检索=不存在、注释检索=存在」双口径并存原样，C-21① 前置件仍缺。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 57. V2-N-08 — FIXED
- 新锚：lib/core/src/module_adapters.cpp::p1_op_noise(:2629 起，P14-N-08 snr.max_sources 唯一开关+truncated)；lib/phase1/noise/README.md::交付样本定义(:49-56)与 provenance 表(:58-68)；回归锁 tests/unit/p1snr/p1snr_frame_parity_test.cpp（注册 tests/unit/CMakeLists.txt:1330）
- 复验证据：修在哪：①交付样本改回「sources 全部测光有效源（flux>0 且 fwhm_px>0），与 psf.max_stars 完全解耦」（README P14-N-08 2026-09-15 修订注记旧缺陷并改判口径；p1_op_noise 读上游 p1_sources.json，:1776 注释「交付 SNR/深度不读该子集」）；②provenance 六字段如实记录（snr_sample/n_sources/n_snr_available/truncated/snr_max_sources/psf_mode 真实模式/n_fit_input/psf_fit_truncated）；③parity 锁三段式：max_stars=0/5000/2 三次运行交付逐位一致(A)+真实性字段(B)+用 snr.max_sources 人为截断证明锁非恒真(C)，测试注释自证「改前 RED 改后 GREEN」。修完全性：建议①②③全落地；psf_mode 恒字面量的姊妹面（V2-N-09）同批改真实模式。kPrecisePsfEnabled=false(:1817) 仍在，但那是精确路径开关本身、非本条机制。⇒ FIXED。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=FIXED

### 58. V6-N-01 — STILL
- 新锚：docs/TRACEABILITY.csv::全表(63 数据行，PSF/REJ 关键词零命中；SCI-PSF-001/SCI-REJ-001/SCI-INT-001/SCI-ACR-EQUIV-001 四行在 HEAD 与现工作树均不存在)；判据 tools/quality/contracts/check_traceability.py::core_keywords(:48/:60)；注册 ci/checks.json::CON-TRACEABILITY(三 profile, waivable=False)
- 复验证据：静态复算：HEAD CSV grep 四被删 ID = 0；PSF/REJ 两关键词在 63 个 requirement_id 中零命中 ⇒ checker 按 :60 必产 TRACE-CORE-MISSING(PSF/REJ) 两条 P1 finding ⇒ 门仍判红（未运行，按判据静态推演）；「以删行处理不可追溯项」的动作未被回滚、端点未恢复（建议处置的改锚恢复未做，CSV 仍无任何指向 p1psf/p1star 的 PSF 行）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 59. V7-N-04 — STILL
- 新锚：lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::WCS 解析段(:405-409 读 CD、:428 has_cd=(cd11!=0||cd22!=0)、:433-441 CDELT·CROTA2 替换支)
- 复验证据：复读：has_cd 仍只看对角两元 ⇒ 合法轴交换/纯旋转 CD（cd11=cd22=0、cd12/21≠0）仍被判无 CD：有 CDELT+CROTA2 时静默用构造矩阵替换真 CD（:433-441 在位），无 CDELT 时走 -9「缺少 WCS」错误文案；CD 值读取无 isfinite 门（:320/:955 的 isfinite 只护 photscal）；drizzle 测试面无「对角双零」正例（p1wcs 的 :336 属他模块断言其自有行为）。三机制原样 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 60. V8-N-02 — STILL
- 新锚：lib/phase1/noise/snr_frame_science.cpp:::3 宣称注释/::median_of(:30，用于 :142/:166/:170)/::2.5*dex→mag(:86)；lib/phase1/noise/README.md:::27
- 复验证据：复读：宣称两句逐字在位；snr_moffat4_profile_f64 仍只出现在 :6 注释清单自身、本 TU 零调用；本地 median_of/2.5·dex→mag/local_snr 比值副本仍在 :30/:86/:142-170。README 本轮为 V2-N-08 改过 :49-68 但 :27 的「零公式副本」未订正。宣称与实况不符原样 ⇒ STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 61. V9-N-04 — STILL
- 新锚：docs/contracts/API_CONTRACTS.csv::API-estimate_mag_lim_by_density(:331，行锚自原 :371 漂移-40)；lib/plate_solve/cpp/ipv/include/ipv_select.h:::69 仅剩注释「替换原 estimate_mag_lim_by_density」
- 复验证据：grep：CSV 行仍在且 status 列仍 VERIFIED、声明签名仍写在该头；ipv_select.h 本轮被改（P14-N-10/ABI 锁）但只加了 mag_iter_apply_to_selection 声明，未删/未 RETIRED 该合同行 ⇒ API-MISSING-AST 判据事实链原样（死符号占 VERIFIED、门真红条件在位）。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

### 62. V9-N-13 — STILL
- 新锚：ci/checks.json::135 门中 104 门 changed_paths 不含自身 checker 脚本、131 门不含 ci/checks.json；例 CON-API-CONTRACTS paths=[docs,lib,cli,tests,contracts]/**不含 tools/quality；口径限定：.github/workflows/ci-linux.yml:141 仅 --profile
- 复验证据：python3 全量复算：基数由 130→135（本窗内新增门），miss_self 99→104、缺 checks.json 124→131——比例恶化 ⇒ 机制未修反随门数增长；CI 实跑仍 --profile 全量（V9 自限照收，P2 定档不升）。STILL。
- 建议标记（账本回写由前台统一执行）：RECHECK-VERDICT=STILL

## 三、备注

- 含部分修复史/多子事实的条目（M1a-C-006、M2a-C-9、M3b-F-02、M7-A-119、M7-A-137、M8-G-001、V1-N-06、V6-N-01 族、V14-N-05）按「所报缺陷机制是否消失」判 STILL，并在证据行逐点注明已修子事实与残站位置。
- FIXED 四条（M2a-H-1、M4-C-01、M5a-G-001、V2-N-08）均有仓内点名标记（代码注释含条目 ID 或 RQS 编号）与专用回归测试佐证；M5a-G-001 附注释残余一处（cli/commands.cpp:939 段注释仍写旧数 0.75/0.50，判定代码已改用常量），不改变 FIXED。
- 行锚漂移集中来源：lib/core/src/module_adapters.cpp（drizzle/p3 段 +约950 行）、lib/gaia_xpsd_client/src/gaia_client.c（P19-gaia +187）、lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp（重排）、docs/contracts/API_CONTRACTS.csv（-40）、lib/snr_estimator/cpp/src/noise_model.cpp（+10）。
- 涉及工作树未跟踪件的条目（M3-I-004 孤立 p1phot 副本、V14-N-05 在途 lib/snr_estimator/CMakeLists.txt、M3-A-005 附注新树）：判定以 HEAD 为准，未跟踪件存在/增长作为附加事实记录；M3-I-004 之载体本就属未跟踪文件，其「对机器门不可见」恰为该条卫生问题的实证。
- 本分片无 MOVED 判定：所有行号漂移均在原文件原地漂（锚改 文件::符号），未发生「位置变了但缺陷原样」的跨文件迁移实例。
- 无 CANNOT_STATIC 判定：62 条均可静态定态（个别条目内已标注需运行期终判的子事实，如 M3b-C-01 偏差本体、M1a-H-001 高并发可达性，但条目主机制静态可证）。
