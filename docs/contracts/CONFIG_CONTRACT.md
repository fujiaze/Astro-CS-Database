# 配置合同（全局默认值 · 滤镜库 · 三命令 phase_config · cpu_profile · run_manifest）

> 上游：ASTROCS_DESIGN.md §3.2（三类配置）、§7.2（配置、事件与退出码）

> ID: CFG-001　状态: 建立（2026-09-16）　任务: 工程控制/PROJECT-GOVERNANCE-01/tasks/CFG-001.md
> 收口: W5-CFG-002（CFG-002，2026-09-17）—— 配置登记类遗留五项闭合（plugin 级默认 / per-module 旋钮归属 / 滤镜名语义 / `os_abi` 值域 / 索引归属）。§8 由「未决」改为「闭合或交接」；登记册 = `eng/packaging/config/config_registry.json`（`astrocs.config-registry/v1`）。
> 权限边界：本文件只承载**配置语义与索引**（文件域 docs/contracts/**）；schema 本体在 `eng/contracts/schemas/`，配置数据在 `eng/packaging/config/`。
> 本文件**不新增**任何科学定义、默认数值或容差；所有数值只能转录，缺权威数值者显式 `pending_authority`。

## 0 权威链

| 事项 | 权威 |
|---|---|
| 输入合同（JSON 数据块：`blocks[]` 多块 + 平铺单块简写、程序根 eng/packaging/config/） | `ASTROCS_DESIGN.md` §3.3（:124-222；§9.68 多块形态） |
| 运行前预检（绿/橙/红、error 强制阻断；**打印报错 + 详细预估，无交互式提示窗**） | `ASTROCS_DESIGN.md` §3.5（**三命令通用预检**：现行口径 = ① 打印有没有报错 + ② 详细预估（含资源与磁盘预估）；不设交互式提示窗，`-y`/`-yes`/`-force` 保留为正式接口） |
| 配置挂载 / 模板 / 机器输出 / 退出码 | `ASTROCS_DESIGN.md` §6.1-§6.3（:574-642） |
| 三类配置严格分离、benchmark 独占 cpu_profile | `docs/design/UNIFIED_MODEL.md` §3（:54-66） |
| CLI 预检与模板职责 | `docs/plugins/infrastructure/18_cli.md` §3-§5 |
| 科学红线（默认容差不可改、cpu_profile 不进科学配置） | `ENGINEERING_SPEC.md` §3（:23-28） |
| 字段名族与跨类禁止名锚点 | `docs/contracts/config_separation_anchors.json`（DATA-001，只读） |

## 1 三类配置（现场清单）

| 类 | 文件 | 写入者 | 语义 | 禁止 |
|---|---|---|---|---|
| phase_config | `eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；模板 `eng/packaging/config/templates/normalize.phase_config.json`、`eng/packaging/config/templates/mosaic.phase_config.json`、`eng/packaging/config/templates/export.phase_config.json` | 用户 / `cli --template` | 科学参数 · 输入路径 · output_dir · 算法选择；可跨机器复现 | 任何硬件字段（isa/isa_level/workers/worker_count/block/block_size/cpu_model/cpu_vendor/thread_budget/affinity_mask） |
| cpu_profile | `eng/contracts/schemas/cpu_profile.schema.json` | **仅 benchmark** | ISA/workers/block + CPU/OS/软件版本/provider hash 机器绑定 | phase_config 的 `sci_*`/`algorithm_*` 字段 |
| run_manifest | `eng/contracts/schemas/run_manifest.schema.json` | 运行时（每次运行冻结） | 源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本 | 科学参数与硬件调优字段 |

- DATA-001 锚点里保留的 phase_config 聚合 schema 路径，由本任务实现为**三份 phase 专属 schema**：`eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；**未**创建同名聚合文件，以免留下第二份等价定义（UNIFIED_MODEL §3、裁决四口径）。回归锁：`eng/tests/config/test_cfg001_contracts.py::TestPhaseConfigFamily::test_no_aggregate_second_definition`。

## 2 `eng/packaging/config/defaults.json`（50 字段，2026-09-17 快照；`astrocs.config-defaults/v1`）

- 结构：`fields[]`，每项 `{key, value, unit, constraint, authority_status, source, source_ref, pending_task, note}`；
  本节的**总数与分组计数是快照**（`noise.*` 正由 MASK-002 扩展）；权威总数以 `eng/packaging/config/defaults.json` 的 `field_count` 为准，由机器门实时对账（`TestDefaultsContract`），本节不复制科学数值。
  `weight.default_mode` 另有 `enum_token`/`enum_target`（SCI 产品名 → phase_config 字段取值 token 的唯一登记，见 §9）。
  `authority_status ∈ {sourced, owner_adjudicated, pending_authority}`。
- 机器门（`TestDefaultsContract`）：字段数 == 带 unit 数 == 带 source 或 pending_authority 数，来源不明字段 == 0；
  且**非 pending 字段的 unit 不得为 unspecified**；每个 `source_ref` 的文件:行必须真实存在，12 个关键锚点逐行核对 token。
- 分组与来源（只转录，不改数值）：

| 组 | 字段数 | 权威锚点 |
|---|---|---|
| calibration（暗场-亮场曝光容差） | 1 | 现行值 5 s；待落 `docs/science/CALIBRATION.md`（SCI-RES-01/R-003 定义 + R-004 落文档） |
| detection（σ 检测阈值） | 1 | `docs/science/STAR_DETECTION.md:21` |
| psf（默认模型/β） | 2 | `docs/science/PSF.md:7`、`:92` |
| noise（噪声模型默认配置） | 14 | `docs/science/NOISE_MODEL.md` §4/§5/§5a/§6 陈述行（:41,:49,:73,:95；逐字段 source_ref 见 defaults.json；MASK-002/SC-009 新增 5 键 k/r_min/fwhm_floor/nq≥8/N_sky≥9216 已含在 14 内）。旧锚 `NOISE_ESTIMATION.md:134` 为实现锚，按负责人指令撤出登记面 |
| rejection（排异阈值表） | 18 | `docs/science/REJECTION.md:68`（§5「阈值冻结锚点」块；W5-CFG-002 重锚，原 :131 已因文档重排失效） |
| photometry（mag_tolerance / Tukey c / IRLS / 最小星数） | 6 | `docs/science/PHOTOMETRY.md:21,24,38,39` |
| weight（默认权重口径；~~权重模式~~ 概念已作废 （已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量）） | 1 | `docs/science/PSF_SIGNAL_WEIGHT.md:14`、`:28` |
| precision（默认精度） | 1 | `docs/science/SCIENCE_SCOPE.md:53` |
| upm（k_corr） | 1 | `docs/science/PHASE2_UPM.md:24`（定义 + 冻结默认，:124 记不可接受变化；W5-CFG-002 重锚） |
| hips（tile 宽） | 1 | `docs/science/PHASE3_HIPS_TO_FITS.md:41`（W 默认 512=2⁹；W5-CFG-002 重锚） |
| drizzle（pixfrac） | 1 | 语义 `docs/science/DRIZZLE.md:25,27,31`；**数值 1.0 已落**（`defaults.json#drizzle.pixfrac`=1.0，authority_status=owner_adjudicated；DOC-SCI-001 §3 裁决 + `docs/science/DRIZZLE.md:79`（通量守恒条件不变量）+ E1 复算；v6 侧 FZ-COND-FLUX-CONSERV 逐字见 `eng/packaging/config/defaults.json#drizzle.pixfrac` 的 source/note） |
| sparse_snr / scalar_gate | 3 | **数值 pending** |

- **pending_authority 三项（禁止编造，值必须为 null）**：

| key | unit | 归属任务 | 依据 |
|---|---|---|---|
| `sparse_snr.density` | `点/度²` | SCI-RES-01/R-001 + 负责人批准 | ASTROCS_DESIGN §3.3 点名；`docs/science/**`、`docs/algorithms/**` 全库无数值（GAP-024）；单位见 `docs/plugins/algorithms_phase1/07_noise_snr.md:112` |
| `scalar_gate.rd` | unspecified | SCI-RES-01/R-002 + 负责人批准 | GAP-024；`07_noise_snr.md:61` 字段名，默认/单位列均为 —— |
| `scalar_gate.trend` | unspecified | SCI-RES-01/R-002 + 负责人批准 | 同上（`07_noise_snr.md:62`） |

- `calibration.dark_light_exposure_tolerance = 5 s`：负责人已裁决值，按裁决**不标** pending；文档侧仅有 `K=t_light/t_dark` 语义（`docs/science/CALIBRATION.md:19,21,90`），数值待 SCI-RES-01/R-004 落 `docs/science/CALIBRATION.md`。
- 负空间（口径保留，去向已定）：`docs/plugins/**` 的 plugin 级默认**不进** `fields[]`——本文件 source 规则限定 docs/science|docs/algorithms；它们改由 `eng/packaging/config/config_registry.json#plugin_knobs` 逐行登记（95 行：归属类 + 登记点 + 缺口/冲突），见 §9。CFG002-ANCHOR: item1-plugin-defaults → eng/packaging/config/config_registry.json
- **默认值 → 字段值域的唯一登记**：`fields[].enum_target`（`{schema, pointer}`）+ `fields[].enum_token`。语义：defaults 里的「产品名/方法名」必须显式映射到承载字段的取值 token；机器门断言 pointer 落到含 `enum` 的节点且 token ∈ enum。首例：`weight.default_mode = psf_information_weight`（SCI 产品名）↔ ~~`phase_config_mosaic.algorithm_weight_mode`~~ （已按 §9.73 A44 作废：键不存在；权重是派生量） 的 token `point_information`（同一口径的两套命名；依据 `docs/science/UNIFIED_SCIENCE_MODEL.md:57` 与 `docs/science/PSF_SIGNAL_WEIGHT.md` §1）。
- **登记册指针**：`registry_ref` 指向 `eng/packaging/config/config_registry.json`（plugin 级默认 / 旋钮归属 / 滤镜名语义 / os_abi / 索引归属的登记面）；本文件与登记册**不得互相复制数值**（数值唯一源 = defaults.json 与 phase_config schema）。

## 3 三命令 phase_config 与模板

结构 = `ASTROCS_DESIGN.md` §3.3 的 JSON 数据块。**两种形态，互斥**：
① **多块形态**（normalize）= 顶层 `{schema_version, blocks[]}`，每块自带一组 `input_lights` + 一套母版 + 运行参数 + **块级 `output_dir`**；
   一块 = 一次运行（独立 `output_dir` / 独立 run manifest / 块级 `name` 归属），多块按序各自成一次运行；
② **平铺单块简写**（normalize 单块时的等价写法，向后兼容）= 顶层 `{schema_version, input_lights, master_*, output_dir, ...}`。
mosaic/export 仍为 `{phase_name, config, inputs}`（`config`/`inputs` 两级 `additionalProperties:false`）；normalize 的块内用 `additionalProperties:false`、顶层键闭包用 `propertyNames`（避免 `schema_version` 与 cpu_profile 同名键冲突，见 §7 跨类不相交门）。
**已否决**（§9.68）：normalize 的逐帧 `{phase_name, config, inputs[]}` 形态已删除；CLI 遇到该形态**明确拒绝并给迁移提示**（退出码 3）。

| phase | 模板 | 必填 | 可选算法选择（逐项权威） | 输入项 |
|---|---|---|---|---|
| normalize（多块） | `eng/packaging/config/templates/normalize.phase_config.json` | 块级 `input_lights` + `output_dir`；顶层 `schema_version` + `blocks` | `algorithm_psf_model`（`docs/science/PSF.md:7,:81,:105`，当前唯一实现 Moffat4）；`sparse_snr_layer`（`ASTROCS_DESIGN.md` §3.4:161-164）；**`algorithm_drizzle_pixfrac`** 与 **`drizzle.pixfrac`**（语义/值域权威 `docs/science/DRIZZLE.md:25,:27,:31` + `docs/algorithms/DRIZZLE_GEOMETRY.md:57,:61,:102`，schema 机器强制 `0 < pixfrac <= 1`；数值默认 1.0 已落 defaults.json 的 `drizzle.pixfrac`，状态 owner_adjudicated；DOC-SCI-001 §3）；`drizzle.precision_mode`（0=FP32/1=FP64，**必须显式**） | 块级 `input_lights[]`（每帧一个 FITS 路径）+ 块级母版 `master_bias/master_dark/master_flat` + `filter_passband`（必须命中滤镜库；空串 = 显式无 filter） |
| mosaic | `eng/packaging/config/templates/mosaic.phase_config.json` | 块级 `output_dir` + 块级 `hips_paths`（平铺单块简写 = `schema_version` + 同名两键；**旧合同分支保留** `{output_dir, precision}`） | ~~`algorithm_weight_mode`~~ （已按 §9.73 A44 作废：键不存在；权重是派生量）（`docs/plugins/algorithms_phase2/13_integration.md:67`；点源默认语义 `docs/science/PSF_SIGNAL_WEIGHT.md:28`）、`algorithm_rejection_method`（method/profile 词表 `docs/science/REJECTION.md:22-23`；默认路由 `:47-53`；W5-CFG-002 重锚）、`algorithm_upm_gauge`（`docs/plugins/algorithms_phase2/11_upm.md:89`） | 块级 `hips_paths[]`（一组输入 HiPS）；**旧合同分支保留** `{product, filter?}` |
| export | `eng/packaging/config/templates/export.phase_config.json` | 块级 `output_dir` + 块级 `source`（一个输入产品）+ **`output_mode`（**必须显式**，缺键即 REJECT）**；平铺单块简写 = `schema_version` + 同名三键；**旧合同分支保留** `{output_dir, precision, output_mode, wcs}` | `wcs.projection`（§5.3:264-266 首批 8 种 + 缺省 TAN；`14_projection.md:34`）、`wcs.{rotation_deg, crpix_px}`（`14_projection.md:35,38`）；**`output_mode` 不在本列**（它**必填**：值域 `surface_brightness`/`point_source_flux`/`visualization`；合同登记的 `surface_brightness` **只作 `--template` 骨架值**，`--json` 运行**不得**靠静默缺省——见 §3 末条） | 块级 `source.hips_dir`（单值 = 一个输入产品；多产品 = 多块）；**旧合同分支保留** `{product}` |

- **落盘形态键 `storage_form`（Phase1 专属）**：Phase1 产品落盘形态由**输入配置**选定 —— 块级（或平铺顶层）`storage_form` ∈ {`archive`（默认）, `bare`}；**键缺失、空串或 `null` ⇒ 取默认 `archive` 并报一条 `level=warn` 事件**（禁止静默取默认），形态来源记入 `manifest.json#storage.form_source`；显式给出则不报 warn。键名与取值的唯一词表 = `eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary`；合同与不变式 F0/F1..F4/M1..M4 = `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` §10，设计 = `docs/design/PRODUCT_STORAGE_FORM.md` §10。**mosaic / export 的输入合同不设该键**（Phase2 固定裸服务面、Phase3 固定裸 FITS 不套壳），出现即 REJECT —— schema 面（`additionalProperties:false` / `propertyNames`）与 CLI 面（`validate_config_full` 的块内未知键门）双双生效。生产写出侧消费点属分阶段实现计划阶段 2，CLI 当前只识别并透传，已登记 `eng/ci/ledgers/dead_config_keys.json`（不得静默 no-op）。
- **索引引用键 `coverage_index`（Phase2 专属，加性可选）**：块级（或平铺顶层）`coverage_index` 是字符串路径，指向数据集级 `coverage.index.json`；存在 ⇒ 阶段二载入它做块级查询，缺失 ⇒ 规定回退 = 读入全部产品级索引现场倒排。`hips_paths` 的元素**保持字符串**（不做元素对象化），逐帧产品级索引路径由命名规则派生（`<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`）。生产消费点同属阶段 2，已登记死键台账。
- **精度显式声明**：mosaic/export 用 `config.precision`（值域 {fp32, fp64}；模板填 `fp64`，`docs/science/SCIENCE_SCOPE.md:53` 默认 FP64）；normalize 用块级 `drizzle.precision_mode`（0=FP32 / 1=FP64，**必须显式**，缺失即拒绝——`config.precision` 是旧键名，不在 CLI 键集内，见 `ASTROCS_DESIGN.md` §3.3 键集权威）。
- export 几何字段名与值域取自科学权威：`center_deg`/[`s_out_deg`]/`width_px`/`height_px`（`docs/science/PHASE3_HIPS_TO_FITS.md:41,41,42,43`（§3 符号表：W/s_out/center/W_out,H_out）；约束 `docs/science/PHASE3_HIPS_TO_FITS.md:72`：abs(dec) ≤ 85°、W_out/H_out ∈ [1,20000]、s_out > 0；W5-CFG-002 重锚）。原 `projection` 插件文档的 `mode` 在 phase_config 中命名为 `output_mode`，以避开 legacy cpu_profile v1 的 `mode` 字段名（UNIFIED_MODEL §3 禁止同名异义，见 §7 门表）。
- **`output_mode` 的必填与默认值口径（DOC-203 订正，与 FIX-207 的 fail-closed 判定一致；**不得两边相反**）**：
  - **必填且必须显式**：`{phase_name, config, inputs[]}` 旧合同分支的 `$defs.export_config.required` 即含 `output_mode`；
    FIX-207 新增的 `blocks[]` 分支（`$defs.export_block.required`）与平铺单块简写分支的 `required` **同样含 `output_mode`**
    ⇒ **三条分支一致 fail-closed**；运行期缺键即 REJECT（`FZ-P3-MODES`；`lib/infrastructure/cli/session_commands.h` 的
    `config_fields(SESSION_EXPORT)` 同面）——**合同不得比运行期松**。
  - **合同登记的 `surface_brightness` 不是「运行默认值」**：它只用于 `<cmd> --template` 的**骨架值**
    （`eng/packaging/config/templates/export.phase_config.json`；`eng/packaging/config/config_registry.json` 的 `export.config.output_mode` 登记点）。
  - 因此本表 export 行的「可选算法选择」列**不再列 `output_mode`**（原文同时把它列进「必填」与「可选/有默认」两处 = 自相矛盾，已订正）。
- **模板示例值声明**：模板中的 `path/to/...` 路径、`filter: "Baader R"`、export 的 `center_deg: [0,0]` 与 `s_out_deg: 0.001`、`width_px/height_px: 512` 都是**示例占位**（用户必须按观测改写），**不是**科学默认值；除 `precision`/`projection`/`rotation_deg` 等有权威默认者外，模板不主张任何数值默认（**`output_mode` 不在其列**：它**必填**，模板骨架值 `surface_brightness` 只是让模板可直接运行，不是「缺省可用」的许可，见上条）。`width_px/height_px = 512` 与 HiPS tile 默认（`PHASE3_HIPS_TO_FITS.md:39`）同值，仅作可运行的示例几何。
- 硬约束：normalize 块内（`additionalProperties:false`）、`drizzle`/`wcs` 两级与顶层 `propertyNames` 都拒绝任何未登记字段；mosaic/export 的 `config`/`inputs` 两级 `additionalProperties:false` ⇒ cpu_profile 的 `workers`/`isa`/`block_size` 混入必失败（负例 ④）。
- 内存/流式预算类字段（`docs/plugins/algorithms_phase3/16_fits_output.md:40-41` 的 `band_height`/`tile_cache_mb`）**不进** phase_config：它们不可跨机器复现，属实现策略，按 UNIFIED_MODEL §3 不得写入科学配置。CFG-002 已把它们登记为 `runtime_policy` 类（权威 = 插件文档；机器门断言此类旋钮不得出现在任何 phase_config 属性面），见 §9。CFG002-ANCHOR: item2-knob-ownership → eng/packaging/config/config_registry.json

## 4 `eng/packaging/config/filters.json`（`astrocs.filter-library/v1`）

- **逐字转录** `lib/algorithms/photometry/data/response_curves/filters.json`（45 条；字段 `name/channel/wavelength_nm/value/n_points`），未重采样、未插值、未改数值；机器门逐条与源文件比对（`TestFiltersLibrary::test_verbatim_transcription`）。
- **provenance**：指向 `lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`；其 source 自述 `unverified: original curve source not recorded in repository`，本库 **45/45 如实标注** `status=unverified`、`verified=false`、`url=null`、`gap_id=GAP-025`；曲线统计量（`curve_stats`）由机器门与曲线逐条对账。**补 provenance 属下一轮工程包专项**（原出处/版本/获取方式/校验和）。
- **未知滤镜 → error**：`lookup.unknown_filter = "error"`（`ASTROCS_DESIGN.md` §3.3 滤镜名反例行、§3.5 红级）；三份 phase_config schema 都保留 45 键 `$defs.filter_name` 枚举（normalize 由块级 `filter_passband` 消费：`anyOf[{const:""}, {$ref: filter_name}]`；mosaic 由 `inputs[].filter` 消费），并由机器门锁定 `enum == eng/packaging/config/filters.json 的 filters 键`（`test_filter_enum_equals_library_keys`）。
- **不含每滤镜零点**（本任务裁决口径）：零点 `location` 是**逐次运行估计量**且满足**零点平移不变量**——`docs/science/PHOTOMETRY.md:7`（估计零点 location、尺度因子 scale…）与 `:73`（零点平移不变量：F_instr 同乘 k ⇒ location 增 log10 k，scale 除 k，sigma_residual 不变）；`docs/algorithms/PHOTOMETRIC_FIT.md:9` 明确 `zero_point` 字段因「无定义式、结构体无字段」被删除。故滤镜库**不出现**任何零点列/占位/null 字段；机器门 `test_no_zero_point_column_anywhere` 对**键路径与全文**双向断言（大小写不敏感）。若将来需要每滤镜零点，属**新科学定义**，须负责人批准后另立任务。
- **匹配语义（CFG-002 冻结）**：`lookup = {unknown_filter: error, match: exact, case_sensitive: true, normalization: none, aliases: {}}` ⇒ `resolve(name) = filters[name] if name ∈ keys(filters) else ERROR(unknown_filter)`；不做大小写/空白/Unicode 归一，不解析别名（`aliases` 为空对象 = 显式声明「本库不解析任何别名」）。冻结理由：库内品牌大小写不一致（`Optolong B/G/R` 与 `OPTOLONG L-PRO Light Pollution` 并存），品牌级归一化会引入歧义；当前 45 键在「大小写 + 空白」折叠下无重名（机器门断言），但该事实不替代显式规则。
- **设计文档反例串的去向**：`ASTROCS_DESIGN.md` §3.3 的 `"bader r"`/`"bader v"`（§9.68 改写后**明确标注为「必须被拒」的反例**，不再是「示例输入值」）登记为 `lookup.non_key_examples`（`kind=design_doc_negative_example`、`resolution=ERROR`）——它们不是合法库键，也不能靠归一化变成合法键（`bader` ≠ `Baader`；且库内不存在 Baader V 曲线）。`non_key_examples` 只把反例与合法值分开，**不**把反例升级为别名。正反例与门见 §10。CFG002-ANCHOR: item3-filter-name-policy → eng/packaging/config/config_registry.json
- **死定义已登记**：三份 schema 都保留 `$defs/filter_name`；被字段 `$ref` 的只有 normalize（块级 `filter_passband`，§9.68 后取代逐帧 `inputs[].filter`）与 mosaic（`inputs[].filter` 可选、模板未用）；export 无字段引用它（导出以 HiPS 产品为单位、滤镜在上游分离）⇒ `config_registry.filter_name_policy.dead_filter_enum_phases` 登记，机器门断言该集合不得静默变化。
- **`channel`（带宽列）语义缺口**：`channel` 值域实测 `{B,G,R,L,HA,OIII,PAN,""}`，其中 7 条为空（`Johnson I`/`Johnson U`/`SDSS g,i,r,u,z`）；`docs/contracts/**` 内不存在滤镜命名/带宽条款 ⇒ **不存在旧名残留或别名冲突**，但 `channel` 语义与空值域无权威定义，登记为缺口（归属：负责人裁决 / 测光文档域，见 §9 缺口清单）。

## 5 `eng/contracts/schemas/cpu_profile.schema.json`（迁移后：单一文件，两分支）

- **单一事实源**：不再有第二份等价定义；文件以 `oneOf` 定义
  - `$defs.legacy_v1`：BENCH-004 旧 JSON（`schema_version=1`，`kernels` 数组，`hardware/build/memory_benchmark/verdict`）；
  - `$defs.profile_v2`：**当前实现** `schema="astrocs.cpu-profile/v2"`（`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp:561-613`、`verify_profile_v2` :648-710）。
- **绑定**：CPU（`host.vendor/family/model/stepping/xcr0`）· OS（`host.os_abi` ∈ {`linux`,`windows`}，CFG-002 冻结，见 §11）· 软件版本（`build.astrocs_version/source_commit/runtime_build_id`）· provider hash（`build.provider_build_ids`、`benchmark_binary_sha256`、`kernels.*.self_test_sha256`）· ISA/workers/block（`kernels.*.provider ∈ {baseline,avx2,avx512}`、`workers ≥ 1`、`block ≥ 1`）。身份绑定字段与 `check_profile_identity_v1`（`lib/infrastructure/benchmark/backend_host/cpu_routing.cpp:216-300`）逐项一致。
- **只有 benchmark 可写**：`x-astrocs-writer = "benchmark"`、`x-astrocs-not-writable-by = ["用户","cli --template","phase_config"]`；机器门 `TestCpuProfileMigration::test_writer_is_benchmark_only` + §7 的跨类不相交门。
- **兼容面（既有检查项不失效）**：顶层 `required` 取两分支共有键 `[build, kernels]`，顶层 `properties` 同时登记两分支顶层键，完整 v1 必填集落在 `$defs.legacy_v1.required`；`properties.kernels.items.required` 保持原访问路径（`eng/tests/backend/test_cpu_profile.py:74-79`、`eng/tools/validate_cpu_profile.py:41-70`）。
- **已知取舍（如实登记，需负责人知悉）**：顶层 `properties.kernels.type = ["array","object"]` 以同时容纳 v1 数组与 v2 对象，因此 `eng/tools/validate_cpu_profile.py:65` 的 `rule.get('type')=='array'` 分支对该字段不再生效；v1 kernel 项约束仍由本文件 `items.required` 与 `$defs.legacy_v1` 强制，并由负例 `cpu_profile_v1_missing_required.json` 复跑证明仍能红。
- **`host.os_abi` 值域冻结（CFG-002）**：枚举 `{linux, windows}` = 生产者字面量集合（`lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp:255-261` 只写 `windows`/`linux`；`profile_gen_v2.cpp:568` 缺 `os` 字段时回落 `linux`），平台面由 `ENGINEERING_SPEC.md` §1（Windows amd64 / Linux amd64）封闭；非该集合一律 REJECT（负例 `eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json`）。原 `x-astrocs-pending-authority` 项已撤销，改记 `x-astrocs-frozen-domains`。CFG002-ANCHOR: item4-os-abi-enum → eng/packaging/config/config_registry.json
- **kernel 接线缺口（跨域交接，不在本卡五项）**：`$defs.kernel_v1`/`$defs.kernel_v2` 被 0 处 `$ref`（`profile_v2.properties.kernels` 仅 `{type:object,minProperties:1}`，`legacy_v1.items` 仅 `{type:object}`），workers/block/provider/self_test_sha256 等约束已定义但**未被 schema 施加**；`config_registry.cpu_profile_kernel_link` 登记该状态并 pin（refs 计数），归属 CPU/benchmark 线——接线会收紧既有 profile 与负例，须同步 `profile_gen_v2.cpp` 与 eng/tests/config 夹具。
- **v1 文本/实现漂移（`x-astrocs-v1-drift` 登记，5 条）**：`hardware.feature_bits`（原文本 array／实测 integer）、`hardware.xcr0`（原文本 string／实测 integer）、`build.backend_sha256`（原文本 object／实测 string）、`kernels[].block_size`（原文本 min 1／实测 0）、`kernels[].oracle_status`（canonical 大写 PASS／实测小写 pass，读取侧归一）。本文件按**实测口径**兼容两义，canonical 值不变；不新增也不删除既有检查项。

## 6 `eng/contracts/schemas/run_manifest.schema.json`

- 冻结源码 SHA（`software_sha`，40hex）· 配置哈希（`config_hash`，sha256 64hex）· 输入/输出哈希（`manifest_input_hashes`/`manifest_output_hashes`，逐项 path+sha256，minItems 1、uniqueItems）· 工具链版本（`toolchain_version` 必填，`toolchain_compiler`/`toolchain_cmake` 可选）· `run_id` · `created_utc`。
- 字段名族取自 DATA-001 锚点（`^run_id$`、`^software_sha$`、`^config_hash$`、`^manifest_(input|output)_hashes$`、`^toolchain_[a-z0-9_]+$`）；`additionalProperties:false` 拒绝 `workers`/`isa`/`block` 等（负例复跑）。`input_hashes` 与 DATA-OBJ-PROVENANCE-001.provenance.input_hashes 同义（锚点 note）。

## 7 机器门清单（复跑命令）

```bash
# 主门（本任务新增，零第三方依赖）
timeout 300 python3 -m unittest discover -s eng/tests/config -t eng/tests/config
# 单例：模板通过 schema / 负例必败
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/packaging/config/templates/normalize.phase_config.json
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/tests/config/fixtures/negative/hardware_fields_in_phase_config.json
```

| 门 | 断言 | 测试 |
|---|---|---|
| 三模板通过对应 schema | 逐模板 `validate()==[]`；phase 身份：mosaic/export 由 `phase_name`、normalize 由 `x-astrocs-phase` + 非空 `blocks[]` | `TestPhaseConfigFamily::test_templates_pass_their_schema` |
| 负例必败 | 未知滤镜/缺 output_dir/precision_mode 越界/硬件字段混入/多块与平铺互斥/块内未知键/逐帧 `inputs[]` 已退役，各自命中预期错误串 | `TestNegativeFixturesMustFail::test_negative_01..04`、`TestMultiBlockForm::test_03..06` |
| defaults 计数 | 字段数 == 带 unit 数 == 带 source 或 pending 数；来源不明 == 0 | `TestDefaultsContract::test_counts_and_no_unknown_source` |
| 转录保真 | 11 个关键 source 锚点 (文件:行:token) 逐行成立；pending 值必为 null | `test_every_source_ref_resolves_and_key_anchors_hold`、`test_pending_items_are_the_adjudicated_gap_set` |
| 跨类不相交 | phase_config ∩ cpu_profile(v2) == ∅；∩ run_manifest == ∅；∩ cpu_profile(全体) == {precision}（登记） | `TestCrossClassDisjointness` |
| 滤镜库 | 45/45 逐字一致 + provenance unverified/GAP-025 + 无零点键 + enum==库键 | `TestFiltersLibrary` |
| cpu_profile | 单一定义、benchmark-only、v1/v2 双分支可绿、缺必有字段可红 | `TestCpuProfileMigration`、`test_cpu_profile_v1_missing_required_still_fails` |
| CFG002-01 登记对应 | `docs/plugins/**` 配置表 96 行 ↔ `eng/packaging/config/config_registry.json#plugin_knobs` 一一对应（缺登记/多登记/默认值漂移/单位漂移/行号漂移/summary 撒谎均判红） | `check_cfg002_registry.py` CFG002-01 |
| CFG002-02 登记点可解析 | 每行登记点必须真实解析（defaults 键存在 / phase_config 指针落到属性 / `blocks[]` 项属性存在（§9.68 后取代 `inputs[]`）/ cpu_profile 指针存在 / 文档 文件:行 非空）；runtime_policy 与 resource_binding 进科学配置即红；phase_config 默认值必须 ∈ 目标 enum | 同上 CFG002-02 |
| CFG002-03 默认值→值域 | `fields[].enum_target` 指针落到含 enum 节点且 `enum_token` ∈ enum | 同上 CFG002-03 |
| CFG002-04 滤镜名语义 | `match=exact` / `case_sensitive=true` / `normalization=none` / `aliases={}`；三 schema enum == 库键；6 反例必拒、4 正例必过；`non_key_examples` 锚点成立；消费 filter 的 phase 面与登记一致 | 同上 CFG002-04 |
| CFG002-05 os_abi 值域 | schema enum == 生产者字面量集合 == 登记册；profile_gen_v2 回落字面量 ∈ enum；pending 项已撤销；负例必拒、正例必过 | 同上 CFG002-05 |
| CFG002-06 module.yaml 键闭包 | 20 份 `lib/**/module.yaml` 顶层键 ⊆ 登记键集；出现 knobs/params/config/defaults 等旋钮声明键即红 | 同上 CFG002-06 |
| CFG002-07 索引归属 | `eng/packaging/config/**` 无 schema、`eng/contracts/config/**` 全 schema、两侧无同名文件；DOCUMENT_INDEX 中本文件恰一次且 ACTIVE_NORMATIVE；`eng/tests/test_index.csv` 登记 eng/tests/config 且未登记目录 ⊆ 已登记缺口清单 | 同上 CFG002-07 |
| CFG002-08 文档锚 | 本文件 5 条 `CFG002-ANCHOR:` 标记行恰一次且指向登记册 | 同上 CFG002-08 |
| CFG002-09 锚存活 | defaults 每个 `source_ref` 行存在且非空；`source` 的 path:line 提示 token 落在该行；paraphrase 例外清单只减不增 | 同上 CFG002-09 |
| CFG002-10 kernel 接线 pin | `$defs.kernel_v1`/`kernel_v2` 的 `$ref` 计数与 kernels 接线状态 == 登记册 | 同上 CFG002-10 |

## 8 CFG-001 未决项的收口（W5-CFG-002，逐条结论）

| # | CFG-001 未决项 | CFG-002 结论 | 证据 / 去向 |
|---|---|---|---|
| 1 | plugin 级默认值的入册口径 | **闭合**（不改 source 规则）：`fields[]` 仍只收 SCI/ALG 数值；plugin 级默认改由 `eng/packaging/config/config_registry.json#plugin_knobs` 逐行登记（96 行 = 23 篇插件文档配置表全集），每行给 owner_class + 登记点 + finding | §9；门 CFG002-01/02 |
| 2 | per-module knobs 的逐次覆盖 | **闭合**（归属面）：96 行逐条定归属（science_param 67 / runtime_policy 20 / cli_surface 6 / resource_binding 3）；其中 22 行「插件声明了默认值但无 SCI/ALG 权威、未进任何配置类」+ 28 行「无默认且字段未登记」+ 4 行冲突 ⇒ 逐行登记为缺口/冲突并点名归属负责人裁决；`module.yaml` 实测无旋钮声明字段，门禁止其新增旋钮键而不登记 | §9；门 CFG002-02/06 |
| 3 | 滤镜名匹配语义 | **闭合**：冻结为 `match=exact` + `normalization=none` + `aliases={}`（字节精确、不归一、不解析别名）；设计示例 `bader r`/`bader v` 登记为 `non_key_examples`（判 ERROR），不改设计文档 | §10；门 CFG002-04 |
| 4 | cpu_profile `host.os_abi` 值域 | **闭合**：枚举 `{linux, windows}`（生产者字面量集合，非编造）；`x-astrocs-pending-authority` 项撤销 → `x-astrocs-frozen-domains`；负例 `cpu_profile_v2_bad_os_abi.json` 必红 | §11；门 CFG002-05 |
| 5 | legacy v1 同名登记（`precision`） | **保留**（口径不变）：v1 `kernels[].precision` 与 phase_config `precision` 同名同义，交集锁定 `{precision}`；彻底消除需迁移 v1 producer（lib/**，本卡文件域外） | 门 `TestCrossClassDisjointness` |
| 6 | 登记面补齐 | **部分闭合 + 交接**：①`docs/DOCUMENT_INDEX.yaml` 已登记本文件（ACTIVE_NORMATIVE，恰一次，门 CFG002-07）；②`eng/tests/test_index.csv` 已登记 `eng/tests/config`（本卡落地）；③**交接**：`eng/ci/checks.json` + `docs/ci/01_CHECKS.md §2` 尚无 `UT-CONFIG` 步骤（`eng/ci/**`、`docs/ci/**` 不在本卡文件域 S6-A），建议新增 `python3 -B -m unittest discover -s eng/tests/config -t eng/tests/config` 一步并与 §2 双向对齐；④`eng/tests/**` 另有 6 个目录未登记（含新出现的 `eng/tests/gaia_zlib`），已登记为缺口清单，门要求「未登记集合 ⊆ 清单」 | §12；门 CFG002-07 |
| 7 | ENGINEERING_SPEC §7 根目录清单缺 `eng/packaging/config/` | **已落**：`ENGINEERING_SPEC.md:105` 现列 `eng/packaging/config/`（程序根全局配置，CFG-001 建立，GAP-033）；本卡不改该文档 | `ENGINEERING_SPEC.md` §7 |

### 8.1 本卡新增的跨域交接项（不在 CFG-002 文件域，供前台派线）

| 项 | 现状（实测） | 建议改法 | 归属 |
|---|---|---|---|
| `eng/ci/checks.json` 无 config 门 | CHK-UNIT 的 13 个 UT-* 步骤不含 eng/tests/config；`CON-CONFIG-CONTRACTS` 检查的是 legacy `docs/development/CONFIG_SCHEMA.md` + `lib/algorithms/coverage/src/stage2_common.cpp` 的 ~~`weight_mode`~~ 整数默认 （已按 §9.73 A44 作废：键不存在；权重是派生量）（与 phase_config 的字符串枚举不是同一面） | 在 CHK-UNIT 增 `UT-CONFIG` 步骤并与 `docs/ci/01_CHECKS.md §2` 双向对齐 | CI 线（`eng/ci/**`、`docs/ci/**`） |
|  `$defs.kernel_v1/kernel_v2` 未接线 | **已闭合（W5-CPU-001，2026-09-17）**：原 0 处 `$ref`；接线后 `kernel_v1` 由 `legacy_v1.properties.kernels.items`、`kernel_v2` 由 `profile_v2.properties.kernels.additionalProperties` $ref（refs=1/1；登记册 `cpu_profile_kernel_link.status=LINKED`，门 CFG002-10）。接线不收紧：接线前约束已由顶层 `properties.kernels.items/.additionalProperties` 等价镜像施加，接线时镜像与 `$defs` 逐字一致（门钉死） | 已完成；负例 `cpu_profile_v1_bad_kernel.json`（size_class 越界）/ `cpu_profile_v2_bad_kernel.json`（provider 越界）必红，producer 面无需改动 | CPU/benchmark 线（已闭合） |
| 插件文档与 SCI/ALG 冲突 4 条 | **已按 DOC-SCI-001 裁决闭合 4/4（2026-09-17，LEDGER-DOC 落地）**：①`04_psf.psf_model`→`moffat4`；②`08_drizzle.pixfrac` 默认 1.0 入 defaults.json（owner_adjudicated）；③`03_star_detection.detection_threshold` 改全局语义（`median(img)+5.0·bgnoise`，局部自适应登记 DISP-STAR-002）；④`06_photometry.flux_zero_point` 行删除（补输出 `location`/`scale` 指针） | 已落地：插件文档 4 处 + `config_registry.json`（conflict 4→0）+ `eng/packaging/config/defaults.json` + `CONFIG_CONTRACT` 本表 + `phase_config_normalize.schema.json` 描述 | DOC 线（variance 登记与 CFG002-01/02 逐行门复跑绿） |
| plugin 级默认缺口 22 行 + 未登记字段 28 行 | 见 `config_registry.json#plugin_knobs`（finding=gap/unregistered） | 由负责人裁决字段归属后另立任务落地（不得由 Agent 自选字段） | 负责人裁决 |

## 9 旋钮与默认登记册 `eng/packaging/config/config_registry.json`（`astrocs.config-registry/v1`）

- **覆盖面**：`docs/plugins/*/*.md` 的配置项表**全集**（23 篇 / 96 行）逐行登记，一行一个 `(module, field)`，字段：`doc/line/declared_default/unit/owner_class/registration/registered_at/registered_key/finding/note/conflict`。
- **owner_class**（归属类，一行恰一个）：`science_param`（影响科学结果，必须有 SCI/ALG 条款或已登记配置类承载）· `runtime_policy`（运行期/实现/IO/观测策略，权威 = 插件文档或 algorithms/contracts 文档，**禁入 phase_config**）· `cli_surface`（命令行参数面）· `resource_binding`（线程/资源预算，cpu_profile 或调度器，禁硬编码、禁入 phase_config）。
- **finding**：`none`（已闭合）· `gap`（插件声明了默认值但无 SCI/ALG 权威、未进任何配置类）· `unregistered`（无默认且字段本身未登记）· `conflict`（与 SCI/ALG 权威或已登记配置冲突，必须带 `conflict.{kind,evidence,owner}`）。
- **实测分布（2026-09-17，DOC-SCI-001 落地后复跑）**：95 = none 45 / gap 22 / conflict 0 / unregistered 28；归属 science_param 66 / runtime_policy 20 / cli_surface 6 / resource_binding 3；登记点 defaults_json 6 / phase_config 9 / inputs_block 8 / cpu_profile 2 / plugin_doc 11 / contracts_doc 3 / cli 6 / none 50。
- **4 条冲突（DOC-SCI-001 裁决后已全部闭合，conflict=0）**：①`04_psf.psf_model` gauss→**moffat4**（`docs/science/PSF.md:7,:81`）；②`08_drizzle.pixfrac` 默认 **1.0** 落 `defaults.json`（owner_adjudicated）；③`03_star_detection.detection_threshold` 冻结语义 = **全局** `median(img)+5.0·bgnoise`（局部自适应为目标态，DISP-STAR-002）；④`06_photometry.flux_zero_point` **行删除**（`docs/algorithms/PHOTOMETRIC_FIT.md:9`）。证据与复跑：`reports/PROJECT-GOVERNANCE-01/research/DOC-SCI-001_插件文档与科学权威冲突裁决.md` + `run/PROJECT-GOVERNANCE-01/DOC-SCI-001/logs/`。
- **module.yaml 面**：实测 20 份 `lib/**/module.yaml` 的顶层键只含契约/端口/构建/证据字段，**无任何旋钮或默认值声明字段**（因此「module.yaml 声明的可调项是否都登记」当前空集成立）；门 CFG002-06 断言键集闭包——出现 `knobs/knob/params/parameters/config/configs/defaults/tunables/options/settings` 任一键即判红，新增旋钮声明必须先在本册登记归属。
- **维护规则**：本册是**人工维护**的登记面；文档变化必须由人重新判定归属。禁止用生成脚本批量刷新制造绿灯（ENGINEERING_SPEC §8 fail-closed）；本册不复制科学数值（数值唯一源 = defaults.json / phase_config schema）。

## 10 滤镜名匹配语义（可执行规则 + 正反例）

```text
rule:   resolve(name) = filters[name] if name in keys(filters) else ERROR(unknown_filter)
        （lookup.match=exact、case_sensitive=true、normalization=none、aliases={}）
positive: "Baader R" | "Johnson V" | "SDSS r" | "OPTOLONG L-PRO Light Pollution"  -> 命中
negative: "bader r" | "bader v" | "baader r" | "BAADER R" | "Baader  R" | " Baader R" -> ERROR
```

- 三份 phase_config schema 的 `filter` 字段 enum 必须逐项等于库键（45），门同时断言 `enum == keys(filters)`。
- 正反例由门 CFG002-04 在每个消费滤镜的 phase（normalize/mosaic）上**双向**复跑：反例必须被拒、正例必须通过；`non_key_examples` 的 `where` 锚点（`ASTROCS_DESIGN.md:182`，§3.3 滤镜名反例行）必须真的含该串。
- 归一化被**显式拒绝**（不是「暂未实现」）：`aliases` 为空对象即声明「无别名」；将来要支持别名必须先登记（登记=改合同，需权威条款 + 机器门 + 迁移说明）。

## 11 `cpu_profile.host.os_abi` 值域（冻结）

```text
enum:      { "linux", "windows" }
derivation: hardware_inspect.cpp:255-261（_WIN32 -> "windows"，否则 -> "linux"）
            profile_gen_v2.cpp:568（缺 os 字段回落 "linux"）
platform:   ENGINEERING_SPEC.md §1（Windows 10+ amd64 / Linux amd64）
consumer:   cpu_routing.cpp:271-275（与 hw os.name 逐字比较，不等 -> stale_machine）
negative:   eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json（os_abi=freebsd -> REJECT）
```

- 值域是**转录**（生产者字面量集合），不是编造；非该集合 fail-closed（不夹逼、不改写、不降级）。
- 合成夹具字面量（`eng/tests/unit/cpu007_profile_store_test.cpp:56` 的 `linux-test`）不经 schema 校验，属测试内构造；若将来把 schema 校验接进 profile_store，该夹具需同步（已登记在登记册 `os_abi.known_non_production_literal`）。

## 12 索引归属（单一事实源）

| 面 | 归属 | 事实源 | 门 |
|---|---|---|---|
| 配置数据与模板 | `eng/packaging/config/**`（defaults.json / filters.json / templates/*.json） | 数据本体；**不含 schema** | CFG002-07（config 下出现 `*.schema.json` 即红） |
| 模块/CLI 契约 schema | `eng/contracts/config/**` | cli_modules_list / cli_selftest / module_dll_contract / module_lifecycle_contract | CFG002-07（全为 schema；与 eng/packaging/config/** 不得同名） |
| phase_config schema | `eng/contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json` | 三份 phase 专属；无聚合第二定义 | `test_no_aggregate_second_definition` |
| 文档事实源 | `docs/contracts/CONFIG_CONTRACT.md`（本文件） | 语义与索引 | CFG002-07（DOCUMENT_INDEX 恰一次 + ACTIVE_NORMATIVE） |
| 文档索引 | `docs/DOCUMENT_INDEX.yaml` | docs/** 与根治理文档 | `eng/tools/doccheck/check_doc_index.py` |
| 测试登记 | `eng/tests/test_index.csv` | eng/tests/** 子目录 | CFG002-07（登记 eng/tests/config；未登记集合 ⊆ 已登记缺口清单） |
| CI 注册表 | `eng/ci/checks.json` ↔ `docs/ci/01_CHECKS.md §2` | 检查项双向对齐 | **交接**：无 UT-CONFIG 步骤（见 §8.1） |

CFG002-ANCHOR: item5-index-ownership → eng/packaging/config/config_registry.json
