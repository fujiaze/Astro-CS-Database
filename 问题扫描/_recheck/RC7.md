# RC7 复核档案 · recheck_round1 verify[6::8]（61 条）

- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（BASE `521095b8`）
- 分片：`问题扫描/_cache/recheck_round1.json` → `verify[6::8]`（i=6），实测 61 条（494 条取模第 7 组）；不触碰他人区间
- 方法：一律按 文件::符号 重新定位（glob/grep/read + 只读 python3 统计 + `git --no-optional-locks diff/show/log` 只读），复算原缺陷机制是否消失；缺失判定走三级复核（①工作树 ②HEAD `git ls-files`/`git show HEAD:<path>` ③改名/移动检索）
- 工作树状态：脏改含 `docs/TRACEABILITY.csv`、`artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv`、`reports/v19r2|v19r3/*`、`设计大纲/**`；涉及时单独注明，结论以 HEAD 入库态为准
- 四态口径：STILL（缺陷仍在，给新锚）/ FIXED（给"修在哪"证据：文件::符号 + 关键 diff 或新逻辑，并判是否修全/同类他站是否仍在）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）
- 本档只输出**建议标记**，不回写账本、不 commit（账本由前台统一处理）

## 一、结论总表（收工回填）

| # | ID | 优先 | 类别 | 四态 | 新锚（文件::符号） | 一句话判据 |
|---|----|------|------|------|--------------------|------------|

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

<!-- 后续 B/D/E/F/G 组收齐后统一回填 -->

## 三、统计（收工回填）

- STILL: / FIXED: / MOVED: / CANNOT_STATIC:
- 建议账本标记（仅建议，不回写）: