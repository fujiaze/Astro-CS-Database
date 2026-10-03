# 审稿-P1 · ALG-resample-001（第 1 遍 · 对抗性重读）

- 片号：`ALG-resample-001`
- 层：`lib/algorithms/resample`
- 基线：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 审稿人姿态：默认现行结论为错；一切结论由本人读完原文 + 独立复算得出。
- 口径声明：本片**不以任何既有「检查通过」为正确性依据**。凡判据，均重新推导并构造反例。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **16** |
| 实际读完份数 | **16** |
| 成员总行数（权威清单「实际行数」） | **3704** |
| 实际读走行数 | **3704** |
| 覆盖率 | **16/16 份 = 100%；3704/3704 行 = 100%** |
| 未读完 | **无** |

口径说明：行数按 `wc -l`（换行符计数）逐文件实测，与 `片清单-权威版.yaml` 的「实际行数: 3704」逐份求和一致（582+501+437+294+256+251+248+209+202+151+144+144+113+68+60+44 = 3704）。覆盖率为**物理全文覆盖率**，不含任何跳读、抽样或「仅看 diff」。

---

## 2. 本片判定

### 判定：**阻断**

最重 3 条：

**BLK-1 — 本片 53% 的生产代码不在产品路径上；合同声明的「唯一方差来源」在出厂二进制中不存在。**
本人亲验：`CMakeLists.txt:1280-1281` 的 `add_library(acsd_phase3_session STATIC …)` 只含 `lib/phase3_session/p3_session.cpp`；`p3_export.cpp` 全仓只出现在 `eng/tests/integration/p3_export/CMakeLists.txt:37`（一个测试可执行）。而 `p3_session.cpp:291,305` 走的是 `p3_resample.cpp` 的**对角** `Σc_k²u_k`。即：片内 7 个生产 `.cpp` 中的 6 个（operator/covariance/propagation/kernel_registry/failclosed/units 中的 5 个，共 1409+256+251+248 = 2164 行，占片内 58%）中，除 `p3_rsmp_units.cpp` 外全部只被测试链接。仓内正本已承认：`docs/science/PHASE3_HIPS_TO_FITS.md:215`「`lib/phase3_session/p3_export.cpp` 未进构建」。
唯一例外（本人核实）：`p3_rsmp_units.cpp` 的 `parse_bunit_string`/`resolve_bunit` 确实进产品闭包，由 `module_adapters.cpp` 的 `p3n_guard_input_units` 调用（同处 :215 记载）。
⇒ `FZ-FORMULA-COV-PROP`「`C_y=R C_x Rᵀ` 是唯一方差来源，禁任何权重标量反推」在出厂件中**为假**；`G-P3-COV-00/01/02/03`、`G-P3-QW-*`、`G-P3-SB-00` 一族门**在生产中永远不会触发**。

**BLK-2 — 本模块的 NaN 掩膜 Oracle 是自愈锚：它的核心断言结构上不可能失败，正好掩盖了它本该抓住的缺陷。**
`tests/p3rsmp/p3_nan_mask_test.cpp:287` `double ww[4] = {0,0,0,0};` → `:290` 调 `p3_sample_bilinear_nanmask_ex` → `:326` `CHECK_MSG(ww[k] == 0.0, "rejected weight must be exactly 0")`。
被检函数 `p3_resample.cpp:443-449` 对被剔除样本执行 `if (!ok[k]) continue;`，**从不写 `weights[k]`**。于是 `:326` 拿测试自己的初始值和自己比 —— **操作数逐位相同**。这条断言对任何实现都通过，包括会输出垃圾权重的实现。
它守的正是 `p3_resample.h:141` 明文承诺的后置条件「被剔除样本**恰为 0.0**」。

**BLK-3 — `Σw=1` 是恒等式，而 DISP-P3RSMP-001 的整个处置建立在这个恒等式之上。**
本人盲复算（见 §5 CE-1）：`wg[0..3] = {(1-u)(1-v), u(1-v), (1-u)v, uv}`，展开恒等于 1，**对任意实数 u、v 成立**（含 u、v 落在 [0,1] 之外）。重归一 `eff_k = w_k/Σ(合格 w_j)` 又使 `Σeff ≡ 1`。故：
- `p3_resample.cpp:414`「G4 几何权重 (Σ=1±k·ULP)」、`README.md:104`/`module.yaml:140`「Σw=1 不变量一致」、`p3_rsmp.h:139,183`「Σc_k=1 不变量保持」——**全部是代数恒真式，不含任何关于几何正确性的信息**。
- `p3_resample.cpp:411-412` 把 `u,v` 静默 clamp 到 [0,1]（几何失效被吸收），clamp 后恒等式**依然成立** ⇒ 恒等式在原理上无法检出 clamp。
- 于是「bilinear 四象限最近中心」与合同要求的「面积重叠分数」之间**没有任何可证伪的判据**；README §6 对 DISP-P3RSMP-001 的结案（「同族一阶插值、Σw=1 不变量一致，离散化方案不同」）因此是**空结案**。

---

## 3. 逐文件清单

| # | 文件 | 行 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|---|
| 1 | `p3_resample.cpp` | 582 | 全文 | 共享 LRU + 负缓存 tile 取数；切平面四象限双线性；样本级掩膜 + 重归一；不确定度传播 | **需修**（多阻断，见 BLK-2/4/5/6/7） |
| 2 | `p3_resample.h` | 202 | 全文 | 公共契约：`P3SampleRejection` 三分类、权重语义、不确定度三态 | **需修**（`:141` 承诺与实现不符；`:3` 与 `:23-27` 自相矛盾；`:66` 与实现不符） |
| 3 | `tests/p3rsmp/p3_nan_mask_test.cpp` | 437 | 全文 | T1–T7；fixture；解析 oracle；非退化锚 | **阻断**（BLK-2 自愈锚；`oracle_value` 与生产逐行同构） |
| 4 | `p3_rsmp_operator.cpp` | 294 | 全文 | R/S 装配、跨 tile 邻域、`apply_operator[_transpose]` | **阻断**（守恒是构造的恒等式；`coverage` 数值臂恒绿；`tile_px=0` 除零） |
| 5 | `p3_rsmp_propagation.cpp` | 256 | 全文 | 三模式传播、输出帧 Q/W 重算 | **需修**（`Status::Ok` 可带 NaN/负方差；门输入为冻结表自身） |
| 6 | `p3_rsmp_kernel_registry.cpp` | 251 | 全文 | 5 核 registry、`validate_registration`、`admit` | **阻断**（BLK-4 注册了不存在的核；Oracle 证据为凭空常量） |
| 7 | `p3_rsmp_failclosed.cpp` | 248 | 全文 | 12 门 + 扩展门、禁止词表 | **阻断**（`forbidden_psfsw_product_keys` 零消费；词表大小写敏感） |
| 8 | `p3_rsmp_units.cpp` | 209 | 全文 | BUNIT 代数、冻结表、串解析、`resolve_bunit` | **需修**（`std::stoi` 部分解析放行尾随垃圾；`is_production_mode` 恒真） |
| 9 | `p3_rsmp.h` | 501 | 全文 | 全部类型与门接口 | **需修**（多处自证默认；`G-P3-KRN-04` 不存在；ID 拼写错误） |
| 10 | `p3_rsmp_covariance.cpp` | 151 | 全文 | `C_y=R C_x Rᵀ`、相关核、Cholesky | **阻断**（`max_abs_offdiag_correlation` 滤掉 NaN 再取极值；`valid_out` 从不参与） |
| 11 | `memory.md` | 144 | 全文 | P3-RSMP-DOC 冻结记忆 | **需修**（大面积失真：行数 58/239 vs 实际 202/582；锚点越界；证据目录不存在） |
| 12 | `module.yaml` | 144 | 全文 | 模块 manifest | **需修**（`known_defects` 收入已失效项；`source_symbols` 收缩口径与实际调用图不符） |
| 13 | `README.md` | 113 | 全文 | 合同链、生产源、DISP 登记 | **需修**（:7-9 与 :39,51 自相矛盾；:34 悬空路径；:111 自认 `missing_tiles` 恒 nullptr） |
| 14 | `tests/p3rsmp/p3_resample_probe_main.cpp` | 68 | 全文 | 6 模式探针 | **需修**（`:1` 自述路径为已删的 `eng/tests/backend/…`；`:58-61` 硬编码 nside=512 与位移 18） |
| 15 | `PHASE3_RSMP_IMPL.md` | 60 | 全文 | IMPL-P3-RSMP-001 生产面与冻结节点对应 | **需修**（`:26` 声明的生产默认核不存在；`:41` 的 20 条 mutation 未在本片） |
| 16 | `CMakeLists.txt` | 44 | 全文 | `acsd_p3_rsmp` target 清单 | 通过**（本片唯一无缺陷文件；但它编译的 6 个 TU 见 BLK-1） |

---

## 4. 发现清单

### 4.1 阻断（11 条）

| ID | 位置 | 问题 |
|---|---|---|
| **BLK-1** | `CMakeLists.txt:19-30` + `CMakeLists.txt:1280-1281` | 6/7 生产 TU 不进产品图；`C_y=R C_x Rᵀ` 不是出厂方差来源；一族门生产永不发火 |
| **BLK-2** | `tests/p3rsmp/p3_nan_mask_test.cpp:287,290,326` | **自愈锚**：断言操作数是测试自己的初始值，逐位相同，结构上不可能失败 |
| **BLK-3** | `p3_resample.cpp:413-414`；`README.md:104`；`module.yaml:140`；`p3_rsmp.h:139,183` | `Σw=1` 是代数恒等式（含 clamp 后），DISP-P3RSMP-001 据此结案为**空结案** |
| **BLK-4** | `p3_rsmp_kernel_registry.cpp:202-228`；`p3_rsmp.h:168`；`PHASE3_RSMP_IMPL.md:26` | `bilinear_area_overlap_exact` 注册为 `production_science_default`、误差界写「0」，但**全仓无实现**，且不在正本与合同 schema 中（本人验：`docs/`+`eng/contracts/` grep 零命中；schema family 枚举只有 `nearest/bilinear_4quad/higher_order`） |
| **BLK-5** | `p3_rsmp_kernel_registry.cpp:190-194,216-222`；`p3_rsmp.h:126` | 「独立 Oracle」证据是凭空常量：`ALG-P3-001-KERNEL-ORACLE/*` 全仓只有 3 个字符串赋值点，无实现、无推导；`max_interp_err=0.027395522883651657`、`err_over_bound=0.9862388238114597`、`zero_fill_error=1.3223` 全无来源。生产默认那一支 `0.0<=0.0` 恒真 ⇒ **误差界门对生产核永不失败** |
| **BLK-6** | `p3_rsmp_operator.cpp:41-47,118-133,154-171` | `full_coverage()` 数值臂恒绿（`area ≡ Ω'`）；`area` 在过滤**之前**求和 ⇒ `coverage` 可报 1.0 而真实行和 <1；零面积格仍被标 `valid_out=true` ⇒ 下游 `y=0, variance=0, Status::Ok` |
| **BLK-7** | `p3_resample.cpp:163-172` + `README.md:111-112` | tile 读失败与「tile 不存在」混同 → `coverage=0` 静默空洞 + **永久负缓存**；`last_err`（`:164`,`:502`）**写了从不读**（`P3SamplerImpl` TU 私有，头无访问器）；`provenance.missing_tiles` 恒 nullptr ⇒ **全链路无任何上报路径** |
| **BLK-8** | `p3_resample.cpp:443-449` vs `p3_resample.h:141` | 被剔除样本权重**从不写 0**；`p3_uncertainty_propagate:563` 靠 `w==0.0` 掩膜 ⇒ 正确性依赖调用方零初始化这一**未文档化**约定（本人验：两个生产调用方恰好都 `= {0,0,0,0}` 逐像素重置，故当前输出正确——正因如此缺陷存活至今） |
| **BLK-9** | `p3_resample.cpp:228-233` vs `p3_resample.h:55-56` | `attach_cache` 零强制头文明令禁止的 signal↔variance 共享；缓存键是无判别位的裸 `tip`（`:148`）⇒ 误接后逐像素返回**另一子产品的值**，且 `P3_RS_OK`、`coverage=1` |
| **BLK-10** | `p3_rsmp_failclosed.cpp:123-128` + `p3_rsmp_propagation.cpp:54-59` | **G-P3-SB-03 是自洽式断言**：`rec.signal/variance/ivar_bunit` 全是硬编码冻结表常量，`bunit_quadratic_law_ok` 又是后两项的合取（首项重复）；`SurfaceBrightnessInput`（`p3_rsmp.h:325-341`）**根本没有 BUNIT 字段** ⇒ 该门在生产中恒绿，从未检视真实产品单位。且 `is_inverse_pair` 只比整数指数，**数值上 `ivar=1/variance` 从未被验证** |
| **BLK-11** | `p3_rsmp_failclosed.cpp:42-50`；`p3_rsmp_covariance.cpp:78-90` | ①`forbidden_psfsw_product_keys()` 15 键拒绝面**无任何门消费**，唯一测试只断言「非空」；②`max_abs_offdiag_correlation` 先 `if(isnan(v)) continue;` 再取极值 ⇒ 全 NaN 矩阵返回 **0.0 =「无相关」**，实为「未定义」，**筛掉的恰是最差那条** |
| **BLK-12** | `memory.md:120-122`；`tests/p3rsmp/p3_nan_mask_test.cpp:2` | 三件套宣称本测试含「**负例**…含『实现回退 any_nan→NaN 即判红』**实证**, run/RSMP-NANMASK-01/evidence/**」。本人通读全 437 行：**无任何 mutation/injection/`#if 0`/环境变量门控的负例实现**，只有两个静态非退化锚（`:336-343`、`:391-393`，二者非空转且真实有效）。且 `run/RSMP-NANMASK-01/` 本人实测**不存在**（`ls` 无此目录，全仓该串仅 `memory.md:122` 一处命中）。⇒ **声称的负向证据为零实现 + 悬空产物路径** |
| **BLK-13** | `p3_rsmp_kernel_registry.cpp:89-106,190-196,216-224` ← `eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h:180,200-203` | **注册证据三项全是结构常量，无一由生产代码算出。** 本人实测：①`p3_rsmp_oracle.h:200-203` 把 `w00=w10=w01=w11` **硬编码为 0.25**（而生产 `p3_rsmp_operator.cpp:246-249` 是 `(1-fx)(1-fy)` 等）；②该 Oracle **从不调用** `make_bilinear_4quad_neighborhood`（`grep -c` = **0**），即「独立 Oracle」不碰被审对象；③故 `mdev ≡ \|(0.25×4) − 1.0\| ≡ 0`，`max_weight_sum_dev` **恒等于 0**，并被直接抄成 registry 的 `:195` 证据；④`boundary_fail_closed` 是 `oracle.h:180` 的**硬编码 `= true`** 初始化器，是注册门 `kernel_registry.cpp:103-106` 的**唯一输入**，registry 三处（`:168/:196/:224`）同样直写 `true`。⇒ **「独立 Oracle + 误差界 + 边界 fail-closed」三项注册前置全部自证；对权重公式的任何变异都保持全绿**。这与 BLK-3 的恒等式是同一病灶在 Oracle 侧的显影 |

### 4.2 须修（18 条，摘要）

| ID | 位置 | 问题 |
|---|---|---|
| M-01 | `p3_resample.cpp:488` | `path_exists` 失败（EACCES/EIO，非仅 ENOENT）与 `props_is_dir` 同走 `continue` ⇒ 落到 `:515 return P3_RS_OK` + `P3_UNC_NONE`，即「产品级谎称不确定度面不可用」，**正是 `p3_resample.h:124-126` 明文禁止的「不静默降级 NONE」** |
| M-02 | `p3_resample.cpp:66,103-107` vs `:44-47`/`:6-7`/`:50-51` | 负缓存 `absent` **无上界、从不逐出**；`put` 的逐出只管 `map`。头与模块自述「峰值内存有界 / 不随核数增长」**为假**。规模按 tile 数 `12·4^K`：K=8 ≈ 786k 条 ≈ 38 MB，K=10 ≈ 12.6M ≈ 600 MB，叠加在被封顶的 tile 缓存之上 |
| M-03 | `p3_resample.cpp:242` vs `p3_resample.h:66` | `resident_tiles` 注释「含已知缺失项」，实现取 `c->map.size()`，**不含** `absent` ⇒ 占用统计少算恰好一个 `absent_entries` |
| M-04 | `p3_resample.cpp:388-391,401-403` | 缺 tile 与无邻域两条早退**不写** `weights`/`leaf_ipix`；同函数 `:441` 的零合格分支却仔细清零 ⇒ 后置条件不一致 |
| M-05 | `p3_resample.cpp:566-578` | FP64 累加器 `acc` **不验有限性**；四个有限但巨大的 u 可溢出成 +Inf 并以 `P3_U_OK` 返回，违反 `p3_resample.h:193-194` |
| M-06 | `p3_resample.cpp:246-249`（经 `propagation`） | `W != 0.0` 对 NaN 为真 ⇒ NaN 方差以 `Status::Ok` 返回；`pi_cinv_pi<0` ⇒ **负方差**返回；`photometric_scale=+Inf` 可过 `failclosed.cpp:151-153`（`Inf>0` 且 `isnan` 假） |
| M-07 | `p3_rsmp_kernel_registry.cpp:99` | 误差界门无 `>=0`/有限性前置 ⇒ `max_interp_err=-1, bound=0` 通过（物理上不可能的负误差满足注册门） |
| M-08 | `p3_rsmp.h:318`；`failclosed.cpp:184-193` | `GateConfig::epsilon_corr` 生产**零读者**；把 `epsilon_corr_ratified=true` 一翻，近似相关面（哪怕 `correlation_approx_error=1e300`）全绿。OPEN 项 SO-07 无签字绑定、无量值比较 |
| M-09 | `p3_rsmp_propagation.cpp:74`；`failclosed.cpp:52-55` | 「禁止权重标量反推方差」靠 `in.variance_from == "weight"` 精确串比 + 大小写敏感全等词表 ⇒ `"Weight"`/`"composite_weight"`/`"MEDIAN_SNR"` 全过。同仓权威口径 `coverage.h:157` 明写「**大小写不敏感**」 |
| M-10 | `p3_rsmp.h:437,445,452,469,474` | 门输入默认取**许可值**且无 `*_declared` 伴随位：`bunit_quadratic_law_ok=true`、`provenance_complete=true`、`psf_sum=1.0`、`covariance=ExactFull`。只设 `mode_declared+mode` 的 `ProductRecord` 可过全部 SB 门 ⇒ 「未提供信息」与「合规」不可区分 |
| M-11 | `p3_rsmp_covariance.cpp:107,114` | Cholesky 无 `isinf` 前置、无对称性检查、无 L 有限性后检 ⇒ `+Inf` 主元可过并产出 Inf/NaN 的 L，`cholesky_solve` 仍返 `Status::Ok`。`isnan(sum)` 是死条件（`!(sum>0)` 已覆盖） |
| M-12 | `p3_rsmp_covariance.cpp:63,71` | 相关核自相矛盾：对角 `v<=0 → kNaN`（未定义），非对角 `den<=0 → 0.0`（**「完全不相关」**） |
| M-13 | `p3_rsmp_covariance.cpp:22-47` | `propagate_covariance` **从不读 `op.valid_out`** ⇒ 无效像素 `C_y` 行列全 0 ⇒ `matrix_diagonal` 给 0.0，`p3_export.cpp` 会在 `SIGNAL=NaN` 处写出 `VARIANCE=0.0`（方差 0 = 「无限精度」，是可能的最强断言） |
| M-14 | `p3_rsmp_units.cpp:131-134` | `std::stoi` 部分解析 + `catch(...)` 空捕获 ⇒ `"ADU^2abc"` 静默解析为幂次 2。同仓正典解析器 `bunit.cpp:51-53` 用 `strtol` + `*end=='\0'` 并限 ±64，本处更宽松 ⇒ 「量纲可判」门比正典还松 |
| M-15 | `p3_rsmp_operator.cpp:210-223,250-254` | `accumulate` **先**判越界再谈权重 ⇒ 权重恰为 0 的越界样本仍把整行打成 NaN ⇒ 足迹边缘产生伪空洞，且与真缺 tile 不可区分 |
| M-16 | `p3_rsmp_units.cpp:33` | `is_production_mode(P3Mode) { return true; }` 参数无名、**恒真**、违反 `p3_rsmp.h:52` 契约；且接受 `static_cast<P3Mode>(42)`。零调用者。`mode_token`/`is_retired_mode_token`/`is_deferred_weight_mode_token` 同为零调用死码 |
| M-17 | `README.md:7-9` vs `:39,51`；`module.yaml:2-3,42-47`；`memory.md:8-9,14,63,68,76` | **同目录三件套大面积失真**：三份都称生产源在 `lib/phase3_session/` 且本目录「仅合同文件、无源码」（`lib/phase3_session/` 实无 `p3_resample.*`；本目录有 7 生产 `.cpp`+2 `.h`）；行数三套互斥（memory.md 写 58/239，README 写 197/586，实测 202/582）；`memory.md:90-97` 的锚点 `:315-464` **越出其自己声明的 239 行** |
| M-18 | `README.md:34`；`module.yaml:12`；`memory.md:62,113`；`tests/p3rsmp/p3_resample_probe_main.cpp:1` | 四处指向 `eng/tests/backend/p3_resample_probe_main.cpp`（**不存在**，本人验）；真实文件在 `lib/algorithms/resample/tests/p3rsmp/` 且**未被任何 CMakeLists 引用** ⇒ README §2 声称的「现状执行证据」整条链不存在 |

### 4.3 建议（9 条）

- S-01 `p3_rsmp.h:168` 引用的 `G-P3-KRN-04` 全仓不存在；`ALG-P3-001_KERNEL_REGISTRY`、`C-P3-PROP-6`、`ALG-P3-007` 在 `docs/` 内零命中；`p3_rsmp.h:3` 的 `IMPL-P3-RSMP-001` 应为 `ALG-P3-RSMP-IMPL-001`。
- S-02 `p3_resample.h:44` 引用的 `ARCH-P3 §3` 无实体文档（仓内只有 `eng/cmake/ARCH-001-migration-manifest.md`）。
- S-03 `p3_resample.cpp:213-217`：`accumulate` 越界 + `tile_px` 为调用方可控公字段，`tile_px==0` → 整数除零。
- S-04 `p3_resample.cpp:409-412`：`u,v` clamp 无诊断；`:392-394` 象限退化填充可使双线性**静默退化为 nearest**（`u=v=0` → `wg={1,0,0,0}`），无标志位。
- S-05 `p3_resample.cpp:340-341`：`rejection` 在 PARAM 守卫**之前**清零 ⇒ 「0 次剔除」与「未采样」不可区分，违反 `p3_resample.h:162-163`。
- S-06 `p3_resample.cpp:346`：`healpix::neighbors` 每像素一次堆分配，模块 `:315-317` 已自认违反 `CODE_STANDARD.md` 的 MUST，属**已披露未修**。
- S-07 `p3_rsmp_units.cpp:58` 引用 `§12.2` 作单位依据（实为 HiPS 输出目录树）；`:61-64` 写 `"px"` 而正本 `DATA_SEMANTICS.md:3213` 明示 `px` 非合法单位串。
- S-08 `p3_rsmp_units.cpp:66` `px_power/2` 向零截断 ⇒ `Bunit{1,-1}.canonical()` 得 `"ADU"`，与 `{1,-2}` 同串 ⇒ canonical 非单射（解析往返不闭合）。
- S-09 两套冻结单位表的立体角编码差 2 倍（本片 `p3_rsmp_units.cpp:142` `px_power = 2*e` vs AIO `bunit.cpp:61` `px_power += e`）。**本人不断言这是现网 factor-2 缺陷**——两者是互不交换的独立类型；判为**潜伏陷阱**：一旦任一侧数值跨模块传递即出错。

### 4.4 明确核对为**干净**的项目（覆盖记录）

- **私建线程池：干净。** 全片零 `std::thread`/`std::async`/`hardware_concurrency`/`pthread_create`。`module.yaml:131` 的 `threading_model: host_executor_lease` 与实现相符。
- **`p3_resample.h` ↔ `p3_resample.cpp` 符号双向配平：零失配。**
- **ODR：无违规。** 7 个 TU 的外部链接定义逐一枚举，无重复、无缺失；5 个 `to_string` 重载分处 3 个 TU 但参数类型互异。
- **Cholesky 无伪逆、无 ridge、无正则化**——「禁伪逆静默」这句是诚实的；`WeightDerived` 被 `G-P3-COV-02` 硬拒。
- `p3_resample.cpp:317-324` 诚实自披露 per-pixel 堆分配未满足 MUST（属披露而非隐藏）。
- `p3_resample.cpp:1-3`「禁止第二套数学核心」在本片内自洽（叶级路径确实全部经 `healpix_core`）。
- **纠偏（防误报）**：`P3SampleRejection` 三分类中 `n_rejected_nonfinite_variance` / `n_rejected_nonpositive_weight` 恒 0 **是合同明文许可**（`DATA_SEMANTICS.md:3086-3088`），**不得**报成缺陷。真正的问题只在**不确定度面**：`p3_uncertainty_propagate`（`p3_resample.cpp:541-547`）签名无 `P3SampleRejection*` 出参 ⇒ 该字段在方差面全仓不可达（已登记为 UNRESOLVED，见 §7 降级 4）。
- **Q/W 输出帧重算本身是真的**：`propagation.cpp:223-249` 确用两次 `cholesky_solve(out.c_y, …)` 以完整 `C_y` 重算，全文件无 `trace`/`diag`/`sum(input)` 反推捷径。`frame_is_output_recompute = true` 名副其实。（但见 M-06：周边守卫有洞。）
- `p3_nan_mask_test.cpp:244-253` 的 T1 是**真实的 harness 自检**（用生产 `p3_sample_nearest` 交叉验证 fixture 的 leaf↔FITS 索引映射），未被自证。
- `p3_nan_mask_test.cpp` **接线正常**：`eng/tests/unit/CMakeLists.txt:1701-1708` `add_test(NAME w34_p3_nan_mask)`，非孤儿。（注意其 `:1704` 的 include 目录 `lib/algorithms/resample/include` **不存在**，但 `p3_resample.h` 仍能经 `acsd_p3_rsmp` 的 PUBLIC include 面找到。）
- `README.md:107-108` 对 LRU 取代 FIFO 的复测记录准确，与 `:76` splice/`:93` 逐出实现一致。

---

## 5. 你主动构造的反例

### CE-1 — 目标：推翻「Σw=1 证明了双线性核几何正确」
**构造**：把 `wg` 的定义按源码逐字展开，在**含 u、v 越出 [0,1]** 的取值上求和。
**期望推翻**：若 Σw=1 能作为几何判据，则 clamp（`p3_resample.cpp:411-412`）或错误 u,v 必然破坏它。
**结果**：**未能推翻——反例不成立**。81 组取值（含 u,v ∈ {−3/2,−1,0,1/3,1/2,7/10,1,2,5/2}）全部给出 Σ=1，**0 例外**。
**这正是要的产出**：该"不变量"对任意实数 u,v 恒真，是恒等式；clamp 前后同样恒真 ⇒ 它在**原理上不可能**检出 clamp 或几何失效。DISP-P3RSMP-001 的结案因此无效。

### CE-2 — 目标：推翻「`full_coverage()` 绿 ⇒ 通量守恒」
**构造**：取 `out_step=2.0`、输入格 `Ω_in=1`、输出格 `Ω_out=4`（一个输出像素跨 2×2 输入像素），走 `build_column_normalized` 的 `S_ij = a_ij/Ω_j`，其中 `a_ij = w_ij·Ω_out`（`p3_rsmp_operator.cpp:257`）。
**期望推翻**：若列归一真的守恒，列和应为 1。
**结果**：**反例成立**。
- `out_step=1.0`：R 行和 = 1.000000，S 列贡献 = 1.000000
- `out_step=2.0`：R 行和 = 1.000000，**S 列贡献 = 4.000000**
- `out_step=0.5`：R 行和 = 1.000000，**S 列贡献 = 0.250000**

⇒ `R` 行和恒 1 ⇒ `full_coverage()`（`operator.cpp:41-47`，只查 `coverage[i]`）**绿灯**；而 `S` 的列和 = `Ω_out/Ω_in ≠ 1` ⇒ **通量不守恒，误差正好是面积比**。`full_coverage()` 从不调用 `col_sums()`。且 `a_ij` 是由权重**合成**的（`sm.w * omega_out_sr`），全模块无一处真正的 `|Ω_j ∩ Ω'_i|` 几何计算 ⇒ 行和=1 不含任何足迹重叠信息。

### CE-3 — 目标：推翻「BUNIT 解析器不会静默接受畸形串」
**构造**：对 `p3_rsmp_units.cpp:132` 的 `std::stoi(f.substr(n+1))` 喂尾随垃圾。
**期望推翻**：`"ADU^2abc"` 应解析失败。
**结果**：**反例成立**。`std::stoi` 在首个非数字处停止且**不抛异常** ⇒ `"ADU^2abc"` 成功解析为幂次 2；`"ADU^0x10"` 解析为 0。对照同仓正典解析器 `lib/infrastructure/aio/product_io/src/bunit.cpp:51-53`（`strtol` + `if (end == nullptr || *end != '\0') return false;` 且限 ±64）——本模块的「量纲可判」门比正典更宽松，是**反向**的 fail-open。另 `:132` 的 `catch (...)` 是项目纪律明列的空捕获。

---

## 6. 盲复算

**方法**：遮住全部既有判定与文档结论，只拿「这个量应该是多少 / 这个门应该红还是绿」独立推导，再回比。

| 判据点 | 独立重算结论 | 与既有结论比 |
|---|---|---|
| `Σw=1` 是否为几何判据 | **恒等式，不可证伪**（CE-1，81/81） | **偏松**：既有把它当成 DISP-001 的结案依据 |
| `full_coverage()` 绿是否意味着守恒 | **不意味着**；列和 = `Ω_out/Ω_in`（CE-2） | **偏松**：既有只看 coverage |
| 被剔除样本权重是否被置 0 | **否**（源码 `continue` 跳过写入） | **一致**（与 README/头注释的宣称**不一致** —— 宣称偏松） |
| `:326` 断言能否失败 | **不能**（测试自初始值对自身） | **偏松**：既有把它当 Oracle 证据 |
| BUNIT 二次律门在生产检视什么 | **冻���表自身**，真实产品 BUNIT 从未进入（`SurfaceBrightnessInput` 无该字段） | **偏松** |
| tile 读失败是否可区分 | **可区分**：`aio_hips_read_tile_f32` 返回 −2/−3/**−4 尺寸非法**/**−5 读失败**/**−6 BITPIX 非法**（本人读 `aio_hips_reader.cpp` 源码确认）——但 `p3_resample.cpp:163` **只看 `!= 0` 全丢弃** | **一致**（判定偏严之处：既有文档称"未接线"，实为已接线；文档偏松） |
| `p3rsmp` 是否进产品 | operator/covariance/propagation/registry/failclosed **否**；units **是** | **既有未区分**（README:7-9 仍称生产源在 `lib/phase3_session/`） |

**结论**：本片既有判定整体**偏松**。最松的三处是把恒等式当不变量（CE-1）、把自初始化值当被检量（BLK-2）、把冻���表自身当被检量（BLK-10）。唯一发现既有结论**偏严**之处：`docs/science/PHASE3_HIPS_TO_FITS.md:215` 把 `p3_rsmp_units.cpp` 一并列为「未接线」，而该 TU 实际已进产品闭包并被 `module_adapters` 调用——文档多算了未接线面。

---

## 7. 子代理派发记录

**派发 6 个，覆盖 4 个互不重叠的分片核验范围**（其中 2 对为**同题盲复本**，用于一致性检验）。**6 份全部收齐**（第 6 个在交付件初稿后回，补记于下）。

| # | 范围 | 状态 | 结论采纳情况 |
|---|---|---|---|
| 1 | `p3_resample.cpp` + `p3_resample.h` | 已收 | 采纳 B1/B2/B3 三条阻断（与本人 BLK-7/8/2 独立撞车，互证） |
| 2 | `p3_resample.cpp` + `p3_resample.h`（1 的盲复本） | 已收 | 采纳 B-3 自愈锚（独立复现 BLK-2）；**否决其机制描述**（见下） |
| 3 | `p3_rsmp.h` + `operator.cpp` + `covariance.cpp` | 已收 | 采纳 BLK-1 产品路径、B4/B5/B6/B7、BLK-11② |
| 4 | 同 3（盲复本） | 已收 | 采纳 BLK-4/BLK-6；两复本对 BLK-1 表述一致，互证 |
| 5 | `units.cpp` + `kernel_registry.cpp` + `failclosed.cpp` | 已收 | 采纳 BLK-4/BLK-5/BLK-10/BLK-11①、M08/M15/M16 |
| 6 | `propagation.cpp` + 两个测试文件 | 已收（**补记**） | 采纳 BLK-12「负例」零实现；采纳 M-20..M-24；**否决其 B3（孤儿构建）**；**降级其 B2（合同原文未能核实）** |

**补记（第 6 个子代理在其报告后到，以下为本人对该报告的独立裁决）**

**否决 3 — 「`eng/tests/unit/p3_rsmp/` 是孤儿 CMakeLists、独立 Oracle 与 mutation driver 全部脱构建」：不成立。**
该子代理只 grep 了 `eng/tests/unit/CMakeLists.txt`（其中确无 `add_subdirectory(p3_rsmp)`），漏查根构建面。本人复核 `CMakeLists.txt:1506-1507`：
```
  add_subdirectory(eng/tests/unit/p3_proj p3_proj)
  add_subdirectory(eng/tests/unit/p3_rsmp p3_rsmp)
```
**确有接线**（位于某个 `endif()` 守卫块内，:1512）。故 `p3_rsmp_core/oracle/gate` 三个可执行与 `run_mutations.py` 是**在构建面内**的，其「同族都在跑」的自述成立。→ **否决，1 条。** 该报告据此推出的 B1/B3 两条「Oracle 失效」结论同时作废。

**降级 4 — 「`p3_uncertainty_propagate` 违反方差面冻结合同」：保留为待裁决项，本人不计入阻断。**
其核心观察（`p3_uncertainty_propagate` 无 `P3SampleRejection*` 出参，故 `n_rejected_nonfinite_variance` 在不确定度面**全仓不可达**；非有限 `V_j` 使整像素 NaN 而非「按不合格样本剔除并重归一」）—— 本人**独立确认前半句为真**（`p3_resample.cpp:541-547` 签名确无该出参）。但其援引的合同原句「`V_j` 有限且 ≤ 0 ⇒ 写 `variance=0 ∧ ivar=0`（禁 NaN）」**本人未能核实**：`grep "有限且 ≤ 0"` 在 `docs/science/DATA_SEMANTICS.md` 零命中；本人实读 §30.7（:3078-3092）只找到**信号面**的三分类冻结，**未见**方差面的对应条款。⇒ 判为**合同缺口 + 实现缺口并存**，登记为 UNRESOLVED 待负责人裁决，不冒充已证缺陷。

**同时据此修正本片一处「差点报成缺陷」的假阳性**：`P3SampleRejection` 的 `n_rejected_nonfinite_variance` 与 `n_rejected_nonpositive_weight` 恒 0，**并非缺陷**——`DATA_SEMANTICS.md:3086-3088` 逐字冻结「信号核只消费 signal 平面 ⇒ variance 项恒 0、几何权重 ⇒ 权重非正项恒 0」。已从候选缺陷中撤回，并补入 §4.4 干净项。

**采纳 5 — 「独立 Oracle 不调用被审对象、权重硬编码为 0.25」：本人独立复核为真，升为 BLK-13。**
本人实读 `eng/tests/unit/p3_rsmp/p3_rsmp_oracle.h:195-210`：`w00=w10=w01=w11` 四行全为字面 `0.25`，紧邻的循环变量 `x=ix+0.5, y=iy+0.5` **未参与权重计算**（即采样点全在格心，正是 Σw 精确可表的最平凡情形）；`grep -c make_bilinear_4quad_neighborhood` 结果 **0**。据此 `mdev ≡ 0`，并与 `kernel_registry.cpp:195` 的 `max_weight_sum_dev = 0.0` 形成「常量对常量」的自洽断言。`boundary_fail_closed` 亦经本人 grep 确认为 `oracle.h:180` 硬编码 `true`、registry 三处直写 `true`、且是 `kernel_registry.cpp:103` 的唯一门输入。

**逐条复核与否决记录**（本人独立验证，非转述）：

1. **否决子代理 1 的「`p3_export.cpp:243` 传 `omega=1.0` 使 R 归一退化」**。
   本人读 `lib/phase3_session/p3_export.cpp:244-251`：紧随其后即以 `nb.geom.omega_in_sr = in.omega_in_sr; nb.geom.omega_out_sr = grid.omega_out_sr;` 覆写，并 `for (double& a : n.overlap_sr) a *= oo;` 重标定，注释明写「FZ-P3-OMEGA-NONCONST：把 helper 的常数占位替换为 p3_proj 的真实逐像素 Ω′」。**该 `1.0` 是显式占位且已被正确替换**，不构成缺陷。→ **否决，1 条。**

2. **否决子代理 2 的「`aio_hips_read_tile_f32` 对所有失败模式统一返回 −1、无判别码」**。
   本人读 `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp` 的 `read_tile_t`：`−2` 文件打不开、**`−3`** 参数读错、**`−4` 「tile 尺寸非法」**、**`−5` 「tile read」失败**、**`−6` 「tile BITPIX 非 −32/−64」**。判别码存在。→ **否决其机制描述（保留其结论：`:163` 只看 `!= 0` 把它们全丢弃，缺陷成立）**。此为两子代理之间的直接冲突，由本人裁决。

3. **下调子代理 1 的「符号配平 21/21」为「配平，零失配」**——计数口径差异（是否把 `P3ResampleStatus` 等聚合体计入），结论一致，不影响判定。

4. **对子代理 5 的 M08（两套单位表 2 倍分歧）作限定采纳**：本人复核 `bunit.cpp:61` 为 `px_power += exp`（1×）、本片 `p3_rsmp_units.cpp:142` 为 `px_power = 2*e*sign`（2×），**分歧为真**；但本人**拒绝**把它升级为「现网 factor-2 缺陷」，因两者是互不交换的独立类型。降级为 S-09 潜伏陷阱。

5. **独立复核通过的子代理主张**（本人逐条验证为真）：`ALG-P3-001-KERNEL-ORACLE` 全仓仅 3 个字符串赋值点；`0.027395522883651657` 全仓无推导来源；`bilinear_area_overlap_exact` 在 `docs/`+`eng/contracts/` 零命中而 schema 枚举只有三项；`epsilon_corr`/`row_sum_tol`/`bunit_tol`/`production_science_default`/`forbidden_psfsw_product_keys` 生产零读者；`G-P3-KRN-04` 全仓不存在。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# —— 覆盖率口径 ——
for f in lib/algorithms/resample/p3_resample.cpp lib/algorithms/resample/p3_rsmp.h \
  lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp lib/algorithms/resample/p3_rsmp_operator.cpp \
  lib/algorithms/resample/p3_rsmp_propagation.cpp lib/algorithms/resample/p3_rsmp_kernel_registry.cpp \
  lib/algorithms/resample/p3_rsmp_failclosed.cpp lib/algorithms/resample/p3_rsmp_units.cpp \
  lib/algorithms/resample/p3_resample.h lib/algorithms/resample/p3_rsmp_covariance.cpp \
  lib/algorithms/resample/memory.md lib/algorithms/resample/module.yaml lib/algorithms/resample/README.md \
  lib/algorithms/resample/tests/p3rsmp/p3_resample_probe_main.cpp \
  lib/algorithms/resample/PHASE3_RSMP_IMPL.md lib/algorithms/resample/CMakeLists.txt; do
  printf "%6d  %s\n" "$(wc -l < "$f")" "$f"; done | tee /tmp/x  # 合计应 = 3704
awk '{s+=$1} END{print "TOTAL",s}' /tmp/x

# —— BLK-1 产品路径 ——
sed -n '1280,1281p' CMakeLists.txt                                   # 只含 p3_session.cpp
grep -rn "p3_export" --include=CMakeLists.txt . | grep -v '^./run/'  # 只在测试 CMakeLists
grep -n "未进构建" docs/science/PHASE3_HIPS_TO_FITS.md

# —— BLK-2 自愈锚（三行并看）——
sed -n '287p;290p;326p' lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp
sed -n '443,449p'   lib/algorithms/resample/p3_resample.cpp            # 对 ok[k]==false 直接 continue

# —— BLK-4/BLK-5 不存在的核与不存在的 Oracle ——
grep -rn "bilinear_area_overlap_exact" docs/ eng/contracts/ | grep -v pyc   # 应无输出
grep -rn "ALG-P3-001-KERNEL-ORACLE" lib/ eng/ docs/ | grep -v pyc          # 仅 3 个字符串赋值点

# —— BLK-7 读失败码（本 人裁决依据）——
awk '/^template <typename T>/,/^}/' lib/infrastructure/aio/src/hips/aio_hips_reader.cpp | grep -n "return -"
sed -n '163,172p' lib/algorithms/resample/p3_resample.cpp                 # 只看 != 0，全丢弃

# —— BLK-11① 无门消费的拒绝面 ——
grep -rn "forbidden_psfsw_product_keys" lib/ eng/ --include=*.cpp --include=*.h

# —— M-14 BUNIT 尾随垃圾 ——
sed -n '131,134p' lib/algorithms/resample/p3_rsmp_units.cpp
sed -n '51,53p'   lib/infrastructure/aio/product_io/src/bunit.cpp        # 正典解析器对照

# —— M-17/M-18 悬空引用 ——
ls eng/tests/backend/p3_resample_probe_main.cpp                          # 不存在
ls run/RSMP-NANMASK-01/                                                 # 不存在
ls lib/phase3_session/p3_resample.cpp                                    # 不存在（三件套仍称生产源在此）
ls lib/algorithms/resample/include                                      # 不存在（CMakeLists:1704 仍引用）

# —— CE-1 恒等式盲复算（纯代数，不编译任何项目代码）——
python3 -c "
from fractions import Fraction as F
bad=0
for u in [F(-3,2),F(-1),F(0),F(1,3),F(1,2),F(7,10),F(1),F(2),F(5,2)]:
  for v in [F(-3,2),F(-1),F(0),F(1,3),F(1,2),F(7,10),F(1),F(2),F(5,2)]:
    if (1-u)*(1-v)+u*(1-v)+(1-u)*v+u*v != 1: bad+=1
print('violations =',bad,'=> Sigma(w)=1 是恒等式')"
```

**未执行声明**：本次全程未编译、未跑 ctest/pytest、未跑任何仓内二进制、未执行任何 `git` 写操作、未修改任何仓内文件（本交付件除外）。CE-1/CE-2/CE-3 的复算为纯代数推导，使用解释器做有理数/整数算术，不导入、不链接、不执行任何项目代码；临时文件写在 `/tmp`，未触碰 `/tmp/acsd_g08/`。

---

**遗留待前台裁决（我不自行判定）**
1. `bilinear_area_overlap_exact` 与 `ALG-P3-001_KERNEL-ORACLE` 是**补实现**还是**从 registry 删除**——属范围决策。
2. `p3_export.cpp` 是否进产品图（决定 BLK-1 是「接线」还是「改文档声明 UNIMPLEMENTED」）。
3. M-18 的「现状执行证据」链需重建：探针文件未被任何构建引用，四处文档指向已删路径。