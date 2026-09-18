# RELEASE-02 设计符合性审计 — 死键清单（dead-keys）

> 定义：**可写但无消费者**的配置键 / CLI 旗标 —— 用户或文档能写、代码从不读取（或读后忽略），
> 造成「声明了但没做」的假象。证据 = `grep -rn '"<key>"' lib/`（排除 tests/archive/third_party）
> 或实测 CLI 行为。生成时间 2026-09-18。

## 0. 判定方法

1. 枚举 `config/templates/*.phase_config.json`、`config/defaults.json`、`contracts/schemas/phase_config_*.json`
   与 CLI `--template` 的全部键；
2. 对每个键在 `lib/`（排除 tests/archive/third_party）grep 带引号的字符串字面量；
3. 0 命中 = 无 C++ 消费者（死键）；命中但只在类型校验处 = "只校验不读"（半死）；
4. 用实测 CLI 交叉验证键是否可达。

---

## 1. 零消费者配置键（确定性）

| 键 | 出现位置（可写） | lib/ 消费者 | 实测 | 判定 | 备注 |
|---|---|---|---|---|---|
| `algorithm_rejection_method` | `config/templates/mosaic.phase_config.json`；`contracts/schemas/phase_config_mosaic.schema.json`（enum auto/sigma/winsorized/averaged_sigma/linear_fit/esd/percentile/minmax） | **0** | 顶层写 → rc=3 "unknown key"；写进 `reject:{}` 块 → 接受但被忽略，运行按硬编码 AUTO | 死键 | 违反 §4.5.5「显式指定单一算法不被自动选择覆盖」；见 DC-401 |
| `algorithm_weight_mode` | 同上（enum point_information/surface_gls/psfsw_robust） | **0** | 不可达（顶层 unknown key） | 死键 | 实际用整数 `weight_mode:{1,2}`，与合同 enum 不一致 |
| `algorithm_upm_gauge` | 同上（enum reference_frame/sum） | **0** | 不可达 | 死键 | — |
| `algorithm_psf_model` | `config/templates/normalize.phase_config.json` | **0** | 不可达 | 死键 | §3.3「默认 PSF 模型」无消费点 |
| `sparse_snr_layer` | `config/templates/normalize.phase_config.json`；DESIGN §3.4:172 | **0** | `normalize --template` 无该键；不可达 | 死键 + 功能未实现 | 见 DC-309 |
| `output_mode` | `config/templates/export.phase_config.json`；DESIGN §5.3:346 | **0** | `export --template/--help` 无该键 | 死键 + 功能未实现 | §5.3 要求的显式输出模式缺失 |
| `phase_name` | 三个 `config/templates/*.phase_config.json` | **0** | 不可达 | 死键 | 整个 phase_config 形态不被 CLI 接受 |
| `config`（顶层对象） | DESIGN §3.3:119；phase_config schema required | 无（`doc["config"]` 无读取点） | phase_config 形态被预检拒绝（rc=2） | 死键 | CLI 用扁平键 |
| `inputs`（对象/块数组形态） | DESIGN §3.3:124；phase_config schema required | 仅 `doc["inputs"]["lights"]`（runtime_client.cpp:74-87）被映射 | phase_config 块数组形态被拒 | 半死 | 块内 `light/bias/dark/flat/filter` 不被消费 |
| `center_deg` | `config/templates/export.phase_config.json` | **0** | 实际用 `center.ra_deg/dec_deg` | 死键 | — |
| `s_out_deg` | `config/templates/export.phase_config.json` | **0** | 实际用 `scale_deg_per_px` | 死键 | — |
| `wcs.projection` | `config/templates/export.phase_config.json` | 0（顶层 `projection` 才被读） | 不可达 | 死键 | — |
| `reject`（对象块的子键） | CLI `--help` 声明"可选 rejection 配置块" | 仅 `module_adapters.cpp:5556` 做 is_object 类型校验，子键**从不读** | `reject:{"algorithm_rejection_method":"sigma"}` 被接受但无效 | 半死 | 违反 §4.5.5 |
| `config/config_registry.json`（整文件） | 仓库存在（71KB） | **0** | — | 死文件 | 无任何 C++/Python 消费者 |
| `config/defaults.json`（整文件） | DESIGN §3.3:147 明确要求 | **0** C++ 消费者 | — | 死文件 | 仅 `session_commands.h:97` 注释与一个 Python 检查脚本引用；见 DC-303 |

## 2. 合同声明但运行时不可达的键

| 键 | 合同位置 | 运行时 | 判定 |
|---|---|---|---|
| `phase_name` / `config` / `inputs`（数据块） | `contracts/schemas/phase_config_normalize.schema.json`、`_mosaic`、`_export` | 预检 rc=2（"output_dir 缺失"/"input_lights 缺失"）；`parser.cpp:271-305` 白名单不含这些键 | 合同与运行时**分叉**；schema 只在 `tests/config/` 孤立校验，从不校验真实 CLI 配置 |
| `algorithm_rejection_method`（enum） | `contracts/schemas/phase_config_mosaic.schema.json` | 顶层 → "unknown key" rc=3；`reject` 内 → 忽略 | 不可达 |
| `algorithm_weight_mode`（enum） | 同上 | 不可达（实际 `weight_mode` int） | 不可达 |

## 3. 声明但被解析器拒绝的 CLI 旗标（代码有消费者，用户不可达）

| 旗标 | 代码消费者 | 命令树白名单 | 实测 | 判定 |
|---|---|---|---|---|
| `--strict-resource-gate` | `commands.cpp:565`（`strict_resource_gate_arg`） | **不在** `command_tree.h` boolean_flags / parser.cpp cmd_boolean_tokens | `unknown flag '--strict-resource-gate'` rc=2 | **死旗标**：唯一能把资源门从 record_only 切到 enforced（exit 10）的开关被解析器拒绝 ⇒ §6.3 exit 10 / §8 hard_fail 在 CLI 面不可达 |
| `--on-resource-gate` | `commands.cpp:566-567` | **不在** 白名单 | `unknown flag '--on-resource-gate'` rc=2 | 同上 |

> 后果：L4 真实日志 `run/RELEASE-01/e2e/l4/logs/p1_m42_t3_m1_red.log` 末尾
> `astrocs: WARNING (recorded, not enforced): resource gate recorded (not enforced): alloc_growth_unbounded ...`，**rc=0**。
> 资源门在进程内**永远不会**改变退出码；ACCEPTANCE_SPEC §3「G-RES-01 enforce 项零违约，exit 0」在此实现下不可被证伪（无红灯路径）。

## 4. 已确认**有**消费者的键（排除误报，供对照）

`input_lights`、`master_bias/dark/flat`、`filter_passband`、`drizzle`(nested/pixfrac/precision_mode)、`wcs`、`snr`、`hips_paths`、`weight_mode`、`legacy_allow_weight_fallback`、`upm`、`upm_save_path`、`persist_upm`、`reject_profile`、`source.hips_dir`、`center.ra_deg/dec_deg`、`scale_deg_per_px`、`width_px`、`height_px`、`projection`、`sampler`、`longitude_parity`、`coverage_output`、`frame`、`master_units/master_scale/master_flat_normalize/master_flat_median_range`、`dark_optimization/dark_scale_factor/cosmetic`。

## 5. 处置建议

1. **删除或接线**：`config/templates/*.phase_config.json`、`config/config_registry.json`、`config/defaults.json` 三者与 CLI 实际形态分叉；
   要么让 CLI 真正消费（含 `algorithm_rejection_method`、`sparse_snr_layer`、`output_mode`），要么退役并订正 §3.3/§4.5/§5.3。
2. **修复 CLI 白名单**：把 `--strict-resource-gate`/`--on-resource-gate` 纳入 `command_tree.h`，或明确退役并订正 §6.3 exit 10 语义。
3. **合同-运行时一致性门**：增加"CLI `--template` 输出必须通过 `contracts/schemas/phase_config_*.schema.json` 校验"的机器门，
   防止 schema 与实现再次分叉（当前绿灯不可信的直接原因）。
