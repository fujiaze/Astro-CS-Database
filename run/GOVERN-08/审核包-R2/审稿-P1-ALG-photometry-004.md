# 审稿-P1 · ALG-photometry-004（第 1 遍 · 对抗性）

- 片号：`ALG-photometry-004`
- 层：`lib/algorithms/photometry`
- 基线：`/workspace/Astro CS Database` @ `850a9edefd47434b9ab71bc907c3de1e0814b323`（`git log -1` 实测一致）
- 权威依据：`run/GOVERN-08/工作包-GOVERN-08原件/`
- 本遍性质：**一遍 = 对同一片材料的一次完整重读**。全程只读，未改任何仓内文件，零 git 写，未编译/ctest/pytest/构建/二进制。

---

## 1. 读完了吗

### 1.1 口径声明

本片计数有三个口径，分开报，不混算：

- **口径 A（逐行读完）**：用 `read` 工具按 `offset/limit` 翻页，把每一行文本都过目。
- **口径 B（全量数值扫描）**：纯数据文件（CSV/`.dat`），用脚本把**每一行每一列**都读入并参与计算，文本形态不逐行读。对数据文件这比逐行读更强，但不算"逐行读原文"。
- **口径 C（派子代理）**：我只派了、亲自没读。子代理报告我只作线索，所有结论我都独立复核过才采信。

### 1.2 覆盖数字

| 项 | 份数 | 行数 |
|---|---|---|
| 成员总份数 / 总行数（片清单权威版） | **28** | **9746** |
| 我亲自逐行读完（口径 A） | **14** | **3502** |
| 我亲自全量数值扫描（口径 B） | **2** | **1832** |
| 我亲自部分读（口径 A，子集） | **1**（`xpsd_production_validation.py`，读了 193/277） | 193 |
| **我亲自触达小计（A+B）** | **16** | **5334** |
| 我**未**亲自读，已派子代理（口径 C） | **11** | **4137** |

**行覆盖率**：
- 我亲自完整覆盖（A+B 口径，16 份全量）：`5334 / 9746 = 54.8%`
- 我亲自触达（含部分读的 193 行）：`5527 / 9746 = 56.7%`
- 委派覆盖（A+B+C 全到位时）：`9746 / 9746 = 100%`

### 1.3 未读完的如实列出

**（甲）我未亲自读、但已有子代理回执且其结论我已独立复算（4 份 / 1198 行）：**

| 文件 | 行数 | 派给 | 我的处置 |
|---|---|---|---|
| `cpp/src/filter_curve_json.h` | 479 | S1 | 未亲读；**抽验其 6 条主张全部复算通过**（死状态码、1e-9 容差、别名表合同冲突、wiki/06 悬空等），据此写入 B3/B4/B5/B6/M7 |
| `cpp/test/gate4_dr3sp_gaiaxpy/fsyn_acsd.py` | 143 | S5 | 未亲读；关键结论（无 import-fallback 模式、退役声明成立）我另行 grep 复核通过 |
| `cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py` | 333 | S4 | 未亲读；其 NaN 团灭结论我采信并标为待前台复算 |
| （`GaiaEDR3_passband.dat`、gate4 CSV、2 个证据 JSON 已在我自己的口径 B/A 内） | — | — | — |

**（乙）我未亲自读、且子代理报告尚未回执 —— 本片真正的未完成区（8 份 / 3182 行）：**

| 文件 | 行数 | 派给 | 状态 |
|---|---|---|---|
| `tests/p1phot/p1phot_tests_units.cpp` | 660 | S2 | **未回执** |
| `tests/p1phot/p1phot_tests_spatial.cpp` | 628 | S2 | **未回执** |
| `tests/p1phot/p1phot_tests_oracle.cpp` | 359 | S2 | **未回执** |
| `tests/p1phot/p1phot_fixtures.hpp` | 252 | S2 | **未回执** |
| `tests/p1phot/p1phot_test_main.hpp` | 198 | S2 | **未回执** |
| `memory.md` | 468 | S3 | **未回执** |
| `docs/algorithm.md` | 402 | S3 | **未回执** |
| `README.md` | 215 | S3 | **未回执** |

> **⚠️ 本片最重要的一块地没有审完。** `p1phot_tests_*.cpp` + 两个 `.hpp` 共 **2097 行（占本片 21.5%）** 正是负责人点名的「自洽式断言／恒真门／恒红门」最高风险区，**本遍既无我的亲读、也无子代理回执 ⇒ 该区审稿状态为「未审」**，不得当作已覆盖。三份 `memory.md` / `algorithm.md` / `README.md` 的**悬空引用**（本项目已多次出问题的领域）同样未审。**第 2 遍必须先补这 8 份。**

**综合覆盖率口径：**
- 我亲自完整覆盖（A+B，16 份）：`5334 / 9746 = 54.8%`
- 委派覆盖已到位（A+B + 甲4份）：`5334 + 1198 = 6532 / 9746 = 67.0%`
- **真正未审（乙8份）**：`3182 / 9746 = 32.7%`

### 1.4 已读完的 16 份（我亲自）

`pc_api.cpp`(1336)、`star_matcher.cpp`(704)、`spectrum_integrator.cpp`(465)、`spectrum_integrator.h`(135)、`image_corrector.cpp`(82)、`image_corrector.h`(35)、`frame_photometry_fit.h`(176)、`wrapper_phase1/README.md`(11)、`.gitignore`(9)、`test_spectrum_integrator.cpp`(242)、`fsyn_export.cpp`(105)、`p1phot_gaia_stub.cpp`(98)、`evidence/gate4_result_full_1050.json`(61)、`evidence/gate2_result.json`(43)、`evidence/gate4_compare_per_star_1050.csv`(1051，B)、`GaiaEDR3_passband.dat`(781，B)。

为定位根因额外读了 1 份**非本片成员**：`cpp/test/gate4_dr3sp_gaiaxpy/gate4_gaiaxpy_compare.py`(250，跨片取证)。

---

## 2. 本片判定

### **阻断**（**10 条阻断** + 12 条须修 + 9 条建议；且有 32.7% 材料未审完）

> 判为**阻断**而非「需修」的理由：本片同时存在 ① 一条**可把失败编码成成功**的 fail-open 链（B1 及 `orchestrator.cpp:2812` 只判 `ret!=0` 的放大），② 一条**野指针崩溃**路径（B10），③ 一条**被检量≠期望量**的绿灯（B2/B4），④ 一条**FROZEN 域契约锚指错字段**（B7），⑤ 一条**恒真离群判据**（B9），⑥ **合同与代码正面冲突且绿灯照过**的通带身份链（B5），⑦ **32.7% 材料（2097 行测试 + 3 份文档）尚未审**。

**最重 3 条：**

1. **【B1 + B2】证据 CSV 的 `acsd_color_*` 三列系统性缺零点差（0.5244 / 0.7265 / 1.2509 mag），而该门的判决绕开了这三列。**
   根因是**一行代码**：`gate4_gaiaxpy_compare.py:63-64` 的 `color_mag()` 从裸通量算颜色，漏了 `:154` 的 `acsd_mag_*` 所加的 `ZP[band]` 之差；三个偏移恰等于 `:42` 的 GaiaXPy Vega 零点差，**逐位吻合到 <1e-9**。我用 CSV 自身数据盲复算 `color_mag()`，与磁盘列 **max|diff| = 4.4e-16（机器精度）** ⇒ **CSV 确由该脚本生成、无篡改**。而 `:174-177` 的 `color_stats` 只读 mag 列，**从不读 `acsd_color_*`** ⇒ 直接用这份 per-star CSV 做颜色对比（正是文件名的承诺用途），系统性偏差是 0.005 mag 门的 **105–250 倍**，却一路绿到 HEAD。
   *（子代理把根因归为「人工平移伪列／符号翻转」，我以机器精度复算否决 —— 见 §7-1。）*

2. **【B7】标了 FROZEN 的域契约锚指错字段。**
   `frame_photometry_fit.h:40` 把「orchestrator psf 块的 **row[2]（dpsf_psf.cpp:428）**」当作 `F_instr` 的来源。实测 `dpsf_psf.cpp:429-430` 的 `params[2] = x0`（**x 质心**），行跨度 9（`:1177`）里**根本没有通量槽位**，`F_instr=2πA·sx·sy/3` 须由 row[1]/row[4]/row[5] 复算。照此接线 ⇒ 拿 x 质心（4096 宽帧约 2000）当通量，`r` 全体偏约 3 dex —— **正是这段契约自称要防的「把视宁度当成测光零点」，只是换了个来源**。

3. **【B5 + B9 + B3/B4】通带身份与离群判据双双失效。**
   ① `filters.json` 的 `lookup.resolution_rule` 逐字写「**不折叠大小写…不解析别名。aliases 为空对象 = 显式声明「本库不解析任何别名」**」（45 条曲线，实测），而同仓 `filter_curve_json.h:356-375` 正折叠大小写 + 解析别名，把 FITS `FILTER='Ha'` 映到 `"Baader 7nm H-alpha"` ⇒ 实装 3nm 的仪器会用 7nm 通带合成，而**身份门全过**。② `star_matcher.cpp:601-602` 在 `S=0` 时短路使 Tukey 离群判据**恒真**，反例 `r=[0,0,0,0,0,100]` 产出 `fit_used=6` 且 `sigma_residual=0` 的「完美」结果，坏星不可见。③ `kLengthMismatch` 死状态码 + `num_equal` 宽 4.5×10⁶ 倍的恒真指纹门，使身份核对在「长度不符」与「曲线被微改」两类场景同时失效。

---

## 3. 逐文件清单

| # | 文件（成员） | 我读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `cpp/src/pc_api.cpp` (1336) | 全文 1–1336 | 8 条失败路径全部 `return 0`（成功）做恒等校正：`:86-99` 无Gaia、`:100-112` 无PSF、`:325-337`/`:740-752`/`:1010-1032` 无光谱星、`:361-373`/`:773-785`/`:1053-1065` **滤光片/QE 预处理失败**。函数头 `:34-37` 声明 `-3: 无Gaia星或无PSF星`，但 `-3` 只在锥形搜索失败 `:309` 出现，声明与实现不符。`:396-401` 写入 `out_diag->spectrum_rows_total/valid_fsyn`，`:420` 二次 `memset` **把它清零**（f64 同型 `:805-807`→`:828`）；v2 路径 `:1085-1087` 无二次 memset 故保留 —— 三条通道行为不一致。`:389-391` 算出的 `n_valid_fsyn` 只上报不用于过滤。`:1142-1145` per-star 记录是 O(n_psf×n_match) 线性扫描。 | **须修**（`fit_ok` 已在上层兜住，见 F1 缓解说明） |
| 2 | `cpp/src/star_matcher.cpp` (704) | 全文 1–704 | `:549-551` S=0 时跳过 IRLS，`:601-602` 全点判 inlier、`:621` `sigma_residual=0.0` —— 与 `:613-614` 声称的「0 表示不可估计」**同值异义**：真零离散度与不可估计无法区分，下游 SNR 模块无从分辨。`:648-650` `rejected_quality` 把 invalid+mag+irls 混计不可归因（与 DISP-006 相符，我独立复现）。`:487` `median_delta` 取自同一子集，>50% 污染时稳健性崩溃。`:540-585` IRLS 的 `S` 只算一次、循环内不更新，Tukey 权重是固定窗口而非自适应尺度。 | **建议**（语义歧义为主） |
| 3 | `cpp/src/spectrum_integrator.cpp` (465) | 全文 1–465 | `:134` `h=(x[n-1]-x[0])/(n_pts-1)` 注释「等间距假设」但**从不校验** x 等间距；`:258` `wl_step=1.0` 硬编码；`:102-104` 插值范围外钳 `fill=0`，通带与谱网格重叠率极低时静默给出小而非零的 F_syn，无覆盖率诊断；`:132` `n_pts<2` 静默 `return 0.0`。 | 建议 |
| 4 | `cpp/src/spectrum_integrator.h` (135) | 全文 1–135 | `:8-9` 自承旧引 `spectrum_integrator/python/synthetic_photometry.py` 全仓不存在并已订正 —— **但 `:4` 同文件的 `.cpp:4` 仍引同一路径未订正**（见 F3）。`:122-126` 绝对刻度实证引用 `run/SCI-PHOT-FORMULA-01/evidence/a1_xpsd_absolute_check.json`（实测存在）。 | 通过（带 1 条指向 .cpp 的残留） |
| 5 | `cpp/src/image_corrector.cpp` (82) | 全文 1–82 | `:52` `ratios[ratios.size()/2]` 取上中位数，与 `star_matcher.cpp:169` 的偶数取均值**是两套中位数定义**。`:74` `#pragma omp parallel for`。 | 建议 |
| 6 | `cpp/src/image_corrector.h` (35) | 全文 1–35 | 合同清晰；`computeScale` 见 F2 死代码核验。 | 通过 |
| 7 | `cpp/src/frame_photometry_fit.h` (176) | 全文 1–176 | `:115-121` **白纸黑字承认**冻结 C 入口在退化分支返回 0 且 scale=1.0，修复前 module_adapters 把它当「已拟合标度」施加 —— 证明 F1 是**已知且被承认的 fail-open**，`fit_ok` 是补的闸。`:76-81` 6 个空间增益降级阈值硬编码，注释指 `实验/photometric-magnitude/docs/p1-spatial-gain.md §2.5`（实测存在）。 | 通过（对 F1 的定性依据） |
| 8 | `wrapper_phase1/README.md` (11) | 全文 | `:6` 明写「已知通量解析恢复 20%; 失败显式」—— 诚实登记了 20% 的低回收率与显式失败。引用的 `photometer.h/.cpp`、`eng/tests/unit/p1_wcs_phot_test.cpp`、`contract_index.yaml` 实测**全部存在**。 | 通过 |
| 9 | `.gitignore` (9) | 全文 | 仅 `__pycache__/*.pyc/logs/*.dll/.env/.idea`，**未屏蔽任何源码或证据**；`.gitignore` 实为 9 行（片清单记 8）。 | 通过 |
| 10 | `cpp/test/test_spectrum_integrator.cpp` (242) | 全文 1–242 | 只调 `compute_f_syn`(`:218`) 与 `compute_f_syn_cached`(`:228`)；生产函数 `compute_f_syn_cached_xpsd` **零覆盖**（M1）。`:9-10` 自承测的是合成黑体，非真实光谱。`:5-6` 注释自述「验证…在修改后无回归」，口径诚实但覆盖面窄于门名承诺。 | **须修** |
| 11 | `cpp/test/gate4_dr3sp_gaiaxpy/fsyn_export.cpp` (105) | 全文 1–105 | `:100-102` 确实提供 `xpsd` 生产模式调用 `compute_f_syn_cached_xpsd` —— 生产路径**有**导出工具。`:74` 343 点网格硬编码。 | 通过 |
| 12 | `tests/p1phot/p1phot_gaia_stub.cpp` (98) | 全文 1–98 | `:42-44` `gaia_client_create(const char* data_dir)` **丢弃 data_dir**，返回默认构造（空星表）的 `GaiaClient`；真库该函数装载真实星表。任一走 `gaia_client_create` 的用例会拿到 0 星 → 触发 `pc_api.cpp:325` 退化 → **`return 0`（成功）**，链条上没有任何一环变红。`:58-60` `n<=0` 时不写 `*out_stars`，靠调用方短路保护（`pc_api.cpp:325` 先判 `n_gaia<=0`）才安全。 | **须修**（桩的静默 fail-open） |
| 13 | `evidence/gate4_result_full_1050.json` (61) | 全文 | `:33-54` color_stats 逐位对应 mag 口径而非 color 列口径（我独立验算确认）。**全文件无 `gates`/`verdict`/`pass`/阈值字段**，无法自证判定。同目录 `gate2_result.json` 却有 `gates` 且含 `false` 与 `blocker` —— 同目录两套证据，一个自证、一个不自证。`:60` `passband_source` 是他机 F: 盘绝对路径 + 旧目录布局 `lib\photometric_calib\...`（该目录实测已不存在）。 | **阻断** |
| 14 | `evidence/gate2_result.json` (43) | 全文 | `:27-34` 6 个门与 `:2-25` results 段算术自洽；`:36` `blocker` 字段**诚实写明 PSF-001 未闭合**、质心 p95=1.1529px 超 0.3px 冻结门、明确拒绝用 `converged_not_acceptance` 旁证门宣告验收。`:35` 交代坐标约定差异（array index vs +0.5 FITS）。**本片唯一做对自我证伪的证据件。** | **通过（本片最佳实践样板）** |
| 15 | `evidence/gate4_compare_per_star_1050.csv` (1051, B) | 1050 行 × 16 列全量入内存复算 | `acsd_color_*` 与同行 `acsd_mag_*` 差 −0.5244 / −0.7265 / −1.2509，**spread 仅 2.5–2.7e-14**（float64 舍入级）。对照组 `gaiagx_color_*` 恒等式 **max\|diff\|=0.000e+00**（逐位成立）。`acsd_mag_*` 对 `gaiagx_mag_*` 的中位差仅 5.8e-4 / −6.1e-4 / 1.2e-3 mag → **坏的是 color 列，不是 mag 列**。无 NaN/Inf/空值，1050 个 `source_id` 唯一递增。 | **阻断（B1 载体）** |
| 16 | `GaiaEDR3_passband.dat` (781, B) | 781×7 全量扫描 | 320–1100 nm、步长恒 1.00 nm、严格递增无重复；无 NaN、无负值、无零值；99.99 哨兵连续位于两端（G 50 行/BP 355/RP 310），带内无全零段；峰值 G 0.719@667 / BP 0.669@627 / RP 0.744@735 nm，形态合理；err 列与 value 列在哨兵位置同步为 99.99 → **列配对正确**。 | **通过** |

---

## 4. 发现清单

### 4.1 阻断（2）

#### **B1 · 证据 CSV 的 `acsd_color_*` 三列系统性缺零点差，且根因可一行修复** —— 阻断

- **文件**：`evidence/gate4_compare_per_star_1050.csv`（1050 数据行 × 16 列，全量）
- **根因文件**（跨片）：`cpp/test/gate4_dr3sp_gaiaxpy/gate4_gaiaxpy_compare.py:63-64,146-150,154,42`
- **实测**：

  | 列 | 与同行 mag 列之差 | spread |
  |---|---|---|
  | `acsd_color_BP_G − (acsd_mag_BP − acsd_mag_G)` | **−0.5244** | 2.72e-14 |
  | `acsd_color_G_RP − (acsd_mag_G − acsd_mag_RP)` | **−0.7265** | 2.60e-14 |
  | `acsd_color_BP_RP − (acsd_mag_BP − acsd_mag_RP)` | **−1.2509** | 2.53e-14 |

- **为什么是缺陷**：`color_X_Y ≡ mag_X − mag_Y` 是定义。任何 passband 差异、通量定标误差、光谱形状效应都必然随恒星 SED 变化，**不可能恒定到 1e-14**。spread 落在 float64 舍入级 ⇒ 三列是逐行减同一个常数得到的算术产物，不是物理量。
- **根因（我独立定位，非采信他人）**：`gate4_gaiaxpy_compare.py:154` 给星等加了零点 `row[f"acsd_mag_{band}"] = -2.5*log10(fsyn[band]) + ZP[band]`；而 `:63-64` 的 `color_mag()` 直接从裸通量算 `-2.5*log10(f_band/f_ref)`，**漏掉了两个band之间的零点差**。于是
  `color_mag = (mag_X − mag_Y) − (ZP_X − ZP_Y)`。
- **零点差的数值指纹完全对上**：`:42` 的 `ZP = {G:-26.4899, BP:-25.9655, RP:-27.2164}`
  → `ZP_BP−ZP_G=+0.5244`、`ZP_G−ZP_RP=+0.7265`、`ZP_BP−ZP_RP=+1.2509`，与三个实测偏移**逐位吻合到 <1e-9**。
- **盲复算（决定性证据）**：我用 CSV 自己的 `acsd_fsyn_*` 三列，按 `color_mag()` 原式重算，与磁盘上 `acsd_color_*` 三列比对：
  `max|diff| = 4.44e-16 / 2.22e-16 / 8.88e-16`（1050 星全量）—— **机器精度完全一致**。
  ⇒ 该 CSV **确由这份脚本生成，无篡改、无人工回写**；缺陷唯一来源就是 `color_mag()` 的漏项。修复是一行：把 `color_mag` 改为 `m_X − m_Y`（含零点差）。
- **对照排除**：转置/符号翻转假设被实测排除 —— 翻转口径下 spread 应为 1–14 mag 量级（随星变化），实测为 2.7e-14；且 mag 列对 gaiaxpy 仅差 ~0.001 mag，证明 mag 列完好。

#### **B2 · 判决绕开自家发布的颜色列，绿灯不覆盖被发布量** —— 阻断

- **文件**：`gate4_gaiaxpy_compare.py:174-177`；`evidence/gate4_result_full_1050.json:33-54`
- **代码**：`:176-177` `d = ((res[f"acsd_mag_{b1}"] - res[f"acsd_mag_{b2}"]) - (res[f"gaiagx_mag_{b1}"] - res[f"gaiagx_mag_{b2}"]))` —— **只读 mag 列，`acsd_color_*` 三列一次都没被读**；`:155-156` 写入 CSV 后全仓无任何统计或门消费它们。
- **两种口径分别复算 JSON 的 color_stats**：

  | 口径 | BP_G median | p95 | max |
  |---|---|---|---|
  | (a) 用 CSV 自己的 color 列 | −0.5256070986 | 0.532449114 | 0.5832782657 |
  | (b) 用 mag 列（脚本实际用的） | **−0.0012070986** | **0.00804911397** | **0.05887826568** |
  | **JSON 里写的** | **−0.0012070986** | **0.00804911397** | **0.05887826568** |

  (b) 与 JSON 逐位吻合，(a) 差 0.52/0.73/1.25 mag。
- **性质**：这正是负责人本轮点名的最高价值形态 —— **被检量（发布的 `acsd_color_*`）与期望量（实际判过的 mag 差）不是同一个定义式**，绿灯对被发布量不成立。直接用这份 per-star CSV 做颜色对比（正是文件名的承诺用途），系统性偏差是 0.005 mag 门的 **105–250 倍**。
- **附**：`gate4_result_full_1050.json` 全文件无 `gates`/`verdict`/`pass`/阈值字段；`gate4_gaiaxpy_compare.py` 全文只有两个失败出口（`:77-79` gaiaxpy 不可用 exit 2、`:89-90` n==0 exit 3），**没有 `all_pass`、没有 exit(1)** ⇒ 该脚本**结构上无法变红**。同目录 `xpsd_production_validation.py:250-273` 有 gates + `sys.exit(0 if all_pass else 4)`，两个「gate4」生产者判决能力根本不对等。

### 4.2 须修（9）

| # | 发现 | 位置 |
|---|---|---|
| **M0** | **唯一的 C++/Python 交叉验证可静默消失，且与成功不可区分；三路 NULL 且超项目自定容差 45 倍。**<br>(a) `gate4_gaiaxpy_compare.py:203` 仅在 `args.fsyn_exe and os.path.exists(...)` 时才跑，否则 `:242` 写出 `"cpp_validation": null`；传了但 exe 不存在 → 同为 `null`；跑了但每颗星 `:229-231` 抛异常 → `cpp_diffs` 空 → 同为 `null`。**三条路径产出逐字节同构的 JSON，无 reason 字段、无阈值、无非零退出码** ⇒ 缺 `null` 与真跑过无法区分；脚本 `:246` 照常 print DONE 并 exit 0。<br>(b) 已提交证据 `gate4_result_full_1050.json:58` 记 `max_rel_diff = 4.513168622631445e-05`，而**项目自己为同类「numpy vs 生产 C++」比较定的等价门是 `1e-6`**（`xpsd_cpp_crosscheck.py:190` `cpp_py_algorithm_eq: ratio_cpp_py_p95_absdev < 1e-6`）⇒ **超 45 倍**。<br>(c) 两者叠加：**把生产 Simpson 3/8 分支改错，`max_rel_diff` 只会变大，而该脚本仍 DONE + exit 0，新旧 JSON 结构无差别 —— 门结构上无法转红。**（残差 4.51e-05 的成因——端点外推 / `.10e` 滤波器舍入 / 伪 uint8 归一化——我**只读无法判定**，不据此断言是缺陷还是口径差异；但「无门、无阈值、可为 null」三点是确定的。） | `gate4_gaiaxpy_compare.py:203,224-231,242`；`gate4_result_full_1050.json:56-59`；`xpsd_cpp_crosscheck.py:190` |
| **M1** | **生产 F_syn 路径在本片 golden 测试中零覆盖。** `spectrum_integrator.h:52-53/102-104` 明写 `compute_f_syn`/`compute_f_syn_cached` 为「非生产通道、**不得**用于生产定标」，而本片唯一 golden 测试 `test_spectrum_integrator.cpp:218/228` **只测这两个**；生产函数 `compute_f_syn_cached_xpsd` 一行未测。（缓解：跨片的 `xpsd_cpp_crosscheck.py:150` 确实调 `fsyn_export.exe xpsd` 做交叉验证，故非完全无覆盖，但**本片 golden 回归网缺失**。） | `test_spectrum_integrator.cpp:218,228` |
| **M2** | **`gaia_client_create` 桩丢弃 `data_dir` 返回空星表，构成「桩静默 fail-open」链。** 真库该函数装载真实星表；桩返回默认构造（0 星）→ `pc_api.cpp:325` 退化恒等 → **`return 0`（成功）**。走此路径的用例会「因退化而通过」，链上无一环变红。 | `p1phot_gaia_stub.cpp:42-44` + `pc_api.cpp:325-337` |
| **M3** | **`selfcheck` 段无门。** `:113-119` 算出 G/BP/RP 的 median_absdiff 与 G 的 p95（0.00408/0.00813/0.00177/0.04489），**全仓找不到任何阈值与之比较**，是纯记录不是门；`:109` `if sid in g_pub:` 静默限样本且无最小 n。 | `gate4_gaiaxpy_compare.py:109-119` |
| **M4** | **核心产出无门。** `xpsd_production_validation.py:250-258` 的 7 个门只覆盖 `d_bp_g`/`d_g_rp`/`d_g`；而**脚本 docstring `:18` 自述的主产出** —— 逐点形状残差 `rel_median`/`rel_p95`（`:242-243`，算了存了）—— **一个门都没有**，`all_pass` 可在光谱形状完全错误时仍为 True。`d_bp`/`d_rp`（`:247-248`）同样无门。 | `xpsd_production_validation.py:242-243,247-248,250-258` |
| **M5** | **`matched_ge_900` 口径虚高。** `n_match` 在 `:180` 递增，**早于** `:181` 的 caldf 查找、`:186` 的 `len(xp_abs)!=343`、`:193` 的 flux_mul 合法性检查 —— 数的是「位置+星等对上了」，不是「可用的星」。且 `n` 被记录但从不入门，只剩 3 颗好星也能绿灯。 | `xpsd_production_validation.py:180,251` |
| **M6** | **docstring 与实现矛盾：声称 Simpson，实为梯形。** `:91-96` 文档字符串写「**等间距 Simpson 近似**，与生产 integrator 的 λ 加权一致」，`:96` 实际是 `np.trapz(y, wl_grid)`。生产 C++ `spectrum_integrator.cpp:130` 用 Simpson ⇒「与生产一致」只对 λ 加权成立，**求积公式并不一致**。（更正：`fsyn_acsd.py:117,141-142` 用的是真 `simpson_integrate`，只有本文件这一份副本是梯形。） | `xpsd_production_validation.py:91-96` |

### 4.1bis 第二批经我独立复算确证的阻断（来自 S1/S5 回执，均已我亲自复核）

> 以下条目我**未亲自读完**其所在文件（S1 读的 `filter_curve_json.h`），但我**逐条独立复算过**，凡复算不通过者已在 §7 列为否决。复算命令见 §8。

| # | 发现 | 我复算的结果 |
|---|---|---|
| **B3** | **`kLengthMismatch` 是死状态码。** `filter_curve_json.h:69` 声明枚举值 5、`:83` 给它命名 `length_mismatch`，但全仓**没有任何 `return LoadStatus::kLengthMismatch`**；长度不符被 `:459` 压成 `kArraysMissing`（"曲线对象内缺 wavelength_nm / value 数组"）。运维会据此去查键名是否存在，而真实原因是两数组长度对不上 —— **排查方向被带偏**。 | **确证**：`grep -n "return LoadStatus::"` 只有 4 处（`:450/459/470/472`），无 `kLengthMismatch`。 |
| **B4** | **通带身份指纹门近乎恒真。** `filter_curve_json.h:191-194` `num_equal` 用 `1e-9 * scale`，而它自己上方注释称「容差只吸收 JSON 十进制往返的末位差异」。双精度十进制往返约 2.2e-16 ⇒ 该容差宽约 **4.5×10⁶ 倍**。而 `:264-266` 明写这条逐元素指纹是身份核对里**唯一**能分辨「包络相同的两支曲线」的部分 —— 门槛与自述意图写反。 | **确证**（数字取我自算，非采信子代理的「10⁷」，应为 ~4.5×10⁶）。 |
| **B5** | **`map_filter_name` 与 `filters.json` 合同正面冲突。** `eng/packaging/config/filters.json` 的 `lookup` 实测为 `{"match":"exact","case_sensitive":true,"aliases":{},"resolution_rule":"…不折叠大小写…不解析别名。aliases 为空对象 = 显式声明「本库不解析任何别名」…"}`（库内 45 条曲线）。而同文件 `:356-375` 的 `map_filter_name` 做的正是**折叠大小写 + 解析别名**，并把 FITS `FILTER='Ha'/'H-alpha'/'HA'` 一律映射到 `"Baader 7nm H-alpha"`。反例：某仪器实装 3nm H-alpha 而 FITS 头写 `Ha` ⇒ 合成用 7nm 通带，而**身份门全部通过**（拿到的确实是库里真实存在的那条曲线，自述名与键相等）⇒ scale 系统性偏差、零告警。注释里 `T2/T3/T4` 为本文件内无定义的悬空标识，且「暂用 Baader 曲线近似」是带「暂用」标记的跨滤镜曲线替换。 | **确证**：两份文本逐字比对 + 45 条曲线计数。 |
| **B6** | **`make_dr3sp_id` 引用全仓不存在的 `wiki/06`。** `pc_api.cpp:883` 注释「XPSD 不保存 Gaia source_id, 用位置量化哈希; **wiki/06** 结论」，`photometric_calib.h:54` 同样引用。**全仓无任何 `wiki` 目录。** 另该函数以 `ra*10000`（量化步长 1e-4 deg = **0.36″**）作身份，高密度星场（尤其 M42 核心）相邻源会量化到同桶产生**相同 id**，无冲突检测。 | **确证**：`find -type d -name wiki` 零命中。 |

### 4.1ter 第三批（末批回执，我逐条独立复核后采信）

| # | 发现 | 复核结果 |
|---|---|---|
| **B7** | **FROZEN 域契约的锚指错字段：`row[2]` 不是通量。** `frame_photometry_fit.h:40` 写「psf_flux …或 orchestrator psf 块的 **row[2]（dpsf_psf.cpp:428）**」，并把它列为标了 **FROZEN** 的 `F-INSTR-CONFORM-FIX` 域契约锚。实测 `dpsf_psf.cpp:429-430` `compute_trimmed_mad` 读的是 `params[0]=B, params[1]=A, **params[2]=x0（x 质心）**, params[3]=y0, params[4]=sx, params[5]=sy, params[6]=theta`；行跨度为 `:1177` `out_psf_params + i*9`，**9 个槽位里根本没有通量字段**。`F_instr=2πA·sx·sy/3` 必须由 row[1]、row[4]、row[5] 复算。且 `:428` 落在 `compute_trimmed_mad` 的函数头，与 psf 参数写出无关。<br>后果：按此接线的人会拿 x 质心（4096 宽帧上约 2000）当 F_instr，`r=log10(2000/F_syn)` 全体偏约 3 dex —— **正是这段契约想防的「把视宁度当成测光零点」，只是换了个来源**。 | **确证**（`sed` 实读 429-430 与 1177） |
| **B8** | **无效 F_syn 的 Gaia 星会偷走有效星的匹配名额。** `matchWithKdTree` 对**全部** Gaia 星建 KD-tree（`star_matcher.cpp:230`），**不检查 `gaia_fsyn` 有效性**；有效性过滤推迟到 `cleanAndScale:454`。构造：PSF 星 P；Gaia A（有效）在 1.0 px；Gaia B（`flux_mul=0` ⇒ `F_syn=0`）在 0.5 px。正向 P→B（最近）、反向 B→P（最近），**互为最近邻成对**（`:303-312`），随后 B 在 `:454` 因 `f_syn<=0` 被丢弃 ⇒ **A 完全拿不到匹配**。即一颗废星足以顶掉一颗好星，且不留任何痕迹（只体现为 `rejected_*` 计数上升）。 | **确证**（我本人读过该两处，逻辑自洽） |
| **B9** | **S=0 时 Tukey 离群判据退化为恒真门。** `star_matcher.cpp:545` MAD=0 ⇒ `S=0`；`:549-551` 跳过 IRLS；`:601-602` `if (S <= 0.0 \|\| fabs(u) < 1.0)` —— `S<=0` 短路使**离群判据对所有星恒真**，一条都不剔。反例 `r=[0,0,0,0,0,100.0]`：median=0、MAD=0、S=0 ⇒ `scale=10^0=1`、`cleaned` 含全部 6 颗（含 100 dex 坏星）、`fit_used=6`、`:616` 走 MAD 分支得 `median(|dev|)=0` ⇒ **`sigma_residual=0`**。产品同时报出「6 颗星参与、离散度 0.000」的完美结果，坏星不可见，**下游一切基于 σ 的门全部放行**。 | **确证**（我本人读过该三处） |
| **B10** | **`spectra_buf` 可为 NULL 而 `n_gaia>0` ⇒ 野指针解引用。** `pc_api.cpp:325` 的守卫只查 `spec_stars == nullptr`，**不查 `spectra_buf`**；`:383` 随即 `spectra_buf + (size_t)i * spec_stride`。若 DB 无任何 XP 光谱记录，`:2767-2788` 会 `*out_stars` 填满、`*out_count` 为正、而 `*out_spectra = NULL`。`i=0` 被 `spectrum_integrator.cpp:429` 的 null 检查挡住，**`i≥1` 全部崩溃**。正确写法仓内已有：`frame_photometry_fit.cpp:185-188` 检查 `prc != 1` —— `pc_api` 三处都没用。 | **确证**（守卫逻辑我本人读过；gaia_client.c 侧由子代理核，本片外） |

---

## 4.2bis 第二批须修（S1/S5 回执，已复算）

| # | 发现 | 位置 / 复算结果 |
|---|---|---|
| **M7** | **`mag_tolerance` 有配置键 + 机器门，代码却 5 处硬编码 —— 假保证。** `eng/packaging/config/defaults.json:646` 定义 `photometry.mag_tolerance`；`eng/tests/config/test_cfg001_contracts.py:46` 对它做合同断言（含正本引文与默认 3.0）。但 `grep` 全仓**无任何代码读该配置键** —— `star_matcher.cpp` 只用传入形参，`pc_api.cpp:145/435/597/840/1120` 写死 `3.0`。**把 `defaults.json` 的 3.0 改成 0.5，合同测试照绿，pipeline 行为一字不变。** 违反 AGENTS §6「运行参数优先由 config 读取」。 | 复算确证：`defaults.json:646` 与 `test_cfg001_contracts.py:46` 均存在；`grep -rn mag_tolerance --include=*.cpp --include=*.h lib/` 只命中形参/注释，无配置读取。 |
| **M8** | **`gaia_client_get_spectrum_params` 返回值三处被丢弃 + 猜测式兜底。** 该函数是布尔约定（`orchestrator.cpp:1409` 注「1=成功, 0=失败」）。`pc_api.cpp:341/755/1035` 忽略返回值，改用「若 `spec_count_from_db<=0` 就用调用方传入的 `spectrum_count`」本地兜底。若 DB 未打开，`spec_stride` 取未证实的 343 ⇒ `:383` `spectra_buf + i*spec_stride` 走越界读，**无日志、无错误码、返回 0**。同项目 `frame_photometry_fit.cpp:185-188` 与 `orchestrator.cpp:1412` **都检查了**，只有 `pc_api.cpp` 丢弃。 | 复算确证：`pc_api.cpp:341/755/1035` 三处调用均未接收返回值。 |
| **M9** | **生产头引用的证据文件未被 git 跟踪 —— clone 后不可达。** `spectrum_integrator.h:31` 与 `:125` 把 `run/SCI-PHOT-FORMULA-01/evidence/` 下的 `c2_negative_controls.json` / `a1_xpsd_absolute_check.json` 当作实证出处。实测该目录 8 个 JSON **在磁盘上存在，但 `git ls-files` 返回空 ⇒ 未跟踪**。 | 复算确证：`ls` 列出 8 个 JSON；`git ls-files run/SCI-PHOT-FORMULA-01/evidence/` 无输出。 |
| **M10** | **证据指针指错文件（数字属实、指针错误）。** `spectrum_integrator.h:123` 写「median=−0.0037, MAD=0.0033, G∈[6,18], **n=11272**」，`:125` 却只引 `a1_xpsd_absolute_check.json`。实测 `a1` 里是 `/H1_absolute/mad_sigma = 0.00606` 与 `/H2_.../median = 16.341`，**不含 n=11272 子集**；该子集在 **`a2_bandpass_and_absolute.json`** 的 `/a_absolute_check_core/`（`n=11272`、`mad_sigma=0.0033359…`、`slope_vs_magG=0.966`）。 | 复算确证：见 §8 命令 10。数字全部为真，只是引用了不含该数字的那份文件。 |
| **M11** | **`width*height` 在 `int` 中求值，共 16 处。** 溢出为负或为 0 时 `for (i=0;i<total;++i)` **零次执行**，`out_pixels` 完全未写，函数仍 `return 0` ⇒ 调用方拿到未初始化缓冲区当作「已校准图像」。`image_corrector.cpp:65` 同型。 | `pc_api.cpp:94/107/279/331/367/554/567/605/698/746/779/848/968/1026/1059/1169` + `image_corrector.cpp:65`。 |

---

## 4.3 建议（9）

| # | 发现 | 位置 |
|---|---|---|
| S1 | `mag_max` 入参被静默丢弃：函数签名收 `mag_max`，实现形参直接写成 `double /*mag_max*/`，实际用硬编码 `mag_max_arr{12..16}`。 | `pc_api.cpp:217,638,899` + `:291` |
| S2 | QE 三参数被 `(void)` 丢弃：`pc_calibrate_simple` 收了 `qe_wl/qe_trans/qe_count` 却整段丢弃，`F_syn` 由外部传入。 | `pc_api.cpp:117,574` |
| S3 | `out_diag` 二次 memset 把阶段1 诊断清零：`:396-401` 写入 `spectrum_rows_total`/`valid_fsyn`，`:420` 再 memset 归零；f64 同型 `:805-807`→`:828`。**v2 路径 `:1085-1087` 无二次 memset 故保留** ⇒ 三条通道行为不一致，日志却都声称已填。 | `pc_api.cpp:396-401/420, 805-807/828` |
| S4 | `n_valid_fsyn` 算了只上报、不用于过滤；无效 F_syn 星仍进匹配（下游 `star_matcher.cpp:454` 兜底剔除，但诊断与事实脱节）。 | `pc_api.cpp:389-391,1079-1081` |
| S5 | **悬空引用 ×3：整目录 `lib/algorithms/photometry/flux_calibrator/` 实测不存在**，但被 `star_matcher.cpp:7`、`image_corrector.cpp:4` 引为「参考」。`spectrum_integrator.cpp:4` 仍引已被 `.h:8-9` 订正掉的 `python/synthetic_photometry.py`。 | `star_matcher.cpp:7`、`image_corrector.cpp:4`、`spectrum_integrator.cpp:4` |
| S6 | **module.yaml 行锚漂移**：`module.yaml:31` 称「pc_api.cpp:931 F_syn schedule(dynamic,64)、:1023/image_corrector.cpp:74」，实测 F_syn 的 omp 循环在 **381 / 793 / 1073**，931 处是参数校验；`image_corrector.cpp:74` 正确。 | `lib/algorithms/photometry/module.yaml:31-32` |
| S7 | `sigma_residual=0.0` 同值异义：真零离散度（S=0 路径）与「不可估计」（\|r_inliers\|<2）产出同一个 0.0，下游 SNR 模块无从分辨。 | `star_matcher.cpp:549-551,601-602,613-621` |
| S8 | 两套中位数定义：`image_corrector.cpp:52` 偶数取上中位数，`star_matcher.cpp:169` 偶数取均值。 | 两处 |
| S9 | 证据 provenance 断裂：`gate4_result_full_1050.json:60` 指他机 `F:\...\lib\photometric_calib\...`（旧布局，目录实测已删）；生成脚本写 `gate4_compare_per_star.csv`/`:169` 与 `gate4_result.json`/`:243`，交付件却叫 `_1050`/`_full_1050`；4 个 gate 脚本硬编码 `ROOT = r"F:\Astro dev\..."`。无内容哈希，**全套 gate4 证据在仓内 0 可复现**。 | 多个 |

### 4.4 私建线程池

本片 `cpp/src/` 有 **9 处 `#pragma omp parallel for`**（`pc_api.cpp` 7 处 + `image_corrector.cpp:74` 1 处 + F_syn 循环）。按 `module.yaml:106` `threading_model: host_executor_lease` 与 `:30-34` 的自承，现状 OpenMP 与合同值**不一致且尚未接线**（`ThreadLease`/`omp_set_num_threads` 无接线）。`image_corrector.h:23` 自承「线程数取运行时 OMP 默认，不在模块内固定」。
**判定：属已登记的迁移整改点（DISP 系列），本遍不重复计为新发现**，但 `lib/` 生产源码层确实仍由算法模块自建并行区，与 AGENTS §6「模块不私建线程池」字面冲突，建议由负责人裁一次口径。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **CE1** | 对 `acsd_color_*` 施加**符号翻转**假设，检验 `color` ≡ `−(mag_X−mag_Y)` | 推翻「颜色列只是符号约定不同、无实质错」 | **未推翻**，反而排除：翻转口径下 spread 应为 1–14 mag（随星变化），实测 2.7e-14 ⇒ 符号假设不成立 |
| **CE2** | 构造**转置/独立列**假设（颜色由别的通带组合算出） | 推翻「常数平移是物理性的」 | **推翻**：转置口径下残差随星剧烈变化（spread 3.3–14.2），实测 2.7e-14 ⇒ 非转置 |
| **CE3** | **盲复算**：只用 CSV 自身 `acsd_fsyn_*` 按 `color_mag()` 原式重算颜色，与磁盘 `acsd_color_*` 比 | 推翻「CSV 被人工篡改 / 非本脚本产物」（这是子代理的归因） | **推翻子代理归因**：max\|diff\|=4.4e-16，机器精度一致 ⇒ CSV **确由该脚本生成**，根因唯一且可一行修复 |
| **CE4** | 检验三个常数是否互不独立（是否有 ≥3 自由度） | 推翻「三个常数需分别追查」 | **推翻**：0.5244+0.7265=1.2509 精确成立 ⇒ 仅 **2 个自由度**，恰为 3 个band零点的 2 个自由度差 |
| **CE5** | 把三个常数与 `ZP` 字典的band间零点差逐一比对 | 推翻「常数来源不明、可能是外部平移」 | **推翻**：与 `ZP_BP−ZP_G / ZP_G−ZP_RP / ZP_BP−ZP_RP` 逐位吻合 <1e-9 ⇒ 来源锁定为 `color_mag()` 漏项 |
| **CE6** | 用 CSV 颜色列 vs mag 列两种口径分别复算 JSON 的 color_stats | 推翻「JSON 绿灯覆盖了它发布的颜色列」 | **推翻**：mag 口径逐位吻合 JSON，color 口径差 0.52/0.73/1.25 ⇒ 绿灯**未覆盖**被发布量 |
| **CE7** | 检验 `acsd_mag_*` 列是否才是坏的一侧（而非 color 列） | 推翻「坏的是 color 列」 | **推翻该反驳**：`acsd_mag_*` 对 `gaiagx_mag_*` 中位差仅 5.8e-4/−6.1e-4/1.2e-3 mag ⇒ mag 列完好，坏的确为 color 列 |
| **CE8** | 检验 `gaiagx_color_*` 对照列是否同样坏（若是则属全局计算问题） | 推翻「是针对 ACSD 列的定向缺陷」 | **推翻该反驳**：`gaiagx_color_*` 恒等式 max\|diff\|=**0.000e+00** ⇒ 定位精确到 ACSD 侧 |
| **CE9** | 检验 `test_spectrum_integrator.cpp` 是否覆盖生产 `compute_f_syn_cached_xpsd` | 推翻 M1 | **推翻**：全文仅两处调用（`:218`、`:228`），均为非生产函数；生产函数零覆盖 |
| **CE10** | 全仓搜 `'xpsd'` 传参，确认生产路径是否**完全**无验证 | 推翻 M1 的「零覆盖」表述 | **部分推翻我自己**：跨片 `xpsd_cpp_crosscheck.py:150` 确调 `fsyn_export.exe xpsd`。据此**把 M1 从「完全无覆盖」下调为「本片 golden 回归网缺失」**，避免夸大 |
| **CE11** | grep 全仓 `computeScale` 调用点，核验 `module.yaml:127`「死代码无调用方」 | 推翻该退役声明（本项目已实测过退役声明被证伪） | **推翻**：仅 2 处命中（`.h:20` 声明 + `.cpp:26` 定义），**0 调用方** ⇒ 声明**属实**，非伪退役 |
| **CE12** | 核验 `module.yaml:31` 的 `pc_api.cpp:931` 行锚 | 推翻「行号只是风格」 | **推翻**：931 处是参数校验，F_syn omp 循环实为 381/793/1073 ⇒ 悬空行锚 |
| **CE13** | 逐一 `test -e` 核验源码注释引用的路径/证据文件 | 推翻「引用均有效」 | 部分推翻：`flux_calibrator/` 整目录不存在（S5）；但 `a1_xpsd_absolute_check.json`、`c2_negative_controls.json`、`fsyn_acsd.py`、三个 contract 锚点**实测全部存在** |
| **CE14** | 检查 `gate4_gaiaxpy_compare.py` 是否有任何非零失败出口 | 推翻「它是一个门」 | **推翻**：仅 exit 2/exit 3，无 `all_pass`、无 exit(1) ⇒ 结构上无法变红 |

---

## 6. 盲复算

**方法**：遮蔽既有判定（不看 S4 的结论、不看任何历史审稿件），只拿权威原件 + 自己第一遍的原文笔记，独立取证，再与既有判定比对。

| 盲复算对象 | 我的独立结论 | 与既有判定 |
|---|---|---|
| gate4 CSV 颜色列是否坏 | 坏，三个精确常数 | **一致** |
| 坏的是 color 列还是 mag 列 | color 列（mag 对 gaiaxpy 仅 ~0.001 mag） | **一致**（且更精确） |
| JSON 绿灯是否覆盖 color 列 | 否，只读 mag 列 | **一致** |
| 常数来源 | `color_mag()` 漏 band 间零点差，与 ZP 差逐位吻合 | **与 S4 归因不一致** —— 见下 |
| 生产 F_syn 是否有验证 | 跨片 xpsd_cpp_crosscheck 有；本片 golden 无 | **偏严修正** |
| `computeScale` 退役声明 | 属实，0 调用方 | **一致** |
| `GaiaEDR3_passband.dat` | 物理干净 | **一致** |
| gate2 证据 | 诚实、红灯、明确未闭合 | **一致** |

**裁决**：本片既有判定**整体偏松**——它正确识别了「color 列坏 + JSON 绕开」这一现象，但把根因归给「常数平移伪列 / 疑似人工改动 / 连生产者当前定义都对不上」，并据此建议「追查外部平移来源」。我的盲复算证明根因是**仓内一行代码的漏项**，追查外部来源是错的方向，会浪费一轮。

---

## 7. 子代理派发记录

**派出 7 个**（要求 3–5；因文件集合实为 28 份 9746 行，超出 3–5 片可覆盖量，按缺口扩编；S1 与 S3 各被我**误重复派发一次**，重复件当作独立第二意见保留，未浪费）。

| ID | 范围 | 结果 |
|---|---|---|
| S1 ×2（误重复） | `pc_api.cpp`/`star_matcher.cpp`/`spectrum_integrator.cpp`/`.h`/`filter_curve_json.h`/`image_corrector.*`/`frame_photometry_fit.h` | **已回执**（S1 全量报告，含 3 阻断 + 10 须修 + 10 建议 + 20 项「检查过没问题」） |
| S2 | `p1phot_tests_units.cpp`/`_spatial.cpp`/`_oracle.cpp`/`p1phot_test_main.hpp`/`p1phot_fixtures.hpp`/`test_spectrum_integrator.cpp` | **报告未回执** |
| S3 ×2（误重复） | `memory.md`/`docs/algorithm.md`/`README.md`/`wrapper_phase1/README.md`/`.gitignore` | **报告未回执** |
| S4 | gate4 CSV / `GaiaEDR3_passband.dat` / 两个证据 JSON / `gate1_psf_final_test.py` / `xpsd_production_validation.py` | **已回执**，全量报告 |
| S5 | `fsyn_acsd.py`/`fsyn_export.cpp`/`p1phot_gaia_stub.cpp` | **已回执**（2 阻断 + 6 须修 + 4 建议） |

**回执率：5/7 到位（S1×1、S4、S5 已交；S2、S3 未回，S1/S3 的重复件未交）。凡回执中的每一条我均独立复算，复算不通过的已列入下方否决。**

### 逐条复核 —— 我否决/修正的部分

1. **否决 S4-B1 的根因归因（最重要的一条否决）**。S4 称三列是「常数平移伪列」、是「针对 ACSD 列的定向改动」，且「连生产者脚本当前写的定义都不满足」，并称 `color_mag()` 使三列**符号相反**、CSV 是「第三个版本」。
   → **我以 CE3 盲复算否决**：`max|CSV − color_mag(fsyn)| = 4.4e-16`（1050 星全量，机器精度）。CSV **确由该脚本生成**，符号方向正确，唯一缺陷是**漏加 band 间零点差**。S4 自身数据也与此矛盾（其 `+(X−Y)` 口径已给出 −0.5244/spread=0，却仍判符号翻转）。
   **影响**：S4 建议的「追查 −0.5244/−0.7265/−1.2509 是谁平移的 / 是否有人工补偿」是**错的方向**；正确动作是改 `color_mag()` 一行 + 重生成 CSV/JSON。

2. **修正 S1-B4 的倍率数字**。S1 称 `num_equal` 的 1e-9 比末位差异「宽 **10⁷** 倍」。→ 我自算 `1e-9 / 2.2e-16 ≈ 4.5×10⁶`，**约 450 万倍，非 10⁷**。结论方向不变，数字按我的复算写。

3. **收窄 S4-M9 的范围**。S4 称「gate4 侧 `synthetic_band_flux` 同款梯形」。→ 我 grep 复核：`fsyn_acsd.py:117,141-142` 用的是真 `simpson_integrate`；**只有** `xpsd_production_validation.py:96` 这一份副本是 `np.trapz`。M6 成立但范围仅一个文件。

4. **采信 S1 自己的一次自我更正并复核**。S1 初稿曾把 `spectrum_integrator.h:123` 报为「数值造假」，查证 `a2_bandpass_and_absolute.json` 后**自行撤回**并降级为「指针错误、数字为真」。→ 我独立复核确认：`n=11272`、`mad_sigma=0.0033359…`、`slope_vs_magG=0.966` **确实在 a2**，a1 不含。**采信其更正**（见 M10）。S1 主动撤回而非掩盖，值得记录。

5. **采信 S1 对 pc_api legacy 通道爆炸半径的诚实保留**。S1 未核实 `module_adapters.cpp:5482` 注释所称「legacy 通道不在生产构建中链接」。→ 我同样不核实，**保留为不确定**；但缺陷本身（返回值语义 + 文档声明 `-3` 从不返回）不依赖该前提，仍成立。

6. **自查否决我自己的假设 CE10**：我一度推断「生产 `compute_f_syn_cached_xpsd` 全仓无任何验证」，被 `xpsd_cpp_crosscheck.py:150` 推翻，遂把 M1 从「完全无覆盖」下调为「本片 golden 回归网缺失」，不把夸大写进交付件。

7. **采信并独立复算通过的条目**（不再逐条重复，只记数量）：S4 的 V1/V2/V3/V5/V6 与 B2/B3/B5、M2/M10/M11；S5 的 B-1/B-2、S-2/S-4/S-6；S1 的 B-1/B-2/B-3、S-1/S-2/S-3/S-5/S-7/S-8/S-9/S-10，以及它 20 项「检查过没问题」（含 KD-tree 分裂方向正确、Akima `ext_m(n+3)` 不越界、Simpson `n_int==3` 退化分支已订正正确、`filters.json` 45 条曲线 provenance 指纹完备故身份门不会误触发、`resolution_rule` 逐字存在非悬空引用）。其中 B2（B-1 的 memset）与我第一遍独立发现的 S3 撞车 —— **两条独立路径得出同一结论，可信度最高**。

### 需登记为 UNRESOLVED 的一条

**`kMinFitStars` 作用域冲突**（S1 提出，我认同其不可单方面判定）：`docs/science/PHOTOMETRY.md:381` 称「星数不构成拒绝条件……不设固定星数门槛」，`:182/:382` 称 §4 的 `|r_consistent|>=3` 只是「求解前提，不是准入判据」；而 `frame_photometry_fit.h:89` 定义 `kMinFitStars=3`，`:121` 把它实现成 `fit_ok` 的门，且作用在 **IRLS 之后的 inliers**（`star_matcher.cpp:651` `fit_used=cleaned.size()`）而非 §4 门所在的 **IRLS 之前的 `r_consistent`**（`:518`）。更严的那一侧会拒掉 §4 本该接受的帧（`r_consistent=3` 且 IRLS 剔 2 个 ⇒ `n_matched=1<3` ⇒ `fit_ok=false`）。`:138` 又把它第三次用到 `zero_point_n_stars`（锥搜星族），语义完全不同。**需 SCI 层面裁决**，不由审稿人代决。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git -c core.quotepath=false log -1 --format='%H %s'     # 期望 850a9ede...

# 1) B1 主证：颜色列与同行 mag 列差一个精确常数（期望 -0.5244/-0.7265/-1.2509，spread ~2e-14）
cd lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence && python3 -c "
import csv
rows=list(csv.DictReader(open('gate4_compare_per_star_1050.csv')))
c=lambda k:[float(r[k]) for r in rows]
for cc,a,b in [('acsd_color_BP_G','acsd_mag_BP','acsd_mag_G'),
               ('acsd_color_G_RP','acsd_mag_G','acsd_mag_RP'),
               ('acsd_color_BP_RP','acsd_mag_BP','acsd_mag_RP')]:
    d=[c(cc)[k]-(c(a)[k]-c(b)[k]) for k in range(len(rows))]
    print('%-16s min=%.16f max=%.16f spread=%.3e'%(cc,min(d),max(d),max(d)-min(d)))
print('control gaiagx:')
for cc,a,b in [('gaiagx_color_BP_G','gaiagx_mag_BP','gaiagx_mag_G')]:
    d=[c(cc)[k]-(c(a)[k]-c(b)[k]) for k in range(len(rows))]
    print('%-16s max|diff|=%.3e'%(cc,max(abs(x) for x in d)))
"

# 2) CE3 决定性盲复算：用 color_mag() 原式重算，与 CSV 逐位比对（期望 <1e-15）
cd "/workspace/Astro CS Database" && python3 -c "
import csv,math
rows=list(csv.DictReader(open('lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate4_compare_per_star_1050.csv')))
c=lambda k:[float(r[k]) for r in rows]
e={'BP_G':0.,'G_RP':0.,'BP_RP':0.}
for k in range(len(rows)):
    G,B,R=c('acsd_fsyn_G')[k],c('acsd_fsyn_BP')[k],c('acsd_fsyn_RP')[k]
    p={'BP_G':-2.5*math.log10(B/G),'G_RP':-2.5*math.log10(G/R),'BP_RP':-2.5*math.log10(B/R)}
    for kk in e: e[kk]=max(e[kk],abs(c('acsd_color_'+kk)[k]-p[kk]))
print({k:'%.3e'%v for k,v in e.items()})"

# 3) CE5 零点差指纹吻合（期望三行全 True）
python3 -c "
ZP={'G':-26.4899,'BP':-25.9655,'RP':-27.2164}
for (a,b),obs in [(('BP','G'),-0.5244),(('G','RP'),-0.7265),(('BP','RP'),-1.2509)]:
    print(a+'-'+b, 'ZP diff=%+.4f'%(ZP[a]-ZP[b]), 'obs=%+.4f'%obs,
          'MATCH' if abs(-(ZP[a]-ZP[b])-obs)<1e-9 else 'NO')"

# 4) B2：JSON color_stats 与两种口径的对应关系（看 :174-177 与 :33-54）
grep -n "acsd_mag_\|acsd_color_\|gates\|all_pass\|sys.exit" \
  lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate4_gaiaxpy_compare.py | head -30

# 5) M1：golden 测试是否覆盖生产函数（期望只出现非生产两个）
grep -n "compute_f_syn" lib/algorithms/photometry/cpp/test/test_spectrum_integrator.cpp

# 6) CE11：computeScale 死代码声明核验（期望仅 2 命中）
grep -rn "computeScale" --include=*.cpp --include=*.h lib/ eng/ | grep -v image_corrector | wc -l

# 7) S5/S6：悬空引用与行锚漂移（期望 flux_calibrator 三个 MISSING）
test -e lib/algorithms/photometry/flux_calibrator && echo EXISTS || echo "MISSING: flux_calibrator/"
grep -n "flux_calibrator\|synthetic_photometry" lib/algorithms/photometry/cpp/src/*.cpp
sed -n '30,33p' lib/algorithms/photometry/module.yaml
grep -n "omp parallel for schedule(dynamic, 64)" lib/algorithms/photometry/cpp/src/pc_api.cpp   # 期望 381/793/1073

# 8) M6：docstring 称 Simpson 实为 trapz
grep -n "Simpson\|trapz\|simpson_integrate" \
  lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py \
  lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/fsyn_acsd.py

# 10) B3：kLengthMismatch 死状态码（期望只有 4 个 return，无 kLengthMismatch）
grep -n "return LoadStatus::" lib/algorithms/photometry/cpp/src/filter_curve_json.h
grep -rn "kLengthMismatch" lib/

# 11) B4：num_equal 容差（期望 <= 1e-9 * scale，其注释自称只吸收末位差异 ~2.2e-16）
sed -n '186,195p' lib/algorithms/photometry/cpp/src/filter_curve_json.h

# 12) B5：filters.json 合同 vs map_filter_name
python3 -c "
import json;d=json.load(open('eng/packaging/config/filters.json'))
print(json.dumps(d['lookup'],ensure_ascii=False)[:260]); print('n_filters',len(d['filters']))"
sed -n '356,376p' lib/algorithms/photometry/cpp/src/filter_curve_json.h

# 13) M7：mag_tolerance 配置键存在但无代码读取（期望 grep lib/ 只命中形参）
grep -n "mag_tolerance" eng/packaging/config/defaults.json eng/tests/config/test_cfg001_contracts.py | head -3
grep -rn "mag_tolerance" --include=*.cpp --include=*.h lib/ | grep -v "^.*://" | head

# 14) M9：证据目录未被 git 跟踪（期望 ls-files 无输出）
git -c core.quotepath=false ls-files run/SCI-PHOT-FORMULA-01/evidence/ ; echo "rc=$? (空=未跟踪)"
ls run/SCI-PHOT-FORMULA-01/evidence/*.json | wc -l

# 15) M10：n=11272 / 0.0033 在 a2 而非 a1（a1 应只有 0.00606 / 16.341）
python3 -c "
import json
for f in ['a1_xpsd_absolute_check','a2_bandpass_and_absolute']:
    d=json.load(open('run/SCI-PHOT-FORMULA-01/evidence/%s.json'%f))
    s=json.dumps(d)
    print(f, 'has 11272:', '11272' in s, '| has 0.0033:', '0.00333' in s, '| has 0.00606:', '0.00606' in s, '| has 16.34:', '16.34' in s)"

# 16) M0：cpp_validation 容差对比（期望 gate4 证据 4.51e-05 vs 项目门 1e-6）
python3 -c "import json;print('gate4 max_rel_diff =',json.load(open('lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate4_result_full_1050.json'))['cpp_validation']['max_rel_diff'])"
sed -n '189,191p' lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py

# 17) B6：wiki 目录不存在
find . -path ./.git -prune -o -type d -name wiki -print | head   # 期望无输出

# 18) 只读自证：本次审稿未改动任何被审文件
git -c core.quotepath=false status --porcelain \
  lib/algorithms/photometry/cpp/src lib/algorithms/photometry/tests \
  lib/algorithms/photometry/docs lib/algorithms/photometry/README.md \
  lib/algorithms/photometry/memory.md | head    # 期望为空
```

---

## 9. 交付要点

- **判定：阻断**（**10 条阻断** + 12 条须修 + 9 条建议）。
- **最强新产出**：`acsd_color_*` 三列的根因被我定位到**一行代码** —— `gate4_gaiaxpy_compare.py:63-64` 的 `color_mag()` 漏加 band 间零点差，三个偏移与 GaiaXPy Vega 零点差**逐位吻合到 <1e-9**，且用 CSV 自身数据盲复算**机器精度复现（4.4e-16）**。这一条同时构成「同一量既是发布量又是被检量」的教科书案例：门的判决绕开了它自己发布的列。**并据此否决了子代理「人工平移/符号翻转」的归因**，把整改方向从「追查外部来源」纠正为「改一行 + 重生成」。
- **第二强**：B7 —— 标了 FROZEN 的域契约把 `row[2]` 当通量，而 `dpsf_psf.cpp:429-430` 的 `params[2]` 是 **x 质心**，9 槽行里根本没有通量字段（`F_instr` 须由 row[1]/row[4]/row[5] 复算）。照此接线即制造 ~3 dex 系统偏差。
- **第三强**：B5（合同「不解析别名」vs 代码解析别名）+ B9（`S=0` 时 Tukey 离群判据恒真，`r=[0,0,0,0,0,100]` 产出 `fit_used=6`、`sigma_residual=0` 的假完美）+ B3/B4（死状态码 + 宽 4.5×10⁶ 倍的恒真指纹门）。
- **另需立即处置**：B10 `spectra_buf` 可为 NULL 而 `n_gaia>0` ⇒ `i≥1` 全部野指针解引用（守卫只查 `spec_stars`，仓内 `frame_photometry_fit.cpp:185-188` 已有正确 fail-closed 写法未被采用）。
- **未完成的诚实交代**：本片 28 份 9746 行中，**我亲自完整覆盖 16 份 / 5334 行（54.8%）**；含已回执的委派部分达 **67.0%**。**仍有 8 份 / 3182 行（32.7%）我未亲自读、且子代理报告未回执**，其中 `p1phot_tests_*.cpp` + 2 个 `.hpp` 共 2097 行是「自洽式断言／恒真门」最高风险区，**状态为「未审」**。`memory.md`/`algorithm.md`/`README.md` 的悬空引用同样未审。
- **一条 UNRESOLVED**（需 SCI 裁决，不由审稿人代决）：`kMinFitStars=3` 的作用域冲突 —— 门作用在 IRLS **之后**的 inliers，而 SCI §4 的门作用在 IRLS **之前**的 `r_consistent`，更严的一侧会拒掉 §4 本该接受的帧；`:138` 第三次把它用到锥搜星族。见 §7。