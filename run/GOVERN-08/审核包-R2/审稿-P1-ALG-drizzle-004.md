# 审稿 P1 · ALG-drizzle-004（第 1 遍 · 对抗性重读）

- 车道：G08-05 对抗审稿 P1
- 片号：`ALG-drizzle-004`
- 层：`lib/algorithms/drizzle`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml`（片号 `ALG-drizzle-004`）
- 负责人口径：**一遍 = 对同一片材料的一次完整重读**。结论全部来自本人读原文；未执行任何构建 / ctest / pytest / 二进制；未做任何 git 写；未修改任何仓内文件（本交付件为唯一写入）。

---

## 1. 读完了吗

### 1.1 计数口径

- **成员份数口径**：权威片清单 YAML 的 `成员文件:` 列表逐条计数。
- **成员总行数口径**：`wc -l` 实测（空行计入，与 YAML 的 `实际行数: 8998` 逐位吻合）。
- **实际读了多少行**：**本人用 `read` 工具逐行读过的行数**，不含子代理读过的行、不含仅 `grep`/`sed` 定位的行（定位用的 `sed -n` 单列在 1.4）。

### 1.2 数字

| 项 | 值 |
|---|---|
| 成员份数 | **26** |
| 存在份数（`[ -f ]` 实测） | **26 / 26**（缺失 0） |
| 成员总行数（`wc -l`） | **8998**（与清单 `实际行数: 8998` 一致） |
| 本人完整读完的文件 | **20 份 / 5406 行** |
| 本人部分读完的文件 | **6 份 / 591 行** |
| **本人直读合计** | **5997 / 8998 = 66.7%** |
| 未由 `read` 工具覆盖的行 | 3001 行（集中在 6 份大测试文件，见 1.3） |

### 1.3 未完整读完的，如实列出（6 份）

| 文件 | 总行 | 我直读 | 缺口 | 缺口成因与补偿 |
|---|---|---|---|---|
| `tests/test_spherical_overlap.cpp` | 1388 | 90（1175-1264）+ 定位 20（781-800） | ~1278 | 时间预算。我直读了本片最关键的 14.2 门整段，并定位核验 11.2 的恒真门。子代理 A、B 均声明全文读过并给出逐条 `文件:行`；我对其每一条载荷结论做了**独立行级复核**（见 §7）。 |
| `tests/reference_overlap.cpp` | 838 | 120（570-689） | ~718 | 同上。我直读了 R4.2/R5 全段（含恒等式门与死分支），另定位核验 495 的筛子集门。 |
| `tests/reverse_drizzle_science_test.cpp` | 667 | 46（340-385）+ 50（455-504） | ~571 | 同上。我直读了零分母区与判据区两段。 |
| `tests/reverse_api_test.cpp` | 204 | 75（40-114） | ~129 | 同上。我直读了非 Windows 恒红门整段。 |
| `tests/p1drz/p1drz_fixtures.hpp` | 264 | 140（1-140） | ~124 | 同上。 |
| `hips/README.md` | 231 | 120（1-120） | ~111 | 同上。 |

**诚实声明**：本片不是 100% 逐行读完。66.7% 是本人 `read` 覆盖率的准确数字。剩余 3001 行由两名子代理全文阅读，我对其**每一条承重结论**都回到原文行号复核后才采信（复核记录见 §7.3）。**凡我未亲自读到行、也未独立复核行号的内容，一律不进本交付件的结论。**

### 1.4 定位用命令（非 `read` 覆盖，单列）

- `sed -n` 定位：`test_spherical_overlap.cpp` 781-800；`spherical_overlap.cpp` 644-660；`reverse_drizzle_science_test.cpp` 340-385 / 455-504；`reference_overlap.cpp` 570-689；`reverse_api_test.cpp` 40-114；`CMakeLists.txt`(tests) 140-195 / 265-285；根 `CMakeLists.txt` 174-182 / 354-372；`hp_drizzle_api.h` 40-44/60-64/68-72/128-142。
- `grep`：全模块 `#pragma omp`、`HP_DRIZZLE_API`、`acsd_drizzle`、`DEFAULT_KNN`。

---

## 2. 本片判定

# 阻断

### 最重 3 条

**【阻断-1】Phase1 产品末端的归一因子与除数用了两个不同的「面积」定义 —— 面亮度发布值可偏到 −50%，且小覆盖叶直接落 NaN。**
`astro_sphere_sink.cpp:539` 用 **未量化** 的 `acc.sumArea` 算因子 `k = D_p/N_p`；`astro_sphere_sink.cpp:544` 却用 **uint8 量化后** 的 `area_q = (q/255)·A_cell` 作除数；writer 再做 `sig = flux/area`。
于是实际发布的是 `S_p · ρ`，`ρ = 255·(D_p/A_cell) / lround(255·(D_p/A_cell))`，**而不是**正本要求的 `S_p = F_p/N_p`（`astro_sphere_sink.h:31-35`）。
反证：当 `D_p/A_cell < 0.5/255 = 1.961e-3` 时 `q=0` → `area_buf=0` → writer 判 `covered=false` → **signal=NaN、support=0**，而 `astro_sphere_sink.cpp:550` 仍把该叶标为 `valid_buf=1`。所有 mosaic tile 的**锯齿边缘叶**都在这一带。
**同一文件的另一个末端自相矛盾**：`write_hips_direct` 在 `astro_sphere_sink.cpp:191` 用**未量化**的 `dense_area = acc.sumArea` 作除数，那里 `sig` 恰好**精确**等于 `S_p`。两个末端对同一个累加器给出不同答案，而 `astro_sphere_sink.h:72-91` 明文声称二者「逐字节等价」。
**唯一触碰该末端的门 `tests/drizzle_pf_sb_gate.cpp:111` 把 `sumArea` 钉在 uint8 量化格点上**，令 `q→area_q` 成为恒等，恰好消掉本缺陷的前提 —— 判据自身的选择使缺陷不可见。

**【阻断-2】本片唯一的独立 Oracle 是被「因为红」而摘掉灯的恒红门，基线上的绿是靠排除取得的，不是靠正确取得的。**
`tests/CMakeLists.txt:272-277`（该文件不在本片，但其裁决直接决定本片 26 份材料的证据地位）原文：

> `# **不注册 ctest**: 一次性实跑该独立蒙特卡洛参考路径得 "32 通过, 3 失败" ... 三条断言分别差 3.4e-19 (tol 3.0e-20)、2.6e-6 (tol 4.1e-13)、rel_err 1.04e-6 (tol 1e-6) ... 只编入构建图 (零漂移), 红灯上呈记录`

`reference_overlap.cpp` 是本片**唯一**不复用被测函数的蒙特卡洛参考路径。三条红灯里第三条 `rel_err 1.04e-6 (tol 1e-6)` 正是 `reference_overlap.cpp:672-676` 的 `Σ(support×A_p)=A_drop` 门 —— 我在 §5 独立推出该式在代数上退化为 `Σ a_jp = A_drop`，其 1.04e-6 是真实的数值地板。
同时 `tests/CMakeLists.txt:159-190` 把 `oracle_independent_test` / `reverse_api_test` / `reverse_drizzle_science_test` 三个门**编入但不注册**，理由是「避免把与本轮无关的**历史红灯带进 ctest 基线**」。
⇒ 本片 9 个测试文件里，**只有 2 个真在 ctest 基线上**。全部「PASS」声称所依赖的独立证据面，处于熄灯状态。

**【阻断-4（本轮新增，见文末「补充」）】算法模块内私建线程池：每帧按 `hardware_concurrency()` 拉起 `std::async` 任务建 KD-tree，且不受调度器租约约束。**
`snr_evaluator.cpp:132-134` 构造 nanoflann 树时只设 `leaf_max_size=32`，`n_thread_build` 保持默认 0 ⇒ vendored 头 `nanoflann.hpp:1915` 执行 `n_thread_build_ = std::max(std::thread::hardware_concurrency(), 1u)` ⇒ 在 `:1433` 与 `:3883` 用 `std::async` 扇出。
`NANOFLANN_NO_THREADS` 在**全仓任何 CMakeLists/.cmake 中零定义**（已实测）。
生产可达：`hp_drizzle_api.cpp:809/834/888` → `buildTree()`。
这是本轮固化检查项「私建线程池」的**第 10 例**（此前已实测 9 例）。与 `drizzle_engine.cpp:1988` 的合规做法（`num_threads(config.threads)`）不同，这条路**从不读 `config.threads`、不读 `omp_get_max_threads()`、不读 host executor 租约**，且本模块自己的入口 `src/module_entry.cpp:909` 就写着 `FORBID-003: 禁私建线程池`。

**【阻断-3】snr_model 块损坏时整帧 SNR 静默退化为 1.0，产品照写、返回码 0；而写侧的「独立参考」又是用被测函数本身造的。**
`hp_drizzle_api.cpp:741-744`（头非法）、`:752-753`（checksum 不匹配）、`:855-857`（v0 截断）三处**只有 `fprintf`，没有 `return`、没有 `setErrorMsg`、没有标志位**。`snrPtr` 保持 `nullptr` → `snr_pts` 为空 → `astro_sphere_sink.cpp:250` 跳过 `aio_hips_write_snr_points` → `run_drizzle_internal` 在 `hp_drizzle_api.cpp:1300` **返回 0**。
同类：`hp_drizzle_api.cpp:810/835/889` 的 `evaluator.build*()` 返回 false 时**没有 else 分支**，且 `snrModelPtr` 在 build **之前**就被赋值（`hp_drizzle_api.cpp:796-808 / 818-830`），于是 HiPS 目录里**照发控制点**，而稠密 SNR 一个像素都没重建。
对照：同一函数对 SIP order 越界是硬失败（`hp_drizzle_api.cpp:465-469` 返回 −10）。同类输入两种口径，是不一致而非既定策略。
**雪上加霜**：`snr_evaluator.cpp:306/317/365/379` 用生产 `build_drop_polygon_adaptive` 造的 `truth`（`reverse_drizzle_science_test.cpp:232/312`）去验生产本身；而 `hp_drizzle_api.cpp:691-713` 辛苦重建的稠密 SNR，在 `drizzle_engine.cpp:1480` 的核参数里是**无名形参**（`float /*snrValue*/`）—— 算完即丢。P4 的接线要么缺失、要么是死重。

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

> 判定三档：**须修** / **建议** / **无发现**。「无发现」= 本人读完该文件后未发现本轮检查项所列缺陷，不等于文件正确。

| # | 文件（行） | 我读了什么 | 看到什么（带行号） | 判定 |
|---|---|---|---|---|
| 1 | `healpix_drizzle/hp_drizzle_api.cpp` (1431) | **全读** | snr_model 三处只记日志不失败（741/752/855）；`build*` 返回值丢弃且指针先发布（810/830/835/887）；variance 块类型/尺寸不对 → `variancePtr` 保持空 → 静默改变产品集（1066-1074 → 1199/1201/1205/1207）；错误码别名（−1 三用、−12 三用、5 两用、−11 两用）；`operation_counts.json` 截断时整块静默跳过（1259） | **须修** |
| 2 | `healpix_drizzle/astro_sphere_sink.cpp` (634) | **全读** | **阻断-1**（539 vs 544 面积定义不一致；191 vs 544 两末端矛盾）；组内公共 F_ref 门把损坏帧 `continue` 掉而非判红（430）；`hips_dir` 无分隔符时静默跳过帧 SNR 块（384-388 无 else）；越界 parent/leaf 无计数丢弃（178/510/517）；`ACSD_DRZ_SB_FAULT` / `ACSD_IVAR_FAULT` 两个生产环境变量故障注入（152-159 / 350-354）；`finalize` 失败路径漏 `aio_hips_abort`（262-268） | **须修** |
| 3 | `healpix_drizzle/astro_sphere_sink.h` (116) | **全读** | `sb_publish_scale` 用未量化 `sum_area`（50-52）；头声称两末端逐字节等价（72-91）—— 被 #2 证伪；`has_variance` 合同（94-97） | **须修** |
| 4 | `healpix_drizzle/snr_evaluator.cpp` (384) | **全读** | **`#pragma omp parallel for`（336）= 算法模块私建线程队，违反 AGENTS.md §6「池所有权归调度器」**；`median_snr`/`idw_power` 非法值静默换 1.0 而 `median_snr` 是全帧 SNR 的分母（211-212/267-268），且所引 `07_noise_snr.md` 不存在；`buildF64` 失败路径零日志（244/261）而 `build` 有日志（174/201）；非有限控制点静默 `continue`（189/253）；`evaluateBatch` 零填充且返回 void（327-332）；`snr_phot_` 零校验（210/266） | **须修** |
| 5 | `healpix_drizzle/wcs_sip.cpp` (348) | **全读** | `\|det\|<1e-15` 时 CD 逆全置零但 `m_hasWcs` 不清 → `skyToPixel` 把全天球映射到 CRPIX 角点（48-51 → 300-301）；`pixelToSkyT` 无效 WCS 返回 `(0,0)`（240-245）是**合法**天球坐标；`pixelToSkyBatch` void 且早退不写调用方缓冲（325-330）；RA 归一化用 `while` 循环（163-164），CRVAL1 极大时近无界迭代；`fabs(NaN)<1e-15` 为假 → NaN CD 走 else 分支除出全 NaN 逆（48-58） | **须修** |
| 6 | `healpix_drizzle/wcs_sip.h` (73) | **全读** | 声明面；`hasWcs()` 不反映 CD 逆已失效 | 建议 |
| 7 | `healpix_drizzle/poly_clip.cpp` (287) | **全读** | **与 #5 正面冲突**：`gnomonicForward` 发散时返回**有限哨兵 ±1e6**（60-71），而 `wcs_sip.cpp:208-216` 就同一 TAN 公式明文规定「**不得返回有限哨兵值** …… 哨兵值会被 isfinite 放行而静默映射到错误像素」并返回 NaN；`clipPolygon` 退化窗口返回**未裁剪** subject（193-195）；`intersect` 分母近零时编造中点（172-174，绝对阈值非相对）；`cosc<0` 背面点被镜像而非拒绝；`POLY_LOG` 默认编译为空 | 建议（**全仓零调用**，已登记 DISP-DRZ-008；两模块对同一数学给出相反处置，是再采纳陷阱） |
| 8 | `healpix_drizzle/spherical_overlap.h` (427) | **全读** | 建立本片正确口径的基线：不支持态一律 **NaN**（90-92「调用方必须显式拦」）、退化输入返 0（116）。→ 正是 #5 与 #7 应遵循而未遵循的规则 | 无发现（作为判据基准） |
| 9 | `healpix_drizzle/hp_drizzle_internal.h` (46) | **全读** | `setErrorMsg` 空指针守卫正确；511 字节静默截断（25）可切碎 UTF-8 | 无发现（1 条建议） |
| 10 | `tests/test_spherical_overlap.cpp` (1388) | 部分 90 + 定位 20 | **恒真门**：`:793-795` `err_16 <= err_8 + 1e-10`，而 `spherical_overlap.cpp:648` 是 `(void)samples_per_edge;` → b8 与 b16 **逐位相同** → 该式是 `x <= x+1e-10`，**算术上不可能失败**，却在断言「16采样不差于8采样（单调提升）」。**静默跳过喂绿门**：`:1183` `max_closure=0.0` 播种，`:1222-1229` 两处 `continue`，`:1241` 只增不减，`:1249` `ASSERT_NEAR(...,0.0,1e-10)` → 五个像素全跳过即 `|0-0|<=1e-10` PASS | **须修** |
| 11 | `tests/reference_overlap.cpp` (838) | 部分 120 | **恒等式门**：`:643-644` `sum_weighted += support * a_p_boundary; // = a_jp` —— 注释自认是恒等式，`:672-676` 的「Σ(support×A_p)=A_drop（通量守恒）」实为 `Σ a_jp = A_drop`。**死分支**：`:636` `if (a_jp <= 1e-15) continue;` 把负面积过滤掉，使 `:650` 的 `support < -1e-6` 检测永不可达。**筛子集取极值**：`:480` `threshold = drop_area*0.005` 在取 `max_rel_err` 之前把边缘薄片像素滤掉。**被算出来却不判**：`:623` `a_pixel_theory = 4π/(12nside²)` 是全组唯一非生产面积参考，只 printf（`:665`）不断言 | **须修** |
| 12 | `tests/reverse_drizzle_science_test.cpp` (667) | 部分 96 | **结构性零值门**：`:372-374` `cr.sb_err = denom_sb > 0 ? err_sb/denom_sb : 0.0`，`:490` `ok = cr.sb_err < 2e-3 && ...` → 分母为 0 时首合取项恒真。`:345` `if (t > 0) { denom_s += t; ... }` 把 `negative_field`（真值恒负）的全部像素筛掉 → `signal_rel_err ≡ 0.0`。`:492-493` 守恒项只有上界，`total_out=0` 平凡满足。pf<1 时整门实际只剩 `false_hole/false_fill/fp32_max_rel` 三项有效 | **须修** |
| 13 | `tests/drizzle_acceptance_test.cpp` (494) | **全读** | **恒真门**：`:449-452` `static_assert(sizeof(float)==4 && sizeof(double)==8)` 是平台属性、检不出任何 `long double`，却无条件 `g_pass++` 并打印「[PASS] …（无 long double）」。**负例控制不触引擎**：`:108-120` 把已算好的 `sum_out64` 乘一个标量再判红，注释自认「**不改变引擎本身**」，而 `:17-18` 声称这「证明本门非恒真」。**恒等阈值**：除 A/B/C/E 外只有 **B 段**用 pf<1；`--sections A/D/E/C/T` 时注入因子 `pf²=1` 为空操作，负例控制**静默失效并返回 0**。**阈值不同源**：`:74` `GATE_CLOSURE=1e-7` 且注释称「与下方各 CHECK 同值」，但 D 段 `:250` 实为 `1e-6`。**FP32 门可空过**：`:124-133` 由 `t32` 自驱，`n_leaf32` 计数却不判。**分母地板**：`:131` `max(|f64|,1.0)` 使低通量叶按绝对 1e-5 判。**丢弃证据**：`:104` 算 `finite64`，`:148` `(void)finite64;` | **须修** |
| 14 | `tests/oracle_independent_test.cpp` (272) | **全读** | 头声称「独立」但 `:70` 用**生产** `get_healpix_boundary` 取真值集几何 → 像素边界缺陷对 oracle 与 SUT 同向不可见。`oracle_intersects`（`:107-120`）是 13 点采样 → 「真集」是真交集集的**严格子集**，薄片交叉恒判不相交 → 「false negative = 0」**证不出**。高 nside 分支（`:152-178`）只测 22 个点。`:144` 未捕获 `used_fallback` | **须修** |
| 15 | `tests/reverse_api_test.cpp` (204) | 部分 75 | **恒红门**：`:58-63` 非 `_WIN32` 时三函数指针硬编码 `nullptr` → `:62` CHECK 恒红 → `:63 return 1` 短路，其后 `:71/132/136/151/166/176/182/191/194/197` 十条真实 ABI 门在 Linux 上**全部不可达**。恒红门把缺陷藏进红灯 | **须修** |
| 16 | `tests/p1drz/p1drz_fixtures.hpp` (264) | 部分 140 | `:13-14` 声称期望值「绝不经过被测函数」；但 `:34-39` `geom_pixel_area` 用 Van Oosterom 四角立体角 —— **与生产同一方法**。`:119-120` 诚实自陈「残差只剩 Van Oosterom vs 生产 Van Oosterom 的算法差」。实现独立、方法不独立 ⇒ 立体角公式的共性错误对本门不可见 | 建议（诚实披露，但披露即等于承认该门对最大缺陷类无判别力） |
| 17 | `tests/bench_write.cpp` (104) | **全读** | `:72-74` `open` rc 只打印不判；`:98-103` `finalize` rc 只打印不判，`:103` **无条件 `return 0`** —— 截断/不一致产品也报成功。`:84-85` 越界 leaf 静默丢弃。对比 `hiss_write_probe.cpp:49/70/80` 三个 rc 全判。`:59` 正确算出 `A_p = 4π/(12nside²)` | 建议（零断言探针，不应计入任何 PASS 统计） |
| 18 | `tests/hiss_write_probe.cpp` (82) | **全读** | 三个 rc 全判（49/70/80），分类正确。`:23` `A_p = 2.438197e-10` 硬编码，而同族 `bench_write.cpp:59` 现算 —— 当前 nside=65536 下数值巧合正确，nside 一变即冻结错误（AGENTS.md §6「可由输入几何导出的改为现场计算」） | 建议 |
| 19 | `CMakeLists.txt` (163) | **全读** | **构建引用全部核实存在**（12 项逐一 `test -f`，0 悬空）。`find_package(OpenMP)`(128) 仅经 `acsd_openmp_link_if_unix` 门控，平台口径已收口（133-141）。**无发现** | 无发现 |
| 20 | `hips/CMakeLists.txt` (132) | **全读** | 同上，`src/module_exports.map` / `src/acsd_p1_hips_writer.def` / `src/module_entry.cpp` / `src/aio_publish.cpp` 全部存在。L3 依赖闭包最小集的推导写在 `:10-17`（含一次 dlopen undefined symbol 的实证），可信 | 无发现 |
| 21 | `hips/include/acsd/hips/publish.h` (149) | **全读** | **自洽式断言**：`:50-51` 声称「0..15 与 70 全域一致于 `aio_abi_v1.h` 的 `aio_status`，`_Static_assert` 编译期对齐证明」；但 `:130-146` 的断言**只比对本头内的字面量**（`AIO_PUBLISH_OK == 0` 而 `AIO_PUBLISH_OK` 就定义于此），本头只 `#include <stdint.h>`（36），**从未包含 `aio_abi_v1.h`**。我把 `lib/include/acsd/io/aio_abi_v1.h:46-66` 拉来逐值比对，当前 0..15/70/71 确实一致 —— 即断言**碰巧**对，但它**不能**发现对方重编号。失败语义设计本身良好（4 原语 + 稳定状态码 + 故障注入必败） | **建议**（这是本片最典型的「用同一式既当被检量又当期望量」） |
| 22 | `module.yaml` (67) | **全读** | **三处悬空引用（逐一实测证伪）**：① `:3-4`「根 CMakeLists.txt:**356-366** 静态库 acsd_drizzle」→ 实测 `add_library(acsd_drizzle STATIC` 在根 **CMakeLists.txt:745**，356-366 是 `add_subdirectory`；② `:3-4`「C ABI 导出 hp_drizzle_api.h:**42,62,70,130,139,140** 六符号」→ 实测八个 `HP_DRIZZLE_API` 函数声明在 **82/102/110/126/155/211/220/221**，六个行号无一命中，且数目是八不是六；③ `:18`「omp … **drizzle_engine.cpp:1639-1644**」→ 实测该处是 trace 记录代码，omp pragma 在 **1988/2319/2344**。④ `:64` `threading_model: host_executor_lease` 与 `:16-20` 自陈的「现状实现为 omp parallel for」**矛盾**，机读字段对任何检查器给**假绿**；且 omp 清单**漏了** `snr_evaluator.cpp:336` | **须修** |
| 23 | `src/module_exports.map` (6) | **全读** | 唯一导出 `acsd_module_query_v1`，其余 `local: *`。与 `CMakeLists.txt:156-162` 一致 | 无发现 |
| 24 | `hips/src/acsd_p1_hips_writer.def` (3) | **全读** | 唯一导出 `acsd_module_query_v1`，与 `.map` 口径一致（双平台等价） | 无发现 |
| 25 | `README.md` (188) | **全读** | 与 #22 **同源**的悬空引用：`:12-13`/`:31`/`:165` 重复「根 CMakeLists.txt:356-366」与「hp_drizzle_api.h:42,62,70,130,139,140」；`:55`「api.cpp:486-503」实测多通道拒绝在 **607-614**，`:56`「api.cpp:541-545 缺 WCS 返回 −9」实测在 **450-453 / 调用点 658-663**，`:60`「api.cpp:191 `<0.0` 才拒」实测 pixfrac 判在 **218**，`:68`「api.cpp:1087-1130」实测 operation_counts 在 **1221-1272**，`:117`「api.cpp:145-151」实测 capability 位在 **173-175**。**正面**：`：95-96` 主动登记「值像素非有限 → 主循环静默 continue，不进累加器、**无计数暴露**（与 DRIZZLE.md:96 不符——DISP-DRZ-004）」—— 自认的生产静默降级。`:171` 诚实登记 `poly_clip.cpp` 生产零调用 | **须修** |
| 26 | `hips/README.md` (231) | 部分 120 | 同一族悬空引用：`:5`/`:30`/`:34` 称 `hips/CMakeLists.txt:**43**` 是 `acsd_p1_hips_writer` 目标 → 实测 `add_library(acsd_p1_hips_writer SHARED` 在该文件 **:50**；`:13`「根 CMakeLists.txt:298-309 编入静态库 acsd_hips」与 `:34` 同源；`:51`「tile_depth!=9/nside<512 在 sink 层拒绝，astro_sphere_sink.cpp:**34-44**」→ 实测 `phase1_tile_depth` 在 33-42、拒绝在 **95-103 / 318-328**。`:93-95` 正面登记 `covered_area≤0 → signal=NaN` —— **这正是阻断-1 的落点**：量化把 `area_q` 打成 0 就走这条 NaN 路径，而本头未意识到 `area_q` 可由 `D_p` 量化而来 | **须修** |

---

## 4. 发现清单

### 4.1 阻断（3 条，均已行级复核）

| ID | 位置 | 缺陷 | 复核 |
|---|---|---|---|
| **BLK-1** | `astro_sphere_sink.cpp:539` + `:544`（对照 `:191`） | 归一因子用未量化 `sumArea`、除数用量化 `area_q` ⇒ 发布 `S_p·ρ` 而非 `S_p`；`D_p/A_cell<1.961e-3` 时 `q=0` ⇒ NaN + support=0，而 `:550` 仍标 valid。`write_hips_direct` 用未量化面积 ⇒ 两末端自相矛盾，证伪 `astro_sphere_sink.h:72-91` 的「逐字节等价」。唯一门 `drizzle_pf_sb_gate.cpp:111` 把 `sumArea` 钉在格点上使缺陷不可见 | 本人逐行读 `astro_sphere_sink.cpp:190-191 / 521-544` 与 `astro_sphere_sink.h:50-52 / 72-91` |
| **BLK-2** | `tests/CMakeLists.txt:272-277`（裁决面）+ `:159-190` | 本片唯一独立 Oracle 因红被摘出 ctest；另 3 个门编而不注册。本片 9 个测试文件中真在基线上的只有 2 个 | 本人 `sed -n '265,285p'` 与 `'140,195p'` 直读原文 |
| **BLK-3** | `hp_drizzle_api.cpp:741-744 / 752-753 / 855-857 / 810 / 835 / 889`（对照 `:465-469`） | snr_model 损坏与 `build*` 失败只记日志 → 整帧 SNR 静默退化 1.0、HiPS 照发控制点、返回 0。稠密 SNR 在 `drizzle_engine.cpp:1480` 为无名形参（算完即丢）；其「独立参考」又由生产 `build_drop_polygon_adaptive` 造 | 本人逐行读该四段；子代理 D 独立复核 `drizzle_engine.cpp:1480` 无名形参 |

### 4.2 须修（14 条）

1. `snr_evaluator.cpp:336` — 算法模块内 `#pragma omp parallel for`，违反 AGENTS.md §6「模块不私建线程池；池所有权归调度器」。`module.yaml:64` 还把 `threading_model` 声明成 `host_executor_lease`。
2. `astro_sphere_sink.cpp:430` — 组内公共 `F_ref` 门把 `!isfinite(fv) || !(fv>0)` 的帧 `continue` 掉。构造：5 帧中 3 帧 `flux_adu=1e-30`、2 帧公共 1.0 ⇒ 门只看健康帧 ⇒ `group_fref_ok=true`，随后 `:456-458` 仅查 `isfinite && >0`，`1e-30` 通过 ⇒ 以「组内公共」之名给非配对帧写键。**筛掉真信号**。
3. `hp_drizzle_api.cpp:1066-1074` + `:1199-1207` — variance 块类型错/尺寸错 ⇒ `variancePtr` 空 ⇒ `has_variance=0` ⇒ `astro_sphere_sink.cpp:355-358` 丢掉 `VARIANCE|IVAR` 产品位。**「块损坏」被改写成「帧本就没有方差」**，根因被销毁。
4. `snr_evaluator.cpp:211-212 / 267-268` — `median_snr` 非法/NaN 静默换 1.0，而它是每个重建值的分母（`:306/317/365/379`）；`idw_power` 的 1.0 有正本背书，`median_snr` 的没有，且注释引用的 `07_noise_snr.md` **在 docs/ 下不存在**。
5. `wcs_sip.cpp:48-51` — `|det|<1e-15` 时 CD 逆全置零而 `m_hasWcs` 不清 ⇒ `skyToPixel` 把**每一个**天球点映到 `(crpix-1, crpix-1)`，返回值完全合法、无法与正常结果区分。反向路径 `reverse_drizzle.cpp:284-290` 只拒 `|det|<1e-30`，**窗口 [1e-30, 1e-15) 正好穿过**。
6. `hp_drizzle_api.cpp:432` — `has_cd = (cd11 != 0.0 || cd22 != 0.0)` 只探对角：① 合法 90° 旋转 `CD=[[0,1],[1,0]]`（det=−1）被**误拒**为「缺 WCS」；② `CD=[[0,0],[1,2]]`（det=0）被**误收**，随后落入第 5 条。
7. `test_spherical_overlap.cpp:793-795` — 恒真门。`spherical_overlap.cpp:648 (void)samples_per_edge;` 使两次结果逐位相同 ⇒ `x <= x + 1e-10` 永真，却在断言「单调提升」。
8. `test_spherical_overlap.cpp:1183/1222-1229/1241/1249` — 静默跳过喂绿门。五个像素全 `continue` 即 PASS；且 `if (closure > max_closure)` 把 NaN 静默排除而非判红。
9. `reference_overlap.cpp:636 vs :650` — `a_jp <= 1e-15` 先滤掉负面积，使 `support < -1e-6` 检测成为**死分支**；`compute_overlap_area` 返回负面积不会被报。
10. `reference_overlap.cpp:643-644 / 672-676` — 恒等式门（注释自认 `// = a_jp`）；且 `:610-613` 明说分母选边界面积正是为了「保证 support ∈[0,1] 精确成立」⇒ 上界按构造成立。`:623` 唯一非生产参考 `a_pixel_theory` 只 printf 不断言。
11. `reverse_drizzle_science_test.cpp:372-374 / 345 / 490 / 492-493` — 零分母 ⇒ 误差指标恒 0.0；`negative_field` 真值恒负被 `t>0` 全筛；守恒项只有上界。pf<1 整门实际只剩三项有效。
12. `reverse_api_test.cpp:58-63` — 非 Windows 恒红门，短路十条真实 ABI 门。**恒红门把缺陷藏进红灯**，且该文件又被排除在 ctest 外 ⇒ 双重遮蔽。
13. `drizzle_acceptance_test.cpp:449-452` 恒真门（`sizeof` 断言检不出 `long double` 却无条件 `g_pass++`）+ `:108-120` 负例控制只乘标量不触引擎 + `:74 vs :250` 阈值不同源 + `:124-133` FP32 门空过 + `:131` `max(|f64|,1.0)` 分母地板 + `:104/:148` 丢弃 `finite64`。
14. `module.yaml:3-4 / 18 / 64` + `README.md:12-13/31/55/56/60/68/117/165` + `hips/README.md:5/13/30/34/51` — 三处（README 共十余处）行号悬空，全部实测证伪；`threading_model` 机读字段给假绿。

### 4.3 建议（8 条）

1. `publish.h:50-51 vs :130-146` — 自洽式断言：本头从未包含 `aio_abi_v1.h`，断言只比对本头字面量，无法发现对方重编号。当前值经我逐值比对确属一致（`aio_abi_v1.h:46-66`），但**证明力为零**。
2. `poly_clip.cpp:60-71` — 与 `wcs_sip.cpp:208-216` 的明文裁决正面冲突（有限哨兵 vs NaN）；`:193-195` 退化窗口返回未裁剪 subject；`:172-174` 绝对阈值编造中点；`cosc<0` 背面点被镜像。**全仓零调用**（已登记 DISP-DRZ-008），按 AGENTS.md §6 建议删除或统一口径。
3. `astro_sphere_sink.cpp:178/510/517` — 越界 parent/leaf 无计数丢弃（对比 `:180-181`+`:279-284` 的 norm-unavailable **有**计数告警），口径不一致。
4. `snr_evaluator.cpp:210/266/915` — `snr_phot_` 零校验；块内 `snr_phot=NaN` ⇒ 全帧 SNR NaN，而块只受 FNV-1a 校验（验字节不验语义）。
5. `hp_drizzle_api.cpp:1259-1271` — `operation_counts.json` 写失败只记日志；`snprintf` 截断时**整块跳过且零消息**。
6. `hiss_write_probe.cpp:23` — `A_p` 硬编码 `2.438197e-10`，同族 `bench_write.cpp:59` 现算。
7. `bench_write.cpp:98-103` — `finalize()` rc 未判 + 无条件 `return 0`。
8. `wcs_sip.cpp:163-164` / `poly_clip.cpp:124-125` — RA 归一化用 `while` 累加而非 `fmod`；`CRVAL1≈1e30` 时近无界迭代，NaN 直穿。

---

## 5. 我主动构造的反例

> 原则：**不采信任何既有「检查通过」**。每条反例给出「构造什么 / 期望推翻什么 / 是否推翻」。

### CE-1（推翻 BLK-1）—— 量化面积 vs 未量化因子

- **构造**：取一个累加器叶，`pixfrac=1.0`，常量面亮度场。令 `D_p/A_cell = 0.004`（非格点）。按 `astro_sphere_sink.cpp:521-544` 逐步算：`S=0.004` → `q=lround(255·0.004)=lround(1.02)=1` → `area_q = (1/255)·A_cell = 0.003922·A_cell`。`k = D_p/N_p`。发布 `sig = sumFlux·k/area_q = S_p·(0.004/0.003922) = S_p·1.02`。
- **期望推翻**：正本 `S_p = F_p/N_p`（`astro_sphere_sink.h:31-35`）。
- **是否推翻**：**推翻**。偏差 +2.0%。再取 `D_p/A_cell=0.01` → `q=3` → `ρ=2.55/3=0.85` ⇒ **−15.0%**。取 `D_p/A_cell=0.001` → `q=0` → `area_q=0` ⇒ writer 判 `covered=false` ⇒ **signal=NaN**。偏差与 nside 无关（是覆盖率的纯函数），不随分辨率加细而摊薄。
- **决定性对照**：同一累加器走 `write_hips_direct`（`astro_sphere_sink.cpp:191` `dense_area = acc.sumArea`，未量化）得 `sig = sumFlux·(D_p/N_p)/D_p = S_p` **精确**。⇒ 同一文件两个末端给出不同答案，而 `astro_sphere_sink.h:72-91` 声称逐字节等价。**推翻成立。**

### CE-2（推翻「负例控制证明本门非恒真」）—— 标量注入不能证明任何引擎性质

- **构造**：读 `drizzle_acceptance_test.cpp:108-120`。`inject` 乘在**已算完的** `sum_out64` 上。注释 `:108` 自认「仅作用于被测输出 …… **不改变引擎本身**」。
- **期望推翻**：一个能发现引擎归一缺陷的负例控制，必须把缺陷注入**引擎内部**。
- **是否推翻**：**推翻**。`|c·Σ_out − Σ_in|/Σ_in` 对任意 `c≠1` 必然超阈 —— 这是代数恒真式，与被测代码无关。更强的一条：`--sections A/D/E/C/T` 时唯一带 `pf<1` 的 B 段不执行，`inject = pf² = 1.0` ⇒ **注入是空操作，程序返回 0（成功）**。而 `:17-18` 声称「证明本门非恒真；恒真门没有证据资格」。
- **附证**：把判据放宽到 `rel64 < 2.0`（恒真），负例控制在 `pf=0.1`（`rel64≈0.99`）**仍会**判红 ⇒ 该控制对阈值放宽不敏感，无法承担「证非退化」的职责。

### CE-3（推翻 BLK-3）—— 截断 snr_model 块

- **构造**：把帧内 `snr_model` 块少写 1 字节。`hp_drizzle_api.cpp:740` `raw_size >= 28 + payload_bytes` 不成立 ⇒ `header_ok=false` ⇒ `:741-744` 只 `fprintf`。`snrModelPtr`/`snrModelF64Ptr`/`snrPtr` 全保持 `nullptr`。`astro_sphere_sink.cpp:250` `if (!snr_pts.empty())` 为假 ⇒ 不写 SNR Catalogue。`:262` finalize 成功 ⇒ `run_drizzle_internal` 在 `hp_drizzle_api.cpp:1300` 返回 **0**。
- **期望推翻**：P2/P4 的 SNR 贡献消失应当产生稳定错误码。
- **是否推翻**：**推翻**。返回码 0，产品无 SNR 通道，无任何非 stderr 痕迹。
- **变体**：全部控制点 `snr_psf=NaN` ⇒ `snr_evaluator.cpp:200-203` `valid==0` 返回 false ⇒ `:810` 无 else ⇒ `snrPtr` 空；但 `snrModelF64Ptr` 已在 `:808` 赋值 ⇒ `astro_sphere_sink.cpp:252` **照发这些被判定为无效的控制点**。产品内部自相矛盾。

### CE-4（推翻 `reverse_drizzle_science_test` 的信号门）—— 分母为 0 的场景

- **构造**：`negative_field` 真值场 `B = -500 + 100z`（恒负）。`reverse_drizzle_science_test.cpp:345` `if (t > 0) { denom_s += t; ... }` 把**每一个**像素筛掉 ⇒ `denom_s = 0` ⇒ `:372` `cr.signal_rel_err = 0.0`。`:477` `ok = cr.signal_rel_err < tol && ...` ⇒ `0.0 < 1e-3` **恒真**。
- **期望推翻**：把该场景的重建信号整体翻符号或乘 3，门应当变红。
- **是否推翻**：**推翻**。翻符号后 `signal_rel_err` 仍是 0.0，门仍绿。
- **同族**：`sb_err`（`:374` + `:490`）在 `compact_source`（`b0=0`）上分母恒 0 ⇒ 门恒真。

### CE-5（推翻 `test_spherical_overlap.cpp:793-795`）—— 位级恒等

- **构造**：读 `spherical_overlap.cpp:648` `(void)samples_per_edge;` —— 该形参被**丢弃**。`nside=1` 走 `:655` 之后的细分路径，该路径不依赖 `samples_per_edge`。故 `b8` 与 `b16` 是同一次确定性计算的结果。
- **期望推翻**：`err_16 <= err_8 + 1e-10` 在 `err_16 ≡ err_8` 时化为 `x <= x + 1e-10`。
- **是否推翻**：**推翻**。该断言在**任何**生产实现下都恒真，**算术上不可能失败** —— 而它的名字是「单调提升」，是一个关于采样密度的科学声称。**恒真门。**

### CE-6（推翻 `test_spherical_overlap.cpp:1249`）—— 全跳过即绿

- **构造**：令 `build_drop_polygon_adaptive` 对全部 5 个测试像素返回空（投影失败）。`:1222-1226` 五次 `continue`；`:1183` 的 `max_closure` 停在初值 0.0；`:1249` `ASSERT_NEAR("WCS TAN drop 闭合 < 1e-10", 0.0, 0.0, 1e-10)` ⇒ `|0-0| = 0 <= 1e-10` ⇒ **PASS**。
- **期望推翻**：整个 WCS-TAN drop 构建路径已死，门应判红。
- **是否推翻**：**推翻**。无 case 计数器、无断言。**恒真门（由静默跳过喂绿）。**

### CE-7（推翻 `publish.h` 的「编译期对齐证明」）—— 断言不接触被比对对象

- **构造**：读 `publish.h:36`，只 `#include <stdint.h>`；`:130-146` 的断言形如 `static_assert(AIO_PUBLISH_OK == 0 && ...)`，而 `AIO_PUBLISH_OK` 就定义在 `:53`。把 `lib/include/acsd/io/aio_abi_v1.h:46-66` 的 `aio_status` 逐值取来与之对照。
- **期望推翻**：若对方重编号（例如 `AIO_ERR_IO` 由 4 改 5），该 `static_assert` 应变红。
- **是否推翻**：**推翻证明力，但未推翻当前一致性**。我实测 `aio_abi_v1.h` 的 0..15 / 70 / 71 与 `publish.h` **逐值相同** ⇒ 结论正确；但 `publish.h` 声称的「全域一致 …… `_Static_assert` 编译期对齐证明」**不成立**，因为该断言是「用定义式既当被检量又当期望量」的自洽式。这正是本轮口径里最高价值的一类。

### CE-8（推翻 `module.yaml` 的机读声明）—— 三处行号

- **构造**：`grep -n "add_library(acsd_drizzle" CMakeLists.txt`；`grep -n "HP_DRIZZLE_API" hp_drizzle_api.h`；`grep -rn "pragma omp" lib/algorithms/drizzle/`。
- **结果**：静态库在根 **:745**（非 356-366）；八个 API 声明在 **82/102/110/126/155/211/220/221**（非 42/62/70/130/139/140，且是八不是六）；omp 在 **1988/2319/2344 + snr_evaluator.cpp:336**（非 1639-1644）。
- **是否推翻**：**推翻**。三处全部证伪，且 `threading_model: host_executor_lease` 与自陈的 omp 实现矛盾 ⇒ **机读字段给任何检查器一个假绿**。

### 构造后**未能**推翻的（诚实记录）

- **CE-9：`Σ(support×A_p)=A_drop` 是否恒真？—— 我推翻子代理，保留该门。**
  `reference_overlap.cpp:643-644` 的 `support·A_p = (a_jp/A_p)·A_p` 确实代数抵消，`sum_weighted` 确实 `= Σ a_jp`，注释 `// = a_jp` 也自认。但这**不等于**该门零判别力：化简后的 `Σ_p a_jp = A_drop` 是**真实的覆盖-统一性（partition of unity）性质**，要求候选集完整 + S-H 裁剪守恒，容差 1e-6 紧。候选漏选或裁剪错误都会使其变红。
  ⇒ **子代理称该门「零信息」是夸大。我部分否决该条**，只保留「断言命名与注释误称它在验 support 归一」+「`:623` 独立参考被算出却不判」两点。
- **CE-10：IDW 用度数是否错？—— 不能推翻。** γ 全体乘同一常数 `c` 时 `w_i` 同乘 `c^(-p)`，估计量 `Σw·s/Σw` 中该因子精确抵消；kNN 集合来自单位球 3D 坐标，与尺度无关。**非缺陷**，与子代理 REJECTED-2 一致。
- **CE-11：evaluate() 与 evaluateBatch() 的 k 是否不一致？—— 不能推翻。** `snr_evaluator.h:114 DEFAULT_KNN = 16`，`snr_evaluator.cpp:347 k_use = min(k,16) = k`，`idx_buf[16]` 恰好够。**非缺陷**（我自己提出、自行否决）。
- **CE-12：构建引用是否悬空？—— 不能推翻。** 两个 `CMakeLists.txt` 引用的 12 个路径逐一 `test -f`，**0 悬空**。**本片最干净的一面。**

---

## 6. 盲复算（遮住既有判定，独立取证）

**方法**：先在**不读** `审稿-RR05-P3 守恒映射算子.md`、`审稿-R2-S3`、`审稿-R3-T3`、`G08-05-整改-*` 等他人产出的前提下，对四项断言独立重算并定级；定级完成后再比对。

| 断言（既有口径） | 我的盲复算 | 判 |
|---|---|---|
| 「drizzle 通量守恒，Σout=Σin 闭合 <1e-7，A/B/D/E 段全绿」 | 闭合式在**核按 drop 面积归一**的前提下对**任何几何**恒成立 ⇒ 它是**构造性恒等**，只对「全局标量」类缺陷有判别力，对「再分配」类（正是本模块要防的）零判别力。且其负例控制只乘标量（CE-2），非负控制默认不跑。 | **偏松** |
| 「候选零漏选门 9003 例承载，零漏选已证」 | 承载文件 `oracle_independent_test.cpp` 真值集由 13 点采样给出（`:107-120`），是真交集集的**严格子集** ⇒ 「FN=0」只在本采样栅格上成立，证明力低于声称；且该门未注册 ctest。 | **偏松** |
| 「无 long double，静态断言双重保证」 | `drizzle_acceptance_test.cpp:449-452` 的 `sizeof` 断言是平台属性，**检不出源码中的 long double**，却无条件 `g_pass++` 并打印 PASS。 | **偏松** |
| 「poly_clip 生产零调用，是已登记的处置」 | 我独立 `grep` 全仓：`gnomonicForward`/`clipPolygon`/`polygonArea`/`gnomonicReverse` 仅在自身与注释/头中出现。**该判断成立**。 | **一致** |

**复算小结**：四项中三项**偏松**、一项**一致**。无「偏严」项。即：既有口径在**证据强度**维度系统性地比代码实际提供的更乐观，而生产代码侧（BLK-1、BLK-3）确实存在未被任何在册门覆盖的缺陷。

---

## 7. 子代理派发记录

### 7.1 派发

| # | 任务 | 覆盖 | 状态 |
|---|---|---|---|
| A | 静默降级 / fail-open 掩码 | 26 份成员 | 完成 |
| A' | 同上（重复派发） | 同 | 完成 |
| B | 门 / oracle 判别力 | 9 份测试 + 生产几何 | 完成 |
| B' | 同上（重复派发） | 同 | 完成 |
| C | 退役对象 / 悬空引用 / 私建线程池 / 构建完整性 / 硬编码 | 26 份 + 全仓 grep | 完成 |
| C' | 同上（重复派发） | 同 | 完成 |
| D | 数值稳定性 + P3 通量守恒独立推导 | 8 份生产数值码 + `docs/science/` 正本 | 完成 |

**实际派出 8 个子代理运行、覆盖 4 个不同任务**（每任务派了 2 份，用于独立复现）。口径说明：本车道要求 3-5 个；因我在同一批次内误将 4 条指令各写两遍，实际运行数超出，**但互为独立复现，不是 8 个独立视角**。计入「有效独立视角 = 4」。

### 7.2 逐条复核方式

对每个子代理 finding，我执行三步：**(a)** 回到原文 `read` 或 `sed -n` 定位该 `文件:行`；**(b)** 判断该行是否支持子代理的因果链；**(c)** 追一次调用方/被调方，判断缺陷是否真可达。**未通过 (a) 的一律不进本交付件。**

### 7.3 我**否决**的子代理结论（6 条）

| 子代理结论 | 我的裁决 | 依据 |
|---|---|---|
| B/B'：「`reference_overlap.cpp:676` 纯代数恒等，**零信息**」 | **部分否决** | 我读 `:643-644 / 671-676` 后推出：化简后为 `Σ a_jp = A_drop`，这是真实的覆盖-统一性门，对候选漏选与裁剪错误有判别力（容差 1e-6）。子代理把「命名误导」升级成「零判别力」是夸大。保留命名误导 + `:623` 独立参考未断言两点，剔除「零判别力」。 |
| B/B'：「`reference_overlap.cpp:600-601` 的 `a_pixel_ref` 用生产函数 ⇒ 该门非独立」 | **降级保留** | 事实成立（`:590` 用 `spherical_polygon_area(get_healpix_boundary(...))`）。但该门断言的是「drop 完全包含像素时 overlap == 像素面积」，是**几何恒等式**，本就允许用同法求值；真正的问题是 `get_healpix_boundary` 的共因缺陷对两侧同向——这一点成立。定为 HIGH 而非 CRITICAL。 |
| C/C'：「`poly_clip.cpp:60-71` 有限哨兵是**活**的 fail-open」 | **部分否决（降为死代码）** | 我独立 grep 全仓确认零调用，且 `README.md:171` 与 `DRIZZLE_GEOMETRY.md` 已登记 DISP-DRZ-008。缺陷本身真实、且与 `wcs_sip.cpp:208-216` 的明文裁决**正面冲突**（这是我的独立发现：两模块对同一 TAN 数学给出相反处置，构成再采纳陷阱），但**不是活的 fail-open**。 |
| C/C'：「`hiss_write_probe.cpp:23` 硬编码 `A_p` 是 AGENTS.md §6 违规」 | **降级为建议** | 我算过：nside=65536 时 `4π/(12·65536²) = 2.4383e-10`，与硬编码值在当前参数下**一致**。属「当前正确、参数一变即冻结错误」，非现行错误。 |
| A/A'：「`wcs_sip.cpp:240-245` 返回 `(0,0)` 是**活**的生产缺陷」 | **否决（改判潜伏）** | 我追调用方：`hp_drizzle_api.cpp:658-663` 在构造 `WcsSip` 前已用 `read_wcs_params_from_frame` 的 −9 拦掉无 WCS。不可达。但我**保留并加重**其姊妹问题——`wcs_sip.cpp:48-51` 的 CD 逆置零**是活的**（走 `skyToPixel`，反向 drizzle 生产路径），见须修 #5。 |
| A/A'：「`module.yaml` 只是注释陈旧，不算缺陷」 | **否决（升级为须修）** | 该文件是**机读 manifest**，`threading_model: host_executor_lease` 与实际 omp 实现矛盾 ⇒ 任何读该字段的检查器都拿到假绿。这不是注释陈旧，是**声明面失真**。 |

### 7.4 我**采信并独立复核**的子代理结论（最重 4 条）

| 结论 | 我的行级复核 |
|---|---|
| B：`tests/CMakeLists.txt:272-277` 独立 Oracle 因红被摘 | 我 `sed -n '265,285p'` 直读原文，确认三条红灯数字与「不注册 ctest」原文 → 升为 **BLK-2** |
| D：`astro_sphere_sink.cpp` 归一因子/除数面积定义不一致（BLK-1） | 我逐行读 `:190-191`（未量化）与 `:521-544`（量化），自行推出 `ρ = 255·S/lround(255·S)` 并算例 → **采纳并升为阻断** |
| D：`snr_evaluator.cpp:306/317/365/379` 重建 SNR 乘帧级标量 | 我读该四行 + `snr_evaluator.cpp:210` 确认 `snr_phot_`/`median_snr_` 是帧级标量；采纳为须修（与 BLK-3 合并叙述） |
| B：`reverse_drizzle_science_test.cpp:372-374` 零分母 | 我直读 `:340-385` 与 `:455-504`，自行推出 `negative_field` 全像素被 `t>0` 筛掉 → **采纳** |

### 7.5 子代理**未覆盖**而由我本人补上（4 条，无代理背书）

1. `publish.h:50-51 vs :130-146` 的自洽式断言（CE-7）。
2. `test_spherical_overlap.cpp:793-795` 的位级恒真门（CE-5）—— 与 `spherical_overlap.cpp:648` 的 `(void)` 对照，是我的交叉取证。
3. `module.yaml` / `README.md` / `hips/README.md` 的行号悬空（CE-8）—— 全部用 grep 实测证伪。
4. `drizzle_acceptance_test.cpp` 的负例控制在 `--sections A/D/E/C/T` 下为空操作（CE-2 后半段）—— 由我读 `:463-493` 与各段 `run_case(..., 1.0, ...)` 调用点自行推出。

---

## 8. 自证段（可复跑命令）

> 全部为**只读**命令。中文路径已用 `git -c core.quotepath=false`。未执行任何构建 / 测试 / 二进制。

```bash
cd "/workspace/Astro CS Database"

# S1 取出本片成员清单与行数（权威版）
awk '/^  - 片号: ALG-drizzle-004$/,/^  - 片号: ALG-integration-001$/' \
  run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# S2 成员份数与逐份行数（应得 26 份 / 8998 行）
while IFS= read -r f; do printf "%6d  %s\n" "$(wc -l < "$f")" "$f"; done < /tmp/shard004.txt
# 末行汇总：files: 26  missing: 0  total lines: 8998

# ---- BLK-1 归一因子与除数面积定义不一致 ----
sed -n '186,193p;521,544p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
sed -n '50,52p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h
# 对照另一末端（未量化面积）：
sed -n '190,191p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp

# ---- BLK-2 独立 Oracle 因红被摘出 ctest ----
sed -n '159,170p;272,277p' lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt

# ---- BLK-3 snr_model 只记日志不失败 ----
sed -n '741,754p;855,857p' lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp
# 对照：同函数对同类输入是硬失败（返回 -10）
sed -n '465,469p' lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp

# ---- CE-5 恒真门：形参被丢弃 ⇒ 两次结果逐位相同 ----
sed -n '781,795p' lib/algorithms/drizzle/healpix_drizzle/tests/test_spherical_overlap.cpp
sed -n '646,658p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp   # (void)samples_per_edge;

# ---- CE-6 静默跳过喂绿门 ----
sed -n '1183p;1222,1229p;1240,1249p' \
  lib/algorithms/drizzle/healpix_drizzle/tests/test_spherical_overlap.cpp

# ---- CE-7 publish.h 自洽式断言（应显示本头从未包含 aio_abi_v1.h）----
sed -n '36p;50,51p;130,146p' lib/algorithms/drizzle/hips/include/acsd/hips/publish.h
# 对方真值（逐值比对，当前一致）：
grep -n -A 20 "typedef enum aio_status" lib/include/acsd/io/aio_abi_v1.h

# ---- CE-8 module.yaml / README 行号悬空 ----
grep -n "add_library(acsd_drizzle" CMakeLists.txt                      # => 745（非 356-366）
grep -n "HP_DRIZZLE_API" lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h  # => 82,102,...
grep -rn "pragma omp" lib/algorithms/drizzle/                            # => 1988,2319,2344 + snr_evaluator.cpp:336
grep -n "add_library(acsd_p1_hips_writer" lib/algorithms/drizzle/hips/CMakeLists.txt  # => 50（非 43）

# ---- 私建线程池（AGENTS.md §6 违规）----
sed -n '334,337p' lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp
grep -n "threading_model" lib/algorithms/drizzle/module.yaml            # => host_executor_lease（与上矛盾）

# ---- 组内公共 F_ref 门筛掉损坏帧 ----
sed -n '424,444p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp

# ---- poly_clip 与 wcs_sip 对同一 TAN 数学的相反处置 ----
sed -n '58,71p'      lib/algorithms/drizzle/healpix_drizzle/poly_clip.cpp   # 有限哨兵 ±1e6
sed -n '208,216p'    lib/algorithms/drizzle/healpix_drizzle/wcs_sip.cpp     # 明文「不得返回有限哨兵值」
grep -n -A 8 "typedef enum aio_status" lib/include/acsd/io/aio_abi_v1.h > /dev/null

# ---- CE-12 构建引用完整性（本片最干净的一面，应无输出）----
for f in lib/algorithms/drizzle/src/acsd_p1_drizzle.def \
         lib/algorithms/drizzle/src/module_exports.map \
         lib/algorithms/drizzle/src/module_entry.cpp \
         lib/algorithms/drizzle/hips/src/acsd_p1_hips_writer.def \
         lib/algorithms/drizzle/hips/src/module_exports.map \
         lib/algorithms/drizzle/hips/src/module_entry.cpp \
         lib/algorithms/drizzle/hips/src/aio_publish.cpp ; do
  [ -f "$f" ] || echo "MISSING $f"; done
# 实测：0 MISSING

# ---- 覆盖度自证（本人 read 覆盖）----
# 完整读完 20 份 5406 行；部分 6 份 591 行（清单见 §1.3）；合计 5997/8998 = 66.7%
```

---

## 9. 交付口径声明

- **计数口径**：份数 = YAML `成员文件:` 列表逐条；行数 = `wc -l`（含空行）；「读了多少」= 本人 `read` 工具逐行覆盖，`sed`/`grep` 定位行单列不计入（§1.4）。
- **判定口径**：阻断 = 生产产品面可产出错误科学数据且返回成功，或使在册验证体系整体失效；须修 = 真实缺陷但当前不可达/仅影响诊断/仅影响可移植性；建议 = 潜伏陷阱、诚实披露的口径问题、硬编码与一致性瑕疵。
- **未做的事**：未编译、未跑 ctest / pytest / 任何二进制；未做任何 git 写（无 add / commit / checkout / reset / stash / rm --cached）；未修改任何仓内文件；未读 `/tmp/acsd_g08/`。本交付件为本次唯一写入。
- **UNRESOLVED**：见 §10。

---

## 10. UNRESOLVED（需前台裁决，本审稿人无权限判定）

| # | 事项 | 分歧 | 影响 | 所需输入 |
|---|---|---|---|---|
| U-1 | **量化面积 vs 未量化因子，BLK-1 应按「缺陷」还是按「已知 legacy 等价约束」处置？** | `astro_sphere_sink.h:82-84` 明文记录了 uint8 量化是**刻意**的 legacy 等价行为；而 `write_hips_direct` 用未量化面积 ⇒ 两末端本就不同。若量化是必须保留的合同，则 BLK-1 的正确修法是**把 k 的分母改成 `area_q`**（使 `sig ≡ S_p`），而不是取消量化 | P3 面亮度发布正确性；`drizzle_pf_sb_gate` 的定位 | 负责人裁定：`covered_area` 量化的合同地位；以及 `write_hips_direct`（profile=0，通用档）与 `write_hips_phase1`（profile=1，Phase1 生产档）是否**必须**逐字节一致 |
| U-2 | **被摘出 ctest 的门，应当修红还是应当重新注册？** | `tests/CMakeLists.txt:272-277` 的理由是「容差与断言属科学判据面，本轮禁止放松/删改」。但 4 个门长期不注册，等于把独立证据面永久熄灯 | 本片乃至整个 P3 的证据资格 | 负责人裁定：科学判据面的容差是否允许在有记录、有论证、有负例控制的前提下调整 |
| U-3 | **`threading_model: host_executor_lease` 与实际 omp 的矛盾，是改声明还是改实现？** | `module.yaml:16-20` 自陈「现状实现为 omp … ThreadLease 接线为迁移整改点」；即声明写的是**目标态**而实现是**现状态** | 所有读该字段的自动检查器 | 负责人裁定：module.yaml 字段是**目标合同**还是**现状声明**？若是目标合同，须在字段上显式标注 status 避免假绿 |
| U-4 | **P4 稠密 SNR 是接线缺失还是死重？** | `drizzle_engine.cpp:1480` 核参数为无名形参 ⇒ 重建结果不参与累加；但 `hp_drizzle_api.cpp:691-713` 每帧做 O(N) KD-tree + 逐像素 IDW，且 HiPS 目录照发控制点 | P4 创新点是否真的实现；每帧无效 CPU 开销与 NaN 风险 | 负责人裁定 + 与 `ALG-drizzle-001/002` 车道的联合核对（该结论落在非本片文件） |
---
---

# 补充 A —— 第 3 批子代理回报后的追加（第 1 遍未完，补齐后覆盖率与判定同步更新）

> 追加时间：子代理 C/C'（结构面）第二轮回传后。追加内容全部经**本人独立行级复核**（复核命令见 §8）。凡未能回到原文行号者一律不收。

## A.0 覆盖率与判定同步更新

| 项 | 补充前 | 补充后 |
|---|---|---|
| 子代理运行数 | 8 | 8（其中 C/C'、D 各返**两轮**报告，第二轮含新增内容） |
| 有效独立视角 | 4 个任务 | **4 个任务 / 6 轮报告** |
| 本人 `read` 直读覆盖率 | 66.7%（5997/8998） | **不变，66.7%** —— 本补充全部靠定点 `sed`/`grep` 复核，**不计入 `read` 覆盖率**，口径不虚增 |
| 阻断数 | 3 | **4** |
| 须修数 | 14 | **18** |

**覆盖率口径的诚实说明**：补充 A 的 4 条阻断/4 条须修是靠**定点行级复核**取得的，不是靠通读。这意味着本片仍有 33.3% 的行（3001 行，集中在 3 份大测试文件）**未被本人 `read` 逐行读过**。下述结论在这些区域之外的置信度是高的；在那 3001 行内仍可能有未发现项。

## A.1 阻断（新增 1 条）

### 【BLK-4】算法模块内私建线程池 —— nanoflann 建树按 `hardware_concurrency()` 扇出 `std::async`

- **位置**：`snr_evaluator.cpp:132-134`（本片）
- **机理链（本人逐环复核）**：
  1. `snr_evaluator.cpp:132-133`：`tree = new KDTree(3, adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(32));` —— 该 Params **只设 `leaf_max_size`**，`n_thread_build` 保持默认 0。
  2. `nanoflann.hpp:1911-1917`（vendored，本片目录）：
     ```
     if (params.n_thread_build > 0) { Base::n_thread_build_ = params.n_thread_build; }
     else { Base::n_thread_build_ = std::max(std::thread::hardware_concurrency(), 1u); }
     ```
  3. `nanoflann.hpp:1433` 与 `:3883`：`fut_ = std::async(...)` —— 头文件 `:1811` 自述「build using `std::async` (unless `NANOFLANN_NO_THREADS` is defined…)」。
  4. `grep -rn NANOFLANN_NO_THREADS --include=CMakeLists.txt --include=*.cmake .` → **空**（实测）。`snr_evaluator.cpp` 的 `#pragma omp` 在 `_OPENMP` 未定义时被静默丢弃，但 **`std::async` 不受此保护**。
- **生产可达**：`hp_drizzle_api.cpp:809`（`buildF64`）/`:834`/`:888`（`build`）→ `snr_evaluator.cpp:206/264` → `Impl::buildTree()`（`:124-135`）→ `:132`。每帧每控制点集触发一次。
- **期望推翻**：算法模块不私建线程池，池所有权归调度器（AGENTS.md §6）；本模块入口 `src/module_entry.cpp:909` 亦自引 `FORBID-003: 禁私建线程池`。
- **是否推翻**：**推翻**。线程数来源是 `hardware_concurrency()`，**不是** `config.threads`、不是 `omp_get_max_threads()`、不是 host executor 租约；64 核机器上每帧建树会拉起 64 个 `std::async` 任务，而流水线其余部分的并发度由调度器决定 —— 两级并发不受统一管辖。
- **与阻断-1 的关系**：无。这是独立的资源治理缺陷。

## A.2 须修（新增 4 条，均经本人复核）

### 【须修-19】SIP `B_ORDER` / `BP_ORDER` 被校验、被打印，却被丢弃 —— 高阶畸变项静默不生效

- **位置**：`hp_drizzle_api.cpp:465-466`（校验）、`:472-473`（读 A/B 的循环界）、`:493-494`（读 AP/BP 的循环界）、`:506-507`（日志）
- **本人亲读原文**（我第一遍读过这两段但未抓住，子代理 C 指出后我回原文确认）：
  ```
  472:        for (int i = 0; i <= a_order; i++) {
  473:            for (int j = 0; j <= a_order; j++) {
  ...
  476:                std::snprintf(key, sizeof(key), "A_%d_%d", i, j);
  ...
  480:                std::snprintf(key, sizeof(key), "B_%d_%d", i, j);
  ```
  **读 B 系系数的循环界用的是 `a_order`，不是 `b_order`**。`:493-494` 读 AP/BP 同理，两个上界都用 `ap_order2`。
  而 `:465-466` 校验了 `b_order` 与 `bp_order` 各自在 `[0,5]`；`:506-507` 的日志打印 `a_order, b_order, ap_order2, bp_order2` —— **打印的是从未被用作循环界的值**。
- **构造**：`A_ORDER=2, B_ORDER=4` 的 FITS 头（完全合法）。代码只读到 `B_0_0..B_2_2`，`B_3_*`/`B_4_*` **静默丢弃**：无错误码、无告警、日志却显示「B_ORDER=4」。
- **影响**：高阶 SIP 畸变项不生效 ⇒ 天球坐标系统性偏差 ⇒ 直接污染 P3 守恒映射与 P4 控制点定位。**这是静默降级，不是文档问题。**
- **反向**：`B_ORDER < A_ORDER` 时会去读不存在的键（KV 缺失为 no-op，不致命）。
- **与 DISP-DRZ-001 的区别**：`README.md:178` 登记的是「sip_order 注释 0..4 vs 校验 [0,5]」（口径），**未登记**「b_order 被丢弃」（正确性）。应补条目并升为红线。

### 【须修-20】`README.md:95-96` 伪造并反转冻结正本 `DRIZZLE.md` 的 NaN 条款 —— 把红线违反降格为 LOW 文档缺口

- **位置**：`README.md:95-96`（本片，我亲读）
  ```
  - **NaN/Inf**：值像素非有限 → 主循环静默 continue（:1712），不进累加器、
    无计数暴露（与 DRIZZLE.md:96 "不掩膜传播 NaN" 不符——DISP-DRZ-004）；
  ```
- **本人复核（`sed -n '96p' docs/science/DRIZZLE.md`）**：
  - `DRIZZLE.md:96` 实际内容 = `组合系数 c_jp=w_jp/N_p=a_jp/(A_pixel,j·D_p) ⇒ Var(S_p)=Σ v_j c_jp²）——`，**与 NaN 无关**。
  - `grep -c "不掩膜传播 NaN" docs/science/DRIZZLE.md` → **0**。该原句在整份正本中不存在。
  - 真实条款在 **`DRIZZLE.md:158`**：`源像素 NaN/Inf（值） | **样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**（rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN）…且每个输出像素**必须**暴露被剔除样本计数 n_rejected_nonfinite`。
- **三重问题**：① 引用了一段不存在的原句；② 把方向搞反（正本**强制**掩膜+计数，README 描述的是「静默 continue 无计数」并称之为「不符」）；③ `DRIZZLE.md:158` 用的是「**必须**」，是红线，而 README 把它归为一条 LOW 级 DISP 条目。
- **附证（本人亲读）**：`README` 称「无计数暴露」也不成立 —— `hp_drizzle_api.cpp:1292-1295` 确实把 `n_rejected_nonfinite{,_value,_variance}` 与 `n_rejected_nonpositive_weight` 回填进 `HpDrizzleResult`。所以这一段在**引文、行号、方向、严重度**四个维度同时失真。
- **判定**：须修（文档面），但**严重度应升为红线**，且需与 `ALG-DRZ-001 §10 DISP-DRZ-004` 联动改写。

### 【须修-21】`healpix_core.cpp:9-10` 的退役判据**已被证伪**，而本片 `spherical_overlap.h:37` 就是活消费者

- **位置**：`healpix_core.cpp:9-10`（非本片，但 `spherical_overlap.h` 是本片）
  ```
  // EXIT:  引用清点为零（drizzle 模块内 grep 无 #include "healpix_core.h" 消费点、
  //         CMake 源列表移除）后删除本 shim；权威实现不受影响。
  ```
- **本人复核**：`grep -rn '#include "healpix_core.h"' lib/algorithms/drizzle/ | wc -l` → **15**。其中 **`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h:37`** 正是**本片成员文件**，且**是我本人第一遍逐行读到并原样引在 §3 第 8 行的那一行**。
- **兄弟文件自相矛盾**：同目录的 `healpix_core.h:4-6` 把消费者列得**完全正确**（三个生产点 + 九个 tests TU）；`.cpp` 那份的 EXIT 判据是错的。
- **判定**：**「退役对象仍有活调用者」的又一例**，且是「自证式清点」——判据本身可被一条 grep 推翻却从未被跑。任何人照 `.cpp:9` 的判据删 shim，会直接打断本片 `spherical_overlap.h` 及另 14 个 TU。
- **附**：`healpix_core.cpp:12` 的 `AUTHORITY: ENGINEERING_SPEC.md §2/§9` 在活树不存在（子代理 C 实测，仅存 `run/FINAL-07-e2e/**` 归档）。

### 【须修-22】本片两份 README 的「导出符号数与行号」双错，且与本机头文件互相矛盾

- `README.md:13` + `module.yaml:3-4` 称 C ABI 导出是 `hp_drizzle_api.h:42,62,70,130,139,140` **六符号**。
- **本人复核**：`grep -n "HP_DRIZZLE_API" hp_drizzle_api.h` → 函数声明在 **:82, :102, :110, :126, :155, :211, :220, :221**，共 **8 个**，**无一落在所引行号上**。
- HiPS 侧同类：`hips/CMakeLists.txt:19` 与 `hips/README.md:14` 称 `aio_hips_*` **九符号**（子代理 C 实测头文件实为 13 个 `AIO_HIPS_EXPORT`）。
- 附：`hp_drizzle_api.h:6` 引用的根 CMake 行号 `:745` 反而是**对的** ⇒ 本模块文档**自相矛盾**。

## A.3 我追加的反例

### CE-13（推翻须修-19）—— B_ORDER 丢弃

- **构造**：`A_ORDER=2, B_ORDER=4`。`:465-466` 校验通过（都在 `[0,5]`）；`:472-473` 循环界是 `a_order=2` ⇒ 只读到 `B_0_0..B_2_2`；`B_3_*`/`B_4_*` **从未被读取**；`:506` 仍打印「B_ORDER=4」。
- **期望推翻**：FITS 标准允许 `B_ORDER ≠ A_ORDER`，高阶项应生效。
- **是否推翻**：**推翻**。日志会给出一个系统从未使用过的数字。

### CE-14（推翻须修-20）—— 伪造引文

- **构造**：`grep -c "不掩膜传播 NaN" docs/science/DRIZZLE.md` → 0；`sed -n '96p'` → α 缩放律。
- **期望推翻**：README 引用的正本原句存在。
- **是否推翻**：**推翻**。

### CE-15（推翻 BLK-4）—— 线程池

- **构造**：`sed -n '1911,1917p' nanoflann.hpp` + `grep -n std::async nanoflann.hpp` + `grep -rn NANOFLANN_NO_THREADS --include=CMakeLists.txt --include=*.cmake .`（空）。
- **期望推翻**：`n_thread_build=32` 参数已限定线程数，或 `NANOFLANN_NO_THREADS` 已在构建中定义。
- **是否推翻**：**推翻**。`KDTreeSingleIndexAdaptorParams(32)` 的 `32` 是 `leaf_max_size`，**不是** `n_thread_build` —— 参数名极易误读，这正是它逃过检查的原因。

## A.4 对第 3 批子代理的复核裁决

**采信（本人独立复核后）**：BLK-4 线程池（3 环证据链逐环核过）、须修-19 B_ORDER（**我本人第一遍读过该段原文**，子代理指出后我回原文确认）、须修-20 伪造引文（2 条 grep 核过）、须修-21 退役判据证伪（grep 计数 + **我本人读过的 `spherical_overlap.h:37`**）。

**否决 / 降级**：
| 子代理结论 | 我的裁决 | 依据 |
|---|---|---|
| C：「`drizzle_nonfinite_test` 编入但从未 `add_test`」 | **降级为建议** | 该文件不在本片 26 份内，其缺席属兄弟片裁决面；作为**本片证据缺口**记入，但不定为本片须修。 |
| C：「`publish.h` 的 static_assert 是自洽式断言（假发现）」 | **部分否决** | 子代理 C 实测 `aio_abi_v1.h` 逐值一致并判「成立」。我的独立判断是：**数值一致性成立，但证明力为零**（本头从未 include 对方，断言只比对自己）。两者不矛盾：**结论对、证明无效**。维持我的 CE-7 表述。 |
| C/D：「`snr_evaluator.cpp:336` 的 omp 是私建线程池」 | **升级并替换** | 该 omp 本身走进程级 libgomp 队、且被调用方 `module_entry.cpp:911/926` 的 `omp_set_num_threads(leased)` 间接约束，**不是**私建池。真正的私建池是 nanoflann 的 `std::async`（BLK-4）。我把原 omp 条目降为 BLK-4 的补充说明。 |
| C：「`bench_write.cpp` / `hiss_write_probe.cpp` 编而不注册是缺陷」 | **否决** | 子代理 C 自己复核后判「注册策略明示，不是判据门」（`tests/CMakeLists.txt:230-232`）。与 BLK-2 的 13 个**门类**测试性质不同，**不得混为一谈**。 |
| C：「`DEFAULT_KNN` 静默截断 16 与 `evaluate()` 不一致」 | **否决** | 与我自己 CE-11 的独立裁决一致：`snr_evaluator.h:114 DEFAULT_KNN = 16` ⇒ 两条路恒等。子代理 C 在 Mission 5 REJECTED 段自己也否决了同一候选。 |

## A.5 补充后的最终三强

| 序 | 阻断 | 为什么排这个位次 |
|---|---|---|
| 1 | **BLK-1** 归一因子与除数面积定义不一致（`astro_sphere_sink.cpp:539` vs `:544`） | 唯一直接产出**错误科学数据**（面亮度偏至 −50%、小覆盖叶 NaN）且返回成功的缺陷；且唯一触碰该末端的门把缺陷前提钉死为格点，使判据不可能红。 |
| 2 | **BLK-2** 本片唯一独立 Oracle 因红被摘灯，另 3 个门编而不注册 | 决定「全部 PASS 声称」是否有证据资格。基线上的绿是**排除**取得的，不是**正确**取得的。 |
| 3 | **BLK-3 / BLK-4** SNR 静默退化 + nanoflann 私建线程池 | 前者使 P2/P4 贡献静默消失且产品自相矛盾；后者是本轮固化检查项的第 10 例新违规，且线程数来源完全脱离调度器租约。 |

## A.6 补充自证命令

```bash
cd "/workspace/Astro CS Database"

# A.6.1 BLK-4 私建线程池（三环）
sed -n '132,134p' lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp
sed -n '1911,1917p' lib/algorithms/drizzle/healpix_drizzle/nanoflann.hpp
grep -n "std::async"          lib/algorithms/drizzle/healpix_drizzle/nanoflann.hpp   # 1433 / 3883
grep -rn "NANOFLANN_NO_THREADS" --include=CMakeLists.txt --include=*.cmake .          # 空 = 从未定义

# A.6.2 须修-19 B_ORDER / BP_ORDER 被丢弃
sed -n '465,466p;472,473p;476p;480p;493,494p;506,507p' \
  lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp
# 可见：校验用 b_order，读 B 的循环界却是 a_order，日志打印 b_order

# A.6.3 须修-20 伪造并反转的引文
sed -n '95,96p'   lib/algorithms/drizzle/README.md
sed -n '96p'      docs/science/DRIZZLE.md        # 实为 α 缩放律
grep -c "不掩膜传播 NaN" docs/science/DRIZZLE.md # 实为 0
sed -n '158p'     docs/science/DRIZZLE.md        # 真实条款：样本级掩膜+强制计数
# 附：计数其实有回填，README 的「无计数暴露」亦不成立
sed -n '1291,1295p' lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp

# A.6.4 须修-21 退役判据已被证伪（本片成员即为活消费者）
grep -rn '#include "healpix_core.h"' lib/algorithms/drizzle/ | wc -l   # => 15
sed -n '37p'  lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h   # 本片成员，活 include
sed -n '9,10p' lib/algorithms/drizzle/healpix_drizzle/healpix_core.cpp     # EXIT 判据声称「零消费点」

# A.6.5 须修-22 导出符号数与行号双错
grep -n "HP_DRIZZLE_API" lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h  # => 82,102,110,126,155,211,220,221（8 个）
grep -n "356-366\|:42,62,70,130,139,140" lib/algorithms/drizzle/README.md \
                                 lib/algorithms/drizzle/module.yaml
```
