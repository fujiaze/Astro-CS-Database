# Issue #92 A1-FIX 报告：统一 SIP 逆解算子（显式 AP/BP 精化后再过残差门）

- Issue: [#92](https://github.com/fujiaze/Astro-CS-Database/issues/92)（施工任务书评论 `6230039931`）
- 修复前取证基线: `06533d3f99149a96e8ee209dd14adb0ae6cb979d`（= A1 定位批最后 commit）
- 修复前生产二进制: `build/linux-sci-p5/acsd` `29d991f5aff87277416c8fd4331b8f8906c020a9b9a27f2ed2b353bbb24bc90b`
- 修复后生产二进制: `build/linux-sci-p5/acsd` `5277fcb895df802d4fd46468453208d3d2ee5a8e37362d06e5ee7958f15d8aa4`
- 本批改动：`lib/algorithms/photometry/cpp/src/wcs_transform.cpp`（唯一生产文件）+ 新增
  `eng/tests/unit/wcs_transform_inverse_test.cpp`、`eng/tests/unit/run_wcs_transform_inverse_test.py`、
  `实验/issue92-wcs-gaia-match/**`

---

## 0. 结论

**PASS**：唯一统一逆解算子落地后，真实 M42 六帧 `photometry.fit.enabled=true` 生产 E2E 从
`exit 4 / 0 入帧 / 0 匹配 / PHOT_FIT_NO_SCALE / 0 帧应用` 变为
**`exit 0 / 入帧 1083–1090 / 正向唯一匹配 954–1005 / 6 帧全部 fitted+applied / photscal=6.222285e-17`**，
且与独立 WCSLIB/Astropy oracle 的逐帧入帧数**完全一致**。

修复只做一件事：显式 AP/BP **仅作不动点迭代初值**，与无 AP/BP 路径**统一执行同一有界精化**，
精化完成后仍以**原有 `1e-7 px` 前向残差门**验收；`#56`（不收敛拒绝）、`#57`（TAN 背半球）、
`#61`（奇异 CD）的 fail-closed 条件全部保留。未放宽任何容差、未关 SIP、未改匹配半径、未改光度公式。

---

## 1. 修复内容（唯一生产文件）

`lib/algorithms/photometry/cpp/src/wcs_transform.cpp::WcsTransform::skyToPixel`（完整 diff 见 `evidence/60-fix.diff`）：

1. `t = CD⁻¹·(ξ,η)` 为目标；`#57/#61` 的天球域/CD 有限非退化门不变。
2. **初值**：有 AP/BP 用 `(u₀,v₀) = t + (AP(t), BP(t))`；无 AP/BP 用 `t`。
   AP/BP 求值非有限 ⇒ 退回受保护的线性初值 `t`（不产生"看似有效"的解），仍由第 4 步裁决。
3. **精化**：两条路径统一执行同一段有界不动点迭代
   `(u_{j+1},v_{j+1}) = t − (A(u_j,v_j), B(u_j,v_j))`，最多 64 轮、步长 `≤1e-10` 早停；
   任一步非有限即置 NaN 并跳出。
4. **验收门（保留原样）**：精化完成后逐分量 `|F(u*)−t| ≤ 1e-7 px` 且全部有限才返回
   `x = u* + (CRPIX1−1), y = v* + (CRPIX2−1)`；否则 NaN。
5. 无 SIP 时仍直接返回 CD 逆（不进 SIP 分支、不进 SIP 门）。

**次级合同缺口（本批只登记，未改 API）**：生产者 `p1_wcs.json` 声明 `sip.ap_order=5`，消费者
`WcsTransform` 只有单一 `m_sip_order(=sip_order=3)`，AP/BP 以 3 阶求值；该缺口只影响初值质量
（帧角落 ~0.5 px），精化后对最终精度无影响。真正的 `ap_order` 独立入参改造需另批最小补丁 + 真实 A/B
（避免牵连 pc ABI）。

---

## 2. 单元 / 对抗性测试

`python3 eng/tests/unit/run_wcs_transform_inverse_test.py`（以**当前树内**源码编译；真实六帧
WCS/samples 由 `实验/issue92-wcs-gaia-match/fixtures/*.json` 生成夹具）。

| 项 | 修复后 | 修复前 A/B（`38eaeba8b^`） |
| --- | --- | --- |
| 判据总数 | **481/481 PASS** | **17/481 PASS**（真实帧判据全 FAIL，即负例复现） |
| 真实六帧 64 samples 过门 | **384/384** | 0/384 |
| 像素往返最差误差 | **1.182e-10 px** | 0.000532 px（仅 C ABI 3 轮迭代路径） |
| 前向一致性最差 | **2.827e-14 deg**（Haversine） | — |

覆盖：真实六帧 WCS 各 64 samples 的 0-based 往返 + FITS 1-based 切点基准；CRVAL 切点（真实
`AP_0_0=−4.33e-5` 的解析反例）；无 SIP / 无 AP-BP / AP-BP 全零 / 真实 AP-BP 四条路径结果一致；
公开 `pc_project_sky_to_pixel` 非 AP 调用回归；**负例**：奇异 CD、TAN 背半球、非有限 ra/dec、
非有限 AP 系数（受保护初值）、非有限前向 A 系数、病态 SIP `A=2u`（不动点发散，门必须拒绝）、
`SIP order>5` 构造层拒绝、C ABI `order>5 → −2`、C ABI 奇异 CD → NaN。

> 注：测试内角距用 Haversine；`acos` 公式在近零角距有 ~8.5e-7 deg 舍入地板，无法判别 1e-13 量级一致性。

---

## 3. 真实六帧 E2E 对比（同一输入、同一配置，仅换二进制）

配置 = `实验/issue12-simpson-domain/evidence/50-cfg6.json` 副本（仅改 `output_dir`），
`photometry.fit.enabled=true`，GaiaDR3SP，真实 M42 T3 M1 Red 六帧。

| # | frame | pre 入帧 | **post 入帧** | pre 正/唯一 | **post 正/唯一** | **post k_photo** | post σ_resid |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 20251126_041940 | 0/2309 | **1085/2309** | 0/0 | 954/954 | 6.222285e-17 | 0.0250 |
| 1 | 20251127_042719 | 0/2309 | **1085/2309** | 0/0 | 985/985 | 5.972444e-17 | 0.0201 |
| 2 | 20251128_043530 | 0/2306 | **1087/2306** | 0/0 | 987/987 | 6.003670e-17 | 0.0227 |
| 3 | 20251211_013548 | 0/2307 | **1090/2307** | 0/0 | 980/980 | 6.554059e-17 | 0.0264 |
| 4 | 20251212_021119 | 0/2310 | **1083/2310** | 0/0 | 1005/1005 | 6.487244e-17 | 0.0200 |
| 5 | 20251213_021555 | 0/2307 | **1084/2307** | 0/0 | 986/986 | 6.209759e-17 | 0.0222 |

| CLI / 产物 | pre | **post** |
| --- | --- | --- |
| `exit_code` | 4 | **0** |
| 最终状态 | `phase1_failed / PHOT_FIT_NO_SCALE` | **`ok / phase1 complete`** |
| `p1_phot.json n_frames_ok / applied / failed` | 0 / 0 / 6 | **6 / 6 / 0** |
| `photscal` | 1.0（失败占位） | **6.222285086050469e-17** |
| `photscale_source` | `gaia_star_matcher_tukey_irls` | 同 |
| `photometry_applied` / `pixel_scaling` / `spatial_gain_mode` | false / none / absent | **true / applied / applied_spatial（6 帧）** |
| `photscale_spread_dex` | 0.0 | 0.040358 |
| `p1_phot` 逐帧 `n_matched`（IRLS inlier） | — | 929 / 956 / 959 / 960 / 972 / 956 |

`p1_wcs.json` 与 A1 批夹具**逐位相同**（frame1 `sha256 f7a981e135677171…`，`n_pairs=54`，
`rms_arcsec=0.126741`）——WCS 解算未受本修复影响。

资源读数（post）：wall 566.5 s，avg equivalent cores 5.61，peak RSS 12.66 GB，io read 4.89 GB / write 6.58 GB，frames 54，threads 78。

---

## 4. 独立 WCSLIB/Astropy 六帧入帧分母（`evidence/65-per-frame-oracle.json`）

真实 Gaia 查询复刻生产口径（锥心=crval、半径=`√|det|·√(W²+H²)/2·1.2`、`mag_min=6`、
`mag_max` 自适应至 `n≥2000`）：

| frame | n_gaia | 修复前 AP 一次式 | **修复后统一算子** | Astropy `all_world2pix` | 一致 |
| --- | --- | --- | --- | --- | --- |
| 20251126_041940 | 2309 | 1085 | **1085** | 1085 | ✅ |
| 20251127_042719 | 2309 | 1085 | **1085** | 1085 | ✅ |
| 20251128_043530 | 2306 | 1088 | **1087** | 1087 | ✅ |
| 20251211_013548 | 2307 | 1090 | **1090** | 1090 | ✅ |
| 20251212_021119 | 2310 | 1083 | **1083** | 1083 | ✅ |
| 20251213_021555 | 2307 | 1084 | **1084** | 1084 | ✅ |

修复后与生产 CLI 实测入帧数**逐帧一致**；修复前 AP 一次式在 20251128 帧多 1 颗（其未精化的
0.5 px 角落误差把 1 颗边界星推进/推出帧），修复后与 Astropy 精确一致。

---

## 5. 残差门通过比例与像素误差分布（`evidence/82-residual-distribution.json`）

对每帧真实 Gaia 星复刻生产数值（逐分量门 `≤1e-7 px`）：

| frame | n | 修复前一次式过门 | 一次式残差中位/max | **修复后过门** | **精化残差中位/max** | 入帧 |
| --- | --- | --- | --- | --- | --- | --- |
| 20251126 | 2309 | 0 | 2.35e-1 / 1.83 px | **2309** | 0 / **4.547e-13 px** | 1085 |
| 20251127 | 2309 | 0 | 2.35e-1 / 1.82 px | **2309** | 0 / 4.547e-13 px | 1085 |
| 20251128 | 2306 | 0 | 2.26e-1 / 1.75 px | **2306** | 0 / 4.547e-13 px | 1087 |
| 20251211 | 2307 | 0 | 2.23e-1 / 1.74 px | **2307** | 0 / 4.547e-13 px | 1090 |
| 20251212 | 2310 | 0 | 2.30e-1 / 1.74 px | **2310** | 0 / 4.547e-13 px | 1083 |
| 20251213 | 2307 | 0 | 2.26e-1 / 1.79 px | **2307** | 0 / 4.547e-13 px | 1084 |

即：精化把门通过率从 **0/13848 → 13848/13848**，残差从 ~0.23 px 降到 ~4.5e-13 px（门 1e-7 仍保留）。

---

## 6. 应用标度 / 方差 / BUNIT 核对（`evidence/80-scale-verify.json`）

对照同一修复后二进制的两次真实运行：`photometry.fit.enabled=true`（施加 k_photo·m(x,y)）与
`=false`（未施加），逐像元比值。

| 检查 | 结果 | 判定 |
| --- | --- | --- |
| signal 比值 `r_s = signal_en/signal_dis`（Norder9，112 tile / 2.35e7 像元） | 中位 **6.2268e-17**；`r_s/k_photo` 中位 **1.0007** | **PASS**（标度确实施加，且含空间增益 m 的整体偏移） |
| 空间增益散布 | `r_s` p16–p84 相对半宽 **4.48%** ↔ `photscale_spread_dex=0.0404`（10^0.0404=1.097 ⇒ ±4.7%） | **PASS**（自洽） |
| `ivar` 与 `variance` 互逆 | `r_v·r_i = 1.000000` | **PASS** |
| 方差按 scale²：`r_v == r_s²` | 中位相对偏差 **4.55%**、p99 **12.8%**；log-log 指数 ≈1.59（非 2） | **未闭合 → #9** |
| BUNIT / ACSD_SIGNAL_UNIT | 两次运行均为 `ADU/sr`，但数值相差 ~6e-17 | **未闭合 → #9** |

**归因（不扩大本批范围）**：enabled/disabled 两次运行并非「同一方差再乘 scale²」的受控实验——光度标度
施加在**帧像素**上（`apply_photometry_spatial`: `I_photo = k_photo·m(x,y)·I_cal`），随后噪声模型/堆叠权重
在**已缩放像素**上重算，Poisson 项与权重随之改变，故逐像元比值不是纯幂律。**本批不据此修改方差或 BUNIT**；
「方差 ∝ scale²、已施加标度的真实单位/scale_state」属 #9（本任务书明确 #9 未修，不冒称完成）。

---

## 7. PASS / FAIL / NOT_RUN

| 项 | 状态 |
| --- | --- |
| 统一逆解算子实现（唯一生产文件） | **PASS** |
| 单元 + 对抗性测试（含 #56/#57/#61 负例） | **PASS**（修复后 481/481；修复前 A/B 17/481） |
| 生产完整构建 `ninja -C build/linux-sci-p5` | **PASS**（exit 0） |
| 真实六帧 `photometry.fit.enabled=true` E2E | **PASS**（exit 0；入帧 1083–1090；6/6 applied） |
| 独立 Astropy 六帧入帧分母一致 | **PASS** |
| 残差门通过率 0/13848 → 13848/13848 | **PASS** |
| signal 标度施加核对 | **PASS** |
| 方差按 scale² / BUNIT 真实单位 | **NOT_RUN → 归 #9**（见 §6） |
| ASAN 构建下本修复运行 | **NOT_RUN**（未构建 asan preset） |
| 其他帧/其他天区的光度科学正确性 | **NOT_RUN**（本批只验证本修复所达范围） |

---

## 8. 复跑命令

```bash
# 单元/对抗测试（当前树内源码）
python3 eng/tests/unit/run_wcs_transform_inverse_test.py

# 修复前 A/B（负例复现）
git show 38eaeba8b^:lib/algorithms/photometry/cpp/src/wcs_transform.cpp > /tmp/wcs_prefix.cpp
python3 eng/tests/unit/run_wcs_transform_inverse_test.py --impl /tmp/wcs_prefix.cpp --work /tmp/wcs_prefix

# 生产构建
ninja -C build/linux-sci-p5

# 真实六帧 E2E（enabled / disabled 各一次；输出目录隔离）
build/linux-sci-p5/acsd normalize --json /workspace/issue92-scratch/cfg6_fix.json -y
build/linux-sci-p5/acsd normalize --json /workspace/issue92-scratch/cfg6_nophot.json -y

# 六帧独立 oracle / pre-post 对比 / 残差分布 / 标度核对
python3 实验/issue92-wcs-gaia-match/code/65_per_frame_oracle.py
python3 实验/issue92-wcs-gaia-match/code/70_e2e_compare.py \
  --pre-stderr /workspace/issue92-scratch/norm6_issue92.stderr \
  --post-stderr /workspace/issue92-scratch/norm6_fix.stderr \
  --pre-out /workspace/issue92-scratch/norm6_issue92.p1-work \
  --post-out /workspace/issue92-scratch/norm6_fix.p1-work \
  --out 实验/issue92-wcs-gaia-match/evidence/70-e2e-compare.json
python3 实验/issue92-wcs-gaia-match/code/82_residual_distribution.py
python3 实验/issue92-wcs-gaia-match/code/80_scale_verify.py \
  --enabled /workspace/issue92-scratch/norm6_fix --disabled /workspace/issue92-scratch/norm6_nophot \
  --frame M42_M1_T3_flying_dutchman-20251126_041940-300S-Red \
  --phot /workspace/issue92-scratch/norm6_fix.p1-work/p1_phot.json --orders 5,9 \
  --out 实验/issue92-wcs-gaia-match/evidence/80-scale-verify.json
```

---

## 9. 未决事项（登记，不在本批修）

1. **#9**：方差按 `scale²` 的逐像元恒等式在本产品对拍中不成立（§6），且 BUNIT 不随标度改变；
   需要受控夹具（固定方差输入 × 已知 scale / `k_photo=1` 对照）才能判定是合同缺口还是噪声模型重算的
   必然结果。**#9 未修，本批不冒称完成。**
2. **次级合同缺口**：`sip.ap_order` 未进入消费者（AP/BP 按 3 阶求值）。修复后只影响初值质量；
   若要做 API 改造，需最小 1 文件补丁 + 真实 A/B，避免牵连 pc ABI。
3. **#80 关联**：`photscal ≈ 6.2e-17`（帧级 k_photo ~6e-17）与 #80 指出的 P1 历史光度标度/单位混用
   （1e32 权重跨度）一致；#92 修好的是「WCS 匹配入帧」，**不等同于** P5 异通带科学建模成功，
   须在 #9/#80 单独确证。
4. `experiment` 证据中的 `20-wcs-probe-frame1.json` 仍是 A1 批的**修复前负例**（post=0/2309）；
   本批逐帧证据在 `20-wcs-probe-<tag>.json`（post=修复后）。重跑 `--tag frame1` 会覆盖该负例。
