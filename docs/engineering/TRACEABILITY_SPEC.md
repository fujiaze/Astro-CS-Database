# 追溯合同与台账（TRACEABILITY_SPEC）

> 上游：ACSD_DESIGN.md §12.4（验证层级与四层验收）

> 矩阵基线 base_main_sha=0d32c07d65c6d7489fa408cbafaa98ddf9ecf4da
> 本文件冻结追溯 ID 格式、唯一性、跨层关系与 source symbol 表达，
> 并以人读表格承载逐模块台账（§9）与需求登记册（§10）。追溯的正本
> 是人读正文，机器不代读。
> 权威顺序与分层语义按 `ACSD_DESIGN.md` §0（权威链）与 `docs/DOCUMENT_INDEX.yaml`（机器一致性检查）；
> 本文件不重复公式、不改科学定义、不放宽既有工程约束。

## 1. 目的与范围

为每个模块建立**可逐格核对**的八层追溯合同，缺口必须**显式表达**（`MISSING`），
空缺一律写显式值；空串、占位符与静默缺列即判红。矩阵的“事实”是**本文件的人读表格**：§2 分层定义、§9 逐模块台账、§9.1 逐模块层状态、§10 需求登记册。
追溯面不再有 JSON/CSV 矩阵文件，也不由机器读取；跨层一致性与断链由人逐轮目视核对，
核对项见 §7，结论回写本文件表格。

## 2. 追溯链与分层

权威链（控制包 16 号文 §1）：`SCI → ALG → DATA/API/ARCH → MOD/SRC → TEST → EVIDENCE`。

仓库矩阵把它展成 8 个**必填层**，同一行（同一模块）形成从科学定义到现场证据的
parent→child 链：

| 层 | 层代码 | 必填列 | 唯一性域 | 状态取值（必须非空） |
|---|---|---|---|---|
| 科学定义 | SCI | science_id / science_doc | 全矩阵唯一（跨模块） | VERIFIED / MISSING |
| 算法/近似/误差 | ALG | algorithm_id / algorithm_doc | 全矩阵唯一 | VERIFIED / MISSING |
| 数据合同 | DATA | data_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 接口/API | API | api_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 架构 | ARCH | arch_id | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 模块承载 | MOD | module_id | 模块唯一（= 行键） | VERIFIED（由行存在即满足） |
| 实现 | SRC | src_id / src_path | 全矩阵唯一 | VERIFIED / MISSING |
| 验证 | TEST | test_id / test_path | 全矩阵唯一 | VERIFIED / MISSING / NONE |
| 证据 | EVIDENCE | evidence_id | 全矩阵唯一 | VERIFIED / MISSING |

实现注意：

- DATA/API/ARCH 三个承载层**并列**（同属“软件承载”），不是先后级联；每层各自可
  `VERIFIED / MISSING / NONE`。`NONE` 仅允许模块文档显式声明“该模块无此类合同”
  （如 conformance 模块无科学合同），且必须在 `notes` 给出原因。
- SCI/ALG 层对 conformance/服务性模块可 `MISSING`（显式），但 MOD/SRC/TEST/EVIDENCE
  对**每个已注册模块**都必须是 `VERIFIED`（有真实文件）或至少 `MISSING` 显式占位；
  服务模块（io）允许 DATA/API `VERIFIED` 而 SCI/ALG `MISSING`。
- “每层必填”= 矩阵每行、每层状态单元格**必须出现且非空**（取值于该层合法集合），
  缺列/空串/空白即判红；`MISSING` 是合法显式值，不是空串。

### 2.1 空缺的表达（空串即判红）

- 状态空缺 → 状态列必须写 `MISSING`（或按层的 `NONE`）；留空与 `-`/`?`/`TBD` 即判红。
- ID 空缺 → ID 列写显式占位：
  - 通用层（SCI/ALG/DATA/API/ARCH/TEST/EVIDENCE/SRC 的 id 列）：`<LAYER>-MISSING`
    （例如 `SCI-MISSING`、`TEST-MISSING`）。
  - SRC 的 source symbol 表达：`<src_path>::MISSING`（占位无符号）。
- 文档/路径空缺 → 列写 `MISSING`（空串即判红）。
- 任何单元格为 `""`、纯空白、`-`、`?`、`TBD`、`TODO` → 按 §7 人读核对记 `EMPTY_CELL_VIOLATION`（记录具体行/列）。

## 3. ID 格式（机器正则）

ID 一律 ASCII 大写，段分隔符取 `-`；空格、点、下划线之外的字符与空段都判红：

```text
SCI      ^SCI-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 SCI-CAL-001、SCI-P1-DRIZ-001
ALG      ^ALG-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 ALG-CAL-001、ALG-005
DATA     ^DATA-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 DATA-P1-FRAME、DATA-HIPS-001
API      ^API-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 API-P1-001、API-ABI-001
ARCH     ^ARCH-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 ARCH-001
MOD      ^MOD-[A-Z0-9]+(-[A-Z0-9]+)*$          例如 MOD-acsd-phase1-calibration
TEST     ^TEST-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 TEST-P1-CAL-001、TEST-BLD003-NOOP-HANDSHAKE
EVID     ^EVID-[A-Z0-9]+(-[A-Z0-9]+)*$         例如 EVID-P1-CAL-001
```

- 占位符 ID 是合法 ID 的超集特例：`SCI-MISSING`、`TEST-MISSING` 等（见 2.1）。
- **唯一性强制域（机器 ERROR）**：`module_id`（行键）全矩阵唯一；SRC 层 id、
  EVIDENCE 层 id（非占位）全矩阵唯一。SCI/ALG/DATA/API/ARCH/TEST 是**合同层**，
  ID 可被多个模块行共享（如 `API-P2-001` 被 8 个 phase2 模块共同承载、
  `TEST-P3-RES-001` 由 phase3.resample/resample2 共享——registry 文档既定事实），
  其**真实唯一性以合同注册表为准**（`eng/contracts/data/contract_index.yaml`；注册表与合同图的双向一致判据不在本仓现行面），
  本矩阵对共享引用只登记不判重。
- 状态 `MISSING` 的层允许保留 **descriptor/registry 已预留的真实 ID**（ID 占用
  命名空间但独立 authority 文档/实现尚未落地），也允许占位符 ID；空串判红。
- 状态 `VERIFIED` 的层必须满足：id 非占位，且锚可机器解析（见 §4/§5 与 §7）。
- 合同 ID 注册表（`eng/contracts/data/contract_index.yaml`）与本文件的登记册沿用各自格式，不由本合同重写；
  本矩阵是模块化事实源（第 5 节给出两表关系）。

## 4. 跨层 parent→child 关系

- 每个模块一行（key=`module_id`），层的 child 关系由**行内同列取值**表达：
  - `science_id` 是 SCI 层的“主对象”（模块级）；
  - `algorithm_id` 是 ALG 层主对象；`data_id`/`api_id`/`arch_id` 是承载层主对象；
  - `src_path::symbol`（SOURCE SYMBOL 表达）给出实现层锚点；
  - `test_id` + `test_path` 给出验证层锚点；
  - `evidence_id` 给出 EVIDENCE 层锚点（证据包/日志/现场 hash）。
- 跨层 parent-child 校验规则（机器可查）：
  1. **前导链非空**：TEST 层引用成立的前提是同一行 MOD（行存在）与 SRC 层非 `MISSING`
     （SRC `MISSING` 时 TEST 必须 `MISSING`；“有测试无实现”即判红）；
  2. **承载层引用**：API `VERIFIED` 的行应能通过 API 注册表/docs/engineering/PUBLIC_API.md
     找到对应 ID（由扩展检查给出具体缺失，不崩溃）；
  3. **证据锚**：EVIDENCE `VERIFIED` 时 evidence_id 应能在 `reports/`（发行包内）、`artifacts/`
     、`returns/` 或 TASK_STATE evidence_refs 中解析（同 2 语义）；
  4. 一行内“下层 VERIFIED 而上层同链 MISSING”即科学链断裂、判红
     （SCI MISSING 但 ALG VERIFIED 之类）→ 判 `CHAIN_BREAK`（给出 module_id 与层）。
     例外（显式登记，不判断链）：conformance/service/provider 行 —— SCI/ALG
     MISSING 而 DATA/API/ARCH/VERIFIED 属宿主/服务边界语义，notes 已给出原因。
  5. TEST 层 `VERIFIED` 时该行 SRC 层必须也 `VERIFIED`（有实现才有测试证据），
     SRC `MISSING` 而 TEST `VERIFIED` 判 `CHAIN_BREAK`（给出 module_id）。
- 说明：台账是**模块↔锚**逐行合同；本文件 §10 的逐条细粒度
  （authority/anchor/oracle）仍由逐条追溯判据负责，二者互补不冲突。

## 5. SOURCE SYMBOL 表达

- SRC 层引用格式：`<repo-relative-path>::<symbol>[,<symbol>...]`，多符号用逗号分隔。
- 文件必须存在且受 Git 跟踪；符号必须在文件文本中可见（宽松匹配标识符边界）。
- 无符号可锚时写 `<path>::MISSING`（文件存在但符号待补）——留空与裸路径冒充即判红。
- 表达示例：`eng/tests/conformance/noop/src/noop_module.c::acsd_module_query_v1`、
  `lib/infrastructure/benchmark/cpu/common/README.md::MISSING`。

## 6. 初始矩阵（基线）

本文件 §9 覆盖仓库**全部已注册模块**：

- `lib/infrastructure/aio/io`（IO-001/IO-002 落地）：`acsd.services.io`
- `eng/tests/conformance/noop`（BLD-003 SKELETON）：`acsd.conformance.noop`
- `docs/detail/registry/acsd.phase*.md` 声明的 22 个 registry 生产模块
  （module_id 以 `acsd.phase1./phase2./phase3.` 开头，唯一源 `lib/infrastructure/scheduler/src/module_adapters.cpp`）
- `lib/infrastructure/benchmark/cpu`（CPU-001 落地，provider 能力清单）

每行 8 层全部显式；尚无科学/算法合同的行用 `SCI-MISSING`/`ALG-MISSING` + 状态
`MISSING`，有实现有测试的行用真实 id/path/符号（`VERIFIED`）；空缺从不为空串。
模块清单的“机器可发现”方法记录于第 7 节。

### 6.1 模块 ID 归一化基线（烧毁式阈值）

模块 ID 归一化以「模块清单规模」与「登记页覆盖」两面对账。规模登记值与三类错误的允许上限如下；
**上限只许下降不许上升**（烧毁式基线），下调只能在对应落地批内手改并写明依据。

| 项 | 值 | 口径 |
|---|---:|---|
| 文档集模块数登记值 `index_module_count` | 23 | 取代交付状态判据中的硬编码模块数；断言「索引解析出的模块数 == 本值」且「映射表 ID 集合 == 索引 ID 集合」。未登记时退化为「索引规模 == 映射表规模」 |
| `PAGE_MISSING_MODULE_ID` | 20 | 登记页缺 `module_id`（上次实测 14） |
| `MODULE_WITHOUT_PAGE` | 17 | 模块无登记页（上次实测 11） |
| `AUTHORITY_MISSING` | 0 | 模块无权威文档（上次实测 0） |

**基线规则（逐字）**：

> 烧毁式基线：每类只许下降不许上升；下调只能在 MOD-002 落地批内手改，并在自证摘要记明「本批下调 X→Y，依据=落地了哪几条」；两类归零后删除本文件 ⇒ 门转全量强制（任何 error 即 rc=1）。负例注入（--fault-inject）一律绕过基线、全量强制。

规则的实质是那最后一次下调的**不对称**：实测已降到 14/11，但**没有**下调阈值，保持基线保守 ——
实际计数低于基线仍判通过，阈值只作上限。负例注入一律绕过基线、按全量强制判。

> 归一化判据；逐类计数按各类错误在输出中的频次统计。

## 7. 追溯核对（人读检查清单）

**本节无机器检查器。** §1 已定：追溯面不再有 JSON/CSV 矩阵文件，追溯正本是人读正文。
下表因此是**人读核对项**：逐轮打开本文件 §9 / §9.1 / §10 逐格核对，发现缺陷当场订正并记入留证区；
**不产生机器 PASS 记录，不注册检查项。**

发现问题时按下表的缺陷代号分类记录，代号只用于留证与统计，不表示有任何程序在产出该输出：

| 核对项 | 缺陷代号 | 人读核对动作与记录内容 |
|---|---|---|
| 空单元格（空串/空白/`-`/`?`/`TBD`/`TODO`） | `EMPTY_CELL_VIOLATION` | 记 模块+列 |
| ID 格式不合法（见 §3） | `ID_FORMAT_VIOLATION` | 记 模块+列+值 |
| 同层真实 ID 重复 | `DUPLICATE_ID` | 记 层+ID |
| 前导链断裂（见 §4 规则 1/4/5） | `CHAIN_BREAK` | 记 module_id+层对+期望/实际状态 |
| 文档/路径/SOURCE SYMBOL 引用悬空（文件不存在/未跟踪/符号不可见） | `DANGLING_REF` | 记 模块+列+路径+期望符号 |
| 引用越界（API/TEST/EVID 允许外部注册表缺失但必须逐条列 WARN） | `REF_OUT_OF_SCOPE` | 记 具体 ID |
| 层状态缺失或与 §2 取值域不符 | `STATUS_DOMAIN_VIOLATION` | 记 模块+层+实际值（状态面见 §9.1） |

前导链与状态面是本节的**主核对对象**：§4 的链规则与 §2 的状态必填都以状态为操作对象，
§9.1 是状态面的唯一落点；状态缺失或取值越域即按 `CHAIN_BREAK` / `STATUS_DOMAIN_VIOLATION` 处置。

## 8. 状态与演进

- 本文件（§2 分层 + §9 模块台账 + §10 需求登记册）构成追溯正本；模块填充把行状态
  `MISSING` → `VERIFIED`，同时补 `EVID-*` 证据锚。
- 科学公式/单位/容差以 docs/science、docs/algorithms 与测试 oracle 为权威；
  本合同只做机器身份与断链报告，不判定科学正确性。
- 冻结约束（项目负责人）优先于本文件；本文件只可在负责人确认后修订。

## 9. 逐模块追溯台账（人读正本）

本节是**人读**的逐模块追溯台账：每模块一行，给出九层（SCI/ALG/DATA/API/ARCH/MOD/SRC/TEST/EVIDENCE）
的 ID 与落点文件；各层**状态**单列于 §9.1（状态不能由 ID 推断，故不并入本表）。
空缺按合同显式写 `MISSING` / `NONE`，不写空串。
模块行键 = `module_id`；`module_anchor` 列给出该模块的登记页。

| module_id | module_kind | 登记页(module_anchor) | SCI | ALG | DATA | API | ARCH | SRC | TEST | EVIDENCE |
|---|---|---|---|---|---|---|---|---|---|---|
| MOD-acsd-conformance-noop | conformance | eng/tests/conformance/noop/module.yaml | SCI-MISSING | ALG-MISSING | DATA-MISSING | API-ABI-001 | ARCH-001 | SRC-MOD-NOOP @ eng/tests/conformance/noop/src/noop_module.c::acsd_module_query_v1 | TEST-BLD003-NOOP-HANDSHAKE @ MISSING | EVID-BLD-003 |
| MOD-acsd-services-io | service | lib/infrastructure/aio/io/include/acsd/io/fits_stream_v1.h | SCI-MISSING | ALG-MISSING | DATA-HIPS-001 | API-IO-STREAM-V1 | ARCH-001 | SRC-IO-FITS-CORE @ lib/infrastructure/aio/io/fits_core.c::acsd_fio_reader_open_v1 | TEST-IO-FITS-SELFTEST @ MISSING | EVID-IO-001 |
| MOD-acsd-providers-cpu | provider | lib/infrastructure/benchmark/cpu/common/README.md | SCI-MISSING | ALG-MISSING | DATA-MISSING | API-CPU-001 | ARCH-CPU-001 | SRC-CPU-CAPABILITY @ lib/infrastructure/benchmark/cpu/common/src/capability_detect.c::acsd_cap_detect_v1 | TEST-CPU-CAPABILITY-MATRIX @ MISSING | EVID-CPU-001 |
| MOD-acsd-phase1-calibration | phase1 | docs/detail/registry/acsd.phase1.calibration.md | SCI-CAL-001 | ALG-CAL-001 | DATA-P1-CAL | API-P1-001 | ARCH-001 | SRC-CAL-001 @ lib/algorithms/calibration/include/astro_calibration.h::ac_generate_master_bias,ac_generate_master_dark,ac_generate_master_flat,ac_calibrate_frame,ac_correct_frame,ac_generate_master_bias_f64,ac_generate_master_dark_f64,ac_generate_master_flat_f64,ac_calibrate_frame_f64,ac_correct_frame_f64,ac_set_num_threads,ac_version | TEST-CAL-001;TEST-CAL-DESIGN-001 @ MISSING | EVID-P1-CAL-001 |
| MOD-acsd-phase1-cosmetic | phase1 | docs/detail/registry/acsd.phase1.cosmetic.md | SCI-CAL-001 | ALG-COS-001 | DATA-P1-COS | API-COS-001 | ARCH-001 | SRC-COS-001 @ lib/algorithms/calibration/src/cosmetic_corrector.cpp::correct_frame,interpolate_pixels,filter_by_structure_size,detect_hot_pixels,detect_cold_pixels | TEST-COS-DESIGN-001 @ docs/science/algorithms/COSMETIC_ALGORITHMS.md | EVID-MISSING |
| MOD-acsd-phase1-drizzle | phase1 | docs/detail/registry/acsd.phase1.drizzle.md | SCI-DRZ-001 | ALG-DRZ-001 | DATA-P1-DRZ | API-DRZ-001 | ARCH-001 | SRC-DRZ-001 @ lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h::hp_drizzle_fits_to_ahpx,hp_drizzle_run,hp_drizzle_run_hips,hp_drizzle_reverse_run,hp_drizzle_reverse_capability,hp_drizzle_reverse_version | TEST-DRZ-DESIGN-001 @ docs/science/algorithms/DRIZZLE_GEOMETRY.md | EVID-MISSING |
| MOD-acsd-phase1-hips-writer | phase1 | docs/detail/registry/acsd.phase1.hips-writer.md | SCI-DRZ-001 | ALG-HIPS-001 | DATA-P1-HIPS | API-HIPS-001 | ARCH-001 | SRC-HIPS-001 @ lib/infrastructure/aio/include/aio_hips.h::aio_hips_product_begin,aio_hips_write_signal_support_tile,aio_hips_write_variance_tile,aio_hips_write_snr_points,aio_hips_set_drizzle_provenance,aio_hips_finalize,aio_hips_abort,aio_hips_write,aio_hips_last_error | TEST-HIPS-DESIGN-001 @ docs/science/algorithms/HIPS_WRITER.md | EVID-MISSING |
| MOD-acsd-phase1-noise-snr | phase1 | docs/detail/registry/acsd.phase1.noise-snr.md | SCI-NOISE-001 | ALG-NOISE-001 | DATA-P1-NOISE | API-NOISE-001 | ARCH-001 | SRC-NOISE-001 @ lib/algorithms/noise_snr/cpp/include/snr_estimator.h::snr_noise_model_v1,snr_noise_model_v1_f64,snr_noise_model_v1_default_config,snr_noise_model_v1_fill,snr_noise_model_v1_free,snr_noise_scale_law,snr_noise_gain_variance | TEST-P1-NOISE-001;TEST-NOISE-DESIGN-001 @ MISSING; 生产源 lib/algorithms/noise_snr/cpp/src/noise_model.cpp | EVID-P1-NOISE-001 |
| MOD-acsd-phase1-photometry | phase1 | docs/detail/registry/acsd.phase1.photometry.md | SCI-PHOT-001 | ALG-PHOT-001 | DATA-P1-PHOT | API-PHOT-001 | ARCH-001 | SRC-PHOT-001 @ lib/algorithms/photometry/cpp/include/photometric_calib.h::pc_calibrate_simple,pc_calibrate_simple_with_gaia,pc_calibrate_simple_f64,pc_calibrate_simple_with_gaia_f64,pc_calibrate_simple_with_gaia_v2,pc_calibrate_simple_with_gaia_f64_v2 | TEST-P1-PHOT-001;TEST-PHOT-DESIGN-001 @ MISSING | EVID-P1-PHOT-001 |
| MOD-acsd-phase1-star-psf | phase1 | docs/detail/registry/acsd.phase1.star-psf.md | SCI-P1-PSF-001 | ALG-STARPSF-001 | DATA-P1-PSF | API-P1-003 | ARCH-001 | SRC-PSF-001 @ lib/algorithms/psf/src/dpsf_psf.cpp::moffat4_fit_tmpl,lm_solve | TEST-PSF-DESIGN-001 @ MISSING | EVID-P1-PSF-001 |
| MOD-acsd-phase1-wcs-platesolve | phase1 | docs/detail/registry/acsd.phase1.wcs-platesolve.md | SCI-WCS-001 | ALG-WCS-001 | DATA-P1-WCS | API-WCS-001 | ARCH-001 | SRC-WCS-001 @ lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp::ipv_solve_create,ipv_solve_destroy,ipv_set_gaia_handle,ipv_set_detector_handle,ipv_get_default_params,ipv_get_last_inlier_count,ipv_get_last_inliers,ipv_solve,ipv_solve_from_memory,ipv_solve_from_detections_v1,ipv_solve_from_memory_with_callback,ipv_solve_from_memory_with_callback_d | TEST-WCS-DESIGN-001 @ docs/science/algorithms/PLATESOLVE.md | EVID-MISSING |
| MOD-acsd-phase1-writer | phase1 | docs/detail/registry/acsd.phase1.writer.md | SCI-P1-WR-001 | ALG-P1-WR-001 | DATA-P1-FITS | API-P1-008 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P1-WR-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase2-coverage | phase2 | docs/detail/registry/acsd.phase2.coverage.md | SCI-UPM-001 | ALG-COV-001 | DATA-COV-001 | API-COV-001 | ARCH-001 | SRC-COV-001 @ lib/algorithms/coverage/src/coverage.cpp::p2_coverage_build, p2_coverage_free, parse_props, inspect_frame | TEST-COV-DESIGN-001 @ docs/science/algorithms/PHASE2_COVERAGE.md::TEST-COV-DESIGN-001 | EVID-MISSING |
| MOD-acsd-phase2-integrate | phase2 | docs/detail/registry/acsd.phase2.integrate.md | SCI-INT-001 | ALG-P2-INT-001 | DATA-P2-INT | API-P2-INT-001 | ARCH-001 | SRC-P2-INT-001 @ lib/algorithms/coverage/include/astro/phase2/integrate.h::p2_integrate_pixel,p2_validate_candidate_weights | TEST-P2-INT-001 @ docs/detail/registry/acsd.phase2.integrate.md::TEST-P2-INT-001 | EVID-MISSING |
| MOD-acsd-phase2-reject | phase2 | docs/detail/registry/acsd.phase2.reject.md | SCI-REJ-001 | ALG-P2-REJ-001 | DATA-P2-REJ | API-P2-REJ-001 | ARCH-001 | SRC-P2-REJ-001 @ lib/algorithms/coverage/include/astro/phase2/rejection.h::p2_reject_plan_resolve,p2_reject_stack_ex,p2_collect_candidate_stack,p2_eligibility_filter,p2_large_scale_apply,p2_rejection_semantic_id | TEST-P2-REJ-001 @ docs/detail/registry/acsd.phase2.reject.md::TEST-P2-REJ-001 | EVID-MISSING |
| MOD-acsd-phase2-resample | phase2 | docs/detail/registry/acsd.phase2.resample.md | SCI-P2-RES-001 | ALG-P2-RES-001 | DATA-P2-RES | API-P2-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P2-RES-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase2-session | phase2 | docs/detail/registry/acsd.phase2.session.md | SCI-UPM-001 | ALG-P2-SESSION-001 | DATA-P2-SESSION | API-P2-SESSION-001 | ARCH-001 | SRC-P2-SESSION-001 @ lib/phase2_session/p2_session.h::p2_session_create,p2_session_validate,p2_session_run,p2_session_inspect,p2_session_destroy | TEST-P2-SESSION-001 @ docs/detail/registry/acsd.phase2.session.md::TEST-P2-SESSION-001 | EVID-MISSING |
| MOD-acsd-phase2-sample | phase2 | docs/detail/registry/acsd.phase2.sample.md | SCI-P2-SMP-001 | ALG-P2-SMP-001 | DATA-P2-SMP | API-P2-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P2-SMP-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase2-upm-apply | phase2 | docs/detail/registry/acsd.phase2.upm-apply.md | SCI-UPM-001 | ALG-P2-UPM-IMPL-001 | DATA-P2-COR | API-P2-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P2-UPM-002 @ MISSING | EVID-MISSING |
| MOD-acsd-phase2-upm-fit | phase2 | docs/detail/registry/acsd.phase2.upm-fit.md | SCI-UPM-001 | ALG-P2-UPM-IMPL-001 | DATA-P2-UPM | API-P2-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P2-UPM-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase2-write | phase2 | docs/detail/registry/acsd.phase2.write.md | SCI-UPM-001 | ALG-P2-HIPS-001 | DATA-P2-HIPS | API-P2-HIPS-001 | ARCH-001 | SRC-P2-HIPS-001 @ lib/algorithms/coverage/tools/stage2.cpp::main, P2Stage2Config, p2_stage2_parse_config, p2_stage2_make_upm_cfg, p2_acr_block_eligible | TEST-P2-HIPS-001 @ docs/detail/registry/acsd.phase2.write.md::TEST-P2-HIPS-001 | EVID-MISSING |
| MOD-acsd-phase3-properties | phase3 | docs/detail/registry/acsd.phase3.properties.md | SCI-P3-PROPS-001 | ALG-P3-001 | DATA-P3-PROPS | API-P3-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P3-PROPS-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase3-resample | phase3 | docs/detail/registry/acsd.phase3.resample.md | SCI-P3-RES-001 | ALG-P3-RES-001 | DATA-TILE-001 | API-P3-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P3-RES-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase3-resample2 | phase3 | docs/detail/registry/acsd.phase3.resample2.md | SCI-P3-001 | ALG-P3-RSMP-IMPL-001 | DATA-P3-RES | API-P3-RSMP-001 | ARCH-001 | SRC-P3-RSMP-001 @ lib/algorithms/resample/p3_resample.h::p3_sampler_open_ex,p3_order_select,p3_resample_check_mode,p3_sample_nearest,p3_sample_bilinear | TEST-P3-RES-001 @ docs/detail/registry/acsd.phase3.resample2.md::TEST-P3-RSMP-DESIGN-001 | EVID-MISSING |
| MOD-acsd-phase3-verify | phase3 | docs/detail/registry/acsd.phase3.verify.md | SCI-P3-VER-001 | ALG-P3-005 | DATA-P3-VER | API-P3-001 | ARCH-001 | SRC-MISSING @ MISSING | TEST-P3-VER-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase3-wcs | phase3 | docs/detail/registry/acsd.phase3.wcs.md | SCI-P3-001 | ALG-P3-PROJ-IMPL-001 | DATA-P3-WCS | API-P3-PROJ-001 | ARCH-001 | SRC-P3-PROJ-001 @ lib/algorithms/projection/p3_wcs.h::p3_wcs_make,p3_wcs_pix2world,p3_wcs_world2pix,p3_wcs_fits_keywords | TEST-P3-WCS-001 @ docs/detail/registry/acsd.phase3.wcs.md::TEST-P3-WCS-DESIGN-001 | EVID-MISSING |
| MOD-acsd-phase3-writer | phase3 | docs/detail/registry/acsd.phase3.writer.md | SCI-P3-001 | ALG-P3-FITS-IMPL-001 | DATA-P3-FITS | API-P3-FITS-001 | ARCH-001 | SRC-P3-FITS-001 @ lib/algorithms/fits_output/p3_output.h::p3_output_write_atomic,p3_output_verify | TEST-P3-WR-001 @ docs/detail/registry/acsd.phase3.writer.md::TEST-P3-WR-DESIGN-001 | EVID-MISSING |
| MOD-acsd-catalog-gaia | service | lib/infrastructure/gaia_xpsd_client/src/gaia_client.h | SCI-AST-001 | ALG-GAIA-001 | DATA-GAIA-001 | API-GAIA-001 | ARCH-001 | SRC-CAT-GAIA-001 @ lib/infrastructure/gaia_xpsd_client/src/gaia_client.c::gaia_client_create_ex,gaia_client_destroy,gaia_client_cone_search,gaia_client_cone_search_for_solver,gaia_client_cone_search_with_spectrum,gaia_client_query_spectrum_by_coords,gaia_client_cone_search_with_photometry,gaia_client_get_spectrum_params | TEST-GAIA-DESIGN-001 @ docs/science/algorithms/GAIA_QUERY.md | EVID-MISSING |
| MOD-acsd-phase1-session | phase1 | docs/detail/registry/acsd.phase1.session.md | SCI-CAL-001 | ALG-CAL-001 | DATA-P1-SESSION | API-P1-SESSION | ARCH-001 | SRC-P1-SESSION-001 @ lib/phase1_session/p1_session.cpp::p1_session_create,p1_session_validate,p1_session_run,p1_session_inspect,p1_session_destroy | TEST-P1-SESSION-001 @ MISSING | EVID-MISSING |
| MOD-acsd-phase1-star | phase1 | docs/detail/registry/acsd.phase1.star-detection.md | SCI-P1-STAR-001 | ALG-STARDET-001 | DATA-P1-STAR | API-STAR-001 | ARCH-001 | SRC-STAR-001 @ lib/algorithms/star_detection/src/sdet_api.cpp::sdet_detect_impl,sdet_gauss_fit,sdet_lm_fit,sdet_detect_ex,sdet_detect_ex_f64,sdet_create,sdet_free_detect_ex | TEST-STAR-DESIGN-001 @ docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md::TEST-STAR-DESIGN-001 | EVID-MISSING |


### 9.1 逐模块层状态（人读正本）

上表给的是各层的 ID 与落点；**§2 规定每层必带状态列**，状态取值域见 §2 表、显式空缺表达见 §2.1。
状态**不能由 ID 推断** —— 例如 `SCI-P1-WR-001` 的 SCI 状态是 `MISSING`（ID 已预留命名空间，
但权威文档尚未落地）；故状态单列成表，与 §9 按 `module_id` 一一对应。

`MOD` 层状态按 §2「由行存在即满足」恒为 `VERIFIED`，不由数据推导，仅作完整性对照。

| module_id | SCI | ALG | DATA | API | ARCH | MOD | SRC | TEST | EVIDENCE |
|---|---|---|---|---|---|---|---|---|---|
| MOD-acsd-conformance-noop | MISSING | MISSING | NONE | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-services-io | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-providers-cpu | MISSING | MISSING | NONE | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-phase1-calibration | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-phase1-cosmetic | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-drizzle | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-hips-writer | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-noise-snr | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-phase1-photometry | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-phase1-star-psf | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED |
| MOD-acsd-phase1-wcs-platesolve | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-writer | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase2-coverage | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase2-integrate | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase2-reject | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase2-resample | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase2-session | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase2-sample | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase2-upm-apply | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase2-upm-fit | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase2-write | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase3-properties | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase3-resample | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase3-resample2 | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase3-verify | MISSING | MISSING | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING | MISSING | MISSING |
| MOD-acsd-phase3-wcs | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase3-writer | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-catalog-gaia | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-session | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |
| MOD-acsd-phase1-star | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | VERIFIED | MISSING |

## 10. 需求→实现→测试 登记册（人读正本）

本节逐条给出需求 ID、权威文档、实现文件与符号、测试 ID 与测试文件、诊断 ID、
错误码与状态。`status` 取 `VERIFIED` / 其余原值。

| requirement_id | type | title | authority_doc | module | implementation_files | test_ids | diagnostic_ids | error_codes | status |
|---|---|---|---|---|---|---|---|---|---|
| SCI-UPM-WEIGHT-001 | science | production UPM 权重 = quality × control_reliability（旧名 geometric_reliability） × control_ivar；禁止 star-SNR/support^p 乘因子<!-- 订正: 检查-跨文档冲突 绿12——authority 正本 PHASE2_UPM.md:23 已定名 control_reliability 并注旧名 --> | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW-001;UPMW-002;UPMW-003;UPMW-006 | UPM_CONTROL_VARIANCE_SCIENCE | - | VERIFIED |
| ALG-UPM-CONTROL-IVAR-001 | science | control_variance = k_corr×(π/2)×σ_bg²/N_retained；control_ivar=1/var；k_corr 由 Drizzle MC 校准（k_gauss×k_geo 两因子查表〔D-08〕） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/sampler.cpp | UPMW-004;UPMW-005;UPMW-007 | ALG-UPM-CONTROL-IVAR-001 | - | VERIFIED |
| DATA-UPM-CONTROL-UNC-001 | science | control estimator = patch median；uncertainty=SE(median) 用 N_retained；ivar 产品缺失显式科学错误 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/sampler.cpp;lib/algorithms/coverage/tools/stage2.cpp | UPMW-006;UPMW-007;V17NonFiniteWeightInvalid | IVAR_MISSING_BEHAVIOR | ERR-P2-UPM-001 | VERIFIED |
| ACR-IVAR-001 | science | weight_policy=ivar 时 ACR 块禁用（cell-ivar×support 与 CPU 逐像素 ivar 不等价）→ CPU canonical path | docs/science/ACR_EQUIVALENCE.md | phase2 | lib/algorithms/coverage/tools/stage2.cpp;lib/algorithms/coverage/src/acr_kernels.cpp | UPMW-006 | CPU_ACR_IVAR_EQUIVALENCE | - | VERIFIED |
| ALG-INTEGRATE-001 | science | integrator 权重资格：NaN/Inf/负→INVALID；0→合法不贡献；>0→可用；reducer 无 policy 知识 | docs/science/algorithms/PHASE2_INTEGRATION.md | phase2 | lib/algorithms/coverage/src/integrate.cpp | V17NonFiniteWeightInvalid;V17StatusesExplicit;V17NonFiniteSupportInvalid | INTEGRATION_ZERO_WEIGHT_CONTRACT | - | VERIFIED |
| ALG-DRZ-GEOM-CACHE-001 | science | bounded target-ipix geometry cache（LRU 8192，run generation 清空）；科学等价 + 操作计数 | docs/science/algorithms/DRIZZLE_GEOMETRY.md | healpix_drizzle | lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp;lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp | UPMW-005 | DRIZZLE_TARGETED_OPTIMIZATION | - | VERIFIED |
| SCI-UPM-PERSIST-001 | science | UPM save→close→open 后 frame_id→theta 绑定不变 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | OpenSavePreservesFrameParameterBinding;UpmPersistAllPermutations;UpmPersistRandomStableIds;UpmPersistSparseDenseBinding;UpmPersistRoundtripChainNoDrift;UpmPersistInsertionOrderIndependent;UpmPersistMosaicSeamEquivalence;UpmPersistInvalidModelRejected | ERR-P2-UPM-001 | - | VERIFIED |
| ALG-UPM-FRAME-BIND-001 | science | parameter_rows[index] ↔ frame_id_by_index[index] 同长无重复 | docs/science/algorithms/UPM_SOLVER.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistAllPermutations;UpmPersistRandomStableIds;UpmPersistInsertionOrderIndependent;UpmPersistInvalidModelRejected | ERR-P2-UPM-001 | - | VERIFIED |
| DATA-UPM-MODEL-001 | science | 模型文件显式 schema/version/frame 列表；重复/缺失/类型损坏稳定报错 | docs/engineering/COMPATIBILITY_POLICY.md | phase2 | lib/algorithms/coverage/src/upm.cpp;lib/infrastructure/aio/src/aio_upm.cpp | UpmPersistInvalidModelRejected | ERR-P2-UPM-001 | format; frames; C | VERIFIED |
| ENG-OWN-001 | science | 模块 ownership/lifetime 契约（OWNERSHIP_AND_LIFETIME） | docs/engineering/OWNERSHIP_AND_LIFETIME.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| ENG-THREAD-001 | science | 线程模型契约（THREADING_MODEL） | docs/engineering/execution_options_contract.md | phase2 | lib/algorithms/coverage/src/stage2_common.cpp | G1ProductionWiringTruth | - | - | VERIFIED |
| ENG-ERR-001 | science | 错误模型契约（ERROR_MODEL，退出码与 orchestrator.h 全量一致） | docs/engineering/ERROR_HANDLING_STANDARD.md | orchestrator | lib/algorithms/coverage/src/upm.cpp;lib/algorithms/coverage/src/integrate.cpp | V17StatusesExplicit | ERR-P2-UPM-001 | INVALID_INPUT;ZERO_VALID_WEIGHT | VERIFIED |
| ENG-IO-001 | science | 原子 I/O 契约（IO_AND_ATOMICITY） | docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md | astro_image_io | lib/infrastructure/aio/src/aio_upm.cpp | SaveOpenRoundtripAndHash | - | - | VERIFIED |
| DATA-HIPS-SIGNAL-001 | science | signal HiPS 数据语义 | docs/science/DATA_SEMANTICS.md | astro_image_io | lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | G3ManifestOrderCanonical | - | - | VERIFIED |
| DATA-HIPS-SUPPORT-001 | science | support HiPS 数据语义（coverage 保守下界） | docs/science/DATA_SEMANTICS.md | astro_image_io | lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | G3ManifestOrderCanonical | - | - | VERIFIED |
| DATA-HIPS-IVAR-001 | science | ivar 产品语义（1/variance） | docs/science/DATA_SEMANTICS.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-DRZ-001 | science | Drizzle 球面重叠科学门（false_negative=0） | docs/science/DRIZZLE.md | healpix_drizzle | lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp;lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp | TEST-DRZ-CAND-001 | DRIZZLE_FALSE_NEGATIVE | - | VERIFIED |
| SCI-DRZ-014 | science | variance 传播 identity（α²v） | docs/science/DRIZZLE.md | healpix_drizzle | lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp | TEST-DRZ-VAR-001 | - | - | VERIFIED |
| SCI-CAL-001 | science | 校准科学门（bias/dark/flat/cosmetic） | docs/science/CALIBRATION.md | calibration | lib/algorithms/calibration/src/calibrator.cpp;lib/algorithms/calibration/include/astro_calibration.h | TEST-CAL-001 | - | - | VERIFIED |
| SCI-AST-001 | science | astrometry/WCS 科学门 | docs/science/ASTROMETRY.md | plate_solve | lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp;lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp | TEST-IPV-001 | - | - | VERIFIED |
| SCI-PHOT-001 | science | photometry 科学门（flux_calibrator） | docs/science/PHOTOMETRY.md | photometric_calib | lib/algorithms/photometry/cpp/src/pc_api.cpp | TEST-SPEC-001 | - | - | VERIFIED |
| SCI-NOISE-001 | science | SNR/Noise 科学门 #1（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-002 | science | SNR/Noise 科学门 #2（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-003 | science | SNR/Noise 科学门 #3（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-004 | science | SNR/Noise 科学门 #4（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-005 | science | SNR/Noise 科学门 #5（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-006 | science | SNR/Noise 科学门 #6（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-007 | science | SNR/Noise 科学门 #7（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-008 | science | SNR/Noise 科学门 #8（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-009 | science | SNR/Noise 科学门 #9（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-010 | science | SNR/Noise 科学门 #10（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-011 | science | SNR/Noise 科学门 #11（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-012 | science | SNR/Noise 科学门 #12（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-013 | science | SNR/Noise 科学门 #13（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-014 | science | SNR/Noise 科学门 #14（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-NOISE-015 | science | SNR/Noise 科学门 #15（noise_model_science_test 子项） | docs/science/NOISE_MODEL.md | snr_estimator | lib/algorithms/noise_snr/cpp/src/noise_model.cpp | TEST-SNR-001 | - | - | VERIFIED |
| SCI-UPM-001 | science | UPM 科学门 #1（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-001 | science | PR#1 frame-binding 测试 #1 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | OpenSavePreservesFrameParameterBinding | - | - | VERIFIED |
| SCI-UPM-002 | science | UPM 科学门 #2（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-002 | science | PR#1 frame-binding 测试 #2 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistAllPermutations | - | - | VERIFIED |
| SCI-UPM-003 | science | UPM 科学门 #3（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-003 | science | PR#1 frame-binding 测试 #3 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistRandomStableIds | - | - | VERIFIED |
| SCI-UPM-004 | science | UPM 科学门 #4（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-004 | science | PR#1 frame-binding 测试 #4 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistSparseDenseBinding | - | - | VERIFIED |
| SCI-UPM-005 | science | UPM 科学门 #5（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-005 | science | PR#1 frame-binding 测试 #5 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistRoundtripChainNoDrift | - | - | VERIFIED |
| SCI-UPM-006 | science | UPM 科学门 #6（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-006 | science | PR#1 frame-binding 测试 #6 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistInsertionOrderIndependent | - | - | VERIFIED |
| SCI-UPM-007 | science | UPM 科学门 #7（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-007 | science | PR#1 frame-binding 测试 #7 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistMosaicSeamEquivalence | - | - | VERIFIED |
| SCI-UPM-008 | science | UPM 科学门 #8（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-008 | science | PR#1 frame-binding 测试 #8 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistRoundtripChainNoDrift | - | - | VERIFIED |
| SCI-UPM-009 | science | UPM 科学门 #9（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-009 | science | PR#1 frame-binding 测试 #9 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistInsertionOrderIndependent | - | - | VERIFIED |
| SCI-UPM-010 | science | UPM 科学门 #10（PHASE2_UPM 语义集） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | S0IdentityCalibrationNoChange;S1KnownAdditiveFieldRecovered;S2LowSnrDoesNotPullHighSnr;SaveOpenRoundtripAndHash | - | - | VERIFIED |
| TEST-PR-UPM-010 | science | PR#1 frame-binding 测试 #10 | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UpmPersistInvalidModelRejected | - | - | VERIFIED |
| TEST-UPMW-001 | science | UPM 权重门 UPMW-001（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW001SnrInvariance | - | - | VERIFIED |
| TEST-UPMW-002 | science | UPM 权重门 UPMW-002（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW002ControlIvarRatio | - | - | VERIFIED |
| TEST-UPMW-003 | science | UPM 权重门 UPMW-003（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW003StarPopulationInvariance | - | - | VERIFIED |
| TEST-UPMW-004 | science | UPM 权重门 UPMW-004（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW004MedianSeIndependentGaussianMc | - | - | VERIFIED |
| TEST-UPMW-005 | science | UPM 权重门 UPMW-005（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW-005 | - | - | VERIFIED |
| TEST-UPMW-006 | science | UPM 权重门 UPMW-006（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW006MissingControlIvarExplicit | - | - | VERIFIED |
| TEST-UPMW-007 | science | UPM 权重门 UPMW-007（V19R3） | docs/science/PHASE2_UPM.md | phase2 | lib/algorithms/coverage/src/upm.cpp | UPMW007PatchEstimatorVsTruth | - | - | VERIFIED |
| SCI-PSF-001 | science | PSF Moffat4 FWHM/拟合质量科学门 | docs/science/PSF.md | psf | lib/algorithms/psf/src/dpsf_psf.cpp | TST-PSF-001;SCI-PSF-001 | - | - | VERIFIED |
| SCI-REJ-001 | science | Rejection 7 种排异自动选择科学门 | docs/science/REJECTION.md | coverage | lib/algorithms/coverage/src/rejection.cpp | TEST-REJ-* | - | - | VERIFIED |
| SCI-INT-001 | science | Integration 信号/support 积分科学门 | docs/science/INTEGRATION.md | coverage | lib/algorithms/coverage/src/integrate.cpp | TST-INT-001 | - | - | VERIFIED |
| SCI-ACR-EQUIV-001 | science | ACR CPU/GPU/混合分块等价与回退科学门 | docs/science/ACR_EQUIVALENCE.md | coverage | lib/algorithms/coverage/src/acr_kernels.cpp | TST-ACR-001 | - | - | VERIFIED |
