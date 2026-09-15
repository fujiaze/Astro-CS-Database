# RC3 复验档案（第 3 片 / 共 8 片）

- 时点 HEAD: a3a343a44080d917089e1f8d548ed2d0400b0c61
- 范围: 问题扫描/_cache/recheck_round1.json 的 verify[2::8]，共 62 条；逐条按 文件::符号 重定位复验（纯静态）
- 状态口径: STILL / FIXED(须给修在哪证据+他站残留判定) / MOVED / CANNOT_STATIC

## 结论表（逐条）

| # | id | 优先 | 状态 | 新锚/证据(简) |
|---|----|------|------|-----------|
| 1 | FD-F-003 | P0 | (待填) | |
| 2 | M2a-B-1 | P0 | (待填) | |
| 3 | M2b-A-01 | P0 | (待填) | |
| 4 | M2b-B-07 | P0 | (待填) | |
| 5 | M3-F-001 | P0 | (待填) | |
| 6 | M3b-F-01 | P0 | (待填) | |
| 7 | M4-A-02 | P0 | (待填) | |
| 8 | M5b-G-01 | P0 | (待填) | |
| 9 | M8a-G-001 | P0 | (待填) | |
| 10 | M9-B-1 | P0 | (待填) | |
| 11 | M9-H-2 | P0 | (待填) | |
| 12 | V11-N-04 | P0 | (待填) | |
| 13 | L28e-E-002 | P1 | (待填) | |
| 14 | M1a-A-007 | P1 | (待填) | |
| 15 | M1a-C-005 | P1 | (待填) | |
| 16 | M1a-E-002 | P1 | (待填) | |
| 17 | M2a-C-8 | P1 | (待填) | |
| 18 | M2a-E-3 | P1 | (待填) | |
| 19 | M2b-F-03 | P1 | (待填) | |
| 20 | M3-A-004 | P1 | (待填) | |
| 21 | M3-C-006 | P1 | (待填) | |
| 22 | M3b-A-07 | P1 | (待填) | |
| 23 | M4-E-01 | P1 | (待填) | |
| 24 | M4-F-07 | P1 | (待填) | |
| 25 | M5a-F-001 | P1 | (待填) | |
| 26 | M5a-G-008 | P1 | (待填) | |
| 27 | M5b-C-06 | P1 | (待填) | |
| 28 | M5b-G-09 | P1 | (待填) | |
| 29 | M5b-G-17 | P1 | (待填) | |
| 30 | M6a-D-004 | P1 | (待填) | |
| 31 | M6a-I-003 | P1 | (待填) | |
| 32 | M6b-E-003 | P1 | (待填) | |
| 33 | M6b-G-004 | P1 | (待填) | |
| 34 | M7-A-118 | P1 | (待填) | |
| 35 | M7-A-126 | P1 | (待填) | |
| 36 | M7-A-136 | P1 | (待填) | |
| 37 | M7-G-102 | P1 | (待填) | |
| 38 | M8-F-005 | P1 | (待填) | |
| 39 | M8-F-013 | P1 | (待填) | |
| 40 | M8a-C-004 | P1 | (待填) | |
| 41 | V10-N-05 | P1 | (待填) | |
| 42 | V13-N-01 | P1 | (待填) | |
| 43 | V14-N-04 | P1 | (待填) | |
| 44 | V18-N-05 | P1 | (待填) | |
| 45 | V2-N-02 | P1 | (待填) | |
| 46 | V5-N-03 | P1 | (待填) | |
| 47 | V8-N-01 | P1 | (待填) | |
| 48 | V9-N-03 | P1 | (待填) | |
| 49 | V9-N-12 | P1 | (待填) | |
| 50 | M1a-G-003 | P2 | (待填) | |
| 51 | M2a-C-13 | P2 | (待填) | |
| 52 | M2a-G-3 | P2 | (待填) | |
| 53 | M3-I-003 | P2 | (待填) | |
| 54 | M5b-I-08 | P2 | (待填) | |
| 55 | M6a-D-012 | P2 | (待填) | |
| 56 | M7-I-205 | P2 | (待填) | |
| 57 | M8a-I-002 | P2 | (待填) | |
| 58 | M9-F-3 | P2 | (待填) | |
| 59 | V1-N-05 | P2 | (待填) | |
| 60 | V12-N-02 | P2 | (待填) | |
| 61 | V12-N-10 | P2 | (待填) | |
| 62 | V7-N-03 | P2 | (待填) | |

## 逐条复验细节

### FD-F-003（P0 F_TEST_GAP）
- 标题: 唯一跑 Windows C++ 单测的门连续 10 次"0 用例 PASS"，且空输出被 `EMPTY_OUTPUT_SILENCE_EXEMPT` 制度化为豁免
- 原 position: :stage_plan:344-356`（缺"用例数>0"断言）；`ci/run.py:103-105 EMPTY_OUTPUT_SILENCE_EXEMPT`；`ci/checks.json::WIN-TEST-UNIT`、`::API-DOCS`、`::WARNING-SUPPRESSION`（其 `detect_build_tree()` 不认 `build/win-msvc-17.14.39-x64`）；对照 `deep_ci_driver.py:248`（持相反说法）
- 原 evidence: ` 元素计数 **0**。
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2a-B-1（P0 B_STD_MISMATCH）
- 标题: 星表位置历元：注册表条款面要求 J2016.0，产品面四处声称 J2000，模块零历元处理且无天测列可传播——CONFORMANT 判定与偏差登记双双失真
- 原 position: :D.catalog（CLAUSES 行 + 判定式首行）；docs/science/ASTROMETRY.md::§3 frame 表、::§10；docs/contracts/DATA_SEMANTICS.md::§8.2 输出行（坐标 frame 段 + ra/dec 行）；docs/algorithms/GAIA_QUERY.md::§1/§2 单位段；lib/gaia_xpsd_client/src/gaia_client.h::文件头坐标契约注释；lib/gaia_xpsd_client/include/astrocs/gaia/types.h::op 参数单位注释；lib/gaia_xpsd_client/integration/astrocs_catalog_gaia.integration.json::coordinate；lib/gaia_xpsd_client/README.md::§2 负责范围；lib/gaia_xpsd_client/src/gaia_client.c::record decode 段 / ::gaia_client_cone_search 结果装配段 / ::GaiaSpectr
- 原 evidence: （STANDARDS_REGISTRY.md:186）- CLAUSES: DR3 source 列面（ra/dec **参考历元 J2016.0**、phot_g_mean_mag/phot_bp_mean_mag/phot_rp_mean_mag）；本地 XPSD 记录布局（ / （STANDARDS_REGISTRY.md:193）| DR3 source 位置列（ra/dec，ICRS，参考历元 J2016.0） | 位置以 ICRS 表达，RA∈[0,360)、Dec∈[-90,90] | **CONFORMANT** | docs/algorith
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2b-A-01（P0 A_SCI_DEF）
- 标题: HiPS 写入口对 nside 零校验：非 2 的幂与 order>29 均被接受并静默产出错产品；lib/hips 的正确校验在生产调用点被绕过
- 原 position: lib/astro_image_io/src/hips/aio_hips_writer.cpp::aio_hips_product_begin（只比大小 + ilog2 向下取整） ；被绕过的正解：lib/hips/src/module_entry.cpp::hips_cfg_parse（require power-of-two + 上界） ；生产调用点 3 处：lib/core/src/module_adapters.cpp::p1_op_writer（write_hips 节点）、lib/core/src/module_adapters.cpp::p2_op_write（write_mosaic 节点）、lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp::写通道 begin；另 lib/phase2/tools/stage2.cpp（legacy 工具） ；合同：docs/contracts/DATA_SEMANTICS.md::§2 非法值拒绝 + ::DATA-P1-HIPS §12.1；lib/astro_image_io/include/aio_hips.h
- 原 evidence: if (!out_dir || !*out_dir || nside < 512 || tile_width != 512 || / (data_type != AIO_HIPS_FLOAT32 && data_type != AIO_HIPS_FLOAT64) || / (flags & ~AIO_HIPS_PRODUCT_ALL_V20) != 0) { / set_error("aio_hips_product_begin: 参数无效 (nside>=512, tile_width=512, dtype 0/1)"); / ps->leaf_order = ilog2_u64(nside
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2b-B-07（P0 B_STD_MISMATCH）
- 标题: 「order ≤ 29」被写成 Górski 标准条款，代码实际无 order 上界且 npix 在 2^31 处静默回绕
- 原 position: :require_valid_nside；lib/common/healpix/healpix_core.cpp::npix；docs/standards/STANDARDS_REGISTRY.md::D.healpix 清单行 1/3/4；docs/algorithms/HEALPIX_MAPPING.md::§2；lib/common/healpix/tests/test_healpix_neighbors.cpp::"child_nest/tile_to_leaf_nest legal roundtrip"
- 原 evidence: inline void require_valid_nside(uint32_t nside) { / if (nside == 0 || (nside & (nside - 1u)) != 0u) { / throw std::invalid_argument("healpix: nside must be a power of two (got " + std::to_string(nside) + ")");
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3-F-001（P0 F_TEST_GAP）
- 标题: SCI-NOISE §11/§15 承诺的「NumPy rtol 1e-9 复算」在仓库内不存在，唯一 Python 对拍面是容差带断言且可整类静默跳过
- 原 position: :§11 验证 Oracle / §15 Acceptance`（复核时点 :117、:140） ；`tests/backend/test_noise_model_oracle.py::模块级与 import 块`（复核时点 :17-:24 —— 仅 `json/subprocess/shutil/tempfile/struct/ctypes/unittest`，**无 numpy**） ；`同文件::KSIG/median()/rsig() 复算助手`（:28-:38 —— 定义后全文零引用） ；`同文件::TestNoiseOracle 类装饰器`（:97 `@unittest.skipUnless(shutil.which("g++") …)`） ；`同文件::各断言容差`（:128、:139、:152、:156、:168 为 `0.05*` 带；:191 fill 为 `0.40*(1/16)`；:177 唯一的 `1e-9` 只作用于 scale_law） ；对照 `ci/checks.json::UT-BACKEND`（:1345-1377，`python3 -B -m unittest discover -s 
- 原 evidence: - **Python 参考**：NumPy 对同 `data` 的 `median/MAD/5σ裁剪/平面最小二乘` 复算 `variance/ivar`（`rtol 1e-9`）。（SCI-NOISE §11:117） / - §11 Oracle 全过：Gaussian 5% 复现、Poisson 诊断 5% 交叉（仅诊断）、平面场 10% 恢复、四不变量门、**Python 参考 rtol 1e-9**；（SCI-NOISE §15:140） / import unittest / import shutil / import subprocess …（test_noise_model_
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3b-F-01（P0 F_TEST_GAP）
- 标题: 新失效形态：质心验收门按「非生产初始化配置」定义判据，生产路径质心无门且脚本零登记
- 原 position: :冻结门(:7-8)/::生产路径 offset(:248-252)/::converged 对照(:270-292)/::门判定(:294-305)/::blocker(:306-313)；同目录 evidence/gate2_result.json(:3-9,:22-28)；ci/checks.json（grep gate[0-9]_ 零命中，M3b 复测）
- 原 evidence: 冻结门 (Phase1 签字微修正): 质心误差 <= 0.01 目标像素 / # 冻结门判定 (解析真值, AstroCS; 使用收敛初始化证明拟合器精度, / # 生产路径质心偏差作为已记录 BLOCKER) / ac = results["astrocs_converged_init"] / "centroid_p95_le_0.01px_converged": ac["centroid_p95_px"] is not None and ac["centroid_p95_px"] <= 0.01,
- 账本 fix_state: PARTIAL | verified_state: VERIFIED
- **复验结论**: (待填)

### M4-A-02（P0 A_SCI_DEF）
- 标题: 冻结 SCI 内 support reducer 两口径互斥：代码取 accepted 口径并有门，§5 公式未订正、旧 ALG 仍称现状=W>0
- 原 position: 58（vs :21,63,75）；lib/phase2/src/integrate.cpp::「B2-A7: canonical reducer」注释与 sup_max 前置更新（复核时点 :44-50，输出 :76）；docs/algorithms/INTEGRATION_ALGORITHMS.md:19-21(F6/F6a),53；docs/algorithms/PHASE2_INTEGRATION.md:42,64,166-174（含 :170-171「实现现状=SCI §5:58 表述」句与 :266-268「F5 现状实现该门 FAIL」句）；tests/unit/p2_output_semantics_test.cpp:85-107（回归门，4b/4c）
- 原 evidence: support = (support 空) ? 1.0 : max_{valid,W>0} support[i]   # canonical reducer, 覆盖并集保守下界 / - **支撑单调性**：`sup_max = max(accepted support)`，增样本不减 `support`。 / // B2-A7: canonical reducer 契约 = max(**accepted** support)，作用域是 / // 通过资格门 (accepted ∧ value/支持 finite) 的全部样本，**不含**权重 / // 正性要求（w==0 合法但不贡献 sig
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5b-G-01（P0 G_GOV_GATE）
- 标题: CLI 命令树与协议门禁全部打在 COMPATIBILITY 二进制上；二进制缺失时门静默零检查
- 原 position: :（cli 子图构建步）`（现 :21-25）——`cmake -S cli -B build/cli` + `cmake --build build/cli --target astrocs` ；`tools/check_api_docs.py::Checker.check_command_tree`（现 :132-142） ；`tests/cli/test_cli_protocol.py::EXE`（现 :7-8）与 `::built`（现 :46-50） ；`cli/CMakeLists.txt::（文件头 COMPATIBILITY 声明 + add_executable(astrocs)）`（现 :1-8、:121）；`cli/CMakeLists.txt::（AIO 源集）`（现 :164-165） ；`CMakeLists.txt::astrocs`（现 :328、:588-609） ；`ci/checks.json::API-DOCS`（现 :232-247）
- 原 evidence: exe = os.path.join(self.repo, "build", "cli", "astrocs") / if os.path.isfile(exe): / missing = sorted(c for c in doc_cmds if c not in help_cmds) / （该 if 之后无 else；文件不存在时不产生任何 failure） / mkdir -p build/cli && cmake -S cli -B build/cli -DCMAKE_BUILD_TYPE=Release / timeout 2400 cmake --build build/cli -
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M8a-G-001（P0 G_GOV_GATE）
- 标题: 依赖与许可登记面三处失真：GPL 组件进入产品链接闭包、五处登记面零覆盖、派生代码挂自著作权 MIT
- 原 position: :astrocs_p1_sdet`（复核 535-546）→ `::astrocs_module_adapters`（646-649）→ `::astrocs_cli_runtime`（665-666）→ `::add_executable(astrocs)`（588-609）；`lib/star_detector/src/sdet_api.cpp::gsl includes`（28-31）；`lib/star_detector/Makefile::LDLIBS`（27）；`lib/star_detector/tests/p1star/CMakeLists.txt::自探注`（55-66）
- 原 evidence: CMakeLists.txt:545 「# sdet PSF 拟合 trust-region LM 求解器依赖 GSL（star_detector Makefile 同款 -lgsl -lgslcblas）」:546 `target_link_libraries(astrocs_ / :646-649 `target_link_libraries(astrocs_module_adapters PUBLIC … astrocs_p1_ipv astrocs_p1_sdet …)`；:665-666 `astrocs_cli_runtime PUBLIC ast / p1star/CMakeLi
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M9-B-1（P0 B_STD_MISMATCH）
- 标题: SIP 前向 A/B/AP/BP 系数以「像素域」数值直写 FITS 标准键 A_i_j/B_i_j；同一数组在合同面有四处互斥单位；桥注释引用不存在的标准文献；并发修复批次已把该错误口径复制进新的写出面
- 原 position: :extract_wcs_sip（系数生成与单位注释）、lib/core/src/module_adapters.cpp::p1_sip_write_header_frame 与 ::p1_tan_forward_reference_sip（写出面 + 消费面）、lib/orchestrator/cpp/src/orchestrator.cpp（回写面两处、回读面一处）、lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::hp_drizzle_run（键读面）、lib/photometric_calib/cpp/src/wcs_transform.cpp::skyToPixel（第二消费面）、docs/contracts/DATA_SEMANTICS.md（三行互斥）
- 原 evidence: **系数生成（像素域）**（ipv_wcs.cpp:351-352）`// A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)` / `// 单位: (像素/角秒) * (角秒/像素^(i+j)) = 1/像素^(i+j-1) (SIP 标准)
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M9-H-2（P0 H_NUMERIC）
- 标题: XPSD 自报 spectrumCount 无任何上限即用作分配步长与每星 memcpy 长度 → 堆越界读 + 数百 MB 级 OOM 放大 + 污染下游测光输入
- 原 position: :load_xpsd_file（自报值读入点）、::spec_collector_init、::spec_collector_push、::search_recursive_spectrum（源指针派生）、::gaia_client_cone_search_with_spectrum（合并/导出面）
- 原 evidence: （:26-31）`#define XPSD_MAGIC "XPSD0100"` / `#define WL_COUNT 343` / `#define STAR_STRIDE_SP (40 + (WL_COUNT + (WL_COUNT & 1)))`（=384）/ `#defi / （:1068）`else if (strncmp(p, "spectrumCount=", 14) == 0) { xf->spectrum_count = atoi(p + 14); }` —— 来自文件内 parameters 串，**无上限、无符号、无与 WL_COUNT  / （:1205-1213）`s
- 账本 fix_state: FIXED | verified_state: VERIFIED
- **复验结论**: (待填)

### V11-N-04（P0 G_GOV_GATE）
- 标题: （P0）ctypes **少传第 9 个参数** `out_status` ⇒ C 侧把未初始化栈槽当指针，**既往垃圾地址写、又据其垃圾值决定"哪些星算成功"**；现行 gate2 PSF 证据产生于这条不匹配 ABI 之后
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### L28e-E-002（P1 E_TRACE_BREAK）
- 标题: DATA_ARTIFACTS.md schema 表 15 枚数据合同 ID 在册而全仓真源零承载；校验器只验表内自洽
- 原 position: 12-16（DATA-IMG-VAR/IVAR/WEIGHT/SUPPORT/MASK-001）、:18-19（DATA-CAT-PSF-001、DATA-CAT-PHOT-001）、:25（DATA-REJ-MAP-001）、:27（DATA-P3-FITS-001）、:39（DATA-REG-001）、:48 附近的 ARTIFACTS 行与 docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md:3、docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:3（DATA-PRODUCT-PROVENANCE-001、DATA-PRODUCT-EXCHANGE-001）、docs/contracts/INDEX.yaml:637/:710（DATA-ARTIFACTS-001）；对照纪律 docs/contracts/DATA_ARTIFACTS.md:10-11、:42-48；机器面 tools/check_data_artifacts.py:2-7 与 ci/checks.json:332
- 原 evidence: docs/contracts/DATA_ARTIFACTS.md:13 → | DATA-IMG-IVAR-001 | inverse variance | f32/f64 | [H,W] | ADU⁻² | pixel | ivar=0 显式不可用 | unique | FIT / docs/contracts/DATA_ARTIFACTS.md:42-48 → 两 ID 早已在 DATA_SEMANTICS §29.5…、生产 descriptor（lib/core/src/module_adapters.cpp:498-499 与 :459/:498）、
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M1a-A-007（P1 A_SCI_DEF）
- 标题: ALG 施工伪代码两处公式级错误：极点排除条件方向反转、像素→天球方向误用 CD⁻¹
- 原 position: :G1/G2 施工规格
- 原 evidence: （PHASE3_RESAMPLE.md:54）  validate(params): frame=icrs, W,H∈[1,20000], s_out>0, **|center.Dec|≥5°**, pixfrac N/A / （PHASE3_RESAMPLE.md:22）  iwc = CD^{-1} · ((x+1)−CRPIX1, (y+1)−CRPIX2)      # deg 偏移 / （对照 SCI-P3:48）… `abs(dec)<=85°` …
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M1a-C-005（P1 C_DOC_CODE_GAP）
- 标题: ALG §3 施工伪代码与生产链步骤错位：容差挂错步骤、生产 build_sip 不含 IRLS/Huber 却按 legacy 路径描述且未注明
- 原 position: :§3 流程伪代码 与::§11.1 DISP-WCS-003；lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip；lib/plate_solve/cpp/ipv/src/ipv_sip.cpp::irls_huber_fit（legacy）
- 原 evidence: （PLATESOLVE.md:29-33）matches = kd_match(gaia, detected, tol=5.0") / trans = iter_trans_solve(...) / sip = build_sip(trans) order 2-3, **IRLS / （PLATESOLVE.md:166-170）DISP-WCS-003 双 SIP 拟合路径并存：**生产 extract_wcs_sip（TRANS 解析 A/B + 7×7 网格反变换 AP/BP…）与 legacy fit_sip IRLS+Huber…仅被 legacy  / （ipv_wcs.cpp:4
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M1a-E-002（P1 E_TRACE_BREAK）
- 标题: C ABI 注释与 ALG/SCI 引用的规范文件在仓内不存在（悬空引用族）
- 原 position: ::126；lib/plate_solve/cpp/ipv/src/ipv_solver.h:::108/:236；lib/plate_solve/cpp/ipv/REPORT.md:::10；docs/algorithms/PLATESOLVE.md（承接引用）
- 原 evidence: （ipv_api.h:126）详见 `docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md`（glob `docs/0[45]_*` → **0 命中**） / （REPORT.md:10）V4.x 旧版 16 项资产已迁移至 `lib/plate_solve_old/v4_archive/`（glob `lib/plate_solve_old/**` → **0 命中**） / （残留物实测）lib/plate_solve/cpp/ipv/ 下存在 `make_clean_err.txt`、`make_clean_out.txt`、`siril_atpmatch_b64
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2a-C-8（P1 C_DOC_CODE_GAP）
- 标题: 并发模型合同仍写"单写者"，实现自陈该前提对 match 类查询失效；lease/cancel 注入是进程级全局
- 原 position: :§4 并发模型；lib/gaia_xpsd_client/src/gaia_client.c::块缓存锁封装注释、::返回缓存指针处的所有权注释、::gaia_leased_workers/gaia_cancel_poll_fn；lib/gaia_xpsd_client/integration/astrocs_catalog_gaia.integration.json
- 原 evidence: （GAIA_QUERY.md:191）## 4. 并发模型（现状，与源码一致）；（:193-194）- 并行：`#pragma omp parallel for schedule(dynamic)` 按文件分片；每文件独立 collector 与 scratch，块缓存仅被处理该 / （gaia_client.c:300-306）/* B4-P1-2: block_cache 锁封装。合同 docs/algorithms/GAIA_QUERY.md §4 单写者约定原本依赖 "并行轴=文件…"; 但 match 类查询（gaia_client_query_sp
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2a-E-3（P1 E_TRACE_BREAK）
- 标题: DATA-GAIA-001 的两处章节引用指向错误小节，且 ctest 已注册的 TEST-GAIA-001 面未回登 module.yaml
- 原 position: :§6 DATA 合同行、::§追溯行；lib/gaia_xpsd_client/module.yaml::test_ids；docs/contracts/DATA_SEMANTICS.md::章节标题面；tests/unit/CMakeLists.txt::gaia_cat_* 注册段
- 原 evidence: （GAIA_QUERY.md:171）- DATA 合同：DATA-GAIA-001（docs/contracts/DATA_SEMANTICS.md **§6**）——输入 XPSD 目录、输出星表行单位/dtype/shape/invalid。；（:244）- DATA-GA / （DATA_SEMANTICS.md:69）## 6. precision；（:86）## 8. Gaia XPSD 星表输入与星表行（**DATA-GAIA-001**）   // 实为 §8，两处引用均错 / （module.yaml:52-53）test_ids:   - TEST-GAIA-DESIGN-
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2b-F-03（P1 F_TEST_GAP）
- 标题: SNR metadata.xml 与 properties 的产品级可解析性无判别测试：只 grep 根元素子串，且真值夹具与生产模板不同构
- 原 position: :u5_snr 段；lib/astro_image_io/tests/p1hips/p1hips_tests_properties.cpp（std::to_string 双侧比较段）；lib/phase2/tools/controlled_rejection_truth.py::snr metadata 夹具；docs/algorithms/HIPS_WRITER.md::§9「MOC/properties 键值精确相等」
- 原 evidence: <VOTABLE version="1.3"/>\')」——自闭合空表，与生产模板（含 FIELD 列表与命名空间）不同构
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3-A-004（P1 A_SCI_DEF）
- 标题: SCI-NOISE 符号表把 `ivar` 单位写成 `pixel⁻²·ADU⁻²`，与同文 §3/§7 及 DATA/GLOSSARY 冲突
- 原 position: :§2 符号表 ivar 行`（复核时点 :16）对照 `同文 §3 :29`、`§7 :76`、`§9a :99`；`docs/GLOSSARY.md::ivar 行`（:11）
- 原 evidence: | `ivar` | `1/variance` (pixel⁻²·ADU⁻²) | `variance_bg_global` 倒数 / `fill` |（NOISE_MODEL:16） / - `x, σ_bg, √variance`: ADU…；`variance`: ADU²；`ivar`: ADU⁻² …（NOISE_MODEL:29） / - **量纲一致**：`variance` [ADU²] → `ivar` [ADU⁻²] 倒数关系精确…（NOISE_MODEL:76） / | ivar | 逆方差=1/variance… | ADU⁻² |（GLOSSARY:11）
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3-C-006（P1 C_DOC_CODE_GAP）
- 标题: 公共符号数量在 ALG/README 写 14、头文件与构建注释写 12（实测 12 个 `AC_API`）
- 原 position: :§1 / §13 / 附录`（复核时点 :16、:207、:415）、`lib/calibration/README.md::§API 面`（:63）对照 `lib/calibration/include/astro_calibration.h`（AC_API 声明实测 12 处：:38/:47/:58/:79/:102/:122/:128/:134/:140/:147/:158/:161；文件头注释 :9「12 个 legacy 符号」）、`lib/calibration/CMakeLists.txt:::8`（「legacy 12 个 AC_API 符号」）
- 原 evidence: 唯一公共头，14 个 …（ALG-CAL:16）；14 个 `AC_API`（ALG-CAL:207）；14 符号（ALG-CAL:415） / （14 AC_API 符号）（README:63） / 头 :9 注释「…12 个 legacy 符号」；构建注释「legacy 12 个 AC_API 符号」
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3b-A-07（P1 A_SCI_DEF）
- 标题: mag 实现统一为 box_sum，文档与偏差登记仍描述已消失的饱和星公式；零点/误差全未定义
- 原 position: :mag 分支(:2283,:2291-2313)/::StarRecord.mag 注释(:238)；docs/algorithms/STAR_DETECTION_ALGORITHMS.md::§2(:71-76)/§11.3(:244-246 DISP-STAR-004)；docs/science/STAR_DETECTION.md::§2(:32-34)；docs/contracts/DATA_SEMANTICS.md::§17.2(:669)；lib/star_detector/tests/p1star/p1star_tests_core.cpp::mag 弱哨兵(:472-474)
- 原 evidence: 饱和星（:2175）: mag = −2.5·log10(A_fit)（A>0），失败=NaN（量纲差异登记 DISP-STAR-003）。 / { / 0.0) ? -2.5f * log10f((float)box_sum) : NAN; / float mag;           // -2.5*log10(A)，拟合失败时为NaN
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M4-E-01（P1 E_TRACE_BREAK）
- 标题: PHASE2_COVERAGE.md 行数/符号锚全面漂移（「实测」自称失准），SCI §5/§12 一致性锚亦过期（根因条目：PHASE2_* 冻结锚整体漂移实例之一）
- 原 position: 10-11、§11.1 锚表（:253-255 复核时点：inspect_frame :59-140/build :144-231/free :233-237）、:48-51；对照 lib/phase2/src/coverage.cpp 现 280 行（实测 inspect_frame :60-169、p2_coverage_build :173-272、p2_coverage_free :274-278）、coverage.h 现 65 行；docs/science/PHASE2_UPM.md:67,120；docs/algorithms/UPM_SOLVER.md:27（自注锚漂移而 SCI 未改）
- 原 evidence: 唯一权威生产源: lib/phase2/src/coverage.cpp（239 行实测）+ 唯一权威签名头 lib/phase2/include/astro/phase2/coverage.h（60 行）；禁止手抄他版。 / 与 `lib/phase2/src/upm.cpp:6-493,1107-1123`、`sampler.cpp:250-364,672`、`aio_upm.cpp:4` 一致。
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M4-F-07（P1 F_TEST_GAP）
- 标题: p2_rejection_test/p2_output_semantics 含空断言与名不副实节（枚举值大小序/局部字面量非空/全零求和充当语义门）
- 原 position: :§4（复核时点 :60-72）、§5（:74-83）、§6（:85-90）；tests/unit/p2_output_semantics_test.cpp::§1（:19-32）、§5（:132-137）、打印句 :140
- 原 evidence: // 6) integration 语义: mean/weighted mean/variance/support + frame identity / // 语义验证: reason 输出与 frame_id 解耦 (kernel 不知 frame_id) / CHECK(true); / for (const char* name : {“signal”, “support”, “ivar”, “variance”}) { / CHECK(name[0] != ‘\0’);
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5a-F-001（P1 F_TEST_GAP）
- 标题: SCI-ACR-EQUIV-001 声明的 TST-ACR-* 无实体，权威八层矩阵亦无该 SCI 行
- 原 position: :§13 追溯与测试`（:110-115） ；`docs/algorithms/ACR_EQUIVALENCE.md::§10`（:70 `- TST: TST-ACR-* 等价/分块/回退`） ；`docs/TRACEABILITY.csv:68`（旧表：SCI-ACR-EQUIV-001 的 TEST 列填 `TST-ACR-001`、状态 `VERIFIED`） ；检索面：`docs/traceability/`（八层权威矩阵）与 `tests/**`、`ci/checks.json` ；相关测试实存但未注册：`tests/api/test_reject_integration_oracle.py`（:75 `@unittest.skipUnless(g++ and build/linux-openmp-on/libphase2.a)`）、`lib/phase2/tests/synthetic_gate.cpp`（:3023 等 6 处 `register_phase2_acr_kernels()`）
- 原 evidence: - 测试: `TST-ACR-001` CPU/GPU等价、`TST-ACR-INV-001` 分块不变量、`TST-ACR-FAIL-001` 极端回退（新增/映射见 `docs/TRACEABILITY.csv`） / - TST: TST-ACR-* 等价/分块/回退 / SCI-ACR-EQUIV-001,science,ACR CPU/GPU/混合分块等价与回退科学门,…,lib/phase2/src/acr_kernels.cpp,kOpMosaicReject; mosaic_reject_legacy,TST-ACR-001,…,VERI
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5a-G-008（P1 G_GOV_GATE）
- 标题: ACR 休眠的机器证明不成立：符号门在 CI 恒被跳过、注册表判据永不触发、可达性只跑 selftest
- 原 position: :check`（二进制面 :19-22 与 :67-71；符号判据 :70；注册表面 :73-76） ；`ci/checks.json::ACR-DORMANT`（:137-147）、`::PROD-REACH-SELFTEST`（:669-679，命令带 `--selftest`） ；`ci/steps/linux_build_root_graph.sh`（:13-14、:22-25 —— CI 实际构建落点） ；`tools/quality/check_prod_reachability.py::ACR_SYMBOLS`（:50）与真实 `scan()`（无 CI 入口） ；`tools/quality/contracts/check_execution_contracts.py::4) 无 ACR 生产接入`（:86-97） ；`CMakeLists.txt::option(ASTROCS_ENABLE_ACR ... OFF)`、`cli/CMakeLists.txt`（ACR 文件不编入生产）
- 原 evidence: bin_path = REPO / "build" / "root-cmake" / "astrocs" / if bin_path.exists(): / out = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True).stdout.lower() / if "acr" in out: / if "acr" in txt.lower() and "register_phase2_acr" in txt: / if "acr" in txt.lower() and "register_phase2_acr"
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5b-C-06（P1 C_DOC_CODE_GAP）
- 标题: run manifest 的 provider 字段为常量 "baseline"、started/finished 取同一秒，provenance 与墙钟交叉核对失效
- 原 position: :（4 处常量 provider）`（现 :4214、:4384、:5128、:5333；对照 :5213 唯一读实际值处）；`lib/core/src/module_adapters.cpp::（P1 节点 manifest 身份）`（现 :5471-5479 段与 module_build_id 拼接处）；`cli/commands.cpp::write_run_manifest`（现 :430-431）；`cli/jsonl.h::iso8601_utc_now`（现 :19-32，秒级）；`cli/commands.cpp::build_run_provenance`（现 :340-401）
- 原 evidence: {"provider", "baseline"}}; / {"provider", wr.value("provider", std::string())}}; / {"started_utc", astrocs::iso8601_utc_now()}, / {"finished_utc", astrocs::iso8601_utc_now()},
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5b-G-09（P1 G_GOV_GATE）
- 标题: ARCHIVED 约束文件被 L0 与 README 列为 ACTIVE_NORMATIVE 根权威，字母条款号在宪章中不存在
- 原 position: :权威表`（现 :93）；`::文件头 权威`（现 :8）；`docs/owner/PIPELINE_OVERVIEW.md::文件头 权威`（现 :8）；`README.md::权威与当前状态`（现 :9、:26、:31、:35-36、:64）；`AstroCS_ENGINEERING_CONSTRAINTS.md::文件头/§D.6`（现 :10、:21、:76、:82-84）；对照 `ASTROCS_PROJECT_CONSTITUTION.md::§1.1/§18 supersession`（现 :15-28、:660）与宪章章节清单（§1-§19 全为数字节，无字母节）
- 原 evidence: | 工程约束（根权威） | `AstroCS_ENGINEERING_CONSTRAINTS.md` | ACTIVE_NORMATIVE | / > 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §B/C/D/F、`docs/contracts/ARCH-001.md`、 / > **AstroCS 项目唯一根冻结约束**（Agent 与人工提交的硬门）。GOV-001 状态：**ACTIVE_NORMATIVE**（根权威，全部 Agent 入口必读）。 / supersession 与冲突消解：自本文冻结起，`AstroCS_ENGINEERING_CO
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5b-G-17（P1 G_GOV_GATE）
- 标题: L0 顶层文档自述「当前提交实测绿」，同仓库 CI 基线自证七项以上非豁免门禁仍为红灯
- 原 position: :1 一句话结论`（现 :20）；`REVIEW.md::6 机器验证入口`（现 :115-118）；`docs/owner/RELEASE_STATUS.md::1 一句话结论`（现 :40）、`::6 发布 Gate`（现 :78-84）；`ci/known_failures.json::contract.excluded_by_policy`（现 :39）与 `::base_commit/generated_utc/failures[UT-CLI]`（现 :41-42、:44-66）；`ci/checks.json`（各红灯项 `waivable:false`）
- 原 evidence: 薄命令面均已落地（源码 + 当前提交实测绿）；遗留 run --phases 连跑已删除。 / `tools/check_l0_docs.py`（DOC-002）；本轮全部 PASS（日志 `run/docconv001/logs/`）。 / "excluded_by_policy": "……本轮 linux-main 其余红灯（AGENTS-GOV/DATA-ARTIFACTS/THREAD-BUDGET/CON-COMMENTS/CON-FULL-INTEGRATION/UT-ARCH/UT-BACKEND/WIN / "check_id": "UT-CLI", … "expected":
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M6a-D-004（P1 D_COMMENT）
- 标题: 公共头把「不影响 FP64 全链路精度」写成结论性宣称，其依据（仅 orchestrator 旧通道不调用）已被两条已接线的 FP64 生产路径推翻；同头另用控制包简写锚指节（来源 L13-009）
- 原 position: :FP64 ABI 段`（复核时点 :110-119，含「(这些函数用于 master 帧预生成与坏点修复, orchestrator 的 run_stage_calibrate 不调用它们, 因此不影响 FP64 全链路精度。)」与 :112 的「R10」历史任务号；:8 同类） ；反证一：`lib/cosmetic/src/module_entry.cpp::is_f64 分支`（复核时点 :800-808 实调 `ac_correct_frame_f64`，同文件 :13 自述 1:1 转发 legacy AC_API） ；反证二：`lib/calibration/src/module_entry.cpp`（复核时点 :970/:975/:979/:1004/:1026 实调 `ac_generate_master_flat_f64/bias_f64/dark_f64` 与 `ac_calibrate_frame_f64`、`ac_correct_frame_f64`） ；实现面：`lib/calibration/src/ac_api.cpp`（复核时点 :141-147、:232-267；:241 注释「坏点修复 (
- 原 evidence: （astro_calibration.h:112-118）双精度 ABI 改造 (R10): FP64 模式下全链路使用 double, 不降级到 float32。…统计/mask 操作, 内部将 double 输入转 float 调用 f32 实现, 输出转回 double。( / （cosmetic/src/module_entry.cpp:802）rc = ac_correct_frame_f64( / （ac_api.cpp:241）// 坏点修复 (mask 操作): 转 float 调用 f32 实现, 输出转回 double
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M6a-I-003（P1 I_DOC_HYGIENE）
- 标题: 活动 README/模块状态面的「N/N PASS」与「全 PASS」宣称无仓内可核锚；本次复核另发现被指的测试脚本在 CI 面根本收集不到用例（来源 L14-014，含新证据）
- 原 position: :文件表/运行测试`（复核时点 :12「tests/abi/test_secure_loader.py | 全部正/负场景编排(36 checks)」、:39「python3 tests/abi/test_secure_loader.py   # 退出码 0 = 36/36 PASS」） ；`tests/abi/test_secure_loader.py`（复核时点 :37/:49/:495-496 `CHECKS_TOTAL` 运行时自增、`FAILURES` 非空即 exit 1；全文件 grep `unittest\|TestCase\|def test_` **零命中**，仅 `if __name__ == "__main__"` 触发） ；`ci/checks.json::UT-ABI`（复核时点 :1642-1658 命令 = `python3 -B -m unittest discover -s tests/abi -t tests/abi`） ；`docs/architecture/abi/ABI_003_SECURE_LOADER.md::§6 测试清单`（:83「36 checks」同数） ；同类：`li
- 原 evidence: （README.md:39）python3 tests/abi/test_secure_loader.py   # 退出码 0 = 36/36 PASS / （UT-ABI 命令）["python3","-B","-m","unittest","discover","-s","tests/abi","-t","tests/abi"] / （lib/hips/memory.md:109）五项自检 rc（run/local/agent_p1_hips_doc/ 日志在案，2026-09-07 实测）： / （lib/hips/memory.md:199）adapter 9 case 全 PASS（
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M6b-E-003（P1 E_TRACE_BREAK）
- 标题: 追溯 ID 无单一登记处：矩阵 152 个非占位 ID 中 85 个不在 contracts/INDEX.yaml（其中 VERIFIED 合同层 33 处 / 去重 28 个 ID），descriptor 侧另有 93 个 ID 中 72 个不在 INDEX
- 原 position: :modules[*].{science_id,algorithm_id,data_id,api_id,arch_id,src_id,test_id,evidence_id}` ；`docs/contracts/INDEX.yaml::entries` ；`lib/core/src/module_adapters.cpp::descriptor 注册（d.module_id / d.sci_id / d.alg_id / d.data_id / d.api_id / d.test_id）` ；`docs/traceability/TRACEABILITY_SPEC.md::§3 ID 命名与唯一性`（现 :83-:93） ；`docs/modules/registry/astrocs.phase2.resample.md::§（合同 ID 行）`（现 :13-20） ；越界形态：`docs/traceability/TRACEABILITY_MATRIX.json::（MOD-astrocs-phase1-star-psf 行 science_doc）`（现 :246-247）
- 原 evidence: ` 条目 = **100 个**；矩阵 ID 中不在 INDEX 者 = **85 个**（56%）。 ；其中位于 `VERIFIED` 合同层（SCI/ALG/DATA/API/ARCH 且该行该层状态=VERIFIED）者 = **33 处**（按 层×行 计），去重后为 **28 个不同 ID**，含 `API-ABI-001`、`DATA-HIPS-001`、`DATA-P1-CAL`、`
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M6b-G-004（P1 G_GOV_GATE）
- 标题: 追溯与文档门的有效性缺陷集合：三条追溯门无一能证伪 VERIFIED，锚门可被 git 异常整体跳过，豁免与不可豁免类别均无执行体
- 原 position: :main return`（现 :159，`broken`/`sym_broken` 收集后仍无条件 `return 0`）、`ci/checks.json::TRACEABILITY`（现 :37-62，`mutates_workspace: true`） ；`tools/check_traceability.py::DEFAULT_TABLES`（现 :16 = `artifacts/prerelease_v5/tables/TRACEABILITY.csv`）、`ci/checks.json::TRACEABILITY-CODE`（现 :64-87） ；`tools/quality/contracts/check_test_contracts.py::伞形豁免`（现 :36-43） ；`docs/algorithms/anchors/check_doc_line_anchors.py::git_ls`（现 :58-62，无 timeout、失败返回 None）、`::main`（现 :194、:206、:234）、`::C4 符号绑定`（现 :269-276）、`::C5 豁免续存`（现 :251-253） ；`doc
- 原 evidence: （quality/check_traceability.py:159）return 0        ← 无论 broken/sym_broken 是否为空 / （tools/check_traceability.py:16）DEFAULT_TABLES = ["artifacts/prerelease_v5/tables/TRACEABILITY.csv"] / （check_test_contracts.py:36-43）if "synthetic_gate" in tf or "TST-" in str(tids):  if (repo / "lib/phase2/tests/synth
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M7-A-118（P1 A_SCI_DEF）
- 标题: SCI-PSF 的 FWHM 冻结常数 1.230310 与解析值 1.2303076 在第 6 位分歧，三条合同语句不能同真（L19-025）
- 原 position: §2/§5/§7/§10/§11`（复核时 :20/:51/:63/:86/:118，grep 1.230310 五处命中）
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M7-A-126（P1 A_SCI_DEF）
- 标题: ALG 中的 σ 未逐法定义：linear_fit 以平均绝对残差充当 σ 且未乘 √(π/2)（L20-016）
- 原 position: §/linear_fit 块`（:14/:17/:98/:106）
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M7-A-136（P1 A_SCI_DEF）
- 标题: PERCENTILE 方法既无分位数也不含噪声尺度，拒绝强度随背景绝对电平线性变化；「全接受」兜底是其症状（L20-015）
- 原 position: §/percentile 块`（:14/:99）、`docs/algorithms/REJECTION_ALGORITHMS.md::§/F14`
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M7-G-102（P1 G_GOV_GATE）
- 标题: ALG 自行出具合规结论：「冻结现状实现为合同基线；五项偏差不构成合同违反」+「无差异项（核对通过）」（L20-020）
- 原 position: §13`（复核时 :307）；`docs/algorithms/DRIZZLE_GEOMETRY.md::§10`（:223「无差异项（核对通过）…HP_CIRCUMRADIUS_FACTOR=1.25」）
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M8-F-005（P1 F_TEST_GAP）
- 标题: 60 个 C/C++ 测试源零注册；而唯一针对注册面的门方向相反（只审「已注册目标是否登记」），此类缺口对 CI 永久隐形
- 原 position: :collect_real`、`::ADD_TEST_NAME_RE`、契约块 C1-C6/S1-S7（复核时点 :8-31）；注册面 `ci/checks.json::CTEST-REGISTRATION`；`tests/cpu/baseline/run_provider_oracle_checks.py` 文档块（:12-13）
- 原 evidence: （check_ctest_registration.py 契约）C1 扫描活动 CTest 面全部 CMake 源，解析 add_test(NAME <target> ...) ... C3 未覆盖目标 → FAIL ... C5 基线漂移：ctest_baseline.json / （同文件 :10）出现非 NAME 形式 add_test 亦判违规 / （run_provider_oracle_checks.py:12-13）provider query/self_test/export ABI 完整: 由 handshake/capability 测试 (C) 与 provider_so
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M8-F-013（P1 F_TEST_GAP）
- 标题: k_corr=1.4 的「项目自产 MC 证据」与 B4-22 原子修复的唯一回归守护均为构建孤儿；文档仍以 .exe 现状口吻延续
- 原 position: 52 `# k_corr = 1.4 保守冻结 (MC 实测 1.3883, control_median_mc_test, pixfrac=0.8, 2000次)`、:94「SparseEqualsDense 1e-12 等价门」）、`docs/algorithms/UPM_SOLVER.md` 同句、`lib/phase2/src/sampler.cpp:79/:85`（UPMW-005 引用）、`lib/phase2/include/astro/phase2/sampler.h:51`；证据 TU `lib/healpix_db/healpix_drizzle/tests/{control_median_mc_test.cpp, kcorr_matrix_test.cpp, concurrency_cache_test.cpp, variance_propagation_test.cpp}`；文档引用面 `docs/TROUBLESHOOTING.md`（variance_propagation_test.exe）、`docs/DEVELOPER_GUIDE.md:25`、`docs/algorithms/DRIZZL
- 原 evidence: （control_median_mc_test.cpp:21）-o control_median_mc_test.exe control_median_mc_test.cpp   ← 全文件唯一"构建方式"=手敲命令行注释 / （concurrency_cache_test.cpp 头注释）-o concurrency_cache_test.exe concurrency_cache_test.cpp / （grep 实测）四名在 CMakeLists 非注释行、ci/**、Makefile、*.sh、*.ps1 命中均为 0；仅有的非文档命中面是 legacy 脚本 tools/qualit
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M8a-C-004（P1 C_DOC_CODE_GAP）
- 标题: lib/healpix_db README 系列把生产真源定性为「独立仓库本地副本、.gitignore 忽略」，与根构建图、安装清单和 .gitignore 三重冲突
- 原 position: :GitHub 仓库/模块表/关联仓库表/依赖`（复核 7-10、14-21、31-37、45）；`lib/healpix_db/healpix_drizzle/README.md::GitHub 仓库地址/OpenMP 16 线程`（复核 11、58-59）；对照 `CMakeLists.txt::astrocs_drizzle`（408-417）、`::astrocs_p1_drizzle`（179-184 区段）、`cmake/install_layout.cmake`（104-105）、`.gitignore`（全文 132 行）
- 原 evidence: README:18-19 「`healpix_stack/` … 稀疏 HEALPix 堆栈存储（**独立仓库本地副本，.gitignore 忽略**）| C++ | 活跃（独立仓库）」「`healpix_drizzle/` …（独立仓库本地副本，.gitignore 忽略）|  / README:7-10 「仓库地址：https://github.com/fujiaze/Healpix-Database」；:31-37 关联仓库表把三个 GitHub 仓列为职责主体；:45 依赖指向另一外部仓
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V10-N-05（P1 C_ALG_IMPL）
- 标题: HiPS SNR TSV 坏行**只 ++ 一个全仓无消费者的计数器** ⇒ 交付星表少行完全不可观测；`%lf` 还接受 NaN/inf
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V13-N-01（P1 G_GOV_GATE）
- 标题: MATRIX 权威侧自铸 3 枚 `EVID`，其 VERIFIED 在结构上不可被证伪 ⇒ 双视图共 5 行假 evidence
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V14-N-04（P1 G_GOV_GATE）
- 标题: runtime 出厂的 `kernel_id`/`sci_contract_id` 三类失登记 ⇒ **按 kernel_id 追溯必断链**（A-36 家族新实例）
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V18-N-05（P1 G_GOV_GATE）
- 标题: （P1·判据取自被检物自身）`check_log_contract.py selfcheck():301-306` **用被测生产实现**（`from runtime.logging.log_event import LogEvent`）造样本，再由自己的 `validate_jsonl_line` 判合法；**函数体内负例数 0**（检索 `bad_`/`invalid`/负例/必拒/`expect_fail`/`neg` → `[]
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V2-N-02（P1 G_GOV_GATE）
- 标题: 已删函数仍以 VERIFIED 留在 API 合同表而新公共函数未登记 ⇒ CON-API-CONTRACTS 在干净检出必红
- 原 position: 371`（仍以 **VERIFIED** 登记已删除的 `estimate_mag_lim_by_density`）、`api_inventory.csv:376` 同；新公共函数 `estimate_mag_lim_iterative` **未登记**。
- 原 evidence: `headers 60 / symbols 439 / rows 422 → missing 恰 1 条 = 该符号` ⇒ 非豁免门 **CON-API-CONTRACTS**（`fast` + `linux-main` + `windows-main` 三 profile）在干净检出上**必红**。
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V5-N-03（P1 H_NUMERIC）
- 标题: 畸形 `magnitudeRange` 经裸 `atof` 进入新剪枝谓词 ⇒ 整文件 100% 静默漏星；该字段在剪枝引入前完全惰性
- 原 position: ::1201-1203`（`magnitude_low/high` 由裸 `atof` 解析，仅校验"XML 逗号解析成功"）→ 谓词 `:2054`（`has_magnitude_range && magnitude_low > mag_high + 0.25` 则**整文件跳过**）→ 叶过滤 `:1556-1559`
- 原 evidence: high` 颠倒、或 `nan`），谓词在**无任何校验**的情况下成立 ⇒ 该 shard 全部参考星消失，**不报错、不改退出码、不进日志**。
- 账本 fix_state: FIXED | verified_state: 
- **复验结论**: (待填)

### V8-N-01（P1 D_COMMENT）
- 标题: （P1·判据②锚无宿主）死码保留的"防清理凭据"是一条**不存在的测试符号** ⇒ 保留决定无凭据
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V9-N-03（P1 G_GOV_GATE）
- 标题: （P1）`TRACEABILITY_MATRIX.csv` 的 **BOM 在 HEAD blob 里**（非本机脏），而 checker 用 `encoding="utf-8"` 打开 ⇒ 干净检出即红
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V9-N-12（P1 G_GOV_GATE）
- 标题: （P1·**直接推翻"定向复跑全绿"这条自证手段**）`impact_map.json` 只触达 71/130 门 ⇒ **74 门既不在任何 rule 也不在 fallback**，其中含**当前必红的 `DOC-LINE-ANCHORS`**
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M1a-G-003（P2 G_GOV_GATE）
- 标题: 同一校验条件在请求层/会话层/描述符层内联重复，未接单一机器源
- 原 position: :（`\|dec\| > 85` 内联）；lib/phase3_session/p3_wcs.cpp::p3_wcs_validate_descriptor（同条件第二处）；lib/core/src/module_adapters.cpp::（节点参数第三处）；contracts/schemas/::（应为单一机器源而缺）
- 原 evidence: （p3_session.cpp:107）if (std::fabs(dec) > 85.0) { s->last_error = "abs(center.dec_deg) must be <= 85° (TAN pole excluded)"; return ACS_ERR_PA / （p3_wcs.cpp 校验段）同一 |dec|>85 判定 + proj!="TAN" 判定（独立第二处） / （registry/SCI）\|dec\|<=85 与 FOV<=20 的权威源为 SCI-P3 §4/§9a-12（合同文本，非机器源）
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2a-C-13（P2 C_DOC_CODE_GAP）
- 标题: gaia_client_get_spectrum_params 缺句柄空指针防护，是同族查询符号里唯一不设防者，且合同未声明该差异
- 原 position: :gaia_client_get_spectrum_params；对照 ::gaia_client_get_db_type / ::gaia_client_get_file_count / ::gaia_client_get_total_sources / ::gaia_client_query_spectrum_by_coords；lib/gaia_xpsd_client/README.md::§6 inspect 面；docs/algorithms/GAIA_QUERY.md::§3.1 inspect 段
- 原 evidence: （gaia_client.c:2425-2427）int gaia_client_get_spectrum_params(GaiaClient *client, int *out_start_nm, int *out_step_nm, int *out_count) { for  / （对照 :2001/:2006/:2011/:1722/:1785）if (!client) return GAIA_DB_AUTO; / if (!client) return 0; / if (!client) return NULL; / if (!client) retu / （GAIA_Q
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M2a-G-3（P2 G_GOV_GATE）
- 标题: 模块版本 / ABI 版本 / 产品 VERSION 三处共用同一字面量 0.11.0-alpha.2，违 §16.1 版本命名空间分离
- 原 position: :module_version；lib/gaia_xpsd_client/include/astrocs/gaia/types.h::ASTROCS_GAIA_VERSION 与 ::ASTROCS_GAIA_ABI_VERSION
- 原 evidence: （VERSION）0.11.0-alpha.2 / （module.yaml:15）module_version: 0.11.0-alpha.2 / （types.h:30-31）#define ASTROCS_GAIA_VERSION     "0.11.0-alpha.2" / #define ASTROCS_GAIA_ABI_VERSION 1u / （宪章 §16.1:614-616区段）… module version、ABI version、schema version 不与产品版本混用。
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M3-I-003（P2 I_DOC_HYGIENE）
- 标题: `lib/photometric_calib/docs/algorithm.md` 以「状态：已实现」呈现，且其两处核心事实被 ALG 判为失实
- 原 position: :文档头`（复核时点 :4、:6）、`::§1.1 目标`（:14）、`::§3 网格`（:107）、`::§9 参数表`（:346）；对照 `docs/algorithms/PHOTOMETRIC_FIT.md::§13.2`（:143-144、:161-163）、`lib/photometric_calib/cpp/src/spectrum_integrator.cpp::F_syn 网格步长`（:246-247）
- 原 evidence: > 文档版本: 1.0 / > 状态: 已实现（algorithm.md:4/:6） / 对单帧天文图像进行全局流量校准，消除空间缓变梯度（残留渐晕、月光、光害、大气消光、大气辉光）…（algorithm.md:14） / 波长步长 0.1nm …（algorithm.md:107 表格；:346 参数表同） / - F_syn 网格为**1.0nm**（spectrum_integrator.cpp:247-255），旧 lib/photometric_calib/docs/algorithm.md:107/:346 "0.1nm" 失实。（ALG-PHOT:143-144） / const
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M5b-I-08（P2 I_DOC_HYGIENE）
- 标题: 版本命名空间文档自述版本与正文互斥，且把无消费者的"扫描器"写成权威依据
- 原 position: :（文首版本行与"本文档 = 1"）`（现 :8、:22）；`::（检查器依据）`；`tools/doccheck/check_version_namespaces.py::SCAN_FILES/SCAN_DIRS`（现 :36-43）；`ci/checks.json::VERSION-NAMESPACES`（现 :1031-1053）；`docs/owner/RELEASE_STATUS.md::（版本单源）`（现 :66）
- 原 evidence: - 版本: 2 / （本文档 = 1）
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M6a-D-012（P2 D_COMMENT）
- 标题: registry 声明的 per-projection 守卫字段无任何强制点：注释称「中心 |dec| 守卫/合法 FOV 声明」，实现读文件级常量且 FOV 字段零消费，selfcheck 只断言「>0」（来源 L14-010）
- 原 position: :P3ProjectionSpec`（复核时点 :44-47 六要素声明「max_abs_crval_dec_deg（适用天区/极点奇点守卫）、max_fov_deg（合法 FOV 声明…）」、:55-56 字段注释「中心 |dec| 守卫（单一条件，四投影统一 85.0 保守冻结）」、:87-88 make 校验序注释）；`lib/phase3_proj/p3_projection.cpp::kMaxAbsDec`（:38）、`::p3_projection_make`（:328 `if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3_PROJ_PARAM;`）、`::kRegistry`（:266-275 四行的 85.0 与 max_fov_deg=20/60/180/360）、`::p3_projection_registry_selfcheck`（:306-307 仅 `> 0.0`）
- 原 evidence: （h:55）double max_abs_crval_dec_deg; // 中心 |dec| 守卫（单一条件，四投影统一 85.0 保守冻结） / （cpp:328）if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3ProjectionStatus::P3_PROJ_PARAM;   ← 读文件级常量，不读 spec 字段 / （cpp:306-307）if (!(kRegistry[i].max_abs_crval_dec_deg > 0.0) || !(kRegistry[i].max_fov_deg > 0.0)) return 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M7-I-205（P2 I_DOC_HYGIENE）
- 标题: 符号 K 在唯一词典内三义并存；electron 许可域标注与四文档现状互斥（L21-014）
- 原 position: K 行`（复核时 :21/:25）、`docs/science/CALIBRATION.md::K=t_light/t_dark`、`docs/science/NOISE_MODEL.md::K`、`docs/contracts/DATA_SEMANTICS.md::electron`
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M8a-I-002（P2 I_DOC_HYGIENE）
- 标题: 门的全覆盖样本恰是最薄样本：5 份 README 缺负责人 8 要素中的 4 项且合同锚写法错误
- 原 position: `lib/phase1/wcs/README.md`（10 行）、`lib/phase1/noise/README.md`（11）、`lib/phase1/photometry/README.md`（11）、`lib/phase1/stars/README.md`（15）、`lib/phase3_session/README.md`（15）
- 原 evidence: wcs README 全文要点：「# phase1/wcs — WCS TAN (L2 模块 README) / 合同: `P1-004` / `SCI-WCS-001` (docs/contracts/INDEX.yaml) / Header: … / Source: … / 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### M9-F-3（P2 F_TEST_GAP）
- 标题: provenance 双键的 pixfrac 半有界、scale 半无界；在册断言把亚弧秒取值钉成合法基线，生产真实值域（0.1–6.3″）无任何代表值被校验
- 原 position: :aio_hips_set_drizzle_provenance；声明面 lib/astro_image_io/include/aio_hips.h:238-243；断言面 lib/astro_image_io/tests/p1hips/p1hips_tests_units.cpp、::p1hips_tests_negative.cpp、::p1hips_tests_properties.cpp、lib/healpix_db/healpix_drizzle/tests/drizzle_science_matrix_test.cpp
- 原 evidence: 校验体（aio_hips_writer.cpp:1281-1282）`if (!(pixfrac > 0.0 && pixfrac <= 1.0)) return 2;` / `if (scale_arcsec < 0.0) return 2;` —— pixfrac 侧是**区 / 声明面（aio_hips.h:238-243）只述用途（`设置 Drizzle provenance（pixfrac / 像素角尺度）… Phase2 sampler 按帧读取以选择 control-ivar 的 k_co
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V1-N-05（P2 C_DOC_CODE_GAP）
- 标题: DATA-P1-FLUX 单位三口径未收敛
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V12-N-02（P2 A_SCI_DEF）
- 标题: （P2·**反向钉死的机器门**）`snr_constants` 把**截断串在场**固化为过门必要条件 ⇒ 按 `S-1` 统一精度反而会让门变红
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V12-N-10（P2 A_SCI_DEF）
- 标题: （P2）1.2 bbox 裕量：三处文本保证强度互斥 + **赤道分支缺"锥触极"回退守卫**；V12 造出可复算漏星反例
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)

### V7-N-03（P2 C_ALG_IMPL）
- 标题: （P2，**机制级**，本卷最值一条）typed-artifact 消费面「缺键 ⇒ 就地默认」全仓规模：兜底把**上游显式拒绝的非法值**制度化
- 原 position: 
- 原 evidence: 
- 账本 fix_state: OPEN | verified_state: 
- **复验结论**: (待填)
