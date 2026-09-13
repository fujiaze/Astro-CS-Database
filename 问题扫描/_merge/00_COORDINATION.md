# 合并期协调记录（00_COORDINATION）

用途：登记叶子代理回报中出现的**跨 slice 依赖**，供第二层合并代理（M1–M6）在定稿前完成交叉验证。
前台基准证据见 `../_cache/F00_FRONT_SPOTCHECKS.md`。

## 已收档

### L05（校准 + cosmetique）→ 19 条：P0:3 / P1:10 / P2:6
- **须与 L12 交叉**：`astrocs phase1 run` 的真实路由（`lib/phase1_session/p1_session.cpp` legacy 通道 vs
  `lib/core/src/module_adapters.cpp` IR 节点通道）。L05-004（master flat 退化 fail-closed 只在 IR 通道，
  legacy 通道仍产出全零 flat → 恒 ×10 放大伪产品且 manifest 报 ok）与 L05-011（cosmetic
  `availability=available` 但检测从未接线）的**可达面与定级**取决于此。若 legacy 通道是生产入口，两条均 P0。
- **须与 L11/L12 交叉**：遗留构建通道（`lib/calibration/Makefile`、`build.ps1`、`lib/orchestrator/cpp/Makefile`，
  含 `-ffast-math`）是否仍在交付路径；`-ffast-math` 与宪章 §5.3/§7.3 float64 与确定性要求是否冲突（H_NUMERIC 候选）。
- **与前台 F00-04 同源**：L05-012 模块 ID 双口径 `astrocs.phase1.*` vs `astrocs.p1.*` —— M5 与
  F00-04（测光两套实现、矩阵全 VERIFIED 但未入根 CMake/install/packaging）并为一条根因。
- **与前台 F00-01 同源**：L05-003（`docs/TRACEABILITY.csv` 中 SCI-CAL-001 挂接的 test_files 是
  `test_photometry_apply.cpp`，与校准门无关；SCI §11/§13 引用的 `TST-CAL-001/INV-*/FAIL-*` 全库无定义）
  —— M6 必须与 F00-01 双头矩阵、以及 L16 的矩阵核查并档。
- **科学域重点（M3 主责）**：L05-001 flat_norm 公式（`max(flat/median(flat),0.1)` vs 实现 `max(flat,0.1)`）
  与 L05-002 `lib/calibration/CALIBRATION_PROCESS.md`（K 定义、cosmetic 检测算法、bilinear vs IDW 回填）
  两条 P0 属「SCI 自称与代码逐字一致而实际不等价」，M3 须重新 read 两侧原文逐字核对后定档。

## 待收（M 层输入）

- M1 ← L01、L02、L15；M2 ← L03、L04、L10；M3 ← L05、L06、L07；
- M4 ← L08、L09；M5 ← L11、L12、L17；M6 ← L13、L14、L16、L18。

## 前台已定基准（M 层不得静默丢失）

| 基准 | 内容 | 应落在 |
|---|---|---|
| F00-01 | 追溯矩阵双头（旧 CSV 仍被 L1 标准与 SCI 尾注当权威） | M6 + M2/M3 相关条目 |
| F00-02 | 发布状态四份并存、V19R8 与 0.10.0 陈旧状态 | M5 |
| F00-03 | 线程预算门对裸 `omp parallel for` 不可见 + 测光自证未接线 + :331「16 线程」注释错 | M5（并与 L07 交叉） |
| F00-04 | 测光双实现、注册与构建/安装/打包面不一致、矩阵全 VERIFIED | M5 + M3 |
| F00-05 | ARCHITECTURE.md「本版实测」无 SHA/命令证据指针 | M6 + M5 |
## 收档 L09（Phase2 排异/积分/写出/会话）→ 16 条：P0:3 / P1:10 / P2:3（M4 输入）
三条 P0 的行锚前台未验证（体量原因），**M4 必须逐条 read 复核**：
`lib/phase2_rej/src/rejection.cpp:1835-1849`（N≤4 全拒→UNDERDETERMINED 静默全接受）、
`docs/science/INTEGRATION.md:58` vs `:75`（support reducer 两口径互斥，代码按 :75）、
`lib/phase2/tools/stage2.cpp:1109-1116 / 1363-1375 / 1254-1263`（ivar tile 读失败→逐像素权重静默换成 support）。
第三条与宪章 §6.3 红线直接冲突，且 L09 已给出 fail-closed 对照组（`lib/core/src/module_adapters.cpp:3362-3373`）——
若成立即为本轮最高级别科学违规之一（`H_NUMERIC`+`G_GOV_GATE` 双挂，主类别建议 C_DOC_CODE_GAP 或 A_SCI_DEF，由 M4 定）。
另 L09-008（`tests/backend/test_p2006_canonical_pipeline.py`/`test_p2007_joint_gate.py` 的「每节点执行证据」
依赖 `p2_session.cpp` 日志字符串，而生产走 7 节点 IR 链）与 **M5 的 L12（CLI 路由）** 交叉：请双方并档。

## 收档 L17（治理与发布门禁）→ 22 条：P0:5 / P1:12 / P2:5（M5 输入）

前台已亲自复核 L17 的两条 P0，结论如下（M5 按此精确表述定档，勿沿用不严谨措辞）：

1. **L17-001 成立（已亲验，定 P0）**：`tools/quality/check_traceability.py:143-159` 计算 `rows_broken`、
   写 `evidence/quality/traceability_check.json` 后**无条件 `return 0`**，`__main__` 用 `raise SystemExit(main())`。
   因此无论断链多少，进程退出码恒 0 → 追溯门 fail-open，宪章 §17-2「追踪无断链」在 CI 上不可能变红。
   类别 `G_GOV_GATE`（门禁形同虚设）。注意与 `tools/traceability/check_traceability_matrix.py`（SPEC 指定的正式检查器）
   **并存两份**，M5/M6 须查清 CI 实际挂哪份、后者是否 exit 非 0（若后者有效而前者挂 CI，则是「检查器接错」根因）。
2. **L17-004 部分成立，须改写成精确表述**：`cli/resource_gate.h:93-95` 确有 `kCpuP50MinPercent=90.0`、
   `kCpuMeanMinPercent=85.0`（85% 平均门存在，宪章 §18.2 的 85% 并非缺失）；但
   `:176-181 compute_cores_threshold()` 用的是 **0.80**×min(selected_workers, available_cpus)（旧 D.6 的 80%），
   `:104-107` 逐样本/队列窗口判据为 **75.0 / 0.70 / 10.0s / 50.0**——宪章 §18.2 冻结的是「连续 10 秒低于 **60%** 即失败」，
   代码里对应窗口用的是 50%；且 `:228-229` 在未提供 active window 或 wall<5s 时直接 `return Ok`（fail-open 分支）。
   → 问题实质不是「85% 缺失」，而是 **0.80 cores 阈与 50% 窗口阈沿用被 §18.2 废止的旧值，且 60% 判据不存在**。
   M5 须按此定档（类别 `G_GOV_GATE`，宪章 §10.5/§18.2 冲突属裁决绕过，P0 成立）。

## 收档 L18（文档体系与交叉引用）→ 26 条：P0:2 / P1:19 / P2:5（M6 输入）

- **L18-002 前台判定为高价值 P0 候选**：`docs/KNOWN_LIMITATIONS.md:13` + `docs/TROUBLESHOOTING.md:14,16` 把
  「ivar 缺失 → 积分权重回退 support」当作既有行为陈述，与宪章 §6.3 红线、`docs/science/UNCERTAINTY_AND_COVARIANCE.md:51,67`
  的 fail-closed 冻结口径冲突。此条与 **L09-003 是同一根因的两面**（文档承认回退 vs 代码确实回退）——
  M4 与 M6 必须跨域并档，不得各自归档；主类别建议 A_SCI_DEF/C_DOC_CODE_GAP，由前台终裁。
- L18-001（ARCH 层标 VERIFIED 但 `docs/architecture/cpu/ARCH_CONTRACTS.md` 不存在）与 L16 的锚点/矩阵核查并档。
- L18-005（两套追溯矩阵互相否定）与 **F00-01 同源**；L18-009/010 与 **F00-02 同源**；由 M6 合并，
  但 README 把 ARCHIVED 文件当现行权威一项应归 M5（L17-003 同根因）。

## 前台对 L05-001（校准 flat_norm P0）的逐行复核结论 —— M3 必须据此改判或反驳

前台亲自读了三处实现（均逐字核对）：

1. `docs/science/CALIBRATION.md:49`：`flat_norm = max(flat / median(flat), 0.1)`；`:18`/`:27` 定义 `flat_norm` 为
   「归一后平场 median=1.0, floor 0.1」，产出者写为 `normalize_flat`。
2. `lib/calibration/src/calibrator.cpp:80-95 normalize_flat()`：先 `med = median_inplace(tmp)`，`if (!(med>0)) return;`，
   再 `inv = 1.0f/med`，逐像素 `v = flat[i]*inv; if (v<0.1f) v=0.1f;` —— **与 SCI:49 字面等价**。
   但 `lib/calibration/README.md:75` 与 `lib/calibration/memory.md:20` 自述该函数**当前无调用方**（DISP-CAL-007）。
3. 实际校准路径 `calibrator.cpp:122/132（f32）与 :165/175（f64）`：`v /= std::max(flat[i], 0.1f)` —— **没有 median 除法**。
4. 上游母帧生成 `lib/calibration/src/master_generator.cpp:176-253 generate_master_flat()`：步骤1 逐帧 `/frame_med` + floor 0.1
   （负 median 拒 AC_ERR_PARAM），步骤2 sigma-clip + mean 合并，步骤3 **再整体 `/final_med` + floor 0.1**（`:233-252`）。

**前台判定**：生产链上母帧已 median=1.0，故 `max(flat,0.1)` 与 `max(flat/median(flat),0.1)` 在数值上**近似等价**
（差异仅来自 floor 裁剪后 median 轻微偏离 1.0），L05 所称「相差数量级」不成立。
真正的问题是：`ac_calibrate_frame` 接受任意 `master_flat` 指针，**不校验也不强制**「median=1.0」这一前置条件；
SCI 把 `normalize_flat` 写成 `flat_norm` 的产出者，而该函数无调用方，前置条件完全靠上游习惯维持。
→ M3 定档要求：把 L05-001 改写为「前置条件未入冻结合同 + SCI 声明的产出函数是死代码」，
类别 `A_SCI_DEF` 或 `C_DOC_CODE_GAP`，优先级建议 **P1**（若能找到任何一条把未归一 flat 直接喂进
`ac_calibrate_frame` 的可达生产路径，则升 P0，并给出该路径 path:line）；不得保留「数量级错误」表述。
另注意 SCI:49 与 SCI:18 的分工（定义式 vs 产出函数名）本身是 SCI 内部不闭合，可另立一条 P2。

## 收档 L07（测光/噪声/SNR）→ 档案 445 行，等其回报正文后由 M3 处理

## 前台对 L17-002（版本单源）的复核：成立，且比叶子报得更严重 —— M5 按此定档

前台亲自读原文确认：

- 根 `VERSION` = `0.11.0-alpha.2`（宪章 §16.1 声明的版本唯一输入源）。
- `packaging/astrocs.product.json:3` → `"product_version": "0.11.0-alpha.1"`（**滞后一版**）。
- `packaging/install-tree.contract.json:5` → `"target_version": "0.11.0-alpha.1"`。
- `packaging/dependency-lock.json:5` → `"target_version": "0.11.0-alpha.1"`。
- **加重情节（叶子未报）**：`packaging/schemas/install-tree-contract.schema.json:12` 把版本写成
  `"target_version": { "const": "0.11.0-alpha.1" }` —— schema 用 JSON-Schema `const` **钉死旧版本字面量**。
  于是安装树合同校验只会拿旧常量比对，版本提升时要么静默失配、要么必须手改 schema；
  「VERSION 为唯一输入」在合同 schema 层被复制成了第二个事实源，属 `G_GOV_GATE` + `C_DOC_CODE_GAP`。
- 对照 `lib/calibration/module.yaml:16` → `module_version: 0.11.0-alpha.2`（模块版本直接借用产品版本串，
  与 L17 报的「21 个 module.yaml 同病、lib/star_detector 停在 alpha.1」一致）。
- M5 须核实两道版本门（`ci/check_version.py`、`tools/doccheck/check_version_namespaces.py`）扫描面是否真如 L17
  所说只覆盖 6 个根文件 + docs/{governance,owner} 并把 product_version/module_version 列为行级豁免；
  若成立 → 门禁无法发现本条漂移，属「门禁形同虚设」，P0 成立（宪章 §17.11 哈希/版本/provenance 全过 + §16.1）。
- 另需与 `evidence`/`artifacts` 免报区**无关**：本条全部证据位于真源 `packaging/**`，不属于规程 §0.4 免报清单，可放心登记。

## 已启动

- **M4（Phase2 域：L08+L09 合并验证）运行中**，代理 id `dcbc3029`；任务书含 7 条 P0 的逐条复核硬要求
  （L08-001 插值须自行复算、L09-002 两句原文对照、L09-003 与 L18-002 跨域 related 双向挂接）。
- 待 L01/L02/L15 → M1；L03/L04/L10 → M2；L06 到位后与 L05/L07 → M3；L11/L12/L17 → M5；L13/L14/L16/L18 → M6。
## 前台对 L17-009（serial-heavy 门）的复核：成立，且可加一条 fail-open 证据

`tools/quality/check_serial_heavy.py:5-8` 自称「从生产调用图定位每个 cpu_heavy 节点实现与编译 target …
扫描裸 thread pool、serial pixel/sample loop、全局锁」，但 `:44-59` 的 `scan()` 实读对象只有
根 `CMakeLists.txt` + 四个写死文件（`lib/phase2/src/sampler.cpp`、`lib/phase2/src/upm.cpp`、
`lib/phase3_session/p3_session.cpp`、`lib/phase3_session/p3_resample.cpp`），其余生产重计算（测光
`pc_api.cpp` 14 处 OMP、`lib/calibration/src/master_generator.cpp:246` 的 `#pragma omp parallel for`、
HiPS 写出、plate solve）**完全不在扫描面**。
另：检查 1 的判定式是 **四者取 OR**（`"P2_ENABLE_OPENMP" not in cmake and "P2_PARALLEL" not in cmake and
"-fopenmp" not in cmake and "std::thread" not in sampler+upm`），只要 sampler/upm 任一文件里出现一次
`std::thread` 字样（哪怕在注释里），整个 Phase2 并行默认关闭就会判为通过 → 结构性 fail-open。
`:28-34` 的 `ALLOWLIST` 仅两条且不参与上述判定（与 L17「allowlist 为死配置」一致）。
→ M5 定档：`G_GOV_GATE`，宪章 §17.6 + §10.5；因它同时是「§17.6 门实际覆盖不到 F00-03 那类未接线重计算」的根因，
建议 **P0**，并与 F00-03 并档为一条根因（机器门覆盖面 ≠ 声明覆盖面）。
注：`:10-13` 自述的负面 fixture 声称要抓「resource gate 无生产 caller」，而实测 `cli/commands.cpp:861-865`
确已给 `GateConfig` 赋值（说明该项已被修过或该自检已失效）——M5 顺带确认自检与现实是否仍一致。

## 收档 L07（测光/噪声/SNR·控制权重）→ 25 条：P0:6 / P1:14 / P2:5（M3 输入）

- 叶子自述已对 **145 个位置行号 + 106 条引文**做 grep+read 双向复验、零失配，M3 可优先复核其 P0。
- **前台已独立佐证 L07-001**：`docs/science/PHOTOMETRY.md:38`（SCI-PHOT-001，FROZEN）规定
  `|r_consistent|>=3` 才进 IRLS、否则 `NO_DATA`，并把锚点写成 `star_matcher.cpp:552-559`；
  前台读 `:540-567` 见该区间**只有** `sigma_residual` 计算与 `if (!r_inliers.empty())`，无 `>=3` 也无 `>=2` 门。
  即「FROZEN SCI 声称的门在其自己给出的行锚处不存在」——三重问题叠加（门缺失 + 锚点错位 + 有效域失守），
  单星/双星即可整帧定标。M3 须按 `A_SCI_DEF` 或 `C_DOC_CODE_GAP` 定 P0，并把锚点错位另立 `E_TRACE_BREAK`。
- **三案合一主线（前台终裁并档）**：`L07-004`（SCI 之间对 weight_mode=2 互斥定义）+ `L09-003`（stage2 tile 级
  静默回退 support 当权重）+ `L18-002`（KNOWN_LIMITATIONS/TROUBLESHOOTING 把回退写成既有行为）
  → 同一条宪章 §6.3 红线的三层断裂：**定义层互斥、实现层违反、限制层承认**。M3/M4/M6 各自落条目并互挂 related，
  前台在 `SUMMARY.md` 作为"主线问题一"陈述。
- L07 亦报**并发编辑导致行号漂移**（同 F00 注记）：M3 复核 `lib/phase2/tools/stage2.cpp`、
  `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` 时按符号 grep 重定位，不得以行号不符为由剔除。
- L07-006（`test_noise_model_oracle.py` 的 median()/rsig() 为死代码、无 numpy、无 1e-9，整类被
  `skipUnless(g++)` 静默跳过，而 SCI-NOISE:117/:140 称 NumPy rtol 1e-9 复算）与 L08-004（UPM 侧同类 Oracle 缺失）
  是**同一模式的两个实例**：M3 与 M4 各自登记本域事实，前台归纳为"SCI 声称的独立 Oracle 系统性缺位"。

## 更正与精确化：前台对 L07-001 的佐证范围（M3 注意，勿照抄我的措辞）

前台实际逐字读到的是：`lib/photometric_calib/cpp/src/star_matcher.cpp:552-559` 处实现为

```
552:     double sigma_residual = 0.0;
553:     if (!r_inliers.empty()) {          // ← 即 |r_inliers| >= 1
```

而 `docs/science/PHOTOMETRY.md:38` 声明的正是 **同一区段**：「`|r_inliers|>=2` 才估计 `sigma_residual`，
否则 `sigma_residual=0`（`552-559`）」。→ **前台已确证**：SCI 给出的行锚与其自己声明的门槛数值不符（≥2 vs 实际 ≥1）。

但「`|r_consistent|>=3` 才进 IRLS，否则 `NO_DATA`」这一条**前台未逐行验证**（未读 :455-470 的 mag_consistent 分支）。
L07 报的是 `star_matcher.cpp:462 只判 empty`。M3 必须自己 read 该处再定档；若成立，则一条 finding 里同时含
「门槛数值不符（可证）」+「门槛缺失（可证）」+「锚点指向错误代码（可证）」三个事实，仍属 P0（有效域失守）。
定稿时请把三个事实拆开表述，不要只写「≥3 未实现」。

## 收档 L10（Gaia 星表 + data/artifact 语义合同）→ 26 条：P0:2 / P1:16 / P2:8（M2 输入）

- **L10-001 前台判为最高优先级国际标准问题（已定向转 L15 用 web 核证）**：
  `docs/standards/STANDARDS_REGISTRY.md:193` 的 D.catalog 把「ra/dec 参考历元 **J2016.0**」写成冻结条款并标
  `CONFORMANT`、偏差「无」；而 `docs/science/ASTROMETRY.md:33`、`docs/contracts/DATA_SEMANTICS.md:104`、
  `docs/algorithms/GAIA_QUERY.md:12`、`lib/gaia_xpsd_client/**/gaia_client.h:14` 对外声明 **ICRS/J2000**，
  且模块内 `epoch|历元|J2016` grep 零命中、XPSD 不含 parallax/PM 无法传播。
  → 二者不可能同时成立；若 XPSD 实存 J2016.0 位置，高自行参考星系统偏差可达角秒级，**直接进入 plate_solve
  与 photometric_calib 数值链**（与 L01、L07 交叉）。M2 落档时 `B_STD_MISMATCH`，等 L15 的 web 原文证据后定 P0 措辞；
  若 L15 未带回权威原文，此条以「注册表条款与产品对外声明互斥 + 无偏差登记」为准，仍属错误合规声明。
- **L10-002**（`lib/gaia_xpsd_client/src/module_entry.c:732,876` 把 `match_idx` 按 matched_count 截断，
  合同 `DATA_SEMANTICS.md:113` 要求按输入坐标序长 n_coords、未匹配 −1）：M2 须核实"部分未命中"是否有测试覆盖
  （L10 称 `gaia_adapter_test.c:607` 用例全命中）；成立即 `C_DOC_CODE_GAP` P0（行↔坐标映射不可恢复会静默错位匹配）。
- L10 的 7 条待复核中**两条需外部权限**（上游 XPSD 建库历元事实、`@f7fa3160` 锚与 V18R3 历史差分需 git）：
  前台已把前者转 L15；后者因全项目禁 git，**列为「本轮无法验证」写入 SUMMARY 的移交项**，不得假装已核。
- 与 M5 交界：`check_standards_registry.py 未登记进 ci/checks.json`、`DATA-001 权威文档
  docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md 不存在`、`astrocs.calibrated_frame.v1 不满足自家 type_id 四段正则`
  ——三条都是"机器门/合同自相矛盾"，M2 落具体事实，M5 落门禁根因，双方 related 互挂。
- 与 M6 交界：`fits_core.c 恒带 CHECKSUM 全零占位卡且 verify 豁免条件逻辑倒置` 属 D.fits 合规失真，
  M2 落 `B_STD_MISMATCH`，L15 已受命核 FITS 4.0 §6 原文。

## 派发层观察（同类失效已出现两次，后续派发须内置自校验）

L12、L16 均在**未落盘**的情况下结束回合（L16 只说"现在把结论落成档案"）。已各自发消息催办并加了
「写完必须回读文件前 12 行 + 末 3 行自校验」的硬要求。**后续 M 层派发任务书一律内置第 0 步：**
先创建档案骨架并回读确认存在，再边取证边追加，禁止把"准备写"当成"已写"。

## 前台补记：L10-001 升级为 F00-06（M2 直接引用，勿重复取证）

见 `_cache/F00_FRONT_SPOTCHECKS.md` §F00-06。要点：P0 成立**不需要 web 证据** ——
`STANDARDS_REGISTRY.md:186` 把「参考历元 J2016.0」写进条款面，`:193` 的「标准要求」列却只剩「ICRS + RA/Dec 值域」,
历元从未被判定却判了 `CONFORMANT`、偏差「无」；同时产品四处（DATA_SEMANTICS:104/:109、GAIA_QUERY:12、
gaia_client.h:14、README:22/39、integration.json:57）自称 J2000。M2 主类别落 `B_STD_MISMATCH`，
并把「注册表判定式与自身条款面不一致」另立一条（该模式在其它域同样可能成立，L15 已在核）。

## 收档 L01（天测/plate solve，20 条：P0:6/P1:11/P2:3）、L02（Phase3 投影，20 条：P0:2/P1:13/P2:5）、L13（注释广度 Phase1，20 条：P0:1/P1:13/P2:6）

### 独立收敛（跨代理同证一条 P0）—— 本轮最高置信度证据
- **L01-004 与 L13-001 是两个不同代理各自独立命中同一事实**：SIP 逆向的 SCI 冻结门 `1e-4 px` 在生产代码里被自证
  「数学不可达 / 需负责人裁决」，实测极限约 `7.6 px`（`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:397-403`），
  而 `docs/science/ASTROMETRY.md §11` 仍声明 Oracle 全过。→ 该条 **P0 成立度极高**，M1 定稿时可引用两个来源互证。
- **加重事实（前台补充，M1 必查）**：`docs/standards/STANDARDS_REGISTRY.md:73`（D.spherical-projection 偏差表 SIP §A 行）
  把 DISP-WCS-006 的口径登记为「AP/BP 以 **7×7 网格**最小二乘拟合而非标准迭代反演；**SCI 层已显式冻结该口径**」，
  而 L01-004 报的实现是 **41×41 阶 5 + 81×81 阶 7**。若经 M1 逐字核实，则构成
  「**已登记偏差的描述与代码事实不符**」+「登记文本声称 SCI 已冻结某口径，而 SCI 实际写的是另一口径」——
  属规程 `B_STD_MISMATCH` 的"登记与实际不符"子类，与 F00-06（历元维度未判定却判 CONFORMANT）同属
  **偏差登记表可信度系统性失真**这一根因。请 M1 落一条独立 finding 并在 SUMMARY 主题化。

### CRPIX 原点簇（三条同根因，M1 并档）
- L01-002（`SCI` 契约"0-based 自洽、与标准恒差 1px" 与实现 `out.x = u + CRPIX` 数值相悖；STD-F1 登记"已闭环"
  与测试面 `crpix+1` 桥、WCS-003-F1"待 owner 裁决"三方冲突）、L01-003（`module_adapters.cpp:1785-1787/1953-1955`
  自检以 0-based 下标喂 1-based 契约 → 样本配带 1px 系统偏移、往返门对原点零判别力）、L02-001（CAR/AIT 不减
  fiducial 偏移，参考像素映射到 δ=0，注册表 `:64` 判 CONFORMANT「偏差=无」，且 C++/Python 两套"独立"oracle
  **内嵌同一错误约定故测不出**）。→ 主题：**参考原点约定在 SCI/代码/注册表/oracle 四层同错或互斥**，
  其中"oracle 与被测实现共源错误约定"是 `F_TEST_GAP` 的独立事实，必须单列（宪章 §12.3 末段：科学正确性须由
  独立 Oracle 证明；oracle 共源 = 独立性不成立）。

### 跨域挂接
- **L01-015（历元/自行/视差传播缺失、无误差预算）与 F00-06 / L10-001 同根因**：M1 落天测侧事实并写
  `related: F00-06, L10-001`；M2 落星表/注册表侧事实。两侧证据合起来才完整，任何一方都不得宣称"已在对方域查证"。
- L02-014（节点重采样 sampler `open` 失败 fail-open 整行带静默 NaN 却照常计数发布）与 M4 的 L09-003（tile 读失败
  静默回退 support）是同一**"读失败静默降级"模式**在 Phase2/Phase3 的两个实例 → 前台归纳为一条系统性根因，
  M1/M4 各自落本域事实并 related 互挂。
- L13 与 L01/L02 重叠项（L13-001↔L01-004；L13-002 悬空合同锚↔L01-014；L13-013 双"唯一生产实现"互斥↔L07 域）
  由 M6 与 M1 会签去重：**同一事实只在一处定稿**，另一处写 related 指过去。

### 派发调整
- **M1 拆两轮**：M1a = L01+L02（已启动）；L15（国际标准横向）到货后另派 **M7**，任务是把 L15 与 M1a/M2 的
  标准合规条目做并档与"登记失真"主题化，避免等 L15 阻塞 M1a。
- **L12、L16 未落盘已催办**；L14 仍在跑；L06 完成后并入 M3（已预告）。

## 收档 L04（Drizzle 球面重建，19 条：P0:4/P1:7/P2:8）

### 头号系统性根因浮现（前台主题化；M 层只落本域事实）
**「独立 Oracle 缺位 / oracle 与被测实现共源」—— 四个不同代理在四个互不相干域独立命中同一模式：**
- Phase3 投影：C++/Python 两套"独立"oracle 内嵌同一错误投影约定（不减 fiducial 偏移），往返测不出 —— L02-001
- Drizzle：oracle 测试无 CMake/ctest/CI 注册；"9003 例全枚举"约 51% 用例复用生产 compute_overlap_area_g —— L04-004
- 噪声模型：test_noise_model_oracle.py 的 median()/rsig() 为死代码、无 numpy、无 1e-9，整类被 skipUnless(g++) 静默跳过 —— L07-006
- UPM：SCI 声称的 NumPy 独立参考 Oracle 在真源 tests/** 不存在；UPMW 硬门不挂产品构建 —— L08-004 / L08-003
→ 宪章 §12.3 末段（科学正确性须由推导 + 独立 Oracle + 性质测试 + 抽查共同证明）与 §13 验证层级在四域同时不成立；
  与 M5 侧的「机器门 fail-open」（追溯门无条件 return 0、serial-heavy 只 grep 四文件、版本门扫描面窄 + schema 用 const 钉死旧版本）
  互为表里：**门不看、oracle 不独立**。各代理只落本域事实，前台在 SUMMARY 并为主题。

### 交给 M2a 的两条必算项（不接受转述）
- **L04-002**：按 docs/science/DRIZZLE.md §5 的 w = a/A_drop 自行推导常量场下 S_p 与 pixfrac 的关系，
  验证"仅 pf=1 等于 B0、否则 B0/pf²"；再核 §11:117 与 §7:82-84 是否字面冲突、生产默认 pixfrac 来源
  （lib/orchestrator/cpp/json_config.h:67）、接线测试是否确实只覆盖 pf=1.0。若成立 → signal 面亮度语义
  系统性偏高 1/0.8² = 1.5625×，属本轮最严重科学数值 P0（改变产品信号 56%）。
- **L04-001**：逐行确认累加域选择链（engine.h:37-39/60 默认、hp_drizzle_api.cpp:1019-1027 按块 dtype 选域、
  module_adapters.cpp:1960 恒投 AIO_BLOCK_FLOAT32、engine:2016-2017/2127 写 HISS signal_dtype），
  判定"config 要 FP64 仍 FP32 累加"与"元数据与真实精度脱钩"是否成立（宪章 §5.3 + §4.3）。

### 分界与不合并说明（M2a 须照此写）
- L10-001/F00-06（历元维度未判定却判 CONFORMANT）与 L04-010（px²↔sr 混用、variance 记 ADU² 实为 ADU²/sr²、
  tile 无 BUNIT）**同症状不同根因**（前者标准条款误判、后者单位链未闭合），不要合成一条；两者都触发 §4.1/§4.3。
- L04-007 与 L10 的「DATA-001 权威文档不存在 / SCI-P1-DRIZ-001 等未定义即被引用」并为「追溯 ID 未经定义即被引用」，
  related: F00-01（矩阵侧由 M6 落，勿重复）。
- L04-008（README 说"无源码/DLL 未建"而实际已接线）方向与 F00-04（矩阵说 VERIFIED 而实际未构建）相反，
  各自落档，由前台归纳为「状态声明无单一事实源」——任何代理不得自行合并。
- L04-005 提到代码修复的 commit 哈希：我们禁用 git，**引用哈希不得作为既成事实**，改写为"代码现状 vs 文档记载失实"。

### 派发调整（当前格局）
- M1a = L01+L02（在跑）；L15 到货后另派 M7（标准横向，与 M1a/M2a 并档"登记失真"主题）。
- M2a = L04+L10（本次启动，45 条）；L03 到货后派 M2b，并与 M2a 在 HIPS_WRITER / IO_003 原子发布重叠项会签。
- M3a = L05+L07（在跑，44 条）；L06 到货后追加给 M3a 续并。
- M4 = L08+L09（在跑，33 条）。M5 = L11+L12+L17；M6 = L13+L14+L16+L18（L12/L16 未落盘已催办补写）。

## 收档 L01（20：P0:6/P1:11/P2:3）、L02（20：P0:2/P1:13/P2:5）、L13（20：P0:1/P1:13/P2:6）

### 跨代理独立收敛（本轮最高置信度证据）
- **L01-004 与 L13-001 由两个不同代理各自独立命中同一事实**：SIP 逆向的 SCI 冻结门 1e-4 px 被生产代码自证
  「数学不可达 / 需负责人裁决」（实测极限约 7.6 px，lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:397-403），
  而 docs/science/ASTROMETRY.md §11 声明 Oracle 全过，冲突只活在代码注释里、未登记偏差。→ P0 成立度极高。
- **加重事实（已写入 M1a 任务书为必查项）**：STANDARDS_REGISTRY 把 DISP-WCS-006 登记为「AP/BP 以 7×7 网格拟合、
  SCI 层已显式冻结该口径」，而 L01 实测实现为 41×41 阶 5 + 81×81 阶 7。若经 M1a 逐字核实，即构成
  「已登记偏差的描述与代码事实不符 + 登记文本声称的 SCI 冻结口径与 SCI 实际文本不一致」，与 F00-06 同属
  **偏差登记表可信度系统性失真**（直接反证发布门禁 §17.1「SCI/ALG 已冻结且无冲突」）。

### CRPIX 原点簇（L01-002/003 + L02-001 并档，M1a）
- L01-002：SCI 契约"0-based 自洽、与标准恒差 1px"与实现 out.x = u + CRPIX 数值相悖；STD-F1 登记"已闭环"与
  测试面 crpix+1 桥、WCS-003-F1"待 owner 裁决"三方互斥。
- L01-003：自检以 0-based 下标喂 1-based 契约（module_adapters.cpp:1785-1787/1953-1955）→ 样本配带 1px 系统偏移，
  往返与前向交叉门**对原点零判别力**。
- L02-001：CAR 取 Y=-θ 且 CAR/AIT 不减 fiducial 偏移（参考像素落到 δ=0），注册表判 CONFORMANT"偏差=无"，
  且 C++/Python 两套"独立"oracle 内嵌同一错误约定 → 必须**另立一条 F_TEST_GAP：oracle 与被测实现共源 = 独立性不成立**。
  L02 自述"当前生产仅 TAN 放行故暂不受损"：M1a 须核实该约束在哪里强制、是否真在生产路径生效；
  前瞻未损**不等于**已免（合同+测试+注册表已把错误口径固定化）。

### 其它挂接
- L01-015（历元/自行/视差传播缺失、无误差预算）与 F00-06、L10-001 同根因：M1a 落天测侧事实并写 related，
  M2a 落星表与注册表侧，两侧都不得宣称"已在对方域查证"。
- L13-002（公共头唯一合同锚指向 SNR_SCIENCE_DERIVATION.md、SNR_REDESIGN_CONTRACT.md、02_FROZEN、
  00_COMMON_CONTRACTS.md 等不存在文档）与 L01-014（接口注释悬空引用 docs/05/24/25）同属"未经定义即被引用"，
  由 M6 与 M1a 会签去重，同一事实只在一处定稿。
- L13-005（PSF [N,9] 布局三方割裂、两份 L1 文档 A/B 命名互为倒置、文档自述"按错布局解析＝科学结果错误"）
  属 L06/M3 域，M3a 续并 L06 时必须与之并档。
- L13-013（同一 SCI-NOISE-001 出现两个互斥"唯一生产实现"宣称：lib/phase1/noise 标量 median+MAD vs
  snr_noise_model_v1）与 L07 域重叠，M3a 主落，M6 只落注释层事实。
- L13-008（dpsf/sdet/ipv 私造 logger 写源码树 logs/、科学路径无条件 stderr，违宪章 §11）交 M6，
  并与「根目录/源码树散落运行产物」区分：源码树内 logs/ 属真源问题，可登记。

## 收档 L12（CLI 单入口 / C ABI / 模块注册 / 构建面，24 条：P0:5/P1:16/P2:3）

### 前台下发两条跨域悬案的结论（M3/M5 必须据此改判）
- **① `astrocs phase1 run` 真实路由 = IR 节点链，不是 p1_session legacy 通道**：
  `cli/commands.cpp:1598` → `:796 run_pipeline({1})` → `cli/runtime_client.cpp:297/:328 build_pipeline_ir` →
  `lib/core/src/module_adapters.cpp:5459-5473`（P1 八节点表）→ `:4262 make_p1_node_module` → `:4109 P1NodeModule` →
  `:4207 p1_op_calibrate`；`p1_session_run/p2_session_run/p3_session_run` 在 `cli/` 下 **grep 零命中**（L12 结论）。
  → **对 M3 的直接影响**：L05-004「p1_session 通道仍产出 ×10 伪产品」**不能因 CLI 入口可达而升 P0**；
  改判要求：(a) 若 `p1_session` 仍有其它生产调用方（executor / 节点内部 / 其它模块 / 测试外的工具），给出 path:line 才保留 P0；
  (b) 否则降为 P1「旁路通道内的退化产物缺陷（未接入 CLI 入口）」，并**另立一条**记录
  `lib/phase1_session` 这类"未被任何入口调用的科学通道仍在仓库内且 README/module.yaml 以现状口吻描述"的 dead-code 风险（`C_DOC_CODE_GAP`）。
- **② 遗留 Makefile/build.ps1（含 -ffast-math）不在根 CMake 交付图**（根 `add_subdirectory` 全集无 `lib/orchestrator`、无 Makefile 调用），
  且 `-ffast-math` 已由 `docs/algorithms/CALIBRATION_ALGORITHMS.md:264/:404` 以 **DISP-CAL-011 登记**为遗留通道。
  → M3/M2 落此条时**必须区分"已登记未修复"与"登记失实"**：本条属前者，不得写成未登记违规（这正是我在 §「L04-005」里立的分类规则）。
  → 但 L12 新发现**两条仍在被 CI/测试/文档当成交付面使用的旁路 CMake 通道**：`cmake -S cli` 与 `cmake -S lib/phase2`（见 L12-001/002），
  这条是"未登记"的，归 M5，属 `G_GOV_GATE`。

### M5 的三条头号 P0（均为"门禁打在假对象上"，与 F00/M5 既有 fail-open 并主题）
- **L12-001**：CLI 命令树/协议门禁跑在**非产品兼容二进制**上 —— `ci/steps/linux_build_root_graph.sh:22-24` 在根图之外另
  `cmake -S cli` 产出**同名 `astrocs`**；`tests/cli` 全部 golden 与 `tools/check_api_docs.py:132-142` 的命令树一致性只对该
  二进制执行，且**二进制缺失时无 else 分支 → 静默 0 检查**；更重：该兼容工程把 ARCH-001 §7 冻结为违例的第二调度器
  `aio_pipeline_engine.cpp`（`cli/CMakeLists.txt:164`，根图无此行）以 `-w` 消警编入。→「单入口」与「ACR/第二调度器不可达」两道门
  **验的都是假对象**，这是 §17.4/§17.5 的直接反证。
- **L12-002**：`docs/architecture/BUILD_GRAPH.md` 与根构建面**零重叠**（对产品目标提及数为 0），而 CON-BUILD-GRAPH 只做
  目标名子串存在性检查 → **订正文档反而会使门变红**（门反向锁定过期事实，宪章 §12.3-1 实质失效）。
- **L12-004**：AST-API 检查器（§12.3-5 指定器）把**同一头文件**的正则声明集合与同一 AST 的 name 集合互比 → **恒真**，
  参数数提取后弃用，且不读任何 API 文档，只挂 linux-main。与 L12-003（`API_CONTRACTS.csv` 422 行全 VERIFIED、
  其中 396 行 `test_ids=TST-GEN-001` —— 该 ID 在 tests/ 与 docs/traceability/ 命中 0、仅存在于检查器负例 fixture）
  构成「合同表批量填 VERIFIED + 检查器恒真」的双层失效。

### 与既有证据的并档
- L12-017（MODULE_MAP 行锚 4257/4282/4309 全错指，真实注册表在 5459/5484/5511；另引不存在的"宪章 §F.1"）
  与 L18-008（owner L0 锚点漂移 4/14）、L05-017、L07-016、L10（行锚 +120~170 漂移）→ 前台归纳为
  **「行锚系统性失效」主题**；M5/M6 各落本域实例，related 互挂。
- L12-016（三套退出码/状态码数值语义冲突、ERROR_MODEL 仍规范已废弃 orchestrator.exe 码表、RESOURCE→5 与 =10 相悖）
  与 L17-008（L0 用归档文件的字母条款号冠以"宪章"引用）同属「引用已被 supersede 的口径」，M5 主落。
- L12-006/007（packaging manifest/schema/contract 停 alpha.1、schema 把旧版钉成 pattern/const、两个版本检查器都不扫
  packaging、`types.h:20` 手抄 alpha.2、检查器整行豁免 module_version）与 **F00 的 L17-002 复核结论完全同源**，
  M5 合并为一条根因「版本单源在 packaging 与 module 两处被复制且门不扫」，实例全列。
- L12-014（产品静态链接、secure_loader 仅测试引用、全部 unit sha256=null → §18.4 内容绑定成为空操作、
  `mod001_*` 名字不匹配 unittest discover 且不在 ci/checks.json → 从不进 CI）→ 「插件签名清单白名单」这条
  **负责人裁决（§18.4 第 4 项）在机器上无实现**，P0 候选，M5 主落并写 related: L17。
- L12-015（lifecycle：`state` 为普通 int 却注释"原子置位"；destroy 先 free 再读已释放内存做 double-destroy 检测，
  且返回 void 无法报 ACS_ERR_STATE）→ `H_NUMERIC`/`C_DOC_CODE_GAP`，与 L11 的并发域重叠，M5 会签。

### 现场（重要）**
- L12 证实并发修改导致 `module_adapters.cpp` 关键锚点在扫描期间 **5023→5435、3790→4202** 位移。
  所有 M 代理：该文件与 `cli/commands.cpp` 的行号一律改用符号锚，不得因漂移判叶子造假。

## 收档 L11（运行时/线程预算/CPU 后端/ACR，21 条：P0:6/P1:13/P2:2）—— 与 F00-03 收敛

### 三重独立收敛：机器门"看不见"未接线重计算
- 前台 F00-03：`tools/arch/check_thread_budget.py` PATTERNS 只有 5 类，**裸 `#pragma omp parallel for` 不产生任何命中**，
  而 `lib/photometric_calib/cpp/src/pc_api.cpp` 有 14 处裸并行区、模块自身 `module.yaml:30-36` 自证未接 ThreadLease。
- L11 独立量化同一漏洞：对 `lib/**` 409 文件重放该检查器，`std::thread` 命中 **0**，而仓库内
  `std::vector<std::thread>` 形态 **≥29 处**、`#pragma omp` 并行区 **85 处**；`cli/`、`providers/`、`runtime/` 根本不在扫描面；
  并且**单测把"0/0"直接写成通过断言**（自证式测试）。→ 与 F00-03 同根因，M5a 与前台并档为一条 P0 根因。
- L13（注释广度）亦命中 `pc_api.cpp:331` 注释写死"OpenMP 16 线程"而 pragma 无 num_threads —— 三源同证。

### L11-003/004/005 是本轮"门禁可信度"主题的最硬证据（M5a 必须逐条复算语义，不接受转述）
- **利用率口径错误（L11-003）**：门按"单核=100%"比较，而生产喂值是 `ΔCPU秒/墙钟` ⇒ 分配 4 核时 0.9 核也判"90% 通过"。
  M5a 要求：把喂值链（`cli/resource_recorder.h` 计算 → `cli/commands.cpp:861-865` 赋 `GateConfig` →
  `cli/resource_gate.h:241-244` 与 `kCpuP50MinPercent/kCpuMeanMinPercent` 比较）两侧口径逐项对齐，
  给出"归一化分母到底是 1 核还是 N 核"的**唯一结论**，并说明与 `resource_gate.h:102` 注释自述的
  "100%=全部 effective available workers 用满"是否互相矛盾。若确认口径不一致 → 宪章 §10.5/§18.2 的门在数学上无效，P0。
- **零调用点（L11-004）**：`ci/checks.json` 全文无 `--gate-required/--gate-workers`，`ci/run.py:70-77` 自述
  "CI 注册表当前零 --gate-required"，`ci/tests` 还把该事实反向断言固化；而 `docs/owner/RELEASE_STATUS.md:109-110` 与
  `docs/architecture/MODULE_MAP.md:41` 给 `IMPLEMENTED` 结论。→ 声明与证据割裂 + 测试把缺陷当规格固化（`F_TEST_GAP`），P0。
- **§10.5 唯一合规实现无人调用**（L11-004 前半）+ **生产门用 0.80 而冻结值是 0.85、连续 10s×60% 判据无实现但注释称在位**
  （L11-002，与前台对 L17-004 的复核完全同源）→ 与 L17-004 **必须并成一条**，不得两处各写一遍。
- **L11-011**：provider 侧 Drizzle/积分内核 float32 累积 + 裸 `1e-6f` 静默归零（违 §5.3）与 L04-001
  （`precision_mode=0` 默认 FP32、`module_adapters.cpp:1960` 恒投 FLOAT32 而元数据按 precision_mode 写 dtype）**同一事实的两个视角**
  → M5a 落 provider/内核侧、M2a 落 drizzle/元数据侧，双方 related 互挂，由前台并为主题"FP64 承诺与实际累加域脱钩"。

### 移交与拆分
- **M5 拆两轮**：M5a = L11（运行时/线程/CPU/ACR，21 条）；M5b = L12 + L17（CLI·ABI·构建面 + 治理门禁，46 条）。
  理由：两者都需逐条重验，合计 67 条超单代理可靠深度；L12-006/007（版本）与 L17-002（版本）由 M5b 内部并档，
  前台已给出 F00 侧证据（`packaging/schemas/install-tree-contract.schema.json:12` 用 const 钉死 alpha.1）。
- L11-001/008/009/010 等 ACR 休眠与 ISA 变体项与 L12-001（"ACR/第二调度器门验的是假对象"）同属
  **「验证明对象 ≠ 交付对象」**，M5a/M5b 各自落档、`related` 互挂，前台归纳为一条根因。

## 收档 L15（国际标准横向合规·六域逐条，22 条：P0:5/P1:14/P2:3）—— 三条经前台亲验

- **L15-001 HiPS tile 命名与 IVOA 相反（前台已复算确认）**：`aio_hips_writer.cpp:137-138` 实为
  `dir = ipix / 10000; npix = ipix % 10000`，`:140` 格式化 `Norder%d/Dir%llu/Npix%llu`，读侧 `aio_hips_reader.cpp:40-42` 同口径；
  标准 §4.1 为 `D=(N/10000)*10000` + 文件名带**完整** N → `Norder6/Dir10000/Npix10302.fits` 被写成 `Dir1/Npix302.fits`。
  **后果**：N≥10000 的 tile（order≥5 起即存在）落错目录且错命名，**对外不可消费**；注册表偏差列写「与读侧同一合同」
  = 以实现自证标准符合（违 §1.1/§1.2）。定档 `B_STD_MISMATCH` P0，归 M2b（L03 域）；M4 已收令做后果引用。
- **L15-004 Gaia 历元已由 ESA 官方原文钉死**（`https://www.cosmos.esa.int/web/gaia/dr3`：reference epoch … is 2016.0，
  positions and proper motions referred to the ICRS）→ **F00-06 的影响量级不再需要自行估计**，直接引用；
  M2a 只需确认 parallax/pmra/pmdec 恒零（`DATA_SEMANTICS.md:121-123` 已自述「未初始化、调用方不得使用」）即闭环。
- **L15-006/007 FITS CHECKSUM + 章节号错挂**：真源是 `runtime/io/fits_core.c`，**不是** `modules/services/io/src/fits_core.c`
  （后者只存在于 `run/**` 影子树）→ 已下令 M2a 订正 L10 的锚并另立一条"读路径归属写错树"。`, 
  注册表 D.fits 五行锚有四行指向不含该主题的章节（DATASUM/CHECKSUM 实在 §4.4.2 + 附录 J，§6 是 random groups）。
- **L15-018/019 两条门侧事实（与 M5 主题并档）**：`check_standards_registry` 在 `ci/checks.json` **零登记**
  （注册表 `:314-318` 自认未承接；且检查器 docstring 宣称 C1–C8、§5 只列 C1–C7、`evaluate()` 内无 C8 断言）；
  `DOC-LINE-ANCHORS` 虽已接 CI，但只做「可解析 + 区间不越界 + 声明式符号绑定」，故 +93 行漂移、165→230 行错记、
  函数起始错位**全部漏检** → 「行锚系统性失效」主题的**机器侧根因**在此闭环：门在，但门的判据与漂移无关。
- **L15-016 一条纯推导证伪**：文档称「TAN 的 r<π/2 与 denom>0 数学等价」，实为 r=tan z ⇒ z<57.5° 而 denom>0 是 z<90°
  → 属 L19/L20（推导独立复算轴）的典型靶子，已请 M7 与两轴证据并档。

### 负责人追加的两条要求（已转为新轴，勿与 L13/L14 混淆）
- **L27 子库 README 完备性与内容正确性**：每库须有 README 写清功能/用途/相关文档/接口/构建/测试/偏差/状态，
  并与 `module.yaml`、`docs/modules/*.md`、`docs/modules/registry/*.md` 四源交叉对账；同时判定
  `tools/check_module_readmes.py` 的真实覆盖面（L12-022 称只覆盖 5 个目录）与"标准有要求但无门"vs"标准本身缺要求"的类别区分。
- **L28 注释溯源三要素审计**（算法来源 / 输入来源 / 被消费者）：与 L13/L14（注释与代码矛盾、流水堆积）正交，
  查的是**要素齐备性**；并要求先判定 `docs/standards/COMMENT_STANDARD.md` 与宪章 §12.2 是否本来就要求溯源——
  若规范要求"只解释单位/数学原因/前后置条件"而未要求来源与消费者，则**规范缺口本身成一条 finding**。
- **L26 角度单位换算链穷举**（deg/arcsec/mas/rad/px 的 3600 因子）：由 L15-002 与 L01-007 两域同因子命中逼出。

## 现场：真源正在被并发修复（对全层的定稿口径调整）

- 并发 agent 已向 main 提交 `9a3b5a7d`（HISS support 量化 / SIP 桥接 / 资源门真实观测）与 B2-A1..A16 系列，
  工作树脏文件 17 → 8，修复仍在继续。**这些提交修的正本次审计报出的 P0**。
- 已下令 6 个合并代理 + 4 个代码向横向轴：定稿前按当前树重读，四类分列（仍成立 / 已修复 / 部分修复 / 无法判定），
  已修复者不进 findings/，改记 _merge 的「已修复」表。
- **新增一条必须登记的形态：修复未固化**（代码改对了但没有回归测试钉住、或 SCI/ALG/偏差登记表/README 未回写）。
  这类写成 F_TEST_GAP 或 E_TRACE_BREAK，标题点明"已修但无回归保护/文档未回写"——
  它比"从没修过"更隐蔽，因为后来者会以为有门守着。第一波已有两例（L04-005 代码已修而五处文档仍记旧行为、
  L05-004 fail-closed 只落在一条通道），第二波请系统穷举。
- 前台在 SUMMARY 中会把「本轮发现 → 并发修复 → 残余风险」做成三态表，供负责人判断哪些仍需在 RESCUE-V3 之后收尾。

## 收档 L16（追溯链与机器检查有效性，19 条：P0:5/P1:10/P2:4）—— 并**修正前台 F00-01 的根因表述**

### 对我下发三项裁决的答复（采纳，M6 按修订版落档）
- **F00-01 双头矩阵：成立，但根因不在 SPEC 文本**。SPEC 明确 JSON 为权威；问题在**活动路由层**——
  `docs/README-DOCS.md:13` 称旧表"唯一矩阵"、`docs/standards/API_STANDARD.md:14-16` 把旧表当 API-ID 登记处
  （L16 实测旧表**无任何 API-\* 行**、所举三个 ID 零命中）、9 份 `docs/science` 尾注把旧表当新增映射入口。
  **加重事实（L16 新证）**：两表可交比的 15 个 SCI/ALG ID，其 TEST 证据 **100% 不一致**
  （例 旧表 `SCI-CAL-001→TEST-CAL-001` 指向真测试；矩阵 `→TEST-CAL-DESIGN-001` 指向 ALG 文档），
  且**无任何一道门同时读这两份表**。→ F00-01 定稿措辞改为"路由层指向非权威表 + 两表证据互斥 + 无门交叉校验"。
- **F00-04 photometry 行：判定为"未经证据支撑的已验证声明"**，且同类共 **10 行**（模块自述未入根 CMake /
  `entrypoint=MISSING` / dll 目标未建，而八层整行 VERIFIED，SRC 锚指向头声明而非定义）。
  → 从"个案"升级为"面"，M5b/M6 定档时按 10 行计。
- **fail-open 根因：判为"两者并存"**（不是我给的二选一）——SPEC 指定的
  `tools/traceability/check_traceability_matrix.py` **确实存在且 fail-closed**（ERROR→exit 1、异常→exit 3、
  TOOLING_FAILURE 通道齐备），**但 CI 命令不带 `--strict`** → EVIDENCE/引用越界只 WARN；同时 CI 挂了**三份**追溯门，
  其中 `TRACEABILITY`（waivable:false）无条件 `return 0` 结构性不可红（前台已亲验），
  `TRACEABILITY-CODE` 默认读 `artifacts/prerelease_v5/tables/TRACEABILITY.csv` **冻结快照**（与 L17-018 同源）。
  → 这是本轮"门在、但门看的不是真源"的最完整一条证据链，M6 与 M5b 会签定稿。

### 另两条高价值结构性事实（M6 主落）
- **L16-001**：八层矩阵 TEST 层 18/22 行的"VERIFIED"锚在**文档**而非测试；检查器 C7 只验"文件存在 + 符号 token 可见"、
  C8 **显式跳过 SRC/TEST**；矩阵 notes 自述"可执行 TEST-* 落地后更新本行"却已标 VERIFIED。
- **L16-002**：EVIDENCE 层 27/30 MISSING，仅存 3 行 VERIFIED 又指向不存在的 `returns/`（glob 0 文件、
  `EVID-\*` 在 evidence/reports 零命中）。→ 宪章 §12.3-12「L0 结论可追溯到当前 SHA 的机器证据」在**证据层整体为空**。
- L16-012：矩阵 152 个非占位 ID 中 **85 个不在 `docs/contracts/INDEX.yaml`**，其中 **33 个处于 VERIFIED 合同层**；
  descriptor 与矩阵对同一模块用两套 ID → 与 F00-01/L12-018 并档。
- L16-015：`§13.1 每模块必备（含独立 Oracle）`在追溯面**无字段、无门**，测试门的判据是"伞文件豁免 + 唯一 TST 数 ≥5"
  计数阈值 → 与 M7/M8 的"Oracle 独立性"主题在**治理面**闭环。

### 前台状态
- 15/18 纵向 + 0/10 横向（L19-L28 在跑）；累计 316 条；已推 8 次提交，第 9 次含本档案与本次协调记录。
- 仍缺：L03（HiPS/HEALPix/AIO）、L06（星点/PSF）、L14（注释广度 Phase2/3）→ M2b/M3b/M6 的输入。

## 收档 L25（依赖与 vendored 合规，12：P0:2/P1:5/P2:5）、L03（HiPS/HEALPix/AIO，19：P0:4/P1:11/P2:4）

### 前台亲自复验为真（两条，可直接引用「复核时点成立」）
1. **HiPS tile 命名与 IVOA 相反**（L15-001）：writer `aio_hips_writer.cpp:137-140` = `dir=ipix/10000; npix=ipix%10000` +
   `Norder%d/Dir%llu/Npix%llu`；reader `:40-42` 同口径。标准 `D=(N/10000)*10000`、文件名带完整 N → N≥10000（order≥5）
   的 tile 对外不可消费。偏差列「与读侧同一合同」= 实现自证。已下发 M2b 定稿、M4 做后果引用。
2. **GSL(GPL-3) PUBLIC 链进生产目标而三处登记为零**（L25-002）：根 `CMakeLists.txt:546`
   `target_link_libraries(astrocs_p1_sdet PUBLIC gsl gslcblas m)`；`DEPENDENCIES.md` 与 `packaging/dependency-lock.json`
   grep `gsl` **0 命中**。**ipv(Siril=GPL-3.0-or-later) 13 TU 编入生产 STATIC `astrocs_p1_ipv`**（`:503-515`），
   而 `lib/plate_solve/LICENSE` 写「MIT / Copyright (c) 2026 fujiaze」、ipv 全树零 SPDX/版权头。
   → 宪章**没有**独立的第三方许可条款（L25 已明示判据来自 §16.2/§17.11/§14.2/§12.3-1/§19 组合），
     **SUMMARY 表述必须注明这一点**，避免"引用不存在的条款"反噬；法律定性留负责人裁决，登记面冲突足以定 P0。

### L03 的四条 P0 与两条独立收敛（M2b 主落）
- nside 零校验（写入口只比大小 + ilog2 向下取整；**lib/hips 已实现正确校验却在生产调用点被绕过** ← 本条最有价值的部分）；
- 生产写出未走 staging/原子提交，而调用点注释称"AIO-002 原子发布原语内建"（DISP-HIPS-004 登记的是能力缺口，注释写成已完成）；
- Moc.fits 缺 ORDERING=NUNIQ/COORDSYS=C 强制键，注册表仍判 CONFORMANT，合规证据是"自家 reader 能读"的循环论证；
- tile 几何合同三源互斥（代码 2^(k+9) / HIPS_WRITER.md 2^k / IO_002 要求 2^(K+9)）+ 入参域四层互斥 → 自产 tile 不被自家输入合同接受。
- 收敛 1：order≤29 口径（L03-005 与 L15-012 独立同证，含 npix 在 2^31 的 uint64 回绕、在册测试反而断言 order 31 合法）。
- 收敛 2：healpix「百万点 astropy oracle / ≤1e-12 deg」（L03-006 与 L15-013 同证：脚本与 oracle.jsonl 不存在、未注册 ctest、
  实测容差 1.2×像素且跳过极点）→ **「独立 Oracle 缺位」主题的第 5 个跨域实例**。

### 叶子主动提出的降级（采纳，M1a/M6 执行）
- L25 §6.5：`orchestrator.exe`、`nul`、`cfitsio/*.o` 等"构建残留混入真源"经 `.gitignore`
  （`lib/orchestrator/cpp/.gitignore:1-5`、根 `.gitignore:25-28`）判定**大概率未入库** →
  L01-020 表述须收敛：不得把本地未跟踪产物算作仓库违规。这是横向轴**反证第一波**的成功案例，按规程写入 §6 并已采纳。

## 收档 L21（跨文档物理量口径矩阵，14 条：P0:1/P1:9/P2:4 + 扩展实例 8 + 待复核 6）

### 前台已裁决的一条：L21-009 属**过度表述，必须降级**（M7 定稿前执行）
- L21 称「hips_frame 在 IO-002 必 equatorial 与 DATA-002/Phase3 校验器必 icrs 之间双向锁死 → 产品互操作被锁」。
- 前台实测：**校验器两个值都收** —— `lib/.../hips_properties.cpp:126`
  `return fail("hips_frame must be equatorial|icrs (got " + out->frame + ")")`，头 `:16` 注明「必需, equatorial|icrs」；
  且 `DATA_SEMANTICS.md:816`（值域 ∈{equatorial,icrs} → rc=1）与 `:819`（**跨帧混用 → rc=1 "hips_frame mismatch: %s vs %s"，
  标注 B2-A8**）说明混合产品早已被显式拒绝，不是静默锁死。
- → 定性改为：**writer 恒写 equatorial（`HIPS_WRITER.md:174`）与两处合同/交换语义的措辞不一致**，
  属 `C_DOC_CODE_GAP`/`I_DOC_HYGIENE` 的 P1/P2（合同措辞互斥、但机器行为一致且有拒绝），
  **不得写成"产品互操作被锁死"**。L21 §6 自己列的待复核项「hips_frame 运行时阻断」由本条证据**判定为不存在**。
- 同法自查：L21 其余"锁死/不可消费"级别的表述，凡涉及运行时行为的，M7 必须按当前树找**实际校验点**后再定强弱。

### L21 的真正贡献（采纳为高价值，无需前台复验其文本事实）



## R 层建立


## 前台自纠：F00-07b 原锚点不成立（由 L24 的 R-1 反证）
















## F00 全量自审（机械复验我自己写的每一条 path::符号）
- 抽出 4 条唯一 `文件::符号` 引用逐条回读真源：**通过 2 条、不通过 2 条**。
- tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword（F00 第 165 行引）→ **SYMBOL_ABSENT**
- tests/abi/test_io_ownership_contract.py::test_main_enforced（F00 第 265 行引）→ **FILE_NOT_FOUND / undefined**
- 结论：F00-02 与 F00-07a 两类错误已全量扫净；后续 F00 新增条目标「已复验」时必须同时留当场命令与输出片段。
