# RC8 分片重验档案（第 8 片 / 共 8 片）

- 时点：HEAD a3a343a44080d917089e1f8d548ed2d0400b0c61（git --no-optional-locks rev-parse HEAD 实测一致）；base 521095b8。
- 子集来源：问题扫描/_cache/recheck_round1.json -> verify[7::8]（按下标取模第 8 组），共 61 条，全部命中 问题扫描/账本/FIX_LEDGER.jsonl。
- 工作区注意：docs/TRACEABILITY.csv、reports/v19r2/*、reports/v19r3/contract_inventory.csv、evidence/v6_1_rework/tasks/CHK-001/* 在工作树为脏 -> 涉及条目以 git show HEAD:<path> 为准；lib/snr_estimator/{src,include,CMakeLists.txt}、lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py、lib/phase1/tests/、tests/unit/p1_noise/ 为未跟踪新文件，HEAD 上不存在，不作为 FIXED 依据。
- 判定口径：四态 STILL / FIXED / MOVED / CANNOT_STATIC；一律按 文件::符号 重定位；FIXED 须给出修在哪的证据并判是否只修一站。
- 建档事故记录：生成中途一次字符串替换的特殊替换符污染正文并造成个别区块损坏，已全量重建；判定结论未受影响。工作数据：RC8_slice.json / RC8_bodies.json / RC8_bodies_extra.json（收工后可清理）。

## 逐条结论

### [0] L28c-E-001（P1/E_TRACE_BREAK）
- 原报标题：runtime/pipeline/typed_dag_contract.h 以「编译期合同 + ABI 冒烟测试锁定」自我背书，但该头全仓零消费者、不在任何构建/测试源列表，被指派的锁不存在
判定：**STILL**（锚点无漂移）
- 复核（git show HEAD:runtime/pipeline/typed_dag_contract.h）：:1 仍写「冻结头，编译期合同」；:62 仍写「冻结字段顺序不得重排（ABI 冒烟测试锁定 layout 断言）」；:48 仍 "DATA-xxx" 占位、:52 仍 "[H,W]" 示例；尾部仍「本任务只冻结合同…RT-002 接入」。
- git grep -l 'TypedDagNode|TypedPort' HEAD → 唯一命中该头自身；git grep 'typed_dag_contract' HEAD -- '*.h' '*.cpp' 'CMakeLists.txt' 'tests' 'ci' 'tools' → **0 命中**（零编译消费者、零构建/测试源登记）。该域现存消费者全走 Python 面（tests/runtime/test_typed_dag_negative.py 等消费 typed_dag.py），与被指派为锁的「ABI 冒烟测试」无关；仓内不存在任何 layout 断言测试。
- 新锚：runtime/pipeline/typed_dag_contract.h:1、:48、:52、:61-62、:119-120（同原报，无漂移）。

### [1] M1a-A-004（P1/A_SCI_DEF）
- 原报标题：cd_inv 在 SCI 符号表被定义为 CD⁻¹ 却注单位 pixel/arcsec，与同文 §5 及实现 inv(trans.linear) 相差 3600 倍
判定：**STILL**（SCI 互斥原样；表内代码锚轻微漂移）
- docs/science/ASTROMETRY.md（该文件 base..HEAD 未变）:19 符号表仍「cd_inv | CD^{-1} 线性逆 pixel/arcsec」；:29 单位表仍 cd_inv: pixel/arcsec；:52-53 前向式仍注 cd_inv = inv(trans 线性项)；:138 §10 禁止项仍只禁「误写为 deg/pixel」。CD=trans.linear/3600 ⇒ CD⁻¹ 与 inv(trans.linear) 相差 3600×，同文互斥未修。
- 实现侧：lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip :336-338 仍注「用 trans 线性项的逆, 不是 result->cd 的逆 (差 3600 倍)」，:353-357 按 inv_det_lin·trans 计算 ⇒ 与 §2 的 CD⁻¹ 标注互斥。PLATESOLVE.md::§2 F2 未变。
- 新锚：ASTROMETRY.md:19/:29/:52-53/:138 ↔ ipv_wcs.cpp:336-338、:353-357（原报表内锚 ipv_wcs.cpp:328-331 → 现 :353-357）。

### [2] M1a-C-002（P0/C_DOC_CODE_GAP）
- 原报标题：trans 线性奇异：SCI 承诺"返回参数错误"，实现仅 warn 并恒设 success=true，可回写冒称 TAN-SIP 的退化头
判定：**FIXED**（报告站已修且带专锁；同类他站为已登记偏差，不改本判）
- 修在哪①（机制复算）：lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip :340-351 —— 原「仅 logger->warn 且恒 success=true」的 det_lin<1e-15 分支现为 result->success=false + snprintf(result->error) + logger->error + return，注释直引 SCI-WCS-001 §8 / ALG-WCS-001 §11.1 / 宪章 §6.3/§14.4。落码提交：c3452d48「fix(RQS/B4)…SIP 奇异失败码」。
- 修在哪②（失败可观测/不回写）：IpvWcsResult 现有 success+error 面（ipv_types.h:128-131「RESCUE-FD-05…success=false 时非空」）；lib/orchestrator/cpp/src/orchestrator.cpp::run_stage_platesolve :2042「if (ret != 1 || wcs_result.success != 1)」→ error_msg + exit_code=PLATESOLVE_FAILED + return false，CRVAL/CD 头写回全部在该门之后 ⇒「冒称 TAN-SIP 回写退化头」路径闭死。
- 修在哪③（专锁）：新增共址回归测试 lib/plate_solve/cpp/ipv/test/test_extract_wcs_sip_failclosed.cpp（HEAD 存在；头注 :6 自述「被验条目: M1a-C-002」；RED/GREEN 判据写明且含非奇异正对照防恒真），同目录 CMakeLists.txt :19-26 注册 add_test(NAME ipv_extract_wcs_sip_failclosed)。旧错位锁 tests/unit/p1wcs/p1wcs_tests_negative.cpp::child_collinear 现断言「if (r.success) return 1」（:152-159）。
- 同类他站：AP/BP 网格拟合奇异仍仅 warn（ipv_wcs.cpp:520、:598）——不在本条报告站，且已在 docs/algorithms/PLATESOLVE.md §11.3 DISP-WCS-004「登记不改码」在册 ⇒ 本条不因他站改判；提醒：DISP-WCS-001 已闭环而 DISP-WCS-004 同机制仍开放。

### [3] M1a-D-002（P2/D_COMMENT）
- 原报标题：科学链路单位注释自矛盾群（重锚定稿）：StarPoint/U 注"角秒"而实际为像素、SIP order 值域注释过期、x00 日志单位名实不符
判定：**STILL**（三站点注释原样，锚点无漂移）
- ipv_types.h 在 base..HEAD 有改，但 diff 显示改动在 StarSelection 收敛位（P14-N-10 m_lim_converged/query_failed）与本条站点无关；:26-29 StarPoint 仍注「角秒坐标/X坐标(角秒)/Y坐标(角秒)」，而 docs/algorithms/PLATESOLVE.md:115 明写 U=(det_x−cx,−(det_y−cy)) 为**像素** ⇒ 注释与实际域仍互斥（:82-83 U/W 同样注「角秒坐标」）。
- :68-69 SIPCoeffs::order/ap_order 仍注「(0=无 SIP, 2/3/4)」，实现按 trans_for_sip.order 直传、AP 布局上限 5（ipv_wcs.cpp:455+ 段、PLATESOLVE.md:124）⇒ 值域注释仍过期。
- ipv_wcs.cpp:331 日志仍「[V4.22] 清零 trans.x00/y00 (saved: x00=%.4f…)」，单位名实不符原样。
- 新锚：ipv_types.h::StarPoint :26-29、::SIPCoeffs :68-69、::StarResult U/W :82-83、PLATESOLVE.md:115、ipv_wcs.cpp::extract_wcs_sip :331。

### [4] M1a-F-005（P1/F_TEST_GAP）
- 原报标题：并发修复面缺回归保护（新增条目：改过但没人守住）
判定：**STILL**（三处修复仍无人守住；行锚已订正）
- ①节点面：module_adapters.cpp::p3n_input_manifest_hash 现 :5525，fail-closed 判定块现 :6005-6031（原报 :5065-5081 漂移 +460 行）。tests/ 复核：p2001_real_nodes_test.cpp:388 只测 Phase2 侧 input_manifest_hash 长度；Phase3 侧 p3002_real_nodes_test.cpp :230-238 仅断言夹具 run_context.json 的 schema 非空，**无「删 run_context ⇒ 节点必败」负断言、无「prov.manifest_hash==sha256(输入 HiPS 字节)」值断言** ⇒ 修复仍可被无声回退。
- ②IPV 链改接：adapter 现 :2363 调 ipv_solve_from_memory_with_callback_d（原 :2011）。base..HEAD 新增 tests/unit/p1001_real_nodes_test.cpp（+323 行）有实质缓解——断言缺参拒绝/P9 header_crval 拒绝/header_pointing 无关键字 fail-closed——但 Linux 分支断言的是「stub 必败」、Windows 分支只断言 rc.ok()，**无冻结 CRVAL/CD/roundtrip 数值锚** ⇒ 本条要求的端到端数值回归仍缺（记为部分缓解）。
- ③选星口径：git grep img_n_target/n_target HEAD -- tests → 相关域 0 命中（唯一命中是 known_failures_baseline 测试的 fake 名）；ipv_select.cpp 在 base..HEAD 大改但改的是极限星等迭代落面（P14-N-10），选星 N/排序键仍无断言 ⇒ 未钉。
- 另记：工作树有未跟踪 lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py（P27「配置不生效」机器锁，登记 triangle_match(60,60,0.002) 字面量硬覆盖等死参数清单）——HEAD 不存在，不计入固化；若入库可部分覆盖③，建议随提交登记 CI。
- 建议标记：维持 OPEN；锚更新为 module_adapters.cpp::p3n_input_manifest_hash :5525 / fail-closed :6005-6031 / adapter :2363。

### [5] M2a-A-1（P0/A_SCI_DEF）
- 原报标题：SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pixfrac²
判定：**STILL**（SCI 互斥与实现偏置原样；引擎改动未触该机制）
- docs/science/DRIZZLE.md 不在 changed/脏列表：:48-51 累加式仍 w_jp=a_jp/A_drop、S_p=F_p/D_p；:82-84 §7 仍以「S_p=C/A_drop≠C 是正确的面亮度语义」自我背书；§11 无 pixfrac 修正项。
- 实现：drizzle_engine.cpp::processPixelSharedTiled（base..HEAD 有 +364 行改动，但 grep 全文 diff 无 pixfrac 相关行）HEAD :1531 仍 `Scalar weight = overlap_area / drop_area`、:1554 sumArea+=overlap_area ⇒ M2a 复算 S_p=B0/pixfrac² 的机制未变；spherical_overlap.cpp::compute_drop_corners 仍 half=0.5·pixfrac；aio_hips_writer.cpp::write_tile_core sig=flux/area 段未变；默认 pixfrac 0.8（json_config.h/stage1.template.json）未变。
- 新锚：DRIZZLE.md:48-51/:82-84 ↔ drizzle_engine.cpp:1531/:1554（行号随并发修复漂移，机制原样）。

### [6] M2a-C-10（P1/C_DOC_CODE_GAP）
- 原报标题：registry 登记的 `astrocs.calibrated_frame.v1` 不满足自家 type_id 词法，该类型一经使用必被词法拒绝（登记即合法 vs 未登记即拒 互斥）
判定：**STILL**（词法互斥原样，锚点无漂移）
- contracts/data/artifact_types.registry.json:34 仍 "type_id": "astrocs.calibrated_frame.v1"（三段），而 contracts/data/artifact_manifest.schema.json:43 的 pattern 仍是四段式 astrocs.字母数字段.下划线段.v数字（第二段不允许下划线、且要求第三段独立）⇒ 登记行本身不满足自家词法。
- runtime/artifact_store/artifact_manifest_validator.py:40 _TYPE_ID_RE 与 phase_product_exchange_validator.py:54 _TYPE_ID_RE 同字面未变；两者 :133/:129 均先做正则拒绝 ⇒ 「registry 登记即合法」与「使用即被词法拒绝」互斥原样（registry:7 note「未登记」拒绝口径亦在）。相关文件均不在 changed 列表。
- 新锚：同原报。

### [7] M2a-C-5（P1/C_DOC_CODE_GAP）
- 原报标题：Gaia 模块 README 与 ALG-GAIA-001 的现状陈述整体滞后于迁移落地（无测试/无 target/无 plan-cancel-lease 三条"缺口"均已不成立）
判定：**STILL**（滞后陈述原样，且「不成立」的反证在 base..HEAD 还在增强）
- lib/gaia_xpsd_client/README.md（未改）:13 仍写「迁移目标 astrocs_catalog_gaia.dll（CAT-GAIA-IMPL 建立，尚未存在）」，而 lib/gaia_xpsd_client/CMakeLists.txt:15 实有 `add_library(astrocs_catalog_gaia SHARED …)`；:130 仍写「当前无可执行测试、无 PASS 声明」，而 tests/unit/CMakeLists.txt:129 add_test(NAME gaia_unshuffle …) 在册、base..HEAD 又新增 gaia_magnitude_range_bounds_test.c；:137 仍写「无 CMake target」。
- docs/algorithms/GAIA_QUERY.md :183/:186/:200 仍写 adapter/取消检查点/host ThreadLease 缺口，而 gaia_client.c 实有 gaia_set_worker_lease :97、gaia_set_cancel_checkpoint :99、gaia_client_collect_plan_stats :285。module.yaml:19 module_status: CONTRACT_READY 未动。
- 新锚：README.md:13/:130/:137；GAIA_QUERY.md:183-200；对照 gaia_client.c:97/:99/:285、CMakeLists.txt:15、tests/unit/CMakeLists.txt:121-129。

### [8] M2a-D-4（P2/D_COMMENT）
- 原报标题：Gaia 侧注释/断言簇：算法名与实现不符、扩展函数声明无对应实现、已修缺陷仍留过期弱断言与恒真断言
判定：**STILL**（五站全数在现文；两处行号漂移已订正）
- gaia_client.h 文件头能力注释（现 :8-13）仍写「|dec|>85° 仍保守（Lipschitz 常数 C=π/2 / C45=π/(2√2)…）」，而 gaia_client.c 里 "85" 仅命中哈希常量 :501，无 85° 分支；C45 实际用于 :969/:1005-1007 的 AE 极冠锥判断（|dec|>45 进平面剪枝 + 紧/松常数切换），与注释口径不同。
- gaia_client.h 仍写「球面角距判定（Haversine 余弦定理）」，实现 :1640/:1769/:1908/:2524 为 acos(球面余弦)，非 haversine。
- CMakeLists.txt:100 仍声明「gaia_client.c 使用 GNU libm 扩展 sincos」，源码 grep sincos = 0。
- tests/unit/gaia_cat_test.c :538/:557 仍 KI-1 弱断言 1e-9（:14-16 头注自述）；:912 恒真断言仍在：`CHECK(res[200000-1].ra == res[199999].ra)` ——200000-1==199999，同元素自比。
- module_entry.c 死数组现 :838-845（原报 778-790 漂移 +60），赋值后仅 `(void)tail_keys;` 仍无引用。

### [9] M2a-F-4（P1/F_TEST_GAP）
- 原报标题：球面 drizzle 的注册测试面不含任何 pixfrac<1 的正面数值用例，M2a-A-1 的偏差因子无人守住
判定：**STILL**（新增 124 行测试仍零 pixfrac<1 正例；锚点漂移）
- p1drz_tests_core.cpp 在 base..HEAD +124 行，但全部正例/不变量/Oracle 仍是 `make_cfg(NSIDE, 1.0, …)`（:71/:198/:254/:272/:294/:313/:330/:384/:407/:438/:481/…），负面矩阵仍只 `for (double pf : {0.0,-0.5,1.5})`（:464，测被拒值）。新增的 merge_pipeline_lock.sh / taskset_invariance.sh / thread_probe.cpp 与本不变量无关。
- ⇒ M2a-A-1 的 S_p=B0/pf² 偏差因子在注册测试面依旧无人守住；DRIZZLE.md §11 Oracle 承诺未落。
- 新锚：tests/p1drz/p1drz_tests_core.cpp::（make_cfg 站点 :71 起全域 1.0）与 :464；p1drz/CMakeLists.txt（+36 行后 add_test 清单扩充，但无 pf<1 正例门）。

### [10] M2a-I-1（P2/I_DOC_HYGIENE）
- 原报标题：SCI-DRZ-001 的符号表与验证段落进了 Gaia 极区 prune 的专用常数（C=π/2、C45=π/(2√2)），drizzle 域内无对应实现
判定：**STILL**（DRIZZLE.md 未变，跨域常数仍在 SCI 符号表内）
- docs/science/DRIZZLE.md 不在 changed/脏列表：:23 符号表行仍「C=π/2, C45=π/(2√2) | 极区 Lipschitz 常数 | 极区 prune」；:33/:100 附近仍「沿用 gaia_client.c 的同一极区保守常数」复用表述；§10 表述行未动。
- lib/healpix_db/healpix_drizzle/**（含 base..HEAD 新代码）grep C45|Lipschitz → 0 命中（新改的 engine/api 未引入该对常数，drizzle 域仍无宿主）；真实归属域仍是 gaia_client.c:969 AE_C45_FACTOR / GAIA_QUERY.md §2.5。
- 新锚：同原报。

### [11] M2b-B-04（P0/B_STD_MISMATCH）
- 原报标题：HiPS 域条款锚整体错挂；properties 必需键集用自定 5 键替代标准 9 键；STD-F4 引用不存在的 obs_bandpass
判定：**STILL**（三机制全部原样；相关文件 base..HEAD 未改）
- docs/standards/STANDARDS_REGISTRY.md:81-86 D.hips 域头仍 CLAUSES 「HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）」——条款对应关系错挂原样；:95 §4.2.1 行仍把必需键集写为自定 5 键（hips_version/order/tile_width/tile_format/frame）并判 PARTIAL；:105/:243 STD-F4 仍写 em_min/em_max/**obs_bandpass**（标准不存在的键名）。
- runtime/io/hips_core.c::hips_parse_properties 现 :201-236 仍只强制该 5 键（误拒合标/误放缺必填双向机制未变）；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md::§3.1 未动；docs/standards/checks/check_standards_registry.py C3_clauses_form/C7 判据未动。
- 新锚：STANDARDS_REGISTRY.md:81-95/:105/:243；hips_core.c:201-236。

### [12] M2b-E-01（P1/E_TRACE_BREAK）
- 原报标题：ALG/DATA/PUBLIC_API/PHASE3 的源码行号锚系统性漂移（+25 ~ +423 行、导出符号数由 9 变 12），而 DOC-LINE-ANCHORS 门只作「范围在界内 + 声明式符号绑定」两项检查故对此完全无判据
判定：**STILL**（漂移进一步扩大；导出数 9→12 不实照在）
- 复算锚漂：docs/algorithms/HIPS_WRITER.md:30 注 aio_hips_product_begin :386-422，HEAD 实际函数起于 lib/astro_image_io/src/hips/aio_hips_writer.cpp:466（复验时点又 +46 行漂移）；:57 注 signal_support_tile :424-562，实际起于 :515。PHASE2_MOSAIC_WRITE.md:457 注 aio_hips_product_begin :592（stage2.cpp 侧另锚）。
- docs/contracts/PUBLIC_API.md::API-HIPS-001 :285 仍写「导出符号（9 个，全部当前真实存在）」，而 lib/astro_image_io/include/aio_hips.h 现 AIO_HIPS_EXPORT 函数 **12 个**（product_begin/write_signal_support_tile/write_variance_tile/write_diag_tile/set_provenance/verify_product_set/write_snr_points/set_drizzle_provenance/finalize/abort/write/last_error），九清单缺 3 ⇒ 「全部当前真实存在」不实。
- 门判据未扩：docs/algorithms/anchors/check_doc_line_anchors.py C2(:229)/C3_range_in_bounds(:244「1<=start<=end<=行数」)/C4_symbol_binding(:262-267) 仍只有「范围内+声明式绑定」两项 ⇒ :386-422 在界内即绿，锚漂零拦截。anchor_contract.json exemptions/bindings 结构未变。
- 新锚：HIPS_WRITER.md:30/:57；aio_hips_writer.cpp:466/:515；PUBLIC_API.md:285；aio_hips.h:114-275。

### [13] M3-A-001（P0/A_SCI_DEF）
- 原报标题：SNR 无唯一定义：进入生产权重的「SNR」实为已退休的 PSF 拟合质量比
判定：**FIXED**（P5-SNR 科学修正，负责人授权 §1.2 流程；三站同批改，非单站）
- 修在哪：lib/snr_estimator/cpp/src/snr_estimator.cpp::sourceSnrFromPsfRow :58-80 —— SNR_F = F·sqrt(ΣP_i²)/sigma_sky（Horne 1986；sigma_sky=residual_scale/0.7316727929211932，委托 snr_science 的 snr_source_snr_f64）；三处逐源站点 :187-189/:356-358/:615-617/:739-741 全部由 `double s = (A - B) / residual_scale;` 改为 `double s = sourceSnrFromPsfRow(row);`，文件头 :4-5/:56 明写「旧退休量不再进入科学输出」。
- SCI 对齐：docs/science/NOISE_MODEL.md:98 §9a SNR 条目已按「P5-SNR 订正 2026-09-14，负责人授权」重写（消费侧 signal/√variance 仅为逐像素探测显著性，源 SNR 另定义），snr_estimator.h:375 字段注改「逐源最优提取 SNR_F [Horne 1986]」。登记面：CHANGELOG.md 头部「P5-SNR 逐源 SNR 科学修正（负责人授权，scientific_change=YES）」——与 _merge 裁决一致。
- 消费面连带：drizzle_engine.cpp:1185/:2299 snr_val=points[i].snr_psf 语义随供值改真，不再是退休质量比。
- 修完全性判定：三站同批 + SCI/ALG/头注/CHANGELOG 闭环；**残余（另条）**：HISS 二进制面语义未随动（本分片 V1-N-02 专报，不并判）。

### [14] M3-C-003（P0/C_DOC_CODE_GAP）
- 原报标题：冻结 SCI 的 `min_samples` 默认 5 与实现 64 相差 12.8 倍，ALG 径行「不改 SCI，以代码为准」= 权威层级倒置
判定：**STILL**（权威倒置原样；行锚微调）
- docs/science/NOISE_MODEL.md（§4 未随 P5-SNR 改动）:37 仍「min_samples（patch 样本数阈）默认 5」；:48 控制点定义未变。
- lib/snr_estimator/cpp/src/noise_model.cpp::snr_noise_model_v1_default_config 现 :350 `cfg->min_patch_samples = 64;`（原 :341，漂移 +9）；collect_patch_sky :90/:103 阈判、:169 钳位未变；include/snr_estimator.h::SnrNoiseModelConfig.min_patch_samples 注「默认 64」未变。
- docs/algorithms/NOISE_ESTIMATION.md:151-153 仍明写「SCI §4 "min_samples 默认 5" 为旧稿数字——**不改 SCI**，以代码为准登记」——宪章 §1.1 权威分层倒置的书面自证仍在。DATA_SEMANTICS.md（base..HEAD 有改）:407 仍「min_patch_samples≥1（默认 64）」（原报 :406 漂移 +1）——三源数字（SCI 5 / 实现与合同 64）互斥原样。
- 新锚：NOISE_MODEL.md:37 ↔ noise_model.cpp:350；NOISE_ESTIMATION.md:151-153。建议标记：维持 OPEN + 锚更新。

### [15] M3-C-011（P1/C_DOC_CODE_GAP）
- 原报标题：`sigma_residual=0` 一值三义、`fit_status` 编码冲突，且暴露一个实现完全不读取的配置字段
判定：**STILL**（三子机制均存；部分站点有语义注释缓解但未给判别位）
- ①sigma_residual 一值三义：lib/photometric_calib/cpp/src/star_matcher.cpp::cleanAndScale 现 :615 初值 0.0（原 :552，+63 漂移）；mad_in>0 才换算现 :623-625（原 :558）；|r_inliers|<2 → 保持 0（B4-2 分支有 LOG_INFO「不可估计（不是零离散度）」——**仅日志**，返回值仍是同一个 0，QA 面不可判别）；pc_api.cpp 一致集为空/退化路径 `*out_sigma_residual = 0.0` 现 :86/:99/:240/:292/:328/:480（原 :91/:104）。docs/algorithms/PHOTOMETRIC_FIT.md 重排后伪码 :32 仍「if S==0 → skip IRLS」+ :37 sigma_res 可由 inliers MAD=0 得 0——「完美 vs 退化 vs 未启动」三义照旧。
- ②fit_status 编码：snr_estimator.h:51 PhotometricCalibrationQuality.fit_status「0=ok,1=degenerate,2=invalid input」与 :82-83 PsfFitQualityRow.fit_status「2=saturated/quality flag」——同码异义仍在两结构并存（V3 注释现文已各自登记含义，跨结构冲突未消）。noise_model.cpp:281-282 sigma<=0→fit_status=2 未变。
- ③use_gain_model：include/snr_estimator.h:119 字段在、lib/snr_estimator/src/module_entry.cpp:467 仍从 config 填值、cpp/src 全域 0 读取；README:109/:115 与 module.yaml:105 登记 DISP-NOISE-003「零读取」，且 lib/snr_estimator/tests/p1noise/p1noise_tests_core.cpp:373-380 把「use_gain_model=1 不改变生产输出」钉成回归期望 ⇒ 「暴露而无效的配置字段被静默接受」不仅仍在，还被反向钉死（簇 9 形态）。
- 新锚：star_matcher.cpp:615/:623-625、pc_api.cpp:86/:99 等、noise_model.cpp:281-282、snr_estimator.h:51/:82/:119、module_entry.cpp:467、p1noise_tests_core.cpp:373-380。

### [16] M3-G-001（P1/G_GOV_GATE）
- 原报标题：产品 manifest 把 cosmetic 报为 `available`，但两通道给检测的 Dark/Bias 恒为 `nullptr` → 检测在配置层不可能生效
判定：**STILL**（两通道 nullptr 原样、manifest 仍 available；偏差已登记但不改判）
- lib/phase1_session/p1_session.cpp::cosmetic 阶段 :403 仍 `nullptr, nullptr, fixed.data()`；manifest availability :446-452 仍 {"cosmetic","available"} 且 status="partial"。
- lib/core/src/module_adapters.cpp::p1_op_cosmetic（现 :1519 起）:1552 仍 `ac_correct_frame(im.px(), im.w(), im.h(), nullptr, nullptr, …)` ⇒ IR 通道检测同样不可能生效（原报 :1401-1403 → 现 :1552，漂移 +149）。
- 登记面：docs/algorithms/COSMETIC_ALGORITHMS.md:288 DISP-COS-009 仍记「p1_session.cpp:296-297 传 nullptr → 检测全禁用、模块空转」（表内锚也漂移，现 :403）；lib/cosmetic/module.yaml:31-33 说明 legacy NULL=恒等通道「行为保持」⇒ 属登记不改码项，本条按原机制判 STILL。
- 新锚：p1_session.cpp:403/:446-452；module_adapters.cpp::p1_op_cosmetic :1552；COSMETIC_ALGORITHMS.md:288。

### [17] M3b-A-04（P1/A_SCI_DEF）
- 原报标题：5σ 检测阈的 σ 取自原图、判决作用于 σ=2 平滑图，虚警预算不自洽
判定：**STILL**（σ 取自原图、判决作用于平滑图，机制原样；报告锚点为 sdet_api.cpp，漂移 +34 行）
- lib/star_detector/src/sdet_api.cpp::sdet_detect_impl（现 :1749 起）：:1770-1774 sdet_gaussian_blur_yvv(_d)(image, smooth, sigma=2.0)；:1783 bgnoise = sdet_compute_bgnoise(image)（原图）；:1786 threshold = img_median(image) + 5.0·bgnoise；:1856-1857 判决 T pixel = smooth[...]; if (pixel <= threshold) continue。5σ 阈作用在 σ=2 平滑图上，虚警预算不自洽原样。
- docs/science/STAR_DETECTION.md:15-18 仍写「召回 ≥99% @SNR≥10；虚警 ≤0.1/千像素（纯噪声场）…threshold = median(img) + 5.0·bgnoise（5σ 语义）」；ALG §2/§3 未动。文件不在 base..HEAD changed 列表（锚漂源于扫描时点更早）。
- 新锚：sdet_api.cpp::sdet_detect_impl :1770-1774/:1783/:1786/:1856-1857。

### [18] M3b-C-05（P1/C_DOC_CODE_GAP）
- 原报标题：SDetParams 五字段中四个全仓零读取而文档称有消费面；检测阈三套并存；伪代码与实现相反
判定：**STILL**（四字段仍零读取、三套阈并存、硬编码原样；锚点漂移）
- lib/star_detector/src/sdet_detector.cpp::sdet_get_structure_map :22-23 仅读 hotPixelFilterRadius；:31 sdet_dynamic_regional_background(..., 4, 3.0f, 3, 16) 硬编码 clip=3.0/rounds=3。
- structureLayers/iterativeClipSigma/iterativeMaxRounds/medianFilterDetail 在 HEAD 全仓 git grep 只见**写侧**：sdet_api.cpp::默认值现 :1005-1009（9.0f/5 等，原 :968-978 漂移 +37）、orchestrator.cpp:1633-1637（9.0f）、module_adapters.cpp:2326-2330（5.0f/3——与 orchestrator 值互异 ⇒「检测阈三套并存」依旧：3.0f 硬编码 / 9.0f / 5.0f）、ctypes 镜像声明与 memory.md 登记；无任何消费读取。
- ALG/README 消费面表述（STAR_DETECTION_ALGORITHMS.md §11.1、README:37/:178-180）文件未改。
- 新锚：sdet_detector.cpp:22-31、sdet_api.cpp:1005-1009、orchestrator.cpp:1633-1637、module_adapters.cpp:2326-2330。

### [19] M3b-H-03（P1/H_NUMERIC）
- 原报标题：FP64 通道按 uint16 满值 65535 判饱和、mag 中间量降级 float32，与合同「全程不降级」字面冲突
判定：**STILL**（四站点全数原样；行号漂移 +40/+36/+36/+36 已订正）
- lib/star_detector/src/sdet_api.cpp：:1834-1835 const float norm=65535.0f、dynrange=min(maxi,norm)−bg（模板双实例共用 ⇒ FP64 通道饱和基准仍按 uint16 满值，原 :1794-1801）；:1931-1933/:2122 sat=min(pixel0,T(norm))−T(satrange)，:2203 注释「非饱和候选 sat=norm=65535」；:2309 rec.is_saturated=(A>dynrange)?1:0（原 :2273）；:2341 float local_B=(float)fit_results[i].B（原 :2305）；:2348 rec.mag=-2.5f*log10f((float)box_sum)（原 :2312）。
- 合同面未动：DATA_SEMANTICS.md:695/:723 仍「FP64 通道全程不降级，仅 out_flux/out_mag 按 ABI 保持 float32」；include/star_detector.h:56-57 仍「全程 double 不降级 float32」⇒ 与 :2341/:2348 的 float 中间量降级字面冲突原样。
- 新锚：sdet_api.cpp:1834-1835/:1933/:2309/:2341/:2348。

### [20] M4-C-05（P1/C_DOC_CODE_GAP）
- 原报标题：采样器并发声明「并行路径不经 g_aio_mu」不实：全部 tile 读经全局锁串行
判定：**STILL**（锁串行与文档失实原样，锚零漂移）
- lib/phase2/src/sampler.cpp：:161 static std::mutex g_aio_mu；:166 read_tile_pair 内 lock_guard（「串行化 cfitsio 读」）；:693-694 per-worker 惰性开句柄也走同一 g_aio_mu；:727 worker 体内 read_tile_pair(...) ⇒ 全部 tile 读仍经全局锁串行。
- docs/algorithms/PHASE2_SAMPLER.md:291/:383 仍写「并行路径 per-worker 独立句柄不经此锁」——对读路径为不实声明（仅 open 不读数据，但数据读全在 read_tile_pair 锁内）。文件不在 changed 列表。
- 新锚：同原报。

### [21] M4-F-04（P1/F_TEST_GAP）
- 原报标题：tests/api UPM 确定性/恢复/接缝门永久静默跳过；驱动 calibrate_sum 恒打印 0.0，worker 等价从未被比较
判定：**STILL（两站未修；一站已修——非单站全固）**
- 已修他站：tests/api/test_seam_metric_gate.py 现改为 fail-closed——:29-47 _require_prerequisites 缺 g++/libphase2.a/AIO 库时 raise AssertionError「不得 skip」（V3 B3-A3/RESCUE-P0-08 整改，注释点名旧 skipUnless 之非）。
- 未修①：tests/api/test_upm_parallel.py:55 与 test_upm_recovery_oracle.py:68 仍是 @unittest.skipUnless(g++ + build/linux-openmp-on/libphase2.a)，确定性/恢复两门在缺件环境仍**静默 OK**（结构上不可红）。注：ci/steps/linux_build_root_graph.sh:48-55 现已构建该归档，Linux job 里条件或可满足——但「缺件=绿灯」机制未除，windows-main/未跑该 step 的场景照旧隐身。
- 未修②：驱动恒 0 事实原样——test_upm_parallel.py:49-50 printf 的 calibrate_sum 实参是**字面量 0.0**（csum 算了但未传），且 Python 侧从不解析 calibrate_sum ⇒ 「worker 数无关性经 calibrate 通道比较」依旧为零。
- 文档锚：docs/algorithms/PHASE2_UPM_IMPL.md:349-352 T1/T1'/T3 仍指这些测试（行号微漂）。
- 新锚：test_upm_parallel.py:55/:49-50、test_upm_recovery_oracle.py:68、test_seam_metric_gate.py:29-47（已修站）、linux_build_root_graph.sh:48-55。

### [22] M5a-C-003（P1/C_DOC_CODE_GAP）
- 原报标题：宪章 §10.5 必采的 PSS/每线程 CPU/I/O wait 未采集，资源工件名与 §11 清单两套且未登记偏差
判定：**STILL**（①-⑤子项全部复算成立，锚点微漂）
- ①PSS 恒 0：cli/monitor.h:70 注释仍「/proc/self/smaps_rollup Pss(可得时)」，但 Linux 采样只读 /proc/self/{status(:146),io(:160),stat(:197)}，全文件无 smaps_rollup 读取；cli/memory_report.h:299-304 唯一读 smaps_rollup 处取 Private_Clean/Dirty（非 Pss）。resource_samples.csv 的 pss_bytes 列恒 0。
- ②queue_depth/lock_wait_ns/progress 注入器在（resource_recorder.h:90 set_queue_depth、:52/:165 等），但 git grep HEAD 显示 set_queue_depth/set_progress/note_lock_wait 在 cli/lib/runtime **零调用方** ⇒ 列恒 0（与 M5a-G-004 同链）。
- ③GateConfig 生产填充段现 :900 astrocs::GateConfig g;（原 :858-943）——iowait_percent/mem_bandwidth_percent/one_worker_ns/io_bytes/progress_stalled 无赋值 ⇒ evaluate_gate 现 :229-237/:271-272 的 IoWaitHigh/ProgressStall/MemoryBandwidthLow 等判据不可触发；「每线程 CPU」在 ProcSample 结构仍不存在。
- ④工件名两套原样：宪章 §11 :396-397 仍 resource-timeseries.csv / resource-summary.json（连字符），实现/CLI_PROTOCOL_V1.md:61 与 recorder :226/:246/:275 产出 resource_samples.csv 等（下划线），无偏差登记。
- ⑤C++ CSV 无 fingerprint/来源链未变（Python 合同侧另有 21 列+指纹）。
- 新锚：monitor.h:70/:146/:160/:197；memory_report.h:299-304；commands.cpp:900；resource_gate.h:130-166/:229-237；recorder :52/:90/:116/:230/:244-246；宪章:396-397。

### [23] M5a-G-005（P0/G_GOV_GATE）
- 原报标题：THREAD-BUDGET 机器门空转：扫描面、正则与行级豁免三重漏检并被单测固化
判定：**STILL**（三重漏检 + 单测固化原样；锚点零漂移）
- tools/arch/check_thread_budget.py：SCAN_ROOTS=[REPO/lib] :6（providers/runtime/cli 面全域不可见——如 providers/cpu/baseline、runtime 侧 omp/std::thread 从不入扫描）；EXEMPT 文件名级豁免 :7-11；PATTERNS :69-75 —— std::thread 正则 `std::thread\s*\(|std::thread\s+\w+` **不匹配 std::vector<std::thread> 成员声明**（">" 非 \s/\(），executor.cpp:39/:217、sampler.cpp:890、upm.cpp pool、p3_session.cpp:327 等 vector 形态全部漏检（HEAD 实测这些行仍在，语义为线程池成员）；行级豁免 `row_exempt = is_exempt or "watchdog" in line` :92-94——任何一行含 "watchdog" 即整行放行，不限于豁免文件。
- 恒定输出串 :118「THREAD_BUDGET_CHECK_PASS 未登记线程创建=0…」被 tests/arch/test_thread_budget.py::test_01_real_repo_passes:16 assertIn 钉成期望（判据=检查器自己的 stdout，簇 1 机制①）。
- ci/checks.json::THREAD-BUDGET :555-576 未变（waivable=false，changed_paths 仅 lib/**/include/**）。
- 新锚：同原报。

### [24] M5b-C-03（P1/C_DOC_CODE_GAP）
- 原报标题：顶层幽灵命令 drizzle 在分发表却不在 help 与冻结命令树；未接线用 ARGS(2) 表达；诊断指向已删选项
判定：**STILL**（两主站原样；第三子项「诊断指向已删选项」已不成立——记录订正）
- cli/parser.cpp::kRules :61 仍登记 {"drizzle", {--config,--events-jsonl,--nside,--pixfrac}}；kHelp :66-92 顶层命令清单仍**无** astrocs drizzle（只列 test synthetic --group）；cli/commands.cpp::dispatch :2514 仍路由 drizzle→cmd_drizzle（:1897-1905，emit ARGS "test_preset_only" 返回 astrocs::ARGS=2——「未接线用 ARGS(2) 表达」未变，与 cmd_stub :216-222 同款）；docs/api/CLI_PROTOCOL_V1.md §1 冻结命令树仍无 drizzle 独立命令；docs/contracts/ARCH-001.md:83 LEG-001 行仍写「CLI drizzle 直呼 hp_drizzle_run_hips」——与现实现（仅回 preset 提示）又添一帧失真。
- 订正：诊断指向的 `test synthetic --group drizzle` 现在**有效**（parser.cpp:199-200 kGroups 含 "drizzle"，commands.cpp:2516-2517 接受），原报「指向已删选项」在 HEAD 不再成立。
- 新锚：parser.cpp:61/:66-92/:199-200、commands.cpp:2514/:1897-1905/:216-222、ARCH-001.md:83。

### [25] M5b-E-04（P1/E_TRACE_BREAK）
- 原报标题：acs_status/acs_head 在 legacy 与 abi 头族重复定义，其声称的机器检查文件不存在
判定：**STILL**（双定义与虚构检查锚原样，锚零漂移）
- include/astrocs/abi/status_codes.h :9-10 仍写「机器检查见 tests/abi/run_abi_checks.sh」——该文件在 HEAD 不存在（git grep run_abi_checks 仅命中此注释与审计档案；tests/abi/ 目录实况为 abi00x_*.c / test_*.py，无 run_abi_checks.sh）；:25-29 仍自述「与 legacy 共享 acs_head/acs_status/acs_span 系列…不得同一 TU 混合 include」但无机器执行者；:151-166 `typedef enum acs_status` 与 include/astrocs/common_abi_v1.h :20-23(acs_head)/:55-67(acs_status) 同名双定义原样。
- docs/standards/API_STANDARD.md:11 禁重定义规则句未动。API-DOCS 类门对该无覆盖（簇 1 之「悬空引用」形态）。
- 新锚：同原报。

### [26] M5b-G-06（P0/G_GOV_GATE）
- 原报标题：§18.4「只加载随产品签名清单发布的官方模块」在机器上无实现：产品不加载、hash 全 null、唯一闭环测试不进 CI
判定：**STILL**（主机制原样；第三子项「闭环测试不进 CI」已过期，需订正措辞）
- ①产品不加载：CMakeLists.txt:629-633 target_link_libraries(astrocs PRIVATE astrocs_core astrocs_aio astrocs_hips astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu …) 静态链全部科学库；git grep acs_secure_loader HEAD -- CMakeLists.txt cmake cli lib → **0 命中**（loader 只活在 runtime/registry 与其测试，产品 CLI 从不调用）⇒ §18.4「只加载随产品签名清单发布的官方模块」在产品运行面仍无实现。
- ②hash 全 null：packaging/astrocs.product.json:8-17 十 unit 的 sha256 仍全 null，:6 note 自证「sha256 维持 null…loader 对空期望跳过 hash」——签名清单无完整性校验。
- ③订正：闭环测试**已在 CI**——tests/abi/test_mod001_install_load_check.py:32-43 有 TestCase（调 mod001.main() 断言 rc==0），且 ci/checks.json UT-ABI（:1645 起 linux-main，discover -s tests/abi）会采集；「唯一闭环测试不进 CI」措辞过期。但该门 hosted-only（windows 产物在 Linux 树）且其有效性依赖 L23-001 已登记的 discover 采集数问题——不改本条主判。
- cli/commands.cpp 现锚：unit_file_present :1970、cmd_modules_verify :2098、cmd_selftest :2177（原 :1889/:2017/:2096 漂移 +81/+113/+175 内）。astrocs_runtime/astrocs_io SHARED 各仅 1 个 C 源（CMakeLists:133/:146）未变。
- 建议标记：维持 OPEN，子项③改述。

### [27] M5b-G-14（P1/G_GOV_GATE）
- 原报标题：导出符号与 ABI 合同一致性（§12.3-6）实际无有效检查：门只判符号表非空，Linux 侧零覆盖
判定：**STILL**（判据=非空、Linux 零覆盖原样；文件未改）
- tools/quality/ci_windows_driver.py:605-626：真跑分支 `ok = res["exit_code"]==0 and bool(exports)`——只判 dumpbin /EXPORTS 退出码+符号表非空，**从不比对 module_api_v1.h 合同符号集**（:586-591 声明文案承诺「astrocs_runtime 导出 acs_artifact_*、astrocs_io 导出 acs_fio_*」但代码无该前缀判据——声明超前于实现）；Linux 分支 :580-601 把全部四项标 executed=False hosted_note（Linux 侧零覆盖），非 Windows 主机该项恒绿（记 executed=False 不判红）。
- include/astrocs/abi/module_api_v1.h:7-12 唯一导出合同未变；cmake/install_layout.cmake:91-96 modules/providers 安装未变；CMakeLists.txt WINDOWS_EXPORT_ALL_SYMBOLS 现 :137/:151/:228（原报 :146-153/:217-236，行锚漂移，内容同）。
- 新锚：ci_windows_driver.py:586-591/:605-626、CMakeLists.txt:137/:151/:228。

### [28] M5b-I-05（P2/I_DOC_HYGIENE）
- 原报标题：CI 元文档的规模计数与多项路径/条目已与注册表脱节
判定：**STILL**（计数脱节进一步恶化）
- ci/INVENTORY_REPORT.md:6/:18/:31 仍称 ci/checks.json「70 项」；HEAD 实测 python json 解析 len(checks)=**135**（base..HEAD 的 checks.json 改动又净增，脱节从 60 → 65）。
- ci/WORKFLOW_BINDING_AUDIT.md:29 仍写「注册表（90+4 项）」——与 135 不符（该文件未改）。
- ci/impact_map.json:649 触发路径仍含 `launch/**`（根目录无 launch/，实体在 packaging/launch/ ⇒ 该路径永不匹配，关联门永不增量触发）；:4 notes 未动。
- 新锚：INVENTORY_REPORT.md:6/:18/:31、WORKFLOW_BINDING_AUDIT.md:29、impact_map.json:649。

### [29] M6a-D-001（P1/D_COMMENT）
- 原报标题：生产注释承担「未登记的裁决请求」角色：SCI 冻结 1e-4 px 门被同文件注释自证一步法数学不可达；裁决与偏差 ID 只活在注释与控制包台账里，docs 权威登记面（STANDARDS_REGISTRY / PLATESOLVE §11.3）对 DISP-WCS-007、DISP-WCS-008 零命中（来源 L13-001 的注释层残余；科学定档在 M1a）
判定：**STILL**（注释自证+登记面零命中，锚微漂）
- lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip 可达性注记现 :406-409（原 :397-400 漂移 +9）：「…为逆映射最近奇点决定的逼近极限 (order 6..15 → 5.3..0.26 px, 收敛率~0.73); 冻结 1e-4 px 在该 fixture 下数学不可达, 需负责人裁决 (finding)」——裁决请求仍只活在注释里；:393（原 :384）SIP 段标题仍写「(WCS-002/DISP-WCS-008 整改…)」引用无登记宿主的 DISP-WCS-008。
- 登记面反证：git grep 'DISP-WCS-00[78]' HEAD -- docs → **0 命中**；PLATESOLVE.md §11.3 标题仍「DISP-WCS-001..006」；STANDARDS_REGISTRY.md wcs 域仍只 DISP-WCS-001/006（:58/:66/:75-76/:240-241）。两个 ID 的实际宿主仍只有控制包台账（工程控制/ACTIVITY_STATE.md 与 evidence/v8_1_ci_control/TASK_STATE.json）。
- 新锚：ipv_wcs.cpp:393/:406-409；docs 零命中复算在案。

### [30] M6a-D-009（P2/D_COMMENT）
- 原报标题：§12.2 禁止形态（任务号/审计轮次/commit 哈希/修复流水）成为注释主干：Phase2/3 侧六目录实测 257 处命中 + Phase1 侧抽样清单，被登记偏差条目自身的注释也在复述历史（来源 L13-015 全域 + L14-003 量化，两切片并档）
判定：**STILL**（禁止形态注释主干未变；个别被点名行已被顺带清理/重写，锚点订正）
- 复算在案的现文站点：ipv_sip.cpp:386「( 修复: 原 10 太小)」+:470「坐标系转换 ( 修复)」（原 :29/:171/:386 站点措辞已变、修复流水族仍在）；ipv_solver.cpp:1772「v3.1 修复: …」；lib/calibration/src/master_generator.cpp:171-175「B13-R13-7: …修复前负中位数…科学数据损坏」修复流水；lib/calibration/include/astro_calibration.h:112「双精度 ABI 改造 (R10)」；commit 短哈希族 git grep adf820ac/1959dc89 → cosmetic/types.h:4、cosmetic/CMakeLists.txt:3、cosmetic/src/module_entry.cpp:3、calibration integration.json:4 等仍多站；noise_model.cpp:410「W1-NOISE-002:」；sha256.cpp:35「QA-001:」任务号前缀仍在。
- 差异记录：原报 ipv_solver.cpp:225-227「缺陷 (P0-1 修复前)…v1.0 曾误设为 6」三行在 HEAD 全域 git grep 已零命中（该单站被并发清理），不影响「257 处/主干形态」总体判定（全域口径未逐字复算，按抽样 10+ 站仍在判 STILL）。
- 建议标记：维持 OPEN；被点名锚更新为上列现文行。

### [31] M6a-G-001（P0/G_GOV_GATE）
- 原报标题：注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的判据被「冻结」二字永久豁免、每文件至多报一条、扫描面只覆盖 lib 的一部分，却恒定自报 coverage=full/1.0 —— 注释质量没有任何机器防线（来源 L14-004，根因单条定稿）
判定：**STILL**（空壳门逐判据复算原样，锚零漂移）
- tools/quality/contracts/check_comments.py：STALE_PATTERNS :9-13 与 REQUIRE_ID_NEAR :15 定义后全域 git grep 仅命中定义行本身（**死常量**，三条正则判据无执行者）；唯一生效判定在 :37-45 —— 陈旧轮次判据退化为 V19R2/V19R3 字面包含测试，且 :39「if 冻结 not in c and history not in str(src).lower()」—— 含「冻结」的文件被**永久豁免**；:27 扫描面只 rglob lib/**/*.cpp|*.h|*.hpp（cli/、tests/、tools/ 不在面内，与 ci/checks.json CON-COMMENTS changed_paths=docs/lib/cli/tests/contracts 不一致——changed_paths 决定触发、扫描面决定判据，双重错位）；:52 coverage 恒自报 ratio=1.0/mode=full；每文件至多一条（append+status 结构未变）。
- ci/checks.json::CON-COMMENTS :773-797（现锚）仍 waivable=false 跑该脚本；.github/workflows/ci.yml V19R4 token 行未动。
- 新锚：check_comments.py:9/:15/:27/:39/:52/:63；checks.json:773-797。

### [32] M6b-C-003（P1/C_DOC_CODE_GAP）
- 原报标题：同一 ACTIVE 文档内冻结投影集新旧两版并存：§0 非目标仍写「不实现 SIN/ZEA/CAR/AIT」，§15 已按负责人裁决冻结 TAN/SIN/CAR/AIT
判定：**STILL**（新旧两版并存原样，且新登记面又添一帧多头）
- docs/algorithms/PHASE3_PROJ_IMPL.md（未改）:20 §0 非目标仍「不实现 SIN/ZEA/CAR/AIT」；:107 仍「projection 仅接受 TAN；SIN/CAR/AIT 即便在…」；:348 §15 仍「版本化 projection registry 与冻结四投影（TAN/SIN/CAR/AIT）」——同一 ACTIVE 文档两版并存。
- 状态面多头复算：docs/owner/RELEASE_STATUS.md:72 已把「Phase3 四投影 registry」标 IMPLEMENTED（lib/phase3_proj registry v1 恰四行 + 两道 ctest 门 + CTEST-P3-PROJECTION-* 在册）；但 docs/modules/registry/astrocs.phase3.wcs.md:134 仍「projection 硬编码 TAN，UNSUPPORTED 枚举无产生点」、lib/phase3_proj/module.yaml:80 entrypoint: MISSING、lib/phase3_session/p3_wcs.cpp:61-64 生产会话路径仍 TAN-only 拒绝、README.md:33 仍把 Phase3 SIN/ZEA/CAR/AIT 列为未实施。宪章 §18.1 裁决在 p3_projection.cpp:10 有引用但 §0 非目标行未订正。
- 新锚：同原报 + RELEASE_STATUS.md:72 新帧。

### [33] M6b-G-001（P0/G_GOV_GATE）
- 原报标题：未经证据支撑的「已验证」声明（单一根因，四组实例）：八层矩阵在 TEST / EVIDENCE / SRC / 整行四个层面给出 VERIFIED，而门的结构只可能判「文件存在 + 符号 token 可见」
判定：**STILL**（结构不变；矩阵数值按 HEAD 复算更新）
- tools/traceability/check_traceability_matrix.py：C7 :17/:241 对 VERIFIED 层锚的机器依据 =「文件存在且被 Git 跟踪」；:262-265 对 MOD/SRC/TEST 层在 C8 直接 continue（TEST 层 VERIFIED 不再核任何 id 文本承载）；_check_anchor :284-315 只做锚行内符号 token 可见性；STATUS_OK :55 把 VERIFIED/MISSING/NONE 并列为合法值；AUTHORITY_DIRS :78 供 C8 但 C8 默认仅 WARN（--strict :146 才升 ERROR），而 ci/checks.json::TRACEABILITY-MATRIX command 仍为不带 --strict 的裸调用 ⇒ 「已验证」声明的门在默认路径上只可能判「存在+可见」。
- HEAD 复算（python json）：docs/traceability/TRACEABILITY_MATRIX.json 30 模块，test_status VERIFIED=22/MISSING=8；VERIFIED 中 test_path 以 docs/ 开头 **15**/22（原报 18/22，矩阵并发期有改动，机制不变；另有 3 行 test_path 为「目录+多门分号长句」非可解析单文件），真实可解析测试文件仅 3-4 行；evidence_status VERIFIED=6/**MISSING=24**（原报 27/30 MISSING，有好转但未修「无据 VERIFIED」根）。
- 新锚：checker :17/:241/:262-265/:284-315/:55/:78/:146；矩阵现值 22/15/24。

### [34] M6b-I-003（P2/I_DOC_HYGIENE）
- 原报标题：文档内统计与清单陈述与实测不符（可复算的口径失真清单）
判定：**STILL**（三处口径失真原样；csv 按 HEAD 复算）
- docs/ARCHITECTURE.md:129 仍写「docs/TRACEABILITY.csv(76 行…SCI-/ALG-/DATA-/ENG- 全 VERIFIED，MUST family docs/TRACEABILITY_family.json + gen_v19_source_snapshot 绑定实现符号/测试)」——git show HEAD:docs/TRACEABILITY.csv = **64 行**（表头+63 数据行；python csv 复算 status 值 63/63 全 VERIFIED，无差别批量值）；docs/TRACEABILITY_family.json（及 FAMILY.csv）在 HEAD **不存在**（git ls-tree docs/ 零命中，悬空引用）；:130 仍写「docs/modules/*.md(13 份 L5)」——HEAD 实测 23 份（另 docs/modules/registry/ 26 份）。
- 工作树该 csv 为脏（并发改动面），本判以 git show HEAD 内容为准；「全 VERIFIED 无据」问题本体在 M6b-G-001，本条只判计数/清单口径失真。
- 新锚：ARCHITECTURE.md:129/:130；实测 64 行 / 63 VERIFIED / 0 family 文件 / 23 份模块页。

### [35] M7-A-123（P1/A_SCI_DEF）
- 原报标题：交换合同要求三 Phase 最小平面集含「mask」，而 mask 在三个 DATA 节都被判「非产品输出」且语义三方不一（L21-007）
判定：**STILL**（三方语义不一原样；DATA_SEMANTICS 有改但站点未动）
- git show HEAD:docs/contracts/DATA_SEMANTICS.md：:174-177（§9.4）仍「坏点掩码 1=坏点仅在修复路径内部使用，不作为产品输出」；:214-216（§10.3）仍「掩码为模块内部产物，不作为产品输出（与 §9.4 一致）」；:314-315（P1-HIPS/AstroSphereTileView）mask 又是 covered_area/valid_mask 覆盖语义——同文档内「非产品输出的修复内部物 / 覆盖位图」两义并存。
- 交换面：docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:39-41 三 Phase 最小平面集仍一律含 mask（P2/P3 为 signal,support,mask），:46 又给 mask=坏点/质量位掩码 第三义；:79 plane_id "mask" units=bitmask；runtime/artifact_store/phase_product_exchange_validator.py:56 _PLANE_ID_SET 含 mask、schema enum 同——校验器要求存在、语义三方互斥的机制未变。docs/science/PHASE3_HIPS_TO_FITS.md:106 仍 coverage 二值 mask 义。
- 运行时阻断强弱的部分属 M2b 会签（原判「无法判定运行时」不变，文档面仍成立）。

### [36] M7-A-131（P1/A_SCI_DEF）
- 原报标题：MAD=0 时热/冷检测阈值退化为「高于中位即热、低于中位即冷」，文档两次判为良性（L20-011）
判定：**STILL**（机制在码、良性措辞在文；锚点从 CALIBRATION_ALGORITHMS 移至 COSMETIC_ALGORITHMS/代码行，已订正）
- lib/calibration/src/cosmetic_corrector.cpp::detect_hot_pixels :125-127 sigma=1.4826f*mad、threshold=med+threshold_sigma*sigma、无 mad==0 护栏 ⇒ MAD=0 时任何 >med 判热；::detect_cold_pixels :145-147 同理 <med 判冷。
- docs/algorithms/COSMETIC_ALGORITHMS.md:51 现文明写「mad=0（无离散度）时 sigma=0，阈值退化为 ±med（检测不禁止）」——退化被书面接受；另一处「良性」判定为 docs/science/CALIBRATION.md:76 的 sigma-clip「MAD=0 → 提前终止不再剔除」（该处对 master 合并确属良性，但同页把 detect_hot/cold 归入同一不变量话语）。M3 的常量母版 ≥50% 同值证据使触发条件现实存在。
- 新锚：cosmetic_corrector.cpp:119-133/:140-152；COSMETIC_ALGORITHMS.md:45-51。

### [37] M7-E-201（P2/E_TRACE_BREAK）
- 原报标题：SCI/合同/注释三处「与实现一致」声明所引代码行锚整段漂移（本代理读码亲证）
判定：**STILL**（五处锚漂全部复算仍在，两处进一步恶化）
- docs/science/ASTROMETRY.md:66 仍引 ipv_wcs.cpp:13-16,153-164,274-420,530-576——Y-down/取反块现于 ::build_fits_wcs_from_solution :651-696（原报 641-696 再漂 10）；同句「ipv_select.cpp:723,712 一致」自相矛盾（复核时 695/708，现文 723/712 明显笔误式漂移）。
- docs/contracts/DATA_SEMANTICS.md@HEAD:774 仍「cd12/cd22 已取反 ipv_wcs.cpp:542-544」（实际现 :658-659 区域）——且同行「trans 线性项 det<1e-15 仅 warn 跳过 SIP（:322-325，DISP-WCS-004）」在 B4-1 修复后**连语义都失真**（该分支现为显式失败 success=false，见本档 M1a-C-002）：锚漂升级为「结论错误、指针失效」双料。
- lib/plate_solve/cpp/ipv/include/ipv_solver.h:74 仍「ipv_wcs.cpp:542-570」→ 实际 :651-696。
- docs/science/DRIZZLE.md:71 仍「spherical_overlap.cpp:40,573,773-931」——HP_CIRCUMRADIUS_FACTOR 现 :42（漂移 2 行未订正）。
- docs/algorithms/PHASE2_SAMPLER.md:196 自注旧行号锚（正面样板，未变）。共性「结论正确、指针失效」+无机器门（§12.3-9 无承载存在性判据）原样。

### [38] M7-I-202（P2/I_DOC_HYGIENE）
- 原报标题：hips_frame 字面值在合同与写侧措辞不一致（L21-009 **按前台裁决降级定稿**）
判定：**STILL**（按前台降级口径复算：三处值域不一致原样，登记句未做，且添一帧新失真）
- HEAD 复算：lib/phase3_session/hips_properties.cpp:125-126 收 {equatorial, icrs}；runtime/io/hips_core.c:236-240 只收 equatorial（「非 equatorial (未知 frame 拒绝)」，icrs 被拒）；DATA-002 交换层两值+跨帧混用拒——DATA_SEMANTICS@HEAD:883/:886 仍登记 {equatorial,icrs}+mismatch 拒（B2-A8）；写侧恒 equatorial：aio_hips_writer.cpp:951/:1196 + HIPS_WRITER.md:174。三处合法值集互不一致的机制与「处置②登记为偏差」未完成。
- 新增事实：DATA_SEMANTICS@HEAD:2115 又把读侧合同写成「frame=ICRS——hips_properties.cpp:113-122」——既与代码两值行为不符（值域措辞第四处漂移），行锚也错（现 :125-126）。
- 判词维持前台裁决措辞：不写「互操作被锁死」，写「writer 恒 equatorial 与合同/读路径值域措辞不一致」。

### [39] M8-F-002（P0/F_TEST_GAP）
- 原报标题：`tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 CHECK 的失败计数不参与退出码，`main` 无条件 `return 0`（覆盖 L23-002 与 L23 §1 的旧措辞）
判定：**FIXED**（报告站修复+带防回退自锁；checker 他站同步修好）
- 修在哪：tests/unit/io_ownership_test.cpp 全文重写（32→138 行）：main 末 :130-137 「failures==0 → printf PASS return 0；否则 fprintf FAIL + return 1（M8-F-002: 失败必须非零退出）」——退出码缺陷机制消失。
- 防回退锁（这条使 FIXED 为「修完全」而非仅删症状）：文件内自带子进程自锁 failure_sets_nonzero_exit（:123-128 probe_failure_exit(argv[0]) 以 --inject-failure 分支 :82 return failures==0?0:1 注入一次必败，父进程 CHECK(rc != 0)）——若将来又吞失败，本用例自己变红。头注 :4-7 点名 M8-F-002。
- 注册面：tests/unit/CMakeLists.txt:136-141 add_executable/add_test(NAME io_ownership)（在册未断）；ci/ctest_baseline.json 引用面未变。
- 他站复算：tools/check_aio_ownership.py 已重写为 55 行版，main :40-52 「violations → return 1 / 无违规 → 0」+ SystemExit(main())——原「:459 恒 return 0」形态消失（同机制他站一并修好）。
- 修完全性判定：测试+静态工具两站均闭环；无同类残留（本条覆盖 L23-002 旧措辞的登记职责已履行）。

### [40] M8-F-010（P1/F_TEST_GAP）
- 原报标题：修复未固化：B2-A14 新增 PHOTDEGRADE / uncalibrated_adu_allowed 显式降级路径已实现并回写文档，但无任何回归用例钉住
判定：**STILL**（实现与文档仍在，测试面依旧零命中；行锚漂移已订正）
- 实现面复算（文件在 base..HEAD 大改，锚已漂）：hp_drizzle_api.cpp::PHOTDEGRADE 读取现 :946-949（原 :935-938），drizzle_engine.cpp 拒绝语义分支现 :1053-1056 与 :2212-2215（原 :1030-1033/:1950-1953），drizzle_engine.h::uncalibrated_adu_allowed :36 未变；module_adapters.cpp 上游发射面现 :3001/:3089/:3093-3114（原 :2269-2351 区，因 P9/phot 改动整体下移）。
- 测试面：git grep 'PHOTDEGRADE|uncalibrated_adu' HEAD -- tests lib/**/tests → **0 命中**——p1drz 新增 124 行测试（见本档 M2a-F-4 复核）未覆盖此路径；文档回写（DRIZZLE_GEOMETRY.md:178、DATA_SEMANTICS:249 一带）在册但无断言钉「默认拒绝 / PHOTDEGRADE=1 放行 / provenance PHOTAPPL=0+BUNIT=ADU」三判据。
- 结论：B2-A14 修复未固化原样；atoi 解析/默认值翻转类回退仍无护栏。

### [41] M8a-C-001（P1/C_DOC_CODE_GAP）
- 原报标题：已废弃第二调度器被 README 写成「正式科学运行只有一条命令」，并整套冻结第二套 `ASTROCS_*` 退出码
判定：**STILL**（唯一入口双头叙述原样；相关文件未改）
- lib/orchestrator/README.md:5-7 仍「## 唯一运行方式 / 正式科学运行只有一条命令：orchestrator.exe <stage1.json>」；:60-75 仍整套 0-10 十级 ASTROCS_* 退出码表（:66 含 ASTROCS_MODULE_MISSING）；:76 仍「Stage1 流水线（Phase1 V4 生产链）」。
- 宪章 §3.1/§8.1 单入口条款不变；docs/contracts/ARCH-001.md §7 G7 行（复核时 :88）仍「canonical run 不链接/调用旧 scheduler」；对照根 CMakeLists add_subdirectory 与 packaging/astrocs.product.json:8-17（PLATFORM-CLI 唯一 exe 交付）口径未动 ⇒ README 给「旧调度器」造成第二「正式」入口的冲突原样。
- 新锚：同原报。

### [42] M8a-E-003（P2/E_TRACE_BREAK）
- 原报标题：DISP-PSF-007 被 README/manifest/测试/HANDOVER 四处当偏差 ID 使用，偏差登记表只到 006
判定：**STILL**（四处使用者 + 登记表止于 006 的缺口原样；dynamic_psf README 在 base..HEAD 有改但仍引用 007）
- HEAD 复算 git grep DISP-PSF-007：lib/dynamic_psf/README.md:4「（DISP-PSF-007 候选收口）后」、lib/dynamic_psf/module.yaml:136「即 DISP-PSF-007 候选」、lib/dynamic_psf/tests/p1psf/p1psf_tests_core.cpp:717「原 DISP-PSF-007 候选测试锚翻转」、HANDOVER.md:38「DISP-PSF-007 整改，三形态 SIGABRT 收口」——四处齐全。
- 登记表 docs/algorithms/STAR_PSF_ALGORITHMS.md:178 标题仍「11.3 DISP-PSF-001..006」，表内无 007 行 ⇒ 「偏差账本缺一行、批 ABI 整改事实无法从 L1 回溯」机制未变（PSF-001 整改已落地但 007 未回写登记）。
- 新锚：同原报（README:4/module.yaml:136/tests:717/HANDOVER:38/STAR_PSF_ALGORITHMS.md:178）。

### [43] M8a-G-007（P1/G_GOV_GATE）
- 原报标题：生成器自述「checker 以本生成器输出为源」不实：registry 无 diff 门，手写页与 descriptor 双向漂移（裁决：维持 P1）
判定：**STILL**（自述不实与登记缺口原样；descriptor 行锚已漂）
- tools/quality/gen_module_readmes.py:7 仍写「非手工清单, checker 以本生成器输出为源」；git grep 'gen_module_readmes' HEAD -- ci → 0 命中（ci/checks.json 无任何以其输出为源的 checker 项，无 diff 门）⇒ 自述的机器闭环不存在。
- docs/DOCUMENT_INDEX.yaml:392/:401/:407 仍登记三页「手写合同页（registry 无 descriptor，生成器不覆盖；重生成时须排除/保留）」——重生成毁档风险仍靠注释自觉、无门拦截。
- 漂移订正：lib/core/src/module_adapters.cpp::p2_coverage_descriptor 现 :862 起（原 762-779，base..HEAD P9/phot 改动 +100 行级）；registry 手写页与 descriptor 的双向漂移机制不变（front matter 无 generated 标记）。
- 新锚：gen_module_readmes.py:1-8/:7；checks.json 零命中；DOCUMENT_INDEX.yaml:392/:401/:407；module_adapters.cpp:862。

### [44] M8a-I-007（P2/I_DOC_HYGIENE）
- 原报标题：toolchain.lock 已刷新，INVENTORY_REPORT 仍以现势口吻归因锁「九类工具全部缺失」— 修复未回写型失真
判定：**STILL**（锁已刷新、报告归因未回写）
- ci/toolchain.lock.json HEAD 现值：python 解析确认 14 个工具键 present:true（cmake/gcc/g++/clang/clang++/ninja/make/ccache/pytest 等在列），missing_tools 已非「九类全缺」形态。
- ci/INVENTORY_REPORT.md:15 仍写「本机工具链（ADOPT-004 锁存）：python3/git/zstd 可用；**cmake / gcc / g++ / clang / clang++ / ninja / make / ccache / pytest 全部缺失**（ci/toolchain.lock.json missing_tools）」——以现势口吻引用旧锁快照，未随 toolchain.lock 刷新回写；另 :1-8 头部口径未变 ⇒ 控制节点能力被系统性低估的失真原样（修复未回写型）。
- 新锚：INVENTORY_REPORT.md:15；toolchain.lock.json present 计数=14。

### [45] M9-D-1（P2/D_COMMENT）
- 原报标题：drizzle 局部尺度推导注释把"量纲为一的切平面坐标"表述为"仍是弧度量级小值"，注释表述失准而实现结果正确
判定：**STILL**（注释表述失准原样，锚基本无漂）
- lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::local_scale_3d_tangent 现 :463 起；注释块 :486-487 仍「切平面坐标 (xi, eta) 由 gnomonic 投影得到, 量级为弧度 /（单位向量 dot 积无量纲, 除以 dot(v, center) 后仍是弧度量级小值）」——xi/eta 实为量纲为一的切平面坐标（R/R0 归一），非弧度；实现结果正确（:508 sigma_min=sqrt(lambda_min) 语义注「弧度/像素」同属名实不符链）。调用点现 :569（原报 :568 邻域一致）。
- 文件在 base..HEAD 有 364 行改动，但本函数与注释未被触及（B2-A15/P23 改动在写侧/归一化路径，不在该注释）。
- 新锚：drizzle_engine.cpp:463/:486-487/:508/:569。

### [46] M9-G-5（P1/G_GOV_GATE）
- 原报标题：IO-001 原子写提交点全程无耐久性屏障：rename 前只 fflush 不 fsync，目录项亦不 sync（掉电/崩溃可留零长度或旧内容产品）
判定：**STILL**（提交点无耐久性屏障原样；「无生产调用点」限定不变）
- runtime/io/fits_core.c 复算：fio_file_flush :176 = fflush 仅此；acs_fio_writer_end_v1 提交链现 :1388/:1473/:1480 —— fflush → fio_file_close →（可选 verify_before_rename）→ rename，**全文件无 fsync/FlushFileBuffers/_commit 任何一处**（grep 命中仅注释/flush/fflush/rename）⇒ rename 提交点在掉电/崩溃下可留零长度或旧内容目标（POSIX 下 rename 原子但需 fsync(文件)+fsync(目录) 才耐久）。
- 同仓正对照仍在：lib/hips/src/aio_publish.cpp 有 fsync 纪律（命中确认）——「新边界未继承老边界纪律」（SUMMARY 簇 6 同型）。
- 影响限定复算：acs_fio_writer_end_v1 在 lib/cli 生产调用点为 0（本分片以 git grep 侧证，与原判一致）⇒ 边界承诺与实现不符类，非现产损失。
- 新锚：fits_core.c:176/:1388/:1473-1480；aio_publish.cpp 正对照。

### [47] V1-N-02（P1/C_DOC_CODE_GAP）
- 原报标题：SNR 物理量改了但二进制版本位没改 ⇒ 新旧 `.hiss` 同标签承载不同物理量；JSON 面加了 `snr_schema` 而二进制面没加
判定：**STILL**（二进制面失同步原样；「DATA_SEMANTICS 零命中」子项已在 HEAD 修复，订正）
- lib/astro_image_io/include/aio_healpix_io.h（原报路径 src/healpix/ 有漂移，HEAD 在此）：:52 附近仍注 `double snr_psf; // (A-B)/mad (double)`、:57 起「对应 snr_format=1 的二进制布局」、snr_phot 注释仍「1/(ln10×sigma_residual)」——P5-SNR 已把生产 SNR 换成 Horne SNR=F/sigma_F（见本档 M3-A-001 FIXED），**同标签（snr_format=1）承载不同物理量**的机制未变，未升版、未在二进制头加 schema 位；读写侧 aio_healpix_io.cpp 原样搬运（snr_format 0/1 仅密/疏布局语义，无物理量版本）。
- JSON 面确有 `DATA-P1-SNR/2` 登记（git show HEAD:docs/contracts/DATA_SEMANTICS.md:428 新增 §13.4，字段注「median(SNR_F)」）⇒ 原报子项「DATA_SEMANTICS 对 DATA-P1-SNR/p1_snr 零命中」**在 HEAD 不再成立**，属修复半区：JSON 面已同步、二进制面未同步（正是原报「半同步」判词的镜像证据）。git grep snr_schema HEAD 命中登记节，但 .hiss 二进制头无对应位。
- 新锚：aio_healpix_io.h（实际 include/aio_healpix_io.h）snr_psf/snr_phot 注释与 snr_format=1 布局段；DATA_SEMANTICS@HEAD:428-440；建议标记：维持 OPEN（子项③已闭环可注记）。

### [48] V10-N-02（P1/C_ALG_IMPL）
- 原报标题：`A_ORDER`/`AP_ORDER` 未做 `[0,5]` 校验 ⇒ SIP 求值按 order 索引 36 元素数组：`order=6` 越入相邻数组、`order=1000` 索引 6000
判定：**STILL**（读侧不夹原样；行锚基本零漂）
- lib/healpix_db/healpix_drizzle/fits_reader.cpp@HEAD：:289/:291 A_ORDER/AP_ORDER 走 parseInt（:76 起 strtol 无 endptr 无界）→ :395-396 直写 img.wcs.sip.order/ap_order；:378-383 仅 A≠B/AP≠BP 时 warning，**无 [0,5] 值域校验**（git grep 'SIP_MAX|>5|<0|clamp' 于该文件零命中）；消费式 wcs_sip.cpp:85 for(i=0;i<=order;i++) 索引 fits_reader.h 的 double a[36] ⇒ order=6 越入相邻数组、order=1000 索引 6000 原样。
- 写侧对照原样：hp_drizzle_api.cpp 现 :99-101「SIP order 必须在 [0,5]」硬失败（frame 边/反向边已夹）——「写侧处处夹、读侧一处不夹」格局未变；三把锁测试仍全走 aio_frame_kv_set 面（p0_sip_order_guard_test.cpp 在 lib/healpix_db/healpix_drizzle/tests/，真文件边畸形 FITS 头负例仍缺）。可达性（导出 API + CLI 链入面）与不判 P0 的理由维持。
- 新锚：fits_reader.cpp:289-291/:378-383/:395-396、wcs_sip.cpp:85、hp_drizzle_api.cpp:99-101。

### [49] V11-N-01（P0/G_GOV_GATE）
- 原报标题：`AioHipsSnrPoint` 镜像落后 C 头两字段 ⇒ 越界读 24 字节，且**交付 HiPS SNR 目录第 2/3 条记录是邻堆垃圾**
判定：**STILL**（P0 越界读结构原样，锚零漂移）
- lib/astro_image_io/include/aio_hips.h:84-92 仍 6 字段（ra_deg/dec_deg/snr/star_id/quality_flags/photometric_status，sizeof=40）；lib/astro_image_io/tests/hips_direct_smoke.py:34-38 镜像仍 4 字段且末字段仍叫**已废弃的 source_id**（sizeof=32），:98 仍 (AioHipsSnrPoint*3) 分配 96 字节喂 aio_hips_write_snr_points(ps,pts,3)；消费侧 aio_hips_writer.cpp:913-914 按 40 字节步长 push_back ⇒ **越界读 24 字节、第 2/3 条记录错位 8 字节**（交付 HiPS SNR 目录 star_id/quality_flags 为邻堆垃圾）——三文件均不在 base..HEAD changed 列表，机制逐字复算在位。
- 门可见性复算：该结构无 struct_size/abi_version；hips_direct_smoke.py 仍不在 ci/checks.json 任何 profile（UT-ABI discover -s tests/abi 不覆盖）。
- 新锚：aio_hips.h:84-92、hips_direct_smoke.py:34-38/:98、aio_hips_writer.cpp:913-914。

### [50] V11-N-09（P1/G_GOV_GATE）
- 原报标题：（P1）导出面白名单只覆盖 5 个模块 DLL，**被跨语言消费的 5 个 DLL 零裁剪**；Gaia 的公共 ABI 头住在 `src/` ⇒ 天然逃过任何 `include/` 面普查
判定：**STILL**（白名单 5+5 与盲区原样；astro_image_io 命中项经查为第三方噪声，订正记录）
- HEAD 复算：find lib/providers/runtime/cli/modules 的 .def/.map 恰 10 个 = 5×astrocs_p1_*.def + 5×module_exports.map，全在 lib/{calibration,cosmetic,drizzle,hips,snr_estimator}/src/，.def 仅放行 astrocs_module_query_v1（EXPORTS 单行复算）。
- 被跨语言消费的 5 个 DLL 目录：git grep 'version-script|WINDOWS_EXPORT_ALL|.def|module_exports.map' HEAD → star_detector/dynamic_psf/ipv/gaia_xpsd_client = **0 命中**；astro_image_io 的 7 命中逐看全在 third_party/cfitsio（ltmain.sh/README_OLD.win 构建脚本）与测试随机种子数字，**模块自身导出裁剪仍为零** ⇒ 「5 个交付物无 §12.3 导出面对账机器面」成立。
- Gaia 口径：lib/gaia_xpsd_client/src/gaia_client.h:66 仍是 GaiaSpectrumStar 权威定义；include/astrocs/gaia/types.h 存在但不含该符号（git grep include 面 0 命中）⇒ src/ 盲区在现文（include/ 面普查仍看不见它）。
- 新锚：10 文件清单 + gaia_client.h:66。

### [51] V12-N-07（P2/A_SCI_DEF）
- 原报标题：（P2）割线迭代 5 个兜底字面量与结构默认值**各写一遍**；`±6.0` 全仓无承载
判定：**STILL**（五对双写原样、无同源引用无锁；「±6.0 无承载」子项已修复——订正）
- lib/plate_solve/cpp/ipv/src/ipv_select.cpp（文件在 base..HEAD 有改，本块锚 :395-400 → 现 :424-429）：safety/tol/max_q/zero_step/alpha 五对「params 字段>0 ? 字段 : 字面量」兜底仍各写一遍 3.0/0.1/4/3.0/0.2885；ipv_types.h :244/:249/:256/:261 等结构默认各写另一遍——两权威点互不引用，改默认即静默分叉、兜底路径生产可达（ipv_api 允许外部填 params），机制原样；test_mag_iter/test_mag_iter_delivery（P14-N-10 锁）比对的是交付面，**兜底==默认**仍无比对。
- 订正：原报「±6.0 步长夹无 params 字段、docs 0 登记」已过期——现设计改为量级夹界 params.m_lim_clamp_lo=6.0/m_lim_clamp_hi=22.0（ipv_types.h:254-255）+ ipv_select.cpp:421-423 从字段取值，±6.0 步长夹形态消失（入字段一半闭环）。
- 建议标记：维持 OPEN（主机制五对双写），子项注记。

### [52] V12-N-15（P2/A_SCI_DEF）
- 原报标题：（P2）门常量 `511` 与 `512` **互为字面量、无派生、无 `static_assert`**
判定：**CANNOT_STATIC**（静态锚不可复现——请求前台人工三级复核）
- 三级复核记录：①git grep 'MAX_PSF_TERMS|PARAM_SLOTS' HEAD（全仓）→ 仅 问题扫描/findings 文本自身命中；②git grep 521095b8 → **0 命中**（base 即无）；③worktree ls/grep 同 0；git log -S 'MAX_PSF_TERMS' --all 无增删提交记录；被引权威文档 docs/algorithms/PHASE1_HIPS_DRIZZLE.md 在 HEAD **不存在**且无删除提交。
- 现文最相近事实：hp_drizzle_api.h:132-137 现行反向输入为 sip_order(注 0..4)+sip_a[36]…数组对；[0,5] 夹在 api.cpp:99-101（frame 边）——「511/512 互为字面量」的门常数对**在任何被跟踪树中不可复现**，疑为扫描时点未落盘/未合并工作区或引文错位。
- 不改判 FIXED（无修复记录）也不改判 STILL（复算不成立）；移交前台按「锚不可复现」处置（V12 产出者对质或撤条）。

### [53] V13-N-06（P1/G_GOV_GATE）
- 原报标题：`c3452d48` 删掉的 4 行 VERIFIED 合同行**无任何对应登记动作** ⇒ 合同面与追溯面永久分叉且恒不红
判定：**STILL**（四面复算全部原样）
- git show HEAD:docs/TRACEABILITY.csv 复算：SCI-PSF-001/SCI-REJ-001/SCI-INT-001/SCI-ACR-EQUIV-001 四 ID **均不在** csv（工作树亦不在）——c3452d48 净删的 4 行未恢复。
- docs/contracts/INDEX.yaml 现文：:42 SCI-PSF-001、:74 SCI-REJ-001、:82 SCI-INT-001 status: ACTIVE，:122 SCI-ACR-EQUIV-001 DORMANT（与原报逐字一致，行号零漂）⇒ 合同面说三条活着、追溯面没行的分叉仍在。
- tools/quality/v19r3_traceability.py（HEAD 版全文 grep）：CONTRACTS 不含四 ID ⇒ 重跑生成即永久缺失；ci/checks.json 中 CONTRACT-GRAPH 与 CON-TRACEABILITY 两门在位但**无 INDEX↔CSV 的 ID 集对账**（结构性恒不红）；CHANGELOG/memory/backlog 零命中复算同原报。
- 新锚：同原报（全部复算命中）。

### [54] V15-N-16（P1/F_TEST_GAP）
- 原报标题：（P1·需运行期定性）`UT-BACKEND` 的 command 只有 `unittest discover`、**无 build 依赖声明**，而三个 backend 测试都走 `EXE = REPO/build/astrocs` 真跑 ⇒ **干净 checkout 上"真行为断言"整体 error**
判定：**STILL**（门无构建依赖声明原样；一站缓解、面扩大——订正「三个测试」口径）
- ci/checks.json@HEAD：UT-BACKEND command 仍裸 `python3 -B -m unittest discover -s tests/backend -t tests/backend`，无任何 build 前置/依赖声明（profiles=linux-main；changed_paths 含 lib/cli/include——改生产码即触发该门但门自己不管 exe 从哪来）。
- 消费面复算：grep 'join(REPO,"build","astrocs")' tests/backend → **10+ 文件**直取产物（test_cpu_profile.py:49-50 直接 subprocess.run 后 assert returncode==0——干净 checkout 上 exe 不存在 ⇒ FileNotFoundError 整体 ERROR 原样）；**唯一缓解**：test_hardware_inspect.py:18-22 加了缺件即 cmake configure+build 的 built() 自举（有工具链则不再 error，但门级依赖声明仍缺）；其余（test_p1004/p2001/p2002/p2003/p2006/p2007/p3003/p3004 等）仍直跑。skipUnless 全目录仅 9 处。
- 原报「三个 backend 测试」为扫描时口径，现面更大；「需运行期定性」保留（顺序/环境效应须 CI 实跑，本判只坐实静态缺依赖声明 + 缺失即 error 的代码路径）。

### [55] V18-N-14（P1/G_GOV_GATE）
- 原报标题：（P1·两套重入判据并存，只有前一套会被空值污染）selfcheck 的"我在哪一阶段"判据**两种写法分裂**
判定：**STILL**（两套判据分裂 + 恒真合取原样；一站行号已漂移到 p1drz 测试目录）
- HEAD 复算：lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp:49 仍 `if (argc >= 1 && std::getenv("ASTROCS_P1DRZ_FAULT") != nullptr)`——argc>=1 恒真、合取退化为「env 存在即注入相」，配 `ASTROCS_P1DRZ_FAULT=`（空值赋值不清位）即可让两阶段整体跳过后 PASS 的路径原样（p1drz 的 CMake 锁脚本在 base..HEAD 改过，但该判据行未动）。
- 另一套 `getenv!=nullptr` 单条件站仍在：p1cal_tests_selfcheck.cpp:39、p1cos:41、p1psf:40、p1noise:46（会被空值污染）；argv/显式相位判据站：p1hips:86、p1sess:82、p1phot:85、p1star:80/:97（A/B 双 fault）——同一类缺陷只存在于一半站点、逐个补会漏的结构原样；无统一 helper、无 AST 禁则落地（E14 仍是工单）。
- 新锚：p1drz:49、p1cal:39、p1cos:41、p1psf:40、p1noise:46、p1hips:86、p1sess:82、p1phot:85、p1star:80/:97。

### [56] V3-N-02（P2/C_DOC_CODE_GAP）
- 原报标题：plugin 面把 -14 折叠进默认域，DATA 语义在第二出口丢失
判定：**STILL**（-14 折叠原样；语义源行号漂移已订正）
- lib/drizzle/src/module_entry.cpp::drz_legacy_status :862-864（锚零漂）：domain 默认 = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION，仅 code∈[-4,-8]→DATA、-1/-2/-10→CONFIG ⇒ **-14（及 -9、-11…-13、-15+）全部落 default**——第二出口 DATA 语义丢失原样（注释自辩「DISP-DRZ 缺陷登记在案, 本迁移不消化」）。
- 语义源复算：hp_drizzle_api.cpp::-14 现在 :1006-1011（原 :992-997，文件 +338 行改动整体下移）——「precision_mode=FP64 但 data 块为 FLOAT32（累积域不一致）显式拒绝」＝数据/精度不匹配的 DATA 域错误，经 plugin 面被折叠为科学前置失败 ⇒ 上游按 PRECONDITION 重试/告警的误判面不变。
- 新锚：module_entry.cpp:857-866、hp_drizzle_api.cpp:1006-1011。

### [57] V6-N-10（P1/G_GOV_GATE）
- 原报标题：有修无账：账本 20 条的 fix_commit 全集与本轮 9 个真改代码的提交完全不相交
判定：**STILL**（fix_commit 全集与真改码提交仍不相交）
- git show HEAD:问题扫描/账本/FIX_LEDGER.jsonl 解析：fix_commit 字段值集大小仅 **6**；V6 点名的 9 个真改码提交（35c85f53/8dc0220e/b858b76d/80c32b19/b0353303/23a7f665/6d74046d/bd4bc23b/c1959436）**无一在 fix_commit 集内**（b858b76d/6d74046d 仅在别的记录 notes 文本里被提及——提及≠挂账）；有账的仍只有 c3452d48(12)/dce8abd4(6)/adaeb531(5)/07eb229b(2)。
- 工作树账本已涨到 761 条（dirty，前台并发写入中，不计为 HEAD 事实）；「每个改生产代码的提交必须在账本有行」的建议规则在 HEAD 未见制度化（账本生成/校验工具无此对账）。
- 新锚：以复算命令口径为准（git show + fix_commit 集合运算）。

### [58] V7-N-08（P?/C_ALG_IMPL）
- 原报标题：（P3）亲和性探测失败 ⇒ 静默按 1 核，**无「探测失败」判别位** ⇒ run manifest 的 `budget=1` 与"真 1 核机"同值
判定：**STILL**（无失败判别位原样；锚零漂移）
- lib/backend_host/hardware_inspect.cpp:165 仍 `sched_getaffinity(0, sizeof(set), &set);` **不查返回值**——CPU_ZERO 后调用失败即空集 → avail=0 → 下游 derive_limits_v1/worker_advisor 保守取 1；run manifest 的 budget=1 与"真 1 核机"同值、无「探测失败」判别位（grep probe_failed/fallback 0 命中）；文件不在 base..HEAD changed 列表，:83-95 的「失败→1」形态未动。
- advisor 侧 reason 串＝半记账（可解释但不可判别），与原报一致。
- 新锚：hardware_inspect.cpp:165（+原 :83-95 段）。

### [59] V8-N-08（P2/D_COMMENT）
- 原报标题：（P3·判据②）角度归一化契约**区间端点写错**：头注释 `(-90, 90]` 左开，而 `-90` 是可达返回值
判定：**STILL**（端点口径不一原样；测试路径已迁 lib/star_detector/test/，锚订正）
- lib/star_detector/src/sdet_angle_guard.h:38 头注释仍「归一化到 (-90, 90]」（左开）；实现 :56-63 `while (std::fabs(angle_deg) > 90.0)` ⇒ 输入恰 -90 不进循环、返回 -90——可达值被文本排除，机制原样（同文件 :34 的 64 次/11610° 复算仍真，原报已注明只登记端点一处）。
- 测试断言闭区间「[-90, 90]」现位于 lib/star_detector/test/sdet_angle_guard_test.cpp（原报 tests/ 路径漂移，fabs(out)<=90 判据在位）⇒ 头注释开区间 vs 测试闭区间两口径互斥未修。
- 新锚：sdet_angle_guard.h:38/:56-63、test/sdet_angle_guard_test.cpp。

### [60] V9-N-09（P1/G_GOV_GATE）
- 原报标题：（P1·机制⑤）39 道 `ctest-target` 门**不带 `--fail-if-no-tests`** ⇒ 目标改名/未构建即记 PASS，且本仓有真机直证其退出码为 0
判定：**STILL**（无 --fail-if-no-tests 原样；门数按 HEAD 复算扩大，CTEST-PHASE2-GATES 缺口在位）
- tools/quality/deep_ci_driver.py::_ctest_argv :201-213：现版只按 CI-BASELINE-001 追加 --output-junit，**无 fail-if-no-tests**（该串在 deep_ci_driver.py 与 ci/checks.json 全文件 grep 计数 = 0）；:256 `_ctest_argv(["ctest","-R","^%s$","--output-on-failure"]…)`——目标改名/未构建时 ctest 输出 "No tests were found" 而退出 0 ⇒ 记 PASS 的机制未变（真机直证 win-test-summary.json 的 exit_code=0 记录仍为既有证据）。
- 复算：kind=ctest-target 门现 **44 道**（原报 39，checks.json 在 base..HEAD 净增 5 道同形态门——暴露面扩大）；CTEST-PHASE2-GATES 条目键集（id/profiles/platform/command/…）**仍无 ctest_targets 字段** ⇒ STD-F7「逐目标登记」对它不成立。20 门零执行证据子项属运行期账目，未静态复算（维持原报）。
- 新锚：deep_ci_driver.py:201-213/:256；44 门计数；CTEST-PHASE2-GATES@checks.json:2478。

## 统计表

| 状态 | 条数 |
|---|---|
| STILL | 57 |
| FIXED | 3 |
| MOVED | 0 |
| CANNOT_STATIC | 1 |
| 待填 | 0 |
