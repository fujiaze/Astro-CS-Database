# 前台独立取证 F00 — 交叉校验基准（不参与叶子统计）

- 代理码：F00（前台，非叶子）
- 方式：仅 read / grep / glob；本文件所列各条均已由前台**亲自读原文**核实，用作校验 L 层与 M 层结论的基准。
- 用法：M1–M6 若在自己范围内遇到同一事实，必须给出与此处一致或明确纠正的证据；不得静默丢失。

## F00-01 追溯矩阵双头：旧表仍被 L1 标准与 SCI 文档当作权威写入点

- 事实：`docs/traceability/TRACEABILITY_SPEC.md` 声明权威是 `TRACEABILITY_MATRIX.json`（机器真相）+ `.csv`（同构视图），
  并规定八层（SCI/ALG/DATA/API/ARCH/MOD/SRC/TEST/EVIDENCE）与显式 MISSING；而仓库同时存在旧表
  `docs/TRACEABILITY.csv`（含 `ENG-*` 层，非八层合同成员）。
- 证据：`docs/README-DOCS.md:13` 仍称「追溯 docs/TRACEABILITY.csv（**唯一矩阵**）」；`docs/standards/API_STANDARD.md:14`
  要求「每个稳定公共 API 关联 API-* ID（见 docs/TRACEABILITY.csv）」；`docs/ARCHITECTURE.md:129` 称
  「docs/TRACEABILITY.csv(76 行，SCI-/ALG-/DATA-/ENG- **全 VERIFIED**)」；`docs/science/``docs/architecture/``docs/DEVELOPER_GUIDE.md`
  等至少 10 处尾注「新增/映射见 docs/TRACEABILITY.csv」（ASTROMETRY:162、CALIBRATION:123、PHOTOMETRY:122、
  PSF:113、NOISE_MODEL:130、INTEGRATION:118、CONTROL_WEIGHT_SNR:86、ACR_EQUIVALENCE:115、DATA_FLOW:35、
  DEVELOPER_GUIDE:38、DOCUMENT_INDEX.yaml:68、diagnostics/TROUBLESHOOTING.md:27）。
  `TRACEABILITY_SPEC.md:92` 只说旧表「沿用历史格式，不由本合同重写」，未声明其废止/降级，也未声明权威归属。
- 判定方向：`E_TRACE_BREAK`（并连带 `I_DOC_HYGIENE`、`G_GOV_GATE`）。按宪章 §1.1 权威分层与 §12.3-1/-9，
  L1 标准把「新增 ID」指向非权威矩阵 = 追溯链事实双头，属 P0/P1 级（M5/M6 定档，须核对旧表与八层矩阵同 ID 的取值是否互相矛盾）。

## F00-02 发布状态文档四份并存，仅一份是宪章 L0，其余未标 superseded

- 证据：`docs/owner/RELEASE_STATUS.md`（DOC-GOV-OWNER-RELEASE-001，ACTIVE_NORMATIVE，宪章 §12.1 指定的 L0，状态词阶梯，
  目标 `0.11.0-alpha.2`）；`docs/RELEASE_STATUS.md:3-8`（V19R8 叙述、「94/95 … D-06 PENDING」、无文档 ID 与状态标签，
  字面量 `PRE_RELEASE_ENGINEERING_FOUNDATION=PASS` / `FINAL_REAL_DATA_VALIDATION=PENDING`）；
  `docs/review/RELEASE_STATUS.md`（DOC-REVIEW-RELEASE-001，ACTIVE_INFORMATIVE，结论 `NOT_READY_FOR_RELEASE`）；
  `docs/archive/review/RELEASE_STATUS.md`（已标 ARCHIVED_NON_NORMATIVE，含 `REVIEW_PENDING` 与 `WAITING_WINDOWS`、目标 0.10.0-alpha.2）。
- 判定方向：`I_DOC_HYGIENE` + `G_GOV_GATE`。三份同名非归档文件对「当前状态」给出不同字面量，且 AGENTS.md 明确
  `REVIEW_PENDING` 现属非法状态；`docs/RELEASE_STATUS.md` 以 V19R8 历史叙述充当现状，违反宪章 §12.3-10
  「活动文档不存在陈旧版本号、历史状态冒充当前状态」。P1 级（M5 定档）。

## F00-03 线程预算门对「裸 omp parallel for」完全不可见，测光模块自证未接线

- 事实：`tools/arch/check_thread_budget.py:69-75` 的 PATTERNS 只有 `std::thread`、`std::async`、
  Win 线程、`omp_set_num_threads\s*\(`、`num_threads\(\s*\\d+\s*\)` 五类；因此**不带 num_threads 子句、也不调用
  omp_set_num_threads 的** `#pragma omp parallel for` 并行区不产生任何命中。
- 证据：`lib/photometric_calib/cpp/src/pc_api.cpp:87,100,232,284,320,335,464,477,512,603,651,684,699,750` 共 14 处裸
  `#pragma omp parallel for`（多数 `schedule(static)`，:335/:699 为 `schedule(dynamic, 64) reduction(+:n_valid_fsyn)`）；
  `lib/photometric_calib/module.yaml:30-36` 自证「threading_model=host_executor_lease **为迁移目标合同值**；现状实现为 OpenMP
  … 但 ThreadLease/omp_set_num_threads **无接线**（迁移整改点，见 … PHOTOMETRIC_FIT.md §13.3 DISP-PHOT-001..009）」。
  对照：`lib/calibration`、`lib/cosmetic`、`lib/drizzle` 三处 `module_entry.cpp` 已按 ARCH-TB-001 走 host 租约注入并登记
  （check_thread_budget.py:49-51）。
- 附带：`lib/photometric_calib/cpp/src/pc_api.cpp:331` 注释写「OpenMP **16 线程**并行计算 F_syn」，而 :335 的 pragma
  并无 num_threads 子句 → 实际线程数来自环境/默认，与注释不符（`D_COMMENT` 且涉及资源合同）。
- 判定方向：`G_GOV_GATE`（门禁对未接线重计算不可见，宪章 §10.4 单一日志预算源 + §17.6 heavy 无硬编码线程）+
  `D_COMMENT`。建议 P1；若 M5 证实其位于生产 heavy 路径且 §17.6 门无法发现，则升 P0。

## F00-04 同一「Phase1 测光」存在两套实现，而八层矩阵对未构建的那套标全 VERIFIED

- 事实 A（被构建的实现）：`CMakeLists.txt:480-486` —— 注释「phase1 wcs + photometry (P1-004)」，
  `add_library(astrocs_phase1_phot STATIC lib/phase1/photometry/photometer.cpp)`。
- 事实 B（被注册与追溯的实现）：`lib/photometric_calib/module.yaml`（`MOD-astrocs-phase1-photometry`、
  `module_id: astrocs.p1.photometry`），:24-26 自述「经 build.ps1 产出 … **未编入根 CMake 主构建（无 photometric_calib
  CMake 目标）**」、「:37 `entrypoint=MISSING`：registry 入口未接；:38-41 descriptor 现状**占位 ID** … **不得反向作为冻结依据**」。
- 事实 C：`docs/traceability/TRACEABILITY_MATRIX.csv:9` 对该模块 SCI/ALG/DATA/API/ARCH/SRC 逐层标 `VERIFIED`，
  SRC 指向 `lib/photometric_calib/cpp/include/photometric_calib.h::pc_calibrate_*`。
- 事实 D：`cmake/install_layout.cmake` 与 `packaging/**` 中检索 `photometry` **零命中**；而
  `lib/core/src/module_adapters.cpp:680-682,5016` 已注册 `astrocs.phase1.photometry` 与
  `astrocs_phase1_photometry_v1` 节点绑定。
- 判定方向：`G_GOV_GATE` + `E_TRACE_BREAK` + `C_DOC_CODE_GAP`。宪章 §12.3-1 要求「模块 manifest、注册表、构建 target
  和产品清单一致」，§17.2 要求追踪无断链；此处四者不一致且矩阵以 VERIFIED 掩盖 —— 属 P0 候选（须 M5 复核
  `lib/phase1/photometry/photometer.cpp` 与 `lib/photometric_calib` 是否给出同一 SCI-PHOT 语义、谁是生产路径）。
- 前台提示：L05/L07 负责测光科学内容，L12 负责构建面与注册表，L16 负责矩阵，M5 必须把三方证据并到一条根因。

## F00-05 活动工程文档以「本版实测」写死检查器通过结论，且基线不透明

- 证据：`docs/ARCHITECTURE.md:114`「## 7. Machine Consistency (S8 gate, **本版实测**)」+:117-124 直接抄录
  `tools/docs_machine_consistency.py PASS 9/9` 与 `config_consistency_check.py PASS mismatches=[]`；:3 顶部标注
  「阶段: Stage C 只读锚定 (**V19R3 True Final Freeze**)」；:130 称「模块级追溯见 docs/modules/*.md(13 份 L5)」；
  :133 落款「Stage C 产出 … 单目的 commit，超时 600s」。
- 判定方向：`I_DOC_HYGIENE`/`G_GOV_GATE`。宪章 §12.3-12 要求「L0 结论可追溯到当前 SHA 的机器证据」，
  §12.3-10 禁止历史状态冒充当前；文档未记 SHA/时间/命令日志指针，且 :130 的「13 份」与实际 `docs/modules/*.md`
  数量（含 registry 26 份）是否相符须由 M 层复核。建议 P1。

## 前台待办（M 层交付后）

1. 逐条比对 F00-01…05 是否被相应 L/M 层发现；未发现的由前台补写定稿条目（标注「前台取证」）。
2. 全部 P0 由前台重新读原文二次确认后才进入 `SUMMARY.md`。

## F00-06 Gaia 位置历元：注册表判定式丢掉「历元」维度却判 CONFORMANT，产品四处自称 J2000（前台已亲验）

- `docs/standards/STANDARDS_REGISTRY.md:186`（D.catalog 的 CLAUSES 行）把「ra/dec **参考历元 J2016.0**」写进标准条款面；
  而 `:193` 该域判定行的「**标准要求**」列被弱化为「位置以 ICRS 表达，RA∈[0,360)、Dec∈[-90,90]」——
  **历元维度不在判定式中**，却据此给出 `CONFORMANT` + 偏差列「无（RA 归一到 [0,360)；frame 契约与 SCI-WCS-001 §3a 一致）」。
  → 判定对象 ≠ 声明的条款面：历元从未被判定，却产出了合规结论。属规程 `B_STD_MISMATCH` 的
  「登记与实际不符 / 错误声称 COMPLIANT」子类。**此点无需外部证据即可定 P0。**
- 产品侧四处一致自称 **J2000**（同一批数值）：`docs/contracts/DATA_SEMANTICS.md:104` 与字段表 `:109`
  （`ra, dec | float64 | deg, ICRS J2000`）、`docs/algorithms/GAIA_QUERY.md:12`（查询锥「单位度，J2000/ICRS」）、
  `lib/gaia_xpsd_client/src/gaia_client.h:14`（「坐标契约: J2000，与 lib/plate_solve 共享 TAN/SIP 坐标约定」）、
  `lib/gaia_xpsd_client/README.md:22,39`、`lib/gaia_xpsd_client/integration/astrocs_catalog_gaia.integration.json:57`。
- `lib/gaia_xpsd_client` 内 grep `epoch|历元|J2016` **无任何历元处理/传播代码**（前台复核：命中仅上述 J2000/ICRS 字样）；
  `:196` 的 XPSD 编码列面声明了 `10 µas/LSB dra`（自行量化单位），但若 L10 所报「parallax/pmra/pmdec 恒零下发」成立，
  则**从 J2016.0 传播到 J2000 或观测历元在数学上不可能**。
- 影响量级：ICRS 是**坐标框架**、J2016.0 是**位置历元**，不可互换；高自行参考星（μ ~ 10″/yr 量级）两历元位置差可达
  **角分级**。`gaia_client.h:14` 明言该坐标约定与 `lib/plate_solve` 共享，测光定标亦以 Gaia 参考星为基准（L07 域）
  → 偏差直接进入天测解算与测光零点数值。
- 处置：已定向 L15 用 web 钉死「Gaia DR3 source 表 ra/dec = 位置在 J2016.0」官方原文并附 URL；
  该原文用于**加强影响**，不是本条成立条件。M2 现在即可落 P0；另须把「注册表判定式与自身条款面不一致」
  这一可复用事实单独立条（它同样适用于其它域的逐行核查）。