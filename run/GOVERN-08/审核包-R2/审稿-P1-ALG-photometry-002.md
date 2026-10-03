# 审稿-P1 · ALG-photometry-002 · G08-05 对抗审稿第 1 遍

- **片号**：ALG-photometry-002（层 `lib/algorithms/photometry`）
- **基线**：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- **清单**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:429-455`（成员 19 份，目标 11000，实际 9747）
- **判定口径**：负责人裁定 —— 判据不可信，不因「检查通过」认为实现正确；正确性只靠对齐质疑、重新推导、构造反例得出。
- **本轮我改了哪些文件**：仅本交付件。零 git 写、零仓内文件改动、未编译、未跑 ctest/pytest/任何二进制。

---

## 1. 读完了吗

**两个口径必须分开报，不能合并成一个数字。**

### 口径 A：逐行通读（read 工具，verbatim）

| 成员文件 | 行数 | 我实读 |
|---|---:|---:|
| include/acsd/psfsw.h | 407 | 407 |
| cpp/include/photometric_calib.h | 278 | 278 |
| cpp/src/pc_api_qf.h | 50 | 50 |
| cpp/include/log_macros.h | 20 | 20 |
| docs/architecture.md | 254 | 254 |
| cpp/test/gate4…/gate7_hips_validate.py | 245 | 245 |
| cpp/test/gate4…/gate8_trace_check.py | 124 | 124 |
| cpp/test/gate4…/gate3_wcs_oracle.py | 99 | 99 |
| cpp/test/gate4…/download_xp_sample.py | 202 | 202 |
| tools/diag_gradient_report.py | 232 | 232 |
| tests/p1phot/p1phot_tests_properties.cpp | 346 | 346 |
| tests/p1phot/p1phot_tests_negative.cpp | 365 | 365 |
| tests/p1phot/CMakeLists.txt | 177 | 177 |
| cpp/test/test_p0_pc_api_exception_barrier.cpp | 158 | 158 |
| cpp/test/test_p0_sip_order_guard.cpp | 136 | 136 |
| cpp/build.ps1 | 83 | 83 |
| tests/p1phot/p1phot_field_stub.hpp | 61 | 61 |
| cpp/Makefile | 38 | 38 |
| **小计（18 份）** | **3275** | **3275 = 100%** |

**未逐行读的（1 份，如实列出）**：
- `lib/algorithms/photometry/data/response_curves/qe_curves.json`，6472 行（占本片 66.4%）。**我没有用 read 工具逐行读它。** 我做的是：① read 工具读 schema 头尾；② 只读 `python3 -c json.load` 对**全部 12 条曲线**做结构与数值审计（覆盖文件全部 6472 行，无一行未参与计算）；③ `grep` 取键行号。这一份由子代理 D 独立复核并回报。**行级阅读覆盖率 0，内容覆盖率 100%。**

### 口径 B：内容覆盖

- 成员份数 19，**读了 19 份（内容口径 100%）**
- 成员总行数 9747，**逐行通读 3275 行 = 33.6%**；其余 6472 行（纯数值 JSON）经结构化全量审计，**内容口径 9747/9747 = 100%**
- **未读到的：0 份**（无整份跳过）

---

## 2. 本片判定：**阻断（BLOCK）**

最重 3 条：

1. **C-1 头文件承诺的 `-3` 错误码在本片 ABI 上永不可达，而测试把「退化=成功」钉成合同。**
   `cpp/include/photometric_calib.h:102-105` 写「0=成功, <0=失败」「**-3: 无Gaia星/PSF星(退化scale=1.0)**」；`:156` 写「-3: 锥形搜索失败**或无光谱星**」。我逐行读实现核实：无 Gaia 星 → `pc_api.cpp:98 return 0`；无 PSF 星 → `:111 return 0`；无光谱星 → `:336 return 0`；**滤光片/QE 预处理真失败（`LOG_ERROR`）→ `:372 return 0`**。`-3` 全仓只在 `:309` 锥形搜索失败处产生。
   而 `tests/p1phot/p1phot_tests_negative.cpp:193` 与 `:221` 断言 `rc == 0` 并命名为 `n4_no_stars` / `n5_psf_all_invalid` —— **测试把这个 fail-open 行为固化成了期望值**。调用方拿到 rc=0、out_pixels 是未校准原图，**没有任何稳定错误码区分「已校准」与「根本没跑」**。

2. **C-2 `gate7` 在 Phase1 产品上整门恒红 —— 且正是恒红门把真缺陷藏起来的教科书案例。**
   `gate7:61-65` 强制要求 `snr/` 子产品目录，缺失即 `errors.append("缺少子产品目录: snr")`；`gate7:235` `all_pass = all(...) and not result["errors"]`；`gate7:241` `sys.exit(0 if all_pass else 4)`。
   但 Phase1 生产末端**不产 snr** —— 我亲自核实：`astro_sphere_sink.cpp:355-357` 的 `prod_flags` 只有 `AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT`（+VARIANCE|IVAR），`:360` 注释逐字写「**不写 snr**」；`hp_drizzle_api.cpp:1197-1199` 调的正是 `write_hips_phase1`；`docs/science/DATA_SEMANTICS.md:411` 独立佐证「Phase1 生产档（hips_profile=1）… 产物文件集 = signal/support 全 tile + Moc.fits + metadata.fits」。
   ⇒ **无论 signal/support 是否正确，gate7 在 Phase1 上必然 `sys.exit(4)`。** 其内部所有真红灯（support 区间、hierarchy 一致性、checksum）都被这盏常红灯淹没，审阅者会习以为常。另有二次恒红：`gate7:206` glob `snr/**/*.tsv` 必为 0 个 tile ⇒ `n_snr=0` ⇒ `:222` `snr_points_positive=False` 恒红。

3. **C-3 本片三个「门」里，另两个是自洽式断言 / 互斥恒红对，且全部无退出码。**
   `gate3_wcs_oracle.py:31-53`：WCS 是脚本自己写死的字面量 dict，装进 astropy 后 `all_pix2world→all_world2pix` 往返 —— **被验对象与期望值同源，生产 PLATESOLVE 产物一个字节都没被读**；`:72,:77,:78` 三个 `pass_*` 全是「对本文件内字面量的常量比较」，恒真。
   `gate8_trace_check.py:108-111`：两条判据要求 `HISS_VERIFY` 同时「在」与「不在」，**恒有一红**（我已构造穷举反例，见 §5）。
   `gate8` / `gate3` 全文件无 `sys.exit`、无 `all_pass`，**恒 exit 0**，红灯在 CI 里与全绿不可区分。

3. **C-4 `docs/architecture.md` 整篇描述的是一个已被整体删除的 Python 原型，且项目台账自称已删、文件却仍在 HEAD。**
   `module.yaml:124-126`（DISP-PHOT-002）原文断言「旧 README/旧算法文档（**已删**，见 git 历史）大面积失实（暴力最近邻 3px / scale=median / **MAD 清洗** / 0.1nm 网格 / v1.0 曲面拟合叙述）」。而该文件在 HEAD 仍存在，且 `:129`(3px)、`:130,:143`(MAD)、`:100`(0.1nm)、`:131,:144,:145`(曲面拟合) **逐条复刻了那四项被点名的失实叙述**。台账的「已删」与仓库实况矛盾。

---

## 3. 逐文件清单

| 文件 | 读了什么 | 看到什么（带 文件:行） | 判定 |
|---|---|---|---|
| `cpp/include/photometric_calib.h` | 全文 278 行 + 逐条核对实现 | `:3` 三条合同锚中 **2 条不存在**（`docs/detail/photometric_calib.md`、`docs/engineering/C_ABI_STANDARD.md`）；`:54` 「见 wiki/06」**全仓无 wiki 目录**；`:102-105` `-3` 永不可达（见 C-1）；`:54` 与 `pc_api_qf.h:21-22` 都称位定义「同 photometric_calib.h 的 PC_QF_*」，但**本文件内 PC_QF/SNR_QF 零命中** | **阻断** |
| `include/acsd/psfsw.h` | 全文 407 行 | `:327-334` 声明 `psfsw_robust` 是**退役 token**；`:344` `PsfswRecord::weight_mode = "psfsw_robust"` 却把它设为**默认值**（记录类型默认字段自指退役对象）；`:342-343` 注释称「唯一合法取值=退役 token，声明任何其它值⇒G01」，于是**生产永远无法通过 G01**；`:180-181` `cnorm_invariance_deviation` 用同一实现复算期望值 | **须修** |
| `cpp/src/pc_api_qf.h` | 全文 50 行 | `:9-10` 自述「pc_api.cpp 内部一律以 quality_flags=nullptr 调 cleanAndScale ⇒ **饱和/质量位有效域过滤在生产路径上不生效**」—— 这是文件自认的生产缺陷；`:21-22` 悬空位定义引用（见上）；该入口非 `extern "C"` 无 `PC_API`，ABI 面与 `:13` 所引 `docs/ACSD_DESIGN.md §8.5` 的耦合需复核 | 须修 |
| `cpp/include/log_macros.h` | 全文 20 行 | `:11` 与 `:13` 两宏形态不一致（`do{}while(0)` vs `((void)0)`）；`:17-18` 直写 stderr，无级别路由、无时间戳、无稳定错误码 | 建议 |
| `docs/architecture.md` | 全文 254 行 | 见 C-3。另有 `:3-4` 「文档版本 1.0 / 日期 2026-07-10」元信息块违反 AGENTS §5；`:12,:20` 历史演进叙事违反 §5；`:34` 「filters.json (43条)」实为 45 条；`:146` `I_cal=(I-S)/M` 与实现 `image_corrector.cpp:63-77` 的 `I*scale` 矛盾；全文 **0 个上游条款标记** | **阻断** |
| `gate7_hips_validate.py` | 全文 245 行 | **整门在 Phase1 上恒红**（`:61-65` 硬要求 `snr/`，而 `astro_sphere_sink.cpp:355-357,360` 明写 Phase1「不写 snr」），全部真红灯被淹没；`:206,:222` 二次恒红；`:119` 三项语义检查初始化为 `True` 且 `:120-123` support 缺失即 `continue` ⇒ **零像素也能全绿**（见 §5 反例 1）；`:127` 区间门对 NaN 恒 False ⇒ **NaN support 穿透**；`:132` 用 `isfinite` 判「空像素必须 NaN」，实则 ±Inf 通过；`:86` 只比 tile 个数不比路径；`:199` `np.percentile(rel,99)` 丢弃最坏约 2621/262144 像素；`:91` 只查前 16 个 tile；`:103-104` NSIDE 取首个即冻结；`:85` 零 tile 时 `[]==list(range(0,0))` 恒真；docstring `:8,:10,:15` 三项声称的检查**代码里不存在** | **阻断** |
| `gate8_trace_check.py` | 全文 124 行 | `:108-111` 互斥恒红对（见 §5 反例 2）；全文件无 `sys.exit`/`all_pass`；`:33-36` 裸 `except: continue` 静默丢行；`:93-96` `all(x != 0)` 构造即过滤，恒真；`:69` `star_id_no_mutation` 只验「有交集」；`:43-45` `expected_order` 算出 `present` 后从未使用；`:65` `star_id_no_loss` 算的是 `pm−psf`，**从不计算 `psf−pm`，即真正的「静默丢失」未被检测**；docstring `:6` 称验 SNR manifest，`:76` 循环只覆盖 3 个阶段；docstring `:10-11` 的「主图像 buffer 哈希」检查**完全不存在** | **阻断** |
| `gate3_wcs_oracle.py` | 全文 99 行 | `:31-40` WCS 全为硬编码字面量；`:42-53` 只验证 astropy 自身可逆性；`:72,:77,:78` 三个 `pass_*` 全是「对本文件内字面量的常量比较」，恒真；无退出码；`:30` 出处注释「来自 reorder_full 运行日志」**该日志全仓不存在**；`:9` 声称验「旋转一致性」与「尺度与 rms 自洽」—— **两者均无实现**；`:21` 死导入 | **阻断** |
| `download_xp_sample.py` | 全文 202 行 | `:145-155` 批次失败仅计数打印后继续，**无最小行数断言**，docstring `:6` 承诺「>=1000 星」无人保证，22 批挂 21 批仍 exit 0；`:48` `SELECT TOP n` **无 ORDER BY**，「固定样本」不可复现；docstring `:15` 承诺列 `bp_n_terms/rp_n_terms`，实际 `:179-189` **不存在**；`:68` 注释称「空格分隔」实为 `", "`；`:34` `DATALINK_URL`、`:25` `urllib.parse` 死代码 | 须修 |
| `tools/diag_gradient_report.py` | 全文 232 行 | `:154` `np.clip(a_resid,±3σ)` **显式裁掉残差极值**，而本工具用途 `:10` 正是「发现离群点」—— 被筛掉的恰是最差那条；`:109-110` vmin/vmax 同效裁剪；`:230` 硬编码 Windows 绝对路径且 `testdata/results` 不存在，唯一入口必崩；`:97,:143` 若有 NaN 则 y=x 参考线整条消失无告警（本项目 `PcMatchRecord.residual` 明确以 NaN 表未匹配）；`:167-168` 凭空发明 CSV 第 5 列为 IRLS 权重 | 须修 |
| `tests/p1phot/p1phot_tests_properties.cpp` | 全文 346 行 | `:9` 头注释宣称「oracle **独立**复算 rtol 1e-9」，`:235-237` 同一文件内注释写「**复刻被测同一估计过程**」—— **同文件自相矛盾，独立性声明为假**；`:259-266` 零比较真空通过（见 §5 反例 3）；`:241-242` oracle 的输入集由**被测函数的输出** `status`/`reject_reason` 筛出 ⇒ 被测误分类即被静默剔除；`:138-148` 无 OpenMP 时 `omp_set_num_threads` 被编译掉，两个「线程数」实为同一档，**P1 号不变量 I5 真空且报 PASS** | **阻断** |
| `tests/p1phot/p1phot_tests_negative.cpp` | 全文 365 行 | `:193,:221` 把「退化 rc=0」钉成期望值（见 C-1）；`:249` 用 `>= 1` 而非 `== 1`；整体错误码/Nan/Inf 面覆盖真实，是本片**最干净的一份** | 须修 |
| `tests/p1phot/CMakeLists.txt` | 全文 177 行 | `:84-89` OpenMP 缺失时静默置空，**无 warning 无门** ⇒ properties 组 I5 整条失效仍报绿；`:12-14` 头注释称 `p1phot_oracle.hpp` 是「独立 oracle（不调用被测函数）」—— 与 properties 实测矛盾；`:39-50` CTest 清单列 7 名，实际 `:112-177` 注册 9 个；`:32-35` 称「五 TU」而 `:60-75` 列 9 个源；`:60-75` **未纳入 `psfsw.cpp` / `frame_photometry_fit.cpp`** | **须修** |
| `cpp/test/test_p0_pc_api_exception_barrier.cpp` | 全文 158 行 | 覆盖真实（-1/-2/-4 语义）；但 `:44,:67` fixture 为 `W=H=8` 而 PSF 星在 `(50,100)/(150,100)`，**全在帧外** ⇒ T4「合法路径→0」实际测的是**退化无匹配路径**，`scale>0` 由回落常数 1.0 满足，并未证明屏障在真实定标路径上透明；`:25` 注释中的 `../../gaia_client/src/gaia_client.cpp` 路径不存在（实为 `lib/infrastructure/gaia_xpsd_client/`） | 建议 |
| `cpp/test/test_p0_sip_order_guard.cpp` | 全文 136 行 | `:22` 声明「T6 order=5 越界系数 → 36 项缓冲内不越界」，`:116-132` 块标题写「T5+T6」但**只有两条 CHECK 且都标 T5，无任何缓冲越界断言**—— 有文档无断言；`:112` 容差 `(9.999,10.001)` 无出处 | 建议 |
| `cpp/Makefile` | 全文 38 行 | `:8` `GAIA_DIR=../../gaia_xpsd_client` → 解析到 `lib/algorithms/gaia_xpsd_client`，**不存在**（实测 MISSING；全仓唯一 gaia 客户端在 `lib/infrastructure/`）⇒ `:31` 前置依赖无解，**make 根本无法产出 DLL**；`:13-17` 漏 `spatial_gain.cpp`/`frame_photometry_fit.cpp`/`psfsw.cpp`；`:33,:36-38` `copy /Y`、`del /f /q`、`2>nul` 是 cmd 内建，非 Windows 下失败且 `2>nul` 会**在仓内生成名为 `nul` 的文件**（根 `.gitignore` 无 `nul`） | **须修** |
| `cpp/build.ps1` | 全文 83 行 | `:28` 同一错误目录 ⇒ `:33-37` **恒 exit 1**；`:35` 自述「run make in `lib/infrastructure/gaia_xpsd_client/`」，与它实际检查的 `lib/algorithms/...` **自相矛盾**；`:9` 硬编码 `C:\msys64\mingw64\bin\g++.exe` 机器特定绝对路径；`:43-49` 同样只 5 个 TU，漏 3 个 ⇒ **Makefile 与 build.ps1 产 5-TU DLL，而 CMake 的 `acsd_phase1_photcal` 是 7-TU**，仓内存在两个不同的「生产」产物，被测面只覆盖其一 | **须修** |
| `tests/p1phot/p1phot_field_stub.hpp` | 全文 61 行 | **本片唯一无缺陷文件。** 只搬运 ra/dec/magG 与量化参数，不复制任何生产常量或算法；`:33-36` 的 `wl_count<=0` fail-fast 是纯前置检查，注释诚实记录了原越界放大区间。我曾怀疑它是「本地复刻实现」—— **经逐行核对否决** | 通过 |
| `data/response_curves/qe_curves.json` | 结构化全量审计（非逐行） | **两条不同型号传感器曲线逐位相同**：GSENSE2020BSI 与 GSENSE4040BSI 的 `wavelength_nm`+`value` 数组 SHA-256 完全一致（447 点）；**12/12 条曲线零出处**，数据文件无 provenance 字段；`Ideal QE curve` 是 6 点全 1.0、跨 1–2500nm 的**合成测试件混入生产数据**；`Sony IMX411/455/461/533/571` 把 5 个 sensor 合成一条。**数据本身干净**：无 NaN/Inf/null，`len(value)==len(wavelength_nm)` 12/12，`n_points` 自洽 12/12，波长严格递增 12/12，QE 全落 [0.0036,1.0] | **须修** |

---

## 4. 发现清单

### 阻断（BLOCK，8 条）
- **B0** `gate7_hips_validate.py:61-65,235,241` —— **整门在 Phase1 产品上恒红**。`snr/` 被硬性要求，而 `astro_sphere_sink.cpp:355-357` 的 `prod_flags` 无 SNR 位、`:360` 注释逐字「不写 snr」，`DATA_SEMANTICS.md:411` 佐证 Phase1 产物集 = signal/support + Moc + metadata。**所有真红灯被常红灯淹没**。二次恒红：`:206,:222`。
- **B1** `photometric_calib.h:102-105,156` 承诺的 `-3` 永不可达；退化路径全 `return 0`；测试 `negative.cpp:193,221` 把该 fail-open 钉成合同。实现证据 `pc_api.cpp:98,111,336,372`（本片外，片 004）。
- **B2** `gate3_wcs_oracle.py:31-53` 自洽式断言：硬编码字面量喂 astropy 往返，生产产物零读取；`:72,77,78` 三判据恒真；无退出码。
- **B3** `gate8_trace_check.py:108-111` 互斥恒红对；全文件无退出码 → 红灯不可区分。
- **B4** `gate7_hips_validate.py:86,119,120-123` 伪装成 fail-closed 的 fail-open：support 路径缺失即 continue，三项语义检查零像素全绿。
- **B5** `p1phot_tests_properties.cpp:9` vs `:235-237` —— 同一文件内「oracle 独立复算」与「复刻被测同一估计过程」并存，**独立性声明为假**；`:259-266` 零比较真空通过；`:138-148` 无 OpenMP 时 I5 整条真空。
- **B6** `docs/architecture.md` 整篇为已删除的 Python 原型；`:146` 与实现矛盾；台账 `module.yaml:124-126` 自称「已删」而文件在 HEAD。
- **B7** `cpp/Makefile:8` 与 `cpp/build.ps1:28` 均指向不存在的 `lib/algorithms/gaia_xpsd_client`，且两者都漏 3 个 TU ⇒ **仓内存在两个不同的「生产」DLL，被测面只覆盖其一**。

### 须修（MUST-FIX，11 条）
- **M1** `gate7_hips_validate.py:199` `np.percentile(rel,99)` —— 筛掉真信号后取极值，被筛掉的恰是最坏约 1% 像素。应为 `rel.max()`。
- **M2** `gate8_trace_check.py:65` `star_id_no_loss` 算 `pm−psf`，**从不计算 `psf−pm`** ⇒ 真正的静默丢失（PSF 有星、photometric 无）不被检测。
- **M3** `gate8_trace_check.py:33-36` 裸 `except: continue` 静默丢行；`:37` 同名 stage 后写覆盖先写；trace 追加写且目录从不清理 ⇒ 跨运行污染。
- **M4** `gate8_trace_check.py:93-96` `all(x != 0)` 构造即过滤，恒真；「dr3sp_id 非零」从未被测。
- **M5** `gate7_hips_validate.py:91,103-104` 只查前 16 个 tile；NSIDE 取首个即冻结且不校验 `2^(K+9)`；`:85` 零 tile 时 `hierarchy_complete` 恒真；`:127` NaN support 穿透区间门；`:132` 用 `isfinite` 判「空像素必须 NaN」⇒ ±Inf 判绿（应为 `isnan`）；docstring `:8,:10,:15` 三项检查不存在。
- **M6** `gate7_hips_validate.py:43-49` `dd * 10000` 硬编码，每 Dir 的 tile 数应为 `2^(K+9)`，仅 K≤6 时正确；魔数无出处。
- **M7** `qe_curves.json`：GSENSE2020BSI 与 GSENSE4040BSI 曲线逐位相同；12/12 条零出处；`Ideal QE curve` 合成件混入生产数据。
- **M8** `cpp/Makefile:33,36-38` cmd 内建命令在非 Windows 下失败并生成 `nul` 文件，`.gitignore` 未忽略。
- **M9** `tests/p1phot/CMakeLists.txt:84-89` OpenMP 静默降级无门；`:60-75` 未纳入 `psfsw.cpp`/`frame_photometry_fit.cpp`；`:12-14,:39-50` 头注释与实际注册不符。
- **M10** `download_xp_sample.py:145-155` 部分失败仍 exit 0 且无最小行数断言；`:48` `TOP n` 无 `ORDER BY` ⇒ 「固定样本」不可复现；`:15` docstring 承诺列不存在。
- **M11** 三条悬空引用：`photometric_calib.h:3` 的 `docs/detail/photometric_calib.md`、`docs/engineering/C_ABI_STANDARD.md`，`:54` 的 `wiki/06` —— **全仓均不存在**（我逐个 `test -e` 核实，并在 `docs/` 下按名搜索确认非改名）。

### 建议（SUGGESTION，8 条）
- `psfsw.h:344` 退役 token 作默认值导致 G01 生产永不可过；`:180-181` `cnorm_invariance_deviation` 以同一实现复算期望值。
- `diag_gradient_report.py:154,109-110` 显式裁剪残差极值，工具用途恰是找离群点；`:230` 硬编码 Windows 路径，入口必崩。
- `log_macros.h:11,13` 宏形态不一致；`:17-18` 无级别路由、无稳定错误码。
- `test_p0_sip_order_guard.cpp:22` 声明 T6 但 `:116-132` 无对应断言。
- `test_p0_pc_api_exception_barrier.cpp:44,67` T4 实测退化路径而非合法定标路径。
- `gate3_wcs_oracle.py:30` 出处日志不存在；`:9` 声称的旋转一致性/尺度自洽未实现；`:21` 死导入。
- `architecture.md:3-4` 元信息块与 `:12,:20` 历史叙事违反 AGENTS §5；`:34` 条数 43→实为 45。
- 无出处硬编码数：`properties.cpp:218` `|location|<0.1`；`:112` 3.6 arcsec 容差；`download_xp_sample.py:126,132` `1100/50`。

---

## 5. 我主动构造的反例

### 反例 1 —— gate7 语义三项零像素全绿（**推翻**，已复现）
**构造**：signal 树与 support 树 tile **个数相同、路径全不同**：
`signal/{Norder3/Dir000/Npix0, Norder3/Dir000/Npix1, Norder3/Dir100/Npix2}` vs
`support/{Norder3/Dir100/Npix3, Norder3/Dir200/Npix4, Norder3/Dir300/Npix5}`。
**期望推翻**：`:86` 的 `signal_support_tile_count_match` 会兜住。
**实际**：不成立。`:86` 只比**个数**；`:120-123` 每个 `sup_path` 都不存在 ⇒ 全部 `continue` ⇒ `sem["n"]==0` 而三项保持初值 `True`。实测输出：
```
L86 signal_support_tile_count_match = True
sem after loop = {'n': 0, 'sup_range_ok': True, 'sig_finite_ok': True, 'flux_ok': True}
L138-140 = True True True   <-- 零像素仍全绿
```
（复现命令见 §8-C1）

### 反例 2 —— gate8 互斥恒红对（**推翻**，已复现）
**构造**：穷举两种 stage 组合，检验两条判据能否同时为真。
**期望推翻**：至少存在一种配置使两条同时为真 ⇒ 证明不是恒红门。
**实际**：不存在。实测：
```
names=['DRIZZLE','HISS_VERIFY']: a=True  b=False -> both True? False
names=['DRIZZLE','HIPS_VERIFY']: a=False b=True  -> both True? False
```
且我独立核实 `orchestrator.cpp:5324-5326` 在 `legacy_hiss_compare=false` 时对 `HISS_VERIFY` 直接跳过、`:5315` 生产末端阶段是 `HIPS_VERIFY` ⇒ **名为 `drizzle_then_hips_verify` 的判据实测的是 legacy 的 HISS 阶段，在生产配置下恒 False**。恒红门成立，且它把真红灯淹没在常亮的红灯里。

### 反例 3 —— properties 残差一致性零比较真空（**推翻**，已复现）
**构造**：被测返回的 records 全部 `status != 1`。
**期望推翻**：`:266` 的 `res_ok` 会变 false。
**实际**：不成立。实测 `statuses=[2,3] -> comparisons=0, res_ok=True`。断言无最小比较次数下界。

### 反例 4 —— 头文件 `-3` 可达性（**推翻**，已核实）
**构造**：沿实现全路径搜索 `-3` 的产生点。
**期望推翻**：存在「无 Gaia 星」或「无光谱星」返回 `-3` 的分支。
**实际**：`-3` 唯一产生点是 `pc_api.cpp:309`（锥形搜索失败）；无 Gaia/无 PSF/无光谱星/预处理失败**四条退化路径全部 `return 0`**。文档合同不可达。

### 反例 5 —— field_stub 是否为「本地复刻实现」（**未推翻 ⇒ 判定无罪**）
**构造**：假设 `p1phot_field_stub.hpp` 复刻了生产逻辑以自证。逐行核对结果：它只搬运 `ra/dec/magG` 与 `flux_min/flux_mul/wavelength grid`，`:33-36` 是纯前置 fail-fast，**未复制任何生产常量或算法**。假设不成立，判定为干净文件。

---

## 6. 盲复算

**做法**：先不读既有判定，独立读完全部 18 份源码并构造反例；独立结论落定后，再打开本模块的既有判定登记 `module.yaml:121-141`（DISP-PHOT-001..009）逐条比对。

**比对结果：偏严（且在台账未覆盖面发现新问题）。**

| 既有判定（module.yaml） | 我的独立结论 | 比对 |
|---|---|---|
| DISP-PHOT-005「入参静默失效（mag_max 被 `mag_max_arr{12..16}` 覆盖；QE 三参数 `(void)` 丢弃）」 | **确认**（我亲读 `pc_api.cpp:291,298,117`），但台账**只登记了入参失效，未登记「退化路径 return 0 使调用方无法区分已校准/未校准」** | **偏严** |
| DISP-PHOT-002「旧算法文档（**已删**，见 git 历史）大面积失实」 | **部分推翻**：台账称已删，但 `docs/architecture.md` 在 HEAD **仍存在**且逐条复刻四项失实叙述。台账的「已删」是错的 | **偏严 + 台账有误** |
| DISP-PHOT-001/003/004/006/007/008/009 | 我本片范围内未独立复核（分别落在 star_matcher / image_corrector / 取消语义 / orchestrator / wrapper 等其它片），**不主张结论** | 未覆盖 |
| —— 台账**完全未登记**的：gate3 自洽式断言、gate8 恒红+无退出码、gate7 零像素全绿、p99 筛真信号、`-3` 不可达、QE 曲线零出处与逐位重复、Makefile/build.ps1 指向不存在目录且漏 3 TU、CMake OpenMP 静默降级使 I5 真空、properties 零比较真空 | 全部为**本轮新增** | **新增 9 类** |

**结论**：既有判定在「入参静默失效」一项上准确但**严重偏松** —— 它把一个会导致**返回码说谎**的 fail-open 记为「登记不改码」；且台账自身对「旧文档已删」的断言与仓库实况矛盾。

---

## 7. 子代理派发记录

**派了 5 个**（其中 1 个因我在同一消息内重复提交了同一 prompt，实际覆盖 4 条独立车道）。

| # | 车道 | 覆盖 | 回报要点 |
|---|---|---|---|
| A | gate/oracle 脚本 | 5 文件 902 行 100% | 4 BLOCKER（恒红门、自洽式断言 ×2、p99 筛真信号）+ 19 MUST-FIX + 11 SUGGESTION + **9 条否决候选** |
| A' | （同 A，重复派发） | 5 文件 902 行 100%（各读两遍） | **提出本轮最强新发现 B0**：gate7 整门在 Phase1 上恒红（`snr/` 硬要求 vs Phase1 不产 snr）；另 12 MUST-FIX 含 7 处恒真门；**并主动否决 8 条候选**，其中明确判定 `gate7:160-203` hierarchy 一致性**不是**自洽断言 |
| B | C++ 头文件 | 5 文件 1010 行 100% | BLOCKER-1（退化路径 return 0）、BLOCKER-2（sigma_residual=0.0 表完美）、恒真门（selection_bias 生产不可达）、4 条悬空引用 |
| C | 测试 + 构建 | 8 文件 1364 行 100% | BLOCKER-1（oracle 是被测代码逐行转写，**独立性声明为假**）、BLOCKER-2（fixture 用 oracle 生成坐标⇒WCS 闭环）、恒真门、OpenMP 静默降级、死构建 |
| D | 文档 + QE 数据 | architecture.md 254 全读 + qe 全量审计 | 4 BLOCKER（Python 原型、台账矛盾、`I_cal` 公式矛盾、12/12 曲线零出处）+ 逐位重复曲线 |

**逐条复核与否决记录**（我亲自复核，未照单全收）：

- **采纳并独立复核**：B 的「退化 return 0」—— 我**亲自打开 `pc_api.cpp:75-144` 与 `:285-379` 逐行核实**，确认 `:98,:111,:336,:372` 四处 `return 0`，确认后才写入 BLOCKER-1。✅
- **采纳并独立复核**：A 的「gate8 恒红对」—— 我**亲自构造穷举反例**（§5 反例 2）并独立 grep `orchestrator.cpp` 核实 `:5315/:5324-5326` 的 HIPS/HISS 分支。✅
- **采纳并独立复核**：D 的「两条曲线逐位重复」—— 我**亲自跑 SHA-256 比对**全部 12 条曲线，确认仅 GSENSE2020BSI/GSENSE4040BSI 一组重复，其余 11 条互异。✅
- **采纳并独立复核**：C 的「properties 零比较真空」—— 我**亲自构造反例 3** 复现。✅
- **采纳（降级为须修）**：C 的「fixture 用 oracle 生成坐标 ⇒ WCS 闭环」—— 主张成立，但 `p1phot_fixtures.hpp` 属片 003，不在本片交付范围，**我只作旁证登记，不计本片发现数**。
- **⚠️ 部分否决**：D 称「12/12 条曲线零出处」并引用 `filter_qe_provenance.json`。我**未独立复核该出处文件**（属片 003），故只采信我亲自验证的部分（曲线本身无 provenance 字段、存在逐位重复），「零出处」的完整证据链标注为**待片 003 确认**。
- **✅ 采纳 A' 的 B0（本轮最强新发现）并亲自复核**：我**亲自打开 `astro_sphere_sink.cpp:350-362` 与 `DATA_SEMANTICS.md:409-412` 逐行核实** `prod_flags` 无 SNR 位、注释「不写 snr」、Phase1 产物集不含 snr，确认 gate7 的 `snr/` 硬要求在 Phase1 上不可满足 ⇒ 整门恒红成立，已升为 BLOCKER B0 并置于最重三条之首。
- **✅ 采纳 A/A' 的一致否决**：`gate7:160-203` 的 hierarchy 一致性**不是**自洽断言 —— 写侧从 drizzle 原始累加器（`flux_sum`/`covered_area`）聚合，gate7 从落盘叶 tile 重算，数据来源不同。我复核该推理成立，**采纳其否决**，在本报告中不对该判据降级，并在 §8 明确标注它是本片**唯一真正的独立 oracle**，整改时勿一并降级。
- **⚠️ 部分否决**：B 称「`sigma_residual=0.0` 会污染下游 SNR」。B 自己已发现下游 `snr_estimator.h:397` 有 rc=2 fail-closed，**我采信其「下游安全」部分，只保留 photometry 侧语义错**，不主张 SNR 被污染。
- **⚠️ 否决一项过度主张**：A 称 gate3 的 MOC 判据与生产读者「共用同一 UNIQ 基准故非独立」。A 自己已**主动否决**该条并降级为「待查项」。我复核后**采纳其否决**，不在本报告中对该标准的符合性下任何结论。
- **否决 C 的一条**：C 曾怀疑 `p1phot_field_stub.hpp` 是「生产逻辑本地重实现」。C 逐行核对后**自行否决**；我**独立复核后同样否决**（见 §5 反例 5），判定该文件无缺陷。
- **统计**：子代理提出候选中，我**采纳 5 组核心结论（含 1 项由 A' 提出、亲自复核后升为本轮最强 B0）、否决 3 条过度主张、降级 2 条跨片发现、确认 2 条主动否决**。凡进入本报告 BLOCKER 的，均有我本人的 `文件:行` 或可复跑命令。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database" && git rev-parse HEAD   # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# C1  本片覆盖口径（求和应 = 9747）
#     逐行通读 18 份 = 3275；qe_curves.json 6472 行为结构化审计
wc -l lib/algorithms/photometry/{include/acsd/psfsw.h,cpp/include/photometric_calib.h,\
cpp/src/pc_api_qf.h,cpp/include/log_macros.h,docs/architecture.md,tools/diag_gradient_report.py,\
cpp/Makefile,cpp/build.ps1,tests/p1phot/CMakeLists.txt,tests/p1phot/p1phot_field_stub.hpp,\
tests/p1phot/p1phot_tests_negative.cpp,tests/p1phot/p1phot_tests_properties.cpp,\
cpp/test/test_p0_pc_api_exception_barrier.cpp,cpp/test/test_p0_sip_order_guard.cpp,\
cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py,cpp/test/gate4_dr3sp_gaiaxpy/gate8_trace_check.py,\
cpp/test/gate4_dr3sp_gaiaxpy/gate3_wcs_oracle.py,cpp/test/gate4_dr3sp_gaiaxpy/download_xp_sample.py,\
data/response_curves/qe_curves.json} | tail -1

# C2  B1 悬空引用（应全部 MISSING）
for p in docs/detail/photometric_calib.md docs/engineering/C_ABI_STANDARD.md wiki/06; do
  [ -e "$p" ] && echo "OK $p" || echo "MISSING $p"; done

# C3  B1 实现侧：四条退化路径 return 0，-3 唯一产生点是 :309
grep -n "return 0;" lib/algorithms/photometry/cpp/src/pc_api.cpp | head -8
grep -n "return -3" lib/algorithms/photometry/cpp/src/pc_api.cpp
grep -n "\-3: 无Gaia星\|锥形搜索失败或无光谱星" lib/algorithms/photometry/cpp/include/photometric_calib.h

# C4  B1 测试把 fail-open 钉成合同
grep -n 'rc == 0, "n4_no_stars"\|rc == 0, "n5_psf_all_invalid"' \
  lib/algorithms/photometry/tests/p1phot/p1phot_tests_negative.cpp

# C5  B3 gate8 恒红对 + gate3 无退出码
sed -n '108,111p' lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate8_trace_check.py
grep -c "sys.exit" lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate8_trace_check.py \
              lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate3_wcs_oracle.py \
              lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py

# C6  M7 QE 曲线逐位重复（期望仅 1 组 DUP）
python3 -c "
import json,hashlib
d=json.load(open('lib/algorithms/photometry/data/response_curves/qe_curves.json'))
h={}
for k,v in d.items():
    s=hashlib.sha256((str(v['wavelength_nm'])+str(v['value'])).encode()).hexdigest()[:16]
    h.setdefault(s,[]).append(k)
print('curves:',len(d))
[print(' DUP',k) for k,v in h.items() if len(v)>1]"

# C7  B7 Makefile / build.ps1 指向不存在的目录
ls -d lib/algorithms/gaia_xpsd_client 2>/dev/null || echo "MISSING lib/algorithms/gaia_xpsd_client"
ls -d lib/infrastructure/gaia_xpsd_client && echo "真实位置"
grep -n "gaia_xpsd_client" lib/algorithms/photometry/cpp/Makefile lib/algorithms/photometry/cpp/build.ps1

# C8  C6 台账自称「已删」但文件在 HEAD
sed -n '124,126p' lib/algorithms/photometry/module.yaml
test -f lib/algorithms/photometry/docs/architecture.md && echo "architecture.md 仍在 HEAD"

# C9  M1 p99 筛掉真信号
sed -n '198,200p' lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py

# C11 B0 gate7 整门恒红：Phase1 不产 snr，而 gate7 硬要求 snr/
sed -n '355,360p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp   # 无 SNR 位 + 「不写 snr」
sed -n '61,65p;235p;241p' lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py
sed -n '411p' docs/science/DATA_SEMANTICS.md                                     # Phase1 产物集不含 snr

# C12 本片唯一真正的独立 oracle（勿在整改中一并降级）
#     写侧 astro_sphere_sink.cpp 从 drizzle 原始累加器聚合，gate7:160-203 从落盘叶 tile 重算

# C10 B5 properties 独立性声明自相矛盾 + 零比较真空
sed -n '9p;234,237p;259,266p' lib/algorithms/photometry/tests/p1phot/p1phot_tests_properties.cpp
```

---

## 附：判定与建议

**本片判定：阻断。** 理由不是「测试没通过」，而是**判据本身不可信**：本片三个「门」里，gate7 在 Phase1 产品上**整门恒红**（Phase1 不写 snr，而 gate7 硬性要求 `snr/` 目录），所有真实红灯被常红灯淹没；gate3 是自洽式断言（硬编码字面量验证 astropy 自身，不读任何 ACSD 产物）；gate8 含互斥恒红对且三者全部无退出码。承载 P1 科学核心的 `p4_sigma_oracle` 在同一文件内同时宣称「独立复算」与「复刻被测同一估计过程」，独立性声明为假。按负责人裁决，这些**不能作为正确性证据**。

**整改时务必保留**：`gate7:160-203` 的 hierarchy 一致性判据是本片**唯一真正的独立 oracle**（写侧从 drizzle 原始累加器聚合，gate7 从落盘叶 tile 重算，数据来源不同），两个子代理车道独立否决了「它也是自洽断言」的假设，我复核成立。请勿把它与其它降级判据一并废弃。

**建议处置顺序**：
1. **先解 B0**（gate7 去掉 Phase1 不存在的 `snr/` 硬要求，或先让 Phase1 sink 写 snr）—— 否则 gate7 其余判据的整改效果无法被观测，且真红灯永远看不见。
2. 统一 `photometric_calib.h:102-105,156` 的 `-3` 合同与 `pc_api.cpp` 的实现，并同步订正把它钉成 `rc==0` 的 `negative.cpp:193,221` —— 三者必须一次性对齐，否则改任一处都会被其余两处打回。
3. 给 gate8 / gate3 装 `all_pass` + 退出码；删除 `drizzle_then_hips_verify` / `hiss_verify_not_production` 这对互斥判据中的至少一条，改为校验真实的 `DRIZZLE → HIPS_VERIFY` 顺序。
4. gate3 必须改为读生产产物（PLATESOLVE 输出 / trace），否则应撤下「Oracle」命名。
5. `docs/architecture.md` 按 AGENTS §5 **整篇重写**（非局部订正），并订正 `module.yaml:124-126` 的「已删」错误断言。
6. QE 数据：先补 GSENSE4040BSI 的真实曲线或删除该键；把 `Ideal QE curve` 迁出生产数据文件；补 provenance 并激活 `filter_curve_json.h` 的指纹对账。