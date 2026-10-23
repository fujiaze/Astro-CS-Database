# M0—M6 实施矩阵：可复用／需重构／缺失／未验收（Issue #112 审计）

> 本文是**只读代码审计**的产物：不改 `src/`、`include/`、`apps/`、`validation/`，不改 git 历史。
> 所有结论都指向仓库内可复核的文件行、case 产物或本次实跑命令；**不采用任何“历史报告声称”**。
> 旧交付报告里的“30/78 operation”等数字在本文 §4.2 重新数过，与旧报告不一致处以本文为准。

## 摘要（三行）

1. **57 个科学 operation 在 free 函数层都有真实实现（唯一例外 `refinable.open` 只有声明）**，但只有 21 个（37%）能被合同驱动解码到达；57 项里 20 项可复用、7 项需重构、1 项缺失、29 项只是“写完了但没有任何 HEAD 级证据”。
2. **基建只有 M0 的“声明”完成**（`cea5e3650` 冻结 runtime 头 + 拆分聚合头）；M1（账本/arena）、M2（Task/Pipe/Host/registry）、M3（异步 Storage/DEV_MODE）**全部为零实现**，且现行 `consume_work` 在上限失败时仍已扣额度。
3. **当前真实基线是 11 个必需 case 里 7 pass / 0 fail / 4 pending**，其中 F01/S01 的 pass 只是成功计数；W01/P01/R01/X01 四项 pending 与表 3 的 36 条（L01—L36）有效未完成项必须逐条进 M0—M6，不得汇总为 pass。

---

## 0. 基线与证据规则

### 0.1 代码基线

| 项 | 值 |
|---|---|
| 分支 | `rebuild/acsd-v2` |
| 任务指定起始 HEAD | `ea984cdbba12fdc5b90d0582a6d2585e0ccb9f44` |
| 审计完成时 HEAD | `cea5e3650388590e563e33b1ce93c9ea8f305c4c`（`feat(runtime): M0 freeze the runtime public headers and split the aggregate contract`，2026-10-23 21:06 +0800） |
| 该提交对科学代码的影响 | **零算法改动**：`git diff --stat ea984cdb cea5e3650 -- src apps validation` 只有一项新增 `src/runtime/internal.hpp`（299 行内部合同声明），`apps/` 与 `validation/` 无变化，其余科学 `src/**` 无变化 |
| 声明位置变化 | `include/acsd/contracts.hpp` 由 498 行聚合头拆成 18 行聚合入口；科学 DTO 与 free 函数签名移到 `include/acsd/model.hpp`（503 行，`namespace acsd`）；新增 `include/acsd/runtime/{types,task,memory,pipe,storage,process,diagnostics}.hpp` 与 `include/acsd/modules/observation.hpp` |

因此：

* **表 1、表 3、表 4 的算法/签名证据以 `ea984cdb` 的 `src/` 与声明为准**（`cea5e3650` 未改算法，也未改 free 函数签名文本，只是移动了位置）。
* **表 2 的基建状态以 `cea5e3650` 为准**（这是 M0 公共合同的落地提交），并同时给出 `ea984cdb` 的对照，以免把“已声明”误读成“已实现”。
* 行号引用格式：`src/<module>/<file>.cpp:<line>`（`ea984cdb` 行号；该文件在 `cea5e3650` 也未改，行号可直接复核）。
* 声明行号引用格式：`include/acsd/model.hpp:<line>`（当前 HEAD）。`ea984cdb` 中同一批声明位于 `include/acsd/contracts.hpp:443-502`，逐行偏移 +5（`make_wcs` 在 `contracts.hpp:443` = `model.hpp:448`）。

### 0.2 审计期间的工作树状态（必须一起读）

审计期间工作树**不是冻结的**：M0 实现在同一目录里并行落地（表 2 的状态即来自这条并行线），`validation/*/observed/*.json` 与 `validation/run_summary.json` 也被另一条验证流程刷新。本文的处理办法：

1. 代码行号只引用**被 git 跟踪且与 HEAD 一致**的文件；`git status` 显示 `src/**`（除新增的 `src/runtime/`）无改动。
2. 独立验证在 **`/tmp/audit-repro` 的隔离副本**（`ea984cdb` 的 `include/src/apps/schemas/tools/validation` + 既有 `build/science`）里跑，两次结果见 §5。
3. 后果：`ctest` 的 `design_integrity` 在**本树**上因并行的 `contracts.hpp` 拆分中间态失败过一次（21:04），`cea5e3650` 落地后复跑 **4/4 通过**（21:27）；隔离副本上 `generate_contract_schema.py --check` 也通过（§5.2）。这条差异本身是 M0 并行施工的风险证据，不是 HEAD 的缺陷。

### 0.3 证据等级与本文缩写

| 缩写 | 含义 | 位置 |
|---|---|---|
| **E1** | 本次实跑的当前证据（可复现） | `validation/run_all.py` 隔离副本、`ctest`、`check_design.py`、`g++ -fsyntax-only` |
| **E2** | 仓库内已存在的历史实跑证据，**产生时间早于 HEAD** | `validation/obs/evidence_phase*.txt`(10-22)、`validation/cat/*.json`(10-22)、`validation/phot/evidence/`(10-22)、`validation/stats/evidence/`(commit `785adc3`) |
| **E3** | 只写在文档里的验收门槛，**没有对应实跑** | `docs/engineering/rebuild/VERIFICATION.md` 的 A01/B01/H01/D01/K01、REG01/LIFE01/SCHED01/PROC01/FORMAT01、POOL01/PIPE01/ASYNC01/COMPOSE01；`standards/SCHEDULER.md` 的 S-FLOOR…S-LIFE；`standards/CALIBRATION_WALKTHROUGH.md` 的 C-CAL-* |

case 缩写：`C01/U01/G01/F01/S01/W01/V01/E01/P01/R01/X01` = `validation/<case>/comparison.json`（本次实跑结果）。
`obs1/obs2/cat/geo/phot/stats` = 上表 E2 目录。`ctest` = `build/science` 的 4 个测试（本次实跑）。`—` = **没有证据**。

### 0.4 表 1 判定词定义

| 列 | 取值 | 判定依据 |
|---|---|---|
| contract driver 已解码可达 | 是 / 部分 / 否 | `apps/contract/driver.cpp:1663-1696` 的 30 条 `dispatch()` 分支；**部分** = 能解码但 operation 名是合并重载（`forward` 用 `point.type` 选重载），或只能由别的 operation 内部间接到达 |
| 科学实现状态 | 有真实实现 / 部分 / 缺失 | 函数体是否给出真实算法与误差/失败语义；**部分** = 存在具名能力分支返回 `unsupported`，或该实现被交付报告列为简化实现 |
| 合同状态 | 可复用 / 需重构 / 缺失 / 未验收 | **可复用**=实现语义可原样搬到 typed registry（只需 adapter）；**需重构**=算法在，但目标合同要求的精度/所有权/provider 可构造性/算法分支缺一项；**缺失**=生产侧没有可用实现；**未验收**=实现存在但 HEAD 级证据不足以支持“行为正确” |
| M 阶段归属 | M4 / M5 | 按 `standards/DEVELOPMENT.md:7-15`：M4 只含 `observation.calibrate` 的完整接入，其余 process 一律 M5（M6 是工作流组合，不在本列） |

### 0.5 关于“旧任务 #113—#122”的检索结论

任务要求把 #113—#122 的有效未完成项映射到 M0—M6。**在本 checkout 中它们不存在**，检索证据：

```
git log --all --format='%s%n%b' | grep -oE '#1[0-9][0-9]' | sort | uniq -c
#   180 #112   10 #106   10 #104    2 #141    2 #136    2 #111    2 #103
grep -rn '#11[0-9]\|#12[0-9]' docs/audit/*.md docs/engineering/rebuild/*.md validation/audit/*.md
#   仅 delivery-112.md 命中 #112
```

即：本仓库能追溯的旧 Issue 只有 **#103 / #104 / #106 / #111 / #112**（其中 #103/#136/#141 仅出现在不可达的旧引用里），`#113—#122` 与“旧 A—H 阶段”在 `git log`、`docs/audit/`、`docs/engineering/rebuild/`、`validation/audit/` 中**零命中**。

因此 §3 的表 3 按“**仓库内可追溯的有效未完成项**”建立，来源限定为：

1. `validation/audit/delivery-112.md` §5「剩余限制」13 条（唯一一份逐条列未完成项的自述）；
2. 11 个必需 case 的 `comparison.json` 中 `status != pass` 的逐条功能项；
3. 两份独立审核报告 `validation/audit/{cat,phot}_review.json` 的 `findings`/`not_run`；
4. `docs/engineering/rebuild/VERIFICATION.md`、`standards/{DEVELOPMENT,SCHEDULER}.md`、`standards/CALIBRATION_WALKTHROUGH.md` 中**尚未有任何实跑证据**的验收门槛（E3）。

如果负责人手里的 #113—#122 是 GitHub 上的 Issue 正文，请把正文投进仓库（或贴给我），我可以按同一张表逐条补映射；**在拿到正文之前，任何“#113—#122 已映射”的说法都是编造**。

---

## 1. 表 1：22 process × 57 operation 逐项矩阵

口径：`docs/engineering/rebuild/processes/README.md:5-28` 的白名单表（22 process / 57 operation，本文用脚本解析该表得到 57 行，见 §4.2）。

### 1.1 矩阵

| # | process_id | operation | 现有 C++ free 函数（文件:行） | driver 可达 | 科学实现 | 合同状态 | 证据 | M |
|---|---|---|---|---|---|---|---|---|
| 1 | observation.calibrate | calibrate_observation | `src/observation/calibrate.cpp:124` | 否 | 有真实实现 | 未验收 | obs1/obs2（E2，84/160 checks 0 失败，10-22）；X01 `normalize` 内部调用但整链未发布 | M4 |
| 2 | observation.regroup | regroup_observation | `src/observation/regroup.cpp:47` | 否 | 有真实实现 | 未验收 | obs1/obs2（E2，`binned[0]=410`）；无 HEAD 级证据 | M5 |
| 3 | coordinates.wcs | make_wcs | `src/coordinates/wcs.cpp:482` | 是 | 有真实实现 | 可复用 | C01 67 pass + 2 info（含 4 个具名失败负例） | M5 |
| 4 | coordinates.wcs | forward.center0 | `src/coordinates/wcs.cpp:810` | 部分 | 有真实实现 | 可复用 | C01（driver 以合并名 `forward` + `point.type` 选重载） | M5 |
| 5 | coordinates.wcs | forward.edge0 | `src/coordinates/wcs.cpp:819` | 部分 | 有真实实现 | 可复用 | C01（同上；`PixelEdge0` 分支） | M5 |
| 6 | coordinates.wcs | inverse | `src/coordinates/wcs.cpp:832` | 是 | 有真实实现 | 可复用 | C01（含背半球/域外具名失败） | M5 |
| 7 | coordinates.wcs | derivative | `src/coordinates/wcs.cpp:976` | 是 | 有真实实现 | 可复用 | C01（相对差 ≤2.7e-7） | M5 |
| 8 | coordinates.wcs | solid_angle_jacobian | `src/coordinates/wcs.cpp:1072` | 是 | 有真实实现 | 可复用 | C01（7 处调用）+ E01 引用 | M5 |
| 9 | coordinates.wcs | bind_wcs | `src/observation/bind.cpp:41` | 否 | 有真实实现 | 未验收 | `validation/obs/cases_bind.cpp`（E2） | M5 |
| 10 | catalogue.access | build_catalogue_index | `src/catalogue/index.cpp:289` | 否 | 有真实实现 | 未验收 | `validation/cat/catalogue_check.json`（E2） | M5 |
| 11 | catalogue.access | make_catalogue_provider | `src/catalogue/provider.cpp:576` | 否 | 有真实实现 | 未验收 | cat（E2） | M5 |
| 12 | catalogue.access | catalogue.query | `src/catalogue/provider.cpp:310` | 否 | 有真实实现 | 未验收 | cat（E2，含历元传播） | M5 |
| 13 | catalogue.access | catalogue.query_quads | `src/catalogue/provider.cpp:402` | 否 | 有真实实现 | 未验收 | `validation/cat/astrometry_check.json`（E2） | M5 |
| 14 | catalogue.access | catalogue.lookup | `src/catalogue/provider.cpp:526` | 否 | 有真实实现 | 未验收 | cat + `epoch_check.json`（E2） | M5 |
| 15 | sources.astrometry | solve_astrometry | `src/sources/astrometry.cpp:436` | 否 | 有真实实现 | 未验收 | `astrometry_check.json`（E2）；X01 盲解 4/8 帧「假定耗尽」 | M5 |
| 16 | sources.psf | fit_psf | `src/sources/psf_fit.cpp:69` | 是 | 有真实实现 | 需重构 | F01 pass（**仅成功计数**；参数数值盒=剖面区间，非 interval-Newton 根证书）；obs2（E2） | M5 |
| 17 | sources.psf | evaluate_psf | `src/sources/psf.cpp:125` | 否 | 有真实实现 | 未验收 | obs2（E2，max│Moffat 积分−参考│=5.4e-11） | M5 |
| 18 | sources.psf | bind_observation_model | `src/observation/bind.cpp:124` | 否 | 有真实实现 | 未验收 | `validation/obs/cases_bind.cpp`（E2） | M5 |
| 19 | sources.measure | measure_science_sources | `src/sources/measure.cpp:97` | 是 | 有真实实现 | 未验收 | F01 pass（成功计数；`variance` 在驱动里被写成 `-1.0`，见 §3.1 L26） | M5 |
| 20 | response.integrate | integrate_response | `src/response/api.cpp:364` | 是 | 有真实实现 | 未验收 | phot（E2，R1/R2 两条 PASS）；F01 调用但无比较行 | M5 |
| 21 | photometry.fit | fit_photometry | `src/photometry/api.cpp:165` | 是 | 有真实实现 | 未验收 | F01 调用但无比较行；phot（E2） | M5 |
| 22 | photometry.apply | apply_scale | `src/photometry/apply_scale.cpp:164` | 是 | 有真实实现 | 未验收 | F01 调用但无比较行；phot（E2，含 identity_conflict 负例） | M5 |
| 23 | geometry.overlap | make_footprint_provider | `src/coordinates/footprint_provider.cpp:155` | 是 | 有真实实现 | 可复用 | G01（driver 内先建 provider 再 `compute_overlap`） | M5 |
| 24 | geometry.overlap | detector_footprint | `src/geometry/overlap.cpp:153` | 否 | 有真实实现 | 未验收 | `validation/geo/geo_validation.cpp`（E2） | M5 |
| 25 | geometry.overlap | footprint | `src/geometry/overlap.cpp:289` | 否 | 有真实实现 | 未验收 | geo（E2，曲边边界） | M5 |
| 26 | geometry.overlap | measure | `src/geometry/overlap.cpp:354` | 是 | 有真实实现 | 可复用 | G01（`drop_area` 与独立真值同区间） | M5 |
| 27 | geometry.overlap | compute_overlap | `src/geometry/overlap.cpp:391` | 是 | 部分 | 需重构 | G01 pass（**仅矩形收集域**）；非矩形/含洞在 `overlap.cpp:417-428` 具名 `unsupported` | M5 |
| 28 | geometry.overlap | refine_geometry | `src/geometry/refine.cpp:29` | 否 | 部分 | 需重构 | G01 经 `validation/native/g01_driver.cpp:325` 的**测试用** provider 通过；生产 provider 不提供 | M5 |
| 29 | geometry.overlap | footprint.forward_interval | `src/coordinates/footprint_provider.cpp:23` | 否 | 有真实实现 | 未验收 | 无 op 级证据；被 cut-cell kernel 内部使用（`src/geometry/kernel.cpp:132`） | M5 |
| 30 | geometry.overlap | footprint.boundary_interval | `src/coordinates/footprint_provider.cpp:81` | 否 | 部分 | 需重构 | `(void)parameter`（`footprint_provider.cpp:84`）：只取 Bernstein 控制点凸包，**不按 parameter 区间求值**，上界更强但证书口径未声明 | M5 |
| 31 | geometry.overlap | footprint.derivative_interval | `src/coordinates/footprint_provider.cpp:53` | 否 | 有真实实现 | 未验收 | `validation/geo/perf_geometry.cpp`（E2） | M5 |
| 32 | geometry.overlap | refinable.open | **无生产实现**：`src/workflows/frames_provider.cpp:85`、`src/workflows/product.cpp:667` 均返回 `unsupported` | 否 | 缺失 | 缺失 | 两处具名 `unsupported`（`the provider carries no refinable geometry closure`） | M5 |
| 33 | geometry.overlap | refinable.refine | `src/geometry/refine.cpp:29`（free）/ `include/acsd/model.hpp:353`（virtual） | 否 | 部分 | 需重构 | 同 #28；且 free 函数与 virtual 是同一算法的两处入口，需收口 | M5 |
| 34 | operators.action | make_action | `src/operators/api.cpp:36` | 否 | 有真实实现 | 可复用 | U01（native 驱动构造 R 的 CSR action）+ ctest `operators_selftest` | M5 |
| 35 | operators.action | apply | `src/operators/api.cpp:193` | 是 | 部分 | 可复用 | U01 pass；`solve` recipe 在 `src/operators/linear_action.cpp:235` 具名 `unsupported` | M5 |
| 36 | operators.action | transpose_apply | `src/operators/api.cpp:201` | 是 | 部分 | 可复用 | U01 pass（真转置 0.5/0.375）；`solve` 同 #35 | M5 |
| 37 | operators.action | compose | `src/operators/api.cpp:209` | 否 | 有真实实现 | 可复用 | ctest `operators_selftest`（本次实跑 pass） | M5 |
| 38 | operators.action | action.dependency_blocks | `src/operators/linear_action.cpp:433` | 否 | 有真实实现 | 未验收 | f01/s01/w01 驱动使用；无独立比较 | M5 |
| 39 | operators.action | action.serialize_recipe | `src/operators/linear_action.cpp:448` | 否 | 有真实实现 | 未验收 | ctest selftest + s01/w01 驱动使用 | M5 |
| 40 | operators.action | action.row_enclosure | `src/operators/linear_action.cpp:450` | 否 | 部分 | 需重构 | 仅 CSR/leaf_merge（`:451` 具名 `unsupported`） | M5 |
| 41 | uncertainty.covariance | make_covariance | `src/uncertainty/api.cpp:94` | 是 | 有真实实现 | 可复用 | U01（同源根合并） | M5 |
| 42 | uncertainty.covariance | covariance_apply | `src/uncertainty/api.cpp:217` | 是 | 有真实实现 | 可复用 | U01（列 [3.75,2]、[2,10.3125]） | M5 |
| 43 | uncertainty.covariance | quadratic_form | `src/uncertainty/api.cpp:254` | 是 | 有真实实现 | 可复用 | U01（**4.328125 = 277/64 精确**，区间 [4.328125,4.328125]） | M5 |
| 44 | uncertainty.covariance | geometry_covariance_bound | `src/uncertainty/api.cpp:315` | 否 | 有真实实现 | 未验收 | —（仅 `validation/lib/availability.py` 提到名字） | M5 |
| 45 | uncertainty.covariance | control_covariance | `src/information/information.cpp:585` | 是 | 有真实实现 | 未验收 | 无观察侧调用（S01 只调 `fit_relative_sky`） | M5 |
| 46 | geometry.drizzle | build_drizzle | `src/geometry/drizzle.cpp:80` | 否 | 有真实实现 | 可复用 | G01 `native` 驱动 drizzle 行 5/5 pass（`bound_validity=certified`） | M5 |
| 47 | information.reference | estimate_flux_information | `src/information/information.cpp:151` | 否 | 有真实实现 | 未验收 | `validation/stats/information_test`（E2） | M5 |
| 48 | information.scene | evaluate_scene_variance | `src/information/information.cpp:442` | 否 | 有真实实现 | 未验收 | stats（E2） | M5 |
| 49 | information.scene | build_supported_triangulation | `src/information/triangulation.cpp:534` | 否 | 有真实实现 | 可复用 | V01 pass（12/12） | M5 |
| 50 | information.scene | reconstruct_scene_variance | `src/information/information.cpp:663` | 否 | 有真实实现 | 可复用 | V01 pass（仿射真值 4.44e-16；无曲率界只给条件预测） | M5 |
| 51 | sky.controls | build_sky_controls | `src/sky/sky.cpp:729` | 否 | 有真实实现 | 未验收 | 无观察侧调用（S01 未覆盖） | M5 |
| 52 | sky.fit | fit_relative_sky | `src/sky/sky.cpp:1232` | 否 | 有真实实现 | 可复用 | S01 `common_sky_protected` pass（KH=0 ≤1e-10） | M5 |
| 53 | sky.fit | apply_sky | `src/sky/sky.cpp:1792` | 否 | 有真实实现 | 未验收 | 无观察侧调用 | M5 |
| 54 | coadd.select | select_samples | `src/coadd/coadd.cpp:436` | 否 | 有真实实现 | 未验收 | W01 pending（观察侧到 `solve_weights` 即失败，选择未单独验收） | M5 |
| 55 | coadd.weights | solve_weights | `src/coadd/coadd.cpp:734` | 否 | 部分 | 需重构 | W01 2/2 pending：「分母无正下界」「等式子问题无正归一」 | M5 |
| 56 | coadd.integrate | coadd | `src/coadd/coadd.cpp:1008` | 否 | 有真实实现 | 未验收 | W01 pending（未到达三阶段联合） | M5 |
| 57 | projection.export | build_export | `src/projection/export_projection.cpp:127` | 是 | 有真实实现 | 可复用 | E01 pass（11/11，含 FITS/闭包往返） | M5 |

### 1.2 统计（脚本核对，见 §4.2 的解析口径）

| 维度 | 计数 |
|---|---|
| process / operation | 22 / **57** |
| 合同状态 | 可复用 **20**、需重构 **7**、缺失 **1**、未验收 **29** |
| 科学实现 | 有真实实现 **48**、部分 **8**（#27、#28、#30、#33、#35、#36、#40、#55）、缺失 **1**（#32）。#35/#36 的“部分”只指 `solve` recipe 未接入，其余 recipe 有真实实现 |
| driver 可达 | 是 **19**、部分 **2**（`forward.center0/edge0`）、否 **36** |
| M 阶段 | M4 **1**（calibrate_observation）、M5 **56** |

需重构的 7 项：#16 `fit_psf`、#27 `compute_overlap`、#28 `refine_geometry`、#30 `footprint.boundary_interval`、#33 `refinable.refine`、#40 `action.row_enclosure`、#55 `solve_weights`。
缺失的 1 项：#32 `refinable.open`（生产无 provider 可返回）。

---

## 2. 表 2：基建逐项矩阵

状态列写法：`ea984cdb → cea5e3650`。**“已声明”不等于“已实现”**：`cea5e3650` 只加了头文件与 CMake 目标，`src/runtime/` 下只有 `internal.hpp`（299 行内部合同），**没有任何 `.cpp`**。

| # | 目标 | 目标章节 | 当前状态（HEAD `cea5e3650`） | 差距 | 阶段 |
|---|---|---|---|---|---|
| B01 | runtime 头 `types.hpp`（Key/InvocationIds/ResourceVector/ScopeLimits/ScopeSnapshot/InvocationContext/TaskState/TaskSpec/StageSpec/RegionSpec/InvocationPlan/PlanOptions/PlanBounds/TypeId/WireRequest/WireResponse/ProbeTags…） | FOUNDATION_API §文件与类型所有权、§资源与数组所有权、§计划、注册和Host | `types.hpp` 205 行**已声明**；`g++ -std=c++20 -fsyntax-only` 通过（含聚合头）；`Services/ScopeState/CancelState` 仅前置声明 | 无 `Services`/`ScopeState` 定义、无 scope 树与 arena；plan/region 只有字段没有 `plan()` 生产者 | M1 |
| B02 | runtime 头 `task.hpp`（`Task<T>`/`ready`/`submit_cpu`/`then`/`settle`/`when_all`/`settle_all`/`wait_external`/`request_cancel`） | FOUNDATION_API §Task、错误与取消的精确规则 | `task.hpp` 346 行**已声明** | 无 continuations、无 `work_reserved` 转换、无 suspend/resume、无 CPU token 归账、无 `wait_external` 阻塞策略 | M2 |
| B03 | runtime 头 `memory.hpp`（`Arena`/`Claim`/`Block<T>`/`Buffer<T>`/`allocate`/`freeze`/`ArrayLayout`/`BuildSpec`） | FOUNDATION_API §资源与数组所有权 | `memory.hpp` 112 行**已声明** | 无 claim/commit 三态票据、无 arena 关闭语义；现有 `Storage` 只做 `copy/read/open` | M1 |
| B04 | runtime 头 `pipe.hpp`（`PipeSpec`/`Pipe<T>`/`make_pipe`/`broadcast`/`PipeEnd`/`Delivery`） | FOUNDATION_API §Pipe与广播 | `pipe.hpp` 58 行**已声明** | 无实现；无 region/edge 绑定校验、无 frontier/ordered key、无 EOF/failed/cancelled 三态 | M2 |
| B05 | runtime 头 `storage.hpp`（`begin_async`/`read_async`/`append_async`/`seal_async`/`abort_async`/`WriterRef`） | FOUNDATION_API §资源与数组所有权（异步 Storage） | `storage.hpp` 22 行**已声明** | **零实现**；现行 `LocalStorage`（`src/storage/local_storage.cpp:48-240`）是同步 `copy/read/open` | M3 |
| B06 | runtime 头 `process.hpp`（`ProcessDescriptor`/`Operation`/`TypeCodec`/`RegistryBuilder`/`Registry`/`ProcessHost`/`make_services`/`open_command`/`make_host`/`run_region`/`admit_epoch`/`WireObjects`/`publish_wire_response`） | FOUNDATION_API §计划、注册和Host；§引导与关闭入口；§重规划、region与清理 | `process.hpp` 450 行**已声明** | 无 registry/host 实现、无状态机、无 `invoke_wire` thunk；`CMakeLists.txt:71-72` 已加 `runtime` 目标，但该目标下只有 `src/runtime/internal.hpp`，没有 `.cpp` | M2（Host）／M4（首个 typed 接入） |
| B07 | codec / manifest 生成（`generated::type_codec<T>`、`register_builtin_types`、`process-manifest.json`、`builtin_processes.cpp`） | FOUNDATION_API §当前科学操作的typed输入标准；DEVELOPMENT §完整模块包与自动登记产物 | **部分**：`tools/generate_contract_schema.py` 从声明生成 302 个 defs / **78 operation variants**（`--check` 在 `ea984cdb` 隔离副本通过，§5.2）；`schemas/science-input.schema.json` 是唯一产物 | 无 `TypeCodec` 注册表、无 `register_builtin_types`、无 process manifest、无能力表、无文档覆盖校验；生成器只做“形状”，不做 TypeId/codec/owner 校验 | M0（形状已生成）／M2（registry 生成） |
| B08 | 唯一 CPU 池 | SYSTEM §Task、异步I/O与就绪调度；SCHEDULER §多command分配与公平性 | **部分**：`src/resources/cpu_resources.cpp:36-70` 每 `CpuResources` 一条固定 worker 池，`parallel_for`（`:135-170`）由调用线程参与 | 无 ready/waiting 队列、无 weighted round-robin、无 token/credit；`parallel_for` 的参与 worker 数**不受传入 `threads` 严格约束**，`job_active_` 计数与实际参与数可以不等（潜在挂起/超售） | M2 |
| B09 | 资源账本（held/claimed/allocated/peak+scope 树） | FOUNDATION_API §资源与数组所有权；SCHEDULER §资源账本的原子操作 | **部分**：`reserve(bytes,threads)` 原子检查并在失败时回滚（`cpu_resources.cpp:75-117`）；`consume_work`、`usage()`（`:110-133`） | 无 scope 树/子域归因、无 held/claimed/allocated 三态、无 arena 复用；**`consume_work` 先 `fetch_add` 再判上限，失败已扣额度**（`cpu_resources.cpp:111-114`），与 SCHEDULER「失败无副作用、不改账」冲突 | M1 |
| B10 | Task（`Task<T>` 生命周期与组合） | FOUNDATION_API §Task；EXECUTION §虚拟计算池的就绪队列与异步流水线 | **仅声明**（B02） | 无实现 | M2 |
| B11 | Pipe（有界强类型块通道与背压） | SYSTEM §内存管道与背压；FOUNDATION_API §Pipe与广播 | **仅声明**（B04） | 无实现 | M2 |
| B12 | 异步 I/O（read/append/seal/abort + I/O lane） | EXECUTION §E01/E02；SCHEDULER §异步状态与锁顺序 | **缺失**：现行 Storage 是同步调用 | 无 I/O lane、无信用、无短读短写/磁盘满处理；`append_async` 的“一次一个在途写”无从验证 | M3 |
| B13 | Storage 封存（`canonical_array_v1`、2^20 规范块、dyadic 1024、-0/NaN 规范化、原子 publish） | OBJECTS §数组封存、规范块与科学内容身份；§当前格式的发布与恢复 | **部分**：`copy`（`:48`）造 `BlockDescriptor`+`raw_sha256`+zstd；`open`（`:168-240`）校验覆盖/字节/hash；产品层 `src/workflows/product_io.cpp` 有 staging/atomic rename | 无规范块重分块、无 dyadic 归一、无 begin/append/seal/abort、无 `canonical_array_v1` 算法身份；封存粒度由产品层决定，不是规范决定 | M3 |
| B14 | ProcessHost（resolve/validate/plan/admit/run/finalize/cleanup） | FOUNDATION_API §统一生命周期（SYSTEM §统一生命周期） | **缺失**（B06 只有声明） | 无状态机、无 usage/Assessment 合并、无 partial 封存规则实现 | M2→M4 |
| B15 | typed registry（`(process_id,operation_id)` 唯一键、装配期拒绝重复/环/缺 codec） | SYSTEM §一次登记与typed绑定；FOUNDATION_API §计划、注册和Host | **缺失**（仅声明 `RegistryBuilder::register_type/begin/add/freeze`） | 无 22 process 登记、无 57 operation 的 TypeId、无 freeze 校验 | M0（类型）／M2（registry） |
| B16 | DEV_MODE 探针（`PerfRecord`/`SnapshotPayload`/`ACSD_DEV_SNAPSHOT`/`ACSD_DEV_PERF`/选择器/sink） | FOUNDATION_API §性能与中间数据入口；API §执行选择与内部诊断协议；EXECUTION §E04 | **部分**：`diagnostics.hpp` 97 行声明 + `CMakeLists.txt:10` `option(ACSD_DEV_MODE ... OFF)` + `:89-91` 传播 `ACSD_DEV_MODE=0/1` | 无 sink、无选择器、无配额/flush/失败隔离；OFF 零求值只在宏层成立（无调用点可验） | M3 |
| B17 | VirtualComputePool（compute/memory/io/transfer lane、cancellation/completion） | SYSTEM §虚拟计算池与硬件抽象；EXECUTION §虚拟计算池的就绪队列与异步流水线 | **缺失**：`grep -rn 'VirtualComputePool' include src` 无命中；唯一近似是 `CpuResources` | 无 lane 抽象、无统一准入、无 transfer/device 域；GPU 扩展无挂载点 | M1→M2 |
| B18 | GPU 扩展合同（`KernelExecutor`/`KernelBatch`/`BackendBuffer`/`ArrayBacking`/`Completion`/`ExecutionPolicy`） | EXECUTION §后端与未来GPU的执行边界（E03/E05）；SYSTEM §调度、多线程与未来后端 | **缺失（按设计）**：`include/acsd/runtime/*.hpp` 中**没有**这些类型（`grep` 只命中 `WireObjects`）；文档明确“本期不实现 GPU” | 无声明、无计费域、无事件/完成语义；请求 GPU 的失败/回退语义未实现 | 不在 M0—M6（设计保留） |

### 2.1 表 2 的三条硬结论

1. **M0 的“公共声明”已经完成，M0 的“可编译/可装配”还没有**：`contracts.hpp` 聚合头 `g++ -fsyntax-only` 通过（本次实跑），但 `registry`/`codec`/`manifest` 的生成物（DEVELOPMENT M0 门槛）不存在，57 项 operation 与文档的逐项匹配也没有机器检查。
2. **M1/M2/M3 全部为 0**：账本三态、arena、Task、Pipe、异步 I/O、CPU token、Host 状态机、诊断 sink 都只有声明。**不能**把 `runtime` 目标能 configure 当作 M1—M3 有进展（`cmake -S . -B /tmp/headbuild -G Ninja -DACSD_BUILD_SCIENCE=ON` exit 0，0.9 s，仅证明 CMake 图能生成）。
3. 现行可复用底座只有两块：`CpuResources`（bytes/threads/work 三个原子计数器 + 固定 worker 池）和 `LocalStorage`（同步 copy/read/open + raw/zstd + sha256）。它们**可以**作为 M1/M3 的起点，但必须按 §2 的差距改造，不能直接包一层 `Task` 就当达标。

---

## 3. 表 3：旧任务／既有证据中的有效未完成项 → M0—M6

### 3.1 映射表

“有效”= 仍能在当前代码或当前产物中定位到缺失/失败事实；“已闭合”= 本次抽查确认 HEAD 已修（见 §3.2）。

| 编号 | 来源（可复核） | 有效未完成项 | 影响 | M 阶段 | 验收出口 | 状态 |
|---|---|---|---|---|---|---|
| L01 | `validation/audit/delivery-112.md:149-150` | **Windows 未编译、未运行**（只有构建配置；Linux amd64 单平台） | 双平台可完成 baseline 不成立 | **M6** | DEVELOPMENT M6「双平台可完成 baseline」；VERIFICATION §性能与资源验收 | 未完成 |
| L02 | 同上 `:151-152` | **真实 HST/M42 数据未验**（全部合成输入 + astropy/wcslib/healpix/scipy） | 物理适用性未知 | **M6** | VERIFICATION §从物理观测构造合成真值（真实数据另计） | 未完成 |
| L03 | 同上 `:153` | **Huber bootstrap 未验**（Huber 默认关，显式开启后的聚类 bootstrap 未做） | `fit_photometry` 的稳健分支无证据 | **M5** | F01 扩展 + `phot` 证据重跑 | 未完成 |
| L04 | 同上 `:154-157`；`validation/x01/comparison.json` | **`acsd normalize` 端到端 published=0**：8 帧全部具名失败（光谱基无效 2、PSF 设计矩阵秩亏 1、像元积分 1、盲解假定耗尽 4） | P-small/P-large 性能基线无法给出；X01 不可能 pass | **M6** | X01；VERIFICATION §性能与资源验收 | 未完成 |
| L05 | `validation/run_summary.json`；`validation/*/comparison.json` | **11 个必需 case：7 pass / 0 fail / 4 pending**（W01/P01/R01/X01） | “7 pass”不能覆盖全矩阵 | **M6** | VERIFICATION §必需验证矩阵 | 部分（见 §6） |
| L06 | `apps/contract/driver.cpp:1663-1696`（本次重数） | **合同驱动 30 个 operation 名**，其中只有 19 个精确命中 57 白名单 + `forward` 覆盖 2 个重载 = **21/57 可达，36 个不可达**；另外 11 个名字不属于白名单：3 个纯值转换（`to_fits`、`from_fits_center`、`from_fits_edge`）、3 个 storage service（`storage.copy/read/open`）、2 个 product service（`open_product`/`publish_product`）、`export_signal`、`export_image`，以及合并重载名 `forward`（它覆盖 2 个白名单 operation） | 用户请求“驱动覆盖 30/78”必须改成“21/57”；36 个科学操作只能走 native/未接线路径 | **M0**（typed registry）/ **M4**（driver 改同一 registry） | API §注册调用与科学操作归属；VERIFICATION PROC01 | 未完成 |
| L07 | delivery-112 §5.7 | **极区 north_polar：A 路线顶区版（不依赖 chart 导数）设计已定、未实现**；TAN 在天极无定义 | 极区科学域只能用具名失败表达 | **M5** | G01 `north_polar` 分支 + VERIFICATION（touch/tiny/极区） | 未完成 |
| L08 | delivery-112 §5.8 | **Drizzle 块级路径的归属误差未进分子**（中心归属 vs 精确交叠 max 1.0、RMS 0.1055）；默认逐像元路径正确但块级不得声称权重正确 | 吞吐路径不可用 | **M5** | G01/U01 drizzle 反例 | 部分（默认路径可复用） |
| L09 | delivery-112 §5.9 | **参数数值盒是简化实现**（剖面区间，不是完整 interval-Newton 根证书） | `fit_psf`/`fit_photometry` 的 certified 声明过强 | **M5** | F01 `counts_scale_interval` 反例 | 未完成（→ 表 1 #16 判“需重构”） |
| L10 | delivery-112 §5.10 | **稀疏 QR/LSQR 未实现**（大星数走稠密分解，超预算只返回失败原因） | 大 K/大参数规模无路可走 | **M5** | EXECUTION §协方差和拟合复杂度；W01/S01 大 N | 未完成 |
| L11 | delivery-112 §5.11；`src/geometry/overlap.cpp:417-428` | **非矩形收集域不支持**：`compute_overlap` 对非矩形/含洞足迹整体 `unsupported` | 真实探测器足迹（crop/bin/Bayer/环洞）不可用 | **M5** | G01 严格支持 + OBJECTS §原生身份、足迹 | 未完成（→ 表 1 #27 判“需重构”） |
| L12 | delivery-112 §5.12；`validation/apps/fits_cfitsio_probe.txt` | **CFITSIO 写路径不可用**（`ffppx` 报成功但数据单元全零），FITS 写出由项目内实现承担 | Windows/MSVC 与 CFITSIO 组合未验 | **M3/M6** | E01 + 双平台 FITS 往返 | 部分 |
| L13 | delivery-112 §5.13 | **共享工作树并发施工导致全树构建失败**（本次审计再次实测：21:04 `ctest design_integrity` 因并行改 `contracts.hpp` 失败一次，`cea5e3650` 落地后 21:27 复跑 4/4 通过，§5.2） | 证据可重复性 | **M0**（流程） | AGENTS 交付纪律 | 未解决（环境性） |
| L14 | `validation/f01/comparison.json` | **F01 `two_source_aperture_flux` pending**：驱动测的是“每源 PSF 窗内积分”，不是请求盒的孔径积分 | F01 的孔径/方差/共享 latent 期望未验 | **M5** | F01 case.json 的 3 正例 + 5 负例 | 未验收 |
| L15 | 同上（F01 只有 1 行 pass） | **F01 的 `pass` 只是成功计数**：case 声明 3 正例 + 5 负例、tolerance 1e-12，`comparison.json` 只有 “fit_psf_and_measure = 2 successes” 一行；通量/方差的数值比较、`finite_window_not_renormalised`、`shared_floor_not_independent`、`colour_position_rank`、`unknown_gain`、`counts_scale_interval` 均无对应行 | **F01 不得被引用为“光度数值通过”** | **M5/M6** | 补齐逐期望比较 | 未验收 |
| L16 | `validation/s01/comparison.json` | **S01 `holdout_variance` pending**（独立探针无法物化 `P`：公开 action 拒绝该单位向量）+ S01 只有 2 行（声明 4 正例 + 3 负例），`build_sky_controls`/`apply_sky`/`control_covariance` 无观察侧调用 | S01 的留出方差、交叉项、共线零空间、异响应不造边均未验 | **M5** | S01 case.json 全项 | 未验收 |
| L17 | `validation/w01/comparison.json` | **W01 BLUE/QP 权 pending**：`the normalising denominator has no positive lower bound`、`the equality constrained sub problem has no positive normaliser`；`select_samples`/`coadd` 未到联合验收 | 选择/权/合成整条统计路径无验收 | **M5** | W01（N=1..7、Γ 解析 BLUE/QP、LOO 100000 trials） | 未完成（→ 表 1 #55 判“需重构”） |
| L18 | `validation/p01/comparison.json` | **P01 `negative:unknown_recipe` pending**（需走 contract dispatch 才能复现；其余 6 项负例 pass） | 当前格式的未知 recipe 拒绝未在公开路径验证 | **M3/M5** | OBJECTS §当前格式；FORMAT01 | 未验收 |
| L19 | `validation/r01/comparison.json` | **R01 `cache_switch_reachable` pending**（配置 schema 无缓存开关，缓存状态无法从外部改变） | R01 矩阵少一维；缓存正确性未验 | **M3/M6** | EXECUTION §E01/E02 cache key | 未验收 |
| L20 | `validation/x01/truth_x01.py:369-372` | **X01 的 `pending` 是硬编码**：`reason="the three command entry points are not available yet"` 与 `normalize/mosaic/export_image` 已在 `src/` 定义、CLI 实跑过（delivery-112）矛盾；观察侧从未接线 | X01 覆盖为 0，且理由会误导 | **M6** | 重写 X01 观察侧（CLI + 产品） | 未验收 |
| L21 | `validation/native/g01_driver.cpp:323-338,588-611`；`src/workflows/frames_provider.cpp:85`、`src/workflows/product.cpp:667` | **`refine_geometry` 只在验证侧成立**：G01 用测试用 `SnapshotProvider` 行使 refine；生产 Frames/Product provider 的 `open_refinable_geometry` 一律 `unsupported`；`refinable.open` 无生产实现 | 几何细化在生产路径不可达 | **M2/M5** | PROC01；VERIFICATION G01「refine」 | 未完成（→ 表 1 #28/#32/#33） |
| L22 | `src/operators/linear_action.cpp:230-238,450-456` | **`apply`/`transpose_apply` 的 `solve` recipe 具名 unsupported；`row_enclosure` 仅 CSR/leaf_merge** | OBJECTS §solve recipe 的真转置无法行使；`row_enclosure` 的“缺能力返回 unsupported”被当作常态 | **M5** | OBJECTS §solve recipe 的唯一载荷和真转置 | 未完成 |
| L23 | `src/resources/cpu_resources.cpp:111-114` | **`consume_work` 失败仍扣额度**（`fetch_add` 先于上限检查）；`parallel_for` 不受传入 `threads` 严格约束 | S-WORK/归因反例必失败；M1 账本语义不成立 | **M1** | SCHEDULER §资源账本的原子操作；VERIFICATION A01 | 未完成 |
| L24 | `validation/obs/evidence_phase1_calibration_regroup.txt`、`evidence_phase2_psf_measurement.txt` | **obs 证据已过期**：phase1 文本仍写 `[psf] not implemented yet` / `[measurement] not implemented yet`，而 `src/sources/{psf,psf_fit,measure}.cpp` 已实现；两份证据都是 2026-10-22、早于 HEAD | 校准/重整/PSF/测量的“84/160 checks 0 失败”不能用于 HEAD 认证 | **M6** | 重跑 `validation/obs/run_validation.sh` | 过期 |
| L25 | `validation/stats/evidence/validation_run.txt`（头注 commit `785adc34b`） | **stats 证据过期**：记录的是 `785adc3` 树，不是 HEAD；`information_test`/`sky_test`/`coadd_test` 的 `0 failure(s)` 不能直接引用 | 信息/天光/合成三模块无 HEAD 级证据 | **M6** | 重跑 `validation/stats/run_validation.sh` | 过期 |
| L26 | `validation/native/f01_driver.cpp:481` | **wire 里 `null` variant 被编码成 `-1.0`**（`flux.variance.value ? *… : -1.0`），与 API.md「optional 为空则 null」冲突；导致 F01 的 variance 语义从未被真正检查 | 统一 codec 的反例；M0 wire 合同偏差 | **M0** | API §JSON 对象编码；H01 | 未完成 |
| L27 | `docs/engineering/rebuild/VERIFICATION.md:69-79` | **A01/B01/H01/D01/K01 无任何生产服务实跑**（调用归因、>H 流式输出、规范身份、探针 OFF/ON、CPU 批次/回退） | M1/M3 的验收没有出口 | **M1/M3** | 同左 | 未实施 |
| L28 | `VERIFICATION.md:98-110` | **REG01/LIFE01/SCHED01/PROC01/FORMAT01 未实施**（登记拒绝、生命周期、共享执行、单模块维护、当前格式） | registry/Host 没有验收出口 | **M0/M2/M4** | 同左 | 未实施 |
| L29 | `VERIFICATION.md:113-122` | **POOL01/PIPE01/ASYNC01/COMPOSE01 未实施** | 池/管道/异步/组合没有验收出口 | **M2/M3** | 同左 | 未实施 |
| L30 | `standards/SCHEDULER.md:104-117` | **S-FLOOR / S-FORK / S-ORDER / S-MULTI / S-WORK / S-IO / S-BARRIER / S-LIFE 八条反例全部未跑** | M2 的“可保证进展/背压/公平”没有证据 | **M1/M2** | 同左 | 未实施 |
| L31 | `standards/CALIBRATION_WALKTHROUGH.md:139-153` | **C-CAL-1/2/DQ/H/ORDER/CLEAN/WRITE/ROOT/DEV 九条固定 case 未跑**（只有 E2 的 obs 校准小例） | M4 门槛不成立 | **M4** | 同左 | 未实施 |
| L32 | `docs/engineering/rebuild/standards/DEVELOPMENT.md:9-15` | **M0 门槛未满足**：57 operation 与文档逐项匹配、未知 key/type 负例拒绝、无重复聚合头（聚合头已拆，但**生成 registry/manifest 不存在**） | M0 无法签字 | **M0** | M0 行 | 部分 |
| L33 | 本次实测 `include/acsd/model.hpp:199` | **声明缺陷：`dependency_blocks(BlockRequest,bool transpose)` 的第一个参数没有名字**；FOUNDATION_API/API.md 要求“驱动 arguments = 参数名”，无名字就无法机械生成 Input 字段（footprint/`refinable` 的同名参数也依赖 API 的固定字段表兜底） | 57 operation 的 typed Input 生成会在 `action.dependency_blocks` 卡住 | **M0** | FOUNDATION_API §当前科学操作的typed输入标准 | 未完成 |
| L34 | 本次实测 `include/acsd/model.hpp:182-186` | **`FootprintProvider` 的 4 个虚方法参数全部未命名**（`forward_interval(Context&,Box)` 等），只有 API.md 的固定字段表给出 `domain/curve/parameter` | 生成器无法从声明推导；两套字段来源有分叉风险 | **M0** | API §构造操作、顺序和单位 | 未完成 |
| L35 | `validation/audit/{cat,phot}_review.json` 的 `not_run` | **审核者自述未跑**：R01 的多线程/spill/取消/写失败、X01、S01/W01/V01/E01/P01 的观察侧、GUI 进程本体、Windows、真实 HST/M42、Huber bootstrap；phot 审核的 response/photometry 部分是**自审** | “独立审核”的覆盖面比标题窄 | **M6** | 重跑审核范围 | 未完成 |
| L36 | `validation/phot/evidence/phot_validation.txt`（10-22） | phot 11 条 case 全 PASS，但**早于 HEAD**，且 `P2_photometry_rank_deficient` 只断言 `code==7`（枚举位置敏感） | `fit_photometry` 的秩/方差分支不能引用为当前证据 | **M5/M6** | 重跑 + 用 `Code::not_identifiable` 名字断言 | 过期/脆弱 |

### 3.2 已闭合的旧审查项（本次抽查证据）

两份独立审核共 19 条（`validation/audit/cat_review.md` 9 条、`phot_review.md` 10 条），delivery-112 §4 称全部有处置。本次**按代码抽查 6 条**，均在 HEAD 闭合：

| 旧发现 | 抽查证据 |
|---|---|
| stdout 被 `printf("DEBUG …")` 污染 | `grep -rn 'printf("DEBUG' src` 无命中；`grep -rn 'std::cout' src` 亦无命中 |
| `propagate_star` 两份实现（违反 pmRA·cosDec） | 唯一实现 `src/catalogue/sky_geometry.cpp:82`，其余均为调用（`src/catalogue/provider.cpp:279,348,362,559`） |
| 目录历元语义不一致（lookup 用观测历元） | `query` 传播到请求历元（`provider.cpp:348,362`），`lookup` 用源自身历元（`provider.cpp:559`），与 OBJECTS `CatalogueRow` 条款一致 |
| CLI 缺键 → SIGABRT(134) | `apps/cli/commands.cpp:960-966,1195,1262` 捕获 `json::exception` → `kInvalidInput`(2) |
| PSF 退化 ρ→±1 静默返 0 | `src/sources/psf_model.cpp:230-231,291-293` 判 `!(std::fabs(rho)<1)` / `1−rho²>0` 返回 `invalid`，不再返 0 |
| `CovarianceAction.influence` 被忽略 | `src/uncertainty/api.cpp` 的 `covariance_apply` 走 influence 组合（U01 列 [3.75,2]/[2,10.3125] 与解析值一致） |

---

## 4. 表 4：57 operation ↔ 现有声明签名 / typed Input 字段

### 4.1 逐项对照

typed 规则（FOUNDATION_API §当前科学操作的typed输入标准）：Input = 公开声明的**非 Context 参数按参数名**收进；`std::span<T>` 换成 `Ref<std::vector<T>>`；对象 `Ref` 保持原类型；最后一个名为 `request` 的参数保持该科学 Request 类型；输出 = `Result<T>` 里的 `T`。下表“Input 字段”列给出机械生成结果，`{…}` 内是例外（FOUNDATION_API 明确写死）。

| # | operation | 现有声明（`include/acsd/model.hpp`） | typed Input 字段 | 备注 |
|---|---|---|---|---|
| 1 | calibrate_observation | `:466` `(Context&, Ref<RawObservation> raw, const Calibration& request)` | **例外** `CalibrateInput{raw:Ref<RawObservation>, request:Calibration}` | FOUNDATION_API §校准条 |
| 2 | regroup_observation | `:467` `(Context&, Ref<NativeObservation> observation, const RegroupRequest& request)` | `{observation, request}` | COMPUTE_MODULES 称 `RegroupInput` |
| 3 | make_wcs | `:448` `(Context&, const HeaderFields& header, Box domain, const AstrometricContext& astrometric_context, Id detector_frame, const NumericBudget& request)` | `{header, domain, astrometric_context, detector_frame, request}` | |
| 4 | forward.center0 | `:454` `(Context&, Ref<WcsModel> wcs, PixelCenter0 point, const NumericBudget& request)` | `{wcs, point:PixelCenter0, request}` | 重载按名拆，不靠缺字段猜 |
| 5 | forward.edge0 | `:455` `(Context&, Ref<WcsModel> wcs, PixelEdge0 point, const NumericBudget& request)` | `{wcs, point:PixelEdge0, request}` | 同上 |
| 6 | inverse | `:456` `(Context&, Ref<WcsModel> wcs, SkyICRS sky, const NumericBudget& request)` | `{wcs, sky, request}` | |
| 7 | derivative | `:457` `(Context&, Ref<WcsModel> wcs, PixelCenter0 point, const NumericBudget& request)` | `{wcs, point, request}` | |
| 8 | solid_angle_jacobian | `:458` `(Context&, Ref<WcsModel> wcs, Box domain, const NumericBudget& request)` | `{wcs, domain, request}` | |
| 9 | bind_wcs | `:461` `(Context&, Ref<NativeObservation> observation, Ref<WcsModel> wcs, Ref<AstrometricSolution> solution)` | `{observation, wcs, solution}` | |
| 10 | build_catalogue_index | `:468` `(Context&, const CatalogueBuildRequest& request)` | `{request}` | |
| 11 | make_catalogue_provider | `:469` `(Context&, Ref<CatalogueIndex> index)` | `{index}` | |
| 12 | catalogue.query | `:292` 虚方法 `(Context&, const CatalogueQuery& request)` | `{provider:Ref<CatalogueProvider>, request}` | provider 引用按 FOUNDATION_API 强持 |
| 13 | catalogue.query_quads | `:293` `(Context&, const QuadQuery& request)` | `{provider, request}` | |
| 14 | catalogue.lookup | `:294` `(Context&, std::span<const U64> source_ids)` | `{provider, source_ids:Ref<vector<U64>>}` | API 固定字段名 `source_ids` |
| 15 | solve_astrometry | `:470` `(Context&, Ref<NativeObservation> observation, const CatalogueProvider& catalogue, const SolverRequest& request)` | `{observation, catalogue:Ref<CatalogueProvider>, request}` | 对象引用改强所有权 |
| 16 | fit_psf | `:471` `(Context&, Ref<NativeObservation> observation, Ref<WcsModel> wcs, std::span<const CatalogueStar> stars, const PsfRequest& request)` | `{observation, wcs, stars:Ref<vector<CatalogueStar>>, request}` | span→Ref<vector> |
| 17 | evaluate_psf | `:472` `(Context&, Ref<EffectivePSF> psf, const PsfEvaluationRequest& request)` | `{psf, request}` | |
| 18 | bind_observation_model | `:462` `(Context&, Ref<NativeObservation> observation, Ref<EffectivePSF> kernel, Ref<Response> response)` | `{observation, kernel, response}` | |
| 19 | measure_science_sources | `:473` `(Context&, Ref<NativeObservation> observation, Ref<WcsModel> wcs, std::span<const CatalogueStar> stars, Ref<EffectivePSF> psf, const PsfRequest& request)` | `{observation, wcs, stars:Ref<vector<CatalogueStar>>, psf, request}` | |
| 20 | integrate_response | `:474` `(Context&, Ref<Response> response, Ref<SpectralBasis> basis, const NumericBudget& request)` | `{response, basis, request}` | |
| 21 | fit_photometry | `:475` `(Context&, std::span<const Ref<SourceMeasurements>> measurements, std::span<const CatalogueStar> stars, std::span<const Ref<SpectralPrediction>> predictions, const PhotometryRequest& request)` | `{measurements:Ref<vector<Ref<SourceMeasurements>>>, stars:Ref<vector<CatalogueStar>>, predictions:Ref<vector<Ref<SpectralPrediction>>>, request}` | 三层嵌套 Ref，codec 要支持 |
| 22 | apply_scale | `:476` `(Context&, Ref<PhotoSolution> solution, Ref<NativeObservation> observation, Id frame)` | `{solution, observation, frame}` | |
| 23 | make_footprint_provider | `:460` `(Context&, Ref<WcsModel> wcs)` | `{wcs}` | |
| 24 | detector_footprint | `:459` `(Context&, Ref<FootprintSet> footprints, U64 source_row)` | `{footprints, source_row}` | |
| 25 | footprint | `:463` `(Context&, const FootprintProvider& provider, Ref<DetectorFootprint> source, double pixfrac, const GeometryBudget& request)` | `{provider:Ref<FootprintProvider>, source, pixfrac, request}` | |
| 26 | measure | `:464` `(Context&, const FootprintProvider& provider, Ref<DetectorFootprint> source, const GeometryBudget& request)` | `{provider, source, request}` | 与虚方法 `footprint.measure` **不是**同一 operation |
| 27 | compute_overlap | `:465` `(Context&, const FootprintProvider& provider, Ref<DetectorFootprint> source, U32 order, double pixfrac, const GeometryBudget& request)` | `{provider, source, order, pixfrac, request}` | |
| 28 | refine_geometry | `:478` `(Context&, const RefinableGeometryProvider& provider, Ref<GeometrySnapshot> base, const GeometryBudget& request)` | `{provider:Ref<RefinableGeometryProvider>, base, request}` | |
| 29 | footprint.forward_interval | `:183` 虚方法 `(Context&, Box) const` **参数未命名** | `{provider, domain:Box}` | 字段名只来自 API 固定表 |
| 30 | footprint.boundary_interval | `:185` 虚方法 `(Context&, const CurvedEdge&, Interval) const` **参数未命名** | `{provider, curve:CurvedEdge, parameter:Interval}` | 同上 |
| 31 | footprint.derivative_interval | `:184` 虚方法 `(Context&, Box) const` **参数未命名** | `{provider, domain:Box}` | 同上 |
| 32 | refinable.open | `:352` 虚方法 `(Context&, Id snapshot) const` | `{provider, snapshot}` | 生产无 provider 可给 |
| 33 | refinable.refine | `:353` 虚方法 `(Context&, Ref<GeometrySnapshot> base, const GeometryBudget& request) const` | `{provider, base, request}` | 与 #28 同算法，需收口 |
| 34 | make_action | `:449` `(Context&, const ActionRecipe& recipe, std::span<const Ref<LinearAction>> children, std::span<const ArrayRef> arrays)` | `{recipe, children:Ref<vector<Ref<LinearAction>>>, arrays:Ref<vector<ArrayRef>>}` | |
| 35 | apply | `:479` `(Context&, Ref<LinearAction> action, ArrayRef values)` | `{action, values}` | |
| 36 | transpose_apply | `:480` 同上 | `{action, values}` | 与 #35 同字段、不同 operation_id |
| 37 | compose | `:481` `(Context&, Ref<LinearAction> outer, Ref<LinearAction> inner)` | `{outer, inner}` | |
| 38 | action.dependency_blocks | `:199` 虚方法 `(BlockRequest, bool transpose) const`：**返回 `Result<vector<Id>>`，第一个参数无名字** | `{range?:BlockRequest, transpose:bool}` —— **字段名不可机械生成** | M0 阻塞项（L33） |
| 39 | action.serialize_recipe | `:200` 虚方法 `() const` | `{}`（空 Input） | |
| 40 | action.row_enclosure | `:201` 虚方法 `(Context&, U64 row) const` | `{row}` | |
| 41 | make_covariance | `:450` `(Context&, Ref<NoiseModel> model)` | `{model}` | |
| 42 | covariance_apply | `:482` `(Context&, Ref<CovarianceAction> covariance, ArrayRef values)` | `{covariance, values}` | |
| 43 | quadratic_form | `:483` `(Context&, Ref<CovarianceAction> covariance, ArrayRef functional)` | `{covariance, functional}` | |
| 44 | geometry_covariance_bound | `:484` `(Context&, Ref<CovarianceAction> covariance, Ref<LinearAction> nominal, Ref<LinearAction> absolute_error, ArrayRef functional)` | `{covariance, nominal, absolute_error, functional}` | |
| 45 | control_covariance | `:487` `(Context&, Ref<CovarianceAction> covariance, Ref<LinearAction> T)` | `{covariance, T}` | |
| 46 | build_drizzle | `:477` `(Context&, Ref<Overlaps> overlaps, Ref<NativeObservation> observation, const DrizzleRequest& request)` | `{overlaps, observation, request}`；输出 `DrizzleResult`（by-value，不是 Ref） | |
| 47 | estimate_flux_information | `:485` `(Context&, const InformationRequest& request)` | `{request}`；输出 `Information`（by-value） | |
| 48 | evaluate_scene_variance | `:486` `(Context&, const FramesProvider& frames, Id frame, std::span<const HealpixLeaf> leaves)` | `{frames:Ref<FramesProvider>, frame, leaves:Ref<vector<HealpixLeaf>>}`；输出 `vector<Variance>` | |
| 49 | build_supported_triangulation | `:489` `(Context&, Ref<PrecisionControls> controls, Chart chart, Ref<SupportMap> support)` | `{controls, chart, support}` | |
| 50 | reconstruct_scene_variance | `:488` `(Context&, Ref<PrecisionControls> controls, Ref<SupportedTriangulation> triangulation, std::span<const SkyICRS> queries)` | `{controls, triangulation, queries:Ref<vector<SkyICRS>>}`；输出 `VariancePrediction` | |
| 51 | build_sky_controls | `:490` `(Context&, const FramesProvider& frames, const SkyRequest& request)` | `{frames:Ref<FramesProvider>, request}` | |
| 52 | fit_relative_sky | `:491` `(Context&, Ref<SkyControls> controls, const SkyRequest& request)` | `{controls, request}` | |
| 53 | apply_sky | `:492` `(Context&, Ref<SignalEstimate> signal, Ref<SkyFit> fit)` | `{signal, fit}` | |
| 54 | select_samples | `:493` `(Context&, const FramesProvider& frames, const CoaddRequest& request)` | `{frames:Ref<FramesProvider>, request}` | |
| 55 | solve_weights | `:494` `(Context&, const FramesProvider& frames, Ref<Selection> selection, const CoaddRequest& request)` | `{frames, selection, request}` | |
| 56 | coadd | `:495` `(Context&, const FramesProvider& frames, Ref<SkyFit> sky, const CoaddRequest& request)` | **例外** `CoaddInput{frames, sky, selection, weights, request}` | FOUNDATION_API §coadd 条：Selection/WeightSolution 必需且身份完全匹配 |
| 57 | build_export | `:496` `(Context&, const ProductProvider& product, const ExportRequest& request)` | `{product:Ref<ProductProvider>, request}`；输出 `ExportResult` | |

### 4.2 三个口径的准确数字（本次重新数，不沿用旧报告）

| 口径 | 数字 | 复核方式 |
|---|---|---|
| 文档白名单 | **57 operation / 22 process** | 解析 `processes/README.md:5-28` 的表格（脚本逐行 split，57 行） |
| `include/acsd/model.hpp` 的 free 声明 | **57 条声明 / 55 个唯一名字**（`forward` 与 `to_fits` 各重载 2 次） | 正则匹配 `// All functions synchronous.` 之后的 `Result<…>/FitsPixel1/PixelCenter0/PixelEdge0 name(` |
| 其中与白名单同名 | **44 条** | `57 − 13`，13 = 非白名单声明（`to_fits`×2、`from_fits_center`、`from_fits_edge`、`forward`×2、`make_frames_provider`、`open_product`、`publish_product`、`export_signal`、`normalize`、`mosaic`、`export_image`） |
| 白名单中属于虚方法/重载 | **13 条** | `catalogue.{query,query_quads,lookup}`、`action.{dependency_blocks,serialize_recipe,row_enclosure}`、`footprint.{forward,boundary,derivative}_interval`、`refinable.{open,refine}`、`forward.{center0,edge0}` |
| 生成 schema 的 “operation variants” | **78** | `tools/generate_contract_schema.py:281`；78 = 57 条 free 声明 + **21** 条 provider/service wire 项（4 catalogue + 7 frames + 4 footprint + 2 refinable + 3 storage + 1 product） |
| **合同驱动实际支持** | **30 个 operation 名** | `apps/contract/driver.cpp:1664-1693` 逐条 `if (operation_ == …)` |
| 其中命中白名单 | **19 个精确名 + 1 个重载名（`forward`）→ 覆盖 21/57 个白名单 operation** | 集合运算 |
| 未被驱动覆盖的白名单 operation | **36 个** | `57 − 21` |
| 驱动里不属于 57 白名单的名字 | **10 个**：`to_fits`、`from_fits_center`、`from_fits_edge`、`storage.copy`、`storage.read`、`storage.open`、`open_product`、`publish_product`、`export_signal`、`export_image`；另加合并重载名 `forward`（覆盖 2 个白名单 operation，故不计入“不可达”） | 19（精确）+ 1（`forward`）+ 10（非白名单）= 30 |

**旧报告“30/78”的问题**：78 不是科学 operation 总数，而是“57 条 free 声明 + 21 条 provider/service wire 项”；把 30 除以 78 会得到 38%，与真实覆盖率（21/57 = 37%）数值接近纯属巧合，两个分母含义完全不同。**对外应写 21/57，并注明其中 2 个是部分可达（`forward` 重载）。**

### 4.3 typed 生成规则下的声明缺陷（M0 必改）

1. **未命名参数**：`action.dependency_blocks(BlockRequest,bool transpose)`（`model.hpp:199`）的第一个参数、`FootprintProvider::{forward_interval,derivative_interval,boundary_interval}`（`model.hpp:183-185`）的全部非 Context 参数、`measure(Context&,const DetectorFootprint&,const GeometryBudget&)`（`:186`）都没有名字。FOUNDATION_API 要求“字段不猜名”，API.md 又用固定字段表兜底——两处标准并存，生成器只能二选一，**这是 M0 的 codec 阻塞项**。
2. **重载**：`forward` 两个重载必须拆成 `forward.center0`/`forward.edge0`（已定），但驱动仍用 `point.type` 分派；typed 生成后旧名字不得保留（AGENTS 无兼容义务）。
3. **span → `Ref<vector<T>>`**：`stars`、`measurements`、`predictions`、`leaves`、`queries`、`source_ids`、`children`、`arrays` 共 8 个字段需要“对象数组”codec；其中 `measurements`/`predictions` 是 `Ref<vector<Ref<T>>>`，wire 上是引用数组，不是内联对象。
4. **纯值函数**：`to_fits`/`from_fits_center`/`from_fits_edge` 没有 `Result`、也没有 process（FOUNDATION_API 明确“纯值运算不创建对象/状态”）；它们**不应**出现在 57 白名单里，也不应被当作 process operation 注册。
5. **service 与 process 分流**：`storage.*`、`frames.*`、`product.read_array`、`open_product/publish_product/export_signal`、`make_frames_provider` 属于 service/工作流，不属于 22 process 的 57 operation；但当前 schema 把 provider 方法与 free 函数混在同一个 `oneOf` 里，**registry 必须按 owner_kind 拆开**，否则 VERIFICATION PROC01 的“不能因旧 driver 支持一个名字就绕过 registry”无法成立。

---

## 5. 测试 / 验证基线复现命令与当前真实结果

### 5.1 命令

```bash
# 设计正本一致性（会顺带跑 generate_contract_schema.py --check）
python3 tools/check_design.py

# 声明形状与生成 schema 一致（单独跑，排除 git 依赖）
python3 tools/generate_contract_schema.py --check

# 公共头 C++20 可编译（M0 门槛的最小检查）
printf '#include <acsd/contracts.hpp>\nint main(){return 0;}\n' > /tmp/tu.cpp
g++ -std=c++20 -fsyntax-only -Iinclude /tmp/tu.cpp

# 既有构建的单元测试
ctest --test-dir build/science --output-on-failure

# 11 个必需 case（会重建 native 驱动；建议在隔离副本里跑，避免与并行施工互相覆盖）
cp -a include src apps schemas tools validation CMakeLists.txt build/science /tmp/audit-repro/
cd /tmp/audit-repro && python3 validation/run_all.py
```

### 5.2 本次真实结果

| 命令 | 结果 | 时间/位置 |
|---|---|---|
| `python3 tools/check_design.py` | **PASS**：`302 definitions, 78 operation variants`；`Design integrity PASS: six owners, schemas, examples, links, 4295 deletion records. No science execution.` | 21:00 首次（`ea984cdb` 头）；**21:27 复跑仍 PASS**（`cea5e3650` 头） |
| `python3 tools/generate_contract_schema.py --check` | **exit 0**（`302 definitions, 78 operation variants`） | 21:03，`/tmp/audit-repro`（`ea984cdb` 头，隔离） |
| `g++ -std=c++20 -fsyntax-only -Iinclude /tmp/tu.cpp` | **OK**（聚合头可编译） | 21:14，本树（`cea5e3650` 头） |
| `cmake -S . -B /tmp/headbuild -G Ninja -DACSD_BUILD_SCIENCE=ON` | **exit 0**（0.9 s，仅生成构建图；**未构建**，不声称能链接） | 21:14，本树 |
| `ctest --test-dir build/science --output-on-failure` | 21:04 首次：**3/4 passed**，`design_integrity` FAILED（`generate_contract_schema.py:143 KeyError: 'ArrayRef'`）；**21:27 复跑：4/4 passed** | 本树。首次失败原因：并行的 `contracts.hpp` 拆分中间态（生成器读到的头正在被改），`cea5e3650` 提交后恢复；同日 21:00 与 21:27 的 `check_design.py` 均为 PASS，隔离副本的 `--check` 也是 exit 0 |
| `python3 validation/run_all.py`（隔离副本） | **pass=7 fail=0 pending=4 other=0**：C01 pass（69 行）、G01 pass（171 行，267 s）、U01 pass（14 行）、F01 pass（2 行：1 pass + 1 pending）、S01 pass（2 行：1 pass + 1 pending）、W01 pending（2 行 0 pass）、V01 pass（12 行）、E01 pass（11 行）、P01 pending（7 行：6 pass + 1 pending）、R01 pending（7 行：6 pass + 1 pending）、X01 pending（1 行 0 pass） | 21:01–21:07，`/tmp/audit-repro`（`ea984cdb` 代码 + 隔离驱动） |
| 同一条命令（本树，另一验证流程 20:58 产物） | `pass=7 fail=0 pending=4`，逐 case 相同（C01 2.8 s / G01 228.9 s / U01 1.1 s / F01 2.2 s / S01 3.5 s / W01 4.7 s / V01 2.2 s / E01 2.0 s / P01 2.1 s / R01 1.9 s / X01 4.8 s） | `validation/run_summary.json`（本树当前文件） |

**结论**：当前唯一“通过”的验收数字是 **11 个必需 case 里 7 pass / 0 fail / 4 pending**，且这 7 个 pass 中 F01、S01 各带 1 条 pending 功能项（见 §6）。11 个 case 里的 4 个（W01/P01/R01/X01）**不是通过**。

### 5.3 结果解释与三个陷阱

1. **`pass` 是 case 级状态，不是“该 case 声明的所有期望都通过”**：F01 声明 3 正例 + 5 负例，`comparison.json` 只有 1 行 pass + 1 行 pending；S01 声明 4 正例 + 3 负例，只有 1 行 pass + 1 行 pending。**不能用 case 级 pass 反推期望级 pass**。
2. **pending 的 case 也可能内部 pass 了 6/7**：P01/R01 都是 6 pass + 1 pending；把它们算成“通过”或“全失败”都不对。
3. **隔离副本的 `ea984cdb` 与当前 HEAD `cea5e3650` 只差声明/构建**，所以上面的验证结果对科学算法仍然有效；但**任何“在 `cea5e3650` 上重跑过 validation”的说法都是假的**（本审计没有跑，也不该在并行施工的树上跑）。

---

## 6. 不得汇总为 pass 的 pending 项清单

以下各项在 `comparison.json` 中 `status != pass`，**任何汇总表都不得把它们计为 pass**：

| 编号 | case | 功能项/原因 | 证据 |
|---|---|---|---|
| P1 | W01 | `blue_weights` —「the normalising denominator has no positive lower bound」 | `validation/w01/comparison.json` |
| P2 | W01 | `qp_weights` —「the equality constrained sub problem has no positive normaliser」 | 同上 |
| P3 | P01 | `negative:unknown_recipe` —「deferred to the contract route」 | `validation/p01/comparison.json` |
| P4 | R01 | `cache_switch_reachable` —「the configuration schema has no cache switch」 | `validation/r01/comparison.json` |
| P5 | X01 | `three_commands_on_disk` — 硬编码 pending（理由已过期） | `validation/x01/truth_x01.py:369-372` |
| P6 | F01 | `two_source_aperture_flux` — 驱动测 PSF 窗积分，不是孔径盒积分 | `validation/f01/comparison.json` |
| P7 | S01 | `holdout_variance` — 公开 action 无法物化 `P` | `validation/s01/comparison.json` |
| P8 | R01 | `cancelled_counter` 虽为 pass，但备注「the frame is counted as failed although the reason names the cancellation」——取消帧的计数语义仍待定 | `validation/r01/comparison.json` |
| P9 | 全部 | **F01/S01 的 case 级 pass 不得转写为“其声明的正/负期望全部通过”**（§5.3-1） | case.json vs comparison.json |
| P10 | 全部 | **E2 证据（obs/cat/phot/stats 的 0 失败）不得计入 HEAD 级通过**：obs phase1 至今仍写「psf not implemented yet」，stats 记录的是 `785adc3` 树 | §3.1 L24/L25 |
| P11 | G01 | `drizzle`/`refine` 两行只由 `validation/native/g01_driver.cpp` 的**测试用** `RefinableGeometryProvider` 产生；生产 `refinable.open` 返回 `unsupported` | §3.1 L21 |
| P12 | 基建 | S-FLOOR/…/S-LIFE、A01/B01/H01/D01/K01、REG01/LIFE01/SCHED01/PROC01/FORMAT01、POOL01/PIPE01/ASYNC01/COMPOSE01、C-CAL-* **一条都没跑**（E3） | §3.1 L27-L31 |

---

## 7. 结论与建议

### 7.1 缺失/最危险项 Top 10（按对 M4 的阻塞程度排序）

| # | 项 | 为什么阻塞 | 建议动作 | 阶段 |
|---|---|---|---|---|
| 1 | **typed registry / ProcessHost 完全缺失**（B06/B14/B15） | 所有 process 都没有 typed 入口；driver 只能按名字手写 30 个分支 | 先落 `RegistryBuilder::freeze` + `ProcessHost::invoke` 的最小实现，再逐个 process 注册 | M0→M2 |
| 2 | **`consume_work` 失败仍扣额度 + `parallel_for` 线程数不受控**（L23） | M1 的账本语义直接不成立，S-WORK/归约反例必失败 | 改成 CAS 预留→消费；`parallel_for` 只让 `use` 个 worker 参与并修正 `job_active_` | **M1（最优先）** |
| 3 | **异步 I/O / Storage 封存全缺**（B05/B12/B13） | 目标“远大于 H 的输出”不可能实现；E02/B01 无路 | 按 OBJECTS `canonical_array_v1` 实作 begin/append/seal/abort + read_async | M3 |
| 4 | **W01 权重分母无正下界**（L17） | `select→weights→coadd` 整条统计链无法验收 | 先修归一化下界（`solve_weights`），再补 W01 全 100000 trials | M5 |
| 5 | **F01/S01 期望级覆盖缺失**（L14/L15/L16） | 光度/天光的 `pass` 是空壳，会污染 Issue 状态 | 把 case.json 的每条正/负期望变成 `comparison.json` 一行 | M5/M6 |
| 6 | **`refinable.open` 无生产实现**（L21/#32） | 几何细化在生产路径不可达；G01 的 refine 是测试替身 | 让 Product/Frames provider 真正携带 refine 闭包，或明确降级为 unsupported 能力并改文档 | M5 |
| 7 | **非矩形足迹 unsupported**（L11） | 真实 detector 足迹（crop/bin/Bayer/洞）不可用 | cut-cell 路线扩到一般 rings，或给出可证的近似分支 | M5 |
| 8 | **`normalize` 端到端 0 发布**（L04） | M6 的 C01—X01 完整路径没有一条能跑完 | 修 X01 的 4 类具名失败（光谱基、PSF 秩、像元积分、盲解） | M6 |
| 9 | **driver 只覆盖 21/57，且 stdin/字段是手写分支**（L06） | 任何“合同驱动已验证”的说法都只覆盖 37% | driver 改为 `invoke_wire` + registry thunk，删掉手写 switch | M0/M4 |
| 10 | **未命名参数/重载/纯值函数混入 wire**（L33/L34） | typed 生成会卡住或生成错误字段名 | M0 冻结前给全部参数命名，并把纯值函数移出 process 白名单 | **M0** |

### 7.2 建议的 M0—M6 顺序（只列与本文矩阵直接相关的门槛）

1. **M0 收口（下个迭代内可完成）**：`dependency_blocks`/`FootprintProvider` 参数命名；57 operation 逐项 `TypeId`+codec 骨架；`register_builtin_types`；`process-manifest.json` 生成；把 `to_fits/from_fits_*` 与 service 从 process 白名单剔除；driver 增加“未注册 key → unsupported”的负例。**完成判据**：REG01 的“缺 schema/binding/文档/重复 ID”四类拒绝实跑。
2. **M1**：账本三态 + arena + `consume_work` 修复 + A01/S-LIFE 反例。**完成判据**：A01 的 50 MiB/10 work 归因与“失败无副作用”实测。
3. **M2**：唯一 CPU 池 + Task + Pipe + S-FLOOR/FORK/ORDER/MULTI/WORK/IO/BARRIER。**完成判据**：8 条反例逐条实跑，等待不占 CPU token。
4. **M3**：异步 Storage + 规范封存 + DEV_MODE sink + B01/H01/D01。**完成判据**：H=32 MiB 输出 256 MiB 的 B01 全流程。
5. **M4**：`observation.calibrate` 的 typed 接入 + 9 条 C-CAL。**完成判据**：C-CAL-WRITE（第二个数组短写/第四个 seal 失败）与 C-CAL-CLEAN（取消后 cleanup 仍完成）。
6. **M5**：按表 1 的 20 条“可复用”先接（U01/C01/G01/E01/S01/V01 已覆盖的 20 项），再按“需重构 7 项 → 未验收 29 项”排序；每接一个就补该 operation 的独立小例与负例。
7. **M6**：Normalize/Mosaic/Export 组合 + X01 + 双平台 + P-small/P-large 性能基线；**L01/L02 在 M6 结束前必须解决，否则 M6 不能签字**。

---

### 附：本文的复核入口

```bash
# 1) 白名单 57 项与声明对照（本文 §1/§4 的数字来源）
python3 - <<'PY'
import re
doc=open('docs/engineering/rebuild/processes/README.md').read()
rows=re.findall(r'^\| \[([\w.]+)\]\(([^)]+)\) \| (\w+) \| (.+?) \|$', doc, re.M)
wl=[op.strip() for _,_,_,ops in rows for op in ops.split(',')]
print('processes',len(rows),'operations',len(wl))
h=open('include/acsd/model.hpp').read(); seg=h[h.index('All functions synchronous'):]
free=re.findall(r'(?:Result<[^;\n]+?>|FitsPixel1|PixelCenter0|PixelEdge0)\s+(\w+)\(', seg)
print('free declarations',len(free),'unique',len(set(free)))
d=open('apps/contract/driver.cpp').read()
disp=re.findall(r'operation_ == "([^"]+)"', d)
print('driver names',len(disp),'exact whitelist hits',len(set(disp)&set(wl)))
PY

# 2) 表 1 的行号抽查（任选一行）
sed -n '124p' src/observation/calibrate.cpp     # calibrate_observation
sed -n '47p'  src/observation/regroup.cpp       # regroup_observation

# 3) 表 2 的“只有声明没有实现”
ls src/runtime/                     # 只有 internal.hpp
grep -rn 'Task<.*> read_async\|class ProcessHost' include/acsd/runtime/*.hpp
```

## 附录 A：实施后状态（2026-10-23 追记，不覆盖原审计证据）

本附录只记录矩阵完成之后真实落地的状态；正文表格仍是实施前审计快照，不得当作当前能力。所有结论以下列提交与实测为准。

| 里程碑 | 状态 | 提交/证据 |
|---|---|---|
| M0 公共合同与登记 | 完成（声明/登记/覆盖） | `cea5e3650` 冻结头；`0a5dbfff3` registry/Host/生成装配；manifest 22 process / 57 operation，`check_process_coverage.py` PASS；`acsd_typed_smoke` 1255 checks（22/57、102 TypeId、codec 负例）、`acsd_registry_negative` 107 checks |
| M1 账本/所有权 | 完成（独立验证 S-LIFE/S-WORK/A01） | `src/runtime/memory.cpp`；verify-rt `life01` 20/20（ASan）、`work01` 15/15、`a01` 23/23 |
| M2 Task/池/Pipe | 完成（S-FLOOR/FORK/ORDER/BARRIER/MULTI/PIPE01） | `src/runtime/{task,pool,pipe}.cpp`；`floor01` 17/17、`fork01`、`barrier01` 20/20、`multi01` 12/12；取消排空 `18 cancelled + 2 ok` |
| M3 异步 Storage/诊断 | 完成（B01/H01/D01/ASYNC01/S-IO） | `src/runtime/{storage,diagnostics}.cpp`；`b01` 24/24、`io01` 14/14、`d01_off` 3/3；DEV_MODE=1 独立构建 `/tmp/acsd-build-dev` 可跑 ON 语义 |
| M4 首个科学 process | 完成（C-CAL 全链） | `33804ef6a`；kernel 36/36、Host chain 25/25、旧/新逐位一致、Host identity 与同步入口相同；C-CAL-ROOT 共享根保持 |
| M5 其余 21 process | **部分完成（注册/计划/小例已接，验收未关闭）** | `6753f822f`、`5b64e5cb0`：22/22 process 由模块自有 typed 注册接管，57/57 operation 全部登记、一次唯一种子；`acsd_typed_smoke` 1255 checks、`registry_negative` 112 checks、manifest 22/57 + coverage PASS；process driver 全部进 ctest（19/19）。真实正例：coordinates 7/7、catalogue 4 op、sources measure、response、geometry 11/11（含 footprint 正例）、operators 7/7、uncertainty 5/5（quadratic_form 5.0）、drizzle/export、photometry/information/sky/coadd 计划与负例。**未关闭**：coadd.integrate 仍具名 unsupported；fit_psf 无噪单高斯 `nonconverged`、astrometry 缺同源正例；每模块取消/重开与 probe 采样证据未齐；多数 R/F/W 仍是声明保守值而非 allocator 实测 |
| M6 工作流/双平台 | **未开始** | Normalize→Mosaic→Export 尚未改接 typed Host；CLI/contract 仍直接调旧 workflow 入口；Windows 无工具链；真实 HST/M42 无输入 |

### 已知规格/文档缝隙（保留为后续修订入口）

1. FOUNDATION_API 的 wire 响应正文要求 resolved input/request hash 与 backend，但代码块原先缺字段；已在唯一正本补 `WireResponse.input_hash/request_hash/backend` 并同步实现。
2. Probe kind 原先只写在 `standards/PROBES.md`，生成器曾用假设表；已改为从 PROBES.md 解析，缺失即生成失败。
3. SYSTEM 依赖表原先未含 observation→runtime；M4 的 science Context 桥需要该依赖，已在唯一正本补上。
4. process 文档的 probe 声明只给 id，kind/边界/载荷仍由 PROBES.md 拥有；TEMPLATE 的“kind”要求未逐篇补齐，属文档待补项。
5. bounds：57 个 operation 中 41 个为显式 provisional 1..65536，16 个以文档候选/page 锚点 1..4096；它们都不是经 allocator 实测的 W_module 上界，M5 逐模块替换前不得称为资源验收通过。
6. pipe 的 `max_queued_bytes` 必须 ≥ 实际 payload；验证 fixture 曾用 16 B 窗口装 40 B Item，导致合法的“无法获得 credit”状态。实现现在对超窗口 payload 立即具名 `resource_exhausted`，fixture 需按 payload 调整。


## 附录 B：独立审核与修复状态（2026-10-24 追记）

独立审核者（未参与实现）以 `d404c99f7` 为锚给出 **REJECT**（M2/M3/M4），报告见 [review-m0m4.md](review-m0m4.md)。修复提交与状态：

| 审核项 | 状态 | 证据 |
|---|---|---|
| when_all 非法/被移动子任务伪造成功 | 已修 | `c546657e0`；非法子任务记 failure，无值不聚合，全 ok 才返回聚合值 |
| `work_upper=0` 绕过 `max_work`；预算耗尽永久等待 | 已修 | `c546657e0`/`6fe0c8aac`；显式 0 为硬上限，`work+units>max_work` 立即具名 `resource_exhausted` |
| 无 2P ready 窗口 / control 绕过 ceiling | 已修 | `c546657e0`；P=1 提交 100000 → 10 接受、99990 具名拒绝；control max_active=1 |
| seal/abort 竞争、spill UAF、sealed 数组永久 pin | 已修 | `c546657e0`；ASan 无堆错误、abort 后 seal=cancelled、最后 ArrayRef 释放容量归零、stored-byte 账本 |
| ordered pipe 不能跨 cohort | 合同闭合（执行器声明） | `pipe_expect` + 头注释；计划删除未使用 edge 并降级诚实串行（window=1） |
| 同步/Host 校验分歧 | 已修 | `6753f822f`；唯一 `validate_calibration_input`，`cal_equivalence` 8/8、cal_host 33/33 |
| M4 plan work/R/F 漏计 | 已修（声明上界，非逐分配实测） | `6753f822f`；N=3 140≤460、N=200000 201906≤268580，R 含 92N 保守项；W 仍非 allocator 实测 |
| C-CAL-WRITE/CLEAN 未跑 | 已跑 | `5b64e5cb0`；16/16（短写 io_error、seal 失败 io_error、写后取消 cancelled+deferred abort ok、spool 归零） |
| cal01 判据被放宽 | 已恢复并自然通过 | `d4ccc8fa6`；严格 required 37/37、usage.work=140 ∈ [3,524]、真实共享 NoiseModel 根 |
| 空 process/共享 document 可 freeze | 已修 | `c546657e0`；registry_negative 112 checks |
| S-MULTI 1:2 未兑现 | 已修 + 正确测量 | `b015fff92` deficit RR；P=2 完整轮 fixture 100:200（complete-round 0.9977），15/15 pass |
| 证据与标题矛盾 | 已修 | 严格判据 + manifest 15/15、失败 attempt 保留；旧 7 pass/4 pending baseline 未改写 |

仍未关闭：M5 的 coadd.integrate、fit_psf 收敛、astrometry 同源正例、逐模块取消/重开与 probe 采样、W_module allocator 实测；M6 全部；Windows；真实 HST/M42；GPU 仅合同。
