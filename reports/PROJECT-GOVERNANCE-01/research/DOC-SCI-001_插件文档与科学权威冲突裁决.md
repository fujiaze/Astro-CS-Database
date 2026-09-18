# DOC-SCI-001 研究报告 —— 插件文档与科学权威的 4 条冲突裁决

> 任务：`run/PROJECT-GOVERNANCE-01/W5-CFG-002/自证摘要.md` §5 跨域交接第 4 条；`docs/contracts/CONFIG_CONTRACT.md` §8.1 第 3 行、§9:171
> 冲突源：`config/config_registry.json#plugin_knobs` 中 `finding=conflict` 的 4 行（`docs/plugins/**` ↔ SCI/ALG）
> 判定规则：`ENGINEERING_SPEC.md` §3（科学正确性优先）+ `工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md`「判定规则」四条（外部标准/可复现实验不符 → 改文档；同文矛盾 → 改错的一处；两份权威互斥且证据判不了 → 才上呈）
> 纪律：零 git 写；未改 `问题扫描/**`、既有 `reports/**`、`docs/**`、`config/**`、`contracts/**`、`tests/**`；本报告为新增交付物。全部证据落 `run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/`
> 环境：Linux x86_64；Python 3 + numpy（实验脚本自包含，不调用生产实现）

---

## 1 结论摘要、判据与证据面

### 1.1 四条裁决（先看这张表）

| # | 冲突 | 唯一结论 | 置信度 | 最强反方论据（一句话） | 落地动作 |
|---|---|---|---|---|---|
| ① | `04_psf.psf_model=gauss` vs SCI/ALG 椭圆 Moffat4 | **既属两层，也是一条文档错误**：检测核 = 椭圆高斯 / PSF 核 = 椭圆 Moffat4 的双模型已由 SCI 冻结；但 `04_psf` 是 **PSF 层**插件文档，其 `psf_model` 默认必须是 `moffat4`，`gauss/moffat/empirical` 家族列表必须删除 | **0.93** | 插件文档可能是在表达一个「可配置的模型家族」旋钮（DESIGN §3.3 确实把「默认 PSF 模型」列为 config），删掉 gauss 等于砍能力 | 改插件文档 1 行 + registry `04_psf:32` 行（零 SCI/ALG 改动） |
| ② | `08_drizzle.pixfrac=1.0` vs defaults `pending`（实现模板 0.8） | **默认值 = 1.0**；冲突的解法是**填 defaults 的 pending 槽**而不是改插件文档。数值依据 = 无条件通量守恒端点 + 独立复算 + 运行史 | **0.75**（数值）／**0.95**（不得再留 pending 悬挂） | Fruchter & Hook 的 `pixfrac<1` 正是欠采样抖动数据恢复分辨率的手段；v6 目标态面亮度归一已使 `S_p` 对全部 pixfrac 不变，`1/pf²` 偏差已被 `flux_conservation_factor` 兜住 | 改 `config/defaults.json` + schema description + CONFIG_CONTRACT + `PENDING_EXPECTED` 测试 + registry 行（零 SCI/ALG 改动） |
| ③ | `03_star_detection.detection_threshold` 局部 σ vs 全局 | **冻结语义 = 全局**：`threshold = median(img) + 5.0·bgnoise`（作用于 σ=2 平滑图）。插件文档的「σ（局部）」是**未实现的目标态被写成当前默认**，必须改插件文档；「局部噪声自适应」作为在册缺口 `DISP-STAR-002` 保留 | **0.90** | 设计层 `PHASE1_DETAILED_DESIGN.md:64` 明确要求局部噪声，且检测模块输入合同含 variance/ivar；按「最高设计优先」应改 SCI/实现而不是改插件 | 改插件文档 3 处 + `PHASE1_DETAILED_DESIGN.md:64` 注记 + ALG/GATES 行锚修正（SCI 正文零改动） |
| ④ | `06_photometry.flux_zero_point` vs 已删字段 | **删除该行**（不删则必须在 SCI 立新定义）。改名会留下无定义输入；保留为派生量会把输出混进配置表 | **0.90** | 用户可能确实想注入外部/先验零点以跨运行锚定绝对光度；删掉就没了这个能力 | 删插件文档 1 行 + registry 行（96→95）+ totals + CONFIG_CONTRACT 措辞 |

**一句话总纲**：4 条冲突里 **3 条（①②④）插件文档是错的一方，1 条（③）是「文档把未实现的目标态写成了现状」**；没有任何一条需要改 SCI 正文。需要改的是：`docs/plugins/algorithms_phase1/{03,04,06,08}*.md`、`config/defaults.json`（pixfrac 槽）、`docs/design/PHASE1_DETAILED_DESIGN.md:64`，以及**已失效的 ALG/GATES 行锚**（§4.3）。

### 1.2 外部标准核验状况（逐字核验 / 未能核验，如实登记）

| 议题 | 外部锚点 | 核验状况 | 逐字内容（抓取所得） |
|---|---|---|---|
| 检测核 vs PSF 核 | photutils 3.0.0 `DAOStarFinder` | **已逐字核验**（HTTP 200） | "Detect stars in an image using the DAOFIND (Stetson 1987) algorithm. DAOFIND searches images for local density maxima that have a peak amplitude greater than `threshold` (approximately; threshold is applied to a convolved image) and have a size and shape similar to the defined **2D Gaussian kernel**." ⇒ 检测侧用高斯核是标准做法 |
| 阈值语义 | 同上，`threshold` 参数定义 | **已逐字核验** | "threshold : **float or 2D ndarray** — The absolute image value above which to select sources. If threshold is a 2D array, it must have the same shape as the input data." ⇒ 标准族**同时**容纳标量（全局）与 2D 图（局部）；「局部」不是标准强制语义，**必须由定义式自己钉死噪声估计域** |
| PSF 模型 | photutils `MoffatPSF` 页 | **已核验页面存在**（HTTP 200，导航含 `MoffatPSF`） | Moffat 与 Gaussian 在标准生态里是**并列的两类对象**：Gaussian 是 DAOFIND 的检测核，Moffat 是 PSF 光度学模型 |
| 光度零点定义域 | Wikipedia "Zero point (photometry)" | **已逐字核验** | "In astronomy, the zero point in a photometric system is defined as the magnitude of an object that produces 1 count per second on the detector. The zero point is used to calibrate a system to the standard magnitude system, as the flux detected from stars will vary from detector to detector." ⇒ 零点是**系统标定因子（派生量）**，不是逐帧用户输入 |
| drizzle `pixfrac` 默认值 | DrizzlePac / AstroDrizzle 文档（`drizzlepac.readthedocs.io`） | **未能逐字核验（如实登记）** | 该站点多个页面在本环境 404（`astrodrizzle.html`/`drizzling.html` 均 404），`web_search` 返回与该主题无关的结果。仓内 `docs/science/DRIZZLE.md:152` 已引 DrizzlePac Handbook（节级定位）。**故 ② 的数值结论不建立在「外部默认也是 1.0」之上**，只建立在仓内可复跑证据（§3.2/§3.3） |
| Moffat 轮廓来源 | Moffat 1969, A&A 3, 455 | 仓内已有既存定位（`docs/science/PSF.md:117`，文章级） | 本报告不新增外部断言 |

### 1.3 判据（SC 四条规则在本任务的读法）

1. 文档与外部标准/文献不符 → **改文档**（执行行，凭标准原文 + 实验）；
2. 文档与可复现实验不符 → **改文档**（或改实现，取决于哪边被证明错）；
3. 文档自相矛盾 → 判定哪一处与标准/实验一致，**改另一处**；
4. 两份权威互斥**且证据无法判定** → 才上呈负责人。

本任务 4 条全部落在 1-3，**没有一条落在第 4 条**；故本报告不给「需负责人裁决」的收尾，只给结论 + 依据 + 交接改动单。唯一需要**签署动作**的是把结论写进登记面（SCIENCE_CORRECTNESS §登记表新增 claim、defaults.json 的 `authority_status` 取 `owner_adjudicated`），这属于流程动作，不属于科学判断动作。

### 1.4 证据与复跑命令

```bash
cd "/workspace/Astro CS Database"
python3 run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/probe_registry.py        # 4 条冲突逐字
python3 run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/exp1_pixfrac_oracle.py   # E1 独立 drizzle 复算（三条 CHECK 全 OK）
python3 run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/probe_anchor_map.py      # 行锚现状图
python3 run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/probe_dist.py            # 缺口分布
python3 run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/probe_zero_hits.py       # 29/30 字段在 SCI+ALG 零命中（selection_function 为同名异义）
```

日志清单：`01_registry_conflicts.log`（4 条冲突原文）、`02_defaults.log`（4 个键的 defaults 行）、`03_grep_local_zp_pixfrac.log`、`04_gaps_unregistered.log`（22+28 行逐行）、`05_exp1_pixfrac_oracle.log`（E1 结果）、`06_distribution.log`、`07_anchor_check.log` + `08_anchor_map.log`（行锚失效）、`09_zero_hits.log`、`10_gap_semantics.log`（缺口语义抽查）。

---

## 2 冲突① `psf_model`：插件写 gauss，SCI/ALG 是 Moffat4

### 2.1 逐字对照（文件:行 + 原文）

**插件侧（错方）**

- `docs/plugins/algorithms_phase1/04_psf.md:32`
  `| `psf_model` | `gauss` | —— | 模型家族（gauss/moffat/empirical） |`

**SCI/ALG 侧（对的一侧）**

- `docs/science/PSF.md:7`：「**目的**：描述点源响应（**椭圆 Moffat4**），估计 PSF 形状/位置及拟合质量代理 `q_psf`，用于 Astrometry/Photometry 的星点建模与剔星/QA。」
- `docs/science/PSF.md:43`：`I(r) = B + A / (1 + Q)^4`（即 β=4 的 Moffat）
- `docs/science/PSF.md:81`：「FP64 拟合 LM 求解器 `lm_solve`（`dpsf_psf.cpp:98-182`），**仅 7 参数 Moffat4 路径**」
- `docs/science/PSF.md:92`：「改变 Moffat β≠4 或 `FWHM_FACTOR` 而无 SCI 变更；」
- `docs/science/PSF.md:94`（**决定性**）：「在**本模块内**（`lib/algorithms/psf`，§13 实现面）引入未文档的高斯备选拟合路径作为主路径（**检测侧 `lib/algorithms/star_detection` 的椭圆高斯母函数不属本条范围**：SCI-P1-STAR-001 §3、DISP-STAR-007）；」
- `docs/science/STAR_DETECTION.md:31-34`：「**检测侧母函数 = 椭圆高斯**（本页 §1/§3，ALG-STARDET-001 §2），**PSF 侧 = 椭圆 Moffat4**（SCI-PSF-001 §5）；两模型**宽度列不可跨块比较**：同 sx 下 `FWHM_gauss/FWHM_moffat = 2.354820/1.230310 = 1.9140×`（DISP-STAR-007；证据 R-3 §2.2）。」
- `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:64`：同一句的 ALG 侧落点（并注「禁止跨块比较（DISP-STAR-007）」）。
- `docs/algorithms/STAR_PSF_ALGORITHMS.md:7`：「上游: `SCI-PSF-001` (Moffat4 β=4, FWHM=1.230310·σ, q_psf=A/residual_scale)」

**已登记配置面（第三方证据）**

- `config/defaults.json` `psf.default_model`：`"value": "moffat4"`，`authority_status: "sourced"`，`source` = 「docs/science/PSF.md:7（点源响应：椭圆 Moffat4）」，`constraint` = 「枚举 id；当前唯一实现路径为椭圆 Moffat4 7 参数拟合」。
- `contracts/schemas/phase_config_normalize.schema.json:132-134`：`algorithm_psf_model` 的 `enum` 只有 `["moffat4"]`；description 逐字「PSF 模型选择；当前唯一实现路径为椭圆 Moffat4」。
- `config/templates/normalize.phase_config.json:7`：`"algorithm_psf_model": "moffat4"`。
- `lib/algorithms/psf/src/dpsf_psf.cpp`：PSF.md §5 声明「与 `dpsf_psf.cpp:13-18,66-95,351-368` 一致」，且 §13 追溯只列 `MOFFAT4_FWHM_FACTOR`。

### 2.2 裁决（唯一结论）

**二者确实属不同层（检测核 vs 拟合核），且这一点已由 SCI 冻结；但在 `04_psf` 这一篇 PSF 层插件文档里写 `gauss` 是错的。**

1. 双模型成立：检测侧（`lib/algorithms/star_detection`）= 椭圆高斯（`fwhm=2.3548·sx`）；PSF 侧（`lib/algorithms/psf`）= 椭圆 Moffat4（`FWHM=1.230310·σ`）；两列**禁止跨块比较**（同 sx 下差 1.9140×，解析流量比 0.902）——见 `SCI-PSF-001 §10:94`、`SCI-P1-STAR-001 §2:31-34`、`DISP-STAR-007`。
2. `04_psf.md` 的模块是 `psf`（`docs/plugins/00_INDEX.md:31`：`04_psf.md | psf | 空间 PSF 建模与参数化`），落在 SCI-PSF-001 §10 的禁则范围内 ⇒ 该行默认值必须是 `moffat4`。
3. 「gauss」若要在插件文档里出现，只能出现在 `03_star_detection.md` 的检测陈述里，并必须带 1.9140× 不可比声明。

**置信度：0.93。**

**最强反方论据**：插件文档写的不是「默认值」而是「模型家族（gauss/moffat/empirical）」，即该旋钮被设想为可选家族；`ASTROCS_DESIGN.md:140` 也把「默认 PSF 模型」列为 `defaults.json` 的内容；删掉 gauss 等于对外宣称本产品永不支持高斯 PSF 拟合。
**反方为何不成立**：SCI-PSF-001 §10 已把「本模块内引入未文档的高斯备选拟合路径作为主路径」定为**不可接受变化**；phase_config 的枚举当前只有 `moffat4` 一个 token；要新增家族必须走 SCI 变更（SC claim），而不是靠插件文档的默认值列「预支」一个能力。

### 2.3 落地（改前原文 → 建议改后原文 → 依据）

**① `docs/plugins/algorithms_phase1/04_psf.md:32`**

- 改前：`| `psf_model` | `gauss` | —— | 模型家族（gauss/moffat/empirical） |`
- 改后：`| `psf_model` | `moffat4` | —— | PSF 母函数（当前唯一实现：椭圆 Moffat4 7 参数；检测侧椭圆高斯不属本模块，见 `docs/science/STAR_DETECTION.md:31-34`、DISP-STAR-007） |`
- 依据：`docs/science/PSF.md:7,:81,:94`；`config/defaults.json#psf.default_model`；`contracts/schemas/phase_config_normalize.schema.json:132-134`。
- 同批：`config/config_registry.json#plugin_knobs` 中 `(04_psf, psf_model)` 行 `finding: conflict → none`、`conflict: null`、`note` 改为「已按 DOC-SCI-001 §2 与 `psf.default_model=moffat4` 对齐；检测/PSF 双模型见 DISP-STAR-007」；`totals.by_finding` 重算。

---

## 3 冲突② `pixfrac`：插件写 1.0，另一处标 pending

### 3.1 逐字对照（四方）

| 面 | 文件:行 | 逐字 |
|---|---|---|
| 插件文档 | `docs/plugins/algorithms_phase1/08_drizzle.md:42` | `| `pixfrac` | 1.0 | —— | drizzle 像素分数 |` |
| 已登记默认 | `config/defaults.json` `drizzle.pixfrac` | `"value": null`、`"authority_status": "pending_authority"`、`"source": null`、`"pending_task": "SCI-RES-01/R-005（drizzle pixfrac 默认值的权威出处）"`；`constraint` = 「0 < pixfrac <= 1（严格校验，非法值显式 NO_DATA，不静默夹逼…）」；note 逐字含「数值默认无科学权威出处：现实现默认 0.8 见 `lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json:41`（实现模板，非科学权威）；旧文档 `docs/development/CONFIG_SCHEMA.md:84` 同值。按 GAP-023 口径以 pending_authority 占位，禁止编造。」 |
| 语义/值域权威 | `docs/science/DRIZZLE.md:21` | `| `pixfrac` | drop 收缩因子 (0,1] | `drizzle_engine:half=0.5*pixfrac` |` |
| 同上 | `docs/science/DRIZZLE.md:31` | 「`w>0,h>0`, WCS 有效，`0<pixfrac<=1` 否则 `NO_DATA`…」 |
| 同上 | `docs/science/DRIZZLE.md:92` | 「`pixfrac` 非法 `<=0/>1` → 拒绝 `NO_DATA`」；:143「drop 收缩因子，有效域 `(0,1]`，非法值显式 `NO_DATA`（§4），不静默夹逼」 |
| 实现 | `docs/algorithms/DRIZZLE_GEOMETRY.md:57-63` | 「drop 四角: `half = 0.5·pixfrac`（…`pixfrac==1.0` 时行级顶点共享缓存…）；pixfrac∈(0,1] 严格校验，≤0 或 >1 拒绝不夹逼（:1570-1577…）」；:112 同 |
| 实现模板 | `lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json:41` | `"pixfrac": 0.8,` |
| 合同面 | `contracts/v6/data/02_signal.md:43-48` | `FZ-COND-FLUX-CONSERV: pixfrac=1: Sum_p F_p = Sum_j x_j 严格;` / `pixfrac<1: 总输出通量 = pixfrac^2 * Sum_j x_j, provenance.flux_conservation_factor = pixfrac^2` / 「通量守恒是**条件不变量**：`pixfrac<1` 时必须显式使用 `flux_conservation_factor`，否则孔径/总通量换算发生 `1/pixfrac^2` 光度零点偏差」 |
| 编排 schema | `contracts/schemas/phase_config_normalize.schema.json:126-131` | 「…**数值默认未冻结**（defaults.json 的 `drizzle.pixfrac` 为 pending_authority，归属 SCI-RES-01/R-005），本字段不声明数值默认。」 |
| 契约文档 | `docs/contracts/CONFIG_CONTRACT.md:52,:59,:75` | §2 表「drizzle（pixfrac）| 1 | 语义 `docs/science/DRIZZLE.md:21,27,31`；**数值 pending**」；§2 pending 四项表把 0.8 明确标为「实现模板，非科学权威」 |
| 运行史 | `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md:328,:336,:341` | 「pixfrac=0.8 收缩源像素导致相邻源像素之间有固有缝隙」/「pixfrac 默认值: 0.8 → 1.0（drizzle_engine.h / hp_drizzle_api.h / pipeline_adapter.py / healpix_drizzle.py / run_forward_drizzle.py）」/「pixfrac<1.0 收缩源像素覆盖范围, 相邻源像素之间必然有间隙, 形成球面空缺」；:303/:349 记「Drizzle 黑色缝隙修复（5基准全1-ring + 菱形像素 + pixfrac=1.0）」 |
| 测试面 | `tests/config/test_cfg001_contracts.py:49-54` | `PENDING_EXPECTED = { "drizzle.pixfrac": "SCI-RES-01/R-005", … }`（门 `test_pending_items_are_the_adjudicated_gap_set` 断言 pending 集合 == 该表） |

### 3.2 独立实验 E1（可复跑，纯解析 + numpy，不调用生产码）

脚本：`run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/exp1_pixfrac_oracle.py`；输出：`05_exp1_pixfrac_oracle.log`。
模型：源平面 17×17 单位像素、常量面亮度 `B0=1`（故 `x_j = B0·A_pixel = 1`）；drop = 以源像素质心为中心、边长 `pixfrac` 的方形（Fruchter & Hook 2002 语义）；目标平面 = 间距 `s`（源像素单位）、偏移 `o` 的方格，按解析交叠面积累加 `F_p`、`D_p`。

| pixfrac | ΣF/Σx（实测） | pixfrac² | support 均值（对齐单帧） | 1−pixfrac | 空洞比 s=1.0 | 空洞比 s=0.5 | 空洞比 s=0.25 |
|---|---|---|---|---|---|---|---|
| 0.5 | 0.25 | 0.25 | 0.25 | 0.5 | 0.0 | 0.0 | **0.4375** |
| 0.6 | 0.36 | 0.36 | 0.36 | 0.4 | 0.0 | 0.0 | **0.4375** |
| 0.7 | 0.49 | 0.49 | 0.49 | 0.3 | 0.0 | 0.0 | **0.4375** |
| 0.8 | 0.64 | 0.64 | 0.64 | 0.2 | 0.0 | 0.0 | 0.0 |
| 0.9 | 0.81 | 0.81 | 0.81 | 0.1 | 0.0 | 0.0 | 0.0 |
| 1.0 | 1.00 | 1.00 | 1.00 | 0.0 | 0.0 | 0.0 | 0.0 |

三条断言：`flux_ratio == pixfrac^2` **OK**；`support_mean == pixfrac^2`（对齐单帧）**OK**；`holes(s) > 0 <=> s < 1-pixfrac` **OK**（pixfrac=1.0 时任意采样率都无空洞）。

**结论**：`pixfrac` 是**条件不变量的开关**——只有 1.0 使 `Σ_p F_p = Σ_j x_j` 无条件成立、无需 `provenance.flux_conservation_factor` 兜底、且单帧 support 恒为 1；`pixfrac<1` 时总通量严格按 `pf²` 缩水（0.8 ⇒ 0.64，缺因子即 1/pf² = 1.5625 倍光度零点偏差），并在目标采样细于间隙宽度（`s < 1-pixfrac`）时产生零覆盖空洞。本仓 1–2× 过采样策略下 `1-pixfrac=0.2 < s`，所以 0.8 不必然出整像素空洞，但 support 恒 <1、通量因子恒需兜底——这正是运行史里「黑色缝隙」的机制面。

### 3.3 裁决（唯一结论）

**默认值 = 1.0。冲突的解法是把 `defaults.json` 的 pending 槽填成 1.0，而不是改插件文档。**

依据（按强度排序）：

1. **合同面**：`FZ-COND-FLUX-CONSERV` 逐字规定 `pixfrac=1` 时通量守恒**严格**成立，`pixfrac<1` 时总输出通量 = `pixfrac²·Σx` 且必须写 `flux_conservation_factor`；缺因子即「不可用于绝对通量」（`contracts/v6/data/02_signal.md:41-48`、`contracts/data/v6_migration_map_v1.json:129-131`）。默认值应落在**无 fail-closed 前置条件的端点**。
2. **E1 复算**：`ΣF/Σx = pixfrac²` 与 `support = pixfrac²` 精确成立；1.0 是唯一使两者同时为 1 的取值。
3. **运行史**：默认值曾在真实缺陷驱动下由 0.8 改为 1.0，理由是 `pixfrac<1` 造成相邻源像素间固有缝隙与球面空缺（`memory_V18R2-V19…:328,336,341,303,349`）。
4. **文档一致性**：插件文档已经写 1.0，与结论一致；实现模板 0.8 属「实现模板，非科学权威」（`CONFIG_CONTRACT.md:59` 逐字），其余 8 处测试/编排夹具用 1.0（`tests/unit/p1001_real_nodes_test.cpp` 多处、`p2001_real_nodes_test.cpp:136`、`tests/cli/*`）。
5. **值域不被收紧**：本裁决只冻结**默认值**，`(0,1)` 仍为合法取值（`DRIZZLE.md:31,:92,:143`），显式使用者按 `flux_conservation_factor` 兜底即可。

**置信度：0.75（数值结论）；0.95（程序结论：不得再让「文档声明 1.0 / 登记 pending」两存）。**

**最强反方论据**：drizzle 方法引入 `pixfrac` 的原始动机正是「收缩 drop 以便在抖动数据组合时恢复欠采样分辨率」（Fruchter & Hook 2002；仓内 `DRIZZLE.md:151-152` 已定位该文献与 DrizzlePac Handbook）。把默认钉死在 1.0，等于默认放弃这一增益；而且 v6 目标态面亮度归一 `S_p=Σ_j B_j a_jp/Σ_j a_jp` 已使 `S_p` 对**全部** pixfrac 保持不变（`SCI-ADJ-001 ADJ-F-OBS-02`/FZ-FORMULA-DRIZZLE-SB），`1/pf²` 只影响 absolute-flux 换算且已有 provenance 因子，故 0.8 并不「错」。
**为何仍取 1.0**：本管线 Phase1 产出**逐帧**球面产品（面亮度 + support/coverage + 相关噪声描述），分辨率恢复是 Phase2 组合效应，不是单帧默认值的职责；把默认放在「无条件通量守恒 + support=1」的端点上，是「默认值不得引入 fail-closed 域」的直接推论。若 R-005 后续以实验证明目标态下 0.8 在验收指标上优于 1.0，则按 SC 规则**以实验改本裁决**（改文档不是禁忌）——但在此之前，默认只能是 1.0，不能是 null。

### 3.4 落地（改前原文 → 建议改后原文 → 依据）

**① `config/defaults.json` `drizzle.pixfrac`**（数值唯一源）

- 改前：`"value": null, "unit": "1", "authority_status": "pending_authority", "source": null, "source_ref": {"path": "docs/science/DRIZZLE.md", "line": 21}, "pending_task": "SCI-RES-01/R-005（drizzle pixfrac 默认值的权威出处）"`
- 改后：`"value": 1.0, "unit": "1", "authority_status": "owner_adjudicated", "source": "docs/science/DRIZZLE.md:21,:31（值域 (0,1]）+ DOC-SCI-001 §3（FZ-COND-FLUX-CONSERV + E1 复算 + V18R2-V19 运行史）", "source_ref": {"path": "docs/science/DRIZZLE.md", "line": 31}, "pending_task": null`
- 依据：`contracts/v6/data/02_signal.md:43-48`；`run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/exp1_pixfrac_oracle.py`；`docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md:328,336,341`。note 字段同步改为「默认 1.0 = 无条件通量守恒端点；实现模板 0.8 非科学权威」。
- **必须同批处理**：`tests/config/test_cfg001_contracts.py:49-54` 的 `PENDING_EXPECTED` **删去** `"drizzle.pixfrac"` 一行（否则门 `test_pending_items_are_the_adjudicated_gap_set` 必红）；`docs/contracts/CONFIG_CONTRACT.md:52`（§2 组表「数值 pending」）、`:59`（§2 pending 四项表删该行）、`:75`（§3 normalize 行里的 pending 括注）、`:171`（§9 冲突清单）同步改写。

**② `contracts/schemas/phase_config_normalize.schema.json:126-131`**（description 里的 pending 断言已过期）

- 改前：`…数值默认未冻结（defaults.json 的 drizzle.pixfrac 为 pending_authority，归属 SCI-RES-01/R-005），本字段不声明数值默认。`
- 改后：`…数值默认见 defaults.json 的 drizzle.pixfrac = 1.0（DOC-SCI-001 §3 裁决，owner_adjudicated）；本字段不声明数值默认。`
- 依据：同上。

**③ `docs/plugins/algorithms_phase1/08_drizzle.md:42`**（值不动，只补语义，避免读者误以为 1.0 是随手写的）

- 改前：`| `pixfrac` | 1.0 | —— | drizzle 像素分数 |`
- 改后：`| `pixfrac` | 1.0 | —— | drop 收缩因子 ∈(0,1]；默认 1.0 = 严格通量守恒端点（`docs/science/DRIZZLE.md:31`；pixfrac<1 须记 `provenance.flux_conservation_factor=pixfrac²`） |`
- 依据：`docs/science/DRIZZLE.md:21,:31,:143`；`contracts/v6/data/02_signal.md:43-48`。

**④ `config/config_registry.json`**：`(08_drizzle, pixfrac)` 行 `finding: conflict → none`、`conflict: null`、note 改为「已按 DOC-SCI-001 §3 裁决：默认 1.0 已入 defaults.json；值域 (0,1] 不变」；`totals.by_finding` 重算。
**⑤ 流程**：在 `工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md` §登记表新增一条 claim（建议 `SC-008`），登记本裁决的证据（E1 日志路径）与影响面（defaults/schema/CONTRACT/registry/测试）。

---

## 4 冲突③ `detection_threshold`：局部 vs 全局语义

### 4.1 逐字对照（全局 5 处 vs 局部 3 处）

**全局侧（冻结语义）**

- `docs/science/STAR_DETECTION.md:18-19`：「…检测是经验性图像处理流程，不宣称解析保证；**全局检测阈值为** `threshold = median(img) + 5.0·bgnoise`（5σ 语义）。」
- `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:36`：「- 全局检测阈值（`:1637-1647`）: `threshold = median(img) + 5.0·bgnoise`。」
- 实现：`lib/algorithms/star_detection/src/sdet_api.cpp:1782-1792`（逐字）
  `1782: // star_finder.c:80,201: threshold = stat->median + sigma*5.0*stat->bgnoise (sigma=1.0)`
  `1783: T bgnoise = sdet_compute_bgnoise<T>(image, width, height);`
  `1790: T threshold = img_median + T(5.0) * bgnoise;`
  `1791: sdet_log(… "peaker: median=%.4f bgnoise=%.4f threshold=%.4f (median+5*bgnoise)", …)`
  同一文件的「局部」量 `:1838: const double locthreshold = 5.0 * (double)bgnoise;  // sf->sigma=1.0`，用于 `:2093-2094` 的对称性质量门（`max(Ar,Ac) < locthreshold`）——**它也是同一个全局 bgnoise**，不是逐像素局部噪声。
- **检测路径完全不消费 variance/ivar**：对 `lib/algorithms/star_detection/src/sdet_api.cpp` 全文 grep `ivar|variance` = **0 命中**。
- `docs/algorithms/GATES_AND_TOLERANCES.md:38-39`：「**SNR_peak 的定义敏感性**：全局检测阈值是 `threshold = median(img) + 5.0·bgnoise`（`sdet_api.cpp:1637-1647`），作用于 **σ=2 平滑后**的图像…」
- `config/defaults.json#detection.threshold_sigma`：`constraint` = 「threshold = median(img) + 5.0 * bgnoise（**全局检测阈值语义**）」；`source` = `docs/science/STAR_DETECTION.md:19`。
- ALG 自己把「没有局部自适应」登记为**缺陷**：`STAR_DETECTION_ALGORITHMS.md:241-243`「DISP-STAR-002 全局单阈值无局部背景自适应: median+5·bgnoise 全局阈值（`:1645`）对渐变背景/星云场漏检低对比星；旧结构图局部背景路径已退出生产 impl」；§11.3 标题逐字「现状缺陷清单（DISP-STAR-001..007，登记不改码，整改归 P1-STAR-IMPL/INT）」。

**局部侧（冲突来源）**

- `docs/plugins/algorithms_phase1/03_star_detection.md:10`：「- 最高设计 `ASTROCS_DESIGN.md` §3.6（硬约束：检测阈值基于局部噪声）」
- `docs/plugins/algorithms_phase1/03_star_detection.md:23`：「- 检测阈值基于**局部噪声**（使用 variance/ivar），不得用全局固定阈值；」
- `docs/plugins/algorithms_phase1/03_star_detection.md:32`：`| `detection_threshold` | 5.0 | σ（局部） | 检测阈值 |`
- `docs/design/PHASE1_DETAILED_DESIGN.md:64`：「- 检测阈值基于局部噪声，输出 selection function 和 completeness 相关参数；检测目录不是图像灵敏度本身；」

**引注核对（重要）**：插件文档 `:10` 把「检测阈值基于局部噪声」归给「最高设计 §3.6」，但 `ASTROCS_DESIGN.md:193-195` 全文只有：「### 3.6 硬约束 / normalize 的详细硬约束（校准方差传播、不裁切负值、共享 master 相关性、检测阈值、标量压缩门、信息权重等）见 `docs/plugins/algorithms_phase1/` 各插件文档与 `docs/science/`。」——**§3.6 是一段转引，不含任何局部/全局语义**。该说法在仓内的真实出处只有 `PHASE1_DETAILED_DESIGN.md:64`。⇒ 插件文档 `:10` 是**错误引注**。

### 4.2 语义拆解（三个被混为一谈的量）

| 量 | 现状 | 归属 |
|---|---|---|
| **检测阈值** `threshold = median(img) + 5.0·bgnoise`，作用于 σ=2 平滑图 | 已实现（`sdet_api.cpp:1790`），**全局** | 冻结语义；`detection.threshold_sigma=5.0` 就是这个量 |
| 「局部阈值」`locthreshold = 5.0·bgnoise`（对称性/振幅质量门） | 已实现（`:1838,:2093-2094`），但仍是**全局 bgnoise** 的倍数 | 候选质量门，不是检测阈值 |
| **逐像素局部噪声自适应**（用 variance/ivar 逐位置算 σ） | **未实现**，已登记 `DISP-STAR-002`，整改归 P1-STAR-IMPL/INT | 目标态/缺口，不得写成当前默认 |

### 4.3 附带发现：ALG/GATES 对 `sdet_api.cpp` 的行锚已系统性失效

`STAR_DETECTION_ALGORITHMS.md:8` 自称「唯一权威生产源: `lib/algorithms/star_detection/src/sdet_api.cpp`（**2373 行**实测）」，实际该文件当前 **2555 行**（唯一在役副本，`find` 实测其余同名文件均在 `run/**` 影子树）。抽查 10 个关键锚，9 个落空（`logs/07_anchor_check.log`、`08_anchor_map.log`）：

| ALG 文档锚 | 文档声称 | 当前实际内容 | 正确锚（现文件） |
|---|---|---|---|
| 检测阈值 | `:1637-1647` | `:1637` 是 maxStars resize | **`:1782-1792`** |
| DISP-STAR-002 内引 | `:1645` | 同上去重/截断段 | **`:1790`** |
| 背景噪声 FnNoise1 | `:440-476` | `:440` 是 `result->fwhm_x` | **`:462-518`**（`sdet_compute_bgnoise` 定义 @ `:462`） |
| 椭圆高斯母函数 `sdet_gauss_fit` | `:483-620` | `:483` 是 MAD clip 内循环 | **`:520-711`**（定义 @ `:520`） |
| `sdet_lm_fit` | `:262-437` | `:262` 是结构体字段 | **`:285-460`**（定义 @ `:285`） |
| `sdet_detect_impl` | `:1599-2353` | `:1599` 是 PSF 拟合回填 | **`:1749-2499`**（定义 @ `:1749`） |
| `sdet_dedup_stars` | `:822-939` | `:822` 是枚举常量 | **`:864-982`** |
| `sdet_sort_stars` | `:941-956` | `:941` 空行/注释 | **`:983-995`** |
| 旧入口 `sdet_detect` | `:992-1274` | `:992` 是 handle 创建 | **`:1034-1323`** |
| DYNRANGE/饱和块 | `:1684-1692` | `:1684` 是 `free()` | **`:1835-1836`**（`dynrange` @ `:1835`，`minsatlevel` @ `:1836`） |

处置：**这不是本任务 4 条冲突之一，但会污染所有以行锚为证据的判定**（尤其 `GATES_AND_TOLERANCES.md` 的「阈值来源」列与 `R1 表外阈值禁止`）。归 DOC/ALG 线一次重锚（含 `ALG-STARDET-001:8` 行数声明），并在 `DISP` 区登记一条锚失效条目。

### 4.4 裁决（唯一结论）

**冻结语义 = 全局：`threshold = median(img) + 5.0·bgnoise`，作用于 σ=2 平滑图；`detection_threshold` 的单位是「全局背景噪声 RMS 的倍数」，不是局部 σ。**

1. 证据面 5:3（SCI :18-19、ALG :36、实现 :1782-1792、门表 :38-39、defaults constraint 五处一致 vs 插件 :10/:23/:32 与设计详设 :64）。
2. 插件 `:23` 的两个断言**逐项为假**：既不是局部，也不用 variance/ivar（全文 0 命中）。
3. 插件 `:10` 的引注为假（DESIGN §3.6 无此语义）；真正要求局部的是 `PHASE1_DETAILED_DESIGN.md:64`。
4. 「局部自适应」不是被否决的科学主张，而是**在册未实现项**（`DISP-STAR-002`，整改归 P1-STAR-IMPL/INT）——本次订正**不得**把该缺口一并抹掉。
5. 外部标准不站在「必须局部」一侧：photutils DAOStarFinder 的 `threshold` 定义同时允许标量与 2D 数组，且阈值的处理对象是**卷积后图像**（与 AstroCS 的 σ=2 平滑一致）；因此「5σ」必须由定义式自己钉死噪声估计域，而本仓钉的是全局。

**置信度：0.90。**

**最强反方论据**：`ASTROCS_DESIGN.md` 是项目最高设计，`PHASE1_DETAILED_DESIGN.md §5:64` 明确写「检测阈值基于局部噪声」，检测模块的输入合同（`03_star_detection.md:16`）也确实要求 variance/ivar；按「最高设计优先」，应当把它当作实现缺陷去改 SCI/实现，而不是把插件文档改成与设计相反。
**为何不成立**：① 最高设计 §3.6 对检测阈值**不含任何语义**（逐字见 §4.1），它把硬约束转引给插件文档与 `docs/science/`——而 `docs/science/` 的冻结文本写的是全局；② 局部噪声是**目标态**（ALG 已用 DISP-STAR-002 登记为缺陷），插件文档把未实现的目标态写成「默认值单位」属于事实性错误，与「局部自适应是否应该实现」是两个独立问题；③ 本裁决**保留了**局部自适应在册（DISP-STAR-002 不动、P1-STAR-IMPL/INT owner 不动），因此不构成"用改文档掩盖缺陷"。

### 4.5 落地（改前原文 → 建议改后原文 → 依据）

**① `docs/plugins/algorithms_phase1/03_star_detection.md:10`**（修正引注）

- 改前：`- 最高设计 `ASTROCS_DESIGN.md` §3.6（硬约束：检测阈值基于局部噪声）`
- 改后：- 最高设计 `ASTROCS_DESIGN.md` §3.6:195（硬约束转引 `docs/plugins/algorithms_phase1/**` 与 `docs/science/**`）；检测阈值的冻结定义见 `docs/science/STAR_DETECTION.md:18-19`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:36`、`docs/algorithms/GATES_AND_TOLERANCES.md:38-39`
- 依据：`ASTROCS_DESIGN.md:193-195` 逐字（无局部语义）。

**② `docs/plugins/algorithms_phase1/03_star_detection.md:23`**

- 改前：`- 检测阈值基于**局部噪声**（使用 variance/ivar），不得用全局固定阈值；`
- 改后：`- 检测阈值 = `median(img) + 5.0·bgnoise`（**全局背景噪声 RMS 的倍数**，`bgnoise` 由 FnNoise1 行差分族估计；阈值作用于 σ=2 平滑图；实现 `sdet_api.cpp:1782-1792`）。检测路径**不消费**逐像素 variance/ivar。「局部噪声自适应」为目标态、当前未实现，登记 `DISP-STAR-002`（整改归 P1-STAR-IMPL/INT），不得写成现状；`
- 依据：`docs/science/STAR_DETECTION.md:18-19`；`sdet_api.cpp:1782-1792`；`STAR_DETECTION_ALGORITHMS.md:241-243`。

**③ `docs/plugins/algorithms_phase1/03_star_detection.md:32`**

- 改前：`| `detection_threshold` | 5.0 | σ（局部） | 检测阈值 |`
- 改后：`| `detection_threshold` | 5.0 | σ（全局 bgnoise） | 检测阈值；语义 = `median(img)+5.0·bgnoise`（σ=2 平滑图上判定），与 `config/defaults.json#detection.threshold_sigma` 同义 |`
- 依据：同 ②；数值 5.0 **不变**（`defaults.json#detection.threshold_sigma` value=5.0、`test_cfg001_contracts.py:36` 关键锚 `STAR_DETECTION.md:19` token `5.0` 仍成立）。

**④ `docs/design/PHASE1_DETAILED_DESIGN.md:64`**（目标态与冻结态分离）

- 改前：`- 检测阈值基于局部噪声，输出 selection function 和 completeness 相关参数；检测目录不是图像灵敏度本身；`
- 改后：`- 检测阈值的**冻结定义**为全局背景噪声倍数 `median(img)+5.0·bgnoise`（`docs/science/STAR_DETECTION.md:18-19`）；以逐像素 variance/ivar 做**局部噪声自适应**为目标态、当前未实现（`DISP-STAR-002`，整改归 P1-STAR-IMPL/INT）；输出 selection function 和 completeness 相关参数；检测目录不是图像灵敏度本身；`
- 依据：同 ②。**注意**：本项改的是设计文档的现状陈述，不改设计目标；实现完成后应把此注记改回"已实现"并同步 SCI（那将是另一次 SC claim）。

**⑤ `docs/algorithms/STAR_DETECTION_ALGORITHMS.md`**（行锚重锚，零语义变化）

- `:8` `（2373 行实测）` → `（2555 行实测，2026-09-17）`
- `:36` `（:1637-1647）` → `（:1782-1792）`；同句补「对称性质量门 `locthreshold=5·bgnoise`（`:1838`，同为全局 bgnoise 倍数）」
- `:91` `# :1637-1647` → `# :1782-1792`；`:196` `| threshold=median+5·bgnoise | :1637-1647 | 阶段3 全局阈值 |` → `| :1782-1792 |`
- `:242` `（:1645）` → `（:1790）`；§11.2 行锚表其余行按 §4.3 表整批重锚
- 依据：`logs/07_anchor_check.log` + `08_anchor_map.log`（可复跑）。

**⑥ `docs/algorithms/GATES_AND_TOLERANCES.md:39`**（FROZEN 文档，锚修正 + claim）

- 改前：`threshold = median(img) + 5.0·bgnoise`（sdet_api.cpp:1637-1647），作用于
- 改后：`threshold = median(img) + 5.0·bgnoise`（sdet_api.cpp:1782-1792），作用于
- 依据：同 ⑤。按 `ALG-GATES-001 R1`（表外阈值禁止）与 `R2`（证据必需），证据行锚必须存活；本次为**锚修正，不动任何阈值数值**。

**⑦ `config/config_registry.json`**：`(03_star_detection, detection_threshold)` 行 `finding: conflict → none`、`conflict: null`、note 记「DOC-SCI-001 §4：冻结语义=全局；局部自适应为 DISP-STAR-002 未实现项」；`totals` 重算。

---

## 5 冲突④ `flux_zero_point`：SCI 已删除，插件文档仍在用

### 5.1 逐字对照

- 插件侧：`docs/plugins/algorithms_phase1/06_photometry.md:34`：`| `flux_zero_point` | —— | —— | 参考零点（可空） |`
- ALG 侧：`docs/algorithms/PHOTOMETRIC_FIT.md:9`：「- 输出: `PhotometricCalibrationQuality` (sigma_mag, sigma_cal_rel) + scale`<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S4：**删除无定义式、结构体无字段的 zero_point**) -->`」
- SCI 侧：`docs/science/PHOTOMETRY.md:7`：「…估计零点 `location`、尺度因子 `scale` 及残差 QA `sigma_residual / sigma_mag`…」；`:18-19` 符号表「`location` | IRLS/Tukey 稳健位置（dex）| `star_matcher.cpp:478-525`」「`scale` | `10^{−location}` 校正因子 `I_cal=I·scale` | 输出」；`:59` 连续定义 `scale = 10^{−location}  # I_cal = I·scale`；`:73` 零点平移不变量。
- 合同侧：`docs/contracts/CONFIG_CONTRACT.md:90`（§4）逐字：「- **不含每滤镜零点**（本任务裁决口径）：零点 `location` 是**逐次运行估计量**且满足**零点平移不变量**……`docs/algorithms/PHOTOMETRIC_FIT.md:9` 明确 `zero_point` 字段因「无定义式、结构体无字段」被删除（P5-SNR 订正，负责人授权）。故滤镜库**不出现**任何零点列/占位/null 字段；机器门 `test_no_zero_point_column_anywhere` 对**键路径与全文**双向断言（大小写不敏感）。若将来需要每滤镜零点，属**新科学定义**，须负责人批准后另立任务。」
- 代码/合同面实测：`grep -ril "zero_point|zeropoint" contracts/schemas lib/algorithms/photometry lib/algorithms/psf include` ⇒ **0 命中**（LOG: exit 0 无输出）。即该旋钮在实现/结构体/schema 三面都不存在。

### 5.2 定义域分析（外部标准）

- 零点 `ZP` 的标准定义（Wikipedia，逐字见 §1.2）："the magnitude of an object that produces 1 count per second on the detector… used to calibrate a system to the standard magnitude system" ⇒ 零点是**系统的标定/导出台量**，其定义域是「仪器 + 滤光片 + 观测条件」，它由观测数据**估计**而来，或由标准星/星表**标定**而来。
- 本仓已经把这件事做完了：`r_i = log10(F_instr/F_syn)` → IRLS/Tukey → `location`（dex）→ `scale = 10^{−location}`，参考标准来自 Gaia XP 合成通量 `F_syn`（CALSPEC 溯源）。也就是说 **`location/scale` 就是本仓的零点**，且是**每次运行的输出**（`PHOTOMETRY.md §3/§5`、`PHOTOMETRIC_FIT.md F6`）。
- 因此 `flux_zero_point` 作为**配置输入**在定义域上是错位的：它要么是（a）一个外部先验零点 → 需要一个贝叶斯/带先验的估计式（本仓 IRLS 是纯稳健位置估计，无先验项，SCI 无此定义）；要么是（b）一个希望手工指定零点从而跳过估计的开关 → 与「零点必须由本帧参考星估计、并受零点平移不变量约束」直接冲突。

### 5.3 三个候选处置逐一否证

| 候选 | 否证 |
|---|---|
| **删**（本次裁决） | 无——删除后能力不丢：绝对锚定由 Gaia XP `F_syn` 承担，跨运行复用由输出 `scale` 的后处理乘法承担（`I_cal = I·scale`） |
| **改名**（如 `zero_point_prior`） | 改名不产生定义式：SCI/ALG 里没有「先验零点如何进入估计」的公式，结构体也没有字段；改名后仍是一个**不可执行的配置项**，只是把错误藏得更深。且 `07_noise_snr.md:60` 已有 `reference_flux`（e⁻/s，m5/SNR 定义必需）——同义新名会在不同模块造成第三处「零点/参考通量」命名分叉 |
| **保留为派生量** | 违反配置/输出分层：`docs/design/UNIFIED_MODEL.md` §3 要求三类配置严格分离、`ENGINEERING_SPEC.md` §3 禁止一个字段承载多义；派生量属于**输出合同**（`location/scale/sigma_residual/sigma_mag` 已在 `PHOTOMETRY.md` §2 登记）。把输出写进 §5「配置项」表会让读者把它当输入配置 |

### 5.4 裁决（唯一结论）

**删除 `flux_zero_point` 行**（不进 phase_config、不进 defaults），并在同处补一行指针说明「绝对通量锚定 = Gaia XP `F_syn` + 输出 `location`/`scale`」。

**置信度：0.90。**

**最强反方论据**：天文实测中「用外部零点（星表零点、前次运行零点）锚定绝对光度」是常规操作，插件文档的「参考零点（可空）」可能正是为此留的入口；删掉它，用户只能事后手工乘 `scale`，跨运行锚定不再有配置面表达。
**为何仍删**：① 该旋钮在实现、结构体、schema 三面**零命中**（实测），留着就是"文档承诺了不存在的入口"，正是 `ENGINEERING_SPEC.md` §3「科学正确性优先 + 文档必须正确」要清掉的对象；② 本仓的绝对锚定路径**已经定义**为 Gaia XP `F_syn`（`PHOTOMETRY.md:7`「锚在 Gaia XP 绝对分光刻度（CALSPEC 溯源）的模型通带光度尺度」），不需要第二个入口；③ 若确实要引入"先验零点"，那是**新科学定义**（需 SCI 条款 + 实现 + 门），按 `CONFIG_CONTRACT §4:90` 的口径须另立任务，不能靠插件文档一行默认值预支。

### 5.5 落地（改前原文 → 建议改后原文 → 依据）

**① `docs/plugins/algorithms_phase1/06_photometry.md`**

- 改前（§5 表第 4 行）：`| `flux_zero_point` | —— | —— | 参考零点（可空） |`
- 改后：**删除该行**；在 §4「算法与公式要点」末尾补一行：
  `- 绝对通量锚定由 Gaia XP 合成通量 `F_syn` 与**输出** `location`/`scale` 承担（`docs/science/PHOTOMETRY.md:7,:18-19,:59`）；`zero_point` 作为配置输入字段已于 P5-SNR 订正删除（`docs/algorithms/PHOTOMETRIC_FIT.md:9`），不得再登记为旋钮`
- 依据：`docs/algorithms/PHOTOMETRIC_FIT.md:9`（逐字）；`docs/science/PHOTOMETRY.md:7,:18-19,:59,:73`；`docs/contracts/CONFIG_CONTRACT.md:90`；`grep zero_point` 在 `contracts/schemas`/`lib/algorithms/photometry`/`lib/algorithms/psf`/`include` 0 命中。

**② `config/config_registry.json`**：删除 `(06_photometry, flux_zero_point)` 行（96 → 95），并在 `totals` 同步 `rows: 95`、`by_finding`（删该行后 conflict 余 3；这 3 条再按 §2/§3/§4 各自转 `none` ⇒ 终态 `{none: 45, gap: 22, conflict: 0, unregistered: 28}`，合计 95；若同批应用 §6.5 A1 把 `interpolation` 升级为冲突，则为 `conflict: 1 / gap: 21`）。`by_owner_class`/`by_registration` 同步重算。
- 门影响：`CFG002-01` 会把「文档行 ↔ 登记行」一一对应重算（`missing/phantom` 双向），只要文档删行与登记删行同批，门保持绿；`totals` 必须与重算一致（`check_cfg002_registry.py:211-217` 硬断言）。

**③ `docs/contracts/CONFIG_CONTRACT.md`**：`:162`（§8.1 交接表第 3 行）与 `:171`（§9 冲突清单）删去本项，并把「负责人裁决 + DOC 线」改为「DOC-SCI-001 已裁决（见 `reports/PROJECT-GOVERNANCE-01/research/DOC-SCI-001_*.md`）」。

**④ 流程**：本条属"删掉一个不存在的入口"，不需要 SC claim（未改任何科学陈述）；但需要在 §6 交接单里与 ①②③ 同批提交，避免门在中间态判红。

---

## 6 缺口 22 行 / 未登记 28 行的处置建议

### 6.1 现状分布（`logs/06_distribution.log`）

96 行 = `none` 42 / `gap` 22 / `conflict` 4 / `unregistered` 28；归属类 `science_param` 67 / `runtime_policy` 20 / `cli_surface` 6 / `resource_binding` 3。
按文档（仅列含缺口者）：

| 文档 | 总 | none | gap | unreg | conflict |
|---|---|---|---|---|---|
| `algorithms_phase1/01_calibration.md` | 7 | 6 | 1 | 0 | 0 |
| `algorithms_phase1/02_cosmetic.md` | 5 | 2 | 3 | 0 | 0 |
| `algorithms_phase1/03_star_detection.md` | 4 | 0 | 3 | 0 | 1 |
| `algorithms_phase1/04_psf.md` | 4 | 0 | 1 | 2 | 1 |
| `algorithms_phase1/05_platesolve.md` | 5 | 0 | 3 | 2 | 0 |
| `algorithms_phase1/06_photometry.md` | 4 | 0 | 2 | 1 | 1 |
| `algorithms_phase1/07_noise_snr.md` | 6 | 4 | 1 | 1 | 0 |
| `algorithms_phase1/08_drizzle.md` | 5 | 1 | 0 | 3 | 1 |
| `algorithms_phase2/09_coverage.md` | 2 | 0 | 1 | 1 | 0 |
| `algorithms_phase2/10_sampling.md` | 3 | 0 | 0 | 3 | 0 |
| `algorithms_phase2/11_upm.md` | 4 | 1 | 1 | 2 | 0 |
| `algorithms_phase2/12_rejection.md` | 4 | 0 | 2 | 2 | 0 |
| `algorithms_phase2/13_integration.md` | 4 | 2 | 1 | 1 | 0 |
| `algorithms_phase3/14_projection.md` | 5 | 4 | 0 | 1 | 0 |
| `algorithms_phase3/15_resample.md` | 3 | 0 | 2 | 1 | 0 |
| `infrastructure/19_runtime.md` | 4 | 3 | 0 | 1 | 0 |
| `infrastructure/20_benchmark.md` | 3 | 2 | 0 | 1 | 0 |
| `infrastructure/21_observability.md` | 3 | 2 | 0 | 1 | 0 |
| `infrastructure/22_gaia_xpsd_client.md` | 5 | 0 | 1 | 4 | 0 |
| `infrastructure/23_hips_browser.md` | 3 | 2 | 0 | 1 | 0 |

**零命中复核**（`logs/09_zero_hits.log`）：22+28 行的字段名在 `docs/science` 与 `docs/algorithms` 中 **29/30 完全零命中**；唯一例外是 `selection_function`（SCI 2 处 / ALG 5 处），但那处指的是 PSFSW 选择函数证据对象（`docs/algorithms/v6/phase2-psfsw/PSFSW_ALGORITHM_SPEC.md:118`），与 star_det 的输出开关**同名异义**。

### 6.2 处置原则（三类动作，且都不是"由 Agent 自选字段"）

登记册的 `finding` 语义（`config_registry.json#rules.finding` 逐字）已经把动作定死了：`gap` = 「插件文档**声明了默认值**，但无 SCI/ALG 权威、未进任何配置类」；`unregistered` = 「插件默认列为 ——（无默认），但字段本身未在 phase_config/defaults 登记」。据此：

1. **动作 R（撤回无权威的默认值主张）**——对 22 行 `gap`：把 §5 表的「默认」列由具体值改为 `——` 并在同格写 `（未登记，见 config_registry.json#plugin_knobs）`。依据：`ENGINEERING_SPEC.md` §3（文档必须正确）+ `CONFIG_CONTRACT.md` §2 source 规则（`defaults.json` 只收 SCI/ALG 数值）。**撤回一个没有权威出处的默认值不是"自选字段"，而是删除一条不成立的事实主张**；若某行的默认值有 SCI/ALG 条款，则改为指向该条款（转 `none`）。
2. **动作 M（归类迁移）**——对 28 行 `unregistered` 按 `owner_class` 搬到它真正的面（`phase_config` / `cpu_profile` / CLI / 插件文档的"运行期"小节），或按 `ALG-GATES-001 R1/R2` 进冻结门表并给证据 ID；仍无出处的标 `UNJUSTIFIED`。
3. **动作 U（唯一化与消歧）**——同名/等价的成对字段必须收敛到一个 canonical 字段 + 别名/派生声明（依据 `UNIFIED_MODEL.md` §3「禁止一个字段承载多个含义」与 `DATA_SEMANTICS` 的唯一事实源原则）。

**顺序约束**：先 R（文档侧，零科学风险，可立即做），再 U（合同/命名唯一化），最后 M（涉及新登记面与门，必须另立任务）；**任何一步都不得反向改动 SCI 数值**。

### 6.3 22 行 `gap` 逐行处置

| 文档:行 | 字段（声明默认） | 类 | 缺什么 | 谁补 |
|---|---|---|---|---|
| `01_calibration.md:48` | `clip_negative`（false） | A 方法开关 | 无 SCI/ALG 字段条款；语义由 `ASTROCS_DESIGN.md:195`「不裁切负值」硬约束 + `ENGINEERING_SPEC.md:26` 支撑 | R 撤回默认值主张 + 由 CFG 线决定是否入 phase_config（布尔开关，勿伪造 SCI 数值） |
| `02_cosmetic.md:33` | `cr_detection`（true） | A | SCI/ALG 零命中；插件自身 §4 已给语义 | R + SCI-RES 线补条款或标"实现固定开启" |
| `02_cosmetic.md:34` | `cr_sigma`（5.0 σ） | **B 阈值** | 零命中；相邻语义只在 `NOISE_MODEL.md:54`「稳健裁剪: cosmic/hot 5σ 阈，≤2 轮」（不同面） | R + 进 `ALG-GATES-001` 表（或标 `UNJUSTIFIED`，发布门=N）；归属 ALG-GATES 线 |
| `02_cosmetic.md:35` | `interpolation`（neighbor） | **A+冲突（见 6.5 A1）** | 生产字段名是 `method`，合法 token 只有 `median`/`bilinear`；`neighbor` 不存在 | **升级 finding=conflict**（证据见 6.5）+ 默认改 `median` |
| `03_star_detection.md:33` | `min_area`（2 px） | B | 零命中；实现的最小连通判据无对应条款 | R + ALG-GATES 或实现固定值登记 |
| `03_star_detection.md:34` | `deblend`（true） | A | 零命中；生产路径无解混阶段（peaker 七步 + 去重，见 ALG §2） | R（默认主张撤回）+ 若确无解混，删除该行并登记为"未实现能力" |
| `03_star_detection.md:35` | `selection_function`（true） | A+U | SCI/ALG 零命中（同名异义：PSFSW 证据对象） | U 消歧（改名为 `selection_function_output` 或加模块限定）+ SCI 条款 |
| `04_psf.md:33` | `psf_spatial_order`（0） | A | 零命中；`PHASE1_DETAILED_DESIGN.md:72` 有"空间变化模型/降级帧级"语义但无阶数 | R + SCI-RES 线补阶数定义（0/1 阶） |
| `05_platesolve.md:33` | `catalog`（gaia） | U | 与 `22_gaia_xpsd_client.md:32` 同名同默认，跨模块重复声明 | U 唯一化：只留 gaia 客户端一处，platesolve 行改为指针 |
| `05_platesolve.md:35` | `tweak_order`（tan） | A+U | 零命中；与 export 的 `wcs.projection` 同为 tan 语义但相位不同 | U 命名区分 + SCI-WCS 线补 tweak 模型条款 |
| `05_platesolve.md:36` | `max_matches`（200） | B | 零命中；匹配数上限是资源/质量权衡 | R + 归 `runtime_policy`（或 GATES 标 UNJUSTIFIED） |
| `06_photometry.md:31` | `mode`（psf） | A | 零命中；SCI-PHOT 只定义 PSF 域通量（`PHOTOMETRY.md:96`「无孔径背景扣除项」） | R（删 aperture 分支主张）+ SCI-RES 线确认是否保留孔径模式 |
| `06_photometry.md:32` | `aperture_radius`（2×FWHM） | B | 零命中且依赖上一行的 aperture 模式 | R；若孔径模式不实现则连带删除 |
| `07_noise_snr.md:63` | `psfsw_enable`（true） | A | `PSF_SIGNAL_WEIGHT.md` 定义 `psfsw_robust_weight` 模式但无"启用开关"条款 | R + SCI-RES 线（开关属编排面，建议归 `runtime_policy`） |
| `09_coverage.md:31` | `connected_components`（true） | A | 零命中；coverage 连通域语义无条款 | R + `docs/algorithms/PHASE2_COVERAGE.md` 补条款（改动时以现场行号核验） |
| `11_upm.md:37` | `bkg_model_order`（0） | A | 零命中；`UPM_SOLVER.md` 的背景/梯度阶数未以该名登记 | R + 与 UPM 求解器的阶数条款对齐后转 `none` |
| `12_rejection.md:31` | `rejection_classes`（全类） | A | 零命中；`REJECTION.md:20-21` 有方法/profile 词表但无"类别开关" | R + SCI-RES 线（类别列表 vs 方法选择是两面） |
| `12_rejection.md:34` | `keep_moving_sources`（true） | A | 零命中（`REJECTION.md` 无"移动源"字样，实测 0 命中）；插件自己 §1/§4 给了语义 | R + SCI 线补"移动源独立层"条款（产品语义，不能只靠插件自述） |
| `13_integration.md:66` | `target_product`（全） | A | 零命中；`INTEGRATION.md` 的"目标产品"维度未以该名登记 | R + 对齐 SCI-INT 的目标量条款后转 `none` |
| `15_resample.md:35` | `kernel`（bilinear） | A+U | 值合法（`PHASE3_RESAMPLE.md:8,:43` `sampler(nearest|bilinear)`），但**字段名不同**（`kernel` vs `sampler`） | U 统一命名（建议以 ALG 的 `sampler` 为准）+ 登记 phase_config 面 |
| `15_resample.md:36` | `correlation_output`（true） | A | 零命中；`UNCERTAINTY_AND_COVARIANCE.md` 有相关核要求但无开关名 | R + SCI 线（建议归 `runtime_policy`：是否额外输出相关核） |
| `22_gaia_xpsd_client.md:32` | `catalog`（gaia） | U | 同 `05_platesolve:33` | U 唯一化（同上一行） |

### 6.4 28 行 `unregistered` 按类处置

**类 C-1 运行期/资源/IO（11 行）——必须移出科学配置表，落 `runtime_policy`/`resource_binding` 面（`UNIFIED_MODEL` §3、`ENGINEERING_SPEC` §10 已禁止它们进 phase_config）**

- `19_runtime.md:34 memory_limit`（resource_binding；cpu_profile schema 亦无该键 ⇒ 需 CPU/调度线定宿主）
- `20_benchmark.md:33 repeats`；`21_observability.md:32 sampling`（runtime_policy；各自的基建文档 + CLI 面）
- `22_gaia_xpsd_client.md:34 cache_dir / :35 query_limit / :36 timeout`（runtime_policy；Gaia 客户端文档 + 运行期配置面）
- `23_hips_browser.md:30 lod_max_order`（runtime_policy；浏览器组件，`docs/plugins/00_INDEX.md:65` 已声明"不进产品 manifest"）
- `05_platesolve.md:34 catalog_version / 22_gaia_xpsd_client.md:33 catalog_version`（**可复现性输入**：星表版本必须随产品登记，建议进 `inputs[]` 而非 config；归属 P1-WCS/DATA 线）
- `08_drizzle.md:41 order / :44 nside`（等价对，见 C-3）

**类 C-2 门/阈值（10 行）——按 `ALG-GATES-001 R1/R2` 处理：进冻结门表 + 证据 ID，或标 `UNJUSTIFIED`（发布门=N）**

- `04_psf.md:34 psf_uniformity_gate / :35 fit_residual_gate`（语义在 `PHASE1_DETAILED_DESIGN.md:72`「若通过均匀性门可降级为帧级 PSF」；无值）
- `05_platesolve.md:37 residual_gate`（已有 `G-P1-WCS-F1` 0.5″/内点域条目，需声明是否同一门）
- `09_coverage.md:30 min_overlap / 11_upm.md:39 convergence_gate / 12_rejection.md:32 sigma_gate / 13_integration.md:67 correlation_approx / 15_resample.md:37 order_limits`
- `11_upm.md:38 max_iter / 12_rejection.md:33 max_iter`（同名异模块，见 C-3）

**类 C-3 等价/同名（7 行）——唯一化**

| 对 | 现状 | 处置 |
|---|---|---|
| `order` vs `nside`（`08_drizzle.md:41,:44`） | 二者等价（`nside=2^order`，`DATA_SEMANTICS` §2/SCI-DRZ §3a） | canonical = `nside`（生产配置/夹具均用 `nside`，`stage1.template.json:42-44` 用 `{"mode":"auto"}`）；`order` 行改为派生记法注记 |
| `pixel_scale` vs `nside`（`08_drizzle.md:43`） | 输出尺度由 nside/auto 决策（`DRIZZLE_GEOMETRY.md` §3 `compute_auto_nside`） | `pixel_scale` 标为只读派生量或删除 |
| `max_iter`：`11_upm.md:38` vs `12_rejection.md:33` vs `defaults.json rejection.<method>.max_iterations` | 三处同名/近名 | 各加模块前缀或改指 defaults 的按方法表（`REJECTION.md:55-61`） |
| `sigma_gate`（`12_rejection.md:32`） vs `defaults rejection.*.lower_sigma/upper_sigma` | 标量 vs 按方法双阈 | 二选一：声明映射或撤回插件行 |
| `cd_matrix / cdelt`（`14_projection.md:47`） vs `s_out_deg + rotation_deg` | phase_config 用后者表达尺度/旋转（`CONFIG_CONTRACT.md:80`） | `cd/cdelt` 标为 FITS 写出面派生形态，不作配置字段 |
| `06_photometry.md:33 sky_annulus` | 孔径测光域，依赖 `mode=aperture`（同类 A 的第 12 行） | 随孔径模式一并裁决（建议撤回） |
| `07_noise_snr.md:60 reference_flux` | 「固定参考通量（e⁻/s，m5/SNR 定义必需）」 | **这条不能简单撤回**：`m5/SNR` 定义需要参考通量；归 SCI-RES 线在 `NOISE_MODEL/CONTROL_WEIGHT_SNR` 域补定义式（高优先） |

### 6.5 附带发现（本次一并交前台，均非 4 条冲突之一）

- **A1（建议升级 finding）**：`02_cosmetic.md:35` 的 `interpolation=neighbor` 不是「缺权威」而是**与冻结算法冲突**——`docs/algorithms/COSMETIC_ALGORITHMS.md:85`「## 3 ALG-COS-003 插值修复（**method=0 中值 / method=1 名义 bilinear**）」；实现 `lib/algorithms/calibration/src/cosmetic_corrector.cpp:159-175`（`AC_METHOD_MEDIAN`/`AC_METHOD_BILINEAR`，无 neighbor）；token 表（**两层行为不同，必须分别写清**）：C 适配层 `lib/algorithms/cosmetic/include/astrocs/cosmetic/types.h:50-56` 逐字「method 词表 (config "method"; 精确字符串匹配, 不截断): "median" → AC_METHOD_MEDIAN (0) / "bilinear" → AC_METHOD_BILINEAR (1…) / **其余字符串 → PARAM + 103**（adapter 词表层拒绝）」即**适配层 fail-closed**；而 phase1_session JSON 路径 `lib/phase1_session/p1_session.cpp:404-406` `const int method = cosmetic_int(c, "method", AC_METHOD_MEDIAN) == AC_METHOD_BILINEAR ? AC_METHOD_BILINEAR : AC_METHOD_MEDIAN;` 配合 `lib/algorithms/cosmetic/README.md:113-118` 逐字「`method`（字符串 "median"/"bilinear" 映射 AC_METHOD_MEDIAN/BILINEAR，**其他→median**）」= 该层**静默回落**。⇒ registry 该行 `gap → conflict`，插件文档改为 `| `method` | `median` | —— | 修正插值方式（median/bilinear；C 适配层未知词 → PARAM+103，session 层未知词 → median，见 types.h:50-56 与 README.md:113-118） |`。附带登记一个 fail-open 观察：**session 层**未知 token 静默回落 median，与插件 §7 的 fail-closed 风格不一致，建议整改——属新缺陷，需另立条目，不混入本次裁决。**行号易漂移提醒**：本项三条锚（`p1_session.cpp:404-406`、`types.h:50-56`、`README.md:113-118`）在本次核查期间已被并线改动刷新过一次（`p1_session.cpp` 由 :391-393 漂到 :404-406），落地时必须以 `grep -n` 现场复核。
- **A2**：`15_resample.kernel` 与 ALG 的 `sampler` 命名分叉（见 §6.4 C-3）。
- **A3**：`ALG-STARDET-001`/`ALG-GATES-001` 行锚系统性失效（见 §4.3），需一次重锚 + 登记锚失效条目。
- **A4（门的能力缺口，必须写进交接单）**：`check_cfg002_registry.py` 的 `CFG002-01/02` 只核对「文档 ↔ 登记册」的 `declared_default/unit/line` 与登记点**可解析**，**不核对登记点的值**。因此把 `pixfrac` 行改成 `finding=none, registration=defaults_json` 而 `defaults.json` 仍是 `value:null, pending_authority` 时，门**仍然全绿**（假绿）。⇒ ②的落地必须"值 + 状态 + `PENDING_EXPECTED` 测试 + CONTRACT 文本"同批修改，不能只改 registry。

### 6.6 交接清单（文件 → 动作 → 归属线 → 回归门）

| # | 文件 | 动作 | 归属线 | 回归 |
|---|---|---|---|---|
| 1 | `docs/plugins/algorithms_phase1/04_psf.md:32` | psf_model 默认 gauss → moffat4，删家族列表（§2.3①） | DOC 线 | `CFG002-01/02` + `tests/config` 47 用例 |
| 2 | `docs/plugins/algorithms_phase1/08_drizzle.md:42` | 默认 1.0 保留，说明列补语义（§3.4③） | DOC 线 | 同上 |
| 3 | `docs/plugins/algorithms_phase1/03_star_detection.md:10,:23,:32` | 引注修正 + 全局语义 + 单位改 σ（全局 bgnoise）（§4.5①②③） | DOC 线 | 同上 |
| 4 | `docs/plugins/algorithms_phase1/06_photometry.md:34` | 删 `flux_zero_point` 行 + §4 补指针（§5.5①） | DOC 线 | 同上 |
| 5 | `config/defaults.json` `drizzle.pixfrac` | value 1.0 + owner_adjudicated + 清 pending（§3.4①） | CFG 线 | `TestDefaultsContract`、`CFG002-09` |
| 6 | `tests/config/test_cfg001_contracts.py:49-54` | `PENDING_EXPECTED` 删 `drizzle.pixfrac`（§3.4①） | CFG 线 | `python3 -B -m unittest discover -s tests/config -t tests/config` |
| 7 | `contracts/schemas/phase_config_normalize.schema.json:126-131` | description 去 pending 断言 | CFG 线 | `run_validation.py` 三模板 + 负例 |
| 8 | `docs/contracts/CONFIG_CONTRACT.md:52,:59,:75,:162,:171` | 去 pending/冲突措辞，登记已裁决 | CFG 线 | `CFG002-08/11` 锚门 |
| 9 | `config/config_registry.json` | 删 1 行 + 4 条 conflict→none + `totals` 重算（§6.2 顺序） | CFG 线 | `CFG002-01/02 + --self-test` |
| 10 | `docs/design/PHASE1_DETAILED_DESIGN.md:64` | 冻结态/目标态分离注记（§4.5④） | DOC 线 | 文档索引 `check_doc_index.py` |
| 11 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8,:36,:91,:196,:242,:199` + §11.2 表 | 行锚整批重锚（§4.3 表） | ALG 线 | 锚存活检查/人工复核 |
| 12 | `docs/algorithms/GATES_AND_TOLERANCES.md:39` | 锚 `:1637-1647 → :1782-1792`（阈值数值不动） | ALG-GATES 线 | `tools/check_gates_and_tolerances.py` |
| 13 | `工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md` §登记表 | 新增 claim（建议 `SC-008`）：defaults pixfrac=1.0 + 插件文档订正 + 行锚重锚 + 证据路径 | 前台/SCI 线 | — |
| 14 | `docs/plugins/algorithms_phase1/02_cosmetic.md:35` + 对应 registry 行 | A1：finding 升级 + 默认改 `median` | DOC 线 + CFG 线 | `CFG002-01/02` |
| 15 | 22 行 `gap` 的「默认」列 | 动作 R：撤回无权威默认值主张（§6.2/§6.3） | DOC 线（逐行按 §6.3 的"谁补"分派） | `CFG002-01`（文档默认为 `——` 时登记册须同步） |
| 16 | 28 行 `unregistered` | 动作 M/U 分三类另立任务（§6.4） | SCI-RES 线（建议新建 `SCI-RES-01/R-006`）/ ALG-GATES 线 / 基建+CPU 线 / CFG 线 | 各自域门 |

### 6.7 残余风险

1. **②的数值结论置信度 0.75**，且外部 DrizzlePac 默认值在本环境**未逐字核验**（§1.2）。若 SCI-RES-01/R-005 的独立实验给出反证，按 SC 规则以实验改本裁决；但在那之前 `defaults.json` 的 `pending` 槽必须填 1.0（"null 默认值"本身是不可交付状态：任何消费方都无法确定该用什么）。
2. **插件文档的默认值列撤（动作 R）会触发登记册漂移门**：`CFG002-01` 逐行比对 `declared_default`，文档与登记册必须同批改，否则必红——这是有意的 fail-closed，不要用"只改一边 + 重生成登记册"绕过（`config_registry.json#rules.no_auto_sync` 逐字禁止）。
3. **`selection_function` 与 PSFSW 同名异义**（`PSFSW_ALGORITHM_SPEC.md:118`）若只做撤回不做消歧，会在下一轮把两处再次混起来。
4. **行锚重锚（A3）是"只减不增"的活**：本次只给出 10 个已核验锚，§11.2 全表未逐条核验；重锚任务必须逐条核对而非整体平移（本次已证明该文件的偏移不是常数：不同段落在 2373→2555 的增长中位移不同）。
5. **A4 的假绿缺口**：在 `CFG002` 增设「登记点值 vs 文档默认值」一致性断言之前，任何 `conflict → none` 的操作都可能掩盖未落地的值修改；本条建议纳入 `ci/checks.json` 的后续加固（CI 线）。
