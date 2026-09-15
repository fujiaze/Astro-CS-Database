# RC8 分片重验档案（第 8 片 / 共 8 片）

- 时点：HEAD a3a343a44080d917089e1f8d548ed2d0400b0c61（git --no-optional-locks rev-parse HEAD 实测一致）；base 521095b8。
- 子集来源：问题扫描/_cache/recheck_round1.json -> verify[7::8]（按下标取模第 8 组），共 61 条，全部命中 问题扫描/账本/FIX_LEDGER.jsonl。
- 工作区注意：docs/TRACEABILITY.csv、reports/v19r2/*、reports/v19r3/contract_inventory.csv、evidence/v6_1_rework/tasks/CHK-001/* 在工作树为脏 -> 涉及条目以 git show HEAD:<path> 为准；lib/snr_estimator/{src,include,CMakeLists.txt}、lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py、lib/phase1/tests/、tests/unit/p1_noise/ 为未跟踪新文件，HEAD 上不存在，不作为 FIXED 依据。
- 判定口径：四态 STILL / FIXED / MOVED / CANNOT_STATIC；一律按 文件::符号 重定位；FIXED 须给出修在哪的证据并判是否只修一站。
- 统计：STILL = _ / FIXED = _ / MOVED = _ / CANNOT_STATIC = _（收工时回填）。

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
- contracts/data/artifact_types.registry.json:34 仍 "type_id": "astrocs.calibrated_frame.v1"（三段），而 contracts/data/artifact_manifest.schema.json:43 pattern 仍 `^astrocs\.[a-z0-9]+\.[a-z0-9_]+\.v[0-9]+# RC8 分片重验档案（第 8 片 / 共 8 片）

- 时点：HEAD a3a343a44080d917089e1f8d548ed2d0400b0c61（git --no-optional-locks rev-parse HEAD 实测一致）；base 521095b8。
- 子集来源：问题扫描/_cache/recheck_round1.json -> verify[7::8]（按下标取模第 8 组），共 61 条，全部命中 问题扫描/账本/FIX_LEDGER.jsonl。
- 工作区注意：docs/TRACEABILITY.csv、reports/v19r2/*、reports/v19r3/contract_inventory.csv、evidence/v6_1_rework/tasks/CHK-001/* 在工作树为脏 -> 涉及条目以 git show HEAD:<path> 为准；lib/snr_estimator/{src,include,CMakeLists.txt}、lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py、lib/phase1/tests/、tests/unit/p1_noise/ 为未跟踪新文件，HEAD 上不存在，不作为 FIXED 依据。
- 判定口径：四态 STILL / FIXED / MOVED / CANNOT_STATIC；一律按 文件::符号 重定位；FIXED 须给出修在哪的证据并判是否只修一站。
- 统计：STILL = _ / FIXED = _ / MOVED = _ / CANNOT_STATIC = _（收工时回填）。

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

（四段，第二段禁下划线）。
- runtime/artifact_store/artifact_manifest_validator.py:40 与 phase_product_exchange_validator.py:54 的 _TYPE_ID_RE 同字面未变 ⇒ 登记即合法 vs 使用即拒 互斥照旧。相关文件均不在 changed 列表。
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

### [23] M5a-G-005（P0/G_GOV_GATE）— 判定：待填
- 原报标题：THREAD-BUDGET 机器门空转：扫描面、正则与行级豁免三重漏检并被单测固化

### [24] M5b-C-03（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：顶层幽灵命令 drizzle 在分发表却不在 help 与冻结命令树；未接线用 ARGS(2) 表达；诊断指向已删选项

### [25] M5b-E-04（P1/E_TRACE_BREAK）— 判定：待填
- 原报标题：acs_status/acs_head 在 legacy 与 abi 头族重复定义，其声称的机器检查文件不存在

### [26] M5b-G-06（P0/G_GOV_GATE）— 判定：待填
- 原报标题：§18.4「只加载随产品签名清单发布的官方模块」在机器上无实现：产品不加载、hash 全 null、唯一闭环测试不进 CI

### [27] M5b-G-14（P1/G_GOV_GATE）— 判定：待填
- 原报标题：导出符号与 ABI 合同一致性（§12.3-6）实际无有效检查：门只判符号表非空，Linux 侧零覆盖

### [28] M5b-I-05（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：CI 元文档的规模计数与多项路径/条目已与注册表脱节

### [29] M6a-D-001（P1/D_COMMENT）— 判定：待填
- 原报标题：生产注释承担「未登记的裁决请求」角色：SCI 冻结 1e-4 px 门被同文件注释自证一步法数学不可达；裁决与偏差 ID 只活在注释与控制包台账里，docs 权威登记面（STANDARDS_REGISTRY / PLATESOLVE §11.3）对 DISP-WCS-007、DISP-WCS-008 零命中（来源 L13-001 的注释层残余；科学定档在 M1a）

### [30] M6a-D-009（P2/D_COMMENT）— 判定：待填
- 原报标题：§12.2 禁止形态（任务号/审计轮次/commit 哈希/修复流水）成为注释主干：Phase2/3 侧六目录实测 257 处命中 + Phase1 侧抽样清单，被登记偏差条目自身的注释也在复述历史（来源 L13-015 全域 + L14-003 量化，两切片并档）

### [31] M6a-G-001（P0/G_GOV_GATE）— 判定：待填
- 原报标题：注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的判据被「冻结」二字永久豁免、每文件至多报一条、扫描面只覆盖 lib 的一部分，却恒定自报 coverage=full/1.0 —— 注释质量没有任何机器防线（来源 L14-004，根因单条定稿）

### [32] M6b-C-003（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：同一 ACTIVE 文档内冻结投影集新旧两版并存：§0 非目标仍写「不实现 SIN/ZEA/CAR/AIT」，§15 已按负责人裁决冻结 TAN/SIN/CAR/AIT

### [33] M6b-G-001（P0/G_GOV_GATE）— 判定：待填
- 原报标题：未经证据支撑的「已验证」声明（单一根因，四组实例）：八层矩阵在 TEST / EVIDENCE / SRC / 整行四个层面给出 VERIFIED，而门的结构只可能判「文件存在 + 符号 token 可见」

### [34] M6b-I-003（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：文档内统计与清单陈述与实测不符（可复算的口径失真清单）

### [35] M7-A-123（P1/A_SCI_DEF）— 判定：待填
- 原报标题：交换合同要求三 Phase 最小平面集含「mask」，而 mask 在三个 DATA 节都被判「非产品输出」且语义三方不一（L21-007）

### [36] M7-A-131（P1/A_SCI_DEF）— 判定：待填
- 原报标题：MAD=0 时热/冷检测阈值退化为「高于中位即热、低于中位即冷」，文档两次判为良性（L20-011）

### [37] M7-E-201（P2/E_TRACE_BREAK）— 判定：待填
- 原报标题：SCI/合同/注释三处「与实现一致」声明所引代码行锚整段漂移（本代理读码亲证）

### [38] M7-I-202（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：hips_frame 字面值在合同与写侧措辞不一致（L21-009 **按前台裁决降级定稿**）

### [39] M8-F-002（P0/F_TEST_GAP）— 判定：待填
- 原报标题：`tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 CHECK 的失败计数不参与退出码，`main` 无条件 `return 0`（覆盖 L23-002 与 L23 §1 的旧措辞）

### [40] M8-F-010（P1/F_TEST_GAP）— 判定：待填
- 原报标题：修复未固化：B2-A14 新增 PHOTDEGRADE / uncalibrated_adu_allowed 显式降级路径已实现并回写文档，但无任何回归用例钉住

### [41] M8a-C-001（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：已废弃第二调度器被 README 写成「正式科学运行只有一条命令」，并整套冻结第二套 `ASTROCS_*` 退出码

### [42] M8a-E-003（P2/E_TRACE_BREAK）— 判定：待填
- 原报标题：DISP-PSF-007 被 README/manifest/测试/HANDOVER 四处当偏差 ID 使用，偏差登记表只到 006

### [43] M8a-G-007（P1/G_GOV_GATE）— 判定：待填
- 原报标题：生成器自述「checker 以本生成器输出为源」不实：registry 无 diff 门，手写页与 descriptor 双向漂移（裁决：维持 P1）

### [44] M8a-I-007（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：toolchain.lock 已刷新，INVENTORY_REPORT 仍以现势口吻归因锁「九类工具全部缺失」— 修复未回写型失真

### [45] M9-D-1（P2/D_COMMENT）— 判定：待填
- 原报标题：drizzle 局部尺度推导注释把"量纲为一的切平面坐标"表述为"仍是弧度量级小值"，注释表述失准而实现结果正确

### [46] M9-G-5（P1/G_GOV_GATE）— 判定：待填
- 原报标题：IO-001 原子写提交点全程无耐久性屏障：rename 前只 fflush 不 fsync，目录项亦不 sync（掉电/崩溃可留零长度或旧内容产品）

### [47] V1-N-02（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：SNR 物理量改了但二进制版本位没改 ⇒ 新旧 `.hiss` 同标签承载不同物理量；JSON 面加了 `snr_schema` 而二进制面没加

### [48] V10-N-02（P1/C_ALG_IMPL）— 判定：待填
- 原报标题：`A_ORDER`/`AP_ORDER` 未做 `[0,5]` 校验 ⇒ SIP 求值按 order 索引 36 元素数组：`order=6` 越入相邻数组、`order=1000` 索引 6000

### [49] V11-N-01（P0/G_GOV_GATE）— 判定：待填
- 原报标题：`AioHipsSnrPoint` 镜像落后 C 头两字段 ⇒ 越界读 24 字节，且**交付 HiPS SNR 目录第 2/3 条记录是邻堆垃圾**

### [50] V11-N-09（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1）导出面白名单只覆盖 5 个模块 DLL，**被跨语言消费的 5 个 DLL 零裁剪**；Gaia 的公共 ABI 头住在 `src/` ⇒ 天然逃过任何 `include/` 面普查

### [51] V12-N-07（P2/A_SCI_DEF）— 判定：待填
- 原报标题：（P2）割线迭代 5 个兜底字面量与结构默认值**各写一遍**；`±6.0` 全仓无承载

### [52] V12-N-15（P2/A_SCI_DEF）— 判定：待填
- 原报标题：（P2）门常量 `511` 与 `512` **互为字面量、无派生、无 `static_assert`**

### [53] V13-N-06（P1/G_GOV_GATE）— 判定：待填
- 原报标题：`c3452d48` 删掉的 4 行 VERIFIED 合同行**无任何对应登记动作** ⇒ 合同面与追溯面永久分叉且恒不红

### [54] V15-N-16（P1/F_TEST_GAP）— 判定：待填
- 原报标题：（P1·需运行期定性）`UT-BACKEND` 的 command 只有 `unittest discover`、**无 build 依赖声明**，而三个 backend 测试都走 `EXE = REPO/build/astrocs` 真跑 ⇒ **干净 checkout 上"真行为断言"整体 error**

### [55] V18-N-14（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1·两套重入判据并存，只有前一套会被空值污染）selfcheck 的"我在哪一阶段"判据**两种写法分裂**

### [56] V3-N-02（P2/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：plugin 面把 -14 折叠进默认域，DATA 语义在第二出口丢失

### [57] V6-N-10（P1/G_GOV_GATE）— 判定：待填
- 原报标题：有修无账：账本 20 条的 fix_commit 全集与本轮 9 个真改代码的提交完全不相交

### [58] V7-N-08（P?/C_ALG_IMPL）— 判定：待填
- 原报标题：（P3）亲和性探测失败 ⇒ 静默按 1 核，**无「探测失败」判别位** ⇒ run manifest 的 `budget=1` 与"真 1 核机"同值

### [59] V8-N-08（P2/D_COMMENT）— 判定：待填
- 原报标题：（P3·判据②）角度归一化契约**区间端点写错**：头注释 `(-90, 90]` 左开，而 `-90` 是可达返回值

### [60] V9-N-09（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1·机制⑤）39 道 `ctest-target` 门**不带 `--fail-if-no-tests`** ⇒ 目标改名/未构建即记 PASS，且本仓有真机直证其退出码为 0

## 统计表

| 状态 | 条数 | 条目 |
|---|---|---|
| STILL | | |
| FIXED | | |
| MOVED | | |
| CANNOT_STATIC | | |