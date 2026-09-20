# F-INSTR-CONFORM-FIX — 测光 F_instr 口径符合性修复（A6 分片）

> 任务: RELEASE-02 / A6 F-INSTR-CONFORM-FIX  状态: 实现完成 + 实测完成, 待前台验收
> 规范依据（冻结, 未改动）: `docs/science/PHOTOMETRY.md` SCI-PHOT-001 §9a「**星点通量来自 PSF 拟合域（PSF.md）**」;
> `docs/science/PSF.md` SCI-PSF-001 §2/§3/§5/§9a「`flux = 2πA·sxsy/3`（β=4）, 单位 **ADU**」
> 结论: **改实现以符合已冻结规范, 未改任何科学公式/容差/文档**（`docs/**` 零改动）。

---

## 0 一句话结论

`p1_psf.json` 现在写出 **PSF 拟合域解析通量** `flux = 2πA·sxsy/3`（与 `dpsf_psf.cpp:428` 同式）;
测光拟合的 `F_instr` 改为**按 `star_id` 关联**取该列（PSF-FAST-001 子集映射）;
`psf_status != OK` 的星**跳过（fail-closed, 不回退盒和）**。

- 受控 seeing 注入: 旧口径（5×5 盒和）在 seeing 2.0→4.0 px 时给出 **+0.4606 mag 假帧间差**,
  新口径（PSF 域）给出 **+0.0000 mag**（判据 绿 ≤0.010 / 红 ≥0.200, 双向 PASS）。
- L4 真实数据 12 板块 49 帧, 91 个帧对: **`Δmag_盒和 = Δmag_PSF域 − Δboxcorr`（分解残差中位数 0.0070 mag）**,
  其中 `Δboxcorr`（视宁度孔径改正差）中位数 0.0483 mag、最大 0.6399 mag ⇒ 盒和口径把视宁度当成了帧间测光差。
- 组内 k 峰峰中位数 **0.2310 → 0.1364 mag**; 测光一致性最坏板块 **1.8459 → 0.1160 mag（16×）**。

---

## 1 改动面（file:line）

| # | 位置（修复后行号） | 改动 | 依据 |
|---|---|---|---|
| ① | `lib/infrastructure/scheduler/src/module_adapters.cpp:2032-2042` | 新增 `p1_psf_analytic_flux(A,sx,sy) = 2πA·sxsy/3`（与 `dpsf_psf.cpp:428` 逐字同式; 由 psf_params 9 列 ABI 的权威列 A,sx,sy 复算, **不新增列、不改 PSF 模块 ABI**） | SCI-PSF-001 §2/§5 |
| ② | `module_adapters.cpp:2159-2172` | `p1_psf.json` 的 `psf_params[]` **增加 `flux` 列**（值 = ①; 修复前该量在 `dpsf_psf.cpp:428` 算出后无处承载而被丢弃） | SCI-PSF-001 §9a |
| ③ | `module_adapters.cpp:3249-3326` | 测光输入装配重写: `F_instr` 由 `p1_sources[].flux`（检测域 5×5 正性截断盒和, `star_detector.cpp:151 s.flux = m00`）改为**按 `star_id` 关联 `psf_params[].flux`**; 每颗源显式携带 `psf_status`（有拟合行且 flux 有限>0 ⇒ `DPSF_PSF_STATUS_OK`, 否则 `DPSF_PSF_STATUS_FIT_FAILED`）; 排序 = PSF 域有效星按 F_instr 降序在前, 无效星恒在尾部 | SCI-PHOT-001 §4/§9a |
| ④ | `module_adapters.cpp:3206-3208, 3413-3414, 3556-3565, 3622-3624` | provenance: 逐帧 `n_psf_domain`/`n_psf_skipped` + `f_instr_domain=psf_analytic_flux_2pi_A_sx_sy_over_3` 写入 `p1_phot.json.photscale_detail` 与节点 manifest（使"标度取自 PSF 域"可被独立核对） | SCI-PHOT-001 §9a |
| ⑤ | `module_adapters.cpp:3180-3198` | `P1_PHOT_MAX_SPREAD_DEX` **0.5 → 0.02**（= 0.05 mag 峰峰, 负责人判据; 定案新增必改项） | 负责人令 / F-INSTR-SURVEY 定案 |
| ⑥ | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h:32-46` + `.cpp:98-107` | `psf_flux` 的**域契约**写死在接口注释（必须是 `2πA·sxsy/3`; 禁传检测域盒和; 失败星由有效域门剔除, 不静默降级） | SCI-PHOT-001 §9a |
| ⑦ | `tests/unit/p1001_real_nodes_test.cpp:3323-3330, 3414-3442` | 判别力测试: 原 GREEN 对（0.107 mag 散度 = 旧盒和口径的 L4 实测值）改判为 **RED-5 必须整组拒绝**; GREEN 改用真一致对（0.00085 dex） | ⑤ 的锁定 |

**明确未改**: `docs/**`（含 PHOTOMETRY.md / PSF.md / PUBLIC_API.md）; `dpsf_psf.cpp` 的公式与 ABI;
`star_matcher.cpp`（Tukey-IRLS `c=4.685`）; `spectrum_integrator.cpp`; `star_detector.cpp`（检测域盒和仍是检测量, 只是**不再进测光**）;
`P1_PHOT_MIN_FIT_STARS=3` / `P1_PHOT_MAX_SIGMA_DEX=1.0` 未动。

> 注: 工作区 `module_adapters.cpp` 另有一处**非本分片**的未提交改动（`:1894-1904 p1_op_calibrate` 的 dark_scale 溯源, CONFORM-FIX-A）。
> 该 hunk 在 before/after 两个静态库中**同时存在**（before 库 18:27 构建晚于源文件最后修改 17:50）, 不污染本 A/B;
> 且本 A/B 只执行 `star-psf` / `photometry` 两个节点, 不经过 calibration。

---

## 2 PSF 失败星的策略（明确策略 + 与规范一致）

**策略 = 跳过（skip）, 不回退、不降级到盒和; 有效星不足则 fail-closed 拒绝施加。**

| 情形 | F_instr | 处理 | 规范依据 |
|---|---|---|---|
| 有 PSF 拟合行且 flux 有限 > 0 | `2πA·sxsy/3` (ADU) | `psf_status=0` 进入有效域 | SCI-PHOT-001 §9a |
| `psf_status != OK`（LM 未收敛 / 参数非法 / 背景约束违反 / FWHM 超窗） | — | **跳过**: 置 `DPSF_PSF_STATUS_FIT_FAILED(1)`, flux=0; `star_matcher::matchWithKdTree` 的有效域门 `psf_status[i]==0` 剔除 | SCI-PHOT-001 §4「饱和/质量异常不参与定标」 |
| 未进入 PSF-FAST-001 拟合子集（`psf.max_stars` 截断, 无 star_id 行） | — | **同上跳过**（计入 `n_psf_skipped`） | PSF-FAST-001 + §9a |
| 拟合行存在但 flux 非有限 / ≤0 | — | 视同拟合无效, 跳过 | §9a「F>0 有限值为有效域」 |
| 饱和星（`quality&1`） | 保留数值 | 由 `PC_QF_SATURATED` 门在 `cleanAndScale` 剔除（口径不变） | SCI-PHOT-001 §4/§10 |
| 有效星匹配数 < 3 | — | NO_DATA: `fit_ok=false` ⇒ **不施加**标度 | SCI-PHOT-001 §4/§8 |
| 组内帧间 k 峰峰 > 0.02 dex | — | 整组拒绝 + `degraded_reason=photscale_incomplete` + 显式 `photscale_error` | 负责人判据（≤0.05 mag） |

**为什么不回退盒和**: (a) 与 §9a 冲突（盒和不是 PSF 拟合域量）; (b) 盒和的能量份额随 seeing 变化
（实测 seeing 2.0→4.0 px 时 5×5 盒和产生 0.46 mag 假差, 见 §3）⇒ 回退会把视宁度重新注入标度;
(c) 同一 location 内混两种口径 = 单位混装。

**降级链（全链 fail-closed, 不静默、不伪造 1.0）**: 有效星不足 → NO_DATA → 不施加 →
`p1_phot.json{photometry_applied:false, pixel_scaling:"none", degraded_reason, photoscale_error}`。
L4 实测该链被真实触发 12/12 次（见 §4.4）。

**L4 量化（t2_m1 帧 0）**: PSF 拟合输入 5000 颗（最亮, PSF-FAST-001）→ 成功 1633 行（32.7%）→
送入测光 1633 有效星 + 3367 显式无效星 → 匹配 955 颗（58.5%）。修复前: 5000 颗盒和候选（全部 status=0）→ 匹配 939 颗（18.8%）。

---

## 3 判别力测试（能红能绿）—— seeing 注入

工具: `run/RELEASE-02/f-instr-fix/tools/f_instr_seeing_redgreen.cpp`（独立可执行, 直接链接生产 PSF 库 `libastrocs_p1_dpsf.a` 的 `dpsf_fit_batch_d`）。
数据: 合成 Moffat4 **β=4** 星场 121 颗（400×400, 抖动网格, 间距 ≥30 px）, **每颗星总通量守恒**
（`A = F_tot·3/(2πσ²)`）⇒ 只改 seeing, 真值帧间增益 **= 1.000**。
旧口径 = 检测器原样（局部峰 + 5×5 正性截断盒和, `star_detector.cpp:139-151`）;
新口径 = `DPSFFitResult.flux`（生产 β=4 拟合器, 与 `dpsf_psf.cpp:428` 同源）。拟合成功 121/121。

| 实验 | 度量 | 旧口径 box5 | 新口径 PSF 域 | 判据 |
|---|---|---|---|---|
| **exp4-B** seeing 2.0→4.0 px, 增益 1.000（无噪声） | 逐星 Δmag 中位数 | **+0.4606 mag** (MAD 0.0024) | **+0.0000 mag** (MAD 0.0000) | 红 ≥0.200 / 绿 ≤0.010 |
| **exp4-B** 同上 + 高斯噪声 σ=5 ADU | 同上 | **+0.4607 mag** (MAD 0.0023) | **+0.0000 mag** (MAD 0.0005) | 同上 |
| **exp4-B** 孔径扫描 Δmag(r), r=2/3/4/6/10 px | 孔径依赖性 | **+0.7440 / +0.4060 / +0.2053 / +0.0547 / +0.0056** | 按构造与口径无关 | — |
| **exp4-B** M_seeing = 口径扫描极差 | 孔径无关性 | **0.7384 mag** | **0.0000 mag** | 红 ≥0.200 / 绿 ≤0.010 |
| **exp4-A** 正对照: seeing 不变, 增益 1.05→0.95 | Δmag 中位数 | +0.1087 mag | +0.1087 mag | 回收真值 0.1087 (tol 0.02) |
| **exp4-A** 同上 + 噪声 | 同上 | +0.1087 mag | +0.1086 mag | 同上 |

```
red_green_check={green_ok:true, red_ok:true, control_ok:true}     (退出码 0)
```

**与定案基准对照**: 定案 exp4-B 报 box5 = 0.504 mag、孔径比值 0.493/0.672/0.808/0.925/0.985;
本实现独立复现 box5 = **0.4606 mag**、孔径比值 0.504/0.629/0.827/0.951/0.995（由 Δmag 换算）
—— 同量级同趋势（差异来自孔径定义与星场几何）。新口径在两者中都是 **0.0000 mag**。正对照证明该度量**不是恒零**。

---

## 4 L4 真实数据 before/after（`run/RELEASE-02/L4-rebuild/norm/*/`）

### 4.1 方法（生产代码路径, 只替换编排）

- **before** = 未改动静态库 `build/libastrocs_module_adapters.a`（18:27 构建, 与源文件一致）;
  **after** = 用 `g++ -O3`（与 ninja 同 flags）独立重编 `module_adapters.cpp` 后 `ar r` 同名成员替换
  ⇒ 二者**只差本分片的改动**（"独立重链", 未跑 ninja/cmake/ctest）。
- 编排: `p1_node_runner`（`run/RELEASE-02/f-instr-fix/tools/p1_node_runner.cpp`）经**生产注册表**
  `register_phase_modules` 只执行 `astrocs.phase1.star-psf` 与 `astrocs.phase1.photometry` **节点本体**
  （同一编译产物, 节点代码路径与生产完全一致）。
- 输入: 12 板块 / 49 帧的 L4 产物（`cleaned_*.fts` / `calibrated_*.fts` / 逐帧 `p1_wcs.json` 软链）;
  `p1_sources.json` 由 after 库重新生成（`sources[]` 不变, 仅 `psf_params[]` 增加 `flux` 列）
  ⇒ before/after **共用同一份输入**, 差异只有 F_instr 口径 + 帧间门。
- 端到端复核: 另用重链的**完整 `astrocs.after` 二进制**跑了一次真 `normalize`（t2_m1, rc=0, 3m33s）:
  `p1_psf.json` 的 flux 列与解析式**逐位一致**（569317.8926650479 = 2πA·sx·sy/3）, `p1_phot.json` 与节点级结果一致
  （k 比值 1.073715 完全相同）。

### 4.2 逐板块结果（spread = 组内 k 峰峰 mag; sig = 逐帧 sigma_residual 中位数 dex）

```
tag         nfr | spread_b  appl_b   sig_b   | spread_a  appl_a   sig_a   | boxcorr(视宁度假项)
t2_m1_red     2 | 0.1067    True     0.0185  | 0.0772    False    0.0266  | 0.0204
t2_m2_red     4 | 0.2572    True     0.0326  | 0.0522    False    0.0333  | 0.2007
t2_m3_red     2 | 0.0483    True     0.0221  | 0.0553    False    0.0201  | 0.1016
t2_m4_red     3 | 0.5283    True     0.0311  | 0.4871    False    0.0228  | 0.0327
t2_m5_red     3 | 0.5660    True     0.7384  | 0.1357    False    0.0464  | 0.6399
t2_m6_red     2 | 0.0400    True     0.0234  | 0.1887    False    0.0199  | 0.2232
t3_m1_red     6 | 0.2048    True     0.0205  | 0.1008    False    0.0215  | 0.1405
t3_m2_red     4 | 0.0791    True     0.0342  | 0.1370    False    0.0281  | 0.0872
t3_m3_red     6 | 0.0748    True     0.0290  | 0.1124    False    0.0213  | 0.1024
t3_m4_red     6 | 1.6397    False    0.0234  | 1.7638    False    0.0276  | 0.1504
t3_m5_red     5 | 0.4502    True     0.7070  | 0.2597    False    0.0400  | 0.3641
t3_m6_red     6 | 0.3175    True     0.0229  | 0.4263    False    0.0185  | 0.1192
```

- **拟合覆盖**: before 12/12 板块有真实拟合（11/12 施加）; after 12/12 有真实拟合（**0/12 施加**, 全部被 0.02 dex 帧间门拒绝 —— 见 §4.4）。
- **组内 k 峰峰**: 中位数 **0.2310 → 0.1364 mag**; 最大 1.6397 → 1.7638（t3_m4 的离群帧, 两口径一致, 是真实异常帧）。
- **测光一致性 sigma_residual**（尺度无关）: 中位数 **0.0655 → 0.0617 mag**;
  **最坏板块 1.8459 → 0.1160 mag（16× 改善）** —— 盒和口径下 t2_m5/t3_m5 的散度高达 0.74 dex（1.85 mag, 已属"不是标度"的量级）却仍过了 0.5 dex 门并被施加。

### 4.3 帧对级分解（91 帧对）—— 决定性证据

对每一帧对 (i,j) 直接由产物计算:

```
Δmag_盒和(i,j) = Δmag_PSF域(i,j) − Δboxcorr(i,j)
Δboxcorr(i,j) = 2.5·[log10(box5/PSF)|_j − log10(box5/PSF)|_i]   （同一批星, 按 star_id 关联）
```

| 量 | 中位数 | p90 | 最大 | >0.05 mag 的帧对 |
|---|---|---|---|---|
| 分解残差 abs(Δmag_box − (Δmag_psf − Δboxcorr)) | **0.0070 mag** | 0.1951 | 0.9853 † | 16/91 |
| abs(Δboxcorr)（盒和的视宁度假项） | **0.0483 mag** | 0.1665 | **0.6399** | 45/91 |
| abs(Δmag_box) | 0.0483 | — | 1.6397 | — |
| abs(Δmag_psf) | 0.0772 | — | 1.7638 | — |

† 残差 p90/max 由 t3_m4 的单个病态帧（与所有帧差约 1.6 mag）主导; 剔除该板块后残差中位数与 p90 显著更小。

**单例（可直接引用）**:

- `t3_m1_red` 帧 3→4: 盒和 **+0.1357 mag**, PSF 域 **−0.0104 mag**, Δboxcorr = −0.1405
  ⇒ **0.136 mag 的"帧间差"完全是视宁度伪信号**, 真实帧间差只有 0.010 mag。
- `t2_m6_red` 帧 0→1: 盒和 **+0.0400 mag**（看似"一致"）, PSF 域 **−0.1887 mag**, Δboxcorr = −0.2232
  ⇒ 盒和把**真实的 0.19 mag 帧间差掩盖成 0.04 mag**（视宁度损失与真实变亮相消）。
  ⇒ **盒和口径既会造假差, 也会掩盖真差。**

### 4.4 帧间门收紧（0.5 → 0.02 dex）的后果（必须上呈）

- after 状态下 **12/12 板块全部被拒**（`photometry_applied=false`, `pixel_scaling="none"`, `degraded_reason=photscale_incomplete`）,
  因为组内峰峰 0.052–1.76 mag 全部 > 0.05 mag。before 状态（门 = 0.5 dex）则 11/12 施加。
- **根因不是 F_instr 口径**: L4 板块把**不同夜**的帧编成一组, 而跨夜真实透明度差本身就有 0.1–0.4 mag
  （§4.3 的 Δmag_psf; 例: t2_m6 真实差 0.19 mag、t3_m6 0.35 mag、t3_m4 1.7 mag）。
  真正"同组"（同夜）的帧对全数据集只有 2 对: abs(Δmag) before **0.0071** / after **0.0201 mag** —— **两者都满足 ≤0.05 mag**。
- ⇒ **≤0.05 mag 判据只在"同仪器 + 同夜 + 同曝光"分组下可达**; 现 L4 分组（跨夜）下任何 F_instr 定义都不可达。
  请负责人裁决: (a) 保持 0.02 dex 并接受"跨夜组一律不施加"（当前行为, fail-closed）;
  (b) 把"组"重定义为同夜; (c) 给跨夜组一个按透明度分组的判据。

---

## 5 验收判据核对（尺度无关, 未用物理闭合反推）

| 判据 | 目标 | 实测 | 判定 |
|---|---|---|---|
| 测光一致性（残差 MAD） | ≤0.03 mag | after 中位数 0.0617 / 最坏 0.1160 mag（before 最坏 1.8459） | **未达**（两口径都未达; 见 §6.1） |
| 帧间一致性（同组 k 峰峰） | ≤0.05 mag | 同夜帧对 after 0.0201 mag **PASS**; 跨夜组 0.05–1.76 mag FAIL（真实透明度差） | **同组可达 / 跨夜分组不可达** |
| 受控 seeing 判别力 | 红 ≥0.200 / 绿 ≤0.010 | 红 0.4606 / 绿 0.0000 | **PASS** |
| 正对照 | 回收 0.1087 mag | 0.1087 / 0.1086 | **PASS** |

**k_photo 绝对值未作任何物理闭合反推或绝对窗口判定**（k ≈ 5–8e-17 只作相对一致性比较）。

---

## 6 诚实边界与上呈事项

1. **PSF 域通量的方差代价（重要）**: PSF 域 flux 是 7 参数拟合的整平面外推（正比于 A·sx·sy）, 单星随机误差大于固定孔径盒和。
   L4 逐帧 sigma_residual 中位数仅从 0.0655 微降到 0.0617 mag（最坏板块大幅改善 16×）。
   ⇒ **"≤0.03 mag 测光一致性"在 M42 拥挤场 + 2.0–2.6 px seeing + 16bit 数据下, 两种口径都达不到**;
   若该判据是硬门, 需额外手段（D2 PSF 加权最优提取 / 更严拟合质量剔除 / 去混叠）, 请负责人裁决是否立项。
2. **L4 分组不是"同组"**: 见 §4.4, 这是当前 0/12 施加的直接原因。
3. **PSF 拟合质量是工程风险**: 实测 `psf_params[].theta` 出现 `-16820.14 rad`（未归一化到 [-π,π]）;
   不影响 flux（2πA·sxsy/3 与 θ 无关）, 但属独立缺陷, **本分片未修**（超出改动面）, 建议登记。
4. **未做数值交叉核对**: 本机无 `photutils`/`sep`（沿用定案自陈的边界）。
4b. **相邻但未改（供负责人裁决）**: `module_adapters.cpp:3754` 的 SNR 节点样本行 `row.flux_adu = s.value("flux", 0.0)`
   仍取**检测域盒和**（`p1_sources[].flux`）。按定案改动面（①-⑤）与 SCI-NOISE-001/SNR 自有口径, 本分片**未动**;
   若 SNR 层也要求 PSF 域通量, 请单独立项（同一 `psf_params[].flux` 列已就绪, 可直接复用）。
   同理 `orchestrator.cpp:2678 psf_flux[i] = row[2]` 走的是 AIO psf 块, 其 `row[2]` 本就 = `DPSFFitResult.flux`
   （`orchestrator.cpp:2459`, 即 `dpsf_psf.cpp:428`）⇒ **该路径本已符合, 无需改**。
5. **未跑 ninja/cmake/ctest**（硬约束）: 用 `g++ -fsyntax-only`（3 个改动文件全部 0 诊断）+ 独立重链 + 端到端真跑替代。
   **新增/修改的测试未在 ctest 下执行过**, 前台合并后请跑 `p1001_real_nodes_test`。
6. **docs 未改**: 检索 `docs/**` 内无 `P1_PHOT_MAX_SPREAD_DEX` 旧值引用（仅 `reports/RELEASE-02/p1-phot-fix2.md:245` 与测试注释）;
   若前台认为需同步文档, 请按 ENGINEERING_SPEC §3 走变更 claim。
7. **零 git 写权限**: 本分片未 commit / 未 push; 工作区改动待前台原子提交。

---

## 7 复现步骤与产物

```bash
export TMPDIR=/dev/shm/astrocs_finstrfix && mkdir -p $TMPDIR
cd '/workspace/Astro CS Database'
bash run/RELEASE-02/f-instr-fix/syntax_check_all.sh      # 3 文件 -fsyntax-only
bash run/RELEASE-02/f-instr-fix/build_after.sh           # 独立重编 module_adapters + ar 同名成员替换（before 库已保全）
bash run/RELEASE-02/f-instr-fix/link_runners.sh          # 链接 before/after 节点 runner
python3 run/RELEASE-02/f-instr-fix/prep_tags.py          # 从 L4 产物建 12 板块工作目录
bash run/RELEASE-02/f-instr-fix/run_all_tags.sh          # star-psf + photometry before/after
python3 run/RELEASE-02/f-instr-fix/analyze2.py           # 主表 + 帧对分解
python3 run/RELEASE-02/f-instr-fix/summary.py            # 汇总统计
bash run/RELEASE-02/f-instr-fix/build_seeing_test.sh     # 判别力测试（红/绿）
bash run/RELEASE-02/f-instr-fix/run_e2e.sh               # 端到端 normalize（after 全链）
```

产物（`run/RELEASE-02/f-instr-fix/`, gitignore）:

| 路径 | 内容 |
|---|---|
| `out/final_table.txt` | §4.2 主表 + §4.3 全部 91 帧对分解 |
| `out/summary.json` / `out/final_rows.json` | 机器可读汇总 |
| `out/seeing_redgreen.txt` | §3 判别力测试完整输出 |
| `out/flux_column_evidence.json` | flux 列样例行 + 2πA·sxsy/3 复算对照（work 大文件已清理） |
| `out/*.phot.{before,after}.p1_phot.json` | 逐板块 before/after provenance |
| `logs/*.phot.{before,after}.stderr` | 逐帧 k / n_matched / sigma 原始日志 |
| `lib/libastrocs_module_adapters.{before,after}.a`, `bin/p1_node_runner.{before,after}`, `bin/astrocs.{before,after}` | 可复核的 before/after 二进制 |
| `tools/*.cpp`, `*.py`, `*.sh` | 全部实验代码 |
| `e2e/t2_m1_red/` | 端到端真跑产物（大 FITS 已清理） |
