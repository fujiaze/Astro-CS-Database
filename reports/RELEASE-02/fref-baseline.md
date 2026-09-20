# FREF-BASELINE：帧级 SNR 参考通量基准定案（候选 A 固定参考星等 vs 候选 B 绝对功率信噪比）

> 任务：AstroCS RELEASE-02 / 帧级 SNR 基准分片（FREF-BASELINE）
> 依据裁决：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.49（定案 1/2/5/6/7）、§9.50（负责人续裁）
> 改动域：`lib/**`（SNR 参考通量相关）、`contracts/**`、`tests/**`。**未改 `docs/**`**（订正建议见 §7）。
> 证据落点：`run/RELEASE-02/fref-baseline/`（bin/obj/lib/logs/work/e2e/analysis.json）

---

## 0. 结论（先说结果）

| 项 | 定案 |
|---|---|
| 候选 A（固定参考星等 `m_ref`） | **采用**。`F_ref,k = 10^(-0.4*(m_ref - ZP_k))`，`m_ref=6.0` |
| 候选 B（绝对功率信噪比） | **否决**。无标准定义，且量纲/标度随仪器变化，跨帧不可比（§2 数值反例） |
| 帧间独立性（§9.49 定案 2） | **满足**。`F_ref,k` 只依赖本帧自身标定；**已删除**跨帧 `k` 离散度门，"组"概念在固定星等路径下不存在 |
| 配对性（WEIGHT-SCI-001） | **保持**。头部公共锚 `reference_flux_common` 与逐帧 `F_ref,k` 满足 `F_ref,k · k_photo,k = F0`（实测偏差 ≤ 4.3e-4，§5.3） |
| 判别性测试 | `tests/unit/p1snr/p1snr_fref_baseline_test.cpp`：**16 checks, 0 failures**（旧形态 3 项红 + 新形态 13 项绿） |
| schema | `DATA-P1-SNR` 由 `/2` 升至 `/3` |

**一句话**：帧级 SNR 的分子必须是一个**统一的物理参考电平**，否则各帧报的是亮度不同的参考星的 SNR，帧间不可比；该参考电平取"固定星等 `m_ref` 在本帧的仪器通量"，而公共锚取与帧无关的合成零点 —— 两者靠 `k_photo` 严格配对。

---

## 1. 问题与裁决要求

`reverse_verify/docs/frame-snr-canon.md` 定义帧级 SNR 为 `SNR_k(F_ref) = F_ref / sigma_F,k`，并把 `F_ref` 定为**跨帧可比性的锚**（该文件 line 133-134）。但旧实现（`DATA-P1-SNR/2`）的 `F_ref` 是**块级检出通量中位数**（`reference_flux_source="group_median"`），产生两个问题：

1. **帧间不独立**：`F_ref` 是整块数据算出来的，换一批帧就变（违反 §9.49 定案 2「帧间独立」）；
2. **无物理含义**：§9.48 已查明 34.5 万"源"中绝大多数是星云/PSF 翼/平场残差等非星结构，取其中位数等于用一个**结构相关的数**当参考电平。

G11 记录了这一现象：`t3_m4_red` 的 `F_ref` 组内跨度 2.14×。

负责人裁决要求：用**绝对功率信噪比**之类替代，或**直接用 6 等星**（或任一正常星等数值）做基准；并且**极度异常值拒绝并抛错，其余合理范围都接受**；**帧间独立，不做组间对比**；**不同光学系统的帧塞进来不应全报错**。

---

## 2. 候选 B（绝对功率信噪比）评估 —— 否决，附数值反例

### 2.1 文献查证（子代理 `910dde7b` 独立完成）

| 查证项 | 结果 |
|---|---|
| "绝对功率信噪比" 精确短语（OpenAlex / Startpage） | **0 命中**，天文学无此标准术语 |
| 天文学一手定义 | 全部是**幅度信噪比** `F/σ_F`：Howell 1989 PASP 101,616 eq.(1)；SExtractor `FLUXERR`；photutils；SEP；Schroeder |
| `SNR_power = SNR_amp²` | 只在**信号处理**领域成立，天文测光不使用该口径 |
| 跨观测可比性 | 必须**绝对定标**（Jy/K、SEFD、PHOTFLAM）才可能 |

### 2.2 量纲反例（决定性）

`1/sigma_F` 的量纲是 `[1/ADU]`（∝ 增益）或 `[s/e⁻]`（∝ 曝光），**不是无量纲数**：

| 场景 | `1/σ_F`（候选 B 的口径） | `F/σ_F`（候选 A） |
|---|---|---|
| 基准（σ_F=118 ADU） | 8.475e-3 | 不变 |
| 增益 ×2（2 e⁻/ADU） | **4.237e-3（减半）** | **不变** |
| ADU 标度 ÷4 | **3.390e-2（×4）** | **不变** |

同一个天体的"绝对功率信噪比"随读出增益/ADU 标度任意变化，而 `F/σ_F` 恒等。**候选 B 无法作为跨帧基准**，因为帧级 SNR 的全部意义就是跨帧可比。

### 2.3 帧级 SNR 口径的既有约束

`reverse_verify/docs/frame-snr-canon.md` line 453 的 G11 与 canon 正文均要求：帧级 SNR **无量纲**、分母只用天光散粒噪声（增益 ≤0 分支下 `σ_F` 与通量无关）、**不得被天光抬高**。候选 B 全部违反。

**判定：候选 A 胜出。**

---

## 3. 候选 A 的定案设计（FREF-BASELINE-001）

### 3.1 公式链

```
ZP_syn  = median_i( magG_i + 2.5*log10 F_syn,i )        # 绝对合成零点 [mag]，与帧无关
ZP_k    = ZP_syn - 2.5*log10(k_photo,k)                 # 本帧测光零点 [mag]
F_ref,k = 10^(-0.4*(m_ref - ZP_k))                      # 固定星等 m_ref 在**本帧**的仪器通量 [ADU]
F0      = 10^(-0.4*(m_ref - ZP_syn))                    # **物理公共锚**（块级，跨帧恒等）
```

其中 `ZP_syn` 由 **Gaia DR3 XP 绝对 XPSD 谱**（`F(λ)=byte*flux_mul+flux_min`）经本帧滤光片+QE 曲线**正向卷积**得到，只依赖 `(filter, QE, 天区星族)` ⇒ 与帧无关。这与 `snr_science.cpp:234` 的 `m_5 = ZP - 2.5*log10(F)` 口径一致。

> **合规声明**：`ZP_syn` 是**一手绝对定标**（Gaia XP 绝对谱 + 仪器响应正向合成），**不是**从 `k_photo` 反推增益/口径/曝光的"物理闭合反推"（§9.42 禁止项）。数据流方向是「已知绝对谱 → 预测仪器通量」，全程无闭环。

### 3.2 为什么这样能同时满足「帧间独立」与「配对性」

这是本次设计的关键点，两条约束看似冲突：

- **帧间独立**（§9.49 定案 2）要求 `F_ref` 不能依赖别的帧 ⇒ 必须用**逐帧** `F_ref,k`；
- **配对性**（WEIGHT-SCI-001）要求 `w_f = SNR_f²/F_ref² = a_f²/σ_f²`，即换算权重时分子必须用**同一个** `F_ref`。

解法：**逐帧 `F_ref,k` 用于帧级 SNR 的分子；块级物理锚 `F0` 用于权重换算**。二者由同一个 `m_ref` 和同一个 `ZP_syn` 导出，恒等式：

```
F_ref,k · k_photo,k = 10^(-0.4*(m_ref - ZP_syn)) = F0      （严格恒等）
```

于是 `SNR_k = F_ref,k/σ_k` 且 `w_k = SNR_k²/F0² = a_k²/σ_k²` —— 配对性成立，同时 `F_ref,k` 只看本帧。

**若把逐帧 `F_ref,k` 直接当权重分子（Convention B）**，会丢掉帧间响应 `a_f²`，权重错误可放大到 **23.3×**（§5.4 实测）。这正是 G11 与 `weight-sci-ruling.md` §2.6 警告的混用错误（最高 3.5×，本数据集更极端）。

### 3.3 与负责人裁决的逐条对应

| 裁决原话 | 落实 |
|---|---|
| 「直接用 6 等星……来做基准就行」 | `m_ref = 6.0`（`snr.reference_mag` 配置键，默认 6.0） |
| 「极度异常值拒绝并抛出错误，其他合理范围都接受」 | `k_photo` 拟合走 Tukey IRLS + `P1_PHOT_MAX_SPREAD_DEX` 极端值拒绝；`zero_point_valid` 需 `n_stars ≥ 3`，否则 **fail-closed**（不写帧级 SNR 键，不伪造） |
| 「帧间独立，为啥要组间对比」 | `F_ref,k` 逐帧；`scope="frame_independent_fixed_magnitude"`，**无"组"概念** |
| 「不同光学系统的帧塞进来那全报错多离谱」 | 逐帧标定 ⇒ 不同望远镜/滤光片各自算自己的 `ZP_k`，**不会互相报错**；跨帧 `k` 不同是**正常且正确**的（§9.49 定案 2） |
| 「更散是正常的。只要测光是对的，能正确拟合就行」 | `MAD ≤ 0.03 mag` **不作为硬门**；判据 = 测光正确 + 拟合收敛 |
| 「能靠实验证据、论文开源库解决的不要找我」 | 候选 B 的否决完全由文献 + 数值反例决定（§2），未上报 |

---

## 4. 改动面（file:line）

### 4.1 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h`
```
+98..102   FramePhotFitResult 新增:
            bool   zero_point_valid;         // n_stars < kMinFitStars ⇒ false（不得使用）
            double zero_point_mag;           // ZP_syn [mag]
            int    zero_point_n_stars;       // 参与中位数的锥形搜索星数
            double zero_point_scatter_mag;   // 1.4826*MAD(ZP_i) [mag]
```

### 4.2 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp`
```
+  #include "spectrum_integrator.h" / <cstdlib>
+  gaia_client_destroy(client) 之前插入 ZP_syn 块:
     - 第二次 gaia_client_cone_search_with_spectrum(...)
     - prepare_filter_cache + compute_f_syn_cached_xpsd
     - ZP_i = magG + 2.5*log10(F_syn,i);  median + 1.4826*MAD
     - zp_vals.size() >= kMinFitStars(3) ⇒ zero_point_valid = true
     - free(zp_stars); free(zp_spectra);
```

### 4.3 `lib/infrastructure/scheduler/src/module_adapters.cpp`（主改动，656 行）
| 行 | 内容 |
|---|---|
| 3211-3214 | `P1FrameScale` 新增 `zero_point_valid/mag/n_stars/scatter_mag` |
| 3424-3425 | `sc.zero_point_* = fr.zero_point_*` |
| **3602-3643** | `photscale_fit` 逐帧证据表**无条件**落盘（修：旧 `P1_PHOT_MAX_SPREAD_DEX=0.02` 门把逐帧 `k` 全丢弃，导致下游拿不到标定） |
| **3858-3936** | FREF 块：读 `p1_phot.json` → `kphoto_of_key/zp_inst_of_key/zp_syn_vals`；`ref_mag = doc["snr"].value("reference_mag", 6.0)`；置 `ref_flux_source="fixed_magnitude"`、`ref_flux_common`、`ref_zero_point_syn` |
| 3981-3990 | 逐帧 `cfg.reference_flux_adu = 10^(-0.4*(ref_mag - frame_zp_inst))`；`cfg.zero_point_mag = frame_zp_inst`（**修 G2**：`m_5` 此前恒为 null）；标定缺失 ⇒ fail-closed 写 `0.0` |
| 4058-4070 | `frame["snr_reference"]` 新增 `scope/flux_common/flux_common_unit/reference_mag/reference_mag_system/reference_zero_point_syn_mag/frame_zero_point_mag/frame_k_photo` |
| 4115-4149 | `p1_snr.json` 块级：`schema_version="3"`、`snr_reference_scope`、`reference_flux_source`、`reference_flux_common`、`reference_mag`…；manifest 同步；`snr_schema="DATA-P1-SNR/3"` |

**provenance 诚实性**：`fixed_magnitude` 生效时，两遍法的块中位数 `group_ref_flux` **未被使用**，故 `reference_flux_adu` 写 `null`（避免下游把陈旧中位数当生效值）。

### 4.4 `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`（363-435）
```
+  const bool use_common = (scope_str == "frame_independent_fixed_magnitude");
+  const char* fref_key = use_common ? "flux_common" : "flux_adu";
+  bool group_fref_ok  = (scope_str == "group" || use_common);
   ... 逐帧 fref 与公共值扫描都改用 fref_key
```
使头部公共通量闸门读**物理锚** `flux_common` 而非逐帧 `flux_adu`，配对性得以在头部保持。

### 4.5 `contracts/schemas/unified/frame_snr.schema.json`
新增顶层 `reference_baseline` 对象（`scope` / `reference_flux_source` / `reference_flux_common` / `reference_flux_common_unit` / `reference_mag` / `reference_mag_system` / `reference_zero_point_syn_mag` / `frame_zero_point_mag` / `frame_k_photo` / `pairing_identity`）。
注：顶层 `propertyNames` 正则禁止 `weight|value|mask|snr` 前缀，故字段名统一用 `reference_*` / `frame_*` 前缀。

### 4.6 `tests/unit/p1snr/p1snr_fref_baseline_test.cpp`（新建）+ `CMakeLists.txt`
夹具常量：`kK1=6.272203e-17`、`kK2=5.685037e-17`、`kZPsyn=-14.269`；帧 2 源通量 ×1.3（**必需** —— 否则 RED 判据退化为恒真）。

---

## 5. 证据

### 5.1 判别性测试（旧红 / 新绿）

```
ok  : RED: 旧形态(无 photscale_fit) ⇒ 参考通量只能是数据派生的 group_median
ok  : RED: 旧形态 ⇒ 没有固定参考星等（无法帧间可比）
ok  : RED: 旧形态 F_ref 随帧集改变（帧不独立 —— 负责人裁决要修的点）
ok  : N1: reference_flux_source == fixed_magnitude
ok  : N1: 参考星等如实落盘 (m_ref = 6.0)
ok  : N4: 物理公共锚 reference_flux_common > 0
ok  : N2: ZP_k = ZP_syn - 2.5log10(k_photo) (帧 1 / 帧 2)
ok  : N2: flux_adu == 10^(-0.4(m_ref-ZP_k)) (帧 1 / 帧 2)
ok  : N3: 配对性 snr_f == flux_adu/sigma_f_adu (帧 1 / 帧 2)
ok  : N4: flux_adu*k_photo == flux_common（同一物理参考星）(帧 1 / 帧 2)
ok  : N5: 新形态 F_ref 与帧集无关（帧间独立, 无需组概念）
ok  : N5: reference_flux_common 与帧集无关（Phase2 公共通量闸门恒过）

16 checks, 0 failures
```

契约回归：`python3 -m pytest tests/contracts/test_unified_object_contract.py -q` ⇒ **35 passed**。

### 5.2 端到端 before/after 对照

`before` = 旧二进制（`DATA-P1-SNR/2`）；`after` = 新二进制（`DATA-P1-SNR/3`，含 provenance null 修正）。

| | t2_m1_red | t3_m4_red |
|---|---|---|
| before schema / scope / src | `2` / `group` / `group_median` | `2` / `group` / `group_median` |
| after schema / scope / src | `3` / `frame_independent_fixed_magnitude` / `fixed_magnitude` | 同左 |
| before F span / σ_F span / SNR span | 1.0000 / 1.0468 / 1.0468 | 1.0000 / 1.6834 / 1.6834 |
| **after F span / σ_F span / SNR span** | **1.0744 / 1.0468 / 1.1246** | **5.0874 / 1.6834 / 3.3613** |
| after `F0`（公共锚） | 7.8291495391e-09 | 7.4823687179e-09 |
| after `ZP_syn` | -14.265714 | -14.314902 |

**注意 `t2_m1_red` 的 SNR 跨度从 1.0468 变成 1.1246（略变差）**：这不是退化，而是**物理上正确**的行为 —— 固定星等基准现在把帧间响应 `a_f`（`k` 比 1.0744）如实带进 `F_ref`。旧实现的"更平"是因为它把 `F_ref` 钉死成常数、**人为抹掉了帧间真实差异**。§9.50 定案：更散是正常的，只要测光对、拟合收敛。

### 5.3 配对性实测（`F_ref,k · k_photo,k / F0`）

| 帧 | t2_m1_red | t3_m4_red |
|---|---|---|
| 1 | 1.0000000000 | 1.0000444146 |
| 2 | 1.0000000000 | 1.0004242527 |
| 3 | — | 0.9999851956 |
| 4 | — | 0.9998849088 |
| 5 | — | 0.9999155507 |
| 6 | — | 1.0000148046 |

偏差 ≤ **4.3e-4**，来自 `ZP_syn` 的块中位数与逐帧 `ZP_k` 的舍入 —— 远小于 Phase2 公共通量闸门的 1e-9 相对容差所要求的物理一致性（该闸门比的是**头部同一个 `flux_common` 值**，恒等通过）。

### 5.4 权重影响（旧方案相对新方案的放大倍数）

`w_old/w_new` 以首帧归一（`w_old ∝ 1/σ_f²`，`w_new ∝ (k_f/σ_f)²`）：

| t3_m4_red 帧 | w_old/w1 | w_new/w1 | 旧方案放大 |
|---|---|---|---|
| 1 | 1.0000 | 1.0000 | 1.000× |
| 2 | 0.9311 | 1.0329 | 0.901× |
| 3 | 1.0241 | 0.8752 | 1.170× |
| **4** | **2.1330** | **0.0915** | **23.306×** |
| 5 | 0.9690 | 0.9302 | 1.042× |
| 6 | 0.7527 | 0.5319 | 1.415× |

**第 4 帧被旧方案高估 23.3×** —— 这正是 G11 的实质危害：一个透光率明显偏低的帧，因为 `F_ref` 被钉成块常数，反而拿到接近最高的权重。

### 5.5 头部公共通量闸门实测（Phase2 权重链，P2b-4）

闸门位置 `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:357-412`，要求头部 `ASTROCS_REFERENCE_FLUX` 逐帧**字节恒等**（相对容差 1e-9），否则 fail-closed 不写帧级 SNR 键。

| | 头部 `ASTROCS_REFERENCE_FLUX` | 头部 `ASTROCS_FRAME_SNR` |
|---|---|---|
| **旧 L4 产物**（`DATA-P1-SNR/2`） | `3492.374` / `3366.263` —— **逐帧不同** | 28.236 / 28.490 |
| **本次 after**（`DATA-P1-SNR/3`） | `7.8291495391477947e-09`（t2_m1，2 帧全同）/ `7.4823687179291568e-09`（t3_m4，6 帧全同）—— **字节恒等** | 逐帧如实携带 `a_f`：2095693 / 2130672 / 1960401 / **633889** / 2021000 / 1528331 |

即：**公共锚恒等（闸门通过）+ 帧级 SNR 逐帧（物理正确）** ⇒ `w = SNR²/F0² = a_f²/σ_f²` 配对性在头部成立。

> **过程中发现并修复的一个真实缺陷**：`ar r` 以不同成员名（`sink_drizzle_astro_sphere_sink.cpp.o`）**追加**对象而非替换，归档内同时存在新旧两个 sink 对象，链接器取用陈旧者 ⇒ 闸门误报"F_ref 非组内公共"。已 `ar d` 删除陈旧成员后重链，闸门通过（证据：`logs/e2e_after3_*.stderr`）。该缺陷只影响本地实验归档，**不影响仓库源码**。

### 5.6 `m_5` 修复（G2）

`before` 全部帧 `frame_depth_m5_mag = null`（`zero_point_mag` 从未产出）。`after` 已产出，例如 `t2_m1_red`：20.0981 / 20.2256；`t3_m4_red`：20.0559 / 20.0739 / 19.9834 / **18.7576** / 20.0165 / 19.7131。

---

## 6. 关键独立发现：`t3_m4_red` 第 4 帧的通量异常是**真实**的

`after` 的 `t3_m4_red` SNR 跨度 3.3613 几乎完全由第 4 帧（`20251212@035745`）贡献（`k_photo=2.835759e-16`，是其余帧的 4.83×）。为判定这是"拟合伪影"还是"真实异常"，做了**四组互相独立的测量**（全部绕过生产链）：

### 6.1 独立探针

| 探针 | 第 4 帧 / 第 1 帧 | 隐含 `k_4/k_1` |
|---|---|---|
| **原始帧同星固定孔径测光**（星表对齐偏移，r=8，环 12-19，n=17 颗非饱和星） | **0.2057** | **4.8614** |
| 已校准帧（cleaned）星表同星通量比 | 0.2221 | 4.5030 |
| 已校准帧 p99.99 亮端比 | 0.2905 | 3.4423 |
| 已校准帧局部天空比（同批星环带） | 0.8524 | 1.1732 |
| 生产 `k_photo` | — | **4.8277** |

**原始帧同星测光（4.8614）与生产 `k_photo`（4.8277）一致到 0.7%** —— 这是最强的证据：`k_photo` 拟合**正确**，第 4 帧确实比第 1 帧暗约 4.8×。

### 6.2 增长曲线判据（区分 PSF 散射 vs 吞吐量）

同星通量比随孔径半径的变化（原始帧，n=60 颗非饱和星）：

| 帧 | r=4 | r=8 | r=16 | r=32 | r=64 | 增长 | `1/k`比 |
|---|---|---|---|---|---|---|---|
| 3 | 0.9459 | 0.9117 | 0.9065 | 0.9011 | 0.9040 | **0.96×（平坦）** | 0.9244 |
| **4** | 0.2155 | 0.2066 | 0.2052 | 0.2330 | 0.3336 | **1.55×** | 0.2071 |
| 6 | 0.8837 | 0.8409 | 0.8352 | 0.8494 | 0.8773 | **0.99×（平坦）** | 0.8406 |

帧 3/6 完全平坦（比值 ≈ `1/k` 比），说明 `k_photo` 对它们是**精确**的。帧 4 从 0.2155 涨到 0.3336（1.55×），说明其星象**外围有额外散射/晕**（PSF 翼被低估）—— 即帧 4 同时存在**真实吞吐量下降**（主因，短孔径即已 4.6×）**与** PSF 形状差异（次因）。

### 6.3 头部佐证

- 第 4 帧原始帧 `CBLACK=1002`（其余 1105-1235）、天空中位 1092（其余 1193-1323）、`n>30000` 像素 1254（其余 3200-3933）、`noise_sigma=13.61`（其余 19.57-22.83）、检出源 66201（其余 87k-103k）；
- 第 4 帧 `FWHM=2.28`（其余 2.94-3.25）—— **帧内 seeing 更好**，与"外围散射"方向一致；
- **第 4 帧 `FOCALLEN=1934.2`，与帧 1/3/5 的 1933.7-1934.4 一致**（帧 2/6 才是 1877.0，且它们缺 WCS）—— 故异常**不是**焦距/光学系统变化引起。

**结论**：第 4 帧是一次**真实的、约 4.8× 的透光率下降**（天气/薄云），`k_photo` 正确捕获了它。按 §9.49 定案 2，跨帧 `k` 不同是**正常且正确**的，**不应**被任何组间一致性门拒绝 —— 这恰好验证了删除该门的裁决。旧方案给这一帧 23.3× 的权重高估，是必须修的实质缺陷。

---

## 7. 规格侧订正建议（**未改 `docs/**`**，请前台按 `ENGINEERING_SPEC.md §3` 走变更 claim）

1. **`docs/contracts/DATA_SEMANTICS.md` §13.4**
   - 标题：`DATA-P1-SNR/2` → **`DATA-P1-SNR/3`**；
   - 字段表新增：`snr_reference_scope`、`reference_flux_source`、`reference_flux_common`、`reference_mag`、`reference_zero_point_syn_mag`，以及帧级 `snr_reference.flux_adu` / `flux_common` / `frame_zero_point_mag` / `frame_k_photo`；
   - 新增约束段「参考基准约束（FREF-BASELINE-001）」：参考通量必须是统一物理基准；`F_ref,k = 10^(-0.4*(m_ref-ZP_k))`；公共锚 `reference_flux_common`；配对性 `w = SNR_k²/F0² = a_k²/σ_k²`；帧间独立、无需组概念；**禁止**用 `ZP_k/k_photo` 反推增益/口径/曝光。
   - （本报告 §4.5 的 schema 已先行落地；`DATA_SEMANTICS` 的对应文字待订正。）

2. **`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1**
   - 帧级 SNR 分子 `F_ref` 的定义需补明"统一物理基准"要求与固定星等取法；
   - 补 `m_5` 现可产出（G2 修复）与 `ZP_k` 的来源说明。

3. **`reverse_verify/docs/frame-snr-canon.md` line 453（G11）**
   - G11 描述的现象（`F_ref` 非组公共、跨度 2.14×）在 `/3` 下**已由设计消除**：`F_ref` 不再是数据派生的块常数，而是固定星等的逐帧仪器通量；
   - 建议将 G11 状态更新为「已修（FREF-BASELINE-001）」，并补充：组内 `F_ref` 跨度**变大**（`t3_m4` 5.09×）是**预期且正确**的，因为第 4 帧确有真实透光率下降（§6 四组独立证据）。

4. **`m_ref=6` 的适用域警告（重要）**
   `m_ref=6` 在两组数据集上都是**线性区之外的形式外推**：
   - M42 Red 300s（BITPIX=16、BZERO=32768、满井 65535 ADU、ZP=26.2369）⇒ `F(m=6)=1.244e8 ADU`，超饱和 **1899×**；
   - HST M16 F657N（AB ZP=22.6350、PHOTFLAM=2.2290223e-18、BUNIT=ELECTRONS/S）⇒ `F(m=6)=4.508e6 e⁻/s`，300s 曝光 1.352e9 e⁻，超 WFC3/UVIS 满井（~7e4 e⁻）**1.9e4×**；实测图像最大仅 2.524e4 e⁻/s。
   
   `m_ref=6` 作为**参考电平**仍然良定义（天光限下 `σ_F` 与通量无关 ⇒ `SNR ∝ F_ref`），但**不得**表述为"本帧能测到的 6 等星"。产品中已落盘 `reference_mag` 与 `reference_mag_system`，建议文档补一句适用域说明。

---

## 8. 兼容性与回退

- **回退路径保留**：无 `photscale_fit` 或 `zero_point_valid=false` ⇒ `reference_flux_source="group_median"`、`scope="group"`，**不伪造**；
- **显式配置优先**：`snr.reference_flux_adu` 显式给出且有限、>0 时优先，`ref_flux_source="config"`；非有限/≤0 ⇒ fail-closed；
- **逐帧中位数回退已删除**（fail-closed），**不得恢复** —— 恢复即重新引入帧间不独立；
- **跨光学系统安全**：逐帧标定 ⇒ 不同望远镜/滤光片各算各的 `ZP_k`，不互相报错（§9.50 明确要求）。

---

## 9. 证据文件索引

| 路径 | 内容 |
|---|---|
| `run/RELEASE-02/fref-baseline/bin/p1snr_fref_baseline_test` | 判别性测试可执行（16 checks） |
| `run/RELEASE-02/fref-baseline/logs/test_fref.stdout` | 测试输出 |
| `run/RELEASE-02/fref-baseline/work/{before,after2}/{t2_m1_red,t3_m4_red}/p1_snr.json` | 端到端前后产品 |
| `run/RELEASE-02/fref-baseline/work/after2/*/p1_phot.json` | `photscale_fit` 逐帧 `k_photo`/`ZP_k` |
| `run/RELEASE-02/fref-baseline/bin/astrocs.{before,after}` | 前后二进制 |
| `run/RELEASE-02/fref-baseline/e2e/cfg_*.json` | 端到端配置 |
| `run/RELEASE-02/fref-baseline/lib/*.after.a` | 补丁归档 |
| `run/RELEASE-02/fref-baseline/logs/e2e_after3_*.stderr` | 头部公共通量闸门通过的证据 |
| `run/RELEASE-02/fref-baseline/work/after2/*/*/signal/properties` | 头部 `ASTROCS_FRAME_SNR` / `ASTROCS_REFERENCE_FLUX` 实测值 |
| `run/RELEASE-02/fref-baseline/analysis.json` | 机器可读汇总（含 `frame4_investigation`、`hips_header_gate`） |
