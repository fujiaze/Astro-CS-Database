# RELEASE-02 Phase3/export 与 aio 面修复报告

- 任务：RELEASE-02 Phase3/export 与 aio 面修复者（子代理）
- 工作目录：`/workspace/Astro CS Database`
- 时点：2026-09-18
- 文件面（授权）：`lib/infrastructure/aio/**`、`lib/algorithms/projection/**`、`lib/algorithms/drizzle/**`；
  另经前台 2026-09-18 追加授权：`config/templates/*.phase_config.json`。
- 约束遵守：未跑 `ninja`/`cmake`/`ctest`（仅 `g++ -fsyntax-only` 与运行既有 `build/astrocs` 复现）；
  `TMPDIR=/dev/shm/astrocs_p3`；零 git 写；产物落 `run/RELEASE-02/phase3/`。
- 禁改面（未触碰）：`lib/infrastructure/cli/**`（CLI 分片）、`lib/infrastructure/scheduler/**`（hub 批处理）、
  `lib/algorithms/coverage/**`、`lib/algorithms/integration/**`、`tests/**`、`docs/**`、`ci/**`、`contracts/**`。

---

## 0. 复核方法与证据标准

- 每条缺陷先独立复核（读权威文档 + 源码 + 独立标准取证 + 运行既有二进制复现），确认后才改。
- 未构建；功能级证据由前台统一构建/测试产出。本报告给的是「源码级改动 + 语法检查 + 既有二进制复现 + 标准取证」。
- 日志目录：`run/RELEASE-02/phase3/logs/`。

---

## 1. 缺陷逐条处置

### 缺陷 1（P0-19）`hips_frame=icrs` 违反 IVOA HiPS 标准 —— 已修（面内）

**复核结论：确认缺陷成立，且原实现注释把标准值/非标准值判反。**

独立标准取证（2026-09-18，非本仓库来源）：
- IVOA HiPS 1.0 Proposed Recommendation（PR-HIPS-1.0-20170406）§4.4.1 关键字表原文：
  `hips_frame: Coordinate frame reference – Format: "equatorial" (ICRS)`，其余取值 `galactic`、`ecliptic`。
  https://www.ivoa.net/documents/HiPS/20170406/PR-HIPS-1.0-20170406.pdf
- IVOA HiPS 1.0 Working Draft 关键字表：`"equatorial", "galactic", "ecliptic"`。
  https://www.ivoa.net/documents/HiPS/20160623/WD-HiPS-1.0-20160623.pdf
- CDS Aladin Lite API：`createImageSurvey(..., HiPS frame ('equatorial' or 'galactic', usually 'equatorial'), ...)`。
  https://aladin.cds.unistra.fr/AladinLite/doc/API/
- Hipsgen 手册（CDS 生产工具）自述其 HiPS 基于 ICRS equatorial 坐标系。
  https://aladin.cds.unistra.fr/hips/HipsgenManual.pdf

结论：`icrs` 不是 `hips_frame` 的合法取值；ICRS 的标准写法是 `equatorial`。
原注释「值域 = {icrs, galactic, ecliptic}；非标准值 equatorial 已废止」系误读（M1a-B-005 引入的回归）。

**改动（面内）：**

| 文件:行 | 改动 |
|---|---|
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1119-1128` | image 子产品 `hips_frame` 写出值 `icrs` → `equatorial`；注释改为标准取证结论 |
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1376-1378` | SNR catalogue 子产品 `hips_frame` 写出值 `icrs` → `equatorial` |
| `lib/infrastructure/aio/io/hips_core.c:239-255` | 读侧：接受标准值 `equatorial` + 兼容别名 `icrs`（行为不变，仅订正注释/诊断文本；原实现已两者都收，故读侧不产生兼容性回归） |
| `lib/infrastructure/aio/v6/include/astro/aio/v6_hips_manifest.h:30-32,61` | `HipsProperties::frame` / `HipsManifest::frame` 默认值 `icrs` → `equatorial` |
| `lib/infrastructure/aio/tests/p1hips/p1hips_tests_units.cpp:286-293` | 断言反转：写侧必须 == `equatorial`，且 != `icrs` |

**读侧兼容**：任务要求「读侧保留 `icrs` 作兼容别名」——满足（`hips_core.c` 接受两者，行为未变）。

**面外连带（已转交，不属我面）：**
- `tests/unit/p1_hips_writer_test.cpp:137-138` 仍断言 `props.find("hips_frame=icrs")` 且否定 `equatorial`，**改后必红**。
  前台已裁决转交 tests 分片反转，我不改。
- `lib/infrastructure/scheduler/src/module_adapters.cpp:3954-3956,5509-5512,6630,6653` 节点 manifest 仍写 `coordinate_frame="icrs"`（注释明言与 properties 的 `hips_frame=icrs` 同源）。HiPS properties 改 `equatorial` 后，CLI 汇总的 `coordinate_frames` 与 properties 会分叉；需 hub 批处理同步。
- `lib/algorithms/coverage/*` 与 `lib/algorithms/coverage/tools/controlled_rejection_truth.py:51` 接受/构造 `icrs`（coverage 为禁改面）；其跨帧一致性门（`coverage.cpp:226-230`）要求跨帧 frame 全等，旧产品（icrs）与新产品（equatorial）混用会 fail-closed，迁移期需注意。
- `lib/phase3_session/p3_v6_export.cpp:240`、`lib/algorithms/integration/v6/src/phase2_integrate.cpp:443`、`v6_phase1/include/astrocs/v6/phase1_product.h:130` 的 `coordinate_frame="icrs"` 属内部 provenance 坐标声明（非 HiPS `hips_frame`），是否统一由前台裁决。

**验证**：`g++ -std=c++17 -fsyntax-only`（writer / v6 头）+ `g++ -std=c11 -fsyntax-only`（hips_core.c）全 0；
日志 `logs/syntax_defect1.txt`。

---

### 缺陷 2（§9 原子产品）`p3_props/p3_wcs/p3_resampled/p3_verify` 原地直写 —— 面内部分修复 + 面外转交

**复核结论：确认成立。** 审计所述四个 p3 中间产物的直写站点**全部在 scheduler 面**（禁改）：

| 产物 | 站点（当前行号） |
|---|---|
| `p3_props.json` | `lib/infrastructure/scheduler/src/module_adapters.cpp:6135` `std::ofstream f(path, ...)` 直写正式路径 |
| `p3_wcs.json` | `module_adapters.cpp:6185` |
| `p3_resampled.bin` | `module_adapters.cpp:6460` |
| `p3_resampled.json` | `module_adapters.cpp:6505` |
| `p3_verify.json` | `module_adapters.cpp:6734` |

均无 tmp → fsync → rename。对应审计 DC-508/DC-509/DC-1104；在册 OPEN 项 `问题扫描/账本/FIX_LEDGER.csv::M2b-C-01`。
（注：审计原文行号 5910/5960/6235/6280/6509 系 P0-21 修复前基线，已漂移。）

**面内修复（aio 的 §9 原子性缺口，与缺陷 2 同源）：**
HiPS 产品集的完成标记 `manifest.json` 与 SNR `metadata.xml` 原为 `std::fopen`+`fprintf` 原地直写（manifest 失败还被静默吞掉）。

| 文件:行 | 改动 |
|---|---|
| `lib/infrastructure/aio/src/aio_atomic_file.h:139-186` | 新增 `aio_atomic::write_file_atomic_stream(final, writer(FILE*), err)`：同目录 tmp → writer → fflush → fsync → 原子 rename，失败清理 tmp、目标不动（与既有 `write_file_atomic` 同语义） |
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1425-1458` | SNR `metadata.xml` 改用该原语；失败返回 false（fail-closed） |
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1691-1781` | `manifest.json`（产品集完成标记）改用该原语；写失败由「静默 return 0」改为返回 `-13`（fail-closed，调用方 `aio_hips_abort` 语义不变） |

**未闭合（面外，转交前台）：**
1. scheduler 侧 5 个 p3 产物站点接统一的 tmp/fsync/rename（hub 批处理已确认）。
2. aio HiPS 写出仍是「就地建树」：SNR `.tsv` tile（`aio_hips_writer.cpp:1349` 附近）与 CFITSIO 写的 FITS tile/MOC/hierarchy 未逐文件原子，且未做**整目录 staging → rename**。真正满足 §9「正式产品目录只出现完整产品」需接入既有 `v6_atomic_publish.cpp::atomic_publish_directory`（staging+rename）或 `.partial`+rename（即 M2b-C-01 的完整闭合）。本次仅闭合了「manifest/metadata 单文件原子」这一半。
3. `lib/algorithms/fits_output/p3_output.cpp:158-161,344-362` 的 FITS 已有 tmp+rename（无目录 fsync），属面外，由 hub 统一。

---

### 缺陷 3（§5.3）`export output_mode` 死合同 —— 复核确认，面内无语义缺口，接线属面外

**复核结论：确认成立。** `output_mode` 在 `lib/` 的 C++ 消费者为 0（`grep -rn output_mode lib/` 仅命中注释/白名单）；生产导出恒按 surface_brightness，从不拒绝/unavailable。

- 面内（projection）：语义门已在位且正确——`lib/algorithms/projection/p3_proj_v6.cpp:625-650`
  `semantics_required_normalization` / `semantics_compatible`（surface_brightness=行归一 R、point_source_flux=列归一 S、visualization=无归一；未知语义 → kParam）。
  **但确无生产调用点**（`grep SampleSemantics lib/` 仅命中本模块自身）。接线属 scheduler 面，不在我文件面。
- 面外消费者/实现：`lib/infrastructure/cli/{session_commands.h:162,v6_runtime_contract.h:134,parser.cpp:313-315}`、
  `lib/phase3_session/p3_v6_export.cpp:52-63`、`lib/algorithms/resample/{p3_rsmp_units.cpp:25-47,p3_rsmp_propagation.cpp}`、scheduler resample。
- CLI 分片已在工作树把 `output_mode` 加入 `parser.cpp` 白名单（`git blame` 显示 Not Committed Yet），生产消费由 hub 批处理。
- **处置：面内不改**（避免加无消费者的 facade/死代码）；接线与「缺失信息 → 拒绝/unavailable」由 CLI+scheduler+resample 分片闭合。

---

### 缺陷 4（§6.3）官方模板不可运行 —— 已修（前台追加授权）

**复核结论：确认成立。** 修复前实测：
```
export --json config/templates/export.phase_config.json   → rc=2（output_dir 缺失 / source 缺失）
normalize --json config/templates/normalize.phase_config.json → rc=2
mosaic --json config/templates/mosaic.phase_config.json   → rc=2
```
根因：模板是 schema 的嵌套形态（`phase_name/config/inputs` + `precision/output_mode/algorithm_*`），
而 CLI 实际接受的是**平铺会话格式**（`--template` 输出即此形态）；两套键名互斥。
证据：`run/RELEASE-02/phase3/logs/defect4_template_repro.txt`。

**改动（前台 2026-09-18 追加授权 `config/templates/*.phase_config.json`）：**
三个模板改写为**实现实际接受的平铺键名**（与 `<cmd> --template` 输出同族），不新增自造键：

| 文件 | 新内容要点 |
|---|---|
| `config/templates/export.phase_config.json` | `schema_version` / `source.hips_dir` / `output_dir` / `center.{ra_deg,dec_deg}` / `width_px` / `height_px` / `scale_deg_per_px` / `projection:"TAN"` / `output_mode:"surface_brightness"` |
| `config/templates/normalize.phase_config.json` | `schema_version` / `input_lights[]` / `master_bias/dark/flat` / `output_dir` / `drizzle.{nested,pixfrac,precision_mode}` / `filter_passband` / `wcs.gaia_data_dir` |
| `config/templates/mosaic.phase_config.json` | `schema_version` / `hips_paths[]` / `output_dir` / `weight_mode` |

键名来源（均为实现既有消费者）：`parser.cpp:275-316` 的 `kSessionKeys` / `kAllowedKeys`；
`module_adapters.cpp:3366-3414`（drizzle.nside/nested/pixfrac/precision_mode）；
export help（`center/scale_deg_per_px/width_px/height_px/projection/coverage_output` 等）。

**修复后实测（既有 `build/astrocs`，18:47 构建）：**
- `normalize` / `mosaic` 模板：**已无结构错**（不再 rc=2 的 config error；走到 §3.5 预检与交互确认）。
  不带 `-y` 时 rc=2 是「未确认即中止」（`not confirmed — aborting`），非结构错；带 `-y` 后配置被接受，
  仅因示例路径为占位符而在运行期 rc=3（`cannot read master` / `properties not found`）。
- `export` 模板：**当前二进制仍 rc=3 `config has unknown key 'output_mode'`**。原因明确：该二进制（18:47）早于
  CLI 分片对 `parser.cpp` 的白名单修改（19:04，工作树已含 `"output_mode"`，`git blame` 为 Not Committed Yet）。
  **这是构建时序问题，不是模板结构问题**：去掉 `output_mode` 用同一二进制实测即可走到预检（rc=2 确认 / `-y` 后 rc=3 占位路径）。
  CLI 分片落地并重建后，`output_mode` 即被接受。若前台希望模板在**当前旧二进制**上也立刻通过，可暂时删除该行；
  但 DESIGN §5.3 与 `contracts/schemas/phase_config_export.schema.json` 均要求显式 output_mode，故保留。
证据：`logs/defect4_after_template_fix.txt`、`logs/defect4_candidate_repro.txt`。

**未闭合（面外）：**
- `contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json` 仍是嵌套形态，与新模板（平铺）**分叉**。
  schema 是 DESIGN §3.3 的「唯一事实源」；本次按前台指示以**实现接受语义**为准改了模板，schema 的订正/退役属 contracts 面，
  需前台裁决（否则 `tests/config` 若校验模板会红）。
- CLI 侧「合法键不再判 unknown」（含 `phase_name/config/inputs` 或平铺的最终取舍）由 CLI 分片负责。

---

### 缺陷 5（§9.1）aio 为唯一 I/O —— 复核：面内无违反；违反点全在面外

**复核结论：确认违反点存在，但不在我文件面。**

- 违反点：`lib/algorithms/fits_output/p3_output.cpp` 直接调 CFITSIO（`fits_create_file`:166、`fits_write_pix`:236/264/297、
  `fits_open_file`:414、`fits_close_file`:240/250/272/303/322/326/539），绕过 aio。**面外，转交前台/hub。**
- **面内核实（projection / drizzle）：零 CFITSIO 直调。**
  - `lib/algorithms/projection/**`：无 `fitsio` 包含、无 `fits_*` 调用。
  - `lib/algorithms/drizzle/healpix_drizzle/fits_reader.cpp` 是**纯 C++17 自解析** FITS 读取器
    （文件头注释与实现均无 CFITSIO；`grep fits_open_file|fits_create_file|fitsio` 在 drizzle 仅命中一条 CMake 注释）。
  - drizzle 的 HiPS 发布走 `lib/algorithms/drizzle/hips/src/aio_publish.cpp`（aio 出口），未见直调 CFITSIO。
- 故本条我面内**无改动**；§9.1 是否修订由前台按审计 §6.4 裁决。

---

## 2. 改动清单（本次全部，file:line）

1. `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1119-1128` — hips_frame 写 equatorial（image）
2. `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1376-1378` — hips_frame 写 equatorial（snr）
3. `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1425-1458` — metadata.xml 原子发布
4. `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1691-1781` — manifest.json 原子发布 + fail-closed(-13)
5. `lib/infrastructure/aio/src/aio_atomic_file.h:139-186` — 新增 write_file_atomic_stream 原语
6. `lib/infrastructure/aio/io/hips_core.c:239-255` — 读侧标准值/兼容别名订正
7. `lib/infrastructure/aio/v6/include/astro/aio/v6_hips_manifest.h:30-32,61` — frame 默认 equatorial
8. `lib/infrastructure/aio/tests/p1hips/p1hips_tests_units.cpp:286-293` — 断言反转
9. `config/templates/export.phase_config.json` — 重写为平铺（含 output_mode）
10. `config/templates/normalize.phase_config.json` — 重写为平铺
11. `config/templates/mosaic.phase_config.json` — 重写为平铺

`lib/algorithms/projection/**`、`lib/algorithms/drizzle/**` 未改（无缺陷/无违反点）。

---

## 3. 验证证据

| 证据 | 路径 |
|---|---|
| 语法检查（writer / v6 头 / hips_core.c 全 0） | `run/RELEASE-02/phase3/logs/syntax_defect1.txt` |
| 缺陷 4 修复前 rc=2 复现 + `--template` 平铺基线 | `run/RELEASE-02/phase3/logs/defect4_template_repro.txt` |
| 缺陷 4 候选平铺配置实测 | `run/RELEASE-02/phase3/logs/defect4_candidate_repro.txt` |
| 缺陷 4 修复后实测（normalize/mosaic 结构通过；export 待重建） | `run/RELEASE-02/phase3/logs/defect4_after_template_fix.txt` |
| 新增原子原语功能探针（成功/覆盖/writer 失败清理/目标不动/无 tmp 残留） | `run/RELEASE-02/phase3/atomic_probe.cpp` → `PROBE_OK ... writer_fail=-2 target_untouched=1` |

原子原语探针独立编译运行结果（`run/RELEASE-02/phase3/atomic_probe`，rc=0）：
```
PROBE_OK: success=0 overwrite=0 writer_fail=-2 target_untouched=1
（tmp 残留计数 = 0）
```

未跑 `ninja`/`cmake`/`ctest`（按硬要求）；功能级红绿门由前台统一构建执行。

---

## 4. 未闭合项 / 转交前台（面外）

1. **P0-19 连带**：`tests/unit/p1_hips_writer_test.cpp:137-138` 断言反转（前台已转交 tests 分片）。
2. **P0-19 连带**：scheduler 节点 manifest `coordinate_frame="icrs"`（`module_adapters.cpp:3954-3956,5509-5512,6630,6653`）与 properties `equatorial` 分叉，需 hub 同步。
3. **DC-508/509/1104（缺陷 2 主体）**：`module_adapters.cpp:6135/6185/6460/6505/6734` 五个 p3 产物接 tmp→fsync→rename（hub 批处理）。
4. **M2b-C-01 完整闭合**：aio HiPS 写出接入整目录 staging+rename（`v6_atomic_publish.cpp::atomic_publish_directory`），本次只闭合单文件（manifest/metadata）。
5. **DC-503/504（缺陷 3）**：CLI+scheduler+resample 接线 `output_mode`，含「缺所需信息 → 拒绝/unavailable」。
6. **缺陷 4 余项**：`contracts/schemas/phase_config_*.schema.json` 与平铺模板分叉需裁决/订正；CLI 白名单最终键集由 CLI 分片定稿后重建，`output_mode` 才在 export 模板上可跑。
7. **DC-515（缺陷 5）**：`lib/algorithms/fits_output/p3_output.cpp` 直调 CFITSIO 收归 aio，或修订 §9.1（审计 §6.4 待负责人裁决）。
8. 观察（未改，供参考）：`lib/infrastructure/aio/io/hips_core.c:14` 的 tile 布局注释写 `D=ipix/10000, N=ipix%10000`，与写出侧标准布局 `D=(ipix/10000)*10000, N=ipix`（`aio_hips_writer.cpp` `tile_rel_path`）相反；疑为另一在册缺陷（M2b-B-01 族），不在本次 5 条内，未动。

---

## 5. 参考

- `reports/RELEASE-02/DESIGN-CONFORMANCE/REGISTER.md` DC-503/504/508/509/514/515/1104
- `reports/RELEASE-02/DESIGN-CONFORMANCE/SUMMARY.md` B7/B8/B9/B14
- `reports/RELEASE-02/DESIGN-CONFORMANCE/dead-keys.md` §1 `output_mode`
- `ASTROCS_DESIGN.md` §5.3（输出模式显式）、§9（原子产品、aio 唯一 I/O）
- `问题扫描/账本/FIX_LEDGER.csv` M2b-C-01（HiPS 写出直写，OPEN）
- IVOA HiPS 1.0 §4.4.1（见缺陷 1 取证链接）
