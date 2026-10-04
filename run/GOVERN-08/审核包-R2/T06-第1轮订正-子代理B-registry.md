# T06 第 1 轮订正 · 子代理 B · docs/detail/registry 车道交付件

> 写作用域：`docs/detail/registry/**`。本单只改该目录下 15 张卡，未触碰其他任何文件。
> 复跑命令（下文所有「复跑」均指此）：`python3 /tmp/verify_anchors.py docs/detail/registry/*.md`
> 校验命令：`grep -n '一节\|见 §\|曾记\|旧版\|作废\|383088f2\|::config_fields' docs/detail/registry/*.md`
> 表格/编码自检：见第 6 节末。

---

## 1. 逐条处置表

（表体分两层：先总表给出编号 / 位置 / 问题 / 处置 / 依据；再按编号给「改前逐字 → 改后逐字」。改后行号以改后文件为准，改前行号以改前文件为准。）

### 1.0 总表

| 编号 | 位置（改前行号） | 问题 | 处置 | 依据 |
|---|---|---|---|---|
| D1-1 | `acsd.phase1.drizzle.md:70-72` | invalid 语义写「值 NaN 经面亮度累加**传播、不掩膜**」，与同卡 `:182-185`、与 `docs/science/drizzle/DRIZZLE.md` 「误差来源与预算」、与引擎实现三处相反 | 已改（按代码 + 上游正本订正） | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 主循环按原因分类剔除非有限样本并计数（`rejected_nonfinite_value` / `_variance` / 权重）；`drizzle_science.cpp` 的 `validate_*` 拒绝非有限 pixfrac |
| D1-2 | `acsd.phase1.drizzle.md:87-88` | 用「严格通量守恒 … 全 `pixfrac ∈ (0,1]`」作无条件断言 | 降级为条件式（归一 + 几何闭合两条件）并指向上游正本 | `docs/science/drizzle/DRIZZLE.md`「几何闭合」；`docs/science/algorithms/DRIZZLE_GEOMETRY.md`「DISP-DRZ-004」把掩膜/闭合写成约束 |
| D1-3 | `acsd.phase1.drizzle.md:95`（`DRIZZLE.md §7`） | 机械跳转锚指向参考文献章 | 已改为自然语言点名真实章名 | 该 §7 实为「参考文献与参考代码」，常量表在「参数与常数」 |
| D1-4 | `acsd.phase1.drizzle.md:219,240-242` | `DRIZZLE_GEOMETRY.md §9` / `ALG-DRZ-001 §10` / `README.md §9` 三处裸跳锚；缺陷清单里「NaN 无计数」与现行实现矛盾 | 已改为点名真实标题；缺陷清单按 DISP-DRZ 现状重写 | `grep -n '^#\{1,3\} '` 取真实标题；DISP-DRZ-004/003/008 原文 |
| D1-5 | `acsd.phase1.drizzle.md:92` | 引「F&H 2002 §7.2 式(7)」，该锚已由 science 车道撤换；另写「drizzlepac 3.11.0」版本号，仓内无任何处核到该版本 | 已改为双锚（式 (2)–(5) ＋ §7 式 (7) 后定义句）并删去版本号 | `DRIZZLE_GEOMETRY.md` DISP-DRZ-009「原引 §7.2 撤换」；`docs/science/drizzle/DRIZZLE.md`「核权重：按 drop 面积归一」与「参考实现」 |
| D1-6 | `acsd.phase1.calibration.md:72` | `lib/algorithms/calibration/{include,src}/` 花括号展开不是可解析路径 | 已展开为两个真实目录 | `ls lib/algorithms/calibration/` → `include/`、`src/` 均存在 |
| D1-7 | `acsd.phase1.noise-snr.md:344` | `lib/algorithms/noise_snr/{include/acsd, src, wrapper_phase1, cpp}/` 同上 | 已展开为四个真实目录 | 同上四个目录均存在（`ls` 核实） |
| D1-8 | `acsd.phase1.noise-snr.md:437` | 引 `lib/algorithms/integration/phase2_integrate/oracle/`，该目录不存在（`find lib/algorithms -type d -name '*oracle*'` 零命中） | 撤回虚构载体，改指真实判据载体并如实写明「仓内无独立 oracle 源码目录」 | `find` 零命中；真实载体 = `实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md`（存在，81437 字节） |
| D1-9 | `acsd.phase1.session.md:58` | `lib/phase1_session/p1_session.{cpp,h}` 花括号展开 | 已展开为两个真实文件 | `ls lib/phase1_session/` → `p1_session.cpp`、`p1_session.h` |
| D1-10 | `acsd.phase1.star-detection.md:124-125` | `file.cpp::symbol()` 伪路径写法 | 符号真实存在，仅改写法为「文件 + 符号名」 | `session_commands.h` 的 `config_fields(SessionId)`、`parser.cpp` 的 `session_keys()` 均在；`SESSION_NORMALIZE` 是真实枚举符 |
| D1-11 | `acsd.phase2.write.md:94` | `lib/algorithms/coverage/{src, include/astro/phase2, tools, tests}/`；其中 `tests/` 在该模块下**不存在** | 展开为三个真实目录，删去不存在的 `tests/`；同时删掉同句的提交号与退场叙事 | `ls lib/algorithms/coverage/` → 无 `tests/`；`find . -name 'acr_kernels*'` 仅命中 `run/` 下的历史快照，`lib/` 下无 |
| D1-12 | `acsd.phase2.write.md:140-141` | `退出码 2 = eng/packaging/config/CLI` 是拼接坏路径；且把 stage2 工具返回值称作「退出码」，与同段「退出码唯一源 = exit_codes.h」自相矛盾 | 已改为「acsd-stage2 工具返回值（工具局部，不是 `acsd::ExitCode`）」并逐条对齐上游表 | `docs/engineering/api/PUBLIC_API.md`「`acsd-stage2` 工具返回值」表；`lib/infrastructure/cli/exit_codes.h` 的枚举 |
| D1-13 | `acsd.phase2.write.md:161-169` | 提交号 `383088f2`、退场删除叙事、悬空文件路径 `acr_kernels.cpp` | 已改为现在时事实陈述（`lib/` 下无 ACR 源 / 无该符号 / 无 CUDA kernel） | 同 D1-11 |
| D1-14 | `acsd.phase2.integrate.md:164,193,196` / `acsd.phase2.reject.md:164-165` | 同类提交号与退场叙事 | 已改为现在时事实陈述 | 同上；`grep -rn mosaic_reject_legacy lib/` 零命中 |
| D1-15 | `acsd.phase3.wcs.md:205` / `acsd.phase3.resample2.md:228` | `docs/detail/registry/acsd.phase3.*` 通配写法不可解析 | 已展开为本目录五张卡名 | `ls docs/detail/registry/acsd.phase3.*` |
| D2-1 | `acsd.phase1.noise-snr.md:62,95-107,277,370,413-416,467,483` | 逐帧参考通量口径（含配置键表 `reference_flux`）与代码/合同不一致 | 已改为「按冻结的参考星等档 `m_ref` 取，`m_ref` 随产品落盘；禁数据派生」 | `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp`（`reference_flux_adu` 必填、逐帧中位数回退已移除）；`module_adapters.cpp` 的 `m_ref = snr.reference_mag`（缺省 6.0）与 `F_ref,k = 10^(−0.4(m_ref − ZP_k))`；`eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline` 必落 `reference_mag` |
| D2-2 | `acsd.phase1.drizzle.md:154` / `acsd.phase2.integrate.md:138` / `acsd.phase2.upm-fit.md:87` | 同上口径的残留写法 | 同 D2-1 处置 | 同上 |
| D2-3 | `acsd.phase2.integrate.md:63-64` | 写「weights f64 1/ADU²，可空 = 等权」；该形态已退役（`weight_mode` 选择键与 support×SNR² / 等权两档均不存在） | 已改为「生产权重 = 调用方构造的逐样本逆方差」并把可空分支写成本 C API 输入合同 + 明写两档退役 | `lib/algorithms/coverage/include/astro/phase2/integrate.h` 头注明文 |
| D2-4 | `acsd.phase2.integrate.md:126` | `frame_reconstruct` 被描述为「等权重面」 | 已改为「逐像素常量面，权重 = `SNR_f²/F_ref,k²`，不等于等权」 | 同上 + `w = SNR²/F_ref² = 1/σ_F²` 换算式 |
| D2-5 | `acsd.phase2.upm-fit.md:33-34,223,238-240` | UPM 权重公式与四套口径冲突；配置表含三个仓内不存在的字段名 | 公式已与代码正本逐字对齐（`raw_w = quality_factor × control_ivar`，几何可靠性在 per-control 归一化施加）；配置表重写为「表 A `P2UpmBuildConfig` / 表 B 判据与天光面字段」，字段名逐个按签名头核实 | `lib/algorithms/coverage/include/astro/phase2/upm.h`；`lib/phase2_session/p2_session.cpp`；`lib/algorithms/coverage/include/astro/phase2/sky_plane.h` |
| D2-6 | `acsd.phase1.drizzle.md:101-112` | 归一发布因子 `k` | 条件式补入（平面极限下 `k = pixfrac²`，球面按残差律近似），并写明 signal 乘 `k` / variance 乘 `k²` | 我自推 + 数值夹具（见第 1.1 节 D2-6 依据栏）；`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h` 的 `sb_publish_scale` 与 `k := D_p / N_p` 定义 |
| D2-7 | `acsd.phase1.noise-snr.md:234-238` | 本模块 `ln R` 序统计量的序号记作 `k`，与科学正本同模块的「掩膜边缘残余系数 `k = 0.1`」混名 | 已把序号改记 `j` 并写明消歧理由 | `docs/science/noise_snr/NOISE_SNR.md` 常数表（`k` 掩膜边缘残余系数 0.1）；GLOSSARY 侧 `k := D_p/N_p` 又一义 |
| D3-1 | `acsd.phase2.write.md:149,178` | 「…一节」两处 | 已去掉「一节」，点名真实章名 | `LOG_AND_ERROR.md` 实有「## 5 错误对象与退出码映射」「## 7 落点合同」 |
| D3-2 | `acsd.phase3.writer.md:15,39,80-81,167,201` | 「IO_003「错误语义」一节」「见 §9」「本页「…」一节」「见 §10」 | 全部改为自然语言点名真实标题 | `ATOMIC_PUBLISH.md` 实有「## 错误语义」「## 发布流水线（原子语义）」；本卡 §9/§10 的真实标题已取 |
| D3-3 | `acsd.phase3.wcs.md:38,65` | 「见 §9」；「文档面**曾**记」历史叙事 | 锚改为点名标题；历史叙事改为现在时 + 保留冲突事实 | 本卡「## 9 验证与测试面（…VERIFIED）」 |
| D3-4 | `acsd.phase3.resample2.md:37,156` | 两处「见 §9」 | 同上 | 本卡「## 9 验证与测试面（…VERIFIED）」 |
| D3-5 | `acsd.phase1.noise-snr.md:363` | 「冻结词表见数值落地一节」 | 改为点名本卡真实小节「稀疏帧内层几何与重建算子」 | `grep -n '^#\{2,4\} '` 取真实标题 |
| D3-6 | `acsd.phase1.photometry.md:91` / `acsd.phase1.star-detection.md:47` | 两处裸跳「见 §8」/「见 §4」（两卡均无编号小节） | 改为点名本卡真实小节 | `grep -n '^#\{2,4\} '` |
| D3-7 | `acsd.phase1.session.md:35,97` / `acsd.phase1.wcs-platesolve.md:127` / `acsd.phase2.upm-apply.md:149` / `acsd.phase2.upm-fit.md:321` | `README.md §3`、`返回码节`、`TEST-DESIGN 节`、指代性的「该节」 | 全部改为点名目标文件与真实标题 | `lib/phase1_session/README.md` 实有「## 3 装配顺序与数据流（p1_session_run 实测）」；`PUBLIC_API.md` 实有「### 返回码」（两处）；`PHASE2_UPM_IMPL.md` 实有「## 12 TEST-DESIGN（TEST-P2-UPM-DESIGN 冻结）」 |
| D4-1 | 全 registry | 悬空 `[n]` 引用（GLOSSARY 红线） | 无需处置：`grep -n '\[[0-9]+\]' docs/detail/registry/*.md` 六处命中**全部是数组下标**（`error_msg[512]`、`last_error[256]`、`model_hash[65]`、`sha256[65]`、`error[512]`），非引用编号 | 复跑命令见第 5 节 |
| D4-2 | `acsd.phase3.writer.md`（文末参考文献有 ［1］［2］，正文零引用） | 参考文献表存在但正文无角标（孤立文献表） | 已补两处正文角标（out 面头卡依据、DATASUM 与 CHECKSUM 的区分） | 见第 4 节文献核对 |
| D4-3 | `acsd.phase3.wcs.md` | 正文 ［2］ 与文末 ［2］ 已闭合；［1］ 无正文角标 | 未动（［1］ Paper I 在本卡内容中无对应断言，凭空加角标等于编造）→ 登记 | 见第 5 节 |
| D5-1 | `acsd.phase1.noise-snr.md` / `acsd.phase1.wcs-platesolve.md` / `acsd.phase1.photometry.md` | 帧级信噪比 / WCS 解 / 测光定容的精度归属未在本片任何卡中声明 | 已在三卡加自然语言精度归属段（指向最高设计「数据对象与配置」章内「精度归属」条），**未单方面改任何数值面** | `docs/ACSD_DESIGN.md` 第 3 章「精度归属」 |
| D5-2 | `acsd.phase1.noise-snr.md`「已知限制」 | 最高设计的双精度要求与 `UNIFIED_OBJECTS.md` / `aio_api.cpp` 的单一全局精度位不可调和 | 登记为**待裁决**，页面只写归属与冲突，不给实现结论 | `docs/engineering/UNIFIED_OBJECTS.md` 对象表精度列（全为 `float32\|float64`）；`lib/infrastructure/aio/src/aio_api.cpp` 的 `g_aio_precision_mode_fp64` 全局位 |

### 1.1 逐字（改前 → 改后）

**D1-1 / D1-2 `acsd.phase1.drizzle.md` invalid 与守恒口径**

改前（`:70-72`）：

> **invalid**：现行实现为值 NaN 经面亮度累加**传播、不掩膜**（DRIZZLE.md；
> drizzle_engine.cpp；回归 finalize 层，covered_area ≤ 0 → variance 记 NaN，
> `p1drz_tests_core.cpp`）；pixfrac ∈ (0,1] 引擎层严格拒绝；仅 NESTED。

改后：

> **invalid**：源像元值非有限（NaN/±Inf）按**样本级掩膜**处理 —— 不合格样本从
> 分子、分母、方差三项中一并剔除并**重新归一**，剔除数逐叶计数（引擎计数
> `rejected_nonfinite_value`，规则正本 = docs/science/drizzle/DRIZZLE.md 的「误差
> 来源与预算」）；只有**零合格样本**的输出像元才取 NaN 且 `support ≤ 0`
> （覆盖级 NaN）；方差面非有限同样按不合格样本剔除，而方差有限但 ≤0 表示「有覆盖
> 但无方差信息」，此时信号与几何权重照常计入覆盖、不计入方差项（回归 finalize 层，
> covered_area ≤ 0 → variance 记 NaN）。`pixfrac ∈ (0,1]` 引擎层严格拒绝（非有限、
> 0、负、>1 均拒绝，不夹逼）；仅 NESTED。

改前（`:87-88`，节选）：

> （`dover /= jaco`）；这是唯一满足**严格通量守恒**（累加的通量总和等于源端
> 积分通量总和，全 `pixfrac ∈ (0,1]`）的口径；

改后（节选）：

> （`dover /= jaco`，`jaco` 为映射后的 drop 面积）；在这一口径**且几何闭合成立**时，
> 累加的通量总和等于源端积分通量总和。通量守恒的成立是**条件式**的：条件一 = 核按
> drop 面积归一（换任何其他归一即破坏），条件二 = 几何闭合 `Σ_p a_jp = A_drop,j`
> 逐 drop 成立（drop 足迹面积在其覆盖的叶上被精确分完，超额与亏损分开具名判红；
> `A_drop,j` 取球面实测值，它与 `pixfrac²·A_pixel,j` 只在平面极限相等，残差见下条
> `k`）……本页不作无条件断言；

**D2-6 `acsd.phase1.drizzle.md` 归一发布因子（新增条，含我的推导）**

改前：该卡**没有**这一条（`k = D_p/N_p` 在 registry 全树零命中；只在
`docs/science/noise_snr/NOISE_SNR.md`、`docs/science/drizzle/DRIZZLE.md`、
`docs/GLOSSARY.md` 出现）。

改后（新增）：

> **归一发布因子 `k = D_p/N_p`（`D_p = Σ_j a_jp` 为覆盖面积，
> `N_p = Σ_j w_jp·A_pixel,j` 为面亮度归一分母）**：在**平面（仿射）极限**下，
> 由 `A_drop,j = pixfrac²·A_pixel,j` 与 `w_jp = a_jp/A_drop,j` 得
> `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²`，即 `k = pixfrac²`……**球面上该等式只近似
> 成立**……残差律 `δ = (1−pixfrac²)·θ²·[0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)²] + O(θ⁴)`
> ……把它代回 `N_p = Σ_j (a_jp/A_drop,j)·A_pixel,j` 作一阶展开可知：`k` 相对
> `pixfrac²` 的偏离与 `δ` **同阶**（面积加权平均、符号相反）⇒ `θ ≲ 10″/px` 时该偏离
> ≲ 1e-10，`θ ≳ 100″/px` 时进入 1e-7–1e-6、与门禁容差同阶……

我的推导（不照抄审稿结论）：

1. 定义：`w_jp = a_jp / A_drop,j`（核按 drop 面积归一）、`N_p = Σ_j w_jp · A_pixel,j`、
   `D_p = Σ_j a_jp`、`A_drop,j = pixfrac²·A_pixel,j`（平面极限）。
2. 代入：`N_p = Σ_j a_jp · A_pixel,j / (pixfrac²·A_pixel,j) = (1/pixfrac²)·Σ_j a_jp = D_p/pixfrac²`。
3. 故 `k := D_p/N_p = pixfrac²`，与 `pixfrac` 取值无关（`pixfrac = 1 ⇒ k ≡ 1`）。
4. 球面修正：`A_drop,j` 与 `A_pixel,j` 由不同四边形在球面上各自量得，
   `A_drop,j / (pixfrac²·A_pixel,j) − 1 = δ ≠ 0`，二阶展开给出上述残差律
   （引 `docs/science/algorithms/DRIZZLE_GEOMETRY.md`「面亮度保持权重的实现口径」段）。
5. 数值夹具（我自写，`/tmp/k_fixture.py`，平面极限构造 + 强制闭合 `Σ_p a_jp = A_drop,j`）：
   `trials=2000, max relative deviation of D_p/N_p from pixfrac^2 = 3.600e-16`
   ⇒ 平面极限下等式在浮点舍入量级成立。
   同一夹具对两种参数化的面亮度：`S_drop=2.6999999999999997`、`S_pixel=2.7000000000000002`
   ⇒ **两套公式独立复算只到双精度舍入量级，不是「逐位相同」**。据此把该卡原句
   「两种参数化给出**逐位相同**的面亮度与逐像素方差」改为「给出同一个 `c_jp`
   ……（实现侧两个直写末端共用同一分母、走同一代码路径 ⇒ 产物逐位相同；两套公式独立
   复算时差在双精度舍入量级）」。
6. 因此**没有**写成无条件等式 `k = pixfrac²`：见第 2 节否决条目 N-1。

**夹具全文（可复跑；我执行时放在 `/tmp/k_fixture.py`，输出见上）**

```python
import random
random.seed(20260101)
print("fixture: k = D_p/N_p = pixfrac^2  (w_jp = a_jp/A_drop,j, A_drop,j = pixfrac^2*A_pixel,j)")
worst = 0.0
for trial in range(2000):
    pixfrac = random.uniform(0.05, 1.0)
    nj = random.randint(1, 12)
    np_ = random.randint(1, 12)
    D = []
    for j in range(nj):
        A_pixel = random.uniform(0.5, 2.0)          # sr
        A_drop = pixfrac**2 * A_pixel
        parts = [random.random() + 1e-3 for _ in range(np_)]
        s = sum(parts)
        a_jp = [A_drop * q / s for q in parts]      # 构造性闭合 Σ_p a_jp = A_drop,j
        D.append((a_jp, A_pixel, A_drop))
    sum_area = [sum(d[0][p] for d in D) for p in range(np_)]
    sum_norm = [sum(d[0][p]/(pixfrac**2*d[1]) * d[1] for d in D) for p in range(np_)]
    for p in range(np_):
        if sum_norm[p] <= 0: continue
        k = sum_area[p]/sum_norm[p]
        worst = max(worst, abs(k - pixfrac**2)/pixfrac**2)
print("trials=2000, max relative deviation of D_p/N_p from pixfrac^2 = %.3e" % worst)
pixfrac = 0.8; A_pixel = 1.7; A_drop = pixfrac**2*A_pixel
a = [0.3*A_drop, 0.7*A_drop]
w  = [x/A_drop for x in a];  Np = sum(wi*A_pixel for wi in w);  Dp = sum(a)
wp = [x/A_pixel for x in a]
S1 = sum(wi*B for wi,B in zip(w,[2.0,3.0]))/sum(w)
S2 = sum(wi*B for wi,B in zip(wp,[2.0,3.0]))/sum(wp)
print("两种参数化面亮度: S_drop=%.17g  S_pixel=%.17g  逐位相同=%s" % (S1,S2,S1==S2))
print("k = D_p/N_p = %.17g ; pixfrac^2 = %.17g" % (Dp/Np, pixfrac**2))
```

实测输出：

```text
fixture: k = D_p/N_p = pixfrac^2  (w_jp = a_jp/A_drop,j, A_drop,j = pixfrac^2*A_pixel,j)
trials=2000, max relative deviation of D_p/N_p from pixfrac^2 = 3.600e-16
两种参数化面亮度: S_drop=2.6999999999999997  S_pixel=2.7000000000000002  逐位相同=False
k = D_p/N_p = 0.64000000000000001 ; pixfrac^2 = 0.64000000000000012
```

（夹具只覆盖**平面极限**；球面残差不由本夹具验证，取 `DRIZZLE_GEOMETRY.md` 的
二阶展开与实验单元实测读数，见 A-1/N-1。）

**D2-1 `acsd.phase1.noise-snr.md` 参考通量口径**

改前（`:95-102`）：

> **逐帧参考通量**：参考通量由该帧自己的零点决定，参考星等固定为 6.0 等，
> `reference_flux_scope = frame_independent_fixed_magnitude`；公共锚与帧无关，
> 满足「帧参考通量 × 光度响应 = 公共锚」的严格恒等。配对性只要求**同一帧内** SNR
> 与参考通量同源，**不要求跨帧相等**……

改后（要点逐字）：

> **参考通量按冻结的参考星等档取，不由数据派生**：参考星等 `m_ref`（配置键
> `snr.reference_mag`，缺省 6.0 等）在一次运行内冻结，参考通量是该星等档在**本帧
> 仪器通量下的读数** `F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，其中
> `ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)` 只来自本帧自身的测光标定；产品另记与帧
> 无关的**物理公共锚** `F0 = 10^(−0.4·(m_ref − ZP_syn))`……该口径的合同正本 =
> `eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline`
> （必落 `scope` / `reference_flux_source` / `reference_flux_common` / `reference_mag` /
> `reference_mag_system`，`scope` 生效值 = `frame_independent_fixed_magnitude`）……
>
> **禁由数据派生参考通量**：逐帧检出通量中位数形态会丢掉帧间标度因子 `a_f²`……
> ⇒ 该形态 **fail-closed**（`snr_frame_science` 的 `reference_flux_adu` 缺失 / 非有限 /
> ≤0 即判红）……

改前（`:370` 配置表首行）：

> | `reference_flux` | 逐帧参考通量 | 见说明 | 参考通量（m5/SNR 定义必需）。**逐帧**，由该帧零点与固定参考星等（6.0 等）决定；`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚满足「帧参考通量 × 光度响应 = 公共锚」。……

改后（拆成两行，键名按代码核实）：

> | `snr.reference_mag` | 6.0 | mag | 冻结的**参考星等档** `m_ref`，SNR 与深度定义必需。……**不由数据派生**，`m_ref` 随产品落盘（合同必落字段见 `eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline`）。星等档是线性区外的形式外推，**用途限定为参考电平** |
> | `snr.reference_flux_adu` | —— | ADU | 显式给出的参考通量（覆盖按 `m_ref` 换算的结果，优先级最高）；缺失 / 非有限 / ≤0 ⇒ 该帧 fail-closed，不回退到数据派生形态。…… |

改前（`:413-416` 错误面）：

> - `reference_flux` 未定义时 m5/SNR 不可输出；无 `photscale_fit` 或
>   `zero_point_valid=false` ⇒ 按显式回退 `group_median`（`reference_flux_scope`
>   落盘，**不伪造**），显式 `snr.reference_flux_adu` 优先；**逐帧中位数回退为
>   fail-closed，该形态保持**；

改后：

> - 参考通量取不到时 m5/SNR 不可输出。三条来源形态各自具名落盘
>   （`reference_flux_source`）：① `fixed_magnitude` = 冻结的 `m_ref` 按本帧测光
>   零点换算（生效路径，作用域 `frame_independent_fixed_magnitude`）；② `config` =
>   显式 `snr.reference_flux_adu`；③ `group_median` = **块级**公共 F0（块内逐帧检出
>   通量中位数的中位数，作用域 `group`），只在 ①② 都不可得时启用……**以本帧检出通量
>   中位数充当本帧 `F_ref`** 的形态（作用域冒充 `fixed_magnitude`）⇒ fail-closed：
>   它丢 `a_f²` 且使帧间不可比；

**D2-3 `acsd.phase2.integrate.md` 候选栈权重**

改前（`:63-65`）：

> 内核级真实 I/O 合同 = DATA-P2-INT（本页输入输出端口表）：输入 `P2PixelStack`
> （values f64 ADU / weights f64 1/ADU²，可空 = 等权 / support f64 [0,1]，可空 =
> 1.0 / accepted u8，可空 = 全接受 / count u32）；输出 `P2PixelResult`（signal f64
> ADU / support f64 [0,1] / 五计数器 / status 0..4）。

改后：

> 内核级真实 I/O 合同 = DATA-P2-INT（本页输入输出端口表）：输入 `P2PixelStack`
> （values f64 ADU / weights f64 1/ADU²，**生产权重 = 调用方构造的逐样本逆方差
> `w = SNR²/F_ref² = 1/σ_F²`**；weights 指针可空 = 本 C API 的输入合同（无权重数组
> ⇒ 等权），生产唯一调用方恒传权重数组，故该分支在生产不可达 / support f64 [0,1]，
> 可空 = 1.0 / accepted u8，可空 = 全接受 / count u32）；输出 `P2PixelResult`
> （signal f64 ADU / support f64 [0,1] / 五计数器 / status 0..4）。
> **单一权重口径（签名头 integrate.h 明文）**：唯一生产策略 = 调用方构造的逐样本
> 逆方差权重；`weights = support × SNR²`（原 `weight_mode = 0`）与 `weight_mode = 1`
> 的等权档这两个**可选口径**及其 `weight_mode` 选择键**均不存在**，出现即判红 ——
> support 是无量纲几何量、等权不是信号/噪声之比，两者都不是权重。UPM 控制点权重是
> 另一个语义（`p2_upm_raw_weight`），禁止与本模块的积分权重混名。

**D2-4 `acsd.phase2.integrate.md` frame_reconstruct**

改前（`:126`）：`- `frame_reconstruct` → 帧级 SNR 重建 / 直接参与（等权重面）；`
改后：

> - `frame_reconstruct` → 用帧级标量把该帧的 SNR 铺满为逐像素常量面（该帧每个像素
>   的权重 = `SNR_f²/F_ref,k²`，逐像素取值相同但**不等于等权**：各帧的 `SNR_f` 与
>   `F_ref,k` 一般不同）；

**D2-5 `acsd.phase2.upm-fit.md` 权重公式与配置表**

改前（`:33-34`）：

> 落地链路：production 权重 `p2_upm_raw_weight` = quality × control_ivar（缺
> control ivar **显式 rc=2**，禁静默回退）→ per-control 归一化（权重除以权重和再乘
> control_reliability）→ Huber IRLS（δ = 1.345 无量纲）+ 图平滑 + 弱零锚 + 连通

改后：

> 落地链路：**production 控制点权重 = `quality_factor × control_ivar`**
> （签名头 `lib/algorithms/coverage/include/astro/phase2/upm.h` 的
> `p2_upm_raw_weight` 是**单一实现**：几何可靠性**不在分子乘 geom**，而是在
> per-control 归一化里施加 —— `out_norm = raw / Σ_cell raw × control_reliability`，
> 单元总权恒为 `control_reliability`；`control_ivar ≤ 0` 或非有限 ⇒ rc = 2
> 显式 INVALID，禁静默回退 support / SNR 权重臂）→ Huber IRLS（δ = 1.345 无量纲）
> + 图平滑 + 弱零锚 + 连通

改前（`:220-241` 表头与三行）：

> 配置 = `P2UpmBuildConfig` 16 字段（upm.h），production 默认单一来源 =
> lib/phase2_session/p2_session.cpp。
> … | `upm_weight_source` | 0 | —— | 0 = snr2_normalized（归一化后按信噪比平方） |
> … | `gauge` | `reference_frame` | —— | `reference_frame` / `sum` |
> … | `bkg_model` | `spline` | —— | 天光面表示：`spline`（稀疏二维样条面）/ `constant`（常数，仅均匀背景） |
> … | `bkg_spline_spacing` | 由输入几何导出 | deg | 样条节点间距（决定面自由度）；缺省 = 由输入几何导出；几何量缺失 ⇒ fail-closed（标定常数属另一形态） |

改后：

> 配置面分三处结构体，字段名一律以签名头为准：`P2UpmBuildConfig`（upm.h，20 字段；
> 本页登记装配面字段见下表 A）、`P2UpmMaBuildConfig`（upm.h，乘法/加性观测求解器；
> 判据与 gauge 字段见表 B）、`P2SkyPlaneConfig`（sky_plane.h，天光面表示与逐帧
> 梯度；字段见表 B）。production 默认取值单一来源 = lib/phase2_session/p2_session.cpp。
>
> 表 A …… | `snr_weight_mode` | 0 | —— | 0 = snr2_normalized（头字段名
> `snr_weight_mode`，生产装配显式置 0）；生产权重口径由 `use_ivar_weight` 决定，
> 本字段不选择科学权重 | …… | `control_reliability` | 1.0 | —— | per-control 相对
> 可靠度，**在 per-control 归一化中施加**（不在 `raw_w` 分子）；实现上是配置常量……
>
> 表 B —— 判据与天光面表示字段（分属另两个结构体）：| `rank_rtol` | …
> `gauge_mode` | …… | `frame_gradient_order` | `P2SkyPlaneConfig` | 1 | ……
> `spline_degree` | `P2SkyPlaneConfig` | 1 | …… | `node_spacing_deg` | ……

（`bkg_model` / `bkg_spline_spacing` 两行**不在仓内检出任何载体**，已删除并以真实字段
`spline_degree` / `node_spacing_deg` 替代；`gauge` 改为真实字段 `gauge_mode`；
`upm_weight_source` 改为真实字段 `snr_weight_mode`。）

**D1-12 `acsd.phase2.write.md` 工具返回值**

改前（`:140-141,148-149`）：

> 退出码 2 = eng/packaging/config/CLI、3 = coverage / target_order、4 = frame_id /
> sampler、5 = UPM、6 = tile 读写 / 块不可行 / finalize、7 = ivar 门 / HIPS_VERIFY。
> ……错误码与退出码唯一源 =
> lib/infrastructure/cli/exit_codes.h；域→码映射唯一源 =
> docs/engineering/contracts/LOG_AND_ERROR.md「错误对象与退出码映射」一节。

改后：

> **acsd-stage2 工具返回值（工具局部，不是 `acsd::ExitCode`）**：0 = 成功；1 = 未捕获
> 异常兜底；2 = config 解析 / CLI 参数错误；3 = coverage 构建 / target_order 校验；
> 4 = frame_id / sampler 域；5 = UPM 构建 / 持久化；6 = 写路径 / 集成块（rejection
> resolve、tile 写、large_scale 等）；7 = ivar 门（ivar 产品缺失且未显式降级）/
> HIPS_VERIFY 回读失败。**该工具不以裸整数冒充进程退出码**：它不消费
> `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode`，两套码值在 3–7 区间重叠
> 且语义不同，按任一面反查都会取到另一面的错值（该重叠已在
> docs/engineering/api/PUBLIC_API.md 的「acsd-stage2 工具返回值」处登记为未决项）。
> ……**进程退出码**唯一源 = lib/infrastructure/cli/exit_codes.h；域→码映射唯一源 =
> docs/engineering/contracts/LOG_AND_ERROR.md 的「错误对象与退出码映射」。

**D5-1 精度归属（新增段，三卡同构）**

`acsd.phase1.noise-snr.md` 新增：

> **精度归属**：帧级信噪比标量、稀疏控制点值与控制点局部 `σ` 属**稀疏与元数据**面，
> 按最高设计的「数据对象与配置」章内「精度归属」条取**全程双精度**；JSON 显式指定
> 位深时以 JSON 为准。稠密逐像素方差面属**稠密大面**，按同条取单精度，两类面不得互相
> 代入（该归属与现行单一全局精度位的冲突登记见「已知限制」）。

`acsd.phase1.noise-snr.md`「已知限制」新增：

> - **精度归属待裁决**：最高设计要求稀疏与元数据（帧级信噪比等）全程双精度，而
>   `docs/engineering/UNIFIED_OBJECTS.md` 的对象登记对全部对象只给「float32 或
>   float64」一个全局精度位、`lib/infrastructure/aio/src/aio_api.cpp` 也只有一个全局
>   精度开关（`aio_set_precision_mode`）—— 一套全局位无法同时满足「稠密大面单精度」
>   与「稀疏元数据双精度」。本卡按最高设计登记双精度归属，实现侧如何满足该归属
>   （按块分精度位 / 分 AIO 句柄 / 接受全局单精度并下调最高设计）属负责人裁决项，
>   未决前不在本页给出实现结论；

`acsd.phase1.wcs-platesolve.md` 与 `acsd.phase1.photometry.md` 同构新增（分别对应
WCS 解 / 星表匹配与测光定标），已改文件的逐字见对应卡。

**D2-7 `acsd.phase1.noise-snr.md` 序号消歧**

改前：

> - **判据实现 = `ln R` 的序统计量**：取最大的 `k` 使第 `k` 个跳变超过保留主体的
>   极差；保留主体下限 = max(⌈n/2⌉, 预算 patch 数）；……

改后：

> - **判据实现 = `ln R` 的序统计量**：取最大的跳变序号 `j` 使第 `j` 个跳变超过保留
>   主体的极差；保留主体下限 = max(⌈n/2⌉, 预算 patch 数)；……序号记作 `j` 而非 `k`，
>   以免与本模块背景方差口径的掩膜边缘残余系数 `k` 混名（后者的取值与出处正本 =
>   docs/science/noise_snr/NOISE_SNR.md 的常数表）；

**D4-2 `acsd.phase3.writer.md` 补正文角标**

改前（`:95-96`，节选）：`……HIPSID / RUNID / ORDERSEL / SAMPLER / SWVER + HISTORY、DATASUM（32-bit）；` `P3OutputResult` = `sha256[65]` ……`
改后：`……HIPSID / RUNID / ORDERSEL / SAMPLER / SWVER + HISTORY、DATASUM（32-bit）；上述头卡与数据模型的依据 = FITS 标准［1］［2］。`P3OutputResult` = `sha256[65]`……`

改前（整改项，节选）：`DATASUM 为 32-bit 数值校验和非 FITS 标准 ASCII CHECKSUM（如实冻结）。`
改后：`DATASUM 为 32-bit 数值校验和，不是 FITS 标准的 ASCII CHECKSUM 约定［1］（如实冻结）。`

---

## 2. 你否决（或部分否决）的条目 —— 保留审稿原判不抹除

> 以下只写「本车道为何不按字面执行」，审稿/前台的原判原文保留在派单与
> `run/GOVERN-08/审核包-R2/T06-*.md` 内，未做任何删除或改写。

| 编号 | 原判（派单/审稿口径） | 本车道处置 | 依据 |
|---|---|---|---|
| N-1 | 「若出现 `k = D_p/N_p` 而未写 `= pixfrac²`，补上」 | **部分否决**：补了 `= pixfrac²`，但写成**条件式**（平面极限严格、球面按残差律近似），并补残差量级与适用域 | `docs/science/algorithms/DRIZZLE_GEOMETRY.md`「面亮度保持权重的实现口径」段明写「恒等式 `A_drop,j = pixfrac²·A_pixel,j` 只在平面（仿射）极限下精确」并给出 `δ` 律与实测（θ=300″/px ⇒ 1.90e-7）；若写成无条件等式，会与同仓科学正本自相矛盾。`docs/GLOSSARY.md:12` 的 `k := D_p/N_p = pixfrac²` 也需同步（他车道） |
| N-2 | 「把『逐帧』措辞全部改掉」 | **部分否决**：改的是「参考通量**来源**」的措辞（不再把它写成数据派生的逐帧检出量），并补 `m_ref` 锚定与落盘；但保留了公式下标 `F_ref,k` 与「作用域名 `frame_independent_fixed_magnitude`」，因为合同正本自己就是这么写的 | `eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline.scope` 描述逐字写「frame_independent_fixed_magnitude = 固定参考星等 m_ref 在本帧的仪器通量 F_ref,k = 10^(-0.4*(m_ref-ZP_k))（**逐帧**、只依赖本帧标定，无「组」概念）」。代码禁止的是**以本帧检出通量中位数充当本帧 F_ref**（`snr_frame_science.cpp` 的 `reference_flux_adu` 必填 + 「per-frame median fallback removed」），不是禁止按 `m_ref` 逐帧换算。若照字面把「逐帧」全删，会与合同和代码同时相反 |
| N-3 | D1 判 `lib/infrastructure/cli/session_commands.h::config_fields(SESSION_NORMALIZE)` 与 `lib/infrastructure/cli/parser.cpp::session_keys()` 为「坏路径」 | **判「事实」不成立、判「写法」成立**：两个符号与枚举符都真实存在；只把 `file::symbol()` 伪路径写法改成「文件 + 符号名」（同时消除 `::` 造成的解析器误报） | `grep -n 'config_fields' lib/infrastructure/cli/session_commands.h` → `:144 inline const std::vector<ConfigField>& config_fields(SessionId s)`；`grep -n 'SESSION_NORMALIZE' …` → 多处命中；`parser.cpp:272 const std::set<std::string>& session_keys()` |
| N-4 | 脚本机械清单里的 `run/inspect/destroy`（`acsd.phase2.session.md:26`、`acsd.phase3.writer.md:118`） | **误报，不改** | 两处都是生命周期词串 `create/validate/run/inspect/destroy` 被脚本的 `run/` 前缀规则误捕；不是路径 |
| N-5 | 「两种参数化给出**逐位相同**的面亮度与逐像素方差」（`acsd.phase1.drizzle.md` 改前 `:92-93`） | **降级为「同一个 `c_jp`」+ 分别说明两种成立条件** | 我的数值夹具：两套公式独立复算得 `2.6999999999999997` vs `2.7000000000000002`（约 2 ulp），不是逐位相同；逐位相同只在「两个直写末端共用同一分母、走同一代码路径」的实现面成立 |
| N-6 | 建议「重建算子 Oracle」改指仓内真实落点目录 | **未新建、只撤回**：仓内确无 oracle 目录；已改为指向实验单元文档并如实写「仓内没有独立的 oracle 源码目录」，并保留原三条判据内容 | `find lib/algorithms -type d -name '*oracle*'` 零命中；`ls 实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md` 存在 |

---

## 3. 需代码侧订正的问题（本单不改代码，逐条登记）

| 编号 | 位置 | 问题 | 证据 | 影响 |
|---|---|---|---|---|
| C-1 | `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h` 头注记 | 注释把发布量口径锚到 `docs/science/DRIZZLE.md §5:50/§5:83`，而现行 `DRIZZLE.md` 只有 7 章、无 §5 的行级语义（锚漂移） | `grep -n '§5:50' …/astro_sphere_sink.h`；`grep -n '^#\{1,3\} ' docs/science/drizzle/DRIZZLE.md` | 注释误导，不影响数值 |
| C-2 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 样本掩膜段注记 | 注释里含历史叙事（「旧行为不再使用」一类），与 AGENTS §5「正文无历史叙事」的同款口径冲突（代码注释是否适用该条由负责人定） | 该段注记 | 规范一致性 |
| C-3 | `lib/infrastructure/aio/src/aio_api.cpp` | 单一全局精度位 `g_aio_precision_mode_fp64` 无法同时满足最高设计的「稠密大面单精度 / 稀疏元数据双精度」 | 该文件 `:34-51` | **待裁决**（见 D5-2） |
| C-4 | `lib/algorithms/coverage/tools/stage2.cpp` | 工具以裸整数占用 `acsd::ExitCode` 码值空间（3–7 重叠），`PUBLIC_API.md` 已登记为未决 | `PUBLIC_API.md`「acsd-stage2 工具返回值」段的 UNRESOLVED 段 | 已在上游登记，本车道只对齐表述 |
| C-5 | `lib/algorithms/drizzle/healpix_drizzle/*` | 帧/文件两通道错误码混用、无集中枚举（该模块自登记缺陷，本车道保留） | 该卡「已知限制」 | 低 |

---

## 4. 需权威补充才能定的问题

| 编号 | 问题 | 需补充什么 | 现状处置 |
|---|---|---|---|
| N-1（续） | `A_drop,j = pixfrac²·A_pixel,j` 在球面上是否按恒等式冻结 | 科学车道裁决：`docs/science/drizzle/DRIZZLE.md`「面积的两个来源」把该式写成恒等式，而 `docs/science/algorithms/DRIZZLE_GEOMETRY.md` 给出二阶残差律；两者需统一措辞（恒等式 + 残差项，或明确限定平面极限） | detail 侧写条件式并指向两者；未改任何 science 文档 |
| A-1 | `k = D_p/N_p = pixfrac²` 在 `docs/GLOSSARY.md:12` 也是无条件写法 | GLOSSARY 车道确认是否补球面残差与适用域 | 未改（不在写作用域） |
| A-2 | `snr.reference_mag` / `snr.reference_flux_adu` 的配置合同 | `eng/contracts/schemas/phase_config_normalize.schema.json` 内**未检出**这两个键（`grep -n 'reference_mag\|reference_flux' …schema.json` 零命中），而生产代码从 `doc["snr"]` 读它们 | 配置车道确认：键是否入 schema、域与默认值（缺省 6.0 等）写在哪张表 | detail 侧按代码行为写，并在配置表标注键名来源为代码 |
| A-3 | UPM 配置表的 `bkg_model` / `bkg_spline_spacing` | 这两个键是否曾在 schema 或已删键面存在（现仓内零命中） | 若曾存在，属删键未同步文档；若从未存在，本车道已按真实字段 `spline_degree` / `node_spacing_deg` 替换 | 已替换并在第 1 节 D2-5 写明 |
| A-4 | 重建算子 Oracle 的仓内可执行载体 | 是并入 `实验/absolute-snr` 的对拍表，还是新建仓内 oracle 源目录 | detail 侧写明「无独立 oracle 源码目录」，判据载体指实验单元 |
| A-5 | `docs/engineering/UNIFIED_OBJECTS.md` 的 `frame_snr` 行仍写「分子 `F_ref` 取**逐帧参考通量**」，并把红线指向不存在的路径 `docs/detail/normalize/modules/noise_snr` §4.1 | 工程车道确认该行措辞与失效路径 | 未改（不在写作用域） |
| A-6 | `docs/detail/UNIFIED_MODEL.md` 的 `sparse_snr_layer` / `sky_samples` 两行仍写「同**逐帧**参考通量 `F_ref`」 | detail 根级车道（同属 detail 但不在本写作用域）确认 | 未改 |

---

## 5. 文献核对

**核到一手（我本人执行）**

| 文献 | 核对方式 | 结论 |
|---|---|---|
| Greisen & Calabretta 2002, *Representations of world coordinates in FITS*, A&A 395, 1061–1075（Paper I） | Crossref API 取 `10.1051/0004-6361:20021326` 完整记录 | 题名、刊名、卷 395、期 3、页 1061–1075、发表 2002-11-18、作者顺序（Greisen, E. W.; Calabretta, M. R.）**逐项吻合** `acsd.phase3.wcs.md` 的 ［1］ |
| Calabretta & Greisen 2002, *Representations of celestial coordinates in FITS*, A&A 395, 1077–1122（Paper II） | Crossref API 取 `10.1051/0004-6361:20021327` | 题名、卷、期、页 1077–1122、作者顺序**逐项吻合** `acsd.phase3.wcs.md` 的 ［2］（我在 `acsd.phase3.wcs.md` 的 registry 冻结要点处补了正文角标 ［2］） |
| Pence, Chiappetti, Page, Shaw, Stobie 2010, *Definition of FITS, version 3.0*, A&A 524, A32 | Crossref API 取 `10.1051/0004-6361/201015362` | 五位作者、卷 524、页 A32、2010 年**逐项吻合** `acsd.phase3.writer.md` 的 ［2］ |
| IAU FITS WG, *FITS Standard*, Version 4.0 | 取永久链接 <https://fits.gsfc.nasa.gov/fits_standard.html> 实读 | 页面确认「Version 4.0 … formally approved by the IAU FITS Working Group on 22 July 2016」，另有 2018-08-13 的 language-edited 版本；`acsd.phase3.writer.md` 的 ［1］著录（2016 / Version 4.0）**成立**（可在该车道补注 2018 版，本车道未动） |

**转引仓内已核对正本（我未取原文，据此使用并标明）**

| 出处 | 我的用法 | 依据链 |
|---|---|---|
| F&H 2002（Drizzle, PASP 114, 144–152） | `acsd.phase1.drizzle.md` 核权重条的一手锚改为「式 (2)–(5) ＋ §7 式 (7) 后 `a + b = 1` 定义句」双锚 | 锚的取舍由 `docs/science/algorithms/DRIZZLE_GEOMETRY.md` 的 DISP-DRZ-009 与 `:49/:54` 给出（一手引文已由该正本核对）；`docs/science/drizzle/DRIZZLE.md` 的「核权重：按 drop 面积归一」载有逐字引文与核对说明 |
| drizzlepac `src/cdrizzlebox.c` 的 `do_kernel_square`（`dover /= jaco`） | 同卡核权重条 | `docs/science/drizzle/DRIZZLE.md`「参考实现」与 DRIZZLE.md 核权重节；**版本号已删**（仓内无任何处可核到版本，写 3.11.0 属无源） |

**核对不到 / 未由本车道核对**

| 项 | 状态 |
|---|---|
| `acsd.phase3.wcs.md` 的 ［1］（Paper I）在本卡正文中无对应断言 | 无正文角标。**未凭空补角标**（补一个不对应任何断言的引用＝编造）；如该车道认为需要，应先在正文写出确由 Paper I 支撑的断言 |
| 本车道未取 F&H 2002 / drizzlepac 的原始文件或原文 | 见上「转引」栏；已在正文保留上游正本作为落点 |
| 其余各卡的文献引用 | 未逐条核对（本车道只动了两张 phase3 卡的文献角标与 drizzle 卡的两处实现引文） |

---

## 6. 索引变更与真解析器验证输出

**索引变更**：本单**未新增/删除/改名任何文档**，因此
`docs/DOCUMENT_INDEX.yaml`、`docs/detail/00_INDEX.md`、各目录 README **零变更**。
目录实有 `docs/detail/registry/` = **25 张卡 + README**（`ls docs/detail/registry/*.md
| wc -l` → 26，其中 `acsd.*` 25 张）；`docs/detail/00_INDEX.md` 原写「26 张卡」，
我核对时该文件已被并发车道改为「25 张卡 + README」，与目录一致 —— **本单未改该文件**。

**改前**：`python3 /tmp/verify_anchors.py docs/detail/registry/*.md`（合计 **13** 处）

```text
=== docs/detail/registry/acsd.phase1.calibration.md : 1 处 ===
  L72    FILE-MISS  lib/algorithms/calibration/{include,src}/
           > `lib/algorithms/calibration/{include,src}/`（CMake 目标 `acsd_calibration`：
=== docs/detail/registry/acsd.phase1.cosmetic.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.drizzle.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.hips-writer.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.noise-snr.md : 2 处 ===
  L344   FILE-MISS  lib/algorithms/noise_snr/{include/acsd,
           > `lib/algorithms/noise_snr/{include/acsd, src, wrapper_phase1, cpp}/`。
  L437   FILE-MISS  lib/algorithms/integration/phase2_integrate/oracle/
           > - **重建算子 Oracle**（`lib/algorithms/integration/phase2_integrate/oracle/`）：
=== docs/detail/registry/acsd.phase1.photometry.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.session.md : 1 处 ===
  L58    FILE-MISS  lib/phase1_session/p1_session.{cpp,h}
           > `lib/phase1_session/p1_session.{cpp,h}`。
=== docs/detail/registry/acsd.phase1.star-detection.md : 2 处 ===
  L124   FILE-MISS  lib/infrastructure/cli/session_commands.h::config_fields(SESSION_NORMALIZE)
           > `lib/infrastructure/cli/session_commands.h::config_fields(SESSION_NORMALIZE)` 与
  L125   FILE-MISS  lib/infrastructure/cli/parser.cpp::session_keys()
           > `lib/infrastructure/cli/parser.cpp::session_keys()`。**缺段不是错误**，全取编译期
=== docs/detail/registry/acsd.phase1.star-psf.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.wcs-platesolve.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.writer.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.coverage.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.integrate.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.reject.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.resample.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.sample.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.session.md : 1 处 ===
  L26    FILE-MISS  run/inspect/destroy
           > p2_session_create/validate/run/inspect/destroy）+ C++
=== docs/detail/registry/acsd.phase2.upm-apply.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.upm-fit.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.write.md : 3 处 ===
  L94    FILE-MISS  lib/algorithms/coverage/{src,
           > `lib/algorithms/coverage/{src, include/astro/phase2, tools, tests}/`。原头文件族里
  L140   FILE-MISS  eng/packaging/config/CLI
           > 退出码 2 = eng/packaging/config/CLI、3 = coverage / target_order、4 = frame_id /
  L169   FILE-MISS  lib/algorithms/coverage/src/acr_kernels.cpp
           > （**无 CUDA kernel**）随 `lib/algorithms/coverage/src/acr_kernels.cpp` 一并删除，
=== docs/detail/registry/acsd.phase3.properties.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.resample2.md : 1 处 ===
  L228   FILE-MISS  docs/detail/registry/acsd.phase3.
           > - 会话编排面现行权威 = docs/engineering/contracts/RUNTIME.md + docs/detail/registry/acsd.phase3.*
=== docs/detail/registry/acsd.phase3.verify.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.wcs.md : 1 处 ===
  L205   FILE-MISS  docs/detail/registry/acsd.phase3.
           > - 会话编排面现行权威 = docs/engineering/contracts/RUNTIME.md + docs/detail/registry/acsd.phase3.*
=== docs/detail/registry/acsd.phase3.writer.md : 1 处 ===
  L115   FILE-MISS  run/inspect/destroy
           > create/validate/run/inspect/destroy + last_error 脱敏，同一签名头）；
=== docs/detail/registry/README.md : 0 处 ===

合计 13
```

**改后**（同命令，合计 **2** 处，两处均为第 2 节 N-4 的误报）

```text
=== docs/detail/registry/acsd.phase1.calibration.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.cosmetic.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.drizzle.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.hips-writer.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.noise-snr.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.photometry.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.session.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.star-detection.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.star-psf.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.wcs-platesolve.md : 0 处 ===
=== docs/detail/registry/acsd.phase1.writer.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.coverage.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.integrate.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.reject.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.resample.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.sample.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.session.md : 1 处 ===
  L26    FILE-MISS  run/inspect/destroy
           > p2_session_create/validate/run/inspect/destroy）+ C++
=== docs/detail/registry/acsd.phase2.upm-apply.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.upm-fit.md : 0 处 ===
=== docs/detail/registry/acsd.phase2.write.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.properties.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.resample2.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.verify.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.wcs.md : 0 处 ===
=== docs/detail/registry/acsd.phase3.writer.md : 1 处 ===
  L118   FILE-MISS  run/inspect/destroy
           > create/validate/run/inspect/destroy + last_error 脱敏，同一签名头）；
=== docs/detail/registry/README.md : 0 处 ===

合计 2
```

**辅助自检（三条，全部通过）**

```text
$ grep -n '一节\|见 §\|曾记\|旧版\|作废\|383088f2\|::config_fields\|::session_keys' docs/detail/registry/*.md
（无输出）

$ grep -n '\.cpp:[0-9]\|\.h:[0-9]\|\.md:[0-9]\|#L[0-9]' docs/detail/registry/*.md
（无输出 —— registry 全树无源码行号锚）

$ python3 - <<'EOF'   # UTF-8 解码 + 替换字符检查 + 表格竖线一致性
encoding problems: none
table check done      # 唯一两条“不一致”是既有的转义竖线 pos\|neg / nearest\|bilinear，合法
EOF
```

---

## 7. 自证段

**我实际做了什么**

1. 按 AGENTS §3 的顺序读了 `AGENTS.md`（会话内全文）、`docs/ACSD_DESIGN.md` 第 2/3/4/8 章、
   `docs/detail/00_INDEX.md`、`docs/detail/UNIFIED_MODEL.md`；
2. 跑改前基线 `verify_anchors.py` 得 13 处机械告警，**逐条人工判读**（不照单全收）：
   真缺陷 11 条、误报 2 条（N-4）；
3. 为每条改动取一手依据：读了 `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp`、
   `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `F_ref` 派生段、
   `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的样本掩膜段、
   `astro_sphere_sink.h` 的 `k` 定义、`lib/algorithms/coverage/include/astro/phase2/upm.h` 与
   `integrate.h` 的头注记、`sky_plane.h` 的配置结构、`lib/phase2_session/p2_session.cpp`、
   `eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline`；
4. 公式自查：`k = D_p/N_p = pixfrac²` 自己逐步推导（见 1.1）并写数值夹具
   （`/tmp/k_fixture.py`，2000 组随机配置，最大相对偏差 3.6e-16；两套参数化面亮度差 ~2 ulp），
   据夹具结果**把原卡「逐位相同」降级**；
5. 文献自查：Crossref API 取 3 条 DOI 的完整著录、IAU FITS WG 永久链接实读 1 条，逐项比对；
6. 改了 15 张卡（D1-1…D1-15、D2-1…D2-7、D3-1…D3-7、D4-2、D5-1…D5-2）；
7. 改后复跑同一解析器 + 三条辅助自检（第 6 节）。

**我没做什么（逐条说明）**

1. **未做任何 git 写操作**（无 add / commit / checkout / reset / stash / tag / clean）；
   读 git 只用 `git -c core.quotepath=false diff --stat` 与 `status --porcelain`。
2. **未改写作用域外的任何文件**：`docs/ACSD_DESIGN.md`、`docs/GLOSSARY.md`、
   `docs/README.md`、`docs/DOCUMENT_INDEX.yaml`、`docs/detail/UNIFIED_MODEL.md`、
   `docs/detail/00_INDEX.md`、`docs/detail/infrastructure/**`、`docs/anchors/**`、
   所有 `docs/science/**`、`docs/engineering/**`、任何代码与配置，一律未动。
   （工作树中这些文件的改动来自并发车道，不是本单；本单只用 edit 工具碰过
   `docs/detail/registry/*.md`。）
3. **未单方面改任何精度数值面**（D5）：只加了归属声明与待裁决登记。
4. **未改 science 侧任何口径**，即使我认为上游需要改（N-1、A-1、A-5、A-6 都只登记）。
5. **未编造**：删掉了核不到来源的 drizzlepac 版本号；`bkg_model` / `bkg_spline_spacing`
   两个查不到载体的字段没有猜替代语义，而是换成签名头里真实存在的字段并说明；
   无法核到的（`snr.reference_mag` 是否入 schema、`acsd.phase3.wcs.md` 的 ［1］ 是否该加角标）
   一律登记为「需补充」而不是补写。
6. **未为消问题而删真内容**：所有退场叙事都改写为现在时事实并保留原判断
   （「输出仅 signal / support 不再是现行限制」「ACR 侧无仓内实现 ⇒ 该等价项当前不可执行」）；
   drizzle 的「守恒因子恒为 1」「两种参数化同一 `c_jp`」等结论一条未删。
7. **未删除任何审稿条目**：第 2 节的否决都写明依据并保留原判表述。

**诚实边界（本单不能保证的部分）**

- registry 内仍有约 280 处 `§N` 形式的节号引用，绝大多数是「`<文件> §N（<真实标题>）」」
  形态（已带人可读标题，符合「自然语言点名文件与标题」）。我只处理了**裸跳锚**
  （`见 §N` 无标题）、`**一节**`、指代性「该节」与 `file::symbol()` 伪路径四类；
  把全部 280 处节号改成标题化是跨全卡的机械大改，风险与收益不匹配，**明确交回前台决定**。
- 改后仍剩 2 条 `FILE-MISS`，是脚本对 `run/inspect/destroy` 的规则性误报（N-4），
  我**没有**为了清零而改写正确的生命周期表述。
- `docs/detail/00_INDEX.md` 的卡片计数由并发车道在本单执行期间从「26 张卡」改为
  「25 张卡 + README」，与目录实有张数一致；**本单未改该文件**（不在写作用域）。
- 各车道正在并发改 `docs/science/**`、`docs/engineering/**`、`docs/detail/**` 根级文件；
  本单的交叉引用按**当时读到的内容**写入，若上游随后改动（例如
  `DRIZZLE.md` 章节名、`frame_snr.schema.json` 字段名），需在合并前复核一次。