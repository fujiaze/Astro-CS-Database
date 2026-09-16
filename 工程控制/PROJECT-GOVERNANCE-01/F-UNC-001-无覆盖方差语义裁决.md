# F-UNC-001 裁决：Phase1 逐帧 variance/ivar 产品的无覆盖/无效像素取值（0 与 NaN 的唯一收敛）

裁决人：IVAR-001（AstroCS 研究+执行线）  处置：前台（涉 SCI/ALG 文本按 ENGINEERING_SPEC §3 落地）
状态：**唯一裁决已给出**（本文即结论；不含「待负责人裁决」项）。实施随 IVAR-001-FUP（ECP-001）批次执行，本文是前置②的收口件。
日期锚：2026-09-17，HEAD=2a6676a7659a6e3fe39a87dac8b2500529668f59。

---

## 0 裁决摘要（可直接执行）

| 编号 | 裁决 | 一句话依据 |
|---|---|---|
| **R1** | Phase1 写侧：**无覆盖 / 无方差信息**的像素写 `variance=0` 且 `ivar=0`（0/0），**不得写 NaN**。 | DATA_SEMANTICS §4a:38「无覆盖像素=0」+ §4a:39「variance=0/缺失 → ivar=0（显式不可用，禁止伪装）」；§4a:40 把 NaN 保留给「产品损坏」。 |
| **R2** | tile 级不变：**全 tile 无信息 → 写请求 rc=−5、不落盘**（§12.2:358 / §12.4:395）。⇒ 0 永不构成「整幅零方差」伪产品。 | §12.4:395-396「显式失败而非空产品」。 |
| **R3** | 上游**损坏**（`var_num_sum` 或 `covered_area` 非有限/负）⇒ 写请求**新增 rc=−6 硬失败**（禁写 NaN 产品、禁 clamp、禁静默跳过）。 | §4a:40（NaN/负 = 损坏）+ §30.2:2409（输入非有限/负 = hard fail）+ §12.4:396。 |
| **R4** | 读侧（Phase2/Phase3）：**先过资格门**（support>0 ∧ finite signal），再按「`ivar=0` ⇒ 合法零权重；`variance=0` ⇒ 不可用（P3 映射 NaN 传播态）；非有限/负 ⇒ 产品损坏 hard fail」；**禁止 1/0→Inf**。 | §20.1:1051 + §30.2:2409 + §30.4:2515-2521（订正：禁 1/0→Inf）。 |
| **R5** | signal/support 语义**不变**；无覆盖像素三产品互补编码：`signal=NaN`、`support=0`、`variance=0`/`ivar=0`。**删除 P1 写侧面「与 signal NaN 同态」的表述**（§30.2:2406 是 Phase2 输出面，继续有效）。 | §12.2:356-357（signal NaN / support 0.0）+ §4a:38-40。 |

三处分界（ECP-001 §3.2）唯一结论：

| 分界 | 裁决 |
|---|---|
| **① 噪声模型消费哪一帧** | **calibrated 帧**（= drizzle 节点实际消费的同一数组，逐像素对齐）。 |
| **② 饱和/掩膜/边界样本** | **置不可用（0/0）**，禁止外推「好看」的方差；源污染像素保留空背景模型值（产品元数据须可读「不含源泊松项」）。 |
| **③ floor** | 单位 **ADU²**，默认值 **1e-12**（冻结合同字段），语义 `var := max(var, floor)` **只作用于有效值**；不可用由 `ivar=0` 表达、不得由 clamp 产生；生效 floor 必须写入产品/manifest 并进门。 |

---

## 1 冲突原文（逐字；四处）

1. **§4a（元素语义）** docs/contracts/DATA_SEMANTICS.md:37-40：
   - `variance`：逐像素随机方差（信号单位²），Drizzle 方差传播 `variance_p = Σ v_j·w_jp² / D_p²`（SCI-DRZ-014）；**无覆盖像素=0**。
   - `ivar`：逆方差 `1/variance`；**variance=0/缺失 → ivar=0（显式不可用，禁止伪装）；NaN/负 variance 视为产品损坏**。
2. **§4a 预存歧义登记（本次任务的对象）** :45-50：「**无覆盖值歧义消解（DATA-UNC-001 登记，2026-09-09）**：本节 「无覆盖像素=0 / ivar=0」 与 §12.4 P1 写侧 「variance/ivar=NaN」 存在预存双值（finding F-UNC-001，归 Phase1 域任务消歧，本文不在此处单方改写）；Phase2 合成与 Phase3 传播**输出产品**的无覆盖/无效语义以 §30 为唯一权威（NaN 同态，§30.1/§30.4）；ivar=0 仅保留为 Phase2 读侧对逐帧 ivar 产品的合法零权重消费态（§20.1/§21）。」
3. **§12.1/§12.2（Phase1 写侧 invalid 列）** :349：`var_num_sum` … 「variance/ivar 产品时强制非 NULL（缺失 rc=−2 :575-576）；**≤0/非有限 → 该像素 variance/ivar=NaN**」；:358-359：variance 「无效或 area·vnum≤0/非有限 → **NaN**（:615-621）；tile 全无效 → 写请求 rc=−5 不落盘（:629-632）」、ivar「同 variance（NaN 对应）」。
4. **§12.4（汇总）** :391-396：「无效判定域（signal/support）：`valid && area>0 && isfinite(flux) && isfinite(area)`；variance/ivar 额外要求 `vnum>0 && isfinite(vnum)`。违反 → signal=NaN、support=0.0、**variance/ivar=NaN**。IEEE NaN 填充，不使用 FITS BLANK。」「全无效 variance tile：写请求返回 −5、不落盘 …——显式失败而非空产品。」

相关（不冲突、继续有效）：

- **§30.2（Phase2 输出面）** :2406：「无有效样本（n_used=0 …）| NaN | NaN | 与 signal=NaN 同态（writer 通道 §12.4 合同：covered_area≤0 → NaN）；禁 0/±Inf 伪装」；:2409：「输入 ivar 非有限/负 | — | — | `p2_validate_candidate_weights` hard fail（现行 rc=6），不入合成」；:2411-2413 注：「Phase1 逐帧输入产品的 0/NaN 双值预存歧义见 §4a 消解注记（finding F-UNC-001）；读侧消费按 §20.1（ivar==0 合法零权重、nonfinite 拒），本表只冻结 Phase2 **输出**产品。」
- **§30.4（Phase3 读侧/输出面）** :2507-2521：「输入 HiPS 含 variance/ 子产品 → u_in = variance 平面；否则含 ivar/ → u_in = 1/ivar（**ivar==0 像素 = 零权重 ⇒ u 无效 = NaN 传播态，非硬错误**）…」「u 值域: NaN = 传播态；负/Inf（**ivar==0 导出的 1/0→Inf 明确禁止**）= 产品损坏 → 显式错误」。
- **§20.1** :1051：`AIO_HIPS_RD_IVAR`（weight_mode=2 时强制打开）「逆方差积分权重；缺失帧 → rc=7 或显式降级标红」。
- **代码现状（读侧资格门先于权重面）** module_adapters.cpp:4631-4640：「调用方资格（SCI-INT §5 valid ∧ W>0 面）: finite ∧ support>0 ∧ 逐样本 accepted; 先资格过滤后权重面 —— 无覆盖像素（support=0 → corrected NaN → 过滤）不进入权重检查（ivar 产品在无覆盖像素 = NaN 同态, §30.1 表注 F-UNC-001）」；:4649-4656：入栈样本 `w = ivar`；「`if (!std::isfinite(w) || w < 0.0)` → hard fail …（DATA-UNC-001 §30.1: p2_validate_candidate_weights hard fail, no clamp/no skip）」。
- **噪声模型自身的不可用表示** docs/science/NOISE_MODEL.md:105-107、:115-116、:135：「不满足 ⇒ `ivar=0, r=1` 拒绝加权（「空 support 不传播」保持）」「**空 support 不传播**：无合格 patch 时 `ivar=0, r=1` 拒绝加权，**不产生伪有效权重**」「`ivar=0` 显式表示不可用，禁止…」。

---

## 2 外部标准（先外部）

**证据状态声明**：本环境无法机读 FITS 标准 PDF（fits.gsfc.nasa.gov/standard40/*.pdf 返回 application/pdf 不可解析；A&A HTML 镜像 403），以下外部条目按标准编号引用其**规则内容**，不冒充逐字引文；仓内引文均为逐字。

1. **FITS Standard 4.0（Pence et al. 2010, A&A 524, A42）**：浮点数组的「未定义值」由 **IEEE-754 NaN** 表示；`BLANK` 关键字**仅适用于整型数组**，不得用于浮点数组。⇒ 对本问题的直接含义：浮点 variance/ivar 产品**只有两种可用编码（数值或 NaN）**，不存在第三种「空值」写法；因此 0 与 NaN 的取舍必须由语义（可用/损坏）决定，不能靠「缺省空值」回避。与仓内 §12.4:394「IEEE NaN 填充，不使用 FITS BLANK」一致。
2. **IEEE-754 / FITS NaN 语义**：NaN 表示「不是数/未定义」，其参与算术会**传播**（NaN in ⇒ NaN out）。⇒ 若把「无覆盖」写成 NaN，则任何未先做资格过滤的算术都会把整幅结果污染为 NaN——这正是 §30.4:2515「NaN = 传播态」的来由；而在**输入产品**上，NaN 与「产品损坏」不可区分（§4a:40）。
3. **IVOA HiPS 1.0（REC-HiPS-1.0）**：HiPS 只规定图像 tile 的 FITS/几何/元数据组织，**不定义 variance/ivar 子产品语义**（本次核对官方文档页 ivoa.net/documents/HiPS/ 全文无 variance/uncertainty 条款）。⇒ 本问题**无外部标准可依**，权威链回到仓内合同（§4a/§12/§20/§21/§30）；外部标准只提供「编码可用性」与「NaN 传播」两条边界。
4. **逆方差/权重图约定（AstrOmatic SWarp、SExtractor 等主流叠加链）**：权重图取 **w = 1/σ²**，并以 **w = 0 表示「无数据/忽略该像素」**；下游以求和 Σw 与 Σw·x 工作，w=0 是求和的零元（不贡献、不污染）。⇒ 与仓内 `ivar` 的读侧语义（§20.1/§21：ivar==0 合法零权重）**完全同构**；本裁决 R1/R4 选择 0/0 与主流实践一致。

**外部标准给出的唯一硬约束**：不能在浮点产品里发明第三种空值；「无信息」必须用一个**在权重面是零元、且不被误读为损坏**的编码 —— 在 ivar 面上只有 0 满足（NaN 是损坏/传播态，任意正数是伪权重）。

---

## 3 唯一裁决与论证

### R1 无覆盖/无方差信息像素 → `variance=0`、`ivar=0`

- **正向依据**：§4a:38 直接规定写侧取值「无覆盖像素=0」；§4a:39 给出唯一可计算配套「variance=0/缺失 → ivar=0（显式不可用，禁止伪装）」——「缺失」二字把「有覆盖但无方差数据」也纳入同一分支。
- **反证（为何不能是 NaN）**：
  1. §4a:40「NaN/负 variance 视为产品损坏」——若写侧用 NaN 表示常态空像素，则**每个无覆盖像素都会被读侧判为损坏**，把常态与损坏混为一谈（违反 §9 可诊断性，且使损坏检测不可用）；
  2. §30.2:2409 + §30.4:2515-2521 已把「非有限/负」定义为 **hard fail / 产品损坏**；读侧现状之所以没炸，仅仅因为资格门（support>0 ∧ finite signal）恰好排在权重检查之前（module_adapters.cpp:4631-4640）——即**正确性依赖门序，而不是依赖值语义**；一旦某消费者先读 variance（§30.4:2507「variance 优先」），NaN 就会以「传播态」身份进入而没有损坏区分；
  3. §30.4:2515-2521 明确禁止 `ivar==0` 导出的 `1/0→Inf`，同时把 `ivar==0` 定义为**合法零权重**——即读侧权威已经把 0 当作「不可用」的正规编码；写侧若改用 NaN，则同一语义在两处编码相反（§4a 注记 :49-50 也指出 ivar=0 是读侧合法零权重态）；
  4. 噪声模型自身对「不可用」的产出就是 **ivar=0**（NOISE_MODEL:105-107/:115-116/:135「不产生伪有效权重」）⇒ 写侧用 NaN 会让**同一个量在管线两端编码不一致**。
- **为何 0 不会被误读为「零方差=完美测量」**：该风险由两条已冻结规则封死——(a) `ivar=1/variance` 在 variance=0/缺失时**定义为 0**（§4a:39），故权重面不会出现 Inf；(b) tile 级 §12.4:395「全无效 → rc=−5 不落盘」保证**不会出现整幅 0 方差的伪产品**（0 只能出现在「该 tile 至少有一个有效方差像素」的语境里）。
- **反向安全论证**：即便某消费者错误地把 variance=0 当「零不确定度」，它仍需先通过 support/signal 资格门（R4 强制）；而若把无覆盖写成 NaN，则该消费者得到 NaN 并大概率污染输出（§30.4:2515 传播态）。两害相权，0 的可检测性与可门控性更强。

### R2 tile 级不变（全无信息 ⇒ rc=−5 不落盘）

保留 §12.2:358 / §12.4:395 原文；这是 R1 的**安全前提**：0 只有在「tile 内存在有效方差像素」时才被写出，因此不存在「全 0 方差产品」这一伪形态。

### R3 上游损坏 ⇒ 新增 rc=−6 硬失败

- 触发条件：`covered_area` 或 `var_num_sum` **非有限**，或 `var_num_sum<0`（负方差）——即 §4a:40 定义的「产品损坏」；
- 行为：`aio_hips_write_variance_tile` 返回新错误码 **rc=−6**（诊断串含 tile ipix 与损坏像素计数）；调用方 `write_hips_phase1` 对 −2/−5 维持既有「跳过+计数」，对 −6 **不得跳过**（默认走 abort 路径）⇒ 损坏不能被静默降级为「少写一个 tile」；
- 为何不沿用 −5：−5 的语义是「无信息」（合法空），−6 的语义是「损坏」（必须可见）；把两者合并会让 §9 溯源与 E2E 诊断失去区分度（IVAR-001 取证中已出现「?? 编码降级」类可读性问题，不应再叠加语义合并）。

### R4 读侧资格门与零权重（写入合同，不改判据强度）

- 强制顺序：**先资格门**（`support>0 ∧ isfinite(signal) ∧ 逐样本 accepted`）→ **再读权重**（`ivar`）；
- `ivar==0` ⇒ 零权重（合法，§20.1/§21）；`variance==0`（P3 variance 优先分支）⇒ **不可用 → u 映射 NaN 传播态**，禁止读成 u=0（§30.4:2515 的既有精神 + 本裁决补全）；
- 非有限/负 ⇒ hard fail（§30.2:2409 现状，保留不动）；
- 任何路径**禁止** `1/0→Inf`（§30.4:2519-2521 逐字）；
- 新增可审计计数（不改变判据强度，只增加可见性）：Phase2 合成产物/manifest 记 `ivar_zero_weight_pixels`（历史）与 `variance_zero_pixels`（当帧消费）；当 `ivar==0` 且该像素**有覆盖且被接受**时计数并进入 diagnostics —— 使「有覆盖但无方差信息」不再静默。

---

## 4 三处分界唯一结论（ECP-001 §3.2）

### ① 噪声模型消费 **calibrated** 帧（并与 drizzle 节点逐像素同一数组）

- 依据（逐字）：NOISE_MODEL.md:7「估计**校准后**空背景随机分量的逐像素方差」；:35 量纲表「`x, σ_bg, √variance`: ADU（或 e⁻，**同输入标度**）；`variance`: ADU²」。
- 推论：**不需要**跨帧 α² 补偿（方差不来自 raw 帧），α² 缩放律只在下游（drizzle 内对样本做加权/缩放时）适用；这与 ECP-001 §3.2-1 的「二选一」收敛为「选 calibrated」。
- **现状不一致（必须在 FUP 实施时统一，本文只登记不改码）**：`p1_op_noise` 读 `p1_cleaned_input_path`（module_adapters.cpp:2758），而 `p1_op_drizzle` 读 `p1_calibrated_path`（:3068）⇒ 两者不是同一数组（cosmetic 会替换热像素）。**裁决：per-pixel variance 平面必须定义在 drizzle 节点实际消费的同一数组上**，并在 manifest 记录该帧路径+sha256，以相等门保证「方差与通量同源」；若 drizzle 节点日后改为消费 cleaned，则方差平面随之改（跟随消费者，而非跟随噪声节点的历史习惯）。

### ② 无效样本 → 置不可用（0/0）；源污染像素保留空背景值

- 无效样本集合：DQ/掩膜剔除、饱和像素（NOISE_MODEL §4 饱和域、SC-008）、非有限输入、以及天空预算不满足（n_qualified<8 或 N_sky<9216 收缩到 r_min 仍不满足）——后者按 NOISE_MODEL:105-106/:115-116 的既有裁决就是 **`ivar=0` 拒绝加权**；
- 禁止对无效样本外推「好看」的方差（NOISE_MODEL:135「ivar=0 显式表示不可用，禁…」；:106「不产生伪有效权重」）；
- 源污染像素：保留空背景模型方差值（NOISE_MODEL §9a:132-134 已冻结「本层不含源泊松项」），并要求在产品元数据/manifest 中保持该限制**可读**（现有 `NOISE_SATURATION_FILTER` 同风格）。

### ③ floor：ADU²、默认 1e-12、只夹有效值、必须显式进门

- 单位：**ADU²**（与 variance 同量纲，NOISE_MODEL:21「`variance_floor` 方差下界」；:107 不变量「`variance≥1e-12`、`ivar` 有限」）；
- 数值：默认 **1e-12**（NOISE_MODEL:21/46/:117 回退值 1e-12 一致；:142 明令「改变 `variance_floor` 默认值 1e-12」属禁止项，须变更流程）；
- 语义：`var := max(var, floor)` **仅对有效（可用）方差生效**；「不可用」一律以 `ivar=0` 表达，**不得**经由 clamp 产生（否则就是把「无数据」伪装成小方差，违 §30.1 禁伪方差）；
- 可审计：生效 floor（含是否使用默认值、逐 model floor 注册的 key 语义）必须写入帧产品/manifest；门：有效像素 `variance≥floor` 且 `ivar` 有限、不可用像素 `ivar==0` 且 `variance==0`。

---

## 5 文档改动清单（改前原文 → 建议改后原文 → 依据；交前台按 §3 落地）

### 5.1 docs/contracts/DATA_SEMANTICS.md §4a:45-50（消解注记 → 裁决记录）

- **改前**（逐字，:45-50）：
  > **无覆盖值歧义消解（DATA-UNC-001 登记，2026-09-09）**：本节 「无覆盖像素=0 / ivar=0」 与 §12.4 P1 写侧 「variance/ivar=NaN」 存在预存双值（finding F-UNC-001，归 Phase1 域任务消歧，本文不在此处单方改写）；Phase2 合成与 Phase3 传播**输出产品**的无覆盖/无效语义以 §30 为唯一权威（NaN 同态，§30.1/§30.4）；ivar=0 仅保留为 Phase2 读侧对逐帧 ivar 产品的合法零权重消费态（§20.1/§21）。
- **建议改后**：
  > **无覆盖值语义（F-UNC-001 已裁决，2026-09-17）**：Phase1 逐帧 variance/ivar 产品的**无覆盖/无方差信息**像素写 `variance=0` 且 `ivar=0`（显式不可用，禁止伪装）；**NaN/负只表示产品损坏**（写侧遇非有限/负输入 ⇒ rc=−6 硬失败；读侧 ⇒ hard fail）；全无信息 tile ⇒ rc=−5 不落盘（§12.2/§12.4）。Phase2 合成与 Phase3 传播**输出产品**的无覆盖/无效语义仍以 §30 为唯一权威（NaN 同态）；`ivar=0` 在 Phase2/Phase3 读侧为合法零权重（§20.1/§21/§30.4），且读侧必须先过资格门（support>0 ∧ finite signal）再读权重，禁止 1/0→Inf。
- **依据**：本文 R1/R2/R3/R4；§4a:38-40；§30.2:2409；§30.4:2515-2521。

### 5.2 §12.1:349（`var_num_sum` 行 invalid 列）

- **改前**：「variance/ivar 产品时强制非 NULL（缺失 rc=−2 :575-576）；**≤0/非有限 → 该像素 variance/ivar=NaN**」
- **建议改后**：「variance/ivar 产品时强制非 NULL（缺失 rc=−2 :575-576）；**`≤0` 或 `covered_area≤0` → 该像素 variance=0、ivar=0（无信息，§4a）；非有限/负 → 写请求 rc=−6 硬失败（上游损坏，禁写伪产品/禁 clamp）**」
- **依据**：R1/R3；§4a:38-40。

### 5.3 §12.2:358-359（variance/ivar 输出行 invalid 列）

- **改前**：variance「无效或 area·vnum≤0/非有限 → **NaN**（:615-621）；tile 全无效 → 写请求 rc=−5 不落盘（:629-632）」；ivar「同 variance（NaN 对应）」
- **建议改后**：variance「**无信息（`area≤0` 或 `vnum≤0`）→ 0.0**；**非有限/负 → rc=−6 硬失败**；tile 全无信息 → rc=−5 不落盘（不变）」；ivar「**无信息 → 0.0**（=1/variance 的显式不可用态，禁 1/0→Inf）；非有限/负 → rc=−6」。
- **依据**：R1/R2/R3；§4a:39；§30.4:2519-2521。

### 5.4 §12.4:391-396（invalid 规则汇总）

- **改前**：「…variance/ivar 额外要求 `vnum>0 && isfinite(vnum)`（:617-621）。违反 → signal=NaN、support=0.0、**variance/ivar=NaN**。IEEE NaN 填充，不使用 FITS BLANK。」
- **建议改后**：「…variance/ivar 的**可用**判定额外要求 `vnum>0 && isfinite(vnum) && area>0`；**不可用（无信息）→ variance=0.0、ivar=0.0**；**损坏（`vnum`/`area` 非有限或 `vnum<0`）→ 写请求 rc=−6 硬失败（禁写、禁 clamp）**。signal 仍用 IEEE NaN 填充（不使用 FITS BLANK）；三产品在无覆盖像素互补编码：signal=NaN、support=0.0、variance=0.0/ivar=0.0。」
- **依据**：R1/R3/R5；FITS 浮点数组无 BLANK（外部标准 2.1）。

### 5.5 读侧条款补强（不改判据强度）

- §20.1:1051 行注记追加：「读侧顺序固定为**先资格门（support>0 ∧ finite signal）后权重面**；`ivar==0` 合法零权重；non-finite/负 hard fail（§30.1）」；
- §30.4:2507-2512 追加一句：「variance 优先分支中，`variance==0`（P1 写侧无信息）⇒ u 记为**不可用（NaN 传播态）**，禁止读作零不确定度；ivar 分支的 `ivar==0` 同义」；
- **依据**：R4；§30.4:2515-2521。

### 5.6 docs/science/NOISE_MODEL.md §7/§8（floor 与不可用）

- **建议追加**（不改变既有数值与公式）：「`variance_floor` 的单位为 **ADU²**，默认 1e-12（本值属冻结项，变更须走工程变更）；clamp `max(var,floor)` **只作用于可用方差**；**不可用一律 `ivar=0`（不得由 clamp 产生）**；每帧生效 floor 与其来源（默认/注册表 key）必须随帧产品登记，并满足不变量：可用像素 `variance≥floor` 且 `ivar` 有限，不可用像素 `variance==0 ∧ ivar==0`。」
- **依据**：本文 ③；NOISE_MODEL:21/:46/:105-107/:117/:125/:135/:142（逐字已在 §1 引）。

---

## 6 代码改动清单（随 FUP 批次；本文不改码）

| 位置 | 改动 | 依据 |
|---|---|---|
| aio_hips_writer.cpp:847-853（variance 叶写） | `var/iv` 初值由 NaN 改为 **0.0**；仅当 `area>0 ∧ vnum>0 ∧ finite` 时算 `vnum/area²`；**非有限/负 → 返回 rc=−6**（新错误码，:800-865 分支内） | R1/R3 |
| aio_hips_writer.cpp:856-859（`var_n` 归约量） | 无信息像素 `var_n=0`（现状已是），**损坏像素不得入归约**（返回 −6 前不写） | R3 |
| aio_hips_writer.cpp:862-865（all-invalid） | **不变**（rc=−5） | R2 |
| astro_sphere_sink.cpp（rc 处理） | −2/−5 维持「跳过+计数」；**新增 −6 ⇒ 不跳过**（默认 abort 路径）+ 明确诊断 | R3 |
| module_adapters.cpp:4631-4656（P2 权重面） | 判据不变；新增 `ivar_zero_weight_pixels` / `variance_zero_pixels` 审计计数并落 p2_integrated.json + manifest | R4 |
| 文档 | §5 清单 | —— |

---

## 7 可复跑判据（正/负臂；命令与期望）

前置：`build/astrocs` 当前二进制；夹具沿用 run/PROJECT-GOVERNANCE-01/IVAR-001/cfg 与 tests/backend/phase2_fixture_main.cpp 的生成器（`--make` / `--make-noivar` / `--make-nan` / `--make-seam`）。

| 门 | 命令（要点） | 期望 |
|---|---|---|
| P1 无信息像素 = 0/0 | 用 `aio_hips_product_begin` + `aio_hips_write_variance_tile` 写一个「部分覆盖」tile（部分像素 area=0） | tile 落盘；无覆盖像素 `variance==0 ∧ ivar==0`；有覆盖且 vnum>0 像素 `variance=vnum/area²`；`any_valid` 判定不变 |
| P1 全无信息 tile | 同上但全像素 area=0 | 写请求 **rc=−5**，tile **不落盘**；normalize 侧计数且不 abort |
| P1 损坏输入 | `var_num_sum` 注入 NaN 或负值 | 写请求 **rc=−6**，run **abort**（禁写产品/禁 clamp/禁 skip）；诊断含 tile+像素数 |
| P2 零权重 | 含「有覆盖但 ivar=0」像素的帧 + 默认 weight_mode=2 | **rc=0**；该像素权重 0（不贡献）；`ivar_zero_weight_pixels>0` 入 manifest |
| P2 损坏仍 hard fail | ivar tile 注入 NaN/负（`--make-nan` 语义） | **rc≠0** hard fail（§30.2:2409 保留），诊断含 frame/tile/pixel |
| P2 缺产品仍 fail-closed | `--make-noivar` + 默认 mode 2 | **rc=2**（IVAR-001 已取证） |
| P3 零方差≠零不确定度 | 含无覆盖像素的 P2 产物 → export | 输出该像素 `u=NaN`（传播态），**不得**为 0；`1/0→Inf` 不出现 |
| 值语义注入（双向可假） | 新增 `ASTROCS_IVAR_FAULT=nan_for_uncovered`（把 0/0 写回 NaN） | 上述「P1 无信息像素 = 0/0」门与「P2 零权重」门必须**判红** |

**现状（裁决前）可观察项（如实）**：目前写侧对「有覆盖但 vnum≤0」写 NaN，读侧在资格门通过后会 hard fail（module_adapters.cpp:4649-4656，诊断文案为「non-finite/negative input ivar … p2_validate_candidate_weights hard fail」）——即把「该像素无方差信息」报成「产品损坏」。本裁决将其改为**零权重 + 审计计数**（runnable 证据链将在 FUP 实施批次补齐，含上表全部红绿臂）。

---

## 8 影响面与回退

- **契约面**：§4a/§12.1/§12.2/§12.4/§20.1/§30.4/NOISE_MODEL 七处文本改动；**不改任何公式**，不改 Phase2/Phase3 **输出**产品的 NaN 同态语义（§30.1/§30.2/§30.4 冻结部分不动）。
- **产品面**：Phase1 variance/ivar 产品的无效填充由 NaN 改 0（该产品在默认链路上尚未产出，IVAR-001-FUP 前无存量产品受影响）；对**已存在的合成夹具**需同步其断言（fixture 注释 :74「masked(area=0)像素由 writer 规则自然取 NaN」需改为 0）。
- **读侧**：P2 判据强度不变（非有限/负仍 hard fail），新增审计计数；P3 读侧条款补一句（variance==0 ⇒ 不可用）。
- **回退**：本裁决是语义收敛；若审查否决 0 方案而取 NaN，则必须同批改 §4a:38-40 与 §30.4:2507-2521（读侧零权重/禁 1/0 条款），并新增「资格门必须先于权重面」的强制条款与门——即**代价更高且需改动读侧权威**，这是本裁决不取 NaN 的工程理由之一。

## 9 未决与风险

- **风险 1**：`variance=0` 对**不看 support 的第三方消费者**可能被误读为零不确定度。缓解：R4 强制资格门 + 产品文档（§12.4 改后文）显式写明三产品互补编码；FUP 的门覆盖该消费者场景（P3 门）。
- **风险 2**：rc=−6 是**新错误码**，需在 §12 错误码表与 sink 侧登记（含 E2E 诊断映射）；若登记遗漏会造成「未知码被当作可跳过」。缓解：§6 清单与 §7 门同批交付。
- **未决（不影响本裁决落地）**：生效 floor 的「逐 model 注册表 key」是否需要在帧产品里逐 key 记录（现为指针 key 隔离语义，NOISE_MODEL:61）——留待 FUP 实施时按最小充分原则定；本裁决只要求「生效值 + 来源类别」可读。

---

附：取证与条款引用同源文件 run/PROJECT-GOVERNANCE-01/IVAR-001/自证摘要.md（§1 契约依据 / §2 取证）；本裁决的前置件 ECP-001 见同目录 ECP-001-方差绑定.md。
