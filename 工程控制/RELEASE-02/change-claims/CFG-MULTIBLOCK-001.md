# 变更 claim：CFG-MULTIBLOCK-001 — normalize 配置 = 多数据块 JSON（每块一套母版 + 一组 light）

- 控制包：RELEASE-02
- 日期：2026-09-20
- 依据（最高权威）：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.68（负责人裁决，2026-09-20，:2611-2643）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（变更 claim + 一致性回归）；`AGENTS.md` §1/§8
- 状态：**已落地**（schema / 模板 / CLI 解析 / 逐块运行 / 设计文档 / 合同文档 / 台账 / 测试 同步完成）
- 影响类：**顶层 JSON 合同变更**（负责人已当面定案，不再上呈 —— §9.68「属顶层 JSON 合同变更（负责人已当面定案，不再上呈）」）

## 1 裁决原文（负责人逐字，不得改写）

> 「**甲**。如果多套设备，多个通道需要不同的校准场以及运行参数的话。可以在一个 json 里写很多块。
> 就像一个 main 下面可以写很多个函数一样。不需要每条都详细写出校准帧。
> **同一组校准帧和运行参数应该支持一组 light**」

（来源：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.68，:2614-2616。）

## 2 语义（已定，本 claim 不再重新设计）

1. **一个 JSON 内可有多个数据块（block）**，形如「一个 main 下面写很多个函数」；
2. **每个 block 自带**：一组 `input_lights` + 一套母版（`master_bias`/`master_dark`/`master_flat`）
   + 运行参数（`filter_passband`/`drizzle`/`wcs`/`snr`/`photometry` 等）+ **块级 `output_dir`**；
3. **同一组校准帧和运行参数支持一组 light** —— 不得要求逐帧重复写校准帧；
4. **否决** `{phase_name, config, inputs[]}` 逐帧形态（CLI 读不了，且与「一组母版支持一组 light」相反）；
5. 现有**平铺单块**形态保留为**单块简写**（向后兼容：只有一块时两种写法等价）；
6. 两种形态**互斥**（同时出现 → 明确报错，不静默取一）。

## 3 变更对象（文件 × 变更性质）

| # | 文件 | 变更性质 | 依据 |
|---|---|---|---|
| 1 | `contracts/schemas/phase_config_normalize.schema.json` | **改写**：多块形态（顶层 `{schema_version, blocks[]}` + 平铺单块简写，`if/then/else` 二分）；**删除**逐帧 `inputs[]` 与 `phase_name`；`$defs.normalize_config` → `$defs.normalize_block`；`x-astrocs-authority` 增列 §9.68 | §9.68 定案 1/3/4；§9.68 待执行 1/3 |
| 2 | `config/templates/normalize.phase_config.json` | **改写**：两块示例（`name: red/ha`，共用 bias/dark、各自 flat/filter/output_dir），与 `astrocs normalize --template` 输出逐字节同源 | §9.68 待执行 1/3/4 |
| 3 | `lib/infrastructure/cli/session_commands.h` | `ConfigField` 增 `scope`（`top`/`block`）；normalize 字段改块级；`config_template()` 组装 `blocks[]` 两块示例；`config_field_help()` 以 `blocks[].<key>` 列出 | §9.68 待执行 2/4 |
| 4 | `lib/infrastructure/cli/parser.cpp` | 新增 `session_keys()`（键集唯一声明，原 `validate_config_full` 内静态表外提）+ `block_keys()` + `config_has_blocks` + `config_has_flat_session_keys` + `config_is_retired_perframe_form` + `retired_perframe_form_message` + `normalize_blocks_errors`；`validate_config_full` 接受会话面 `blocks` 并逐块校验 | §9.68 待执行 2/5 |
| 5 | `lib/infrastructure/cli/subcommand.h` | 预检逐块：`config_structure_errors` / `input_path_errors` / `calibration_checks` / `precheck_config` 的 blocks 分支；逐帧形态在预检面即明确拒绝 + 迁移提示（rc=3） | §9.68 待执行 2 |
| 6 | `lib/infrastructure/cli/commands.cpp` | `cmd_session1_run` 拆为「形态判定 + 逐块 `run_phase1_block`」：每块独立 `output_dir`/`run_context`/run manifest；块级 `name` 写入日志与 manifest 的 `block{name,index,count}`；`final` 事件恰一个 | §9.68 待执行 2 |
| 7 | `ASTROCS_DESIGN.md` §3.3 | **改写**：多块形态 + 单块简写 + 互斥规则；`:166`（现 :205）「数据块」定义细化为「一块 = 一组 light + 一套母版 + 波段 + 块级运行参数与 output_dir」；§9.56 越层分叉标记改为**已收口**；保留订正留痕 | §9.68 待执行 1；§9.68「与既有设计的关系」 |
| 8 | `config/config_registry.json` | 8 条 `inputs_block` 登记点 `#/properties/inputs[]` → `#/properties/blocks[]`，`registered_key` `inputs[].*` → `blocks[].*`；2 条 `sparse_snr_layer` 登记点 `$defs/normalize_config` → `$defs/normalize_block`；`dead_filter_enum_note` 同步 | §9.68 待执行 4 |
| 9 | `config/defaults.json` | `sparse_snr.spacing_px` 的 `note`：承载字段指针改 `$defs/normalize_block`；说明模板不再逐键列可选键 | §9.68 待执行 4 |
| 10 | `config/filters.json` | `lookup.non_key_examples[].where` 锚点 `ASTROCS_DESIGN.md:144/:146` → `:182`（§3.3 滤镜名反例行）；`kind` `design_doc_illustration` → `design_doc_negative_example` | §9.68 待执行 4（示例串语义随 §3.3 改写同步） |
| 11 | `docs/contracts/CONFIG_CONTRACT.md` | §0 权威链行、§3 结构/表/必填/精度/硬约束、§4 滤镜门与反例去向、§7 门表同步 | §9.68 待执行 4 |
| 12 | `docs/contracts/DATA_SEMANTICS.md` §16.1 | 增「多数据块形态」说明：会话层仍只看到单块键集（多块只是外壳） | §9.68 待执行 4 |
| 13 | `docs/contracts/unified_object_registry.json` | normalize schema 的 `note` 增 §9.68 形态 | §9.68 待执行 4 |
| 14 | `ci/check_config_consumed.py` | `template_keys()` 支持 `blocks[]` 模板面（`blocks[].<key>` 前缀）；`--self-test` 增 blocks 绿/红两例 + 「既无 config 也无 blocks」fail-closed 例 | §9.68 待执行 4（`ci/` 相关检查） |
| 15 | `ci/ledgers/dead_config_keys.json` | `dead_config_key:sparse_snr_spacing_px` → `dead_config_key:blocks[].sparse_snr_spacing_px`；说明该键已不在模板键面（保留为预备豁免） | §9.68 待执行 4（`ci/` 台账） |
| 16 | `tests/config/**` | 4 个负例夹具改块形态；新增 2 正例 + 3 负例夹具；`test_cfg001_contracts.py` / `test_cfg001_negative.py` 同步；新增 `test_cfg003_multiblock.py`（8 例）；`gate_assertions.py` 的 G2 负例清单 4 → 7（+多块互斥/块内未知键/逐帧退役）；`check_cfg002_registry.py` 支持 `blocks[]` 指针与 `$defs.normalize_block` | §9.68 待执行 4 + 本 claim §6 |
| 17 | `tests/cli/**` | 新增 `test_multiblock_normalize.py`（**9 例**：覆盖 ①-⑦ + 事件流「一次运行恰一个 final」）；`test_cli_build.py` / `test_command_tree.py` / `test_cli_protocol.py` 的模板形态断言同步 | §9.68 待执行 2/4 + 本 claim §6 |

**未改**（明确边界）：
- 科学公式 / 默认容差 / SCI/ALG 冻结定义（AGENTS §5）：本 claim 只改**配置形态**，不改任何数值与公式；
- mosaic / export 的 `{phase_name, config, inputs}` 形态与两份 schema（不在 §9.68 裁决面内）；
- `run/RELEASE-02/paper/`、`工程控制/RELEASE-02/GAP_AUDIT.md`（任务硬约束：只读）。

## 4 影响面

1. **用户可见合同面**：`normalize --json` 顶层键集新增 `blocks`（仅会话面；V1 `config validate` 顶层合同不变）；
2. **CLI 行为**：
   - 多块形态 → 逐块各成一次运行（独立 `output_dir` / `run_context.json` / `astrocs_run_*.json` / 资源三件套）；
   - 块级 `name` → stderr 日志 + manifest 的 `block{name,index,count}`；
   - 块间 `output_dir` 重复 → rc=2（否则两块会互相覆盖 manifest，违反「每块独立 manifest」）；
   - 首块失败不阻断后续块（块间独立，后续块仍留自己的 manifest），聚合返回**首个**非零 rc；取消（9）立即停止后续块；
   - 逐帧 `{phase_name, config, inputs[]}` → rc=3 + 迁移提示（预检面与运行期同文案、同码；`-force` 亦同）。
3. **退出码面**（`exit_codes.h` 数值表未改）：结构/互斥/块级 `output_dir` 缺失 → 2；块内未知键 / 逐帧形态 / 路径缺失 → 3；块级 `input_lights` 为空 → 2；
4. **manifest 面**：新增可选 `block` 子对象（只在多块形态出现；单块简写逐字节不变，向后兼容）；
5. **schema 面**：`phase_config_normalize` 的 `properties` 顶层键集变化（`phase_name`/`config`/`inputs` 消失，`blocks` + 平铺键出现）；
   `$defs.normalize_config` → `$defs.normalize_block`（登记点同步见 §3 表 8/14/15）；
   `schema_version` 不进 `properties`（避免与 cpu_profile 同名键冲突，跨类不相交门 `phase_config ∩ cpu_profile == {precision}` 不变）——顶层键闭包改用 `propertyNames`；
6. **CI 门面**：`CHK-CONFIG-CONSUMED` 模板键数 19 → 36（多块展开）；死键集合由 {center_deg, s_out_deg, snr_path, sparse_snr_spacing_px} 变为 {center_deg, s_out_deg, snr_path}（sparse 键已不在模板键面）；
7. **不受影响**：p1_session 会话层键集（`docs/contracts/DATA_SEMANTICS.md` §16.1）与全部科学节点 —— 多块只是外壳，会话永远只看到单块键集。

## 5 向后兼容策略

| 旧用法 | 新行为 |
|---|---|
| 平铺单块 `{schema_version, input_lights, master_*, output_dir, ...}` | **原样可用**（唯一判定变化：同时出现 `blocks` 时报错）；manifest 不含 `block`，逐字节兼容 |
| V1 顶层形态 `{"inputs": {"lights": [...]}}`（`runtime_client` 映射） | **原样可用**（`inputs` 为对象 ⇒ 不触发逐帧形态判别） |
| 逐帧 `{phase_name, config, inputs[]}` | **明确拒绝**（rc=3）+ 迁移提示；**不**静默降级、**不**自动转换 |
| `--template` 输出 | 形态改变（多块骨架，占位路径 `path/to/...`）——用户需按块填路径；这是本裁决的**预期**变化 |

## 6 一致性回归（本 claim 的机器门）

| 门 | 命令 | 结果 |
|---|---|---|
| 构建 | `ninja -C build` | 通过（见 `run/RELEASE-02/merge/MULTIBLOCK.md` §构建/测试实测输出） |
| 全量 ctest | `ctest --test-dir build` | 467/467（与基线一致） |
| CFG-001/002/003（tests/config） | `python3 -B -m unittest discover -s tests/config -t tests/config` | 58 例，3 失败 = **与 HEAD 基线同一组预存红**（CFG002-01/09/11 + defaults 锚漂移，均与本变更无关；CFG002-04 由本变更**修绿**） |
| CLI 面（tests/cli） | `python3 -B -m unittest discover -s tests/cli -t tests/cli` | 见 `run/RELEASE-02/merge/MULTIBLOCK.md` |
| 生产配置键消费门 | `python3 ci/check_config_consumed.py [--self-test]` | PASS（36 键 / 3 死键均已登记）+ self-test 全绿 |
| 登记册门 | `python3 tests/config/check_cfg002_registry.py` | CFG002-02 回到基线（仅剩预存 07_noise_snr:113 一条） |

## 7 遗留 / 判不了（上呈前台）

1. `ci/ledgers/dead_config_keys.json` 的 `sparse_snr_spacing_px` 条目在 §9.68 后**不再对应任何模板键**
   （模板只列必填骨架）。本 claim 选择**保留为预备豁免**（改 id 为 `blocks[].sparse_snr_spacing_px`）而非删除，
   理由：删除会让「键重回模板」时门直接判红且失去既有登记上下文；保留则条目暂时为惰性。
   **是否删除该惰性条目，属前台/负责人裁量。**
2. 模板文件与 `--template` 输出**逐字节同源**（本次选择），因此模板不再列出 `sparse_snr_layer` /
   `sparse_snr_spacing_px` / `algorithm_psf_model` 等可选键（它们仍在 schema 与 `config_registry` 登记面）。
   若前台要求模板保留可选键，则须放弃「文件 == `--template`」的同源约束（会产生两份骨架）。
3. `tests/config` 的 3 例预存红（CFG002-01/09/11 + defaults 锚漂移）**不在本 claim 文件域**，未修。
