# 跨帧绝对信噪比链路科学核验报告

> 本件由"链路核验报告 ＋ 该报告的第②层独立复核（正件与补件）"合并而成：凡被复核推翻的说法不进入正文，
> 集中列在文末「已撤销的说法」；凡被复核降级的说法只以降级后的范围陈述。
> 追溯信息一律走表格的「来源成稿」列，不在正文段落里归因。

**读数入库状态口径**（与同批三份共用）：`入库` = 原始件在仓库跟踪面；`包内复算件` = 本轮独立复算脚本与输出在工作包
`独立审计/复算件/` 内、仓库跟踪面无副本；`不可复核` = 读数原始产物在 gitignore 的临时目录（`run/*`）且跟踪面无副本；
`需复测` = 必须重跑编译产物或端到端才能立证，本阶段不写成"已核验"。

---

## 1. 链路口径与公式

**该链路在最高设计中的条款**：`ASTROCS_DESIGN.md` §2.2（创新点二：跨帧绝对 SNR）、§5.3（三种重建口径由 JSON 显式指定、
产出同一物理量的稠密表示、实际生效口径记在 `snr_path_effective`）；
`:156-158`（以背景方差倒数定权会使权重不含源光子散粒项、把亮源像素过权）、`:171-174`（**加权方差必须含源项**为正向约束；
只含空背景项的方差是**背景受限**口径，只用于天光建模与误差报告）、`:181`（完整信息核 `W = a²PᵀC⁻¹P`，白噪声近似只在噪声白时成立）、
`:188-191`（帧内空间变化由 `sparse_snr_layer` 承载）；
科学正本 `docs/science/CONTROL_WEIGHT_SNR.md`、`docs/science/NOISE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`、
`docs/science/UNCERTAINTY_AND_COVARIANCE.md`；插件层 `docs/plugins/algorithms_phase1/07_noise_snr.md`、
`docs/plugins/algorithms_phase2/13_integration.md`；合同面 `eng/contracts/schemas/unified/sparse_snr_layer.schema.json`、
`docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:99`。

**现行口径**（生产链 = scheduler 路径，`lib/infrastructure/scheduler/src/module_adapters.cpp`）：

```text
Phase1  逐源 σ_F,i⁻² = Σ_k P_k²/σ_k,i² ，  σ_k,i² = σ_sky,i² + (RN/g)² [仅 shot_only 声明时] + F_i·P_k/g
        frame_snr = F_ref / σ_F(ref profile) ，  F_ref,k = 10^(−0.4(m_ref − ZP_k)) ，m_ref = 6.0
        HiPS 头写 ASTROCS_FRAME_SNR = snr_reference.snr_f          （astro_sphere_sink.cpp:438-460）
Phase2  w_k = actual_snr_k² / F_ref,k² · g_k² ，  actual_snr = frame_snr × intra_snr
        现状：intra_snr ≡ 1.0（weight_chain.cpp:714，因 module_adapters.cpp:11534 `in.sparse = nullptr`）
        或 w = 逐样本 ivar（module_adapters.cpp:11936，即背景方差面的倒数）
```

**逐像素权重的真实来源面**（两条臂，口径互不相同）：

| 臂 | 权重取值点 | 上游数组与写入者 | 含不含源项 |
|---|---|---|---|
| A 逐样本 ivar | `module_adapters.cpp:11936` | HiPS `ivar/` 子产品 ← `hp_drizzle_api.cpp:1024` 读帧内 `variance` 块 ← `drizzle_engine.cpp:1649` `sumVarNum += v·w²` ← `astro_sphere_sink.cpp:547`；写入点 `module_adapters.cpp:8155`，值 = `snr_noise_model_v1_fill`（`:8100`） | **按构造不含**：`snr_estimator.h:116` 头注为"source-masked blank-sky 稳健方差"，场是 `var(x,y)=a+b·x+c·y`，自变量只有位置 |
| A′ 逐像素 corrected variance | `weight_from_corrected_variance` → `module_adapters.cpp:11903`，读 `p2_corrected_var_f*.bin`（`:10459`） | 同上背景面 ＋ UPM 校正场残差 `PΣPᵀ` | 两项都**不含源光子散粒** |
| B 帧级 SNR 链 | `module_adapters.cpp:11919` `w = snr_weights[slot]` | `:11534 in.sparse=nullptr` → `weight_chain.cpp:714 intra=1.0` → `:739 actual = frame_snr×1.0` → `:779 w = actual²/F_ref,k²` → `:783 w *= g²` | 含源项**但只在固定 `F_ref` 处求值**（`snr_science.cpp:201`，`F ≡ cfg.reference_flux_adu`）⇒ 一帧一个数，与像素 `(x,y)` 无关 |

**每个科学量的五件事**

| 量 | 单位 | 坐标系/归一化 | 精度归属 | 有效域 |
|---|---|---|---|---|
| `σ_sky` | ADU rms（解码后计数域） | 帧内/patch 场 | 稠密大面 FP32 | 空背景随机分量（掩膜外） |
| `σ_i²` | ADU² | `σ_sky² + (RN/g)² + F·P_i/g`，**读噪只出现一次** | 元数据/标量 FP64 | 声明与来源须匹配（见 DEV-06） |
| `σ_F` | ADU | `(ΣP_i²/σ_i²)^(−1/2)`；`Σ_i P_i = 1` 在**截断网格**上成立 ⇒ 是"该网格内通量"的不确定度 | 标量 FP64 | `ΣP²` 随半宽收敛极快（`half=12→60`：`0.09268653→0.09268408`）⇒ 截断不构成误差源 |
| `frame_snr` | 无量纲 | `F_ref/σ_F(ref profile)`，`m_ref = 6.0` | 稀疏/元数据 FP64 | 与天光无关、跨帧可比的**未加权原始 SNR** |
| 稀疏控制点值 | 无量纲 | 合同冻结为 `SNR_c = F_ref/σ_F,c`（`sparse_snr_semantics = "absolute_flux_type_snr"`，const） | 同上 | 生产当前**无该对象**（见 DEV-03） |
| Phase2 权重 `w(x,y)` | ADU⁻² | 叠加权应为 `1/σ_w²`，`σ_w² = σ_bg² + S_src/g` | 稠密大面 FP32 | 背景受限域才可用 `1/σ_bg²` |
| `ivar` | ADU⁻² | `1/variance`，`variance` 为 blank-sky 面 | 稠密大面 FP32 | 天光建模与误差报告；**不冒充叠加权** |
| 重建算子 | — | 三算子（自然三次样条／双线性／最近邻），须在控制点精确复现节点值、返回预测方差 | FP64 求值 | 合同要求尚未满足（DEV-05） |

**恒等式与其前提**：`w = SNR²/F_ref² ≡ 1/σ_F²` 在 SNR 定义为 `F_ref/σ_F` 时按机器精度成立
（最大相对偏差 `3.01e−16`，四组异源参数）。**该恒等式成立的前提是分子取 `F_ref`**；
它是代数恒等式，只保证换算代码不出错，**不保证生产写入的分子真的是 `F_ref`**（这正是 DEV-03 的落点）。

---

## 2. 证据清单

只登记本轮取证过、且文献池判定不为「关联不成立／标识符错误／版本不符／UNPROVEN」的来源；
pinpoint 未核到者标 ○，且不得写"原文如此"。

| # | 证据 | 级别 | 出处 | 支撑的主张 | 适用域 | 读数入库 | 来源成稿 |
|---|---|---|---|---|---|---|---|
| S1 | Horne 1986, PASP 98, 609 | A 论文 ○ 书目/题域层 | DOI 10.1086/131801；Crossref＋OpenAlex 两通道书目四字段全等；题名即 *An optimal extraction algorithm for **CCD spectroscopy*** | 最优提取的统计结构 `F̂=ΣP d/σ² / ΣP²/σ²`、`Var=1/ΣP²/σ²` | **一维 CCD 光谱抽取**；用于成像点源属推广。**其逐像素方差式的文本面本轮未取到（ADS 全文仅影像）⇒ 不作判据锚**，只作题域与书目锚 | 书目层 | AUD-202-SNR核验 §2；文献复算-旧判批；复核-AUD202 V3 路 2 |
| S2 | Zackay & Ofek 2017 I, ApJ 836, 187 | A 论文（正文级） | DOI 10.3847/1538-4357/836/2/187（arXiv:1512.06872）；式 (3) 逐字 `V[ε_j] = B_j + (F_j T) ⊗ P_j`，`B_j` = 位置无关噪声方差（天光＋读出＋暗流） | 逐帧 matched filter 后再加权求和最优；噪声组成的分箱语义 | 点源；I/II 与 arXiv 末位序号绑定已双向核过（`1512.06872↔836,187↔I`） | 书目＋正文层（外部） | 文献复算-旧判批；AUD-202-SNR核验 AUD202-002 |
| S3 | Zackay & Ofek 2017 II, ApJ 836, 188 | A 论文 ○ 书目/题名层 | DOI 10.3847/1538-4357/836/2/188；题名逐字含 *"…Optimal for Any Purpose **in the Background-dominated Noise Limit**"* | "最优叠加"这句话**本身带**背景主导极限的限定语 | 去掉该限定语即不成立 ⇒ 不得引它支撑源受限域的结论 | 书目层 | AUD-202-SNR核验 AUD202-002；复核-AUD202 V3 路 2 |
| S4 | photutils `utils.errors.calc_total_error` | A 开源 | 文档页本轮取回；摘要逐字 "Calculate a total error array by combining a **background-only** error array with the Poisson noise of sources."，式逐字 `σ_tot = sqrt(σ_bkg² + I/g_eff)` | 成熟实现把 `σ_bkg` 明确叫 background-only，逐像素"总误差"必须再加源项 | 方法学对照（常数量级与实现约定不取自此） | 外部一手（URL 已记） | 复核-AUD202 V3 路 2 |
| S5 | Melchior et al. 2018, Astron. Comput. 24, 129–142 | A 论文 | DOI 10.1016/j.ascom.2018.07.001（arXiv:1802.10157）；两句摘要引文逐字相符 | 相关噪声与逐波段卷积的处理推导 | 支撑强度只到"提供推导/适用"，**不到"已证正确"** | 书目＋摘要层 | 文献池 §7 链路② #6 |
| S6 | Saydjari & Finkbeiner 2022, ApJ 933, 155 | A 论文 | DOI 10.3847/1538-4357/ac6875（arXiv:2201.07246）；摘要逐字（LPI similar to GPR／local covariance predicts background **and uncertainty**） | 逐像素内插**须同时给预测方差**（DEV-05 的口径先例） | 摘要层 | 书目＋摘要层 | 文献池 §7 链路② #3 |
| S7 | Akhlaghi & Ichikawa 2015, ApJS 220, 1 | A 论文 | DOI 10.1088/0067-0049/220/1/1（arXiv:1505.01664）；原文 "Let Sa and σs be the average and standard deviation of the count in Rs" | sky/σ 只用**未检像元**的局部噪声场惯例 | 限定为检测/分割语境；"占比足够"与附录节号未证 ⇒ 不得带 pinpoint | 书目＋部分正文层 | 文献池 §7 链路② #1 |
| S8 | Kelvin, Hasan & Tyson 2023, MNRAS 520, 2484–2516 | A 论文 | DOI 10.1093/mnras/stad180（arXiv:2301.05793）§3.3.2 逐字 "reduces the risk that any individual mesh element becomes severely compromised by source flux" | 背景网格尺度与单格被源污染的风险 | 只支撑该句；"mesh 决定噪声"半句无原文支撑 ⇒ 不得引 | 正文层 | 文献池 §7 链路② #4 |
| S9 | 本仓噪声/SNR 源码 | A 开源（本仓自证） | `snr_science.cpp:139-143,:177,:198-202,:209,:214-215`；`information_weight.cpp:128,:140,:190`；`snr_estimator.cpp:65-91,:80,:90,:900,:130,:304,:596`；`snr_frame_science.cpp:189-201`；`weight_chain.cpp:479-486,:529-550,:695,:714,:739,:779,:783`；`phase1_product.cpp:396` | 生产实现口径与可达性 | 静态实读 ⇒ 判口径不判生产数值 | 入库 | AUD-202-SNR核验 各条；复核-AUD202 V1/V2/V4/V5 |
| S10 | 本仓合同与门表 | A 开源（本仓自证） | `eng/contracts/schemas/unified/sparse_snr_layer.schema.json`（`const "absolute_flux_type_snr"` 与描述逐字）；`negative/n6_sparse_snr_relative_semantics.schema-violation.json`（控制值 `1.0/0.878`）；`eng/ci/ledgers/dead_config_keys.json:34,:110`；`eng/ci/prod_wiring_baseline.json:165`；`ACCEPTANCE_SPEC.md:60`；`eng/ci/checks.json:4359-4364,:4547-4625,:4722` | 冻结语义、死键与不可达登记、验收行 | 合同与登记面 | 入库 | AUD-202-SNR核验 AUD202-004/009；复核-AUD202 V2/V4/V5 |
| S11 | 本仓绝对 SNR 实验归档 | B 本仓实验 | `实验/absolute-snr/results/b1_sky_scan.json`、`b2_noise_terms.json`、`b4_integration.json`、`b3_domain_map.json`、`exp04_e1_analytic.json`（固定 seed 20260921，跟踪） | 天光单调性、双计偏差、配对恒等；以及"实验三臂的对象是 σ̂ 而非 SNR"（DEV-04 的反证锚） | 各 JSON 的冻结配置内 | 入库 | AUD-202-SNR核验 §3；复核-AUD202 V4 (c) |
| S12 | 独立单位与传导推导 | 独立推导 | 包内复算件 `独立审计/复算件/aud202/aud202_recompute.py`、`独立审计/复算件/aud202-v/v1_weight_degeneracy.py`、`独立审计/复算件/aud202-rest/v5_units_and_tautology.py`、`v6_doublecount.py` | 单位链闭合、过权解析式 `R = 1 + S_src/(g·σ_bg²)`、恒真判据的换源自测、双计偏差闭式 | 通用 | 包内复算件 | AUD-202-SNR核验；复核-AUD202 V1；复核-AUD202-补 V5/V6 |
| S13 | 负责人裁决面（既有记录） | B 本仓实验/文档 | `实验/absolute-snr/docs/snr-propagation-design.md:356`（"帧级 × 帧内相对因子"分解已取消之裁决记录）、`:541`（预测方差要进权重分母） | `weight_chain.cpp` 现算子实现的是**已被取消的**分解 | 只到仓内登记面 | 入库 | 复核-AUD202 V2 补充 |

**未入本清单的来源及原因**：`arXiv:1506.00837`／`ApJ 821, 11` 与 The Tractor 的绑定（文献池判 `标识符错误`，两号均与他文无关，
只可保留 `ASCL:1604.008` 软件形态）；`DOI 10.1051/0004-6361/202243797`（A37 被记为"DR3 内容总括/测光验证"＝`关联不成立`）；
Schmitt+2010 以 arXiv 号引用（`版本不符`，须改引 `A&A 517, A26` 且 §3.2.1/Anscombe 那句作废）；
Nguyen+2016 的"0.56% 原文实测量"与"原文明确不可外推到宽带"（`关联不成立`，只可引 ±30 nW m⁻² sr⁻¹ 并标本项目推论）；
Liu & Miller 的 §4.3 与 knot 脚注（未证）；`DOI 10.1002/9780470316481` Serfling §2.3.2（`UNPROVEN`）；
Holland & Welsch 的"§2 效率表"（`UNPROVEN`）；Jacob et al. 的"两旋钮分离"（未证，须改挂 SWarp 文档）；
PixInsight `ImageWeighting` 式 [16][18][20]（本轮未独立取回原文，只有本仓文档转引 ⇒ 不作 A 级行）。
文献池内其余链路②条目（Stetson 1993、Rosner 1983、Lupton 2001、Roellinghoff 2025、Akinshin 2022a/b）
本轮未被本报告任何主张引用，故不入表（不凑数）。

---

## 3. 三类数据结果

| 数据类 | 设置 | 关键结果读数 | 状态 | 入库 | 来源成稿 |
|---|---|---|---|---|---|
| ① 物理仿真成像（电子域 Poisson ＋ 读噪 ＋ 增益） | 自有 4000 次 MC，两组参数（基准点 F=1000 e⁻/B=100 e⁻·px⁻¹/D=0.5/RN=10/g=1.3；独立换源点 F=417 e⁻/σ_psf=2.3/B=39/D=7/RN=3.4/g=2.7） | `σ_F` 定义式 vs MC：`46.0332` vs `45.8528`（z=+0.35）；`16.8812` vs `17.2179`（z=−1.75）；负例（自引入读噪双计）同门 z=13.37 | **可复算**（但复算件在包内，仓库跟踪面无副本） | 包内复算件 | AUD-202-SNR核验 AUD202-011 |
| ② 代数合成（解析恒等式与量纲） | 四组异源 `(g,RN,sky,F)`；`ΣP²` 网格敏感性；`SNR_rep/SNR_true` 精确比值 | 权重恒等式最大相对偏差 `3.01e−16`；`ΣP²` 网格无关（`14.501%` 稳定）；`pred_doublecount_bias` 闭式 `+14.5009%` 与归档解析字段逐位同；文档式 `√(1+F/σ_bg²)` 相对精确值偏 `1.03–3.72` 倍 | **可复算**：恒等式绿；`07_noise_snr.md:121` 那条解析式**红** | 包内复算件 | AUD-202-SNR核验 AUD202-006/011；复核-AUD202-补 V5/V6 |
| ② 代数合成（退化判据自测） | 自建三算子 × 六类不同源输入（常场／线性梯度／二次／高频正弦／分辨正弦／跨 6 量级随机）＝18 组 | `node_reproduction_max_abs` **逐位 0.000e+00**（18/18）；1e-9 门限永不触发；把控制点整体乘 `10^0.3`（2 倍系统标定偏置）后仍全 0，而对真值场误差 `54.5–65.3` | **可复算** | 包内复算件 | 复核-AUD202-补 V5 ② |
| ③ 真实测试数据（M42/银心实拍帧端到端） | 需跑 normalize＋mosaic 读产品三键 | — | **需复测**（R-1） | 不可复核（产品在 `run/`） | AUD-202-SNR核验 §5 |
| 归档 B 级证据逐字段回读 | `b1/b2/b4/b3/exp04` JSON | `G1_def_max_abs_z = 2.681`(n=29)；`G4c_bias_at_base = 0.12791` vs 同件解析字段 `pred = 0.145009`；`H1_identity_rel_dev = 2.22e−16`；`G4c_pred_vs_measured_mc_max_abs_diff = 0.0700303`；两份结果件里字符串 `"snr"` 出现 **0** 次、`rmse_log_rho` 各 1152/2964 次 | **仅归档**（逐字段回读，未重跑） | 入库 | AUD-202-SNR核验 AUD202-012；复核-AUD202 V4(c) |
| 过权倍数与叠加功率损失 | 自有 Moffat4 离散轮廓＋自扫参（σ_px=1.4863 px、FWHM 3.5 ⇒ `P_0 = 0.215914`） | `R = w_used/w_opt`：F=10³、σ_bg=5、g=1.3 ⇒ 7.644；F=10⁶ ⇒ 6645；叠加功率损失 `E = Var_w/Var_opt − 1`：F=10⁴ ⇒ 7.16(n=8)/16.36(n=2)；**mosaic 误差棒被低估**：F=10³ ⇒ 1.83/4.32 倍，F=10⁶ ⇒ 831/3323 倍 | **可复算**（包内） | 包内复算件 | 复核-AUD202 V1 (c)(d) |
| 归档过权表的常数可复核性 | 对既有 `×1.011/×1.106/×2.057/×32.7/×1058` 做反解 | 隐含 `P_0 ≈ 0.3175–0.3176`（四点一致到 3e-4）与 `σ_sky,e² ≈ 300.5 e⁻²`（逐点相对差 ≤8.7e-3）⇒ 该组数**能命中**，但对应 `σ_px≈1.205 px` 的窄 PSF；同 `σ_sky` 下把 PSF 放宽到 σ_px=2.0，头条倍数从 ×1058 降到 ×398（2.7×） | **可复算**；原表未附 `P_0`/PSF 宽度/`σ_sky` 取值 ⇒ 引用时必须补 | 包内复算件 | 复核-AUD202 V1 与成稿差异 #1 |

**需复测项的最小复算命令与所缺证据**

| 项 | 缺哪条证据 | 最小复算命令（被授权节点执行） | 立证判据 |
|---|---|---|---|
| R-1 现行生产究竟走哪条权臂 | `07_noise_snr.md:138` 声明 scheduler 未挂 variance 块（恒走帧级常量）与 `module_adapters.cpp:7880-8230` 显示 variance 块已挂载且 fail-closed —— 两条互斥，只能由实跑产品裁决 | 跑一轮 normalize 后查产品面 `<frame>/p1_final.json#products` 是否含 `variance`/`ivar`；再跑 mosaic 读 `p2_integrated.json` 的 `weight_basis`/`weight_source`/`uncertainty_unavailable_reason`；并按 `08_drizzle.md:70` 附 `n_variance_tiles>0` 的磁盘证据 | 三键取值唯一确定臂；`08_drizzle.md:70` 要求"凡逐像素方差已产出的主张必须附该证据" |
| R-2 三条重建口径是否产出同一物理量 | 归档三臂测的是 σ̂ 估计器（结果件 `"snr"` 计数 0），不是合同三口径 | 补一组以 `F_ref`（或 §2b 的 `S_src`）为分子的 SNR 场三臂对拍：`dense`（须先补生产者）/`sparse_reconstruct`/`frame_reconstruct` 同一输入逐像素比，报 `max|SNR_a/SNR_b − 1|`，含"真值无源⇒归零"负例 | 三臂同物理量且互差受控；否则撤销"三口径并存"的文档主张 |
| R-3 `noise_model.cpp` 内 `variance` 面是否在任何分支被写入源项 | 1482 行未逐行 | 全文读 ＋ `git grep -n "gain" lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 逐命中判分支 | 任一分支写入源项即改写 DEV-01 的范围 |
| R-4 帧级 SNR 的 `m_5` 与 `frame_snr` 在产品头里的取值一致性 | 需读落盘 HiPS 头 | 读 `astro_sphere_sink.cpp:430-470` 写的 `ASTROCS_FRAME_SNR=snr_reference.snr_f`、`ASTROCS_REFERENCE_FLUX` 与 `frame["frame_snr"]`（`:7278-7284`，`= F_5/m_5`）两处键的实际消费者 | 同名不同物的重叠键必须消歧 |
| R-5 恒真判据修复后的能红证据 | 缺一条"合同正例 → 消费侧权重"端到端夹具（现只有自检测试的合成网格） | 把 `weight_chain_selfcheck.cpp:189,:202-207` 的 2×2/4×4 夹具值改为**绝对量级**（≈ frame_snr 量级、逐点跨数量级）并同步 Oracle；补 `prediction_variance` 落盘门，对无源/平坦控制网格须非零 | 同一注入从绿转红 |
| R-6 两条配置门与实验门的 HEAD 状态 | 测试全停 | 按 `eng/ci/checks.json` 注册项跑 drizzle/SNR 相关档（本阶段禁止） | 读数与本报告静态结论一致 |

---

## 4. 判据判别力

| 判据 | 位置 | 正例（应绿） | 负例（注入什么应红） | 现有实现会不会真红 | 结论 | 入库 | 来源成稿 |
|---|---|---|---|---|---|---|---|
| 读噪双计保护 | `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp:392-396`（`|zB|>3`） | 声明与来源匹配 ⇒ 绿 | `sigma_sky_source` 声明与来源不符（总 rms 再加 `(RN/g)²`） | **会**（独立 MC 复现同形偏差 z=13.37） | 有判别力 | 入库 | AUD-202-SNR核验 §4 |
| 天光单调性 / log-log 斜率 | `实验/absolute-snr/results/b1_sky_scan.json::gates_bright/gates_faint` | 真效应：比值随天光单调降 | 冻住 `σ_sky`（不随 B 变） | 比值 = `1.000000`、斜率 `1.0e−17` ⇒ **红** | 有判别力 | 入库 | AUD-202-SNR核验；复核-AUD202 V1 |
| 权重换算恒等式 `H1/H5_identity_lt_1e-12` | `实验/absolute-snr/code/b4_integration.py` | 机器精度 ⇒ 绿 | 分母换成逐帧检出中位数、或幂次 2→1 | 会 | 有判别力但**只保证代数不出错**；分子取 `F_i` 的口径漏洞恰从这条门下通过 | 入库 | AUD-202-SNR核验 §4 |
| `node_reproduction_max_abs ≈ 0`（"重建误差"） | `weight_chain.h:167`；`weight_chain.cpp:529-550`；`13_integration.md:39` | — | 任何节点间插值偏差 | **不会**：算子被约束必须在节点精确复现 ⇒ 构造恒 0；换源 18 组实测逐位 0；唯一能红面（半格错位）已被上游 `:479-486` 的 cell 中心门 fail-closed 接管 ⇒ **在可达输入集上恒 0** | **无证据资格**（连"索引 bug 自检"这一面都拿不到） | 入库 | AUD-202-SNR核验 AUD202-008；复核-AUD202-补 V5 ② |
| 合同要求的预测方差 | `DATA-002:99`、`ACCEPTANCE_SPEC.md:60`、`ASTROCS_DESIGN.md:327`；`snr-propagation-design.md:541`（该方差要进权重分母） | 出参含预测方差 ⇒ 绿 | 无载体可注入 | **不适用**：`SparseReconstruction` 的 9 个字段无 var/sigma/uncert；`git grep "predicted_variance_adu2" -- lib/` = 0 命中；`git log -S` 两条命中均在实验侧 ⇒ **从未实现，非被删** | 验收项无载体 | 入库 | 复核-AUD202-补 V5 ③ |
| 稀疏层"绝对语义"复现门 | `07_noise_snr.md:252` | 层值按绝对量级 ⇒ 绿 | 把控制值改成相对因子 | 节点复现仍逐位成立（相对场也是绝对数的插值）⇒ 只有 `sparse_snr_semantics` 键的声明面能红 | **断言对象错位**：测的是"乘没乘帧级标量"，测不到分子用 `F_i` 还是 `F_ref` | 入库 | AUD-202-SNR核验 §4 |
| 权重链语义门 `kFluxTypeUnweightedSnr` | `weight_chain.cpp:695`；调用方 `module_adapters.cpp:11531` 硬编码 | 正确 kind ⇒ 绿 | 填相对质量权／median 诊断 | 会红，但 `kind` 由调用方**自填** ⇒ 生产恒绿 | 有判别力，对象校验缺位 | 入库 | AUD-202-SNR核验 §4 |
| SNR 控制点等值门 | `p1snr_science_test.cpp:424`（`checkClose(pts[i].snr_psf, e.snr_optimal, 1e-12)`） | 与同式 oracle 一致 ⇒ 绿 | 把 oracle 换成 `F_ref` 分子 | **任何分子口径错误都不发红**（oracle 是同式 long-double 镜像） | 测自洽性不测正确性，且把错误口径锁成期望值 | 入库 | AUD-202-SNR核验 §4 |
| 帧级 SNR `snr_phot == median(SNR_F)` | 同文件 `:435-437` | 内部一致 ⇒ 绿 | 帧级 SNR 改按参考轮廓口径（`reference_snr_f`） | 会红 | **恒真风险**：把帧级 SNR 钉在逐源中位数上，而 HiPS 头实际写 `snr_reference.snr_f`（`astro_sphere_sink.cpp:453-454`）⇒ 两个不同数，门不覆盖产品面 | 入库 | AUD-202-SNR核验 §4 |
| 测试面把"相对语义"锁成正例 | `weight_chain_selfcheck.cpp:189,:202-207`（夹具值 1.0/1.2/0.8/1.0、0.6–1.5）；`:281` 断言 `200.0*intra0`；`weight_chain_oracle.py:240` | — | — | 合同判红的形态（n6 的 `1.0/0.878`）与自检测试的**绿例同形** | **反向缺陷**：测试期望本身就是被作废的语义 | 入库 | 复核-AUD202 V2 (e) |
| drizzle 侧对 SNR 权重链的覆盖 | `drizzle_pf_sb_gate` 等在册门 | — | 逐叶面积错分／量化偏差 | 属另一链路（见同批《面积交叠核验报告》§4） | 不在此链计 | 入库 | 交叉引用 |

---

## 5. 偏差清单与订正

「复验处置」取值：`确认`／`降级后仍成立`／`单层（第②层未覆盖）`。

### 5.1 第②层确认后成立

| # | 位置 | 现行口径 | 正确口径 | 证据 | 影响范围 | 订正要求 | 复验处置 | 入库 | 来源成稿 |
|---|---|---|---|---|---|---|---|---|---|
| DEV-01 | `module_adapters.cpp:11936`、`:8155`、`:8100`、`snr_estimator.h:116`、`:11744` | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权 | 最高设计 `:156-158,:171-174` 逐字点名；S4 photutils 总误差式；S3 Z&O II 标题限定；Cauchy–Schwarz 唯一取等条件 `w_i ∝ 1/v_i` 且 `v_i` 必须是该样本自身方差；解析式 `R = 1 + S_src/(g·σ_bg²)` 与自建倍数表 | 亮源像素系统性过权（F=10³ → ×7.6、F=10⁶ → ×6.6e3 @σ_bg=5,g=1.3），**且写进交付产品**：`variance = 1/W`（`:12004`）⇒ mosaic 误差棒按构造偏小 1–3 个数量级（F=10³ → 1.83/4.32 倍，F=10⁶ → 831/3323 倍） | 把含源项的加权方差面作为独立第二张面产出并入产品（或由 UPM 后的 `S_src = Σ F̂_i P_i` 现算），Phase2 集成读该面；`ivar` 继续只服务天光建模与误差报告；复用 §2b 的"漏源项臂与完整臂在无源帧逐位恒等"作验收判据；实跑附 `n_variance_tiles>0` 磁盘证据 | 确认 | 入库（代码）／倍数为包内复算件 | AUD-202-SNR核验 AUD202-001；复核-AUD202 V1 |
| DEV-02 | 臂 B（`weight_chain.cpp:714,:739,:779`；`module_adapters.cpp:11534,:11919`） | 曾与 DEV-01 捆成一条头条，称"两条臂各中一次：权重在信号维退化为常数" | 臂 B 只在 `ivar` 缺失时进入（`ivar_missing>0`），且**显式声明** `uncertainty_available=false` ＋ `weight_source="frame_snr"` ＋ `weight_basis="frame_snr_ivar"` ⇒ 属**已声明的降级**，与逐帧标量权重在业界（SWarp `sigfac`、PixInsight 帧权）同型，**非缺陷** | V1 (e) 可达性判定 | 若把已声明降级与未声明口径错混排，会把整改方向误判为"接稀疏层"——接了稀疏层**不解决** DEV-01 的源项缺失（除非控制点真的按含源项 σ 求值） | 文档须写成"降级路径且无逐像素权"，不得与"权重链闭合"并列声称科学完备 | 降级后仍成立 | 入库 | 复核-AUD202 V1 |
| DEV-03 | `snr_estimator.cpp:65-91,:80,:90`→`snr_science.cpp:215`→`drizzle_engine.cpp:2504`；合同 `sparse_snr_layer.schema.json`；`weight_chain.cpp:739`、`weight_chain.h:240` | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象** `source_snr`／HiPS `snr/` 星表，其消费者是相对质量场（`coverage/src/sampler.cpp:409,:599`、`coverage/tools/stage2.cpp:90` → `local_snr/frame_snr_medians`），当前**不接权重链**。另外消费算子 `actual = frame_snr × intra` 实现的是**已被取消的**分解（`weight_chain.h:240` 所引"DESIGN §3.4:174 帧级×帧内"在现权威里 grep 零命中；裁决取消见 S13） | 写入点逐锚核对 ＋ 生产唯一 `sparse` 赋值为 `nullptr`（其余 `a.sparse=&L` 只在 `weight_chain_selfcheck.cpp:270-271`） | 一旦按默认口径接入即触发：层存绝对值 ⇒ 权被乘 `frame_snr²`（实测帧 21.4–47.4 ⇒ ×456–2245，帧间相对权被压平）；层存逐源值 ⇒ 权被乘 `(F_i/F_ref)²`（`m_ref=6.0`、ZP 18/20/22 ⇒ `F_ref` = 6.3e4/4.0e5/2.5e6 ADU，F_i=10³ ADU 时 ×2.5e-4/6.3e-6/1.6e-7 ⇒ 权变成 ∝通量² 的"信号权"）；两者叠加 ×`frame_snr²·(F_i/F_ref)²` | 整改动作是**新增生产 ＋ 改消费侧算子（去掉 ×frame_snr）**，不是改写入侧口径；同时把自检测试夹具改成绝对量级并同步 Oracle（否则测试持续把错误语义当期望值） | 确认（定性收窄） | 入库 | AUD-202-SNR核验 AUD202-004；复核-AUD202 V2 |
| DEV-04 | `ASTROCS_DESIGN.md:398-403`；`CONTROL_WEIGHT_SNR.md:205-222`（§8b）；`实验/absolute-snr/code/b3_domain_map.py:111,:139`；`docs/EXP-04-RECONSTRUCTION.md:96-97`；`eng/ci/ledgers/dead_config_keys.json`（`snr_path`） | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_fits_to_ahpx` 已被 `prod_wiring_baseline.json:165` 登记 unreachable；`hp_drizzle_run_phase1_hips` 内的 IDW 稠密重建依赖从未被 scheduler 挂上的 `snr_model` 块（`git grep "snr_model" -- lib/infrastructure/scheduler` = 0），且逐像素 SNR 在 `drizzle_engine.cpp:1417` 以匿名参数丢弃、无数据时兜底 `snrValue=1.0f`，用的还是被取消的乘性口径）；④`sparse`/`frame` 在权重链里同值 ⇒ 三口径在生产只剩一条；⑤被引为定案依据的实验臂名是 4 个 `dense/sparse/frame_median/frame_mad`，**每个都是 σ̂ 场估计器**（`sigma_field_fast` = 逐块 `1.4826×MAD`），度量 `rmse_log_rho`（两侧中位数归一 ⇒ 水平信息被消掉）与 `E`（其权重形式 `w=1/σ̂²` 正是 DEV-01 判为缺陷的背景权），数据面合成"无源"、`frame_median` ≠ 生产 `frame_reconstruct` | 结果件表头实测：`b3_domain_map.json` 与 `exp04_e1_analytic.json` 中 `"snr"` 出现 **0 次** | 论文核心实验的选型依据与它支撑的对象不是同一个；默认口径的生产链不存在 | 补一组以 `F_ref`/`S_src` 为分子的三臂实验（含"真值无源⇒归零"负例），或把 §8b 结论显式改挂到"σ̂ 估计器选型"；`snr_path` 升为在架键并写 `snr_path_effective`；给 `dense` 一个生产者或在文档撤销该口径；**唯一可原样保留的图谱结论**是 `dense` 超 1 MiB 预算 64 倍（`4·4096²/1048576`）这一与对象无关的存储门 | 确认（并强于原报） | 入库 | AUD-202-SNR核验 AUD202-009；复核-AUD202 V4 |
| DEV-05 | `13_integration.md:39`；`weight_chain.h:167`；`DATA-002:99`；`ACCEPTANCE_SPEC.md:60`；`eng/ci/checks.json`（drizzle/SNR 档） | manifest 的 `node_reproduction_max_abs` 登记为「重建误差」；`weight_chain.h:167` 自注"应 ~0" | 二者对同一字段给出互斥定性（一名为误差量、一称为应恒 0）；该字段在可达输入集上恒 0 ⇒ **无证据资格**；合同要求的逐像素**预测方差**从未实现（字段面缺位、`git log -S` 两条命中均在实验侧）。另：`EXP-04-RECONSTRUCTION.md:648`（判据 S4）本仓已自登记"容差 0.5 dex 对精确插值类过松，不具举证资格"，但 manifest 名面未跟着改 | V5 ②③ | 下游会把该字段读作"重建误差已受控" | ①改名 `operator_node_selfcheck_max_abs`（保留原语义，不作精度证据）；②新增 `recon_error_vs_independent_reference`，其参考场必须与构造算子所用节点数组**不同源**、采样点必须**非节点**、并带守卫 `n_control_points<4 ∨ n_ref_samples==0 ∨ 参考场方差==0 ⇒ RED`；③实现合同要求的预测方差（解析传播 `Var = Σ a_k(x,y)²Var(SNR_k)` 与经验自举孰进权重分母属设计裁决项）；正例=带非零梯度的场须报出 >0，负例=常数真值须归零，两条都进测试 | 确认 | 入库＋包内复算件 | AUD-202-SNR核验 AUD202-008；复核-AUD202-补 V5 |
| DEV-06 | `module_adapters.cpp:7159`（`sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS`）、`:7151`、`:6794`；`phase_config_normalize.schema.json:229`；`eng/packaging/config/defaults.json` | 决策树分支 1（`shot_noise_only` 且 gain>0 ⇒ 散粒口径 ＋ `(RN/g)²`）为设计本意路径 | 生产只有两条可达臂：①gain 未给（**默认**）⇒ 天空受限臂 `σ_F² = σ_sky²/ΣP²`，既无源散粒项也无独立读噪项；②用户手填 gain ⇒ 经验总 rms ＋ 源项（读噪计一次，正确）。分支 1 从生产**不可达**（声明被接线层写死）。`NOISE_MODEL.md:223-242` §5c 第 1 条"本帧自估 `V = σ0² + S/g` 斜率–截距回归"是正式数据面上的主路径，**未实现**；帧头关键字只作交叉校验（`:239-240` 实测 M42 真实帧 124 个关键字中 `GAIN/RDNOISE/EGAIN` 全部缺失） | 代码接线事实 ＋ §5c 条款；`snr.gain_e_per_adu` 在 defaults 与出厂模板均无默认值登记（四侧默认缺位形态） | 无增益 ⇒ `σ_F` 缺源项 ⇒ 上报 SNR 单调偏高、权重偏高，偏差随源亮度无界增长 | 实现 §5c 自估增益主路径（含杠杆臂可辨识性判据与"不可辨识 ⇒ 显式不可得"）；把 `sigma_sky_source` 的择一交回声明面；已实现的 `snr_caliber = upper_bound_no_gain` 分支（`:7130-7136`）证真、保持；在 §4.2a 决策树上如实标"分支 1 生产不可达"或把它接上 | **单层（第②层未覆盖）** | 入库 | AUD-202-SNR核验 AUD202-005 |
| DEV-07 | `snr_science.cpp:202,:209`（对角累加）；`information_weight.cpp:128,:140`（`w_info_dense`／`w_info_low_rank`，实现正确）；唯一生产调用点 `phase1_product.cpp:396` 是**对角版** `w_info_diagonal` | 帧级 SNR／稀疏控制点／深度 `m_5` 全部走对角（白噪声）形式 | Phase1 产品是 drizzle 重采样后的 HEALPix 叶（相关长度 1–2 px），属 `07_noise_snr.md:130` 的"相关噪声"分支，却未用完整信息核；完整核**已实现、零接入** | `NOISE_MODEL.md:131`（协方差非对角不落盘）、`UNCERTAINTY_AND_COVARIANCE.md:142`（必须显式加协方差项，`Σc_k²u_k` 只在 `C_in` 对角时成立）、`ASTROCS_DESIGN.md:181`、S1 适用域限定 | 相关噪声域下的方差低估 | `snr_source_snr_f64` 的对角累加改经 `CovarianceView` 走 `w_info_solve`（同 TU 已提供 `WhiteNoiseGate`，`reason="non_diagonal_covariance"` 即回退完整核）；SNR 产物 provenance 写 `covariance_kind`；文档须写明 `σ_i²` 内用**实测** `F` 使权重依赖数据 ⇒ `Var=1/ΣP²/σ²` 是一阶近似（现文未写） | **单层（第②层未覆盖）** | 入库 | AUD-202-SNR核验 AUD202-007 |
| DEV-08 | `07_noise_snr.md:121` | 天空受限口径的解析式 `SNR_rep/SNR_true = √(1+F/σ_bg²)`；"φ=0.5 偏高 41%、φ=0.95 偏高 347%" | 量纲不齐确认：右端字面加数 `F[ADU]/σ_bg²[ADU²] = ADU⁻¹` 与 1 相加 ⇒ 不闭合；须同时带 `1/g` **与** `ΣP²`（或逐像素 `P_i`）。两个锚点并非不可复算——它们精确等于 `1/√(1−φ)`（φ = 源方差占总方差之比）在 φ=0.5/0.95 的值（实测 `41.42%`/`347.21%`）⇒ **写出式与其数字不属于同一个对象**（该理想化下比值不需要 `g`）。更要紧的是该分支进入条件是 `gain_e_per_adu<=0` ⇒ 带 `1/g` 的式子在其唯一触发条件下**根本不可求值** | 独立单位推导（取单位的位置：`snr_science.cpp:200,:201,:209,:214-215`）＋ V5 ① | 纯文档缺陷（生产不引用该式；仓内唯一复述点是 `module_adapters.cpp:7122` 注释） | 替换为 φ 形式并给出 φ 的定义与样本面；若坚持 F/σ 形式则须给该分支的 `g` 来源；同时按 `sigma_sky_source_effective=3` 的显式退化标注走，不得"补个 `1/g`"了事 | 确认（量纲）／归因订正 | 入库 | AUD-202-SNR核验 AUD202-006；复核-AUD202-补 V5 ① |
| DEV-09 | 文档头条 `07_noise_snr.md:123`、`CONTROL_WEIGHT_SNR.md:201`、`NOISE_MODEL.md:324`、`PSF_SIGNAL_WEIGHT.md` §7a ＋ 代码注释 `snr_science.cpp:176`、`module_adapters.cpp:7156` | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4.2% ⇒ 4.8364pp 头条散布 100% 来自分母；自有 20 块 N=1000 重跑给出 sd = 2.9447pp、块极差 10.7451pp ⇒ 归档头条与闭式差 1.7096pp = 0.58 sd。另测到实现值系统性偏低 −1.22pp（29 点均值，2.58 sem）。配对记录量 `G4c_pred_vs_measured_mc_max_abs_diff = 7.0030pp` **判定方 0 处**（grep 3 命中：写/存/回显），而被 2pp 布尔门约束的那条（臂比值）噪声只有 ≈0.003pp ⇒ 门对头条零设防 | 逐字段回读 ＋ V6 独立闭式与 MC | 引用面精度：定性与量级不变（+14.5% vs +12.8%），生产数值不受影响；**本仓实验单元已自行登记该局限**（`实验/absolute-snr/REPORT_paper.md:225,:371`）⇒ 属"已承认的不确定度未传导"，是传导缺陷而非新发现 | ①四处文档＋两处注释的 `+12.8%~+34.0%` 改为闭式 `+14.50%~+38.25%`（并给字段名 `pred_doublecount_bias`）；②订正 `07_noise_snr.md:123` 句尾"天光主导点（B≥10⁵）偏差 <1%"的错出处（b2 不扫 B，该数出自 `b1_sky_scan.json` 的 `sky_scan_bright/faint`）；③给 `G4c_pred_vs_measured_mc_max_abs_diff` 配布尔门（阈值按 sd 2.94pp 定，如 ≤3×sd）或显式标 `informational_only`；④引用实现值强制带 `N_MC` 与相对标准误；另登记：`README.md:111`/`REVIEW.md:186` 的"最坏 +36.6%"与本件闭式 +38.25% 三处三个值，"最坏"须统一口径 | 确认（成因降级为引用面缺陷） | 入库 | AUD-202-SNR核验 AUD202-012；复核-AUD202-补 V6 |
| DEV-10 | `NOISE_MODEL.md:9`（§1 目的句）、`:328`（§11"权重归一与适用域"）、`07_noise_snr.md:201`（§4.6"权重不落盘"） | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS` §30.1 ＋ `module_adapters.cpp:8805`）相比是**列举不全**，层级向下即可自洽 | V3 三路独立复算收敛（最优加权理论／S3＋S4 一手对照／本仓层级链：`docs/design/UNIFIED_MODEL.md` 对象表把"可否作权重"条件化在估计目标上；`NOISE_MODEL.md` §5b/§5c/§10 实质条款本身正确） | 文档口径唯一性；数值后果已全部计入 DEV-01 | 按 §1/§11 两句补适用域（可直接替换文案：背景面①天光建模与 UPM 拟合权 ②误差报告与诊断 ③背景受限域叠加权近似；叠加与拟合的逐像素科学权重 = §5c 加权方差面的倒数，两面对 §8c 定权式各自独立、取值互不代用），§4.6 列举订正；随后做一次一致性回归（§8c/§5c/UNIFIED_MODEL/ASTROCS_DESIGN 四处对"权重来源面"的表述复核）。**不动任何公式与容差** | 确认（从"需上呈裁决"降为 P2 文档订正） | 入库 | AUD-202-SNR核验 AUD202-010；复核-AUD202 V3 |
| DEV-11 | Zackay & Ofek／Horne 的引用面 | `PSF_SIGNAL_WEIGHT.md:145` 用 Z&O **I** 支撑"点源信息权重 `Q/W/Var(F)=1/W`"；`UNCERTAINTY_AND_COVARIANCE.md:142` 与 `PHASE2_MOSAIC_WRITE.md:518` 用 Z&O **II** 支撑 `C_out = R C_in Rᵀ` 的相关噪声传播 | I/II 与 arXiv 末位序号**绑定正确、全库 19 处零绑反**（计数命令与逐处核对成立）；缺陷只在适用域标注：前者原始出处是 Horne 1986（一维光谱抽取）⇒ 引用可保留但须写明"成像点源推广"；后者须补"背景主导极限"的适用域声明或另找覆盖源受限的锚 | V3 与 S1/S2/S3 | 引用适用域 | 补两处适用域声明；Horne 的式号层按 §2 表限定，不得升为逐像素方差式的一手锚 | 单层（绑定部分经 V3 独立同向） | 入库 | AUD-202-SNR核验 AUD202-002 |
| DEV-12 | 噪声组成实现 `snr_science.cpp:200-202`、`:177`；`NOISE_MODEL.md:324`；`07_noise_snr.md:110` | `σ_i² = σ_sky² + (RN/g)² + F·P_i/g`，读噪只出现一次 | **成立**：单位三项同为 ADU²、`ΣP²/σ²` 为 ADU⁻² ⇒ `σ_F` 为 ADU、SNR 无量纲；`Σ_i P_i = 1` 在截断网格上成立且 `ΣP²` 随半宽第 5 位起不动 ⇒ 截断非误差源；`P_i` 归一约定与 Horne 一致；**未发现重复计入**；暗电流无独立项（靠经验总 rms 吸收，由写死 `EMPIRICAL_TOTAL_RMS` 与 `p1snr_science_skysource` 负例共同挡住双计） | 独立单位推导 ＋ 两组 MC（z=+0.35/−1.75）＋ 负例 z=13.37 ＋ 退化自测（冻住天光散粒 ⇒ 比值 1.000000、斜率 1.0e−17） | 本链代数核心成立 ⇒ DEV-08/DEV-09 只改引用面不动结论 | 不订正公式；把"实测 `F` 进入 `σ_i²` 使权重依赖数据、故为一阶近似"写进文档（随 DEV-07 一并） | **单层（第②层未覆盖）** | 入库＋包内复算件 | AUD-202-SNR核验 AUD202-011 |
| DEV-13 | `defaults.json:694`（`sparse_snr.spacing_px` 论证串）；`07_noise_snr.md:102`；`CONTROL_WEIGHT_SNR.md:215`（§8b） | "默认 64 = tile_width/8…实测相关长度 ℓ=40.4–57.9 px ⇒ Δ/ℓ ≈ 1.1–1.6（**够密**）" | `Δ/ℓ > 1` 是**欠采**表述，与该仓自身体实测（Δ\*=32 px 起失效）方向相反；把 `Δ = tile_width/8` 写成"复用 UPM 控制网格"是几何便利而非由 SNR 场自身空间尺度导出 | 本条第③问未派入第②层 ⇒ **未获独立复算** | 间距选型论证 | 按 §8b 把 Δ 与算子/数据来源绑定、给可陈述的采样判据（`Δ ≤ ℓ/2` 一类），并把 HST 类域的 32 px 推荐变成按域生效的规则 | **单层（第②层明确未覆盖本问）** | 入库 | AUD-202-SNR核验 AUD202-008；复核-AUD202-补 V5 与成稿差异 #6 |

---

## 6. 诚实边界

**适用域**

- 本报告落在跨帧绝对 SNR 链的公式、口径、接线与判据面。测光零点求取、UPM 平面拟合、面积交叠不在本报告判据内，
  但 `F_ref,k = 10^(−0.4(m_ref−ZP_k))` 的数值正确性**依赖测光链的 ZP**；本报告只核到"`F_ref` 逐帧、同帧配对、
  锚定固定星等"这一结构（`module_adapters.cpp:7168-7182`）。`m_ref = 6.0` 的常数出处**四条链路的取证面均未登记**（不判错，登记为共同缺口）。
- Horne 1986 的一手适用域是一维 CCD 光谱最优抽取；把它用于成像点源 PSF 测光是推广，且 `Var=1/ΣP²/σ²` 只在噪声白时成立
  ——本仓在 drizzle 重采样后的相关噪声域仍用对角形式（DEV-07），这是判为偏离的根据。
- Z&O II 的标题限定是背景主导噪声极限；凡引它支撑"任意用途最优"均属外推。
- 所有"过权倍数／误差棒低估"类数值随 PSF 宽度与 `σ_sky` 取值一次方变化 ⇒ 引用必须连常数一起给（§3 末两行）。

**反例与已知偏差方向**

- DEV-01 方向确定：以背景 ivar 定权 ⇒ 亮源像素系统性过权，叠加结果向亮帧的亮源像素偏移；
  面亮度不受影响，受影响的是点源测光、SNR 产品与 **mosaic 误差棒（偏小 1–3 个数量级）**。
- DEV-06/DEV-08 方向确定：无增益 ⇒ `σ_F` 缺源项 ⇒ 上报 SNR 单调偏高、权重偏高，偏差随源亮度无界增长；
  帧级 `SNR(F_ref)` 用 `F_ref` 而非真实源亮度 ⇒ 该偏差以参考星等档计量，不随帧内具体源变化，方向仍为偏高。
- DEV-03 若被接入：权重被乘 `(F_i/F_ref)²` 与 `frame_snr²` ⇒ 亮星邻域过权、暗星邻域欠权，跨帧权重比被污染。
- 本报告**未证伪**本链的代数核心：`σ_F^(−2)=ΣP²/σ_i²` 的实现、`w=SNR²/F_ref²` 恒等、天光单调趋零三条均经独立复算成立。

**未验证部分**

- 本阶段未构建、未跑 ctest/`run_checks.py`/三命令端到端 ⇒ 关于生产数值与产品面的陈述一律按 §3 标"需复测"或"仅归档"。
- Horne 1986 与 photutils 之外的成熟实现交叉核对未做版本钉定（Siril 侧本轮未取证）。
- `noise_model.cpp` 1482 行未逐行；`实验/absolute-snr` 196 个跟踪文件的文件级覆盖率约 5%，
  但被文档引用的全部数字锚点已逐字段回读并交叉复算——若未读文件里有相反口径，本报告的成立面会相应收窄。
- `lib/algorithms/psf/`（27 文件）、`lib/algorithms/rejection/`（4 文件，全为 .md/.yaml，真实排异实现在
  `lib/algorithms/coverage/src/rejection.cpp`）以"与 SNR/权重口径面无交集"的定点扫描替代通读，该判断本身若错会漏掉缺陷。
- 恒真判据"能红的那一面"是否已被上游门完全接管，只由静态读码判定（`:479-486` 先于残差循环 fail-closed）；未实跑。

**列入 UNRESOLVED 的项**

| 项 | 为什么不能现在定 | 缺什么 |
|---|---|---|
| 现行生产走哪条权臂 | 两条权威陈述互斥，只能由实跑产品裁决 | R-1 |
| 三口径是否同一物理量 | 归档实验对象是 σ̂，非合同三口径 | R-2 |
| 预测方差的口径 | 解析传播与经验自举孰进权重分母，属设计裁决项 | 负责人裁决（DEV-05） |
| `m_ref = 6.0` 的出处 | 四链取证面均未登记 | 常数登记 |
| 实现值 −1.22pp 系统偏低是否为一阶近似偏置 | 需把闭式分母换成同批帧的 `F̂` 迭代权重重算 | 一次复算 |
| DEV-06/07/12 的第②层确认 | 工包只派 6 条，这三项不在其中 | 补派第②层复核 |

---

## 7. 结论

- **成立**：`σ_F^(−2)=ΣP²/σ_i²` 的实现与噪声组成（读噪单次计入、单位闭合、截断不构成误差源）；
  `w = SNR²/F_ref² ≡ 1/σ_F²` 的代数恒等；天光升高 SNR 单调趋零且判据有判别力；
  Z&O I/II 的标识符绑定全库零绑反；`snr_caliber = upper_bound_no_gain` 的显式退化声明。
- **不成立，须订正（根因见 §5）**：以背景方差面倒数充当 Phase2 逐像素科学权重（DEV-01，产品误差棒同时偏小）；
  稀疏控制点合同对象在生产里不存在、消费算子实现的是已作废的分解（DEV-03）；
  三种重建口径在生产只剩一条且 `snr_path`/`snr_path_effective` 双缺载体，选型图谱的被测对象与主张对象错位（DEV-04）；
  "重建误差"字段无证据资格而合同要求的预测方差从未实现（DEV-05）；
  设计本意分支生产不可达、自估增益主路径未实现（DEV-06）；相关噪声域未用完整信息核（DEV-07）；
  文档解析式量纲不齐且式与数字不属同一对象（DEV-08）；头条数字取自噪声更大的单次实现值且无门约束（DEV-09）；
  两处文档句的适用域指称越界（DEV-10）；`m_ref` 与 Δ=64 的论证缺口（DEV-13）。
- **证据不足，列入 UNRESOLVED**：见 §6 末表。
- **按三重数据佐证判定**：②代数合成两路（恒等式、单位、量纲、退化自测）与①自有物理仿真 MC 一致；
  ③真实数据端到端**未做** ⇒ **本链路未达 standard 02 §2 的三方一致**。
  链路的代数核心成立、接线与引用面不成立，端到端项以"需复测"结转。

---

## 8. 已撤销的说法（保留原文与撤销依据，防止重复上报）

| 曾呈报的说法 | 撤销依据（独立复算） | 现在的处置 |
|---|---|---|
| "`NOISE_MODEL.md` §9a 与最高设计/SCI-CW §2a 直接打架 ⇒ **两篇 FROZEN 科学文档相互排斥**，须按 AGENTS §10 上呈负责人二选一（A 依最高设计／B 承认背景 ivar 可作权重）" | (B) 一支在科学上不存在：撤销"必须含源项"要同时推翻 photutils 的总误差式、Z&O II 的标题限定、最优加权的 Cauchy–Schwarz 取等条件与本仓 §5b/§5c/§10 三处实质条款 ⇒ "二选一"是假两难；`NOISE_MODEL.md` 的实质条款与最高设计同向，越界的只有 §1 与 §11 两句缺适用域限定 | 撤销"两篇互斥／需裁决"定性与 (A)(B) 方案；改为 P2 文档订正（DEV-10），不占负责人裁决额度 |
| "`ivar` 落盘 ⇒ 红线'权重不入库'被**实质破坏**（落盘即入库）" | `ivar` 是 13 个 canonical 对象之一，对象表已把"可否作权重"条件化在估计目标上 ⇒ 落盘本身不违红线；真正互斥的是插件层"只存"的列举不全，层级向下即可自洽 | 撤销"红线被破坏"定性；保留 §4.6 列举订正 |
| "两条臂各中一次：无论走 ivar 还是走帧级 SNR 链，权重在信号维都退化为常数"（作为同一条头条） | 臂 B 只在 ivar 缺失时进入，且显式声明 `uncertainty_available=false` 与 `weight_source` ⇒ 属已声明的降级路径，与逐帧标量权重的业界做法同型，非缺陷；且"接稀疏层"不解决臂 A 的源项缺失 | 头条只保留臂 A（P0）；臂 B 改列为文档表述要求（DEV-02） |
| "稀疏层把 `sparse_snr_layer` 写成了 `source_snr`（即把 A 写成了 B）" | 合同对象 `sparse_snr_layer` 在 `lib/**` **零生产者**；写逐源 SNR 的是另一个在册对象 `source_snr`/HiPS `snr/` 星表，其消费者是相对质量场且不接权重链 | 定性改为"该产出的没产出、已产出的语义与消费侧算子不匹配"，整改动作相应改写（DEV-03） |
| "`dense` 口径**没有生产者**" | 有两条潜在生产者，**两条都不可达**：`hp_drizzle_fits_to_ahpx` 已登记 unreachable；`hp_drizzle_run_phase1_hips` 的 IDW 稠密重建依赖从未被挂上的 `snr_model` 块，且产物在累加处以匿名参数丢弃 | 证据更强，表述订正（DEV-04） |
| "`SNR_rep/SNR_true = √(1+F/σ_bg²)` 的数值偏差 1.03–3.72 倍 ⇒ 整改是补上 `1/g`" | 只丢 `1/g` 的倍数上界是 `√g`（g=1.3 ⇒ 1.14 倍）；3.717 只能由**同时丢 `ΣP²`** 解释（`√(g/ΣP²)=3.742`）。且该分支进入条件是 `gain<=0` ⇒ 带 `1/g` 的式子在其唯一触发条件下不可求值 | 归因订正为"须补两个因子，并改走 φ 形式"（DEV-08） |
| "`41%`、`347%` 两个锚点 UNPROVEN（φ 未定义，无法复算其口径）" | 两数精确等于 `1/√(1−φ)` 在 φ=0.5/0.95 的值（`41.42%`/`347.21%`），可复算；对象是平场/单像素理想化，不是写出的那条式子 | 改判为"可复算但对象错"（DEV-08），其结论方向不变 |
| "同一物理点沿两条扫描轴相差 3.1pp ⇒ 待解释的不一致" | 同物理点实为**四条轴四个数**（极差 4.8364pp）；分子极差 0.027%、分母极差 4.2% ⇒ 全部来自分母的样本噪声（N=1000 样本标准差的标准误 `1/√(2·999)=2.24%` 一致）；本仓实验报告已自登记该不确定度 | 成因降级为"引用面选了噪声大的数、且未带不确定度"（DEV-09），定级 P2、不进 P1 |
| "`max|pred − measured| = 0.0700` 被算出但无门约束"（作为新发现） | 成立且精确到字段；但需同时说明：被 2pp 门约束的那条噪声只有 ≈0.003pp（阈值为其 666 倍），即门物理上有意义、只是对头条零设防 | 保留（DEV-09），整改方向改为"门要卡住引用面真正用的那个数" |
| "`07_noise_snr.md:123` 句尾'天光主导点（B≥10⁵）偏差 <1%'" | b2 根本不扫 B（`SCANS` 五轴、B 固定 100）；该数出自 `b1_sky_scan.json` | 撤销其出处（DEV-09 整改 ②） |

<!-- PROGRESS: SNR链路 1/1 落盘 -->
