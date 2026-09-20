# RELEASE-02 分片报告：SNR 配置与 schema 落地（SNR-CONFIG-LAND）

- 分片：SNR-CONFIG-LAND（SNR 配置与 schema 落地）
- 工作目录：`/workspace/Astro CS Database`
- 日期：2026-09-19
- 基线 HEAD：`cbda64a1`（CANON-FREEZE 文档订正**已提交**；本分片**零 git 写**）
- 变更 claim：`工程控制/RELEASE-02/change-claims/FIX-SCI-SNR-CANON-001.md`（§3.2 D-2、§4.1 配置/schema 跟随项）
- 裁决来源：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.45（:1299-1324，Δ=64 px）、§9.46（:1326-1370，Phase2 默认消费稀疏层）
- 依据条款：`ENGINEERING_SPEC.md` §3；`AGENTS.md` §4/§8；`config/config_registry.json#rules`（no_second_source / no_auto_sync）
- 状态：**配置 / schema / 登记册已落地**；Phase1 稀疏层生产者、Phase2 消费点、CLI 白名单等**实现跟随项仍登记未实现**（见 §5.3）

## 0 摘要

| 项 | 结论 |
|---|---|
| 键名是否统一 | **是**：mosaic 面统一为 **`snr_path`**（不是 claim §3.2/§4.1 表中的 `algorithm_snr_path`）；normalize 面沿用 `sparse_snr_layer`；defaults 面用命名空间键 `snr.path`（经 `enum_token/enum_target` 唯一映射到 phase_config 字段）。依据见 §3 |
| 密度键 | **新增 `sparse_snr_spacing_px`（normalize）/ `sparse_snr.spacing_px`（defaults）= 64 px**，承载 §9.45 的 Δ（控制点间隔）；**不复用** `sparse_snr_density`（文档/登记册单位为「点/度²」且 pending）——同名同键承载两种单位会违反「一个字段只承载一个含义」，依据见 §4.2 |
| 默认路径 | `snr_path = sparse_reconstruct`（Phase2 **默认消费**稀疏层，§9.46；SP-0 降为诊断量、不作开关） |
| 门禁 | `CHK-CONFIG-DEFAULTS` / `CHK-CONFIG-CONSUMED` / `CHK-REGISTRY-DOC-SYNC` / `CHK-SCHEMA` / `CHK-DANGLING` **全 PASS**；schema 正例 2/2 PASS、负例注入 5/5 判红；dead-key 门极性 A(去登记)→恰好 2 红 / B(登记)→PASS（§6） |
| 剩余红灯 | 全部为**既有**（HEAD 已含的其它分片文档重排）：`UT-CONFIG` 3 条 + `CFG002` 5 项；其中 **SNR 面仅剩 3 处文档书写形态缺陷**（`**true**` ×2、字段列含括注 ×1），逐字订正建议见 §5.2 |

## 1 裁决 → 落点

| 裁决 | 语义 | 落点（本分片） |
|---|---|---|
| D-2 / A3（claim §3.2） | SNR **三条路径** `dense` / `sparse_reconstruct` / `frame_reconstruct`，**默认稀疏**，JSON 可显式指定 | `contracts/schemas/phase_config_mosaic.schema.json:150-158`（enum + default）；`config/templates/mosaic.phase_config.json:9`；`config/defaults.json:573-585`（`snr.path` + enum_target/enum_token） |
| §9.45（:1299-1324） | 稀疏层默认密度 **Δ = 64 px**（控制点间隔），复用 Phase2 UPM 的 8×8/tile 控制网格 | `contracts/schemas/phase_config_normalize.schema.json:127-132`（integer ≥1，default 64）；`config/templates/normalize.phase_config.json:7`；`config/defaults.json:562-572`（`sparse_snr.spacing_px`） |
| §9.46（:1326-1370） | Phase2 **默认消费**稀疏层（`snr_path = sparse_reconstruct`），**不再按 SP-0 判据决定**；三条路径保留；SP-0 = 诊断量 | 同 D-2 三处 + schema 描述内写明 SP-0 定位与「不静默降级 / 损坏层 fail-closed」 |
| D-3 / B1（claim §3.3） | 只有 Phase1 HiPS 带 SNR 数据块；Phase2 不输出 SNR 面（叠加中消费、不复用）；Phase3 无 | `sparse_snr_layer`（normalize，默认产出）+ `snr_path`（mosaic，消费面键）；本分片不新增任何 Phase2/Phase3 的 SNR 输出键 |
| 默认稀疏（claim §4.1） | `sparse_snr_layer` false → **true** | `config/templates/normalize.phase_config.json:6`；`phase_config_normalize.schema.json:122-126`（default true）；`config_registry.json` 两行 `declared_default` false→true（`:665`、`:770`） |

## 2 改动清单（文件:行 + 依据）

| 文件 | 行 | 改动 | 依据 |
|---|---|---|---|
| `config/templates/normalize.phase_config.json` | :6-7 | `sparse_snr_layer` false→**true**；新增 `sparse_snr_spacing_px: 64` | claim §4.1；§9.45；§9.46 |
| `config/templates/mosaic.phase_config.json` | :9 | 新增 `snr_path: "sparse_reconstruct"` | claim §4.1；§9.46；ASTROCS_DESIGN §3.3:124 |
| `contracts/schemas/phase_config_normalize.schema.json` | :122-132 | `sparse_snr_layer` 补 `default: true` 并重写描述（含「本键只决定是否产出稀疏层」的单一含义声明）；新增 `sparse_snr_spacing_px`（`type: integer`、`minimum: 1`、`default: 64`、单位 px、Δ=hips.tile_width/8、ℓ=48px⇒Δ/ℓ=1.33） | §9.45；claim §4.1 |
| `contracts/schemas/phase_config_mosaic.schema.json` | :150-158 | 新增 `snr_path`（enum 三路径、`default: "sparse_reconstruct"`、描述含三路径定位/SP-0 诊断量/不静默降级/损坏层 fail-closed/键名依据） | D-2；§9.46；claim §4.1 |
| `config/defaults.json` | :11 | `field_count` 51→**53** | 机器门 `TestDefaultsContract::test_counts_and_no_unknown_source` |
| `config/defaults.json` | :560 | `sparse_snr.density` 的 `note` 补「与 `sparse_snr.spacing_px` 不是同一字段」交叉引用（**value/unit/source/pending 状态一律未改**） | 「一个字段只承载一个含义」；§9.45 |
| `config/defaults.json` | :562-572 | 新增 `sparse_snr.spacing_px = 64`（unit `px`，`owner_adjudicated`，source=§9.45） | §9.45；先例 `calibration.dark_light_exposure_tolerance`（负责人裁决值不标 pending） |
| `config/defaults.json` | :573-585 | 新增 `snr.path = "sparse_reconstruct"`（unit `1`，`owner_adjudicated`，source_ref=`07_noise_snr.md:114`，`enum_token`/`enum_target`→mosaic `snr_path`） | §9.46；claim §4.1；CFG002-03（默认值→值域唯一登记） |
| `config/config_registry.json` | :596、:611、:626、:641、:654 | `07_noise_snr` 行号 60/61/62/63/63(evidence) → 108/109/110/111/111（文档重排回填） | CFG002-01（行号漂移判红） |
| `config/config_registry.json` | :663、:665、:672 | `sparse_snr_layer`（07_noise_snr）行号 64→112、`declared_default` false→**true**、note 更新 | claim §3.2；§9.46 |
| `config/config_registry.json` | :678、:687-702 | `sparse_snr_density` 行号 65→113 + note 交叉引用；**新增 `snr_path` 行**（field=`snr_path`，line=114，default=`sparse_reconstruct`，registration=phase_config，registered_key=`mosaic.config.snr_path`） | claim §4.1；CFG002-01/02 |
| `config/config_registry.json` | :768、:770、:777 | `08_drizzle.sparse_snr_layer` 行号 45→46、`declared_default` false→**true**、note 更新 | claim §2.3（08_drizzle:46）；§9.46 |
| `config/config_registry.json` | :1672、:1674、:1679、:1688 | `totals` 重算：rows 95→**96**、none 45→**46**、science_param 66→**67**、phase_config 9→**10** | CFG002-01（totals 必须与重算一致） |
| `ci/ledgers/dead_config_keys.json` | :30-55 | 新增 2 条死键登记：`dead_config_key:snr_path`、`dead_config_key:sparse_snr_spacing_px`（reason/owner/exit_condition/evidence 齐备） | `CHK-CONFIG-CONSUMED` 的唯一豁免途径；claim §4.2 实现跟随项 |
| `reports/RELEASE-02/snr-config-land.md` | 全篇 | 本报告 | — |

**未改**：`lib/**`、`tests/**`、`docs/**`（`docs/**` 仅提订正建议，见 §5.2）。`ci/ledgers/dead_config_keys.json` 属该门自身的豁免台账（`ci/**` 不在任务声明的 `config/**`+`contracts/**` 白名单内）——**若前台认为越域，请复核该 2 条登记**（去掉即该门对 2 个新键判红，见 §6 极性证据）。

## 3 键名一致性（任务 §5 / §7）

三处候选键名与裁决/权威的核对：

| 面 | claim 写的键名 | 现状权威写法 | 本分片落地 | 依据 |
|---|---|---|---|---|
| Phase1（normalize） | `sparse_snr_layer` | `sparse_snr_layer`（07_noise_snr.md:112、08_drizzle.md:46、schema、登记册、`ASTROCS_DESIGN.md` §3.4:174） | **`sparse_snr_layer`（不变）** | 四处同名，无分歧 |
| Phase2（mosaic） | `algorithm_snr_path`（claim §3.2:89、§4.1:165） | **`snr_path`**：`ASTROCS_DESIGN.md` §3.3:124（**最高设计，前台已改**）、`docs/design/UNIFIED_MODEL.md:46`、`docs/plugins/algorithms_phase1/07_noise_snr.md:114`、`GAP_AUDIT` §9.46:1362/1369 | **`snr_path`（统一）** | 见下 |
| defaults | `snr.path` | `snr.path`（claim §4.1:166） | **`snr.path`** | defaults 命名空间先例：`weight.default_mode` ↔ mosaic `algorithm_weight_mode`（`enum_token/enum_target` 唯一映射，CFG002-03 机器门）；**defaults 键名不是 phase_config 键名**，两者不构成「同名异义」 |

**统一到 `snr_path` 的依据（不采用 `algorithm_snr_path`）**：

1. **最高权威与三篇文档一致指向 `snr_path`**：`ASTROCS_DESIGN.md` §3.3:124（根级最高设计，前台已按裁决订正）写 `"snr_path": "sparse_reconstruct"`；`UNIFIED_MODEL.md:46`（数据对象表）写 `snr_path（配置）`；`07_noise_snr.md:114`（CANON-FREEZE 订正行）写 `snr_path`。`ASTROCS_DESIGN.md` 属**根级最高设计、不在本分片文件域** ⇒ 若改用 `algorithm_snr_path`，本分片**无法**把系统改回一致（只能留下矛盾）。
2. **负责人裁决用语即 `snr_path`**：`GAP_AUDIT` §9.46 定案 2/待通知分片两处都写 `snr_path = sparse_reconstruct`。
3. `algorithm_*` 族的适用面是**「算法实现选择」**（`algorithm_weight_mode`/`algorithm_rejection_method`/`algorithm_upm_gauge`/`algorithm_psf_model`/`algorithm_drizzle_pixfrac`，见 `docs/contracts/config_separation_anchors.json` `^algorithm_[a-z0-9_]+$` 与 schema `x-astrocs-notes`）；而 `snr_path` 选择的是**Phase1 产物形态 → 稠密 SNR 的重建路径**，与 `sparse_snr_layer` 同属「SNR 产物/数据面」命名对（根设计 §3.3 把两者并列在同一 `config` 块内）。**命名族是约定、不是强制**（该 regex 只用于「算法选择类」字段的登记说明，机器门只断言跨类不相交与硬件名禁令，未断言全量匹配）。
4. **结论**：claim §3.2/§4.1 的 `algorithm_snr_path` 是**未落地的命名提议**，按任务第 7 条「与 `ASTROCS_DESIGN.md` §3.3 核对、不一致以裁决语义为准」⇒ 语义（三路径/默认稀疏/JSON 显式指定）逐字保留，**键名统一为 `snr_path`**。建议 claim 侧补一行命名订正登记（`algorithm_snr_path` → `snr_path`，依据 §3.3:124 与 §9.46）。

## 4 「一个字段只承载一个含义」核对（任务 §6）

| 键 | 唯一含义 | 不承载 |
|---|---|---|
| `sparse_snr_layer`（normalize） | **是否产出**稀疏帧内 SNR 层（Phase1 标准层入 HiPS） | 不决定 Phase2 走哪条重建路径（那是 `snr_path`）——schema 描述已显式声明 |
| `snr_path`（mosaic） | Phase2 **重建/消费路径**（三选一） | 不决定 Phase1 是否产出稀疏层；不承载权重（`frame_snr` 才是科学量，权重在叠加现场换算 `w=1/σ_F²`） |
| `sparse_snr_spacing_px`（normalize） | 稀疏层**控制点间隔 Δ**（px） | 不承载「点/度²」面积密度（见 §4.2） |
| `sparse_snr.density`（defaults，**未改**） | 「点/度²」面积密度（pending_authority） | 不承载 §9.45 的 px 间隔（由新键承载，note 已交叉引用） |
| `frame_snr`（Phase1 科学量，本分片未涉） | 点源 PSF 信号 SNR（`F_signal/σ_F`，已扣局部背景） | 不是权重；不得与 stage2 `quality_weight`（相对质量场）同名互指（claim §3.4 登记为实现跟随项） |

**默认值不双写**：`sparse_snr_layer` 的默认值只登记在 phase_config 侧（schema `default` + 模板 + 登记册 `declared_default`），**不复制进 `defaults.json`**——依据 `config_registry.rules.no_second_source`（「phase_config 行以 `contracts/schemas/phase_config_*.schema.json` 为唯一事实源」）。claim §4.1 表「`sparse_snr_layer` 默认值登记」按此口径实现为**登记（schema+登记册）**，而非新增 defaults 字段。

## 5 与裁决矛盾的既有配置 / 既有缺陷

### 5.1 已修（与裁决直接矛盾）

| 文件:行（修前） | 矛盾 | 修后 |
|---|---|---|
| `config/templates/normalize.phase_config.json:6` | `"sparse_snr_layer": false` ↔ D-2「默认稀疏」 | `true` |
| `contracts/schemas/phase_config_normalize.schema.json:124`（修前） | 描述「默认 false 见 §3.3 示例」 ↔ §3.3 已改为 `true`、D-2 | `default: true` + 描述重写 |
| `config/config_registry.json:665`（07_noise_snr）、`:755`（08_drizzle） | `declared_default: "false"` ↔ 文档 `**true**`、D-2 | `true` |
| `ASTROCS_DESIGN.md` §3.3:122/124 | （前台已改）`sparse_snr_layer: true` + `snr_path` | **核对一致**（本分片键名与之统一，见 §3） |

### 5.2 未改（文件域外 / claim 已覆盖）——逐字订正建议

> `docs/**` 与 `tests/**` 不在本分片文件域；以下为**建议**，请前台按 claim 流程落地（①②③ 是 SNR 面仅剩的 3 处 CFG002-01 红灯，均为**书写形态**而非语义）。

| # | 文件:行 | 现状 | 建议 | 影响 |
|---|---|---|---|---|
| ① | `docs/plugins/algorithms_phase1/07_noise_snr.md:112` | 默认列 `**true**`（Markdown 加粗） | 改裸 `true`（与其余 94 行一致） | CFG002-01 报该行 `declared_default` 漂移（登记册已按语义值 `true` 登记；加粗会使任何数值比对失配） |
| ② | `docs/plugins/algorithms_phase1/08_drizzle.md:46` | 同上 | 同上 | 同上 |
| ③ | `docs/plugins/algorithms_phase1/07_noise_snr.md:114` | 字段列 `snr_path（Phase2 消费面键，在 mosaic 配置）` | 字段列只留 `snr_path`，括注移入说明列 | CFG002-01 报「缺登记 1 行 + 多登记 1 行」（同一根因；登记册按字段名 `snr_path` 登记，不把括注写入数据字段） |
| ④ | `docs/plugins/algorithms_phase1/07_noise_snr.md:113`、`:130` | `sparse_snr_density` 单位「点/度²」、「仍为 pending_authority，禁止编造数值」、§7「未定案 ⇒ 稀疏层不可生产（联锁缺口）」 | 注明 §9.45 已定案：稀疏层控制点间隔 Δ = 64 px（像素域），由 `sparse_snr_spacing_px` 承载；「点/度²」表述与 R-001 收口一并订正 | 消除「联锁缺口」的**配置侧**部分（剩余联锁 = 实现未落地，见 §5.3）；否则文档仍宣称「不可生产」 |
| ⑤ | `docs/plugins/algorithms_phase1/07_noise_snr.md` §5 配置表 | 无 `sparse_snr_spacing_px` 行 | 补一行（默认 64、单位 px、说明 Δ 与 UPM 8×8/tile 网格同几何） | 该 phase_config 键现无插件文档行 ⇒ CFG002-01 无法登记（现由 defaults+schema+本报告承载） |
| ⑥ | `docs/contracts/CONFIG_CONTRACT.md` §2（:30「50 字段」快照、:53/:55-61 pending 表）、§3（:74-75 可选算法选择列） | 未含 `snr.path`/`sparse_snr.spacing_px`/`snr_path`；pending 表仍只列 3 项 | §2 快照更新为 `field_count=53` 并补 2 键（owner_adjudicated，非 pending）；§3 normalize 列补 `sparse_snr_spacing_px`、mosaic 列补 `snr_path` | 文档↔登记册一致性（CFG002-11 引用锚同批处理） |
| ⑦ | `docs/plugins/algorithms_phase2/13_integration.md` | 只写 `snr_path_effective`，未写键名 | 与 07_noise_snr:114 对齐，写明键名 `snr_path`（mosaic 配置） | 键名唯一性 |
| ⑧ | claim `FIX-SCI-SNR-CANON-001` §3.2:89、§4.1:165 | `algorithm_snr_path` | 补命名订正：`snr_path`（依据 §3.3:124、§9.46） | 避免后续分片按旧名实现 |
| ⑨ | `config/config_registry.json` 其它模块行 | 12 行缺登记（`10_sampling`×3、`11_upm`×4、`19_runtime`×2、`22_gaia_xpsd_client`×3）+ 1 行多登记（`11_upm.bkg_model_order`）+ 25 行漂移 | 由**其它分片**（文档重排方）回填；**非 SNR 面**，本分片不认领 | CFG002-01 当前 13 缺登记中 12 条属此类 |

### 5.3 实现跟随项（`lib/**`，本分片不改；已登记）

1. **CLI 白名单缺 2 键（配置不可达）**：`lib/infrastructure/cli/parser.cpp:325` 的 `kSessionKeys` 含 `sparse_snr_layer`/`algorithm_psf_model` 等，但**不含 `snr_path` 与 `sparse_snr_spacing_px`** ⇒ 按现实现这两个键会被判 `unknown key` 并退出 3（与 RELEASE-02 **SD-15** 同类：合同 schema 已声明但 CLI 不可达）。**这是「JSON 可显式指定」落地的最后一米**，建议随 Phase1/Phase2 实现分片一并补。
2. **Phase1 稀疏层生产者**：按 `sparse_snr_spacing_px` 生成控制点 SNR 层（复用 Phase2 UPM 的 8×8/tile 控制网格）；`dense` 路径所需的 Phase1 稠密 SNR 面亦未实现。
3. **Phase2 消费点**：按 `snr_path` 重建（线性插值足够，§9.46）；无稀疏层而路径为 `sparse_reconstruct` ⇒ 必须记 `snr_path_effective` 并计数（不得静默）；损坏层 fail-closed；SNR 消费为叠加现场 `w=1/σ_F²`。
4. **stage2 字段改名**：`local_snr`/`frame_snr_medians` → `quality_weight`（`snr_v` 别名），与 `frame_snr` 科学量分离（claim §3.4/§4.2）。

## 6 门禁复跑（命令 + 结果 + 日志）

日志目录：`run/RELEASE-02/snr-config-land/logs/`；`TMPDIR=/dev/shm/astrocs_snrland`。

| 门 | 命令 | 基线（改动前实跑） | 复跑（改动后） | 日志 |
|---|---|---|---|---|
| CHK-CONFIG-DEFAULTS | `python3 ci/run_checks.py --check CHK-CONFIG-DEFAULTS --quiet` | PASS | **PASS**（`entries=1 steps=1 pass=1`；`config_key_count=62 scanned=24 divergent=6`，divergent 全为既有键，**无 SNR 键**） | `baseline_config_defaults.log` / `after_config_defaults.log` |
| CHK-CONFIG-CONSUMED | `python3 ci/run_checks.py --check CHK-CONFIG-CONSUMED --quiet` | PASS | **PASS**（`template_key_count=19 dead_keys=4`，4 = 既有 export `wcs.center_deg`/`wcs.s_out_deg` + 新 2 键，**均已登记**） | `baseline_config_consumed.log` / `after_config_consumed.log` |
| CHK-REGISTRY-DOC-SYNC | `python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet` | PASS | **PASS**（`steps=2 pass=2`，含 self-test） | `baseline_registry_doc_sync.log` / `after_registry_doc_sync.log` |
| CHK-SCHEMA | `python3 ci/run_checks.py --check CHK-SCHEMA --quiet` | 未跑 | **PASS**（`steps=2 pass=2`） | `after_schema.log` |
| CHK-DANGLING | `python3 ci/run_checks.py --check CHK-DANGLING --quiet` | 未跑 | **PASS**（`steps=2 pass=2`，含 `check_doc_symbols`） | `after_dangling.log` |
| schema 正例 | `python3 tests/config/run_validation.py <schema> <template>` | — | **2/2 VALIDATION_PASS**（normalize、mosaic） | 控制台输出（§6.1） |
| schema 负例注入 | `tests/config/cfg_common.validate`（/dev/shm 临时实例） | — | **5/5 判红**（见 §6.1） | 控制台输出 |
| UT-CONFIG（CFG-001/002 unittest） | `python3 -B -m unittest discover -s tests/config -t tests/config` | 未跑 | **FAILED (failures=3)**：`test_every_source_ref_resolves_and_key_anchors_hold`（KEY_ANCHORS 3 条：`REJECTION.md:56`/`PHOTOMETRY.md:24`/`DRIZZLE.md:31`）+ 2 条 CFG002 包装 | `after_ut_config.log` |
| CFG002 直接跑 | `python3 tests/config/check_cfg002_registry.py` | **FAIL 5/11** | **FAIL 5/11**（`CFG002-01/04/07/09/11`；`CFG002-02` resolved 95→**96** value_unchecked 8→9、`CFG002-03` mapped 1→**2**） | `baseline_cfg002.log` / `after_cfg002.log` |
| 门自检（能红能绿） | `ci/check_config_consumed.py --self-test`、`ci/check_config_defaults.py --self-test`、`ci/check_registry_doc_sync.py --self-test` | — | **全 PASS**（含 red 用例） | 控制台输出（§6.2） |

### 6.1 schema 正/负例（本分片新增键的机器约束确实生效）

```text
VALIDATION_PASS contracts/schemas/phase_config_normalize.schema.json <- config/templates/normalize.phase_config.json
VALIDATION_PASS contracts/schemas/phase_config_mosaic.schema.json    <- config/templates/mosaic.phase_config.json
RED  normalize + snr_path="bogus_path"            -> additionalProperties: 'snr_path' unexpected   （键只在 mosaic 面）
RED  mosaic    + snr_path="bogus_path"            -> enum: 'bogus_path' not in ['dense','sparse_reconstruct','frame_reconstruct']
RED  normalize + sparse_snr_spacing_px=0          -> minimum
RED  normalize + sparse_snr_spacing_px="64"       -> type: expected 'integer'
RED  normalize + sparse_snr_layer="yes"           -> type: expected 'boolean'
POLARITY PASS
```

### 6.2 dead-key 门极性（`CHK-CONFIG-CONSUMED`）

沙箱（`/dev/shm/astrocs_snrland/deadkey`，`--repo` 指向沙箱，仓库零改动）：A = 台账去掉本分片 2 条 → **恰好 2 红**（`dead_config_key:snr_path`、`dead_config_key:sparse_snr_spacing_px`）；B = 真实台账 → **PASS**（`template_keys=19 dead_keys=2（均已登记）`）。⇒ 证明新键的绿灯来自**显式登记**（含 owner/exit_condition），不是门失效。

### 6.3 剩余红灯归因（**均为既有，非本分片引入**）

| 红灯 | 归因（证据） |
|---|---|
| UT-CONFIG `KEY_ANCHORS` 3 条 | `docs/science/{REJECTION,PHOTOMETRY,DRIZZLE}.md` 的**已提交**内容与 `tests/config/test_cfg001_contracts.py:35-48` 硬编码锚不符；3 文件工作区**无改动**（`git status` 空），且 `tests/**`/`docs/**` 非本分片文件域。当前实测行：`REJECTION.md:56`=`6 ≤ n ≤ 15 → winsorized_sigma …`、`PHOTOMETRY.md:24`=`sigma_mag` 行、`DRIZZLE.md:31`=`## 4 输入有效域` |
| CFG002-01 | 13 行缺登记中 **12 行**属 `10_sampling/11_upm/19_runtime/22_gaia_xpsd_client`（其它分片文档重排，需人工定 owner_class/registration）；**1 行**为 ③ 的字段列括注；另有既有 1 行多登记（`11_upm.bkg_model_order`，文档已改名 `bkg_model`） |
| CFG002-04 | `non_key_examples` 的 `where` 锚 `ASTROCS_DESIGN.md:130/132` 已因 §3.3 重排失效（`ASTROCS_DESIGN.md` 由前台按裁决订正，非本分片） |
| CFG002-07 | `tests/validation` 未登记（其它分片新增目录；`tests/**` 非本分片域） |
| CFG002-09 / CFG002-11 | `docs/science/{REJECTION,PHOTOMETRY,DRIZZLE}.md` 与 `docs/plugins/algorithms_phase2/11_upm.md` 的行锚漂移（CANON-FREEZE/其它分片已提交文档重排）；其中 `07_noise_snr.md:65 指向空行` 属 CONFIG_CONTRACT §2 旧行锚（建议随 ⑥ 一并重锚到 :113） |
| SNR 面自有红灯 | **仅 3 处，全部为文档书写形态**（①②③，§5.2），已给逐字订正建议；**无语义矛盾** |

## 7 门禁盲区与诚实边界

1. **`CHK-CONFIG-DEFAULTS` 不比较 `defaults.json` 的 `value`**：该门只把 `lib/**` 默认字面量与**模板值**比对（`config_key_set` 只从 defaults 取**键名**、不取值）。⇒ `defaults.json#snr.path` / `#sparse_snr.spacing_px` 与模板/schema 的**同值性无机器门覆盖**。本分片以「三处同值 + 交叉引用（schema 描述 / defaults note）」固定，并在此显式登记该盲区（建议后续加门或纳入 CFG002-03 类判据）。
2. **CFG002-01 的 `field` 逐字比对**使「字段列写括注」必然判红。本分片选择**登记语义字段名**（`snr_path`）+ 报文档缺陷（③），而**不**把 Markdown 标记/括注写进登记册数据字段（同理 `declared_default` 记 `true` 而非 `**true**`）——若前台希望「先绿后改文档」，把 ③ 与 ①② 改为逐字转录即可（3 处值改动），但会把书写标记固化进登记面，故本分片不采用。
3. **未跑 `ninja`/`cmake`/`ctest`**（硬约束）；**未跑 `CHK-UNIT`**（其步骤含 `CTEST-LINUX-FULL`/`V6-CTEST-UNIT`），只直接跑了其 `UT-CONFIG` 步骤的等价命令。
4. **不在本分片范围**：三条路径的精度对比（SP-0，SNR-EXP-AUDIT / 论文）、帧级 SNR 具体定义式（FRAME-SNR-CANON）、`k_photo` 尺度无关验收（D-5）。
5. **未使用任何物理闭合式**；Δ=64 px 与 ℓ=48 px、`hips.tile_width=512` 的关系均为**几何/像素域**陈述，不涉物理单位（D-5 口径）。

## 8 纪律与环境

- **零 git 写**：未 commit/push/amend（`git status` 仅显示工作区修改，提交由前台串行执行）；只用只读 `git status/log/diff` 做归因。
- **未跑 `ninja`/`cmake`/`ctest`**；未改 `lib/**`、`tests/**`、`docs/**`。
- 文件域：`config/**`、`contracts/**`、`reports/**`，外加 `ci/ledgers/dead_config_keys.json`（该门自身豁免台账，已在 §2 显式声明请前台复核）。
- `TMPDIR=/dev/shm/astrocs_snrland`（schema 负例与 dead-key 极性沙箱），**已清理**；门禁日志保留在 `run/RELEASE-02/snr-config-land/logs/`。
- 负例自查：schema 5/5 判红、dead-key 门 A/B 极性、三份门自检 —— 证明本分片改动**能红能绿**。
