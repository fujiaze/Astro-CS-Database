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