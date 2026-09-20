# 变更 claim：DESIGN-GAP-LAND-001 — 最高设计增补落地（星表引导检测 / apply photometry 节点 / 物理噪声合成 / UPM 与权重 formulation）

- 控制包：RELEASE-02 / 分片 **DESIGN-GAP-LAND**（设计增补落地）
- 变更对象（本轮**已改**）：
  - `ASTROCS_DESIGN.md`（**①最高权威**）—— 13 条增补，逐条见 §2；
  - `ci/checks.json` + `ci/id_migration_map.json` + `docs/ci/01_CHECKS.md` §2 —— 新增门禁 **CHK-SPEC-NAMED-IMPL-ON-PROD-PATH** 的注册（DG-C-01，见 §3）；
  - 新增（非文档变更面）：`ci/check_spec_named_impl.py`、`ci/spec_named_impls.json`（登记表）、`ci/ledgers/spec_named_impl_gaps.json`（缺口台账）。
- 台账来源：`reports/RELEASE-02/design-gap-synthesis.md` + `.json`（86 条，字段 `id/target/file/loc/current/proposed/rationale/basis/needs_claim/priority/status`）
- 日期：2026-09-19
- 依据条款：`ENGINEERING_SPEC.md` §3（科学正确性优先 + 变更 claim + 一致性回归）；`AGENTS.md` §8；`ASTROCS_DESIGN.md` §0（改本文须负责人批准并记录变更原因与影响面）
- 状态：**ASTROCS_DESIGN.md 增补已落地**；门禁已注册并自检通过；**doc 侧 20 条 needs_claim 条目仅登记未改**（见 §4）；**未 commit**（本分片零 git 写权限，交前台原子提交）

## 1 授权与依据（逐条可追溯，无自创裁决）

本轮全部改动均为**已有负责人裁决/既有 claim 的转录落地**，不新立科学语义：

| 授权来源 | 覆盖条目 |
|---|---|
| `GAP_AUDIT.md` §9.49 定案 1（星表引导检测） | DG-A-01、DG-A-09 |
| `GAP_AUDIT.md` §9.49 定案 2（帧间独立） | DG-A-02（§3.5） |
| `GAP_AUDIT.md` §9.38 D-1/A2 + §9.40 C3（UPM 纯加性） | DG-A-05（前台已改，本轮只登记）、DG-A-10、DG-A-11 |
| `GAP_AUDIT.md` §9.38 D-4/A1 + §9.39 C1 + §9.49 定案 7（帧级 SNR 红线/定义式） | DG-A-04、DG-A-13 |
| `GAP_AUDIT.md` §9.42（消除物理单位/禁物理闭合） | DG-A-06 |
| `GAP_AUDIT.md` §9.29/§9.37 指令 1/§9.39 A4（apply_photometry 零调用者） | DG-A-07 |
| `GAP_AUDIT.md` §9.41 + §9.47（合成测试物理噪声过程） | DG-A-08 |
| `GAP_AUDIT.md` §9.46（SNR 三路径/默认稀疏/产物边界） | DG-A-03（前台已改，本轮补 §3.4/§4.3） |
| `GAP_AUDIT.md` §9.28 Q1/Q2 + §9.30 + §9.33（UPM formulation / gauge） | DG-A-11 |
| `GAP_AUDIT.md` §9.28 Q3 + §9.30 ⑤ + §9.23（堆叠权重 PΣPᵀ） | DG-A-12 |
| `GAP_AUDIT.md` §9.48 ② + §9.49 定案 1（检测权威源） | DG-B-11（**未改**，待 D-04） |
| `GAP_AUDIT.md` §9.44（负责人令新增门禁） | DG-C-01 |
| change-claim `FIX-SCI-SNR-CANON-001` §2/§3.1/§3.2/§3.4/§3.6 | DG-A-03/04/05/06/10/11/12/16 |
| `reverse_verify/docs/frame-snr-canon.md` §2.3/§2.4/§3.0/§4.2 | DG-A-04、DG-A-08、DG-A-13 |
| `reverse_verify/docs/snr-propagation-design.md` §3.3/§3.7/§13 | DG-A-11、DG-A-16 |

## 2 `ASTROCS_DESIGN.md` 逐条增补登记（每条：条目 → 落点 → 依据）

| # | 台账条目 | 落点（改后行号） | 增补内容摘要 | 依据 |
|---|---|---|---|---|
| 1 | DG-A-01 星表引导检测（完整条文） | §3.2:118-119；§3.6:247-249 | 检测定义域 = Gaia 投影位置；只拟合星表位置；失败直接丢弃；top 2–5 万；极限星等**宁多勿少**且与 WCS CD 板比例互校、**禁物理闭合**；取消盲检测权威性 | §9.49 定案 1；§9.43；§9.48② |
| 2 | DG-A-07 apply photometry 节点 | §3.2:109（流程图新节点）+ :120（条文）；§3.6:252 邻域 | 流程补 `apply photometry → 像素`；`I_photo = k_photo·m(x,y)·I_cal`，`m` 低阶、**用星点估**；`photappl`/`photscal` 如实落盘；未启用 `degraded_reason` + fail-closed | §9.29；§9.37 指令 1；§9.39 A4 |
| 3 | DG-A-08 物理噪声合成 | §11.1:618-619；§11.3:650（L1 行） | Poisson(电子域)+Gaussian 读出+增益量化+平场乘性+天空梯度；**严禁纯加性天光**；负例「真值无效应 ⇒ 度量归零」 | §9.41；§9.47；frame-snr-canon §3.0 |
| 4 | DG-A-04 帧级 SNR 定义式 | §3.4:168-176 | `SNR_frame=F_ref/σ_F`；`σ_F⁻²=ΣP_i²/σ_i²`（Horne 1986 对角）；gain 缺省分支；单调性/B→∞⇒0；点源口径；`F_ref` 落盘作用域 | §9.38 D-4/A1；§9.39 C1；§9.49 定案 7 |
| 5 | DG-A-06 测光标定语义 | §3.1:94；§3.6:250 | 目标 = 真实测光坐标系（星等）；消除物理单位；**禁物理闭合反推**；**禁绝对窗口**；判据只有尺度无关两条 | §9.42（负责人逐字纠正） |
| 6 | DG-A-09 单一星表/单一通量/三处复用 | §3.2:119；§3.6:251 | 一次检测→一次通量积分→测光/PSF/SNR 共用 `star_id`；唯一 `flux` 口径 `2πA·sxsy/3`(ADU)；`fwhm_px` 轮廓定义生产者声明、禁跨块反解 | §9.49 定案 1 末段；§9.43 A6 |
| 7 | DG-A-10 加性移除依据+实验证据 | §4.4:311 | 理论（正确归一化后乘性残留归 Phase1 空间增益）+ 世界(i)/(ii) 数值 + 证据文件指向 | §9.49 定案 5 |
| 8 | DG-A-11 UPM 公共面 formulation | §4.4:313-320 | (c) 排除自身 + 阻尼 α≈0.5 + 拟合/叠加权重同源 + 末端扣残差场；gauge 平移不改接缝（不得夸大） | §9.28 Q1/Q2；§9.30；§9.33 |
| 9 | DG-A-12 堆叠权重 = PΣPᵀ | §4.3:292-293 | `w_fit` ≠ `w_stack`；`Var(c_i(p))=Σ_j P²σ²` + 梯度协方差；禁无条件加 `Var(ĝ)`；过渡期不得声称逆方差加权 | §9.28 Q3；§9.30⑤；§9.23 |
| 10 | DG-A-14 σ 来源（掩膜 patch 红线） | §3.6:252 | `σ_bg` 必须 8×8 patch + 逐星掩膜 + 饱和过滤 + 天空预算门；禁整帧未裁剪 MAD；单一 σ 口径 | CONFORM-SWEEP-2 NS-01/NS-07；snr-propagation-design P1-1..3 |
| 11 | DG-A-15 WCS 绝对零点进验收 | §11.1:626-628 | 真实数据验证必须含：Gaia 绝对零点残差及空间恒定性、`samples[]` 原点一致性、SIP 声明与残差量级相容 | §9.48 旁支；§9.49 负责人第 8 条 |
| 12 | DG-A-16 稀疏层数据面语义 | §3.4:190-192 | 存无量纲相对场 `rho_c`（p50=1）；`SNR=SNR_frame×rho_c`；`value_semantics`/`support_scale_px` 冻结在合同；重建算子必须返回预测方差 | §9.46；snr-propagation-design §3.3/§3.7 |
| 13 | DG-A-03 三路径补全（前台已改 §4.3:256） | §3.3:127-129（`sparse_snr_spacing_px`）；§3.4:189-192（Δ=64 px）；§4.3:279-303（路径显式 + 不得预设稀疏最优 + fail-closed） | 配置键/默认值/间隔落点补齐；三路径由 JSON 显式指定、默认稀疏重建、**不得静默降级** | §9.45；§9.46；snr-config-land |

**登记不改（前台已落地，本轮不重改）**：DG-A-05（§4.2:266 mermaid 纯加性）、DG-A-02 的 §4.3 部分（前台标注）、DG-A-03 的 §4.3:288 首条。
**注意（编号口径）**：任务书把「§4.3:256 自动检测」写作 DG-A-02、「§3.2:103 星表引导」写作 DG-A-03；本 claim 一律以 `design-gap-synthesis.json` 的台账 id 为准（DG-A-03 = 三路径；DG-A-01 = 星表引导；DG-A-02 = 帧间独立）。

## 3 门禁 DG-C-01（`CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`）

- 注册：`ci/checks.json`（top-level id + 2 steps：`SPEC-NAMED-IMPL-ON-PROD-PATH`、`…-SELFTEST`）；`docs/ci/01_CHECKS.md` §2 同步行；`ci/id_migration_map.json` 的 `targets`/`mappings`/`coverage.absorbed_entries` 登记（R13 通过）。
- 登记表：`ci/spec_named_impls.json`（5 条：sdet 生产闭包 + sdet 调用点 + noise_model 权威源 + p3_v6_export 构建图 + 清单声明一致性）。
- 缺口台账：`ci/ledgers/spec_named_impl_gaps.json`（4 条，**不是豁免**：stdout 打印 `ledgered_gaps`、JSON 落盘、`--strict` 判红、条目不再复现判 `ledger_stale`）。
- 自检：`python3 ci/check_spec_named_impl.py --self-test` = 13 组（正例 3 + 负例 6 + fail-closed 4）全绿；真仓 `PASS entries=5 passed=1 ledgered_gaps=4`；`--strict` = `FAIL 8 findings`（4 缺口 + 4 ledger_stale，因 --strict 忽略台账）。
- 真实构建图红绿实证（`/dev/shm` 真仓结构夹具，未改真仓 CMake）：摘除 `astrocs_p1_sdet` 链接 ⇒ `SNI-S1-002-PROD-PATH` 判红 rc=1；恢复 ⇒ rc=0。把生产节点改为调用 `sdet_create` 并删对应台账条目 ⇒ 该条 `pass`、无 stale。

## 4 doc 侧 `needs_claim=true` 20 条：**登记，未改**（本轮范围见任务书第 5 条，收口令下不再展开）

| 条目 | 应走流程 | 本轮处置 |
|---|---|---|
| DG-B-08 | 既有 claim 补命名订正行（`algorithm_snr_path`→`snr_path`） | **未改**（claim 文件属前台面；语义/键名已由 `snr-config-land` 落地） |
| DG-B-09 | `docs/science/PHASE2_UPM.md` §14a 补实验证据 | **未改**（数值已在 `ASTROCS_DESIGN.md` §4.4:311 落地） |
| DG-B-12 | `06_photometry.md` 补 m(x,y)/应用点/F_instr | **未改**（m(x,y) 与落盘要求已在 §3.2:120/§3.6 落地；F_instr 口径待 D-05） |
| DG-B-15 | `UNIFIED_MODEL.md:42` 补红线判据指向 | **未改** |
| DG-B-10 / DG-B-11 / DG-B-S2-PH-05 / DG-B-S2-PH-06 | **待负责人裁决 D-03/D-04/D-05** | **未改**（按硬约束「D-01..D-10 只登记不动」） |
| DG-B-S1-024 / S2-UNIT / S2-PR-02 / S4X-AIO02 / S4X-AIO03 / S4X-AIO25 / S4X-P3X13 | 合同/SCI 变更流程（方向未定） | **未改**（登记待裁决） |
| DG-B-S2-DZ-07 / DZ-08 / DZ-12 / PR-04 / S3-020 | 规范侧订正（DZ-07/DZ-08/DZ-12/S3-020）或实现接线（PR-04） | **未改**（登记；其中 DZ-08/PR-04 含 `lib/**` 面，本分片不改 `lib/**`） |

## 5 影响面与一致性回归

- **影响面**：最高设计条文（新增/细化，**不改任何科学公式与容差数值**）；CI 注册表（+1 检查项，2 steps）；文档一致性（§2 表同步）。
- **回归**：`ci/check_registry_doc_sync.py` PASS（54 == 54）；`tools/quality/check_impact_map.py` `IMPACT_MAP_PASS`；`ci/run_checks.py --check CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` PASS（2/2 steps）；`python3 -m py_compile` 通过。
- **已知非本分片红**：`ci/validate_registry.py --strict` 仅剩 `R13 孤儿 unit: CTEST-AIO-{CHECKSUM,PRECISION_DUAL,QUERY_PIXEL,TILE_MODEL,TRANSFORM}`，由**工作区他人未提交改动**引入（`git diff ci/checks.json` 显示这 5 个 step 为新增行），本分片未触碰、不代修。
- **未验证项**：未跑 `ninja/cmake/ctest`（任务硬约束）；未跑 e2e（前台统一跑）。
