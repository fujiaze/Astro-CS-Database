# 变更 claim：DOC-201-WEIGHT-MODE-VOID-001 — 详细层「权重模式」整套作废（术语级订正）

- 控制包：RELEASE-03 / 任务 **DOC-201**（`工程控制/RELEASE-03/tasks/DOC-201.md`）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §2.1（总纲：**全程只有 SNR，不存在「权重模式」这个概念**；
  §2.1 第 4 条禁止任何「权重模式 / 权重档位 / 默认权重模式 / mode0·mode1·mode2」的**键名、枚举、
  配置项或产物**；第 5 条变量名直接叫 `weight`）、§2.2（数据对象唯一性）、§2.3（质量代理不进科学权重）
- 依据（裁决正本）：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73【裁决 A44】（负责人逐字，见 §1）；
  `run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1（W01–W22 + B/C/D 组逐处清单）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（科学正确性优先 + **变更 claim** + 一致性回归）、
  §6/§7/§8；`AGENTS.md` §1.1/§8；`docs/ci/CI_SPEC.md` §4/§7
- 状态：**已落地**（文档侧）；**代码侧**归 FIX-202（HiPS 格式/ABI）、FIX-203/FIX-207（配置键）、
  FIX-206（`snr_weight_mode` → `upm_weight_source` 改名，见 §6）
- 影响类：**术语级**（**不改**任何公式、常数、默认容差、SCI/ALG 冻结定义、数值结论）

## 1 裁决原文（负责人逐字，不得改写）

> 「2.**这是一个概念吗？而且哪里有权重**。在HiPS里面存的是**帧级SNR和稀疏的相对SNR比值**。
> **全程都是SNR才对**。**只有阶段二消费SNR的时候，根据这个位置上像素对应的集合计算权重**，
> 其他时候哪里有权重？至于叫什么那个是变量名，**直接叫weight不行吗**」

定案（同节）：① 全程的科学量是 **SNR**；HiPS 内**存**帧级 SNR + 稀疏的相对 SNR 比值；
② 权重**不是被存的东西**，是**阶段二消费 SNR 时按该天位像素对应帧集合现场算出的派生量**；
③ 三套键名（`weight_mode` / `algorithm_weight_mode` / `weight.default_mode`）**一并作废**；
④ 一切「权重模式 / 权重档位 / mode0·mode1·mode2」表述**须删改**；
⑤ 阶段一/阶段三**不产生也不消费权重**。

## 2 变更内容（三层分级处置）

| 层 | 处置 | 说明 |
|---|---|---|
| **历史/归档/冻结层**（`docs/archive/**`、`docs/science/v6/**`、`docs/algorithms/v6/**`、`docs/validation/v6/**`、`docs/contracts/v6/**`、`docs/audit/**`、`docs/research/**`） | **不重写历史内容**：文件头部加作废横幅 + **每一处** `weight_mode` 同行留痕「已按 §9.73 A44 作废：该概念不存在」 | 32 文件（JSON 件以等价的可加性注记键实现横幅/留痕；CSV 件只做同行留痕以免破坏 CSV 结构） |
| **活文档层**（`docs/contracts`（非 v6）、`docs/architecture`、`docs/modules`、`docs/design`、`docs/development`、`docs/owner`、`docs/plugins`、`docs/GLOSSARY.md`、`docs/ARCHITECTURE.md`、`docs/API_REFERENCE.md`、`docs/DOCUMENT_INDEX.yaml`、`README.md`） | **删键 / 改写 / 退役**：删 `weight_mode`、`legacy_allow_weight_fallback`、`ASTROCS_WEIGHT_MODE`、`weight_mode_version` 等键；`sci_weight_mode` 示例删除；`weight_mode_vocabulary` 角色标注作废；「权重模式 / 三种生产权重模式」改写为「权重口径 / 权重（阶段二派生量）」 | 见 §3 逐文件表 |
| **科学/算法层**（`docs/science/**`（非 v6）、`docs/algorithms/**`（非 v6）） | **只做术语订正**（本 claim 即其变更记录）：`weight_mode=2` → 「纯逆方差权重」；`weight_mode=0` → 「历史 legacy 分支」；`snr_weight_mode` → `upm_weight_source` | **公式/常数/容差/冻结定义零改动** |

### 2.1 统一替换口径

- 「权重模式 / 权重档位 / mode0·mode1·mode2 / 三模式」→「**权重（阶段二按该天球像素对应帧集合现场算出的派生量）**」或「权重口径」；
- `weight_mode` / `algorithm_weight_mode` / `legacy_allow_weight_fallback` / `weight_mode_version` /
  `sci_weight_mode` 等**键名** → **删键**，并在原处一行注明「已按 §9.73 A44 作废（键不存在；权重是派生量）」；
- `snr_weight_mode`（UPM 控制点权重来源）→ **改名** `upm_weight_source`（语义结论待 EXP-201；文档侧本次改名，代码侧归 FIX-206）；
- 变量名一律用 `weight`（不加 `_mode`）。

## 3 逐文件处置（活文档层，节选关键项）

| 文件 | 处置 | 依据 |
|---|---|---|
| `docs/contracts/DATA_SEMANTICS.md` | §20.1 删 `weight_mode`/`legacy_allow_weight_fallback` 两行键；§20.3/§21.6/§30.1/§30.3/§30.5 术语订正；§25 三处 `snr_weight_mode` → `upm_weight_source`；§31.3 整节标作废 | W01–W09 |
| `docs/contracts/PUBLIC_API.md` | §「Phase2 mosaic write 公共消费面」删两字段；§「V6 消费面」整节标作废；`snr_weight_mode` → `upm_weight_source` | W10–W11 |
| `docs/contracts/CONFIG_CONTRACT.md` | `algorithm_weight_mode` 指针行删键；`weight` 组改「默认权重口径」 | W12 |
| `docs/contracts/UNIFIED_OBJECTS.md` | 禁止名示例删 `sci_weight_mode` | W13 |
| `docs/contracts/config_separation_anchors.json` | 4 处 `sci_weight_mode` 示例/禁止名删除（机器 JSON，不加注释） | W14 |
| `docs/contracts/unified_object_registry.json` | `weight_mode_vocabulary` 角色与 note 标作废（条目保留：`tests/contracts` 要求 `contracts/schemas/v6/**` 全登记） | W20 边界 |
| `docs/contracts/API_CONTRACTS.csv` / `docs/architecture/api_inventory.csv` | `aio_ahpx_write` 签名与 FIX-202 已退役的 C ABI 对齐（删 `weight_mode/weight_data/grid_w/grid_h`）；同行留痕 | D 组 + FIX-202 代码事实 |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | 5 处术语订正（**行数不变**，保 CFG002 行号锚） | C 组 |
| `docs/plugins/algorithms_phase2/13_integration.md` | 4 处术语订正；配置表 `weight_mode` 行**字段名保留**（config/config_registry.json 锚点非本任务文件域）+ 说明列标作废 | C 组 + 文件域边界 |
| `docs/modules/registry/astrocs.phase2.write.md`、`docs/modules/phase2.md`、`hips_p2.md`、`phase2_rej.md`、`phase2_upm.md`、`registry/astrocs.phase2.{integrate,reject}.md` | 术语订正 + 键名删改 | C 组 |
| `README.md`、`docs/ARCHITECTURE.md`、`docs/API_REFERENCE.md`、`docs/GLOSSARY.md`、`docs/DOCUMENT_INDEX.yaml`、`docs/owner/PROJECT_SPEC.md`、`docs/design/PHASE2_DETAILED_DESIGN.md`、`docs/development/CONFIG_SCHEMA.md` | 术语订正（`CONFIG_SCHEMA.md` 保留 `weight_mode` 字面以满足 `check_config_contracts.py` 的文档 token 门，同行留痕） | C 组 |

## 4 证据

1. `工程控制/RELEASE-02/GAP_AUDIT.md` §9.73【裁决 A44】（负责人逐字 + 5 条定案）；
2. `ASTROCS_DESIGN.md` §2.1（最高权威已写死的总纲）；
3. `run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1（W01–W22 + B/C/D 组）与 `CTR-01.md` 🔴1；
4. 机器判据（可复跑，见 §5）：`grep -rn "weight_mode" docs/ README.md | grep -v "已按 §9.73 A44 作废"`
   在订正前为 **194 行 / 70 文件**，订正后为 **0**；
5. 正面证据：`lib/infrastructure/cli/parser.cpp` 已按 A44 注释「块面不收 `weight_mode`」
   ⇒ 合同/schema 层的该键当前必然 exit 3，详细层改口径是唯一出路（DESIGN-DRAFT §3.1-D）。

## 5 一致性回归（本次实测）

| 命令 | rc | 结果 |
|---|---|---|
| `grep -rn "weight_mode" docs/ README.md \| grep -v "已按 §9.73 A44 作废"` | 1 | **空**（0 行） |
| `grep -rn "权重模式" docs/ README.md \| grep -v "不存在" \| grep -v "已作废"` | 1 | **空**（0 行） |
| `python3 ci/check_no_weight_mode.py --self-test` | 0 | `SELF_TEST PASS cases=5`（绿 1 / 红 3 / fail-closed 1） |
| `python3 ci/check_no_weight_mode.py` | 0 | `files=346 lines=56992 无未留痕命中` |
| `python3 tests/config/check_cfg002_registry.py --self-test` | 0 | **CFG002 11/11 PASS + `SELF_TEST PASS injections=21 problems=0`**（行号锚未漂移：CFG002-01/02/09/11 全绿） |
| `git diff --numstat` 行数中性核查 | 0 | 除冻结层 2 行横幅外，被锚点约束文件**逐行原地替换**（行数不变）；见 §5 末段 |
| 负例注入：临时在 `docs/` 写回一处裸 `weight_mode` → `ci/check_no_weight_mode.py` | 1 | **判红**（`[R1-ASCII-KEY] docs/__doc201_negative_probe.md:2`）；撤回后复跑 **rc=0 复绿**（日志 `run/RELEASE-03/logs/DOC-201-negative-injection.log`） |
| 术语替换自证（`docs/science/**` 非 v6 + `docs/algorithms/**` 非 v6） | 0 | **PASS**：45 处替换行全部 1:1 原地替换，**P2 新引入数字字面量 = 0**、**P3 丢失数字 = 0** ⇒ 公式/常数/容差/行号锚零改动（日志 `run/RELEASE-03/logs/DOC-201-termproof.log`） |
| `python3 tools/quality/contracts/check_doc_symbols.py` | 0 | PASS（187 文档） |
| `python3 tools/doccheck/check_doc_index.py --strict` | 0 | DOC_INDEX_PASS |
| `python3 tools/quality/contracts/check_api_contracts.py` | 0 | PASS（381 行签名与头文件实测一致） |
| `python3 tools/quality/contracts/check_config_contracts.py` | 0 | PASS |
| `python3 -B -m unittest discover -s tests/contracts -t tests/contracts` | 0 | 63 tests OK |
| `python3 tools/check_glossary.py` / `tools/check_l0_docs.py` / `tools/doccheck/check_engineering_constraints.py` / `tools/doccheck/check_version_namespaces.py` / `ci/check_registry_doc_sync.py` | 0 | 全 PASS |

**行数中性**：除历史/冻结层的 2 行作废横幅外，所有被 config 锚点/文档引用约束的文件
（`docs/plugins/**`、`docs/science/**`、`docs/algorithms/**`、`docs/contracts/{DATA_SEMANTICS,PUBLIC_API,CONFIG_CONTRACT}.md`）
均**逐行原地替换**，文件行数不变（`DATA_SEMANTICS.md` 2775、`PUBLIC_API.md` 2161、`CONFIG_CONTRACT.md` 218、
`07_noise_snr.md` 171、`13_integration.md` 93）。

## 6 影响面与交接

- **科学数值**：**零影响**——未改任何公式、常数、默认容差、SCI/ALG 冻结定义或数值结论；
  仅「权重模式」这一**不存在概念的键名/表述**被删除或改写。
- **跨任务交接（明确登记）**：
  1. `snr_weight_mode` → `upm_weight_source`（UPM 控制点权重来源，W06；语义结论待 **EXP-201**）：
     **文档侧本任务已改名**；**代码侧改名由 FIX-206 承担**（`lib/algorithms/coverage/**`、
     `lib/infrastructure/scheduler/src/module_adapters.cpp` 等，见 DESIGN-DRAFT §3.1-D）。
  2. 配置键注销：`config/config_registry.json` 的 `plugin_knobs` 中 `weight_mode` 行
     （`docs/plugins/algorithms_phase2/13_integration.md:67`）、`config/defaults.json` 的
     `weight.default_mode` + `enum_target`、`config/templates/mosaic.phase_config.json` 的
     `algorithm_weight_mode`、`contracts/schemas/phase_config_mosaic.schema.json` 的该枚举
     → 归 **FIX-203/FIX-207**（`config/`、`contracts/` 不在 DOC-201 文件域）。
     **落地现状（2026-09-20 收尾复核）**：FIX-207 已把该登记行的 `registration` 由 `phase_config`
     降为 `none`、`registered_at` 清空（登记点注销），**行本身保留**以维持 CFG002-01 的
     文档行 ↔ 登记行一一对应（否则其后 3 行行号整体漂移）；文档侧对应行保留字段名 + 标作废，
     两处由此**一致且 CFG002 全绿**。schema/template/defaults 侧的键注销仍由 FIX-207 收口。
  3. `docs/contracts/v6/**` 与 `contracts/{schemas,data}/v6/**`（B 组）的**去留**（删除/退役 vs 保留）
     → 待 **DOC-203 的 Q2 裁决**（DESIGN-DRAFT §4.2-Q2）；本任务对 v6 层只做留痕、未退役。
  4. `psfsw_robust_weight` **数据对象条目**（C01）**未删除**：`tests/contracts/test_unified_object_contract.py:35,47`
     把该对象硬编为「14 对象」之一，删除会使 CHK-CONTRACT-TEST 判红，而 `tests/**`、`contracts/**`
     均不在 DOC-201 文件域 ⇒ 上呈前台裁决（见任务回执「未决」）。
- **未触碰**：三命令划分、JSON 输入输出合同结构、HiPS 产品数据模型、`testdata/`、用户数据、交付物。

## 7 机器门登记（由前台在 BLD-201 统一写入 `ci/checks.json`）

| ID | 命令 | 期望 rc | 说明 |
|---|---|---|---|
| `CHK-NO-WEIGHT-MODE` | `python3 ci/check_no_weight_mode.py --json-out run/ci/no-weight-mode/no_weight_mode.json` | 0 | R1 `weight_mode` 行必须同行留痕；R2「权重模式」行必须含「不存在/已作废」；R3 fail-closed |
| `CHK-NO-WEIGHT-MODE-SELFTEST` | `python3 ci/check_no_weight_mode.py --self-test` | 0 | 临时目录正例（绿）+ 3 类负例（红）+ fail-closed，证明能红能绿 |

## 8 构建/全量测试归因（2026-09-20 收尾，`flock` 串行）

- `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ninja -C build` → **rc=1**，
  失败目标 4 个：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（3 个 target）、
  `tests/unit/p2002_unc_rej_prov_test.cpp`（1 个 target）——均为 **FIX-202/FIX-206 在飞的
  `aio_hips_set_provenance` 签名改动**；与 DOC-201 文件域（`docs/**`、`README.md`、
  `ci/check_no_weight_mode.py`）**0 命中**（日志 `run/RELEASE-03/logs/DOC-201-build-attribution.log`）。
- `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ctest --test-dir build --output-on-failure`
  → **rc=8**：`99% tests passed, 1 tests failed out of 471`，唯一失败 `p2002_unc_rej_prov`
  （`tests/unit/p2002_unc_rej_prov_test.cpp:518` 的 `astrocs_adaptive_pixel` 方法映射，
  属 **FIX-204** 在飞改动）；该失败输出中对 DOC-201 改动文件的命中数 = **0**
  （日志 `run/RELEASE-03/logs/DOC-201-ctest-attribution.log`）。
- **本任务导致的构建/测试失败 = 0**；本任务未改任何 `lib/**`、`tests/**`、`CMakeLists.txt`。
