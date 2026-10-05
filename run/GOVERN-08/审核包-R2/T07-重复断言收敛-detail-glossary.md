# T07 · 重复断言收敛 —— 车道 DOC-DETAIL-GLOSSARY

**车道**：`docs/detail/**`、`docs/GLOSSARY.md`、`docs/README.md`、`docs` 顶层（`docs/ACSD_DESIGN.md`）。
**职责**：把重复断言收敛为唯一正本，其余改指或删除；跨全部正本（顶点 / science / engineering / detail）逐对象、逐创新点做一致性核查。
**写面**：19 份文件，+99 / −84 行（`git diff --numstat`，逐行列于 §7）。
**git**：全程零写操作（只读 `status` / `diff`），中文路径一律 `git -c core.quotepath=false`。
**并发**：本车道工作期间，science 与 engineering 车道在同一工作树并行落改动（`T07-重复断言收敛-science.md` / `-engineering.md` 已生成）。本单**只写本车道 19 份文件**，未触碰 `docs/DOCUMENT_INDEX.yaml` 与 `docs/science/**`、`docs/engineering/**`；索引校验是在对方改动后的工作树上跑的。

---

## 1 收敛总账

统计口径：**重复断言 22 条**（= 22 组同主题断言在全仓有 ≥2 处出现）。处置分布：
**唯一正本 6 条 / 删副本（重复公式被删、改指正本）3 处 / 改指 39 处 / 同名不同义保留 5 组**。
其中 **7 处属于「同一断言在 3 层以上各有副本、本轮只收敛了其中 1–2 处」的典型重复**，是前两轮「改一处、在别处造新矛盾」的止血目标。

| # | 重复断言（主题） | 全部出现位置 `文件:行`（本车道写面加粗） | 处置 |
|---|---|---|---|
| **D-01** | **进程退出码的码值语义表在哪一份文档** | 唯一正本 `docs/engineering/standards/ERROR_MODEL.md:88 ## 7 进程退出码`（`:90` 自称「全仓唯一一份码值语义表」）；指错的副本 4 处：**`docs/detail/infrastructure/19_runtime.md:13`**、**`:116`**、**`docs/detail/LOG_AND_ERROR_SYSTEM.md:133`**、**`docs/detail/infrastructure/21_observability.md:52`**；另有车道外 2 处 `docs/engineering/contracts/CLI_PROTOCOL.md:36`、`docs/engineering/api/PUBLIC_API.md:1215`（登记，不改） | 4 处**改指** ERROR_MODEL.md，并把同句里真正的域→码表分名回 LOG_AND_ERROR.md「错误对象与退出码映射」 |
| **D-02** | **点源信息量的对象名与写法** | 唯一正本对象名 `point_information`（`docs/engineering/UNIFIED_OBJECTS.md:42`、schema `eng/contracts/schemas/unified/point_information.schema.json` 的 `required` 含 `W_info`）；野生别名 2 处：**`docs/detail/PHASE1_DETAILED_DESIGN.md:186`**、**`:210`** | 2 处**改指** canonical 名；`:186` 顺带**删副本**（重复的 `W_psf = PᵀC⁻¹P`，丢 `a_k²`），改为指回本文件 `:173` 的带因子式 |
| **D-03** | **稀疏层「默认产出」** | 正本（生产侧状态）**`docs/detail/registry/acsd.phase1.noise-snr.md:62`**、`:384`；重复副本 2 处：**`docs/detail/UNIFIED_MODEL.md:45`**、**`docs/detail/registry/acsd.phase1.drizzle.md:173`** | 2 处副本**改指**正本，自身只保留「配置缺省请求产出」这一层次（见 §2-A 分层） |
| **D-04** | **`m_ref` 是冻结常数还是可覆盖** | 正本（可覆盖、不是冻结常数）`docs/science/unified/DATA_SEMANTICS.md:206`；硬化副本 8 处：**`docs/detail/UNIFIED_MODEL.md:45,:74`**、**`docs/detail/PHASE2_DETAILED_DESIGN.md:21`**、**`docs/detail/registry/acsd.phase1.noise-snr.md:62,100,101,128,284,379,424`**、**`acsd.phase1.drizzle.md:173`**、**`acsd.phase2.integrate.md:138`**、**`acsd.phase2.upm-fit.md:87`** | 全部**改写措辞**为「配置缺省的参考星等档，缺省 6.0、可被输入 JSON 覆盖、一次运行内取值不变」，并在 `UNIFIED_MODEL.md:74` 点名一级正本原句 |
| **D-05** | **`snr_path` 键属哪个命令的配置** | 正本 `docs/detail/UNIFIED_MODEL.md:46`（「**本键属 mosaic 配置**：normalize 配置不含 `snr_path`」）+ `eng/contracts/schemas/phase_config_mosaic.schema.json`；错面副本 **`docs/detail/registry/acsd.phase1.noise-snr.md:387`**（phase1 模块卡的配置表里登记 mosaic 键） | 1 处**改指** mosaic 卡配置表行（`acsd.phase2.integrate.md:165`） |
| **D-06** | **ACSD_DESIGN「文档权威与索引 / 文档写法」的引用形态** | 唯一正本 `docs/ACSD_DESIGN.md:5 ## 0 文档权威与索引`、`:30 ### 0.1 文档写法`；副本 **`docs/README.md:3`** 用 `§0`/`§0.1` 机械锚 | 1 处**改指**真实节名 |
| **D-07** | **GLOSSARY 的权威范围自我声明** | 声明 3 处（同一断言的三个副本）：**`docs/GLOSSARY.md:5`**、**`docs/README.md:10`**、`docs/science/unified/DATA_SEMANTICS.md:282` 与 `UNIFIED_SCIENCE_MODEL.md:206`（车道外）；反例来自顶点自己 `docs/ACSD_DESIGN.md:562` 点名 22 术语、词典只覆盖 7 个 | 本车道 2 处**改写**为可兑现的窄声明（见 §3-9、§3-10）；车道外 2 处登记 |
| **D-08** | **GLOSSARY 20 条词条的权威锚点写法** | 20 条全部写成 `路径 §N` 机械锚（`docs/GLOSSARY.md:11–31`），违反 AGENTS.md §5 | 20 条**逐条改指真实节名**（逐条映射见 §7 附表），改后 `docs/GLOSSARY.md` 内 `§` 计数 = **0** |
| **D-09** | **ACSD_DESIGN 附录 A 的 canonical 对象清单** | §3.1（`:149`）列 13 个对象；附录 A（`:562`）列 12 个，**漏 `depth_m5`** | 附录 A**补齐**并把「定义见 detail 与 GLOSSARY」改成带优先级的表述 |
| **D-10** | **`P1/P2/P3` 记号** | 创新点 P1–P5：`:84–87`、`:94`、`:104`、`:115`、`:124`、`:132`；阶段 P1/P2/P3：**:370–376**（§8.1 mermaid）、`VALIDATION_EVIDENCE.md:315–317`（车道外） | §8.1 的 mermaid 节点 ID **改指** `NRM/MZC/EXP`（纯 ID，不改任何语义文字），并在 `:141` 加一句记号消歧 |
| **D-11** | **检测阈值冻结定义的落点** | 唯一正本 `docs/detail/STAR_DETECTION_IMPL_DESIGN.md:145 ### 5.3 O3 检测阈值`；错文件副本 **`docs/detail/PHASE1_DETAILED_DESIGN.md:112`**（指向 `docs/science/detection/STAR_DETECTION.md`，该文件无此节） | 1 处**改指**正确文件（节名本身真实，见 §6 推翻项 O-1） |
| **D-12** | **面亮度单位推导的落点** | 唯一正本 `docs/science/unified/DATA_SEMANTICS.md:95 ### 3.4 面亮度单位的推导`；伪造节名副本 **`docs/detail/PHASE2_DETAILED_DESIGN.md:11`**（「量纲与逐像素语义」全仓无此标题） | 1 处**改指**真实节名 |
| **D-13** | **HEALPix 索引与 tile 布局的落点** | 唯一正本 `docs/science/unified/DATA_SEMANTICS.md:28 ### 3.1 坐标语义`；伪造节名副本 **`docs/detail/common.md:32`**（「HEALPix 索引与 tile 布局」全仓无此标题） | 1 处**改指**真实节名 |
| **D-14** | **export 输入语义守卫的落点** | 唯一正本 `docs/ACSD_DESIGN.md:316 ### 6.3 投影算法`；伪造章名副本 **`docs/detail/PHASE3_DETAILED_DESIGN.md:15`**（「FITS 产品」章全仓不存在） | 1 处**改指**真实节名（原句两处职责由同一条承载，正本自证） |
| **D-15** | **G-RES-01 判据章的落点** | 唯一正本 `docs/detail/infrastructure/21_observability.md:56 ## 8. 重计算负载资源门（G-RES-01）`；副本 3 处：`21_observability.md:15`（已对）、**`19_runtime.md:69`**（「资源门」缩写）、**`19_runtime.md:95`**（「资源门判定与 exit 10」一段，全仓无此节） | 2 处**改指**真实节名（`:69` → 全标题，`:95` → `:105 ### 8.4 record / enforce 划分与判定点`） |
| **D-16** | **排障表入口的节名** | 唯一正本 `docs/detail/merged_TROUBLESHOOTING.md:38 ## 4 症状主表`；副本 **`docs/detail/merged_TROUBLESHOOTING.md:29`**（「症状索引」，同文件 `:9` 另有一处「通用定位顺序」已对） | 1 处**改指**真实节名 |
| **D-17** | **创新点小标题（`§2.1` / `§2.2` 的自称）** | 顶点 `:94` §2.1 = 「P1 通量积分拟合：测光校准到测光星等坐标系」、`:104` §2.2 = 「P2 跨帧绝对信噪比」；副本 **`acsd.phase1.star-detection.md:4`**（自造「创新点一：星点位置由星表逆映射获得」）、**`acsd.phase1.drizzle.md:4`**（自造「创新点二：跨帧可用的绝对信噪比」） | 2 处**改指**顶点正名 |
| **D-18** | **`registry/` 登记面的体例描述** | 事实源 `lib/infrastructure/pipeline/module_ports.registry.json`（20 个 `module_id`）vs `docs/detail/registry/acsd.*.md`（25 张卡）；自述 **`docs/detail/00_INDEX.md:33`**（只说「每个生产 DAG 节点模块一页」） | 1 处**改写**为可兑现表述，点名 5 张不在端口图内的卡并说明理由 |
| **D-19** | **`star-detection` 卡的模块归属声明** | 自述 **`docs/detail/registry/acsd.phase1.star-detection.md:13`**（「`acsd.phase1.star-detection` 为 registry descriptor 单源」）；实测该 `module_id` 不在端口注册表内，端口图上是 `acsd.phase1.star-psf` | 1 处**改写**为分层表述（卡片名 / 文档路径 / 端口 `module_id` 三者分开） |
| **D-20** | **面亮度星等的 `Ω_ref` 项符号** | 唯一正本 `docs/science/unified/DATA_SEMANTICS.md:111,114`（**减号**，带可复算自检 `:117`）；错符号副本 **`docs/detail/PHASE1_DETAILED_DESIGN.md:161`**（**加号**，却引用同一落点） | 1 处**改指**正本并补符号由来（自推，见 §4-b） |
| **D-21** | **球面残差律 `δ(θ)` 的摘要阈值** | 唯一正本 = 同段数值表 `docs/detail/registry/acsd.phase1.drizzle.md:119–120`（2″→8.5e-12 / 6.3″→8.4e-11 / 60″→7.6e-9 / 300″→1.9e-7）；与之矛盾的摘要句在同段 `:122–123` | 摘要句**删副本改指**表，并按自推补真实边界（见 §4-a）。根因同句在 science `DRIZZLE_GEOMETRY.md:420–421`，**登记给 science 车道** |
| **D-22** | **两档权重式 `w = SNR²/F0²` 与 `w = SNR²/F_ref²` 是否同一式** | 同一文件内并存：`docs/detail/UNIFIED_MODEL.md:81`（F0 档）、`:87`（配对档），节内**无对账** | **不合并**（见 §2-B：两者相差 `a_k²`，是不同档不是错字），在唯一正本 §2.1 **新增一条对账**，把隐含矛盾转成显式分名 |

### 确认信息未丢失的依据

| 处置 | 信息未丢的依据 |
|---|---|
| D-02 `:186` 删重复式 `W_psf = PᵀC⁻¹P` | 该式是本文件 `:173` `W_psf,k(x,y) = a_k(x,y)^2 P_kᵀ C_k⁻¹ P_k = 1/Var(F_hat_k)` 的**缺因子退化版**；`:173` 仍在、含因子与 `= 1/Var(F_hat_k)` 的等价式，`:179` 另给白噪声闭式 `a_k²/(σ_pix,k² A_NEA,k)`。删的是退化副本，不是唯一承载 |
| D-03 两处副本改指 | 「配置缺省请求产出」这一层留在副本处（`UNIFIED_MODEL.md:45`、`drizzle.md:173` 的配置缺省列均仍是 `true`）；被删的只是**生产侧产者状态**那句重复陈述，该陈述的唯一正本 `noise-snr.md:62` 与 `:384` 一字未动 |
| D-07 自我声明改写 | 原声明的可兑现部分（「每个核心术语恰一个含义」「legacy alias 列出迁移去向」「冲突时以锚点正本为准」）**逐句保留**，只把不可兑现的「唯一术语权威」四字换成「术语叫法与单位口径的登记面 + 对象定义以 canonical schema 与 UNIFIED_OBJECTS.md 为准」，并补一句「本词典不收录全部核心术语」。**词典 21 条词条一条未删** |
| D-11 改指正确文件 | 节名「O3 检测阈值」逐字保留，只换承载文件（science 侧 `STAR_DETECTION.md` 无 O3 算子体系，O1–O16 算子编号是 detail 层 `STAR_DETECTION_IMPL_DESIGN.md` 的） |
| D-14 删「FITS 产品」章 | 被删的「输入语义守卫」与「硬约束」两条职责**同在** `ACSD_DESIGN.md:320`（§6.3 第三条），改为单章引用后两条都仍可达 |
| D-21 删摘要句、改为读表 | 原摘要句的两个错误边界（`θ≲10″→1e-10`、`θ≳100″→1e-7–1e-6`）**都不是信息**，是与同段表矛盾的错误读数；表的四档读数原样保留，新句的三个边界由表内同式自推（§4-a） |
| D-16 七项准入格式 | `merged_TROUBLESHOOTING.md:12` 的**七项清单本身逐项保留**（症状/阶段/状态码·证据/最小复现/期望不变量/源码位置/文档位置与测试位置），只把「表即七项」的错误蕴含改成「表按检索需要压成五列、余四项就近写在表外」——因为表实有 5 列、21→**20** 行，凭空补 4 列要编数据，违反「不得为了看起来一致而编数值」 |

---

## 2 同名不同义的保留项（逐条给依据，供复核）

**这些我读了内容、确认是不同的事，一律保留并明确区分，没有合并。**

### A. `sparse_reconstruct`（默认）—— 配置默认值 vs 生产侧产者状态，**两层不是一条断言**

| 层 | 断言 | 位置 | 处置 |
|---|---|---|---|
| **配置默认值** | mosaic 配置缺省 `snr_path = sparse_reconstruct` | `ACSD_DESIGN.md:279`、`UNIFIED_MODEL.md:46`、`PHASE2_DETAILED_DESIGN.md:21,:114`、`acsd.phase2.integrate.md:115,116,129,165`、`eng/contracts/schemas/phase_config_mosaic.schema.json:170`、`eng/packaging/config/defaults.json#snr.path` | **保留**，一处未动。缺省值是合同面事实，与「稀疏层有没有产者」无关；输入无稀疏层时按已登记的降级语义执行并记 `snr_path_effective` |
| **生产侧产者状态** | 稀疏层的侧车写者与载体发布者尚未落地 | `acsd.phase1.noise-snr.md:62,:384`（唯一正本） | **保留为唯一正本**，两处副本改指它 |

依据：`acsd.phase1.noise-snr.md:144` 明文「`sparse_reconstruct`（含默认）⇒ 按帧级执行但**必须显式记录实际路径**」——即配置默认值在无产者时有**已登记的合法降级**，两条断言可以同时为真。合并会把这个降级语义删掉。**注意 `acsd.phase2.integrate.md:116,165` 与 `acsd.phase1.noise-snr.md:138,387` 说的都是第一层，不在本轮改指范围。**

### B. `w = SNR²/F0²` 与 `w = SNR²/F_ref,k²`—— 两个 scope 档，不是错字

- `F0 = 10^(−0.4·(m_ref − ZP_syn))` 是**与帧无关的物理公共锚**，`F_ref,k = F0/k_photo,k` 是逐帧参考通量；`a_k := F_ref,k/F0 = 1/k_photo,k`。
- 由 `SNR = F_ref,k/σ_F^{frame,k}` 与 `σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}` 可得 `SNR²/F0² = 1/σ_F^{sys,k}²`、`SNR²/F_ref,k² = 1/σ_F^{frame,k}²`，两式相差 `a_k²`。
- 合同 `frame_snr.schema.json` 的 `reference_baseline.scope` 取值域恰是 `frame_independent_fixed_magnitude`（逐帧档，配 F0）与 `group`（块级公共档）两项，两档都是合法写侧约定。
- **处置**：不合并、不删任一式；在 §2.1 补一条对账把「差一个 `a_k²`」写明，并注明「`scope` 声明决定走哪一档，读侧不得跨档互相代入」（`UNIFIED_MODEL.md:89–99`）。
- **⚠ 未收敛、须负责人裁**：一级正本 `docs/science/noise_snr/NOISE_SNR.md:338` 逐字写「归一化因子**必须**取公共锚 `F0`，不能取该帧的 `F_ref,k`」，而顶点 `ACSD_DESIGN.md:111,:280` 与另 10 处 detail/engineering 文档写 `F_ref²`。这是**科学口径之争**，不在文档车道可闭，且 `UNRESOLVED.md` 27 条裁项**未登记**此项。已按 §5 登记。

### C. `错误对象与退出码映射` vs `进程退出码` —— 同属错误面、不同载体，**必须分名**

- `docs/engineering/contracts/LOG_AND_ERROR.md:89 ## 5 错误对象与退出码映射` = 错误对象字段表 + 域→码映射（IO→7 / INTEGRITY→8）。
- `docs/engineering/standards/ERROR_MODEL.md:88 ## 7 进程退出码` = 码值语义表（11 码逐码语义），`:90` 自称「全仓唯一一份码值语义表」。
- `LOG_AND_ERROR.md:105` 自己已经写「`../standards/ERROR_MODEL.md` 的进程退出码一节」——即「进程退出码」这个节名是真实的，只是不在 `LOG_AND_ERROR.md`。
- **处置**：4 处错指文件的一律改指 ERROR_MODEL.md，**同句里真正的域→码表指回 LOG_AND_ERROR.md**，两者并列写清，不合并成一个指针。

### D. `valid` / `invalid` / `validity` / `mask` —— 四个不同概念，未合并

`GLOSSARY.md:19` 的 `invalid`（逐像素布尔判定，NaN 或 `support<=0`）与 `UNIFIED_MODEL.md:52` 的 `validity`（canonical 状态对象，坏点/缺失/越界）**是两个对象**；`star_mask`（天球坐标）、`bad_mask`（校准域，极性 1=坏）、排异接受掩膜（归 `rejection`）是三个不同掩膜面。
**处置**：**未合并**，只在 `UNIFIED_MODEL.md:58` 新增一条把三面分工写明（「裸 `mask` 不在这 13 个对象内」，并点名 `validity` 已是 canonical 对象）——这是**补一条缺失的分名声明**，不是新增定义。
**⚠ 未收敛**：science 层 `PHASE2_COVERAGE.md:25` 的 `validity` 指「排异接受掩码」、与 canonical 的「传感器状态」是第三种含义，且 dtype 有布尔 vs integer 之争 —— 跨车道，登记。

### E. `support` 的「有效输入/面积贡献度」与 `PHASE2_DETAILED_DESIGN.md:56` 的「几何覆盖帧数 N」—— 同词不同量，未合并

后者是排异路由参数 `N`（该输出像素几何可贡献的帧数），前者是 canonical 对象 `[0,1]`。两者在原文里已用不同字形（`N` vs `support`）分名，未动。

### F. `mask` 与 `rejection` 的四概念/三概念分组冲突 —— 未合并，登记

`NUMERIC.md:185` 的「四概念分离」列 `support/coverage/validity/mask`，而 `UNIFIED_OBJECTS.md:47` 的 13 对象里第 12 位是 `rejection` 不是 `mask`。合并任何一侧都会吞掉另一侧并废掉既有负例。**保留现状，登记给 engineering 车道。**

---

## 3 改动前后逐字（关键处）

> 完整逐行 diff 见 `git diff -- docs/ACSD_DESIGN.md docs/GLOSSARY.md docs/README.md docs/detail/`；下表只列需要判读的 12 处。

**3-1 面亮度星等符号（D-20）** `docs/detail/PHASE1_DETAILED_DESIGN.md:161`

```
- 面亮度星等：`SB_mag = ZP_k − 2.5·log10(signal) + 2.5·log10(Ω_ref)`（`docs/science/unified/DATA_SEMANTICS.md` 的面亮度星等一段）；
+ 面亮度星等：`SB_mag = ZP_k − 2.5·log10(signal) − 2.5·log10(Ω_ref)`（`docs/science/unified/DATA_SEMANTICS.md` 的面亮度星等一段；由 `m = ZP_k − 2.5·log10(signal·Ω_ref)` 直接展开得**减号**，`Ω_ref = 1 sr` 时该项为零，写成加号仅在 `Ω_ref ≡ 1 sr` 时与正确式退化相同）；
```

**3-2 点源信息量（D-02）** `docs/detail/PHASE1_DETAILED_DESIGN.md:186,:210`

```
- 以及 `W_psf = PᵀC⁻¹P` 作为点源充分统计量（`point_source_information`）；
+ 以及点源充分统计量 `point_information`，其标量度就是本节 8.1 的 `W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k`（本页不另立第二套写法）；
- - `point_source_information`（map/model + summary）；
+ - `point_information`（map/model + summary）；
```

**3-3 球面残差律阈值（D-21）** `docs/detail/registry/acsd.phase1.drizzle.md:122–123`

```
- 与 `δ` **同阶**（面积加权平均、符号相反）⇒ `θ ≲ 10″/px` 时该偏离 ≲ 1e-10，
- `θ ≳ 100″/px` 时进入 1e-7–1e-6、与门禁容差同阶。
+ 与 `δ` **同阶**（面积加权平均、符号相反）。由上式在 `r_c = ξ_c = 0` 下取
+ `δ = 0.09·θ²`（θ 以弧度计）直接读出边界：`θ ≲ 7″/px` 时偏离 ≲ 1e-10、
+ `θ ≳ 217″/px` 起进入 1e-7、`θ ≳ 688″/px` 起进入 1e-6（后两档才与门禁容差同阶）。
```

**3-4 `k = pixfrac²` 括号矛盾（D 附带）** `acsd.phase1.drizzle.md:114–115`

```
- `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²`，即 `k = pixfrac²`（与 `pixfrac`
- 取值无关，`pixfrac = 1` 时 `k ≡ 1`）。
+ `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²`，即 `k = pixfrac²`（与各 `a_jp` 交叠面积的
+ 具体取值无关；`pixfrac = 1` 时 `k ≡ 1`）。
```

**3-5 `①②③` 与判据表冲突** `docs/detail/infrastructure/21_observability.md:109`

```
- **exit 10（RESOURCE）在资源门判定域内的充分条件**：判定域内 ①②③ 任一违约且处于 enforce 面
+ **exit 10（RESOURCE）在资源门判定域内的充分条件**：判定域内 ② 任一违约且处于 enforce 面
```

**3-6 同段方向词** `21_observability.md:97`

```
- `exit 10` 只属磁盘写满/写盘失败（见下文「错误与边界」）。
+ `exit 10` 只属磁盘写满/写盘失败（见本章「错误与边界」一节）。
```

**3-7 `m_ref` 冻结（D-04）** `docs/detail/UNIFIED_MODEL.md:74`

```
- `m_ref` 是**冻结的参考电平约定**（不是需标定的量）：默认 `6.0`，星等制为
+ `m_ref` 是**配置缺省的参考电平约定**（不是需由数据标定的量）：缺省 `6.0`、可被输入 JSON 覆盖、一次运行内取值不变；合同只约束它是 number，故它不是冻结常数。星等制为
```

**3-8 两档权重对账（D-22，新增）** `docs/detail/UNIFIED_MODEL.md:89–99`

```
+ - **两档权重式的差一个 `a_k²`（本页两处权重式不是同一式，必须分名读）**：
+   由 `ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)` 逐项展开得
+   `F_ref,k = 10^(−0.4·(m_ref − ZP_k)) = 10^(−0.4·(m_ref − ZP_syn,k))·(k_photo,k)^−1 = F0/k_photo,k`，
+   即 `a_k := F_ref,k/F0 = 1/k_photo,k` 是该帧的标度因子。于是
+   ```text
+   w = SNR²/F0²    = a_k² · (SNR²/F_ref,k²) = a_k²/σ_F^{frame,k}² = 1/σ_F^{sys,k}²   （逐帧档，公共锚口径）
+   w = SNR²/F_ref,k²                       = 1/σ_F^{frame,k}²                     （配对口径）
+   ```
+   两式相差逐帧因子 `a_k² = k_photo,k^−2`；`k_photo,k ≡ 1` 时同值。`scope` 声明决定
+   走哪一档，读侧不得跨档互相代入。
```

**3-9 术语权威自我声明（D-07）** `docs/GLOSSARY.md:3,5–8`

```
- > 上游：ACSD_DESIGN.md 附录 A（术语）
+ > 上游：《ACSD 最高设计》的「核心科学方法：五个创新点」与「数据对象」两章；附录 A 点名的非对象术语定义见其各自 science 分册
- 本词典是**唯一术语权威**。每个核心术语恰一个含义；legacy alias 列出迁移去向。
- 任何文档/代码/接口与本文冲突时，以本文锚点所指的权威文件为准并回改词典——定义只有一套。
+ 本词典是**术语叫法与单位口径的登记面**，不是术语定义的正本：每个 canonical 数据对象的定义、单位、无效值与可否作权重，以「权威锚点」列所指的正本与 `eng/contracts/schemas/unified/` 下的 canonical schema 为准；本词典只登记叫法、单位/极性/域与 legacy alias 的迁移去向。
+ 任何文档/代码/接口与锚点正本冲突时，以锚点正本为准并回改词典——定义只有一套。
  同一符号在不同正本给出不同定义时，词典只登记锚点正本的那一个，并按权威链把另一处登记为待裁决，不在词典里另立第二含义。
+ 本词典不收录全部核心术语：13 个 canonical 数据对象的词条以 `docs/engineering/UNIFIED_OBJECTS.md` 的对照表为准，本词典收录其中在本链出现歧义风险的那些。
```

**3-10 `docs/README.md:10`**

```
- `GLOSSARY.md` 为术语唯一权威；
+ `GLOSSARY.md` 为术语叫法与单位口径的登记面（对象定义以 canonical schema 与 `engineering/UNIFIED_OBJECTS.md` 为准）；
```

**3-11 `P1/P2/P3` 记号（D-10）** `docs/ACSD_DESIGN.md:141,:369–379`

```
- 科学推导见 science 各分册（……），实验单元编排见第 12 章。
+ 科学推导见 science 各分册（……），实验单元编排见本章「创新点实验单元」一节。
+
+ 本节的 P1–P5 指五个创新点，与「phase1/phase2/phase3」的阶段指代是两套记号，全文不混用：阶段一律写阶段全名 normalize / mosaic / export。

-     CLI["唯一入口"] -->|拉起| P1["normalize 调度器"]   …   H2 -->|磁盘| P3
+     CLI["唯一入口"] -->|拉起| NRM["normalize 调度器"]  …   H2 -->|磁盘| EXP
```

**3-12 附录 A（D-09）** `docs/ACSD_DESIGN.md:562`

```
- signal、variance、ivar、source_snr、frame_snr、…… —— 定义见 detail 与 `docs/GLOSSARY.md`。
+ signal、variance、ivar、source_snr、depth_m5、frame_snr、…… —— 定义见 detail 与 `docs/GLOSSARY.md`（对象定义以 `docs/engineering/UNIFIED_OBJECTS.md` 的对照表与 canonical schema 为准，`docs/GLOSSARY.md` 只登记叫法与单位口径）。
```

**3-13 排障表计数** `docs/detail/merged_TROUBLESHOOTING.md:12,:24`

```
- 每条条目的准入格式是固定的七项，缺项即不算合格条目：（下列 1.–7. 编号清单）
+ 每条条目的准入格式是固定的七项：症状、阶段、状态码或错误域 / 证据落点、最小复现、期望不变量、
+ 源码位置、文档位置与测试位置；缺项即不算合格条目。下表按第一读者（操作者）的检索需要把七项
+ 压成五列（症状、阶段、状态码·证据、定位、修复动作），其中「最小复现」「期望不变量」「源码位置」
+ 「文档位置与测试位置」四项按条目就近写在表外的正文与对应模块卡里，表内不重复；
- 本表覆盖 10 类高风险场景，每类都有对应的状态码、证据字段与回归测试面。
+ 本表覆盖 20 行高风险场景，每行都有对应的状态码、证据字段与回归测试面。
```

---

## 4 我自己重推过的公式 / 常数（推导，不照抄前两轮任何一方）

### a. 球面残差律 `δ(θ)` 的三个边界 —— `acsd.phase1.drizzle.md`

由该文档自带的式子（`:118`）

```
δ = (1 − pixfrac²)·θ²·[ 0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)² ] + O(θ⁴)
```

取表内同列条件 `pixfrac = 0.8`、中心在参考点（`r_c = ξ_c = 0`）：

```
括号项 = 0.25/(1+0) − 0 = 0.25
(1 − 0.8²)·0.25 = 0.36 · 0.25 = 0.09
⇒ δ = 0.09·θ²      （θ 以弧度计）
```

反解边界（θ = √(δ/0.09)，1 角秒 = 4.84813681e-6 rad）：

| δ | θ (rad) | θ (arcsec/px) |
|---|---|---|
| 1e-10 | 3.3333e-5 | **6.9** |
| 1e-9  | 1.0541e-4 | 21.7 |
| 1e-7  | 1.0541e-3 | **217.4** |
| 1e-6  | 3.3333e-3 | **687.5** |

回代验证该文档表内四档（`python3`，`reldiff < 1e-3`）：

| θ | 文档表 | 我的复算 |
|---|---|---|
| 2″ | 8.5e-12 | 8.4616e-12 |
| 6.3″ | 8.4e-11 | 8.3960e-11 |
| 60″ | 7.6e-9 | 7.6154e-9 |
| 300″ | 1.9e-7 | 1.9039e-7 |

⇒ **表对、摘要句错**。原句「`θ ≲ 10″/px` 时该偏离 ≲ 1e-10」实测 θ=10″ 给 2.1154e-10（差 2.1×）；「`θ ≳ 100″/px` 时进入 1e-7–1e-6」实测 θ=100″ 给 2.1154e-08（差约一个数量级）。

### b. 面亮度星等 `Ω_ref` 项的符号 —— `PHASE1_DETAILED_DESIGN.md:161`

定义式（`DATA_SEMANTICS.md:114` 同）：`m(Ω) = ZP − 2.5·log10(F)`，`F = signal·Ω`。

```
直接展开：m(Ω) = ZP − 2.5·log10(signal·Ω) = ZP − 2.5·log10(signal) − 2.5·log10(Ω)
⇒ Ω_ref 项是减号。
```

用一级正本自带的自检参数独立复算（`ZP=0`、`signal=1e-4 /sr`、`Ω_ref = 1 arcsec² = 2.3504430539097885e-11 sr`）：

```
−2.5·log10(Ω_ref) = 26.5721
直接定义式 m(Ω_ref) = 36.5721
减号式             = 36.5721   ✓
加号式             = −16.5721  ✗
两式差             = 53.1443 mag
```

`Ω_ref = 1 sr` 时该项 = −0.0（退化相同），故加号写法只在 `Ω_ref ≡ 1 sr` 时不显形 —— 与一级正本 `:114` 的自述一致。

### c. 两档权重式的对账关系 —— `UNIFIED_MODEL.md:89–99`

由该文档自带的 `ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)` 与 `F_ref,k = 10^(−0.4·(m_ref − ZP_k))`：

```
0.4·ZP_k = 0.4·ZP_syn,k − 1.0·log10(k_photo,k)
⇒ F_ref,k = 10^(0.4·ZP_syn,k)·10^(−0.4·m_ref)·(k_photo,k)^(−1) = F0/k_photo,k
⇒ a_k := F_ref,k/F0 = 1/k_photo,k
```

代入 `SNR_k = F_ref,k/σ_F^{frame,k}` 与 `σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}`：

```
SNR²/F0²    = (F_ref,k/F0)²/σ_frame² = a_k²/σ_frame² = 1/σ_sys²     （a_k² = 1/k_photo²）
SNR²/F_ref,k²                        = 1/σ_frame²
⇒ 两式相差逐帧因子 a_k²；k_photo,k ≡ 1 时同值
```

数值自检（`python3`，取 `ZP_syn=25.0, k=2.0, m=6.0, σ_frame=3.0`）：

```
F0 = 39810717.055349775   F_ref = 19905358.527674884   F0/k = 19905358.527674887
a_k = F_ref/F0 = 0.5（= 1/k）
SNR²/F0² = 0.027777777777777766  = 1/σ_sys²  = 0.027777777777777776   ✓
SNR²/F_ref² = 0.11111111111111113 = 1/σ_frame² = 0.1111111111111111   ✓
a_k² · (1/σ_frame²) = 0.027777777777777766                               ✓
```

### d. G-RES-01 判据的 enforce 面 —— `21_observability.md:109`

读 `eng/contracts/resource_gate_v1.json` 的 `compute` 块，逐判据列 enforcement 键：

```json
"min_active_compute_threads": 2,          // ① 无 enforcement 键
"queue_low_window_enforcement": "hard_fail",  // ② 唯一带硬失败面
"mean_utilization_enforcement": "record_and_justify",   // ④
"p50_utilization_enforcement":  "record_and_justify",   // ⑤
"per_sample_enforcement":       "record_and_justify"    // ⑥
```

③ 无界内存增长（C++ 分配报告面）亦无键；顶层 `enforcement.in_process_hard_fail = "retired"`、`in_process_default = "record_only"`。
⇒ 只有 ② 能在资源门判定面产生 exit 10。`:89`/`:91` 的表与 `:97` 的结论**对**，`:109` 的「①②③」**错**。

### e. 排障表行数 —— `merged_TROUBLESHOOTING.md`

自写脚本逐行判表（表头 `:43` 5 列、分隔 `:44` 5 列、数据行逐行计）：**表头 5 列、数据行 20 行**（`:45–:64`）。
⇒ R2 审稿给的「21 行」把分隔行算进去了；我按行号区间与竖线计数复算，**正确值是 20**。原句「10 类」两个数都不对。

### f. `registry/` 卡片与端口注册表差集 —— `00_INDEX.md:33`

```
python3: module_ports.registry.json 的 module_id 集合 = 20
         docs/detail/registry/acsd.*.md = 25
         有卡不在注册表 = [acsd.phase1.hips-writer, acsd.phase1.session,
                           acsd.phase1.star-detection, acsd.phase2.resample,
                           acsd.phase2.session]
         注册表无卡 = []
```

⇒ `:33` 只说「每个生产 DAG 节点模块一页」不覆盖后 5 张。改写后点名这 5 张并说明「一页一模块、只是不进端口图」，计数 `:22`/`:41` 的 25 保持不变。

---

## 5 索引真解析器输出

`docs/DOCUMENT_INDEX.yaml` 本车道**未改**（本单只改文档正文，未增删文档、未改路径/职责面；索引中与本单相关的字符串实测：`数据对象（各自具名）` 0、`FITS 产品` 0、`唯一术语权威` 0、`point_source_information` 0、`默认稀疏路径要求默认产出` 0；`进程退出码` 1 命中是 `ERROR_MODEL.md` 的 duty 描述文字、非节名引用，无需改）。

校验在**science / engineering 车道改动后的工作树**上跑，真解析器 `yaml.safe_load`：

```
yaml.safe_load: OK
schema_rev: 3
entries: 148 | duplicates: 0 | dangling: 0
docs/*.md total: 167 | unregistered: 27 | of which README: 27 | non-README: []
entries missing status/duty: 0 []
```

对账规则：`path` 重复计数 0；`path` 不存在于磁盘的 0；`docs/` 下 167 份 `.md` 中未登记的 27 份**全部**是目录招牌 `README.md`，落在索引自订条款 `coverage` 里「目录招牌件按『每目录必有 README』治理，不进规范索引面」的豁免内，**非 README 的未登记件 = 0**。

**车道内交叉引用复验**（自写校验器：建立全仓标题索引 + 解析 `路径 「节名」一节/一章/一段` 形态，支持前缀匹配与同名文件解析）：

```
scanned: 108   unresolved: 0
```

初次跑（严格等值）报 20 条，逐条读内容后确认：2 条是本单新写的引用（已核真实标题）、其余 18 条全部是**前缀引用**（目标节名后带括注，如「参考通量基准」← `### 2.1 参考通量基准（F_ref 的口径）`）或**我的解析器把裸文件名当路径**（`21_observability.md`/`19_runtime.md` 同目录）。加前缀与同名解析后 **0 悬空**。

---

## 6 我推翻的前两轮判定

| # | 被推翻的判定 | 出处 | 我的反证 |
|---|---|---|---|
| **O-1** | 「`PHASE1_DETAILED_DESIGN.md:112` 的「O3 检测阈值」是**被伪造出来的节名**，原文的 `§3.1` 才是对的」 | T06-r2 审稿 DOC-DETAIL-GLOSSARY §2.3 N08 ① | **节名是真的，错的是文件**。`docs/detail/STAR_DETECTION_IMPL_DESIGN.md:145` 逐字是 `### 5.3 O3 检测阈值 —— [论文锚定]`；O1–O16 是 detail 层算子编号体系，`docs/science/detection/STAR_DETECTION.md` 里根本没有 O 编号。所以**不该另造节名，该换文件**。按 R2 的改法去做会把真节名换成假节名 |
| **O-2** | 「`merged_TROUBLESHOOTING.md` 表实有 **21 行**」 | T06-r2 审稿 §3 M03 | 我逐行判表（表头 `:43`、分隔 `:44`、数据 `:45–:64`）得 **20 行**。R2 的 `awk '/^\| 症状 \|/{f=1;next} f&&/^\|/{c++}…'` 把 `\|---\|` 分隔行也计了 1。按 R2 的数去改会把正文明明可数的数字改错 |
| **O-3** | 「`21_observability.md:109` 应保留「①②③」并改 `:89`/`:91`」 | 隐含在 R2 §2.1 的「判据表 ①③ 改对了」 | 我读合同 `eng/contracts/resource_gate_v1.json` 的 `compute` 块：**只有 `queue_low_window_enforcement = "hard_fail"` 一个键**，①③ 确实没有 enforcement 键，`in_process_hard_fail` 已 `retired`。所以 `:109` 是**唯一写错的那句**，改 `:109`、不动表 —— R2 把方向弄反了 |
| **O-4** | 「`acsd.phase1.star-detection.md:13` 的「registry descriptor 单源」照第 1 轮改即可」 | T06-r2 §2.2 X-01 缺陷 2 | 改法要给，但「单源」这四个字**不能留**：实测该 `module_id` 不在 `module_ports.registry.json` 的 20 个 `module_id` 内，端口图上是 `acsd.phase1.star-psf`。本单改成三层分名（卡片名 / 文档路径 / 端口 `module_id` 各归各），比「照第 1 轮改」多一层，避免下一轮再撞 |

**我采信并独立复核通过的子代理结论**：GLOSSARY「唯一术语权威」不成立（反例是顶点 `ACSD_DESIGN.md:562` 点名 22 术语、词典覆盖 7 个，我逐条 grep 核过）；附录 A 漏 `depth_m5`（`:149` 列 13、`:562` 列 12，我逐字比过）；`point_source_information` 是全仓不存在的野生别名（改后全仓 0 命中）；`SB_mag` 加号错 53.14 mag（我用一级正本自带自检参数独立复算，非采信）。

**我否决的子代理结论**：

| # | 子代理结论 | 我的裁决 |
|---|---|---|
| **V-1** | 子代理 B（引用审计）报「`PHASE1_DETAILED_DESIGN.md:112` 的 `§3.1` 本来就是对的，被换错」 | 见 O-1：节名本身真实、文件被换错。该结论的**问题定位**（有一处被换错）成立，**改法**不成立 |
| **V-2** | 子代理 D（数据对象）报「附录 A 指针断 2/3 ⇒ GLOSSARY 缺 9/13 个 canonical 对象」，建议把 9 个对象补进 GLOSSARY | **否决补词条**。补进去就等于让 GLOSSARY 第二次自称对象定义正本，重犯它自己的病。改走 D-07：把自我声明降级为「叫法登记面」并把对象定义指回 `UNIFIED_OBJECTS.md` 对照表——**指针修好，词条不扩** |
| **V-3** | 子代理 A（公式常数）报「两档权重 `F_ref²` vs `F0²` 应统一成 `F0²`」，理由是代码按 `F0` 实现 | **否决单方统一**。顶点 `ACSD_DESIGN.md:280` 同时声称「是 point information 最优集成」并写 `F_ref²`，这两句在 `F0` 口径下不可能同真；改哪一侧要动 science 一级正本与顶点，不在文档车道权限内。按 D-22 处理为**显式分名 + 对账**，争议按 §5 登记 UNRESOLVED |
| **V-4** | 子代理 E（对抗复核）报「`1.152` 与 `1.166` 是同一个量，应统一」；另一代理报「`1.817/1.65/1.77` 应并进同一张偏差因子表」 | **两条都否决**：`1.152` 是 `√N·sd(σ̂)/σ`（二阶离散度），`1.1663872874444212 = 1/(4φ(d)d)` 是渐近系数；`1.817` 是守恒映射抽头核的相关偏差因子 `1+ρ(M_eff−1)`、与其余三者量纲与成因全不同。三者都不在本车道文件里，且合并会造出仓内不存在的量 |

---

## 7 自证段

### 7.1 我实际做了什么

1. **逐行读完必读件**：`AGENTS.md`（127 行，全文）、`docs/ACSD_DESIGN.md`（568 行，全文）；R2 五份审稿中与本车道相关的 `T06-r2-审稿-DOC-DETAIL-GLOSSARY.md` 全文（184 行）。
2. **逐段读完并动手的本车道文件**：`GLOSSARY.md`(31)、`README.md`(12)、`detail/README.md`(3)、`00_INDEX.md`(79)、`common.md`(72)、`UNIFIED_MODEL.md`(121)、`PHASE1_DETAILED_DESIGN.md`(227)、`PHASE2_DETAILED_DESIGN.md`(145)、`PHASE3_DETAILED_DESIGN.md`(173)、`LOG_AND_ERROR_SYSTEM.md`(214)、`merged_TROUBLESHOOTING.md`(90)、`anchors/ANCHOR_CONTRACT.md`(152)、`infrastructure/{19_runtime,21_observability}.md`、`registry/{acsd.phase1.noise-snr, acsd.phase1.drizzle, acsd.phase1.star-detection, acsd.phase2.coverage, acsd.phase2.integrate, acsd.phase2.upm-fit}.md`。
3. **跨片对照面**：`science/unified/DATA_SEMANTICS.md`（§3.1/3.2/3.3/3.4/4.1/4.2 全读）、`science/detection/STAR_DETECTION.md`（标题集 + §3.1 全读）、`science/noise_snr/NOISE_SNR.md`（标题集 + §3.3/3.4/3.5/6.4/7）、`engineering/UNIFIED_OBJECTS.md`、`engineering/contracts/LOG_AND_ERROR.md`、`engineering/standards/ERROR_MODEL.md`、`engineering/contracts/CONFIG.md`、`eng/contracts/schemas/unified/point_information.schema.json`、`eng/contracts/resource_gate_v1.json`、`eng/packaging/config/defaults.json`（`snr.path` 段）、`lib/infrastructure/pipeline/module_ports.registry.json`。
4. **自己动手算的量**（§4 a–f 六组，全部 `python3` 复算，未采信任何前两轮结论作为唯一依据）。
5. **自己跑的真解析器对账**：`yaml.safe_load` 索引全量对账（148/0/0/27）；车道内 108 条节名引用解析（0 悬空）；`module_ports.registry.json` 差集；`resource_gate_v1.json` 逐判据 enforcement 键；`point_information.schema.json` 的 `required` 字段表；排障表逐行判列判行。

### 7.2 派发了哪些子代理、否决了哪些

用 `subagent` 派发 **5 个只读子代理**（各自零 git 写、零文件修改）：

| 子代理 | 车道 | 复核结论 |
|---|---|---|
| A1 / A2 | 公式与常数重复普查（全仓 × detail 为重点） | **采信**并全部独立复算：`SB_mag` 加号错 53.14 mag、`k = pixfrac²` 括号自相矛盾、`δ` 摘要阈值错、`m_ref` 冻结措辞、`snr_path` 归属 phase1、`support` 三种归约互斥、`F0` vs `F_ref` 分裂。**否决 1 条**（V-4 的常数合并） |
| B | 节名与引用失效普查 | **采信**伪造节名清单并逐条开目标文件核实（`量纲与逐像素语义`、`HEALPix 索引与 tile 布局`、`症状索引`、`FITS 产品`、`数据对象（各自具名）` 全部确认无此标题）。**否决 1 条**（V-1：`O3 检测阈值` 的改法）。**额外采信其独立发现**：`docs/engineering/SCIENTIFIC_REFERENCES.md` 已删但 28 份 science 文档仍指它（31 处）——本车道外，登记 |
| C | 术语表与「唯一术语权威」自查 | **采信**（反例是顶点自己、反例可 grep 复现）；**否决 1 条**（V-2：补 9 个 canonical 词条进 GLOSSARY）。**采信其对 `mask`/`validity` 四名四义的逐处读证**，但**未合并**（见 §2-D/F） |
| D | 13 个数据对象 × 四级口径 + P1–P5 + 三种重建方式 | **采信**其 `point_information` 别名与丢因子（我已在 PHASE1 落地）；**否决**其「附录 A 与 GLOSSARY 都补全」的处置方向（改走 D-07 降级声明）；**采信**其「`depth_m5` 口径 science「PSF 与孔径」vs detail「禁另起孔径口径」」并登记（science 车道） |
| E | 对抗复核：同名不同义 + 独立重推常数 | **采信**其 §2 的常数推导（`1.152` 独立 MC 1.15231±0.00099、`c_se` 逐位复现 1.1663872874444212、`1/Φ⁻¹(0.75)` 的 1 ULP 离群点）。**否决 2 条**（V-4）。其 §1 的 SNR 族/权重族/`sky_plane ≠ B_ref` 分层分析**采信为保留依据**，据此确认 §2 的 A/B/D/E 四项不合并 |

### 7.3 我没有做的事（诚实边界）

- **没有做 git 写操作**：全程零 `add/commit/checkout/reset/stash/tag`；只跑了 `git -c core.quotepath=false status --short` 与 `diff --numstat/--stat`。
- **没有编译、没有跑测试、没有跑端到端**：所有「代码为准」的判定来自源码阅读 + JSON 解析 + `python3` 复算。
- **没有取任何一手文献全文**：`Rousseeuw & Croux 1993` Table 2 的 `1.361`、`WD-HiPS-2.0` §4.3.2、PixInsight 的 `(Σf)²/σ_n²` 逐字出处 —— 一律报「核不到」，不凭印象裁。
- **没有裁决三条科学口径之争**（`F_ref` vs `F0`、`depth_m5` 参考轮廓、`validity` 三套枚举），全部按 AGENTS.md §9 登记 UNRESOLVED，理由是它们要动一级正本或顶点，且顶点修改须负责人批准。
- **没有改车道外文件**：`docs/science/**`、`docs/engineering/**`、`eng/**`、`lib/**`、`实验/**` 一律只读；跨车道发现按「文件:行 + 建议改法 + 不改的理由」登记。
- **没有删任何一条术语词条**：GLOSSARY 21 条全留（V-2 的否决理由之一）。
- **没有做引用形态的全局迁移**：AGENTS.md §5 禁「见 §几」式机械锚，但全仓跨文档 `X.md §N` 仍有 424 行、`X.md「节名」一节` 255 行。本单只清掉本车道 19 份文件里的 GLOSSARY 20 条与 `docs/README.md` 1 条；registry 26 张卡的抬头（`> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）…`）仍有大量 `§` 锚，**逐批迁移需前台定硬规则后统一做**，不在本单做半套。

### 7.4 收敛判定与「还有哪些没收敛」

**本车道未达收敛。** 本单解决了 22 条重复断言中的 22 条（车道内 0 残留，已 grep 复验：`LOG_AND_ERROR.md`「进程退出码」0、`point_source_information` 0、`症状索引` 0、`量纲与逐像素语义` 0、`HEALPix 索引与 tile 布局` 0、`「FITS 产品」` 0、`唯一术语权威` 0、`「资源门」一章` 0、`资源门判定与 exit 10` 0、`冻结的参考星等档` 0 全仓、`冻结参考星等` 0、`默认稀疏路径要求默认产出` 0），但**遗留项已明确清单化**：

**车道外、已定位待裁（不属本车道写面）**

| 编号 | 位置 | 事实 | 影响 |
|---|---|---|---|
| **U-01** 🔴 | `docs/engineering/UNIFIED_OBJECTS.md:4,:9,:34,:54,:58` + `docs/engineering/data/ARTIFACTS.md:43,45,46,50–62` | **20 处**引用 `docs/detail/UNIFIED_MODEL.md`「数据对象（各自具名）」一节；该节实际标题是 `## 2. 数据对象与字段歧义消解` | 13 个 canonical 对象的整条登记面挂在不存在的节名上。**改法**：20 处逐字替换为「数据对象与字段歧义消解」一节 |
| **U-02** 🔴 | `eng/contracts/schemas/phase_config_mosaic.schema.json:171` + `eng/packaging/config/defaults.json:843–852` | 31 处（`eng/packaging` 129、`lib` 19、`eng/contracts` 15、`docs/science` 17）仍指已删除的 `docs/detail/algorithms_phase{1,2,3}/**`（17 个目标路径全 MISSING）。其中 `defaults.json` 的 `snr.path` 内容锚 `path` 也指向 MISSING 的 `algorithms_phase1/07_noise_snr.md`，而其 `quote` 逐字等于 `docs/detail/registry/acsd.phase1.noise-snr.md` 旧行 | 内容锚 fail-closed（D 类判据）。本单改了那行（归属修正），**故该锚的 `sha256` 必须同步重算**；重算需要先由 engineering 车道定 `snr_path` 登记正本落在哪张卡 |
| **U-03** 🔴 | 28 份 `docs/science/**`（31 处） | 「参考代码库（含许可证）正本 = `docs/engineering/SCIENTIFIC_REFERENCES.md` §M」——该文件不存在 | 违反 AGENTS.md §6「引用任何条款前先读原文确认真实存在」，且是三类证据里「开源科学代码」那一腿的登记面 |
| **U-04** 🔴 | `docs/engineering/contracts/CLI_PROTOCOL.md:36`、`docs/engineering/api/PUBLIC_API.md:1215` | 仍指 `LOG_AND_ERROR.md`「进程退出码」一节（该节名不在此文件）；`PUBLIC_API.md:1215` 另指 `LOG_AND_ERROR.md`「验收」一节（该文件**无「验收」节**，而 `PUBLIC_API.md` 自己的 `## 验收` 在 `:99`） | 与 D-01 同源，本车道 4 处已改，车道外 2 处未改 |
| **U-05** 🟠 | `docs/ACSD_DESIGN.md:111,:280` + 一级正本 `docs/science/noise_snr/NOISE_SNR.md:338` | 叠加权重归一因子两派互斥（`F0²` vs `F_ref,k²`），science 正本逐字禁止后者，顶点与另 10 处文档用后者。`UNRESOLVED.md` 27 条裁项**未登记**此项 | 须负责人裁 + 改 science 一级正本，非文档车道可闭 |
| **U-06** 🟠 | `docs/science/unified/DATA_SEMANTICS.md:282`、`docs/science/unified/UNIFIED_SCIENCE_MODEL.md:206` | 「术语的唯一权威是 `docs/GLOSSARY.md`」——同 D-07 的自我声明，本车道 2 处已改，这 2 处是它的下游放大器 | 改指 `docs/engineering/UNIFIED_OBJECTS.md` |
| **U-07** 🟠 | `docs/ACSD_DESIGN.md:118` | 「12 个 HEALPix 基面的等面积 chart，每个面是**单位正方形**、Jacobian 恒为 π/3」——子代理 E 用仓内 HEALPix 上游源码实算：标准 base-pixel chart 定义域是 `(x,y) ∈ [−1,1]²`（面积 4，覆盖 **4 个**基面像元），J ≡ π/3 数值正确但与「单位正方形」不配对（若缩放到单位正方形 J 应为 `4π/3`）。12 × 4 × π/3 = 16π ≠ 4π | 三者不能同时为真，顶点须删或改其中一个限定 |
| **U-08** 🟠 | `docs/detail/anchors/ANCHOR_CONTRACT.md`（152 行）+ `anchors/README.md` + `docs/detail/README.md:3` + `docs/detail/00_INDEX.md:24,36` | AGENTS.md §5 明文「**不设锚点合同类文档**」，而本车道存在一整套「文档—代码锚合同」文档 + 目录 + 三处自述 | 删整棵子树是结构性变更，且该合同被 `19_runtime.md:14` 引为行号锚合同依据；**须负责人裁定**，本单不擅自删 |
| **U-09** 🟡 | `docs/science/unified/UNIFIED_SCIENCE_MODEL.md:140`（`depth_m5` =「固定参考 **PSF 与孔径**下」）vs `docs/detail/UNIFIED_MODEL.md:42`（「固定参考 **PSF** 下…**禁另起孔径口径**」） | detail 明文禁止 science 写的口径，且是三级压一级 | 须 science 车道裁 |
| **U-10** 🟡 | `docs/science/algorithms/PHASE2_COVERAGE.md:25` 的 `validity`（排异接受掩码）vs canonical `validity`（传感器状态）；`UNIFIED_OBJECTS.md:46`/`ARTIFACTS.md:60` 写 integer 而 `UNIFIED_SCIENCE_MODEL.md:146`/`DATA_SEMANTICS.md:65` 写布尔 | 同一个 canonical 对象四名四义 + dtype 分歧 | 须 science + engineering 同批 |
| **U-11** 🟡 | `docs/engineering/standards/NUMERIC.md:185` 的「四概念分离」含 `mask`，而 `UNIFIED_OBJECTS.md:47` 的 13 对象里是 `rejection` | 分组不一致；按 NUMERIC 建词典会把 `mask` 提升为 canonical 并吞掉 `rejection` | 须 engineering 车道裁 |

**车道内、已定位待裁（本单无权自决）**

| 编号 | 位置 | 事实 |
|---|---|---|
| **U-12** | `acsd.phase2.integrate.md:64` 等 10 处仍写 `w = SNR²/F_ref²` | 待 U-05 裁决后统一，本单只在本车道唯一正本（`UNIFIED_MODEL.md` §2.1）补了对账 |
| **U-13** | `registry/` 26 张卡抬头仍有大量 `docs/ACSD_DESIGN.md §8.5（模块与 ABI）` 式机械锚 | 违反 AGENTS.md §5；本单只清了 2 处**伪造小标题**（D-17），未做 `§N` → 节名的批量迁移 |
| **U-14** | `acsd.phase1.noise-snr.md:144,:435` 与 `acsd.phase2.integrate.md:129` 的降级语义 | 与 D-03 保留的配置默认值层配套，语义正确，无需改 |

### 7.5 逐字改动附表：GLOSSARY 20 条锚点（`文件:行` → 改指）

| 行 | 改前（`§` 机械锚） | 改后（真实节名） |
|---|---|---|
| 11 | `CALIBRATION.md §2.2` | 「标度的物理解释链」一节 |
| 12 | `ACSD_DESIGN.md §2.2`；`SCIENCE_SCOPE.md` | 《ACSD 最高设计》「P2 跨帧绝对信噪比」一节；`SCIENCE_SCOPE.md`「标度类别链与标度律」一节 |
| 13 | `DRIZZLE.md §3.6`；`DATA_SEMANTICS.md §3.3` | 「方差与协方差传播」一节；「方差与逆方差的三态编码」一节 |
| 14 | `DATA_SEMANTICS.md §3.3` | 「方差与逆方差的三态编码」一节 |
| 15 | `DATA_SEMANTICS.md §3.3/§3.9` | 「方差与逆方差的三态编码」与「权重词表：登记面与输入面」两节 |
| 16 | `NOISE_SNR.md §6.4` | 「质量面与信息面的分离」一节 |
| 17 | `UNIFIED_OBJECTS.md §2`；`UNIFIED_MODEL.md §2` | 「13 个对象 → canonical schema → schema ID（对照表）」一节；「数据对象与字段歧义消解」一节 |
| 18 | 同上 | 同上 |
| 19 | `DATA_SEMANTICS.md §3.2` | 「三个基本对象的语义」一节 |
| 20 | 同上 | 同上 |
| 21 | `cosmetic_corrector.cpp` 的 `interpolate_pixels`；`COSMETIC_ALGORITHMS.md` | `COSMETIC_ALGORITHMS.md` 的坏点修复段（**去代码锚**：AGENTS.md §4 权威链，术语权威锚到最低层代码方向反了） |
| 22 | `ARTIFACTS.md`（…）；`aio_hips.h` | `ARTIFACTS.md`（…）**（去代码锚）** |
| 23 | `DATA_SEMANTICS.md §3.1` | 「坐标语义」一节 |
| 24 | `ASTROMETRY.md §2.3` | 「像元坐标约定与桥接」一节 |
| 25 | `DATA_SEMANTICS.md §3.1` | 「坐标语义」一节 |
| 26 | `ACSD_DESIGN.md §6.3`；`DRIZZLE.md` | 《ACSD 最高设计》「投影算法」一节；`DRIZZLE.md` |
| 28 | `DATA_SEMANTICS.md §3.7` | 「帧身份与输入清单摘要」一节 |
| 29 | `DATA_SEMANTICS.md §3.2` | 「三个基本对象的语义」一节 |
| 30 | `DRIZZLE.md §3.3` | 「面亮度归一分母」一节 |
| 31 | `CALIBRATION.md §4` | 「参数与常数」一节 |

改后 `grep -c '§' docs/GLOSSARY.md` = **0**；27、11、27（`stacking` / `projection` / `INVALID` 行的锚本已无 `§`，未列）。