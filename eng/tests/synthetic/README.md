# 合成全链层测试集（`eng/tests/synthetic/`）

**这是工具，不是裁判。** 它产出的是**红项报告**，不是流水线判决。

依据（逐字）：`run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md` §1
「测试代码是独立的一套代码集，目标是主动暴露缺陷、检验科学口径，**不作为门禁去约束代码**」；
§1「**测试是工具不是裁判**：帮助发现问题，不堆叠豁免、不用空断言充数」；§4「测试集可以接入
CI，但**以非阻塞为常态**」。另见 `docs/engineering/testing/TEST.md` §9 逐字
「执行者是人：结论由人读对抗审核给出，每条结论都附可复算的证据。**测试集不产出流水线判决**。」

## 分层里的位置

规范 §2 的六层里，本目录是**合成全链**：哈勃仿真与代数合成端到端，且逐字
「合成全链覆盖五个创新点的关键科学不变量」。`unit/` `module/` `integration/` `pipeline/`
`e2e/` 由其它车道负责，**写域不重叠**。本目录的写域是 `__init__.py`、`_kit.py`、
`tolerances.py`、`run_synthetic.py`、`README.md` 与 `test_*.py`——**一个字都不写进其它层**。

## 运行

```bash
python3 -m eng.tests.synthetic.run_synthetic                  # 报告；退出码**恒为 0**
python3 -m eng.tests.synthetic.run_synthetic --verbose        # 附带每条用例的实测读数
python3 -m eng.tests.synthetic.run_synthetic --list           # 只列元数据（意图/输入/预期/来源/注入）
python3 -m eng.tests.synthetic.run_synthetic --only p3        # 只跑 id 含该词的用例
python3 -m eng.tests.synthetic.run_synthetic --exit-code      # 开发期自查：退出码 = 红项数（**不得接 CI**）
python3 eng/tests/synthetic/tolerances.py                     # 打印容差冻结表
```

退出码默认恒 `0` 是**刻意设计**，由 `harness.verdict()` 恒返回 `warn` 保证（`05` §4）。
`--exit-code` 只是开发期自查开关，**不得**接进 CI。

## 目录

| 文件 | 内容 | 判据来源 |
|---|---|---|
| `tolerances.py` | **合成层容差冻结表（唯一源）**。通用档 + 统计口径 + `synth.p1..p5` 门限占位 | `TEST.md` §4/§4.1/§4.3、`05` §4、`04` §2 |
| `_kit.py` | 两类实验数据的构造器：哈勃仿真前向过程 + 纯解析代数合成 | `04` §2.1 / §2.2 逐字 |
| `run_synthetic.py` | 报告器（非阻塞） | `TEST.md` §9 |
| `test_*.py` | 用例（由另一车道写）。骨架复用 `eng.tests.unit.harness` | `05` §1 |

### 骨架复用（一条已登记的耦合）

本层的注册表 / 断言 / 证据累积 / 非阻塞裁决**复用 `eng/tests/unit/harness`**，
`run_synthetic.py` 只调它的 `discover("eng.tests.synthetic")`。理由与两条不变量写在
`__init__.py` 的 docstring 里。**这是写域约束的产物，不是设计选择**：若日后允许在本目录
加骨架文件，应把 `harness` 上提到 `eng/tests/harness.py` 由各层共用。
**容差表不共享**——合成层有自己的 `tolerances.py`。

## 两条硬纪律

### 1. 期望值不得来自被测实现自身

期望值只能来自四类来源（`_kit.py` 模块 docstring 与 `tolerances.py` §1 同款分类）：
① **正本条款**（`docs/science/*`、`TEST.md` 可逐行核对）；② **闭式/解析推导**；
③ **第三方独立实现**（`astropy` / `numpy`，白名单见 `TEST.md` §13）；
④ **定种子蒙特卡洛**。

`_kit.py` **是数据构造器，不是 Oracle**。它刻意把「解析真值」与「抽样实现」分成两个对象
（`AnalyticStarfield.total_flux_e` vs `.signal_e`），二者对拍才有意义；
拿实现输出去期望实现输出是空转。

⚠ **禁止转调仓内另一个前向模型**：`实验/shared/synthetic/noise_model.py` 虽实现了
`04` §2.1 的全部环节，但 `predicted_variance_adu2()` / `sky_sigma_adu()` / `canon_snr()` /
`clipped_std_adu()` 与生成器**同文件**，拿它们当期望值就是**往返自证**。
本层保持自实现（`05` §1「测试代码是独立的一套代码集」），**不 import 该模块**。

### 2. 负例必须真的能红

负例断言的是「**独立判据能抓住这个注入的缺陷**」。若注入后判据仍然通过，那条负例**无效**
——必须改造成有牙齿的版本，不是加豁免。

`_kit.forward_model(defects={...})` 提供 8 个注入面（`seeing_fwhm` / `read_noise_e` /
`sky_electrons` / `dark_electrons` / `quantize` / `saturation_e` / `pointing_scale` /
`flat_scale`），**默认值全部是「不注入」**。注入后判据必须给出**超界读数**
（不是用例失败），并与未注入时的绿读数并列可比。

## 两类实验数据与物理环节对照

逐字依据 `04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §2：

| §2.1 逐字要求 | 本层落法 | 函数 |
|---|---|---|
| 「以 HST 真实数据（本仓 M16，带 PHOTFLAM 定标）作为**纯信号模板**」 | 按窗口读 269 MB DRZ 的 64×64 子窗；`PHOTFLAM` 从头读 | `load_m16_template` |
| 「源、天光、暗流在**电子域做 Poisson 采样**」 | `λ = τ·(源×平场 + 天光面) + 暗流`，`N_e ~ Poisson(λ)` | `sample_electrons` |
| 「读出噪声做 **Gaussian**」 | 电子域 `N_e + Normal(0, RN²)`，再 `ADU = N_e/g` | `add_read_noise_adu` |
| 「电子经**增益、饱和与量化**转 ADU」 | 依次 `apply_saturation` → `quantize` | `apply_saturation` / `quantize` |
| 「加入**平场响应与天空梯度**」 | 平场**乘性**作用于光；天空梯度**加性**、梯度乘在天光电平上 | `apply_flat` / `apply_sky_gradient` |
| 「**不同透明度**」 | 显式标量大气透过率 `transparency`，乘在**入射光**上、不乘暗流 | `Scene.transparency_range` |
| 「天光」 | `sky_electrons_range`，进入 λ | `Scene.sky_electrons_range` |
| 「视宁度」 | 卷积核 FWHM 变化（`apply_pointing` 的卷积步） | `Scene.seeing_fwhm_range` |
| 「指向」 | 旋转 + 平移（`apply_pointing` 的几何步） | `Scene.pointing_dx/dy/rot_range` |
| 「生成多帧」 | 四轴逐帧独立均匀抽样，固定抽取顺序 | `render_sequence` |

| §2.2 逐字要求 | 本层落法 | 函数 |
|---|---|---|
| 「已知通量星点、已知背景、已知噪声参数」 | 闭式星场 + 解析方差；`rng=None` ⇒ 完全不抽样 | `analytic_starfield` |
| 「检验测光、方差传播、守恒映射」 | `total_flux_e`（闭式）vs `signal_e_sum`（实现）；`background_variance_e2` 闭式 | 同上 |
| 「构造『真值无效应 ⇒ 归零或报警』负例」 | `analytic_negative_arm` | `analytic_negative_arm` |
| 「天光对信噪比的影响**通过散粒噪声体现**」 | 天光只作 Poisson 入射率进 λ；**禁止**进信噪比分子 | 见下 |

### 天光口径（最硬的一条）

`NOISE_SNR.md` §4 逐字：「**分子只有源**：天光只经散粒噪声进入分母。」本层把它写死成
唯一一条数据通路：`sky → λ → Poisson 方差`。**没有**任何「天光项」进信噪比公式的写法。
`analytic_negative_arm` 给出「把天光错计入分子」的**朴素错误式**读数
（`snr_numerator_excess_e`），专门供判据在注入后超界。

### 「三项和一次抽样」≡「三项各自抽样」

`04` §2.1 逐字「源、天光、暗流在电子域做 Poisson 采样」字面上像三项各自抽一次，
本层实现的是三项**先求和再抽一次**。二者**分布等价**（泊松可加性，Newberry 1991,
PASP 103, 122）；但等价的是**分布**不是**实现**——同 seed 下逐像元实现值不同。
⇒ 用例不得把「逐项分别抽样」的结果当本层输出的期望值，只能取分布层面的统计量。

## 容差冻结表

唯一源是 `tolerances.py`，**冻结发生在写任何用例之前**（`TEST.md` §3）。
`get(key)` 对未冻结 key 抛错——不许在用例里现编容差。

- **通用档**：`F64_RTOL=1e-12`、`F64_ATOL_PER_SCALE=1e-13`、`F32_RTOL=5e-6`、
  `F32_ATOL_PER_SCALE=1e-6`、`U_F64=2⁻⁵³`、`U_F32=2⁻²⁴`、`REDUCTION_C=4`、`EXACT=0.0`，
  加 `ulp()` / `reduction_tolerance()` / `ci_half_width()`。与单元层**逐字同值**。
- **统计口径**：`synth.mc.method`（分位数法 = 非参数 bootstrap 百分位法）、
  `synth.mc.replicates = 200`、`synth.mc.abs_snr_rel_ci95 = 7.0e-3`、
  `synth.mc.aperture_flux_rel_ci95 = 2.7e-3`、`synth.mc.var_ratio_ci95 = 2.0e-1`、
  `synth.mc.dense_field_ci95 = 1.8e-2`。σ 全部由**解析式在冻结输入上**导出，不从程序输出反推。
- **创新点门限占位**：`synth.p1.linear_scale_rel=5e-2`、`synth.p1.integrated_flux_rel=1e-3`、
  `synth.p2.cross_frame_snr_spread_rel=1e-1`、`synth.p3.flux_conservation_rel=1e-6`、
  `synth.p4.dense_snr_rel=2.5e-1`、`synth.p5.seam_residual_rel=5e-2`、
  `synth.p5.sky_residual_rel=1e-2`。**全部是占位**：保守但非恒真，来源由写用例的子代理填实。
- **构造级恒等**（`0.0` 的合法使用点）：`synth.sky.shot_noise_only`（比的是**项集合**不是
  浮点量，故不涉浮点误差）、`synth.poisson.count_exact`（计数档）。

## 数据依赖（fail-closed）

`testdata/HST_M16/` 被 `.gitignore:167` 整目录忽略，`git ls-files` 实测 **0 条**——
三帧 FITS 只在工作盘上，**不在版本库**。读不到时 `load_m16_template` 抛
`TemplateUnavailable`（显式失败面），**不返回零数组**。按 `TEST.md` §13，
这类判据记为**未执行**并说明缺什么，**不以跳过冒充通过**。
纯解析代数合成臂不依赖这些文件，在任何盘上都能跑。

## 诚实边界

1. **DRZ 是多次曝光叠加图，不是单次曝光**。像元值乘 `EXPTIME` 得到的是
   「按单次曝光时长归一化后的每像元电子数」，**不是**任何一次真实曝光的电子数。
   `04` §2.1 只要求它当纯信号模板，本层就是这么用的；**不得**据此声称模板通量就是
   单帧注入通量——真值侧必须另取 `analytic_starfield`。
2. **文档债（不静默修，登记待裁决）**：
   - `实验/shared/synthetic/m16_scene.py:176` 的 `read_noise_e: 3.1` 与真实头
     `READNSEA = 3.03` 差 **2.3%**，而同文件 `:174-175` 注释自称取自 header
     ⇒ **声称与证据不符**。同一行的 `gain_e_per_adu: 1.5` 同理：头里是
     `ATODGNA..D = 1.5599999`。
   - `实验/shared/synthetic/noise_model.py` 有 8 个构造器默认值
     （`read_noise_e=5.0` / `bias_adu=1000.0` / `dark_current_e_per_s=0.02` /
     `full_well_e=120000.0` 等）**无仓内出处**，违反 `AGENTS.md` §7
     「硬编码数值按来源处置」。本层 `_kit.py` 的每个默认值都带来源标注，且都是可覆盖 kwarg。
3. **既有场景缺大气透过率轴**：`grep -rn "transparent|extinction|transmiss"
   实验/shared/synthetic/scenes/*.json` 零命中，现有场景靠 `exposure_s` 扫掠 +
   `flux_scale` 钩子顶替。本层不沿用该顶替：四轴各自独立。
4. **合成层的五轴数据不构成对产品的任何结论**：本层只判**构造数据面上的科学不变量**。
   未链接 `libacsd`、未调 CLI、未构建。
5. **`convolve_same` 与 `gaussian_psf_kernel` 的边界口径**（反射填充 / 反射边界）是
   **合成约定，不是物理断言**；对第三方实现对拍时**只在内部区域**比较。
   `astropy.convolution.Gaussian2DKernel` 的第一个位置参数是 **σ 不是 FWHM**
   （踩过：会让核宽放大 2.355 倍且不报错），正确写法见 `gaussian_psf_kernel` 的 docstring。

## 需要先构建才能跑的测试

**本目录 0 条**。本单（骨架）不链接 `libacsd`、不调 CLI、不起子进程、不编译、不跑端到端。
需要构建的对拍（`libacsd` 的实现在真实符号下是否满足被测公式）属模块层/集成层，
按 `TEST.md` §11 记为「**绑定已冻结、执行面未落位**」，不在本目录冒充已执行。

## 不做的事

- 不裁决代码、不产出阻塞退出码、不进默认构建（构建面 0 个 `add_test`）、不接 CI
  （接 CI 的方式由 T12 统一设计，届时也只取 warn）。
- 不堆叠豁免、不用空断言充数、不以「跳过」冒充「通过」。
- 不把 `_kit.py` 的输出当期望值、不转调 `实验/shared/synthetic/noise_model.py`。
- 不静默修上游的文档债（见「诚实边界」第 2 条）。