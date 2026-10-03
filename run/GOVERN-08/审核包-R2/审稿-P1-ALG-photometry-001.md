# 审稿 G08-05 对抗审稿 P1 · ALG-photometry-001

- 片号：`ALG-photometry-001`
- 层：`lib/algorithms/photometry`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9ede`
- 本片判定：**阻断**
- 计数口径见第 1、9 节；所有中文路径均按 `git -c core.quotepath=false` 口径处理。

---

## 1. 读完了吗

| 项 | 数 | 口径 |
|---|---|---|
| 成员份数 | **1** | 片清单 `片清单-权威版.yaml:422` 声明 `成员份数: 1` |
| 读了几分 | **1** | 唯一成员已逐段读完，无跳读 |
| 成员总行数 | **14 079** | 片清单 `:424` 声明 `实际行数: 14079`；`wc -l` = 14079 |
| 实际读了多少行 | **14 079 + 末行** | `read` 工具报 `totalLines=14080`。差异是**文件末尾无换行符**：末行 `14080:` 为 `}`，无尾随 `\n`，故 `wc -l` 少计 1 |
| **覆盖率** | **100.00%（14080/14080 行全部经我本人 `read` 过）** | 分段：1-120、121-1370、1371-3070、3071-4770、4771-6520、6521-8320、8321-10120、10121-12120、12121-14080 |
| 未读完的成员 | **无** | — |

成员清单（`片清单-权威版.yaml:427-428`）仅一条：
`lib/algorithms/photometry/data/response_curves/filters.json`

身份校验：`md5 = b5d9f17d92185a8a9e36739d746fe9e1`，`sha256 = 76e919a0fd3745db1daa423e227ce4b7e5e4cfe1c5800e195ff0fed340cc5dcc`，`183763` 字节，45 个顶层条目。

> **口径声明**：除本片成员外，为判定本片数据的后果是否进入生产，我另读了消费方源码与门文件（`spectrum_integrator.cpp`、`filter_curve_json.h`、`filter_curve_json` 门、`architecture.md`）。这些**不计入本片覆盖率**，只作佐证。

---

## 2. 本片判定

**阻断。**

最重 3 条：

1. **【阻断】本片的 4 条 `channel:"L"` 数据是理想方波，经生产 Akima 重采样后产生**负透过率**，且该通路生产可达。** `filters.json:2059/2082/2106/2548`。`filter_curve_json.h:365-366` 把 FITS `FILTER ∈ {Lum, L, Luminance}` 映射到 `Baader UV/IR Cut / L CMOS Optimized`（= `:2548`），因此**每一帧亮度/光帧都命中**。在生产 XPSD 网格（336–1020 nm，步长 2，343 点）上实测 `T_min = −0.83333`，81/343 点为负，面积误差 **−29.54%**。`docs/science/PHOTOMETRY.md:51` 冻结 `T(λ)∈[0,1]`，`spectrum_integrator.cpp:281` 把负 T 直接乘进被积函数，无钳位。
2. **【阻断】两个不同产品共用同一条曲线，逐字节相同。** `Astrodon E-series B`（`:369-511`）与 `Astrodon I-series B`（`:816-958`）的 `wavelength_nm` **与** `value` 数组**逐字节相同**（各 143 行块中 141 行相同，仅键名与 `name` 值不同）。两个产品线不可能有相同的实测透过率。
3. **【须修】`architecture.md:34` 声称 `filters.json (43条)`，实为 45 条。** 生产文档中的计数失真，且被 sha256 冻结与 `MIN_EXPECTED_FILTERS = 45`（`test_cfg001_contracts.py:33`）掩盖。

---

## 3. 逐文件清单

### 唯一成员：`lib/algorithms/photometry/data/response_curves/filters.json`

| 读了什么 | 看到什么 | 判定 | 位置 |
|---|---|---|---|
| 全 45 条记录的 5 个字段 + 1 个异常字段 | 结构层**无瑕**：45/45 `n_points==len(wavelength_nm)==len(value)`；45/45 波长严格递增；值域全在 [0,1]；无 NaN/Inf、无重复键、无尾逗号、UTF-8 干净、`name` 全等于对象键 | 通过 | 全文件 |
| 曲线重复性 | `Astrodon E-series B` ≡ `Astrodon I-series B`，波长与值数组**逐字节相同** | **阻断** | `:369` / `:816` |
| `channel` 字段 | 7 条为空串：`Johnson I` `:4926`、`Johnson U` `:7416`、`SDSS g` `:10196`、`SDSS i` `:10831`、`SDSS r` `:11470`、`SDSS u` `:12077`、`SDSS z` `:12452`。恰是全部非 B/G/R 条目；而 `Johnson V` 却映射为 `"G"`（`:7869`），语义不一致 | 须修 | 见下 |
| `channel:"L"` 的 4 条 | 全部 7 点、非均匀栅格（步长 5–210 nm）、值恰为 `[0,0,1,1,1,0,0]`，峰值恰为 `1.000000`（高于其余 41 条的 ≤0.999） | **阻断** | `:2059`、`:2082`、`:2106`、`:2548` |
| `default` 字段 | 全文件唯一多余键，只出现在 `Astronomik UV-IR Block L-2`，值 `true`。全仓**零消费者**，且转录门**强制**丢弃它 | 须修 | `:2104` |
| 栅格均匀性 | 39 条均匀（2.0/1.0/0.5 nm），**6 条**非均匀：4 条 L 方波 + `Baader 7nm H-alpha` `:2571`（步长 0.7/1.0/1.3/2.0）+ `Baader 8.5nm OIII` `:2622`（0.3/0.7/1.0/2.0）。后两条**故意**在 656.3 nm / 500.7 nm 插入了 Hα / [O III] 静止波长 —— 这是物理正确的设计，不是缺陷 | 建议（记录） | — |
| 窄带曲线分辨率 | `Baader 7nm H-alpha` 与 `Baader 8.5nm OIII` 均为非均匀栅格，**经生产 Akima 后面积误差 −1.15% / 有限**，但对均匀步长假设极敏感 | 建议 | `:2571`、`:2622` |
| 谱覆盖 | 并集 [299.0, 1100.0] nm 连续无空洞；交集为空。**0/45** 条覆盖完整 Gaia XP 带 330–1050 nm；仅 `SDSS z` 超出 1050（至 1100）。8 条需在 XP 范围外外插 | 建议 | 全文件 |
| 截断峰 | `SDSS z` 在 1100 nm 处仍为 `0.995`（`:13657`），全跨度单调；真实 FWHM 在表内不可测 | 建议 | `:12450-13659` |
| 文件格式 | 末尾无换行符（`wc -l` 14079 vs 14080 行） | 建议 | `:14080` |

**零命中的检查项（阴性结果，同样重要）**：无静默降级（读文件失败走 `aio_file_io.h:41-53` fail-closed）；无自愈判据/锚（`transcription.source_sha256` 与实际文件**完全吻合**，`source_bytes` 183763 亦吻合）；无恒红门；无私建线程池（数据文件无代码）；无退役对象活跃调用者（`default` 是退役标记且确无消费者）。

---

## 4. 发现清单

### 阻断（3）

| # | 发现 | 证据 |
|---|---|---|
| B1 | **4 条 `L` 方波经生产 Akima 产生负透过率，且生产可达。** `Baader UV/IR Cut / L CMOS Optimized` 由 `FILTER=L/Lum/Luminance` 直接命中，XPSD 生产网格上 `T_min=−0.83333`、81/343 点为负、面积误差 −29.54%；L-3 `−0.62500`/−24.77%；L-1 `−1.50968`/−15.51%；L-2 `−0.50000`/−10.68%。负瓣落在 700–800 nm 红端 | `filters.json:2548` + `filter_curve_json.h:365-366` + `spectrum_integrator.cpp:46-125,347` + `docs/science/algorithms/PHOTOMETRIC_FIT.md:217` |
| B2 | **两个产品共用一条曲线。** `Astrodon E-series B` 与 `Astrodon I-series B` 的波长与值数组逐字节相同；全库 990 组配对中仅此 1 对归一化形状差为 `0.0` | `filters.json:369` / `:816` |
| B3 | **4 条 `L` 记录不是实测，是理想方波。** 值恰为 `[0,0,1,1,1,0,0]`，恰 2 个不同取值，峰值恰 `1.0`，零值区占其声明跨度的 27.4–40.0% 却只用每侧 2 个采样点。把 UV/IR 截止建模为**完美**拒带 —— 本文件自己另外两条真实宽带曲线（`IDAS LPS P3`、`OPTOLONG L-PRO`）在 700–750 nm 仍透 1.7–5.9%、峰值 14–50% | `filters.json:2059-2081,2082-2105,2106-2128,2548-2570` |

### 须修（6）

| # | 发现 | 证据 |
|---|---|---|
| M1 | 生产文档计数失真：`filters.json (43条)`，实为 45 | `lib/algorithms/photometry/docs/architecture.md:34` |
| M2 | 7 条 `channel:""`，而 `Johnson V` 却为 `"G"`；`channel` 在全部生产代码中**零消费者**，唯一读者是注册门 `check_cfg002_registry.py:584-589`。注册表自身把该列语义登记为 gap（`config_registry.json:2476`） | `filters.json:4926,7416,10196,10831,11470,12077,12452` vs `:7869` |
| M3 | `default:true` 是死标签，且转录门**强制**丢弃它 —— 门对唯一真正发散的字段结构性失明 | `filters.json:2104`；`test_cfg001_contracts.py:280,284-287` |
| M4 | `n_points` 校验**fail-open**：字段缺失或非数值时整个校验被跳过，而同族的 `name` 缺失是 fail-closed | `filter_curve_json.h:221-228` vs `:233-238` |
| M5 | 强 provenance 指纹（`wl_sum/val_sum/val_sumsq`）对本文件**整段跳过**：源文件无 `per_filter` 段，门静默从「逐元素密码学校验」降级为「名字串 + n_points」 | `filter_curve_json.h:245-247`；`grep -c per_filter` 源文件 0 / 转录件 1 |
| M6 | QE 曲线解析失败被吞掉并以 `Q(λ)≡1` 继续。注释称「由调用方决定是否判红」，但**没有任何调用方决定**；输出结构体无机器可读标志，下游无法区分「未建模 QE」与「QE 解析失败」 | `frame_photometry_fit.cpp:148-156`；`orchestrator.cpp:2692-2700` |

### 建议（7）

| # | 发现 | 证据 |
|---|---|---|
| S1 | `stage1_gc_panel{1,2,3}_Red.json` 指向 `F:\Astro dev\…\lib\photometric_calib\data\response_curves\filters.json` —— 目录 `lib/photometric_calib/` **不存在**（已 `ls` 确认），且是 Windows 绝对路径 | 三个配置文件 `:30-31` |
| S2 | 数值门只跑 `filter_names[0]` 与 `[-1]`（即 `Antlia V Pro Series B` 与 `ZWO R`，均为 2 nm 均匀），**恰好绕开全部异常条目** | `test_spectrum_integrator_golden.py:333-335,462-464,532` |
| S3 | `kLengthMismatch` 是死枚举：声明于 `:69`、命名于 `:83`，但 `extract_curve_arrays` 返回 bool、任何 false 都被映射成 `kArraysMissing`，真正的长度不等被误报为「数组缺失」 | `filter_curve_json.h:69,83,437,458-460` |
| S4 | `load_curve:458-460` 返回 `kArraysMissing` 时**未清空** `out_wl/out_trans`，与 `:468-469` 的清空分支不一致；注释承诺「不留下半份曲线」为假 | `filter_curve_json.h:441-443` vs `:458-460` |
| S5 | 门名悬空：转录件声明可复跑断言 `test_cfg001_contracts.py::test_filters_transcription_is_verbatim`，该符号**全仓不存在**，真名是 `test_verbatim_transcription` | `eng/packaging/config/filters.json` 的 `transcription.statement` vs `test_cfg001_contracts.py:277` |
| S6 | 3 条曲线在生产网格上面积误差异常（虽 T≥0）：`SDSS u` −32.37%、`SDSS z` −28.99%、`Johnson U` −8.74%，源于这些曲线在 336 nm 以下/1020 nm 以上的部分被 `fill=0` 截断 | 我的 XPSD 网格复算 |
| S7 | 文件末尾无换行符 | `filters.json:14080` |

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| C1 | 把 `akima_interpolate`（`spectrum_integrator.cpp:46-125`）逐行移植到 Python，用**生产 XPSD 网格**（336–1020 nm，步长 2，343 点，来源 `docs/science/algorithms/PHOTOMETRIC_FIT.md:217`）驱动全部 45 条 | 推翻「存储数据值域 [0,1] ⇒ 生产透射率也 [0,1]」 | **推翻。** 4 条 `T<0`，最差 `−0.83333` |
| C2 | 同上，改用非生产 `compute_f_syn` 的 1.0 nm 重叠区网格 | 找出更多潜在负值条目 | **部分推翻。** 多出 `Chroma B`(`−0.06072`,2 点) 与 `Astrodon E-series G`(`−0.01582`,1 点)。**子代理 30c2e1e9 判定此二者在生产网格上为假，我复核后接受该订正** —— 但 1 nm 路径确实存在（`PHOTOMETRIC_FIT.md:216` 明载非生产 `compute_f_syn` 用 1.0 nm 网格），故保留为次级发现 |
| C3 | 45 条两两配对，精确哈希 `(wavelength_nm, value)` 元组；再在 400–700 nm@1 nm 公共网格上比较峰值归一化形状 | 推翻「45 条互相独立的厂商实测曲线」 | **推翻。** 990 组中恰 1 组为 0.0 —— Astrodon 那对 |
| C4 | 把 `test_verbatim_transcription` 的判定逻辑逐行复刻，对**源文件副本**注入新字段 | 检验「逐字转录」门是否真能发现两文件间的任何发散 | **门未发现。** 注入 `provenance_url`、`n_points_raw` 后门仍判 GREEN（STEP 3）；注入真实缺陷（`value` 篡改）才变红（STEP 4）。且仓库中**已经存在**一处门看不见的真实发散：源件独有 `default` 键 |
| C5 | 对每条曲线算「真实梯形积分」vs「按首步长 `wl[1]-wl[0]` 均匀积分」 | 量化非均匀栅格对朴素积分器的杀伤 | **我最初据此断言生产会错 —— 这是错的，我撤回。** 生产用 Akima 重采样到 1 nm/XPSD 网格，不用均匀步长。真实杀伤改由 C1 给出 |
| C6 | 4 条 `L` 曲线用解析函数穷举拟合（硬方波 / logistic） | 检验「占位符而非实测」 | **对 4 条 L 成立**：硬方波**精确**复现全部 7 点（残差 0.0）。**对其余 41 条不成立**：最佳超高斯拟合残差 1.64e-2，远高于 5e-4 量化地板 |

**最强反例**：`filters.json:2548` 的 7 个点 `[300,400,420,500,685,700,800] / [0,0,1,1,1,0,0]` × `spectrum_integrator.cpp:46-125` 的三次 Hermite = **750 nm 处透过率 −0.83333**，且由 `filter_curve_json.h:365-366` 使其对每一帧亮度图生产可达。`docs/science/PHOTOMETRY.md:51` 冻结值域 [0,1]，此处被生产代码自己违反。

---

## 6. 盲复算

**遮蔽既有判定**（未先读 `审稿-RR03-P1`、`审稿-P1-EXP-photometric-magnitude-001/002` 等任何既有结论），仅依据权威件与源码独立取证。

| 既有说法 | 我的独立结论 | 判定 |
|---|---|---|
| 「文件结构完好」 | 成立。45/45 三长度一致、严格递增、值域 [0,1]、无重复键/NaN/尾逗号 | **一致** |
| 「provenance 已如实标注为 unverified（GAP-025），可接受」 | 标注本身诚实，但 **45 条 `curve_stats` 全对、sha256 全对，仍掩盖了两个结构性缺陷**：重复曲线与方波化。消费者会读成「只是没记来源」，而实际是「两个产品共用一条曲线、四条是方波」 | **偏松** |
| 「逐字转录门保证两份滤镜库一致」 | 门只比 5 个字段的白名单，白名单恰好排除了唯一真正发散的字段 `default`；且白名单本身无测试与 JSON 声明交叉核对，可各自漂移 | **偏松** |
| 「数值门覆盖滤镜库」 | 只跑 2 条，且是两条 2 nm 均匀曲线 —— 恰好绕开全部异常条目 | **偏松** |
| 「负透过率只影响 4 条 L」 | 生产网格上正确（4/45）；1 nm 非生产路径上为 6/45 | **一致（补充分路径限定）** |

**净判定：既有结论对「结构」一致，对「语义」偏松。**

---

## 7. 子代理派发记录

派发 **5 个**（`ALG-photometry-001` 单片；4 个角度 + 1 个独立重复，用于交叉验证）。全部只读、无 git 写、未编译未运行任何项目二进制、未读 `/tmp/acsd_g08/`。

| ID | 角度 | 产出 |
|---|---|---|
| `30c2e1e9` | 消费方对齐 | 完整消费者表；发现 B1 的生产可达性；**否定了我 2 条额外负值条目** |
| `71254432` | 消费方对齐（独立重复） | 独立复现同族发现；补 M4/M5/D4/D5 |
| `0b18884e` | 溯源与自洽式断言 | 报告未回收（见第 10 节） |
| `b87b8c4d` | 数值取证 | 独立复现 B1；**自行撤回 3 项自洽式分析**（见下） |
| `4077bc14` | 数值取证（独立重复） | 独立复现 B1/B2/B3，给出可复跑脚本 |

### 逐条复核与否决

**我否决的**：
1. **否决「厂商曲线是合成模板」**（子代理 `4077bc14` 提出「22/45 拟合超高斯 R²≥0.99，指数聚集在 p≈13–14，疑为模板」）。理由：R² 在平坦通带上是弱判据；`b87b8c4d` 用局部多项式留一残差做标定后得到相反结论 —— 厂商曲线残差约为平滑模板对照的 36 倍，**证据反对**该假设。我采纳 `b87b8c4d` 的阴性结果。
2. **否决「NaN 会进入 IRLS 残差」**（两代理均自查否定）。`star_matcher.cpp:454` 与 `image_corrector.cpp:37` 有 `f_syn<=0 → continue` 守卫。真实危害是**静默丢弃红端恒星**（负瓣在 700–800 nm），不是 NaN。
3. **否决「空 `channel` 导致匹配/穿透/跳过/默认/抛错/NaN」**（我最初的假设）。`channel` 在全部生产代码中零消费者，是惰性字段，既不匹配也不抛错。
4. **否决我自己的一条反例（C5）**：均匀步长积分误差在生产路径不成立，我公开撤回。
5. **接受对我自己的订正**：`Chroma B` / `Astrodon E-series G` 在生产网格上非负，我下调为次级发现。

**子代理自行撤回的**（记录在案，其自我审查是本轮可信度的一部分）：`b87b8c4d` 撤回了 3 项 —— ① 「非对称度」统计量把质心定义为半高宽中点，**恒等于 0，自洽式**；② 高斯拟合在平坦通带上退化到 `A≈1e23, b≈−1e23`；③ 样条残差按构造恒为 0。另有 `30c2e1e9` 自行否定自己的花括号计数误报、`71254432` 自行标注 `run/` 目录不在 `git grep` 范围内。

**交叉一致性**：4 个代理独立复现 B1，`T_min` 数值完全吻合（`−1.550000@331nm` / `−0.833333@750nm` / `−0.625000` / `−0.500000`）。面积/积分误差的数值分歧（−26.55% vs −18.22% vs −29.54%）**全部由光谱权重代理 S(λ) 与积分区间不同解释**，非事实冲突 —— 符号恒为负，量级随 S(λ) 在 −8%…−35%（蓝加权 Gaia BP）到 −92%（红加权）间变化。

**待裁决**（子代理 `30c2e1e9` 提出）：`test_spectrum_integrator_golden_results.json` 点名全部 45 条，但它是**生成产物**（写于 `test_spectrum_integrator_golden.py:700`，名字在 `:253` 由库派生，从不回读，不注册在任何构建目标）。算夹具则「27 条零消费者」集合**清空**；算生成产物则该集合成立。我不替负责人裁决，登记为 UNRESOLVED。

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"

# 身份
sha256sum lib/algorithms/photometry/data/response_curves/filters.json
# 期望 76e919a0fd3745db1daa423e227ce4b7e5e4cfe1c5800e195ff0fed340cc5dcc

# B2 两个产品共用一条曲线
python3 -c "import json;d=json.load(open('lib/algorithms/photometry/data/response_curves/filters.json'));print(d['Astrodon E-series B']['value']==d['Astrodon I-series B']['value'], d['Astrodon E-series B']['wavelength_nm']==d['Astrodon I-series B']['wavelength_nm'])"
# 期望 True True

# B3 4 条 L 是硬方波
python3 -c "import json;d=json.load(open('lib/algorithms/photometry/data/response_curves/filters.json'));print({k:d[k]['value'] for k in d if d[k]['channel']=='L'})"
# 期望 4 条全部为 [0.0,0.0,1.0,1.0,1.0,0.0,0.0]

# B1 生产可达的负透过率
sed -n '362,367p' lib/algorithms/photometry/cpp/src/filter_curve_json.h
sed -n '216,217p' docs/science/algorithms/PHOTOMETRIC_FIT.md
python3 /tmp/p1f001/akima_port.py   # 我的逐行移植，1 nm 网格
python3 /tmp/p1f001/fsyn.py         # f_syn 相对误差，三种 S(λ)

# M1 文档计数失真
sed -n '34p' lib/algorithms/photometry/docs/architecture.md   # filters.json (43条)
python3 -c "import json;print(len(json.load(open('lib/algorithms/photometry/data/response_curves/filters.json'))))"  # 45

# M2 7 条空 channel
grep -n '"channel": ""' lib/algorithms/photometry/data/response_curves/filters.json
sed -n '7869p' lib/algorithms/photometry/data/response_curves/filters.json   # Johnson V -> "G"

# M3 门对 default 结构性失明
python3 /tmp/p1f001/blind_gate.py   # STEP2 现存发散 / STEP3 注入未被捕获 / STEP4 篡改 value 才红

# M4/M5
sed -n '221,228p;233,238p;245,247p' lib/algorithms/photometry/cpp/src/filter_curve_json.h

# S1 悬空路径
ls -d lib/photometric_calib
sed -n '30,31p' lib/infrastructure/pipeline/orchestrator/configs/stage1_gc_panel1_Red.json
```

---

## 9. 计数口径

| 计数 | 值 | 口径 |
|---|---|---|
| 成员份数 | 1 | 片清单声明 |
| 覆盖行数 | 14080 / 14080 | `read` 工具 `totalLines`；`wc -l`=14079 因缺尾随换行 |
| 覆盖率 | 100.00% | 分母取 `read` 口径 14080；若取 `wc -l` 口径 14079 则仍为 100% |
| 阻断 | 3 | 我亲自复核原文后认定 |
| 须修 | 6 | 同上 |
| 建议 | 7 | 同上 |
| 子代理 | 5 | 4 角度 + 1 独立重复 |
| 我否决的子代理论断 | 5 条 | 见第 7 节 |
| 子代理自行撤回 | 4 处 | 见第 7 节 |
| 未回收的子代理 | 1（`0b18884e`） | 见第 10 节 |
| 阴性结果（假设未证实） | 9 类 | 见各节「阴性结果」 |

---

## 10. UNRESOLVED（须负责人裁定）

1. **零消费者集合口径**：`test_spectrum_integrator_golden_results.json` 是否算夹具？算夹具则「27 条零消费者」清空；算生成产物则成立。这直接决定「退役对象仍有活消费者」这条本轮重点是否在本片成立。
2. **B1 的修法归属**：缺陷在数据（本片）还是代码（`akima_interpolate` 缺正值性守卫）？两侧都可修，但按项目规范「缺陷归属某个创新点时在该实验单元内部完善」，应归 P1。本片为数据片，我只报不改。
3. **B1 的验证需前台构建**：我的结论来自 Python 逐行移植，非编译后二进制。执行级确认须由前台统一执行（`AGENTS.md` §9）。
4. **`0b18884e`（溯源与自洽式断言角度）未在交付前回收报告**。该角度的核心问题（是否存在用同一定义式既当被检量又当期望量的自洽式断言）我已由 C4 自行独立取证并给出结论，但缺少该角度的独立第二意见。
5. **`channel` 列的最终处置**：保留为空串、改用 `PAN` 补齐、还是从 schema 撤下？`config_registry.json:2476` 已把它登记为 gap，需负责人定口径。