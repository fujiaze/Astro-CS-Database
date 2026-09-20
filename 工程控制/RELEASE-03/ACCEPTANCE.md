# RELEASE-03 验收（ACC-201 · §7.1 三层）

> 本文件由**前台**（非 SubAgent）独立复跑后写入。每条的「机器门结果」= 前台亲自跑的命令与输出；
> SubAgent 自述**不作为**验收依据，仅用于定位证据路径。
> 依据：`CONTROL_PACK_SPEC.md` §6.3/§7.1/§7.2/§7.3/§9；`AGENTS.md` §8；`00_README.md` §5。

---

## 0. 总判定

| 层 | 结论 |
|---|---|
| **L1 机器门** | `python3 ci/run_checks.py --all` → **rc=0，verdict=PASS，entries=52 / steps=99 / pass=99 / fail=0**（timeout=0 / prereq=0 / skip_platform=0 / skip_waivable=0） |
| **L2 证据核验** | 每条任务前台独立复跑关键命令，见 §2；**不复用 SubAgent 自述** |
| **L3 文档-代码一致性** | 逐任务核对改动与任务声明文件域一致；越界项逐条列出并给出授权依据（见 §4） |
| **构建** | `ninja -C build` rc=0，0 error |
| **测试** | `ctest --test-dir build --output-on-failure` → **100% tests passed, 0 failed out of 471** |
| **waiver** | `ci/exemptions.json` 零新增；**但有 1 条「已登记已知分歧」**（见 §3），不得写成「无任何豁免」 |
| **未达成项** | 「全量重建 0 警告」未达成（见 §3）——如实登记，不以口径掩盖 |

---

## 1. 任务归宿总表（§7.2 格式）

| 任务 | 状态 | 机器门结果（前台复跑） | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-201 | **PASS** | `grep -rn weight_mode docs/ README.md \| grep -v "已按 §9.73 A44 作废"` → 0 行；`ci/check_no_weight_mode.py --self-test` → SELF_TEST PASS cases=5；`ci/check_no_weight_mode.py` → rc=0 files=346 | `工程控制/RELEASE-03/change-claims/DOC-201-WEIGHT-MODE-VOID-001.md`；`run/RELEASE-03/logs/DOC-201-*.log` | PASS。74 份 docs + README + 新判据 + 变更 claim。C01 越界项已按负责人裁决 B 转 FIX-209 |
| DOC-202 | **PASS** | `docs/standards/checks/check_standards_registry.py --root .` → **rc=1 → rc=0**（既有 STD-REG 红转绿）；CFG002 11/11 + SELF_TEST PASS；config_registry 64 锚点 DRIFTED=0 | `change-claims/DOC-202-DETAIL-RED-001.md`、`DOC-202-EXP202-NAN-MASK-001.md`、`DOC-202-EXP203-PHASE2-SIGNAL-001.md` | PASS。R07–R36 36 条全部有归宿（30 落地 / 4 域外转 DOC-203 / 2 保留不改）；越界删 `SNI-S4-P3X-12` 已复核认可（该台账 exit_condition 要求） |
| DOC-203 | **PASS** | 全量 fast `pass=79 fail=7`（基线 78/8）；版本窗口 grep 仅剩排除件；`docs/api/` 第二套命令树 0 匹配 | `change-claims/DOC-203-PREREQ-Q2Q4Q6Q8-SYNC-001.md` | PASS。Q2/Q4/Q6/Q8 四项裁决各有唯一落点；S04/S05/S06/S10/S11 残留已穷尽登记（非本域） |
| DOC-204 | **PASS** | 全量 fast `pass=80 fail=6`（本任务前 78/8，失败集合 ⊆ 基线）；DOC_INDEX_PASS；DOC_002_L0_PASS；CFG002 DRIFTED=0 | `change-claims/DOC-204-AUTHORITY-STATUS-001.md` | PASS。域内「唯一权威」60→3 行（余 3 行在归档快照）；U03 lib/ 40 条只登记不改（代码非其域）。**§0.2 与 DOC-INDEX 的门禁冲突已如实上呈** |
| DOC-205（补充分片） | **PASS** | `ci/run_checks.py --check CHK-SCI-REF CHK-DANGLING CHK-CONTRACT-TEST` → **verdict=PASS entries=3 steps=23 pass=23 fail=0 rc=0**；全量 fast `pass=83 fail=3` | `change-claims/DOC-205-EXP-LANDING-001.md`、`DOC-205-ANCHOR-SYMBOL-001.md` | PASS。六项实验结论落地；16 处行锚订正；`PHASE2_SAMPLER.md:301` 自封权威清除 |
| FIX-201 | **PASS（附残留）** | `ci/check_aio_io_boundary.py --self-test` → PASS（正例+8 负例）；`ci/check_aio_io_boundary.py` → rc=0（HARD=0 / A44=0 / 已登记 155 未登记 0）；`grep ASTROCS_WEIGHT_MODE lib/infrastructure/aio/` 源码面 = 0；全量 ctest 471/471 | `ci/check_aio_io_boundary.py`、`ci/ledgers/aio_io_boundary_inventory.json` | PASS（设计点名的 3 处 + 域内同族 3 处清零）。**残留如实登记**：§9「除 aio 外写操作 = 0」尚未全局达成，台账余 155 文件全在其文件域之外，owner=后续控制包 |
| FIX-202 | **PASS** | （已由 BLD-201 全量门覆盖）`ahpx_hips_format` ctest 绿；AHPX-WEIGHT-RETIRED 判据已登记并实测能红 | `change-claims/AHPX-WEIGHT-RETIRE-20260920.md` | PASS |
| FIX-203 | **PASS** | `build/astrocs mosaic --help \| grep -c snr_path` → 1；export rotation_deg/crpix_px → 1/1；`ci/check_config_consumed.py` → rc=0 template_keys=34 dead_keys=0；`unittest ... -p "test_fix203_*.py"` → Ran 4 OK | `tests/cli/test_fix203_promoted_keys.py`；`run/RELEASE-03/logs/FIX-203-*.log` | PASS。**precision 未新造同义键**（阶段一 drizzle.precision_mode / 阶段二三 bitpix）——依 §3.3:256 |
| FIX-204 | **PASS** | `ctest -R "rejection\|p2002"` → 100% passed, 0 failed out of 2；决策点 `kPixelSmallNPolicy=kConservativeNone`（rejection.cpp:1139）；1/N worker 生产 e2e 四 bin IDENTICAL | `run/RELEASE-03/logs/FIX-204-receipt.md` | PASS。最终映射表 `1≤N≤3 none / 4≤N≤5 percentile / 6≤N≤15 winsorized / N≥16 linear fit`；**改判对称读法只需改这 1 行** |
| FIX-205 | **PASS** | `ctest -R "p3_projection\|p3_wcs"` → 100% passed, 0 failed out of 6；`p3_projection_unsupported_cli.py build/astrocs` → rc=0（8 码逐一 rc=2 + 明确原因 + 已支持清单） | `lib/algorithms/projection/p3_projection_registry.h`；`run/RELEASE-03/logs/FIX-205-*.log` | PASS。实测产品可运行集 = {TAN} 1/8；注册表 declared==implemented==runnable |
| FIX-206 | **PASS**（由 EXP-206 定案 + FIX-201 A44 收口） | EXP-206 定案取 A（`cpp/src/noise_model.cpp`）；`grep ASTROCS_WEIGHT_MODE lib/infrastructure/aio/` = 0 | `run/RELEASE-02/实验/E12-噪声模型两套/`；GAP_AUDIT §5.2 | PASS（见 §5 说明：B 退役 + A 为生产唯一实现；第三 σ 估计器已登记为后续包） |
| FIX-207 | **PASS** | `pytest tests/config -q` → 72 passed（基线 58 例 3 红）；CFG002 11/11 + SELF_TEST PASS；`ci/check_config_consumed.py` → template_keys=37 dead_keys=0 | `contracts/schemas/phase_config_{mosaic,export,normalize}.schema.json`；`run/RELEASE-03/logs/FIX-207-report.md` | PASS。三分支同构；块内键集与模板键集由 CLI 唯一键表现场派生；FLAT_ONLY_RESIDUE 19 键已登记 |
| FIX-208 | **PASS** | `unittest tests.cli.test_fix208_event_stream_default tests.cli.test_fix208_disk_gate` → Ran 13 tests OK；`ctest -R "mon004\|cli004"` → 0 failed；真实 1MiB tmpfs 磁盘满 ⇒ rc=10 + failure_kind=disk_full | `lib/infrastructure/cli/disk_gate.h`；`contracts/resource_gate_v1.json` | PASS。事件流默认输出；资源门收窄为磁盘门；**顺带修 2 处磁盘满 fail-open 实缺陷** |
| FIX-209 | **PASS** | `grep -rn psfsw_robust_weight contracts/schemas/unified/ \| grep -v retired` → 0；OBJECTS=13 / VERDICT=13；`pytest tests/contracts -q` → 67 passed；全量 ctest 471/471 | `change-claims/CHG-2026-09-20-PSFSW-RETIRE.md` | PASS。负责人裁决 B（14→13）逐条落地；v6 层按 Q2 在位保留并加归档标注 |
| EXP-201 | **CLOSED（结论：取 control_ivar）** | 实验单元自检 + 判据冻结 sha256 `0d41e29b…`（08:29:34Z，跑后未变） | `run/RELEASE-02/实验/E07-天光采样点权重/` | CLOSED。**订正理由严格限定为「SNR² 不是有效逆方差代理」**（禁止「已被证明劣化」——grep 零命中）；预注册两条非退化判据在该配置下**不通过**已如实登记 |
| EXP-202 | **CLOSED（结论：掩膜）** | 判据冻结 sha256 `539d83a0…`；三数据面 | `run/RELEASE-02/实验/E08-NaN处置/` | CLOSED。「传播、不掩膜」被三面实测推翻 ⇒ DISP-DRZ-004 由 CLOSED 改回 TRACKED；文档/标准/接口三处同 rule_id 同文字落地 |
| EXP-203 | **CLOSED（结论：面亮度）** | 判据冻结 sha256 `562d9f74…`（08:39:28Z）；`code/selftest.py` → ALL PASS | `run/RELEASE-02/实验/E09-Phase2信号量纲/` | CLOSED（**条件式**）：直接证明的是「与 Phase1 逐位同量纲 + 密度算子 + 链内零换算」；绝对标签为继承。**新发现 f32 层级累加缺陷**（dk=9 偏差 2.5e-3）已登记 |
| EXP-204 | **CLOSED（结论：保留 N≤3 → none）** | 判据冻结 sha256 `bd8982d6…`（08:36:04Z）；6 轮复核含对抗轮 | `run/RELEASE-02/实验/E10-小N排异/` | CLOSED（**保守读法**）。理由：低/中电平可达且常见 + 反例区超出实验网格上界 + **维持负责人 2026-09-19 原裁决**。R5 高电平反例已如实登记，改判只需 1 行 |
| EXP-205 | **CLOSED（结论：无全局最优，只有适用域）** | 判据冻结 sha256 `81244053…`（08:30:41Z）+ 追加 01/02；6 轮复核 | `run/RELEASE-02/实验/E11-SNR三口径精度/` | CLOSED。**未预设稀疏最好**；稀疏失效边界 Δ*/ℓ≈1；**判据自身两处退化已如实登记**（帧级精度门恒真、偏差门灵敏度差 2 个数量级） |
| EXP-206 | **CLOSED（结论：取 A）** | 判据冻结 sha256 `d5119c6f…`；B 偏差 +104%…+231% | `run/RELEASE-02/实验/E12-噪声模型两套/` | CLOSED。生产唯一实现 = A；B 已退役；第三 σ 估计器（star_detector +71.8%）登记为后续包 |
| BLD-201 | **PASS（附 1 条已登记分歧 + 1 项未达成）** | `ninja -C build` rc=0 / 0 error；`ctest` **471/471**；`ci/run_checks.py --all` **rc=0 verdict=PASS 52/99/99/0**；waiver 零新增 | `run/RELEASE-03/logs/BLD-201-receipt.md`、`BLD-201-runchecks-all.json` | PASS（见 §3 两项如实登记） |
| FIX-210（E2E 派生） | **PASS** | 真实 testdata 多块 `normalize`（2 块）→ rc=0「phase1 complete (blocks=2)」+ 逐块独立 manifest；`unittest ... -p "test_fix210_*.py"` → Ran 5 OK；ctest **472/472**；`run_checks --all` rc=0 PASS 52/99/99/0 | `tests/cli/test_fix210_block_key_parity.py`、`lib/infrastructure/cli/parser.cpp`、`lib/algorithms/coverage/src/sampler.cpp` | PASS。修 E2E-201 发现的 **D1 阻断缺陷**（块内键集误用模板骨架表 ⇒ 多块 normalize 不可运行）与 **D2 并行归约丢计数** |
| E2E-201 | **PASS（附 2 项已登记例外/口径）** | 三命令真实数据 rc=0；多块（mosaic/export/normalize 修后）rc=0 + 逐块独立 manifest + CWD 残留 0；1/N worker 科学产品逐位一致（D2 修后含诊断计数）；事件流默认输出 5 流全绿 + 负例 8/8 判红；6 个 manifest 逐 artifact sha256 重算全等；bash/python 双实现树哈希一致；三命令不串（三 run_id/三 phase/产物互斥） | `run/RELEASE-03/e2e/RECEIPT.md`（= `run/RELEASE-03/logs/E2E-201-receipt.md`）+ 复跑脚本 6 个 | PASS。**门 6（kill ⇒ 无半成品）严格读法 FAIL** —— 残留 49 个半写 tile，但与 `ASTROCS_DESIGN §9` **明文登记的既有例外逐字吻合**（无完成清单、无锁/临时残留、下游消费 fail-closed），非本包回归（见 §6） |
| ACC-201 | **PASS** | 本文件 | `ACCEPTANCE.md`、`SUMMARY.md` | PASS |

---

## 2. L2 证据核验 —— 前台独立复跑记录（节选，全部为前台亲自执行）

| # | 命令 | 结果 |
|---|---|---|
| 1 | `grep -rn "weight_mode" docs/ README.md \| grep -v "已按 §9.73 A44 作废" \| wc -l` | 0 |
| 2 | `grep -rn "权重模式" docs/ README.md \| grep -v 不存在 \| grep -v 已作废 \| wc -l` | 0 |
| 3 | `python3 ci/check_no_weight_mode.py --self-test` | SELF_TEST PASS cases=5（1 绿 / 3 红 / 1 fail-closed） |
| 4 | `python3 ci/check_no_weight_mode.py` | rc=0 files=346 lines=57882 |
| 5 | `python3 docs/standards/checks/check_standards_registry.py --root .` | **rc=0**（本包开工时 rc=1，既有 STD-REG 红转绿） |
| 6 | `python3 tests/config/check_cfg002_registry.py --self-test` | CFG002 11/11 PASS + SELF_TEST PASS injections=21 problems=0 |
| 7 | `python3 ci/check_config_consumed.py` | rc=0 CHK-CONFIG-CONSUMED_PASS template_keys=37 dead_keys=0 |
| 8 | `python3 -m pytest tests/config -q` | 72 passed（基线 58 例 3 红） |
| 9 | `python3 -m pytest tests/contracts tests/version -q` | 99 passed |
| 10 | `python3 ci/check_aio_io_boundary.py --self-test` / 无参 | PASS（8 负例）/ rc=0（HARD=0 A44=0） |
| 11 | `ctest --test-dir build -R "p3_projection\|p3_wcs"` | 100% passed, 0 failed out of 6 |
| 12 | `ctest --test-dir build -R "rejection\|p2002"` | 100% passed, 0 failed out of 2 |
| 13 | `ctest --test-dir build -R "mon004\|cli004"` | 100% passed, 0 failed |
| 14 | `unittest tests.cli.test_fix208_event_stream_default tests.cli.test_fix208_disk_gate` | Ran 13 tests OK |
| 15 | `python3 -B -m unittest discover -s tests/cli -t tests/cli -p "test_fix203_*.py"` | Ran 4 tests OK |
| 16 | `./build/astrocs mosaic --help \| grep -c snr_path` / export rotation_deg / crpix_px | 1 / 1 / 1 |
| 17 | `./build/astrocs export --template \| grep -c output_mode` | 1 |
| 18 | `python3 -c "…OBJECTS…"`（tests/contracts） | OBJECTS 13 / VERDICT 13 |
| 19 | `grep -rn psfsw_robust_weight contracts/schemas/unified/ \| grep -v retired \| wc -l` | 0 |
| 20 | `grep -rn "0\.11\.0-alpha\.2\|0\.12\.0" docs/contracts docs/design docs/api docs/development` | 0 行 |
| 21 | `ctest --test-dir build --output-on-failure` | **100% tests passed, 0 failed out of 471** |
| 22 | `python3 ci/run_checks.py --all` | **rc=0 verdict=PASS entries=52 steps=99 pass=99 fail=0** |
| 23 | `ninja -C build`（含本包 W1 修复后复编） | rc=0，0 error；`aio_pipeline.h` 的 `-Wcomment` 消失 |
| 24 | `grep -rn ASTROCS_WEIGHT_MODE lib/infrastructure/aio/ --include=*.h --include=*.cpp` | 0（源码面） |

**开工基线对照**（`run/RELEASE-03/logs/DOC-201-runchecks-baseline.log`，16:30:49）：
`verdict=FAIL entries=40 steps=86 pass=81 fail=5`（CHK-SCI-REF / STD-REG / CHK-CONFIG-CONSUMED /
CHK-CONFIG-DEFAULTS / CHK-SPEC-NAMED-IMPL-ON-PROD-PATH）。
**收口**：`--all` → `entries=52 steps=99 pass=99 fail=0`（判据条目 +12，步骤 +13，红灯 5 → 0）。

---

## 3. 必须如实登记的两项（禁止掩盖）

### 3.1 「1 条已登记已知分歧」（不是 waiver，但也不是「无任何豁免」）
- **finding 原文**：`config_default_divergence:precision_mode values=['0.0', '1.0']`
- **性质**：登记于 `ci/ledgers/config_default_divergences.json` 的 `known_divergence`（该台账是检查器 docstring 指定的**唯一**入口，五字段 + 可执行 `exit_condition`）。
- **为何不能改 config/**：分歧源只在 `lib/`（`hp_drizzle_api.cpp:988/992/995`），而 `ci/check_config_defaults.py::scan_defaults()` **只扫 `repo/lib`**（`gate_common.py:85-96`）；`precision_mode` 进键集的唯一来源是 `contracts/schemas/phase_config_normalize.schema.json` 的 `$defs.drizzle_config`。⇒ **任何 config/** 改动都不可能改变该 finding**。
- **权威面无分歧**：`ASTROCS_DESIGN.md §3.3:256` + `module_adapters.cpp:4510-4528`（缺失即拒绝）。
- **exit_condition**：收敛 `hp_drizzle_api.cpp:981-1009` 的 PRECISION 优先级为单一决定点后删除本条。
- **前台定性（如实）**：该台账在功能上是**绕过 `ci/exemptions.json`「负责人批准 + 高水位只减不增 + expiry」纪律的 de-facto 豁免通道**（唯一保护是 `load_ledger` 强制的五字段）；既有 7 条同形先例（RELEASE-02）。**本条已上呈负责人复核。**

### 3.2 「全量重建 0 警告」未达成
- `BLD-201.md` 验收门要求 `ninja -C build` **0 警告**。**实测未达成**：全量 clean 重建 59 条 warning 行（生产面 18 / 测试面 41）。
- 其中 **1 条由本包引入**（`lib/infrastructure/aio/include/aio_pipeline.h:31` 的 `-Wcomment`，FIX-201 新增注释里的 `aio/**` 字面）⇒ **前台已修**（改注释写法），复编后该警告消失（前台独立复跑 #23）。
- 余者全部落 `lib/**`、`tests/**`（**超本包文件域，只登记不改**）：`orchestrator.cpp` 5 站点 `-Wformat-truncation`（且不在 astrocs 闭包）、`rejection.cpp:100` `-Wunused-function`、`gaia_client.c:2072` `-Walloc-size-larger-than`、`p1_session.cpp`、`tests/unit/aio_abi_tests.cpp:108-119` `-Wenum-compare` ×28。
- **口径缺口已上呈**：注册门 `CHK-WARN` 的文档语义是「增量单 TU 基线」，故仍绿；若改 `--clean-first` 会在 `lib/**` 未清零时立即判红。**本轮未改该口径**（避免把门打红），如实登记。

---

## 4. L3 文档-代码一致性 —— 越界项逐条

| # | 越界改动 | 授权依据 | 前台判定 |
|---|---|---|---|
| 1 | DOC-202 删 `ci/ledgers/spec_named_impl_gaps.json` 的 `SNI-S4-P3X-12` | 该台账条目自身的 `exit_condition` 要求（清单更正后删除本条目）；留着会引入新 finding | **认可**（`ci/ledgers/**` 由前台在 BLD-201 统一复核） |
| 2 | DOC-202 改 `lib/algorithms/coverage/hips_p2/{README.md,module.yaml}`（EXP-203 C1b） | 前台在 DOC-202 任务书中明确列为本域扩展 | **认可** |
| 3 | DOC-205 改 `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md:81`、`docs/contracts/DATA_ARTIFACTS.md:53`、`tests/quality/test_mod002_migration_refs.py` | 前台**明确授权**（三处一行补丁，非 waiver） | **认可** |
| 4 | BLD-201 改 `tools/check_warning_suppression.py`、`ci/ledgers/**`、`config/**`（未改）、`docs/ci/01_CHECKS.md` | 前台在 BLD-201 任务书中明确列为本域扩展 | **认可** |
| 5 | BLD-201 改 `ci/id_migration_map.json`（+12 targets/13 mappings/13 absorbed，纯增） | R13 ID 治理要求；无注册门消费 | **认可**（不接受可单独回退） |
| 6 | 前台改 `config/filters.json:720,727` 锚点 182→221 | 修 26cb79d9 引入的既有回归（CFG002-04 自那时起判红） | **认可**（独立 commit `175c13a7`） |
| 7 | 前台改 `tests/cli/{test_phase3_inprocess,test_monitor_events,test_iso_acr_gpu_isolation}.py` 的配置面 | A44 摘键 + FZ-P3-MODES 同步欠账（非放宽断言） | **认可** |

---

## 5. 实验类任务的处置说明（§7.1 第三层：科学结论 ↔ 文档一致性）

六项实验**全部 CLOSED**，结论已落到文档（变更 claim 见 §1 各行），并**逐条给出实验单元判据冻结 sha256**。
其中**三处触及 `ASTROCS_DESIGN.md`（最高设计）**：

| 变更 | 对象 | 依据实验 |
|---|---|---|
| `CHG-2026-09-20-UPM-CTRLWEIGHT` | §4.4（天光采样点权重：SNR → control_ivar） | EXP-201 |
| `CHG-2026-09-20-REJ-SMALLN` | §4.5（排异映射表 + 电平依赖注记） | EXP-204 |
| （EXP-205 落地） | §4.3 SP-0（帧级精度门退化） | EXP-205 |

> ⚠ **须负责人复核追认**：`ASTROCS_DESIGN.md §0` 规定「修改本文必须由项目负责人明确批准」。
> 本包按 `00_README.md` §5 自主裁决授权 + §9.72「科学问题一律待定、由实验证明」执行，
> 并在此**显著登记**，供负责人复核。三处改动**均未改任何科学公式/常数/容差本身**（EXP-201 的 §4.3 公式逐字不变，仅追加边界句）。

---

## 6. E2E-201 详录（前台复核）

| 门 | 判定 | 证据 |
|---|---|---|
| 三命令 rc=0 | ✅ normalize×3（真实 testdata M42 T2 三帧 + 真实 T2 .xisf 母版；12 artifacts；ivar/variance 各 115 tile）/ mosaic rc=0（3 个真实 P1 输入，203 leaf tile）/ export rc=0（4 HDU 1024²，coverage_ok=1 / reopen_ok=1 / canonical_match=true） | `run/RELEASE-03/e2e/` |
| 多块：逐块 manifest + CWD 残留 0 | ✅ mosaic 2×2 帧、export 2 块、**normalize 2 块（FIX-210 修后）** rc=0，逐块独立 manifest（`block={name,index,count}`、`status=complete`）；块 1 fail-closed 时块 2 仍 complete；CWD 跑前/跑后逐字节相同 | 同上 |
| 1/N worker 逐位一致 | ✅ normalize 全载荷逐位一致；**mosaic 全部 tile + 全部 stats 逐位一致（FIX-210 修 D2 后）**；export 4 HDU 像素逐位 + `canonical_sha256` 相等 | 同上 |
| 事件流默认输出 + schema | ✅ 冻结合同 5 条流全 GREEN（10 必含字段、sequence 0..n-1 严格单调、单 run_id、final 末行、stdout 纯 JSONL）；负例注入 **8/8 判红**；`--json` 时 stdout 恰一个 JSON 文档 | 同上 |
| 完成清单 + 树哈希可重算 | ✅ 6 个 run manifest `status=complete`，逐 artifact sha256/size 重算全等；目录树哈希 bash 与 python 两种独立实现给出相同值 | 同上 |
| 中途 kill ⇒ 无半成品 | ❌ **严格读法 FAIL**（已登记例外，见下） | 同上 |
| 三命令不串 | ✅ 三进程 / 三 run_id / 三 phase；产物互斥；进程树下无第二个 astrocs 阶段进程 | 同上 |

**E2E-201 发现的缺陷与处置**：

| 编号 | 性质 | 处置 |
|---|---|---|
| **D1** 多块形态块内键集误用模板骨架表 ⇒ normalize 多块在真实 testdata 上**不可运行** | **阻断级实现缺陷**（由 `ac75f3f3` 引入，非本轮回归） | **已修**（FIX-210）：块内键 = `session_keys() ∪ block_keys()`，与平铺门同一份键表；新增机器判据 + 真实 testdata 多块回归 |
| **D2** `p2_samples.json` 的 `rejected_insufficient_support` 并行归约丢计数（1/4/16 worker = 4214/178/322） | 既有实现缺陷（违反 §8「并行开关不得改变科学数值」字面要求） | **已修**（FIX-210）：worker 内独立局部量接收后累加；1 vs 16 worker 全部 stats 逐位一致、恒等式缺口 0/0；新增 ctest 判据（能红） |
| **D3** 实际事件流 kind 含 `stage_start`/`stage_end`/`resource_gate`/`graph`/`v6_mode_route`，超出 §6.3 列举 5 类 | **口径差异，非协议违规**（`protocol.h:39/51` 明文 kind 开放） | **只登记**：权威清单 = **10 类**（`run/RELEASE-03/logs/FIX-210-d3-kinds.log`）；建议 §6.3 与 `docs/api/CLI_PROTOCOL_V1.md` 登记，E2E 门改判据为「五类 ⊆ 实际 kind」 |
| **D4** kill -9 残留 49 个半写 tile | **既有已登记例外**（`ASTROCS_DESIGN §9` 明文登记 HiPS tile 非原子 + phase2 直写） | **如实判 FAIL**（严格读法），非本包回归；修复归后续包（`IO_AND_ATOMICITY`） |

**观察项（只登记，未处置）**：O1 两块共用同一 `run_id`；O2 `p2_final.json` 仍出 `weight_mode:2`（A44 已判该概念不存在）；O3 export FITS 恒含 `RUNID`/`CHECKSUM` ⇒ 严格字节判据对 export 恒红（需改口径）；O4 `astrocs verify` 不在命令树（rc=2）⇒ 完成清单只能外挂重算。

---

## 7. 遗留项（如实登记，未过终验不写「完成」）

见 `SUMMARY.md` §5「遗留项」。**本包不宣布发布**（`AGENTS.md` §5：只有负责人可作最终发布决定）。
