# 配置合同

上游：最高设计的输入合同、三类配置、运行前预检、机器输出与退出码四章。

三类配置严格分离：阶段配置是科学参数与输入路径、可跨机复现；CPU 机器画像由 benchmark 独占写；
运行清单冻结本次运行。

本文是三类配置的字段、值域、默认来源、滤镜库语义与配置校验判据的正本。全文由三个互不重叠的配置面组成，每面各自声明作用域与键集合，判读任一面时以该面的作用域声明为准：

- **配置面一 · 三命令 phase_config 与三类配置**（本文件正本主体）：三命令阶段配置的键集合与值域、三类配置的分离与登记、滤镜库语义、默认登记册、配置校验判据。
- **配置面二 · orchestrator Stage2 配置**：orchestrator 阶段二配置文法与其解析缺省。作用域仅限 orchestrator Stage2 配置面，不是三命令 `phase_config` 合同，也不是生产写出侧的运行清单面。
- **配置面三 · 阶段生产调用链与 Stage1 配置**：阶段内生产入口到实现符号的登记与阶段一配置。作用域仅限编排调用链，不承担键集合与值域的声明。

三个配置面的键集合互不通用：在某一面的配置里出现另两面的键名即判错。

## 配置面一 · 三命令 phase_config 与三类配置

本块是三命令 `phase_config` 合同与三类配置字段面的正本。本块键集合 = 三份 `phase_config` schema 的属性并集，逐类字段面见本块各节；出现配置面二的键名（`inputs.hips`、`model`、`integration`、`sky_plane`）即判错。

### 权威链

| 事项 | 权威 |
|---|---|
| 输入合同（JSON 数据块：`blocks[]` 多块 + 平铺单块简写） | 最高设计的「输入合同」一节 |
| 运行前预检（绿/橙/红、error 强制阻断；**打印报错 + 详细预估；无 error 时须用户输入 `yes` 确认（stdin 确认，非 GUI 弹窗）**） | 最高设计的「运行前预检」一节（**三命令通用预检**：① 打印有没有报错 + ② 详细预估（含资源与磁盘预估）；确认 = stdin 输入 `yes`，`-y`/`-yes` 跳过确认、存在 error 时不可越过，`-force` 跳过整个检查步骤） |
| 配置挂载 / 模板 / 机器输出 / 退出码 | 最高设计的「机器输出与退出码」一节 |
| 三类配置严格分离、benchmark 独占 cpu_profile | `../../detail/common/unified_model.md` 的「3. 三类配置严格分离」一节 |
| CLI 预检与模板职责 | `../../detail/infrastructure/18_cli.md` 的「5. 配置项」一节 |
| 科学红线（默认容差不可改、cpu_profile 不进科学配置） | 最高设计 「权威链」一节（文档权威与索引） |
| 字段名族与跨类字段名锚点 | `eng/contracts/data/config_separation_anchors.json`（DATA-001，只读） |

### 三类配置（现场清单）

| 类 | 文件 | 写入者 | 语义 | 边界外字段 |
|---|---|---|---|---|
| phase_config | `eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；模板 `eng/packaging/config/templates/normalize.phase_config.json`、`eng/packaging/config/templates/mosaic.phase_config.json`、`eng/packaging/config/templates/export.phase_config.json` | 用户 / `cli --template` | 科学参数 · 输入路径 · output_dir · 算法选择；可跨机器复现 | 任何硬件字段（isa/isa_level/workers/worker_count/block/block_size/cpu_model/cpu_vendor/thread_budget/affinity_mask） |
| cpu_profile | `eng/contracts/schemas/cpu_profile.schema.json` | **仅 benchmark** | ISA/workers/block + CPU/OS/软件版本/provider hash 机器绑定 | phase_config 的 `sci_*`/`algorithm_*` 字段 |
| run_manifest | `eng/contracts/schemas/run_manifest.schema.json` | 运行时（每次运行冻结） | 源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本 | 科学参数与硬件调优字段 |

- **cpu_profile 落点口径与适用域（项目负责人裁决）**：落点 = 发行布局（程序安装目录）；输出路径相对发行布局解析，**源码树内运行构建期二进制不构成产品契约面**。安装目录不可写或落在版本控制工作树内时转用户可写落点（Linux `$XDG_DATA_HOME/ACSD`，Windows `%LOCALAPPDATA%\ACSD`）并在 stderr 明示，**不静默换落点、不把 profile 或原始样本写入版本控制工作树**。「运行产物不入版本控制工作树」是本块的守卫条款（与本文件「旋钮与默认登记册 `eng/packaging/config/config_registry.json`」一节同面）：守卫拒绝的是写入版本控制工作树，本条口径给出的是合法替代落点，两者不冲突。正本 = 最高设计的旋钮与默认登记册一节；接口登记 = 本文件的 `CLI_PROTOCOL.md` 「配置与 `output_dir`」一节；唯一实现 = `lib/infrastructure/cli/commands.cpp`（`cli_resolve_cpu_profile_path()`）与 `lib/infrastructure/benchmark/backend_host/profile_store.cpp`（原子写 + 校验）。

- phase_config 由**三份 phase 专属 schema** 定义：`eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json`；**不存在**同名聚合文件（唯一正本，`../../detail/common/unified_model.md` 的「3. 三类配置严格分离」一节）。

### `eng/packaging/config/defaults.json`（`acsd.config-defaults/v1`）

- 结构：`fields[]`，每项 `{key, value, unit, constraint, authority_status, source, source_ref, pending_task, note}`；
  字段总数与分组计数的权威 = `eng/packaging/config/defaults.json` 的 `field_count`，由校验判据实时对账；本节不复制科学数值。
  本文件**不含** `weight` 分组（`weight.default_mode` 及其 `enum_token`/`enum_target` 登记不在 `fields[]` 内）：权重是阶段二按该天球像素对应帧集合现场算出的派生量，全链没有「权重模式」这一可选概念（见本块「三命令 phase_config 与模板」一节的键集合声明）。
  `authority_status ∈ {sourced, owner_adjudicated, pending_authority}`。
- 校验判据：字段数 == 带 unit 数 == 带 source 或 pending_authority 数，来源不明字段 == 0；
  且**非 pending 字段的 unit 一律为具体单位**（`unspecified` 只属 pending 字段）；每个 `source_ref` 必须是**内容锚**
  `{id,path,quote,sha256,value_text}`——引文在目标文档内**唯一**命中（区分力自检）、指纹自洽、`value_text` 落在引文内。
  锚形态规则正本 = `../../detail/anchors/ANCHOR_CONTRACT.md` 的「4. 内容锚（content anchor）」一节；
  登记面与校验判据一律**不内嵌行号**：需要位置信息时用「锚 id + 内容指纹」。
- 分组与来源（只转录，不改数值）：

| 组 | 字段数 | 权威锚点 |
|---|---|---|
| calibration（暗场-亮场曝光容差） | 1 | 值 5、单位 **s**；科学判据 = `docs/science/calibration/CALIBRATION.md` 的「3.4 暗场与亮场的曝光容差：科学判据」一节（判据式 `|K·Δb| ≤ ε·σ_frame`，量纲逐项：`K` 无量纲、`Δb` ADU、`σ_frame` ADU、`ε` 无量纲）。本键是**预检面**（判定变量 `|t_light − t_dark|`，结论 🟠 warn 不阻塞），与该节的**科学面**判定变量不相关，**两者各自独立判定** |
| detection（σ 检测阈值） | 1 | `docs/science/detection/STAR_DETECTION.md` |
| psf（默认模型/β） | 2 | `docs/science/psf/PSF.md`（两处陈述） |
| noise（噪声模型默认配置） | 14 | **数值来源** = `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 的 `snr_noise_model_v1_default_config`（唯一实现，14 键的唯一取值处）；科学面 = `docs/science/noise_snr/NOISE_SNR.md` 的「3.1 背景方差面：空背景稳健方差」与「4 参数与常数」两节。**逐键标度类别（强制，封闭二分）**：① **承载 ADU 标度量的键只有 `noise.variance_floor`（ADU²）与 `noise.saturation_level`（ADU）**——两者标度必须与所作用数组同标度，数组为 `photo_scaled_adu` 时按 α²（方差）/ α（电平）换算，换算责任方 = 调用方；② 其余 12 键（`patch_grid`/`clip_sigma`/`min_patch_samples`/`max_clip_rounds`/`spatial_field_enabled`/`source_mask_radius_px`/`mask_radius_scale`/`mask_k_sigma`/`mask_r_min_px`/`mask_fwhm_floor_scale`/`mask_budget_min_patches`/`mask_budget_min_sky`）**不承载 ADU 标度量**——其量纲为像素几何长度 / 计数 / 无量纲比值，**标度不变**（对 α 免疫），不需要标度换算。标度词表 = `../standards/NUMERIC.md` |
| rejection（排异阈值表） | 18 | `../../science/REJECTION.md` 的「5 连续定义」与「2 符号表」两节 |
| photometry（mag_tolerance / Tukey c / IRLS / 最小星数） | 6 | `../../science/photometry/PHOTOMETRY.md` 的「4 输入有效域」「2 符号表」与 IRLS/Tukey biweight 记述三处 |
| precision（默认精度） | 1 | **无来源，需补充**：原指向的 `docs/science/unified/SCIENCE_SCOPE.md`「参数与常数」一节没有精度行，该文件全文不含精度条目；补源前该默认档不得当作已有权威背书 |
| upm（k_corr） | 1 | `../../science/sky/UPM.md`（定义 + 代码默认 1.4） |
| hips（tile 宽） | 1 | `../../science/PHASE3_HIPS_TO_FITS.md`（W 默认 512=2⁹） |
| drizzle（pixfrac） | 1 | 语义 `docs/science/drizzle/DRIZZLE.md`；**数值 0.8**（`defaults.json#drizzle.pixfrac`；通量守恒条件不变量见该文的条件不变量 FZ-COND-FLUX-CONSERV） |
| sparse_snr / scalar_gate | 3 | **数值 pending** |

- **权重面不设默认登记项**：权重是派生量，`fields[]` 中**不存在** `weight` 分组，也**不存在** `algorithm_weight_mode` / `weight_mode` / `legacy_allow_weight_fallback` 三个键名中的任何一个（`phase_config_mosaic.schema.json` 的 `description` 明确登记三套键名均不在本面键集合内）。权重口径 = 阶段二按天球像素对应帧集合现场算出的 `w = SNR²/F_ref² = 1/σ_F²`；判定键面见本块「三命令 phase_config 与模板」一节。

- **pending_authority 三项（值一律保持 null；取值须在 `docs/science/**` 给出数值来源后填入）**：

| key | unit | 取值来源 | 依据 |
|---|---|---|---|
| `sparse_snr.density` | `点/度²` | 待 `docs/science/**` 给出数值来源 | 最高设计的「输入合同」一节 点名（`defaults.json` 默认参数含「稀疏控制点间隔」）；`docs/science/**`、`docs/science/algorithms/**` 全库无数值；单位见 `docs/detail/registry/acsd.phase1.noise-snr.md` 的配置键表（`sparse_snr_spacing_px` / `sparse_snr_density` 两行）。**本键不承载生产取值**（生产用像素域 `sparse_snr.spacing_px`，默认 64 px）；两者是**同一几何量的两种表述**，换算恒等式（强制，两者取值由恒等式绑定）：`density[点/度²] = 1 / (Δ_px · s_pixel_scale[度/px])²`，其中 `s_pixel_scale = (180/π)·√(π/3)/nside`（IVOA REC-HIPS-1.0 的「瓦片像素角尺度」一节 定义的同名键单位为**度**）。换算责任方 = 任何同时登记两者的消费者；只声明其一即可 |
| `scalar_gate.rd` | unspecified | 待 `docs/science/**` 给出数值来源 | `defaults.json` 中该字段的 `source` 与 `source_ref` 均为空，仓内无可核验的字段名来源登记 |
| `scalar_gate.trend` | unspecified | 待 `docs/science/**` 给出数值来源 | 同上 |

- `calibration.dark_light_exposure_tolerance = 5 s`：**sourced**（值 5、单位 s）。**科学判据落 `docs/science/calibration/CALIBRATION.md` 的「3.4 暗场与亮场的曝光容差：科学判据」一节**（判据式、推导与两档反例齐备，证据面读数见该节）。**适用域**：本键只作**预检面**（判定变量 `|t_light − t_dark|`，🟠 warn、不阻塞、不参与科学可信判定）。**科学面判据的唯一承担者 = 该节的「3.4 暗场与亮场的曝光容差：科学判据」**——两者判定变量不相关，预检面判 PASS 不蕴含科学面残留达标（该节的反例读数见 `实验/` 证据面）。
- 负空间：`docs/detail/**` 的 plugin 级默认**不进** `fields[]`（本文件 source 规则限定 docs/science|docs/algorithms）；它们由 `eng/packaging/config/config_registry.json#plugin_knobs` 逐行登记（归属类 + 登记点 + 缺口/冲突），见本文件的「旋钮与默认登记册 `eng/packaging/config/config_registry.json`」一节。CFG002-ANCHOR: item1-plugin-defaults → eng/packaging/config/config_registry.json
- **默认值 → 字段值域的唯一登记**：`fields[].enum_target`（`{schema, pointer}`）+ `fields[].enum_token`。语义：defaults 里的「产品名/方法名」必须显式映射到承载字段的取值 token；校验判据断言 pointer 落到含 `enum` 的节点且 token ∈ enum。权重面无此登记（`fields[]` 内无 `weight` 分组，见上）。
- **登记册指针**：`registry_ref` 指向 `eng/packaging/config/config_registry.json`（plugin 级默认 / 旋钮归属 / 滤镜名语义 / os_abi / 索引归属的登记面）；本文件与登记册**各自只引用数值**（数值唯一源 = defaults.json 与 phase_config schema）。

### 三命令 phase_config 与模板

**本块键集合（三命令 phase_config 面）** = 三份 `phase_config` schema 的属性并集。本面键集合中**不存在** `algorithm_weight_mode`、`weight_mode`、`legacy_allow_weight_fallback`、`acr_route` 四个键名；在 `phase_config` 的任一分支给出这四个键之一即判错。权重是阶段二按该天球像素对应帧集合现场算出的派生量 `w = SNR²/F_ref² = 1/σ_F²`，全链没有「权重模式」这一可选概念（最高设计的「数据对象」一节；`eng/contracts/schemas/phase_config_mosaic.schema.json` 的 `description` 登记三套权重键名均不在键集合内）。

结构 = 最高设计的「输入合同」一节 的 JSON 数据块。**两种形态，互斥**：
① **多块形态**（normalize）= 顶层 `{schema_version, blocks[]}`，每块自带一组 `input_lights` + 一套母版 + 运行参数 + **块级 `output_dir`**；
   一块 = 一次运行（独立 `output_dir` / 独立 run manifest / 块级 `name` 归属），多块按序各自成一次运行；
② **平铺单块简写**（normalize 单块时的等价写法，向后兼容）= 顶层 `{schema_version, input_lights, master_*, output_dir, ...}`。
mosaic/export 仍为 `{phase_name, config, inputs}`（`config`/`inputs` 两级 `additionalProperties:false`）；normalize 的块内用 `additionalProperties:false`、顶层键闭包用 `propertyNames`（避免 `schema_version` 与 cpu_profile 同名键冲突，见本文件的「配置校验判据」一节 跨类不相交门）。
normalize 只接受**多块**与**平铺单块**两形态；CLI 遇到逐帧 `{phase_name, config, inputs[]}` 形态一律**具名拒绝**（退出码 3）。

| phase | 模板 | 必填 | 可选算法选择（逐项权威） | 输入项 |
|---|---|---|---|---|
| normalize（多块） | `eng/packaging/config/templates/normalize.phase_config.json` | 块级 `input_lights` + `output_dir`；顶层 `schema_version` + `blocks` | `algorithm_psf_model`（`docs/science/psf/PSF.md`，当前唯一实现 Moffat4）；`sparse_snr_layer`（最高设计的「输出合同」一节）；**`algorithm_drizzle_pixfrac`** 与 **`drizzle.pixfrac`**（语义/值域权威 `docs/science/drizzle/DRIZZLE.md` + `../../science/algorithms/DRIZZLE_GEOMETRY.md`，schema 机器强制 `0 < pixfrac <= 1`；数值默认 0.8 落 defaults.json 的 `drizzle.pixfrac`）；`drizzle.precision_mode`（0=FP32/1=FP64，**必须显式**） | 块级 `input_lights[]`（每帧一个 FITS 路径）+ 块级母版 `master_bias/master_dark/master_flat` + `filter_passband`（必须命中滤镜库；空串 = 显式无 filter）。**命中后的解析责任（强制）**：`resolve(name)` 必须按本文滤镜库一节的三条冻结条款执行——① 匹配语义（字节精确、无归一、无别名）；② **曲线身份核对**（对象 `name` / `n_points` / `curve_stats` 逐项对账，缺字段即 fail-closed）；③ **`channel` 非空校验**（`channel == ""` 的曲线只走不依赖通带类别的路径；`F_syn` 通带积分需要通带类别）。**取错通带的后果与判据**：通带错配表现为 `F_syn` 的跨星散度 ⇒ 单帧测光一致性残差是通带声明正确性的机器判据（正本 = science 分册的测光卷的通带公式一节，读数见其证据面）。**适用域**：本条只约束 `filter_passband` → 曲线的解析与消费，不改变滤镜库内容 |
| mosaic | `eng/packaging/config/templates/mosaic.phase_config.json` | 块级 `output_dir` + 块级 `hips_paths`（平铺单块简写 = `schema_version` + 同名两键；`{phase_name, config, inputs}` 形态的 `config` 键 = `{output_dir, precision}`） | `algorithm_rejection_method`（method/profile 词表与默认路由见 `../../science/REJECTION.md`）、`reject` 块（接受与 integration 段同一 `large_scale{enabled, min_structure_pixels, low_grow_radius_pixels, high_grow_radius_pixels}` 文法；三命令面缺省 enabled=1）、`algorithm_upm_gauge`（`docs/detail/registry/acsd.phase2.upm-fit.md` 的配置键表 `gauge_mode` 行）。权重是阶段二现场算出的派生量，本面**不存在** `algorithm_weight_mode` / `weight_mode` / `legacy_allow_weight_fallback` / `acr_route` 任一键名（见本节开头的键集合声明） | 块级 `hips_paths[]`（一组输入 HiPS）；`{phase_name, config, inputs}` 形态的 `inputs[]` 项 = `{product, filter?}` |
| export | `eng/packaging/config/templates/export.phase_config.json` | 块级 `output_dir` + 块级 `source`（一个输入产品）+ **`output_mode`（**必须显式**，缺键即 REJECT）**；平铺单块简写 = `schema_version` + 同名三键；`{phase_name, config, inputs}` 形态的 `config` 键 = `{output_dir, precision, output_mode, wcs}` | `wcs.projection`（最高设计的「投影算法」一节：首批 8 种、缺省 TAN；`docs/detail/registry/acsd.phase3.wcs.md` 的「投影集口径」条目（口径源自最高设计「投影算法」一节））、`wcs.{rotation_deg, crpix_px}`（同一 science 分册的投影卷）、**`crop`（导出裁剪范围，可选，缺省 = 不裁剪；在**已定义好的输出画幅**上取矩形子窗，不改画幅定义/投影/重采样）**：两种**互斥**形式 —— `pixels`（平面像素矩形，FITS 1-based 闭区间，用户到平面后手动裁剪）/ `sky`（天球轴对齐矩形 ICRS deg，GUI 的 HiPS 浏览器框选导出走这一形式）；键形固定可机器生成、GUI 直接填；判别键名取 `crop_form`（避开 `cpu_profile` 的 `mode`，统一模型卷的同名即同义条款，与 `output_mode` 同一处置）；fail-closed 全部具名报错（越界 / 宽高非正 / 两形式同时给 / 裁剪后为空 / `sky` 越出 TAN 半球），**禁静默夹取**；写出 FITS 的 WCS = 原画幅 WCS 在窗口上的**精确限制**（`CRVAL`/`CD` 逐位不变、`CRPIX` 减**整数**窗口原点）⇒ 窗口内像素与不裁剪时**逐位相同**。正本 = `docs/detail/export/pipeline.md`「导出裁剪范围（crop）」一节；字段合同 = `eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop`；几何唯一实现 = `lib/algorithms/projection/p3_wcs.h`（CLI 配置面与节点面共用）；接口登记 = `CLI_PROTOCOL.md` ；生产消费点 = scheduler p3 节点链 wcs/writer/verify（非死键）；`crop` 不进 `--template` 骨架（模板不替用户主张裁剪）；**`output_mode` 不在本列**（它**必填**：值域 `surface_brightness`/`point_source_flux`/`visualization`；合同登记的 `surface_brightness` **只作 `--template` 骨架值**，`--json` 运行的取值只走显式声明——见本节末条） | 块级 `source.hips_dir`（单值 = 一个输入产品；多产品 = 多块）；`{phase_name, config, inputs}` 形态的 `inputs[]` 项 = `{product}` |

- **落盘形态键 `storage_form`（Phase1 专属）**：Phase1 产品落盘形态由**输入配置**选定 —— 块级（或平铺顶层）`storage_form` ∈ {`archive`（默认）, `bare`}；**键缺失、空串或 `null` ⇒ 取默认 `archive` 并报一条 `level=warn` 事件**（取默认与 warn 事件成对出现），形态来源记入 `manifest.json#storage.form_source`；显式给出则不报 warn。键名与取值的唯一词表 = `eng/contracts/schemas/hips_storage_form.schema.json#x-acsd-field-vocabulary`；合同与不变式 F0–F4 与 M1–M4 的唯一正本 = `HIPS_STORAGE_FORM.md` 的「形态的输入配置与输出清单字段」一节（F0–F4 在该节的 normalize 输入配置键与输出清单两个子节，M1–M4 在其「运行完成清单 `manifest.json#storage`（加性）」子节）[6]；设计说明 = `../../detail/PRODUCT_STORAGE_FORM.md` 的「形态的输入配置与清单登记」一节。**mosaic / export 的输入合同不设该键**（Phase2 固定裸服务面、Phase3 固定裸 FITS 不套壳），出现即 REJECT —— schema 面（`additionalProperties:false` / `propertyNames`）与 CLI 面（`validate_config_full` 的块内未知键门）双双生效。生产写出侧消费点属分阶段实现计划阶段 2；CLI 当前只识别并透传，写出侧尚未消费该键。
- **索引引用键 `coverage_index`（Phase2 专属，加性可选）**：块级（或平铺顶层）`coverage_index` 是字符串路径，指向数据集级 `coverage.index.json`；存在 ⇒ 阶段二载入它做块级查询，缺失 ⇒ 规定回退 = 读入全部产品级索引现场倒排。`hips_paths` 的元素**保持字符串**（不做元素对象化），逐帧产品级索引路径由命名规则派生（`<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`）。生产消费点同属阶段 2。
- **精度显式声明**：mosaic/export 用位深键显式声明（fp32/fp64；模板填 `fp64`）；normalize 用块级 `drizzle.precision_mode`（0=FP32 / 1=FP64，**必须显式**，缺失即拒绝）。CLI 精度键集只有上述键：键集外字段（含 `config.precision` 形态）一律拒绝（键集权威 = 最高设计的「输入合同」一节）。逐对象应归属档位的唯一正本 = `../UNIFIED_OBJECTS.md` 的对象对照表精度列（其判定规则与未决项见该表读法段）。**默认 FP64 的数值来源需补充**：`docs/science/unified/SCIENCE_SCOPE.md` 全文不含精度条目，其「参数与常数」一节没有精度行，本文件与 `../standards/NUMERIC.md` 的默认档声明在仓内**没有可核的数值源**；补源前该默认值不得当作已有权威背书。
- **精度的科学不变量（强制，跨三命令同面）**：精度选择只改变**表示误差**，任何科学量的**标度类别**（`../standards/NUMERIC.md` 标度词表）与**量纲**保持不变。判据（非退化）：同一输入在 fp32 与 fp64 下，各产品面的标度类别与单位串必须逐项相同；数值差异只允许落在该面事前冻结的浮点容差内（通用浮点容差与 NaN/Inf 语义的唯一正本是 `../testing/TEST.md`，本合同不复述其数值[7]）。**标度换算与精度无关**；`BUNIT` 与缺失编码在 fp32/fp64 下取值相同。三命令的精度键名不同（`precision` / `drizzle.precision_mode`）但**语义与不变量同面**；键名差异不构成两套口径。
- export 几何字段名与值域取自科学权威：`center_deg`/[`s_out_deg`]/`width_px`/`height_px`（`../../science/PHASE3_HIPS_TO_FITS.md` 的「2 符号表」一节：W/s_out/center/W_out,H_out）；约束 `../../science/PHASE3_HIPS_TO_FITS.md` 的「4 输入有效域」一节：abs(dec) ≤ 85°、W_out/H_out ∈ [1,20000]、s_out > 0）。原 `projection` 插件文档的 `mode` 在 phase_config 中命名为 `output_mode`，以避开 legacy cpu_profile v1 的 `mode` 字段名（统一模型卷的同名即同义条款，见本文件的「配置校验判据」一节 门表）。
- **`output_mode` 的必填与默认值口径（三条分支同面，fail-closed）**：
  - **必填且必须显式**：`{phase_name, config, inputs[]}` 形态（`$defs.export_config.required`）、`blocks[]` 形态
    （`$defs.export_block.required`）与平铺单块简写形态的 `required` **都含 `output_mode`**
    ⇒ **三条分支一致 fail-closed**；运行期缺键即 REJECT（`FZ-P3-MODES`；`lib/infrastructure/cli/session_commands.h` 的
    `config_fields(SESSION_EXPORT)` 同面）——**合同面 ≥ 运行期面（同严或更严）**。
  - **合同登记的 `surface_brightness` 不是「运行默认值」**：它只用于 `<cmd> --template` 的**骨架值**
    （`eng/packaging/config/templates/export.phase_config.json`；`eng/packaging/config/config_registry.json` 的 `export.config.output_mode` 登记点）。
  - 因此本表 export 行的「可选算法选择」列**不列 `output_mode`**（它只属「必填」列）。
- **模板示例值声明**：模板中的 `path/to/...` 路径、`filter: "Baader R"`、export 的 `center_deg: [0,0]` 与 `s_out_deg: 0.001`、`width_px/height_px: 512` 都是**示例占位**（用户必须按观测改写），**不是**科学默认值；**示例滤镜串必须始终是滤镜库的合法键**（`"Baader R"` 是合法键；改成库外串会让模板自身通不过 schema 枚举 ⇒ 模板回归必红）——示例值不主张「本次观测的真实通带就是它」，通带声明正确性由本文件的滤镜库一节的曲线身份核对与测光残差判据承担；除 `precision`/`projection`/`rotation_deg` 等有权威默认者外，模板不主张任何数值默认（**`output_mode` 不在其列**：它**必填**，模板骨架值 `surface_brightness` 只是让模板可直接运行，不是「缺省可用」的许可，见上条）。`width_px/height_px = 512` 与 HiPS tile 默认（`../../science/PHASE3_HIPS_TO_FITS.md`）同值，仅作可运行的示例几何。
- 硬约束：normalize 块内（`additionalProperties:false`）、`drizzle`/`wcs` 两级与顶层 `propertyNames` 都拒绝任何未登记字段；mosaic/export 的 `config`/`inputs` 两级 `additionalProperties:false` ⇒ cpu_profile 的 `workers`/`isa`/`block_size` 混入必失败（负例 ④）。
- 内存/流式预算类字段（`docs/detail/registry/acsd.phase3.writer.md` 配置键表的 `band_height`/`tile_cache_mb`）**不进** phase_config：它们不可跨机器复现，属实现策略，按 `../../detail/common/unified_model.md` 的「3. 三类配置严格分离」一节 只入 `runtime_policy` 面。登记面把它们登记为 `runtime_policy` 类（权威 = 插件文档；校验判据断言此类旋钮只出现在 `runtime_policy` 面（phase_config 属性面零命中）），见本文件的「旋钮与默认登记册 `eng/packaging/config/config_registry.json`」一节。CFG002-ANCHOR: item2-knob-ownership → eng/packaging/config/config_registry.json

### `eng/packaging/config/filters.json`（`acsd.filter-library/v1`）

- **逐字转录** `lib/algorithms/photometry/data/response_curves/filters.json`（45 条；字段 `name/channel/wavelength_nm/value/n_points`），未重采样、未插值、未改数值。**`channel` 是转录字段之一**，因此其角色与空值语义按本节下文的冻结条款判定（见「`channel`（通带类别列）的角色与空值语义」）；转录保真只保证「值与源文件逐字相同」，**不**主张曲线物理正确（provenance 见下条）；校验项逐条与源文件比对（`TestFiltersLibrary::test_verbatim_transcription`）。
- **provenance**：登记在本文件自己的 `eng/packaging/config/filters.json` 的 `provenance` 面（`source_statement` 逐字为 `unverified: original curve source not recorded in repository`，另记 `status=unverified` 与 `gap_id=GAP-025`；原外部 provenance 源文件已不在现行文件树内，其内容指纹留在该面的 `source_sha256`），本库 **45/45 如实标注** `status=unverified`、`verified=false`、`url=null`、`gap_id=GAP-025`；曲线统计量（`curve_stats`）由校验项与曲线逐条对账。**适用域与验证方式（严格自洽说明）**：曲线数据只以「与源文件逐字相同的转录」身份入库，其物理正确性未经外部来源核对；任何「与标准滤光片/文献一致」的主张必须另附独立可核验出处（原始曲线来源 + 版本 + 获取方式 + 校验和）。**缺口登记**：原始曲线来源未入库，登记在册（`gap_id=GAP-025`，登记面 = `eng/packaging/config/config_registry.json`）。
- **未知滤镜 → error**：`lookup.unknown_filter = "error"`（最高设计的「输入合同」一节 滤镜型号逐字匹配、红级 error）；三份 phase_config schema 都保留 45 键 `$defs.filter_name` 枚举（normalize 由块级 `filter_passband` 消费：`anyOf[{const:""}, {$ref: filter_name}]`；mosaic 由 `inputs[].filter` 消费），并由配置校验项锁定 `enum == eng/packaging/config/filters.json 的 filters 键`（`test_filter_enum_equals_library_keys`）。
- **不含每滤镜零点**（合同口径）：零点 `location` 是**逐次运行估计量**且满足**零点平移不变量**——`../../science/photometry/PHOTOMETRY.md`（估计零点 location、尺度因子 scale…）与 `../../science/photometry/PHOTOMETRY.md` 的零点平移不变量条款：F_instr 同乘 k ⇒ location 增 log10 k，scale 除 k，sigma_residual 不变）。**适用域（正向约束）**：该不变量成立的条件是 `r_i = log10(F_instr,i/F_syn,i)` 取**同一 scale 口径**（同一参考通量单位与同一 `F_syn` 定义）且 `k` 是**纯乘性**变换（线性区，无饱和/无截断）。跨运行比较 `location` 时，`F_syn` 口径变化会整体平移 `location` ⇒ **数值不可跨运行直接比较**；量纲与换算见 `docs/science/unified/DATA_SEMANTICS`  的量纲条；测光拟合算法卷明确 `zero_point` 字段因「无定义式、结构体无字段」被删除。故滤镜库**不出现**任何零点列/占位/null 字段；校验项 `test_no_zero_point_column_anywhere` 对**键路径与全文**双向断言（大小写不敏感）。若将来需要每滤镜零点，属**新科学定义**，须走变更流程另立条款。
- **匹配语义（冻结）**：`lookup = {unknown_filter: error, match: exact, case_sensitive: true, normalization: none, aliases: {}}` ⇒ `resolve(name) = filters[name] if name ∈ keys(filters) else ERROR(unknown_filter)`；不做大小写/空白/Unicode 归一，不解析别名（`aliases` 为空对象 = 显式声明「本库不解析任何别名」）。冻结理由：库内品牌大小写不一致（`Optolong B/G/R` 与 `OPTOLONG L-PRO Light Pollution` 并存），品牌级归一化会引入歧义；当前 45 键在「大小写 + 空白」折叠下无重名（校验项断言），但该事实不替代显式规则。
- **曲线身份核对（装配期 fail-closed，冻结）**：`resolve(name)` 命中键后，装载器**必须**再核对「取到的曲线对象就是该键声明的曲线」，四项同时成立才算通过：① 对象 `name` 字段与库键**逐字节相等**；② 对象 `n_points` 与实际 `wavelength_nm` 数组长度相等；③ 对象自述与 `provenance.per_filter[<键>].curve_stats` 的 `n_points`/`wl_min`/`wl_max`/`val_min`/`val_max` 一致；④ 同上的 `wl_sum`/`val_sum`/`val_sumsq`（逐元素和）与**实际数组**算出的值一致。任一项不成立 ⇒ 具名错误 `curve_identity_mismatch`（作用域 = 环境/配置），装载器只返回该具名错误。
  - ③ 与 ④ 的分工（冻结）：`min`/`max` 只是包络，两支不同曲线可以共用同一包络 ⇒ ④ 的逐元素指纹是**必需项**，`curve_stats` 缺这三个字段即 fail-closed（缺字段 ≠ 身份已核对）。
  - 该核对**不改变** `lookup` 语义（仍是字节精确、无别名），也不放宽任何科学阈值；它约束的是「解析器有没有取错对象」。
  - 消费侧另有两条独立约束：配置声明的通带名与 `FILTER` 关键字解析出的库键必须相等（不等 ⇒ `passband identity mismatch`）；同一次运行内各组帧只能有一条模型通带（不一致帧判 `PHOT_PASSBAND_IDENTITY_INCONSISTENT`）。
  - 判据锚：`lib/infrastructure/pipeline/orchestrator/cpp/tests/test_photometry_curve_resolve.cpp`（`[I0..I9]`，含恒真自检）与 `test_p1phot_passband_identity.cpp`（`[F1..F6]`）（两个路径在跟踪集内不存在，登记为需代码侧订正）。正本 = `../../science/photometry/PHOTOMETRY.md` 的「2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）」一节。
- **反例串的权威锚点**：`"bader r"`/`"bader v"` 的**唯一权威锚点 = 本文件 「滤镜名匹配语义」一节 负例表**（`eng/packaging/config/filters.json#lookup.non_key_examples[].where` 指向它）。最高设计全篇不含 `bader`，因此这两个串的锚点只落在本文件 「滤镜名匹配语义」一节 负例表。两个串仍登记为 `lookup.non_key_examples`（`kind=design_doc_negative_example`、`resolution=ERROR`，值与判据未变）——它们不是合法库键，也不能靠归一化变成合法键（`bader` ≠ `Baader`；且库内不存在 Baader V 曲线）。`non_key_examples` 只把反例与合法值分开，**不**把反例升级为别名。正反例与门见 「滤镜名匹配语义」一节。CFG002-ANCHOR: item3-filter-name-policy → eng/packaging/config/config_registry.json
- **死定义已登记**：三份 schema 都保留 `$defs/filter_name`；被字段 `$ref` 的只有 normalize（块级 `filter_passband`，后取代逐帧 `inputs[].filter`）与 mosaic（`inputs[].filter` 可选、模板未用）；export 无字段引用它（导出以 HiPS 产品为单位、滤镜在上游分离）⇒ `config_registry.filter_name_policy.dead_filter_enum_phases` 登记，校验项断言该集合的每次变化都显式登记。
- **`channel`（通带类别列）的角色与空值语义（冻结）**：最高设计的「输入合同」一节 冻结 `filters.json` 承载**三列**——「型号、通带、波长」；`channel` 即其中的**通带类别**列，与 `wavelength_nm`（波长轴，nm）配合。本文件**不新增取值**，只冻结其角色与空值语义：
  - 值域 = `{B,G,R,L,HA,OIII,PAN,""}`；空值组 = `Johnson I`、`Johnson U`、`SDSS g`、`SDSS i`、`SDSS r`、`SDSS u`、`SDSS z`。**复算方法（就地，不依赖外部目录）**：读 `eng/packaging/config/filters.json`，对每个滤镜条目取 `channel` 字段
（缺字段按 `""` 计），按取值分组计数；判据 = 按上述方法就地复算出的分组计数与空值组型号清单恒与 `eng/packaging/config/filters.json` 一致，空值组恰为列出的 7 个型号名。
反例（负对照）：把任一空值条目的 `channel` 填成合法类别后重跑，计数与空值清单必须同时变化 ⇒ 判据非退化。
  - **空值语义（冻结）**：`""` = **未声明通带类别**（≠「无通带」、≠「宽带」）。
  - **解析器的校验责任（冻结）**：`resolve(name)` 在名字命中后**必须**再校验 `channel` 非空；`channel == ""` 的曲线只走不依赖通带类别的路径：判定、筛选、换算与 `F_syn` 通带积分都需要通带类别 ⇒ **必须 fail-closed**（具名报错），通带类别只取自 `channel` 列。
  - 适用域：本条只约束 `filter_passband` → 曲线的解析与消费；**不改变**已冻结的 `lookup = {unknown_filter: error, match: exact, case_sensitive: true, normalization: none, aliases: {}}` 语义。
- **通带正确性的证据资格与判据锚（冻结）**：
  - 本库全部曲线 `status=unverified`、`verified=false`、`url=null`（见上一条的出处声明）⇒ 「与标准滤光片/文献一致」的主张只走独立可核验出处（原始曲线来源 + 版本 + 获取方式 + 校验和，见本册的旋钮归属登记）；本库曲线本身只承担曲线数据。
  - **判据锚（可执行）**：通带错配的后果是 `F_syn` 的跨星散度；正本与四类证据 = `../../science/photometry/PHOTOMETRY.md` 的「2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）」一节（刻度实证读数见该节所引的 `run/SCI-PHOT-FORMULA-01/evidence/` 证据面；`SCI-PHOT-FORMULA-01` 是该实验单元的目录名，不是条款号）。⇒ **单帧测光一致性残差是通带声明正确性的机器判据**：`channel` 未声明却按通带消费、或曲线取错时，该残差判红。
  - **转录偏差（按行登记，修复归 `eng/packaging/config/filters.json` 域）**：逐字段复算 45 条曲线，除 `Astronomik UV-IR Block L-2` 外在库条目与源文件逐字段一致；该条在库中**缺少**源文件具有的 `default` 字段（曲线数值未变，仅该键缺失）。

### `eng/contracts/schemas/cpu_profile.schema.json`（单一文件，两分支）

- **单一事实源**：定义以 `oneOf` 给出两个分支
  - `$defs.legacy_v1`：v1 形态（`schema_version=1`，`kernels` 数组，`hardware/build/memory_benchmark/verdict`）；
  - `$defs.profile_v2`：**当前实现** `schema="acsd.cpu-profile/v2"`（`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp`，`verify_profile_v2` 同文件）。
- **绑定**：CPU（`host.vendor/family/model/stepping/xcr0`）· OS（`host.os_abi` ∈ {`linux`,`windows`}，见 「cpu_profile.host.os_abi` 值域」一节）· 软件版本（`build.acsd_version/source_commit/runtime_build_id`）· provider hash（`build.provider_build_ids`、`benchmark_binary_sha256`、`kernels.*.self_test_sha256`）· ISA/workers/block（`kernels.*.provider ∈ {baseline,avx2,avx512}`、`workers ≥ 1`、`block ≥ 1`）。身份绑定字段与 `check_profile_identity_v1`（`lib/infrastructure/benchmark/backend_host/cpu_routing.cpp`）逐项一致。
- **只有 benchmark 可写**：`x-acsd-writer = "benchmark"`、`x-acsd-not-writable-by = ["用户","cli --template","phase_config"]`；校验项 `TestCpuProfileMigration::test_writer_is_benchmark_only` + 本文件的「配置校验判据」一节 的跨类不相交门。
- **兼容面（既有检查项不失效）**：顶层 `required` 取两分支共有键 `[build, kernels]`，顶层 `properties` 同时登记两分支顶层键，完整 v1 必填集落在 `$defs.legacy_v1.required`；`properties.kernels.items.required` 保持原访问路径（`eng/tests/backend/test_cpu_profile.py`、`eng/tools/validate_cpu_profile.py`）。
- **顶层 `properties.kernels.type = ["array","object"]`（双分支兼容）**：该取值同时容纳 v1 数组与 v2 对象，因此 `eng/tools/validate_cpu_profile.py` 的 `rule.get('type')=='array'` 分支对该字段不再生效；v1 kernel 项约束仍由本文件 `items.required` 与 `$defs.legacy_v1` 强制，并由负例 `cpu_profile_v1_missing_required.json` 复跑证明仍能红。
- **`host.os_abi` 值域冻结**：枚举 `{linux, windows}` = 生产者字面量集合（`lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp` 只写 `windows`/`linux`；`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp` 缺 `os` 字段时回落 `linux`），平台面由 最高设计 「cpu_profile.host.os_abi` 值域」一节（双平台发行）（Windows amd64 / Linux amd64）封闭；非该集合一律 REJECT（负例 `eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json`）。登记面记 `x-acsd-frozen-domains`。CFG002-ANCHOR: item4-os-abi-enum → eng/packaging/config/config_registry.json
- **kernel 接线**：`$defs.kernel_v1` 由 `legacy_v1.properties.kernels.items`、`$defs.kernel_v2` 由 `profile_v2.properties.kernels.additionalProperties` 各 `$ref` 一次（refs=1/1）；接线面与 `$defs` 逐字一致，由门 CFG002-10 按 `config_registry.cpu_profile_kernel_link` 的 refs 计数钉死，负例（`cpu_profile_v1_bad_kernel.json` / `cpu_profile_v2_bad_kernel.json`）必红。
- **v1 兼容口径（canonical 值 + 读取侧归一）**：v1 在 `hardware.feature_bits`、`hardware.xcr0`、`build.backend_sha256`、`kernels[].block_size`、`kernels[].oracle_status` 五处存在文本与生产值两义（`oracle_status` 读取侧大小写归一到 canonical 大写 PASS）；schema 按 canonical 值定义、按两义兼容读取，不新增也不删除既有检查项。

### `eng/contracts/schemas/run_manifest.schema.json`

**本节描述的是 `eng/contracts/schemas/run_manifest.schema.json` 冻结的 run_manifest 类字段名族（预留合同面）**，字段集 = `manifest_schema`/`run_id`/`software_sha`/`config_hash`/`manifest_input_hashes`/`manifest_output_hashes`/`toolchain_version`/`toolchain_compiler`/`toolchain_cmake`/`created_utc` + 加性可选键 `storage`，且 `additionalProperties:false`。

**显式登记（两个不相交的 run manifest 对象）**：该 schema **未被任何生产写出点消费**。生产写出点是 `lib/infrastructure/cli/commands.cpp` 的 `write_run_manifest()`，它写的是**另一套字段名族**——`schema_version`/`kind`(`acsd_run_manifest`)/`run_id`/`acsd_version`/`platform`/`config_path`/`config_sha256`/`cpu_profile_path`/`cpu_profile_sha256`/`phases`/`artifacts`/`status`/`started_utc`/`finished_utc`/`summary`（失败时另加 `error`，节点级科学事实经 `extra` 并入），落盘为 `acsd_run_<run_id>.json`，由同文件的 `inspect`/`resume` 读回。该生产字段名族除 `run_id` 外**逐字段**落在本 schema 的 `additionalProperties:false` 拒绝面内。

⇒ 「run manifest」在本仓存在**两个不相交的对象**：本节的 schema 预留字段名族，与生产写出点的运行清单字段名族。两套词表**不合并、不互相改写**；哪一套是正本需负责人裁决（UNRESOLVED，见交付审核包）。

- 字段名族取自 DATA-001 锚点（`^run_id$`、`^software_sha$`、`^config_hash$`、`^manifest_(input|output)_hashes$`、`^toolchain_[a-z0-9_]+$`）；`additionalProperties:false` 拒绝 `workers`/`isa`/`block` 等。`input_hashes` 与 DATA-OBJ-PROVENANCE-001.provenance.input_hashes 同义（锚点 note）。**加性可选键 `storage`**：运行级落盘形态事实，**不属**上列哈希/版本字段名族，已在锚点 `config_classes[run_manifest].additive_field_names` 登记；**条款归属** = `HIPS_STORAGE_FORM.md` 的「运行完成清单 `manifest.json#storage`（加性）」一节（字段词表与不变式 M1..M4 的唯一正本，该组条款所在的大节是其上的「形态的输入配置与输出清单字段」一节）[6]，**唯一机器事实源** = `eng/contracts/schemas/hips_storage_form.schema.json#/$defs.manifest_storage`——本 schema 故意**不复写**字段（只登记键位与 `type:object`），避免同名异型；缺失 ⇒ 无形态事实（不判红）。

### 配置校验判据（当前无机器执行器，判据由人读）

**本节门表在本仓没有执行器。** 核对：`eng/tests/` 下只有 `conformance/` 与 `validation/` 两个目录，其中不含配置合同测试；`eng/tools/` 下无 `config_consistency_check.py`；配置合同的门脚本 `check_cfg002_registry.py` 与测试 `test_cfg001_contracts.py`/`test_cfg002_registry.py` 在跟踪集内不存在。运行期依赖 `jsonschema` 的模板/schema 门亦无执行器（该依赖不在仓内）。⇒ 下表列出的是**应门禁的判据**，不是可复跑的命令；逐条核对判据由人读执行，登记为需代码侧订正项。

下表中的测试 ID 与脚本路径为判据的既有标识，仓内无对应实现，改动判据时同步更新本节与登记面。

**可复现轮 T05–T10 诚实化（CONFIG 门表 2 项）**：

- T-CFG-01（门表全部 20 行：三模板过 schema / 负例必败 / defaults 计数 / 转录保真 / 跨类不相交 / 滤镜库 / cpu_profile / CFG002-01..12）：**不可验收（缺复现载体）**。本节已明示门表在本仓没有执行器（`eng/tests/` 下只有 `conformance/` 与 `validation/` 两个目录，其中不含配置合同测试；`eng/tools/` 下无 `config_consistency_check.py`；门脚本 `check_cfg002_registry.py` 与测试 `test_cfg001_contracts.py`/`test_cfg002_registry.py` 在跟踪集内不存在；运行期 `jsonschema` 依赖不在仓内）。下表列出的是应门禁的判据，不是可复跑的命令；逐条核对判据由人读执行，登记为需代码侧订正项。实验单元指向：配置合同执行器单元（待建：门脚本 + 测试文件 + jsonschema 依赖落地）。
- T-CFG-02（转录保真 11 个关键 source 锚点）：**不可验收（缺复现载体，人工可核）**。与 T-CFG-01 同一缺口；11 个锚点（文件 + 值文本 + 内容指纹）可由人读逐条核对，但无机器复跑入口，不得写成已生效判据。改动判据时同步更新本节与登记面。

| 门 | 断言 | 测试 |
|---|---|---|
| 三模板通过对应 schema | 逐模板 `validate()==[]`；phase 身份：mosaic/export 由 `phase_name`、normalize 由 `x-acsd-phase` + 非空 `blocks[]` | `TestPhaseConfigFamily::test_templates_pass_their_schema` |
| 负例必败 | 未知滤镜/缺 output_dir/precision_mode 越界/硬件字段混入/多块与平铺互斥/块内未知键/逐帧 `inputs[]` 形态被拒，各自命中预期错误串 | `TestNegativeFixturesMustFail::test_negative_01..04`、`TestMultiBlockForm::test_03..06` |
| defaults 计数 | 字段数 == 带 unit 数 == 带 source 或 pending 数；来源不明 == 0 | `TestDefaultsContract::test_counts_and_no_unknown_source` |
| 转录保真 | 11 个关键 source 锚点（文件 + 值文本 + 内容指纹）成立；全部 `source_ref` 内容锚存活；pending 值必为 null | `test_every_source_ref_resolves_and_key_anchors_hold`、`test_pending_items_are_the_adjudicated_gap_set` |
| 跨类不相交 | phase_config ∩ cpu_profile(v2) == ∅；∩ run_manifest == ∅；∩ cpu_profile(全体) == {precision}（登记） | `TestCrossClassDisjointness` |
| 滤镜库 | 45/45 逐字一致 + provenance unverified/GAP-025 + 无零点键 + enum==库键 | `TestFiltersLibrary` |
| cpu_profile | 单一定义、benchmark-only、v1/v2 双分支可绿、缺必有字段可红 | `TestCpuProfileMigration`、`test_cpu_profile_v1_missing_required_still_fails` |
| CFG002-01 登记对应 | `docs/detail/**` **配置表**（表头白名单 = `字段|默认|单位|说明`、`字段|默认|说明`、`字段|说明`）全集 ↔ `eng/packaging/config/config_registry.json#plugin_knobs` 一一对应（缺登记/多登记/默认值漂移/单位漂移/**表行内容指纹漂移**/登记行内嵌行号/summary 撒谎均判红）；位置由内容锚实时解析，不再断言行号 | `check_cfg002_registry.py` CFG002-01 |
| CFG002-02 登记点可解析 | 每行登记点必须真实解析（defaults 键存在 / phase_config 指针落到属性 / `blocks[]` 项属性存在 / cpu_profile 指针存在 / 文档路径存在且带内容锚）；登记点内嵌行号即红；runtime_policy 与 resource_binding 进科学配置即红；phase_config 默认值必须 ∈ 目标 enum | 同上 CFG002-02 |
| CFG002-03 默认值→值域 | `fields[].enum_target` 指针落到含 enum 节点且 `enum_token` ∈ enum | 同上 CFG002-03 |
| CFG002-04 滤镜名语义 | `match=exact` / `case_sensitive=true` / `normalization=none` / `aliases={}`；三 schema enum == 库键；6 反例必拒、4 正例必过；`non_key_examples` 锚点成立；消费 filter 的 phase 面与登记一致 | 同上 CFG002-04 |
| CFG002-05 os_abi 值域 | schema enum == 生产者字面量集合 == 登记册；profile_gen_v2 回落字面量 ∈ enum；负例必拒、正例必过 | 同上 CFG002-05 |
| CFG002-06 module.yaml 键闭包 | 20 份 `lib/**/module.yaml` 顶层键 ⊆ 登记键集；出现 knobs/params/config/defaults 等旋钮声明键即红 | 同上 CFG002-06 |
| CFG002-07 索引归属 | `eng/packaging/config/**` 无 schema、`eng/contracts/config/**` 全 schema、两侧无同名文件；DOCUMENT_INDEX 中本文件恰一次且 ACTIVE_NORMATIVE | 同上 CFG002-07 |
| CFG002-08 文档锚 | 本文件 5 条 `CFG002-ANCHOR:` 标记行恰一次且指向登记册 | 同上 CFG002-08 |
| CFG002-09 内容锚存活 | defaults 每个 `source_ref` 形态完整（`id/path/quote/sha256/value_text`）；引文在目标文档内唯一命中（0 次 = 锚不成立，≥2 次 = 无区分力）；指纹自洽；`value_text` 落在引文内；`source` 描述内嵌 文件:行 即红；`defaults_anchor_exceptions.paraphrase` 非空即红（内容锚形态下无豁免面） | 同上 CFG002-09 |
| CFG002-10 kernel 接线 pin | `$defs.kernel_v1`/`kernel_v2` 的 `$ref` 计数与 kernels 接线状态 == 登记册 | 同上 CFG002-10 |
| CFG002-11 科学锚 | `contract_doc_citations.token_anchors` 每条 = 内容锚（`{id,path,quote,sha256,value_text}`，零行号）：引文唯一命中、`value_text` 落在引文内、本文件在**文件级**仍引用该 path | 同上 CFG002-11 |
| CFG002-12 锚形态与区分力 | ① 登记面递归扫描**零 文件:行**；② 四个锚面（defaults/token_anchors/filters.non_key_examples/declared_negatives）全过内容锚判据；③ `plugin_knobs[].anchor` = 「模块.字段」复合键路径 + 16 位指纹；④ `retired_knobs` 行**只减不增**（复活即判红：既不在 `plugin_knobs` 也不在文档配置表）；⑤ 非白名单表头的 `字段` 表必须显式登记在 `non_config_tables`（悬空声明亦判红） | 同上 CFG002-12 |

### 旋钮与默认登记册 `eng/packaging/config/config_registry.json`（`acsd.config-registry/v1`）

- **覆盖面**：`docs/detail/*/*.md` 的**配置项表**（表头白名单见 门表 CFG002-01）**全集**逐行登记，一行一个 `(module, field)`，字段：`doc/declared_default/unit/owner_class/registration/registered_at/registered_key/finding/note/conflict/anchor`；`anchor = {id, sha256}` 是**内容锚**（`id` = 复合键路径 `模块.字段`，`sha256` = 对应文档表行按 `../governance/DOCUMENT_GOVERNANCE.md` 的「锚纪律」一节归一化后的内容指纹），**位置不入册**——行号由锚实时解析，文档重排不再让登记失效，而表行**内容**一改指纹即不符。行数与分组计数由登记册与校验项按登记行实时给出，本节不复制计数。
- **退役面**：`retired_knobs[]` 保留「无对应文档行的存量登记」这一事实（`reason/evidence/retired_at/review`），门 CFG002-12 断言该集合与 `plugin_knobs`／文档配置表**无交集**——退役留痕，不删事实。
- **非配置表面**：`non_config_tables[]` 显式登记 `docs/detail/**` 内**不是**配置键的表（如事件协议字段表 `字段|承载事实`），含权威来源；未登记的白名单外表头即判红（fail-closed）。
- **owner_class**（归属类，一行恰一个）：`science_param`（影响科学结果，必须有 SCI/ALG 条款或已登记配置类承载）· `runtime_policy`（运行期/实现/IO/观测策略，权威 = 插件文档或 algorithms/contracts 文档，**禁入 phase_config**）· `cli_surface`（命令行参数面）· `resource_binding`（线程/资源预算，cpu_profile 或调度器，禁硬编码、禁入 phase_config）。
- **finding**：`none`（已闭合）· `gap`（插件声明了默认值但无 SCI/ALG 权威、未进任何配置类）· `unregistered`（无默认且字段本身未登记）· `conflict`（与 SCI/ALG 权威或已登记配置冲突，必须带 `conflict.{kind,evidence,owner}`）。
- **分布**：行数与 `finding` / `owner_class` / `registration` 分组计数由门 CFG002-01 按登记行实时重算并与登记册比对；本节不复制计数。
- **冲突面**：登记册的 `conflict` 计数必须为 0（门 CFG002-01 判定）。争议项的现行权威取值：①`04_psf.psf_model` = **moffat4**（`docs/science/psf/PSF.md`）；②`08_drizzle.pixfrac` 默认 **0.8** 落 `defaults.json`；③`03_star_detection.detection_threshold` = **全局** `median(img)+5.0·bgnoise`（局部自适应为目标态）；④`06_photometry.flux_zero_point` 行**不存在**（`../../science/algorithms/PHOTOMETRIC_FIT.md`）。
复跑判据 = 把任一插件文档的声明值改成与 `docs/science/` 权威不符时，该计数必须变正 ⇒ 判据非退化。
- **module.yaml 面**：20 份 `lib/**/module.yaml` 的顶层键只含契约/端口/构建/证据字段，**无任何旋钮或默认值声明字段**；门 CFG002-06 断言键集闭包——出现 `knobs/knob/params/parameters/config/configs/defaults/tunables/options/settings` 任一键即判红，新增旋钮声明必须先在本册登记归属。
- **维护规则**：本册是**人工维护**的登记面；文档变化必须由人重新判定归属。绿灯只出自人工判定（生成脚本批量刷新不构成归属判定，`docs/DOCUMENT_INDEX.yaml` fail-closed）；本册不复制科学数值（数值唯一源 = defaults.json / phase_config schema）。

### 滤镜名匹配语义（可执行规则 + 正反例）

```text
rule:   resolve(name) = filters[name] if name in keys(filters) else ERROR(unknown_filter)
        （lookup.match=exact、case_sensitive=true、normalization=none、aliases={}）
positive: "Baader R" | "Johnson V" | "SDSS r" | "OPTOLONG L-PRO Light Pollution"  -> 命中
negative: "bader r" | "bader v" | "baader r" | "BAADER R" | "Baader  R" | " Baader R" -> ERROR
```

- 三份 phase_config schema 的 `filter` 字段 enum 必须逐项等于库键（45），门同时断言 `enum == keys(filters)`。
- 正反例由门 CFG002-04 在每个消费滤镜的 phase（normalize/mosaic）上**双向**复跑：反例必须被拒、正例必须通过；`non_key_examples` 的 `where` 是**内容锚**（`{id,path,quote,sha256,value_text}`）：引文必须在本文件内**唯一**命中且含该反例串（`value_text` 逐字相等）。最高设计不含这两个反例串，故锚点只落在本文件（规则正本 = `../../detail/anchors/ANCHOR_CONTRACT.md` 的「4. 内容锚（content anchor）」一节：位置优先用内容锚表达，不写行号）。
- 归一化被**显式拒绝**（不是「暂未实现」）：`aliases` 为空对象即声明「无别名」；将来要支持别名必须先登记（登记=改合同，需权威条款 + 校验项 + 迁移说明）。

### `cpu_profile.host.os_abi` 值域（冻结）

```text
enum:      { "linux", "windows" }
derivation: lib/infrastructure/benchmark/backend_host/hardware_inspect.cpp（_WIN32 -> "windows"，否则 -> "linux"）
            lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp（缺 os 字段回落 "linux"）
platform:   最高设计的双平台发行一章（Windows 10+ amd64 / Linux amd64，两个平台均为交付平台）[3]
consumer:   lib/infrastructure/benchmark/backend_host/cpu_routing.cpp（与 hw os.name 逐字比较，不等 -> stale_machine）
negative:   eng/tests/config/fixtures/negative/cpu_profile_v2_bad_os_abi.json（os_abi=freebsd -> REJECT）
```

- 值域是**转录**（生产者字面量集合），不是编造；非该集合 fail-closed（不夹逼、不改写、不降级）。
- 合成夹具字面量（`eng/tests/unit/cpu007_profile_store_test.cpp` 的 `linux-test`）不经 schema 校验，属测试内构造；若将来把 schema 校验接进 profile_store，该夹具需同步（已登记在登记册 `os_abi.known_non_production_literal`）。

### 索引归属（单一事实源）

| 面 | 归属 | 事实源 | 门 |
|---|---|---|---|
| 配置数据与模板 | `eng/packaging/config/**`（defaults.json / filters.json / templates/*.json） | 数据本体；**不含 schema** | CFG002-07（config 下出现 `*.schema.json` 即红） |
| 模块/CLI 契约 schema | `eng/contracts/config/**` | cli_modules_list / cli_selftest / module_dll_contract / module_lifecycle_contract | CFG002-07（全为 schema；与 eng/packaging/config/** 各自具名） |
| phase_config schema | `eng/contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json` | 三份 phase 专属；无聚合第二定义 | `test_no_aggregate_second_definition` |
| 文档事实源 | `CONFIG.md`（本文件） | 语义与索引 | CFG002-07（DOCUMENT_INDEX 恰一次 + ACTIVE_NORMATIVE） |
| 文档索引 | `docs/DOCUMENT_INDEX.yaml` | docs/** 与根治理文档 | 文档索引判据 |

CFG002-ANCHOR: item5-index-ownership → eng/packaging/config/config_registry.json

规则：C++ struct 默认值、parser 默认值、JSON schema、template config、
docs、tests 必须一致。该一致性门在本仓无执行器（`eng/tools/config_consistency_check.py` 不在跟踪集内），判据由人读核对。

## 配置面二 · orchestrator Stage2 配置

**本块只描述 orchestrator Stage2 配置面，作用域仅限 orchestrator 阶段二配置文法与解析缺省**（parser = `lib/algorithms/coverage/src/stage2_common.cpp`）。它**不是**三命令 `phase_config` 合同、不是生产写出侧的运行清单面、也不承担三类配置的字段与登记（本文件的配置面一才是三命令 `phase_config` 与三类配置的正本）。三命令 `phase_config` 的语义与索引以本文件的配置面一为准。

**本块键集合（Stage2 配置面）** = 下列文法给出的键。**三套退役键 `integration.weight_mode`、`integration.legacy_allow_weight_fallback`、`integration.acr_route` 出现即 fail-closed 拒绝**（既不能被设、也不能被读）：出现本块以外的键名、或出现这三个键名，一律判错。`weight_mode` 作为现行键只出现在 `sky_plane` 子段（稀疏信号面的定权口径），与被拒的 `integration.weight_mode` 是两个不同键位，不得混用。

- 本块的 `precision(fp32)` 是 **orchestrator** 的解析缺省（仅用于「doc ↔ parser/struct 一致」判据）；三命令 `phase_config` 的精度**必须显式**（normalize = 块级 `drizzle.precision_mode`；mosaic/export = 位深键），口径见本文件的配置面一。
- **`output_mode` 不属本块**（它只出现在 export 的 `phase_config`）：**必填且必须显式** —— `blocks[]` 分支、平铺单块简写分支、`{phase_name, config, inputs[]}` 简写分支的 `required` **都含 `output_mode`**（fail-closed）；合同登记的 `surface_brightness` **只作 `--template` 骨架值**，运行期缺省另有显式来源。⇒ 本块与配置面一在此点上**两边同向**。

### Stage2 config 段

```text
inputs.hips[] / target_order
model: control_grid_per_tile(8) patch_radius_leaf(2) min_samples(5)
       snr_search_radius_deg(0.05)
       background_patch_radius(8) background_clip_sigma(3.0)
       background_clip_iters(3) background_max_contamination(0.20)
       background_contamination_sigma(3.0)
       background_min_retained_fraction(0.60)
       background_tolerance(3.0) background_neighbor_radius(2)
       background_catalog_veto(1)
       huber_delta(1.345) smoothing(auto→0.1) zero_anchor_weight(1e-3)
       max_irls_iterations(100) tolerance(1e-6) sigma_floor(1e-3)
       support_power(1.0) robust_loss(huber) snr_weight_mode(snr2_normalized)  # UPM fit 内部诊断开关（键名以实现解析点 lib/algorithms/coverage/src/stage2_common.cpp 为准；最高设计的「数据对象」一节（数据对象）的「无权重模式可选概念」约束的是 Phase2 集成 weight_mode 概念，与本键无关）
integration: precision(fp32) memory_limit_mb rejection{method
             none|sigma|winsorized_sigma|averaged_sigma|linear_fit|
             generalized_esd|rcr|percentile|median_sigma|minmax|auto
             profile(acsd_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档)|wbpp_current alias|acsd_adaptive)
             underdetermined_n(0=按 profile 解析；pixel=3/其余=2/extreme_prior=1)
             normalization(none|acsd_median_center_v1|acsd_median_scale_v1)
             normalization_floor(1e-12)
             large_scale{enabled(false) min_structure_pixels(8)
                         low_grow_radius_pixels(2)
                         high_grow_radius_pixels(2)}
             robust_mad_clip{lower_sigma 4 upper_sigma 3 max_iterations 8}
             winsorized_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             averaged_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             linear_fit{lower 5 upper 3.5 max_iterations 8}
             generalized_esd{alpha 0.05 max_outliers 10}
             percentile{low_fraction 0.2 high_fraction 0.1}
             median_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             minmax{reject_low_count 1 reject_high_count 1
                    min_kept 4}
             rcr{technique ss_median_dl}}
  # integration 段的现行键集合 = {precision, memory_limit_mb, rejection}。
  # 出现在 integration 段的下列键名一律 fail-closed 拒绝（既不能被设、也不能被读）：
  #   low / high / max_iterations / min_samples   （档级阈值的旧键位）
  #   weight_mode / legacy_allow_weight_fallback  （权重模式面：全链无「权重模式」可选概念）
  #   acr_route                                   （集成执行路由面：生产计算后端恒为纯 CPU）
  # 其余任何键名同样判错。拒绝面必须存活，不得静默忽略或静默取默认值。
  # 需要上述键位的配置由 eng/tools/migrate_stage2_config.py 迁移。

rejection.method 说明：
  - 默认 `method=auto` + `profile=acsd_adaptive_pixel`
    （**Astro Celestial Sphere Database（ACSD） 自研**，逐输出像素几何 N 的三档自动映射见下方排异档位映射一条，本块不复述档位表与阈值来源）；`wbpp_2_9_1` 为**对照档**（`wbpp_current` 为
    alias，解析并序列化为 wbpp_2_9_1）；
  - auto 在 **planning 层**按 integration cohort/tile 的 nominal
    contributors（几何可贡献独立 exposure 数）解析一次，禁止在 pixel loop
    内按 effective count 路由；对照档 = **本仓冻结解析表**：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
    该表的档界本应逐档对应 WBPP 的 `bestRejectionMethod`；**其一手原文在本仓核对不到**
    （`pixinsight.com/doc/**` 各路径均返回 404，WBPP 为 PixInsight 付费更新包、不可公开取得），
    故此处的对照档只承担**本仓冻结行为**，不主张与 WBPP 逐档同源。
    **仓内权威登记存在**：`../../science/REJECTION.md` 的「14 Primary literature（引用定位声明）」
    第 3 条与「14a 参考文献与参考代码库（含许可证）」一节逐字登记了档界对照的可核验形式——
    WBPP 2.5.9 的 `WeightedBatchPreProcessing-engine.js:1421-1429` `bestRejectionMethod()`、
    官方更新包 sha1 `712cc7c3fdb523643ad0e685104592d511996f82`、该版 `n > 15` 档为 `Rejection_ESD`
    与 WBPP ≤2.3.x 该档为 `n < 25 → LinearFit`。本块以那两节的登记为准，需一手原文补源后另行登记。
  - `acsd_adaptive` = ACSD 自有策略（tile nominal depth 自适应，
    独立命名，不冒充 WBPP exact）；
  - effective 候选数 <= underdetermined_n（**默认 0 = 按 profile 解析**：wbpp/adaptive=2；
    acsd_adaptive_pixel=3；显式 request=EXTREME_VALUE_PRIOR_SIGMA（opt-in）=1）或 < 方法 minimum N →
    REJECTION_UNDERDETERMINED（可全接受但必须记录，禁止偷偷换算法）；
  - normalization：判定工作域与科学值域分离（decision 作用于
    working stack，accepted mask 应用回原始 calibrated 值）；
    percentile 必须 acsd_median_center_v1（负值安全）；rcr 必须 none；
  - large_scale：acsd.large_scale_rejection.v1（8-连通分量 grow，
    min_structure_pixels 过滤，low/high 独立半径；默认关闭）；compact
    cosmic/星点不会无限生长；PIXINSIGHT_EXACT=NOT_CLAIMED；
  - percentile: 相对 median 的百分比 clip（low_fraction/high_fraction
      为小数，默认 0.2/0.1 = WBPP Light percentileLow/High）；
  - median_sigma: median 位置 + SD 尺度迭代 clip（WBPP Median Sigma）；
  - minmax: 一次性固定 rank 剔除最小 reject_low_count 与最大
      reject_high_count 个样本（n−low−high >= min_kept；无 max_iterations）；
  - sigma = acsd.robust_mad_clip.v1（median + MAD 迭代 clip；
      Astropy sigma_clip(mad_std) oracle）；旧字符串 "sigma" 为 alias；
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代，Huber 体系）。
      **一手原文（PixInsight ImageIntegration 官方文档）在本仓核对不到**，故本块不以文献编号背书，
      只陈述本仓实现口径。仓内权威登记见 `../../science/REJECTION.md` 的「14 Primary literature（引用定位声明）」
      第 2 条：语义来源为 PixInsight ImageIntegration 官方文档中的相应公式（±1.5σ winsorize、
      常数 1.134、迭代限 5e-4），概念出处 = Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.。
      Siril 1.4.3 仅作**次生参考实现对拍**，不是语义来源。
  - 档级阈值的现行键位在 `rejection` 段的各方法子对象内（如
      `winsorized_sigma{lower_sigma, upper_sigma, max_iterations}`）；
      `low`/`high`/`max_iterations`/`min_samples` 出现在 integration 顶层即硬错误。
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

**排异档位映射**（三档逐像素自动路由的**唯一正本**）：`1≤N≤3` none / `4≤N≤5` percentile / `N≥6` winsorized；N = 该输出像素的**几何可贡献帧数**，逐像素自动路由；**min/max 不用于生产**。阈值表的机器来源 = `lib/algorithms/coverage/src/rejection.cpp` 的 `kPixelSmallNPolicy` 与 `acsd_n_map_method`；**文档面无可核来源**——最高设计的逐像素排异一章只给「按 N 自动选择」与生产算法集四种（none、percentile、winsorized、linear fit），不含任何 N 阈值，把阈值挂到该章属误挂，阈值入库前须补文档层承载。`linear fit` 档可显式指定但不参与逐像素自动路由（自动路由只出上列三档），「生产算法集四种」与「自动路由三档」是两个不同集合，不是两套口径。上方 fenced 块的 `acsd_adaptive_pixel` 档位与本条同值；WBPP 对照档（`nominal<6 / 6..15 / >15`）只描述 `wbpp_2_9_1` 对照 profile 自身，不参与生产路由。`docs/science/unified/DATA_SEMANTICS.md` 首注同面。

### 掩膜三组键文法（普通星 / Gaia 晕 / 缝回退，已废弃）

**废弃声明**：星掩膜通道已停止使用——采样器侧 simple(6)/halo(7) 两个拒绝分支在代码中整段注释，只保留 catalog(5)；编排侧 Gaia 查询整段停用、恒走空帽直通。本节三组键只作停用前的合同与「为什么停用」论证保留，**不是现行可用配置面**：现状使用即被未知键门拒绝（`rc = 3`），`model` 空对象仍可通过。语义正本（含停用原因与恢复条件）见采样算法分册的普通星简单掩膜、Gaia 晕掩膜、缝回退三节及其废弃声明。

本节给出 Stage2 配置面 `model` 段内掩膜三组的键集合、缺省与值域。本节键属配置面二，在三命令 `phase_config` 或运行清单面出现即判错。缺键取默认，显式给出须满足值域，否则拒绝并报错，本面以外的任何键名同样判错。

普通星组沿用既有两键：`model.star_mask_snr_factor`（默认 10.0，无量纲，必须大于零）与 `model.star_mask_radius_deg`（默认 0.012，单位为度，必须大于零）。语义为信噪比域相对阈值加球面圆帽的保底分支：阈值等于该因子乘所在帧星表信噪比中位数，半径取该角半径。同一对阈值与半径在两处以同口径消费，一处为第一遍星表邻域否决，另一处为跨帧位置去重后的球面圆帽生成。语义正本为采样算法分册的普通星简单掩膜一节。

Gaia 晕组共五键：`model.halo_mag_thresh`（默认 8.0，Gaia 星等，须为有限数）、`model.halo_r8`（默认 150，单位为像素，必须大于零）、`model.halo_a`（默认 1.5，无量纲，每星等倍数，必须大于一）、`model.halo_r_min`（默认 30，单位为像素，必须大于零）、`model.halo_r_max`（默认 300，单位为像素，必须大于零，且下限不得大于上限）。半径函数为星等定半径：原始半径等于晕锚点半径乘每星等倍数的阈值减星等次方，只对亮于阈值的星成帽，再按上下限截断。取默认时即还原采样算法分册的成品公式。半径像素口径与锚定帧相同，帧像素口径按像素尺度比换算。阈值对应的星等窗查询、排序与取最亮由掩膜模块自做。普通星阈值半径键与 Gaia 星等限晕半径键分键配置，不可共用信噪比中位数倍数语义，成帽逻辑不得复用固定星掩膜半径。语义与默认数值正本为采样算法分册的 Gaia 晕掩膜一节。

缝回退组共两键：`model.seam_fallback_factor`（默认 1.5，无量纲，不得小于一）与 `model.seam_fallback_r_max`（默认 450，单位为像素，必须大于零，对应成品公式的回退上限）。回退半径等于该因子乘常规晕半径再按回退上限封顶。触发判据为缝验收失败且暗段不动，仍失败时检查触发源而不继续放大，上限只用于缝邻域。重算语义为全量重跑：覆盖构建、采样探查与回填、模型持久化与分块求值均无区域子集重算入口，引用回退重算一律按全量语义解释。语义与默认数值正本为采样算法分册的缝回退一节。

多尺度低频组退役：`model.ms_enabled/ms_sigma_px/ms_thresh` 不再是生产配置面（单语义一次扣除下无开关；`ms_verify` 测试程序内自带口径）。`model` 内出现三键中任一即判错。环境变量 `ACSD_UPM_MS_*` 只作测试覆写，不得作为生产配置面。

现状分叉声明：既有三键 `model.control_k_corr`（默认 1.4）、`model.star_mask_snr_factor`、`model.star_mask_radius_deg` 在编排采样算子可读，在阶段二结构体、解析白名单与工具组装三处均无对应，工具链路以零初始化后按实现修补回默认运行，两条链路对同一配置给出不同生效面，引用时必须区分。新七键在采样器结构体、阶段二解析白名单、阶段二工具组装、编排采样算子四处均无读取，现状使用即被未知键门拒绝，本节为合同先行，消费面落地前不得视为可用。分叉事实的登记面为注册表模块页的逐字段登记、星掩膜函数登记与已知限制三节。

停用现状（证伪停用，只注释不删除；性能优化与 export slim 保留）：`model` 七子键（`halo_mag_thresh`、`halo_r8`、`halo_a`、`halo_r_min`、`halo_r_max`、`seam_fallback_factor`、`seam_fallback_r_max`）现状使用即被未知键门拒绝——块内面与平铺面均按 unknown-key 判错（`rc = 3`），`model` 空对象仍可通过；编排侧 Gaia 查询整段停用，恒走空帽直通（`halos.n = 0`，查询计数恒 `0`，`source = "disabled-halo-falsified"`，`anchor_scale = 0`，`with_halos` 入口保留），查询不到不停采样流程。停用原因一句话：RERUN2/RERUN3 对比洞为次因，缝主因为 M1/M4 混合 pedestal 差；恢复条件为 task-4 UPM 路先落地。停用期间的函数留桩口径见注册表采样页的星掩膜函数登记与已知限制两节。

## 配置面三 · 阶段生产调用链与 Stage1 配置

**本块只描述阶段生产调用链与 Stage1 配置面，作用域仅限编排入口到实现符号的登记与阶段一配置**：它登记每个生产入口由哪条配置门控制、落到哪个实现符号、以何种执行模式运行、写出哪个诊断字段，以及 Stage1 配置文件面的取值。本块**不承担**三命令 `phase_config` 的键集合与值域（见配置面一）、也**不承担** orchestrator Stage2 配置文法（见配置面二）；表中「配置门」列引用的是本文件对应配置面已声明的键，不在本块另行定义键集合。

### 生产调用链（入口符号 → 配置门 → 实现符号 → 诊断字段）

每个生产入口由哪条**配置门**控制、落到哪个实现符号、以何种
执行模式运行、写出哪个**诊断字段**、由哪个测试 ID 守护。配置门的键名与诊断字段名是写 config
时实际要敲的键，不可从别处推断，故在此逐条登记。

字段口径：`entry_symbol` 生产入口符号；`config_gate` 控制该入口的配置门（`*` 表示该节点由
一组门共同控制）；`target` 目标模块域；`source_symbol` 实现符号；`execution_mode` 执行模式；
`diagnostic_field` 该入口落盘/回传的诊断字段；`test_id` 守护该行的测试 ID。

#### stage1（`normalize` 子命令）

| 入口符号 | 配置门 | 目标 | 实现符号 | 执行模式 | 诊断字段 | 测试 ID |
|---|---|---|---|---|---|---|
| normalize --json <config.json> (唯一 CLI 子命令) | phase_config_normalize.schema.json | acsd(cli) | normalize_subcommand (lib/infrastructure/cli/normalize/normalize.h) + session_dispatch (commands.cpp:2404) | serial | exit_code | TEST-CLI-001 |
| Orchestrator::run_stage_calibrate | stage1.calibrate.enabled | astro_calibration | ac_calibrate_frame / calibrate | parallel_cpu (OpenMP 16) | calibrate.t_ms | TEST-CAL-001 |
| Orchestrator::run_stage_platesolve | stage1.platesolve.enabled | ipv_solver | ipv_solve_from_detections_v1 / build_wcs | serial | platesolve.rms_px | TEST-IPV-001 |
| Orchestrator::run_stage_photometric | stage1.photometric.enabled | photometric_calib | pc_calibrate_simple | serial | photometric.zero_point | TEST-SPEC-001 |
| Orchestrator::run_stage_drizzle | stage1.drizzle.enabled | healpix_drizzle | drizzleTiled / processPixelSharedTiled | parallel_cpu (OpenMP) | drizzle.n_tiles | TEST-DRZ-CAND-001 |
| Orchestrator::run_stage_snr | stage1.snr.enabled | snr_estimator | snr_noise_model_v1 | parallel_cpu | snr.sigma_bg | TEST-SNR-001 |
| aio_hips_write_signal_support_tile | stage1.output.hips | astro_image_io | aio_hips_product_begin | serial+async_io | hips.nside | TEST-HIPS-001 |

#### stage2（`mosaic` 子命令）

| 入口符号 | 配置门 | 目标 | 实现符号 | 执行模式 | 诊断字段 | 测试 ID |
|---|---|---|---|---|---|---|
| mosaic --json <config.json> (唯一 CLI 子命令) | phase_config_mosaic.schema.json | acsd(cli) | mosaic_subcommand (lib/infrastructure/cli/mosaic/mosaic.h) + session_dispatch (commands.cpp:2404) | serial | exit_code | TEST-CLI-002 |
| coverage_union | stage2.inputs.hips | phase2/coverage | p2_coverage_union | serial | coverage.n_frames | TEST-COV-001 |
| control_sample | stage2.model.* | phase2/sampler | p2_sample_controls | serial (P2_ENABLE_OPENMP OFF) / parallel_cpu if ON | sampler.n_controls | TEST-UPMW-004 |
| upm_build | stage2.model.* | phase2/upm | p2_upm_build | serial | upm.n_components | TEST-UPMW-001 |
| upm_persist | stage2.output.upm | astro_image_io | aio_upm_write_sparse | serial | upm.sha256 | UpmPersistAllPermutations |
| block_calibrate | stage2.integration.* | phase2/block | p2_upm_calibrate_block | parallel_cpu | block.t_ms | TEST-P2-CALIB-001 |
| rejection | stage2.integration.rejection.* | phase2/rejection | p2_reject_stack_ex (三档自动选择，阈值表见下条) | tile 级并行 (p2_parallel_for,  std::thread) | rejection.n_rejected_low/high | TEST-REJ-* |
| integration | stage2.integration.* | phase2/integrate | p2_integrate_pixel | tile 级并行 (p2_parallel_for,  std::thread) | integrate.signal/support | V17StatusesExplicit |
| hips_write | stage2.output.hips | astro_image_io | aio_hips_writer | serial+async_io | hips.nside | TEST-HIPS-001 |

### Stage1 config

见 `lib/infrastructure/pipeline/orchestrator/configs/stage1_*.json` 模板。

- `drizzle.pixfrac` (0,1]：`stage1.schema.json` 默认 0.8（生产默认收缩滴落，`stage1.template.json` 同）；
  银心三面板 `stage1_gc_panel{1,2,3}_Red.json` 为 `pixfrac=1.0` 无收缩分支（最大覆盖/GC 专用），与默认分支在 `lib/infrastructure/pipeline/orchestrator/configs/` 并存。

## 参考文献

[1] IVOA. HiPS — Hierarchical Progressive Survey, Version 1.0. REC-HIPS-1.0.
https://www.ivoa.net/documents/HiPS/

[2] FITS 工作组. FITS 标准 4.0. IAU, 2018. https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf

[3] 内部文档 `../../ACSD_DESIGN.md`，最高设计的输入合同与三类配置两章。

[4] 内部文档 `CLI_PROTOCOL.md`，命令行协议合同。

[5] 内部文档 `../governance/DOCUMENT_GOVERNANCE.md`，文档治理规范。

[6] 内部文档 `HIPS_STORAGE_FORM.md`，落盘形态与归档容器合同，不变式 F0–F4 与 M1–M4 的唯一正本。

[7] 内部文档 `../testing/TEST.md`，测试标准，通用浮点容差与 NaN/Inf 语义的唯一正本。
