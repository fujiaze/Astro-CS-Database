# 审稿-P1 · ALG-platesolve-002（G08-05 对抗审稿第 1 遍）

- 仓库：`/workspace/Astro CS Database` ｜ 基线：HEAD = `850a9ede`
- 口径：负责人裁定「一遍 = 对同一片材料的一次完整重读」。本片 20 个成员文件**全部从头读完**。
- 本人立场：红队。默认现行结论是错的；任何「检查通过」一律不作为正确性证据。

> **第 2 轮修订说明**：本件在收到 **4/4** 份子代理报告后修订。修订内容含**对我自己第 1 轮结论的更正**——`test_select_domain.cpp` 原判「通过」，经子代理 B 举证 + 我独立复核，**该判定错误**，已在 §3/§4 更正为须修。其余新增条目均标注来源与我的复核结果。
>
> **第 3 轮增补**（子代理 C 文档核验返回后）：新增 **M13（工具内 0.5 px 系统性偏差）**、**M14（假退役措辞——负责人点名缺陷类的第二例）**、**M15（REPORT.md 中位数/均值分母不一致）**，并给 **B4** 补上机器锁自身的覆盖漏洞。全部经我独立取证。

---

## 1. 读完了吗

**计数口径**：`wc -l` 物理行（含空行/注释），与《片清单-权威版.yaml》`实际行数: 7753` 一致。覆盖率 = 实际逐行读 ÷ 成员总行。

| 项 | 数值 |
|---|---|
| 成员份数 | **20 / 20**（清单无缺项，磁盘无缺失） |
| 实际读完份数 | **20** |
| 成员总行数 | **7753** |
| 实际逐行读 | **7753** |
| **覆盖率** | **100.0%** |
| 未读完的文件 / 行 | **无 / 无** |

片外只读取证（不计入覆盖率）：`module_adapters.cpp:340-389,3745-3770,4681,5103`、`eng/tests/unit/CMakeLists.txt:1345-1360`、`eng/tests/unit/p1wcs/CMakeLists.txt:185-214`、根 `CMakeLists.txt:1-20,1420-1511`、`orchestrator.cpp:2013-2034`、`ipv_select.cpp:295-316`、`ipv_dead_params_manifest.json`、`.gitignore:15-20`。

---

## 2. 本片判定

### **判定：阻断（BLOCK）**

最重三条：

1. **`iterative_reproject` 在自己的失败出口上返回 `success=true`，且交出内部不一致的三元组**（`ipv_solver.cpp:346`）。中心在 `:254-255` 已前移，而 `:292-296` / `:300-303` 的 `break` 发生在**重拟合之前**，于是 `result.trans` 属于**旧中心**、`result.ra0/dec0` 属于**新中心**；调用方随即用新中心重建 `W_final`（`:626-642`）并与旧中心的 trans 一起交给 `extract_wcs_sip`（`:762`）。我构造的反例证明：对外上报的残差对 **477″ 中心偏差数值完全无感**。而唯一能发现它的 `convergence` 全仓**只被 5 处 `logger_.infof` 消费**，`ipv_solver.h:16-25` 里连 `converged` 字段都没有，调用方无从补救。
2. **`iter_trans_inner` 的 sigma-clip 循环恒定只跑一轮**（`ipv_itertrans.cpp:435`）。`bool is_ok = true` 是初值，唯一的 `=false`（`:583`）后面紧跟 `break`（`:584`），于是 `:594` 的 `if (is_ok) break;` 永远立即成立。`max_iterations=5` 是死参数，`:553/:576` 的「break 被注释掉」证明多轮是**原设计意图**。
3. **机器锁 `ipv_dead_params_lock` 把「把硬编码常量换成配置」列为必须判红的反例**（`test/CMakeLists.txt:183`），`ipv_api.h:275-276` 公开声明这些字面量被冻结。这不是记录缺陷，是**闸门主动防守缺陷并惩罚 AGENTS.md §6 要求的整改**——治理方向反了。

---

## 3. 逐文件清单

| # | 文件 | 行 | 读了什么 | 看到什么（关键） | 判定 |
|---|---|---|---|---|---|
| 1 | `src/ipv_solver.cpp` | 1836 | 全 4 块 | :346 success 丢弃 converged；:254-255 中心先移、:292-303 失败时未重拟合 ⇒ **trans 与 ra0/dec0 不一致**；:441/:854 单元素「自适应」+ :475 假日志；:651-709/1054-1108/1357-1411 三处逐字复制；:679 `rms>0?…:true` fail-open；:718/:1114/:1417 `sig` 当角秒用（实为角秒²）；:1224 先用后判；:1818 残差口径与 `ipv_api.h:246` 合同冲突 | **需修** |
| 2 | `src/ipv_itertrans.cpp` | 1057 | 全 2 块 | :435 单趟；:490-516 在算 sigma **之前**先剔掉 5-50″ 最差残差（筛掉真信号）；:602-648 上报 rms 只在三轮剔除后的幸存集上算；:835-842/:900-906 越界记 `dist2=0.0`（记成完美拟合）且 :879/:928 **把坏对发布进 `result.inliers`**；:541-548 sigma 钳制；:634/:863/:923 `sig`=角秒²；:336 注释称 tolerance「不再使用」但 :491/:541 两处在用；:698-701 恒真护栏 | **需修** |
| 3 | `src/ipv_triangle.cpp` | 744 | 全 2 块 | :17「不做 scale 约束」与 :342 实际施加**同文件互斥**；:18「自适应 20→40→60」不存在；:16「N=20」与 :288「1.17 亿次」互斥；:237 `AT_MATCH_RATIO` 悬空宏；:624 `success=(max_v>0)` 可与空 `top_pairs` 并存（生产侧被 `max_vote>=3` 救回）；:698-718 非预算失败被 `0>0` 吞掉、丢诊断；:728-730 假日志 | **需修** |
| 4 | `src/ipv_distortion.cpp` | 373 | 全文 | 全仓**零调用者**的死模块（仍被编入生产 target）；:164-174 按 `r_err` 取子集、:184-192 又在**同一子集**算 R² 分母；:286-317 换单参模型后 :322 仍上报**已丢弃**模型的 R²；:82/:97 `1/s` 无守卫；:31-33 除零；:348-352 `!dist.valid` 原样返回输入 | **需修**（虽为死码） |
| 5 | `test/test_select_domain.cpp` | 350 | 全文 | **⚠ 更正**：A/B/F 判据**确为真负向对照**（F2/F3 是正确形状）；但 **C 判据是恒真式**——`:184` 的期望侧 `40/60` 就是实现自身代数恒等式（两次调用仅第 5 参不同，分母按构造相同）；`:186` 断言 `nt<=60`，而实现是 `min(60,max(50,…))`，自证 `x<=x`；其「负向对照」`:189-195` 是测试内自算，从不调用 `compute_fov_density` | **须修**（第 1 轮误判为「通过」，已更正） |
| 6 | `test/test_extract_wcs_sip_failclosed.cpp` | 143 | 全文 | 断言 3（`:122-135`）为真判别力对照；case 2（`:104-108`）刻意与 case 1 解耦以便单独命中 `det<1e-15` 子判据，设计正确。弱点：`:53-54` 称注入「共线秩亏」，实际触发只是 Trans 系数为 0/1e-15，共线几何无因果作用 | **通过**（措辞失真） |
| 7 | `test/ipv_params_abi_failclosed_test.cpp` | 132 | 全文 | 五入口 + NULL 放行 + ABI 自洽放行，双向对照完整，非恒真 | **通过** |
| 8 | `test/ipv_abi_layout_probe.cpp` | 151 | 全文 | `:23-30` 四条 `static_assert` 是编译期真锁 | **通过** |
| 9 | `test/CMakeLists.txt` | 197 | 全文 | :22-25 目标**直编**生产源而不链接 `acsd_p1_ipv`（与其余 6 目标不一致，「同名同选项」不可核）；:70 声称 C 判据有负向对照——**经证伪**；:183 **把整改判红** | **需修** |
| 10 | `include/ipv_api.h` | 314 | 全文 | :152 把**生产入口**标为「实验性」；:246-248 残差合同与 `:1818` 实现**不是同一个量**；:271/:274/:285 三个锚**全错**；:286「24 字段」漏 `good_rms_threshold`；:227 指向的 `PUBLIC_API.md §P27` **实为 0 命中**；:309 证据指向未跟踪 `run/` | **需修** |
| 11 | `include/ipv_types.h` | 285 | 全文 | :59 声明 `sig` 为角秒、`:634` 实现为角秒²；:8/:89 引用**不存在**的 `vm44/45_types.h`；:91 注释称 U 是「角秒 50 颗」与 :37/:127 互斥；:221 漏一个字段；:247「自适应 20→40→60」不存在；:254/:262 的科学先验（α=0.2885、safety=3）**证据文件已不存在** | **需修** |
| 12 | `include/ipv_itertrans.h` | 163 | 全文 | :10「5px 半径贪心」、:149「按距离升序贪心」、:105「不做 sigma-clip」——**三处全与实现相反**；:59 量纲与实现矛盾 | **须修** |
| 13 | `include/ipv_triangle.h` | 96 | 全文 | :73-76 scale 约束说明正确，与 `ipv_triangle.cpp:17` 互斥；:27 `AT_MATCH_MINVOTES=2` 对应 `REPORT.md:1138` 自认的「过严门槛过滤真匹配」，至今未回退 | **须修** |
| 14 | `include/ipv_wcs.h` | 86 | 全文 | :19-23 仍以 `flip_mode`/`W_flipped` 描述 `build_wcs`，与 `ipv_types.h:132` 互斥；:6 include 已核实存在 ✓ | **须修** |
| 15 | `include/ipv_angle.h` | 53 | 全文 | :7 引用**不存在**的 `ipv_cda_distortion_design.md §5.3` | **须修** |
| 16 | `wrapper_phase1/wcs_tan.cpp` | 66 | 全文 | :61 CD 奇异 ⇒ 返回 `(crpix1,crpix2)`，无错误通道；**唯一调用方 `module_adapters.cpp:3759` 只挡 `isfinite`，挡不住有限值 CRPIX**，`:3763` 边界检查还会放过 → 是 DISP-WCS-001 的**未登记同族**，而该登记的整改范围只覆盖 `ipv_wcs.cpp`；:52-55 注释亲口承认「roundtrip 自洽掩盖」——本轮方法论的仓内背书；:35-36 RA 归一到 [-π,π]，与 `ipv_solver.cpp:99-100` 冲突 | **须修** |
| 17 | `REPORT.md` | 1154 | 全 2 块 | 通篇历史流水违 §5；:52-66 行数全错；:89「6 导出」实为 12；:191-197 已知问题已过时；:281-289 把 18× RMS 差归为「预期」；:514-517 引用 `ipv_select.cpp:268-269` 的 `n_target_cap` 但真实在 :315 且值不同；:843-849 十个证据 `summary.json` **全部不存在**；:960/:931 拿测量值与基线自身比较判 ✅；:1069-1076 的失败归因被同文 :1129-1132 自我推翻 | **需修** |
| 18 | `memory.md` | 113 | 全文 | :7「版本号 V4.30」与根 `VERSION`（`0.1.0-alpha.1`）冲突；:8 与 :47 两个「最新commit」互相矛盾且**都不是合法 git 对象**；:80 锚错；:87「selfcheck ALL PASS」是**文档可追溯性**自检，非解算正确性；:110-111 声称的可执行门 `p1wcs_closure_metric_gate` **已被从构建中移除** | **需修** |
| 19 | `tools/diag_gaia_psf_projection.py` | 393 | 全文 | :165-171 ABI fail-closed（正面）；:42/:51 路径失效；:331-332 硬编码 `512.0` 而 :304 同量现算；:239/:248 裸 `except` | **须修** |
| 20 | `.gitignore` | 47 | 全文 | :42 被 :48 的 `logs/` 覆盖，永不命中 | **建议** |

---

## 4. 发现清单

### 阻断（5）

**B1 · 失败出口返回 success=true，且 trans 与 ra0/dec0 内部不一致**
`ipv_solver.cpp:254-255` 先前移中心；`:292-296`（有效内点 <3）与 `:300-303`（重拟合失败）在**未重拟合**的情况下 `break`；`:331` MAX_ITERS 耗尽退出时 `converged` 仍为 false。三条路径统一落到：
`:339-346` `result.trans = trans_cur`（**旧中心**）、`result.ra0 = ra_cur; result.dec0 = dec_cur`（**新中心**）、`result.success = (inliers_cur.size() >= 3)`（**从不看 `converged`**）。
调用方 `:611/:1017/:1322` 的门 `!success || n_matched<3` 中第二子句被第一子句逻辑蕴含；随后 `:626-642` 用**新**中心重建 `W_final`，与**旧**中心的 trans 一起送进 `:762 extract_wcs_sip`。
`ipv_solver.h:16-25` 的 `IterativeReprojectResult` **只有 `double convergence`、没有 `converged` 布尔**，调用方无法区分。
→ §5 反例一证明上报残差无法兜底。

**B2 · sigma-clip 迭代恒为单趟**
`ipv_itertrans.cpp:435` `bool is_ok = true;`（初值）→ :552/:575 只置 true → :583 置 false 但 :584 立即 break → :593-594 `iters_so_far++; if (is_ok) break;`
⇒ 循环体恰好执行一次；`max_iterations`（`:997` 传 5）无效；`result.n_iterations`（`:650`）恒 1，上报「iters=1」看似干净收敛。`:322-324` 的流程文档与 `:553/:576` 的「不退出」注释均与代码相反。
附带：`:998` 传 `halt_sigma=0.0` 注释「0=不提前退出」，而 `:551` 的 `sigma <= halt_sigma` 实为「sigma 恰为 0 时才停」——哨兵语义反直觉。

**B3 · `Trans::sig` 量纲三方矛盾**
合同 `ipv_itertrans.h:59`「角秒」；实现 `ipv_itertrans.cpp:634/:863/:923` 取自 `dist2`（角秒²）；实现自认 `:870/:873`（按方差用 `sqrt(sig)`）；消费 `ipv_solver.cpp:718/:1114/:1417` 当角秒喂进 `robust_refine_wcs`。

**B4 · 机器锁把整改判红（治理反向），且锁自身有覆盖漏洞**
`test/CMakeLists.txt:183` 把「P4 常数换成配置」列为必红反例；`ipv_api.h:275-276` 公开声明字面量冻结。违 AGENTS.md §6「运行参数优先由 config 读取」。
**漏洞（子代理 C 举证，我已独立核实）**：`ipv_api.h:306-307` 声称「死匹配器被接线…任一发生即变红」，但 `ipv_dead_params_lock.py` 只在 `manifest.source_files` 的 **10 个文件**内扫描死符号，而 `ipv_polygon.cpp` / `ipv_angle.cpp` / `ipv_distortion.cpp` **均不在该清单**，且未解析的外部符号被显式不断言。⇒ 恰恰是那些持有死匹配器定义的源文件**逃出了这张网**；selfcheck 的 P3 用例之所以能过，只因它改的是清单内的 `ipv_solver.cpp`。

**B5 · 对外 inlier 残差字段的合同与实现不是同一个量**
`ipv_api.h:246-248`「`residual_x_px` = det_x − pred_x」；`ipv_solver.cpp:1818` 实为 `(ξ_实际 − ξ_预测)/s0`（角秒域差 ÷ 各向同性 s0）。该数组是「WCS Gate v2 双层闭环」A 层输入。

### 须修（12）

| 编号 | 位置 | 问题 |
|---|---|---|
| M1 | `ipv_distortion.cpp:62/:338` | 零调用者的 373 行死模块仍被编入生产 target |
| M2 | `ipv_itertrans.cpp:490-516 → :602-648` | **筛掉真信号**：在算 sigma 之前先剔掉 5-50″ 最差残差，之后上报的 `sig`/`rms` 只在「绝对50″ → tol → 相对10σ」三轮剔除后的幸存集上计算，无 `p95`/`max` 暴露；与 `memory.md:100-102` 冻结口径要求「必须同报 n_matched/match_rate/p95/max」冲突 |
| M3 | `ipv_itertrans.cpp:835-842, :900-906` + `:879/:928` | 越界记 `dist2=0.0` 冒充完美拟合，拉低 `sig`，**并缩小 `:873` 的裁剪阈值误伤正常对**，坏对还被发布进 `result.inliers`。当前不可达，但公共 API 直接调用即触发 |
| M4 | `ipv_distortion.cpp:164-174 → :184-192, :286-322` | R² 分母在幸存集上算（系统偏负）；R²<0 换单参后仍上报**已丢弃**模型的 R² |
| M5 | `ipv_distortion.cpp:348-352, :82/:97, :31-33` | `!dist.valid` 原样返回输入（fail-open）；`1/s` 与宽高无零守卫 |
| M6 | `ipv_solver.cpp:441/:854`、`ipv_triangle.cpp:662` | 单元素「自适应」+ `:475/:728` 宣告不可能发生动作的假日志 |
| M7 | `wcs_tan.cpp:61` | **DISP-WCS-001 的未登记同族**；唯一调用方 `module_adapters.cpp:3759` 只挡 `isfinite`，CRPIX 有限 ⇒ `:3763` 边界放行 ⇒ `:3768` fail-closed 永不触发 |
| M8 | `test_select_domain.cpp:184, :186, :189-195` | **本轮最高价值新增**：C 判据恒真（期望侧即实现自身代数恒等式）+ `nt<=60` 对 `min(60,max(50,…))` 自证 + 其「负向对照」从不调用被测函数 ⇒ `test/CMakeLists.txt:70` 的「非恒真门」声称对 C **被证伪** |
| M9 | `ipv_api.h:271/:274/:285` | P27 裁决依据三个锚全错（真 `p1_op_wcs` 在 `module_adapters.cpp:4681`，实调 :5103；真 `solve_post_select` 在 `ipv_solver.cpp:1198`） |
| M10 | 悬空引用 | `ipv_cda_distortion_design.md`（4 处）、`vm44/vm45_types.h`、`AT_MATCH_RATIO`、`PUBLIC_API.md §P27`（**0 命中**）、`run/perf-fix/**`、`engineering_v1.2/**`、`logs/siril_compare/**` |
| M11 | `ipv_types.h:254/:262` + `memory.md:110-111` | **科学常数的证据已消失**：α=0.2885（实测 0.243-0.456, R²>0.986）与 safety=3（10 帧样本）所依据的 `run/perf-fix/P4-magiter/REPORT.md` 不存在；`memory.md` 声称的可执行门 `p1wcs_closure_metric_gate` 已被 `eng/tests/unit/p1wcs/CMakeLists.txt:185-190` 显式移除，而 `GATES_AND_TOLERANCES.md:67` 仍以其为状态 Y 的可执行门 |
| M12 | `REPORT.md` | 行数表全错（实 10699 vs 声称 ~3500）；「6 导出」实为 12；`:514-517` 代码锚指向无关声明；`:843-849` 十个证据文件**全不存在**；`:960/:931` 拿测量值与基线自身比较判 ✅；`:1069-1076` 的失败归因被同文 `:1129-1132`（det 恒为 0，含成功帧）推翻；**时序倒置**（V4.28 记 2026-07-09 排在 :863，V4.22 记 2026-07-05 却在 :1004） |

| M13 | `tools/diag_gaia_psf_projection.py:12-13, :305, :330` | **文档承诺的 -0.5 对齐从未执行**。docstring 明写「astropy WCS 的 FITS 像素坐标减去 0.5 对齐」，但全文 `0.5` 只出现在 :12（docstring 自身）与 :381（绘图标注线）；`:305`/`:330` 直接用 `all_world2pix(...,1)`（1-based）与 sdet 的「索引+0.5」连续坐标相减 ⇒ 工具打印的**每一个偏移量都带恒定 +0.5 px 偏差**。这正是本轮要找的「合同与实现不是同一个量」，且它自己写在同一文件的 docstring 里 |
| M14 | `include/ipv_types.h:222-223` | **假退役措辞——负责人点名缺陷类的第二例**。该处写 `dead (0 引用点)`，但 `n_pivot` 实被 `ipv_polygon.cpp:364,470` 读取、`ransac_inlier_threshold_arcsec` 实被 `ipv_polygon.cpp:647-648` 与 `ipv_ransac.cpp:331,340,370,1022,1296,1369` 读取。**实质**（生产路径确实不读）成立——A/D 按生产路径口径判 TRUE 是对的；**但「0 引用点」这一措辞字面为假**。对照 `ipv_api.h:290` 的写法「生产路径 0 读取点」才是准确的。另：`good_rms_threshold` 被 `ipv_ransac.cpp:486,579,589,713` 读 4 次，却不在本 dead 清单里（只在 manifest 里），**且它在 `IpvParams` 中没有 ABI 载体**——用户配置根本无处可设 |
| M15 | `REPORT.md:1072` | **筛掉真信号（报告层）**。「中位 1.669px, 均值 1.577px」：均值 1.577 取自 `:1050-1066` 全部 **12** 个成功行（=1.5769 ✓），中位 1.669 却是**剔除宽视场那一行（0.352，全组最优）后 11 行**的中位数。12 行真中位 = **1.6515**。同一句话里两个统计量用了**不同分母**，且被剔除的恰是最好的一条 |

### 建议（14）

S1 `ipv_triangle.cpp:624` `success=(max_v>0)` 可与空 `top_pairs` 并存（生产侧被 `max_vote>=3` 救回，公共 API 潜伏）。
S2 `ipv_triangle.cpp:698-718` 非预算失败因 `0>0` 不成立而被丢弃、丢诊断（`budget_exhausted` 路径反而处理正确）。
S3 `gauss_solve`（`ipv_itertrans.cpp:92-132`）对未归一化正规方程用绝对阈值 `1e-15`；order=3 @4096² 实测 `cond(M)=6.15e19`，超 double 的 15.95 位（见 §5 反例二）。
S4 `ipv_itertrans.cpp:698-701` 恒真护栏（`U_pred` 按 `U.size()` 构造）。
S5 `ipv_itertrans.cpp:336` 注释称 tolerance「保留兼容、相对阈值不再使用」，实则 :491/:541 两处在用。
S6 三处静默替值：`ipv_distortion.cpp:152/:221`、`ipv_solver.cpp:719/:1115/:1418`。
S7 `ipv_solver.cpp:1224` 在 `selection.success` 校验（:1239）之前调用 `triangle_match`。
S8 `ipv_solver.cpp:651-709/1054-1108/1357-1411` 三段逐字复制。
S9 `ipv_itertrans.h:10/:105/:149` 三处算法描述与实现全反。
S10 **`test_synthetic.cpp` 与 `test_kvector.cpp` 从未被任何 CMake 目标构建**（实测 CMake 引用数 = 0），而 `REPORT.md:64-65/:105-116` 却以二者的「15/15 通过」「ALL PASS」作为证据；`eng/ci/checks.json` 不存在 ⇒ 本套测试**无 CI 门**。
S11 `.gitignore:42` 被 :48 覆盖，永不命中。
S12 `diag_gaia_psf_projection.py:331-332` 硬编码 `512.0`（同文件 :304 现算）；:42/:51 路径失效；:239/:248 裸 `except`。
S13 `memory.md:8/:47` 两个 commit 非合法 git 对象；:87「selfcheck ALL PASS」属文档可追溯性自检，非解算正确性。
S14 `REPORT.md` / `memory.md` 通篇历史流水违 AGENTS.md §5，且负责人「实验域只留正向代码与报告，具体运行结果归档不留」正指向此类文件。

---

## 5. 我主动构造的反例

### 反例一（决定性）· 残差对未收敛完全无感
40 颗像素星（4096² 帧，U 中心原点）、`s0=0.968"`、带小旋转的真实线性映射；分别以**正确切点**与**偏离 477″ 的错误切点**生成 W，各自按 `ipv_itertrans.cpp` 的 `monomial_basis(3)` 最小二乘拟合，再按 `ipv_solver.cpp:1810-1819`（`get_last_inliers` 口径）算上报残差。

```
=== 报告量 (in-sample residual; get_last_inliers:1818 的口径) ===
  收敛解  : rms=0.000001"   ||x00,y00||=14.1510"
  未收敛解: rms=0.000001"   ||x00,y00||=491.4957"
  rms 比值 = 0.623836
=== 真实中心误差 ===
  收敛解  中心偏差 = 14.1510"
  未收敛解 中心偏差 = 477.3471"
```

**期望推翻**：上报残差可作为未收敛的兜底判据。**结果：彻底推翻**——两个解的上报 RMS 都是机器精度 1e-6″，对 477″ 中心偏差毫无区分。
**机理（即负责人点名的自洽式）**：`ipv_solver.cpp:1810` 用 `apply_trans(trans,…)` 算「预测」，而 `trans` **正是用这批点拟合的**，且 `x00/y00` 把中心偏差**吸收进常数项**（491.50 ≈ 477.35 的分量）。**残差与中心偏差结构性正交**，它不可能发现 `converged` 要发现的那类缺陷——被检量与期望量同源。

**仓内三方独立佐证**（非我构造）：`memory.md:55-56`（5.9×/7.2×）、`REPORT.md:281-289`（18×，被归因为「预期」）、`memory.md:105-109`（2.8×/3.4×/1.9×）。三处都把同一现象当正常。

### 反例二 · 正规方程条件数超出 double 表示能力
```
cond(A) = 8.204e+09 ;  cond(M=A^T A) = 6.150e+19   (正方程使条件数平方)
```
**期望推翻**：`calc_trans_general` 的高斯消元在 4096² 帧上数值可靠。**结果：推翻**——`ipv_itertrans.cpp:214-241` 用未中心化、未归一化的原始像素坐标构造正规方程。6.15e19 意味着丢掉 19.8 位十进制精度，而 double 只有 15.95 位；`:105` 的 `max_val<1e-15` 作用在跨 1…1e19 的矩阵上无物理意义。对照收敛门是 **0.01″**（仅 2 位有效数字）。

### 反例三 · REPORT.md 自证其统计量分母不一致
直接用 `REPORT.md:1050-1066` 表内 12 个成功行的 RMS 重算：
```
rows(pass)=12
median all12      = 1.6515      <- 12 行的真中位
mean   all12      = 1.5769      <- 与 REPORT.md:1072 的「均值 1.577」吻合
median excl-wide(11) = 1.669     <- 与 REPORT.md:1072 的「中位 1.669」吻合
```
**期望推翻**：「中位 1.669px, 均值 1.577px」出自同一组 12 帧。**结果：推翻**——均值用 12 行、中位用剔除宽视场（0.352，全组最优）后的 11 行。同句两个统计量分母不同，且**被筛掉的恰是最好的一条**，方向使报告显得更差（1.669 > 1.6515），属保守方向；但它仍是「先筛子集再取统计量」且未声明。

### 反例三 · REPORT.md 自证其统计量分母不一致
直接用 `REPORT.md:1050-1066` 表内 12 个成功行的 RMS 重算：
```
rows(pass)=12
median all12      = 1.6515      <- 12 行的真中位
mean   all12      = 1.5769      <- 与 REPORT.md:1072 的「均值 1.577」吻合
median excl-wide(11) = 1.669     <- 与 REPORT.md:1072 的「中位 1.669」吻合
```
**期望推翻**：「中位 1.669px, 均值 1.577px」出自同一组 12 帧。**结果：推翻**——均值用 12 行、中位用剔除宽视场（0.352，全组最优）后的 11 行。同句两个统计量分母不同，且**被筛掉的恰是最好的一条**，方向使报告显得更差（1.669 > 1.6515），属保守方向；但它仍是「先筛子集再取统计量」且未声明。

### 被我推翻的反例（如实记录）
- **「本片测试可能从未注册」** → 推翻。钩子在 `eng/tests/unit/CMakeLists.txt:1352`，根 `CMakeLists.txt:1473`，守卫 `if(ACSD_BUILD_TESTS)`（:1463），`option(... ON)`（:19）默认 ON。
- **「算法模块私建线程池」** → 推翻（详见 §7）。
- **子代理 B 的「`run/` 未被 gitignore、`run/perf-fix/` 存在但为空」** → **推翻**。根 `.gitignore:17-18` 明文 `run/*` + `!run/.gitkeep`；`git ls-files run/` 仅 2 个文件；`test -d run/perf-fix` → **不存在**。子代理此处结论错误，我保留自己经 `git` 直接取证的一版。

---

## 6. 盲复算

**口径**：先遮住 `分片清单/逐份判定-权威版.csv` 对本片 20 份的既有判定，独立取证后再对照。

- 本片 20 份的既有判定**全部**是同一句 `默认保留（非产出面或非数据形态）`，`tier=HUMAN`，`reason` 无差异化 ⇒ **本片此前的实质审核覆盖率为 0**。

| 维度 | 结论 | 依据 |
|---|---|---|
| 既有判定 vs 本次 | **偏松（严重）** | 既有把 20 份全判「默认保留」，其中含 B1 fail-open、B2 死循环、B3 量纲矛盾、B4 治理反向、M8 恒真门 |
| 我 vs 子代理 A | **判一致** | 独立得出同一条阻断（`ipv_solver.cpp:346`）；其 `is_ok` 条目与我 B2 独立重合 |
| 我 vs 子代理 B | **子代理 B 推翻了我** | B 举证 `test_select_domain.cpp:184/:186` 为恒真式；我独立复核 `ipv_select.cpp:315-316` 确认 `min(60,max(50,…))`，**接受更正**，原「通过」判定作废 |
| 我 vs 子代理 D | **部分一致、一处分歧** | 一致：`:346`（D 补出 trans/ra0/dec0 不一致）、越界记零并发布、M2 caller 穿透。分歧：OpenMP（见 §7），我维持否决 |
| 我对「恒真/恒红门」的预期 | **偏严** | 三个 `*_failclosed*` 与 A/B/F 判据**确有真负向对照**，比我预期好；真正的自洽式在**生产代码残差口径**与 **C 判据**，不在 failclosed 测试里 |

**诚实声明**：我**没有**把 `审稿-RR*.md / R2-* / R3-* / P1-*` 当证据；「既有判定」只取自权威 CSV 的机器分类字段，实质结论均出自我逐行阅读 + 自建反例 + 子代理举证后的**我方复核**。

---

## 7. 子代理派发记录

**派出 4 个**（`subagent`，独立上下文；零 git 写、零编译、禁读 `/tmp/acsd_g08/`）。**4 份全部返回**；每条结论我只采信其**可复现证据**，并自行复核后才落判。

| 代理 | 任务 | 状态 | 回报量 |
|---|---|---|---|
| A `88280f98` | 静默降级 / fail-open | ✅ | 1 阻断 7 须修 10 建议 + 8 自否决 |
| B `a7a69b80` | 自洽断言 / 恒真恒红门 / 筛掉真信号 / 自愈锚 / 测试接线 | ✅ | 20 项报告核验 + 10 自否决 + 4 条恒真/恒红门 |
| C `dc4ee202` | 悬空引用 / 退役声明 / 数值复算 / 硬编码 / 历史叙事 | ✅ | 17 条悬空引用、12 项退役声明、~30 项数值重算 + 8 自否决 |
| D `77c5d6c9` | 静默降级（与 A 独立重复） | ✅ | 2 阻断 4 须修 14 建议 + 8 自否决 |

### 逐条复核：采纳项（均经我独立取证）

| 来源 | 条目 | 我的复核 | 处置 |
|---|---|---|---|
| A | `:346` 丢弃 `converged` | grep 全仓 `.convergence` 消费点＝5 处，全在 `logger_` 内 | 采纳为 **B1** |
| A | `ipv_itertrans.cpp:435 is_ok` | 逐行核 :435/:552/:575/:583/:584/:594 | 采纳为 **B2** |
| A | 悬空 `ipv_cda_distortion_design.md` / `vm44/45_types.h` | `git ls-files` 0 命中 | 采纳为 **M10** |
| A | 畸变模块零调用者 | grep 全仓，命中仅声明/定义/自身日志 | 采纳为 **M1**（并自补 M4/M5） |
| B | **`test_select_domain.cpp:184` 恒真** | 重推：`rho40/rho60 ≡ 40/60`（两次调用仅第 5 参不同，分母按构造相同） | 采纳为 **M8**，**推翻我自己** |
| B | **`:186` 恒真门** | 读 `ipv_select.cpp:315-316` = `min(60,max(50,…))`，输出按构造落在 [50,60] | 采纳为 **M8** |
| B | `:189-195` 负向对照不调被测函数 | 重读原文确认 `legacy_select` 为测试内 helper，`compute_fov_density` 从未以它调用 | 采纳为 **M8** |
| B | `test_synthetic.cpp`/`test_kvector.cpp` 未注册 | 实测 CMake 引用数 = **0** | 采纳为 **S10** |
| B | REPORT 十个 `summary.json` 缺失 | `ls lib/algorithms/platesolve/logs/` → **不存在** | 采纳为 **M12** |
| B | `p1wcs_closure_metric_gate` 已从构建移除 | 采信并交叉 `memory.md:110-111` 与 `GATES_AND_TOLERANCES.md:67` | 采纳为 **M11** |
| B | `ipv_select.cpp:315-316` 夹取 | 直接读原文 | 采纳为 **M8** 佐证 |
| D | `:254-255` 中心先移 + `:292-303` 未重拟合 ⇒ **trans 与 ra0/dec0 不一致** | 逐行核对，并追 `:626-642` → `:762` | **采纳并升级 B1**（比我的原描述更锋利） |
| D | `ipv_itertrans.cpp:879/:928` 坏对被发布进 `result.inliers` | 重读确认 | 升级为 **M3** |
| D | `module_adapters.cpp:3759` 只挡 `isfinite`，CRPIX 有限故穿透 | 读 `:3756-3770` 确认 | 升级 **M7** 为「未登记同族」 |
| D | `ipv_distortion.cpp` 仍被 `CMakeLists.txt:1167` 编入 | 采信 | 并入 **M1** |
| D | 9 项退役声明核查**全部为真** | 与 A 独立结论一致（`build_wcs`、`vote_threshold`、7 个 dead 参数、`img_n_target`） | **本片无假退役声明（按生产路径口径）** |
| C | **`ipv_types.h:222`「dead (0 引用点)」字面为假** | 我重读 `:222-223` 原文确为「0 引用点」；`n_pivot`/`ransac_*`/`s_min`/`s_max` 确有 `ipv_polygon.cpp`、`ipv_ransac.cpp` 读取点 | 采纳为 **M14**，并**明确与 A/D 调和**：实质（生产路径不读）为真，措辞为假 |
| C | **机器锁覆盖漏洞**：`source_files` 10 项不含 `ipv_polygon.cpp`/`ipv_angle.cpp`/`ipv_distortion.cpp` | 我解析 manifest：`source_files` 长度 10，三者 `listed? False` | 采纳，**并入 B4** |
| C | **`.py` 的 -0.5 对齐从未执行** | 我 grep 全文 `0.5` ⇒ 只在 `:12` docstring 与 `:381` 绘图标注；`:305/:330` 确为 `all_world2pix(...,1)` | 采纳为 **M13** |
| C | **REPORT.md:1072 中位/均值分母不一致** | 我用其表内 12 行独立重算（§5 反例三） | 采纳为 **M15** |
| C | 17 条悬空引用（含 `python/`、`obj/`、`.trae/specs/`、`lib/plate_solve_old/v4_archive/`） | 采信并并入 **M10**（已自行核验其中 5 条） |
| C | `REPORT.md` 时序倒置、`-ffast-math` 与 `build.ps1:27` 不符、dll 尺寸四个互斥数字 | 采信并入 **M12**（时序倒置我原读时已注意到） |
| C | `.gitignore` 反向缺陷：`build_err*.txt`/`build_out*.txt` 匹配不到任何东西，实际入库的 `make_clean_err.txt`/`make_clean_out.txt` 反而不被任何规则覆盖且被跟踪 | 与我自查出的 `:42` 被 `:48` 覆盖构成镜像对 | 并入 **S11** |

### 否决 / 降级项

| 候选 | 出处 | 否决理由 |
|---|---|---|
| **「OpenMP 私建线程池 = 阻断违规」** | **D 列为 B1**（A 已否决） | **我独立复核后维持否决**：`module_adapters.cpp:375-389` `ScopedOmpWorkerInjection` 以 RAII 在节点 execute 作用域内注入调度器 `ThreadLease` 线程数、退出时恢复；`:369` **点名 `ipv_triangle.cpp` 的 `omp_get_max_threads()` 为消费方**；`:374`「线程数仍唯一来自 host budget, 无任何硬编码」。`omp_get_max_threads()` 在并行区**之外**调用，读的是被注入的 ICV。D 未读 `:355-389`（其只读了 `:3745-3789`），故该阻断**证据不足，不予采纳** |
| **「`run/` 未被 gitignore、`run/perf-fix/` 存在但为空」** | **D 的 S3/S4** | **我直接推翻**：根 `.gitignore:17-18` = `run/*` + `!run/.gitkeep`；`git ls-files run/` = 2；`test -d run/perf-fix` = **不存在**。D 的**结论**（证据文件缺失）我采纳，其**成因描述**不采纳 |
| 「`test_extract_wcs_sip_failclosed.cpp` 是恒真门」 | B 审视后自否决 | 采纳其否决：case 2（`:104-108`）刻意与 case 1 解耦以便单独命中 `det<1e-15`，设计正确 |
| 「`test_select_domain.cpp:184` 是恒红门」 | B 提出后**自行否决** | 采纳其否决：`rho_img` 不依赖 `n_target`，非恒红 |
| 「`ipv_dead_params_selfcheck` 的 P5-恢复是 P0 的重言」 | B 自行否决 | 采纳：其 `:78-81` 每例前从 `args.src` 重铺影子树、突变落在 `mkdtemp`，是真恢复+重跑 |
| 「`test_triangle_budget` 的 500ms 墙钟门 = 恒真」 | B 提出 | **部分采纳**：结构性质（不分配/不枚举）由源码保证，但墙钟代理证不了「不分配」；降级为 S10 邻项 |
| 「`ipv_abi_layout_lock --selfcheck` 的负控太弱（mutant 不含 IpvWcsResult）」 | B 提出 | **采纳为建议**：主门 `:193-194` 是跨实现真校验，仅 selfcheck 臂弱 |
| 「`good_rms_threshold` 未被 P27 登记」 | 我提出后**自否决** | `ipv_dead_params_manifest.json:432/:642` **确有**登记，缺口只在源码注释 ⇒ 降级 |
| 「`ipv_get_last_inliers` 返回 0 与 -1 歧义」 | A、D 均提 | 唯一调用方已双重把守（`diag_gaia_psf_projection.py:229-237`），且合同已在 `ipv_api.h:260` 说明 ⇒ 不单列 |
| 「`p1wcs_clip_consistency_test` 可能是 V1 类同源断言」 | B 提出**但未读源码** | **不采纳**——B 自己标注「flagged not concluded」。留作前台待办，我未核实不写结论 |
| 「7 个 dead 参数声明全假（因 polygon/ransac 读了）」 | C 提出后**自行否决** | 采纳其否决：标注范围明写「生产路径」，且已验证 polygon/PROSAC 从任何生产入口不可达。**但其残留的措辞问题我保留为 M14**——两者不矛盾：实质真、字面假 |
| 「`ipv_abi_mirror.py` 未被跟踪」 | C 提出后**自行否决** | 采纳（其自查原因是 `head -60` 截断） |
| 「`IpvParams` 有 26 字段而非 24」 | C 提出后**自行否决** | 采纳：24 = 去掉 2 个 ABI 自描述头字段后的求解参数，口径可辩护，降为注释遗漏 |
| 「`wcs_sky_to_pixel_iterative` 的『生产参考实现』是假声明」 | C 提出后**自行否决** | 采纳：`docs/science/ASTROMETRY.md:119` 已登记其非生产 |
| 「`make_clean_err.txt` 被 `build_err*.txt` 覆盖」 | C 提出后**自行否决** | 采纳其更正——真实缺陷方向相反（规则匹配不到已存在的文件），已并入 S11 |
| 「7 个 dead 参数声明全假（因 polygon/ransac 读了）」 | C 提出后**自行否决** | 采纳其否决：标注范围明写「生产路径」，且已验证 polygon/PROSAC 从任何生产入口不可达。**但其残留的措辞问题我保留为 M14**——两者不矛盾：实质真、字面假 |
| 「`ipv_abi_mirror.py` 未被跟踪」 | C 提出后**自行否决** | 采纳（其自查原因是 `head -60` 截断） |
| 「`IpvParams` 有 26 字段而非 24」 | C 提出后**自行否决** | 采纳：24 = 去掉 2 个 ABI 自描述头字段后的求解参数，口径可辩护，降为注释遗漏 |
| 「`wcs_sky_to_pixel_iterative` 的『生产参考实现』是假声明」 | C 提出后**自行否决** | 采纳：`docs/science/ASTROMETRY.md:119` 已登记其非生产 |
| 「`make_clean_err.txt` 被 `build_err*.txt` 覆盖」 | C 提出后**自行否决** | 采纳其更正——真实缺陷方向相反（规则匹配不到已存在的文件），已并入 S11 |

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"   # 全部只读；中文路径用 git -c core.quotepath=false
git -c core.quotepath=false log --oneline -1        # 850a9ede

# 成员行数（20 份 / 7753，与权威清单一致）—— 见本件 §1 所列 for 循环

# B1 中心先移 + 失败出口 + success 丢弃 converged
sed -n '253,256p;291,304p;330,347p' lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp
grep -rn "rep_result.convergence" lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp   # 全在 logger_ 内
sed -n '16,25p' lib/algorithms/platesolve/cpp/ipv/include/../ipv_solver.h 2>/dev/null || \
  sed -n '16,25p' lib/algorithms/platesolve/cpp/ipv/include/ipv_solver.h                  # 无 converged 字段

# B2 单趟
sed -n '435,436p;551,554p;573,577p;580,595p' lib/algorithms/platesolve/cpp/ipv/src/ipv_itertrans.cpp

# B3 sig 量纲
sed -n '59p'      lib/algorithms/platesolve/cpp/ipv/include/ipv_itertrans.h
sed -n '630,637p;868,875p' lib/algorithms/platesolve/cpp/ipv/src/ipv_itertrans.cpp
sed -n '716,720p;1113,1116p;1416,1419p' lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp

# B4 机器锁把整改判红
sed -n '180,186p' lib/algorithms/platesolve/cpp/ipv/test/CMakeLists.txt
sed -n '273,277p' lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h

# B5 残差合同 vs 实现
sed -n '239,249p' lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h
sed -n '1808,1830p' lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp

# M8 恒真门与恒真断言（★本轮最高价值新增）
sed -n '184,186p;189,195p' lib/algorithms/platesolve/cpp/ipv/test/test_select_domain.cpp
sed -n '295,301p;313,317p'   lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp    # rho=n/area ; min(60,max(50,…))

# M3 越界记零并发布
sed -n '835,842p;869,880p;926,934p' lib/algorithms/platesolve/cpp/ipv/src/ipv_itertrans.cpp

# M7 WcsTan 穿透
sed -n '60,61p' lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp
sed -n '3756,3770p' lib/infrastructure/scheduler/src/module_adapters.cpp

# M9 失效锚
sed -n '2098,2101p' lib/infrastructure/scheduler/src/module_adapters.cpp   # 真 p1_op_wcs 在 :4681
sed -n '1183,1186p;1196,1199p' lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp

# M10 悬空引用
for n in ipv_cda_distortion_design.md vm44_types.h vm45_types.h; do
  printf "%-32s tracked=%s\n" "$n" "$(git -c core.quotepath=false ls-files | grep -c "/$n$")"; done
grep -rn "AT_MATCH_RATIO" lib/                              # 仅 1 处注释
grep -c "P27" docs/engineering/PUBLIC_API.md               # => 0
ls eng/ci/ 2>&1 ; ls lib/algorithms/platesolve/logs/ 2>&1 # 均不存在
git cat-file -t 9dafd79c ; git cat-file -t 3a0db4a6       # 均 invalid

# S10 测试未注册
git -c core.quotepath=false ls-files | xargs grep -l "test_synthetic\|test_kvector" 2>/dev/null | grep -c CMakeLists   # => 0

# 被我推翻的两条（留证）
sed -n '369,374p;375,389p' lib/infrastructure/scheduler/src/module_adapters.cpp  # ScopedOmpWorkerInjection
sed -n '15,20p' .gitignore ; git -c core.quotepath=false ls-files run/ | wc -l    # run/* 被忽略，仅 2 文件
sed -n '19p;1463p;1473p' CMakeLists.txt ; sed -n '1352p' eng/tests/unit/CMakeLists.txt

# §5 两个反例（纯数学，不触碰本仓任何构建），可用任意 numpy 复跑
```

---

## 9. 交付摘要

| 项 | 值 |
|---|---|
| 片号 | `ALG-platesolve-002` |
| 覆盖率 | **20/20 份，7753/7753 行，100.0%** |
| 阻断 | **5** |
| 须修 | **15** |
| 建议 | **14** |
| 最强反例 | 上报残差对 **477″ 中心偏差数值完全无感**（`ipv_solver.cpp:1810-1819`：残差由被检模型自身定义，常数项吸收中心偏差） |
| 次强 | `test_select_domain.cpp:184` 期望侧 `40/60` 即实现自身代数恒等式；`:186` 断言 `nt<=60` 而实现是 `min(60,max(50,…))` —— **两条恒真门**，其「负向对照」从不调用被测函数 |
| 子代理 | 派 4（A/B/C/D），**4/4 全部返回**；逐条复核后采纳 **24 条**、**否决 2 条**（OpenMP 私建池、`run/` 被忽略的成因）、采纳其自否决 **10 条** |
| 是否发现**新**问题 | **是**——B2（恒定单趟）、B3（量纲三方矛盾）、M8（两条恒真门）、M13（工具 0.5px 系统偏差）、M14（假退役措辞，负责人点名类的第二例）、M15（报告统计量分母不一致）、S10（两个被引为证据的测试从未被构建）此前均无记录；且本片既有判定 20/20「默认保留」，实质覆盖率为 0 |

**给负责人的四句话**：
1. 本片不是「有几处待修」，而是**唯一的非收敛探测器被 gate 抛弃（B1）、唯一的质量上报量对该缺陷结构性无感（反例一）、而唯一能发现问题的机器锁正在主动防止修复（B4）**这三件事同时成立。
2. 我自己在第 1 轮把 `test_select_domain.cpp` 判为「通过」，**是错的**；子代理 B 的举证经我复核成立。**本片最危险的东西不在生产代码的显眼处，而在看起来最严谨的判据文件里。**
3. 负责人点名的「退役对象仍有活调用者」缺陷类，本片**找到第二例**（M14，`ipv_types.h:222`「0 引用点」字面为假）——但须说清：它与已确认的那例**不同**，是**措辞**假而实质（生产路径不读）真，修复是改措辞而非改代码。别把它当成新的一例实锤。
4. 在 B1/B2/B4 处置前，本片对外交付数字（`rms_arcsec` / `n_pairs` / `success`）**不足以作为正确性证据**；`REPORT.md` 全部定量证据文件已不存在，`memory.md` 声称的可执行门已从构建移除——**本片目前没有任何可复跑的证据链**。