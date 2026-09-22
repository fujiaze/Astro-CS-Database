# RELEASE-05 交接上下文（供新对话直接使用）

> 生成时间：本会话结束前。仓库：`/workspace/Astro CS Database`，分支 `main`，
> HEAD = `ad46016f`。**注意：`origin/main` = `7bb30701`，本地领先若干提交，尚未 push。**
> 未跟踪目录 `.dsh-code-index/`（工具缓存，非交付物）。

---

## 0. 给新对话的开场指令（可直接粘贴）

```
仓库 /workspace/Astro CS Database，分支 main。先读 AGENTS.md（尤其 §1 权威链、§6 硬禁令、
§9 自查自修、§10 停止条件、§11 不许因执行预算停下），再读 ENGINEERING_SPEC.md §7 与
工程控制/RELEASE-05/{ACCEPTANCE.md,OPEN_QUESTIONS.md,SUMMARY.md}。
继续 RELEASE-05 收口，当前正在做「第一阶段帧级并行接线」（PERF-P1）。所有面向用户的说明用中文。
```

---

## 1. 项目与权威链（不要跳读）

AstroCS = 天文 CCD/CMOS 校准数据库；三个**独立**命令 `normalize`/`mosaic`/`export`，
阶段间只经磁盘产品 + manifest + 哈希交换。三个科学创新点：测光星等坐标系(SCI-A)、
跨帧绝对 SNR(SCI-B)、加性天光无接缝(SCI-C)。

权威顺序（AGENTS §1.1）：`ASTROCS_DESIGN.md` → `docs/design/` → `docs/plugins/` →
`docs/science/`+`docs/algorithms/`（只读权威）→ `docs/contracts/` → `ENGINEERING_SPEC.md` →
`ACCEPTANCE_SPEC.md` → `CONTROL_PACK_SPEC.md` → `docs/ci/`。
**权威链胜过控制包任务文字。**

硬禁令要点：不动科学公式/默认容差；不串三阶段；**不硬编码线程/ISA/block**（由 `benchmark`
生成的 profile 决定）；只 main 原子提交；不 amend/force-push；不用 waiver 盖红灯；
不用 facade/空骨架冒充实现；不宣布发布。

---

## 2. 构建 / 测试 / 环境（实测可用）

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build                                   # 增量约 30 s
ctest --test-dir build -j8 --output-on-failure   # 全量 482 项
python3 eng/ci/run_checks.py --all --profile fast          # 120 项
python3 eng/ci/run_checks.py --check <ID>                  # 单项
# deep 树（另一个构建树，含额外检查）
python3 eng/tools/quality/deep_ci_driver.py build-gcc-release --build-dir run/ci/build-gcc-release
python3 eng/tools/quality/deep_ci_driver.py ctest-full --build-dir run/ci/build-gcc-release \
        --output run/ci/build-gcc-release/ctest-full.junit.xml
```

**实测环境**：16 核 / 23 GB 内存 / 磁盘 `/dev/vdb1` 503G（89% 已用）。
python3 + numpy/astropy/PIL 可用；**无 `unzip`**（用 `python3 zipfile`）。
沙箱 `danger-full-access`，审批提示已禁用（**不要**设 `sandbox_permissions`）。

**已知坑（都踩过）**：
- `ctest` 默认**串行**；`-j16` 会让时序敏感的 `cpu006_bench_report` 假红（单跑 PASS）。
  全量门建议 `-j8`。deep 驱动设计为串行，全量约 2 h+。
- `deep_ci_driver.py` 子命令是 `build-gcc-release`（不是 `build`）。
- `KNOWN-FAILURES-BASELINE-CHECK` 消费 `run/ci/build-gcc-release/ctest-full.junit.xml`；
  该工件陈旧时会 fail-closed 判红（行为正确），需重跑 ctest-full 刷新。

---

## 3. 本会话已完成（按提交倒序）

| 提交 | 内容 |
|---|---|
| `ad46016f` | **PERF-P1：第一阶段帧级并行接线（calibrate/cosmetic）+ 1/N 逐位等价门** |
| `8ef6ec62` | REPORT-501：审核包索引 `artifacts/evidence/release-05/AUDIT_PACKAGE.md` + `SUMMARY.md` 收口报告（结论页四问） |
| `0745f662` | known-failures 基线随提交刷新 |
| `dd9c2404` | ACCEPT-501 独立复核闭环：修 export_stream 数据竞争 + 补登记册 + 门禁补盲区 |
| `60689b99` | CLEAN-403/aio 收口：调度器新代码 I/O 全经 aio + 门禁登记补齐 |
| `02bf4704` | E2E-501：三命令真实数据全链贯通 + 预检矩阵 + manifest 链独立复算 |
| `557ddd68` | ARCH-505 部分落地 + 上呈阻塞（块流规格/执行器/一致性门） |
| `6811803b` | ARCH-504：export 子块流式调度器 |

**已通过的机器门**：ctest 481/482（唯一红是上述 `-j16` 假红，单跑 PASS）；
`--profile fast` 120/120；registry 91 项 0 error；doc_sync 91==91；零 waiver。

---

## 4. 本会话的关键实测结论（**新对话必须知道，否则会重复劳动**）

### 4.1 三个阶段的实际并行状况（实测，不是推测）

| 阶段 | 生产路径并行现状 | 证据 |
|---|---|---|
| **normalize (P1)** | **原本完全帧串行**（本会话开始接线） | 32 帧耗时 4782 s；星点检测/drizzle 各恰 32 次、严格一帧接一帧 |
| **mosaic (P2)** | **已经是帧级/tile 级并行** | `p2_parallel_for` 三处调用：`upm_apply`(帧级)、`reject`(tile 级)、`integrate`(tile 级)；`__workers` 由 Runtime lease 注入 |
| **export (P3)** | **resample 已是行带并行** | `nw=min(cap,h)` 行带提交 `rt::shared_work_executor`；writer 串行但只占很小 |

**根因**：`module_adapters.cpp` 的 **Phase2 dispatch 注入了 `cfg2["__workers"]=cap`，Phase1 dispatch 从未注入**，
且 P1 没有任何帧级并行执行器（`p1_parallel_for` 计数为 0）。**真正的并行缺口只在第一阶段。**

### 4.2 ARCH-502/503/504「三个调度器」的真实性质（重要，避免误接线）

- 三者在生产路径**零引用**，且**其内部工作是合成的、不是真实科学计算**：
  - `mosaic_window.cpp` 的 `run_window` **不读任何真实像素**，调的是合成式
    `integrate_pixel`（`snr=10+fmod(tile_bytes,97)+…`、`value=100+fmod(tile_bytes+frame_id,53)`）。
    **把它接进生产 = 伪造科学结果，绝对不能接。**
  - `export_stream.cpp` 产出「文本头 + 裸 f64」单文件，与生产要求的 **cfitsio 4-HDU BITPIX=-32 FITS**
    不兼容；`memory_limit_bytes` 声明了但实现里零引用。
  - `normalize_workflow.cpp` 同为调度骨架（帧内 DAG + 预取），逐帧工作由回调注入。
- **结论**：它们是**调度骨架 + 自证**，不是可替换生产的实现。正确做法是**复用既有
  `p2_parallel_for` 规范**把并行度接进真实节点，而不是接线这些骨架。
  （此事已上呈 OQ-9，见 §6。）

### 4.3 逐节点耗时（T4，3 帧，348 s；**注意该次采样与 VIS 32 帧 normalize 并发，有污染**）

`drizzle 273 s (78%)` > `noise-snr 19 s` > `wcs 15.7 s` > `star-psf 15.3 s` >
`photometry 14 s` > `calibration 11 s` > `writer 0.22 s` > `cosmetic ~0`。
干净口径下（VIS 32 帧 / 4782 s）drizzle 占 1414 s ≈ **30%**。
开启 `ASTROCS_NODE_TRACE=1` 可复测；注意 `[nodetrace] END <node> <elapsed_seconds>` 的数值是**耗时**，
BEGIN 的数值是**起始时刻**（别把两者相减）。

---

## 5. 当前进行中的任务：PERF-P1 第一阶段帧级并行接线

### 5.1 已完成的接线（`ad46016f`）

- 新增 `p1_parallel_for(workers, n, body)` + `p1_workers(doc)`：与 `p2_parallel_for` **同规范** ——
  原子计数动态认领、每任务只写自己下标结果槽与**自己帧**的产物路径、跨任务零浮点归约、
  异常经 `exception_ptr` 回传后 join 后重抛。
- P1 dispatch 增补 `doc["__workers"] = cap`。
- **calibration** 与 **cosmetic** 两节点已改为帧级并行 + join 后帧序归约。
  其中**修掉一处真实缺陷**：calibration 原用循环内共享的 `W`/`H` 推进尺寸基准
  （`W = light.w(); H = light.h();`）——并行下即数据竞争；改为「母版尺寸（若有）或首帧尺寸」
  为基准逐帧独立校验。
- 新增 1/N 逐位等价门 `eng/tests/unit/p1001_real_nodes_test.cpp::
  test_perf_p1_frame_parallel_bitwise_1_vs_n`（budget=1 vs 16 全链、产物集合相同 + 逐字节相同 +
  1 bit 篡改必须被检出）。实测 **rc=0、0 失败**。

### 5.2 剩余待接线节点（按收益排序，方法完全相同）

**方法（照抄 calibrate/cosmetic 的写法即可）**：
1. 在节点函数内取 `const uint32_t W = p1_workers(doc);` 与帧数；
2. 声明 per-frame 结果槽：`std::vector<Result<void>> f_err(n, success)`、产物路径槽、
   以及**每一个累加器**的 per-frame 槽（含 `std::vector<Json>` 行）；
3. 把循环体搬进 `p1_parallel_for(W, n, [&](uint64_t fi, uint32_t w){ ... })`：
   循环变量 `l`/`lp` 换成 `doc["input_lights"][fi]`，所有 `return Result<void>::fail(X)`
   换成 `f_err[fi] = Result<void>::fail(X); return;`，
   所有 `(*man)["error_kind"]`/`st["status"]` 写操作**移出**并行体（用 `f_errkind[fi]` 标记，
   在归约里按序写）；
4. join 后**按下标升序**归约累加器与产物列表，遇到首个失败即设 status 并 `return f_err[fi]`；
5. **任何在循环外声明、循环内被修改的变量都是数据竞争**，必须改成 per-frame 槽
   （calibration 的 `W`/`H` 就是这样一处）。

| 节点 | 函数起始行（会随改动漂移，用 grep 定位） | 帧循环规模 | 备注 |
|---|---|---|---|
| **drizzle** | `p1_op_drizzle` | 4755–5235（**480 行，20+ 处错误返回**） | **收益最大（30–78%）**；每帧写自己的 HiPS 产物目录，帧间零共享；改造量最大 |
| **star-psf** | `p1_op_star_psf_impl` | 2271–2406（135 行） | 持有 `const StarDetector det(5.0)`（循环外）→ **必须按 worker 分实例**：`std::vector<std::unique_ptr<StarDetector>> dets(workers)`，体内 `if(!dets[w]) dets[w]=std::make_unique<...>(5.0);`。累加器：`frames`(Json)、`fwhm_xs/fwhm_ys/ells`(vector<double>)、`n_valid_total/n_total_total/n_fit_total` |
| **noise-snr** | `p1_op_noise` | ~4094/4159/4202 三处 | 逐帧归约 + 逐帧 JSON 产物 |
| **photometry** | `p1_op_photometry` | ~3148 起 | 含**跨帧**测光标度解算（`p1_photscale.json`）→ 跨帧部分**不能**并行，只能并行逐帧部分；需先读清边界 |
| **wcs** | `p1_op_wcs` | ~2953 起 | 持有 `ipv`/`gaia`/`sdet` 句柄（循环外）+ `cleanup()` lambda → **必须按 worker 分实例**；IPV 求解器内部状态未知，风险最高 |
| **writer** | `p1_op_writer` | ~5179 起 | 只占 0.22 s，收益可忽略，可最后做 |

**线程安全已核查**：算法入口只有不可变 `static const` API 表；drizzle 的错误槽与计数是
`thread_local`（`g_hips_error`、`g_tl_n_quick`）；帧间无共享可变状态。
**但持有句柄的节点（star-psf / wcs）必须按 worker 分实例。**

### 5.3 验证要求（每个节点接完都要做）

1. `ninja -C build` 全绿；
2. `p1001_real_nodes_test` rc=0（含 1/N 逐位等价门）；把新节点也纳入该门的链；
3. `ctest --test-dir build -j8` 无新增红；
4. 真实数据实测加速比：`ASTROCS_NODE_TRACE=1 ./build/astrocs normalize --json <cfg> -y`，
   对比接线前后同一数据集的墙钟与逐节点耗时（**别与其他重任务并发跑**，否则采样污染）；
5. 记录到 `run/RELEASE-05/logs/`。

---

## 6. 阻塞项与待负责人裁决（不要自行绕过）

- **OQ-9（BLOCKER）**：块流结构合同（`eng/contracts/block_flow/stage_block_flow.json`）声明的
  端口与代码真实数据流不一致 —— 已登记 **18 条偏差、13 条 blocker**
  （`eng/contracts/block_flow/conformance_deviations.json`）。按声明图迁移生产节点只会得到
  facade，故 ARCH-505 的「生产节点改写 + Session/orchestrator 退役」**保留并登记阻塞**。
  agent 推荐方案 1（改注册表对齐代码）。**在负责人裁决前，不要硬退役 Session 与文件约定。**
- **OQ-10（BLOCKER）**：M42 真实数据 Gaia 解算失败。根因已定位：`Step 3 选星` 按 box 积分
  mag 升序取前 60 颗**全是饱和星** ⇒ 密度估计被污染（`rho_img=49.46` vs `rho_target=74.18`）
  ⇒ `Step 5` 首个 Gaia 查询 `N_returned=0, m_lim=20.435` 即放弃。
  **不是** parity/scale 拒绝（`module_adapters.cpp:3006` 的错误串归因有误），**不是**数据缺失。
  且 `module_adapters.cpp:2946` **刻意把 `ip.log_dir` 置空**（节点面禁写求解日志），使失败不可诊断。
  候选修复方向已列在 `OPEN_QUESTIONS.md`，**未擅自改科学算法**。
- 其余：Q-1 发布/成品帧判定、Q-2 目标态排期、Q-3 RELEASE-04 出库、Q-5 测光 n≥100 未落地
  （`PHOTOMETRY.md` §16.5 定案 n≥100，生产仍 `kMinFitStars=3`）、Q-8 Windows 腿退出码 9。

---

## 7. 尚未完成的任务与遗留缺口

- **ARCH-505**：阻塞于 OQ-9。
- **CLEAN-501**：依赖 ARCH-505（独立部分已做：aio 收口、门禁登记）。
- **PERF-501**：依赖 ARCH-505/CLEAN-501；端到端优化前后对照与计时热点表**未做**。
- **BLD-501**：Windows MSVC/CI 腿未跑（本环境无 Windows，应登记平台限制）；单入口产品核对、
  版本串未做。
- **VIS-501**：银心 T4 组 32 帧 normalize 已完成（rc=0，3 块 3 产品，`run/RELEASE-05/vis/`），
  mosaic/export 链未走完；**M42 组阻塞于 OQ-10**；负责人目检未提交。
- **REPORT-501**：`SUMMARY.md` 已填；审核包索引已建。
- **D-17 遗留**（未修，已在 `ACCEPTANCE.md` 登记）：`eng/tools/check_ast_api.py:49` 用
  `<repo>/include`；linux-main 档步骤超时预算偏小；2 条 UT-BACKEND 持续红；
  `eng/tests/validation/release02/c_delta_composition/seam_fixed.py:44` 语法错误；
  `sigma_sky_source` fail-closed 未实现（`snr_science.cpp:177-182` vs `07_noise_snr.md` §4.2a）；
  PHOTOMETRY §16.5 n≥100 未落地。
- **门禁加固未做**：`check_block_flow_conformance.py` 只校验 `registry_ref` token 存在，
  不校验其指向目标真实存在。
- **push 未做**：本地 main 领先 `origin/main`。

---

## 8. 纪律提醒（最容易踩的几条）

1. **中文回复**（负责人明确要求）。
2. **不许因执行预算停下**（AGENTS §11）：任务没做完的默认动作是继续做，不是写「未完成」交差；
   需要汇报时**先继续推进**，在自然停点给状态。
3. **重型任务要用满资源**（负责人明确要求）：16 核；不要把重任务串行跑，也不要让两个重任务
   互相抢 CPU 导致采样污染。
4. **子代理零 git 写权限**；只 main 原子提交，不 amend/force-push。
5. **日志落 `run/RELEASE-05/logs/`**；运行产物落 `output_dir` 或 `run/`，不入库。
6. 发现「看起来通过」要怀疑：空断言、SKIP 充数、证据为空、检查器静默退化都算未完成。
7. 真实数据的坑（已踩过，配置合同是 fail-closed 的）：
   - master 为归一化域（PixInsight）时必须给 `master_units.{bias,dark,flat}="normalized"`
     + `master_scale.*=65535`，否则 `MASTER_UNIT_MISMATCH`；
   - 有 `master_dark` 时必须给 `dark_optimization` + `dark_scale_factor`；
   - `wcs.gaia_data_dir` 必须指向 **`gaia/GaiaDR3`**（`.xpsd` 索引所在层）；
   - mosaic 的 `hips_paths` 必须是**逐帧** HiPS 产品树（取自 `p1_products.json` 的
     `frames[].hips_path`）；
   - export 的 `center` 必须落在 mosaic 覆盖内，否则 `coverage_ok=1` 但 `covered_px=0`；
   - 每个 `blocks[]` 要有**自己的** `output_dir`。

---

## 9. 现成可用的工具与配置

- `eng/tools/e2e/make_e2e_configs.py`、`run_e2e_chain.py`（E2E 全链 + 独立复算）、
  `make_vis_configs.py`、`render_vis.py`（L4 渲染 + V1–V5 fail-closed 判据）。
- `eng/tools/quality/`：`deep_ci_driver.py`、`known_failures_baseline.py`、
  `check_block_flow_spec.py`、`check_block_flow_conformance.py`、`check_scheduler_probe_schema.py`、
  `run_preflight_matrix.py`。
- `eng/tools/arch/check_thread_budget.py`：**新增线程池必须登记**，否则门禁红。
- 真实数据：`testdata/`（BASS_DR3、HST_M16 等）、`gaia/GaiaDR3`（只读，不入库）。
- VIS 配置与产物：`run/RELEASE-05/vis/`（银心 T4 组 32 帧）。
