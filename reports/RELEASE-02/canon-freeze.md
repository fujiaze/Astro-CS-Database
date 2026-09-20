# RELEASE-02 设计意图固化报告（CANON-FREEZE）

- 分片：CANON-FREEZE（设计意图固化）
- 工作目录：`/workspace/Astro CS Database`
- 日期：2026-09-19
- 变更 claim：`工程控制/RELEASE-02/change-claims/FIX-SCI-SNR-CANON-001.md`
- 依据条款：`ENGINEERING_SPEC.md` §3（科学正确性优先 + 变更 claim + 一致性回归）；`AGENTS.md` §8
- 裁决来源：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.37（:1003-1030）、§9.38（:1031-1069）、§9.39（:1070-1113）、§9.40（:1114-1142）

## 0 本轮固化的裁决（D-1..D-4 + C2）

| 编号 | 裁决 | 落点 |
|---|---|---|
| D-1 / A2（含 C3） | UPM = **纯加性**；`g_k ≡ 1` 不启用；`÷g²` 恒等式；理论依据入文档 | SCI-UPM、UPM/sampling 插件、UNIFIED_MODEL、PHASE2_DETAILED_DESIGN |
| D-2 / A3 | SNR **三条路径**（dense / sparse→稠密 / 帧级→稠密）；**默认稀疏**；配置文件 JSON 显式指定；论文核心实验（SP-0） | 07_noise_snr、13_integration、UNIFIED_MODEL |
| D-3 / B1 | **只有 Phase1 HiPS 带 SNR 数据块**；Phase2 不输出 SNR 面（叠加中消费、不复用）；Phase3 无 SNR；合同只冻结 variance/ivar | 07_noise_snr、UNIFIED_MODEL、13_integration |
| D-4 / A1 + C1 | 帧级 SNR = **PSF 信号 SNR，纯信号/噪声**；`F_signal` 已扣局部背景、天光只进 `σ_F`；单调性；点源口径不得与面亮度 SNR 混用；两篇权威互斥定案 | 07_noise_snr、CONTROL_WEIGHT_SNR、PSF_SIGNAL_WEIGHT、UNIFIED_MODEL |
| C2 | 像素剔除算法**自研** ⇒ canonical profile = `astrocs_adaptive_pixel`；`wbpp_2_9_1` 降为对照档 | SCI-REJ + 合同/算法/模块文档共 11 篇 |
| D-5（§9.42） | 测光标定 = **消除物理单位、只使用星等**；`k_photo` 绝对值无物理意义；**禁**物理闭合反推；判据只有测光一致性 + 帧间一致性（均尺度无关） | SCI-PHOT、06_photometry 插件、CALIBRATION_ALGORITHMS |

## 1 supersede 处理

- 新 claim `FIX-SCI-SNR-CANON-001` §2 明确 **否决 / supersede** `FIX-A-UPM-001` 的模型订正提议
  （该 claim 提议把 `PHASE2_UPM.md` 由纯加性改为乘性 `y_k=g_k·s+b_k`，与负责人 A2 裁决**方向相反**）；
- `工程控制/RELEASE-02/change-claims/FIX-A-UPM-001.md` 状态行改为
  **「已否决 / SUPERSEDED by `FIX-SCI-SNR-CANON-001`（2026-09-19）」**，并说明其证据面保留为
  **加性天光面表示层**参考、不得再用于支撑乘性模型。

## 2 改动清单（文件:行 + 依据裁决）

### 2.1 变更 claim / 控制面

| 文件 | 行 | 内容 | 依据 |
|---|---|---|---|
| `工程控制/RELEASE-02/change-claims/FIX-SCI-SNR-CANON-001.md` | 新建（全篇） | 裁决原文、supersede、逐条订正决定、影响面、未闭合项 | §3 流程 |
| `工程控制/RELEASE-02/change-claims/FIX-A-UPM-001.md` | :8 | 状态 → SUPERSEDED | D-1 |
| `工程控制/RELEASE-02/DOC_PACK_MANIFEST.json` | :50-54、:78-83、:85-118 | 刷新 7 条 `authorized_edits`（本 claim 6 篇 + 2 条**既有**未回填哈希，见 §3.3） | §3 第 3 点 |

### 2.2 D-1（纯加性）

| 文件 | 行 | 改动 |
|---|---|---|
| `docs/science/PHASE2_UPM.md` | :8 | §1 非目标标注「本期决议：纯加性模型，`g_k ≡ 1` 不启用」 |
| `docs/science/PHASE2_UPM.md` | :81 | §6 假设同口径订正（乘性残留归 Phase1 低阶空间增益） |
| `docs/science/PHASE2_UPM.md` | :181-182 | §14a：稀疏样条改标「**加性**天光面的表示候选」；**UNRESOLVED 关闭**为「本期决议：纯加性」，写入负责人理论依据、`g_k≡1`、`÷g²` 恒等式、Phase1 `I_photo=k_photo·m(x,y)·I_cal` 分工 |
| `docs/plugins/algorithms_phase2/11_upm.md` | :5-6、:10、:19、:27-33、:53、:60、:66、:86、:99 | 职责/权威依据/输出/模型式/目标函数/gauge/协方差/entrypoint/Oracle 全链由「乘性+加性」订正为**纯加性** |
| `docs/plugins/algorithms_phase2/10_sampling.md` | :5、:51、:84 | 采样职责改为定**加性**校正场与加性天光面 `C_k(x)`；`g_k ≡ 1` |
| `docs/design/UNIFIED_MODEL.md` | :42、:45-46、:56 | 数据对象表补帧级 SNR 红线、`sparse_snr_layer` 默认产出、`snr_path` 三路径、SNR 产物边界 |
| `docs/design/PHASE2_DETAILED_DESIGN.md` | :23-29 | §4 模型式 `y_k=s(x)+C_k(x)`（纯加性），删除 `g_k` 乘法响应表述 |

### 2.3 D-2 / D-3 / D-4（SNR）

| 文件 | 行 | 改动 |
|---|---|---|
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | :5-6 | 职责/非职责：Phase1 HiPS 为**唯一**带 SNR 数据块产物；不替 Phase2/Phase3 产出 SNR |
| 同上 | :20-26 | 输出合同：`frame_snr` 标注点源 PSF 信号 SNR；`sparse_snr_layer` 默认产出；显式「不输出」条款 |
| 同上 | :44-50 | **帧级 SNR 红线**（纯信号/噪声、`F_signal` 已扣局部背景、天光只进 `σ_F`、单调性、点源口径、C1 注入-回收验收；具体定义式待 FRAME-SNR-CANON） |
| 同上 | :81-99 | §4.2 重写为**三条路径表 + 默认 `sparse_reconstruct` + SP-0 诚实约束 + 不静默降级** |
| 同上 | :110-115 | §5 配置表：`sparse_snr_layer` 默认 **true**；新增 `snr_path`（Phase2 面键，默认 `sparse_reconstruct`）；`sparse_snr_density` 联锁缺口注 |
| 同上 | :120、:124-128、:133-137 | §6 接口 / §7 错误边界 / §8 测试 Oracle（三路径对比、注入-回收、单调性负例、口径分离负例） |
| `docs/plugins/algorithms_phase1/08_drizzle.md` | :46 | `sparse_snr_layer` 默认 false→**true**（默认稀疏路径） |
| `docs/plugins/algorithms_phase2/13_integration.md` | :18-22、:34-36、:87-88 | SNR 三路径 + 配置显式指定 + 无层时**显式记录实际路径**（不静默）+ 损坏层 fail-closed + SP-0 测试 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | :16-21、:33、:58、:120、:148 | **同名两义分离**定案：Phase1 `frame_snr` = 科学量（点源 PSF 信号 SNR）；stage2 `local_snr`/`frame_snr_medians` = 相对质量场（改名 `quality_weight`）；**§9 UNRESOLVED 关闭** |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | :117 | 原 UNRESOLVED → 指向同名两义分离定案 |

### 2.4 C2（像素剔除自研档）

| 文件 | 行 | 改动 |
|---|---|---|
| `docs/science/REJECTION.md` | :21、:38、:47-62、:88、:161、:173、:177 | 符号表/有效域/§5 连续定义（生产默认档 + 内置映射 + **适用域** + 与 WBPP 三处偏离）/不变量/§9a/§14 文献定位/§14a 溯源注 |
| `docs/algorithms/PHASE2_REJECTION.md` | :187-194、:395-396 | profile 合法集与 canonical 订正；工具链现状 vs 生产默认标注 |
| `docs/algorithms/REJECTION_ALGORITHMS.md` | :54 | 伪代码注释订正 |
| `docs/algorithms/PHASE2_MOSAIC_WRITE.md` | :80-84 | 计划解析：`wbpp_2_9_1` 标为对照档/工具链现状；生产编排入口默认 `astrocs_adaptive_pixel` |
| `docs/contracts/PUBLIC_API.md` | :11-14、:1163、:1326-1332 | 接口说明与默认值表订正（含工具链默认分歧标注） |
| `docs/contracts/DATA_SEMANTICS.md` | :1043、:1381-1385、:2502 | 生产表默认值、plan 解析语义、HiPS provenance 键来源订正 |
| `docs/validation/SCIENCE_FREEZE.md` | :16-22 | 新增 `ASTROCS_REJECT_PROFILE = FROZEN`；`WBPP_AUTO_POLICY` 改标对照档 |
| `docs/development/CONFIG_SCHEMA.md` | :25、:46-56 | profile 取值域与「production 默认」说明订正 |
| `docs/modules/phase2_rej.md` | :44-46 | 模块页 AUTO 解析订正 |
| `docs/modules/registry/astrocs.phase2.reject.md` | :40-42 | registry 页订正 |
| `docs/modules/registry/astrocs.phase2.write.md` | :87-89 | 配置说明订正（工具链默认 vs 生产默认） |

### 2.5 D-5（测光标定消除物理单位，GAP_AUDIT §9.42）

| 文件 | 行 | 改动 |
|---|---|---|
| `docs/science/PHOTOMETRY.md` | :9 | §1 新增**测光标定语义**（真实测光坐标系/星等；消除物理单位；`k_photo`/`scale` 绝对值无物理意义；禁物理闭合式；判据 = 测光一致性 + 帧间一致性） |
| `docs/science/PHOTOMETRY.md` | :105-106 | §10 不可接受变化：物理闭合反推仪器参数；为标定因子设**绝对窗口** |
| `docs/plugins/algorithms_phase1/06_photometry.md` | :6 | §1 边界：不以物理单位论证标定因子（`a_k`/`k_photo`） |
| `docs/algorithms/CALIBRATION_ALGORITHMS.md` | :266-270 | §3.6 `I_photo=k_photo·I_cal` 处补 `k_photo` 语义与尺度无关验收 |

> 本分片**未使用**任何物理闭合式（`k = g·h·c·1e9/(A·t)` 之类）；D-1 中引用的 `I_photo = k_photo·m(x,y)·I_cal`
> 是负责人 `GAP_AUDIT` §9.37 给出的模型式，本 claim 只把它作为**相对**标定因子引用，未论证其绝对量值。

**未改**（文件域外，已在 claim §4.3 登记）：`contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67`、
`lib/algorithms/coverage/include/astro/phase2/stage2_common.h:69`（工具默认 `wbpp_2_9_1`）、
`config/templates/*.json`、`contracts/schemas/phase_config_*.schema.json`、`config/defaults.json`、`ASTROCS_DESIGN.md` §3.3。

## 3 一致性回归

### 3.1 结果汇总（全部本地实跑，日志见 `run/RELEASE-02/canon-freeze/logs/`）

| 检查 | 命令 | 结果 |
|---|---|---|
| DOC-101 文档包核验 | `python3 工程控制/RELEASE-02/verify_doc_pack.py` | **PASS**（R1 0 / R2 0 / R3 0 / R4 0；授权差异 **12** 条；36 篇全在位） |
| 检查注册表 ↔ 文档 | `python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet` | **PASS**（entries=1 steps=2 pass=2） |
| 配置默认一致性 | `python3 ci/run_checks.py --check CHK-CONFIG-DEFAULTS --quiet` | **PASS**（steps=1 pass=1） |
| 悬空引用/文档符号 | `python3 ci/run_checks.py --check CHK-DANGLING --quiet` | **PASS**（steps=2 pass=2；`check_doc_symbols` PASS docs=187） |
| 科学引用/行锚（8 步） | `python3 ci/run_checks.py --check CHK-SCI-REF` | **FAIL 7/8**：仅 `DOC-LINE-ANCHORS` 红，**与本轮改动无关**（§3.2） |
| SCI 合同 lint | `python3 tools/science_contract_lint.py <doc>` | `PHASE2_UPM.md` **PASS**（15 节）；`REJECTION.md` **PASS**（15 节）；`PHOTOMETRY.md` **PASS**（15 节）；`CONTROL_WEIGHT_SNR.md`/`PSF_SIGNAL_WEIGHT.md` 报缺章节——**HEAD 版本同样报**（§3.3），属既有结构欠账 |

### 3.2 `DOC-LINE-ANCHORS` 红灯归因（**既有**，非本轮引入）

```text
DOC_LINE_ANCHORS_FAIL:
  [C4_symbol_binding] P3RSMP-DESCRIPTOR: BINDING_VIOLATION symbol now at lines [695, 8945]
  [C4_symbol_binding] NOISE-CMAKE: BINDING_VIOLATION symbol now at lines [620, 624, 626, 632, 706, 817]
  [C4_symbol_binding] CAL-CMAKE: BINDING_VIOLATION symbol now at lines [446, 456, 461, 464, 465, 705]
  [C4_symbol_binding] COS-CMAKE: BINDING_VIOLATION symbol now at lines [446, 456, 461, 464, 465, 705]
  [C4_symbol_binding] DRZ-MAXANGLE: BINDING_VIOLATION symbol now at lines [1066, 1091, 1245, 1273, 1327, 1329]
```

- 涉及锚（`docs/algorithms/anchors/anchor_contract.json` bindings）指向的**代码目标**：
  `lib/infrastructure/scheduler/src/module_adapters.cpp`、`CMakeLists.txt`、`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`；
- 本轮**未改**上述任何代码文件（`git status` 中它们是**其它分片**的修改），也未改对应 5 篇 ALG 文档；
- 结论：该红灯由**其它分片的代码行漂移**造成，须由前台在统一构建/回归阶段随代码提交重锚；**本 claim 不认领**。

### 3.3 两条既有哈希欠账（随本 claim 一并刷新，非本 claim 内容改动）

`verify_doc_pack.py` 在修复前对 2 篇报 `R1_hash_mismatch`，经 `git log` 核实均为**已提交**的前台编辑、只是 DOC-101 清单未回填：

- `ENGINEERING_SPEC.md`（commit `ddf692ed`：§7 根条目补 `ACCEPTANCE_SPEC.md`、`reverse_verify/` 逆向验收工作区登记）→ 已登记 `authorized_edits` 并回填当前哈希；
- `docs/ci/01_CHECKS.md`（commit `7ae4b449`：§2 补登记 `CHK-E2E-REPRO`/`CHK-EXIT-CONSISTENCY`，负责人已认可保留，见控制包 `00_README.md` §3 第 5 项）→ 同上。

## 4 影响面登记（**本轮不实现代码**）

### 4.1 配置 / schema

- `config/templates/normalize.phase_config.json`：`sparse_snr_layer` false → **true**；补 `sparse_snr_density`（数值待定，见 §5）；
- `config/templates/mosaic.phase_config.json`：新增 `"algorithm_snr_path": "sparse_reconstruct"`；
- `config/defaults.json`：新增 `snr.path`（默认 `sparse_reconstruct`）与 `sparse_snr_layer` 默认登记；**须守 `authority.transcription_rule`**（值只能转录自 `docs/science|docs/algorithms`）⇒ 先由 SCI 侧承接；
- `contracts/schemas/phase_config_{normalize,mosaic}.schema.json`：增字段 + enum；
- `ASTROCS_DESIGN.md` §3.3 示例 JSON `"sparse_snr_layer": false` 与默认稀疏裁决矛盾，**属根级最高设计、超出本分片文件域**，须前台/负责人授权订正。

### 4.2 代码

- Phase1 SNR 三路径实现（dense 面 / 稀疏层 / 帧级）与 `algorithm_snr_path` 消费；无稀疏层时 `snr_path_effective` 显式记录；
- Phase2 确认「不输出 SNR 面」，SNR 消费点为叠加现场 `w=1/σ_F²`；
- stage2 字段改名 `local_snr`/`frame_snr_medians` ⇒ `quality_weight`（`snr_v` 别名），与 `frame_snr` 科学量分离；
- `lib/algorithms/coverage/include/astro/phase2/stage2_common.h:69` 工具默认 `wbpp_2_9_1` 与生产默认 `astrocs_adaptive_pixel` 的分歧（`CHK-CONFIG-DEFAULTS` divergent 面，已有登记）；
- `stage2_common.cpp:258-265` 错误串拼接缺分隔，随改名一并修；
- **标定因子验收面（D-5）**：删除/否决 `k_photo`/`a_k`/`scale` 的**绝对窗口守卫**（如 `k_photo ∈ [0.1,10]`；`ESC-P1-PHOT-ABS-WINDOW` 据此裁决），改用尺度无关判据。

### 4.3 合同 / 产品

- `contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67` `source` 串 `wbpp_2_9_1` → `astrocs_adaptive_pixel`；
- `docs/contracts/DATA_SEMANTICS.md` 目标合同**只冻结 variance/ivar**（D-3 明确不批 SNR 面），本轮不改该冻结面；
- `OPEN-P2S-02`：模型语义部分随本 claim 关闭，数据面/schema 仍待合同流程。

### 4.4 测试

- SNR 三路径精度对比（SP-0）：三路径正例 + 静默降级负例 + 损坏层 fail-closed 负例；
- 帧级 SNR 注入-回收（`F_s`/`B`/噪声已知 ⇒ 回收 = `F_s/σ_F`）与**天光单调性负例**（`B` 增大 ⇒ SNR 单调下降，`B→∞` ⇒ 0）；
- 点源/面亮度 SNR 口径分离负例；
- `astrocs_adaptive_pixel` `n≤3 → none` 保守档与 provenance `underdetermined_no_rejection` 正/负例；冻结 `wbpp_2_9_1` 路由回归不变。

## 5 未闭合项（显式登记，不猜测）

1. **帧级 SNR 具体定义式**：待分片 **FRAME-SNR-CANON** 文献结论补入；本 claim 只固化红线与方向（纯信号/噪声、已扣局部背景、天光只进 `σ_F`、单调性、点源口径、不得与面亮度 SNR 混用）；
2. **A6 `F_instr` 测光口径**：负责人要求「查论文，科学软件算法等」定案（A4 前置）；本轮涉及星点通量口径处**只登记待 A6 定案**；
3. **`sparse_snr_density` 数值**：仍 `pending_authority`；默认稀疏路径与「默认产出稀疏层」之间存在**联锁缺口**，数值定案前不得编造；
4. **`ASTROCS_DESIGN.md` §3.3 示例 / `config/templates` / `config/defaults.json` / schema / 合同 JSON**：见 §4，超出本分片文件域，须前台执行。

## 6 纪律与环境

- **零 git 写**：未 commit / push / amend（`git status` 仅显示工作区修改，提交由前台串行执行）；
- **未跑 `ninja` / `cmake` / `ctest`**（硬约束）；
- 未改 `lib/`、`tests/`、`ci/`（除一致性回归所需的读取与检查执行）；
- `TMPDIR=/dev/shm/astrocs_canon`（已建；检查日志落 `run/RELEASE-02/canon-freeze/logs/`）；
- 负例自查：文档符号检查在本轮曾因新增未登记符号 `p2_op_reject` 判红（`CON-DOC-SYMBOLS`），
  改为「生产编排入口」表述后转绿 —— 证明该门对本轮改动**能红能绿**（证据见 §3.1 与 `run/RELEASE-02/canon-freeze/logs/fast_checks.log`）。
