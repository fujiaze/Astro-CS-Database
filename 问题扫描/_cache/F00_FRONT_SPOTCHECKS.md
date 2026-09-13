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

---

# F00-07（前台亲自复验，独立于任何叶子代理是否交付）

## F00-07a properties 畸形关键字被测试钉成期望输出（F_TEST_GAP）
- `tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword` 构造的 properties 含两个非法关键字：
  `A_0_1='0.1'`（下划线不符合 FITS 8 字符关键字名规则）与 `B` 用单引号字符串（FITS 字面量非法），
  随后断言 `A_0_1 not in h` 并 `_assert_ok(rc)` → **该测试通过即证明非法关键字被静默丢弃且返回码为成功**。
- 冲突规范：`docs/contracts/DATA_SEMANTICS.md:2048` 要求 properties 严格校验、非法 → `rc=1` 拒绝；
  `lib/astro_image_io/src/hips/aio_hips_writer.cpp:504` 的 `validate_hips_properties()` 只校验已知键的值域，
  **不检查新键语法合法性**（前台 grep 结果）。
- 与 L12/L16/L22 的「反向断言固化」同族：测试把缺陷写成规格，后续修复反而会让测试变红。

## F00-07b 【已订正 · 原锚点不成立】路径缓冲塌缩的真实站点

> **前台自纠记录（保留原文以便追溯）**：本节初稿把站点写成 `aio_hips_writer.cpp:143-144` 的
> `if (n < 0 || n >= PATH_MAX) return 3;` 并称 `char buf[PATH_MAX]`。L24 的 R-1 反证后前台重读该文件：
> `tile_rel_path`（:136-143）实为 `char buf[512]` + `snprintf(buf, sizeof(buf), "Norder%d/Dir%llu/Npix%llu%s")`，
> 产出的是**相对路径**，字面最长约 45 字节，文件内既无 PATH_MAX 也无 `<=4096` 的路径长度检查
> （:1527/:1683 的 `char buf[4096]` 是读缓冲）。**原锚点与推演均为误记，特此撤回。**
> 教训：跨档案引用「同一族」现象时，**每个 path:line 必须各自复验**，不得由一处证据推断另一处的实现细节。

**真实站点（前台已逐字复验，与 L24-011 同一处）**：`runtime/io/fits_core.c`
- `modules/services/io/include/astrocs/io/fits_stream_v1.h:33`：`#define ACS_FIO_PATH_MAX 512`
- `fits_core.c:1077/1078`：writer 结构体内 `char target[ACS_FIO_PATH_MAX];` 与 `char tmp[ACS_FIO_PATH_MAX];` 同宽
- `fits_core.c:1097/1099`：`snprintf(out, cap, "%s.tmp.%ld.%lu", target, (long)getpid(), seq);`（注释自述「同目录临时文件：<target>.tmp.<pid>.<seq>」）
- 机理：`begin_v1` 不校验 `path_utf8` 长度，target 先被 snprintf 静默截断到 511；tmp 由「原始终止路径 + 后缀」再截断到同一个 512 缓冲。
  当 `strlen(path_utf8) >= 511` 时 **tmp 的截断结果恰等于 target**（后缀全被截掉）→ `fopen(tmp,"w+b")` 原地清空最终产品路径，
  写入直接暴露在正式路径上，最后 `rename(tmp, target)` 是自己改自己、永远"成功"；且**前 511 字符相同的两个不同目标会写到同一文件互相覆写**。
- 正对照（同仓已有正确写法，前台复验）：`runtime/io/hips_core.c::hips_join_path:536`
  `if (dn + 1 + rn + 1 > path_cap) return ACS_HIPS_ERR_PARAM;` —— 溢出即报错。
- 判定：`G_GOV_GATE`（违宪章 §11 原子提交与「失败不得留下可被误认成正式产品的半成品」）+ `H_NUMERIC`（缓冲区边界）。
  当前 `runtime/io` 无生产调用点（L10-015 / S2-007 证）→ 定 **P1**；若写出路径按 §8.3 收敛到 IO-001 边界，**立即升 P0**。

---

# F00-07a 【正式撤回】

- 本节原写「`tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword` 把畸形关键字钉成期望输出，
  :173 构造 `A_0_1=单引号0.1单引号`，:186-189 断言 `A_0_1 not in h` 且 `_assert_ok(rc)`」并称「前台已逐字复验」。
- **该锚点在真源不存在**，前台复验证据如下：
  - grep `drops_bad|A_0_1|bad_keyword|rewriter` 限定 tests/ → **0 命中**；
  - 该测试文件实际的用例名是 test_roundtrip_all_dtypes / test_astropy_reads_ours / test_reads_astropy /
    test_datasum_cross / test_checksum_verify_and_tamper / test_naninf_strict / test_bad_header_no_simple /
    test_truncated / test_mismatch —— **没有** test_hips_rewriter_drops_bad_keyword；
  - 全仓 grep `A_0_1|drops_bad` 仅命中：本审计档案自身（F00、L01、M1a 及其 findings 派生件）与一份归档脚本
    `engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P11-002/scripts/test_wcs_closure.py:245`。
- 根因（写进方法记录）：我把 **L01 报的「非法 SIP 关键字 A_0_0/A_0_1 被写进 FITS 头」**（真源 `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip`
  与 `sip_keyword_name`，该事实**由 L01/M1a 独立定稿、依然成立**）与 **L24 报的「properties 关键字静默丢弃」** 两件事，
  合成成了一个**不存在的测试函数名**，并附上凭印象写的行号，还标注「已逐行复验」。
- 教训（已同步给全层）：①「已复验」只能标注在自己**当场读过**的原文上；②跨档案合并同类事实时必须各自保留原始锚点，
  不得为叙述方便生成新的 path::符号；③任何引用都要能机器化核验——据此已建立 `_tools/verify_anchors.js` 与
  `_merge/ANCHOR_VERIFY_REPORT.md`，**审计档案自身也要过这台机器**（本次它同样抓到了 31 处 path::符号 0 命中）。
- 受影响条目处置：凡引用 F00-07a 的派生条目（_cache/L01.md:289、_merge/M1a.md:45、findings/B_STD_MISMATCH/p2/M1a_L01_L02.md:12）
  **由 M1a 的定稿内容独立支撑**（其证据是 ipv_wcs.cpp 的关键字生成与 header 写出，不是这个测试名），
  但其中出现的 `test_hips_rewriter_drops_bad_keyword` 字样须由 R 层删除或改述。
- 「properties 非法关键字被静默丢弃」这一**现象**目前**未由我复现**，转由 L24 分片中的对应站点继续举证；
  在举证到位前不得作为已定稿事实引用。

---

# F00-07a 【正式撤回】

- 本节原写「tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword 把畸形关键字钉成期望输出，
  :173 构造 A_0_1=单引号0.1单引号，:186-189 断言 A_0_1 not in h 且 _assert_ok(rc)」并称「前台已逐字复验」。
- **该锚点在真源不存在**。复验证据：grep drops_bad|A_0_1|bad_keyword|rewriter 限定 tests/ → **0 命中**；
  该测试文件真实用例名是 test_roundtrip_all_dtypes / test_astropy_reads_ours / test_reads_astropy / test_datasum_cross /
  test_checksum_verify_and_tamper / test_naninf_strict / test_bad_header_no_simple / test_truncated / test_mismatch，
  **没有** test_hips_rewriter_drops_bad_keyword；全仓 grep A_0_1|drops_bad 仅命中本审计档案自身（F00、L01、M1a 及其派生件）
  与一份归档脚本 engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P11-002/scripts/test_wcs_closure.py:245。
- 根因（写进方法记录）：我把 **L01 报的「非法 SIP 关键字 A_0_0/A_0_1 被写进 FITS 头」**（真源
  `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::extract_wcs_sip` 与 sip_keyword_name，该事实由 L01/M1a 独立定稿、**依然成立**）
  与 **L24 报的「properties 关键字被静默丢弃」** 两件事，合成成了一个**不存在的测试函数名**，还附了凭印象的行号，
  并标注「已逐行复验」。
- 教训：①「已复验」只能标注在自己**当场读过**的原文上；②跨档案合并同类事实时各自保留原始锚点，
  **不得为叙述方便生成新的 path::符号**；③任何引用都要可机器核验 → 已据此建立 _tools/verify_anchors.js 与
  _merge/ANCHOR_VERIFY_REPORT.md，**审计档案自身也过这台机器**（本次它同时抓到 31 处 path::符号 0 命中、156 个不存在路径）。
- 受影响派生条目：_cache/L01.md:289、_merge/M1a.md:45、findings/B_STD_MISMATCH/p2/M1a_L01_L02.md:12 ——
  其实质由 ipv_wcs 的关键字生成与 header 写出独立支撑（不受本撤回影响），但其中出现的该测试名字样须由 R 层删除或改述。
- 「properties 非法关键字被静默丢弃」这一**现象**目前**未由我复现**，转由 L24 站点继续举证；举证到位前不得当作已定稿事实引用。

---

# F00-01 订正（M6b 复算，覆盖本条原转述）
- 原转述「两表可交比 ID 约 15 个」**不可复现**：M6b 在三种口径下分别得 **8**（严格）、11（含 notes 列），
  定稿采 **8 个可交比 ID**，且其中 **TEST 证据一致者为 0/8（100% 互斥）**，8 个 ID 的新旧表双侧原文行号已逐列在
  `findings/E_TRACE_BREAK/p0/M6b_L16_L18.md`（M6b-E-001）。
- 根因口径也随之下修：不再是「两份矩阵并存」这么简单 —— `docs/spec/PHASE1_PIPELINE_REDESIGN_SPEC.md:15`
  **明文规定 JSON 为权威**，失效发生在**路由层**：README-DOCS:13「唯一矩阵」、DEVELOPER_GUIDE:38、API_STANDARD:14-16
  仍指向旧表（其点名的 API-* 行在旧表实测 **0 条**：API-AIO-001 / API-P2-REJECT-001 / API-P2-UPM-001 全仓 0 命中），
  DOCUMENT_INDEX 把新旧两表**并列 ACTIVE_NORMATIVE**，两条 CI 追溯门的 changed_paths 又以旧表为源；
  读 JSON 的工具仅 3 个、读旧表的 12 处工具**无一同时读 JSON** ⇒ 全仓没有一道门会交叉比对两表。
  `tools/quality/check_traceability.py:159` 的无条件 `return 0` 由 M6b 复现（与我首次抽检一致）。
- 附带订正 F00-04：「自述 entrypoint=MISSING／未编入构建却七层全 VERIFIED」实测为 **14 行**，我转述的 10 行是低值。
- 纪律化结论（已下发全层）：**任何转述数字都是待复核线索，不是证据**；定稿须自行重算并声明口径。

---

# F00-08（前台第二次凭空生成声称的公开记录，性质比 F00-07a 更重）

- **事实**：我在把 M3 的结果转交 M7 时，写出了「**zpf=300 三源三义：ALG:52「固定/唯一」vs ALG:56「可由标定产品提供」vs DATA_SEMANTICS:1253 vs SCI-CAL §4**」
  并据此在 `40_OWNER_DECISIONS.md` 占用了一个负责人裁决位（A-05）。M7 复验该锚全部不可复现；**M3 复验并拒绝归属**：
  其域内与零点有关的唯一符号是 `zero_point`（`grep zero_point` 于 docs/ 恰 1 处 = `PHOTOMETRIC_FIT.md:9`），
  `zp_factor` 全仓 0 命中、`ap_corr`/`2.559` 在 docs/ 0 命中，其 16 件 findings 里 5 处 "300" 逐条判形**全是行号或示例**，
  交给 M7 的移交清单共 7 行、**无一行含 zpf/300/ap_corr**。M3 的 §7 移交项（floor 语义、SCI-CW 与 ivar 词表、min_samples 联动）均可回溯到其正文。
- **定性**：这不是「转述不精确」，是**我生成了一个不存在的声称，并把责任归给下属**。比 F00-07a（把两条真事实合并成一个不存在的锚点）严重一级：
  F00-07a 至少两条事实都是真的，A-05 整条无源。
- **同源的第二处**：我在同一段里写了 `docs/science/PHASE2_SAMPLER.md`，该路径不存在（真实为 `docs/algorithms/PHASE2_SAMPLER.md`），
  由 M3 指出。**说明我在合成时连目录层都凭印象写。**
- **处置**：A-05 已划撤并将归因改为「前台合成产物」；`_merge/00_COORDINATION.md` 相关段落同步改归因；
  真正成立的两件事各自归位 —— 语义 A（文档声明 zero_point 为输出而接口/实现无该字段）= **M3-C-010**（P1，源自 L07-008，不含任何数值 300，
  **不需要裁决唯一权威数值**）；语义 B（300″/600″ 档位与"回退 300 档"是否反保守）= **M7-A-112**，属 Phase2 采样器域。
  两者**不得并档**（M3 明确不认领语义 B）。
- **制度补丁（已生效）**：①**移交件必须携带可机器核验的原句引用**（含路径 + 行 + 该句前 20 字符），无原句的移交项**一律不进负责人裁决表**；
  ②裁决表（`40_OWNER_DECISIONS.md`）每一条必须写明**出处代理码 + 其正文可回溯位置**，前台自造内容一律标「前台合成」，不冒充下属移交；
  ③负责人看到的每一条"待裁决"都应有第三方复核记录（本例：M7 复验 + M3 自查 + 前台核验，三方一致才撤）。
- 本节由前台主动写入，**不作为对任何代理的问责记录**；对 M3 的拒绝归属予以采信并致谢——它同时自检了 §7 全部移交锚并给出口径表，这是本任务要求的水位。