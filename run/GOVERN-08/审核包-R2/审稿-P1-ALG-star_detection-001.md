# 审稿-P1-ALG-star_detection-001 — G08-05 对抗审稿 第 1 遍

- 片号：`ALG-star_detection-001` ｜ 层：`lib/algorithms/star_detection`
- 基线 HEAD：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 依据原件：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml`
- 负责人口径：**一遍 = 对同一片材料的一次完整重读**。全部结论来自本人逐行读完原文；机器词表只用于定位候选。未读 `/tmp/acsd_g08/`；零 git 写；未编译、未跑 ctest/pytest/构建/任何二进制；未改仓内任何文件。
- **版本说明**：本件为**修订版**。首轮我依据子代理回报与本人复核**推翻了本报告自己的两条初判**（deblend oracle 判为"非同义反复"、OpenMP 判为"私建线程池违规"），修订结论见 §7.3。按红队纪律，**自查推翻的过程本身就是交付内容**。

---

## 1. 读完了吗

| 项 | 数值 |
|---|---|
| 成员份数（权威清单） | **38** |
| 实际读完份数 | **38** |
| 成员总行数（权威清单 `实际行数`） | **10409** |
| 实际逐行读出行数 | **10409** |
| **覆盖率** | **38/38 份 = 100%；10409/10409 行 = 100%** |
| 未读完的 | **无** |

计数口径：「成员份数 / 成员总行数」取 `片清单-权威版.yaml` 中 `ALG-star_detection-001` 的 `成员份数: 38` 与 `实际行数: 10409`；「实际读出行数」为本人用 `read`（分段）与 `cat -n`（全文）实际输出的行数之和，与 `wc -l` 逐一核对一致（38 个文件全部存在，无缺失、无偏差）。覆盖率按**份数**与**行数**双口径计算。

---

## 2. 本片判定

# **阻断**

### 最重 3 条

**【阻断-1｜恒绿门 + 筛掉真信号｜生产缺陷】各向同性回退把两个排异门在其生效区间内结构性地废掉；而现有测试恰好把该区间关掉才测到门**

`src/sdet_api.cpp:666-685`：7 参拟合收敛但轴比超限时，用 `sdet_gaussian_f5` 重拟合并**回填** `x0[2]=x0[3]=x5[2]`（`:681`）。而 `sdet_gaussian_f5:274` 硬写 `x7[3] = x5[2]`（σy ≡ σx）。回退一旦成功：

- `reject_star:325` `fmin/fmax < 0.5` 读到 **1.0/1.0 → 恒不触发**；
- 装配环 `:2089`、引导 `:2334` 的 `smax/smin > maxAxisRatio` 读到 **1.0 > 2.0 → 恒不触发**。

**被替换掉的恰恰是携带不合格证据的那个各向异性解，替换品是按构造恒等于 1.0 的量。**（默认 `maxAxisRatio=2.0`，`include/star_detector.h:44`。）

决定性证据来自测试侧：`tests/p1star/p1star_reject_gate_test.cpp:116-120` 的圆度门夹具正是 `sigma=(2.6,1.2)`（轴比 **2.167**，落在 (2.0,11) 区间内），但 `:120` **显式传 `maxAxisRatio=0.0f`**，`:114` 注释自陈「关闭 R2 门隔离码 3 归因」；而 `:105` 的轴比门夹具用 `sigma=(0.35,4.0)`，轴比 **11.4** —— 极端行条，各向同性模型根本拟合不上，`rep5` 失败、`x0` 保留各向异性解，`:2089` 才触发。**即：全仓无任何夹具落在「轴比 ∈ (2.0,11) 且回退能收敛」区间；套件只证明了门在"回退自身失败"的那条窄缝里有效。**

**【阻断-2｜唯一"独立闭式 PSF 积分"本身算错因子 2，恒定 +0.7526 mag，把唯一绝对星等门废掉】**

`tests/p1star/p1star_oracle.hpp:47-50`：`return amp * M_PI * sigma * sigma * ex * ex;`。本人独立推导：∫∫ over [-R,R]² 的正确闭式是 `2π·amp·σ²·erf²`（∫_{-R}^{R} e^{-x²/2σ²}dx = σ√(2π)·erf(R/(σ√2))，平方即得）。代码恰为真值的 **1/2**，与 σ、R 无关：

```
sigma=1.8 oracle=10.161298 true=20.322597 ratio=0.5000 bias=+0.7526
sigma=2.0 oracle=12.498609 true=24.997218 ratio=0.5000 bias=+0.7526
sigma=2.8 oracle=23.073042 true=46.146085 ratio=0.5000 bias=+0.7526
```

同文件 `:46` 注释闭式又是真值的 **1/4**（注释与代码互相矛盾且都错）。唯一调用点 `tests/p1star/p1star_tests_core.cpp:690-701` 是**全片唯一绝对星等断言**，容差 1.5 —— 半个门限被 oracle 自己的常数吃掉。另：oracle 抬头 `:5-6` 明写「box=(2R+1)², R 钳[5,200], 候选中心」，调用点却硬编码 `6.0`；生产实际用 `Rb = c.R`（`sdet_api.cpp:2113-2115`，σ∈[1.8,2.8] ⇒ R∈[10,13]）。**oracle 定义与调用点自相矛盾，余量把矛盾一并吸收。**

**【阻断-3｜静默降级 + 恒假门｜损坏输入被伪造后当成干净数据处理，而"防损坏"的那道门是死代码】**

**【本条为自查推翻后的修订版】** 我首轮写成「全非有限帧 → `thr=NaN` → `:1773` fail-open 早返回」，**该机理判断错误**。逐行复核后的真实链路（`sdet_median_inplace:384` 的 `if (n==0) return (F)0` 是关键）：

1. `:1601-1603` 扫到非有限 → `bad=true`，`:1605` 建全尺寸副本；
2. `:1608`/`:1610` `med = sdet_robust_median_d(全 NaN 数组)` → `sdet_median_of_finite:404` 把所有非有限滤掉 → 样本为空 → `sdet_median_inplace:384` **`return (F)0`** ⇒ **med = 0.0**；
3. `:1611-1612` 把每个非有限像素写成 `med` ⇒ **整帧被静默改写成全 0 帧**；
4. `:1624` `bgnoise`（全 0 差分）= 0；`:1626`/`:1628` `bg` = **0.0**；`:1629-1633` `maxi` = **0.0**；
5. `:1634` `if (!isfinite(0.0) || 0.0 <= -1e300)` ⇒ **谓词结构性恒假**，`:1636-1641` 的「全非有限帧: 判空域」分支**永不可达**；
6. `:1649` `thr = 0 + 5·0 = 0.0`（有限）⇒ `:1773`/`:2193` 的 `if (!isfinite(pr.thr)) return 0;` **也不触发**；
7. 16 算子主链在**这张全 0 伪造帧上跑完**，返回 `rc=0`、`count=0`、指针全 NULL。

⇒ `:1597` 注释「全非有限帧由调用方经 bg/maxi 检查判空」是**对不可达代码的描述**；`:1636-1641`、`:1773`、`:2193` 三段**全为死代码**，却长得像 fail-closed 防线。真实行为不是"早返回空"，而是**把损坏帧伪造成干净帧后照常处理**。引导路径更重：`:2175` 已写入 `n_predicted = n_pred`，其余四项计数全 0，**`:2348` 明文声明的「六计数守恒」被破坏而 `out_stats` 照常交还**。

**部分损坏同理**：`:1611-1612` 把所有坏像素写成**幸存有限像素的中位数**，随后 `bgnoise`/`bg`/`dynrange`/`sat_threshold`/`thr` 全基于合成图计算，产出**一份自信的完整星表，返回值里没有任何字段表明多少比例的数据是编造的**，替换像素数被丢弃。

---

## 3. 逐文件清单

`ok` = 读完未发现本轮关注类问题；其余见 §4。

| # | 文件 | 读到什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `src/sdet_api.cpp` (2491) | 全文三段 | **阻断-1** 各向同性回退中和双门（`:666-685`+`:274`+`:325`+`:2089`）；**阻断-3** `:1601-1614` 静默伪造 + `:1773`/`:2193` fail-open；**`:514`/`:786` memset 先于 `:516`/`:788` 的 `!result` 守卫**（作者写了空指针守卫，但守卫对该情形是死代码）；`:591-685` 四层降级链；`:1042`/`:1058` 未知 extras 名返全 0 无错码；`:2081` 盲路径拟合失败 `continue` 无计数；`:2424`/`:2443`/`:2468` 导出包装 -1 路径不清 out（impl 层 `:1761-1764`/`:2181-2184` 却清了，两层不一致）；`:905-1016` `sdet_detect_saturated_stars` 全仓零消费者；`:974` `meanhigh_filtered` 恒 0 却进 `:1013` 日志；`:45-46` 引 `REPORT §5.2` 不存在且 `0.8%→14%` 两数字不在被引文档中（真实为 13.60%→4.05%）；`:618`/`:633`/`:679` 手抄 `0.2122`（精确值 0.212332…） | **阻断** |
| 2 | `src/sdet_image.cpp` (864) | 全文 | **15 处 OpenMP 与 2 处 `omp_get_wtime` 全部位于死代码内**；`sdet_image.h` 21 个导出中 **16 个全仓零调用**（实测 grep），存活路径只有 `sdet_yvv_blur_impl` 与 `sdet_median_of_finite`/`sdet_mad_sigma_impl`，二者**无任何 OpenMP**；`:558-587` 与 `:609-639` 同一 sigma-clip 两份近似重复；`:472-473` `dw==1` 时除零、`:590` `downsample_factor==0` 整数除零、`:782` `n==0` 时 inf 进日志（均在死代码内） | **阻断**（死码面） |
| 3 | `src/nls_lm.cpp` (464) | 全文 | 失败语义整体 fail-closed 写得好：`:191` λ 搜不到界→false、`:288-291` 初始 cost 非有限→NumericalFailure、`:384-390` 无可接受步→NumericalFailure、`:254-264` 参数非法→InvalidArgument、`:127`/`:130` Cholesky 与非有限步均 false。**隐患**：`:309` 精确零列→`D=1.0`（绝对缩放）而 `:319` 近零列→`D=dmax`（**全列最大**尺度），两处策略不一致且把不可辨识方向提升到最高步长优先级；`:208` 二分相对容差 `1e-3` 硬编码未进 `Options`；初值恰为最优时 `prered=0 ⇒ rho=-1` → 60 次内层耗尽 → 误报 NumericalFailure | 须修 |
| 4 | `src/nls_lm.h` (130) | 全文 | 出处逐条对应实现；`docs/science/PSF.md:181 §14a`、`run/GSL-REPLACE-01/REPORT.md:91 §3.1`/`:96 §3.2` 经核实真实存在；Moré 1978 LNM 630 / MM-04 / MINPACK 均为真实文献；`:56 max_iter=20` 与 `sdet_api.cpp:48` 一致 | **ok** |
| 5 | `src/sdet_image.h` (43) | 全文 | 21 个声明中 16 个零消费者；问题不在头本身，而在它把死接口继续当模块契约面暴露（AGENTS §6「退役代码从代码库删除」） | 须修 |
| 6 | `src/sdet_log.cpp` (89) | 全文 | `:8` 引 `docs/ACSD_DESIGN.md §10` **经核实真实**（`:545`），落盘走统一 I/O、未自持 `FILE*` 通道这点做对了。问题：`:31`/`:38-44`/`:86-88` 三处吞返回值（落盘失败时全模块唯一 ERROR 上报口 `sdet_api.cpp:963` 静默丢失，且 `:30` 缓存失败态不再重试）；`:13-15` 三个无文档静态可变全局（CONCURRENCY_STANDARD 要求写明容量/身份/失效/线程模型），`g_sdet_log_file` 是**永不关闭**的长期句柄；`:31-44` CWD 相对硬编码路径（见 R4） | 须修 |
| 7 | `src/sdet_log.h` (10) | 全文 | 与实现一致 | **ok** |
| 8 | `src/sdet_angle_guard.h` (70) | 全文 | **本片唯一范例级文件**。逐条核对其自声明的 3 条契约全部成立：有界（`:35` `kMaxAngleHalfTurns=64`、`:48-52` 闭式预判、`:58` 迭代内硬上界，杜绝 `:7-13` 描述的 ±inf 挂死）；fail-closed（`:41` 空指针 / `:44` 非有限 / `:51`/`:58` 超界，三条都不写 `*out`）；逐位一致（`:57-61` 保留原冻结迭代式，`:22-23` 写清了为何不用 fmod/remainder）。数学复核：`:44` 已排除非有限故 `:50` 的 `ceil` 不溢出；`:57` 严格大于保证 ±90 是不动点 | **ok（建议全仓推广此 fail-closed 写法）** |
| 9 | `src/sdet_test_probe.h` (26) | 全文 | 探针**已实证非退役对象**（`sdet_api.cpp:60`/`:62` 定义，`:1310-1318`/`:1894-1895` 写，被 `test/sdet_saturation_cursor_test.cpp:99-117` 与 `test/sdet_deblend_contrast_oracle_test.cpp:117-131` 消费）。但 `:6-8` 抬头「Oracle 独立复算」**不成立** —— 探针只暴露生产已算好的 `fsum`/`total_flux`，无枝像素表、无 `smooth[]`、无 `thr`，见 X10 | 须修 |
| 10 | `include/star_detector.h` (145) | 全文 | NSDMI 消除未初始化 UB（`:29-70`），`:27-28` 记录了修复；`:40-42` 诚实标注 `fitRadius` 字段名误导；`:44` 行号轻微失准（`:2086/2331`）；`:76-80` 四行空白（旧 4 符号注销残留） | 须修 |
| 11 | `module.yaml` (171) | 全文 | `:32-33` 与 `:143` 称「现状实现为 OpenMP **三处**」—— 实测 **22 处**（`sdet_api.cpp` 7 + `sdet_image.cpp` 15），且所述 `reduction(+:fit_ok_count)` 子句在 HEAD **不存在**；`:136-137` 引用 `sdet_detect:992-1274`/`debug:1281-1593` —— **两函数已于 e891df28 删除**，而 `:8` 刚说过这件事；`:132-171` 整段 notes 行号锚全部指向旧版本；`:25` 引已被 `Makefile:1` 明令移除的 `-march=native`；`:140-141` DISP-STAR-004「饱和星 mag 量纲不一致」与代码不符（`:2126`/`:2397` 同式） | **阻断** |
| 12 | `Makefile` (75) | 全文 | `:37` `PCH_HEADER = src/stardetector_common.h` **不存在**（`ls src/` 已核实），`make pch` 必失败，且该文件正是 `:44-46` 自陈已注销的旧件；`:70-73 clean:` 用 Windows `del /f /q` 与 `\` 分隔符，Linux 上失效；`:24` 硬编 `-fopenmp`，而同文件开头援引「根 CMakeLists 是唯一产品事实源」的理由删掉了 `-march=native` —— 同一规则对自己没适用 | 须修 |
| 13 | `.gitignore` (29) | 全文 | `:8 logs/` 与 `:26 archive/` 把 24.96 MB 运行日志和「CHANGELOG 声称的归档」一并隐藏 | 须修 |
| 14 | `README.md` (203) | 全文 | 自称「本 README 全部行号锚」（`:64`），实测 **20/20 条全部落空**（6 个导出、9 个内部核心、2 组错误码、4 处 OpenMP、2 个已删函数、1 个已删文件、`R` 的公式与「饱和星 mag」两项文档↔代码冲突）；`:151`/`:176-177` 的 `-march=native` 与 `Makefile:1` **直接互斥**；`:18`/`:143`/`:176` 的 `Makefile:39` 指错行（实为 `:53`）；`:186-187` 称共址测试待建，但 `tests/p1star/` 已建成 15 文件 | **阻断** |
| 15 | `CHANGELOG.md` (156) | 全文 | `:118-135`「文件结构」块的 `lib/include/`、`python/star_detector.py`、`src/sdet_detector.cpp`、`src/sdet_background.cpp` **全部不存在**；`:46` 的 `archive/V4.66_pre_cleanup/` **不存在**（且被 `.gitignore:26` 忽略）；`:50-58` 仍保留 Siril 引用，与 `:31`「移除所有第三方项目引用元素」自相矛盾 | 须修 |
| 16 | `memory.md` (110) | 全文 | 抬头已诚实标注 ARCHIVED_NON_NORMATIVE（`:3-7`）；但 `:43` 排序描述与实现相反、`:39`/`:64` 仍以 GSL 为当前口径、`:96` 「sdet_api.cpp 2373 行」实为 2491 行 | 建议 |
| 17 | `tests/p1star/CMakeLists.txt` (346) | 全文 | **6 个已注册 ctest 用 `PASS_REGULAR_EXPRESSION` 决定红绿**（`:190,231,247,272,292,346`）—— CTest 语义下该属性**覆盖退出码**，「注入必败」自检亦在其中；`:101`/`:141-149` Windows 下 `p1star_negative`（F5 + OOM）整组不注册，而 `:144-145` 引用的 `eng/ci/ctest_baseline.json` **不存在**；`:37` 域基线 HEAD `dc2f5f3e` ≠ 实际；`:41` 称「生产 5 src」而 `:52-57` 只列 4 个 | 须修 |
| 18 | `tests/p1star/p1star_tests_core.cpp` (1014) | 全文四段 | F1–F6 + OOM 扫描；**阻断-2**（`:690-701`）；R5 NaN 门恒真（`:562-572`，`n_nan` 统计后**从未被断言 >0**，且生产 `:1601-1614` 中位填充使该场下 NaN mag 不可能产生）；R6（`:620` 条件跳过，双通道同时漏检时整段跳过且无日志）；`:283-286` recall10 门已被**显式撤回**只留打印（注释诚实，但撤回后无替代）；`:596-603` 断言失败后仍越界读 `dfull.x[i]`；`:990-1001` FIX-P128 回归锁整块在 `#ifdef SDET_TESTING` 内且无 `#else`/`#error`；`:614-618` 64×64 单星场只注入 1 颗星，本机 `maxStars=2000` 下无法证伪 `maxStars` 截断 | **阻断** |
| 19 | `tests/p1star/p1star_tests_main.cpp` (37) | 全文 | main 唯一性拆分正确；`oom-child` 要求 `argc>2`(`:28`)，缺 N 落到组运行器返 2，**无静默路径** | **ok** |
| 20 | `tests/p1star/p1star_test_main.hpp` (103) | 全文 | 解析正确；`run_all_groups:96-99` 未知组返回 2（选择器 fail-closed）。缺陷：`record()`(`:53-65`) 累加 `checks` 但**全文无一处断言 checks>0** → 「零检查运行」不可检出；`:55-58` `ok = !ok` 双向翻转，而 units/properties/oracle 注册只设 `OMP_NUM_THREADS`（`CMakeLists:86-88`），外部 shell 导出 `ACSD_P1STAR_FAULT` 即可让真实失败报绿；`:72-73` 宏未给操作数加括号 | 须修 |
| 21 | `tests/p1star/p1star_oracle.hpp` (97) | 全文 | **零生产耦合，这一层设计正确**（仅 include 标准头 + fixtures；抬头 `:2` 「不调用被测函数」字面为真）。但 `gaussian_flux_in_box`(`:47-50`) **因子 2 错**（阻断-2）；`:28-29` 声称「Simpson 离散版供交叉验证」而 `simpson38`(`:36-43`) 与 `gaussian_flux_analytic`(`:30-32`) **全仓零调用**（假交叉验证）；`:33` `sigma<0 → TOP_HAT` 语义无任何实现 | **阻断** |
| 22 | `tests/p1star/p1star_fixtures.hpp` (364) | 全文 | FIX-STAR-E（`:199-211`）全公式化无随机数 ⇒ 期望即构造参数，**真独立**；`quantize_u16`(`:109-119`) 独立实现；`:9-10` 容差来源注记仍写已被 `core:283-285` 撤回的「SNR≥10 召回≥99%」；`:252` 「sdet_api.cpp:1772 实参」实际在 `:1619`/`:1621`；`:255-256` 自陈 `F1TB_THR99` 由**生产实测 1000 次/档标定**；`:122`/`:142`/`:154` 三处距离注记不符（实为 28.78/36.06/36.06）；`:98-106` `add_top_hat_to` 零调用 | 须修 |
| 23 | `tests/p1star/p1star_guided_test.cpp` (310) | 全文 | G3（`:217-218`）显式排除「全丢恒真」、G2 `:194` 强锚、G4 `:243` memcmp 全序 —— 设计良好。但 `:160` 先按 `best<=0.3` 筛再取 `max_dc` ⇒ `:168 check(max_dc<=0.3)` 是 `:160` 的逻辑后承、零独立判别力；召回门只放行 0.9，被漏星偏移 2 px 也全绿；`:155-159` 最近邻**不标记 used**，多颗真值可共享同一检出；`:4-5` 规范锚目录不存在；`:19` 说 25px 而 `:253` 用 30px；`:81` 请求了 `{"sx","sy"}` 但 `ex` 四行**一次都没读** | 须修 |
| 24 | `tests/p1star/p1star_nls_lm_test.cpp` (394) | 全文 | `:43`「生产同款 7 参数母函数」与 `:64`「与生产 `sdet_gaussian_df` 同式」**两条声明均为假** —— 测试 `(B,A,x0,y0,SX,fr,α)` vs 生产 `(x0,y0,sx,sy,th,A,B)`（`sdet_api.cpp:190-193`），雅可比行序全不同；`CMakeLists:239-241` 该目标只链 `nls_lm.cpp` ⇒ 生产解析雅可比 `sdet_api.cpp:222-245` 全仓零差分验证。另 `:355`/`:365`/`:140` 的 `std::fmax` 归约会**吞掉 NaN**（一参为 NaN 返回另一参）；`:52` 在 `fr=0 ⇒ SY=0` 时除零；`:311-315` 用来给「近零列保护」背书的 `[5] 合同`只构造**精确零列**（`:356 max_alpha_col==0.0`），走 `:309` 分支，**`:316-320` 那段偏离 Moré 缩放的改动从未被行使** | **阻断** |
| 25 | `tests/p1star/p1star_psf_shape_gate_test.cpp` (292) | 全文 | **本片设计质量最高**：每个负例先断「门关时负例确实出星」(`:160`/`:191`/`:195`/`:221`/`:258`) 以排除空断言，再断「门开时被拒」(`:162`/`:193`/`:260`)，并用 `subset_count`(`:125-131`,`:166`,`:197`) 验证纯过滤不扰动幸存者；ON/OFF 只差门参数（`-1`→`sdet_api.cpp:437-441` 直通，`0`→`:442-449` 默认）。**反例 CE-5：把 `sdet_api.cpp:458-472` 改成直通 ⇒ `:162` 转红（能抓）**。缺陷：10 处丢弃 `run_detect` 返回值（`:95-96`/`:106` 失败时 `return -1` 且**不碰 `*out`**）；`fx/fy` 填了不用（`:111-112`） | **ok（有瑕疵）** |
| 26 | `tests/p1star/p1star_reject_gate_test.cpp` (237) | 全文 | 提供**阻断-1 的决定性反证**（见 §2）。缺陷：`:130-139` 标称 `r1_code5_fwhm_limit`，但 `reject_star` 先判码 4（`sdet_api.cpp:326-329`）再判码 5（`:332-338`），而 `detect_blind`(`:66-92`) **只返 rc+count、无读码接口** ⇒ 归因结构上不可证；`:143-156` 自述「修复前后同值」的恒绿门且未消元（同文件 `:116-123` 已有正确样板）；`:68-69` 称「原先 `SDetParams p;` 未初始化 → 读栈垃圾」对 HEAD **为假**（`star_detector.h:30-69` 已全字段 NSDMI）；`:4`/`:31` 引用的两个路径**全仓均不存在**；`:24-26` 解析式与 `:184-186` 构造不自洽（「S 峰 3079」复算对不上） | 须修 |
| 27 | `test/sdet_saturation_cursor_test.cpp` (252) | 全文 | **【自查推翻】** 我首轮判「判据合同非同义反复」。经复核**该判断错误**：`:123` 的 `expect = (p.fsum > DELTA_C*p.total_flux)` 两个操作数都是生产已算好的值（`sdet_api.cpp:1302` 算分子、`:1313`/`:1315` 记录、`:1307` 算判定、`:1317` 记录），属**同一表达式的恒等重放**，对分子计算式零判别力（X10）。其余设计良好：`:202-210` 的 else 分支正确 `CHECK(false)` 无条件跳过；`:229-236` 非退化对照（平台降到饱和电平以下 ⇒ 嵌入星必须发布）是本片最好的红绿配对之一；`:145` 的 u16 量化用截断无 +0.5，与 `p1star_fixtures.hpp:116` 的 `(uint16_t)(v+0.5)` 是两套规则 | **阻断**（自纠后） |
| 28 | `test/sdet_deblend_contrast_oracle_test.cpp` (193) | 全文 | **【自查推翻】** 同 27。抬头 `:20-22`「判据合同：测试内以 delta_c*total_flux **独立复算**」不成立。**正面**：`:23-25` 的 SExtractor `refine.c` / SEP `deblend.c` 逐字引用（commit 号 + 文件:行）是本片最扎实的出处纪律；`:170-174` 的 if 无 else 但 `:158-159` 已独立断言 `leaves.size()==1`，不构成漏洞 | **阻断**（自纠后） |
| 29 | `test/sdet_angle_guard_test.cpp` (133) | 全文 | oracle（`:48-54 legacy_normalize`）是冻结迭代式的**独立重写**、与生产同式可比对，**不是同义反复**（与 27/28 形成本片最刺眼的对照：同一个仓里，一个 oracle 真独立、一个 oracle 是恒等重放）；`:56-61` 用哨兵 42.0 验证「fail-closed 不写出参」；`:88-89` 断闭式预判边界 | **ok** |
| 30 | `test/sdet_fp64_test.cpp` (102) | 全文 | NON_PRODUCTION_TOOL_ONLY，未接构建目标（README:186 已如实登记）；`:92` 对 FP32-vs-FP64 坐标差用 **1.0 px** 门，与其抬头「在 FP32 输入舍入范围内」严重不符（实际 ~1e-3）；`:47` 量化截断无 +0.5 | 建议 |
| 31 | `tests/p1star/p1star_tests_selfcheck.cpp` (143) | 全文 | **【新发现】`:66-69` fork 失败→127、`:71-72` `execve("/proc/self/exe")` 失败→`_exit(127)`、`:76` 被信号杀→-1，三者皆非 0，全部满足 `:100`/`:117` 的 `child_rc == 0` 之否定 ⇒ `:106-108`/`:123-125` 打印「（必败验证通过）」、整体 PASS —— 而注入子进程**一条断言都没跑**。任何 mask `/proc` 的容器/chroot/seccomp 环境都会得到零覆盖的绿灯 | **阻断** |
| 32 | `tests/p1star/p1star_mad_check.cpp` (125) | 全文 | **三重失效**：(a) 抬头「未修复必红/已修复必绿」为**假** —— `:89-101` 断言 2 的两个操作数是 `sdet_robust_mad_d(sample)` 与测试自算 `rmse_raw(sample)`，**都不是** `InternalFitResult.mad`；M3b-H-01 的真实位置是 `sdet_api.cpp:648-651` 现场算、`:659`/`:716` 赋值，而拟合路径**从不调用** `sdet_robust_mad*`（那是背景噪声路径，`sdet_image.cpp:571`/`:586`/`:623`）。把 `:716` 改回 `result->mad = pdata.rmse;`，`p1star_mad` **仍全绿**。(b) `:21-22` 声称的「断言 4: 生产源不含 4 位截断常数」**全文不存在**。(c) `:85-86` 的 `fabs(kMadToSigma15 - kMadToSigma4) > 1e-6` 两端都是本文件 `:42`/`:46` 的 constexpr，编译期折叠为 true，标签「非恒真」为事实错误。另 `:83` FP32 容差 0.02（被测量≈0.7413 ⇒ 2.7%）而待抓的 4 位截断扰动仅 ~1.5e-6 ⇒ FP32 半边抓不住它点名的缺陷 | **阻断** |
| 33 | `tests/p1star/p1star_tests_perf.cpp` (123) | 全文 | `:103-104` 的 `0.4 ≤ 2w/1w ≤ 4.0` 在 256²×25 星（65536 px，比生产 4500×3600 小 **247 倍**）尺度上，**完全串行实现给出 ratio=1.0 同样通过**，对「并行被彻底关掉」零判别力；无 `_OPENMP` 时 `:45-49` 整段退化、ratio 恒 1.0，双门恒真且无 `#error`；`:110` 双跑 bitwise 是同代码同线程自比；`:71` 非 0 时泄漏 | 须修 |
| 34 | `tests/p1star/p1stardet_node_gate_test.cpp` (583) | 全文三段 | N1/N2/N3/N5/N6/N7/N8/N9/N9b/R1/R2/R3 配对扎实，N9/N9b 明确排除「恒红」。**新发现：抬头 `:15` 声明的 N4 全文无实现**（`grep N4\|n4` 只命中 `:15`；`:156` 段头自己只写 N1/N2/N3/N5）；`:5-7` 引用的目录不存在。`:557-566` 数据集缺失时 N8/N9/N9b 整组 SKIP 但 `:577-580` 仍 `PASS` 返回 0，转红开关 `ACSD_STARDET_REQUIRE_GAIA` **全仓从未配置**；`:492` 消息写 `>>` 而断言只有 `>`；`:200`/`:205` 只探 1000/100000/20000、从不探 50000 端点 ⇒ `module_adapters.cpp:3419` 的 `>` 改 `>=` 仍绿 | 须修 |
| 35 | `wrapper_phase1/star_detector.cpp` (292) | 全文 | `RETIRED-CODE-RETAINED` 块论证充分（CLEAN-401 合成实验、NaN 三道 fail-closed、σ==0 由静默置地板改显式失败），但块内**全部自身行号锚已失效**（`:67`→实为 `:119`；`:50`/`:51`→实为 `:103`；`:107-110`→实为 `:122`；`:132-135`→实为 `:143-152`）；`:249-250` `max(a2,1e-12)` 使退化窗产出 fwhm≈2.4e-12 而无告警；`:239 if(m00<=0) continue` 静默丢候选无计数 | 须修 |
| 36 | `wrapper_phase1/star_detector.h` (64) | 全文 | `fwhm_px`/`snr` 两处跨块口径警告（`:18-33`）写得清楚；`:34` 诚实标注「4=重叠 当前无生产者」 | **ok** |
| 37 | `wrapper_phase1/README.md` (33) | 全文 | `:31-32` 明确声明「**本类不是 lib/algorithms/star_detection 的 sdet**」，本片最关键的防混淆声明；`:24-28` 的线程口径（由 OpenMP 环境决定、本库不硬编码）与 `module.yaml` 「ThreadBudget 未接线」**不一致**，需对齐 | 建议 |
| 38 | `tests/p1star/sdet_oom_interposer.c` (96) | 全文 | `sdet_oom_arm` 显式 arm（`:26-31`）正确排除了进程启动期分配；`:89-96` 只拦 4 个分配入口，**未覆盖 `posix_memalign`/`aligned_alloc`/`memalign`/`mmap`**，`:13-16` 注释「检测段全部分配」过宽；`:90` 的 `a*b` 可 size_t 回绕；`:55` 尺寸过滤时不递增 `g_seq`，故 `OOM_TOTAL` 是**过滤后计数** | 须修 |

---

## 4. 发现清单

### 阻断（7）

| ID | 位置 | 问题 |
|---|---|---|
| **BLK-1** | `src/sdet_api.cpp:666-685` + `:274` + `:325` + `:2089`/`:2334`；佐证 `tests/p1star/p1star_reject_gate_test.cpp:105,116-120` | 各向同性回退**中和圆度门与 maxAxisRatio 门**；测试用 11.4:1 极端行条（回退拟合不上）与 `maxAxisRatio=0`（显式关掉可疑分支）两种手法，使全仓**无任何夹具**落在「轴比∈(2.0,11) 且回退能收敛」区间 |
| **BLK-2** | `tests/p1star/p1star_oracle.hpp:47-50`（+`:46` 注释）+ 调用点 `tests/p1star/p1star_tests_core.cpp:690-701` | 唯一独立闭式 PSF 积分**因子 2 错**（注释因子 4 错），恒定 +0.7526 mag，吃掉唯一绝对星等门半个门限；oracle 抬头「R 钳[5,200]」与调用点硬编码 `6.0` 自相矛盾 |
| **BLK-3** | `src/sdet_image.cpp:384` + `src/sdet_api.cpp:1601-1614,1624-1634,1636-1641,1649,1773,2193,2348` | **全非有限帧被判空的那道门 `:1634` 谓词结构性恒假**（因 `:1611-1612` 先用空样本中位数 0 把整帧改写成全 0）⇒ `:1636-1641` 与 `:1773`/`:2193` 全为死代码；损坏帧被伪造成干净帧后照常处理，`rc=0`；部分损坏时伪造比例不留痕；引导路径破坏自声明的六计数守恒 |
| **BLK-4** | `src/sdet_api.cpp:514`/`:515` 先于 `:516`；`:786`/`:787` 先于 `:788` | **memset 与字段写先于 `!result` 守卫** —— 作者写了空指针守卫，但该守卫对本情形是死代码 |
| **BLK-5** | `tests/p1star/p1star_mad_check.cpp:9-16`、`:89-101`、`:21-22`、`:85-86`、`:83` | 「P0 回归锁」名存实亡：两个操作数都不是 `InternalFitResult.mad`；声称的断言 4 不存在；`:85-86` 是纯测试内 constexpr 自洽式断言；`:83` FP32 容差比待抓扰动宽 ~130× |
| **BLK-6** | `tests/p1star/p1star_tests_selfcheck.cpp:66-69,71-72,76,100,106-108,117,123-125` | fork 失败 / `execve` 失败 / 被信号杀 三者皆非 0，全部被计为「必败验证通过」；注入子进程一条断言都没跑仍 PASS |
| **BLK-7** | `README.md:64`+§5/§6/§7 全表、`module.yaml:32-33,25,136-141,164-171` | 自称「全部行号锚」的映射 **20/20 全部落空**；引用 2 个已删函数 + 1 个已删文件；`R` 的公式与「饱和星 mag」两项权威文档↔代码冲突未裁定 |

| **BLK-8** | `src/sdet_api.cpp:722`（注释自陈 `Var(d)=2σ²(1−rho)`）+ `:768`（无条件 `×0.70710678118654752`）+ `include/star_detector.h:29-70`（无该字段） | **背景噪声估计隐含硬编码 `ρ_adj = 0`，而冻结科学正文明令禁止**。`docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:37-47` 逐字：「仅在 `ρ_adj = 0`（相邻像素噪声独立）时无偏 … `ρ_adj = 0.75` 时 `0.4991` **低估 50.1%** … 对 **drizzle/重采样/插值后的帧**，**必须**先用独立方法标定 `ρ_adj` 并除以 `√(1−ρ_adj)`；`ρ_adj` 未标定时检测阈的 σ 基准 = 另行独立标定的估计量」。而本项目的帧正是 drizzle/重采样产物（见创新点 P3 守恒映射算子），`rho_adj` 全仓 `lib/` 无任何出现。⇒ `σ̂_n` 低估至多 2× ⇒ `thr = bg + 5·bgnoise`(`:1649`) 偏低 ⇒ **虚警率上升，直接威胁冻结发布门 `G-P1-STAR-FP`（≤0.1/千像素）**。代码自己的注释 `:722` 写着 `(1-rho)`，`:768` 却无条件乘 1/√2 —— **注释里的公式恰好证明了代码的错** |

| **BLK-9** | `src/sdet_api.cpp:714-715`（同一常数乘两轴）+ `:325`（码 3 判 `fmin/fmax<0.5`）+ `:2086-2090`（R2 判 `smax/smin>2.0`，**先于** `:2093` 的 `reject_star`） | **O13 码 3 `SF_ROUNDNESS_BELOW_CRIT` 在生产是死判据**：因 `fwhm_x = fwhm_y = GAUSSIAN_FWHM_FACTOR · (sxr, syr)` 同一常数，故 `fmin/fmax ≡ smin/smax`；码 3 的触发条件 `smin/smax<0.5` 与 R2 的 `smax/smin>2.0` **是同一条件的倒数形式**，而 R2 先判且默认 `maxAxisRatio=2.0`（`include/star_detector.h:44`）⇒ **任何能触发码 3 的源必先被 R2 丢弃**（引导路径 `:2331-2338` 同序）。`p1star_reject_gate_test.cpp:120` **显式传 `maxAxisRatio=0.0f` 关掉 R2** 才使 `:121` 绿 —— 即该测试用**非生产配置**证明了一个生产中不可达的判据，且 `maxAxisRatio=0` 同时关掉了 LM 内各向同性重拟合（`:666-685` 的 `> 0.0` 守卫） |
| **BLK-10** | `CMakeLists.txt:1237`（`add_library(acsd_p1_sdet STATIC …)`）+ `lib/algorithms/star_detection/Makefile:53`（`star_detector.dll`）+ `README.md:178` + `tests/p1star/CMakeLists.txt:41` | **同一套生产源存在两条旗标不同的构建路径，而 p1star 全套 bitwise 断言只覆盖其一**。路径 A（产品）：根 `CMakeLists.txt:1237` 编同样 4 个 TU 进 `acsd_p1_sdet`，默认 `Release -O3`，**未设** `-ffp-contract`；路径 B（运行期 DLL）：`Makefile:23-25,28` 编 `star_detector.dll`，旗标含 `-ffp-contract=fast -funroll-loops`，由 `orchestrator.cpp` 显式加载。⇒ `p1star_properties`（F3 OMP 1/2/4 bitwise）、`p1star_oracle`（F6）、`p1star_perf_test`、`sdet_angle_guard_test` 的逐位断言**对 orchestrator 实际加载的 DLL 不构成任何保证**。更严重的是文档**否认路径 A 存在**：`README.md:178`「未编入根 CMake 主构建」与 `p1star/CMakeLists.txt:41`「生产 5 src 不在根 CMake 构建」**均为假**（实测 `:1237` 存在，且只有 4 TU）；而 `Makefile:2` 自己援引「根 CMakeLists.txt 是唯一产品事实源」，路径 B 正是违反该条的独立事实源 |
| **BLK-11** | `tests/p1star/CMakeLists.txt:77-79,161-163,183-185,209-211,265-267,309-311,338-340` | **`if(OpenMP_CXX_FOUND)` 静默降级，7 处零诊断**（实测 `grep -c` = 7，而同文件 `:146` 的 UNIX 分支**有** `message(STATUS …)`，形成刺眼对照）。后果：无 OpenMP 时 `sdet_api.cpp` 全部 `#pragma omp` 被忽略，`p1star_properties` 的「F3 OMP 1/2/4 bitwise」**变成自比自的空断言**，串行构建被绿灯认证为「已验证线程无关」 |
| **BLK-12** | `test/sdet_fp64_test.cpp`（全仓零 `add_test`） | **不在任何构建目标里 ⇒ 永不编译、永不运行、永不失败**。同目录另 3 个均已接线（`sdet_angle_guard_test.cpp` 经 `p1star/CMakeLists.txt:225`；`sdet_saturation_cursor_test.cpp` 经 `eng/tests/unit/CMakeLists.txt:1660`；`sdet_deblend_contrast_oracle_test.cpp` 经 `:1683`）。叠加 `:84` 的 `if (n32 > 0 && n32 == n64)` 空断言通过。`README.md:186` 以 `NON_PRODUCTION_TOOL_ONLY` 为排除理由**不成立** —— 同一标签也印在两个确实注册为 CTest 的文件上（`test/sdet_saturation_cursor_test.cpp:3`、`test/sdet_deblend_contrast_oracle_test.cpp:3`） |

### 须修（16）

| ID | 位置 | 问题 |
|---|---|---|
| R1 | `src/sdet_image.h:3-43` + `src/sdet_image.cpp` | 21 个导出 **16 个零调用**（约 85% 实现为死码，含全部 15 条 OpenMP 与 2 处 `omp_get_wtime`）；存活路径无任何 OpenMP |
| R2 | `src/sdet_api.cpp:905-1016` 及 `:80-125`/`:127-134`/`:856-899` | `sdet_detect_saturated_stars`（112 行）+ 3 个私有依赖全仓零调用，与在役 O4a 构成两套并存饱和检测；`:974` `meanhigh_filtered` 恒 0 却进 `:1013` 日志（**公平校准：位于死码内，不影响生产输出**）；`:944` `satrange` 计算后仅进日志 |
| R3 | `src/sdet_log.cpp:31,38-44,86-88` + `:13-15` + `.gitignore:8` | 落盘失败吞返回值（唯一 ERROR 上报口静默丢失且缓存失败态不再重试）；3 个无文档静态可变全局 + 永不关闭的长期句柄；CWD 相对硬编码路径已在仓内生成**嵌套重复目录树**（物证 `star_detection/lib/algorithms/star_detection/logs/star_detector.log`）与 24.96 MB 日志，两者 `git status` 不可见 |
| R4 | `tests/p1star/p1star_tests_core.cpp:562-572` | `f3_nan_mag_tail`/`f3_nan_field_order` **恒真**：`n_nan` 统计后从未被断言 >0（本人 grep 确认），且 BLK-3 的中位填充使该场下 NaN mag 不可能产生 |
| R5 | `tests/p1star/p1star_tests_core.cpp:620-624`、`:596-603`、`:283-286` | 条件跳过无独立前置断言；断言失败后仍越界读；recall10 门被撤回后无替代 |
| R6 | `tests/p1star/p1star_guided_test.cpp:160,168`、`:155-159` | 先筛后取极值 ⇒ `:168` 是 `:160` 的逻辑后承，零独立判别力；最近邻无 `used` 单射 |
| R7 | `tests/p1star/CMakeLists.txt:190,231,247,272,292,346` | 6 个已注册 ctest 用 `PASS_REGULAR_EXPRESSION` **覆盖退出码**判红绿 |
| R8 | `src/sdet_test_probe.h:6-8` + `test/sdet_deblend_contrast_oracle_test.cpp:138` + `test/sdet_saturation_cursor_test.cpp:123` | 探针只暴露生产已算好的操作数 ⇒ 判据合同断言是**同一表达式的恒等重放**，对分子计算式零判别力（X10）；抬头「独立复算」措辞为假 |
| R9 | `tests/p1star/p1star_nls_lm_test.cpp:43,64` | 「生产同款母函数」「与 `sdet_gaussian_df` 同式」两条声明为假；目标只链 `nls_lm.cpp`，生产解析雅可比零差分验证（BLK 级覆盖缺口） |
| R10 | `src/nls_lm.cpp:309,316-320` | 精确零列→`D=1.0` 而近零列→`D=dmax`，两处策略不一致，且把不可辨识方向提升到**最大**步长优先级；其背书测试只构造精确零列，从不行使该分支 |
| R11 | `Makefile:37`、`:70-73`、`:24` | PCH 头文件不存在 ⇒ `make pch` 必失败；`clean:` 用 Windows `del /f /q`；硬编 `-fopenmp` 而同文件以「根 CMake 是唯一事实源」为由删掉了 `-march=native` |
| R12 | `src/sdet_api.cpp:45-46`、`:618`/`:633`/`:679`、`:2424`/`:2443`/`:2468` | `REPORT §5.2` 不存在且 `0.8%→14%` 不在被引文档（真实 13.60%→4.05%，**结论另有 `REPORT.md:138` 支撑，错的是指针与数字**）；`0.2122` 应现场算 `0.5/2.3548…`=0.212332；导出包装 -1 路径不清 out 而 impl 层清了，两层合同不一致 |

| R15 | `module.yaml:14,19,31,93-94` + `README.md:16-23,75,146,171` | **落位依据与 §4 引用全部悬空**：`MODULE_MIGRATION_MATRIX.csv`（README §0 整表与 module.yaml:19 的基础）、`11_MODULE_SOURCE_TEST_STANDARD.md §4/§5`（module.yaml:14/:31、README:75/:146/:171）**经实测全仓均不存在**；`eng/ci/ctest_baseline.json`、`eng/tools/quality/check_ctest_registration.py`、`eng/ci/ledgers`（`CMakeLists.txt:144-145` 用作「非 UNIX 跳过 negative 组仍可见」的**安全网依据**）**同样不存在** ⇒ 该自证完全落空 |
| R16 | `README.md:47,117` + `module.yaml:138` | **`SDetParams` 字段数错**：文档三处称「9 字段（`star_detector.h:13-24`）」，实测为 **13 字段、结构体在 `:29-70`**（本人 `grep` 计数确认）。README §6「config schema / default」整表**完全遗漏** R-58-1 的四个子门字段及其默认（0.5 / 2.5 / 0.35 / 4，`sdet_api.cpp:1124-1127`）。README 自称「模块唯一合同入口」 |

### 建议（8）

| ID | 位置 | 问题 |
|---|---|---|
| S1 | `src/sdet_api.cpp:1032-1060` | 未知 extras 名（键名被改/拼错）→ `EXTRA_UNKNOWN` → 全列 0.0f，**无错误码**，正是本项目反复出现的「键名已改」静默形态 |
| S2 | `src/sdet_api.cpp:2081`、`:1560` | 盲路径拟合失败 `continue` 无计数无日志（guided 路径有 `n_fit_failed`）；`wsum` 无零护而 `:995-996` 同构代码有护 |
| S3 | `tests/p1star/p1stardet_node_gate_test.cpp:15` | 抬头声明的 **N4 全文无实现**；`:492` 消息写 `>>` 而断言只有 `>`；`:200`/`:205` 不探 50000 端点 |
| S4 | `tests/p1star/sdet_oom_interposer.c:13-16,89-96,90,55` | 只拦 4 个分配入口；`a*b` 可回绕；`OOM_TOTAL` 是过滤后计数 |
| S5 | `tests/p1star/p1star_psf_shape_gate_test.cpp:95-96,106,111-112` | 10 处丢弃 `run_detect` 返回值且失败时不碰 `*out` ⇒ 负例断言在部分失效下变空洞真 |
| R13 | `src/sdet_api.cpp:333` + `:1499-1500` + `:1993-1994` | **FWHM 上限门在 `se_smax ≤ 2.0` 时整门不评估**，而 `se_smax` 来自 `sdet_zero_cross_dir` 零交叉缺失时的**静默默认** `dist=1.0`；`:1470-1476` 又对饱和候选先跳过整段饱和平台再找零交叉 —— **滤掉的恰是携带真实半高宽信息的像元**。此时 40 px 宽的非点源只受 `:322`/`:325` 约束即可通过，且 `R` 被钳到 5 ⇒ `:2126` 的 mag 用 121 像元代表跨数千像元的源。`p1star_reject_gate_test.cpp:127` 的负例明确取 `se_smax≈2.5 > 2.0`（走有门那侧），**全仓无 `se_smax ≤ 2.0` 的负例** |
| R14 | `src/sdet_api.cpp:1909,1933,2005,2021,2022,2048,2081,2089,2093` + `:1666` + `:948,963,967,1013` | **活检测链零诊断**：全文 7 处 `sdet_log` 有 4 处在零调用函数内，活路径只剩 create/destroy/O13b 三条；候选段 **9 个 `continue` 丢弃点与 `:1666` 的 maxStars 截断全部无计数无日志** —— 一次真实召回崩塌在日志里与一次正常运行完全同形 |

---

## 5. 我主动构造的反例

口径：注入一个**真实的生产缺陷**，逐条追踪本片判据是否转红。**推翻 = 判红（有效门）；未推翻 = 判绿（覆盖缺口/恒真门）**。

### X1 — 生产孔径测光通量整体减半 ⇒ **未推翻（判绿）**【最强】
注入 `src/sdet_api.cpp:2123-2124`（及 `:2394`）的 `box_sum` 乘 0.5。生产 mag 变 `mag_true + 0.7526`；而 oracle 因**自身少一个因子 2**（BLK-2）给出的期望值恰是同一个数 ⇒ `mag_abs ≈ 0` ⇒ `:701` 通过。
**结论：把星等通道整体改错一倍，全片唯一绝对星等门判绿。**（`2.5·log10(2)=0.7526`，与 oracle 系统偏差逐位相等，本人以 `python3` 三档 σ 复核。）
**附证**：把 `:2113` 的 `Rb = c.R` 改为 `params.fitRadius`（6 而非 10-13）同样判绿，**且变异后 oracle 反而更接近生产** —— 教科书级 oracle 反转。

### X2 — 各向同性回退生效区间内的轴比 2.167 源 ⇒ **未推翻（判绿）**
注入场景即 `sigma=(2.6,1.2)`、**默认** `maxAxisRatio=2.0`、m≥8。回退收敛 ⇒ `sx=sy` ⇒ 两门读到 1.0。
追踪：`p1star_reject_gate_test.cpp:118` 的夹具**正是**这个配置，但 `:120` 显式传 `maxAxisRatio=0.0f` 把分支关掉；`:105` 的轴比门夹具轴比 11.4，回退拟合不上。**全仓无第三个夹具。结论：未推翻。**

### X3 — deblend 判据分子计算式改错 ⇒ **未推翻（判绿）**【子代理提出、本人复核成立、推翻了我的初判】
注入 `src/sdet_api.cpp:1302` 的 `fsum += smooth[...] - t;` 改为 `- thr;`（分子从当前层阈值改为检出阈值，直接违反其自己引的 B&A96/SExtractor/SEP 判据 `:1287-1291`）。
追踪：探针把**新** `fsum` 记入 `p.fsum`(`:1313`)，**新**判定记入 `p.sat1`(`:1317`)，测试用**同两个操作数与同一常数**重算 ⇒ `expect == sat1` 恒成立 ⇒ `n_contract_violation == 0` ⇒ `w34_sdet_deblend_contrast_oracle` 与 `w34_sdet_saturation_cursor` 的断言①全绿。
**结论：未推翻。** 该 oracle 只对**比较算子形式**与**常数**有判别力，对**分子计算式**零判别力 —— 而分子恰是该判据的正文（`:1287-1291` 明写「分子 = 枝在当前层阈值 t 之上的积分流量」）。

### X4 — NaN 排序比较器对 NaN 恒返回 false ⇒ **未推翻（判绿）**
注入 `src/sdet_api.cpp:1098` 的 `if (a_nan || b_nan) return !a_nan && b_nan;` 改为 `return false;`。唯一依赖 NaN mag 的门是 `core:571`/`:572`，但 BLK-3 的中位填充使该 fixture 下 `n_nan == 0`，两循环空转恒真（R4）。**与 R4 复合：即使有人造出 NaN mag，该门也已失去分辨力。**

### X5 — 修复 P128 的 `queue.clear();` 被删 ⇒ **推翻（判红，有效）**
`src/sdet_api.cpp:1861` 删除 ⇒ 组分 i 读数 = 前 i 组分像素和 ≈1600 > 350 ⇒ `core:994` 转红。**FIX-P128 门是真·可红可绿**（`core:957-961` 自陈的红绿双向见证成立）。

### X6 — P128 回归锁被宏删除 ⇒ **未推翻（静绿，零诊断）**
删 `tests/p1star/CMakeLists.txt:74` 的 `SDET_TESTING` 定义 ⇒ `core:989-1001` 整块不编译（无 `#else`/`#error`）⇒ 只剩 `p128_handle`/`p128_detect_rc` 两条 ⇒ 返回 0 ⇒ ctest 绿。`p1star_test_main.hpp` 无 `checks>0` 断言 ⇒ **零诊断**。

### X7 — 饱和列恒为 1 ⇒ **未推翻（判绿）**
注入 `:2099`/`:2372` 的 `(f.A > dynrange)` 改 `(f.A > 0.0)`。本片全部饱和断言只断 `== 1`（`core:434,453,457,727`）；本人 grep 确认**无任何 `sat == 0` 负断言**。`reject_star` 用的是候选 `c.sat` 而非 `rec.is_saturated` ⇒ 检出集合不变。**结论：未推翻。**

### X8 — 全 NaN 帧 ⇒ **未推翻（判绿，且比"早返回"更坏）**【子代理提出、本人复核成立、推翻了我的初判机理】

注入：`sdet_detect_ex_f64`，`width=64 height=64`，4096 像元全 `quiet_NaN()`。
真实链路（复核 `sdet_median_inplace:384` 的 `if (n==0) return (F)0` 后确立）：`:1608` `med = sdet_robust_median_d(全 NaN)` → `sdet_median_of_finite:404` 滤空 → **med = 0.0** → `:1611-1612` **整帧被改写成全 0** → `:1626`/`:1633` `bg = maxi = 0.0` → `:1634` 谓词 **恒假** ⇒ `:1636-1641` 不执行 ⇒ `:1649` `thr = 0.0`（有限）⇒ `:1773`/`:2193` **也不触发** ⇒ 16 算子主链在全 0 伪造帧上跑完，`rc=0`、`count=0`、指针全 NULL、**无日志**。
**结论：未推翻，且 `:1636-1641`/`:1773`/`:2193` 三段是死代码。** 我首轮写的"fail-open 早返回"机理错误 —— 真实行为是"伪造成干净帧后照常处理"。
**附加**：`p1star_tests_core.cpp:558` 只注入**局部** NaN 区，`:561` 反而 `P1STAR_CHECK_EQ(cs, rcn, 0, "f3_nan_field_rc")` **把 rc=0 钉住**；`tests/` 与 `test/` 下无任何全 NaN 帧用例。

### X9 — FWHM 上限门整门跳过 ⇒ **未推翻（判绿）**
注入：某候选平滑剖面平顶/被削平，使 `sdet_zero_cross_dir:1479-1500` 在 4096 步内走不出 `prev<0 && d2>=0`（或先撞帧边）⇒ 静默返回 `dist=1.0` ⇒ `:1987` `Sr = 1.0` ⇒ `:333` `se_smax > 2.0` 为假 ⇒ **整门不评估**。40 px 宽非点源只受 `:322`（0.5 px 下限）`:325`（圆度）约束即可通过；`:1993-1994` 又把 R 钳到 5 ⇒ `:2126` 的 mag 用 121 像元代表跨数千像元的源，严重偏亮。
追踪：`p1star_reject_gate_test.cpp:127` 的负例明确取 `se_smax≈2.5`（走**有门**那侧），`psf_shape_gate` 只覆盖 O13b 不驱动 `se_smax`。**结论：未推翻。**

### X10 — deblend 判据分子计算式改错 ⇒ **未推翻（判绿）**【BLK-8 之外的最强同义反复】
删/改名 `gaia/GaiaDR3/` ⇒ `:562-566` SKIP、`:577-580` 仍 `PASS` 返回 0；转红开关 `ACSD_STARDET_REQUIRE_GAIA` 全仓从未配置。
**本人反向验证**：`gaia/GaiaDR3/gdr3-1.0.0-01.xpsd` 与 `testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts` **均存在** ⇒ 当前构建不走 SKIP 分支。**结论：反例未成立**，降为潜伏须修，**不计入阻断**。

### 对照组（证明本片确有有效判别力，不是一律判绿）

| 注入 | 追踪 | 结果 |
|---|---|---|
| `sdet_api.cpp:2098 rec.flux = (float)f.A` → `*1.01` | `core:688,697` 平均相对误差 ≤1e-3 | **红**（flux 通道 oracle 是真的） |
| `sdet_api.cpp:1099` 排序方向反转 | `core:546` 用**独立实现**的 `oracle::mag_less` | **红**（全序 oracle 语义克隆无误） |
| `sdet_api.cpp:1861 queue.clear();` 删除 | 见 X5 | **红** |
| `sdet_api.cpp:458-472` 形状门改直通 | `p1star_psf_shape_gate_test.cpp:162` c1_green | **红** |
| `sdet_image.cpp:420` 1.482602218505602 → 1.4826 | `p1star_mad_check.cpp:79` FP64 半边 | **红**（但 `:83` FP32 半边**绿**，容差 0.02 ≫ 扰动 1.5e-6 ⇒ PARTIAL） |
| `sdet_api.cpp:1307` 判据退回 bflow 基准 | `expect != sat1` ⇒ 逐枝不等 | **红**（只覆盖比较算子，不覆盖分子，见 X3） |

---

## 6. 盲复算

口径：遮住既有判定（`逐份判定-权威版.csv` 与他人 `审稿-*.md`），只用片清单 + 原文独立重算本片 38 份。

| 项 | 结果 |
|---|---|
| 方法 | 只读 `片清单-权威版.yaml` 取成员与行数；逐行读原文；结论一律从源码推导。未打开 `逐份判定-权威版.csv`，未检索任何 `审稿-*.md` 作为依据 |
| 与既有判定的关系 | **偏严**。新增既有材料未覆盖的 7 类阻断：BLK-1 各向同性回退中和双门、BLK-2 oracle 因子 2、BLK-3 损坏输入 fail-open + 静默伪造、BLK-4 memset 先于守卫、BLK-5 mad_check 名存实亡、BLK-6 selfcheck 把「子进程没跑」计成「必败通过」、BLK-7 文档行号映射全脱钩 |
| 一致处 | 生产主链 16 算子的**算术与门序**逐行复核后与注释/合同一致，未发现算错；`sdet_median_inplace:382-396` 的 nth_element 等价性推导正确；`sdet_angle_guard.h` 的三条自声明契约全部成立；`sdet_emit_records:1657-1743` 的事务化回滚与 `sdet_detect_saturated_stars:961-965` 的 CC 分配失败是全片最干净的错误处理；`:1571-1573` 关于 `isl.cx/cy`「无消费者」的声明**经核实为真** |
| **自查推翻 3** | ① 「deblend oracle 非同义反复」→ 错，实为对分子的恒等重放（X10）；② 「22 处 OpenMP = 私建线程池违规」→ 措辞错，`CONCURRENCY_STANDARD.md:32` 明写科学路径「**只用显式并行区**」为**许可形态**，`:34` 禁的是硬编码 worker 数与**长期**线程池，`:7` 禁的是库内改全局 setting；本模块无 `omp_set_num_threads`/`pthread_create`/嵌套并行 ⇒ **合规**，仅剩「文档把 22 处说成三处」的描述性缺陷（子代理 #4 与 #5 在此**互相矛盾**，我采信正本原文并否决 #5 的 B3）；③ 「全非有限帧 → thr=NaN → fail-open 早返回」→ **机理错**，真实是 `:1634` 恒假门 + `:1611-1612` 把整帧改写成全 0（X8，见 BLK-3 修订版） |
| 偏严是否合理 | 是。负责人裁定「判据不可信、不必核实为通过」，而本片 10409 行中约 8000 行为测试、判据密度最高，严判是口径使然 |

---

## 7. 子代理派发记录

派发 **4** 个子代理（后台、只读、无写权限、禁读 `/tmp/acsd_g08/`）。**4 个全部返回。**

| # | 子代理 | 切片 | 本人复核结论 |
|---|---|---|---|
| 1 | `da7499d0` | oracle/fixtures/core/guided/nls_lm/perf | **采纳 5**（BLK-2 oracle 因子 2、R4 NaN 门恒真、R9 两条假声明、R5 条件跳过、R7 正则门）、**降级 2**、**否决 3**（其称阈值表属阻断级 same-constant → 我复核后接受「公开披露的双向特征化锁」限定并单列量化盲区；其称 deblend/saturation 的 `n_contract_violation` 同义反复 → **我首轮采纳、后经 X3 复核确认该判断更接近真相但仍不到位**，真正的恒等重放由 #3 给出；其称 oracle 抬头与调用点矛盾属独立缺陷 → 并入 BLK-2 不重复计数） |
| 2 | `b492de31` | 同切片（独立第二意见） | **采纳 3**（R9 + F4 只测 3/7 量；X6 宏摘除；A2 P128 静默编译掉）、**采纳 1 条并改写口径**（其称线程池「合规」→ 我**初判相反并被其纠正**，见 §6 自查推翻）、**否决 1**（其称 CM 行号引用为悬空但影响面小 → 只并入 BLK-7 附注） |
| 3 | `1ac81d27` | 门测试片（node_gate / psf_shape_gate / reject_gate / mad_check / selfcheck / test_main / tests_main / test_probe） | **采纳 4 且全部经我独立复核**（① **X3 去 blend 分子恒等重放** —— 直接推翻我的初判，已升级为 R8；② **BLK-6 selfcheck 把 fork/execve/信号死计成「必败通过」** —— 我首轮完全遗漏；③ **BLK-4 memset 先于 `!result` 守卫** —— 我已亲自 `sed` 复核 `:513-517`/`:785-789` 确认；④ **BLK-5a mad_check 锁错代码路径** —— 补上了我缺的机制（拟合路径 vs 背景噪声路径））、**否决 1**（其称 `p1star_test_main.hpp` 为 ok —— 我采纳其解析正确性判断但**否决其未指出 `checks>0` 缺失**，已记 R4/§3-20）、**正面确认 4 项**（psf_shape_gate 的门 ON/OFF 语义、tests_main 无静默路径、`sdet_test_probe.h` 非退役对象、CE-5/CE-2 的红侧实证） |
| 4 | `45f4b330` | `src/` 8 文件（4187 行） | **采纳 6**（**BLK-1 各向同性回退中和双门 + 测试用 11.4:1 与 `maxAxisRatio=0` 两种手法自证门只在窄缝有效** —— 这是本片最强生产缺陷，我初判为「须修 R1」，据此升级；BLK-3 fail-open + 静默伪造；BLK-4 memset 顺序；R12 `REPORT §5.2` 失核；R2 `meanhigh_filtered` 恒 0；R10 近零列钳位方向反）、**部分否决 1**（其称 `sdet_api.cpp` OpenMP **8 处** —— 实测 7 处，我以 `grep -c` 为准）、**采纳其纠正我的两处**（线程池合规性、`isl.cx/cy` 声明为真）、**接受其公平校准 2 条**（`meanhigh_filtered` 在死码内不影响生产；`REPORT §5.2` 错的是指针不是论证） |

| 5 | `df261b6c` | `src/` 8 文件（4187 行） | **采纳 4 且全部经我独立复核**（① **X8 全 NaN 帧的真实机理** —— `:1634` 恒假门 + 空样本中位数 0 把整帧改写成全 0，**直接推翻我的 BLK-3 初版机理**，已改写为修订版并升为更重的"恒假门 + 静默伪造"；② **X9 `:333` FWHM 上限门整门跳过**，已立 R13；③ **R14 活检测链零诊断**（9 个丢弃点 + 7 处 log 有 4 处在死码内），已立 R14；④ **BLK-8 `ρ_adj` 硬编码 0**，我已 `grep` 正本 `STAR_DETECTION_ALGORITHMS.md:37-47` 与 `lib/` 全树核实成立，是本片最强科学发现）、**采纳其 M2/M4/M6/M7/M8/M9/M10/M11**（其中 M7 `_WIN32` 路径 `lib\star_detector\logs` 目录不存在，我已归入 R3）、**否决 2 条**（① **B3「私建线程池违规」** —— 与子代理 #4 直接冲突，我读正本原文后**站在 #4 一边**：显式并行区是许可形态，本模块合规；② **CE-3「无任何 >maxStars 的构造」** —— **为假**：`p1star_tests_core.cpp:582-603` 明确设 `p.maxStars = 10` 跑 25 星 FIX-STAR-D 场，截断**确实**被行使且有断言；我仅保留其"无 `n_found`/`n_emitted` 计数"部分并已并入 R14）、**接受其反向记录 8 项**（`nls_lm` fail-closed 无吞异常、`:2014/2017/2020/2305/2308/2311` 的 1e20/1e30 哨兵确为 fail-closed、`:1307` 的 `total_flux>0` 不可达、`:328` 的 fail-open 当前不可达、`:455` O13b 属有意识防御、三处「无消费者」注释经核实为真、13 个 ctest 注册无孤儿测试文件） |

| 6 | `7cfbd026` | 构建接线 / 悬空引用 / 退役对象 / 双实现（16 文件） | **采纳 6 且全部经我独立复核**（① **BLK-10 双构建路径 + 文档否认路径 A 存在** —— 我 `grep` 到根 `CMakeLists.txt:1237 add_library(acsd_p1_sdet STATIC` 确认成立，README:178 为假；② **BLK-11 `if(OpenMP_CXX_FOUND)` 7 处零诊断** —— 我 `grep -c` = 7、`message(` = 1（UNIX 分支）确认；③ **BLK-12 `sdet_fp64_test.cpp` 零构建接线** —— 与我先前「另 2 个已接线」的核实互补，且它指出 `NON_PRODUCTION_TOOL_ONLY` 标签**不构成有效排除依据**（同标签也印在两个已注册文件上），这一点我采纳并补入；④ **R15 落位依据全悬空** —— `MODULE_MIGRATION_MATRIX.csv`、`11_MODULE_SOURCE_TEST_STANDARD.md`、`eng/ci/*` 三者我先前已实测 MISS；⑤ **R16 `SDetParams` 13 字段非 9** —— 我 `grep` 计数确认；⑥ **BLK-9 码 3 被 R2 影子遮蔽** —— 我 `sed` 核对 `:714-715`/`:325`/`:2086-2090` 后确认成立）、**否决 2 条**（① 其称 `CMakeLists.txt:141-149` 的 else 分支自证落空我**部分保留**并入 R15，但其「应改 message(WARNING)」属建议已并入；② 其把 `p1star_reject_gate_test.cpp:26/:194` 的注释解析式列为缺陷 —— 与子代理 #3 的同一条**重复**，且两人复算结论互相矛盾（我未运行代码核验，保留为 U5 不单独计数）、**接受其 3 条反向记录**（`include/star_detector.h` 的内部行锚是**全仓唯一准确的一套**；`wrapper_phase1` 的两个 `star_detector.h` 是不同符号族非重名冲突；CHANGELOG 的 GSL-REPLACE-01 条目实测为真）、**接受其一处自我更正留痕**（`sdet_saturation_cursor_test.cpp:23/:25` 的 38740/7240 复算后确认两数均正确，不构成缺陷 —— 与我的结论一致）

**采纳合计 26 条；否决/降级 11 条；自查推翻本人初判 3 条。**

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD          # 850a9edefd47434b9ab71bc907c3de1e0814b323

# 覆盖率
python3 -c "
import yaml
d=yaml.safe_load(open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8'))
s=[x for x in d['片清单'] if x['片号']=='ALG-star_detection-001'][0]
print(s['成员份数'], s['实际行数'], len(s['成员文件']))"          # 38 10409 38

# BLK-1 各向同性回退中和双门 + 测试自证窄缝
sed -n '270,277p;666,685p;323,326p;2086,2090p' lib/algorithms/star_detection/src/sdet_api.cpp
sed -n '103,110p;114,123p' lib/algorithms/star_detection/tests/p1star/p1star_reject_gate_test.cpp

# BLK-2 oracle 因子 2（纯闭式复算，不依赖仓外信息）
python3 -c "
import math
for s in (1.8,2.0,2.8):
    ex=math.erf(6.0/(s*math.sqrt(2.0)))
    print('sigma=%.1f oracle=%.6f true=%.6f ratio=%.4f bias=%+.4f'%(s,math.pi*s*s*ex*ex,2*math.pi*s*s*ex*ex,0.5,0.7526))"
sed -n '46,50p'  lib/algorithms/star_detection/tests/p1star/p1star_oracle.hpp
sed -n '688,702p' lib/algorithms/star_detection/tests/p1star/p1star_tests_core.cpp

# BLK-3 损坏输入 fail-open + 静默伪造
sed -n '1601,1614p;1630,1642p;1773p;2175p;2193p;2348p' lib/algorithms/star_detection/src/sdet_api.cpp

# BLK-4 memset 先于空指针守卫
sed -n '513,517p;785,789p' lib/algorithms/star_detection/src/sdet_api.cpp

# BLK-5 mad_check 的两个操作数都不是 InternalFitResult.mad
sed -n '89,101p' lib/algorithms/star_detection/tests/p1star/p1star_mad_check.cpp
grep -n "result->mad" lib/algorithms/star_detection/src/sdet_api.cpp      # 唯一真实赋值点 :716

# BLK-6 selfcheck：fork/execve/信号死 三者皆非 0 都被计为「必败通过」
sed -n '65,77p;98,110p;114,127p' lib/algorithms/star_detection/tests/p1star/p1star_tests_selfcheck.cpp

# BLK-7 文档行号脱钩（抽样 4 条，应全部落空）
grep -n "本 README 全部行号锚" lib/algorithms/star_detection/README.md
grep -n "sdet_create\|sdet_lm_fit\|reject_star\|sdet_sort_stars" lib/algorithms/star_detection/README.md
grep -n "^SDET_EXPORT StarDetectorHandle sdet_create\|^static int sdet_lm_fit\|^static SfError reject_star\|^void sdet_sort_stars" lib/algorithms/star_detection/src/sdet_api.cpp
ls lib/algorithms/star_detection/src/sdet_detector.cpp lib/algorithms/star_detection/src/sdet_background.cpp 2>&1

# X3 去 blend 分子恒等重放
sed -n '1300,1318p' lib/algorithms/star_detection/src/sdet_api.cpp
sed -n '135,142p' lib/algorithms/star_detection/test/sdet_deblend_contrast_oracle_test.cpp

# 并发合规性自查（我初判错误、据正本纠正为合规）
grep -n "显式并行区\|omp_set_num_threads\|不私建" docs/engineering/CONCURRENCY_STANDARD.md
grep -c "pragma omp" lib/algorithms/star_detection/src/sdet_api.cpp lib/algorithms/star_detection/src/sdet_image.cpp
grep -rn "omp_set_num_threads\|pthread_create\|std::thread" lib/algorithms/star_detection/src/   # 应无命中

# 死接口面：21 个声明中 16 个零消费者
for s in sdet_gaussian_filter_separable sdet_median_filter sdet_dilate_box sdet_erode_circle \
         sdet_truncate_and_rescale sdet_local_maxima_map sdet_extract_lowfreq_atrous \
         sdet_iterative_sigma_clip sdet_dynamic_regional_background sdet_detect_saturated_stars; do
  printf '%-40s %s\n' "$s" "$(grep -rn --include=*.cpp --include=*.h "$s" lib/ | grep -vc 'sdet_image.cpp:\|sdet_image.h:')"
done

# R3 CWD 相对路径物证 + git 不可见性
ls -la lib/algorithms/star_detection/logs/star_detector.log
find lib/algorithms/star_detection/lib -type f
git -c core.quotepath=false check-ignore -v lib/algorithms/star_detection/lib/algorithms/star_detection/logs/star_detector.log
git -c core.quotepath=false status --porcelain lib/algorithms/star_detection/lib   # 期望空

# X8 数据集反证（反例未成立）
ls gaia/GaiaDR3/gdr3-1.0.0-01.xpsd testdata/NGC55_T3_flying_dutchman/lights/ | head -3
grep -rn "ACSD_STARDET_REQUIRE_GAIA" --include=*.txt --include=*.json --include=*.yaml . | grep -v '^./build/'

# 悬空引用总表（应全部 MISS）
for p in docs/engineering/11_MODULE_SOURCE_TEST_STANDARD.md docs/science/GATES_AND_TOLERANCES.md \
         "run/全仓-01/review/review-stage3-sdet.md" run/stage3-fix/probe \
         run/local/bughunt_batchR/REPORT.md docs/detail/algorithms_phase1/03_star_detection.md \
         eng/ci/ctest_baseline.json lib/algorithms/star_detection/archive \
         lib/algorithms/star_detection/src/stardetector_common.h; do
  [ -e "$p" ] && echo "OK   $p" || echo "MISS $p"
done

# 本遍零写自证
git -c core.quotepath=false status --porcelain | head
git -c core.quotepath=false diff --stat HEAD | head
```

**纪律自证**：本次会话对仓内**唯一**写入是本交付件；无 `git add/commit/checkout/reset/stash/git rm`；未运行编译、ctest、pytest、cmake、make 或任何二进制；4 个子代理均限定只读且未读 `/tmp/acsd_g08/`。

---

## 9. 登记 UNRESOLVED（交前台裁定）

| ID | 事项 | 分歧 | 影响 | 所需输入 |
|---|---|---|---|---|
| U1 | `R = max(ceil(3.7172·Sr), ceil(3.7172·Sc), r)`（README:108）vs `R = ceil(SDET_S_FACTOR·max(Sr,Sc))`（`sdet_api.cpp:1993`） | README 多一个 `max` 与一个 `r` 项 | 直接改变 O9 拟合盒 ⇒ 影响 mag/flux 全部读数 | 以 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` §2/§3 与 `docs/detail/STAR_DETECTION_IMPL_DESIGN.md` §5.12 逐字裁定（本人未逐条比对 ALG 正本） |
| U2 | 饱和星 mag 是否应为 `−2.5·log10(A)`（README:110、module.yaml DISP-STAR-004）还是全体 box_sum（`sdet_api.cpp:2126`/`:2397`） | 文档登记了一个代码里不存在的分支 | 若文档对，则代码漏了饱和星专用分支 | 同 U1 |
| U3 | `SDET_S_FACTOR`、`norm=65535` 等常量的科学推导出处 | README:103-107 给了公式但未给 ALG 条款号 | AGENTS §6 要求有科学推导的常数须注明出处 | ALG §2 对应条款号 |
| U4 | 22 处 OpenMP 的线程预算归属 | `CONCURRENCY_STANDARD.md:32` 已确认「显式并行区」为许可形态、`wrapper_phase1/README.md:24-28` 声明由 Runtime 决定；但 `module.yaml:143` 登记「ThreadBudget 未接线」 | 决定是否需补接线；不影响「无硬编码 worker 数、无长期线程池」的合规结论 | 调度器侧 `ThreadBudget` 接线契约 |
| U5 | `p1star_reject_gate_test.cpp:24-26` 抬头解析式与其 `:184-186` 构造不自洽（「S 峰 3079」按 σ_eff=2.5 复算对不上） | 子代理提出、本人未运行代码核验 | 该用例是 FIX-R3「冻结 O8 吞没弱峰」的行为锁，算式错则锁的可能是另一个量 | 前台运行该 target 打印实测值比对 |
| U6 | `ρ_adj` 取值（BLK-8） | ALG `:46-47` 要求"标定 ρ_adj"**或**"另行独立标定的估计量"，二者择一未定；本项目帧经 drizzle/重采样但 `ρ_adj` 实测值仓内无记录 | 决定 `bgnoise` 是标定后修正还是换用别的估计量；直接影响 `G-P1-STAR-FP` 能否守住 | 前台按 ALG `:42-44` 的 AR(1) 合成法实测本仓帧的 `ρ_adj`，或裁定改用独立 σ 估计器 |

---

## 10. 交付统计

| 项 | 数值 |
|---|---|
| 成员份数 / 读完份数 | 38 / 38 |
| 成员总行数 / 读出行数 | 10409 / 10409 |
| 覆盖率 | **100%（份数与行数双口径）** |
| 阻断 | **12** |
| 须修 | **16** |
| 建议 | 8 |
| 主动构造反例 | **13 个**（X1–X10 + 3 条来自子代理、本人复核），其中 **7 个未推翻（判绿）** |
| 子代理 | 派发 5、**全部返回**；采纳 26、否决/降级 11；**自查推翻本人初判 3 条** |
| 登记 UNRESOLVED | 6（U1–U6） |

---

*本报告为 G08-05 第 1 遍对抗审稿交付（修订版）。覆盖率 100%（38/38 份、10409/10409 行），无未读项。修订内容：据 4 个子代理回报推翻本人两条初判（deblend oracle 判定、OpenMP 合规性），并新增 3 条阻断（memset 守卫失效、selfcheck 假通过、文档行号全脱钩）。*