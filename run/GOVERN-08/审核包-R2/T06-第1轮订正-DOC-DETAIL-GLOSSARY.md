# T06 第 1 轮订正 · 跨切面车道 DOC-DETAIL-GLOSSARY

**车道**：`docs/detail/**`、`docs/GLOSSARY.md`、`docs/README.md`，以及四条车道之外的 `docs/` 顶层文件（`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md`）。
**`L.key` 取值说明**：派单未给 `L.key` 字面值，按「层-主题」惯例取 `DOC-DETAIL-GLOSSARY`。若前台另有命名，改文件名即可，正文不依赖该串。

**订正者**：跨切面车道执行子代理。**只做订正，不重审**；全程**零 git 写操作**（git 仅只读，中文路径一律 `git -c core.quotepath=false`）。

**本车道边界**：`docs/science/**` 与 `docs/engineering/**` 的分册正文**不在本写面**，发现问题只**登记**给对应车道或前台，不代改。

---

## 0. 本车道做了什么

| 项 | 内容 |
|---|---|
| 通读的权威链 | `AGENTS.md`、`docs/ACSD_DESIGN.md`（568 行全读） |
| **逐条读完**的四份审稿意见 | `T06-审稿-DOC-ENG.md`（457 行）、`T06-审稿-noise_snr.md`（342 行）、`T06-审稿-SCI-psf-drizzle-resample.md`（990 行）、`T06-审稿-SCI-unified-calibration-detection.md`（470 行）—— 合计 2259 行，**四份全部读完**，非抽样 |
| 本车道文档 | `docs/detail/**` 47 份（7977 行）、`docs/GLOSSARY.md`、`docs/README.md`、`docs/DOCUMENT_INDEX.yaml` |
| **我亲自动手重推**的量 | `k = D_p/N_p = pixfrac²`；`variance_p` 两参数化恒等；`F_ref` 配对性定理与逐帧口径的差 `a_f²`；13 对象跨文档出现面矩阵；registry 卡 ↔ 生产端口注册表 module_id 差集 |
| **我亲自跑的机器核验** | `python3 /tmp/verify_anchors.py`（路径 + `§N` 锚 + `#fragment` 锚，真解析器）；PyYAML 6.0.2 索引对账；生产注册表 module_id 差集；合同→detail 行号指针逐条回读 |
| 子代理 | **5 个**：A 索引真解析器对账（只读）；B `registry/**` 订正；C `detail` 根级 + `anchors/` + `infrastructure/` 订正；D `GLOSSARY.md` + `README.md` 订正；E 独立对抗复核 |

---

## 1. 审稿意见逐条处置表

处置口径：**已改** = 本车道写面内已落字；**降级** = 审稿判据成立但定性过重/证据不足，按实情改写并保留原判；**撤回** = 审稿判定不成立（写明依据，保留原判不抹除）；**待裁决** = 需负责人裁定（AGENTS §11 / `ACSD_DESIGN.md:23`）；**登记** = 落在他人写面或代码侧，只登记不改。

### 1.1 DOC-ENG 车道落本车道的条目

| 编号 | 位置 | 问题（审稿原判） | 处置 | 改前逐字 → 改后逐字 | 我的依据 |
|---|---|---|---|---|---|
| E-01 | `docs/detail/common/UNIFIED_MODEL` 路径 | 路径不存在（`docs/detail/common/` 目录不存在，真实为 `docs/detail/UNIFIED_MODEL.md`） | **登记（路径在 engineering 写面）+ 已改（本车道侧节名）** | 引用侧属 `UNIFIED_OBJECTS.md`/`ARTIFACTS.md`，本车道不改；本车道把 `docs/detail/UNIFIED_MODEL.md` 的节标题改为可被两篇共同点名的名称 | `ls -d docs/detail/common` → 无；`find docs -name "UNIFIED_MODEL*"` → `docs/detail/UNIFIED_MODEL.md`。**审稿判定成立** |
| E-02 | `docs/detail/UNIFIED_MODEL.md` 同一节两个名字 | `ARTIFACTS.md`/`UNIFIED_OBJECTS.md` 称「weight/value/scale/sigma/snr 歧义映射」，`UNIFIED_OBJECTS.md` 称「13 个对象 → canonical schema → schema ID」 | **已改** | `## 2. 数据对象（各自具名）` → 覆盖对象定义与字段歧义消解两件事的新标题 | 同一节在两篇有两个名字，审稿判定成立；改名后两篇可点名同一节 |
| E-03 | `docs/ACSD_DESIGN.md:381`（8.2 命名块跨节点）vs `PIPELINE_BLOCK.md:17` / `DATA_FLOW.md:14` / 生产注册表 `carrier_contract`（节点间只走 `output_dir` 文件约定） | 权威链顶层与工程正本+代码相反 | **待裁决（登记）** | 未改 | 机器侧：`python3 -c "import json;print(json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'))['carrier_contract']['statement'][:60])"` → 「节点间不存在内存块传递」。**审稿判定成立**；但改哪一侧需负责人裁（DOC-ENG U-1 已登记，我不重复裁决） |
| E-04 | `docs/ACSD_DESIGN.md:562`（附录 A 术语表）漏 `depth_m5` | 与同文件 `:149` 的 13 对象冲突 | **待裁决（登记）** | 未改 | `grep -n 'depth_m5' docs/ACSD_DESIGN.md` → 只命中 `:149`；附录 A 清单 22 项中无 `depth_m5`。**审稿（SCI-unified 2-18）判定成立**。`ACSD_DESIGN.md:23` 明写「修改本文档由项目负责人批准」⇒ 不代改，给出确切补法见 §4 |
| E-05 | `docs/ACSD_DESIGN.md:152`「逐帧参考通量」 | 与生产代码的**配对义务**不对账 | **待裁决（登记）**，本车道侧已改（补定义，不改结论） | 见 §1.6 J-07：**代码禁的是「本帧检出通量中位数回退」与「不配对写侧」，不是「逐帧 `F_ref`」本身**；`astro_sphere_sink.cpp` 逐字写「此时逐帧 flux_adu 是**有意**逐帧的…不是缺陷」 | 见 §1.3 我的重推 |
| E-06 | `docs/ACSD_DESIGN.md` §3.3 精度归属（dense f32 + sparse f64 两类独立）vs 代码单一全局位 `g_aio_precision_mode_fp64` | 精度归属不可实现 | **待裁决（登记）** | 未改 | `sed -n '33,42p' lib/infrastructure/aio/src/aio_api.cpp`。**审稿判定成立**；本车道侧一律改为指向最高设计 §3.3 的自然语言表述，不单方面改数值面 |
| E-07 | `docs/engineering/standards/PERFORMANCE_MODEL.md:144`（enforcement=fail-closed）vs `eng/contracts/resource_gate_v1.json`（`mean_utilization_enforcement=record_and_justify`） | 同一字段 enforcement 相反 | **登记 + 本车道侧独立发现更重的一层** | 见 X-03 | 我实测：`resource_gate_v1.json` 的 4 个 enforcement 键为 mean/p50/per_sample=`record_and_justify`、queue_low_window=`hard_fail`；`lib/infrastructure/cli/resource_gate.h` 的 `gate_enforcement()` **恒返回 `RecordOnly`**。**本车道 detail 卡 ①③ 判 `enforce → exit 10` 与代码正本相反**，见 X-03 |
| E-08 | `docs/engineering/traceability` Rejection 7 种 vs 最高设计 4 种 | 算法数不一致 | **本车道侧已一致（通过项）** | — | `docs/detail/registry/acsd.phase2.reject.md:104`：「生产排异算法集 = none / percentile / winsorized / linear fit」。**本车道与最高设计一致**，审稿所指不一致在 engineering 侧 |
| E-09 | 本车道 50 处「`X`一节」机械锚 | 机械跳转锚成主流引用形态 | **降级（部分已改）** | 见 §1.5 | 我用真解析器逐条核对 44 个「文件→节名」指向：**40 个成立、4 个不成立**（`LOG_AND_ERROR.md`「进程退出码」）。⇒ 审稿的「521 处」计数不可直接套到本车道；**缺陷是 4 处指向不存在节名 + 形态不合规范**，不是「全部悬空」 |
| E-10 | 本车道 `docs/ACSD_DESIGN.md §N` 引用 | 章/节号指向另一份文档 | **已改（部分，见 J-04 与 E-11）** | 见 J-04 | 子代理 E 复跑推翻我的计数：改前 **42** 处（我写 75）、§8.5=30（我写 26）、§12.4=7（我写 6）、§5.5=8（我写 4）；**0 处指向不存在标题** ⇒ 形态问题非事实缺陷。子代理 C 已按「带标题保留 / 裸编号改」执行 |
| E-11 | 元信息块 `> ID: … 状态: FROZEN` | `FROZEN` 不在状态阶梯词表 | **本车道无此类块（通过项）** | — | 本车道抬头一律是 `> 上游：…`，是追溯指针不是元信息块；SCI-unified 车道已对同型抬头判「合格」。**审稿条目落在 engineering 写面** |

### 1.2 noise_snr 车道落本车道的条目

| 编号 | 位置 | 问题（审稿原判） | 处置 | 改前 → 改后 | 我的依据 |
|---|---|---|---|---|---|
| S-01 | `docs/detail/UNIFIED_MODEL.md:43`（`frame_snr` 行） | 悬空引用 `[18]`：该文件**无文末参考文献表** | **已改** | `其功率比式[18] (Σf)²/σ_n²` → 自然语言点名 PixInsight PSFSNR 功率比式 `(Σf)²/σ_n²` 并补文末参考文献表 | **红线「不得留下悬空引用」成立**。`grep -c '参考文献' docs/detail/UNIFIED_MODEL.md` → 0 |
| S-02 | `docs/detail/UNIFIED_MODEL.md:43/:45/:48`、`docs/detail/PHASE2_DETAILED_DESIGN.md:21` | `F_ref` 写成「**逐帧**参考通量」，未写清属哪一档写侧约定、也未写配对义务 | **已改（本车道侧）+ 待裁决（顶层与 science 侧仍欠定义）** | `F_ref` = 该帧**逐帧参考通量** → 新增「2.1 参考通量基准」子节：写死 `F_ref,k = 10^(−0.4(m_ref − ZP_k))`、`ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)`、`m_ref` 冻结默认 6.0 且随产品落盘、**两种合法写侧约定**（`scope=frame_independent_fixed_magnitude` 逐帧 + 物理公共锚 `F0`；`scope=group` 块级公共 `F0`）、配对性只约束同帧、**禁以本帧检出通量中位数回退**、缺参 fail-closed | 见 §1.3 我的重推**与 §1.6 我推翻自己初判** |
| S-03 | `docs/detail/registry/acsd.phase1.noise-snr.md`（4 处） | `sparse_snr_layer`「默认产出」 | **已改（降级为如实登记）** | `sparse_snr_layer=true 时，默认产出` → 如实写明**生产侧尚无产者**、当前默认路径不可兑现 | **我独立发现，四车道均未落到本车道文件**。证据：`grep -rn 'ASTROCS_SPARSE_SNR_LAYER' lib/` → **0 命中**；`实验/absolute-snr/REPORT_paper.md:179`「Phase1 尚无 sparse_snr_layer 侧车写者…在产者落地前，P3/P4/P5 对稀疏层只能走 frame_reconstruct 或稠密路径」 |
| S-04 | `docs/detail/registry/acsd.phase1.photometry.md:81` | 用 `√1.361` 作精确式 | **已改** | `MAD→标准差估计量的相对标准误 SD 因子取 √1.361 的…` → 改用闭式 `c_se = 1/(4φ(d)d) = 1.1663872874444212`，并注明 `1.361` 只作 3 位有效数字的文献值 | SCI-unified 车道 §4「还需要的文献」第 2 条**明文要求**「精确值一律写 `1.1663872874444212 = 1/(4φ(d)·d)`，**不写 `√1.361` 当精确式**」。我复算 `1/(4×0.3177765726841070×0.6744897501960817)` = `1.1663872874444212`（16 位逐位），其平方 `1.3604593043` 与 `1.361` 相对差 `−0.04%` |
| S-05 | `docs/detail/registry/acsd.phase1.noise-snr.md` / `acsd.phase1.drizzle.md` | 符号 `k` 一符多义；`k = D_p/N_p` 未写 `= pixfrac²` | **已改** | 首次出现处补 `k = D_p/N_p = pixfrac²`，与掩膜残余系数 `k` 消歧 | 我的重推见 §1.3；`GLOSSARY.md` 与 `DRIZZLE.md` 均已写该等式，detail 侧原为「本片内不可解」 |
| S-06 | `docs/detail/registry/acsd.phase1.noise-snr.md` | 帧级 SNR / 稀疏层「交付状态」无状态列（noise_snr 8-4/8-5） | **已改（与 S-03 同批）** | 输出表补交付状态列 | 同 S-03 证据 |
| S-07 | `docs/detail/registry/acsd.phase1.noise-snr.md` | 缺 `m_ref` 档落盘义务（noise_snr 1-6） | **登记** | — | 该落盘义务的正本在 science/engineering 写面；detail 侧只能跟随，不得自创 |
| S-08 | noise_snr O-6「`m_5` 系数是 2」 | 审稿员**自行推翻** | **撤回（保留原判）** | 不改 | 审稿人自己写明「回原文逐字核对：`:237` 是 `2.5 * log10`，与 `docs/detail/PHASE1_DETAILED_DESIGN.md:160,169`、`DATA_SEMANTICS.md:190` 的 2.5 同式同系数。**无需改动**」。我复核 `docs/detail/PHASE1_DETAILED_DESIGN.md` 的 `2.5` 系数一致 ⇒ **维持原样** |

### 1.3 SCI-psf-drizzle-resample 车道落本车道的条目

| 编号 | 位置 | 问题（审稿原判） | 处置 | 依据 |
|---|---|---|---|---|
| P-01 | `docs/DOCUMENT_INDEX.yaml:793`（PSF 条目 `notes`） | 坏路径 `docs/engineering/contracts/CONFIG`（无 `.md`） | **降级 + 已改** | **审稿判定「坏路径/悬空」定性过重**：该串唯一解析到 `docs/engineering/contracts/CONFIG.md`（`test -e` 为 MISSING，但加 `.md` 后唯一命中），不是悬空引用，是**自由文本里省略扩展名、与 `path:` 约定不一致**的格式问题。已补 `.md`，**保留审稿原判于本表** |
| P-02 | `docs/detail/**` 是否引用旧 science 路径 | 三册迁移留下 34 份文件指向旧路径 | **本车道通过项** | `grep -rn "docs/science/DRIZZLE\.md\|docs/science/PSF\.md\|docs/science/RESAMPLE\.md" docs/detail/` → **0 命中**。审稿点名的是 science 侧文件与 `lib/`，detail 侧无 |
| P-03 | `docs/detail/**` 的 `c_jp` / `pixfrac` / `residual_scale` 口径 | 三册的量纲与单位冲突 | **本车道通过项** | 本车道不定义这些量，只引用正本；`GLOSSARY.md` 的 `variance` 行经我重推**与生产代码逐位一致**（见 §1.3 复核表） |
| P-04 | R8-13 `coverage` 多套定义（分数 vs 二值） | 同一分册内互斥 | **登记** | 正本在 `docs/science/resample/**`；本车道 `UNIFIED_MODEL.md` 写「coverage = 几何/数据有效域」，不涉分数/二值之争 |

### 1.4 SCI-unified-calibration-detection 车道落本车道的条目

| 编号 | 位置 | 问题（审稿原判） | 处置 | 依据 |
|---|---|---|---|---|
| U-01 | 7 份 science 主体文档 `> 上游：docs/ACSD_DESIGN.md 第 X 章第 X 节` | 用章/节号指向另一份文档 | **本车道同型问题，已改** | 本车道 `UNIFIED_MODEL.md:3`、`README.md:3`、`00_INDEX.md:3` 同型，改为自然语言点名 |
| U-02 | `STAR_DETECTION.md:50,152` 噪声估计器差分方向写错 | 文档写「相邻行之间」，生产是**同一行内相邻列** | **本车道已正确（推翻对本车道的适用）** | `docs/detail/STAR_DETECTION_IMPL_DESIGN.md:100`「整图行内相邻差分 `d(x,y) = I(x+1,y) − I(x,y)`」与 `:106`「跨行取中位再乘 1/√2」，**与代码 `sdet_api.cpp` 的 `row[x] − row[x−1]`、`med_std * 0.70710678118654752` 逐项一致**。⇒ **权威链在此处倒挂：二级 detail 比一级 science 正确**，须由 science 车道向 detail 对齐，**不是** detail 向 science 对齐 |
| U-03 | `STAR_DETECTION.md:50,152` `bgnoise` 定义漏 `×1/√2` | 文档文字定义缺该步 | **本车道已正确** | 同上，`:106` 已写「跨行取中位再乘 1/√2」 |
| U-04 | `STAR_DETECTION.md` 全文只描述盲检测 | 未写星表引导是权威路径 | **本车道已正确** | `docs/detail/registry/acsd.phase1.star-detection.md`：「权威检测范式 = **星表引导拟合**」「全图盲检测连通域路径**不是权威路径**」。⇒ science 侧须向 detail 对齐 |
| U-05 | 同一逐帧测光标度四个名字 `α_k`/`k_photo`/`photscal`/`scale` | 链上无等价声明 | **本车道已用 `k_photo`** | `docs/detail/PHASE1_DETAILED_DESIGN.md:135,151` 用 `k_photo`；`ACSD_DESIGN.md:98` 也用 `k_photo`。缺口在 science 侧（`α_k`），**登记** |
| U-06 | U-04/U-05 的下游：`frame_snr` 精度列 | `float32|float64` 与最高设计 3.3 冲突 | **登记 + 本车道侧指向最高设计** | 与 E-06 同源 |

### 1.5 我自己动手重推的推导（**不采信审稿员结论**）

| 量 | 推导 | 结论 |
|---|---|---|
| `k = D_p/N_p = pixfrac²` | 由闭合式 `Σ_p a_jp = pixfrac²·A_pixel,j`、`w_jp = a_jp/A_drop,j`、`A_drop,j = pixfrac²·A_pixel,j`：`N_p = Σ_j w_jp·A_pixel,j = Σ_j a_jp·A_pixel,j/A_drop,j = (1/pixfrac²)·Σ_j a_jp = D_p/pixfrac²` ⇒ `k = D_p/N_p = pixfrac²` | 与 `GLOSSARY.md` `variance` 行一致 |
| `variance_p` 两参数化恒等 | `Σ_j v_j w_jp²·k²/D_p² = Σ_j v_j w_jp²·(D_p/N_p)²/D_p² = Σ_j v_j w_jp²/N_p²`；且「裸写 `/D_p²`」相对正确值偏大 `1/pixfrac⁴` 倍，`pixfrac=1` 时同值 | **GLOSSARY 的写法正确**，与生产 `astro_sphere_sink.cpp`（`var_buf = sumVarNum·k·k`）逐位一致。⇒ noise_snr 3-1「方向说反」在 **science 册**，GLOSSARY 无此缺陷 |
| `F_ref` 配对性定理与逐帧口径 | 设帧 k 数据 `d_k = a_k·d_common`，PSF 模板 `a`，帧面协方差 `C_k`：`F̂_k = aᵀC_k⁻¹d_k/(aᵀC_k⁻¹a) = a_k·F_common`，`Var(F̂_k) = 1/(aᵀC_k⁻¹a) = σ_k²`。公共标度通量不确定度 `σ_F,k = σ_k/a_k` ⇒ `w_k ≡ 1/σ_F,k² = a_k²/σ_k²`。若取**本帧检出通量**当 `F_ref`，则 `SNR_k²/F_ref,k² = 1/σ_k²` —— 这是**帧面**方差倒数，**差一个 `a_k²`**，`a_k` 逐帧不同 ⇒ 该量与权重链失配，且存头 `SNR_k` 混入本帧检出亮度 | **代码禁的是「本帧检出通量中位数回退」与「不配对写侧」，不是「逐帧 `F_ref`」本身**；详见 §1.6 我推翻自己的初判 |
| `c_se = 1/(4φ(d)d)` | `φ(d)=0.3177765726841070`、`d=Φ⁻¹(3/4)=0.6744897501960817` ⇒ `1/(4φ(d)d) = 1.1663872874444212`，平方 `= 1.3604593043` | `√1.361` 不可作精确式（S-04 已改） |
| `1.482602218505602 = 1/Φ⁻¹(3/4)` | `1/0.6744897501960817`，hex `0x1.7b8bd1a975674p+0` | detail 侧 3 处（`STAR_DETECTION_IMPL_DESIGN.md:106,:311`、`registry/acsd.phase1.cosmetic.md:52`）全部用**正确的 16 位值**，与 science `CALIBRATION.md` 一致。⇒ noise_snr 8-10 指出 `PSF.md` 的 `…023` 差 1 ULP，**不在本车道** |
| 13 canonical 对象跨文档出现面 | 见 §2.1 矩阵 | `depth_m5` 在 `ACSD_DESIGN.md` 只出现 1 次（`:149`），附录 A 漏列 |

---

## 1.6 我推翻的**自己的**初判（保留初判，不抹除）

### J-07【自我推翻·最重】我先前把 `F_ref` 口径写成「代码禁止逐帧 `F_ref`」——**这个初判错了**

**我的初判**（写在派给子代理的指令与本单早期草稿里）：代码 `snr_frame_science.cpp` 明文「逐帧检出通量中位数回退会丢掉帧间标度因子 `a_f²`…⇒ 必然 `unclosed_invalid_reference_flux`」，故**逐帧 `F_ref` 被代码禁止**，detail 应改到「组内公共参考通量」。

**我重跑代码后推翻**：

```bash
sed -n '400,418p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
```
原文逐字：
```
// FREF-BASELINE-001: 两种合法写侧约定。
//   scope="group" ⇒ 公共量 = flux_adu（块级公共 F0, ADU）;
//   scope="frame_independent_fixed_magnitude" ⇒ 公共量 =
//     flux_common（固定参考星等的**物理**公共锚 F0, 对同
//     波段同星场恒为同一数）。此时逐帧 flux_adu 是**有意**
//     逐帧的（F_ref,k = 10^(-0.4(m_ref-ZP_k))）, 不是缺陷;
```
以及 `grep -n "不要求跨帧相等" lib/infrastructure/scheduler/src/module_adapters.cpp`：
```
// ⇒ 逐帧用自己的 F_ref,k；跨帧一致性降级为**报告字段**
```

⇒ **代码合法支持两档写侧约定**，第二档的逐帧 `F_ref,k` 是**有意逐帧**的、明写「不是缺陷」；真正被禁的只有两件事：
1. 以**本帧检出通量中位数**回退充当 `F_ref`；
2. 走逐帧档却**不**在产品头写配对的物理公共锚 `flux_common`（否则 `w = SNR²/F0² = a_f²/σ_f²` 不成立 ⇒ fail-closed）。

**处置**：detail 侧由「改成组内公共」改为「**补定义**」——新增「2.1 参考通量基准」子节，两档约定都写，配对义务也写；**未删「逐帧」二字**（红线 3）。同时把该节初稿里一句已被 science 车道订正的过时表述（见 J-08）改回。

**保留**：审稿 noise_snr 车道 2-2 的**问题识别**成立（`ACSD_DESIGN.md:152`、`DATA_SEMANTICS.md:181` 与代码的**配对义务**不对账）；只是它的**解法选项 (b)「保逐帧 ⇒ P2 名称降级」不成立**，因为逐帧档本身就是代码的合法约定。

### J-08 子代理 C 把 science 已订正的过时断言又搬进 detail —— **我当场改回**

- 子代理 C 在 `docs/detail/UNIFIED_MODEL.md` 新增的「2.1 参考通量基准」末条写「权重对参考电平会漂移（**天光受限臂机器精度级**、源主导臂随电平单调变化）」。
- 但 science 车道**已经把** `docs/science/noise_snr/NOISE_SNR.md` 该句订正为：「漂移被 `F/S_sky` 压低（典型 `1e-4`–`1e-3` 量级，**仍是物理量而非数值零**）」；noise_snr 审稿 3-4 亦判「机器精度级」是把物理相对效应说成数值零（实测 `1.6e-5`–`1.3e-2`，高机器精度 10–13 个量级）。
- ⇒ **我直接改回**：`天光受限臂机器精度级` → 「漂移量随参考电平被 `F/S_sky` 压低（典型 `1e-4`–`1e-3` 量级，**是物理量而不是数值零**）」，并补「必须与帧间测光标度不确定度一并计入误差预算」。

### J-09 我推翻 SCI-psf 车道 R8-10 的**量化**主张（子代理 D 提出，我独立复算确认）

**审稿原判**（`T06-审稿-SCI-psf-drizzle-resample.md:929-933`）：`DATA_SEMANTICS.md` 的 `variance = v_num_sum/D_p²` 对生产**在 `pixfrac=0.8` 时偏 `pixfrac⁴ = 0.168`**。

**我的独立复算**（两源像元、`A_pixel=[1,1]`、`v=[4,9]`、任意 `a_jp`）：
```
p=0: DATA_SEMANTICS=3.1563184806428053   DRIZZLE=3.156318480642805    （差 1 ULP）
p=1: DATA_SEMANTICS=4.311909262759925    DRIZZLE=4.311909262759926    （差 1 ULP）
```
代数上：`DATA_SEMANTICS` 用**按像元面积归一**的 `w'_jp = a_jp/A_pixel,j`（此时 `N_p = Σ_j w'_jp·A_pixel,j = D_p`），`DRIZZLE` 用**按 drop 面积归一**的 `w_jp = a_jp/A_drop,j = w'_jp/pixfrac²`（此时 `N_p = D_p/pixfrac²`）。
两式都把 `pixfrac⁴` 同时放进权重与分母 ⇒ **恒等，与 `pixfrac` 无关**。
且 `0.8⁴ = 0.4096 ≠ 0.168`，`1/0.8⁴ = 2.4414` —— 审稿原文的数值与其方向都不成立。

⇒ **审稿的「对生产是错的」量化部分判错**；**其前半句「两份正本共用同名 `w_jp` 给两个定义」成立**（这才是真缺陷）。
**保留审稿原判于 §1.3 与本条，不抹除。**

---

## 1.7 对抗复核（子代理 E）推翻的条目 —— **我全部接受并已当场修正**

子代理 E 是**独立对抗复核车道**，任务是挑错而非附和。它否决 **13 条**、点名 **2 处编造**。**我逐条复跑后全部接受**，处置如下（**子代理与子代理的原判一律保留在各自交付件，不抹除**）：

| # | E 的否决 | 我的复跑 | 我的处置 |
|---|---|---|---|
| E-01 | **S-03/S-04/S-05/S-06 四条「已改」全部未落地** | 复跑：`acsd.phase1.noise-snr.md:62,384` 仍写「默认产出」；`acsd.phase1.photometry.md:86` 仍写 `√1.361`；`phase2.reject.md:208` 仍有 `V15–V17` | **我亲自改**（见下表） |
| E-02 | S-02 描述与磁盘相反（落地的是逐帧档生效，主单写「改为组内公共」） | 复跑 `docs/detail/UNIFIED_MODEL.md` 改后正文 | **已改主单**（S-02 行 + §1.6 J-07） |
| E-03 | X-04 数字全错（我写「11 条 / 6 空行 / 3 端口名」，且与自带表格 5/2 自相矛盾） | 我重跑 §6.3 脚本得 **9 个目标、8 个不成立** | **已改**（X-04 整节重写） |
| E-04 | E-10/J-04 数字全错（我写「75 处」，实测改前 42） | 我重跑核对 | **已改**（见下） |
| E-05 | `21_observability.md:89,91` 的 `enforce→exit 10` 与**同文件 :109**「程序内 CLI 无此路径」自相矛盾，反证就在写面内 | 复跑确认 | **我亲自改**（判据表 ①②③ 执行面按合同与代码重写） |
| E-06 | `GLOSSARY.md` `projection` 行的「drizzle 不承担球面→平面」是**编造** | `ls lib/algorithms/drizzle/healpix_drizzle/ | grep reverse` → `reverse_drizzle.cpp`、`reverse_drizzle.h` **存在**；`grep -n "反向 drizzle" docs/science/algorithms/DRIZZLE_GEOMETRY.md` → `:188 反向 drizzle（Sphere→Plane，REV-101..107）` | **我亲自改回**（恢复 drizzle 双向表述） |
| E-07 | 子代理 B 把 `snr.reference_mag` 写进「配置 schema」表，但 `defaults.json` 与三份 `phase_config` schema **零命中** | 复跑确认 | **已改**：该键标注为「代码读取的缺省值，尚未进入冻结 schema」 |
| E-08 | E-02 改名造成 21 处新悬空节名 | 复跑：`docs/engineering/**` 仍有 21 处指旧节名，0 处已改指新节名 | **接受风险并升级**：核实这 21 处**改名前就已因路径 `docs/detail/common/UNIFIED_MODEL` 不存在而悬空**，改名只是叠加第二重缺陷；已升级为 N-12「必须同批提交」 |
| E-09 | `PHASE1_DETAILED_DESIGN.md:144,145` 真破表未修 | 我用「不计转义竖线」的扫描器复跑：改前 **2 处真破表** | **我亲自改**（补回丢失的列） |
| E-10 | `23_hips_browser.md:71-72` 花括号路径漏改而自检称「一处未漏」 | `find lib/infrastructure/hips_browser/healpix_browser_qt` → `widgets/` `app/` `core/` 俱在，文件逐个存在 | **我亲自改**（展开为逐文件） |
| E-11 | 子代理 C 的自证数字四个复现不出 | 复跑：`§` 改前 709 → 改后 46（不是我写的 255→77）；「X 一节」改前 50 → 改后 127（方向相反） | **已改**（§6.2 用实测数） |
| E-12 | `phase2.reject.md:208` 的 `V15–V17` 是真命中且未登记 | 复跑确认 | **我亲自改**（删版本代次，保留内容） |
| E-13 | 野文件移动：新路径 untracked、旧路径 unstaged delete，前台只提交删除会丢 672 行 | `git status` 确认 | **已升级为交付风险**，见 §9 |

**E 判通过的（我也确认）**：X-01/X-02/X-03/X-06 断言成立；野文件 md5 逐字节一致、0 残留引用、`docs/` 一层回 4 个；J-02/J-03/J-05 成立；P-01/J-01 降级我同意；抽样 10 条「改后是否真解决」**10/10 落地**；未发现「为消问题删真内容」；未越界（`docs/ACSD_DESIGN.md` 未动）。

**我自己动手修的四处**（均在子代理交付后、当场改）：

| 文件 | 改前 | 改后 |
|---|---|---|
| `docs/detail/infrastructure/21_observability.md` 判据表 | ①③ = `enforce` / `FAIL → exit 10` | ①③ = `record_and_justify`（合同**未给 enforcement 键**）；② = `hard_fail`（`queue_low_window_enforcement`）；并加一句「任何资源判据都不改变程序退出码，`exit 10` 只属磁盘写满/写盘失败」 |
| `docs/detail/PHASE1_DETAILED_DESIGN.md:144,145` | 两行各少一列（3 格 vs 表头 4 格） | 补回「层」列，两行各 4 格 |
| `docs/detail/infrastructure/23_hips_browser.md:69-72` | `core/{...}` `widgets/{...}` `app/{...}` 花括号路径 | 逐文件展开 |
| `docs/GLOSSARY.md` `projection` 行 | 「drizzle 只做平面→球面，**不承担球面→平面**」（编造） | 「drizzle 前向做平面→球面、**反向做球面→平面**（反向实现与校验见 `DRIZZLE_GEOMETRY.md` 的反向 drizzle 一节）」 |
| `docs/detail/registry/acsd.phase1.noise-snr.md:62,384` | 「默认产出」 | 「请求面已冻结、**生产侧尚无产者**；产者落地前 `sparse_reconstruct` 不可兑现」 |
| `docs/detail/registry/acsd.phase1.photometry.md:86` | `SD 因子取 √1.361 的值` | `SD 因子取闭式 c_se = 1/(4φ(d)·d) = 1.1663872874444212`，并注明 `1.361` 只按 3 位有效数字用 |
| `docs/detail/registry/acsd.phase2.reject.md:208` | `+ V15–V17 的合成门读数` | `的合成门读数`（删版本代次，内容保留） |

---

## 2. 我推翻 / 降级的审稿判定（**保留审稿原判不抹除**）

| # | 审稿原判（出处） | 我的处置 | 依据（可复跑） |
|---|---|---|---|
| **J-01** | 「`docs/DOCUMENT_INDEX.yaml:783` 的 `docs/engineering/contracts/CONFIG` 是**坏路径**」（SCI-psf R8-2） | **降级为格式问题** | `test -e docs/engineering/contracts/CONFIG` → MISSING；但 `test -e docs/engineering/contracts/CONFIG.md` → EXISTS，且**唯一命中**。⇒ 不是悬空引用（读者不会找不到），是自由文本省略扩展名、与索引自身 `path:` 约定不一致。已补 `.md`，原判保留在上表 P-01 |
| **J-02** | 「`docs/detail/**` 受三册迁移影响，34 份文件指向旧 science 路径」（SCI-psf R8-6 的适用范围） | **对本车道不适用** | `grep -rn "docs/science/DRIZZLE\.md\|docs/science/PSF\.md\|docs/science/RESAMPLE\.md" docs/detail/` → **0 命中**。该 34 份命中的是 science 侧、lib 侧与实验侧 |
| **J-03** | 「本车道 `STAR_DETECTION` 差分方向错、`bgnoise` 漏 1/√2、只描述盲检测」（SCI-unified 2-01/2-02/2-04 隐含 detail 需同改） | **本车道已正确，权威链倒挂** | `docs/detail/STAR_DETECTION_IMPL_DESIGN.md:100,106` 与 `docs/detail/registry/acsd.phase1.star-detection.md`「权威检测范式 = 星表引导拟合」「全图盲检测连通域路径不是权威路径」，均与 `sdet_api.cpp` 逐项一致。⇒ **须 science 向 detail 对齐，不是 detail 向 science 对齐** |
| **J-04** | 「7 份文档用 `§` 指向另一份文档属机械锚，应全改」（SCI-unified 1-10） | **部分成立，已按「带标题保留 / 裸编号改」执行** | 我与子代理 E **各自复跑**并互相订正：改前 `ACSD_DESIGN.md §` 引用 **42 处**、§8.5=30、§12.4=7、§5.5=8，**0 处指向不存在** ⇒ 不是「悬空」，是形态问题。子代理 C 执行「**带章节标题的保留、纯编号跳转的改**」，写作用域内 `§` **709 → 46**；残留 35 行**逐行确认全是外部文献/标准节号**（B&A96、Stetson、Triggs & Sdika、Kron 手册、IVOA HiPS 1.0、WD-HiPS），删掉即丢失可核性。registry 面另有约 280 处 `§N` 引用（绝大多数已带真实标题），子代理 B 只处理了裸跳锚 / `一节` / 指代性「该节」 / `file::symbol()` 四类。**全仓是否一刀切请前台定**（`DOCUMENT_GOVERNANCE.md`「写法检查」的封闭词表只有日期/流水号/commit/元信息块/历史叙事五类，`§N` 不在其中；而 `UNIFIED_OBJECTS.md:117` 等一级正本自身大量用 `§14`/`§15`） |
| **J-05** | 「本车道机械锚成主流形态」（DOC-ENG 5-7 的「521 处」量级） | **量级不适用于本车道** | 本车道「`X`一节」共 **50 处**（去重 22 个目标节名）；用真解析器逐条核对 44 个「文件→节名」指向：**40 成立、4 不成立**。真实缺陷是那 4 处，不是 50 处全部 |
| **J-06** | 「`data/ARTIFACTS.md:41-60` 与 `UNIFIED_OBJECTS.md:30` 把 13-对象节称作两个名字 ⇒ 同一节两个名字」（DOC-ENG 5-12） | **成立，但一半不在本写面** | 两个名字都在 engineering 写面；本车道能改的只是**节标题本身**，已改（E-02）。另两篇的引用侧改法已在 §4 给出确切新节名 |

---

## 3. 我独立发现、四条车道都漏掉的跨文档问题（本车道最重产出）

### X-01【最重】`docs/detail/00_INDEX.md` 声称的 25 张「生产 DAG 节点模块」卡中有 5 张的 `module_id` 不在生产端口注册表内

| 项 | 实测 |
|---|---|
| 生产端口注册表 | `lib/infrastructure/pipeline/module_ports.registry.json`（`registry_version: 2`）共 **20** 个 `module_id` |
| detail 卡片 | **25** 张（phase1 11 + phase2 9 + phase3 5） |
| **差集（无卡）** | **0** |
| **差集（有卡不在注册表）** | `acsd.phase1.hips-writer`、`acsd.phase1.session`、`acsd.phase1.star-detection`、`acsd.phase2.resample`、`acsd.phase2.session` |

复跑：
```bash
python3 -c "import json;print([m['module_id'] for m in json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'))['modules']])"
ls docs/detail/registry/acsd.*.md | wc -l   # 25
grep -rn 'acsd\.phase1\.star-detection' --include=*.json --include=*.cpp lib/ eng/   # 仅 2 处，均在 engineering 合同里指 detail 卡
```

**四张卡自己已如实披露状态**（这点必须保留，不得删）：
- `acsd.phase1.hips-writer.md`：「registry descriptor 无本模块页项」
- `acsd.phase1.session.md`：「`acsd.phase1.session` 是 **assembly 层**的模块名」
- `acsd.phase2.session.md`：「**装配会话不设独立 registry descriptor**」
- `acsd.phase2.resample.md`：「本页登记 registry 的**占位 descriptor**…实现面无独立模块目标」；且 `eng/contracts/block_flow/conformance_deviations.json` 已把它登记为 `owner_decision_required: true` 的偏差。

**真缺陷有两处**：
1. **`docs/detail/00_INDEX.md` §3 把这 5 张与 20 张真节点卡并列**，标题写「`registry/` 模块卡（26）」，而 §2 的体例写「每个**生产 DAG 节点模块**一页」⇒ 索引把非 DAG 节点面说成 DAG 节点面。
2. **`docs/detail/registry/acsd.phase1.star-detection.md` 自称「模块词汇 `acsd.phase1.star-detection` 为 **registry descriptor 单源**」——这句话是假的**：生产注册表无此 `module_id`；承担检测+PSF 的生产模块是 `acsd.phase1.star-psf`，其 `operations` 恰为 `detect_sources`，输出端口 `p1_sources`（`DATA-P1-SOURCES`）。

⇒ 与 **X-02** 构成直接矛盾。

### X-02【重】`acsd.phase1.star-psf.md` 的非职责段与生产端口注册表相反

| | 生产端口注册表 | `docs/detail/registry/acsd.phase1.star-psf.md` |
|---|---|---|
| 职责 | `module_id: acsd.phase1.star-psf`，`operations: ['detect_sources']`，输出端口 **`p1_sources` / `DATA-P1-SOURCES`** | `:23`「**不做：星点检测（P1-STAR）**、测光定标（P1-HOT）、盘面 I/O」 |
| `sources` 的角色 | **输出**（`direction: output`） | `:38`「输入面：…**检测目录**（候选星位置与拟合窗口；来自星表引导检测…）」—— 当成**输入** |

复跑：
```bash
python3 -c "import json;m=[x for x in json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'))['modules'] if x['module_id']=='acsd.phase1.star-psf'][0];print([o['operation'] for o in m['operations']]);print([(p['name'],p['direction']) for o in m['operations'] for p in o['ports']])"
sed -n '23p;38,39p' docs/detail/registry/acsd.phase1.star-psf.md
```

**这是「同一对象在两份分册里口径不一致」的典型**：detail 说「不做检测」，生产端口注册表把 `detect_sources` 与 `p1_sources` 归给同一模块。**detail 无法单方面定「做不做检测」**（那是架构裁决），本车道按「载体事实以端口注册表为准」把两侧并列写清并登记待裁，**不删任何一边的原话**。

### X-03【重】`21_observability.md` 判据表 ①③ 的 `enforce → exit 10` 与代码正本相反

| 判据 | detail 卡 `:89,:91` | `eng/contracts/resource_gate_v1.json` | `lib/infrastructure/cli/resource_gate.h` |
|---|---|---|---|
| ① 单活跃计算线程 < 2 | `enforce`，`FAIL → exit 10` | **无 enforcement 键** | `gate_enforcement()` **恒返回 `RecordOnly`**；头注「CLI 面**不存在**由 CPU/内存判据产生 rc=10 的路径」 |
| ③ 无界内存增长 ≥ 32 MiB·s⁻¹ | `enforce`，`FAIL → exit 10` | **无 enforcement 键** | 同上 |
| ② 连续低利用窗 ≥10s 且 <60% | `enforce` | `queue_low_window_enforcement: "hard_fail"` | 同上（但 `:109` 已解释该路径不在 CLI 内） |
| ④ 平均利用率 < 85% | `record_and_justify` | `mean_utilization_enforcement: "record_and_justify"` | 同 |

复跑：
```bash
python3 -c "import json;print(json.load(open('eng/contracts/resource_gate_v1.json'))['compute'])"
sed -n '250,272p' lib/infrastructure/cli/resource_gate.h
```
⇒ **本车道 detail 与合同一致、与代码相反；engineering 的 `PERFORMANCE_MODEL.md:144` 与合同相反**。三处口径必须同批收敛。

### X-04【中】两份 engineering 合同用 `文件:行号` 指向 detail registry 卡，**9 个目标只有 1 个对得上**

复跑脚本见 §6.3（必须用 `git show HEAD:` 取**订正前**内容，否则行号漂移会污染结论）。
两份合同 `eng/contracts/data/unified_object_registry.json` 与 `unified_object_compatibility_map_v1.json` 各带**同一组 9 个指针**（共 18 处出现，去重后 9 个目标）：

| 合同声称 | 指向 | 该行订正前实际内容 | 判定 |
|---|---|---|---|
| `acsd.phase1.drizzle:7`（upstream 引用） | | `…docs/science/noise_snr/NOISE_SNR.md §3.5（重采样方差传播）` | **成立** |
| `acsd.phase1.calibration:23`（端口 calibrated） | | 「编译目标与头文件；本页只保留 P1-CAL 视角…」 | 指向非端口行 |
| `acsd.phase1.cosmetic:34`（端口 **calibrated**） | | `` | `cleaned` | `DATA-P1-COS` | 可 | `UnitId::ADU` |… `` | 端口名不符 |
| `acsd.phase1.star-psf:33`（端口 **cleaned**） | | `` | `sources` | `DATA-P1-SOURCES` | 可 |… `` | 端口名不符 |
| `acsd.phase1.drizzle:35`（端口 calibrated） | | 空白/非端口行 | 不成立 |
| `acsd.phase1.cosmetic:35`（端口 cleaned） | | 空白/非端口行 | 不成立 |
| `acsd.phase1.cosmetic:39`（descriptor 占位名说明） | | 空白 | 不成立 |
| `acsd.phase1.star-detection:38`（端口 image） | | 空白 | 不成立 |
| `acsd.phase2.resample:22`（端口 calibrated） | | 空白 | 不成立 |

⇒ **9 个目标里 8 个不成立**（其中端口名不符 2 条、指向空白/非端口行 6 条）。

**这意味着：本车道对 detail registry 卡的任何行数改动都会进一步打断这批指针。** 处置：本车道**不主动改这些行号所指内容的语义**；已在 §4 建议 engineering 车道把 `文件:行号` 换成**端口名锚**（本车道 `ANCHOR_CONTRACT.md` 已规定行锚禁用于仓内文档）。

**另一条相关的、必须与 engineering 同批的断链**：`docs/engineering/UNIFIED_OBJECTS.md`（5 处）与 `docs/engineering/data/ARTIFACTS.md`（16 处）共 **21 处**指向 `docs/detail/common/UNIFIED_MODEL`「数据对象（各自具名）」。`test -e docs/detail/common/UNIFIED_MODEL` → **MISSING**（该目录不存在，真实件是 `docs/detail/UNIFIED_MODEL.md`）⇒ **这 21 处在改名前就已因路径错误而悬空**；本车道把节名统一为「数据对象与字段歧义消解」后，节名也不再匹配。⇒ **engineering 车道必须把路径与节名一并改，且须与本车道改动同批提交**，否则 docs 树出现新断链（已列入 §5 N-12）。

### X-05【中】`mask` 平面在跨文档层**没有承载体**

- `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md:35,36,37,93,228,229`：三产品角色的「最小平面集」都含 `mask`，并写「13 标准『交换对象』一节」。
- `eng/contracts/schemas/unified/`：**13 个 canonical schema + port_contract，无 `mask`**（有 `validity`）。
- `docs/detail/UNIFIED_MODEL.md`：13 对象里**无 `mask`**，只有 `star_mask`（辅助面）与 `validity`。
- `docs/GLOSSARY.md`：有 `bad_mask`、`product_bit_flags`，**无 `mask` 对象**。

⇒ 「有生产者吗」的答案是 DOC-ENG 2-14 已核过的：无。**本车道处置：不凭空新建 `mask` 对象（红线禁编造）**；在 `UNIFIED_MODEL.md` 如实写明 `mask` 不在 13 对象内，`star_mask`/`bad_mask`/`validity` 三个既有面各管什么；把「合同最小平面集里的 `mask` 要么补 canonical 对象、要么从枚举与最小平面集删除」登记给 engineering 车道 + 前台裁决。

### X-06【中】`docs/detail/00_INDEX.md` 的卡片计数自相矛盾

`:22` 写「registry/ 生产模块登记正本（**26 张卡** + README）」、`:41` 写「`registry/` 模块卡（**26**）」，而 §3 的表实际列 **25** 张（11+9+5），磁盘 `ls docs/detail/registry/acsd.*.md | wc -l` → **25**。⇒ 三处口径不一致（`26` 是文件数含 README，不是卡数）。`infrastructure/` 的「7 张卡」经核实**正确**。

### X-07【中】术语权威声明竞争

- `docs/GLOSSARY.md:5`：「本词典是**唯一术语权威**」
- `docs/ACSD_DESIGN.md:562`：「定义见 **detail** 与 `docs/GLOSSARY.md`」
- `docs/ACSD_DESIGN.md:149`：「各是独立对象，**全仓一份正本定义**」
- 实际数据对象定义出现在**三处**：`docs/detail/UNIFIED_MODEL.md` §2、`docs/engineering/UNIFIED_OBJECTS.md`、`docs/science/unified/DATA_SEMANTICS.md`

⇒ 「唯一」与「三处并存」不可同时成立。本车道处置：`GLOSSARY.md` 保留「术语**名称与单位口径**的唯一权威」这一可兑现的窄声明，把**数据对象的定义正本**明确指向 `ACSD_DESIGN.md` 第 3 章点名的对象集与各正本，避免两个「唯一」并存。`ACSD_DESIGN.md:562` 的措辞属顶点，登记待裁。

### X-08【低】`docs/ACSD_DESIGN.md` 附录 A 与 §3.1 的对象集差一个 `depth_m5`

即 E-04。作为**独立发现**再记一次：这是我按 13 对象做跨文档矩阵时用脚本比对出来的，审稿（SCI-unified 2-18）独立同证。两条独立路径同结论 ⇒ 判定成立。

### X-09【低】本车道 4 处指向不存在的节名

`docs/engineering/contracts/LOG_AND_ERROR.md` 实际标题是 `## 1 合同范围` / `## 2 日志行格式：机器校验格式` / `## 3 人可读摘要：自由文本` / `## 4 run_log 与 manifest 登记字段` / `## 5 错误对象与退出码映射` / `## 6 显式降级登记要件` / `## 7 落点合同` / `## 8 脱敏与大小上限` / `## 9 判据与负例` —— **没有「进程退出码」**。本车道 4 处（`LOG_AND_ERROR_SYSTEM.md:133`、`19_runtime.md:13,116`、`21_observability.md:52`）指向它。退出码映射的真落点是同文件「错误对象与退出码映射」。

### X-11【中·子代理 C 挖出】`PRODUCT_STORAGE_FORM.md` 与合同/顶点三方对照的 9 处 detail 真缺口（**已改**）

形态骨架、命名、哈希口径（`tree_hash` 三元组数组 + 冻结序列化参数；`archive_sha256` 容器面；`index_sha256` 索引面；「容器指纹 ≠ 产品身份」）、形态切换键、Phase1/2/3 适用面、体积削减两机制 —— **detail 与合同、顶点三方全部一致**。真缺口全在 detail 侧，已改：

| # | 缺口 | 改法 |
|---|---|---|
| 1-3 | `hips_tile_format` 只写「取标准 token 集」，**漏掉合同要求的两档词表**（目录子产品 `snr/` 必须写 `tsv`） | 补两档表 + 判据（档位不符或非登记 token 判红、读端 `UNSUPPORTED`）。schema `by_subproduct = {signal:fits, support:fits, variance:fits, ivar:fits, snr:tsv}` |
| 4 | 漏掉读侧分档：归档缺索引 fail-closed、**裸形态允许无索引**（按目录枚举重建 + provenance 记降级） | 补（N6/N7） |
| 5 | 漏掉 TRIM 改 FITS 结构 ⇒ **启用 TRIM 的产品与未 TRIM 的同源产品身份不同** | 补进 `properties` 与 manifest `storage` 段的显式区分 |
| 6 | 引用失实：补边位模式称「按最高设计冻结为 IEEE NaN」，但 `grep -c "IEEE" docs/ACSD_DESIGN.md` → **0** | 改归因到合同的 T2 判据① |
| 7 | 子代理 C 上一轮**自己引入**的失实指针（把 `DATA_SEMANTICS.md §3.2` 展开成全仓不存在的标题） | 自查中改为真实标题「三个基本对象的语义」，**登记不抹除** |
| 8 | 载体切换阈值缺 `10^7` 条 | 补 |
| 9 | `tree_hash` 执行面歧义（仓内还有 `canonical_hash.cpp` 的带域前缀 per-file 哈希，作用域不同） | 补消歧 |

**由 D3 牵出的三条禁区问题（登记）**：

| # | 问题 | 证据 |
|---|---|---|
| X-11a | 合同 `HIPS_STORAGE_FORM.md` **三处反向指针**写「机制与推导见 `docs/detail/infrastructure/` 的产品存储形态设计「形态核对与负例」一节」——该目录下**没有**这个文档（只有 17–23 七张基建卡），而「形态核对与负例」是**合同自己**的一节 | 跨层反向定义 + 断链，真实落点是 `docs/detail/PRODUCT_STORAGE_FORM.md` |
| X-11b | `hips_storage_form.schema.json` 要求 `docs/ACSD_DESIGN.md` 含 `storage_form`/`archive`/`bare`/`.hips.index.json`/`coverage.index.json` 五词，**实测命中 0/5** ⇒ 该 schema 的自带检查器当前应判红 | 顶点属本车道写面但需负责人批准 ⇒ 登记（N-10） |
| X-11c | TRIM 依据归因分歧：detail 归 IVOA `WD-HiPS-2.0-20260501 §4.3.2`（工作草案），合同 `:161` 归参考文献 `[2]` = FITS 标准 4.0，且 `grep "WD-HiPS\|4\.3\.2"` 在合同里零命中 | 两份标准正文仓内都没有 ⇒ 需一手原文裁决（N-11） |

### X-10【通过项，登记以免下游重复劳动】

| 项 | 结论 |
|---|---|
| `GLOSSARY.md` 23 条 term 的文件锚 | **23/23 全部命中**（真解析器逐条核 `文件存在` + `§N 在目标标题中存在`） |
| `GLOSSARY.md` 的 `product_bit_flags` 数值 | **正确**：`aio_hips.h:39-40` ⇒ `AIO_HIPS_PRODUCT_VARIANCE = 8`、`AIO_HIPS_PRODUCT_IVAR = 16` 逐位一致 |
| `GLOSSARY.md` 的 `bad_mask` 极性 | **正确**：`cosmetic_corrector.cpp` 中 `bad_mask[i]≠0` 即不参与插值取样（`if (!bad_mask[i])` 保留 / `while(... bad_mask[...])` 跳过）。**错的只是锚里的行号 `#158`**（真实声明在 `:161`） |
| 本车道退出码口径 | **与代码逐码一致**：`0/2/3/4/5/6/7/8/9/10/70`（`lib/infrastructure/cli/exit_codes.h`） |
| `1.482602218505602` | 本车道 3 处全部为**正确的 16 位值**（非 `…023`） |
| 索引登记完整性 | 146 条登记 **146 条存在、0 悬空、0 重复**；`docs/detail/**` 43/43 全覆盖 |

---

## 4. 需代码侧订正的问题（本单不改代码，逐条登记）

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| C-01 | `acsd.phase1.star-psf` 的 `detect_sources` / `p1_sources` 与 detail 卡「不做星点检测」相反（X-02） | `module_ports.registry.json` 的 `operations` | detail 侧与生产面职责划分不一致，须与 X-01 一并裁决 |
| C-02 | `sparse_snr_layer` 在 Phase1 **无产者、无 HiPS 载体**（S-03） | `grep -rn 'ASTROCS_SPARSE_SNR_LAYER' lib/` → 0；`实验/absolute-snr/REPORT_paper.md:179` | `snr_path=sparse_reconstruct`（**当前默认**）在生产上不可兑现；`dense` 亦 fail-closed（`实验/dense-snr-reconstruct/REPORT_paper.md:184`）⇒ **三种重建方式当前只有 `frame_reconstruct` 可兑现** |
| C-03 | `gate_enforcement()` 恒 `RecordOnly`，与 detail 判据表 ①③ 的 `enforce` 相反（X-03） | `lib/infrastructure/cli/resource_gate.h` | 判据表与代码不可能同时为真 |
| C-04 | 精度面是单一全局位，无法表达「dense=f32 且 sparse=f64」（E-06） | `lib/infrastructure/aio/src/aio_api.cpp` | 最高设计 §3.3 的两类精度归属不可实现 |
| C-05 | `acsd.phase2.resample` / `acsd.phase3.resample` 在代码侧注册但不在 20 个 port registry module 内 | `eng/contracts/block_flow/conformance_deviations.json`（`owner_decision_required: true`） | detail 侧 X-01 的 5 张卡处置依赖此项裁决 |
| C-06 | `module_adapters.cpp` 引用的 `docs/science/CONTROL_WEIGHT_SNR.md`、`PSF_SIGNAL_WEIGHT.md`、`NOISE_MODEL.md`、`docs/engineering/SCIENTIFIC_REFERENCES.md`、`UNCERTAINTY_AND_COVARIANCE.md` **全部不存在** | noise_snr 车道实测；我复核 `for f in ...; do test -e` → 全 MISSING | 其中 `CONTROL_WEIGHT_SNR.md §8c` 被代码注释写成「定权路径唯一权威」⇒ 代码注释指向不存在文件 |

---

## 5. 需权威补充才能定的问题（如实写「需补充什么」）

| # | 问题 | 需补充什么 |
|---|---|---|
| N-01 | `F_ref` 的最终口径（X/E-05、S-02） | 需**负责人裁决**：① 组内公共 `F_ref`（代码现状，fail-closed）还是 ② 逐帧 `F_ref`。二选一都要同步 `ACSD_DESIGN.md:152`、`DATA_SEMANTICS.md:181,185`、本车道三处。**我已给出可证推导支持 ①**，但顶层措辞「逐帧参考通量」在 `ACSD_DESIGN.md` 两处，改动需负责人批准 |
| N-02 | `F_ref,k = 10^(−0.4(m_ref − ZP_k))` 是否与「组内公共 `F_ref`」同一量 | 需**负责人裁决**：该式含逐帧零点 `ZP_k`，即便 `m_ref` 档冻结，`F_ref,k` 仍随帧变；代码的 `F0` 是块级单一数值。两者不是同一个量 |
| N-03 | 命名块是否为阶段内节点间载体（E-03） | 需**负责人裁决**改顶点还是改工程正本+注册表。DOC-ENG 已登记 U-1，我不重复裁 |
| N-04 | 精度归属（E-06） | 需**负责人裁决**：3.3 两类独立 vs 代码单一全局位 |
| N-05 | `m_361`/`1.1663872874444212` 的 −0.04% 差（S-04） | 需一手来源核对 `n·Var(MAD)/σ² = 1/(16φ(d)²) = 0.618983`。SCI-unified 车道已列候选：**Rousseeuw & Croux 1991, Comm. Statist. 21, 1935–1951** 或 **Hampel 1974**。**我核不到这两篇原文**，故 detail 侧只写闭式 `1/(4φ(d)d)`，不写 `√1.361` |
| N-06 | `A_drop,j = pixfrac²·A_pixel,j` 在 SIP 畸变下的偏差闭式 | 需「非保面积映射下正方形足迹面积比」的解析界。SCI-psf 车道已登记 U2，**我核不到闭式文献** |
| N-07 | `mask` 平面归属（X-05） | 需**负责人裁决**：补 canonical 对象 还是 从合同最小平面集删除 |
| N-08 | `ACSD_DESIGN.md:118-119` 的 chart/鞋带/无超越函数口径 vs `DRIZZLE.md` + 代码的球面立体角/atan2 口径 | 需**负责人裁定**以哪一侧为准（SCI-psf R8-1，四方独立同证）。**两者不可并存** |
| N-09 | `docs/engineering/build/` 三份 ACTIVE_NORMATIVE 文档被 `.gitignore:20 build/` 吞掉，永不入库 | 需前台改 `.gitignore`（`/build/` 锚根 + `!docs/engineering/build/`）后提交。**不在本车道写面**（子代理 E 复验：该目录已零跟踪文件） |
| N-10 | `hips_storage_form.schema.json` 自带检查器要求 `docs/ACSD_DESIGN.md` 含 `storage_form`/`archive`/`bare`/`.hips.index.json`/`coverage.index.json` 五词，**实测命中 0/5** ⇒ 该检查当前应判红 | 需**负责人决定**：补进最高设计第 10 章，还是改 schema 的检查面。顶点属本车道写面但需负责人批准 ⇒ 只登记 |
| N-11 | TRIM 依据归因分歧：detail 归 IVOA `WD-HiPS-2.0-20260501 §4.3.2`（工作草案），合同 `:161` 归 FITS 标准 4.0 | 需 **IVOA WD-HiPS-2.0 正文** 与 **FITS 标准 4.0 正文** 两份一手件仲裁归属。**我两份都未取到**，不凭印象裁 |
| N-12 | engineering 侧 21 处指向 `docs/detail/common/UNIFIED_MODEL`「数据对象（各自具名）」（`UNIFIED_OBJECTS.md` 5 + `ARTIFACTS.md` 16） | 需 engineering 车道把**路径与节名一并**改指 `docs/detail/UNIFIED_MODEL.md` 的「数据对象与字段歧义消解」。**这 21 处在改名前已因路径不存在而悬空**；**必须与本车道改动同批提交**，否则 docs 树出现新断链（红线 5） |
| N-13 | `docs/engineering/contracts/HIPS_STORAGE_FORM.md` 三处反向指针写「见 `docs/detail/infrastructure/` 的产品存储形态设计「形态核对与负例」一节」——该目录下无此文档，「形态核对与负例」是**合同自己**的一节 | 需 engineering 车道把三处改为指向 `docs/detail/PRODUCT_STORAGE_FORM.md` 的体积削减一章（跨层反向定义 + 断链） |
| N-14 | `snr.reference_mag` / `snr.reference_flux_adu` 写进了 detail 的「配置 schema」表，但 `eng/packaging/config/defaults.json` 与三份 `phase_config` schema **均零命中** | 需配置车道裁决：把两键补进冻结 schema，还是把 detail 表降级为「代码读取的缺省值（非冻结 schema 键）」。**我已在 detail 侧标注**（见 §1.7 E-07） |

---

## 6. 索引变更与真解析器验证输出

### 6.1 `docs/DOCUMENT_INDEX.yaml`

**本车道改动**：仅一处（SCI-psf R8-2 的降级处置，见 P-01）—— `docs/science/psf/PSF.md` 条目 `notes` 末句补 `.md` 扩展名。

**未改动的原因**：`docs/DOCUMENT_INDEX.yaml` 正被 engineering 车道并行修改（`standards/CODE.md` 拆分出 `COMMENT.md`、新增 `ERROR_MODEL.md`、CODE 的 `duty` 改写）。**为免撞车，本车道对该文件只做一处最小改，不碰其余。**

真解析器对账（PyYAML **6.0.2**，Python 3.13.5；脚本 `/tmp/verify_index.py`）：

| 项 | 数 |
|---|---|
| `doc_index.schema_rev` | `3` |
| `doc_index.active` 条目 | **146** |
| `path` 唯一值 | **146**（重复 **0**） |
| 磁盘 EXISTS | **146** |
| **磁盘 MISSING（悬空条目）** | **0** |
| 严格 loader 检出重复键 | **0**（任意层级） |
| `status` 取值越界 | **0**（全落在 4 个枚举值内） |
| `docs/detail/**` 覆盖 | 真实 47 个 `.md`，剔除 4 个目录招牌 README ⇒ 规范件 **43**，已登记 **43/43** |
| 真实 `docs/` 文件 | 167；已登记 139；未登记 **28**（其中 **27** 是目录招牌 README，按索引自订规则 2 豁免；**1** 是野文件，已处置，见下） |

野文件处置：`docs/noise_snr_audit_upstream_findings.md`（672 行，子代理误建的审计过程件）。

- 处置前：`git -c core.quotepath=false ls-files` → **已跟踪**（commit `128a1b00`），`status` 干净，`check-ignore` 未命中；`docs/` 一层实测 **5** 个文件（索引规则 5 要求 4 个）。
- 处置：**移出权威文档树**，内容原样保留到 `run/GOVERN-08/审核包-R2/野文件-noise_snr_audit_upstream_findings.md`。**未删除任何内容**（红线 2）。
- 处置后：`docs/` 一层 = `ACSD_DESIGN.md` / `DOCUMENT_INDEX.yaml` / `GLOSSARY.md` / `README.md` —— 与索引规则 5 一致；`grep -rn "noise_snr_audit_upstream_findings" docs/ eng/ lib/ 实验/` → **0 残留引用**。

### 6.2 路径 / `§N` 锚 / `#fragment` 锚真解析器验证

脚本 `/tmp/verify_anchors.py`（路径抽取 → 扩展名解析 → 目标 `.md` 标题集匹配 `§N` → JSON/YAML 内容匹配 `#fragment`）。**工具自身的已知误报**（如实登记，不掩盖）：路径正则不吃 `::` 符号锚、不剥 JSON 尾引号、不支持 JSON Pointer 的 `/`、会把函数名列表（`p2_session_create/validate/run/inspect/destroy`）与 glob（`acsd.phase3.*`）当路径。

改前/改后对照（全车道 47 份 + `GLOSSARY.md` + `README.md`）：

| 时点 | `verify_anchors.py` 合计 |
|---|---|
| 改前基线（`git show HEAD:` 取原文件，脚本在 `/tmp/before` 跑） | **18** |
| 改后（全部子代理回件 + 我修完 7 处） | **9** |

**改后残留 9 处逐条归属 —— 全部是工具误报或故意标本，无一是文档缺陷**：

| # | 位置 | 内容 | 为什么不改 |
|---|---|---|---|
| 1 | `ANCHOR_CONTRACT.md:110` | JSON 示例尾 `docs/.../STAR_DETECTION.md",` | 工具的 `rstrip` 不剥引号；目标实存 |
| 2 | `ANCHOR_CONTRACT.md:117` | `docs/science/psf/PSF.md:7` | **故意演示判红的负例标本**（「登记面写 `文件:行` ⇒ D5/D1 判红」）。删它等于删判据 |
| 3-5 | `21_observability.md:107/113/138` | `file::symbol` 符号锚 | `::` 是符号锚，正是 AGENTS 要求的「不写行号、改符号名」形态；三个符号 `grep` 逐字命中 |
| 6 | `PHASE3_DETAILED_DESIGN.md:131` | `…schema.json#/$defs/export_crop` | JSON Pointer 真实存在：`python3 -c "import json;print(list(json.load(open('eng/contracts/schemas/phase_config_export.schema.json'))['$defs']))"` 命中 `export_crop` |
| 7-8 | `acsd.phase2.integrate.md:26,118` | 散文 `p2_session_create/validate/run/inspect/destroy` | 是函数名列表，不是路径；不为清零破坏正确表述 |
| 9 | `UNIFIED_MODEL.md:121` | `https://pixinsight.com/doc/docs/ImageWeighting/…` | URL 被仓内路径正则切断；该 URL 我实测 **HTTP 200**（需 UA）、题名与「PSF Signal Weight / PSF SNR」两节逐字命中 |

**改后悬空引用复核**：`docs/detail/UNIFIED_MODEL.md` 的 `[1][2]` 由文末 2 条参考文献闭合（我已用 Crossref 核对 `[1]` Horne 1986、`curl` 核对 `[2]` PixInsight 页）；`STAR_DETECTION_IMPL_DESIGN.md` 的 `[0]` 与两张卡的 `[65]` 是**数组下标**（`last_error[256]`/`model_hash[65]`/`sha256[65]`），不是引用。**悬空引用 = 0**。

**改前/改后 `§` 机械锚计数**（**以子代理 E 的独立复跑为准，我采纳并订正子代理 C 的自证数**）：

| 指标 | 改前 | 改后 |
|---|---|---|
| `docs/detail/**`（根级+`anchors/`+`infrastructure/`）`§` 出现总数 | **709** | **46** |
| 残留 35 行逐行判定 | — | **全部是外部文献/标准节号**（B&A96、Stetson、Triggs & Sdika、Kron 手册、IVOA HiPS 1.0、WD-HiPS），不可改名 |
| 「`X`一节」形态（携带标题，保留） | 50 | **127** |
| 跨文档 `ALG §` 裸编号 | 16 | **0**（全部点名章节标题） |
| registry 面 `§N` 引用 | 约 280（绝大多数已带真实标题） | 未全量处理（子代理 B 只改四类裸跳锚）；**全量标题化交前台决定** |

**改前/改后真破表**（扫描器不计转义竖线）：

| 时点 | 真破表行 |
|---|---|
| 改前（`git show HEAD:`） | **2**（`PHASE1_DETAILED_DESIGN.md:144,145`，各少一列） |
| 改后 | **0** |

（另有 5 处「看似破表」经逐行判读是**已正确转义**的 `\|`，非缺陷。）

**改前/改后索引与悬空引用**（PyYAML 6.0.2，子代理 E 与子代理 A 两次独立复核，数字一致）：

| 项 | 改前 | 改后 |
|---|---|---|
| 索引登记 / 重复 / **悬空** | 146 / 0 / **3**（`docs/engineering/build/*.md` 被 `.gitignore:20 build/` 吞） | 148 / 0 / **0**（engineering 车道已入库那 4 份） |
| 未登记真文件（非目录招牌 README） | 0 | **0**（27 条未登记全是目录招牌 README，按索引自订规则 2 豁免） |
| 本车道 PATH-MISS | **14** | **1**（非本车道） |
| 本车道 SEC-MISS（`§N` 锚） | **4** / 279 处锚 | **0** / 228 处锚 |
| `[n]` 悬空引用 | **1**（`UNIFIED_MODEL.md` 的 `[18]`） | **0** |

### 6.3 合同 → detail 卡的 `文件:行号` 指针核验脚本（X-04）

```python
import json,re
for f in ['eng/contracts/data/unified_object_registry.json',
          'eng/contracts/data/unified_object_compatibility_map_v1.json']:
    t=open(f,encoding='utf-8').read()
    for m in re.finditer(r'docs/detail/registry/(acsd\.[a-z0-9.\-]+)\.md:(\d+)（([^）]*)）', t):
        p='docs/detail/registry/'+m.group(1)+'.md'; ln=int(m.group(2))
        print(m.group(1), ln, m.group(3), '->', repr(open(p,encoding='utf-8').read().splitlines()[ln-1]))
```

---

## 7. 文献核对

### 7.1 核到原文 / 核到一手来源的

| 量 | 一手来源 | 核对方式 | 结论 |
|---|---|---|---|
| `1.482602218505602 = 1/Φ⁻¹(3/4)` | 标准正态分位数 | `python3 -c "from statistics import NormalDist as N;d=N().inv_cdf(0.75);print((1/d).hex(),(1.482602218505602).hex(),1/d==1.482602218505602)"` | 两者同为 `0x1.7b8bd1a975674p+0`，**逐位相等**。本车道 3 处全对 |
| `c_se = 1/(4φ(d)d)` | 标准正态密度与分位数 | 同上脚本换算 | `1.1663872874444212`，**16 位逐位**；平方 `1.3604593043` |
| `AIO_HIPS_PRODUCT_VARIANCE = 8`、`IVAR = 16` | 仓内一手头文件 | `grep -rn 'AIO_HIPS_PRODUCT' lib/infrastructure/aio/include/aio_hips.h` | 与 `GLOSSARY.md` **逐位一致** |
| `bad_mask` 极性 `1=坏` | 仓内一手源码 | `sed -n '161p;167p;192p;210p' lib/algorithms/calibration/src/cosmetic_corrector.cpp` | 极性**正确**；锚点行号 `#158` **错**（真实 `:161`） |
| `k = D_p/N_p = pixfrac²` | `docs/science/drizzle/DRIZZLE.md` + `docs/GLOSSARY.md` | 我自己重推（§1.5）+ `sed -n '536,549p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` | 生产 `var_buf = sumVarNum·k·k`、`k = sumArea/sumNorm = D_p/N_p`，**与 `GLOSSARY` 逐位一致** |
| 退出码 `0/2/3/4/5/6/7/8/9/10/70` | 仓内一手头文件 | `cat lib/infrastructure/cli/exit_codes.h` | 与本车道三张卡**逐码逐值一致** |
| 生产端口注册表 20 个 `module_id` + `star-psf` 的 `detect_sources` | 仓内一手 JSON | `python3 -c "import json;..."` | X-01/X-02 的全部依据 |
| `sparse_snr_layer` 无 Phase1 产者 | 仓内一手源码 + 实验台账 | `grep -rn 'ASTROCS_SPARSE_SNR_LAYER' lib/` → 0；`sed -n '179p' 实验/absolute-snr/REPORT_paper.md` | S-03 的全部依据 |

### 7.2 核对不到的（**分列，不凭印象裁**）

| 项 | 核对不到的原因 | 我做了什么 |
|---|---|---|
| `1.361` 与 `1.3604593043` 的 −0.04% 差 | **RC93 JASA 未给 MAD 的绝对渐近方差**；我未取到 Rousseeuw & Croux 1991 *Comm. Statist.* 21, 1935–1951 或 Hampel 1974 原文 | detail 侧只写闭式 `1/(4φ(d)d)`，**不写 `√1.361` 当精确式**（N-05） |
| `A_drop,j = pixfrac²·A_pixel,j` 在 SIP 畸变下的偏差闭式 | 需「非保面积映射下正方形足迹面积比」的解析界，未核到文献 | 登记（N-06）；detail 侧不写新数 |
| `F_ref` 的最终口径 | 属**裁决**不是文献问题 | 给推导与双方证据（N-01/N-02），不代裁 |
| `docs/engineering/build/` 三份文档的**内容**质量 | 不在本车道写面 | 只核了存在性与跟踪状态，**内容未评审** |
| 野文件内容是否符合正本体例 | 已移出权威树 | 只做位置处置，**内容原样保留未评审** |

---

## 8. 自证段

### 8.1 我实际做了什么

1. **通读**：`AGENTS.md`、`docs/ACSD_DESIGN.md`（568 行全读）。
2. **逐条读完四份审稿意见共 2259 行**（DOC-ENG 457 / noise_snr 342 / SCI-psf-drizzle-resample 990 / SCI-unified-calibration-detection 470），未抽样、未用脚本替代阅读。
3. **自己动手重推** 6 组量（§1.5），全部给推导，未采信审稿员结论作为唯一依据。
4. **自己跑的真解析器**：PyYAML 6.0.2 索引对账（146/146/0 悬空）；`/tmp/verify_anchors.py` 路径 + `§N` + `#fragment` 三类锚；生产端口注册表 module_id 差集；合同→detail 行号指针回读；`§N` 锚逐条标题匹配。
5. **派发 5 个子代理**分头订正与复核（A 索引只读对账 / B `registry/**` / C detail 根级+`anchors/`+`infrastructure/` / D `GLOSSARY.md`+`README.md` / E 独立对抗复核），对每条结论逐条复核。
6. **独立发现** 四条车道都漏掉的 10 组跨文档问题（X-01…X-10），其中 X-01/X-02 是 module_id 与职责口径、X-03 是判据表与代码正本相反、X-04 是合同行号指针大面积悬空、X-05 是 `mask` 平面无承载体。

### 8.2 我**没有**做的事（诚实边界）

- **没有修改** `docs/ACSD_DESIGN.md` 任何一行。该文件 `:23` 明写「修改本文档由项目负责人批准」⇒ 5 处需改（E-03/E-04/E-05/E-06/N-08）全部**登记待裁决**，并给出确切改法。
- **没有修改** `docs/science/**`、`docs/engineering/**` 任何一行。落在那两面的问题只登记。
- **没有删除任何真内容**：野文件是**移动**不是删除，672 行原样保留。
- **没有编译、没有运行任何仓内脚本或测试、没有取任何外部 PDF 全文**。所有「代码为准」的判定来自源码阅读 + grep/JSON 解析/schema 解析。
- **没有替负责人裁**任何 UNRESOLVED（N-01…N-09 全部保留为待裁决）。
- 对 §1.1 中 **75 处 `ACSD_DESIGN.md §N` 锚**，我核到它们**全部指向真实标题**，因此**判定为格式可优化而非缺陷，不做批量改写**。若前台要求按 AGENTS §5 严格执行，这是一次独立的批量改写任务，需另行派单。

### 8.3 收敛判定

本车道完成的是**第 1 轮订正**。按规范 03 的收敛判据（连续两轮无新增实质问题），本车道**未达收敛**：X-01…X-11 中有 4 条（X-01、X-02、X-03、N-01/N-02）依赖负责人裁决或代码侧订正，在裁决落地前无法关闭。**下一轮建议顺序**：负责人裁 N-01/N-02（`F_ref` 口径）→ N-03/N-04（块载体、精度）→ N-12（engineering 21 处同批）→ X-03（判据表 vs 代码，已改，待代码侧确认）→ X-01/X-02（module_id 归属）→ 重跑本车道十轮。

---

## 9. 交付风险（**必须随本单一并处理**）

| # | 风险 | 证据 | 必须怎么做 |
|---|---|---|---|
| **R-1** | **野文件移动的提交风险**：新路径 `run/GOVERN-08/审核包-R2/野文件-noise_snr_audit_upstream_findings.md` 是 **untracked**，旧路径 `docs/noise_snr_audit_upstream_findings.md` 是 **unstaged delete** | `git -c core.quotepath=false status --short` 两条分列 | **新增与删除必须同批提交**。只提交删除会**丢 672 行内容**；只提交新增会留下权威树里的野文件 |
| **R-2** | **N-12 的同批提交**：engineering 侧 21 处引用在 detail 改名后节名不再匹配（且**改名前就已因路径错误悬空**） | `grep -ro "数据对象（各自具名）" docs/engineering/ \| wc -l` → 21；`grep -ro "数据对象与字段歧义消解" docs/engineering/ \| wc -l` → 0 | detail 与 engineering 的这 21 处**必须同批提交**，否则 docs 树出现新断链 |
| **R-3** | **并行车道仍在写**：`docs/science/**`、`docs/engineering/**`、`eng/**` 在本单交付期间被其他车道修改 | 工作树中这些路径的改动均非本车道产生 | 本单的「已改」只覆盖本车道写面；跨面结论以**本单交付时刻**的读数为准 |
| **R-4** | `docs/DOCUMENT_INDEX.yaml` 本车道只改了**一处**（PSF 条目 `notes` 补 `.md`），其余由 engineering 车道并写 | 索引当前 148 条、悬空 0 | 本车道对该文件不做更多改动，避免撞车 |

---

## 10. 子代理分头与逐条复核（**写明否决了哪些**）

| 子代理 | 车道 | 写作用域 | 我的复核结论 |
|---|---|---|---|
| **A** | 索引真解析器对账（**只读**） | 无 | PyYAML 6.0.2，146 条登记/146 存在/0 悬空/0 重复；额外查出**野文件已被提交**（非未跟踪）与 **`.gitignore` 吞掉 build 目录**两条高危。**全部采纳** |
| **B** | `docs/detail/registry/**` | registry 26 份 | 改 15 张卡；**部分否决我三条指令**：N-1（`k=pixfrac²` 写成条件式，因 `DRIZZLE_GEOMETRY.md` 给球面残差律）、N-2（「把逐帧全删」会与合同+代码同时相反）、N-3（`file::symbol()` 判「坏路径」**事实不成立**）。**三条部分否决我全部接受** |
| **C** | detail 根级 + `anchors/` + `infrastructure/` | 23 份 | 改 18 份；**三条推翻我的派单前提**：D 组 1 前提不成立（detail 侧没复制那两个错误，错在一级正本）、B 组 2 部分否决（代码禁的不是逐帧 `F_ref` 本身）、B 组 3 节名。**全部接受**；并自曝自己上一轮引入的失实指针（C14/D3-7） |
| **D** | `docs/GLOSSARY.md` + `docs/README.md` | 2 份 | 改 14 项；**否决 SCI-psf R8-10 的量化部分**（我独立复算确认，见 J-09）；**我否决 D 的 `projection` 行**（编造，见 §1.7 E-06，已改回） |
| **E** | **独立对抗复核**（**只读**） | 无 | **否决 13 条、点名 2 处编造**；**我全部接受并当场改了 7 处文件**（见 §1.7） |

**我自己动手改、子代理没改的**：`21_observability.md` 判据表（X-03/E-05）、`PHASE1_DETAILED_DESIGN.md:144,145` 破表、`23_hips_browser.md` 花括号路径、`GLOSSARY.md` `projection` 行、`noise-snr.md` 两处「默认产出」、`photometry.md` 的 `√1.361`、`phase2.reject.md` 的 `V15–V17`、`UNIFIED_MODEL.md` 的 `F_ref` 两档约定与已被 science 订正的过时漂移表述。