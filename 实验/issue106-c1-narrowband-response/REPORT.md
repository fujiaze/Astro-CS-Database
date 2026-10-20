# Issue #106（C1）参数化窄带响应生产实施 — 证据报告

- 任务：[#106 ACSD-CPP/C1] Gaia XP 生产窄带参数化与精确响应积分
- 上级：[#105 总任务板]；科学设计：[#104 已审核]；真实数据节点：[#103]
- 实施基线：`ba81a1631e006ccb4fe2684d2e1356c2e293213a`（= 本轮起点 main）
- 环境：Linux x86_64、g++ 14.2.0、CMake 3.31.6、Makefiles、16 核；NumPy 2.2.4、SciPy 1.15.3
- 数据：仓内 `gaia/GaiaDR3SP`（真实 Gaia DR3 XP 光谱库）、`config/response_curves`（58 条真实曲线，含 `KAF-16803` QE、`Ideal QE curve`、`Baader 7nm H-alpha`、`Astrodon E-series R`）
- **无用户真实 FITS/HST** ⇒ 本报告只覆盖数学、合成数据与可执行 C++ 验证；真实数据验收属 #103

本报告分两轮：**第一轮** = C1 实施（`67f15831d`/`426ed016e`/`a60a5d514`）；**第二轮** = 独立源码审核（2026-10-20 04:16Z 评论）的 P1 阻断项修复 + P2 订正（见 §6）。

## 1. 改动范围（生产路径接线）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/photometry/cpp/src/spectrum_integrator.h/.cpp` | 新增参数化响应的**边界精确分段求积**（`make_parametric_band` / `prepare_segmented_response_cache` / `compute_f_syn_segmented_xpsd`）；Akima 重构为 `AkimaSpline`（`akima_interpolate` 行为**逐位不变**，A/B 见 §3） |
| `lib/algorithms/photometry/cpp/src/pc_api_qf.h` / `pc_api.cpp` | C++ 链接期入口追加可选 `ParametricBand*`；6 个冻结 C 导出签名/行为不变（传 nullptr） |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h/.cpp` | `ResponseSpec`/`ResponseIdentity`；按 mode 分发（参数化**不查滤镜库**、QE 仍强制）；响应身份与覆盖状态 |
| `lib/algorithms/photometry/cpp/src/filter_curve_json.h` | 曲线**有序样本内容** SHA-256（`curve_content_sha256`），替代 n/sum/sumsq 作为身份 |
| `lib/infrastructure/scheduler/src/photometry_response_config.h`（新） | 响应来源 one-of 的**站点解析唯一实现**（可单测） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | `p1_op_photometry` / `p1_resolve_frame_binding`：配置解析、逐帧绑定、`RESPONSE_SOURCE_CONFLICT`、provenance |
| `lib/infrastructure/scheduler/src/normalize_precision.cpp` + `lib/include/acsd/normalize_precision.h` | 帧侧 `photometry.response_id` / `application_id` 纳入响应 id |
| `eng/contracts/schemas/phase_config_normalize.schema.json` | `photometry.fit.response` + `photometry.frame_overrides[].response`（tagged one-of） |
| `CMakeLists.txt` | `acsd_phase1_photcal` 链接 `acsd_common`（SHA-256 单一实现） |
| `docs/`（4 个正本/工程文档） | 只更新**接线状态**与配置口径，不改科学定义 |
| **第二轮**：`lib/infrastructure/scheduler/src/photometry_frame_identity.h`（新） | 逐帧响应身份的落盘序列化 / 解析 / **解析规则**（per_frame / group_legacy / unavailable）单一实现（生产与单测共用） |
| **第二轮**：`lib/algorithms/shared/canonical_double.h`（新） | 无损浮点规范串（hexfloat）唯一实现；`normalize_precision.cpp` 与 `frame_photometry_fit.cpp` 共用 |
| **第二轮**：`frame_photometry_fit.{h,cpp}` | 响应身份**分列**（物理 `response_id` + 合成 `synthesis_id` + XP 网格）；规范串浮点改无损；**修复第一轮打断旧 measured 路径的回归** |
| **第二轮**：`normalize_precision.{h,cpp}` | 施加身份 `photoapplied:v2` → **v3**（逐帧有序内容摘要 + 逐帧物理响应 id） |
| **第二轮**：`module_adapters.cpp` | 光度节点逐帧落盘响应身份；Drizzle 逐帧取身份；`n_passband_identity_mismatch` 改按 `response_id` 判 |
| **第二轮**：`eng/tests/unit/frame_response_identity_test.cpp` + runner（新） | 逐帧响应身份判据（34 项），含旧 measured 路径回归判据 |

生产调用链：
`p1_op_photometry` → `frame_photometry_fit::fit_frame_photometry` → `pc_calibrate_simple_with_gaia_f64_v2_qf` → `run_with_gaia_impl<T>` → `compute_f_syn_segmented_xpsd`（参数化）/ `compute_f_syn_cached_xpsd`（measured）。

## 2. 数学依据

- F_syn 定义不变：`F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ`（XPMODEL 原生积分单位，**不除 hc**）。
- 参数化窄带：`T(λ)=1` 当 `c−Δ/2 ≤ λ ≤ c+Δ/2`，否则 0；峰值归一为 1、FWHM=Δ（PHOTOMETRY.md §2.1）。
- 求积：断点 = 解析端点 ∪ XP 采样点 ∪ QE 插值节点；每段 4 点 Gauss–Legendre。
  **精确性**：Akima 在节点区间内是三次多项式 ⇒ 段内 `F(λ)·Q(λ)·λ` 是 ≤7 次多项式；4 点 GL 对 ≤2n−1=7 次多项式精确 ⇒ **求积误差 = 0**（仅浮点舍入）。
- 误差分账：求积误差（=0）｜XP 重构误差（Akima）｜XP 自身分辨率/外定标｜**响应形状误差**（top-hat 是具名假设，PARAMETRIC_APPROX ≠ MEASURED_CURVE）。

## 3. 独立 Oracle 与数值对拍

| 对拍 | 参照实现 | 结果 |
|---|---|---|
| Akima 重构无回归 | 重构前实现逐字拷贝（测试内） | **200/200 随机 + 边界用例逐位相等** |
| 分段求积（闭式解） | 独立解析积分（二次 F × 线性 Q ⇒ 四次被积函数） | 相对误差 **1.16e-16** |
| 分段求积（SciPy） | `scipy.interpolate.Akima1DInterpolator` + `scipy.integrate.quad` | 3/5/7 nm Hα（真实 KAF-16803 QE）相对误差 **≤ 5.0e-16** |
| 生产路径（真实 XP） | 172 颗**真实 Gaia DR3 XP 光谱星** | 生产 F_syn vs SciPy 复算 **max rel = 5.045e-16**（median 1.19e-16） |
| 生产自洽 | 注入 `F_instr = F_syn·c`（c=1e-3） | 恢复 `k_photo = 1/c`，相对误差 **0.000e+00**，残差散度 **0.000e+00** |

**#43 反例（可复现）**：同一 top-hat 走历史 2 nm 粗格路径 vs 精确值 ⇒ 3 nm **55.6%**、5 nm **33.3%**、7 nm **4.8%**；且 5 nm 与 7 nm 在粗格路径上给出**完全相同**的 F_syn（`3.4986666667e-03`），而精确值相差 40%。真实 `Baader_7nm_H_alpha` 曲线的粗格路径误差 **1.10%**。
生产路径同输入对比：参数化 `k_photo=2.040202e-14` vs measured 等价矩形 `k_photo=9.082279e-15`，比值 2.2464，与逐星 F_syn 比的中位数倒数 2.2468 一致 ⇒ 响应确实进入了生产 F_syn 计算。

> 措辞订正（第二轮 P2-3）：历史 2 nm 路径的 F_syn 是精确值的 **0.4451 倍**，即**相对精确值低估 55.5%**；「相对该实测值需提高 124.7%」是**另一分母**的表述，两者不可混用。已改 `narrowband_production_oracle.py` 的输出措辞（数值本体未变）。

## 4. 执行命令与退出码

```
cmake --build build/linux-control --target acsd -j 16            # 0
python3 -m eng.tests.unit.run_unit                                 # 0（160/160）
python3 eng/tests/unit/run_simpson_integrator_domain_test.py       # 0（62/62）
python3 eng/tests/unit/run_photo_identity_test.py --build-dir build/linux-control  # 0
python3 eng/tests/unit/run_narrowband_response_test.py             # 0（252/252）
python3 eng/tests/unit/narrowband_response_oracle.py               # 0
python3 eng/tests/unit/run_narrowband_production_path_test.py      # 0（16/16）
python3 eng/tests/unit/narrowband_production_oracle.py             # 0
python3 eng/tests/unit/narrowband_response_config_test.py          # 0（15/15）
python3 eng/tests/unit/run_photometry_response_config_test.py     # 0（40/40）
python3 eng/tests/unit/run_frame_response_identity_test.py         # 0（34/34）★第二轮新增
python3 eng/tests/conformance/normalize_frame_science_schema_test.py  # 0（18 PASS / 0 FAIL）
```

生产 E2E（真实 M42 T2 帧, 两帧不同响应; 详见 §6）：

```
./build/linux-control/acsd normalize --json evidence/production_p1fix_ab_config.json -y
# A 组（修复前二进制 a60a5d514）: exit 0 ⇒ 两帧 frame_science.response_id 相同（首帧污染）
# B 组（本轮修复后）:            exit 0 ⇒ 两帧 frame_science.response_id 互不相同
```

日志见 `evidence/`。

## 5. 回归与性能

- measured 路径**逐位不变**（Akima A/B 200/200；`simpson_integrator_domain_test` 62/62；既有 160 项单元集全绿）。
- 施加身份串**版本升级**（第二轮，见 §6-P2）：`photoapplied:v2:` → `photoapplied:v3:`。
  **v2 与 v3 的 id 不可混用**：v3 的参与量改为**逐帧有序内容摘要**与**逐帧物理响应 id**，
  同一物理情形在两版下得到**不同** id。第一轮报告的「既有产品身份逐字不变
  `photoapplied:v2:455519fe…`」**在本轮起不再成立**（口径已按审核要求订正，见 §6）。
- 性能（同一批 343 点 XP 谱，2000 次重复，`--bench` 模式，g++ -O1）：参数化分段路径 **2.56–2.59 µs/星**，
  历史 measured 路径 **1.70–1.73 µs/星** ⇒ 开销约 **1.5×**（≈0.85 µs/星，来自逐星重建 f_λ 的
  Akima 样条；3 nm 带只切 2 段）。该比值随机器负载在 **1.3–1.5×** 间波动（第一轮同机测得 4.45/3.41 µs/星、比值 1.30）⇒ 结论只取「同一量级、无数量级差异」。5000 星/帧 ⇒ 每帧多约 4 ms，对 Normalize 无实际影响。

## 6. 独立审核 P1/P2 修复（第二轮，2026-10-20 04:16Z 评论）

审核裁决：核心求积条件通过，但 **P1 阻断**（跨帧响应身份被首帧组级值污染）⇒ #106 不关闭。
本轮按下述三条要求做**手术刀修复**，并补最小测试。

### P1-1 逐帧身份传播（Drizzle 侧）

- **根因**：`p1_op_drizzle` 读**块级** `p1_phot.json.passband_identity.response.response_id`
  （首个拟合成功帧冻结）来构造每帧的 `photo_response_identity` / `photo_application_identity`。
- **修复**：Drizzle 改为读 `p1_phot.json#photscale_fit[frame_key].response` 的**逐帧**记录，
  用该帧的 `response_id` / 滤镜与 QE 库键 / **有序样本内容**摘要构造逐帧身份；
  组级 `passband_identity` 降级为**摘要**（新增 `scope=group_summary` 标记）与**旧产物兼容回退**。
- **缺项语义**：逐帧记录缺失 ⇒ 具名 `response_identity_unavailable`
  （`frame_science.photometry.response_id` 置 null、`reason` 记明、施加身份不含响应参与量），
  **不**按第一帧静默代入；只有「整份产物无任何逐帧记录」的旧产物才允许一次组级回退，
  且标记 `group_legacy`（可机检）。
- 新增单一实现：`lib/infrastructure/scheduler/src/photometry_frame_identity.h`
  （落盘序列化 / 解析 / 解析规则；生产与单测共用同一份）。

### P1-2 逐帧诊断判据

- `n_passband_identity_mismatch` 的判据由「`filter_key`/n_points/范围」改为**逐帧真实响应
  身份 `response_id`**。旧判据在参数化窄带上**恒等**（filter_key 空、数量/范围全 0）⇒ 混响应
  检不出；且「同滤镜不同 QE」也不计数。新判据覆盖不同滤镜、不同 QE、参数化不同中心/带宽三类。
- 合法混通带**仍不判失败**（N04 裁决：帧间独立，差异只作具名诊断）。

### P1-3 最小测试（两帧不同滤镜 / 不同 QE）

新增 `eng/tests/unit/frame_response_identity_test.cpp` + `run_frame_response_identity_test.py`
（**34** 项判据全过，证据 `evidence/frame_response_identity.log`）：

- **真实** Gaia DR3 XP 光谱 + **真实**曲线库下跑 **6 次生产拟合**（生产入口 `fit_frame_photometry`）：
  A = measured `Baader 7nm H-alpha` + QE `KAF-16803`；B = measured `Astrodon E-series R` + 同 QE
  （**不同滤镜**）；C = measured A + QE `Ideal QE curve`（**同滤镜不同 QE**）；D = 参数化 500/5 nm；
  E = 参数化 656.28/3 nm；A2 = A 的重复（确定性）；F = **旧 measured 路径**（`req.filter_name`）。
- 断言：各 `response_id` 互不相同（A≠B、C、D、E）且 A=A2（可复现）、F=A（旧形态与新形态同曲线 ⇒ 同身份）；
  经**生产同一份**落盘/解析实现 + 生产身份函数后，两帧的 `frame_science.response_id` 与
  `application_id` **不同**（不同滤镜、不同 QE、参数化+measured 混合三类各一项）；
- 帧失败（无逐帧记录）⇒ 具名不可用且**不借用他帧**；旧产物 ⇒ 组级回退且标记 `group_legacy`；
  `application_id` 对**仅响应变化**敏感；身份是**内容**身份（曲线文件字节摘要不入串）；
  无损浮点规范串对相邻 binary64 单射。

### P2（非阻断，同轮订正）

1. **无损浮点规范串**：`response_id` 规范串的浮点写法由 `%.12g` 改为**无损 hexfloat**
   （新唯一实现 `lib/algorithms/shared/canonical_double.h`；`normalize_precision.cpp` 改为薄封装）。
   原写法会把 `656.28` 与 `nextafter(656.28,+∞)` 折叠成同一串 ⇒ 两个不同响应共享同一身份。
2. **物理响应身份与合成模型身份分列**：`response_id`（物理：形状 + 滤镜/QE 有序内容）
   不再吸收 XP 覆盖/积分约定/XP 网格 ⇒ **换 XP 网格不换「滤镜身份」**；
   新增 `synthesis_id`（合成模型：物理响应 + 覆盖 + 求积 + XP 网格）作为**标定 provenance**，
   逐帧与组级均发布，并写入 `frame_science.photometry.scale_model_id`
   （该字段此前恒为 null，而 schema 对 `relative_only` 要求非空 ⇒ 顺带修正了一处产品不符合）。
3. **报告措辞**：见 §3 的 124.7% / 55.5% 分母订正（数值本体未变）。

### P0′ 生产 E2E 发现并修复的**第一轮回归**（旧 measured 路径被打断）

第一轮（`67f15831d`）的响应来源分发把 `req.response.mode` **归一**为 `kMeasuredCurve` 后再用它
决定滤镜曲线名取自 `req.response.curve_name`；而**旧路径**（`photometry.fit.filter` /
块级 `filter_passband` / 逐帧 `override.filter`）的曲线名在 `req.filter_name` ⇒ 归一后读到
**空的** `curve_name` ⇒ 拟合入口具名失败「filter_name required（photometry.fit.filter /
filter_passband）」⇒ **旧 measured 路径在生产上不可用**。

- **发现途径**：本轮的生产 E2E（真实 M42 T2 帧 + 真实曲线库）第一次把旧路径真正跑起来，
  运行以全局配置失败中止。第一轮单测**没有**覆盖旧路径形态（全部走新 `response` 对象或
  直接给 `req.response.mode=kMeasuredCurve`），故第一轮「旧路径行为逐位不变」的登记
  **不成立**——本报告在此订正。
- **修复**：`filter_key` 改按**请求声明的来源**取（`req.response.mode == kUnspecified`
  ⇒ `req.filter_name`；`== kMeasuredCurve` ⇒ `req.response.curve_name`），归一后的局部
  `mode` 不再参与来源判别。
- **回归判据**（新增于 `frame_response_identity_test.cpp` 的用例 F）：旧形态
  （`req.filter_name`）与新形态（`measured_curve.curve_name`）指向同一条曲线时，
  **都必须跑通**且给出**同一** `response_id` 与**同一** `k_photo`。

### P1 修复的**生产 A/B**（真实 M42 T2 帧, 同一 block 两帧不同响应）

配置见 `evidence/production_p1fix_ab_config.json`（真实 M42 T2 亮帧 + 真实 `KAF-16803`
QE + 真实 `Baader 7nm H-alpha` 曲线）：帧 M2 = measured_curve（块级声明），帧 M4 =
parametric_narrowband 656.28/3 nm（逐帧 override）。A 组 = 修复前 `a60a5d514` 的二进制，
B 组 = 本轮修复后；同机、同数据、同配置（`acsd normalize --json … -y`，两次退出码 0）。

| | A 组（修复前） | B 组（修复后） |
|---|---|---|
| 帧 M2 `frame_science.photometry.response_id` | `Baader 7nm H-alpha@118b002dd883+KAF-16803@03256b2a8ae6+resp:response:v1:23e5e7e3…` | `Baader 7nm H-alpha@c2397c9c5ff8+KAF-16803@18a324d6ce93+resp:response:v1:c6071a1d…` |
| 帧 M4 `frame_science.photometry.response_id` | **与 M2 完全相同**（= 首帧组级值） | `KAF-16803@18a324d6ce93+resp:response:v1:1cc67094…`（本帧参数化响应） |
| 帧 M4 `application_id` | `photoapplied:v2:493ea515…`（响应部分同 M2） | `photoapplied:v3:fe395dca…`（含本帧响应 id） |
| `scale_model_id` | `null` | `synthesis:v1:…`（逐帧） |
| `p1_phot` 逐帧 `response_id` | 两帧**本就不同**（逐帧记录一直正确） | 同左（不变） |
| `n_passband_identity_mismatch` | 1 | 1 |

⇒ 审核给出的最小反例在**生产**上被复现（A 组）并被消除（B 组）；两组的差异只在
Drizzle 取身份的那一处。原始输出：`evidence/production_p1fix_ab.log`。

> 口径说明：`n_passband_identity_mismatch` 在 A 组也是 1（该组是 measured vs parametric，
> 旧判据因 filter_key/n_points/范围不同而命中）；旧判据**检不出**的是「两帧都是参数化
> 但中心/带宽不同」与「同滤镜不同 QE」两类 —— 这两类由新的 `response_id` 判据覆盖
> （见 `frame_response_identity_test.cpp` 的 B/C/D/E 四个用例）。

### 本轮未闭合（仍不宣称已验证）

- 响应**形状**误差、#43 实测曲线粗格求积、#44 XP 域外通量、窄线谱 SED 偏差：同第一轮，未变。
- 真实 FITS/HST/M42 全链：属 #103，本报告不代表真实数据科学验收。
- `photoapplied:v3` 与 v2 身份**并存期**的迁移策略（是否需要双读）：本轮未实现；
  现有 v2 产品只能在重新计算后得到 v3 身份（**无**就地迁移路径）。

## 7. 仍未闭合（不宣称已验证）

1. **响应形状误差**：top-hat 是具名工程近似；OD5+ 不证明带内为矩形；快光锥蓝移/温漂不可由 c/Δ 恢复。
2. **实测曲线的 2 nm 粗格求积误差**（#43）**仍 OPEN**：本轮只对参数化模式给精确路径。
3. **XP 覆盖/截断**已发布状态（full/xp_truncated/qe_truncated/no_overlap），但 XP 域外真实通量/外定标误差未建模。
4. **窄线谱（Hα/[N II]）对未知星云 SED 的不可消除偏差**（PHOTOMETRY.md §3.1）不因本轮改变。
5. **真实 FITS/HST/M42 全链**属 #103；本报告**不**代表真实数据科学验收。

## 8. 证据文件

`evidence/` 内为各命令的原始输出（文本，可复算）。逐星生产数据（真实 XP 光谱字节 + 两条 F_syn）不入库，脚本按需重生成（`run_narrowband_production_path_test.py` → `/tmp/narrowband_production_path.json`）。
