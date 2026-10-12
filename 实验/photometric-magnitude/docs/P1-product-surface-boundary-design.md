# 阶段一产品面与工作区的边界设计（D1–D4）

本文件是**设计回报**，不含实施。依据：负责人「标准模式下真正的产物只有逐帧 HiPS 和一个给阶段二输入用的 JSON」「开关只决定中间产物是否额外打印，不改变执行路径」；`AGENTS §7`（不写未声明文件、经内存管线）、`§13`（不设门禁、同一件事一份实现）；`docs/engineering/architecture/DEV_MODE.md`（本步已订正）。

清点数据来自 `P1-product-surface-inventory.md`（T1 消费者表、§12 结构障碍）。

---

## D1 产品面与工作区的边界

### D1.1 逐项：现在写到哪 / 新写去哪 / 谁读它

| 对象 | 现在位置 | 新位置 | 读它的（T1 实测） | 能否移出产品面 |
|---|---|---|---|---|
| 逐帧 HiPS 产品树（`signal/`、`support/`、`variance/`、`ivar/`、`snr/`） | `<output_dir>/<frame_key>/` | **不动**（产品） | 阶段二 AIO（`sampler.cpp:543/552/565/831`）、`p2_frame_id:459` | — 是产品 |
| `<frame_key>/manifest.json` | 同上 | **不动**（产品集完成标记） | `aio_hips_reader.cpp:92`（`aio_hips_open` 完成态断言） | — 是契约 |
| `<frame_key>/p1_stack.json` | 同上 | **不动**（跨进程契约） | **阶段二** `module_adapters.cpp:10344`（`p2_op_upm_fit` 取 `finest_input_arcsec`）、`:13214` | **不可移出**（见 D1.3） |
| 阶段二输入 JSON（现无代码消费者；计划由 `p1_products.json` 接线） | `<output_dir>/p1_products.json` | **不动**（产品） | 目标：阶段二帧列表（D4） | — 是产品 |
| `p1_wcs.json` | `<output_dir>/<frame_key>/` | **工作区** | 阶段一内部 `:3862`/`:6017`/`:8175`；实验/eng 38 脚本 | 可（阶段二不读） |
| `p1_final.json` | 同上 | **工作区** | lib 内零读点；eng 2 脚本 | 可 |
| `p1_sources.json` / `p1_phot.json` / `p1_snr.json` / `p1_flux.json` / `p1_psf.json` | `<output_dir>/` | **工作区** | 阶段一内部读回（见 §3 R4–R7）；实验/eng 多处 | 可 |
| `calibrated_` / `cleaned_` / `photoapplied_` / `badcol_` / `badcolsrc_` | `<output_dir>/` | **工作区** | `p1_calibrated_path`（12 处）等；实验 22/49/3/6/0 脚本 | 可 |
| 运行级 manifest、资源读数（`alloc_*`、`resource_*`、`worker_balance.csv`）、调度图（`graph/**`） | `<output_dir>/` | **不动**（运行记账） | 验收/审计/性能档案 | 不属本设计（另议） |

### D1.2 工作区的位置与清理语义

**选定：`<output_dir>` 的**同级兄弟目录**，默认名 `<output_dir 基名>.p1-work/`，路径可由配置键覆盖（暂定 `work_dir`）。**

理由（逐条）：

1. **必须在产品树之外**（您的裁决 2）：同级兄弟目录不在 `<output_dir>` 内，`<output_dir>` 里只剩「逐帧 HiPS 产品树 + 一个 JSON」。
2. **必须与 `<output_dir>` 同一文件系统**：现状落盘走「同目录临时文件 → `rename` 原子发布」（`module_adapters.cpp:1497` `p1_atomic_publish` → `aio_fs::rename_replace`）。`rename` **不跨文件系统**。工作区与产品目录同级 ⇒ 同文件系统 ⇒ 原子语义保持。若改用系统临时区（`/tmp` 常为 tmpfs），不仅要处理跨 FS 的 rename 失败，**还会把 ≈64 MiB/帧的平面件放进内存**，直接恶化峰值 RSS（与后置性能目标冲突）。
3. **一次运行专属**：基名取自 `output_dir`，同一 `output_dir` 的重跑覆盖同一工作区，不跨运行串味。
4. **可配置**：`work_dir` 键缺省取上述规则；显式给出时按用户路径（用户自担同 FS 约束，缺失时回退默认并报一条 warn——与 `storage_form` 缺省取默认的动作口径一致）。

**清理语义**：开关只决定工作区**是否保留**——保留（默认）供逐节点复核与复算，或在运行收尾时整体删除。删除不得发生在运行中途（热路径读回仍在用）。

### D1.3 不可移出清单

| 对象 | 为何不可移出 |
|---|---|
| `<frame_key>/p1_stack.json` | **跨进程契约**：阶段二在另一个 `acsd mosaic` 进程里按 `<hips_paths[i]>/p1_stack.json` 读 `finest_input_arcsec`（`module_adapters.cpp:10344`），并按注册表 `:1071` 归入「跨阶段唯一载体 = HiPS 产品树」。它**不是调试输出**，与本步的开关无关。 |
| `<frame_key>/manifest.json` | `aio_hips_open` 的完成态断言（`aio_hips_reader.cpp:92`：缺失/空/无 products 声明 ⇒ fail-closed）。它是 HiPS 产品树的组成部分。 |
| 逐帧 HiPS 子产品 | 产品本身。 |
| 运行级 manifest / 资源读数 / 调度图 | 运行记账与验收面，不属「开发期额外输出」。是否一并收敛**另议**（我在清点里已登记为 K6）。 |

### D1.4 标准模式运行结束后的产品树清单

**目标（本设计落地后）**：

```text
<output_dir>/
├── <frame_key>/                    ← 逐帧 HiPS 产品树（产品）
│   ├── manifest.json               ← 完成标记（契约）
│   ├── p1_stack.json               ← 跨进程契约（阶段二读 finest_input_arcsec）
│   └── signal/ support/ variance/ ivar/ [snr/]
├── p1_products.json                ← 阶段二输入 JSON（产品）
└── （运行记账：manifest / 资源读数 / 调度图 —— 是否留下另议）

<output_dir 基名>.p1-work/          ← 工作区（产品面之外；默认保留，可关）
├── calibrated_*.fts  cleaned_*.fts  photoapplied_*.fts  badcol_*  badcolsrc_*
├── p1_sources.json  p1_phot.json  p1_snr.json  p1_flux.json  p1_psf.json
└── <frame_key>/p1_wcs.json  <frame_key>/p1_final.json
```

**实测基准（改造前，4 帧真实 M42 运行）**：`<output_dir>` 内除上列产品外，另有 `calibrated_*`(×4)、`badcol_*`(×4)、`badcolsrc_*`(×4)、`badcol_report.json`、`p1_sources/p1_phot/p1_snr/p1_flux/p1_psf.json`，以及 `<frame_key>/{p1_wcs,p1_final}.json` —— 即工作区要接走的那一批。

### D1.5 产品树边界风险与规避（D1 子问题）

**问**：逐帧 JSON 现与 HiPS 树**同一层**；移走后会不会让「逐帧 HiPS 产品树」的边界变模糊，例如阶段二扫该目录时看到多余文件？

**答：风险评估为低，且移动是净改善；但需按下列口径约束。**

1. **阶段二不枚举该目录**（实测）：它按**显式路径**取数——`aio_hips_open(hips_path, AIO_HIPS_RD_{SIGNAL,SUPPORT,SNR,IVAR})`（`sampler.cpp:126/467/543/565/831`）与 `<hips>/p1_stack.json`（`module_adapters.cpp:10344`）。
2. **唯一的目录枚举点作用域更窄**：`lib/algorithms/coverage/hips_properties.cpp:151` 只枚举 `<product_dir>/Norder<order>/Dir*/Npix*.fits`，且 `if (kind != 1) return 0;` **只下钻目录**、文件名须以 `Npix` 开头且以 `.fits` 结尾 ⇒ 树根上的 JSON **不可能**被它看到。
3. **移动是净改善**：`p1_wcs.json`/`p1_final.json` 现在就躺在 `<frame_key>/`（产品树目录）里，属**产品树内的非 HiPS 成员**；把它们移入工作区后，产品树内只剩 HiPS 成员 + 两个已声明的契约文件。
4. **残留风险**：`aio_hips_open` 的完成态断言是**存在性**判定（`manifest.json` 在/非空/有 products 声明），**不是精确成员判定** ⇒ 多余文件既不会破坏它，也不会被它检出。因此边界靠**约定**而非靠校验。
5. **规避方式**：
   - 产品树内**只允许**三类成员：HiPS 定义成员（`properties`、`Moc.fits`、`Norder*/Dir*/Npix*.fits`、子产品目录）、`manifest.json`、`p1_stack.json`；其余一律工作区。
   - 若将来有代码需按目录列举产品树，**必须按声明名过滤**，不得按「目录里有什么」推断产品集；该约束写进 `DEV_MODE.md`「标准模式下的产物面」。
   - 不采用「在产品树内放隐藏文件/隐藏子目录」的做法——那样只是把中间件换个位置留在产品树里，不满足负责人「产品树内只有两样」的要求。

---

## D2 开关语义与消费者更新

### D2.1 唯一开关口径

- 开关**只**决定中间产物**是否保留**：**保留**（写到工作区，供逐节点复核与复算）vs **运行收尾即删**（工作区整体清理）。
- 开关**不改变**执行路径、不改变任何数值/判据/阈值、不改变节点顺序、不改变产品面内容（产品面在任何取值下都只有两项）。
- 中间产物**总是先写到工作区**（热路径读回仍按现有磁盘机制，本步不动）——开关不参与「写不写」，只参与「留不留」。

### D2.2 默认取值：建议**保留**

关键约束：**10/12 项有活树脚本消费者**，含 `eng/tests/validation/release02/{q1_photometry_gradient,q3_additive_truth,unc_propagation}` 与 `eng/tools/monitoring/concurrency_sweep.py` 这类**契约级**消费者。

- **若默认「删除」**：这些消费者在默认运行后立刻读不到文件 ⇒ 批量失败。届时必须先改全部消费者（`eng/**` 优先、`实验/**` 其次），再把默认翻成删除——这是一次**跨模块行为变更**，代价远超本步。
- **若默认「保留」**：负责人的要求**已经满足**——要求针对的是**产品面**（产品树内只有两项），而工作区在产品面**之外**；同时零消费者破坏，`AGENTS §13.2`（实验单元保留过程与证据）也得以维持。删除成为**打包/发布运行**的显式选择。

⇒ **建议：默认保留；「运行即删」为显式选项**，供发布打包与磁盘受限场景使用。

### D2.3 若负责人选择「默认删除」，消费者更新的最小集与顺序

| 序 | 更新对象 | 改什么 |
|---|---|---|
| 1 | `eng/tests/validation/release02/q1_photometry_gradient/src/star_residual.py`、`q3_additive_truth/src/{q3core,q3lib}.py` | 改读工作区路径，或在该测试运行配置里显式开启「保留」 |
| 2 | `eng/tests/validation/release02/unc_propagation/audit_evidence.py` | 同上（读 `p1_final.json`） |
| 3 | `eng/tools/monitoring/concurrency_sweep.py` | 同上（读 `p1_final.json`，另读 `photoapplied_*`） |
| 4 | `eng/**` 其余读 `p1_phot`/`p1_wcs`/`p1_sources`/`cleaned_*`/`calibrated_*` 的脚本 | 同上 |
| 5 | `实验/**`（`photometric-magnitude`、`P5-signal-extraction`、`absolute-snr`、`additive-sky-seamless`） | 改读工作区；或在其配置里显式开启「保留」 |
| 6 | 默认值翻成「删除」 | 仅在第 1–5 全部通过后执行 |

**顺序不可倒置**：默认值翻转必须在消费者全部改完之后，否则留下批量失败的半坏状态（即我在 T5 登记的 I-4）。

---

## D3 科学量不变的证明方式

**改动范围**：只改**写/读位置**，不改任何数值、判据、阈值、节点顺序、并行轴。

**证明三步**：

1. **差集证明（静态）**：`git diff` 只应出现在路径表达式的构造处（工作区前缀拼接），不得出现在任何算式、常数、容差、比较、节点序列处。逐条列出被改的路径表达式，并声明「无算式改动」。
2. **同配置前后实跑（动态）**：同一份 4 帧真实 M42 配置（跨 3 夜晚），改造前后各跑一次，取**逐帧 `sigma_residual_dex`**（`p1_phot.json#photscale_detail`）与**帧级尺度**（`k_photo`）逐帧对拍，要求**逐位一致**（同帧、同配置、同二进制行为）。
3. **H4 grep（结构）**：证明不存在「文件路径取数」与「工作区取数」以外的第二条路径，且无 `#if defined(ACSD_DEV_INTERMEDIATE_CACHE)` 残留。

---

## D4 阶段二输入 JSON（计划里的 ⑦，仅设计）

目标：`p1_products.json` 成为**唯一**的阶段二输入 JSON（接线修好「声明了却没人读」，不新建第二个）。

- **须汇总的字段面**：`pipeline.md:216` 已规定的 `storage_form`/`index_path`/`index_sha256`/`archive_sha256`/`coverage_index`；注册表 `:944` 要求的**逐帧 `hips_path` 清单**；`DATA_SEMANTICS §4.4` 溯源最小集；`unified_model §2.1` 的 `m_ref`/`F_ref`；`DATA_SEMANTICS §3.7` 的 `frame_id`。这些字段目前散落在 HiPS `properties`、逐帧 `p1_final.json`、`p1_stack.json`、运行 manifest 四处，**汇总本身是主要工作量**。
- **跨命令影响（必须端到端复测）**：阶段二的帧列表现在取自**配置** `doc["hips_paths"]`（各 `p2_op_*`），改造后须改为从该 JSON 读。这跨越 `normalize → mosaic` 两个命令，是本计划中**唯一的跨命令改动**。
- **排序**：**等 D1–D3 落地后再排**（您已裁定先不动）。

---

## D5 正本同步要点（`DEV_MODE.md` 本轮订正）

| 处 | 订正后口径 |
|---|---|
| 开关节 | 新增**授权边界**（开关只决定额外输出是否保留，不改变执行路径）＋**实现现状**（四宏中三个零消费者；有消费者的那处已按退役处置） |
| 覆盖的能力 | 平面件＝额外输出、**不属产品面**；**内存块传递＝性能阶段目标，尚未达成**；检查点/探针标注**未实现** |
| 标准模式下的产物面 | **产物只有两项**；内部载荷应落**产品面之外的工作区**；**不再声称「已经不经磁盘」**；逐帧 JSON 不应出现在产品面内；`p1_stack.json` 边界保留（跨进程契约） |
| 中间产物的工作区与开关 | 新增：产品面与工作区边界、开关语义、**取数保持现状**（热路径读回本步保留，内存块是性能阶段项） |
| 中间产物的取数与缺失判定 | 取数**只保留一条路径**；两处实现已退役为单一行为（`p1_calibrated_path` 单路径；`p1_prerun_calibrated_check` 改为**不阻塞的显式诊断读数**，`calibrated_fell_out` 不留） |
| 清点与耦合 | 内存块传递的清点＋两条结构约束（执行器按帧 vs 节点体按整次运行；公开 API 只装扁平标量缓冲）＋「耦合约束不约束本步」 |

---

## A0 实测结果（子批 A0：删两处编译期条件）

### A0.1 改动点（`lib/infrastructure/scheduler/src/module_adapters.cpp`，19 插入 / 34 删除）

| 位置 | 改动 |
|---|---|
| `p1_calibrated_path`（原 `:1543-1558`） | 删 `#if/#else/#endif` 与回退分支，**只留一条路径**：`return p1_product_path(doc, light, "calibrated");` |
| `p1_prerun_calibrated_check`（原 `:1629-1654`） | 删两分支（`fell_out` 留痕 + fail-closed），改为**不阻塞的显式诊断读数**：逐帧 `fprintf(stderr, …)` 上报缺失，恒 `return Result<void>::success();` |
| 4 处调用点注释（`p1_op_cosmetic`/`p1_op_wcs`/`p1_op_photometry`/`p1_op_drizzle` 各 1 处） | 注释由「发布构建缺失即 fail-closed；开发构建落 fell_out/why」改为「calibrated 缺失的显式诊断读数（不阻塞；取数只有一条路径）」 |

依据 `AGENTS §13`（门禁式机制改为**显式诊断读数**且**不得阻塞**）与正本 `DEV_MODE.md`《中间产物的取数与缺失判定（退役登记）》。

### A0.2 标准构建

```
[ 94%] Built target acsd_module_adapters
[ 97%] Built target acsd_cli_runtime
[100%] Linking CXX executable acsd
[100%] Built target acsd
```
退出码 0；`build/linux-control`（Release，Unix Makefiles，`ACSD_DEV_*` 全 OFF，即交付构建形态）。

### A0.3 H3 科学量前后对拍（**逐位一致**）

同一配置（4 帧真实 M42，跨 **3 个夜晚** 20251212/20251216/20251224），改造前后各跑一次，退出码均为 0。指纹 = 逐帧 WCS `rms_px` + 逐帧通量（源数/通量和/误差平方和）+ 帧级 `variance` + 全部 HiPS 科学载荷（2952 个文件）sha256 汇总。

| 量 | 改造前 | 改造后 |
|---|---|---|
| WCS `rms_px`（20251212 M2） | 0.126444154383 | 0.126444154383 |
| WCS `rms_px`（20251216 M2） | 0.109686357856 | 0.109686357856 |
| WCS `rms_px`（20251224 M2） | 0.119054814545 | 0.119054814545 |
| WCS `rms_px`（20251212 M6） | 0.134241145835 | 0.134241145835 |
| 通量 源数 / 和 | 1585 / 179441340.754776 | 1585 / 179441340.754776 |
| 通量 源数 / 和 | 1746 / 163600160.847497 | 1746 / 163600160.847497 |
| 通量 源数 / 和 | 1802 / 160626635.817726 | 1802 / 160626635.817726 |
| 通量 源数 / 和 | 2753 / 165558827.476738 | 2753 / 165558827.476738 |
| 帧级 `variance` | 290.627924333619 / 323.949823511642 / 309.752595443366 / 180.864348837784 | 同左，逐位相同 |
| HiPS 载荷 | 2952 文件，digest `73107dbada0c291d4093a1be8f70d968` | 2952 文件，digest `73107dbada0c291d4093a1be8f70d968` |
| 峰值 RSS（`/usr/bin/time -v`） | 5 923 600 KB | 5 492 684 KB（运行间波动，非本改动引起） |

整份指纹文件 `diff` 无差异 ⇒ **行为等价**。

**必须说明的口径限制**：本配置未启用测光尺度拟合（`p1_phot.json` 记 `photscale_source: "none"`、`degraded_reason: "photscale_absent"`、`n_frames_applied: 0`）⇒ **不产生逐帧 `sigma_residual_dex` 与帧级尺度 `k_photo`**。故改用**对被测路径更敏感**的等价见证：全部四个量都在 `p1_calibrated_path` 的下游，且 HiPS 载荷做了逐文件 sha256。若要按 `sigma_residual_dex`/`k_photo` 复验，需另配启用测光尺度拟合的配置（登记为待办）。

### A0.4 H4 grep 证据（单路径）

| 检查 | 结果 |
|---|---|
| `grep -rn "ACSD_DEV_INTERMEDIATE_CACHE" lib/` | **0 处**（全 lib 树无残留） |
| `p1_calibrated_path` 函数体 | 只有一个 `return`（单一取数路径） |
| `grep -c "return light;"` | **0**（回退原帧已消失） |
| `grep -c '(*man)["calibrated_fell_out"]'` | **0**（留痕已消失） |
| `grep -c "prerun: calibrated product required but missing"` | **0**（fail-closed 已消失） |
| `p1_prerun_calibrated_check` 返回值 | 仅 `Result<void>::success()`（不阻塞） |

⇒ 不存在「文件路径取数 / 原帧取数」并存的第二条路，也不存在「按构建形态分叉」的第二种行为。

---

## B 轮登记与设计：解算几何并入逐帧 HiPS 产品树（SIP 裁决）

### 问题（实测事实）

| 事实 | 证据 |
|---|---|
| 解算后的几何**今天只活在** `p1_wcs.json` 里 | 帧产物头部**没有回写**解算 WCS：`calibrated_*.fts` 头 WCS ≡ `input_light` 头 WCS，4/4 帧逐位相同 |
| 产品侧**不承载**几何 | `p1_products.json` 的 `frames[]` 无 `crval/crpix/cd/wcs` 键；HiPS `signal/properties` 无 `CRVAL`；tile 头只有 `NSIDE` |
| 故若直接移走 `p1_wcs.json` | 解算几何变成**可清理的调试产物** ⇒ 违 `AGENTS §13.1`「几何取自产品自身」，产品不可解释 |

### 裁决与前置关系

**B 轮必做**：把解算几何（`CRVAL` / `CRPIX` / `CD` / **SIP** 系数）**承载进逐帧 HiPS 产品树**（`manifest.json` 与/或 `properties`），使「几何取自产品」**可满足**。
**前置关系**：`p1_wcs.json` 移入工作区 **前置 = 上一条落地**。
⇒ 移出清单由 **12 项改为 11 项**（`p1_wcs.json` 的解算几何先并入产品树，再移走该文件）。

**登记落点**：`eng/packaging/config/config_registry.json#engineering_status`（条目「解算后的几何…并入逐帧 HiPS 产品树」= B 轮必做；「p1_wcs.json 移入工作区」= B 轮，`blocked_by` 指向前者）。

### 承载面设计（B 轮定稿，此处为方向）

- **位置**：逐帧 HiPS 产品树的 `manifest.json` 增一个 `geometry` 对象（该文件已是机器可读 JSON 且是产品集完成标记，不新增载体，合 §13）；`properties` 内同时落可读键供人查阅。
- **字段（方向）**：`ctype1/2`、`crval1/2`、`crpix1/2`、`cd11/cd12/cd21/cd22`、`pixel_origin`（像素口径必须显式，沿用 `p1_wcs.json` 的 "0-based array index (index-is-center); FITS 1-based xp = x + 1"）、`sip{a,b,ap,bp,order}`。
- **一致性判据**：新承载面与 `p1_wcs.json` 的几何**逐位一致**（B 轮须给逐帧比对读数），否则不得移走原文件。
- **消费者**：`q3lib.load_wcs()` 等改读产品内 `geometry`，此后**不再有"移走即断"的依赖**。

### 本轮不做

不写实现代码（含 `module_adapters.cpp`），不移动任何文件，不改 `docs/` 正本（随 B 轮实现同批改）。
