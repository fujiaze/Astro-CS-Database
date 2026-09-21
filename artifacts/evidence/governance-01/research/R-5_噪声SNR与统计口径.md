# R-5 决策建议书 —— 噪声/SNR 与统计口径

> 研究线 ID：**R-5**（依据 `工程控制/PROJECT-GOVERNANCE-01/RESEARCH_TASKS.md:28`）
> 覆盖议题：M3-C-003(P0)、M7-A-203、M3-A-004/005/006、M7-A-104/105/106、V12-N-03/16、M6a-D-007、V10-N-07、M3-F-001
> 复现基线：仓库 HEAD `e9b31285af6dd6c9bb2636f9e4d707a433f8abd7`（branch main）；Linux amd64，python 3.13.5 / numpy 2.2.4 / g++ 14.2.0
> 纪律声明：**本轮零 git 写、零仓库受控文件修改**；全部新增文件只落 `run/PROJECT-GOVERNANCE-01/R-5/`（`.gitignore:19 run/*` 已确认忽略，`git status --porcelain run/` 空）。
> 本报告不采信 `问题扫描` 账本的 `fix_state/verified_state`；每条结论均由本轮复跑输出支撑（§6 列全部命令与退出码）。

---

## 0. 一页结论表（负责人只需确认/否决）

| # | 议题 | 原判据是否正当 | **唯一推荐结论** | 置信度 |
|---|---|---|---|---|
| 1 | **M3-C-003(P0)** 5 ⇔ 64 | 方向对（确存在 SCI 文本 vs 实现的默认值冲突），**处方错**、严重度量化错 | **不改代码**；把 SCI-NOISE-001 §4 的默认值由 5 升为 **64**（走 SCI 变更 claim，附本报告 §2 的 MC 推导）；ALG §13.2「不改 SCI，以代码为准登记」改写为「R-5 裁决 + 变更 claim 升级 SCI」 | **高** |
| 2 | **M7-A-203** | **判据对象写错**（该条是 §5 兜底式记号两读，不是 ivar 单位；ivar 单位属 M3-A-004） | SCI §5:51 由 `max(vmed_or_sig², floor)` 拆成两条显式式（合格 patch 支 `max(vmed, floor)`；退化支 `max(sig², floor)`）；**不改码** | 高 |
| 3 | **M3-A-004** ivar 单位 | 正当、可测量 | SCI §2:16 的 `ivar` 单位 **`pixel⁻²·ADU⁻²` → `ADU⁻²`**（同一 claim 内一并改） | 高 |
| 4 | **M3-A-005** `abs(det)>1e-24` | 正当，但**严重度被低估** | 判据改为**相对条件数**（`det > eps·sxx·syy`）并在 **build 阶段**回退 `has_spatial_field=0`（fill 自动走全局稳健中位数）；给出复现例（场 13.7→40.5 ADU²，真值 25） | 高 |
| 5 | **M3-A-006** gain 方向/常数 | 正当 | ①`wrapper_phase1/noise_model.h:33` 与 `.cpp:207` 注释补 `/gain²`；②单测 fixture 改回 SCI 约定（gain=e⁻/ADU）并把解析式与容差对准 SCI §5/§12；③`photometer.cpp:90` 的 `1.4826` 换冻结常数 | 高 |
| 6 | **M7-A-104** 无权重 LS | 正当，但**根因不在 LS** | 由 #1 一并解决：`min_samples=64` 的场误差 0.0105 ≈ 逆方差最优加权 0.0104；`min_samples=5` 为 0.0686（×6.6）。**不需要**改 LS 为加权 | 高 |
| 7 | **M7-A-105** 注入门 | **前提部分失效**（参考量可构造；缺的是实现） | 由 A_SCI_DEF **改回 F_TEST_GAP**：补 PHOTOMETRY §11 合成注入与 NumPy 复算测试；不判 SCI 缺陷 | 中 |
| 8 | **M7-A-106** `sigma_cal_rel` | 正当 | SCI-PHOTOMETRY §2/§9a 增补「`sigma_cal_rel` 是逐星散度，**不是**零点不确定度；零点标准误 `1.253·sigma_residual/√N_eff`」；代码已实现该式，**无码改** | 高 |
| 9 | **M6a-D-007** status==3 | 正当 | `SNR_QF_PSF_OK ⇔ status==0`（ALG §11.2 冻结四码：3=ITERATION_LIMIT 是失败码）；删 `orchestrator.cpp:4392` 的 `|| psf_status==3.0` | 高 |
| 10 | **V12-N-03** 0.6745 | 正当（量级小） | 统一到精确常数 `Φ⁻¹(3/4)=0.6744897501960817`（=1/1.482602218505602）：改 `star_matcher.cpp:21` + SCI-PHOT 的 0.6745 字样；`sigma_residual` 变化 +1.52e-5 相对 | 中 |
| 11 | **V12-N-16** kLn10 双写 | 正当（纯重复） | 同一模块内单一 `constexpr`；两位数字面量在 double 下**逐位相同**，无数值变化 | 高 |
| 12 | **V10-N-07** `(uint64_t)d` | **别题误挂**（与噪声/统计无关），事实成立但严重度高估 | 三处解析器加 `isfinite` 与 `2^53` 上限 + 数组 count 上限；实测生产平台可见行为是「1e999→0」而非内存破坏 | 中 |
| 13 | **M3-F-001** Oracle 独立性 | 门正当；P1-002 交付后「无实现」已消除，**「独立性」仅部分达标** | 保留为**实现回归对拍**；补 3 项（常数由 `NormalDist().inv_cdf(0.75)` 导出、default_config 逐字段断言、公式级偏差区间用例）后才可称 SCI §11 的 Python Oracle | 高 |

**需负责人确认的那一句话**：是否批准把 `docs/science/NOISE_MODEL.md` §4 的 `min_samples` 默认值由 **5** 升为 **64**（并同批勘误 §2:16 的 ivar 单位、§5:51 的兜底式记号），即**维持生产实现现状、改正 SCI 文本**，而不是按原门禁处方把代码改回 5？

---

## 1. 权威原文（逐条 `文件:行` + 逐字原文 + 权威链位置）

### 1.1 权威链本身

**A. `ASTROCS_DESIGN.md:7-33`（§0 权威声明，链①最高设计）** —— 逐字：

```text
§0「权威声明」（原文小节标题，此处降级以避免与交付 6 节混淆）
（mermaid 图，节点）
    D["① 本文 最高设计<br/>是什么 · 做到什么 · 顶层架构 · CLI · 发行 · 验收"]
    A["② AGENTS.md 机器干活手册"]
    E["③ 工程规范 ENGINEERING_SPEC"]
    C["④ 控制包规范 CONTROL_PACK_SPEC"]
    CI["⑤ CI 规范 docs/ci/"]
    P["⑥ 插件文档 docs/plugins/ 23 篇"]
    S["docs/science 科学公式（权威）"]
    AL["docs/algorithms 算法推导（权威）"]
    U["docs/design/UNIFIED_MODEL 数据对象与配置"]
    D --> A & E & C & CI & P
    D --> U
    D -.公式引用.-> S
    D -.算法引用.-> AL
    A --> P
    E --> CI
    C --> P

- 本文与其他任何文档冲突时，**以本文为准**。
- 修改本文必须由项目负责人明确批准，并记录变更原因与影响面。
- Agent 无权放宽、重新解释或"为通过检查而改写"本文。
- 本文档只写**要怎么做**；具体硬约束与细节写入对应下级文档（插件文档、docs/science、docs/algorithms）。
```

> **权威链位置**：`docs/science` 是「科学公式（权威）」，`docs/algorithms` 是「算法推导（权威）」——**推导不得凌驾科学定义**；两者的冲突由更高层（本文 + 负责人）裁决。**注意**：现行 §0 并未写「SCI 优先于 ALG 优先于代码」这一句明文（该句出自已废止的旧宪章，见 §4.1）。

**B. `ENGINEERING_SPEC.md:23-28`（链③工程规范 §3 科学代码红线）** —— 逐字：

```text
§3「科学代码红线（最高优先级）」（原文小节标题）

- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差 **不可修改**；
- 架构重构**不得**同时改动科学语义；迁移必须 bitwise 相等（顺序变化时先冻结容差并登记）；
- 模块不得根据 CPU 型号改变公式；`cpu_profile` 只影响并行/ISA，不进入科学配置；
- 数据对象按 `docs/design/UNIFIED_MODEL.md` 区分，禁止一个字段承载多个含义。
```

> 「**精度与默认容差不可修改**」是 M3-C-003 的直接上位约束：任何一方的改动都必须走文档集变更流程。

**C. `ENGINEERING_SPEC.md:48-59`（§5.1 每模块必备测试）** —— 逐字摘录：

```text
### 5.1 每模块必备测试

- 确定性合成数据生成器；
- 不调用生产实现的独立 Oracle 或解析解；
- 科学不变量/性质测试；
- 边界、NaN/Inf、空输入、极端参数、错误输入；
```

**D. `ENGINEERING_SPEC.md:115-120`（§8 机器一致性检查）** —— 逐字：

```text
§8「机器一致性检查」（原文小节标题）

- `eng/ci/checks.json` 是唯一检查注册表；`eng/ci/` 提供确定性执行器；
- 每项检查有正例与负例（能红能绿）；豁免必须显式登记且只减不增；
- 修改代码/测试后必须本地复跑对应检查项；
- 检查器覆盖（至少）：… 算法引用有效 SCI/ALG、核心合同有独立测试 …
```

**E. `ASTROCS_DESIGN.md:113` 与 `:135-144`（§3.3 配置分离）** —— 逐字：

```text
输入为 **JSON 配置**（数据块结构）。数据块只含**必要参数 + 帧路径**；容差与默认参数放在**程序根目录 `config/`**（见下）。
…
**程序根目录 `config/`（全局配置，放配置文件）**：
config/
├── filters.json      滤镜库（bader r / bader v / ...）：型号、通带、波长等
└── defaults.json     默认参数：暗场-亮场曝光容差、默认 PSF 模型、检测阈值、标量门、稀疏层密度等
- `config`（必要参数）+ `inputs`（一大组数据）；**容差和默认参数从 `config/defaults.json` 读取**，运行 JSON 只写必要参数与路径；
```

### 1.2 噪声科学合同（`docs/science/NOISE_MODEL.md`，链「docs/science 科学公式（权威）」，状态 FROZEN T104 2026-08-23）

- `:3` —— `> ID: SCI-NOISE-001  范围: SCI-NOISE-001..015 (legacy SNR-001..015)  状态: FROZEN (T104 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-NOISE-001..  模块: snr_estimator (NoiseWeightModelV1)`
- `:16`（§2 符号表）—— `| ivar | 1/variance (pixel⁻²·ADU⁻²) | variance_bg_global 倒数 / fill |`  ← **M3-A-004 的被诉行**
- `:17` —— `| σ_bg | 1.4826022185·MAD(|x−median|) | noise_model.cpp:robust_sigma |`
- `:29`（§3 物理量和单位）—— `- x, σ_bg, √variance: ADU（或 e⁻，同输入标度）；variance: ADU²；ivar: ADU⁻²；a: ADU², b,c: ADU²/pixel；gain: e-/ADU；read_noise_e: e-；signal: ADU；掩膜半径/坐标: pixel；floor: ADU²。`
- `:37`（§4 输入有效域）—— `- 维度 h>0,w>0，data 非空且含有限值；min_samples（patch 样本数阈）默认 5；rmax 为固定值，不按星亮度/振幅缩放（API 仅 star_x/y 无 amplitude，见 §6）。`  ← **M3-C-003 的被诉行**
- `:46-53`（§5 连续定义，冻结式）—— 逐字：
```text
patch grid 8×8；星点掩膜 fixed conservative rmax = max(1,r0)·max(1,scale)（统一半径，不按亮度缩放）
σ_bg = 1.482602218505602 · median(|x − median(x)|)   # MAD→σ，Gaussian 假设
稳健裁剪: cosmic/hot 5σ 阈，≤2 轮
控制点: 合格 patch（样本数 ≥ min_samples）的 patch variance
空间场: 最小二乘平面 var(x,y) = a + b·x + c·y；负预测 clamp 至 variance_floor (1e-12)
全局兜底: 合格 patch variance 的稳健中位数 vmed
variance_bg_global = max(vmed_or_sig², variance_floor)
g_model_floor: 以 model 指针为 key 注册 floor，snr_noise_model_v1_free 时按指针擦除，无全局共享
ivar = 1 / max(variance, floor)   # fill 阶段 max(a+b·x+c·y, floor)；control 点亦 max(patch_var, floor)
```
  ← 第 7 行即 **M7-A-203 的被诉行**（记号两读）
- `:74`（§7 独立不变量）—— `- **空 support 不传播**：无合格 patch 时 ivar=0, r=1 拒绝加权，不产生伪有效权重。`
- `:76` —— `- **量纲一致**：variance [ADU²] → ivar [ADU⁻²] 倒数关系精确，gain 模型量纲 max(signal,0)/gain [ADU²] 无量纲混。`
- `:82`（§8 极端/退化）—— `| 无合格 patch | degenerate=1, 若 sky 样本<min_samples/2 或 robust_sigma 非有限/≤0 ⇒ ivar=0,r=1 拒；否则 degenerate=1 全局常量场 has_spatial_field=0, r=0 fallback | noise_model.cpp:210-235 |`  ← **min_samples 与该阈值的同一性锚**（代码 :225 正是 min_samples/2）
- `:91-93`（§9 精度策略）—— `- FP64 全链路；MAD 常数 1.482602218505602（15 位截断，与 1.4826022185 差 <1e-12）；… - 5σ 裁剪 ≤2 轮，避免过度剔除。`
- `:98`（§9a）—— `… 本层唯一产出为 variance/ivar（GLOSSARY variance/ivar）。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S5) -->`
- `:101` —— `- **权重归一与适用域**：ivar 作为 Phase2 逐像素科学权重直接入加权（归一在消费侧 Σw/Σ）…；ivar=0 显式表示不可用，禁止伪装（§7 空 support 不传播）。`
- `:109`（§10 不可接受变化）—— `- 改变 8×8 patch 网格或 5σ ≤2 轮 裁剪策略而无 SCI 变更。`
- `:113`（§11 验证 Oracle）—— `- **Gaussian 合成**：N(0,σ²) 空背景合成帧（σ=5 ADU），经验 σ_bg 在 5% 内复现（SNR-004）。`
- `:117` —— `- **Python 参考**：NumPy 对同 data 的 median/MAD/5σ裁剪/平面最小二乘 复算 variance/ivar（rtol 1e-9）。`
- `:135`（§14 文献）—— `2. MAD→σ 换算 1.482602218505602=1/Φ⁻¹(3/4)：标准正态 MAD 分位恒等式（Φ⁻¹(3/4)≈0.674490），教科书级，Project-defined 采纳；与 SCI-PHOT 的 0.6745 同源。`  ← **V12-N-03 的被诉行（"同源"声明）**
- `:140`（§15 Acceptance）—— `- §11 Oracle 全过：Gaussian 5% 复现、Poisson 诊断 5% 交叉（仅诊断）、平面场 10% 恢复、四不变量门、Python 参考 rtol 1e-9；`

### 1.3 噪声算法推导（`docs/algorithms/NOISE_ESTIMATION.md`，链「docs/algorithms 算法推导（权威）」）

- `:4` —— `> (docs/science/NOISE_MODEL.md，FROZEN T104 2026-08-23，共享引用不改动)`
- `:109-116`（§13 抬头）—— 逐字：`> 上游: SCI-NOISE-001..015（docs/science/NOISE_MODEL.md，FROZEN T104 2026-08-23，不改 SCI） …` / `> 本节由源码逐符号核对后追加（P1-NOISE-DOC）：§1-§12 为 T204/V5 既有登记，根公式（MAD→σ、5σ≤2 轮、平面场、floor、ivar=1/variance）不变；…` / `> 禁止声明 IMPLEMENTED（迁移落码由 P1-NOISE-IMPL 执行）。**禁止根据代码缺陷反向修改 SCI——全部差异登记 DISP-NOISE-***。`
- `:132-140`（§13.1 逐符号锚）—— `| ALG-NOISE-001 | snr_noise_model_v1_default_config | noise_model.cpp:333-346（默认 8×8/r0=10/scale=6/clip 5.0/**min 64**/rounds 2/spatial 1/floor 1e-12） |`
- **`:151-153`（权威层级倒置点）** —— 逐字：
```text
- min_patch_samples 默认 **64**（snr_estimator.h:117、default_config :333），
  SCI §4 "min_samples 默认 5" 为旧稿数字——**不改 SCI**，以代码为准登记；
  patch 合格阈即 64。
```
- `:154-157` —— `- 掩膜统一半径 rmax = max(1, source_mask_radius_px)·max(1, mask_radius_scale) = 默认 10·6 = **60 px**（noise_model.cpp:144-147），对所有星统一，不按振幅/星等缩放（API 无 amplitude 输入）；`
- `:173-181`（§13.3 DISP-NOISE 缺陷清单）—— `DISP-NOISE-007 | 参数下限静默钳位无返回码区分：patch_grid<2→2、clip_sigma<1→1、min_patch_samples<1→1、max_clip_rounds<0→0（调用方不可知被钳位） | noise_model.cpp:166-170`
- `:189`（§13.4 容差冻结）—— `> 既有容差锚：σ 5%（SNR-004）、平面场 10%（SNR-006）、Poisson 诊断交叉 5%（SNR-005）、NumPy 参考 rtol 1e-9（SCI §11）——本节不得放宽。`

### 1.4 合同/词典层（链①→`docs/design/UNIFIED_MODEL`/`contracts`；DOC-001 冻结）

- `docs/GLOSSARY.md:3` —— `本词典是**唯一术语权威**。每个核心术语恰一个含义;legacy alias 列出迁移去向。任何文档/代码/接口与本文冲突时,以本文锚点所指的权威文件为准并回改词典——**禁止两套定义并存**。`
- `docs/GLOSSARY.md:10-12` —— `| variance | 逐像素随机方差… | 信号单位²(ADU²) | … |` / `| ivar | 逆方差=1/variance;variance=0/缺失 → ivar=0(显式不可用,禁止伪装);NaN/负 variance=产品损坏 | **ADU⁻²** |` / `| pixel_weight | 像素级科学权重=ivar… | 无量纲 |`
- `docs/contracts/DATA_SEMANTICS.md:393-397`（DATA-P1-NOISE 抬头）—— `> 本节是该模块单位/dtype/shape/invalid 的唯一权威；§4a 产品语义（ivar=1/variance、ivar=0 显式不可用）在此落地为模块级 I/O 语义。`
- `docs/contracts/DATA_SEMANTICS.md:406` —— `| cfg（SnrNoiseModelConfig） | … | patch_grid_x/y≥2、cosmic_clip_sigma≥1、**min_patch_samples≥1（默认 64）**、max_clip_rounds≥0 静默钳位（DISP-NOISE-007）；…`
- `docs/contracts/DATA_SEMANTICS.md:413` —— `| NoiseWeightModelV1 | … | σ: ADU；variance: ADU²；**ivar: ADU⁻²** | 合格 patch 的 max(patch_var, floor) 与 1/var；n_qualified_patches+n_rejected_patches==64（8×8） |`
- `docs/contracts/DATA_SEMANTICS.md:416` —— `| fill 输出 out_variance / out_ivar | float32 [h·w] 行主序 | ADU² / ADU⁻² | … 无合格 patch 时 ivar=0 拒绝加权（SCI §7） |`
- `docs/contracts/DATA_SEMANTICS.md:499` —— `| out_sigma_residual | double 标量 | dex（log10 flux-ratio） | MAD(r_inliers)/0.6745（:551-560）；下游换算 sigma_mag/sigma_cal_rel 由 snr_phot_cal_quality 承担（API-NOISE-001 边界） |`
- `docs/contracts/DATA_SEMANTICS.md:553` —— `| [0]=status | double（整值 0..3） | 拟合状态码（STAR_PSF_ALGORITHMS §11.2 四码语义）；**PHOTOMETRIC 仅 status=0 行入匹配**（pc 匹配侧 status≠0 记 reject） |`
- `docs/contracts/DATA_SEMANTICS.md:765` —— `| star_measurements（orchestrator 侧权威源，非 C ABI 直入） | FLOAT64 [N,≥15] 行主序 | … | 缺失/格式错 → BLOCK_MISSING（:1830-1839）；**过滤 status∉{0,3}**、sat r[13]、fwhm r[7]∉[0.5,20]、边缘 5px（:1852-1862） |`
- `docs/algorithms/STAR_PSF_ALGORITHMS.md:165` 起（§11.2 拟合失败语义，冻结）—— 逐字表行：
```text
| 0 | DPSF_FIT_OK | 收敛 :163 且过验证链一~三 | 全参数回填 :391-403 | 计入 out_n_valid；写 9 字段 :784-794/907-916 |
| 1 | DPSF_FIT_NO_CONVERGENCE | 验证链一 :336-338 / 二 :341-346 / 三 :349-354 | … | 不计入 compact 行；逐星 out_status=1 |
| 2 | DPSF_FIT_INVALID_PARAMS | 空指针/w≤0/h≤0 :431-434；rect 面积<9 :236-239；… | 同上 | 批整体 -1（:700-707），不触碰输出 |
| 3 | DPSF_FIT_ITERATION_LIMIT | max_iter=200 耗尽 :187 | 仍回填当前最优参数 :391-403 | 非 OK→NaN，不计 valid |
```
- `docs/science/PHOTOMETRY.md:20/:22/:29/:52/:60/:107/:127`（链「docs/science」）—— `| S | MAD(r)/0.6745 初值尺度 (dex) |`；`| sigma_residual | MAD(r_inliers)/0.6745 dex | QA |`；`… sigma_cal_rel: 相对误差（sigma_cal_rel = ln10·sigma_residual）；qf 无量纲标志。`；`- **合成注入**：已知 scale 的 F_instr=k·F_syn 注入场，估计 location≈log10 k（rtol 1e-4）。`；`2. MAD→σ 换算 1/0.6745：标准正态 MAD 分位（Φ⁻¹(3/4)≈0.6745），教科书级恒等式，Project-defined 采纳。`
- `docs/plugins/algorithms_phase1/07_noise_snr.md:12`（链⑥插件文档）—— `- docs/science/NOISE_MODEL.md、docs/science/PSF_SIGNAL_WEIGHT.md`（即插件层把 SCI 列为上位依据）。

### 1.5 配置层（`config/defaults.json`，链①§3.3 + `docs/contracts/CONFIG_CONTRACT.md:29`）

- `config/defaults.json:6-7` —— `"design": "ASTROCS_DESIGN.md §3.3（程序根 config/：defaults.json 放暗场-亮场曝光容差、默认 PSF 模型、检测阈值、标量门、稀疏层密度等）"` / `"preflight": "ASTROCS_DESIGN.md §3.5（预检三级：容差等判定基准来自 config/defaults.json）"`
- `config/defaults.json:101-111`（逐字）：
```json
    {
      "key": "noise.min_patch_samples",
      "value": 64,
      "unit": "count",
      "constraint": ">= 1；patch 合格样本数下限（默认 64）",
      "authority_status": "sourced",
      "source": "docs/algorithms/NOISE_ESTIMATION.md:134（默认 min 64）",
      "source_ref": {"path": "docs/algorithms/NOISE_ESTIMATION.md", "line": 134},
      "pending_task": null,
      "note": "同文件 :151 记 min_patch_samples 默认 64（snr_estimator.h:117、default_config :333）。"
    },
```
- `docs/contracts/CONFIG_CONTRACT.md:29` —— `## 2 config/defaults.json（44 字段；astrocs.config-defaults/v1）`

### 1.6 权威链位置小结

| 事实 | 文本所在 | 链上位置 | 是否可自行改 |
|---|---|---|---|
| `min_samples` 默认 **5** | SCI-NOISE-001 §4:37 | docs/science（科学公式权威，FROZEN） | 否（须负责人批准的变更 claim） |
| `min_patch_samples` 默认 **64** | ALG-NOISE-001 §13.1:134 / §13.2:151；`snr_estimator.h:117`；`noise_model.cpp:350`；`config/defaults.json`；DATA_SEMANTICS:406 | ALG（推导权威）+ 头/实现 + 配置 + 合同 | ALG/合同/配置可随 SCI 变更同步；代码默认值属 §3 红线 |
| `ivar` 单位 **ADU⁻²** | SCI §3:29 / §7:76 / §9a:101；GLOSSARY:11；DATA_SEMANTICS:413/416 | 科学 + 词典 + 合同 | 同 5→64 的同一 claim |
| `ivar` 单位 **pixel⁻²·ADU⁻²** | SCI §2:16 | 科学（同一文件内自相矛盾） | 同上 |

---

## 2. 外部依据（标准/文献/参考实现 + 本轮实验）

### 2.1 参考实现：MAD→σ 常数就是 1/Φ⁻¹(3/4)，不是一个可自由截断的"经验数"

**astropy v8.0.1 `astropy/stats/funcs.py`**（`mad_std`，逐字，来自 <https://raw.githubusercontent.com/astropy/astropy/main/astropy/stats/funcs.py>）：

```python
    # NOTE: 1. / scipy.stats.norm.ppf(0.75) = 1.482602218505602
    MAD = median_absolute_deviation(data, axis=axis, func=func, ignore_nan=ignore_nan)
    return MAD * 1.482602218505602
```

→ 与 SCI-NOISE-001 §5:46 的常数**逐位相同**；§14:135 的「1/Φ⁻¹(3/4)」表述与 astropy 的 NOTE 同源。**结论：冻结常数是对的，`0.6745`（4 位截断）不是它的等价写法。**

独立复算（命令见 §6.4）：`1/statistics.NormalDist().inv_cdf(0.75) = 1.482602218505602`（逐位相等）、`1/0.6745 = 1.4825796886582654`（相对差 **-1.519615110204526e-05**）——与 V12-N-03 报的 -1.5196e-05 一致。

### 2.2 参考实现：MAD 裁剪的离散性与零 MAD（GNU Astronomy Utilities 0.24，MAD clipping）

逐字（<https://www.gnu.org/software/gnuastro/manual/html_node/MAD-clipping.html>）：

```text
for a pure Gaussian distribution, the median absolute deviation will be roughly 0.67449σ
…
The algorithm of MAD-clipping is identical to σ-clipping, except that instead of σ, it uses the
median absolute deviation … Since the median absolute deviation is smaller than the standard
deviation by roughly 0.67, if you regularly use 3σ there, you should use (3/0.67)MAD=(4.48)MAD
when doing MAD-clipping. The usual tolerance should also be changed due to the differing
(discrete) nature of the median absolute deviation (based on sorted differences) in relation to
the standard deviation …
Another difference … is a special condition when the MAD becomes zero …
```

→ 两条可移植判据：①**MAD 是离散次序统计量**，其终止/收敛容差不能用 σ 类连续量的口径；②**MAD=0 必须特判**（本仓 `noise_model.cpp:96`、`:192`、`:230` 均已处理）。

### 2.3 文献：MAD 的有限样本偏差必须显式修正（这正是 5 与 64 的分野）

**arXiv:2207.12005**《Finite-sample bias-correction factors for the median absolute deviation…》摘要逐字（<https://arxiv.org/abs/2207.12005>）：

```text
The median absolute deviation is a widely used robust measure of statistical dispersion. Using a
scale constant, we can use it as an asymptotically consistent estimator for the standard deviation
under normality. For finite samples, the scale constant should be corrected in order to obtain an
unbiased estimator. The bias-correction factor depends on the sample size and the median estimator.
… The obtained estimators are especially useful for samples with a small number of elements.
```

→ **1.4826 只保证渐近无偏**；小 N 下 E[MAD_N] < σ 是已知事实，且本仓**没有**任何 c_N 修正。

### 2.4 统计口径（本轮 MC 直接验证的解析结论）

- `SE(σ̂_MAD)/σ ≈ 1.166/√N`（等价于 MAD 的渐近效率 ≈37%）。本轮 EXP-1 实测 N=1024：SE=0.036504 vs 1.166/32=0.036438 ✔
- 中位数（位置统计量）的标准误：`SE(median) ≈ 1.253·σ/√N`（1.253=√(π/2)，本仓 `snr_science.cpp:238` 已按此写）。
- `P` 个独立 patch 取中位数时：`SE ≈ 1.253·SE_patch/√P`（P=64 时约 /8）。
- **中位数不能消除系统偏差**：`E[median_P(σ̂²)] → median of the sampling distribution`，对右偏的 σ̂² 分布 **median < mean**，故小 N 的负偏差会被"稳健中位数"再放大一次（本轮 EXP-2 实测：单 patch N=5 时 E[σ̂]/σ=0.8078，而 `sqrt(median_{64}(σ̂²))/σ`=0.7488）。
- **variance 场的相对误差 ≈ 2×σ 的相对误差**（δ(v)=2δ(σ)），所以"σ 5%"对应"variance 10%"。

### 2.5 本轮实验（全部输出逐字，命令与退出码见 §6）

#### EXP-1 MAD→σ 有限样本偏差/标准误 + 5σ≤2 轮裁剪（`logs/exp1_mad_bias_mc.log`，EXIT=0）

```text
== R-5 EXP-1: MAD->sigma 有限样本性质 (纯高斯 N(0,1); K=1.4826022185056; clip=5.0 sigma x 2 轮) ==
     N    trials   E[raw]/sigma     SE_raw E[clip]/sigma    SE_clip E[drop_frac] E[n_kept/N]  rounds
     3    200000       0.672280   0.555063       0.657205   0.568007    0.069908     0.93009    0.21
     5    200000       0.821429   0.479661       0.807823   0.496516    0.035647     0.96435    0.15
     8    200000       0.888628   0.366798       0.883932   0.374691    0.009857     0.99014    0.06
    12    200000       0.927992   0.312602       0.925925   0.316513    0.003827     0.99617    0.04
    16    200000       0.948571   0.276991       0.947525   0.279112    0.001803     0.99820    0.02
    24    200000       0.966612   0.230155       0.966250   0.230918    0.000561     0.99944    0.01
    32    200000       0.975631   0.201583       0.975466   0.201935    0.000230     0.99977    0.01
    48    200000       0.982903   0.165222       0.982842   0.165354    0.000073     0.99993    0.00
    64    200000       0.987500   0.143872       0.987473   0.143927    0.000031     0.99997    0.00
   128    156250       0.993906   0.103000       0.993900   0.103012    0.000007     0.99999    0.00
   256     78125       0.996376   0.072619       0.996375   0.072622    0.000003     1.00000    0.00
  1024     19531       0.999053   0.036504       0.999051   0.036505    0.000001     1.00000    0.00
  4096      4882       0.999640   0.018247       0.999640   0.018247    0.000001     1.00000    0.00
```

**判读（关键）**：

| N/patch | 单 patch 系统偏差 | 单 patch SE | 判据（SCI §11 的 5%） |
|---|---|---|---|
| **5** | **-19.2%** | **49.7%** | 单 patch 就**已超** 5% 容差 3.8 倍 |
| 16 | -5.25% | 27.9% | 卡在门限上 |
| 24 | -3.37% | 23.1% | 通过 |
| **64** | **-1.25%** | **14.4%** | 通过，余量 4 倍 |
| 256+ | -0.36% | 7.3% | 充分 |

⇒ 「**min_samples=5**」意味着允许一个**自身系统偏差 -19%、标准误 50%** 的量进入控制点集合与全局兜底——它已经吃掉了 SCI §11/§12 全部 5% 容差预算。裁剪本身还额外引入 **-0.2%~-1.7%** 的负偏差（纯高斯下剔除 3.6% 的内点，理想值应为 5.7e-7），即"小 N + 5σ 裁剪"是**双重向下偏**。

#### EXP-2 全局兜底（= sqrt(median of 64 patch var)）的统计口径（`logs/exp2_pipeline_mc.log`，EXIT=0）

```text
== R-5 EXP-2: 全局兜底 sigma_bg_global (=sqrt(median of P=64 patch var)) 的统计口径 ==
纯高斯 sigma=1; P=64 patch, 每 patch N 个 sky 样本; T trials; 判据 = SCI-NOISE-001 §11:113 SNR-004『sigma 复现 5%』
N/patch   trials    E[g]/sigma  SE(g)/sigma P(|.|<=5%) P(|.|<=2%)    场RMS(rel)
      5     4000      0.748842     0.076269     0.0060     0.0015       0.2286
      8     4000      0.851922     0.058364     0.0503     0.0127       0.1715
     16     3906      0.928650     0.042708     0.3024     0.0981       0.1158
     24     2604      0.954128     0.036334     0.5227     0.1951       0.0942
     32     1953      0.966793     0.031479     0.6871     0.2837       0.0828
     48     1302      0.977417     0.025572     0.8633     0.3978       0.0673
     64      976      0.983170     0.022736     0.9283     0.5092       0.0570
     96      651      0.987961     0.018878     0.9739     0.6283       0.0479
    128      488      0.991642     0.015845     0.9980     0.7275       0.0399
    256      400      0.995297     0.010925     1.0000     0.9150       0.0297
   1024      400      0.998610     0.005449     1.0000     1.0000       0.0145
```

**判读**：交付给 Phase2 的是这一列 `sigma_bg_global`。

- `min_samples=5`：**E=0.749（-25.1%）**，满足 5% 门的概率 **0.6%** → 与 SCI §11 的 SNR-004 oracle **不可兼容**。
- `min_samples=64`：E=0.983（-1.7%），5% 门通过率 **92.8%**；`min_samples=96/128`：97.4%/99.8%。
- 即：**64 是"5% 容差"这一冻结判据所能支撑的量级**（严格 95% 通过率口径需 96~128，见 §5.1 反方论证）。

#### EXP-3 生产 API 直连（`lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 零改动编译）（`logs/exp3_prod_threshold.log`，EXIT=0）

```text
== R-5 EXP-3: 生产 API min_patch_samples 5 vs 64 ==
512x512, 8x8 patch = 64x64 px, 真 variance = 25.0 ADU^2 (sigma=5.0, SCI §11 SNR-004 门=sigma 5%)
每 patch 精确可用 sky 样本数 n（其余掩膜）; 60 seeds/格。

[0] 无掩膜基线 minsamp=5 : rc=0 nq=64 sp=1 degen=0 sigma=4.9893 场mean=24.8531 rms=0.0088
[0] 无掩膜基线 minsamp=64: rc=0 nq=64 sp=1 degen=0 sigma=4.9893 场mean=24.8531 rms=0.0088
[0] 判据: 两者 nq/sigma/场统计是否逐位一致 = True

n/patch minsamp  rc!=0  n_qual  spat degen field    E[sigma/5]   SE(sigma/5)      <=5%    场RMS/25
     5       5      0      58     1     0 plane      0.799023      0.076938      0/60      0.2422
     5      64      0       0     0     1 const      0.993981      0.058817     35/60      0.0958
    16       5      0      64     1     0 plane      0.920898      0.043582     12/60      0.1240
    16      64      0       0     0     1 const      0.989331      0.038275     47/60      0.0651
    32       5      0      64     1     0 plane      0.956644      0.031053     36/60      0.0854
    32      64      0       0     0     1 const      0.992545      0.023598     59/60      0.0396
    64       5      0      64     1     0 plane      0.978418      0.024390     54/60      0.0619
    64      64      0      64     1     0 plane      0.978852      0.024266     54/60      0.0610
  4096       5      0      64     1     0 plane      0.999374      0.002798     60/60      0.0075
  4096      64      0      64     1     0 plane      0.999374      0.002798     60/60      0.0075
```

**三条判读（本轮最关键的证据）**：

1. **无掩膜时两个阈值逐位同输出**（`bitwise 相同 = True`）——凡 patch 样本数远超 64 的常规帧，`min_samples` 5 与 64 **对结果零影响**。因此账本的"相差 12.8 倍"是**参数空间**的比值，不是输出空间的误差（§4.1④）。
2. 阈值只在**掩膜/小图 regime** 起作用，且 64 **在两个 regime 都更准**：
   - n=5/16/32 时，`minsamp=64` 把 patch 全拒→走**全帧稳健兜底**（degenerate=1, const），E[σ]/5 = 0.994/0.989/0.993，场 RMS 9.6%/6.5%/4.0%；
   - `minsamp=5` 反而把 5~32 样本的 patch 收进平面（spat=1），E[σ]/5 = 0.799/0.921/0.957，场 RMS 24.2%/12.4%/8.5%；
   - **"fail-closed 到全帧兜底"比"收进小样本 patch"更接近真值**——因为兜底用的是**全部**未掩膜像素（本例 320 个），样本数比单个 patch 多得多。
3. `min_samples` 的语义是**准入阈值**而非估计样本数：patch 一旦准入，估计仍用它的**全部** sky 像素。所以"5 vs 64"的真正含义是"**要不要把 5~63 样本的 patch 当控制点/兜底样本**"，而不是"用 5 个点还是 64 个点去估 σ"。

#### EXP-4 坏点污染下的裁剪有效性（同 `logs/exp2_pipeline_mc.log`）

```text
== R-5 EXP-4: 坏点污染下的裁剪有效性 (N=5 vs 64), 污染=+100sigma 宇宙线 ==
 N/patch       污染比 E[clip]/sigma E[no_clip]/sigma
       5      0.00      0.807650      0.821191
       5      0.02      0.800173      0.863321
       5      0.10      0.763221      1.062792
       5      0.20      0.695915      1.275050
      64      0.00      0.987792      0.987821
      64      0.02      0.987746      1.012859
      64      0.10      0.986514      1.138874
      64      0.20      0.983950      1.398277
```

**判读**：5σ≤2 轮裁剪在 N=64 下把 20% 重污染（+100σ）的偏差从 +39.8% 压回 **-1.6%**（裁剪有效）；在 N=5 下即使裁剪后仍有 **-30%** 偏差（小 N 的 MAD 本身失真，裁剪阈值 med±5σ̂ 也随之失真）。**"5σ 裁剪"不能成为降低 min_samples 的理由。**

#### EXP-5 生产默认掩膜（rmax=60 px）下的 patch 存活面（`logs/exp5_mask_survival.log`，EXIT=0）

```text
== R-5 EXP-5: 默认掩膜 rmax=60 px 下的 patch 存活 (8x8 网格, 空背景 N(1000,5)) ==
-- 帧 1024x1024, patch=128x128 px --
 n_stars     掩膜面积比    minsamp       rc   n_qual    n_rej     spat    degen
       0     0.000          5        0       64        0        1        0
       0     0.000         64        0       64        0        1        0
       5     0.054          5        0       64        0        1        0
       5     0.054         64        0       64        0        1        0
      20     0.216          5        0       64        0        1        0
      20     0.216         64        0       64        0        1        0
      50     0.539          5        0       64        0        1        0
      50     0.539         64        0       64        0        1        0
     100     1.000          5        0       64        0        1        0
     100     1.000         64        0       64        0        1        0
     200     1.000          5        0       57        7        1        0
     200     1.000         64        0       57        7        1        0
     400     1.000          5        0       27       37        1        0
     400     1.000         64        0       22       42        1        0
-- 帧 256x256, patch=32x32 px --
 n_stars     掩膜面积比    minsamp       rc   n_qual    n_rej     spat    degen
       0     0.000          5        0       64        0        1        0
       0     0.000         64        0       64        0        1        0
       5     0.863          5        0       43       21        1        0
       5     0.863         64        0       41       23        1        0
      20     1.000          5        0       13       51        1        0
      20     1.000         64        0       11       53        1        0
      50     1.000          5        1        0       64        0        1
      50     1.000         64        1        0       64        0        1
     100     1.000          5        1        0       64        0        1
     100     1.000         64        1        0       64        0        1
     200     1.000          5        1        0       64        0        1
     200     1.000         64        1        0       64        0        1
     400     1.000          5        1        0       64        0        1
     400     1.000         64        1        0       64        0        1
```

（"掩膜面积比"为 Σ π·rmax²/帧面积 的**上界**，未扣重叠。）

**判读**：①两个阈值的差别只在**残余 sliver patch**（1024²/400 星：27 vs 22；256²/20 星：13 vs 11），差异 2~5 个 patch，量级很小；②**真正的运行期风险不是 5 vs 64，而是 rmax=60 px 的默认掩膜**：256² 帧只要 50 颗星就整帧退化（`rc=1`、`ivar_bg_global=0`、Phase2 权重全失），而两个阈值**同样**退化。该风险不在本轮裁决范围内，但与 M3-C-003 同属"默认值口径"族，一并上报（§5.14）。

#### EXP-6 P1-002 NumPy Oracle 复跑（`logs/exp6_p1noise_oracle.log`，EXIT=0）

```text
NUMPY-ORACLE PASS: 37/37 项通过（rtol model=1e-09 / plane f32=2e-06）
```

→ 与任务书所述"37/37 PASS"一致。**其独立性审查见 §3.8 / §4.7。**

#### EXP-7 M3-A-005 复现（生产 API 直连）（`logs/exp7_det_degenerate.log`，EXIT=0）

```text
== R-5 EXP-7: 平面 LS 退化判据 |det|>1e-24 复现 (512x512, 8x8 grid, patch=64 px) ==
真 variance=25 ADU^2 (=sigma 5)。minsamp=64。控制点=满足样本数阈的 patch。

case                                  rc n_qual   spat  degen   VG[0](var)          场均值       场min       场max
A 共线(仅 py=4 一行, 8 patch x4096px)       0      8      1      0     25.53129     25.54495   25.54495   25.54495
B 近共线(8 个 + py=5 一个 x64px)             0      9      1      0     25.49043     27.09347   13.71680   40.47014
C 对照(两行各 4 个 x4096px)                  0      8      1      0     25.20274     25.63652   22.00051   29.27253
```

**判读**：A 证明 `det==0` 时 `has_spatial_field` 仍报 1 且填充值 = patch 方差的**算术平均**（25.545），与 `variance_bg_global` 的**稳健中位数**（25.531）不是同一个量；B 证明 `det>1e-24` 但几何近共线时，平面**病态外推**把权重场从 13.72 拉到 40.47（真值 25，跨度 ×2.9，边缘误差 ±62%），`has_spatial_field=1` 全程为真。**这比原判据描述的"静默 b=c=0"更严重。**

#### EXP-9 min_samples 政策对 Phase2 权重场的代价（`logs/exp9_plane_weights.log`，EXIT=0）

```text
== R-5 EXP-9: min_samples 政策对平面 variance 场的影响 (真 variance=1, T=800) ==
每 patch 样本数 n_i: p=0.5 为 4096(好), 否则 U(lo,hi)(差); 场误差=逐像素相对 RMS
   差 patch 域        A 政策5       B 政策64       C 最优加权       A 录取       B 录取
      [5,63]      0.06864      0.01045      0.01040       64.0       31.8
      [5,12]      0.11718      0.01061      0.01061       64.0       32.1
     [16,63]      0.05503      0.01056      0.01050       64.0       31.8
```

**判读**：A（=min_samples 5，全部录取 + 生产的**无权重** LS）的场误差是 B（=min_samples 64，剔除低样本）的 **5.2~11.0 倍**；而 **B 与 C（逆方差最优加权）几乎完全相等**（0.01045 vs 0.01040）。⇒ **M7-A-104 指控的"无权重 LS"确实是缺陷，但它的充分补救就是 min_samples=64，不需要改 LS 为加权**（这是 M7-A-104 与 M3-C-003 必须合并裁决的理由）。

#### EXP-8 V10-N-07 的 `(uint64_t)d`（复制三处解析器的 6 行等价代码）（EXIT=0）

```text
1.0        d=1 isfinite=1 -> (uint64_t)d = 1 (0x1)
1e999      d=inf isfinite=0 -> (uint64_t)d = 0 (0x0)
INFINITY   d=inf isfinite=0 -> (uint64_t)d = 0 (0x0)
-INFINITY  d=-inf isfinite=0 -> (uint64_t)d = 0 (0x0)
NAN        d=nan isfinite=0 -> (uint64_t)d = 9223372036854775808 (0x8000000000000000)
1e300      d=1e+300 isfinite=1 -> (uint64_t)d = 0 (0x0)
strtod("1e999") = inf isfinite=0 -> 0
```

**判读**：`1e999` 在 `strtod` 下变成 `inf`（isfinite=0），转换在本平台（g++ 14.2.0 / x86-64 / -O2）**得到 0**——即"静默把参数改成 0"，不是内存破坏；NaN 路径实测得 0x8000000000000000，但**该路径不可达**（`json_get_f64` 首字符必须是 `-` 或数字，见 §3.7）。⇒ 严重度应维持 P2，描述须由"转整 UB"订正为"非有限字面量静默变 0 / 无上限分配"。

---

## 3. 实现事实（命令 + 逐字输出 + `文件:行`）

### 3.1 默认值 `min_patch_samples = 64` 的落点（M3-C-003）

命令与输出（逐字）：

```console
$ grep -n "min_patch_samples" lib/algorithms/noise_snr/cpp/src/noise_model.cpp lib/algorithms/noise_snr/cpp/include/snr_estimator.h
lib/algorithms/noise_snr/cpp/include/snr_estimator.h:117:    int    min_patch_samples;       // patch 合格最小 sky 样本数 (默认 64)
lib/algorithms/noise_snr/cpp/src/noise_model.cpp:169:    const int min_samples = std::max(1, c.min_patch_samples);
lib/algorithms/noise_snr/cpp/src/noise_model.cpp:350:    cfg->min_patch_samples = 64;
```

实现侧逐字（`noise_model.cpp:342-355`）：

```cpp
SNR_API int snr_noise_model_v1_default_config(SnrNoiseModelConfig* cfg) {
    if (!cfg) return 3;
    std::memset(cfg, 0, sizeof(SnrNoiseModelConfig));
    cfg->patch_grid_x = 8;
    cfg->patch_grid_y = 8;
    cfg->source_mask_radius_px = 10.0;
    cfg->mask_radius_scale = 6.0;
    cfg->cosmic_clip_sigma = 5.0;
    cfg->min_patch_samples = 64;
    cfg->max_clip_rounds = 2;
    cfg->enable_spatial_field = 1;
    cfg->variance_floor = 1e-12;
    return 0;
}
```

阈值消费点逐字（`noise_model.cpp:90`、`:103`、`:107`、`:169`、`:225`）：

```cpp
    if ((int)out_samples.size() < min_samples) return false;          // :90
        if ((int)kept.size() < min_samples) return false;             // :103
    return (int)out_samples.size() >= min_samples;                    // :107
    const int min_samples = std::max(1, c.min_patch_samples);         // :169
        if ((int)all.size() < std::max(1, min_samples / 2)) {         // :225
```

⇒ SCI §8:82 的 `min_samples/2` 在 `:225` **逐字对应**，证明 SCI 的 `min_samples` 与实现的 `min_patch_samples` 是**同一个参数**（不是两个不同的东西）——这一前提成立，可以裁决。

### 3.2 默认配置就是生产路径（**关键**：不是"仅 API 兜底值"）

命令与逐字输出（`orchestrator.cpp:4688-4730`）：

```cpp
            // （NOISE-WIRE-001）：必须先加载模块默认配置，再覆盖
            // 显式元数据。传零结构体不会触发模块内部默认（P1-1 修复）。
            SnrNoiseModelConfig ncfg = {};
            const bool noise_cfg_ok =
                fn_noise_default_cfg &&
                fn_noise_default_cfg(&ncfg) == 0;
            …
            const char* gain_s = fn_kv_get ? fn_kv_get(frame_, "header", "GAIN") : nullptr;
            const char* rn_s = fn_kv_get ? fn_kv_get(frame_, "header", "READNOI") : nullptr;
            if (gain_s && gain_s[0]) ncfg.gain_e_per_adu = std::atof(gain_s);
            if (rn_s && rn_s[0]) ncfg.read_noise_e = std::atof(rn_s);
```

⇒ 生产只覆盖 `gain_e_per_adu/read_noise_e`；`min_patch_samples`、`patch_grid`、`mask_radius_scale`、`cosmic_clip_sigma` 全部走模块默认。**故 `64` 是生产实际值，`5` 只是 SCI 文本值。**（`grep -rn "min_patch_samples" lib/infrastructure/ lib/algorithms/noise_snr/src/` 在编排层零命中，见 §6.3。）

### 3.3 ivar/variance 单位与 §5:51 两读（M3-A-004 / M7-A-203）

- 实现（`noise_model.cpp:255-262`）：`ctrl_ivar[i] = 1.0 / var;`、`noise_model.cpp:212`：`out_model->ivar_bg_global = 1.0 / out_model->variance_bg_global;` → 单位只能是 `ADU⁻²`（variance 为 ADU²）。
- 兜底式两支（`noise_model.cpp:207-239`）逐字：

```cpp
    if (!patch_var.empty()) {
        std::vector<double> vc = patch_var;
        const double vmed = robust_median(vc);
        out_model->variance_bg_global = std::max(vmed, c.variance_floor);      // :210  vmed 已是方差
        …
        out_model->sigma_bg_global = sig;                                       // :234
        out_model->variance_bg_global = std::max(sig * sig, c.variance_floor);  // :235  这里才平方
```

⇒ `max(vmed_or_sig², floor)` 的**唯一自洽读法**是「(已平方的) vmed **或** (未平方的) sig 的平方」，即两个分支各写一次；写成 `(vmed_or_sig)²` 会与 §3:29 的 `variance: ADU²` 断链（比 ADU⁴）。P1-002 Oracle 也正是这样复算的（`noise_model_numpy_oracle.py:254-257`：`vg = max(float(np.median(var)), floor_v)`）。

### 3.4 平面 LS 退化判据（M3-A-005）

`noise_model.cpp:401-407` 逐字：

```cpp
        double b = 0, c = 0;
        const double det = sxx * syy - sxy * sxy;
        if (std::fabs(det) > 1e-24) {
            b = (sxv * syy - syv * sxy) / det;
            c = (syv * sxx - sxv * sxy) / det;
        }
        const double a = mv - b * mx - c * my;
```

- `det` 量纲 = px⁴（`sxx·syy`），`1e-24` 是**绝对**阈值；无 else 分支；`has_spatial_field` 在 `:263` 仅由 `enable_spatial_field && n>=4` 决定，与 `det` 无关。
- 复现见 EXP-7（A 共线 → 常量场但 flag=1；B 近共线 → 场 13.72~40.47）。

### 3.5 gain 方向与常数三套（M3-A-006 / V12-N-03 / V12-N-16）

| 位置 | 逐字 | 事实 |
|---|---|---|
| `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:465-473` | `SNR_API double snr_noise_gain_variance(double signal, double gain_e_per_adu, double read_noise_e) { if (gain_e_per_adu <= 0.0) return 0.0; const double s = std::max(0.0, signal); return s / gain_e_per_adu + (read_noise_e * read_noise_e) / (gain_e_per_adu * gain_e_per_adu); }` | ✔ 与 SCI §5:58 **逐字一致** |
| `lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp:207-208` | `// variance = signal/gain + read_noise² (e⁻ 域 → ADU²)` / `double var = signal / gain + read_noise_e * read_noise_e / (gain * gain);` | 实现对，**注释漏 `/gain²`** |
| `lib/algorithms/noise_snr/wrapper_phase1/noise_model.h:33` | `// Poisson+read noise 解析模型 (SCI §10 诊断): variance = signal/gain + read_noise²` | 注释漏 `/gain²` |
| `tests/unit/p1_noise_test.cpp`（`test_monte_carlo_poisson`） | `double mean = signal / gain;` / `double adus = electrons * gain + rdist(rng);` / `// 解析: variance = signal*gain + read_noise²  (ADU²)` / `const double analytic_var = signal * gain + read_noise * read_noise;` / `CHECK(std::fabs(r.value().variance - analytic_var) / analytic_var < 0.15);` | fixture 用 **ADU=e⁻×gain**（增益方向反于 SCI `gain=e-/ADU`），解析式随之反转；容差 ±15% vs SCI 5% |
| `lib/algorithms/photometry/cpp/src/star_matcher.cpp:21` | `static constexpr double _MAD_SCALE = 0.6745;`（`:545`、`:621` 用 `mad / _MAD_SCALE`） | 4 位截断常数：1/0.6745 = 1.4825796886582654，相对差 **-1.5196e-05** |
| `lib/algorithms/photometry/wrapper_phase1/photometer.cpp:90` | `sky_sigma = 1.4826 * dev[dev.size() / 2];` | 第三套字面量 1.4826（相对差 -1.4964e-06） |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:33` | `constexpr double kLn10 = 2.302585092994045684;` | 与下条**逐位相同** |
| `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:34` | `constexpr double kLn10 = 2.302585092994045684017991454684;` | 31 位 vs 18 位双写 |

独立复算（§6.4 逐字）：`float('2.302585092994045684') == float('2.302585092994045684017991454684')` → **True**，且 == `math.log(10)` ⇒ V12-N-16 是**纯重复**，无数值分歧。

M3-A-006 的算术证据（§6.4）：SCI 式 = 337.3333333333333；测试解析式 = 759.0；比值 = **2.25 = gain²**（gain=1.5）。即测试的"解析真值"与 SCI 诊断式相差整整一个 gain²，而测试**从未调用**生产诊断式（它调 `NoiseModel::estimate`，即经验 MAD 路径）⇒ 自洽但同源，抓不住公式错。

### 3.6 PSF 状态口径（M6a-D-007）

```console
$ grep -rn "SNR_QF_PSF_OK" lib/ tests/
lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:4392:                if (psf_status == 0.0 || psf_status == 3.0) qf |= SNR_QF_PSF_OK;
lib/algorithms/noise_snr/cpp/include/snr_estimator.h:412:    SNR_QF_PSF_OK          = 1u << 0,  // PSF 拟合状态有效 (status==0 或 3)
$ grep -n "status != 0" lib/algorithms/noise_snr/cpp/src/noise_model.cpp
321:        if (status != 0.0) {
```

- 同一公共头 `snr_estimator.h:82-83` 又写：`uint32_t fit_status; // 0=ok, 1=rejected(status!=0), 2=saturated/quality flag, 3=invalid input 行` → **头内自相矛盾**。
- 消费侧 `lib/algorithms/coverage/src/upm.cpp:179-188` 逐字：

```cpp
    // Phase1 quality_flags：1=PSF_OK 2=saturated 4=has_saturated
    // 8=photo_matched 16=photo_rejected
    if (flags & 16u) return 0.0;          // photo_rejected -> 不可信
    if (flags & 2u) return 0.1;           // saturated -> 低可信
    if (flags & 1u) return 1.0;           // PSF_OK
    if (flags == 0u) return 0.5;          // 未知 -> 中性偏低（禁止默认为高权）
    return 0.5;
```

⇒ `status==3`（ALG §11.2 冻结为 `DPSF_FIT_ITERATION_LIMIT` = 未收敛）会拿到 **1.0 全权重**，而不是"未知"的 0.5。同时 `DATA_SEMANTICS:553` 明确"PHOTOMETRIC 仅 status=0 行入匹配"，`:765` 的 star_measurements 过滤才是 `status∉{0,3}` ⇒ **同一状态码在链上有两套消费口径**。

### 3.7 三个 `module_entry` 的 JSON 解析器（V10-N-07）

`lib/algorithms/drizzle/hips/src/module_entry.cpp:191-196`（`lib/algorithms/drizzle/src/module_entry.cpp:168-173`、`lib/infrastructure/gaia_xpsd_client/src/module_entry.c:169-174` **逐行相同**）逐字：

```cpp
static uint64_t json_get_u64(const char* obj, const char* key, int* found) {
    int f = 0;
    double d = json_get_f64(obj, key, &f);
    if (!f || d < 0.0) { if (found) *found = 0; return 0; }
    if (found) *found = 1;
    return (uint64_t)d;
}
```

`json_get_f64`（同文件 `:173-188`）：`if (*p != '-' && (*p < '0' || *p > '9')) return 0.0;` … `double d = strtod(p, &end);` ⇒ **"nan"/"inf" 字面量不可达**（首字符检查挡住），但 `1e999` 可达并为 `inf`。
`json_get_u64_array`（同文件 `:201-229`）：先 `int count` 扫全串计数，再 `malloc(count * sizeof(uint64_t))`，**无任何上限**；调用方（如 `:461-462`）是在分配**之后**才比对 `an != n_tiles`。

### 3.8 P1-002 NumPy Oracle 的独立性事实（M3-F-001）

文件 `lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py`（336 行，EXIT=0，37/37 PASS）。逐字要点：

```python
MAD_TO_SIGMA = 1.482602218505602   # NOISE_ESTIMATION.md §3:46 / §9:91（MAD→σ 唯一常数）   # :63
RTOL_MODEL = 1e-9                  # NOISE_MODEL.md §15:140 承诺 rtol 1e-9                  # :64
```
```cpp
    SnrNoiseModelConfig cfg;                       // DRIVER :94-98 逐字段覆盖默认
    snr_noise_model_v1_default_config(&cfg);
    cfg.patch_grid_x = gx; cfg.patch_grid_y = gy;
    cfg.min_patch_samples = minsamp; cfg.cosmic_clip_sigma = clip;
```
```python
        out_a = run_case(exe, "A_plane_outlier", data_a, 8, 8, 32, 5.0, 2, 1e-12, tmp)   # :306
        out_b = run_case(exe, "B_blank_gauss",  data_b, 8, 8, 32, 5.0, 2, 1e-12, tmp)   # :307
```

**事实清单**：
1. ✅ 它**不调用**生产实现：生产源被编译成"被测对象"，参考值由 NumPy 复算（符合 ENG_SPEC §5.1 的机械要求）。
2. ⚠️ 但它是**同算法转写**（transcription）：patch 几何、过滤、裁剪轮数语义、floor、平面 LS 全部照实现的读法重写；**抓得住编码错，抓不住"公式/默认值错"**。
3. ❌ 常数是**抄**的（`:63` 直接写字面量），不是第一性原理导出——而导出只需一行：`1.0/statistics.NormalDist().inv_cdf(0.75)`（本机实测逐位相同）。
4. ❌ **default_config 被逐字段覆盖**（DRIVER `:94-98`），且两个用例都传 `minsamp=32`（`:306-307`）⇒ **对 `min_patch_samples` 5↔64 完全无区分力**（EXP-3[0] 已证无掩膜时两者逐位同输出，故即使不覆盖也测不出）。
5. ❌ 只有 2 个数据用例（H=W=64、8×8 网格、minsamp=32 → 每 patch 恰 64 px，阈值取一半），无边界/退化/离群比例梯度矩阵。

**独立性结论：满足"不调用生产实现"，不满足"第一性原理"。** 它现在的正确定位是**实现回归对拍（differential test）**，不是 SCI §11:117 所承诺的科学 Oracle。

---

## 4. ⭐ 门禁自审（这条门/判据本身是否正当）

### 4.1 M3-C-003（P0）的门

| 审查项 | 判定 |
|---|---|
| ① 有无权威依据 | **部分**。原判据引的是**旧宪章** §1.1「SCI 优先于 ALG 优先于代码」+§1.2+§12.3-10（见 `问题扫描/REBASE_TABLE.md:238`），而现行 `ASTROCS_DESIGN.md` §0 **没有这句话**（§0 只说 docs/science 是"科学公式（权威）"、docs/algorithms 是"算法推导（权威）"）。现行链上真正可用的是 §0:31「Agent 无权放宽、重新解释或"为通过检查而改写"」+ `ENGINEERING_SPEC.md:25`「精度与默认容差**不可修改**」⇒ **结论不变（该冲突必须由负责人裁决），但引据必须换成现行条款**。 |
| ② 可测量、可复现 | **否（按原文）**。REBASE_TABLE:238 记录的命令是 `grep -n "min_patch_samples = 64" lib/snr_estimator/cpp/src/noise_model.cpp` —— 本机 `ls lib/snr_estimator` → **"没有那个文件或目录"**（退出码 2）。正确路径是 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp`（P1 迁移后）。**门禁的"证据"在当前树上不可原样复跑**；同型问题见 M3-A-006 的 `lib/phase1/noise/noise_model.cpp`（同样不存在）。 |
| ③ 误判方向 | **是（方向正确、处方错误）**。冲突客观存在（SCI 5 vs 实现 64），但门把它定性为"实现违背冻结 SCI ⇒ 改代码"，而 EXP-1/2/3/9 证明**照 SCI 改成 5 会把 -19% 的单 patch 偏差、-25% 的全局偏差、×6.6 的权重场误差写进生产**（并直接违反 SCI 自己的 §11 5% oracle 与 §7「不产生伪有效权重」）。正确方向是**改 SCI**（升为 64 + 变更 claim）。 |
| ④ 阈值/量测域是否定义清楚 | **否**。"相差 12.8 倍"是**参数空间**的比值，未定义量测域（哪个输入、哪类帧、什么统计量受影响）。EXP-3[0] 实测：常规（无掩膜）帧上两个阈值**逐位同输出**，输出空间差异 = 0；只有在"patch 残余样本 5~63"的掩膜/sliver regime 才有差异（场 RMS ×6.6）。用"12.8×"当严重度是**量纲错配**。 |
| ⑤ 误报/漏报 | **误报**：把它当"代码缺陷"会修错对象。**漏报**：真正 P0 的是"SCI §4 的默认值在统计上不成立"（§11 5% oracle 通过率 0.6%）以及"§5:51 兜底式记号不可判读"，而门未识别。 |

**门应为 X**：拆成两条——
- **门 A（文档同步，P1）**：「任何默认值必须在 SCI 与 ALG/registry/config 三面一致」——可机检（文本比对），当前 **FAIL**（5≠64），修法=改 SCI 文本。
- **门 B（科学有效性，P0）**：「SCI 冻结的默认参数必须能通过 SCI 自己的验收 oracle」——本条的判据应是 **EXP-2 表**（N=5 通过率 0.6% ⇒ FAIL；N=64 92.8% ⇒ PASS），修法=把 64 写进 SCI。

### 4.2 M7-A-203 的门（**判据对象写错**）

- **议题前提不成立**：派单书写「M7-A-203：`NOISE_MODEL` 的 ivar 单位两值互斥」。查 `问题扫描/REBASE_TABLE.md:107` 与 `findings/A_SCI_DEF/p2/M7_A_SCI_DEF_p2.md:31`，**M7-A-203 是 `max(vmed_or_sig², floor)` 的量纲两读**；ivar 单位两值互斥是 **M3-A-004**（两者本轮都已覆盖，但结论不同）。
- **该门本身**：正当但**定级过高**。①依据：`ENGINEERING_SPEC.md:28`「禁止一个字段承载多个含义」+ `UNIFIED_MODEL` 一量一义 ⇒ 依据成立；②可测量：可（记号能否唯一解析）；③方向：**纯文档缺陷**（实现 `:210/:235` 与 P1-002 Oracle `:254` 一致，无代码歧义）；④域：明确（§5 冻结式）；⑤误报：作为 P2 勘误合理，作为"科学缺陷"是误报。
- **门应为**：「冻结公式中的**记号**必须可唯一解析（含单位标注）」——P3 级勘误，修 SCI §5:51。

### 4.3 M3-A-004 的门（ivar 单位）

- ①依据：`GLOSSARY.md:3`（`禁止两套定义并存`）+ `:11`（`ivar … ADU⁻²`，唯一术语权威）+ `DATA_SEMANTICS.md:413/416`（合同唯一权威）⇒ **依据充分且是"文档自相矛盾"**。
- ②可测量：可（同一文件 `:16` vs `:29`/`:76`/`:101` 文本比对；实现 `1.0/var` 唯一）。
- ③方向：**纯文档缺陷**（GLOSSARY 明确要求回改冲突文档）。
- ④域：清楚（单位字符串）。
- ⑤误报/漏报：无误报；漏报风险=若继续让 `pixel⁻²·ADU⁻²` 外流到 schema/产品声明，会污染下游量纲校验。
- **门正当，保留原级 P1，处方=改 SCI §2:16。**

### 4.4 M3-A-005 的门（`abs(det)>1e-24`）

- ①依据：门引 `ENGINEERING_SPEC §3`（"阈值须有唯一出处且不得静默降级"）——**逐字核对**：现行 `ENGINEERING_SPEC.md:23-28` **没有**这两句（属旧宪章/旧门措辞）。但 `ASTROCS_DESIGN.md:31`「Agent 无权放宽…为通过检查而改写」+ `ASTROCS_DESIGN.md:82`（数据对象禁止互相冒充）**足以支撑**"无出处阈值 + 标志不实"违反现行链。⇒ **依据须换，结论成立**。
- ②可测量、可复现：**是**（EXP-7 生产 API 直接复现 A/B/C 三例）。
- ③方向：**真实现缺陷**（不是文档错）：`1e-24` 无 SCI/ALG 出处，`has_spatial_field` 的语义由 `DATA_SEMANTICS:415` 定义为"空间场可用"，A 例证其恒假。
- ④阈值/量测域：**不清楚**——`det` 量纲 px⁴，绝对阈值 `1e-24` 与帧尺度、patch 数、几何都无关；正确判据应是**无量纲条件数**（`det/(sxx·syy)`）。
- ⑤误报/漏报：原判据**漏报**了 B 例（`det>1e-24` 但病态外推，场跨度 ×2.9）。
- **门应为 X**：「平面 LS 必须在**相对**条件数合格且控制点几何张成二维时启用；否则 `has_spatial_field=0` 并走全局稳健中位数」。本轮 A/B 两例可直接作为负例夹具。

### 4.5 M3-A-006 的门

- ①依据：`ENGINEERING_SPEC.md:19`「注释只写：单位、数学原因…」+ §3:25（公式不可擅改）⇒ 成立。
- ②可测量：是（注释与实现逐字比对；fixture 解析式与 SCI 式的算术比对给出精确整数比 gain²）。
- ③方向：**混合**——注释两处=文档缺陷（改注释）；单测 fixture/容差=测试缺陷（改测试）；生产诊断式与 SCI 一致=**无代码缺陷**。**不得**"改 SCI 去迁就测试"。
- ④阈值/量域：单测 ±15% 与 SCI §11/§13.4 的 5% 冲突且无豁免登记 ⇒ 门应为"测试容差不得宽于 SCI 冻结容差，除非登记豁免"。
- ⑤误报/漏报：原判据正确；漏报点是"该测试从未调用生产诊断式，所以它绿不代表公式对"。
- **门正当，处方=注释×2 + fixture/容差 + `photometer.cpp:90` 常数。**

### 4.6 M6a-D-007 的门

- ①依据：`STAR_PSF_ALGORITHMS.md` §11.2（**冻结四码语义**：3=ITERATION_LIMIT）+ `DATA_SEMANTICS.md:553`（PHOTOMETRIC 仅 status=0）+ `ASTROCS_DESIGN.md:82`（数据对象禁止互相冒充）⇒ 依据充分。
- ②可测量：是（一个 `||` 的静态比对；UPM `quality_factor` 的返回值 1.0 vs 0.5 可运行期断言）。
- ③方向：**真实现缺陷**（把失败码当有效码），注释层也自相矛盾；不是文档错。
- ④阈值/域：状态码域 0..3 由 §11.2 冻结，清楚；缺的是"哪套口径适用"的**唯一登记**。
- ⑤误报/漏报：无误报；漏报风险=UPM 全权重被给到未收敛 PSF 的帧。
- **门正当，处方：`SNR_QF_PSF_OK ⇔ status==0`；`DATA_SEMANTICS:765` 的 star_measurements 行准入（{0,3}）与"质量位"解耦并写明理由。**

### 4.7 M3-F-001 的门（Oracle 承诺）

- ①依据：`ENGINEERING_SPEC.md:52-53`「不调用生产实现的独立 Oracle 或解析解」+ `ENGINEERING_SPEC.md:120`「核心合同有独立测试」+ SCI §11:117/§15:140 ⇒ 门正当。**P1-002 交付后「仓库内无任何实现」这一 P0 事实已消除**（EXP-6：37/37 PASS）。
- ②可测量、可复现：现在可（一条命令、退出码 0）。
- ③方向：**门本身没错，但"独立性"的量测域写得太松**——机械版（不 import 生产）已被满足，实质版（第一性原理/不同信息源）未满足。若只按机械版收口，门会被**形式满足**（transcription oracle），正是 ENG_SPEC §5.1 想防的东西。
- ④阈值/域：`rtol 1e-9` 只对 FP64 model 量成立，fill 面是 float32（Oracle 已按 2e-6 处理，正确）；**但"测什么"没有域定义**——没测 default_config、没测偏差/有效性。
- ⑤误报/漏报：**漏报**（公式错、默认值错、偏差超容差都测不出）。
- **门应为 X**：「合同承诺的 Oracle 必须①不调用生产实现 ②常数/公式**独立导出** ③覆盖**冻结默认值** ④至少一个用例能判**系统偏差**（解析或 MC 区间）」。三条补丁见 §5.13。

### 4.8 V10-N-07 的门（**别题误挂**）

- R-5 主题是"噪声/SNR 与统计口径"；V10-N-07 是三个 `module_entry` 的 JSON 解析健壮性，**与统计/噪声零交集**（无 M3/M7 科学引用）。派单把它挂到 R-5 属归类误挂，**建议改挂解析/健壮性域**（V10 线自身）。
- 门的事实核验：①"数组先 count 后 malloc 无上限" = 成立（`:201-229`）；②"`(uint64_t)d` 无 isfinite" = 成立（三处逐行相同）；③"→ 转整 UB" = **表述过强**：生产平台实测 inf→0（静默），NaN 路径因 `json_get_f64` 的首字符检查**不可达**（EXP-8）。
- **门应为 X**：「非有限/超范围数值字面量必须显式拒绝（`isfinite` + 2^53 上限），数组 count 必须有上限」——P2 维持，描述订正。

---

## 5. 建议裁决（唯一推荐结论 + 置信度 + 反方论证 + 最小改动路径 + 影响面）

### 5.1 M3-C-003（P0）：**不改代码；把 SCI §4 的默认值升为 64**

**唯一推荐结论**：`min_patch_samples`（SCI 名 `min_samples`）默认值认定为 **64**，以 **SCI 变更 claim** 形式写入 `docs/science/NOISE_MODEL.md` §4:37；生产代码、头文件、`config/defaults.json`、`DATA_SEMANTICS` **一律不动**。ALG §13.2:151-153 的「不改 SCI，以代码为准登记」**改写**（这是本次唯一必须废除的倒置表述）。**置信度：高。**

**MC 支撑的推导（写进 SCI 的"依据"栏）**：以 §11:113/§12:140 冻结的 5% oracle 为判据，取 8×8 网格、P=64 patch：`min_samples=64` ⇒ 单 patch 系统偏差 −1.25%、相对 SE 14.4%（EXP-1），全局 `sigma_bg_global` 偏差 −1.7%、5% 门通过率 92.8%（EXP-2）；`min_samples=5` ⇒ 单 patch 偏差 −19.2%、全局偏差 −25.1%、通过率 **0.6%** ⇒ 5 与 SCI 自己的验收条款不可兼容（EXP-2/EXP-3）。

**反方论证（最可能被反驳的点）**：严格按"95% 通过率"读 §11，N=64 只有 92.8%，N=96/128 才到 97.4%/99.8% ⇒ 反方会说"应把默认值定为 96~128 而不是 64"。
**答辩**：①§11 是**验收 oracle**（对验证帧的判据），不是逐帧生产合同；②生产真实帧的 patch 样本数是 10³~10⁴ 量级（EXP-3 的 n=4096 行：60/60 通过、场 RMS 0.75%），64 只影响掩膜重的小样本 patch 的**准入**；③把 64 改成 96/128 会**在无实测收益的前提下改变生产行为**，且同样需要走 SCI 变更流程——**增量风险 > 增量收益**；④64 已是"能通过 5% 门"的整数级取值下沿（48→86%，64→93%），再小一档就明显掉出。

**最小改动路径**：
1. `docs/science/NOISE_MODEL.md:37`：`min_samples（patch 样本数阈）默认 5` → `默认 64`，行尾加变更 claim 注释（沿用本仓既有格式，先例见 `NOISE_MODEL.md:98`：`<!-- (R-5 裁决 2026-09-16，负责人授权；依据 reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md §2 EXP-1/2/3) -->`）。
2. `docs/science/NOISE_MODEL.md` §12/§14 增一行"默认值导出依据"（引本报告）。
3. `docs/algorithms/NOISE_ESTIMATION.md:151-153`：删除「不改 SCI，以代码为准登记」，改为「SCI §4 已按变更 claim 升级为 64；ALG 以 SCI 为准」。
4. `config/defaults.json` `noise.min_patch_samples` 的 `source/source_ref` 由 `NOISE_ESTIMATION.md:134` 改指 `docs/science/NOISE_MODEL.md:37`（值不变，仍是 64）——消除"配置以推导层为源"的同型倒置。
5. 变更 claim 记账：**需要**（属 `ENGINEERING_SPEC.md:25`「默认容差不可修改」范畴）。

**影响面**：`docs/science/NOISE_MODEL.md`（§4、§12/§14）、`docs/algorithms/NOISE_ESTIMATION.md`（§13.2）、`config/defaults.json`（source 字段）、`tests/config/test_cfg001_contracts.py`（当前只校验 `noise.source_mask_radius_px`/`noise.variance_floor` 两键，**无需改**，建议补 `noise.min_patch_samples` 正例）、`docs/contracts/DATA_SEMANTICS.md:406`（已写 64，无需改）。**代码/测试/ABI 零改动。**

### 5.2 M7-A-203：SCI §5:51 记号拆分（**不是 ivar 单位**）

**唯一推荐结论**：`NOISE_MODEL.md:51` 改写为两条显式式子（合格 patch 支 `variance_bg_global = max(vmed, variance_floor)`；全帧退化支 `= max(sig², variance_floor)`），各标单位 ADU²。**不改码。置信度：高。**
**反方**：有人认为 `vmed_or_sig²` 只是排版紧凑、读者不会误读。**答辩**：`REBASE_TABLE:107` 的 P2 条已证"两读"客观存在，且 `vmed` 是方差而 `sig` 是 σ，**同一表达式里混了两种幂次**；`GLOSSARY.md:3` 明确"禁止两套定义并存"。
**最小改动路径**：同一变更 claim（R5-NOISE-001）内改 `NOISE_MODEL.md:51`。**影响面**：无代码；无测试。

### 5.3 M3-A-004：SCI §2:16 的 ivar 单位改为 `ADU⁻²`

**唯一推荐结论**：`NOISE_MODEL.md:16` 的 `(pixel⁻²·ADU⁻²)` → `(ADU⁻²)`。**置信度：高。**
**反方**：可能有人主张"pixel⁻² 表示逐像素归一"。**答辩**：`ivar=1/variance` 且 `variance` 是 ADU²（§3:29、§7:76、§9a:101、GLOSSARY:11、DATA_SEMANTICS:413/416 六处一致），`pixel⁻²` 无任何实现依据（`noise_model.cpp:261` 只有 `1.0/var`）。
**路径**：同 claim 内改一行。**影响面**：`contracts/schemas/**` 本轮 grep 未发现 ivar 单位声明（仅 `DATA_SEMANTICS` 文本，已是 ADU⁻²）⇒ 无连带改动。

### 5.4 M3-A-005：`det` 判据改相对条件数 + 标志回退

**唯一推荐结论**：把 `fill_impl` 的 `fabs(det) > 1e-24` 改为**无量纲相对判据**，并在 **build 阶段**（`noise_model_impl` 控制点数组生成后，`:263` 处）用同一判据决定 `has_spatial_field`：

```text
det = sxx·syy − sxy²;   若 det <= eps_rel · sxx · syy   (建议 eps_rel = 1e-12)
  ⇒ has_spatial_field = 0（fill 自动走 variance_bg_global/ivar_bg_global 全局常量）
```

**置信度：高**（有 EXP-7 正/负例复现）。
**反方**：`1e-24` 从未在生产触发（共线几何罕见），"不改也能跑"。**答辩**：①它**已经**可触发（EXP-7 B 例只需"一行 patch + 一个异行 patch"，这在重掩膜帧里正是 EXP-5 实测的存活形态）；②触发后果是 Phase2 权重场 ±62%，远超所有科学容差；③`has_spatial_field` 是 `DATA_SEMANTICS:415` 定义的字段，恒假的字段属"一个字段承载多个含义"（`ENGINEERING_SPEC.md:28`）。
**最小改动路径**：`noise_model.cpp` 两处小改（`:263` 判据、`:403` 同判据）+ 新增 `DISP-NOISE-010` 登记 + 一个负例夹具（共线/近共线控制点 → `has_spatial_field=0` 且场为常量）。**影响面**：`docs/algorithms/NOISE_ESTIMATION.md` §13.3 增条；`docs/science/NOISE_MODEL.md` §5 增一句"几何退化 ⇒ 全局常量场"；回归测试加负例；**不需要** SCI 数值变更（SCI 未规定 det）。

### 5.5 M3-A-006：注释、fixture、常数三处对齐

**唯一推荐结论**：①`wrapper_phase1/noise_model.h:33` 与 `noise_model.cpp:207` 注释补 `/gain²`；②`tests/unit/p1_noise_test.cpp` 的 Poisson MC fixture 改为 SCI 约定（`electrons ~ N(signal·gain, signal·gain)`、`adus = electrons/gain + N(0, rn/gain)`），解析式改回 `signal/gain + (rn/gain)²`，容差由 ±15% 收到 SCI 的 5%（8192 样本下 MAD 的 SE≈1.3%，5% 可达）；③`photometer.cpp:90` 的 `1.4826` 改用冻结常数 `1.482602218505602`。**置信度：高。**
**反方**：改 fixture 会"动历史期望值"。**答辩**：现 fixture 的 `signal*gain` 与 SCI §5:58 的 `signal/gain` 相差 gain²=2.25，是**方向性错误**而非精度问题；`ENGINEERING_SPEC.md:25` 要求以 SCI 为准。
**最小改动路径**：3 文件小改 + 1 条登记。**影响面**：`lib/algorithms/photometry` 的 `sky_sigma` 变化 1.5e-6 相对（可忽略）；无 ABI 变化。

### 5.6 M7-A-104：随 5.1 一并解决，**不改 LS 为加权**

**唯一推荐结论**：保留生产"无权重平面 LS"，以 `min_samples=64` 作为其**充分前提**；不允许为了"给 5 样本 patch 加权"而引入加权 LS（那会新增一条未冻结的科学公式）。**置信度：高。**
**证据**：EXP-9 —— 政策 5 = 0.0686 / 0.1172 / 0.0550；政策 64 = 0.0105 / 0.0106 / 0.0106；最优逆方差加权 = 0.0104 / 0.0106 / 0.0105（B≡C）。
**反方**：严格讲无权重 LS 在 N 不均匀时仍非最优（B 与 C 差 ~0.5%）。**答辩**：差别 ≪ 科学容差，而改 LS 会违反 `ENGINEERING_SPEC.md:25`；性价比判给阈值。
**影响面**：无额外改动（同 5.1）；建议把 EXP-9 登记为 `M7-A-104` 的关闭证据。

### 5.7 M7-A-105：**改回 F_TEST_GAP**，不判 SCI 缺陷

**唯一推荐结论**：`docs/science/PHOTOMETRY.md:107` 的注入门**参考量是可构造的**（给定 k 生成 F_instr=k·F_syn，真值 log10 k 已知），不存在"门的残差不可计算"；缺的是**实现**（`lib/algorithms/photometry/tests/` 下无该注入用例；§11:110 的 NumPy `rtol 1e-9` 复算亦无对应实现，现有 `p1phot_oracle.hpp` 是 C++ 同语言复刻）。⇒ 归类改回 F_TEST_GAP（测试覆盖缺口），派 P1-PHOT 补测。**置信度：中。**
**反方**：若真有测试以"实现输出的 scale"反推 k 再注入，则同义反复成立。**答辩**：本轮 `grep -rn "log10 k|合成注入"` 在 `tests/` 与 `lib/algorithms/photometry/tests/` **零命中**，判据无从落地；在无实现的前提下判"SCI 参考量未定义"属**判据先行**。
**影响面**：无 SCI 改动。

### 5.8 M7-A-106：SCI 补"零点标准误 ≠ 逐星散度"

**唯一推荐结论**：`PHOTOMETRY.md:29`/`:97` 增补定义：`sigma_residual` 是**逐星回归散度**（dex），`sigma_cal_rel = ln10·sigma_residual` 是**散度**而非零点误差；零点（median 位置）的统计标准误为 `sigma_kappa,stat ≈ 1.253·sigma_residual/√N_eff`（1.253=√(π/2)）。**代码已实现该式，无码改**（`snr_science.cpp:236-246`、`noise_model.cpp:292-296`）。**置信度：高。**
**反方**：旧字段名已在下游使用，改语义有兼容风险。**答辩**：本推荐**不改字段值**（`sigma_cal_rel` 仍 = ln10×散度），只补定义 + 禁混条款；零点标准误早已由 `snr_phot_cal_quality` 输出为 `sigma_location_se_dex/mag`。
**最小改动路径**：`PHOTOMETRY.md` 两行 + `DATA_SEMANTICS` §14 登记 `sigma_location_se_*`（若尚未登记）。**影响面**：`docs/contracts/DATA_SEMANTICS.md:499` 一行注记。

### 5.9 M6a-D-007：`SNR_QF_PSF_OK ⇔ status==0`

**唯一推荐结论**：位定义改为"**status==0（DPSF_FIT_OK）** 才置 PSF_OK"；`orchestrator.cpp:4392` 删 `|| psf_status == 3.0`；`snr_estimator.h:412` 注释同步；`DATA_SEMANTICS:765` 的**行准入** `status∉{0,3}` 保留但加注"准入 ≠ 质量有效：status=3 行不得置 PSF_OK"。**置信度：高。**
**反方**：§11.2 说 status=3 "仍回填当前最优参数"，参数也许可用，故给 PSF_OK 有工程理由。**答辩**：①该码名为 `DPSF_FIT_ITERATION_LIMIT`，`STAR_PSF_ALGORITHMS` §11.2 把它列在"**拟合失败语义**"表内；②`DATA_SEMANTICS:553` 已规定 PHOTOMETRIC 仅 status=0 入匹配——同一码在同一链上不能既"失败"又"OK"；③若要"参数可用"语义，必须**新增一个位**并登记，不能复用 PSF_OK。
**最小改动路径**：`orchestrator.cpp` 一行 + `snr_estimator.h` 注释一行 + `DATA_SEMANTICS:765` 注记。**影响面**：`upm.cpp:183` 的权重由 1.0 降为 0.5（未收敛 PSF 星），`lib/algorithms/coverage` 的回归期望需复跑；SNR 产品 quality_flags 位 1 的取值分布变化需在证据里说明。

### 5.10 V12-N-03：统一到 `Φ⁻¹(3/4)` 的精确值

**唯一推荐结论**：全仓该换算只保留一个常数 `1/Φ⁻¹(3/4) = 1.482602218505602`（等价写法 `0.6744897501960817`）：`star_matcher.cpp:21` 的 `0.6745` 改为 `0.6744897501960817`，SCI-PHOT 的 `0.6745` 字样与 `NOISE_MODEL.md:135` 的"同源"表述同步订正（变更 claim）。`sigma_residual` 相对变化 **+1.52e-5**（远小于 5% 科学容差）。**置信度：中**（科学上无关紧要，属一致性问题；"只改声明不改数"的替代路径会使两套常数并存，违反 `GLOSSARY.md:3`，故不取）。
**反方**：改动跨模块交付量（`sigma_residual`→`sigma_mag`→`sigma_cal_rel`→帧 QA），收益仅 1.5e-5 ⇒ 不值。**答辩**：本轮同时发现 `photometer.cpp:90` 的第三套 1.4826，三套并存本身违反 `ENGINEERING_SPEC §3`「不得多源」；一次收敛的代价是一行代码 + 一处测试锚（`p1phot_tests_properties.cpp:234` 的 P4 复算用的是同一个 0.6745，须同步）。
**影响面**：`lib/algorithms/photometry/**`（1 行 + 头注释）、`docs/science/PHOTOMETRY.md:20/22/52/60/127`、`docs/science/NOISE_MODEL.md:135`、`docs/contracts/DATA_SEMANTICS.md:499`。

### 5.11 V12-N-16：`kLn10` 单一定义

**唯一推荐结论**：在同一模块内保留**一处** `constexpr double kLn10`（提到共享内部头，或由 `snr_science.cpp:33` 引用 `noise_model.cpp:34` 的定义），另一处删除。两位数字面量在 double 下**逐位相同**（§3.5 复算 = True，且 == `math.log(10)`），故**零数值变化**。**置信度：高。**
**反方**：两处都是同一 double，改它属洁癖。**答辩**：风险是"单边后续修改"漂移（正是 V12-N-16 的登记理由），修复成本≈0；`ENGINEERING_SPEC §3` 禁同一常数多源。
**影响面**：2 文件；无行为变化。

### 5.12 V10-N-07：加 `isfinite`/上限（P2，描述订正）

**唯一推荐结论**：三处 `json_get_u64` 增加 `!std::isfinite(d) || d > 9007199254740992.0`（2^53）⇒ `found=0`（显式拒绝）；`json_get_u64_array` 增加 count 上限（或先校验后分配）。**描述订正**：由"转整 UB"改为"非有限/超范围字面量**静默变 0**（EXP-8 实测）+ 无上限分配"。**置信度：中。**
**反方**：这是解析器硬化，与 R-5 主题无关，应移出本条线。**答辩**：同意移出主题（建议改挂解析健壮性域），但**事实核验已完成**，一并给出最小修法避免二次派单。
**影响面**：3 个 `module_entry`（hips/drizzle/gaia_xpsd_client）；无科学语义变化。

### 5.13 M3-F-001：Oracle 独立性补丁（3 项）

**唯一推荐结论**：**承认 P1-002 的交付有效**（"承诺无实现"事实已消除，可关闭该子项），但把"独立 Oracle"的达标口径提高为以下 3 项，由后续任务补齐：
1. `noise_model_numpy_oracle.py:63` 的 `MAD_TO_SIGMA` 改为 `1.0/statistics.NormalDist().inv_cdf(0.75)`（本轮实测与冻结值逐位相同，证明"第一性原理可得"）；
2. 增一例 **default_config 契约断言**：`snr_noise_model_v1_default_config` 的 8 个字段与 SCI/ALG/registry 逐字段比对（这样 `min_patch_samples` 5↔64 这类冲突会被抓住）；
3. 增一例 **公式级偏差用例**：纯高斯 patch（N=64）的 `σ̂/σ` 必须落在 EXP-1 给出的 MC 区间内（例如 `0.9875 ± 2×0.1439/√T`），从而能判"公式错"，而不只是"抄写错"。
**置信度：高。**
**反方**：这些用例与实现强耦合、维护成本高。**答辩**：②③ 恰是 ENG_SPEC §5.1 与 SCI §11 要求的"科学不变量/性质测试"，② 的成本是 8 行断言。
**影响面**：`lib/algorithms/noise_snr/tests/p1noise/**`、`eng/ci/checks.json` 中该用例的注册（若以 ctest 目标接入）。

### 5.14 附带发现（需登记，不属本轮任何原议题）：rmax=60 px 默认掩膜导致整帧退化

EXP-5 实测：256×256 帧、50 颗星 ⇒ `rc=1`（`ivar_bg_global=0`，Phase2 权重全失），且**与 min_samples 取值无关**。默认掩膜半径 `rmax = max(1,10)·max(1,6) = 60 px`（`noise_model.cpp:144-147`、ALG §13.2:154）在 256² 帧上单星即覆盖 17% 面积。**建议**：单独立条（"默认掩膜几何在中小帧上的可用域"），候选修法两条（按 PSF FWHM 标定 r0 而非固定 60 px；或把 `n_qualified==0` 的 `degenerate`/`rc` 语义登记为"可接受的 fail-closed"）。本轮**不代为裁决修法**：其权威落点在 `docs/science/NOISE_MODEL.md` §6:67 的"掩膜半径与星亮度解耦"冻结条款，改动需负责人批准。

---

## 6. 证据清单（全部命令、退出码、产物路径）

### 6.1 环境与基线

```console
$ cd "/workspace/Astro CS Database" && git rev-parse HEAD && git rev-parse --abbrev-ref HEAD
e9b31285af6dd6c9bb2636f9e4d707a433f8abd7
main
$ python3 -c "import sys,numpy;print(sys.version.split()[0],numpy.__version__)"; g++ --version | head -1
3.13.5 2.2.4
g++ (Debian 14.2.0-19) 14.2.0
$ git status --porcelain run/            # 空输出 ⇒ 本轮产物全部在忽略区
$ git check-ignore -v run/PROJECT-GOVERNANCE-01/R-5/logs/exp1_mad_bias_mc.log
.gitignore:19:run/*	run/PROJECT-GOVERNANCE-01/R-5/logs/exp1_mad_bias_mc.log
$ git status --porcelain lib/algorithms/noise_snr docs/science/NOISE_MODEL.md docs/algorithms/NOISE_ESTIMATION.md config/defaults.json
（空输出 ⇒ 本域受控文件无改动）
```

（`git status --porcelain` 全仓显示的若干 `docs/**` 修改为**进场前既有**状态；本轮未写任何受控文件，亦未执行任何 git 写操作。）

### 6.2 实验（脚本与日志均在 `run/PROJECT-GOVERNANCE-01/R-5/`）

| ID | 命令 | 退出码 | 日志 |
|---|---|---|---|
| EXP-1 | `cd run/PROJECT-GOVERNANCE-01/R-5 && timeout 110 python3 exp1_mad_bias_mc.py > logs/exp1_mad_bias_mc.log 2>&1; echo EXIT=$?` | 0 | `run/PROJECT-GOVERNANCE-01/R-5/logs/exp1_mad_bias_mc.log` |
| EXP-2/4 | `… timeout 110 python3 -u exp2_pipeline_mc.py > logs/exp2_pipeline_mc.log 2>&1` | 0 | `…/logs/exp2_pipeline_mc.log` |
| EXP-3 | `… timeout 110 python3 -u exp3_prod_threshold.py > logs/exp3_prod_threshold.log 2>&1`（内部 `g++ -std=c++17 -O2 -I lib/algorithms/noise_snr/cpp/include exp3_driver.cpp lib/algorithms/noise_snr/cpp/src/noise_model.cpp lib/algorithms/noise_snr/cpp/src/snr_science.cpp -pthread`，**生产源零改动**） | 0 | `…/logs/exp3_prod_threshold.log`，`…/exp3_driver.cpp` |
| EXP-5 | `… timeout 110 python3 -u exp5_mask_survival.py > logs/exp5_mask_survival.log 2>&1` | 0 | `…/logs/exp5_mask_survival.log`，`…/exp5_driver.cpp` |
| EXP-6 | `timeout 115 python3 -u lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py` | 0 | `…/logs/exp6_p1noise_oracle.log`（`NUMPY-ORACLE PASS: 37/37`） |
| EXP-7 | `… timeout 110 python3 -u exp7_det_degenerate.py > logs/exp7_det_degenerate.log 2>&1` | 0 | `…/logs/exp7_det_degenerate.log` |
| EXP-8 | `g++ -std=c++17 -O2 exp8_u64_ub.cpp -o /tmp/r5_ub && /tmp/r5_ub` | 0 | 输出见 §2.5；源 `…/exp8_u64_ub.cpp` |
| EXP-9 | `… timeout 110 python3 -u exp9_plane_weights.py > logs/exp9_plane_weights.log 2>&1` | 0 | `…/logs/exp9_plane_weights.log` |

（EXP-9 前两版脚本分别因 per-trial Python 循环超时（EXIT=124）与对照设计缺陷作废，最终版为向量化 NaN-padding 设计，EXIT=0；作废过程不影响结论，最终版脚本即上表所列文件。）

### 6.3 文本证据命令（只读）

```console
$ grep -n "min_patch_samples" lib/algorithms/noise_snr/cpp/src/noise_model.cpp lib/algorithms/noise_snr/cpp/include/snr_estimator.h
$ grep -rn "min_patch_samples|SnrNoiseModelConfig" lib/infrastructure/ lib/algorithms/noise_snr/src/          # 编排层零命中
$ sed -n '4688,4730p' lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
$ grep -rn "SNR_QF_PSF_OK" lib/ tests/
$ grep -n "status != 0" lib/algorithms/noise_snr/cpp/src/noise_model.cpp
$ sed -n '179,188p' lib/algorithms/coverage/src/upm.cpp
$ sed -n '191,229p' lib/algorithms/drizzle/hips/src/module_entry.cpp
$ sed -n '168,174p' lib/algorithms/drizzle/src/module_entry.cpp
$ sed -n '169,180p' lib/infrastructure/gaia_xpsd_client/src/module_entry.c
$ ls lib/snr_estimator                # → "没有那个文件或目录"（退出码 2）⇒ 账本复跑路径过期，见 §4.1②
$ grep -n "_MAD_SCALE" lib/algorithms/photometry/cpp/src/star_matcher.cpp
$ grep -n "1.4826" lib/algorithms/photometry/wrapper_phase1/photometer.cpp
$ grep -n "kLn10" lib/algorithms/noise_snr/cpp/src/snr_science.cpp lib/algorithms/noise_snr/cpp/src/noise_model.cpp
$ sed -n '165,192p' docs/algorithms/STAR_PSF_ALGORITHMS.md
```

### 6.4 数值复算命令

```console
$ python3 -c "import statistics as s; print(repr(1.0/s.NormalDist().inv_cdf(0.75)), repr(1.0/0.6745))"
1.482602218505602 1.4825796886582654
$ python3 -c "print((1.0/0.6745-1.482602218505602)/1.482602218505602)"
-1.519615110204526e-05
$ python3 -c "import math; print(float('2.302585092994045684')==float('2.302585092994045684017991454684'), float('2.302585092994045684')==math.log(10))"
True True
$ python3 -c "s,g,rn=500.,1.5,3.; print(s/g+(rn/g)**2, s*g+rn*rn, (s*g+rn*rn)/(s/g+(rn/g)**2))"
337.3333333333333 759.0 2.25
```

### 6.5 外部依据 URL

- astropy `mad_std` 源码（常数 1.482602218505602）：<https://raw.githubusercontent.com/astropy/astropy/main/astropy/stats/funcs.py>
- astropy `mad_std` API 文档：<https://docs.astropy.org/en/stable/api/astropy.stats.mad_std.html>
- astropy `sigma_clipping.py`（`maxiters=5` 默认、`cenfunc='median'`、`stdfunc='mad_std'` 可选）：<https://raw.githubusercontent.com/astropy/astropy/main/astropy/stats/sigma_clipping.py>
- Gnuastro 0.24 MAD clipping（MAD≈0.67449σ、离散性、MAD=0 特判）：<https://www.gnu.org/software/gnuastro/manual/html_node/MAD-clipping.html>
- Gnuastro Sigma clipping：<https://www.gnu.org/software/gnuastro/manual/html_node/Sigma-clipping.html>
- arXiv:2207.12005 有限样本 MAD 偏差修正：<https://arxiv.org/abs/2207.12005>

### 6.6 交付物清单

| 路径 | 内容 |
|---|---|
| `reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md` | 本报告（唯一交付物） |
| `run/PROJECT-GOVERNANCE-01/R-5/exp1_mad_bias_mc.py` + `logs/exp1_mad_bias_mc.log` | MAD 有限样本 MC |
| `run/PROJECT-GOVERNANCE-01/R-5/exp2_pipeline_mc.py` + `logs/exp2_pipeline_mc.log` | 全局兜底口径 + 污染裁剪 MC |
| `run/PROJECT-GOVERNANCE-01/R-5/exp3_driver.cpp`、`exp3_prod_threshold.py` + `logs/exp3_prod_threshold.log` | 生产 API 阈值实验 |
| `run/PROJECT-GOVERNANCE-01/R-5/exp5_driver.cpp`、`exp5_mask_survival.py` + `logs/exp5_mask_survival.log` | 掩膜存活面 |
| `run/PROJECT-GOVERNANCE-01/R-5/logs/exp6_p1noise_oracle.log` | P1-002 Oracle 复跑 |
| `run/PROJECT-GOVERNANCE-01/R-5/exp7_det_degenerate.py` + `logs/exp7_det_degenerate.log` | det 退化复现 |
| `run/PROJECT-GOVERNANCE-01/R-5/exp8_u64_ub.cpp` | `(uint64_t)d` 行为实测 |
| `run/PROJECT-GOVERNANCE-01/R-5/exp9_plane_weights.py` + `logs/exp9_plane_weights.log` | 平面场权重政策 MC |
