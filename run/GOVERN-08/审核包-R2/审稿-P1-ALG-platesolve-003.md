# 审稿-P1 — ALG-platesolve-003（第 G08-05 遍 · 对抗性重读）

- **片号**：`ALG-platesolve-003`
- **层**：`lib/algorithms/platesolve`
- **基线**：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- **性质**：生产源码；本遍 = 对同一片材料的一次完整重读
- **纪律遵守**：零 git 写（未 add/commit/checkout/reset/stash）、零编译、零 ctest/pytest/二进制、未读 `/tmp/acsd_g08/`、未改任何仓内文件（本交付件除外）

---

## 1. 读完了吗

**口径声明**：`wc -l` 计数，与片清单 `实际行数: 7738` 一致（本人独立 `wc -l` 21 份求和 = 7738，逐份对齐）。

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | **21** |
| 本人打开并逐行读过的份数 | **21 / 21**（100%） |
| 成员总行数 | **7738** |
| **本人读到最后一行的份数** | **19 / 21** |
| **本人亲自读到的行数** | **6300 / 7738 = 81.4%** |
| 本人 + 2 名子代理独立全文覆盖后的行数 | **7738 / 7738 = 100%** |

**未由本人读到最后一行的（2 份，如实列出）**：

| 文件 | 本人实读 | 缺口 | 缺口如何补上 |
|---|---|---|---|
| `ipv_dead_params_selfcheck.py` (159) | 1–154 | **155–159**（末尾 5 行 print/`return`） | 子代理 `1ab68fcd` 独立读毕 159/159；缺口为纯收尾打印，不含判据 |
| `ipv_dead_params_manifest.json` (1513) | 1–40、586–625 | **约 1433 行未由本人亲读**（冻结计数表主体） | 两名子代理各自独立读毕 1513/1513 并逐项核对；本人在其结论基础上**复核了关键条目**（见 §3 F1/F4/F5 的行号证据，均由我亲自 `read`/`grep` 复现） |

**本人独立复核过的子代理关键断言**（不盲信，见 §7）：`vote_threshold` 判定分支、`ip_s.` 注入、`isfinite` 零命中、`run/perf-fix` 不存在、`test_synthetic` 未注册、CMake 有 `acsd_p1_ipv`、`aio_atomic_file.h` 位置、`test_kvector` 未注册、`ipv_angle.cpp` R 未钳位 —— 9/9 全部复现。

---

## 2. 本片判定：**阻断（BLOCKER）**

最重 3 条：

1. **【自洽式/假证明 · 本轮最高价值】死参数锁 `vote_threshold` 登记为「log_only · 不参与判定」，而它在生产链里就是一个真判定分支。**
   `ipv_dead_params_manifest.json:674-675` 声明该字段 `"status": "log_only"`、理由「读取值仅写入日志, 不参与判定」。实际：生产链函数 `IPVSolver::solve_from_memory` 中 `ipv_solver.cpp:851 int vote_threshold = params.vote_threshold;` → `ipv_solver.cpp:882 if (tri_result.max_vote >= vote_threshold) { … break; }`。**被检面与期望面用的是同一个定义式的两面**：清单用「生产判据是字面量 3」作期望，而真判据读的是配置值；两者只在 `ipv_solver.cpp:854 for (int n_target : {60})` 恰好是**单元素**列表时才偶然等价。该单元素集合**没有任何 `frozen_literal` 钉住**。把它改成 `{60,120}`，`vote_threshold` 与 `img_n_target` 同时变真，锁**全绿**。
   同族：`ipv_dead_params_manifest.json:591` 的 `"pattern": "\\bip\\.(?!log_dir\\b)[A-Za-z_]", "count": 0` 声称「无任何配置值注入 IpvParams」，但 `module_adapters.cpp:5075-5077` 正在注入 `ip_s.gaia_query_radius_factor` / `ip_s.m_lim_max_iter`——`\bip\.` 对 `ip_s.` 永不匹配，**这条 count:0 靠别名盲区才绿**。**两条 `count:0` 冻结事实源本身是仓库里的假陈述。**

2. **【fail-open · 验收边界自我关闭】`extract_wcs_sip` 的 fail-closed 尺度闸门在 `s0` 非法时不拒绝、只是不检查；残差闸门同时失效。**
   `ipv_wcs.cpp:734 if (std::isfinite(s0) && s0 > 0.0) {` 把整个 §8.5 合理性闸门（731-747）包在里面。全函数**从不校验 `s0`**（本人核对：`s0` 仅出现于形参 247、`:643` 除法、`:734` 守卫、`:736` 比值）。`s0` 非法时闸门静默消失。同时 `ipv_wcs.cpp:643 result->rms_px = result->rms_arcsec / s0;` 使残差门 `:753 rms_px > 0.5` 失效：`s0<0` ⇒ `rms_px≤0` ⇒ 恒假 ⇒ 任意大残差放行；`s0=+Inf` ⇒ `rms_px=0.0` ⇒ **上报「完美拟合」**。与同函数 `:343-353` 对 `det_lin` 的硬失败形成鲜明对比：**同一失效类两套语义**。

3. **【筛掉真信号 + 生产事实与文档相反】约 2700 行「核心管线」在生产链零可达，而 P27 机器锁把这一状态登记成合法，使缺陷被机器锁背书而非暴露。**
   `ipv_ransac.cpp`(1504) 与 `ipv_polygon.cpp`(806)+`ipv_angle.cpp`(198) 的全部公开符号在 `src/` 内**零调用点**（本人 grep 复核：`ipv_solver.cpp` 对 `ransac|full_verify|iter_trans_verify` 零命中；生产实路径是 `ipv_triangle.cpp` + `ipv_robust_refine.cpp`）。唯一仓内调用者 `test/test_synthetic.cpp` **未被任何 CMakeLists 注册**（本人复核 `grep -rn test_synthetic --include=CMakeLists.txt` → 空）。`Makefile:26` 仍把它编进 `ipv_solver.dll`，根 `CMakeLists.txt:1164` 仍编进 `acsd_p1_ipv`。而 `README.md:123` 反向失实称「根 CMakeLists 无 ipv 目标」。**约 2700 行代码零可执行测试覆盖、仍在编译、且被冻结清单登记为 dead。**

---

## 3. 逐文件清单

| # | 文件 | 行 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|---|
| 1 | `README.md` | 168 | 模块合同全文 | 冻结合同。**反向失实** `:123` 「根 CMakeLists 无 ipv 目标」（实为 `CMakeLists.txt:1164 add_library(acsd_p1_ipv STATIC …)`）；`:120-122` 称 `Makefile:6` 为 `-O3`（实为 `Makefile:9` 的 `-O2`，`-O3` 在 `build.ps1:27`）；§5 锚点大面积漂移（`:73-74` 称 CRPIX 在 `ipv_wcs.cpp:274-277`，实在 `:290-291`）；`docs/engineering/DATA_SEMANTICS.md`、`MODULE_MIGRATION_MATRIX.csv` 全仓不存在；`:3-6` 元信息块含日期/版本 + 「旧 README 重写取代 V4.30」历史叙事，违反 AGENTS §5 | **须修** |
| 2 | `wrapper_phase1/README.md` | 10 | L2 模块 README 全文 | **干净**。4 条引用（`wcs_tan.h`/`wcs_tan.cpp`/`p1_wcs_phot_test.cpp`/`contract_index.yaml`）逐条核实存在 | 通过 |
| 3 | `wrapper_phase1/wcs_tan.h` | 22 | `WcsTan` 结构 + 2 方法声明 | `pix2sky`/`sky2pix` **均返回 `void`**，无任何失败通道出参。与 `wcs_tan.cpp:61` 的「CD 奇异 → 返回 CRPIX」组合 ⇒ **假解不可观测**（DISP-WCS-001 同族，README §7 已登记但仍在 wrapper 侧存活） | **阻断（组合后）** |
| 4 | `ipv_sip.h` | 57 | `fit_sip` 声明 | 失败以 `order=0` 单一哨兵表达，无错误码；`:29` 的 `U[u].x = s*R*W[w] + t + A_poly(dx,dy)` 方向与 `:11` 的 `linear_pred` 相反（`:11` 是「预测 W」，`:29` 是「把 W 代入」），注释自相矛盾 | 建议 |
| 5 | `ipv_distortion.h` | 71 | `DistortionModel` + 2 函数声明 | `:32 bool valid = (n_pairs>=10 && \|k1\|<0.1)` —— 硬编码 0.1 无推导；`:53 tau_match = 15.0` 像素硬编码，应由 `s0` 导出；`:14 dx=(x-cx)*dr/r̃` 在 `r̃=0`（正中心）为 0/0；`:7` 指向 `ipv_cda_distortion_design.md §4` —— **全仓 0 文件**（该模块唯一设计依据指针） | 须修 |
| 6 | `Makefile` | 74 | 全文 | `:10 INCLUDES=-Iinclude`，但 `ipv_polygon.cpp:19`→`ipv_log_sink.h`→`aio_atomic_file.h`（实测在 `lib/infrastructure/aio/src/`）—— **默认目标编译必失败**；`:49 if not exist`/`:57 -lkernel32`/`:71-73 rmdir /s /q` 为 Windows-only；`:62-63 test` 目标**只跑 test_kvector**，test_mag_iter 不在其中；`:9 -fopenmp` 私建并行域（README §11 自承未接 ThreadBudget，即 DISP-WCS-005 未整改）。仓内已提交 `make_clean_err.txt` 失败日志且其 recipe 与现 Makefile 不一致 | **须修** |
| 7 | `ipv_ransac.h` | 101 | 5 个导出声明 | 5 个符号全部零消费者；`:36` 引 `vm44_fit.cpp` —— 全仓不存在；`:57` 「< 1ms」无实测依据的性能断言；`:79-80` 承诺「剔除残差 > 10*sigma」而实现实为 ≈2.9σ（见 §5 反例 4） | 须修 |
| 8 | `ipv_polygon.h` | 132 | 6 个声明 | **三处悬空契约**：`:85` 承诺 K=5→4→3→2 **四档**，实现只有 K=5/K=2 两档（`ipv_polygon.cpp:485-486`）；`:89` 承诺触发条件「max_vote<3 且 n_polygon_passed<n_pivot/2」，实际是 `ipv_polygon.cpp:538 max_v>=5.0` 单条件；`:123` 说取 top-1，实际 `ipv_polygon.cpp:742 K_w=3` 取 top-3；`:107/:114` 三处承诺 `tol=3*sigma_d`，实现把 `sigma_d` **注释掉**（`ipv_polygon.cpp:622 double /*sigma_d*/`）改用另一参数 | 须修 |
| 9 | `ipv_robust_refine.h` | 135 | `RobustRefineParams`(31 字段) + 主入口 | `:28-32` 自称「**唯一**生产调用点 (ipv_solver.cpp::solve_post_select)」—— **事实错误**：生产链上有两个（`ipv_solver.cpp:721`/`:1117`/`:1420` 共 3 个调用点，其中 `:1117` 的 `solve_from_memory` 在生产链内）。三处均传 `RobustRefineParams{}`，故「字段全不可达」的**结论**成立，但**理由是假的**且无任何锁定保护。`:116` 失败时回退并返回 `initial_trans`，`fallback=true` —— 调用方若只看 `success` 无法区分 | 须修 |
| 10 | `ipv_select.h` | 291 | 句柄访问器 + `MagIterOutcome` + 5 选星入口 | `:124` 「`density_match_iterate` 生产路径不再使用」—— **本人 grep 复核为真**（零调用点）；`:40/:44` 句柄为 `nullptr` 时「模块应返回错误」—— **复核为真**（`ipv_select.cpp:1018-1025`/`:1914-1921` 均 `return -1`）；`:102` m0 公式的 `6/1.5/2/13` 为散文硬编码，其中 `13.0` 静默压低可配的 `m_lim_clamp_hi`；`:163` 「flux 当前未使用」**为假**（`ipv_select.cpp:862` 用作循环上界）；`:7/:12/:14/:48/:51/:191` 六处「从 X 迁移」中 **X 被批量替换吃空**（「与⎵一致」「从⎵迁移」） | 建议 |
| 11 | `ipv_solver.h` | 294 | 结果结构 + `IPVSolver` 公共 API | **`:72` 冻结正本写 `NB_GRID=7`，实现 `ipv_wcs.cpp:417` 是 `NB_GRID=41`**（整改 7→41 后正本未同步）；`:70` CRPIX 指向 `:154`（实为 `cd2_2`）与 `:276`（实为 `cd11`），真值 `:161-162`/`:290-291`；`:74` Y-down 指向 `:542`（真值 `:657-711`）；`:108` 引 `25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md` —— 全仓 0 文件；`:241` 「返回 0 表示无缓存或求解失败」把两种语义塌缩成同一哨兵 | 须修 |
| 12 | `ipv_angle.cpp` | 198 | `wrap_*` + `circular_stats` + `angle_cyclic_verify` | `:78 R = sqrt(…)/m` **未钳位到 1.0**，`:81 sqrt(-2*log(R))` 在 R=1+ε 时得 **NaN**；`:192-193` 两道钳位对 NaN 均无效 ⇒ NaN 原样返回 ⇒ `ipv_polygon.cpp:409` 把 NaN 写入票阵 ⇒ `:791 std::sort` 比较器含 NaN 时**违反严格弱序（UB）**。**完美匹配恰是最易触发路径**。`:163-183` 把 **2Δθ 空间的 σ**（`:74-76`）与 **Δθ 空间的 dev**（`:169`）比 3σ，单位错配。`:191` `angle_tol_deg` 未校验，0 ⇒ 完美匹配时 0/0=NaN，负值 ⇒ consistency 恒 0（恒红方向且不报错） | 须修 |
| 13 | `ipv_polygon.cpp` | 806 | 6 个函数全文 | ① `:226-229` `occur[pr.first]+=1; occur[pr.second]+=1` 数的是**对端点出现次数**而非 `:180-181` 承诺的「出现在 ≥min_occur 个**查询**中」—— 一对同时落在 k=0/k=1 两窗即可单对刷满 `min_occur=2`；② `:612` 注释 `[0.05,0.95]×fov_diag` 与实现 `:659-660` 的 `0.1/0.9` 不符，`:613` 「>100 跳过」与实现 `:686 >200` 不符，「始终启用」与 `:658` 条件启用不符；③ `:157-158`（描述符侧用**过剪枝后**的 `selected[0]`）与 `:290-301`（验证侧用 **W 中未剪枝**最近邻）**d_ref 锚不同源**，`use_ratio` 路径带系统缩放误差；④ `:686 >200 跳过` 在密集场把最难的场型整对剔出投票，只进日志不升级错误码；⑤ `:436-438` 空 `namespace {}` 残留；⑥ `:405 alpha=0.5` / `:407 传死值 5.0°` 硬编码 | 须修 |
| 14 | `ipv_ransac.cpp` | 1504 | 全文（6 函数 + 匿名 ns 5 函数） | **见下方 6 条最重** | **阻断** |
| 15 | `ipv_wcs.cpp` | 1015 | 全文 | ① `:734` s0 闸门自我关闭（见 §2-2）；② `:526`/`:604` AP/BP 与 APx/BPx 拟合失败**只 `warn`** 后继续，`:654 result->success = true` 照设 —— 即 DISP-WCS-004 至今未改，且本片 README §7 已自认「无标志位」；③ `:643` 除以 `s0` 无守卫；④ `:70 if (max_val < 1e-15)` 对 NaN 恒假 ⇒ 含 NaN 的正规方程被判**非奇异**，`:519/:598` 打印「拟合成功」；⑤ `:930 f_prev=+inf` ⇒ `:998 f2 <= f_prev` 首轮恒真 ⇒ **牛顿首步永不阻尼**，与 `:926-927` 注释矛盾；⑥ `:762` 与 `:753` 的 `reject_code`/error 语义在 `wcs_sky_to_pixel_iterative` 中 `=2` 复用于 7 种失效面；⑦ `:658` 引 `ipv_select.cpp:687` 为 Y-up 依据（687 实为无关注释）；⑧ `:147` `default:` 把越界 `flip_mode` 静默降为 NONE | **阻断** |
| 16 | `test_mag_iter.cpp` | 351 | 9 组用例全文 | ① **`:251-252` 恒真门**：断言文本说「小 alpha → 步进更大」，实际只比 `m_lim_final >= m_seen[0]`；手工推演 prior=0.05 → 步长被钳到 6.0 → 收敛 15.9595，prior=0.2885 → 步长 4.0148 未钳 → 收敛 15.8792，**两种不同行为同满足一断言**，该门对「先验是否被消费」零分辨力；真正有分辨力的 `sa.m_seen[1]` 从未断言。② `:78-80 n_true()` 是 Stub `:64` 同一前向模型的复制，`:103` 阈值又用生产自己的 `p.density_tolerance` ⇒ **近自洽断言**（非严格同义反复：残差用连续 N、生产用整数，能抓判据错；抓不到容差被放宽/错配）。③ 金标准 `:130` `12.9850849855` 经手工解析复算**成立**（m0=11.86438→n1=85→step=1.120586→m2=12.984967→n2=178），是独立解析锁 | 须修 |
| 17 | `test_kvector.cpp` | 347 | 8 组用例全文 | **无自洽断言**：`:40-55 brute_force_query` 是**独立** O(M²) oracle，`:165/198` 用 `euclidean_dist` 独立重算，是本片质量最高的测试面。但 ① `:187-207 test_range_query` **无 `result.empty()` 断言** —— 永远返回 `{}` 的实现仍打印 `[PASS]`；② `:156-159` 前置条件不足时 `[SKIP]` + `return 0`（成功），整套仍打印 `[ALL PASS]`，违反 fail-closed；③ **整体未被任何 CMakeLists 注册**（本人复核），只由 Makefile 目标 `:62-63` 驱动，而该 Makefile 编译不过 | 须修 |
| 18 | `tools/ipv_abi_mirror.py` | 115 | 全文 | **干净**。全文**零文件读写**（无 `open`/无写回）⇒ **结构上不可能**出现「本次运行覆盖的产物成为下次输入」的自愈判据。锁定另一侧是独立编译的 C 探针，且 `ipv_abi_layout_lock` / `ipv_dead_params_lock` 均带 `_selfcheck` 阴性对照，符合「正确实现绿、注入缺陷红」 | 通过 |
| 19 | `test/ipv_dead_params_lock.py` | 375 | **全文（本人）** | **不是恒真门**（`main()` 无 try/except，`:237-238` 缺文件直接 `raise`；`except` 路径在 selfcheck `:86-89` 记为 `failures` ⇒ fail-closed）。但：① `:132-134 count_reads` 数的是**命中行数**不是引用次数，`:341` 却打印「逐字段引用点核对」；② `:254 ext_observed = sorted(unresolved)` 是**死变量**，`:252-253` 已把 `external_symbols` 白名单降级为「仅信息」，而 `:194-195` docstring 仍宣称「由清单的 external_symbols 白名单核对」且清单里**根本没有该 key**；③ `:256-262` 只把 `config_structs` 绑到头文件，`:267`/`:290` 只遍历 `fields` ⇒ **两套字段集合只验了一套**，新字段可零覆盖逃逸；④ `:366` 判红时指引用户同步 `run/perf-fix/P27-dead-params/REPORT.md` —— **该目录不存在**（本人 `ls` 复核） | 须修 |
| 20 | `test/ipv_dead_params_selfcheck.py` | 159 | 1–154（本人） | **4 个阴性对照真实有效**（P1 死字段新接线 / P2 真消费被删 / P3 死匹配器接线 / P4 常数换配置），锚点唯一性由 `:52-53` 强制。但：① `:114` P0 与 `:142` P5 参数**完全相同**（`None, False, None`），而 `case()` 每次开头 `:78-81` 清空并重拷影子树 ⇒ P5 与 P0 语义等价，`:15` docstring 与 `:154` 收尾打印宣称的「恢复后变绿」**未被检验**；② `:67` `--work` 无守卫；③ 4 个对照**全部只 patch `solve_post_select`**，生产链上的 `solve_from_memory` 零覆盖 | 须修 |
| 21 | `test/ipv_dead_params_manifest.json` | 1513 | 1–40、586–625（本人；其余由 2 名子代理独立读毕） | 冻结事实源。`:23-34 source_files` **不含** `ipv_polygon.cpp`/`ipv_ransac.cpp` ⇒ 6 个「dead」字段的判决靠「把持有活消费者的文件排除出扫描集」成立（`n_pivot` 真在 `ipv_polygon.cpp:364/:470` 被消费，`ransac_max_iter` 真在 `ipv_ransac.cpp:370` 被消费，`s_min/s_max` 真在 `:398/:521/:632/:1450/:1464` 被消费）。`:494-508 dead_symbols` 只列 5 个符号，**漏** `iter_trans_verify` 与 `full_verify_transform` ⇒ 二者被接线也不会红。`:588-593` 的 `count:0` 见 §2-1。`:3 generated_from_commit` 从不被校验 | **阻断（清单本身）** |

### 3.1 `ipv_ransac.cpp`（1504 行）最重 6 条

1. **`ipv_ransac.cpp:209-212` / `:223-226`：`umeyama_estimate` 对 `pairs[i].u` / `.w` 无下标校验即索引 `U[u]`/`W[w]`** —— 同文件 `:140-143` 的 `solve_similarity_transform` **校验了**。作为 public API（`ipv_ransac.h:37`）接受任意 `pairs`，是越界读。**阻断**（内存安全）。
2. **`ipv_ransac.cpp:453-454` vs `:475-478`（及 `:547/:568`、`:657/:678` 三处同构）：best 记账把宽阈值计数 `n_inliers` 当比较量、把紧阈值计数 `n_tight` 当存储量** —— 真解可被「宽内点多、紧内点为 0」的垃圾假设覆盖且不可恢复（见 §5 反例 1）。
3. **`ipv_ransac.cpp:1177-1184`：未收敛被写成 `result.success = true`** —— 注释自承「IPV 容错」；而 `:1329` 选优只看 `itr.success`、**从不检查 `converged`**。典型 fail-open。
4. **`ipv_ransac.cpp:1162-1173`：`final_lt.valid == false` 时保留上一轮旧 `result.lt`、不写任何日志，却照样设 `converged=true, success=true` 并把「导致 final_lt 奇异的那批点」写进 `result.inliers`** —— 返回的变换与上报的内点不同源，且退化征兆 100% 吞掉。
5. **`ipv_ransac.cpp:341`：`tau_tight = std::max(1.5, tau_wide*0.5)` 在 `tau_wide < 3.0` 时大于 `tau_wide`** —— 「渐进收紧」被静默反转；用户把 `ransac_inlier_threshold_arcsec` 调到更严（<3）时判定反而变松。硬编码地板 `1.5` 无出处。
6. **`ipv_ransac.cpp` 全文 `std::isfinite` 零命中**（本人 `grep -c` 复核 = 0）—— 所有退化守卫都写成 `if (x < eps)` 形式，而 `NaN < eps` 恒假 ⇒ NaN 穿透全部守卫。`ipv_select.cpp:876-880` 显式处理 `mag` 的 NaN，证明该输入在本项目内是现实的。

---

## 4. 发现清单

### 4.1 阻断（BLOCKER）— 3

| ID | 摘要 | 证据 |
|---|---|---|
| **B1** | 死参数锁两条冻结事实是假陈述：`vote_threshold` 声明 `log_only` 实为真判据；`no_ipv_params_from_config` 的 `count:0` 靠 `ip_s.` 别名盲区才绿 | `manifest:591,674-675`；`ipv_solver.cpp:851,882,854`；`module_adapters.cpp:5075-5077` |
| **B2** | `extract_wcs_sip` 尺度闸门在 `s0` 非法时自我关闭，残差闸门同时失效；`s0=+Inf` 上报「完美拟合」 | `ipv_wcs.cpp:734,643,753` |
| **B3** | 约 2700 行核心管线生产链零可达 + 零测试覆盖 + 仍在编译；`README.md:123` 反向失实；机器锁把该状态登记为合法 | `Makefile:26`；`CMakeLists.txt:1164`；`README.md:123`；`manifest:494-508` |

### 4.2 须修（MUST-FIX）— 22

| ID | 摘要 | 证据 |
|---|---|---|
| M1 | `umeyama_estimate` 越界读（无下标校验，与同文件 `:140-143` 不一致） | `ipv_ransac.cpp:209-212,223-226` |
| M2 | best 记账宽/紧阈值量纲混用，三阶段同构 | `ipv_ransac.cpp:453/475,547/568,657/678` |
| M3 | 未收敛 → `success=true`，且 `converged` 从不参与选优 | `ipv_ransac.cpp:1177-1184,1329` |
| M4 | `final_lt` 奇异静默吞掉，仍报 `converged/success=true` 且内点与变换不同源 | `ipv_ransac.cpp:1162-1173` |
| M5 | `ipv_ransac.cpp` 全文件零 `isfinite`，NaN 穿透全部退化守卫 | `grep -c isfinite` = 0 |
| M6 | `tau_tight` 下界 1.5 反转「渐进收紧」语义 | `ipv_ransac.cpp:341` |
| M7 | 文档承诺 10σ 剔除，实现实为 ≈2.9σ | `ipv_ransac.cpp:1114,1100` vs `ipv_ransac.h:80` |
| M8 | `iter_trans_verify` 只采样 vote 排名前 18 的候选，其余**永不尝试**；`break` 静默丢弃后续组 | `ipv_ransac.cpp:1276-1311,1292` |
| M9 | 三处 RMS 接受门互相矛盾（5.0 / 50.0 / 0.5px），单位标注 8 处错 | `ipv_ransac.cpp:757,1486`；`ipv_wcs.cpp:753` |
| M10 | AP/BP 与 APx/BPx 拟合失败只 `warn`，`success` 照设为 true（DISP-WCS-004 未整改） | `ipv_wcs.cpp:526,604,654` |
| M11 | `gauss_solve_wcs` 的 `1e-15` 对 NaN 恒假 ⇒ 含 NaN 正规方程被判非奇异并打印「拟合成功」 | `ipv_wcs.cpp:70,519,598` |
| M12 | 牛顿首步永不阻尼（`f_prev=+inf` 使 `:998` 首轮恒真） | `ipv_wcs.cpp:930,998` |
| M13 | `reject_code=2` 复用于 7 种失效面，无稳定错误码 | `ipv_wcs.cpp:862,920,990,1005,1010` |
| M14 | `ipv_angle.cpp` 未钳位 `R`，`R>1` ⇒ NaN ⇒ 票阵 NaN ⇒ `std::sort` 违反严格弱序（UB） | `ipv_angle.cpp:78,81,192-193`；`ipv_polygon.cpp:409,791` |
| M15 | `ipv_angle.cpp` 3σ 剔除把 2Δθ 空间的 σ 与 Δθ 空间的 dev 相比，单位错配 | `ipv_angle.cpp:74-76,92,169` |
| M16 | `collect_candidates` 的 `min_occur` 数的是对端点出现次数而非查询数，单对即可刷满 | `ipv_polygon.cpp:226-229` vs `ipv_polygon.h:54` |
| M17 | `geometric_vote` 把入参 `sigma_d` 注释掉改用另一参数，而 3 处文档承诺 `tol=3*sigma_d` | `ipv_polygon.cpp:622,646-650`；`ipv_polygon.h:107,114` |
| M18 | `use_ratio` 两侧 `d_ref` 锚不同源（过剪枝 vs 未剪枝）⇒ 系统缩放误差 | `ipv_polygon.cpp:157-158,290-301` |
| M19 | `ipv_polygon.h` 三处悬空契约（K 降阶档数/触发条件/top-K） | `ipv_polygon.h:85,89,123` |
| M20 | `test_mag_iter.cpp:251-252` 恒真门，对「alpha 先验是否被消费」零分辨力 | `test_mag_iter.cpp:251-252` |
| M21 | `count_reads` 数命中行数非引用次数，却以「引用点」名义冻结与打印 | `lock.py:132-134,341` |
| M22 | `fields` 与 `config_structs` 两套字段集合只验了一套；`external_symbols` 白名单降级为死变量 | `lock.py:256-262,267,290,252-254` |

### 4.3 建议（SUGGESTION）— 18（择要）

`README.md` 大面积锚点漂移与 2 处全仓不存在的文件（`:9-10,:20`）；`README.md:3-6` 违反 AGENTS §5；`Makefile` 缺 aio include 面致默认目标编译失败 + Windows-only + 已提交失败日志；`test_kvector.cpp:187-207` 空过、`:156-159` `[SKIP]` 返 0 却报 ALL PASS，且整体未被任何 CMake 注册；`selfcheck.py` P5 与 P0 参数完全相同、4 个对照只覆盖 `solve_post_select`；`lock.py:366` 指向不存在的 `run/perf-fix/…`（6 处同族引用）；`manifest` 的 `dead_symbols` 漏 `iter_trans_verify`/`full_verify_transform`；`ipv_robust_refine.h:30` 「唯一生产调用点」为假；`ipv_select.h:163` 「flux 未使用」为假；`ipv_distortion.h:7`/`ipv_solver.h:108` 指向全仓 0 文件；`ipv_polygon.cpp:436-438` 空 namespace 残留；`ipv_wcs.cpp:147` 越界 `flip_mode` 静默降级；`ipv_ransac.cpp:297` 文件头 T0 公式与 `:367` 实现不符；`ipv_polygon.cpp:612-613` 三处注释与代码不符。

---

## 5. 我主动构造的反例

**口径**：全部**手工逐步推演，未执行任何代码**（遵守「不跑二进制/不跑测试」）。每条给出「构造什么 / 期望推翻什么 / 是否推翻」。

### 反例 1 —— 推翻「PROSAC best 记账正确」（**推翻成功，本轮最强**）
- **构造**：`tau_wide=3.0, tau_tight=1.5`，M=20。假设 A 使 11 个残差落在 [1.5,1.9)（宽内点 11、**紧内点 0**、`rms≈1.6>good_rms_threshold=1.5` 故不早停）。假设 B 使 12 个残差落在 [1.5,3.0)（宽 12、紧 0）。
- **推演**：`:453` 判 `n_inliers(12) > best_n_inliers(11)` 成立 → `:475-478` 写入 `best_n_inliers=0`、`best_RMS=1e9`、`best_transform=tf_B`。A 被丢弃且此后不可恢复（任何宽计数 ≤12 的假设都进不来）。出口 `:757 success=false`、`n_inliers=0`。
- **期望推翻**：证明「比较量与存储量同量纲」。**是否推翻：推翻成功。** 另两名子代理独立构造出同类反例（A：10 紧→3 紧退化；B：11→0），**三方独立收敛**。
- **当前代码是否守住**：**否**。三阶段同构，无「best 不可退化」约束。

### 反例 2 —— 推翻「尺度闸门是 fail-closed」（**推翻成功**）
- **构造**：`s0 = +Inf`（生产可达：`ipv_select.cpp:1900` 只挡 `pixel_size_um <= 0.0`，`Inf<=0` 为假）。
- **推演**：`:734 if (isfinite(s0) && s0>0)` ⇒ **闸门整块跳过，不拒绝**；`:643 rms_px = rms_arcsec/Inf = 0.0`；`:753 !isfinite(0.0)` 假、`0.0>0.5` 假 ⇒ **放行**；`:761 n_pairs>=12` 若满足 ⇒ `success=true, rms_px=0.0` ⇒ 上报「完美拟合」。
- **期望推翻**：证明「验收边界不因非法输入而拒绝」。**是否推翻：推翻成功。**
- **另一支**：`s0<0` ⇒ `rms_px≤0` ⇒ `rms_px>0.5` 恒假 ⇒ **任意大残差放行**。

### 反例 3 —— 推翻「`vote_threshold` 是 log_only」（**推翻成功**）
- **构造**：`ipv_solver.cpp:854` 把 `for (int n_target : {60})` 改成 `{20, 40, 60}`。
- **推演**：`vote_threshold` 在 `solve_from_memory` 的命中仍为 `:851` 一行 = 冻结值 1 ✓；`img_n_pivot` 同理 ✓；`frozen_literals` 中**无任何一条** pattern 覆盖 `for (int n_target`（本人已核对 manifest:514-595 全部 10 条）；生产链闭包不变 ⇒ **锁返回 PASS**。而此时 `vote_threshold` 真正决定「是否扩充到 120」，`log_only` 与「不参与判定」彻底成为谎言且**无人报警**。
- **期望推翻**：证明「冻结事实源与源码语义一致」。**是否推翻：推翻成功**（两名子代理各自独立构造并确认）。

### 反例 4 —— 量化「10σ 剔除」文档偏差（**部分修正子代理数值**）
- **构造**：2 维残差 `r²/σ² ~ χ²₂`。求其 35 百分位：`1-exp(-x/2)=0.35 ⇒ x=0.8616`。故 `sigma = 0.8616σ²`；阈值 `10σ = 8.616σ²` ⇒ `r > √8.616 σ = 2.94σ`。
- **期望推翻**：`ipv_ransac.h:80` / `ipv_ransac.cpp:855` 均写「剔除残差 > **10\*sigma**」。
- **是否推翻**：**推翻，但幅度需订正**——实际约 **2.94σ**，不是子代理给出的 2.2σ（子代理用 `σ²₃₅≈0.49σ²` 有误；那是 |N| 分布而非 χ²₂）。文档与实现偏差近一个量级，结论不变、数值由我重算。

### 反例 5 —— `REQUIRED_PAIRS=3` 使收敛判据在边界恒真（**推翻成功**）
- **构造**：`fit_linear_trans` 拆成两个独立 3×3 系统（x 行 3 未知数、y 行 3 未知数），每对贡献 2 个方程。取 `working_set` 恰为 3 对 ⇒ 每系统恰 3 个方程、3 个未知数 ⇒ **恰定** ⇒ 残差**构造性恒为 0**（矩阵非奇异时）。
- **推演**：`sigma = good_d2[idx_35] = 0` ⇒ `:1106 if (sigma <= HALT_SIGMA(0.1))` ⇒ `is_ok=true` ⇒ `:1162` 跳出并写 `converged=true, success=true`。
- **期望推翻**：证明「收敛判据在最小工作集上有判别力」。**是否推翻：推翻成功**——在 `REQUIRED_PAIRS=3`（`:1018` 硬编码）边界上，该门**恒真**，任何 3 点拟合都被判为完美收敛。

### 反例 6 —— 试图推翻「`n_valid==0` ⇒ rms=0 ⇒ success=true」（**未能推翻，如实记录**）
- **构造**：`matched` 全部索引越界 ⇒ `:624-625` 静默 `continue` ⇒ `n_valid=0` ⇒ `:645-646 rms_px=0` ⇒ `:654 success=true`。
- **期望推翻**：证明存在「0 残差 + 成功」的假阳性出口。
- **是否推翻**：**被推翻（防线成立）** —— `:761 if (result->n_pairs < 12)` 拦住（`n_pairs=n_valid=0`）。**但**：`n_valid` 在 `[12, matched.size())` 区间时，`matched` 中被静默丢弃的越界对**不进 `n_pairs`**，调用方无从得知有多少对被吞。

### 反例 7 —— `angle_cyclic_verify` 的 3σ 剔除（**推翻「剔除有效」，但我发现子代理的一处推理需修正**）
- **构造**：(a) Δθ={+3,−3,+3,−3,+3} ⇒ 每点 `dev=3.0 > thresh=0.8547` ⇒ 5 点全剔 ⇒ `filtered.size()=0 < 3` ⇒ 不剔除、垃圾全留；(b) Δθ={0,0.01,−0.01} ⇒ 全留。
- **一处自我修正**：我最初怀疑 `:176-177` 的 `if (filtered.size() >= 3 && filtered.size() < m_eff)` 是**恒红门/反转条件**（只在「确实剔掉了东西」时才应用过滤）。**推演后否决**：若无点被剔，`dtheta_filtered == dtheta` 完全相同，跳过重算是**纯优化、结果逐位相同**。**记为否决候选，不报。**
- **保留结论**：该剔除因 M15（2Δθ-σ 与 Δθ-dev 单位错配）**既不保证剔除也不保证保留**。

---

## 6. 盲复算

**方法**：先取既有判定（遮蔽我的结论），再独立取证比对。

| 项 | 既有判定 | 我的独立取证 | 比对 |
|---|---|---|---|
| 本片 21 份的逐份判定 | 全部 **`默认保留（非产出面或非数据形态）`**（`分片清单/逐份判定-权威版.csv`，逐份核对） | 21 份全部含实质缺陷；3 份（`wrapper_phase1/README.md`、`ipv_abi_mirror.py`、`ipv_sip.h`）确属干净，其余 18 份需修 | **严重偏松** |
| `README.md` | 默认保留 | 冻结合同含反向失实 + 2 处全仓不存在的文件 + AGENTS §5 违规 | **偏松** |
| `ipv_ransac.cpp` / `ipv_polygon.cpp` | 默认保留 | 生产链零可达 + 零覆盖 + 6+16 条缺陷 | **严重偏松** |
| P27 三件套 | 默认保留 | 锁不是恒真门（这点既有判定未否），但两条冻结事实是假陈述、字段集只验一套、计数语义错 | **偏松** |
| `ipv_abi_mirror.py` | 默认保留 | 独立复核：**确属干净**（零文件 I/O，结构上排除自愈判据） | **一致** |

**结论：既有判定对本片整体「偏松」——21/21 份以「非产出面或非数据形态」为由零实质审查放行，而其中至少 3 条构成阻断。** 偏松的根因是把「不是产出面/不是数据形态」当成了免审理由，而本片恰是**算法正确性面**（判据、阈值、退化语义），与产出面无关。

---

## 7. 子代理派发记录

**派发 5 个**（覆盖 21 份，无重叠、无遗漏）：

| # | 子代理 | 负责文件 | 报告结论 |
|---|---|---|---|
| 1 | `1ab68fcd` | `ipv_dead_params_manifest.json` + `_lock.py` + `_selfcheck.py`（2047 行） | 1 BLOCKER（`vote_threshold`）+ 11 项；**明确否决**自指断言、自愈锚、fail-open、恒红门 |
| 2 | `d6235b3b` | 同上（独立重复派发，作盲复核） | 2 BLOCKER（`vote_threshold` + `ip_s.` 别名盲区）+ 10 项 |
| 3 | `c56bd9db` | `ipv_ransac.cpp` + `ipv_ransac.h` + `ipv_select.h` | 3 BLOCKER + 9 MUST-FIX |
| 4 | `f83491b7` | 同上（独立重复，作盲复核） | 2 BLOCKER + 8 MUST-FIX（与 #3 在 F3/F4/F6/F7 上收敛） |
| 5 | `1be4d632` | `ipv_wcs.cpp` + `ipv_solver.h` + `ipv_sip.h` + `ipv_distortion.h` + `wcs_tan.h` | 2 BLOCKER + 8 MUST-FIX |
| 6 | `ac11066b` | `ipv_polygon.cpp` + `ipv_polygon.h` + `ipv_angle.cpp` + `test_mag_iter.cpp` + `test_kvector.cpp` + `ipv_abi_mirror.py` + `Makefile` + 2×README | 5 MUST-FIX + 11 SUGGESTION |

> **如实说明**：第 1–2、3–4 组因我在同一消息内并行发起而被重复派发各一次（工具层行为，非我本意）。**结果正面**——两组各形成独立盲复核，**在关键结论上完全收敛**（`vote_threshold` 假陈述 ×2；宽/紧阈值记账缺陷 ×2）。实际派发 6 个子代理实例，覆盖 21 份无重叠。

### 7.1 逐条复核：本人独立复现的（9/9 全部成立）

| 子代理断言 | 本人复核方式 | 结果 |
|---|---|---|
| `ipv_solver.cpp:882` 是真判据、`vote_threshold` 非 log_only | 亲自 `sed` `:849-856,880-886` | ✅ 成立 |
| `module_adapters.cpp:5075-5077` 有 `ip_s.*` 注入 | 亲自 `sed` | ✅ 成立 |
| `ipv_ransac.cpp` 中 `isfinite` 零命中 | 亲自 `grep -c` = **0** | ✅ 成立 |
| `run/perf-fix` 目录不存在 | 亲自 `ls` | ✅ 成立 |
| `test_synthetic.cpp` 未被任何 CMake 注册 | 亲自 `grep -rn --include=CMakeLists.txt` → 空 | ✅ 成立 |
| 根 `CMakeLists.txt` 有 `acsd_p1_ipv`（README:123 失实） | 亲自 `grep -n` → `:1164 add_library(acsd_p1_ipv STATIC` | ✅ 成立 |
| `aio_atomic_file.h` 在 `lib/infrastructure/aio/src/`（Makefile 缺该 include 面） | 亲自 `find` | ✅ 成立 |
| `test_kvector.cpp` 未被任何 CMake 注册 | 亲自 `grep -rn --include=CMakeLists.txt` → 空 | ✅ 成立 |
| `ipv_angle.cpp:78-81` 的 `R` 未钳位 | 亲自 `sed` | ✅ 成立 |

### 7.2 我**否决**的子代理结论（4 条）

| 被否决项 | 提出者 | 我的否决理由 |
|---|---|---|
| **「`ipv_ransac.cpp:1100` 实际剔除门 ≈2.2σ」** | #3 | **数值错误。** 子代理用 `σ²₃₅≈0.49σ²`；2 维残差 `r²/σ² ~ χ²₂`，其 35 百分位为 **0.8616**（`1-e^{-x/2}=0.35`）。故实际阈值 `r > 2.94σ`。结论（文档说 10σ 有误）**保留**，数值**由我重算订正**。 |
| **「`ipv_angle.cpp:176-177` 的 `filtered.size() < m_eff` 是恒红门/反转条件」** | 我自己初判 + #6 隐含 | **否决。** 若无点被剔，`dtheta_filtered == dtheta` 逐位相同，跳过重算是纯优化，不改变结果。非缺陷。**记入 §5 反例 7 的自我修正。** |
| **「`umeyama_estimate` 返回 `s=0` ⇒ `prosac_verify` 假成功」应报 BLOCKER** | #3 提出后自行降级 | **接受其降级为 SUGGESTION。** 采样期 `tf.s ∈ [s_min,s_max]` 闸 + 两点解析解残差精确为 0，使被接受假设的 tight 内点集至少含两个**不同** `w`，`H≡0` 需测度零构型。**但代码无任何显式防御**，且 `umeyama_estimate` 作为 public API 可被直接调用 —— 我把 M1（下标无校验）提为阻断、此项留在建议。 |
| **「`ipv_angle.cpp:81` R 可达 1+ε ⇒ NaN」** | #6 | **接受但降级为须修而非阻断。** 推演成立（`sin²+cos²` 浮点值可 >1），但后果落在 `ipv_polygon.cpp` 的票阵上，而该文件生产链零可达（B3）。**按「死代码 + 零覆盖」定性为须修**，不虚报为阻断。 |

### 7.3 我**补正**的子代理盲区

- #3/#4 均未发现 `ipv_wcs.cpp:734` 的 s0 闸门自我关闭（由 #5 与**我本人独立阅读时同步发现**）。
- #5 未发现 `extract_consensus` 的 `top-K` 与 `ipv_polygon.h:123` 的矛盾（由 #6 与我本人发现）。
- 我本人补上：`:643 rms_px = rms_arcsec/s0` 与 `:734` 的**耦合**（子代理只分别报了「无校验」与「闸门失效」，未点出二者叠加才产生「上报完美拟合」）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log -1 --format='%H'          # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# --- 分母自证：21 份 / 7738 行 ---
sed -n '585,613p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
# 逐份 wc -l 求和 = 7738

# --- B1：vote_threshold 是真判据（锁称 log_only）---
sed -n '849,856p;880,886p' lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp
grep -n '"status": "log_only"' lib/algorithms/platesolve/cpp/ipv/test/ipv_dead_params_manifest.json
# 锁未钉住 {60}：frozen_literals 全部 pattern 里搜不到 "int n_target"

# --- B1 别名盲区：\bip\. 不匹配 ip_s. ---
sed -n '5074,5078p' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -n 'no_ipv_params_from_config' -A5 lib/algorithms/platesolve/cpp/ipv/test/ipv_dead_params_manifest.json

# --- B2：s0 闸门自我关闭 ---
sed -n '643p;734p;753p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp
awk 'NR>=243 && NR<=786 && /s0/ {printf "%d: %s\n", NR, $0}' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp   # 仅 4 处

# --- B3：零可达 + 零覆盖 ---
grep -rn 'ipv_ransac.h' lib/algorithms/platesolve/cpp/ipv/{src,include,test}
grep -rn "test_synthetic\|test_kvector" --include=CMakeLists.txt .        # 期望：无输出
grep -n "acsd_p1_ipv" CMakeLists.txt | head -3                            # 期望 :1164 add_library
sed -n '123p' lib/algorithms/platesolve/README.md                          # 失实断言
grep -c "isfinite" lib/algorithms/platesolve/cpp/ipv/src/ipv_ransac.cpp    # 期望 0

# --- M2：宽/紧阈值量纲混用 ---
sed -n '453,454p;475,478p;547,548p;568,571p;657,658p;678,681p' lib/algorithms/platesolve/cpp/ipv/src/ipv_ransac.cpp

# --- M3/M4：未收敛→success / final_lt 静默吞 ---
sed -n '1162,1173p;1177,1184p;1329p' lib/algorithms/platesolve/cpp/ipv/src/ipv_ransac.cpp

# --- M6：tau_tight 反转 ---
sed -n '337,343p' lib/algorithms/platesolve/cpp/ipv/src/ipv_ransac.cpp

# --- M5：REQUIRED_PAIRS=3 使收敛判据恒真 ---
sed -n '896,905p;1095,1100p;1104,1110p' lib/algorithms/platesolve/cpp/ipv/src/ipv_ransac.cpp

# --- M10：AP/BP 拟合失败只 warn，success 照设 true ---
sed -n '525,527p;603,605p;654p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp

# --- M14：R 未钳位 ---
sed -n '77,82p;190,195p' lib/algorithms/platesolve/cpp/ipv/src/ipv_angle.cpp

# --- M16：min_occur 语义 ---
sed -n '226,229p' lib/algorithms/platesolve/cpp/ipv/src/ipv_polygon.cpp
sed -n '54p' lib/algorithms/platesolve/cpp/ipv/include/ipv_polygon.h

# --- M17：sigma_d 被注释掉 ---
sed -n '616,623p;646,653p' lib/algorithms/platesolve/cpp/ipv/src/ipv_polygon.cpp

# --- M20：test_mag_iter 恒真门 ---
sed -n '250,253p' lib/algorithms/platesolve/cpp/ipv/test/test_mag_iter.cpp

# --- M21/M22：锁的计数语义与字段集缺口 ---
sed -n '122,135p;252,262p;267p;290p;341p;366p' lib/algorithms/platesolve/cpp/ipv/test/ipv_dead_params_lock.py
ls run/perf-fix 2>&1          # 期望：没有那个文件或目录

# --- M9/scope：robots ---
grep -n "ipv_api.h（238 行）\|无 ipv 目标\|DATA_SEMANTICS.md\|MODULE_MIGRATION_MATRIX" lib/algorithms/platesolve/README.md
wc -l lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h
grep -n "NB_GRID" lib/algorithms/platesolve/cpp/ipv/include/ipv_solver.h lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp

# --- 已核实的干净项（备查）---
grep -rn "density_match_iterate" lib/                    # 仅定义+声明，零调用点
sed -n '1018,1025p;1914,1921p' lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp   # 句柄为空的 fail-closed
git -c core.quotepath=false ls-files lib/algorithms/platesolve/cpp/ipv/test/__pycache__   # 无输出 ⇒ .pyc 未入库
```

**限制声明**：本遍未执行任何编译、ctest、pytest 或项目二进制；「哪个门实际是红的」**未经实测**，所有运行时数值行为结论均为源码级手工推演并已标注。反例 1/2/3/5 已推翻，反例 6 被既有防线挡住并如实记录。

---

## 9. 给前台的最小行动建议

1. **先定 B3 的去留方向**（删死代码 vs 补退役注释块）—— 在此之前，`ipv_ransac.cpp`/`ipv_polygon.cpp` 的任何数值改动都**不改变产品行为**。注意顺序：先补 `manifest` 的 `dead_symbols`（`iter_trans_verify`/`full_verify_transform`）再删文件，否则 `defined_in` 行号断言会先红。
2. **B1 是本轮最该立刻修的**：两条 `count:0` 冻结事实源（`manifest:591`、`manifest:674-675`）是**仓库里的假陈述**，且各有一行改动即可让它们被击败。同时订正 `ipv_types.h:218-221` 的「无任何注入」与 `ipv_robust_refine.h:30` 的「唯一生产调用点」。
3. **B2 一行可修**：`extract_wcs_sip` 入口加 `if (!std::isfinite(s0) || s0 <= 0) { success=false; error=...; return; }`，把 §8.5 闸门从条件块里提出来。
4. **恒红/恒真门三处零成本修复**：`test_mag_iter.cpp:252` 改断言 `sa.m_seen[1] - sa.m_seen[0]`（真正有分辨力的量）；`test_kvector.cpp:187-207` 补 `!result.empty()`；`:156-159` 的 `[SKIP]` 改 `return 1`。
5. `README.md` 的反向失实（`:123`）与 2 处全仓不存在的文件应优先清掉——它们会误导集成判断。