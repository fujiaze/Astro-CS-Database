# 变更 claim：DOC-205-EXP-LANDING-001 — 六项科学实验结论落地（EXP-201…EXP-206）

- 控制包：`工程控制/RELEASE-03` / 任务 **DOC-205**（任务 A）
- 日期：2026-09-20
- 状态：**已落地**（前台提交前复核）
- 影响类：文档口径订正 + 权威链落地（**不改任何科学公式、常数、阈值、容差、归约顺序**）
- 依据（最高权威）：`ASTROCS_DESIGN.md` §0.1（只有一份权威链）、§4.3/§4.4/§4.5、§5.3、§9.72、§9.73 A44、§11.1.1（实验佐证强制）
- 依据（流程）：`ENGINEERING_SPEC.md` §3（文档订正走变更 claim：记录证据、影响面、版本递增 + 一致性回归）、`AGENTS.md` §8（独立证据优先）
- 依据（任务书）：`工程控制/RELEASE-03/GAP_AUDIT.md` §5.1–§5.8 与 §6 变更声明表（**不得自行发挥**）

## 1 逐条落地表（文件:行 → 改前 → 改后 → 依据）

| # | 实验单元（sha256 of `PREREGISTRATION.md`，冻结判据） | 文件:行 | 改前 | 改后 | GAP_AUDIT 依据 |
|---|---|---|---|---|---|
| EXP-201 | `run/RELEASE-02/实验/E07-天光采样点权重/` `0d41e29bf6ed07fb4068e877f99d418aac10bc66ecaa9681419b35a88460bcdc` | `docs/plugins/algorithms_phase2/10_sampling.md:38`（mermaid 节点） | `P --> W["每点赋 SNR² 权重<br/>w_ki = snr_ki²"]` | `P --> W["每点赋 control_ivar 权重<br/>w_ki = control_ivar_ki = N_retained/(k_corr·(π/2)·σ_bg²)"]` | §5.6 |
| EXP-201 | 同上 | `10_sampling.md:45` | `w_ki = 1/σ²_ki ∝ SNR²_ki`（正文） | `w_ki = control_ivar_ki = N_retained/(k_corr·(π/2)·σ_bg²)`；并补「为什么不是 SNR²」边界：`∝` 关系要求**信号恒定**，天光面的值在变 ⇒ **SNR² 不是有效逆方差代理** | §5.6 |
| EXP-201 | 同上 | `ASTROCS_DESIGN.md:493`（§4.3 公式行） | 公式不变、无边界说明 | 公式**逐字不变**，**追加一句边界**：`SNR²` 不是有效逆方差代理（∝ 要求信号恒定） | §5.6 |
| EXP-201 | 同上 | `ASTROCS_DESIGN.md:537`（§4.4） | 「采样点带 SNR 权重」 | 「采样点带**控制权重**（`control_ivar`）」+ 同一边界句 | §5.6 |
| EXP-202 | `run/RELEASE-02/实验/E08-NaN处置/` `539d83a0560b48cc2de605ec332eafefcc535bbac596cc783cb473664fa13c55` | `docs/science/DRIZZLE.md:116`（NaN 处置行） | 「NaN **传播、不掩膜**」（与生产实现相反） | **样本级掩膜 + 重归一 + 覆盖率级 NaN + 强制计数**（`n_rejected_nonfinite`），`rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN`；与 DOC-202 在 `docs/standards/NUMERIC_STANDARD.md`/`STANDARDS_REGISTRY.md` 与 `docs/interfaces/data/DATA-002_PHASE_*.md` §2a 的**同一 rule_id、同一文字**；旧表述标「已作废」留痕 | §5.1 |
| EXP-202 | 同上 | `docs/algorithms/DRIZZLE_GEOMETRY.md:120`、`:234` | `:120` 只写规则；`:234` 的 `DISP-DRZ-004` 锚 `DRIZZLE.md:96` | `:120` 记录**实现现状**（静默 `continue`、**未暴露计数**）为对 `rule_id` 的开放偏差（代码修复 → `P1-DRZ-IMPL`）；`:234` 锚订正为 `DRIZZLE.md:116` | §5.1 |
| EXP-203 | `run/RELEASE-02/实验/E09-Phase2信号量纲/` `562d9f7447b91f650d547ce0a322fc519e91de270956da9c49094b7f163d5de0` | `docs/algorithms/PHASE3_PROJ_IMPL.md` §16（新增）、`docs/science/PHASE3_HIPS_TO_FITS.md` §16（新增） | 无 | 登记 **C1a/C4/C5/C6/C9**（`lib/**` 侧，只登记不改码）+ 复核结论（**C1/C1b/C2/C3/C8 已由 DOC-202 完成**，逐条点名）；明确「**§5.3 未在生产生效**」（守卫未接线） | §5.4 |
| EXP-204 | `run/RELEASE-02/实验/E10-小N排异/` `bd8982d67bf8534383638af2b851598ec7f8a082972eff3c40ffb221259d6326` | `docs/science/REJECTION.md:48`（§4 偏差代价） | 未量化 | 补**偏差代价量化**：`N=2` 泄漏 **1/2**、`N=3` 泄漏 **1/3**；低电平 `N=3` 精度损失 `ρ−1` = **0.3%–27%**（≫ `τ_ρ` = 0.31%）；`N=2` **83.5% 像素无输出** | §5.8 |
| EXP-204 | 同上 | `REJECTION.md:52` | 无科学依据与实验号 | 补科学依据（泄漏分数 = 1/N 的秩论证）+ 实验号 EXP-204 + BPP 锚 | §5.8 |
| EXP-204 | 同上 | `REJECTION.md:36`（SC-005 行）与 §16（新增） | 「全拒容错」未给可达条件 | 补 **SC-005 三条可达条件**（① 几何 `n=4`；② 路由分派 percentile；③ 百分位带退化）+ 电平依赖（翻转边界 ≈3000–3400 e⁻/pix，**超出实验网格上界 1734 e⁻/pix，属外延**） | §5.8 |
| EXP-205 | `run/RELEASE-02/实验/E11-SNR三口径精度/` `812440535cf77f2248876a6f61f165ad6606bb2a1df6275c6596fdf2aa18528d` | `docs/plugins/algorithms_phase1/07_noise_snr.md:138`（config 行） | 「实测 SNR 场相关长度 ℓ = 40.4–57.9 px ⇒ Δ/ℓ ≈ 1.1–1.6」（隐含「近临界=安全」） | 原地补记：**同一 testdata** 上按操作定义 `ℓ_SE(P=32)` 实测 **ℓ = 50.9–208.3 px**、`Δ/ℓ = 0.31–0.84`；失效边界 `Δ*/ℓ = 0.50–0.98` ⇒ Δ=64 的安全性来自**实际 ℓ 更大**，**不是**「近临界」 | §5.7 |
| EXP-205 | 同上 | `ASTROCS_DESIGN.md:518`（SP-0） | 帧级精度门取 `K·s_field` | **追加**：登记 E11 的**判据退化发现** —— 帧级臂 `RMSE_frame ≡ s_field`（定义上恒等）⇒ 该门**恒为真**、不携带信息；并记实测适用域（testdata M42 三帧稀疏 0.093/0.228/0.099 vs 帧级 0.111/0.384/0.115；HST M16 帧级 0.112 vs 稀疏 0.261/稠密 0.198）与 `Δ*/ℓ ≈ 1` | §5.7 |
| EXP-205 | 同上 | `docs/science/NOISE_MODEL.md:86` | 「帧级标量 ≈ 1.152 × 稀疏」类换算 | **复核通过，不改**（见 §3） | §5.7 |
| EXP-206 | `run/RELEASE-02/实验/E12-噪声模型两套/` `d5119c6f0b3c78e7691337d915ea91b67723fcca2d158d1b9acdd97f22be00ad` | `docs/plugins/algorithms_phase1/07_noise_snr.md:108-127`（§4.4） | 把 A/B 并列写为「两套实现」 | 改写为「**噪声模型两套实现：取 A（EXP-206 定案；B 已退役）**」：A = `lib/algorithms/noise_snr/cpp/src/noise_model.cpp`（唯一生产实现），B 退役（HEAD `f0d4a468`）；A 编入 `astrocs_phase1_noise`（`CMakeLists.txt:644-659`，主程序链接 `:747`）；E12 数字 A vs B；`variance_floor` 伪有效模型加固 → `P1-NOISE`；A44 留痕保留。**段落行数保持 20 行（108–127）不变** | §5.2 |
| EXP-206 | 同上 | `ci/ledgers/dormant_algorithms.json` | 7 条 `dormant_symbol:astrocs.p1.noise:*` 陈旧条目（snr_noise_model_v1、_f64、_default_config、_fill、_free、snr_noise_scale_law、snr_noise_gain_variance） | **删除 7 条**（条目 67 → 60）；逐条 `nm -C build/astrocs` 复核**均已出现在生产二进制** ⇒ 台账「只减不增」合规 | §5.2 |
| EXP-206 | 同上 | `docs/science/NOISE_MODEL.md:150`（Oracle 平面场恢复） | 未含 patch 内天光梯度真值项 | 原地补：**强制**真值项 `((dμ/dx)²+(dμ/dy)²)·P²/12`（省略时假红 +49.9%）；E12 D-05 数字（含项 26.04 ADU² 时 +3.56%/+3.67%/+4.05%）；备选「同几何独立参考」 | §5.2 |
| EXP-206 | 同上 | `docs/algorithms/NOISE_ESTIMATION.md:120-128`、`:256-262`、`:203` | 称 `wrapper_phase1/noise_model.{h,cpp}` 为现行封装、CMake 锚 `:620-632`/`:706`、死锚 `wrapper_phase1/noise_model.h:33` | 标注 B **已退役、文件已删除**；明确「现行生产唯一实现 = A」；CMake 锚订正 `:644-659` / `:747`；死锚按 `ANCHOR_CONTRACT.md` §5 处置（见 `DOC-205-ANCHOR-SYMBOL-001`） | §5.2 |

## 2 `ASTROCS_DESIGN.md` 三处改动的逐字实验证据

### 2.1 §4.3:493 与 §4.4:537（EXP-201）

> E07 `README.md` §0 逐字：「**在生产型（柔性稀疏样条 + 逐帧平面）天光面模型下，`SNR²` 与 `control_ivar` 对重建天光面的影响不可区分**」；
> 「**因此可判定的结论是**：1. **不可区分**（主配置，`H_null` 成立）……2. 在可区分的刚性极限下 **`control_ivar` 更优**（`H_ivar` 成立），`SNR²` 更差；3. 无论哪个配置，**差异都不足以改变任何科学结论**」；
> 「**据此的落地建议**：科学权重取 **`control_ivar`**……订正 `docs/plugins/algorithms_phase2/10_sampling.md:38,45` 与 `ASTROCS_DESIGN.md:537` 的 `w ∝ SNR²`；**但订正理由必须写成「`SNR²` 不是有效的逆方差代理（∝ 关系要求信号恒定，而天光面恰恰是变化的）」，不得写成「`SNR²` 已被证明会劣化天光面」**（后者本实验不支持）」；
> §2「设计文档写的 `w = 1/σ² ∝ SNR²` 只在「信号恒定」时成立；天光面拟合的对象恰恰是**变化的**天光面 ⇒ 该比例关系由实验判定」。

**本 claim 严格遵守该措辞约束**：全部落地文字只使用「`SNR²` 不是有效逆方差代理」，**未出现**「`SNR²` 已被证明劣化天光面」类表述。

### 2.2 §4.3:518（EXP-205 / SP-0）

> E11 `README.md` §4.6 逐字：「**P1 退化是结构性的**：帧级臂的 RMSE 在定义上**恒等于** `s_field`（常数场 vs 中位归一真值场），而 `tau_A(frame) = K·s_field`（K=1.25）⇒ **帧级精度门永远为真**，不携带任何信息。」
> §5 逐字：「**判据 SP-0 本身有两处退化**（帧级精度门恒绿、偏差门灵敏度不足 1 个数量级），已给可执行订正建议（`§4.8）」。
> 偏差表 D2 逐字：「`ASTROCS_DESIGN.md:518`（SP-0 三口径精度对比判据）｜帧级口径的精度门取 `K·s_field`｜记录 E11 的**判据退化发现**：帧级臂 RMSE 在定义上恒等于 `s_field` ⇒ 该门**恒为真**」。

## 3 复核结论：不改项（逐条给数）

| 项 | 文件:行 | 复核 | 数字 |
|---|---|---|---|
| EXP-205 D3 | `docs/science/NOISE_MODEL.md:86` | **复核通过，不改** | 该行口径「帧级标量 ≈ 1.152 × 稀疏」与 E11 一致：设计系数 `1.152 × 1.25 = 1.44`；E11 解析 `1.1525 → 1.4406`、MC `1.1662 → 1.4578`（差 ≤ 1.3%，属 MC 噪声） |
| EXP-203 C1/C1b/C2/C3/C8 | registry/hips_p2/DATA_SEMANTICS | **DOC-202 已完成**，本包只复核不改 | `UnitId::SURFACE_BRIGHTNESS`；锚 `:1040-1057`；`ADU/px²`；`astrocs_support_clamped_pixels = 262144` |
| EXP-204 档位表 | `docs/plugins/algorithms_phase2/12_rejection.md` §9 | 已存在冻结路由表（`1≤N≤3 none / 4≤N≤5 percentile / 6≤N≤15 winsorized / N≥16 linear fit`）⇒ 不改 | 与 §5.8 终表一致 |

## 4 验收证据

- `timeout 900 python3 ci/run_checks.py --check CHK-SCI-REF CHK-DANGLING CHK-CONTRACT-TEST` → 见 `run/RELEASE-03/logs/DOC-205-gates-final.log`；
- `timeout 600 python3 tests/config/check_cfg002_registry.py --self-test` → `SELF_TEST PASS injections=21 problems=0` / `checks=11 pass=11 fail=0`（**config 锚 DRIFTED=0**）；
- `timeout 900 python3 docs/standards/checks/check_standards_registry.py --root .` → `STANDARDS_REGISTRY_PASS` rc=0；
- `timeout 600 python3 ci/check_no_weight_mode.py` → `CHK-NO-WEIGHT-MODE_PASS: files=346 lines=57882` rc=0；
- `timeout 900 python3 -m pytest tests/contracts -q` → `68 passed`。

## 5 影响面与残留

- **影响面**：仅文档口径与台账（`docs/science/**`、`docs/algorithms/**`、`docs/plugins/**`、`ci/ledgers/dormant_algorithms.json`、`ASTROCS_DESIGN.md` 指定行）；
  **零代码改动、零公式/常数/阈值/容差改动、零 `config/**` 改动、零 `ci/checks.json` 改动、无 waiver**；
- 所有行锚敏感文件（`REJECTION.md`≤164、`NOISE_MODEL.md`≤85、`PHASE3_HIPS_TO_FITS.md`≤39、`07_noise_snr.md`≤140、`DRIZZLE.md`≤33）的改动**逐行等长或在末尾追加**；
- **残留（归 `lib/**`，本包只登记）**：EXP-203 C1a/C4/C5/C6/C9、EXP-202 `P1-DRZ-IMPL` 计数暴露、EXP-206 `P1-NOISE` `variance_floor` 加固；
- **残留（科学口径）**：`ASTROCS_DESIGN.md` §4.5 下表仍写 `N<6 → percentile`，与 §5.8 终表 `1≤N≤3 → none` 的差异由内核闸 `underdetermined_n=3` 在计划层解释（§16 已写明不得用 `plan.method` 断言「N≤3 ⇒ none」）；设计文档未逐字陈述该闸，**未改**（不在本包变更清单内），登记待裁决。
