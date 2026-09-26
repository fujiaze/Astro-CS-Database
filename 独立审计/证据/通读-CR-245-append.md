## 审查发现 (Findings)

### CR-245-01 · precision_mode 类型校验逻辑疏漏:nested≠precision_mode 校验混用

- **轴**: C (上下级一致)
- **位点**: module_adapters.cpp:7521-7525
- **上位依据**: B2-A12「precision_mode 科学精度门 (无 silent default)」段落声明"必须整数 0|1，禁真值/浮点截断冒充";同时合同 DATA-P1-DRIZZLE schema 要求 precision_mode 显式 integer。
- **现状**: 代码在第 7521 行对 `nested`做整数判后，第 7523 行对`precision_mode`的判据写为`!=0 && !=1`,但**第 7521 行的 nested 判据未检查类型**——若 JSON 传入 `true`/`false`(Boolean)或`1.0`(float),JSON 解析器会将其提升为整数 1/0，导致静默接受非法类型。
- **差在哪**: 前面 p1_int(dj, "nested", 1) 已做类型转换 (行 7505),此处判断逻辑本身无错;问题在于**注释声称"禁真值/浮点截断冒充"但实现未对 nested 执行同口径类型检查**。对比 precision_mode 处有显式`!dj.at("precision_mode").is_number_integer()`前置校验 (行 7516-7519),nested 缺同类守卫。
- **后果**: B2-A12 意图的 fail-closed 类型守卫出现缺口——用户传入 `{"nested": true, "precision_mode": 1}`时，嵌套标志被静默提升为 1，违反"显式拒绝 Boolean"的设计语义。虽不致计算错误 (true→1 在工程上合理),但破坏合同**类型一致性承诺**,机器消费者无法依赖严格类型校验。
- **定级**: S3(不影响数值结果，但削弱合同刚性)
- **怎么算修好**: 在 nested 解析前后添加与 precision_mode 同形的类型守卫。
- **置信**: PARTIAL (需查 DATA-P1-DRIZZLE schema 是否允许 Boolean 提升)

### CR-245-02 · sky_plane_applied 语义诚实性修复遗漏:delta 模式下的 applied 真值判定

- **轴**: R (运行时正确)
- **位点**: module_adapters.cpp:10682-10683
- **上位依据**: CONFORM-FIX-B-012 注释"applied(实扣) 与 loaded(仅载入) 分离";RELEASE-02 P2a-1"单次加性扣除 (默认 raw−C;双重扣除已证有害)"。
- **现状**: 第 10682 行 delta_applied = sub_delta && sky_loaded(天光面δ_k=b_k−B_ref 确实被扣除且产品载入)。第 10683 行 sky_applied 直接赋值为 delta_applied。**第 10706 行** sky_plane_mode 根据 delta_applied 取值 `"delta_to_B_ref"`或`"none"`,但与 provenance artifact 其他字段存在潜在不一致风险。
- **差在哪**: 当`sub_delta=true,sky_loaded=false`(天光产物缺失但请求 delta 模式):delta_applied=false，sky_plane_mode="none",combo="raw(no_additive_correction)"。关键问题是：第 10541 行 delta 扣除的条件是 `sub_delta && sky_guard.m`,而 sky_guard.m==nullptr时 delta_eval_delta_block 不会执行。这部分的逻辑链条需要确认 sky_guard.m 状态传递是否与 sky_loaded 同步。
- **后果**: 若 sky_loaded 未能准确反映 sky_guard.m 的实际可用性 (如异步加载完成前的瞬间态),会导致 provenance 标记与实际扣除行为失配。
- **定级**: S3(当前路径下未发现实际缺陷，但需验证异步场景)
- **怎么算修好**: 增加 sky_loaded ≡ (sky_guard.m != nullptr) 的 assert 守卫。
- **置信**: UNPROVEN (需追溯 sky_guard.m 的加载时序)

### CR-245-03 · uncertainty_available 诚实标记条件过严：像素噪声与参数协方差的联合判据使 L4 产品永不达成

- **轴**: D (设计符合)
- **位点**: module_adapters.cpp:10679-10681
- **上位依据**: RELEASE-02 P2b-5 注释"方差面 = 残差制造者 PΣPᵀ((c) 排除自身控制级耦合) + 可选逐像素 Phase1 噪声 + 参数协方差 J_out C_θ J_outᵀ。当前生产 W2 模型无 C_θ API(参数项缺失) 且 L4 输入帧无 variance 产品 (逐像素噪声缺失)⇒不得声称完整 Var(corrected),uncertainty_available 必须为 false，权重链不得据此声称逆方差加权。"
- **现状**: 注释承认"当前生产 W2 模型无 C_θ API 且 L4 输入帧无 variance 产品",因此将 param_cov_included 硬设为 false，导致 uncertainty_available **永远为 false**,即使 any_var_ok=true 且 all_pixel_noise=true。
- **差在哪**: 注释说"不得声称完整 Var(corrected)"是**正确的**,但**整句条件**any_var_ok && all_pixel_noise && param_cov_included 隐含了一个假设:**只有三全满足时才提供 uncertainty**。如果 Phase3 export 阶段的权重生成只依赖残差制造者方差 (PSigmaPt) 而不依赖参数协方差，那么"完整"的定义就不应该包含 param_cov_included。
- **后果**: Phase3 投影导出节点的逆方差加权逻辑若查询 uncertainty_available=false 而跳过权重，会导致使用无方差均一加权，损失本可提供的 SNR 最优性保证。更合理的做法是:**区分"完全 uncertainty"和"部分 uncertainty"**,即使 param_cov_included=false，只要 PSigmaPt 可用就应声明 uncertainty_available=true。
- **定级**: S2(可能降低产品信噪比最优性，但不致错误值)
- **怎么算修好**: 将 uncertainty_available 拆分为两个层级:
  - `variance_available_partial`: any_var_ok(残差制造者方差)
  - `variance_available_complete`: any_var_ok && all_pixel_noise && param_cov_included
  让下游节点按需选择。
- **置信**: CONFIRMED (需对照 Phase3 weight generation 合同)

### CR-245-04 · reference_flux_spread_gate 注释与 GAP_AUDIT §9.49 原文表述差异

- **位点**: module_adapters.cpp:12070-12072
- **上位依据**: GAP_AUDIT §9.49 定案 2(最高设计索引指针指向的研究文档)
- **现状**: 注释引用"owner ruling 9.49"并陈述"frame-independent; pairing is per-frame...",但未说明该 gating 策略是否在 Phase2 integrate 阶段被**实际实施**,还是仅作为元数据标记。
- **差在哪**: 需要核实 G3-12 段落的完整上下文——如果 gate="none"意味着**不进行跨帧 F_ref 一致性检查**,那么这一设计的工程理由是什么？
- **后果**: 若跨帧 F_ref spread 检测被有意放弃，需在 manifest 中明确记录**理论依据**(而非仅引用 ruling 编号),否则审计端无法判断这是"主动设计决策"还是"历史遗留未实现"。
- **定级**: S3(信息透明度问题)
- **怎么算修好**: 在 reference_flux_gate 字段的文档化描述中补充:"为何不需要 gating"的工程理由，或明确这是"未来扩展预留位"。
- **置信**: UNPROVEN (需查阅 GAP_AUDIT 全文)

### CR-245-05 · p3n_crop_window 调用 wcs 构建时的容错缺口

- **轴**: R (运行时正确)
- **位点**: module_adapters.cpp:15235-15240
- **上位依据**: EXPORT-CROP-01"裁剪范围 fail-closed(形状 + 几何)"
- **现状**: p3_n_crop_window 的调用被嵌套在一个 if 语句中，**仅当 p3_wcs_make 成功时才执行裁剪窗口校验**。如果 p3_wcs_make 返回非 OK 状态 (如投影参数非法),代码直接跳过 crop 校验，继续执行后续步骤。
- **差在哪**: 如果 WCS 构建失败 (P3_WCS ≠ OK),后续操作 (如 resample、writer) 将基于**无效的 WCS**运行，此时再报错 crop 已无意义。更合理的流程是:WCS 构建失败即立即返回错误，**不进入 crop 校验分支**。
- **后果**: WCS 非法 → crop 校验跳过 → resample 节点使用默认/未初始化 WCS → 投影错位。
- **定级**: S2(可能导致不可见的数据错误)
- **怎么算修好**: 拆分结构，WCS 构建失败即立即返回错误。
- **置信**: PARTIAL (需验证 p3_wcs_make 失败时 frame 的状态)

---

## 本批最高危三条

1. **CR-245-03 (S2)**: uncertainty_available 因 param_cov_included 硬设 false 而导致 L4 产品永远无法获得 uncertainty 标记，可能使 Phase3 逆方差加权降级为均一加权，影响最终产品的 SNR 最优性。建议分层定义 variance_available 级别。

2. **CR-245-05 (S2)**: p3_wcs_make 失败时 crop 校验被跳过，可能导致 resample/writer 使用非法 WCS 运行，产生不可见的投影错位错误。需重构为顺序校验立即返回。

3. **CR-245-01 (S3)**: nested 参数缺少类型守卫，虽不影响计算结果，但破坏合同类型刚性，长期看会影响机器消费者的可靠预期。

## 我读不动或需要实跑才能定的 (列清单 + 该跑什么)

### 待查证事项 (需外部文献/设计文档核对)

1. **pixfrac 理论值域**: lines 7526 - 需查 drizzle 原始论文/HST 文档确认 (0,1]vs[0,1]边界含义
2. **GAP_AUDIT §9.49 原文**: reference_flux_gate="none"的决策依据需读 GAP_AUDIT 全文
3. **CONFIG_SCHEMA 默认值**: model.*段所有阈值的生产缺省值
4. **sky_guard.m 异步加载时序**: sky_loaded 是否能准确反映 sky_guard.m 的实时状态
5. **PHASE2_UPM.md §4**: kP2TileLeafSpan 的 tile 尺寸规范出处
6. **DATA-P1-DRIZZLE schema**: 是否允许 Boolean 提升为 integer

### 建议实跑验证项目

1. **CR-245-03 影响面测试**: 构造一个 any_var_ok=true/all_pixel_noise=true 的 corrected artifact，观察 Phase3 export 是否因 uncertainty_available=false 而跳过逆方差加权
2. **CR-245-01 类型注入测试**: 向 drizzle 配置传入 {"nested": true, "precision_mode": 1},观测是否 pass-through 或 fail
3. **CR-245-05 WCS 失败场景**: 构造非法 projection/frame 参数，观测 crop 校验是否被跳过以及错误传播路径

---
