# 配置合同（全局默认值 · 滤镜库 · 三命令 phase_config · cpu_profile · run_manifest）

> 上游：docs/ASTROCS_DESIGN.md §3.2（三类配置）、§7.2（配置、事件与退出码）

## 0 权威链

| 事项 | 权威 |
|---|---|
| 输入合同（JSON 数据块：`blocks[]` 多块 + 平铺单块简写） | `docs/ASTROCS_DESIGN.md` §4.3 |
| 运行前预检（绿/橙/红、error 强制阻断；**打印报错 + 详细预估，无交互式提示窗**） | `docs/ASTROCS_DESIGN.md` §4.5（**三命令通用预检**：① 打印有没有报错 + ② 详细预估（含资源与磁盘预估）；不设交互式提示窗，`-y`/`-yes`/`-force` 保留为正式接口） |
| 配置挂载 / 模板 / 机器输出 / 退出码 | `docs/ASTROCS_DESIGN.md` §7.2 |
| 三类配置严格分离、benchmark 独占 cpu_profile | `docs/design/UNIFIED_MODEL.md` §3（:54-66） |
| CLI 预检与模板职责 | `docs/plugins/infrastructure/18_cli.md` §3-§5 |
| 科学红线（默认容差不可改、cpu_profile 不进科学配置） | `ENGINEERING_SPEC.md` §3（:23-28） |
| 字段名族与跨类字段名锚点 | `docs/contracts/config_separation_anchors.json`（DATA-001，只读） |

## 1 三类配置（现场清单）

| 类 | 文件 | 写入者 | 语义 | 边界外字段 |
|---|---|---|---|---|
| phase_config | `eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；模板 `eng/packaging/config/templates/normalize.phase_config.json`、`eng/packaging/config/templates/mosaic.phase_config.json`、`eng/packaging/config/templates/export.phase_config.json` | 用户 / `cli --template` | 科学参数 · 输入路径 · output_dir · 算法选择；可跨机器复现 | 任何硬件字段（isa/isa_level/workers/worker_count/block/block_size/cpu_model/cpu_vendor/thread_budget/affinity_mask） |
| cpu_profile | `eng/contracts/schemas/cpu_profile.schema.json` | **仅 benchmark** | ISA/workers/block + CPU/OS/软件版本/provider hash 机器绑定 | phase_config 的 `sci_*`/`algorithm_*` 字段 |
| run_manifest | `eng/contracts/schemas/run_manifest.schema.json` | 运行时（每次运行冻结） | 源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本 | 科学参数与硬件调优字段 |

- phase_config 由**三份 phase 专属 schema** 定义：`eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；**不存在**同名聚合文件（唯一正本，`docs/design/UNIFIED_MODEL.md` §3）。回归锁：`eng/tests/config/test_cfg001_contracts.py::TestPhaseConfigFamily::test_no_aggregate_second_definition`。

## 2 `eng/packaging/config/defaults.json`（`astrocs.config-defaults/v1`）

- 结构：`fields[]`，每项 `{key, value, unit, constraint, authority_status, source, source_ref, pending_task, note}`；
  字段总数与分组计数的权威 = `eng/packaging/config/defaults.json` 的 `field_count`，由机器门实时对账（`TestDefaultsContract`）；本节不复制科学数值。
  `weight.default_mode` 另有 `enum_token`/`enum_target`（SCI 产品名 → phase_config 字段取值 token 的唯一登记，见 §9）。
  `authority_status ∈ {sourced, owner_adjudicated, pending_authority}`。
- 机器门（`TestDefaultsContract`）：字段数 == 带 unit 数 == 带 source 或 pending_authority 数，来源不明字段 == 0；
  且**非 pending 字段的 unit 一律为具体单位**（`unspecified` 只属 pending 字段）；每个 `source_ref` 的文件:行必须真实存在，12 个关键锚点逐行核对 token。
- 分组与来源（只转录，不改数值）：

| 组 | 字段数 | 权威锚点 |
|---|---|---|
| calibration（暗场-亮场曝光容差） | 1 | 值 5、单位 **s**；科学判据 = `docs/science/CALIBRATION.md` §6a「暗场-亮场曝光容差的科学判据（冻结）」（判据式 `|K·Δb| ≤ ε·σ_frame`，量纲逐项：`K` 无量纲、`Δb` ADU、`σ_frame` ADU、`ε` 无量纲）。本键是**预检面**（判定变量 `|t_light − t_dark|`，结论 🟠 warn 不阻塞），与 §6a 的**科学面**判定变量不相关，**两者各自独立判定** |
| detection（σ 检测阈值） | 1 | `docs/science/STAR_DETECTION.md:21` |
| psf（默认模型/β） | 2 | `docs/science/PSF.md:7`、`:92` |
| noise（噪声模型默认配置） | 14 | `docs/science/NOISE_MODEL.md` §4/§5/§5a/§6 陈述行（:41,:49,:73,:95；逐字段 source_ref 见 defaults.json）。**逐键标度类别（强制，封闭二分）**：① **承载 ADU 标度量的键只有 `noise.variance_floor`（ADU²）与 `noise.saturation_level`（ADU）**——两者标度必须与所作用数组同标度，数组为 `photo_scaled_adu` 时按 α²（方差）/ α（电平）换算，换算责任方 = 调用方；② 其余 12 键（`patch_grid`/`clip_sigma`/`min_patch_samples`/`max_clip_rounds`/`spatial_field_enabled`/`source_mask_radius_px`/`mask_radius_scale`/`mask_k_sigma`/`mask_r_min_px`/`mask_fwhm_floor_scale`/`mask_budget_min_patches`/`mask_budget_min_sky`）**不承载 ADU 标度量**——其量纲为像素几何长度 / 计数 / 无量纲比值，**标度不变**（对 α 免疫），不需要标度换算。标度词表 = `docs/standards/NUMERIC_STANDARD.md` |
| rejection（排异阈值表） | 18 | `docs/science/REJECTION.md` §5「阈值冻结锚点」（`docs/science/REJECTION.md:114`） |
| photometry（mag_tolerance / Tukey c / IRLS / 最小星数） | 6 | `docs/science/PHOTOMETRY.md:21,24,38,39` |
| weight（默认权重口径；权重是阶段二按该天球像素对应帧集合现场算出的派生量） | 1 | `docs/science/PSF_SIGNAL_WEIGHT.md:14`、`:28` |
| precision（默认精度） | 1 | `docs/science/SCIENCE_SCOPE.md:53` |
| upm（k_corr） | 1 | `docs/science/PHASE2_UPM.md:24`（定义 + 代码默认 1.4；不可接受变化见其 §10） |
| hips（tile 宽） | 1 | `docs/science/PHASE3_HIPS_TO_FITS.md:41`（W 默认 512=2⁹） |
| drizzle（pixfrac） | 1 | 语义 `docs/science/DRIZZLE.md:25,27,31`；**数值 0.8**（`defaults.json#drizzle.pixfrac`；通量守恒条件不变量见 `docs/science/DRIZZLE.md` §7 与 FZ-COND-FLUX-CONSERV） |
| sparse_snr / scalar_gate | 3 | **数值 pending** |

- **pending_authority 三项（值一律保持 null；取值须在 `docs/science/**` 给出数值来源后填入）**：

| key | unit | 取值来源 | 依据 |
|---|---|---|---|
| `sparse_snr.density` | `点/度²` | 待 `docs/science/**` 给出数值来源 | `docs/ASTROCS_DESIGN.md` §4.3 点名（`defaults.json` 默认参数含「稀疏控制点间隔」）；`docs/science/**`、`docs/science/algorithms/**` 全库无数值；单位见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §5 配置表。**本键不承载生产取值**（生产用像素域 `sparse_snr.spacing_px`，默认 64 px）；两者是**同一几何量的两种表述**，换算恒等式（强制，两者取值由恒等式绑定）：`density[点/度²] = 1 / (Δ_px · s_pixel_scale[度/px])²`，其中 `s_pixel_scale = (180/π)·√(π/3)/nside`（IVOA REC-HIPS-1.0 §4.4.1 定义的同名键单位为**度**）。换算责任方 = 任何同时登记两者的消费者；只声明其一即可 |
| `scalar_gate.rd` | unspecified | 待 `docs/science/**` 给出数值来源 | `07_noise_snr.md:61` 字段名，默认/单位列均为 —— |
| `scalar_gate.trend` | unspecified | 待 `docs/science/**` 给出数值来源 | 同上（`07_noise_snr.md:62`） |

- `calibration.dark_light_exposure_tolerance = 5 s`：**sourced**（值 5、单位 s）。**科学判据落 `docs/science/CALIBRATION.md` §6a**（判据式、推导与两档反例齐备，证据面读数见该节）。**适用域**：本键只作**预检面**（判定变量 `|t_light − t_dark|`，🟠 warn、不阻塞、不参与科学可信判定）。**科学面判据的唯一承担者 = §6a**——两者判定变量不相关，预检面判 PASS 不蕴含科学面残留达标（§6a 的反例读数见 `实验/` 证据面）。
- 负空间：`docs/plugins/**` 的 plugin 级默认**不进** `fields[]`（本文件 source 规则限定 docs/science|docs/algorithms）；它们由 `eng/packaging/config/config_registry.json#plugin_knobs` 逐行登记（归属类 + 登记点 + 缺口/冲突），见 §9。CFG002-ANCHOR: item1-plugin-defaults → eng/packaging/config/config_registry.json
- **默认值 → 字段值域的唯一登记**：`fields[].enum_target`（`{schema, pointer}`）+ `fields[].enum_token`。语义：defaults 里的「产品名/方法名」必须显式映射到承载字段的取值 token；机器门断言 pointer 落到含 `enum` 的节点且 token ∈ enum。首例：`weight.default_mode = psf_information_weight`（SCI 产品名）的 token = `point_information`（同一口径的两套命名；依据 `docs/science/UNIFIED_SCIENCE_MODEL.md:56` 与 `docs/science/PSF_SIGNAL_WEIGHT.md` §1）。
- **登记册指针**：`registry_ref` 指向 `eng/packaging/config/config_registry.json`（plugin 级默认 / 旋钮归属 / 滤镜名语义 / os_abi / 索引归属的登记面）；本文件与登记册**各自只引用数值**（数值唯一源 = defaults.json 与 phase_config schema）。

## 3 三命令 phase_config 与模板

结构 = `ASTROCS_DESIGN.md` §4.3（:224-261）的 JSON 数据块。**两种形态，互斥**：
① **多块形态**（normalize）= 顶层 `{schema_version, blocks[]}`，每块自带一组 `input_lights` + 一套母版 + 运行参数 + **块级 `output_dir`**；
   一块 = 一次运行（独立 `output_dir` / 独立 run manifest / 块级 `name` 归属），多块按序各自成一次运行；
② **平铺单块简写**（normalize 单块时的等价写法，向后兼容）= 顶层 `{schema_version, input_lights, master_*, output_dir, ...}`。
mosaic/export 仍为 `{phase_name, config, inputs}`（`config`/`inputs` 两级 `additionalProperties:false`）；normalize 的块内用 `additionalProperties:false`、顶层键闭包用 `propertyNames`（避免 `schema_version` 与 cpu_profile 同名键冲突，见 §7 跨类不相交门）。
normalize 只接受**多块**与**平铺单块**两形态；CLI 遇到逐帧 `{phase_name, config, inputs[]}` 形态一律**具名拒绝**（退出码 3）。

| phase | 模板 | 必填 | 可选算法选择（逐项权威） | 输入项 |
|---|---|---|---|---|
| normalize（多块） | `eng/packaging/config/templates/normalize.phase_config.json` | 块级 `input_lights` + `output_dir`；顶层 `schema_version` + `blocks` | `algorithm_psf_model`（`docs/science/PSF.md:7,:81,:105`，当前唯一实现 Moffat4）；`sparse_snr_layer`（`ASTROCS_DESIGN.md` §4.4:273）；**`algorithm_drizzle_pixfrac`** 与 **`drizzle.pixfrac`**（语义/值域权威 `docs/science/DRIZZLE.md:25,:27,:31` + `docs/science/algorithms/DRIZZLE_GEOMETRY.md:57,:61,:102`，schema 机器强制 `0 < pixfrac <= 1`；数值默认 0.8 落 defaults.json 的 `drizzle.pixfrac`）；`drizzle.precision_mode`（0=FP32/1=FP64，**必须显式**） | 块级 `input_lights[]`（每帧一个 FITS 路径）+ 块级母版 `master_bias/master_dark/master_flat` + `filter_passband`（必须命中滤镜库；空串 = 显式无 filter）。**命中后的解析责任（强制）**：`resolve(name)` 必须按 §4 的三条冻结条款执行——① 匹配语义（字节精确、无归一、无别名）；② **曲线身份核对**（对象 `name` / `n_points` / `curve_stats` 逐项对账，缺字段即 fail-closed）；③ **`channel` 非空校验**（`channel == ""` 的曲线只走不依赖通带类别的路径；`F_syn` 通带积分需要通带类别）。**取错通带的后果与判据**：通带错配表现为 `F_syn` 的跨星散度 ⇒ 单帧测光一致性残差是通带声明正确性的机器判据（正本 = `docs/science/PHOTOMETRY.md` §2a，读数见其证据面）。**适用域**：本条只约束 `filter_passband` → 曲线的解析与消费，不改变滤镜库内容 |
| mosaic | `eng/packaging/config/templates/mosaic.phase_config.json` | 块级 `output_dir` + 块级 `hips_paths`（平铺单块简写 = `schema_version` + 同名两键；`{phase_name, config, inputs}` 形态的 `config` 键 = `{output_dir, precision}`） | `algorithm_rejection_method`（method/profile 词表 `docs/science/REJECTION.md:22-23`；默认路由 `:47-53`）、`algorithm_upm_gauge`（`docs/plugins/algorithms_phase2/11_upm.md:92`）。权重是阶段二现场算出的派生量，**不存在** `algorithm_weight_mode` / `weight_mode` 键（点源默认语义 `docs/science/PSF_SIGNAL_WEIGHT.md:28`） | 块级 `hips_paths[]`（一组输入 HiPS）；`{phase_name, config, inputs}` 形态的 `inputs[]` 项 = `{product, filter?}` |
| export | `eng/packaging/config/templates/export.phase_config.json` | 块级 `output_dir` + 块级 `source`（一个输入产品）+ **`output_mode`（**必须显式**，缺键即 REJECT）**；平铺单块简写 = `schema_version` + 同名三键；`{phase_name, config, inputs}` 形态的 `config` 键 = `{output_dir, precision, output_mode, wcs}` | `wcs.projection`（§5.3:264-266 首批 8 种 + 缺省 TAN；`14_projection.md:34`）、`wcs.{rotation_deg, crpix_px}`（`14_projection.md:35,38`）、**`crop`（导出裁剪范围，可选，缺省 = 不裁剪；在**已定义好的输出画幅**上取矩形子窗，不改画幅定义/投影/重采样）**：两种**互斥**形式 —— `pixels`（平面像素矩形，FITS 1-based 闭区间，用户到平面后手动裁剪）/ `sky`（天球轴对齐矩形 ICRS deg，GUI 的 HiPS 浏览器框选导出走这一形式）；键形固定可机器生成、GUI 直接填；判别键名取 `crop_form`（避开 `cpu_profile` 的 `mode`，UNIFIED_MODEL §3 同名即同义，与 `output_mode` 同一处置）；fail-closed 全部具名报错（越界 / 宽高非正 / 两形式同时给 / 裁剪后为空 / `sky` 越出 TAN 半球），**禁静默夹取**；写出 FITS 的 WCS = 原画幅 WCS 在窗口上的**精确限制**（`CRVAL`/`CD` 逐位不变、`CRPIX` 减**整数**窗口原点）⇒ 窗口内像素与不裁剪时**逐位相同**。正本 = `docs/design/PHASE3_DETAILED_DESIGN.md` §8；字段合同 = `eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop`；几何唯一实现 = `lib/algorithms/projection/p3_wcs.h`（CLI 配置面与节点面共用）；接口登记 = `docs/api/CLI_PROTOCOL_V1.md` §7.1；生产消费点 = scheduler p3 节点链 wcs/writer/verify（非死键）；`crop` 不进 `--template` 骨架（模板不替用户主张裁剪）；**`output_mode` 不在本列**（它**必填**：值域 `surface_brightness`/`point_source_flux`/`visualization`；合同登记的 `surface_brightness` **只作 `--template` 骨架值**，`--json` 运行的取值只走显式声明——见 §3 末条） | 块级 `source.hips_dir`（单值 = 一个输入产品；多产品 = 多块）；`{phase_name, config, inputs}` 形态的 `inputs[]` 项 = `{product}` |

- **落盘形态键 `storage_form`（Phase1 专属）**：Phase1 产品落盘形态由**输入配置**选定 —— 块级（或平铺顶层）`storage_form` ∈ {`archive`（默认）, `bare`}；**键缺失、空串或 `null` ⇒ 取默认 `archive` 并报一条 `level=warn` 事件**（取默认与 warn 事件成对出现），形态来源记入 `manifest.json#storage.form_source`；显式给出则不报 warn。键名与取值的唯一词表 = `eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary`；合同与不变式 F0/F1..F4/M1..M4 = `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` §10，设计 = `docs/design/PRODUCT_STORAGE_FORM.md` §10。**mosaic / export 的输入合同不设该键**（Phase2 固定裸服务面、Phase3 固定裸 FITS 不套壳），出现即 REJECT —— schema 面（`additionalProperties:false` / `propertyNames`）与 CLI 面（`validate_config_full` 的块内未知键门）双双生效。生产写出侧消费点属分阶段实现计划阶段 2；CLI 当前只识别并透传，写出侧尚未消费该键。
- **索引引用键 `coverage_index`（Phase2 专属，加性可选）**：块级（或平铺顶层）`coverage_index` 是字符串路径，指向数据集级 `coverage.index.json`；存在 ⇒ 阶段二载入它做块级查询，缺失 ⇒ 规定回退 = 读入全部产品级索引现场倒排。`hips_paths` 的元素**保持字符串**（不做元素对象化），逐帧产品级索引路径由命名规则派生（`<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`）。生产消费点同属阶段 2。
- **精度显式声明**：mosaic/export 用位深键显式声明（fp32/fp64；模板填 `fp64`，`docs/science/SCIENCE_SCOPE.md:53` 默认 FP64）；normalize 用块级 `drizzle.precision_mode`（0=FP32 / 1=FP64，**必须显式**，缺失即拒绝）。CLI 精度键集只有上述键：键集外字段（含 `config.precision` 形态）一律拒绝（键集权威 = `docs/ASTROCS_DESIGN.md` §4.3）。
- **精度的科学不变量（强制，跨三命令同面）**：精度选择只改变**表示误差**，任何科学量的**标度类别**（`docs/standards/NUMERIC_STANDARD.md` 标度词表）与**量纲**保持不变。判据（非退化）：同一输入在 fp32 与 fp64 下，各产品面的标度类别与单位串必须逐项相同；数值差异只允许落在该面事前冻结的浮点容差内（`docs/contracts/SCHEDULER_CONTRACT.md` §2.1）。**标度换算与精度无关**；`BUNIT` 与缺失编码在 fp32/fp64 下取值相同。三命令的精度键名不同（`precision` / `drizzle.precision_mode`）但**语义与不变量同面**；键名差异不构成两套口径。
- export 几何字段名与值域取自科学权威：`center_deg`/[`s_out_deg`]/`width_px`/`height_px`（`docs/science/PHASE3_HIPS_TO_FITS.md:41,41,42,43`（§3 符号表：W/s_out/center/W_out,H_out）；约束 `docs/science/PHASE3_HIPS_TO_FITS.md:72`：abs(dec) ≤ 85°、W_out/H_out ∈ [1,20000]、s_out > 0）。原 `projection` 插件文档的 `mode` 在 phase_config 中命名为 `output_mode`，以避开 legacy cpu_profile v1 的 `mode` 字段名（UNIFIED_MODEL §3 同名即同义，见 §7 门表）。
- **`output_mode` 的必填与默认值口径（三条分支同面，fail-closed）**：
  - **必填且必须显式**：`{phase_name, config, inputs[]}` 形态（`$defs.export_config.required`）、`blocks[]` 形态
    （`$defs.export_block.required`）与平铺单块简写形态的 `required` **都含 `output_mode`**
    ⇒ **三条分支一致 fail-closed**；运行期缺键即 REJECT（`FZ-P3-MODES`；`lib/infrastructure/cli/session_commands.h` 的
    `config_fields(SESSION_EXPORT)` 同面）——**合同面 ≥ 运行期面（同严或更严）**。
  - **合同登记的 `surface_brightness` 不是「运行默认值」**：它只用于 `<cmd> --template` 的**骨架值**
    （`eng/packaging/config/templates/export.phase_config.json`；`eng/packaging/config/config_registry.json` 的 `export.config.output_mode` 登记点）。
  - 因此本表 export 行的「可选算法选择」列**不列 `output_mode`**（它只属「必填」列）。
- **模板示例值声明**：模板中的 `path/to/...` 路径、`filter: "Baader R"`、export 的 `center_deg: [0,0]` 与 `s_out_deg: 0.001`、`width_px/height_px: 512` 都是**示例占位**（用户必须按观测改写），**不是**科学默认值；**示例滤镜串必须始终是滤镜库的合法键**（`"Baader R"` 是合法键；改成库外串会让模板自身通不过 schema 枚举 ⇒ 模板回归必红）——示例值不主张「本次观测的真实通带就是它」，通带声明正确性由 §4 的曲线身份核对与测光残差判据承担；除 `precision`/`projection`/`rotation_deg` 等有权威默认者外，模板不主张任何数值默认（**`output_mode` 不在其列**：它**必填**，模板骨架值 `surface_brightness` 只是让模板可直接运行，不是「缺省可用」的许可，见上条）。`width_px/height_px = 512` 与 HiPS tile 默认（`PHASE3_HIPS_TO_FITS.md:39`）同值，仅作可运行的示例几何。
- 硬约束：normalize 块内（`additionalProperties:false`）、`drizzle`/`wcs` 两级与顶层 `propertyNames` 都拒绝任何未登记字段；mosaic/export 的 `config`/`inputs` 两级 `additionalProperties:false` ⇒ cpu_profile 的 `workers`/`isa`/`block_size` 混入必失败（负例 ④）。
- 内存/流式预算类字段（`docs/plugins/algorithms_phase3/16_fits_output.md:42-43` 的 `band_height`/`tile_cache_mb`）**不进** phase_config：它们不可跨机器复现，属实现策略，按 UNIFIED_MODEL §3 只入 `runtime_policy` 面。登记面把它们登记为 `runtime_policy` 类（权威 = 插件文档；机器门断言此类旋钮只出现在 `runtime_policy` 面（phase_config 属性面零命中）），见 §9。CFG002-ANCHOR: item2-knob-ownership → eng/packaging/config/config_registry.json

## 4 `eng/packaging/config/filters.json`（`astrocs.filter-library/v1`）

- **逐字转录** `lib/algorithms/photometry/data/response_curves/filters.json`（45 条；字段 `name/channel/wavelength_nm/value/n_points`），未重采样、未插值、未改数值。**`channel` 是转录字段之一**，因此其角色与空值语义按本节下文的冻结条款判定（见「`channel`（通带类别列）的角色与空值语义」）；转录保真只保证「值与源文件逐字相同」，**不**主张曲线物理正确（provenance 见下条）；机器门逐条与源文件比对（`TestFiltersLibrary::test_verbatim_transcription`）。
- **provenance**：指向 `lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`；其 source 自述 `unverified: original curve source not recorded in repository`，本库 **45/45 如实标注** `status=unverified`、`verified=false`、`url=null`、`gap_id=GAP-025`；曲线统计量（`curve_stats`）由机器门与曲线逐条对账。**适用域与验证方式（严格自洽说明）**：曲线数据只以「与源文件逐字相同的转录」身份入库，其物理正确性未经外部来源核对；任何「与标准滤光片/文献一致」的主张必须另附独立可核验出处（原始曲线来源 + 版本 + 获取方式 + 校验和）。**缺口登记**：原始曲线来源未入库，登记在册（`gap_id=GAP-025`，登记面 = `eng/packaging/config/config_registry.json`）。
- **未知滤镜 → error**：`lookup.unknown_filter = "error"`（`ASTROCS_DESIGN.md` §4.3 滤镜型号逐字匹配、§4.5 红级 error）；三份 phase_config schema 都保留 45 键 `$defs.filter_name` 枚举（normalize 由块级 `filter_passband` 消费：`anyOf[{const:""}, {$ref: filter_name}]`；mosaic 由 `inputs[].filter` 消费），并由机器门锁定 `enum == eng/packaging/config/filters.json 的 filters 键`（`test_filter_enum_equals_library_keys`）。
- **不含每滤镜零点**（合同口径）：零点 `location` 是**逐次运行估计量**且满足**零点平移不变量**——`docs/science/PHOTOMETRY.md:7`（估计零点 location、尺度因子 scale…）与 `:73`（零点平移不变量：F_instr 同乘 k ⇒ location 增 log10 k，scale 除 k，sigma_residual 不变）。**适用域（正向约束）**：该不变量成立的条件是 `r_i = log10(F_instr,i/F_syn,i)` 取**同一 scale 口径**（同一参考通量单位与同一 `F_syn` 定义）且 `k` 是**纯乘性**变换（线性区，无饱和/无截断）。跨运行比较 `location` 时，`F_syn` 口径变化会整体平移 `location` ⇒ **数值不可跨运行直接比较**；量纲与换算见 `docs/contracts/DATA_SEMANTICS.md` §14.3 的量纲条；`docs/science/algorithms/PHOTOMETRIC_FIT.md:9` 明确 `zero_point` 字段因「无定义式、结构体无字段」被删除。故滤镜库**不出现**任何零点列/占位/null 字段；机器门 `test_no_zero_point_column_anywhere` 对**键路径与全文**双向断言（大小写不敏感）。若将来需要每滤镜零点，属**新科学定义**，须走变更流程另立条款。
- **匹配语义（冻结）**：`lookup = {unknown_filter: error, match: exact, case_sensitive: true, normalization: none, aliases: {}}` ⇒ `resolve(name) = filters[name] if name ∈ keys(filters) else ERROR(unknown_filter)`；不做大小写/空白/Unicode 归一，不解析别名（`aliases` 为空对象 = 显式声明「本库不解析任何别名」）。冻结理由：库内品牌大小写不一致（`Optolong B/G/R` 与 `OPTOLONG L-PRO Light Pollution` 并存），品牌级归一化会引入歧义；当前 45 键在「大小写 + 空白」折叠下无重名（机器门断言），但该事实不替代显式规则。
- **曲线身份核对（装配期 fail-closed，冻结）**：`resolve(name)` 命中键后，装载器**必须**再核对「取到的曲线对象就是该键声明的曲线」，四项同时成立才算通过：① 对象 `name` 字段与库键**逐字节相等**；② 对象 `n_points` 与实际 `wavelength_nm` 数组长度相等；③ 对象自述与 `provenance.per_filter[<键>].curve_stats` 的 `n_points`/`wl_min`/`wl_max`/`val_min`/`val_max` 一致；④ 同上的 `wl_sum`/`val_sum`/`val_sumsq`（逐元素和）与**实际数组**算出的值一致。任一项不成立 ⇒ 具名错误 `curve_identity_mismatch`（作用域 = 环境/配置），装载器只返回该具名错误。
  - ③ 与 ④ 的分工（冻结）：`min`/`max` 只是包络，两支不同曲线可以共用同一包络 ⇒ ④ 的逐元素指纹是**必需项**，`curve_stats` 缺这三个字段即 fail-closed（缺字段 ≠ 身份已核对）。
  - 该核对**不改变** `lookup` 语义（仍是字节精确、无别名），也不放宽任何科学阈值；它约束的是「解析器有没有取错对象」。
  - 消费侧另有两条独立约束：配置声明的通带名与 `FILTER` 关键字解析出的库键必须相等（不等 ⇒ `passband identity mismatch`）；同一次运行内各组帧只能有一条模型通带（不一致帧判 `PHOT_PASSBAND_IDENTITY_INCONSISTENT`）。
  - 判据锚：`lib/infrastructure/pipeline/orchestrator/cpp/tests/test_photometry_curve_resolve.cpp`（`[I0..I9]`，含恒真自检）与 `test_p1phot_passband_identity.cpp`（`[F1..F6]`）。正本 = `docs/science/PHOTOMETRY.md` §2a.7。
- **反例串的权威锚点**：`"bader r"`/`"bader v"` 的**唯一权威锚点 = 本文件 §10 负例表**（`eng/packaging/config/filters.json#lookup.non_key_examples[].where` 指向它）。最高设计全篇不含 `bader`，因此这两个串的锚点只落在本文件 §10 负例表。两个串仍登记为 `lookup.non_key_examples`（`kind=design_doc_negative_example`、`resolution=ERROR`，值与判据未变）——它们不是合法库键，也不能靠归一化变成合法键（`bader` ≠ `Baader`；且库内不存在 Baader V 曲线）。`non_key_examples` 只把反例与合法值分开，**不**把反例升级为别名。正反例与门见 §10。CFG002-ANCHOR: item3-filter-name-policy → eng/packaging/config/config_registry.json
- **死定义已登记**：三份 schema 都保留 `$defs/filter_name`；被字段 `$ref` 的只有 normalize（块级 `filter_passband`，§9.68 后取代逐帧 `inputs[].filter`）与 mosaic（`inputs[].filter` 可选、模板未用）；export 无字段引用它（导出以 HiPS 产品为单位、滤镜在上游分离）⇒ `config_registry.filter_name_policy.dead_filter_enum_phases` 登记，机器门断言该集合的每次变化都显式登记。
- **`channel`（通带类别列）的角色与空值语义（冻结）**：`docs/ASTROCS_DESIGN.md` §4.3 冻结 `filters.json` 承载**三列**——「型号、通带、波长」；`channel` 即其中的**通带类别**列，与 `wavelength_nm`（波长轴，nm）配合。本文件**不新增取值**，只冻结其角色与空值语义：
  - 值域 = `{B,G,R,L,HA,OIII,PAN,""}`；空值组 = `Johnson I`、`Johnson U`、`SDSS g`、`SDSS i`、`SDSS r`、`SDSS u`、`SDSS z`。**复算方法（就地，不依赖外部目录）**：读 `eng/packaging/config/filters.json`，对每个滤镜条目取 `channel` 字段
（缺字段按 `""` 计），按取值分组计数；判据 = 按上述方法就地复算出的分组计数与空值组型号清单恒与 `eng/packaging/config/filters.json` 一致，空值组恰为列出的 7 个型号名。
反例（负对照）：把任一空值条目的 `channel` 填成合法类别后重跑，计数与空值清单必须同时变化 ⇒ 判据非退化。
  - **空值语义（冻结）**：`""` = **未声明通带类别**（≠「无通带」、≠「宽带」）。
  - **解析器的校验责任（冻结）**：`resolve(name)` 在名字命中后**必须**再校验 `channel` 非空；`channel == ""` 的曲线只走不依赖通带类别的路径：判定、筛选、换算与 `F_syn` 通带积分都需要通带类别 ⇒ **必须 fail-closed**（具名报错），通带类别只取自 `channel` 列。
  - 适用域：本条只约束 `filter_passband` → 曲线的解析与消费；**不改变**已冻结的 `lookup = {unknown_filter: error, match: exact, case_sensitive: true, normalization: none, aliases: {}}` 语义。
- **通带正确性的证据资格与判据锚（冻结）**：
  - 本库全部曲线 `status=unverified`、`verified=false`、`url=null`（§上条 provenance 行）⇒ 「与标准滤光片/文献一致」的主张只走独立可核验出处（原始曲线来源 + 版本 + 获取方式 + 校验和，见 §9 缺口清单）；本库曲线本身只承担曲线数据。
  - **判据锚（可执行）**：通带错配的后果是 `F_syn` 的跨星散度；正本与四类证据 = `docs/science/PHOTOMETRY.md` §2a（SCI-PHOT-FORMULA-01，读数见其证据面）。⇒ **单帧测光一致性残差是通带声明正确性的机器判据**：`channel` 未声明却按通带消费、或曲线取错时，该残差判红。
  - **转录偏差（按行登记，修复归 `eng/packaging/config/filters.json` 域）**：逐字段复算 45 条曲线，除 `Astronomik UV-IR Block L-2` 外在库条目与源文件逐字段一致；该条在库中**缺少**源文件具有的 `default` 字段（曲线数值未变，仅该键缺失）。

## 5 `eng/contracts/schemas/cpu_profile.schema.json`（迁移后：单一文件，两分支）

- **单一事实源**：定义以 `oneOf` 给出两个分支
  - `$defs.legacy_v1`：v1 形态（`schema_version=1`，`kernels` 数组，`hardware/build/memory_benchmark/verdict`）；
  - `$defs.profile_v2`：**当前实现** `schema="astrocs.cpu-profile/v2"`（`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp:561-613`、`verify_profile_v2` :648-710）。
- **绑定**：CPU（`host.vendor/family/model/stepping/xcr0`）· OS（`host.os_abi` ∈ {`linux`,`windows`}，见 §11）· 软件版本（`build.astrocs_version/source_commit/runtime_build_id`）· provider hash（`build.provider_build_ids`、`benchmark_binary_sha256`、`kernels.*.self_test_sha256`）· ISA/workers/block（`kernels.*.provider ∈ {baseline,avx2,avx512}`、`workers ≥ 1`、`block ≥ 1`）。身份绑定字段与 `check_profile_identity_v1`（`lib/infrastructure/benchmark/backend_host/cpu_routing.cpp:216-300`）逐项一致。
- **只有 benchmark 可写**：`x-astrocs-writer = "benchmark"`、`x-astrocs-not-writable-by = ["用户","cli --template","phase_config"]`；机器门 `TestCpuProfileMigration::test_writer_is_benchmark_only` + §7 的跨类不相交门。
- **兼容面（既有检查项不失效）**：顶层 `required` 取两分支共有键 `[build, kernels]`，顶层 `properties` 同时登记两分支顶层键，完整 v1 必填集落在 `$defs.legacy_v1.required`；`properties.kernels.items.required` 保持原访问路径（`eng/tests/backend/test_cpu_profile.py:74-79`、`eng/tools/validate_cpu_profile.py:41-70`）。
- **顶层 `properties.kernels.type = ["array","object"]`（双分支兼容）**：该取值同时容纳 v1 数组与 v2 对象，因此 `eng/tools/validate_cpu_profile.py:65` 的 `rule.get('type')=='array'` 分支对该字段不再生效；v1 kernel 项约束仍由本文件 `items.required` 与 `$defs.legacy_v1` 强制，并由负例 `cpu_profile_v1_missing_required.json` 复跑证明仍能红。
- **`host.os_abi` 值域冻结**：枚举 `{linux, windows}` = 生产者字面量集合（`lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp:259-265` 只写 `windows`/`linux`；`profile_gen_v2.cpp:571` 缺 `os` 字段时回落 `linux`），平台面由 `ENGINEERING_SPEC.md` §1（Windows amd64 / Linux amd64）封闭；非该集合一律 REJECT（负例 `eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json`）。登记面记 `x-astrocs-frozen-domains`。CFG002-ANCHOR: item4-os-abi-enum → eng/packaging/config/config_registry.json
- **kernel 接线**：`$defs.kernel_v1` 由 `legacy_v1.properties.kernels.items`、`$defs.kernel_v2` 由 `profile_v2.properties.kernels.additionalProperties` 各 `$ref` 一次（refs=1/1）；接线面与 `$defs` 逐字一致，由门 CFG002-10 按 `config_registry.cpu_profile_kernel_link` 的 refs 计数钉死，负例（`cpu_profile_v1_bad_kernel.json` / `cpu_profile_v2_bad_kernel.json`）必红。
- **v1 兼容口径（canonical 值 + 读取侧归一）**：v1 在 `hardware.feature_bits`、`hardware.xcr0`、`build.backend_sha256`、`kernels[].block_size`、`kernels[].oracle_status` 五处存在文本与生产值两义（`oracle_status` 读取侧大小写归一到 canonical 大写 PASS）；schema 按 canonical 值定义、按两义兼容读取，不新增也不删除既有检查项。

## 6 `eng/contracts/schemas/run_manifest.schema.json`

- 冻结源码 SHA（`software_sha`，40hex）· 配置哈希（`config_hash`，sha256 64hex）· 输入/输出哈希（`manifest_input_hashes`/`manifest_output_hashes`，逐项 path+sha256，minItems 1、uniqueItems）· 工具链版本（`toolchain_version` 必填，`toolchain_compiler`/`toolchain_cmake` 可选）· `run_id` · `created_utc`。
- 字段名族取自 DATA-001 锚点（`^run_id$`、`^software_sha$`、`^config_hash$`、`^manifest_(input|output)_hashes$`、`^toolchain_[a-z0-9_]+$`）；`additionalProperties:false` 拒绝 `workers`/`isa`/`block` 等（负例复跑）。`input_hashes` 与 DATA-OBJ-PROVENANCE-001.provenance.input_hashes 同义（锚点 note）。

## 7 机器门清单（复跑命令）

```bash
# 主门（零第三方依赖）
timeout 300 python3 -m unittest discover -s eng/tests/config -t eng/tests/config
# 单例：模板通过 schema / 负例必败
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/packaging/config/templates/normalize.phase_config.json
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/tests/config/fixtures/negative/hardware_fields_in_phase_config.json
```

| 门 | 断言 | 测试 |
|---|---|---|
| 三模板通过对应 schema | 逐模板 `validate()==[]`；phase 身份：mosaic/export 由 `phase_name`、normalize 由 `x-astrocs-phase` + 非空 `blocks[]` | `TestPhaseConfigFamily::test_templates_pass_their_schema` |
| 负例必败 | 未知滤镜/缺 output_dir/precision_mode 越界/硬件字段混入/多块与平铺互斥/块内未知键/逐帧 `inputs[]` 形态被拒，各自命中预期错误串 | `TestNegativeFixturesMustFail::test_negative_01..04`、`TestMultiBlockForm::test_03..06` |
| defaults 计数 | 字段数 == 带 unit 数 == 带 source 或 pending 数；来源不明 == 0 | `TestDefaultsContract::test_counts_and_no_unknown_source` |
| 转录保真 | 11 个关键 source 锚点 (文件:行:token) 逐行成立；pending 值必为 null | `test_every_source_ref_resolves_and_key_anchors_hold`、`test_pending_items_are_the_adjudicated_gap_set` |
| 跨类不相交 | phase_config ∩ cpu_profile(v2) == ∅；∩ run_manifest == ∅；∩ cpu_profile(全体) == {precision}（登记） | `TestCrossClassDisjointness` |
| 滤镜库 | 45/45 逐字一致 + provenance unverified/GAP-025 + 无零点键 + enum==库键 | `TestFiltersLibrary` |
| cpu_profile | 单一定义、benchmark-only、v1/v2 双分支可绿、缺必有字段可红 | `TestCpuProfileMigration`、`test_cpu_profile_v1_missing_required_still_fails` |
| CFG002-01 登记对应 | `docs/plugins/**` 配置表全集 ↔ `eng/packaging/config/config_registry.json#plugin_knobs` 一一对应（缺登记/多登记/默认值漂移/单位漂移/行号漂移/summary 撒谎均判红） | `check_cfg002_registry.py` CFG002-01 |
| CFG002-02 登记点可解析 | 每行登记点必须真实解析（defaults 键存在 / phase_config 指针落到属性 / `blocks[]` 项属性存在 / cpu_profile 指针存在 / 文档 文件:行 非空）；runtime_policy 与 resource_binding 进科学配置即红；phase_config 默认值必须 ∈ 目标 enum | 同上 CFG002-02 |
| CFG002-03 默认值→值域 | `fields[].enum_target` 指针落到含 enum 节点且 `enum_token` ∈ enum | 同上 CFG002-03 |
| CFG002-04 滤镜名语义 | `match=exact` / `case_sensitive=true` / `normalization=none` / `aliases={}`；三 schema enum == 库键；6 反例必拒、4 正例必过；`non_key_examples` 锚点成立；消费 filter 的 phase 面与登记一致 | 同上 CFG002-04 |
| CFG002-05 os_abi 值域 | schema enum == 生产者字面量集合 == 登记册；profile_gen_v2 回落字面量 ∈ enum；负例必拒、正例必过 | 同上 CFG002-05 |
| CFG002-06 module.yaml 键闭包 | 20 份 `lib/**/module.yaml` 顶层键 ⊆ 登记键集；出现 knobs/params/config/defaults 等旋钮声明键即红 | 同上 CFG002-06 |
| CFG002-07 索引归属 | `eng/packaging/config/**` 无 schema、`eng/contracts/config/**` 全 schema、两侧无同名文件；DOCUMENT_INDEX 中本文件恰一次且 ACTIVE_NORMATIVE；`eng/tests/test_index.csv` 登记 eng/tests/config 且未登记目录 ⊆ 已登记缺口清单 | 同上 CFG002-07 |
| CFG002-08 文档锚 | 本文件 5 条 `CFG002-ANCHOR:` 标记行恰一次且指向登记册 | 同上 CFG002-08 |
| CFG002-09 锚存活 | defaults 每个 `source_ref` 行存在且非空；`source` 的 path:line 提示 token 落在该行；paraphrase 例外清单只减不增 | 同上 CFG002-09 |
| CFG002-10 kernel 接线 pin | `$defs.kernel_v1`/`kernel_v2` 的 `$ref` 计数与 kernels 接线状态 == 登记册 | 同上 CFG002-10 |

## 9 旋钮与默认登记册 `eng/packaging/config/config_registry.json`（`astrocs.config-registry/v1`）

- **覆盖面**：`docs/plugins/*/*.md` 的配置项表**全集**逐行登记，一行一个 `(module, field)`，字段：`doc/line/declared_default/unit/owner_class/registration/registered_at/registered_key/finding/note/conflict`；行数与分组计数由登记册与机器门按登记行实时给出，本节不复制计数。
- **owner_class**（归属类，一行恰一个）：`science_param`（影响科学结果，必须有 SCI/ALG 条款或已登记配置类承载）· `runtime_policy`（运行期/实现/IO/观测策略，权威 = 插件文档或 algorithms/contracts 文档，**禁入 phase_config**）· `cli_surface`（命令行参数面）· `resource_binding`（线程/资源预算，cpu_profile 或调度器，禁硬编码、禁入 phase_config）。
- **finding**：`none`（已闭合）· `gap`（插件声明了默认值但无 SCI/ALG 权威、未进任何配置类）· `unregistered`（无默认且字段本身未登记）· `conflict`（与 SCI/ALG 权威或已登记配置冲突，必须带 `conflict.{kind,evidence,owner}`）。
- **分布**：行数与 `finding` / `owner_class` / `registration` 分组计数由门 CFG002-01 按登记行实时重算并与登记册比对；本节不复制计数。
- **冲突面**：登记册的 `conflict` 计数必须为 0（门 CFG002-01 判定）。争议项的现行权威取值：①`04_psf.psf_model` = **moffat4**（`docs/science/PSF.md:7,:81`）；②`08_drizzle.pixfrac` 默认 **0.8** 落 `defaults.json`；③`03_star_detection.detection_threshold` = **全局** `median(img)+5.0·bgnoise`（局部自适应为目标态）；④`06_photometry.flux_zero_point` 行**不存在**（`docs/science/algorithms/PHOTOMETRIC_FIT.md:9`）。
复跑判据 = 把任一插件文档的声明值改成与 `docs/science/` 权威不符时，该计数必须变正 ⇒ 判据非退化。
- **module.yaml 面**：20 份 `lib/**/module.yaml` 的顶层键只含契约/端口/构建/证据字段，**无任何旋钮或默认值声明字段**；门 CFG002-06 断言键集闭包——出现 `knobs/knob/params/parameters/config/configs/defaults/tunables/options/settings` 任一键即判红，新增旋钮声明必须先在本册登记归属。
- **维护规则**：本册是**人工维护**的登记面；文档变化必须由人重新判定归属。绿灯只出自人工判定（生成脚本批量刷新不构成归属判定，ENGINEERING_SPEC §8 fail-closed）；本册不复制科学数值（数值唯一源 = defaults.json / phase_config schema）。

## 10 滤镜名匹配语义（可执行规则 + 正反例）

```text
rule:   resolve(name) = filters[name] if name in keys(filters) else ERROR(unknown_filter)
        （lookup.match=exact、case_sensitive=true、normalization=none、aliases={}）
positive: "Baader R" | "Johnson V" | "SDSS r" | "OPTOLONG L-PRO Light Pollution"  -> 命中
negative: "bader r" | "bader v" | "baader r" | "BAADER R" | "Baader  R" | " Baader R" -> ERROR
```

- 三份 phase_config schema 的 `filter` 字段 enum 必须逐项等于库键（45），门同时断言 `enum == keys(filters)`。
- 正反例由门 CFG002-04 在每个消费滤镜的 phase（normalize/mosaic）上**双向**复跑：反例必须被拒、正例必须通过；`non_key_examples` 的 `where` 锚点必须指向**本文件 §10 负例表所在行**且该行必须真的含该串（最高设计不含这两个反例串，故锚点 = 本文件 §10 负例表所在行）。
- 归一化被**显式拒绝**（不是「暂未实现」）：`aliases` 为空对象即声明「无别名」；将来要支持别名必须先登记（登记=改合同，需权威条款 + 机器门 + 迁移说明）。

## 11 `cpu_profile.host.os_abi` 值域（冻结）

```text
enum:      { "linux", "windows" }
derivation: hardware_inspect.cpp:259-265（_WIN32 -> "windows"，否则 -> "linux"）
            profile_gen_v2.cpp:571（缺 os 字段回落 "linux"）
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
| 模块/CLI 契约 schema | `eng/contracts/config/**` | cli_modules_list / cli_selftest / module_dll_contract / module_lifecycle_contract | CFG002-07（全为 schema；与 eng/packaging/config/** 各自具名） |
| phase_config schema | `eng/contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json` | 三份 phase 专属；无聚合第二定义 | `test_no_aggregate_second_definition` |
| 文档事实源 | `docs/contracts/CONFIG_CONTRACT.md`（本文件） | 语义与索引 | CFG002-07（DOCUMENT_INDEX 恰一次 + ACTIVE_NORMATIVE） |
| 文档索引 | `docs/DOCUMENT_INDEX.yaml` | docs/** 与根治理文档 | `eng/tools/doccheck/check_doc_index.py` |
| 测试登记 | `eng/tests/test_index.csv` | eng/tests/** 子目录 | CFG002-07（登记 eng/tests/config；未登记集合 ⊆ 已登记缺口清单） |
| CI 注册表 | `eng/ci/checks.json` ↔ `docs/ci/01_CHECKS.md` §2 | 检查项双向对齐 | config 域的 `UT-CONFIG` 步骤登记在 CI 侧（正本 = `docs/ci/01_CHECKS.md` §2） |

CFG002-ANCHOR: item5-index-ownership → eng/packaging/config/config_registry.json
