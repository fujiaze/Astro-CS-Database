# 审稿-P1 · ALG-drizzle-001（第 1 遍，对抗审稿）

- **仓库**：`/workspace/Astro CS Database`，HEAD `850a9ede`
- **片号**：`ALG-drizzle-001`（层 `lib/algorithms/drizzle`）
- **负责人裁定口径**：一遍 = 对同一片材料的一次完整重读。本片**从头到尾逐行读完**，未换焦点、未跳读。
- **纪律自证**：零 git 写；零仓内文件修改；未编译、未跑 ctest/pytest/cmake、未跑任何二进制；未读 `/tmp/acsd_g08/`；中文路径一律 `git -c core.quotepath=false`。

---

## 1. 读完了吗

**计数口径**：以 `wc -l`（换行计数）为准；「读完」= 本人用 `read` 工具自第 1 行读至文件末行，且工具回报的 "End of file - total N lines" 与 `wc -l` 一致（无半行歧义、无未读尾巴）。

| # | 成员文件 | 声明行数 | 本人实读 | 覆盖 |
|---|---|---|---|---|
| 1 | `healpix_drizzle/nanoflann.hpp` | 4091 | 4091 | 100% |
| 2 | `healpix_drizzle/tests/drizzle_freeze_test.cpp` | 638 | 638 | 100% |
| 3 | `healpix_drizzle/fits_reader.cpp` | 532 | 532 | 100% |
| 4 | `healpix_drizzle/tests/drizzle_science_matrix_test.cpp` | 481 | 481 | 100% |
| 5 | `tests/p1drz/p1drz_oracle.hpp` | 394 | 394 | 100% |
| 6 | `healpix_drizzle/drizzle_engine.h` | 361 | 361 | 100% |
| 7 | `tests/kcorr_matrix_test.cpp` | 296 | 296 | 100% |
| 8 | `tests/drizzle_pf_sb_gate.cpp` | 275 | 275 | 100% |
| 9 | `tests/p1drz/p1drz_geom.hpp` | 266 | 266 | 100% |
| 10 | `hips/src/aio_publish.cpp` | 245 | 245 | 100% |
| 11 | `healpix_drizzle/spherical_overlap_science.cpp` | 207 | 207 | 100% |
| 12 | `tests/drizzle_trace_concurrency_test.cpp` | 191 | 191 | 100% |
| 13 | `tests/p1drz/p1drz_test_main.hpp` | 164 | 164 | 100% |
| 14 | `tests/p1drz/p1drz_thread_probe.cpp` | 150 | 150 | 100% |
| 15 | `tests/p1drz/CMakeLists.txt` | 141 | 141 | 100% |
| 16 | `healpix_drizzle/snr_evaluator.h` | 119 | 119 | 100% |
| 17 | `tests/representative_probe.cpp` | 110 | 110 | 100% |
| 18 | `hips/module.yaml` | 88 | 88 | 100% |
| 19 | `healpix_drizzle/Makefile` | 78 | 78 | 100% |
| 20 | `healpix_drizzle/poly_clip.h` | 67 | 67 | 100% |
| 21 | `tests/p1drz/p1drz_taskset_invariance.sh` | 61 | 61 | 100% |
| 22 | `healpix_drizzle/.gitignore` | 18 | 18 | 100% |
| 23 | `healpix_drizzle/healpix_core.cpp` | 16 | 16 | 100% |
| 24 | `tests/p1drz/p1drz_tests_main.cpp` | 8 | 8 | 100% |
| 25 | `src/acsd_p1_drizzle.def` | 3 | 3 | 100% |

- **成员份数**：25　**读了几分**：25　**成员总行数**：9000　**实际读了多少行**：9000　**覆盖率：100.0%**
- 与权威清单 `实际行数: 9000` **逐位一致**。
- **未读完的**：**无**。无「读不到 / 跳过 / 抽样」项。
- 为取证另读片外文件（非本片成员，不计入覆盖）：`spherical_overlap_science.h`(99)、`poly_clip.cpp`(287)、`drizzle_science.cpp:160-249`、`drizzle_engine.cpp` 若干区间、`hp_drizzle_api.cpp` 若干区间、`healpix_drizzle/healpix_core.h`、`tests/CMakeLists.txt`、`eng/ci` 与 `docs/science/algorithms/HEALPIX_MAPPING.md`。

---

## 2. 本片判定：**阻断（BLOCK）**

最重 3 条：

1. **B-01｜T1「coverage/hole Oracle」的期望值与被检量是同一个生产函数** —— 教科书式自洽式断言，且无防空转护栏。`drizzle_freeze_test.cpp:167/169/171` 调用的正是引擎自己在 `drizzle_engine.cpp:1617/1659` 调用的 `query_candidate_pixels_fast` / `compute_overlap_area_g`。内核里的任何错误在两侧同时发生、误差相消，门恒绿。而 `drizzle_freeze_test.cpp:182` 的 `CHECK(fh.empty() && ff.empty(), ...)` **没有非空护栏**，双侧全空也判 PASS。
2. **B-02｜`fits_reader.cpp:422-427` 截断读取只发警告、`return true`，且 `img.width/height` 仍是完整值** —— 下游 `drizzle_engine.cpp:2104` 按 `img.width` 线性索引 `pixels[]`，全仓**无任何一处**校验 `pixels.size() == width*height*channels`。截断 FITS ⇒ 成功返回 ⇒ 堆越界读，**把相邻堆内存当作天像素写进科学产品**。这是静默降级 + 内存安全的合并缺陷。
3. **B-03｜`drizzle_pf_sb_gate.cpp:262-263` 的「判据非退化」检查 (5) 是恒真门** —— 由 (1)(2) 的通过条件代数可证其恒真：`|(1+δ)·pf² − 1| ≥ 1 − pf² − 1e-6`，对 `pf ∈ {0.8,0.5,0.25}` 恒 ≥ 0.36 ≫ 1e-3；方差侧 `pf⁴` 同理恒 ≥ 0.59。该检查被文件头 `:17` 宣称为「判据有牙」的机器证据，实际**零判别力**。

---

## 3. 逐文件清单

| 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|
| `drizzle_freeze_test.cpp` | 638 行全读；T1–T8 逐门 | **B-01** T1 oracle 复用生产核（`:167,169,171`）+ `:182` 无空集护栏；**M-04** 求和型通量门被当验收硬门（`:113,213,235,256,268,282,512`，与 `DRIZZLE_GEOMETRY.md:320-324`「只作辅判据」冲突）；**M-05** FP32/FP64 单向比较只算 `missing`（`:116-127`），`|FP64\FP32|` 从不计算，而生产写盘走 FP32（`:372`）；**M-06** `if (a <= 0) continue`（`:442`）先滤后取极值，恰滤掉「有通量零面积」这一最坏成员；**M-07** `:452` 均匀性门 `1e-3` 比正本 `DRIZZLE_GEOMETRY.md:330` 的 `1e-4` 松 10×；**S-24** `:334,396` 硬编码 `2*9` 而 `:105` 用 `compute_tile_depth`；硬编码 `cfg.threads=16`（`:93,143,325,359,432,474,496`） | 阻断 |
| `fits_reader.cpp` | 532 行全读 | **B-02** `:422-427` 截断只警告仍 `return true`（`:529`）+ `:513-514` 尺寸不缩；**M-01** `:63-70` `parseDouble` 用 `strtod(...,nullptr)`，失败与 0.0 不可分 ⇒ `BSCALE` 损坏 → `:491-496` 全像素乘 0，仍成功；**M-02** `:337-341` `NAXIS3∉{1,3}` 强制降为 1 通道且在 `:418` 算 `data_size` 之前发生 ⇒ 多平面被静默丢弃；**M-03** `:162` SIP 阶数 >5 的系数静默丢弃（返回 0），而 `:394` 把**头里的完整 A_ORDER** 写进 `sip.order` ⇒ 「声称 order=8、系数只有 0..5」；`:373-375` 缺 WCS 只警告（但调用方 `hp_drizzle_api.cpp:242`、`drizzle_engine.cpp:881/1006/1835` 均已拦，故降级为建议）；`:418-419` 未验头尺寸 ⇒ 溢出/bad_alloc | 阻断 |
| `drizzle_science_matrix_test.cpp` | 481 行全读；Part A/B 逐门 | **M-15** `:300,312,323` 判的是 9 个 drop 的**均值**，单个 drop 8e-6 误差被其余 8 个 ~1e-12 稀释到 8.9e-7 < 1e-6 判绿；**M-16** `:301,315-317` 唯一的最坏值探测器只 `printf("[info]")`，`g_fail` 从不触碰；**M-17** Part B `:371-374` 以 FP64 作参考，**无任何独立参考**，共模几何缺陷整段通过；**S-21** `:229` 硬编码 `create_directories("run/temp/precise_hardening")` 而 `:470-476` 尊重 `argv[1]`；**S-22** `:436-438` 消息写 `(<10)` 实则门是 `p95<10 && pmax<64`；**S-23** `:397-414` 分桶表与 `:351,418` 的 `ulp_p99_all` 算完从不使用；`:359` 硬编码 `threads=16`；**正面**：`area_ref_ld:174-195` / `area_ref_planar:198-222` 是真正独立的高精度参考 | 须修 |
| `p1drz_oracle.hpp` | 394 行全读；O1–O9 逐条 | **M-11** `extract_leavs:71-74` 用 `sumNorm>0? sumFlux/sumNorm : 0.0`，**sumNorm/sumVarNum 里的 NaN 被这个护栏吞掉**，而 `:324` 只扫 `sumFlux/signal`；**M-12** `:318-322` 独立算出的 `expect_polluted` 从不参与 `ok`（`:338-339`），「三源互校」（`:297-298`）实为两源；**M-04** `:279` `if (l.nContrib != 1) continue` 把多贡献者边界叶全滤掉——正是唯一需要按 `w_jp²` 聚合的那批；单叶时 `w` 精确相消 ⇒ 该门对权重律零判别力；**M-13** `:236-237` 声称的第二面「Σx²w² oracle 重算」不存在；`:243/277/284` `n_checked` 与真正受检的 `nc1_checked` 是两个计数器；`:362-369` `DeterminismOracle` 从不实例化且两判定位默认 `true`；`:209-241` 之外的 `geom_overlap_area` 亦为死码（见 geom）；**正面**：`:113-125/205-226` 的期望侧（Σ输入、`amp`、`B0`、全量 `a_drop_sum`）确与被测侧不共式 | 须修 |
| `drizzle_engine.h` | 361 行全读 | **悬空引用** `:14` 「实现见 healpix_stack/healpix_core.h」——该目录**只存在于** `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack`，权威实现在 `lib/algorithms/shared/healpix/`；`:25` `nside=32768` 默认值无推导出处；`:36` `uncalibrated_adu_allowed` 降级开关（经复核**诚实**，见 §6）；`:41` `threads=0` 合规；`:352-358` `drizzle_trace::init_from_env()` 为进程级全局态 | 须修 |
| `kcorr_matrix_test.cpp` | 296 行全读 | **M-08** 头 `:7` 称「retained N 至少 2 档」，但 `:195` 的 `break` 只取**第一个**非空 tile ⇒ 恒 1 档；**M-09** `:192/197/205` 三重幸存者筛选，`patch.size()<4` 丢弃的正是退化 patch，而 1000 次里丢 800 次仍 `≥200` 判绿；**M-10** `:228` `k=var_emp/baseline`、`:230` `n_eff=baseline/var_emp=1/k` 精确 ⇒ `k>0` 已保证 `n_eff>0`，`:116-117` 的 "zero-n-eff" 负例**生产永不可达**（`n_retained` 同理），`:287` 却宣称「6 负例判红」；`:248,251-255` A/B 科学判定两分支都 `return 0` ⇒ 结论翻转无声；**S-26** `:202` MAD→σ 常数 `1.482602218505602` 合物理但**未注出处**，违反「有科学推导的保留推导与出处」；`:220` `:205` `:174` 同样裸值。**本人独立复算**：committed 证据 `max_rel_dev=1.3007` ⇒ verdict=B，k 跨 `1.21..3.20`（2.6×），而该测试**不注册为 ctest**（`tests/CMakeLists.txt:178`） | 须修 |
| `drizzle_pf_sb_gate.cpp` | 275 行全读 | **B-03** `:262-263` 恒真门（代数见 §5）；**M-14** `:146` 注释称「唯一有覆盖叶」，`:154-161` 只要求 `sup>0` 且 last-wins，`:225` 只判 `n_covered>0` ⇒ writer 若把 support 抹满 512² 全部 262144 叶、末叶又恰读出 B0，五门全绿；`:116-117` 注释「4 个叶」实为 `locals[1]={0}` 一个；`:63` `kLeafPerTile` 死常量；`:225/229` profile=0 用 `1e-6`、profile=1 用 `1e-5`，10× 不对称无注释；`:178-180` 裸常量；**正面** `:109` `a_cell=4π/(12nside²)` 推导正确、`:270-273` 有 `g_pass==0` 空跑护栏（优于 freeze test） | 阻断 |
| `p1drz_geom.hpp` | 266 行全读 | **M-15** `:209-241` `geom_overlap_area`（33 行独立 gnomonic+S-H+shoelace 交叠原语）**零调用点** ⇒ 唯一能做逐叶独立交叠验证的原语是死码，所有 oracle 退化为对「全局和」的红istribution-blind 判据；`:140` `fabs(num)` 丢弃缠绕方向；`:112-114` 明示与生产共用 `half=0.5·pixfrac` 与角序 ⇒ 能抓错面积**公式**、抓不到错 drop **足迹约定**；`:21` 与 `:14-16` 的 astropy 交叉验证声明**经复核属实**（见 §6 裁决） | 须修 |
| `aio_publish.cpp` | 245 行全读 | **M-18** `:243` `fsync_parent_dir(out_s)` **返回值丢弃**后 `return OK` —— rename 的原子性达成但**持久性未达成**，而同文件 `:143-147` 把目录 fsync 失败当硬 `ERR_DISKFULL`；**M-19** `:206-207` 与 `:221-222` 同一「stage 缺失/非目录」条件分别返 `ERR_IO` 与 `ERR_STATE`，违反自身合同 `publish.h:60`；`:52-58` 故障注入 env 在**生产** TU 内无条件编译；**正面**：`:110` 递归深度上限、`:130/145` ENOSPC/EDQUOT 分类正确、`:206` 非目录 fail-closed、`:179` EEXIST→STATE 明确 | 须修 |
| `spherical_overlap_science.cpp` | 207 行全读 | **M-20** `:38-40,77-80` 把 `ACSD_DRZ_FAULT=invalid_area` 的**数值故障注入器编进生产几何路径**，且注释 `:37` 指向的门 `p1_drz_area_invalid` **全仓不存在**（`run/` 外零命中，真实门名为 `p1_m42_t2` 一族）⇒ 该负例门永远不可能红；**M-21** `:168` 闭合失败路径 `return e` **不回填 `diag_out`**，与本函数 `:154-157` 自己写的「失败路径也必须把被吞掉几个回传」原则直接矛盾（`:159` 守了、`:168` 没守）；`:23` `return 1e-6` 为**死分支**（唯一调用点 `:47` 已被 `:44` 的 `A_pixel>0` 守卫）；`:73,99` 的 `sum` 被 `:111` 丢弃重算（死算）；**正面**：`kMaxInvalidAreaHits=0`（头 `:62`）真 fail-closed、`:111-112` 去重后重算正确、`:34` 先清零输出、`:90` 每个面积先 isfinite | 须修 |
| `drizzle_trace_concurrency_test.cpp` | 191 行全读 | **M-22** `:101-103` 注释宣称校验「`sum_contribution` 与 `contribs` 之和一致」，但 `:114-129` **一个数字都没解析**（只做子串存在 + `back()!='}'` + 括号配平）⇒ 注释所述检测通道不存在；**M-23** `:179` 只对 lineage 有 `n==0` 护栏，**`leaf_internal.jsonl` 无对应护栏**，而 `jsonl_is_wellformed` 对存在的空文件返回 true ⇒ 缺陷 B 的 leaf 侧（`shrink_to_fit` use-after-free）完全失测且不红；**M-24** `:143-154` 每轮自建 6 个 `std::thread` 并 join，调度器池外私建线程（`:90 threads=1` 的选择在竞争面上是**对的**，予以否决）；`:50-51` `kFrames=6/kRounds=3` 无出处 | 须修 |
| `p1drz_test_main.hpp` | 164 行全读 | **M-25** `:144-159` 组选择器匹配不到任何组时**全部 `continue`、`total_fail` 恒 0、`return 0`**，且 `:157` 打印 `(int)n - total_fail` 把**全部注册组**报成「通过」⇒ `./p1drz_tests unitsx` 打印「4 通过, 0 失败」退出 0。当前组名恰好匹配（潜伏），但任一改名即把 4 个 ctest 目标静默变成恒绿空跑；`:24-34` `drz_check`/`g_p1drz_pass|fail` 零调用且不参与退出码；`:73-75` `fault_name_usable` 的**历史**恒真门（`:66-72` 自述）已真改为运行期内容判定 —— **此项为正面修正** | 须修 |
| `p1drz_thread_probe.cpp` | 150 行全读 | **M-26** `:120-121` 打印的是 **argv 的 `threads`**（`:135`），脚本恒传 0 ⇒ 每行都是 `threads=0`，`:50` 抓到的「预算变了」的证据**结构上恒为常量**；**M-27** `:102-107` 的 `.canon` 逐字节转储含 `sumFlux/sumArea/sumVarNum/nContrib` 却**独缺 `sumNorm`**，而 `:129-130` 自述用途正是复核 DISP-DRZ-009（其判别量恰是 `sumNorm`）；`:102/110/116/118` 写返回值全丢 ⇒ 短写仍 `return 0`；`:88,112` 裸 `std::fopen` 绕过 aio（与 `fits_reader.cpp:27-31` 已修的同一违规） | 须修 |
| `tests/p1drz/CMakeLists.txt` | 141 行全读 | `:128-132` `p1drz_taskset_invariance` **只设 `TIMEOUT`、无 `SKIP_RETURN_CODE`** ⇒ 脚本 `:19,22,38,53` 的 4 个 `exit 0` 在 CTest 里一律记 **PASSED**；`:56` 注释引用的冻结目标 `eng/ci/ctest_baseline.json` **不存在**；`:112-118` `p1drz_thread_probe` 有 `add_executable` 无 `add_test`（`:108` 已显式说明「非 ctest 门」，**不算缺口**）；`:61-65` 的 `if(UNIX)/else message(STATUS)` 是**非静默跳过，正面**；8 个 `add_test` 全部存在、无 `LABELS` 陷阱 | 须修 |
| `snr_evaluator.h` | 119 行全读 | 纯 PIMPL 声明面，`:110` `idw_power_` 默认 1.0 并**注明规范出处** `docs/detail/algorithms_phase1/07_noise_snr.md:192`（正面，符合「有科学推导的保留并注明出处」）；`:114` `DEFAULT_KNN=16` 为「精度与性能平衡」经验值，`:113` 已标注但无推导 —— 按 AGENTS.md §6 应自适应；本 TU 无实现可供复核，故不断言 | 建议 |
| `representative_probe.cpp` | 110 行全读 | **M-28** `:8` 头称记「science hash（signal DATASUM 与快照一致）」，`:97-108` **无任何 hash、无任何参考**，全文件零断言、`:109` 无条件 `return 0` ⇒ 实质恒绿探针；`:70-71,90-93` 计数器为 0 时比值静默打印 `0.000`；`:103-104` `sum_flux`（全部 touched）与 `touched`（仅 `sumArea>0`）**总体不同却并列打印**；`:50` 硬编码 `threads=8`。**缓解**：`tests/CMakeLists.txt:235` 已把它归入 `DRZ_PROBE_TESTS` 不注册 ctest ⇒ 报「头夸大」而非「CI 里的绿门」 | 建议 |
| `hips/module.yaml` | 88 行全读 | **M-29** `:71-79` `source_symbols` 只列 8 个 `aio_hips_*`，**完全不含**本 TU `aio_publish.cpp` 定义的 5 个符号（`aio_publish_stage_create_v1/discard_v1/tree_fsync_v1/promote_v1`、`hips_publish_fault_slow_write_v1`），而 `hips/CMakeLists.txt` 确实把该 TU 编进模块 ⇒ 模块声明面不描述模块，违反 AGENTS.md §6「增删模块对应调整生产者/消费者声明」；`:1,4,64` 头部含日期与「W1 声明面收缩」变更叙事（AGENTS.md §5）；`:39` `entrypoint` 与 `.def:3` 导出的 `acsd_module_query_v1` 一致（正面） | 须修 |
| `healpix_drizzle/Makefile` | 78 行全读 | **M-30** 三条悬空路径：`:50` `DEPS = ../../common/healpix/healpix_core.cpp ...` ⇒ **`lib/algorithms/common/` 根本不存在**（权威在 `lib/algorithms/shared/healpix/`）；`:36` `AIO_DIR ?= ../../astro_image_io` ⇒ `lib/algorithms/astro_image_io` 不存在；`:49` 同一错误路径；`:46` 的 9 个 SRCS **全部存在**（正面）；`:2` 自己援引 BLD-002「根 CMakeLists 是唯一产品事实源」与 AGENTS.md §6 禁模块级独立构建路径，**而本文件正是模块级独立构建路径**；`:22-24` 叙述性历史（AGENTS.md §5）；`:31` `-Wpedantic` 与 `-Wno-c++20-extensions` 并存 | 须修 |
| `poly_clip.h` | 67 行全读 | `:55` 声明「clip 顶点数 < 3 时返回 subject（**未裁剪**）」= fail-open 合同；**经复核：`PolyClip` 的 4 个静态方法在全仓 `lib/` 零调用点**，`poly_clip.cpp` 却仍编入产品（`lib/algorithms/drizzle/CMakeLists.txt:23`）⇒ 装载的陷阱而非活缺陷。正确处置是**删除**而非加护栏（见 §5 反例 4） | 须修 |
| `p1drz_taskset_invariance.sh` | 61 行全读 | **M-31** `:16` 仅 `set -u`，无 `-e`/`pipefail`；`:21` `|| true` 吞掉 taskset 失败；`:23` `python3` 缺失未检 ⇒ `n=0` → `:38` `exit 0` ⇒ **零探针执行而门绿**；`:48` `sha256sum` 失败未检 ⇒ 五个 `h` 全空 ⇒ `:55-60` 空串相等 ⇒ `:61` 打印 `[PASS] ... ()`；`:5-6` 头称「同一预算重复运行亦然」而 `:41-52` 每预算**只跑一次**；`:42` 跳过高预算后 `:61` 仍印「1..16」；`:40` `ok=1` 赋值后从不使用；`:39` `rm -rf "$WORK"` 无防护（CTest 侧 `:131` 传的是构建目录子路径，已降级） | 须修 |
| `healpix_drizzle/.gitignore` | 18 行全读 | 仅通用构建产物模式；无本模块特有项，无误纳/漏纳证据。无发现 | 通过 |
| `healpix_core.cpp` | 16 行全读 | **M-32（本人推翻了一条退役声明）**：`:9` 的 EXIT 条件断言「drizzle 模块内 grep 无 `#include "healpix_core.h"` 消费点」——**实测 13 处**（含生产 `drizzle_engine.cpp:4`、`reverse_drizzle.cpp:17`、`spherical_overlap.h:37`、`spherical_overlap_science.h:19`）。文件 `:6` 自认「本轮未做引用清点」。这正是负责人预告的「退役声明被证伪」类，本片再添一例。幸而 `:19` shim 头转发至权威实现，未造成链接断裂，故判**须修**而非阻断 | 须修 |
| `p1drz_tests_main.cpp` | 8 行全读 | 纯转发 `main` → `p1drz_run_core_groups`。无发现 | 通过 |
| `src/acsd_p1_drizzle.def` | 3 行全读 | 单导出 `acsd_module_query_v1`，与 `module.yaml:39` 一致（正面）。无发现 | 通过 |
| `healpix_drizzle/nanoflann.hpp` | **4091 行全读**（4 段读完） | BSD-3-Clause 上游 nanoflann，`NANOFLANN_VERSION 0x190`。**M-33** `:2706-3946` 约 **1240 行**为本仓自写的 LiDAR/增量索引代码（`KDTreeIncrementalIndexParams`、`KDTreeSingleIndexIncrementalAdaptor`、`...AdaptorMT`，含「LiDAR odometry」「ikd-Tree's async-rebuild idea」「PCL/pthread dependency」等注释），**与上游文件混排且无任何本地修改标记**，版本宏仍宣称 1.9.0 ⇒ 一次例行的上游同步会静默删掉这 1240 行；`:3693-3945` 的 `...AdaptorMT` 用 `std::async` 自建后台线程（潜在私建线程面）；`:4091` 只 `#undef NANOFLANN_RESTRICT`，`NANOFLANN_NODISCARD/VERSION/FALLTHROUGH` 泄漏 | 须修 |

---

## 4. 发现清单

### 阻断（3）
- **B-01** 自洽式断言：T1 oracle 直接调用被测生产函数，且双侧全空也判 PASS。`drizzle_freeze_test.cpp:167,169,171,182`（对位 `drizzle_engine.cpp:1617,1659`）
- **B-02** 静默降级 + 越界读：截断 FITS 返回成功、尺寸不缩、全仓无长度校验。`fits_reader.cpp:422-427,513-514,529`（对位 `drizzle_engine.cpp:2104`）
- **B-03** 恒真门：`drizzle_pf_sb_gate.cpp:262-263`「判据非退化」检查由 (1)(2) 代数恒真

### 须修（30，摘要）
M-01 `parseDouble` 失败不可分（`fits_reader.cpp:63-70,491-496`）｜M-02 `NAXIS3` 静默降通道（`:337-341`）｜M-03 SIP 阶数 >5 系数静默丢弃而 order 仍写全（`:162,394`）｜M-04 求和型通量门被当验收硬门（`drizzle_freeze_test.cpp:113` 等 8 处；`p1drz_oracle.hpp:279` 滤掉多贡献者）｜M-05 FP32/FP64 单向比较（`drizzle_freeze_test.cpp:116-127`）｜M-06 先滤后取极值（`drizzle_freeze_test.cpp:442`）｜M-07 均匀性门松 10×（`:452`）｜M-08 kcorr「≥2 档」实为 1 档（`kcorr_matrix_test.cpp:7,195`）｜M-09 kcorr 三重幸存者筛选（`:192,197,205`）｜M-10 kcorr 两个负例生产不可达却宣称 6 例（`:228,230,287`）｜M-11 oracle 自身护栏吞 NaN（`p1drz_oracle.hpp:71-74,324`）｜M-12 `expect_polluted` 算而不用、「三源互校」实为两源（`:318-322,338-339`）｜M-13 声称的方差第二面不存在（`:236-237`）｜M-14 `n_covered` 只判 `>0`（`drizzle_pf_sb_gate.cpp:146,225`）｜M-15 `geom_overlap_area` 死码（`p1drz_geom.hpp:209-241`）｜M-16 `jsonl_is_wellformed` 不解析数字（`drizzle_trace_concurrency_test.cpp:101-129`）｜M-17 leaf jsonl 零记录无护栏（`:174-179`）｜M-18 父目录 fsync 丢返回值（`aio_publish.cpp:243`）｜M-19 同一条件两种码（`:206,221`）｜M-20 生产 TU 内数值故障注入 + 门不存在（`spherical_overlap_science.cpp:37-40,77-80`）｜M-21 闭合失败路径不回填诊断，自违本函数原则（`:168`）｜M-22 组选择器空匹配退出 0（`p1drz_test_main.hpp:144-159`）｜M-23 探针打印 argv 而非实际预算（`p1drz_thread_probe.cpp:120`）｜M-24 `.canon` 独缺 `sumNorm`（`:102-107`）｜M-25 taskset 门无 `SKIP_RETURN_CODE`（`CMakeLists.txt:128-132`）｜M-26 module.yaml 漏声明 `aio_publish` 全部符号（`:71-79`）｜M-27 Makefile 三条悬空路径（`Makefile:36,49,50`）｜M-28 PolyClip 死码携 fail-open 合同（`poly_clip.h:55`）｜M-29 taskset 脚本 4 处 `exit 0` + 空哈希判 PASS（`p1drz_taskset_invariance.sh:16,21,23,38,48,53`）｜M-30 healpix_core 退役声明被证伪（`healpix_core.cpp:9`）｜M-31 nanoflann 内混排 1240 行本仓代码无标记（`nanoflann.hpp:2706-3946`）｜M-32 `drizzle_engine.h:14` 悬空目录引用

### 建议（14）
S-01 `snr_evaluator.h:114` `DEFAULT_KNN=16` 应自适应｜S-02 `representative_probe.cpp` 零断言且头夸大（`:8,97-109`）｜S-03 同文件比值总体不同（`:103-104`）｜S-04 science_matrix 硬编码输出目录（`:229`）｜S-05 ULP 消息与门不符（`:436-438`）｜S-06 science_matrix 死算（`:351,397-414`）｜S-07 freeze 硬编码 `2*9`（`:334,396`）｜S-08 `drizzle_freeze_test` 等 7 处 + science_matrix + probe 硬编码 `threads`｜S-09 kcorr 科学常量无出处（`:202,205,220,174`）｜S-10 pf_sb_gate 注释与代码不符 + 死常量 + 容差不对称（`:63,116-117,225,229`）｜S-11 `geom_solid_angle_tri` 用 `fabs` 丢缠绕方向（`p1drz_geom.hpp:140`）｜S-12 `DeterminismOracle` 默认判真（`p1drz_oracle.hpp:364-365`）｜S-13 `drz_check` 死码（`p1drz_test_main.hpp:27-34`）｜S-14 module.yaml 头部含日期叙事（`:1,4,64`）

---

## 5. 我主动构造的反例

### 反例 1（推翻 B-01，构造成立）
**构造**：在 `compute_overlap_area_g` 内注入——对跨越 `u+v=1` 接缝的叶（`drizzle_freeze_test.cpp:520-523` 自述该类缺陷曾真实发生，弦表示亏缺至 ~9.97%/叶）把面积返回 `0.0` 而非一个小的正数。
**期望推翻**：`test_coverage_oracle()` 的 false-hole 门应当判红。
**是否推翻**：**是，门恒绿**。引擎侧 `drizzle_engine.cpp:1668-1671` 谓词 `!isfinite(overlap) || overlap < 1e-20 → quick_rejects++; continue` 把该叶跳过；oracle 侧 `drizzle_freeze_test.cpp:171` 的 `> 0.0` 同样为假、同样不插入。两侧**丢弃同一批叶** ⇒ `fh` 空 ⇒ `:182` 打印 `[PASS] false hole=0 false fill=0`。**一个真实的天区覆盖空洞被判为完美覆盖。**

### 反例 2（推翻 B-02，构造成立）
**构造**：把一份合法 FITS 的数据单元尾部截掉 30%（拷贝中断 / 磁盘满 / 网络盘短读）。
**期望推翻**：`readFits` 应返回 `false`；若不返回 `false`，下游必须拒绝。
**是否推翻**：**否，双重失守**。`:424-427` 只 `fprintf` 警告并把 `n_pixels` 缩小，`:529` 仍 `return true`；`:513-514` 的 `img.width/height` 保持头声明的完整值。`grep -rn 'img\.pixels\.size()' lib/algorithms/drizzle/` 显示**无任何一处**比对 `pixels.size()` 与 `width*height*channels`。引擎 `:2104` `pixels[(size_t)y * (size_t)img.width + (size_t)x]` 按完整 width 索引 ⇒ 越界读 `std::vector<float>` 尾部 ⇒ 可能崩溃，更糟的是**把相邻堆内存当成天像素写进 HiPS 产品**。这正是并行作业最常见的重跑场景。

### 反例 3（推翻 B-03，构造成立，纯代数）
**设** 检查 (1)（`:225`）通过 ⇒ `|pd.sig − B0|/B0 ≤ 1e-6` ⇒ `pd.sig = B0(1+δ)`，`|δ| ≤ 1e-6`。
**设** `sig_legacy = B0/pf²`（`:254`）。则 `pd.sig/sig_legacy = pf²(1+δ)`，
`|pf²(1+δ) − 1| ≥ 1 − pf² − 1e-6`。
**代入** `pfs = {1.0, 0.8, 0.5, 0.25}`（`:182`，`pf<1` 才进 `:253`）：
| pf | `1 − pf² − 1e-6` | > `1e-3`？ |
|---|---|---|
| 0.8 | 0.359999 | 是（360× 余量） |
| 0.5 | 0.749999 | 是 |
| 0.25 | 0.937499 | 是 |
方差侧同构（`pf⁴`，最小余量 `1−0.8⁴=0.5904`）。
**结论**：只要 (1)(2) 通过，(5) **恒真**；若 (5) 失败则 (1)(2) 必已失败、测试早已红。**该检查的独立判别力严格为零**，而文件头 `:17` 把它列为「判据非退化」的机器证据。

### 反例 4（推翻「PolyClip 污染通量守恒」，**构造不成立**——本人推翻自己的初判）
**初判**：`poly_clip.h:55` 的 `clip<3 → 返回未裁剪的 subject` 是 fail-open，一旦发生则 `a_jp = A_drop`（全量），`Σ_p w_jp = 1+k`，闭合残差 `rel ≈ +k`，通量守恒被乘性破坏。
**复核推翻**：`grep -rn 'clipPolygon|PolyClip::' lib/` 显示 `PolyClip` 的 **4 个静态方法全部零调用点**，唯一命中是 `drizzle_engine.cpp:1110` 的一句注释。`poly_clip.cpp` 虽编入产品（`lib/algorithms/drizzle/CMakeLists.txt:23`），但无调用方 ⇒ **污染不可达**。正确处置是**删除** `poly_clip.{h,cpp}` 以消除装载的陷阱，而不是给它加护栏。**据此把该项从「阻断」降为「须修」。**

### 反例 5（推翻 M-10「kcorr 负例不可达」，构造成立）
**设** `k = var_emp / baseline`（`:228`），`n_eff = 0.5π σ² / var_emp`（`:230`），而 `baseline = 0.5π σ² / n_med`（`:220`）。
**推导** `n_eff = baseline / var_emp = 1/k` **精确**。`:221-222` 已强制 `k > 0` ⇒ `n_eff > 0` 恒成立 ⇒ `:116-117` 的 `"zero-n-eff"` 负例**主流程永不可达**。同理 `n_retained = n_med ≥ 4`（因 `:197` `patch.size() < 4 → continue`）⇒ `:114-115` 的 `"zero-n-retained"` 亦不可达。而 `:287` 向证据 JSON 写死 `"judge_selftest": "6 negative red + 1 positive green"` ⇒ **自检为守卫覆盖率给出虚假信心，真实可达者仅 4/6。**

### 反例 6（我自己的独立复算，取代对既有结论的采信）
不依赖任何既有报告，直接从 committed 证据重算 kcorr 结论：
```
baseline k = 1.3924 ; my max_dev = 1.3007 ; 门 <=0.10 ⇒ verdict = B
k 范围 1.21122 .. 3.20344 （跨 pixfrac 的 2.6 倍差异）
```
即：**k_corr 在不同 pixfrac 间相差 2.6 倍，结论已翻到「B：k_corr 作 per-frame 量」并写进了仓内受版本控制的证据文件，而该测试 `return 0`、且未注册为 ctest。** 一个量级明确的科学结论翻转，全程零红灯。

---

## 6. 盲复算（遮住既有判定独立取证）

| 命题 | 我独立取证的结果 | 与既有判定比 |
|---|---|---|
| `healpix_core.cpp:9` 的「零消费者」退役声明 | **不成立**：实测 13 处 `#include "healpix_core.h"`，含 4 处生产文件 | 既有（他人）未测；本人新推翻一条退役声明 |
| `drizzle_engine.h:14` 的 `healpix_stack/healpix_core.h` | **悬空**：该目录只存在于 `.../archive/legacy/healpix_stack` | 一致 |
| Makefile 的 `../../common/healpix` 与 `../../astro_image_io` | **两者皆不存在**（权威在 `shared/healpix`） | 一致 |
| `ACSD_DRZ_FAULT` 是否有设置方、门 `p1_drz_area_invalid` 是否存在 | **无设置方；门名全仓（含 `run/`）零命中** | 一致 |
| `p1drz_taskset_invariance` 的 SKIP 是否被 CTest 记为 PASSED | **是**：`:128-132` 无 `SKIP_RETURN_CODE` | 一致 |
| oracle 与生产共用 `ang2pix_nest` 是否构成自洽 | **不构成**：共享的是**单权威且被硬门验证**的地址映射（`lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp:6,112` 硬门 `mismatch == 0`，`docs/science/algorithms/HEALPIX_MAPPING.md:45,51` 记录 mismatch=0 / order 0..22）。几何路径确为不同源 | **判一致**（推翻了另一子代理「astropy 声明无代码支撑」的结论） |
| `drop_src_scale_rad` 返回 `sqrt(A_pixel)` 是否量纲错误 | **不错误**：球面小面元面积 ≈ 角尺度²，故 `sqrt(A)` 正是线性角尺度（rad） | 一致 |
| `validate_overlap_closure` 是否恒红/只抓超额 | **不恒红、抓两侧**：`drizzle_science.cpp:204-205` `rel > tol → exceeds_drop` / `rel < -tol → area_deficit` | 一致 |
| `uncalibrated_adu_allowed` 是否 fail-open | **不 fail-open**：缺 `PHOTDEGRADE` 键 → 该位为 `false`（缺参数**关闭**门而非打开门）；且 `drizzle_engine.cpp:1216/2506` 的 `photometry_done = apply_photometry \|\| photometry_applied_upstream` **刻意不含**该降级位 ⇒ 一旦靠降级位开门，产品必被打上 `PHOTAPPL=0`，无法洗成已标定 | 一致（正面） |
| nanoflann 两份副本是否已漂移 | **仅注释级差异**（URL 前多余空格、`for abs` → `for abs()`），无语义漂移 | 否决了「漂移」假设 |
| nanoflann 是否违反 CODE_STANDARD §52 的引入点窄隔离 | **不违反**：`snr_evaluator.cpp:24-29` 有成对 `#pragma warning(push/disable 4324 4127/pop)` | 否决 |
| nanoflann 是否在生产中私建线程 | **不私建**：唯一消费者 `snr_evaluator.cpp:133` 用 `KDTreeSingleIndexAdaptorParams(32)`，`n_thread_build` 取默认 1 ⇒ `divideTree` 而非 `divideTreeConcurrent` | 否决 |
| `std::max(fabs(ref), 1.0)` 这个 1.0 底是否把相对门松成绝对门 | **当前 fixture 下不生效**：两文件的 `sumFlux` 量级为 O(10²–10³)（源像素 ~1000、每叶权重 O(1)），远高于底 1.0。**但对暗源场（本项目的目标域）该底会静默把 `1e-5` 相对门变成 `1e-5` 绝对门**，列为潜伏陷阱 | 降级为建议 |
| freeze test T8 是否自洽 | **不自洽**：`:531-551` 用 long double Van Oosterom 扇形三角剖分作独立参考，对位生产的扇形/对角剖分，是真正的独立 oracle | 正面 |
| science_matrix Part A 是否有独立参考 | **有**：`area_ref_planar:198-222`（切平面 shoelace）与 `area_ref_ld:174-195`（64 段 + long double），与生产核不同源 | 正面 |

**一致性判语**：本片既有判定总体**偏松**——几乎所有既有结论都停在「注释/门不可信」层面，无人独立重算过 kcorr 的实际数值，也无人去证伪 `healpix_core.cpp` 的退役声明。本人新增 B-01/B-02/B-03 三条阻断与反例 6，均为**先复算后定性**。

---

## 7. 子代理派发记录

**派发 4 个**（工具对每次调用各派发一次，实得 5 个实例；其中「自洽式断言猎手」被重复派发 1 次，内容一致，互为冗余）。

| # | 主题 | 分派范围 | 回报 |
|---|---|---|---|
| A | 自洽式断言 / 恒真门 / 恒红门 / 筛掉真信号 | `tests/p1drz/*`（7 份，1184 行） | 报告完成 |
| A' | 同上（重复实例） | 同上 | 报告完成 |
| B | 静默降级 / 错误码 / 数值稳定性 / PolyClip 反例 | `fits_reader.cpp`、`spherical_overlap_science.cpp`、`aio_publish.cpp`、`module.yaml`、`.def` | 报告完成 |
| C | 线程池 / 硬编码 / 退化门 / 自愈锚 | 6 份测试文件（1991 行） | 报告完成 |

**逐条复核与否决结果**：

| 子代理结论 | 我的处置 | 依据 |
|---|---|---|
| A：T1 oracle 复用生产函数（BLOCKER） | **采纳** | 我本人已读 `drizzle_freeze_test.cpp:167-182`，构造反例 1 成立 |
| A：组 runner 把跳过组计为通过（BLOCKER） | **采纳** | 我本人已读 `p1drz_test_main.hpp:144-159`，逐行确认 |
| A：taskset 打印 `threads=0`、从不比较 | **采纳** | 我本人已读 `p1drz_thread_probe.cpp:120` + 脚本 `:44,50` |
| A：空 sha256 判 PASS、SKIP exit 0 | **采纳** | 我本人已读脚本 `:48,53,55-61` + `CMakeLists.txt:128-132` |
| A：`nc1_checked` 未上报（`n_checked` 报错计数） | **采纳** | 与 A' 独立同证，我读 `p1drz_oracle.hpp:243,270,277,284` 确认 |
| **A：astropy 交叉验证「无任何代码支撑」（B10）** | **否决** | 我亲自打开 `lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp:6,70,112,116`（硬门 `mismatch == 0`）与 `HEALPIX_MAPPING.md:45,51`；A 只在 `healpix_drizzle` 子树内 grep，漏掉了真正承载该门的 `shared/healpix` |
| A/B：O7 方差门对权重律零判别力 | **采纳并升级** | 我独立推导单叶下 `w` 精确相消；且我另加「滤掉的恰是多贡献者边界叶」这一更强表述 |
| B：截断 FITS 越界读（BLOCKER） | **采纳** | 我已读 `fits_reader.cpp:422-427,513-514` 并自行 grep 出 `drizzle_engine.cpp:2104` 的无界索引 |
| B：`BSCALE` 不可解析 → 全像素归零仍成功 | **采纳** | 我已读 `:63-70,491-496` |
| B：`clipPolygon` 污染通量守恒 | **否决（降级）** | 我 grep 出零调用点，把该项由阻断降为「须修·删除死码」，见反例 4 |
| B：`uncalibrated_adu_allowed` fail-open | **否决** | 我读 `drizzle_engine.cpp:1216` 的 `photometry_done` 不含该位 ⇒ 产品必被打 `PHOTAPPL=0`，属**正面诚实降级** |
| B：`ACSD_DRZ_FAULT` 可被用来产出降级产品 | **否决（降为建议）** | 注入只能把行推向 `overlap_area_invalid`，只会更严不会更松；仅剩卫生问题 |
| C：T1 oracle 自洽（BLOCKER） | **采纳** | 同反例 1（C 与 A 独立同证） |
| C：求和型通量门被当验收硬门 | **采纳** | 我核对 `DRIZZLE_GEOMETRY.md:320-324` 确载「只作辅判据，不作验收判据」 |
| C：`pf_sb_gate` 检查 (5) 恒真 | **采纳** | 我独立完成 `pf²/pf⁴` 代数推导，见反例 3 |
| C：kcorr 两个负例生产不可达 | **采纳** | 我独立推出 `n_eff ≡ 1/k` |
| C：`drizzle_trace` 私建 6 线程 | **采纳（但为测试面）** | 我已读 `:143-154`；同时**否决**其对 `:90 threads=1` 的指控——该选择在「竞争面在帧间」前提下是正确的 |
| C：nanoflann 与第三方声明不符 | **部分否决** | 「0 通过 0 失败」与 `expect_polluted` 两点我确认；但其对 astropy 的同类指控一并否决（同 A） |
| **我自己的新发现（任何子代理均未提出）** | — | ① `fits_reader.cpp:162` SIP 阶数 >5 系数静默丢弃而 `:394` 仍写完整 order；② `spherical_overlap_science.cpp:168` 失败路径不回填 `diag_out`，与本函数 `:154-157` 自订原则矛盾；③ `aio_publish.cpp:243` 父目录 fsync 丢返回值（对比 `:143-147` 同类失败被当硬错）；④ `module.yaml:71-79` 漏声明 `aio_publish.cpp` 全部 5 个导出符号；⑤ `Makefile:36,49,50` 三条悬空路径；⑥ `healpix_core.cpp:9` 退役声明证伪（13 处消费者）；⑦ kcorr committed 证据的独立复算（`max_dev=1.3007` → verdict=B，2.6× 跨档差异，零红灯）；⑧ nanoflann 内混排约 1240 行本仓 LiDAR 代码而版本宏仍称 1.9.0 |

**净结果**：采纳 20 条、否决 6 条、部分采纳 1 条；否决理由全部为「我亲自复跑取证后证伪」，其中 1 条（A 的 astropy 指控）推翻了子代理的高置信度结论。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# —— 覆盖率：与权威清单逐位对齐 ——
sed -n '202,229p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
for f in lib/algorithms/drizzle/healpix_drizzle/nanoflann.hpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_freeze_test.cpp \
         lib/algorithms/drizzle/healpix_drizzle/fits_reader.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_science_matrix_test.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_oracle.hpp \
         lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.h \
         lib/algorithms/drizzle/healpix_drizzle/tests/kcorr_matrix_test.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_pf_sb_gate.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_geom.hpp \
         lib/algorithms/drizzle/hips/src/aio_publish.cpp \
         lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_trace_concurrency_test.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_thread_probe.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/CMakeLists.txt \
         lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h \
         lib/algorithms/drizzle/healpix_drizzle/tests/representative_probe.cpp \
         lib/algorithms/drizzle/hips/module.yaml \
         lib/algorithms/drizzle/healpix_drizzle/Makefile \
         lib/algorithms/drizzle/healpix_drizzle/poly_clip.h \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh \
         lib/algorithms/drizzle/healpix_drizzle/.gitignore \
         lib/algorithms/drizzle/healpix_drizzle/healpix_core.cpp \
         lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_main.cpp \
         lib/algorithms/drizzle/src/acsd_p1_drizzle.def ; do printf "%6d %s\n" "$(wc -l < "$f")" "$f"; done | awk '{s+=$1} END{print "TOTAL LINES =", s}'   # → 9000

# —— B-01 T1 oracle 自洽：证明 oracle 调的正是引擎调的那三个函数 ——
sed -n '165,182p' lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_freeze_test.cpp
grep -n 'query_candidate_pixels_fast\|compute_overlap_area_g_ctx_cached' \
     lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp

# —— B-02 截断读取返回成功 + 尺寸不缩 ——
sed -n '420,430p;510,530p' lib/algorithms/drizzle/healpix_drizzle/fits_reader.cpp
grep -n 'pixels\[(size_t)y \* (size_t)img.width' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
grep -rn 'img\.pixels\.size()' lib/algorithms/drizzle/ --include=*.cpp --include=*.h   # → 无长度校验

# —— B-03 恒真门 ——
sed -n '220,264p' lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_pf_sb_gate.cpp
# 代数自查：pf=0.8 → 1-0.64-1e-6 = 0.359999 > 1e-3 ；pf=0.5 → 0.749999 ；pf=0.25 → 0.937499

# —— M-30 SIP 阶数>5 系数静默丢弃，而 order 写全 ——
sed -n '151,164p;392,400p' lib/algorithms/drizzle/healpix_drizzle/fits_reader.cpp

# —— M-29/M-27/M-32 悬空引用 ——
ls -d lib/algorithms/drizzle/healpix_stack                    # → 不存在
ls -d lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack  # → 仅此一处
(cd lib/algorithms/drizzle/healpix_drizzle && ls ../../common/healpix/healpix_core.cpp)  # → 不存在
(cd lib/algorithms/drizzle/healpix_drizzle && ls -d ../../astro_image_io)                  # → 不存在

# —— M-32 退役声明证伪：13 处真实消费者 ——
grep -rn --include=*.cpp --include=*.h -F '#include "healpix_core.h"' lib/algorithms/drizzle/

# —— M-20 生产 TU 内故障注入 + 门不存在 ——
sed -n '35,41p;75,81p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp
grep -rn 'ACSD_DRZ_FAULT' lib/          # → 仅 1 处（只有读方，无设置方）
grep -rn --exclude-dir=.git --exclude-dir=run 'p1_drz_area_invalid' .   # → 零命中

# —— M-21 诊断回填自相矛盾：:159 守了、:168 没守 ——
sed -n '151,170p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp

# —— M-25 / M-29 taskset 门 ——
sed -n '126,133p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/CMakeLists.txt   # 无 SKIP_RETURN_CODE
sed -n '16,24p;37,53p;54,61p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh

# —— M-23 探针打印 argv 而非实际预算 ——
sed -n '118,122p;133,136p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_thread_probe.cpp

# —— M-22 组选择器空匹配退出 0 ——
sed -n '143,160p' lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp

# —— M-26/M-28 死码证据 ——
grep -rn 'PolyClip::\|clipPolygon' lib/ --include=*.cpp | grep -v 'poly_clip.cpp'
grep -rn 'geom_overlap_area' lib/ --include=*.hpp --include=*.cpp
grep -rn 'DeterminismOracle' lib/ --include=*.hpp --include=*.cpp

# —— 反例 6：kcorr committed 证据的独立复算 ——
python3 -c "
import json;d=json.load(open('实验/engineering-evidence/science/kcorr_matrix.json'))
b=[c['k_corr'] for c in d['cells'] if c['pixfrac']==0.8 and c['scale_arcsec']==300.0][0]
print('baseline',b,'max_dev',round(max(abs(c['k_corr']-b)/b for c in d['cells']),4),
      'verdict','A' if max(abs(c['k_corr']-b)/b for c in d['cells'])<=0.10 else 'B')"
git -c core.quotepath=false ls-files -- '实验/engineering-evidence/science/kcorr_matrix.json'
grep -n 'kcorr_matrix_test\|representative_probe\|add_test' \
     lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt | sed -n '1,30p'

# —— M-31 nanoflann 本仓代码段 ——
sed -n '2706,2712p;3663,3670p;3883,3893p;3946,3947p' \
     lib/algorithms/drizzle/healpix_drizzle/nanoflann.hpp
grep -n 'NANOFLANN_VERSION' lib/algorithms/drizzle/healpix_drizzle/nanoflann.hpp
grep -n 'KDTreeSingleIndex\|n_thread_build' lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp

# —— astropy 声明属实（否决子代理指控）——
ls -la lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp
sed -n '5,8p;110,117p' lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp
sed -n '44,46p;50,53p' docs/science/algorithms/HEALPIX_MAPPING.md

# —— 纪律自证：工作树未被本审稿改动 ——
git -c core.quotepath=false status --porcelain | head -30
git -c core.quotepath=false rev-parse HEAD
```

---

**判定**：**阻断**。B-01（自洽式断言）与 B-02（静默降级致越界读）直接违反项目明禁项，B-03 是被文件头当作验收证据的恒真门；本片 100% 逐行读完，25/25 份成员零遗漏。
