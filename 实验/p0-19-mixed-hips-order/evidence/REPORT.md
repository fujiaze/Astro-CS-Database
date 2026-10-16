# P0-19 跨 hips_order fail-closed —— 证据索引

- Issue: [#19](https://github.com/fujiaze/Astro-CS-Database/issues/19)（施工任务书评论 `6216942225`）
- 起始 main: `cebef3ebb48772a5a891582fd0f201de3fa38792`
- 生产代码 commit: `d8cf94687`（仅 `lib/infrastructure/scheduler/src/module_adapters.cpp`；
  在远端并发文档提交 `cebea1025` 之上 rebase 后）
- 测试/证据 commit: `7198f50ef`、`65f49f0db`
- 本轮报告 main: `65f49f0dbb56f8c1f75aadc55d67119d2621ded1`
- 本轮**不实现**跨阶面积保真重采样；只做可判定的临时 fail-closed。

## 1. 改动

`p2_coverage_build`（`lib/algorithms/coverage/src/coverage.cpp`）的跨阶 MOC union、
`target_order=min`、球面几何、科学权重**均未改动**。新增的是生产准入/消费端闸门：

| 位置 | 作用 |
| --- | --- |
| `p2_hips_order_uniform` | 唯一最小判据：各帧真实 `max_leaf_order`（= HiPS `hips_order`）不全同 → false + 具名错误（order 集合升序、逐输入明细排序 ⇒ 与输入排列无关） |
| `p2_coverage_artifact_orders_uniform` | 从 coverage artifact `frames[]` 复用同一判据（供直接读 artifact 的消费点） |
| `p2_op_coverage` | 第二次 `p2_coverage_build` 回填后、写 `p2_coverage.json` 之前：`st.status=fail` + `man.error_kind=input` + 具名错误 |
| `p2_coverage_from_artifact` | 重建 `P2CoverageView` 后复用同一校验（`p2_op_sample` 消费端第二防线） |
| `p2_op_upm_apply` | 直接读 `p2_coverage.json` 的独立像元映射入口（按 target_order 反推 nside 求 `pix2ang_nest`） |
| `p2_op_write` | 发布端：外部混阶 artifact + 被构造 integrated 产物不得落 complete |

具名错误：
`mixed hips_order unsupported without spherical resampling: distinct orders={...}; inputs=[<order>|<path>|<frame_id>,...]`

## 2. 复现命令

```bash
# 夹具（生产写侧 aio_hips.h）
g++ -std=c++17 -O2 -I lib/infrastructure/aio/include -I lib/infrastructure/aio/src \
  实验/p0-19-mixed-hips-order/code/make_hips_fixture.cpp \
  -Wl,--start-group build/linux-sci-p5/libacsd_{aio,cfitsio,common,hips,core}.a -Wl,--end-group \
  -lz -lpthread -o /tmp/make_hips_fixture
# 真实 CLI 矩阵（10 例，写 02-post-fix-cli-matrix.json）
python3 实验/p0-19-mixed-hips-order/code/run_issue19_cli_matrix.py
# C++ 生产入口验证（ModuleRegistry→IModule::plan→execute→last_manifest，19 项）
python3 eng/tests/unit/run_p0_19_mixed_hips_order_test.py --fixtures /tmp/p019/fx
```

## 3. 反例：pre-fix 静默接受（bug 证据）

`docs/` 未改；pre-fix 二进制 `build/linux-sci-p5/acsd`
sha256 `502556166b992f577035cc7877f2e4de8b6ebbdb601875928048ce05fde1ca88`。

| 输入 order | coverage | 产物 | 证据 |
| --- | --- | --- | --- |
| (0,1) | **ok**（cells=1, target_order=0） | `p2_coverage.json` 发布 | `01-pre-fix-mixed01-coverage.json`（sha256 `8e5138df…`） |
| (1,0) | **ok** | 发布 | 同上（2 帧 path/order 记录） |
| (0,1,0) | **ok** | 发布 | 见 pre-fix CLI 日志 |

混阶通过后链继续到 sample（`obs=128`）与 upm_apply，仅因夹具缺观测几何在
sky_plane 失败——与 order 无关。即：**天球位置错配的科学链在 pre-fix 是被放行的**。

## 4. 正例/反例矩阵（post-fix，真实 CLI）

`acsd mosaic --json <cfg> -y`；退出码语义：3=INPUT，2=DATA。
证据：`02-post-fix-cli-matrix.json`、`02-post-fix-cli-logs/`。

| 用例 | orders/filters | exit | `p2_coverage.json` | 结果 |
| --- | --- | --- | --- | --- |
| same00_samepath | (0,0) Halpha/Halpha | 2 | present | 通过 |
| same00_filter_diff | (0,0) Halpha/Red | 2 | present | 通过 |
| same00_empty_filter | (0,0) Halpha/显式空 | 2 | present | 通过 |
| same11_filter_diff | (1,1) Halpha/Red | 2 | present | 通过 |
| single0 / single1 | 单帧 | 2 | present | 通过 |
| mixed01 | (0,1) | **3** | **ABSENT** | 具名拒绝 |
| mixed10_perm | (1,0) | **3** | **ABSENT** | 诊断与 mixed01 逐字相同 |
| mixed010_three | (0,1,0) | **3** | **ABSENT** | 集合 `{0,1}` |
| mixed01_filter_diff | (0,1) 异滤镜 | **3** | **ABSENT** | 具名拒绝 |

混阶运行目录仅有 run 级诊断文件；无 `p2_coverage.json`、无 `p2_samples.json`、
无 `p2_final.json`、无 mosaic HiPS；run manifest `status=incomplete`。

> 同阶用例 exit=2 并非本修复引入：夹具缺 `sky_plane` 所需观测几何，
> pre-fix 同配置同样 exit=2（同样在 upm_apply 因缺 `p2_sky_plane.bin` 失败）。

## 5. C++ 生产入口验证（19/19 PASS）

证据：`eng/tests/unit/p0_19_mixed_hips_order_test.cpp` +
`run_p0_19_mixed_hips_order_test.py`。经真实 `ModuleRegistry`（`register_phase_modules`
→ `create` → `plan` → `execute` → `last_manifest`）驱动四个生产节点：

- `acsd.phase2.coverage`：混阶 (0,1)/(1,0)/(0,1,0) 拒绝且 manifest `error_kind=input`；
  不写 artifact；排列互换诊断逐字相同；同阶 (0,0) 异滤镜、(1,1)、单帧通过并写 artifact。
- `acsd.phase2.sample`：合法 JSON schema 但 `max_leaf_order` 混阶的 artifact
  → 拒绝（`coverage artifact rebuild failed: mixed hips_order …`）；同阶 artifact
  不被 order 门拒绝（后续因缺真实输入失败，属预期）。
- `acsd.phase2.upm-apply` / `acsd.phase2.write`：最小合法 `p2_sky_plane.bin` /
  `p2_integrated.json` 越过前置读后，混阶 artifact 在 artifact 读取处被同一具名校验拒绝。

## 6. 同阶科学数值回归（逐字节未变）

证据：`04-same-order-regression.json`。
pre-fix 与 post-fix 在**同一同阶构型**上的生产产物逐字节相同：

| 构型 | coverage artifact sha256 | sampler artifact sha256 |
| --- | --- | --- |
| (0,0) Halpha/Halpha | `b4e2112b…`（pre = post） | `cb21641a…`（pre = post） |
| (1,1) Halpha/Red | `672ab929…`（pre = post） | `dd010ecb…`（pre = post） |

即修复对同阶输入的 coverage 与采样数值**零影响**。完整信号产品（`mosaic/` signal）
需要带观测几何（pointing spacing / pixel scale）的真实多帧构型；本轮最小夹具缺该几何，
**不编造 E2E**，故同阶回归止于 coverage+sampler 两级真实产物。

## 7. Sanitizer

`build/asan`（`ACSD_ENABLE_SANITIZERS=ON`）下运行混阶 (0,1) 与同阶 (0,0) 真实 CLI：
AddressSanitizer/LeakSanitizer 命中数 **0**（`03-asan-logs/`）。

## 8. 既有测试基线（与本修复无关）

`python3 -m pytest eng/tests/integration/test_registry_code_parity.py eng/tests/module -q`
在**未含本修复的 pristine HEAD** 上同样 `26 failed, 56 passed`（ANCHOR_STALE：
`AGENTS.md`/`docs/**` 行号漂移，Codex 正在重构文档）。本修复不新增失败。

## 9. 残余问题（如实登记）

1. **跨阶面积保真球面重采样仍未实现**：本轮只做 fail-closed；合法混合阶输入当前被明确拒绝，
   不是"最终禁止跨分辨率组合"。
2. `p2_coverage_artifact_orders_uniform` 在 artifact **缺 `frames`** 时返回 true
   （不新造拒绝面）。`p2_coverage_from_artifact` 本身硬要求 `frames`，故 sample 路径
   仍严格 fail-closed；upm-apply/write 对无 `frames` 的旧产物保持既有语义。
3. 混阶 blocked 后 run manifest 的记录键（`error_kind`）沿用 coverage 节点既有
   `input` 语义；未新增退出码。
