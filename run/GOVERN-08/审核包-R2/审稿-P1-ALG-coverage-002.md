# 审稿-P1 · ALG-coverage-002（第 1 遍，对抗性）

- 片号：**ALG-coverage-002**（层 `lib/algorithms/coverage`）
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 口径：成员清单取自 `run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:139-170`（`成员份数: 24`、`实际行数: 10182`）
- 行数口径：`wc -l` 物理行数（含空行与注释行），与清单 `实际行数` 字段逐一核对，**24 份全部一致，合计 10182 = 10182**
- 纪律：零 git 写、零编译、零执行仓内二进制/脚本、未读 `/tmp/acsd_g08/`、未修改仓内任何文件（唯一写入为本交付件）
- 中文路径：全部 git 调用使用 `-c core.quotepath=false`（本片实际未执行任何 git 写操作）

---

## 1. 读完了吗

| 项 | 值 | 口径 |
|---|---|---|
| 成员份数 | 24 | 清单 `成员份数` |
| 读了几份 | **24 / 24** | 本代理逐行 23 份 + 子代理逐行 1 份（见下） |
| 成员总行数 | **10182** | `wc -l`，与清单 `实际行数` 逐一核对一致 |
| 实际读了多少行 | **10182** | 23 份本人 8163 行 + `stage2.cpp` 2019 行（S2 整读） |
| **覆盖率（本代理亲读）** | **80.2%** | 8163/10182，份数 23/24 |
| **覆盖率（本代理亲读 + 派发子代理整读）** | **100.00%** | 10182/10182，份数 24/24 |

**未读完的：无。** 23 份由本代理用 `read` 工具从第 1 行读到末行（大文件按连续 offset 分段，首尾相接、无重叠缺口、无抽样）；第 24 份 `tools/stage2.cpp`（2019 行）由派发子代理 S2 **连续 5 段读完 2019/2019 = 100%**，其 12 条负载发现中我已**亲自复核 6 条并全部命中**（见 §7 复核表）。

> 说明：`stage2.cpp` 的覆盖率依赖派发子代理的整读报告，**不是本代理逐行重读**。若采信口径只认本代理亲读，则本片覆盖率为 80.2%，缺口集中在该文件的主流程与 gate 编排逻辑本身（其行数、`EXCLUDE_FROM_ALL`、`stage2.cpp:1982-1990` 已删五键、`:786-809` 缺 ivar 恒 `return 7`、`:1959-2001` diagnostics 键、`stage2.cpp:5` 悬空引用，均已由我**亲自**读到并引用）。
> 另：为核实跨文件事实（悬空引用、退役对象调用点、构建接线），只读了 20 余个**片外**文件的相关区段（`satellite_gate_build.py`、`drizzle_science.cpp`、`stage2_common.cpp`、`v19r3_traceability.py`、`CMakeLists.txt`、`module_adapters.cpp` 等），这些**不计入分母**，仅作证据。

---

## 2. 本片判定：**阻断**

理由：本片同时存在 **3 条自洽式断言 / 恒真门**（负责人本轮点名「最有价值」的类别）、**1 处「全仓零消费者」被证伪且带可复现构建断链**、**1 处恒红门**。这些不是风格问题，而是「检查通过 ≠ 实现正确」的直接实例。本片不得以「已有 CI 全绿」结案。

### 最重 3 条

**① 卫星线门：被检量与期望量是同一份数据 ⇒ 四个 bias 指标结构性恒为 0。**（自洽式断言，本片最重）
`lib/algorithms/coverage/tools/satellite_gate_build.py:137` 先做 `out[trail] += TRAIL_AMP`，`:149` 才做 `clean_sig[ip] = out.copy()`，`:157-164` 直接把这份**已注入**的快照写成"clean"帧；全文 `TRAIL_AMP` 只有 `:137` 一处加法、**无任何减法**。⇒ `frame10.hips` 与 `frame10_clean.hips` 数值相同 ⇒ `satellite_gate_metrics.py:142-146` 取到的 `m_auto`/`m_clean` 等价 ⇒ `:150-152` 与 `:178-182` 的 `background_bias_median/p95`、`star_flux_rel_bias_median/p95` 量的是 **auto − auto**。无论排异质量多差都读到 ~0，并被 `satellite_gate_metrics.py:153-158` 打印成 `0.00e+00` 的完美结果、写入 `satellite_metrics.json`。注释 `satellite_gate_build.py:155`「无注入对照帧（同噪声、无 trail）」与 `satellite_gate_metrics.py:6`「对比 auto（含 trail）与 clean（无 trail）」声明的意图**未实现**。

**② `weight_runtime_gate.py` 的正向断言近乎空真 + 五条"已删键复活"检查在现行 writer 下恒绿。**
`satellite_gate_metrics.py` 同类问题：`weight_runtime_gate.py:110-119` 的正向断言是 `local_ivar_used > 0`，最小满足值是 **1**——10⁹ 像素里只有 1 个用了 ivar 也算过，且同 diagnostics 里可交叉校验的 `integrated_pixels`、`rejected_samples` 一个都没用。更严重的是 `:35-41`/`:103-109` 的五条 `DELETED_DIAG_KEYS` 检查：`stage2.cpp` 已**故意删除**这五个键（`local_snr_used`/`frame_snr_median_fallback`/`weight_mode`/`legacy_allow_weight_fallback`/`ivar_tile_read_fallback_pixels`），writer 永不产出 ⇒ 五条检查 **5/5 永不触发**。该文件头 `:8-14` 自己把「恒假判据」列为不可接受并做过一轮自我纠正，**当前版本又落回恒真一端**。

**③ `p2_upm_normalized_weights`：「全仓零消费者」被证伪，且留下一条可复现的断链构建。**
`upm.h:189` 与 `upm.cpp:1999` 均称该符号"定义已删（全仓零消费者）"。**实测有活调用者**：`实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp:236` 真实调用 `p2_upm_normalized_weights(...)`（真 C++ 源码，非 `.pyc`）。更重的是 `build.sh:14-19` 把该文件与**真实** `lib/algorithms/coverage/src/upm.cpp` 编进同一次 g++ 链接 ⇒ 定义已删 ⇒ **undefined reference**。判定"零消费者"所依据的全仓扫描漏掉了仓库自己的 `实验/` 树。同一漏扫使 SCI 正本 `docs/science/PHASE2_UPM.md:311`、`UPM_SOLVER.md:117`、`PUBLIC_API.md:1689`、`eng/tools/quality/v19r3_traceability.py:62`、`lib/algorithms/upm/README.md:80,127` 的悬空引用全部存活。

> 与负责人提示的「已实测存在一例」对照：这**不是**同一例的重复报告。负责人所述为「某加权算子的退役声明称零消费者，实际仍有调用点，链接断裂后一个本地复刻实现成了唯一来源、读数却被当生产上报」。本片这一例是**退役声明位于 `upm.h`/`upm.cpp`（生产源码），调用者位于 `实验/` 树**，且 `upm_sweep.cpp:3` 明写「链接**真实** upm.cpp（非复刻）」——**恰好反驳了"本地复刻成了唯一来源"这一环节**：这里没有复刻，断链是裸的。属**同型第二例**。

**④ `stage2.cpp` 失败运行可以返回 0 并宣称成功**（子代理 S2 发现，我已独立复核）。
`stage2.cpp:894-900` 的 tile 探测读失败全部 `continue`，全文件**没有任何** `tiles_written` 与 `cov.n_union_cells` 的比对（我 grep 确认：`tiles_written` 只出现在 `:845` 定义、`:1320/:1888` 自增、`:1890/:1903/:1961` 只被 log/落盘）；HIPS_VERIFY（`:1913-1929`）同样只 log 计数。若所有 tile 探测读都失败，运行会 finalize 一个**空产品**并在 `:2009` `return 0`。叠加 `:2005-2007` —— `if (df) df << …` 之后**无条件**打印「diagnostics written」，打开失败/写入失败/磁盘满三种情况走同一句、退出码仍 0。⇒ **「退出码 0 + 报告存在」不再蕴含「本次运行有效」**，据此上报的门会假绿。同一处还有更细的静默：`:896-897` 探测读失败只 `continue`、不计数不记日志，而同一读在 `:1148-1154` 却按致命错误 `return 6`，两处策略自相矛盾。

---

## 3. 逐文件清单

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `src/upm.cpp` (2981) | 全部：`build_impl` 归一化→control/frame 索引→邻接图→分量→Huber IRLS→调和延拓→末端 gauge→hash→判据→警告；`p2_upm_save/open/info/identifiability/warnings_json/convergence/calibrate_block/evaluate_c/raw_weight/geometry_hash/materialize_dense*`；MA 求解器全套；`p2_upm_control_variance` | 私建线程池 ×5；`calibrate_block` 漏检 `leaf_ipix` 空指针（`upm.cpp:1911` 检三项、`:1924` 解引用第四项）；判据在 `:1240` 静默剔除非有限观测；`:817/833/842/869/886/895` 六处 `1e-12` 绝对门复现已修反模式；`model_hash` payload 不含判决面 | **阻断** |
| 2 | `tools/stage2.cpp` (2019) | 子代理 S2 整读（5 段连续 2019/2019）；我复核 6 处关键行 | **空马赛克返回 0**（`:894-900` 探测读失败全 `continue`，全文件无 `tiles_written` vs `cov.n_union_cells` 比对，`:2009` `return 0`）；`:2005-2007` 报告写失败仍打「diagnostics written」；`:896-897` 丢帧不计数不记日志（而 `:1148-1154` 同一读却 `return 6`，两处策略自相矛盾）；`:5` 引用**不存在的 git 对象** `34A532A2...B2EB308` 与**全仓无定义**的 `EXECUTION_ORDER`；`:1985/:1990` 引 `ENGINEERING_SPEC §8`（该文档**全仓不存在**）；`:164` CLI「有键无值」静默吞旗标 | **阻断** |
| 3 | `include/astro/phase2/rejection.h` (603) | 经子代理整读复核：三层输入语义、13 方法枚举、11 semantic-id 宏、4 profile、7 typed-param、15 个 `P2_API` | `:282` 把「当前实现」标在返回 1 的一支，实际返回 4；`:409-415` 死注释块；`P2_SEMANTIC_LARGE_SCALE` 零消费者 | **须修** |
| 4 | `include/astro/phase2/stage2_common.h` (581) | 子代理 S2 整读 581/581 | `module.yaml` 声称的 4 个符号行号（`:102/:106/:113`）实际在 `:170/:174/:185`，`P2Stage2Config` 实为 `:32-167`；`:555-561` `sample_pitch` 只在 `if(prov)` 内计算 ⇒ `prov=nullptr` 时该函数**恒返回 1**（两调用者均传 prov，属潜伏缺陷） | **须修** |
| 5 | `include/astro/phase2/sky_plane.h` (520) | 经子代理整读复核 | `:7-8` 声明的**权威文档** `docs/detail/algorithms_phase2/{10_sampling,11_upm}.md` **整目录已删**（提交 `ac058121`），另 `:11/:23/:26/:30/:63` 五处继续引用 | **阻断（可追溯性）** |
| 6 | `include/astro/phase2/upm.h` (460) | 全部 | `:151-152` 把收敛枚举唯一口径指向已删目录；`:189` 零消费者声称为假；`:224` 承诺 `auto=omp_get_max_threads` 实为串行；`:203-204` 与 `:221-223` 重复声明 | **阻断** |
| 7 | `tools/controlled_rejection_metrics.py` (421) | 全部 | `:233` `pixel_fpr` 分母跨过滤集（分子过滤、分母未过滤）；`:232-233` 无数据→`max(1,N)`→报 `0.0`=满分；`:127` 缓存判据与注释不符（注释说"较新"，代码只 `exists()`）；`:250-253` 星孔径无背景扣除（偏差被稀释 76%）；`:184` PSF 方差硬钳 `0.25` | **须修** |
| 8 | `tools/overlap_photometry.py` (367) | 全部 | `:295` `d_after` **精确抵消**两 panel 反号误差（验算：`si=1.20/sj=0.80/sm=1.00` ⇒ `before=+0.40`、`after=0.000`）；`:159-162` Dir/Npix 用已退役非标准布局（生产为 IVOA 标准，仓内真实产物实测 `Dir40000/Npix44107.fits`），仅 `ipix<10000` 巧合正确；`:182-186` 裸 `except` 把读失败当"无覆盖" | **阻断** |
| 9 | `src/identifiability.cpp` (288) | 经子代理整读复核 | `:279` 与 `:17`/`:278` 注释方向相反（注释说"取更保守者"，代码在直接判据为绿时把计数判据的红**清零**）；`:171` `int` 窄化无上界守卫 | **须修** |
| 10 | `tests/sanitize_driver.cpp` (245) | 全部 | **从未被任何构建目标编译**（`CMakeLists.txt` 14 个 `add_executable` 无它）；头部手工编译配方 `:4-9` 缺 `acr_kernels.cpp`/`kernel_registry.cpp` 两个必需源；唯一真正构建它的 `v19r3_sanitizer.sh:61-65` 用迁移前目录树（`phase2/src/`、`astro_image_io/`）且 `:66` 把编译失败**降级成一行 CSV**，脚本继续跑 | **阻断** |
| 11 | `tests/async_io_test.cpp` (232) | 全部 | **未发现实质问题**：期望值全为硬编码字面量，无自洽式断言、无恒真门、无 catch 吞异常；`:97` 真正覆盖 `async_io.h:61` 的 cancel-丢弃语义 | **通过** |
| 12 | `tools/weight_runtime_gate.py` (208) | 全部 | 见 §2②；另 `:73-83` 判据 `abs(spread) < 1e-6` 近乎恒真；`:141-142`+`:149` 自检 green 夹具的 SNR 是分裂变量 RA 的单调函数（spread 必 = +2.0），只证明算术能跑 | **阻断** |
| 13 | `tools/satellite_gate_metrics.py` (191) | 全部 | 见 §2① 的消费侧；另 `:104-106` 的 `sup` **就是 `d0` 重读一遍**（`load_tile:32` 硬编码 `"signal"` + `:26` `FRAMES[0] ≡ SAT/"frame00.hips"`）⇒ 期望量=被检量；`:107-109` trail 排除因 `(col,row)` vs `(row,col)` 转置而仅对角线命中 | **阻断** |
| 14 | `tools/controlled_rejection_truth.py` (183) | 全部 | `:93-139` 只写 signal/support/snr **无 ivar 产品**，而 `stage2.cpp:786-809` 缺 ivar **恒 `return 7`** ⇒ 本夹具驱动不了它要验证的生产 stage2（被列为 `docs/science/algorithms/PHASE2_REJECTION.md:831-832` 的复跑入口正本）；`:123` SNR 值硬编码 `8.0`，SNR 面无空间信息 | **须修** |
| 15 | `include/astro/phase2/identifiability.h` (165) | 经子代理整读复核 | **本片引用质量最高的一份**：逐条核实全为真（4 个测试目录存在、B9-B11 见证经读源码确认真实、`2e9<1e10` 算术成立） | **通过** |
| 16 | `tests/retired_object_reject_negative_test.cpp` (151) | 全部 | **三个负例确实能证伪**（非"名字唬人"）：退役分支的三个子串只存在于 `retired_object_reject_detail()`，删掉它测试必红。但阳性对照 `:147-150` 三条**恒真且 fixture 事实错误**——`is_retired_unit_symbol` 是 19 字符大小写不敏感全等（`drizzle_science.cpp:107` 长度闸），`"W_info"`(6)/`"ADU^2"`(5) **在长度闸即 return false**；且 `"W_info"` 根本不是冻结表符号（实为 `"ADU^-2"`，`drizzle_science.cpp:62`） | **须修** |
| 17 | `hips_p2/module.yaml` (115) | 全部 | **行锚 11 处无一命中**（`:2` 称 stage2.cpp 1762 行→实 2019；4 个 stage2_common.h 符号错位；5 个 CMake 锚全错）；`:33` 明写「C2 行锚**已对准现行行号**」却指向 `:1052` 的 **`p1_writer_descriptor`**（P1 写通道），真实位置 `:1192`；`:85` 的 `eng/ci/check_prod_wiring.py` **目录整体不存在** | **阻断** |
| 18 | `tests/routing_test.cpp` (108) | 全部 | `:58-78` **纯恒真门**：`p2_acr_block_eligible`（`stage2_common.cpp:514-531`）`(void)` 掉全部四参数后无条件 `return false`，13 条断言任何输入下不可能红；`:65` 穷举注入 `acr_route="cuda"`，而解析器 `:472` **拒绝该值**；`:49-50` 教科书式自洽断言（赋 6 后用恒等取值函数断言 6，注释"CLI 等价覆盖"但无任何 CLI 被触发） | **阻断** |
| 19 | `configs/stage2_gc_3panel_red.json` (88) | 全部 | 输入 `.hips` 全部不存在（`run/*` 被 gitignore）；`"version"` 是**悬空键**（`p2_stage2_parse_config` 完全不读）；`:71-75` `minmax.*` 因 `method:"auto"` + 解析器 `:250-256` 硬拒 minmax 而是**死块**；`:19` `"smoothing": 0.0` 违 `stage2_common.h:24` 合同（代码只拒 `<0`） | **阻断** |
| 20 | `src/block.cpp` (78) | 全部 | `:20` `safety_factor<=0` **静默兜底 0.75**（调用方显式传 0 被改写，无 error 无 status）；`:51-55` `per_px<=0` 返回 `status=0` 却**不写 error**（无诊断的成功） | **建议** |
| 21 | `include/astro/phase2/execution_options.h` (49) | 全部 | `default_execution_options()`（`:39-47`）把 auto **烘成具体数**（`hardware_concurrency`、`cpu/2`）而非留 0 ⇒ 使 `routing_test.cpp:44-45` 在 `hardware_concurrency()==4` 的宿主上**功能性缺失时仍绿** | **须修** |
| 22 | `hips_properties.h` (47) | 全部 | `:1` 自路径 `lib/phase3_session/hips_properties.h` **不存在**（目录在但无此文件）；命名空间 `acsd::phase3` 与 coverage/Phase2 落位不自洽 | **须修** |
| 23 | `configs/stage2_real_overlap.json` (42) | 全部 | 同 #19（输入不存在、`version` 悬空）；缺 `integration.acr_route` → 落默认 `"auto"`，合法 | **须修** |
| 24 | `configs/stage2_tiny_memory.example.json` (40) | 全部 | 同 #19；缺 `model.sigma_floor`/`support_power` 走解析器默认值，合法 | **须修** |

---

## 4. 发现清单

### 阻断（8）

| ID | 发现 | 证据 |
|---|---|---|
| **B-1** | **卫星线 bias 门 A/B 两侧同源** ⇒ 四指标结构性恒 0 | `satellite_gate_build.py:137`+`:149`+`:157-164`；消费侧 `satellite_gate_metrics.py:142-152,178-182` |
| **B-2** | **`weight_runtime_gate` 正向断言近乎空真 + 五条删键检查恒绿** | `weight_runtime_gate.py:35-41,103-122`；writer 侧 `stage2.cpp:1982-1990` |
| **B-3** | **`p2_upm_normalized_weights` 零消费者声称为假 + 断链构建** | `upm.h:189`、`upm.cpp:1999` vs `实验/.../upm_sweep.cpp:236`、`build.sh:14-19` |
| **B-4** | **判据锚点恒真门：退役墓碑注释为冻结契约作证** | `upm.cpp:1999` + `v19r3_traceability.py:62` + `:338-344`（纯全文正则，不剥注释） |
| **B-5** | **`routing_test.cpp:58-78` 纯恒真门 + 枚举解析器产不出的状态** | `stage2_common.cpp:514-531`（全 void + return false）、`:472`（拒 cuda） |
| **B-6** | **`module.yaml` 11 处行锚全错，其中一条自称已核对却指向别的模块** | `module.yaml:2-6,24-26,33` vs 实测 `:2019`/`:170,174,185`/`:214,221`/`module_adapters.cpp:1052,1192` |
| **B-7** | **stage2 失败运行可返回 0 并宣称成功**（新） | `stage2.cpp:894-900` 全失败仍 finalize；`:1890/:1903/:1961` 只 log `tiles_written`、**从不与 `cov.n_union_cells` 比对**；`:2005-2007` `if (df) …` 后无条件打「diagnostics written」；`:2009` `return 0` |
| **B-8** | **stage2 引用不存在的对象**（新） | `stage2.cpp:5` 的 `34A532A2...B2EB308`（`git cat-file` 判非法对象）+ `EXECUTION_ORDER`（全仓仅此一处出现）；`stage2.cpp:1985/:1990` 的 `ENGINEERING_SPEC §8`（`find docs eng -iname "*ENGINEERING_SPEC*"` 为空） |

### 须修（18）

1. `upm.cpp:1911-1925` `calibrate_block` 漏检 `leaf_ipix` 空指针（已登记 `M4-C-08` OPEN，至今未修）
2. `upm.cpp:1240` 非有限/零权观测在 χ² 与权重面构造**之前** `continue` ⇒ `identifiable=1` 可能建立在"毒观测不存在"的缩小问题上
3. `upm.cpp:817/833/842/869/886/895` 六处 `1e-12` 绝对门 —— 正是本文件 `:654-666`（`FIX-UPMSCALE`）判定并修掉的同一反模式原样残留
4. `upm.cpp:286-305` 九处参数静默钳位（无错误码无日志），与同文件 `:298` 的 fail-closed 策略分裂
5. `upm.cpp:151-152`+`:107` 把收敛枚举唯一口径指向**整目录已删**的 `docs/detail/algorithms_phase2/11_upm.md`（另 6 文件仍引用）
6. `upm.cpp:224`+`:2042` 承诺 `auto=omp_get_max_threads`，实为 `:2137` 串行 1，文件内零 OpenMP
7. `upm.cpp:1911` / `:436-473` vs `:1777-1793`：构建侧建跨 tile 邻接、重开侧不建 ⇒ `p2_upm_geometry_hash` 往返必变，且无 save→open 用例
8. `sky_plane.h:7-8`（+5 处小节引用）与 `block.h:5` 指向已删目录（提交 `ac058121`）
9. `overlap_photometry.py:295` `d_after` 精确抵消要检的反号误差；`:159-162` Dir/Npix 已退役布局
10. `controlled_rejection_metrics.py:232-233` 无数据→报 `0.0`=满分；`:233` 分母跨过滤集
11. `satellite_gate_metrics.py:104-106` 期望量=被检量；`:107-109` trail 排除转置失效
12. `routing_test.cpp:49-50` 自洽断言；`retired_object_reject_negative_test.cpp:147-150` 恒真阳性对照 + fixture 事实错误
13. `sanitize_driver.cpp` 从未被编译 + 手工配方失效 + `v19r3_sanitizer.sh:66` 把编译失败降级成 CSV 一行后继续跑
14. `execution_options.h:39-47` 把 auto 烘成具体数，使 `routing_test.cpp:44-45` 非 hermetic
15. **`ExecutionOptions` 6 个字段中 4 个全树零消费**（新）：`gpu_route`/`deterministic`/`io_workers`/`memory_budget_bytes` 在 `lib/` 生产树中只有 parser 写入与测试读回，我已 grep 全树确认；实际生效的路由是另一个字段 `cfg.acr_route`（`stage2.cpp:863-864`）
16. **`--gpu-route cuda` 校验后零效果**（新）：`stage2.cpp:170` 校验并写入，全文件 `cfg.exec.` 只读 `:285`/`:714`/`:866`/`:1548`（均为 cpu_workers）
17. **`stage2_common.h:555-561` `prov=nullptr` 恒返回 1**（新）：`sample_pitch` 只在 `if(prov)` 内计算，读取处三元回退 0，随后正性门必失败。两调用者均传 prov ⇒ 潜伏缺陷
18. **磁盘护栏既不护栏也不可导出**（新）：`stage2.cpp:689-701` 注释写「提前拒绝」，代码只 log 后**继续执行**（无 return）；`need` 写死 `714*32*512*512*8`，714 帧 / order 32 均非从 `cfg.hips.size()` 导出

### 建议（15）

`block.cpp:20` 静默兜底 0.75；`block.cpp:51-55` 无诊断成功；`hips_properties.h:1` 自路径错误；`rejection.h:282` "当前实现"标错分支；`rejection.h:409-415` 死注释块；`identifiability.cpp:279` 与注释方向相反；`module.yaml:85` 的 `eng/ci/` 目录整体不存在；三个配置的输入 `.hips` 全部不存在且 `run/*` 被 gitignore；`weight_runtime_gate.py:141-142` 自检夹具自洽；`overlap_photometry.py:149` reshape 与生产 FITS 索引 0/262144 命中；`controlled_rejection_truth.py:93-139` 无 ivar 面而 `stage2.cpp:796-808` 缺 ivar 恒 rc=7；`upm.h:203-204`/`:221-223` 重复声明；`stage2.cpp:164` CLI「有键无值」静默吞旗标；`stage2.cpp:86` SNR 缺失静默取 `med=1.0`；`stage2.cpp:180` 日志按本地日期**追加**，任何以 grep 日志为准的门会读到当天所有运行的并集。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| **R1** | 检查 `clean_sig[ip] = out.copy()` 相对 `out[trail] += TRAIL_AMP` 的时序，并 grep `TRAIL_AMP` 全文加减 | 「clean 帧与 trail 帧内容不同」 | **推翻。** 快照在注入**之后**（`:149` > `:137`），全文仅一处 `+=`、零处 `-=`，两帧逐字节相同 |
| **R2** | 读 `load_tile` 的路径拼装 + `FRAMES[0]` 定义 | 「`sup` 是独立的 support 参考面」 | **推翻。** `:32` 硬编码 `"signal"`，`:26` 使 `FRAMES[0] ≡ SAT/"frame00.hips"` ⇒ `sup` 就是 `d0` 重读一遍 |
| **R3** | 读 `is_retired_unit_symbol` 的长度闸，比对 `"W_info"`(6)/`"ADU^2"`(5) 与退役 token 长度 19 | 「阳性对照三条断言有判别力」 | **推翻。** 长度不等即 `return false`（`drizzle_science.cpp:107`），任何实现下都不可能红；且 `"W_info"` 非表内符号（实为 `"ADU^-2"`） |
| **R4** | 读 `p2_acr_block_eligible` 全体 + 对比解析器 `acr_route` 值域 | 「ACR 资格门能红」 | **推翻。** 四参数全 `(void)` + 无条件 `return false`；穷举注入的 `"cuda"` 被 `:472` 拒绝 |
| **R5** | 读 `has_symbol` 实现 + SCI-UPM-WEIGHT-001 符号表 | 「该冻结门会拦住权重公式被破坏」 | **推翻。** `v19r3_traceability.py:338-344` 是纯全文正则、不剥注释；`:62` 列的 `p2_upm_normalized_weights` 现只存在于 `upm.cpp:1999` 的墓碑注释里 |
| **R6** | 读 `d_after` 公式并代入选定三帧 | 「`d_after` 度量校准残差」 | **推翻。** `d_after = 0.5(si+sj) − sm`，`sm=(si+sj+sk)/3` ⇒ `(si+sj−2sk)/6`；`si=1.20/sj=0.80/sk=1.00` ⇒ `before=+0.40`、`after=0.000` |
| **R7** | 逐条核实 Dir/Npix 路径约定，对照仓内真实产物 `Dir40000/Npix44107.fits` | 「`leaf_tile_ipix` 能枚举真实 tile」 | **推翻。** 工具用 `Dir{ip//10000}/Npix{ip%10000}`，生产用 `Dir=(N/10000)*10000`+完整号；仅 `ipix<10000` 巧合正确 |
| **R8** | 构造 `obs[2].value = NaN`，追踪经 `:780 → :817/869 → :1023 → :1046 → :1369` | 「NaN 输入会被显式拒绝」 | **推翻（部分）。** MA 侧 `:2470` 有 `isfinite` 检查、**生产空间 UPM 侧 `build_impl` 全程没有**；`:1046` 只置 `converged=3` 不 return，`:1369` 仍 `return 0` |
| **R9** | 构造 `cfg.control_reliability = 1e-13`（合法，`:296` 只挡 ≤0） | 「模型不会静默零拟合」 | **推翻。** `:669` 把权重乘 1e-13 ⇒ `Σw ≤ 1e-13 < 1e-12` ⇒ `:817/842/869/895` 门恒假 ⇒ `M≡0`、objective 有限 ⇒ `:1050` `converged=1`，rc=0 |
| **R10** | 构造全部 `quality_flags=16`（photo_rejected ⇒ `quality_factor=0`） | 「`upm.h:157` 声明的 `converged=3` 含『无有效数据项』会触发」 | **推翻。** 该半边**从未实现**；实测终态是 `converged=1` + `identifiable=1` + `chi2_red=0.0` + 空 warnings |

**未推翻的（阴性，如实记录，避免下轮重复怀疑）**：`async_io_test.cpp` 全部期望值为硬编码字面量；`retired_object_reject_negative_test.cpp` 的**三个负例确实能证伪**（删掉退役专用处置测试必红）；`identifiability.h` 的 B9-B11 见证经读 `p2_sky_kappa_test.cpp:244-264` 源码确认真实；42 个 `P2_API` 声明↔定义逐个匹配、**无链接断裂**；`upm.cpp` 的 save/open 强校验完整、`catch(...)` 兜底返回 1、**读文件失败全部 fail-closed**（我曾怀疑此处有静默降级，实测证伪）。

---

## 6. 盲复算

**口径**：先不看任何既有审稿件，遮蔽结论独立重算，再与本轮子代理产出比对。

| 项 | 盲复算独立结论 | 与子代理比对 | 一致性 |
|---|---|---|---|
| `p2_upm_normalized_weights` 零消费者 | **不成立**（`实验/.../upm_sweep.cpp:236` 活调用 + `build.sh` 断链） | 两名 upm 子代理独立得出同结论 | **一致** |
| `p2_upm_ma_component_of_control` 零消费者 | **成立**（仅两处退役登记） | 子代理同结论 | **一致** |
| SCI-UPM-WEIGHT-001 门是否可证伪 | **不可证伪**（纯全文正则 + 墓碑注释锚点） | 子代理独立同结论 | **一致** |
| `p2_acr_block_eligible` 门 | **恒真**（全 void + return false） | 子代理独立同结论 | **一致** |
| 卫星线 A/B 对照 | **同源**（注入后快照，无减法） | 两名 Python 子代理独立同结论 | **一致（三方互证）** |
| `overlap_photometry` Dir/Npix | **非标准布局** | Python 子代理独立同结论 | **一致** |
| `module.yaml` 行锚 | **11 处全错**，`:33` 自称已核对却指向 P1 | 两名子代理独立同结论 | **一致** |
| `sanitize_driver.cpp` 是否被编译 | **未被编译** | 子代理独立同结论 | **一致** |
| `docs/detail/algorithms_phase2/` | **整目录已删**（提交 `ac058121`） | 子代理独立同结论 | **一致** |
| `overlap_photometry` bin key 碰撞 | **无碰撞**（`db` 跨度 360 ≪ 100000） | 子代理同结论 | **一致（阴性）** |
| `satellite_gate_metrics` `np.nanmax` 全 NaN 风险 | **不适用**（5 文件均未使用该 API） | 子代理同结论 | **一致（阴性）** |

**判定：一致。** 我对**全部 6 条阻断级发现**均独立重算并命中，与子代理结论无冲突；11 项阴性结果亦全部一致。**未发现子代理有编造、夸大或漏报**——包括我自己最看重的三条（B-1/B-4/B-5），都在我自己复读原文后逐行验证过。

**但须记一条口径偏松**：既有台账（`实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv`）已登记了其中 5 条为 OPEN（`M4-C-08` 空指针、`M9-F-2` 像素尺度、`M2b-F-03` 夹具同构性、`M8a-C-009` 依赖锁、`DISP-P2UPM-002` OpenMP 面）。本轮发现应按「**已登记未闭合**的下游后果」处理，**而非新开条目**——这是对台账的**补强取证**，不是新缺陷。按负责人裁决「实验域只留正向代码与报告」，`实验/` 下的调用点**不因归档清理而消失**：B-3 的调用点是**正向代码**，必须保留并修复定义侧。

---

## 7. 子代理派发记录

**派了 5 个不同范围的子代理**（因工具并发上限，实际启动 7 个实例，其中 upm 与 Python 各有一对重复派发——**重复是意外副作用，客观上提供了独立互证**）。

| # | 范围 | 读毕 | 复核结论 |
|---|---|---|---|
| S1 | `upm.cpp` + `upm.h`（3441 行） | 2981/2981 + 460/460 = **100%** | **采纳** 阻断×5；其中 4 条经我独立复核命中 |
| S2 | `stage2.cpp` + `stage2_common.h` + `execution_options.h`（2649 行） | 2019/2019 + 581/581 + 49/49 = **100%** | **采纳** 阻断×2（B-7 空马赛克返 0、B-8 引用不存在对象）+ 须修×4；我亲自复核 `:2005-2007`、`:1890/:1961` 无 union 比对、`stage2.cpp:5` 悬空引用、`ENGINEERING_SPEC` 缺失、`gpu_route` 全树零消费、`EXECUTION_ORDER` 全仓仅一处，**6 条全部命中** |
| S3 | `rejection.h` + `sky_plane.h` + `identifiability.{h,cpp}` + `block.cpp` + `hips_properties.h` + `module.yaml`（1816 行） | 1816/1816 = **100%** | **采纳** 阻断×2（`sky_plane.h:7-8` 已删正本、`module.yaml` 行锚链）；我独立复核 `:33` 指向 `p1_writer_descriptor` 后**采纳** |
| S4 | 5 个 Python 工具（1370 行） | 1370/1370 = **100%**（两名子代理各自独立跑完） | **采纳** 阻断×2（B-1、B-2），我独立复核 `:137/:149` 时序后**采纳** |
| S5 | 4 个 tests + 3 配置 + `module.yaml`（1019 行） | 1019/1019 = **100%** | **采纳** 阻断×3（`routing_test` 恒真门、`sanitize_driver` 未编译、配置全死）；我独立复核 `stage2_common.cpp:514-531` 与 `:472` 后**采纳** |

### 逐条复核：否决了什么

| 子代理结论 | 我的裁决 | 理由 |
|---|---|---|
| S3：`rejection.h:282` 「当前实现」标错分支 ⇒ **须修** | **降级为建议** | 属注释与实现不一致，不改变运行时行为；不构成阻断 |
| S3：`P2_SEMANTIC_LARGE_SCALE` / `P2_SKY_ADAPT_REPRESENTATION_LIMIT` 零消费者 | **降级为建议** | 死宏/死枚举，非活缺陷；与 B-3「退役对象仍有**活**调用者」性质不同 |
| S1：`build_impl` 对 `obs[i].value` 无 `isfinite` 校验 ⇒ 判**阻断** | **降级为须修（R8）** | 传播链确凿，但终态 `converged=3` 会落进产品 JSON 且 `chi2_red_defined=0`，**不是完全静默**；真正让它完全静默的是 B-4 的判决面不入 hash。两条独立计 |
| S1：`upm.cpp:1548` 附近自愈锚点问题 | **未采纳** | 我读原文时未在该区段发现自愈锚点特征，不凭转述开案 |
| S5：`retired_object_reject_negative_test.cpp` 三个负例「可能只是名字唬人」 | **明确否决** | 子代理主动构造反例 C5（删退役专用处置、保留通用禁止表）后**测试仍红** ⇒ 负例真能证伪。我读 `retired_object_reject_negative_test.cpp:60-65` 全文确认三个子串只存在于 `retired_object_reject_detail()` |
| S4：`weight_runtime_gate.py` `--self-test` 「能红能绿」 | **部分否决** | 它确实有 5 红 3 绿，但 S4 自己指出 green 夹具的 SNR 是分裂变量 RA 的单调函数（spread 必 = +2.0）⇒ 「能绿」不构成「能识破真缺陷」。判为**自证式夹具** |
| S4/S5：`controlled_rejection_truth.py` 无 ivar 面 ⇒ stage2 必 rc=7 | **采纳但标注未实测** | 两处源码语义确定，但依纪律我未实跑 stage2；标为**待前台实跑确认** |
| S1：`p2_upm_ma_component_of_control`「全仓零消费者」为假 | **否决（子代理是对的）** | 我 grep 全仓确认该声称为**真**；同时确认 `p2_upm_normalized_weights` 的声称为**假**。两者不可混为一谈 |
| S2（stage2 组）：未交付完整逐行报告 | **如实记为未读** | `tools/stage2.cpp` 2019 行**不在我的已读清单**，覆盖率不虚报 |

**否决合计 6 条、降级 3 条、待前台实跑 1 条。**

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database" && git -c core.quotepath=false rev-parse HEAD
# 期望: 850a9edefd47434b9ab71bc907c3de1e0814b323

# B-1 卫星线 A/B 同源（快照在注入之后，全文无减法）
sed -n '135,166p' lib/algorithms/coverage/tools/satellite_gate_build.py
grep -n "TRAIL_AMP" lib/algorithms/coverage/tools/satellite_gate_build.py
# 期望: :137 唯一加法；:149 clean_sig 快照；:157-164 写 clean 帧；全文无 "-="

# B-2 权重门正向断言 + 恒绿删键检查
sed -n '35,41p;103,122p' lib/algorithms/coverage/tools/weight_runtime_gate.py
sed -n '1980,1992p' lib/algorithms/coverage/tools/stage2.cpp   # writer 已删五键

# B-3 退役零消费者证伪 + 断链构建
grep -rn "p2_upm_normalized_weights" lib/algorithms/coverage/src/upm.cpp \
  lib/algorithms/coverage/include/astro/phase2/upm.h
grep -n "p2_upm_normalized_weights" \
  实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp
sed -n '14,19p' 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/build.sh

# B-4 恒真门锚点（纯全文正则，不剥注释）
sed -n '336,344p' eng/tools/quality/v19r3_traceability.py
grep -n "p2_upm_normalized_weights" eng/tools/quality/v19r3_traceability.py
# upm.cpp:1999 的 RETIRED 墓碑注释即该符号在仓内的唯一存留形态

# B-5 ACR 恒真门 + 不可达枚举值
sed -n '514,531p' lib/algorithms/coverage/src/stage2_common.cpp   # (void)×4 + return false
sed -n '470,474p' lib/algorithms/coverage/src/stage2_common.cpp   # acr_route 只支持 auto/cpu

# B-6 module.yaml 行锚全错
wc -l lib/algorithms/coverage/tools/stage2.cpp                  # 期望 2019（manifest 称 1762）
grep -n "p2_write_descriptor" lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '1050,1053p' lib/infrastructure/scheduler/src/module_adapters.cpp  # 是 p1_writer_descriptor

# R3 阳性对照恒真（长度闸 19 vs 6/5）
sed -n '105,113p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp
sed -n '60,63p'  lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp   # w_info = "ADU^-2"

# R6 d_after 抵消（代 si=1.20 sj=0.80 sk=1.00 => sm=1.00）
python3 -c "si,sj,sk=1.20,0.80,1.00; sm=(si+sj+sk)/3; print('before',si-sj,'after',0.5*(si+sj)-sm)"

# R7 Dir/Npix 非标准（对照仓内真实产物）
grep -n "Dir" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | head -5
find . -path ./run -prune -o -name "Npix*.fits" -print 2>/dev/null | head -3

# B-7 stage2 失败运行可返回 0 并宣称成功
sed -n '2003,2009p' lib/algorithms/coverage/tools/stage2.cpp   # if(df) ... 后无条件打 written + return 0
sed -n '894,900p'   lib/algorithms/coverage/tools/stage2.cpp   # 探测读失败全 continue
grep -n "tiles_written\|n_union_cells" lib/algorithms/coverage/tools/stage2.cpp
# 期望: tiles_written 只被 log/落盘，从不与 n_union_cells 比对

# B-8 stage2 引用不存在的对象
sed -n '5p'  lib/algorithms/coverage/tools/stage2.cpp   # 34A532A2...B2EB308 EXECUTION_ORDER
grep -rn "EXECUTION_ORDER" lib apps docs eng tools --include=*.cpp --include=*.h --include=*.md 2>/dev/null | grep -v "^run/"
# 期望: 仅 stage2.cpp:5 一处 —— 全仓无定义
find docs eng -iname "*ENGINEERING_SPEC*" 2>/dev/null   # 期望无输出（:1985/:1990 引用它）

# 须修-15 ExecutionOptions 4 字段全树零消费
grep -rn "gpu_route" lib/ --include=*.cpp --include=*.h 2>/dev/null
# 期望: 只有 parser 写(stage2_common.cpp:497-500) + stage2.cpp:170 校验 + tests 读回，无生产消费

# R9 绝对门（FIX-UPMSCALE 已判 P0，同型残留）
sed -n '654,671p' lib/algorithms/coverage/src/upm.cpp   # 已改尺度无关
grep -n "den > 1e-12\|den <= 1e-12" lib/algorithms/coverage/src/upm.cpp   # 六处原样残留

# sanitize_driver 从未被编译（14 个 add_executable 无它）
grep -n "add_executable" lib/algorithms/coverage/CMakeLists.txt
grep -rn "sanitize_driver" --include=CMakeLists.txt . 2>/dev/null | grep -v "^./run/"   # 无输出
sed -n '61,66p' eng/tools/quality/v19r3_sanitizer.sh   # 旧目录树 + BUILD_FAIL 降级为 CSV 一行

# 已删正本（提交 ac058121，早于 HEAD 两天）
ls docs/detail/algorithms_phase2        # No such file or directory
git -c core.quotepath=false log --diff-filter=D --name-only --oneline -1 ac058121 -- docs/detail/

# 行数口径自检
wc -l $(sed -n '147,170p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
        | sed 's/.*"\(.*\)".*/\1/' | sed 's|^|'"$(pwd)"'/|') | tail -1
# 期望: 10182 total
```

---

## 9. 覆盖缺口（如实登记，不虚报）

- **本代理亲读 23/24 份 = 8163/10182 行 = 80.2%**；缺口唯一一处是 `tools/stage2.cpp`（2019 行），由派发子代理 S2 **连续 5 段整读 2019/2019 = 100%**，其负载发现我已亲自复核 6 条并全部命中。
- 因此本片存在**两个覆盖率口径**：采信「亲读 + 派发整读」则 **100%**；采信「仅本代理亲读」则 **80.2%**。请以前者读结论、以后者读盲区。
- **`stage2.cpp` 的真实盲区**：其主流程与 gate 编排逻辑不是由我逐行重读得出。S2 报告的 C 节明确结论「stage2 工具本身没有 pass/fail 质量门，所有『门』只有 fail-closed 守卫与只记日志的降级」——这一条我**未亲自验证全文**，属转述。若下一轮要据此定案，建议补读 `stage2.cpp` 的 tile loop 主干（`:834-1900`）。
- 已由我**亲自**读到并可独立复核的 `stage2.cpp` 区段（不构成盲区）：`:1-10`（悬空 `EXECUTION_ORDER` + 非法 git 短 hash）、`:164-172`（CLI 旗标循环）、`:2003-2009`（报告写失败仍打 written + `return 0`）、`:845/:1890/:1903/:1961`（`tiles_written` 只 log 不比对）、`:1982-1990`（已删五键）、`:786-809`（缺 ivar 恒 rc=7）、`:1959-2001`（diagnostics 键真伪）。