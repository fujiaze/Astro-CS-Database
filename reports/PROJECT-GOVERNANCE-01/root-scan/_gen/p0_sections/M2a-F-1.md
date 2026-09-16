## M2a-F-1 SCI-DRZ-001 §11 的五个验证 Oracle 中三个所指测试文件从未注册进任何构建/CTest/CI 面，注册表还以其中未注册的一条作为 CONFORMANT 证据

- 类别: F_TEST_GAP
- 优先级: P0
- 来源: L04-004（前台指定的逐条确认项之一：须给"9003 例中约 51% 复用生产 compute_overlap_area_g"的行级证据）
- 复核时点结论: **仍成立**
- 位置: docs/science/DRIZZLE.md::§11 验证 Oracle、::§13 追溯与测试、::§15；docs/standards/STANDARDS_REGISTRY.md::D.drizzle 判定表（§3 方差/权重传播确定性行）与偏差表；lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp、::variance_propagation_test.cpp、::drizzle_freeze_test.cpp、::drizzle_nonfinite_test.cpp、::control_median_mc_test.cpp；lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt；tests/unit/CMakeLists.txt；ci/ctest_baseline.json；ci/checks.json
- 证据摘录（逐字，复核时点现文）:
  > （DRIZZLE.md:115-119）- **零漏选门**：`candidate_oracle_test` 9003 例 `false_negative=0`（`TEST-DRZ-CAND-001`）。… - **Python 参考**：`healpy` 球面多边形面积对同 `drop` 的 `a_jp` 复算（`rtol 1e-9`）。 - **几何缓存等价**：`TargetGeomCache` 命中/未命中结果 `max_abs==0`（`DRIZZLE_TARGETED`）。
  > （DRIZZLE.md:133）- 测试: `TEST-DRZ-CAND-001` 9003例零漏选、`TEST-DRZ-VAR-001` 缩放律、常数场、`TargetGeomCache` 等价（`candidate_oracle_test.cpp, variance_propagation_test.cpp`）
  > （tests/p1drz/CMakeLists.txt:48-72）add_test(NAME p1drz_units …) / p1drz_properties / p1drz_oracle / p1drz_negative / p1drz_performance / p1drz_selfcheck   // 全仓 add_test 中**无** candidate_oracle / variance_propagation / drizzle_freeze / drizzle_nonfinite / control_median_mc
  > （tests/unit/CMakeLists.txt:478）add_subdirectory(${CMAKE_SOURCE_DIR}/lib/healpix_db/healpix_drizzle/tests/p1drz p1drz)   // 唯一被登记的 drizzle 测试目录；上级 tests/**/CMakeLists.txt grep "candidate_oracle|variance_propagation|drizzle_freeze|drizzle_nonfinite|control_median" → 0 命中
  > （ci/checks.json）无 drizzle/p1drz 检查项（grep "drizzle|p1drz" → NO HIT）
  > （STANDARDS_REGISTRY.md:162）| §3（方差/权重传播确定性）… | CONFORMANT | … | 无（9003 例全枚举 false_negative=0） |
  > （candidate_oracle_test.cpp:55-65）oracle_exhaustive：真值集 = 对**全部** leaf 像素调用生产 `spherical::compute_overlap_area_g(...)>0`；（:67-78）oracle_high_nside：候选超集 = 生产 `query_candidate_pixels(...)`，真值集 = 其中 `compute_overlap_area_g(...)>0`；（:90-92）`(nside <= 32) ? oracle_exhaustive : oracle_high_nside`
  > （candidate_oracle_test.cpp 自述 :147）"16/32: 全像素穷举 Oracle (12288/49152 像素); 64/128: 保守参考"
  > 计数复算（本代理独立复算，与叶子一致）：矩阵 111 位置 × 5 尺度 × 4 pixfrac × 4 nside = 8880；+ 高 nside 附加 48；+ 4194304 面 75 = **9003 例**；其中 nside∈{64,128} 分支 111×5×4×2 = 4440，加 48、75 ⇒ **4563/9003 ≈ 51%** 的"真集合"来自生产代码（query_candidate_pixels ∩ compute_overlap_area_g）
  > （TargetGeomCache 检索）tests/** grep "TargetGeomCache" → 0 命中；全仓 grep "healpy" → 仅 docs/science/DRIZZLE.md:118 与归档 memory，无脚本/测试实现
- 权威依据: 宪章 §13.1（每模块必备"不调用生产实现的独立 Oracle 或解析解"）、§13.2-2（模块数值测试须对 SCI/ALG Oracle）、§12.3-4（核心合同至少存在一个独立测试）、§17.2（发布门：文档—模块—源码符号—测试追踪无断链）；SCI-DRZ-001 §11/§13
- 问题说明: 三层同时断：**(1) 注册层** —— SCI §11/§13 点名的 `candidate_oracle_test`、`variance_propagation_test`、`drizzle_freeze_test`（以及 `drizzle_nonfinite_test`、`control_median_mc_test`）在当下树里没有任何构建/CTest/CI 登记：唯一的 drizzle 注册面是 `tests/p1drz` 的 6 个 `p1drz_*` 目标，`ci/checks.json` 里 drizzle 一项都没有 ⇒ 这些文件既不会被 CI 跑，也不会被本地 `ctest` 跑到，`TargetGeomCache` 等价门与 `healpy` 独立面积参考在仓内根本不存在实现。**(2) 独立性层** —— 即便跑起来，9003 例里约 51%（4563 例）的"真集合"由生产函数 `compute_overlap_area_g`/`query_candidate_pixels` 自身充当判据（行级证据见上），剩下 nside≤32 的穷举分支也仍以生产 `compute_overlap_area_g>0` 为真值 ⇒ "零漏选"实际只证明了枚举与面积函数**自相一致**，不满足 §13.1"不调用生产实现的独立 Oracle"。**(3) 登记层** —— 注册表 D.drizzle 把这条未注册、且自证的 9003 例门写进 CONFORMANT 行的偏差列（"无（9003 例全枚举 false_negative=0）"），使结构性机器门读到的"证据"与实际可执行面脱节（该模式与 M2a-B-2 同族）。
- 影响: SCI §11 声明的验收门（零漏选、方差 α² 缩放律、常数场流量守恒、几何缓存等价、Python 参考）在当下树上没有一项可执行 ⇒ §13/§17.2 的发布前提不成立；配合 M2a-A-1（常数场不变量本身写错）与 M2a-F-5（同名的平面 kernel 测试冒充 drizzle Oracle），drizzle 科学面的"已验证"声明整体失去证据支撑。
- 建议处置: ① 把 `candidate_oracle_test`/`variance_propagation_test`/`drizzle_freeze_test` 以显式 target 注册进 `tests/p1drz/CMakeLists.txt` 并同步 `ci/ctest_baseline.json`/`ci/checks.json`；② 真值集改为解析面积（L'Huilier 球面三角，独立实现）或 healpy 参考并落文件，消除自证；③ §11 未实现的两个门（healpy 参考、TargetGeomCache 等价）从 SCI 删除或改记为"未建立"；④ 注册表 D.drizzle §3 行的证据列改为"可执行测试名 + 注册位置"，未注册不得作为 CONFORMANT 证据。
- 置信度: 高（CMakeLists/add_test/ctest_baseline/ci 四处登记面逐一 grep；9003 与 4563 由本代理独立复算）
- related: L02-001, L07-006, L08-004（同为"测试未注册/证据面与断言主体错位"跨域模式）；M2a-A-1、M2a-F-5、M2a-B-2；F00-01（追溯矩阵双头，归 M6）
- **【E2-实测 2026-09】** (1) 注册层实测证实：五个 Oracle TU（candidate_oracle/variance_propagation/drizzle_freeze/drizzle_nonfinite/control_median_mc）在 `--include=CMakeLists.txt` 非注释行命中 = 0/0/0/**0**/0（drizzle_nonfinite 的 2 处命中经逐字核对均为 `# 与 drizzle_nonfinite_test … 互补不重复` 式注释），全树 `file(GLOB` = 0，`ctest --test-dir run/ci/build-gcc-release -N` 对五名 grep 命中 **0**（Total 288 中 p1drz_* 仅 6），`build/` 各树无任何对应产物；唯一 drizzle 测试登记面仍是 `tests/unit/CMakeLists.txt:478 add_subdirectory(…/tests/p1drz p1drz)`。四文件（含 kcorr_matrix、concurrency_cache）在 `ci/`+`.github/` 亦 0 命中 ⇒ 无旁路执行。⇒ 分类定为「**根本不在图里**」，非「在图未被采集」。 (2)/(3) 层本轮不复核（属静态判定，非执行可判）。
