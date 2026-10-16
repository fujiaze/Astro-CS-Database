# R0 基线核对：生产调用图、旧开发开关与单位表

> 本轮执行单元。基线 commit `942c978e875bf7673ebc30a89fc0b59738092e26`
> （分支 `audit/acsd-science-docs-p5`，`git merge --ff-only origin/...` 后）。
> 构建目录 `build/linux-sci`（Release，全部 `ACSD_*` 开发开关 OFF）。
> 本文只记录**源码静态追踪 + 本地实际运行事实**，不把"函数存在"当成"真实执行"。

## 1. 环境事实（可复跑）

```text
cmake 3.31.6 / ninja / g++ (Debian 14.2.0-19) -std=c++17
CPU 16 核, RAM 23G, /workspace 786G (71% used)
HEAD 942c978e875bf7673ebc30a89fc0b59738092e26
构建 SHA (build_stamp_manifest head_sha) 4492782057f561189b9c148ae8721e45fb139e63
```

注意：`build_stamp_manifest.json` 的 `head_sha` 与该构建时的 git HEAD 一致，用来
确认"二进制来自哪个源码状态"。

## 2. 旧开发/诊断开关清点（源码事实）

`CMakeLists.txt` 顶层 option：

| option | 默认 | 派生 compile definition | 落点 |
|---|---|---|---|
| `ACSD_PROBES` | OFF | `ACSD_PROBES=1`（仅 `acsd_probes` 这个 target PUBLIC） | 性能探针库 |
| `ACSD_DEV_MODE` | OFF | `ACSD_DEV_MODE=1` → **只有** `acsd_module_adapters` | 主开关 |
| `ACSD_DEV_INTERMEDIATE_CACHE` | `${ACSD_DEV_MODE}` | `ACSD_DEV_INTERMEDIATE_CACHE=1` → `acsd_module_adapters` | 子项 |
| `ACSD_DEV_CHECKPOINT` | `${ACSD_DEV_MODE}` | `ACSD_DEV_CHECKPOINT=1` → `acsd_module_adapters` | 子项 |
| `ACSD_DEV_PROBES` | `${ACSD_DEV_MODE}` | `ACSD_DEV_PROBES=1` → `acsd_module_adapters` | 子项 |

源码命中（`grep -rn`，全 `lib/**`）：

- `ACSD_DEV_MODE`：**1 处**，`module_adapters.cpp:6224`（`#if defined(ACSD_DEV_MODE)`）。
- `ACSD_DEV_INTERMEDIATE_CACHE` / `ACSD_DEV_CHECKPOINT` / `ACSD_DEV_PROBES`：
  **0 处源码命中**（纯 CMake 侧死开关；定义传了但没人用）。
- `ACSD_PROBE_SCOPE*` / `ACSD_PROBE_GAUGE` / `ACSD_PROBE_COUNT`：6 个文件
  （`module_adapters.cpp`、`coverage/src/sky_plane.cpp`、`phase2_session/p2_session.cpp`）。
  这些调用**没有**被 `#if ACSD_DEV_MODE` 包住；它们的生死完全由 `ACSD_PROBES`
  这个**独立** option 决定，与 `ACSD_DEV_MODE` 无关。

### 结论（与 DEV_MODE.md §8 / DELTA D19 一致，本轮复核）

1. **用户可见的诊断编译入口有 2 个**：`ACSD_PROBES`（性能）与
   `ACSD_DEV_MODE`（主）+ 3 个从不生效的子项。不满足"唯一公开编译开关"。
2. `ACSD_PROBES` 与 `ACSD_DEV_MODE` **互不相干**：可以
   `-DACSD_DEV_MODE=OFF -DACSD_PROBES=ON`，得到"发布版 + 详细性能日志"，
   这正是 DEV_MODE.md §7 明令禁止的形态。
3. `ACSD_DEV_MODE=ON` 当前**只**多做一件事：写
   `<work_dir>/p1_phot_stars_<frame>.json`（A8/A10 逐星诊断，非产品面）。
   没有快照工作区、没有 `debug_manifest.json`、没有 perf JSONL 通道。
4. 运行期环境变量 `ACSD_PROBE_LOG` 是 `ACSD_PROBES=ON` 时的**第二个**开关；
   旧 `ACSD_NODE_TRACE` / `ACSD_LEASE_TRACE` / `ACSD_UPM_PROFILE` / `ACSD_*_FAULT`
   需逐项核对（见 §5）。

## 3. "写盘再读回"是否改变科学数据来源（关键中立性问题）

`module_adapters.cpp` 的工作区中间产物（`p1_work_path`）：

| 文件 | 生产者 | 消费者 | 是否计算依赖 |
|---|---|---|---|
| `p1_sources.json` / `p1_psf.json` | wcs/psf 节点 | photometry 节点 | **是** |
| `<frame>/p1_wcs.json` | wcs 节点 | drizzle/writer 节点 | **是** |
| `p1_flux.json` | photometry | snr/下游 | **是** |
| `p1_phot.json` | photometry | provenance | **是** |
| `calibrated_<base>.fits` | calibrate 节点 | cosmetic 节点 | **是** |
| `cleaned_<base>.fits` | cosmetic 节点 | drizzle 节点 | **是** |
| `badcol_report.json` | cosmetic | 审计 | 审计 |
| `p1_phot_stars_<frame>.json` | photometry | **无** | 否（诊断） |

**核对结论**：这些中间文件是阶段一节点之间**真实的数据依赖载体**，在
`ACSD_DEV_MODE=OFF` 时同样写入、同样被读回。唯一被 `ACSD_DEV_MODE` 门住的
`p1_phot_stars_*.json` **没有任何消费者**（只写不读、写失败仅 stderr 警告）。

因此：当前仓库**不存在**"开发模式决定下游读哪个文件"的科学数据分叉。
但 DEV_MODE.md §8.2 指控的"先写盘再读回"在事实层面确实存在于工作区协议中，
只是它**不是**开发诊断，而是阶段间的既定的科学交换面。这一点必须写清：
整改方向是"把纯诊断快照与科学依赖中间物分离"，**不是**删掉后者。

## 4. 阶段二权重链实际路径（源码追踪）

`p2_op_integrate`（`module_adapters.cpp` ~12352–13270）裁定顺序：

1. `p2_corrected.json` 的 `variance_available && uncertainty_available` 且每帧
   `var_file` 非空 ⇒ `corr_var_ready=true` ⇒
   `weight_from_corrected_variance`（`weight_source="corrected_variance"`）。
2. 否则逐帧 `aio_hips_open(..., AIO_HIPS_RD_IVAR)`；全部命中 ⇒ `per_sample_ivar`。
3. 任一帧缺 ivar ⇒ 走帧级 SNR 链：
   `compute_inverse_variance_weights`，`w_k = SNR_k²/F_ref,k²·g_k²`。
   `snr_path_effective ∈ {frame_reconstruct, sparse_reconstruct}`，
   当且仅当**所有帧**都声明且校验通过稀疏层时才可能取 `sparse_reconstruct`。
4. 帧级 SNR 链闭合失败 ⇒ `Result<void>::fail`（fail-closed，无等权降级）。

**关键事实**：即使 `snr_path_effective == "sparse_reconstruct"`，帧级链
`compute_inverse_variance_weights` 仍然只产出 `F_ref,k` 与 `g_k`（层不进该链），
逐像素权在下方 `weight_from_sparse_layer_pixel_prepared` 现场求值。这与
weight_chain.h 的接口设计一致。

**未闭合项**（本轮未运行真实数据，属静态结论）：
- `ACSD_FRAME_SNR` / `ACSD_REFERENCE_FLUX` 两个 HiPS 属性键的**阶段一写入端**
  是否真的写、写的是什么统计量，未在本地运行验证（D03）。
- `ACSD_SPARSE_SNR_LAYER` 属性在生产运行中是否出现，未验证（D03）。

## 5. 阶段二产品面（源码追踪）

`p2_op_write_mosaic`（`module_adapters.cpp:13758`）：

```cpp
int flags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT;
if (uncertainty_available) flags |= AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR;
```

- 至少写 `signal` + `support` 两个科学图像子产品；`uncertainty_available` 时
  再加 `variance` + `ivar`。与"最终只发布一个 signal 通道"冲突（D02）。
- `obs_filter` 取 `cov_doc["frames"][0].filter_passband`（第一帧身份冒充总体
  响应，D07）。
- `lib/algorithms/coverage/tools/stage2.cpp:828` 同样 `SIGNAL|SUPPORT`
  （legacy 兼容子图，非产品入口）。
- `covered_area = support * a_cell` 是 signal 归一化的**内部分母**，删 support
  产品时不能动它。

`p2_coverage_build`（`coverage/src/coverage.cpp:194-238`）：
- `f != filter_ref` ⇒ `"filter mismatch"` 返回 1（D01，直接与用户目标冲突）。
- `fr != frame_ref` ⇒ `"hips_frame mismatch"`（坐标一致性，合法保留）。
- `target_order = min(max_leaf_order)`（D06）。

阶段一 photometry 另有 `PASSBAND-IDENTITY-GATE-01`
（`module_adapters.cpp` ~6288-6320）：同组后续帧 filter 身份与首帧不一致 ⇒
**该帧 fail**。这是 normalize 组内的通带身份门；与阶段二 coverage 的 union 门
是两处独立实现。

## 6. 本轮执行范围声明

R0 完成。R0D（唯一编译开关 + 探针复用 + 快照工作区）为下一提交。
R1（取消跨滤镜拒绝）、R6（单 signal 产品）按依赖次序推进。

真实数据索引与 E2E 在 R0D 之后建立；未运行的结论一律标注"静态/未运行"。


---

## 7. 本轮已由实跑证实/推翻的静态结论（追加，不改上文）

| 静态结论 | 实跑判定 | 证据 |
|---|---|---|
| D03：`ACSD_FRAME_SNR`/`ACSD_REFERENCE_FLUX` 是否真的写进 HiPS 头 | **证实写出且可读**，数值与 `p1_snr.json` 的 `snr_f`/`flux_adu` 一致 | §3 E2E_RUN.md |
| D03：`ACSD_SPARSE_SNR_LAYER` 规整层是否有生产者 | **无生产者**（HiPS 头无该键）⇒ `sparse_reconstruct` 生产不可达 | 同上 |
| D05：生产实际走哪条权重路径 | **`per_sample_ivar`**（ivar 可用 ⇒ SNR 链不触发，`snr_path_effective=not_used`） | §5.1 E2E_RUN.md |
| D02：mosaic 是否发布多层科学图像 | **改动前** `["signal","support","variance","ivar"]`；**改动后** `["signal"]` 且 signal 逐位不变 | §5.2 E2E_RUN.md |
| D11：drizzle 面亮度归一是否正确 | **正确**。`B = 58.46 ADU/px ÷ 2.0799e-11 sr = 2.81e12 ADU/sr`，HiPS signal 中位数 2.756e12（1.02 差 = median vs 覆盖加权均值） | §7 E2E_RUN.md |
| D19：ON/OFF 科学等价 | **证实**（1496/1496 科学 tile 逐位相同；差异仅时间戳/wall-clock 诊断） | §4 E2E_RUN.md |
| 旧 dev 开关是否造成路径分叉 | **未发现**。唯一 dev-gated write 是 `p1_phot_stars_*.json`（无消费者、写失败仅 stderr） | §3 R0_baseline.md |

仍**未运行**的项集中在 R2 闭环、R3 T 桥梁、R4 曲边 oracle、R5 连通分量实测、
跨滤镜合成与 HST 前向；完整清单见 E2E_RUN.md §9。
