# CPU-003: 实现真实 benchmark 与 profile

任务 ID: CPU-003
Gate: G3
依赖: CPU-001; CPU-002
平台: Linux+Windows
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` CPU-003 与 `08_CPU_RESOURCE_ACCEPTANCE.md` §4：

> 命令：`astrocs benchmark cpu --quick|--full --output profile.json --events-jsonl`。
> 固定流程：能力→provider correctness→memory copy/read/write/triad→每代表 kernel
> small/medium/large→workers 1..available→block/tile→3 warmup+7 measure→median/MAD→winner。
> 错误候选不计时；AVX512提升<3%选AVX2；worker增加收益<3%可少选但 available≥2 heavy不得选1。
> 保存全部原始候选，独立 `--verify-profile` 复读。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| `astrocs benchmark cpu --quick|--full --output --events-jsonl` | `cli/commands.cpp` benchmark 分支调用 `generate_profile_v2` | c01 日志 |
| 能力探测 | `hardware_inspect_json_v1` + `astrocs_cpu_detect_features_v1` → host/logical_available/quota_signature | profile host 字段 |
| provider correctness | 加载 `backends.manifest.json` 中 provider DSO（预检 hash/ABI/ISA）；每 provider 每 kernel 独立 scalar Oracle（`oracle_ref`，与 kernel 实现不同路径），失败候选不计时被剔除 | 12 kernel Oracle PASS；篡改候选被拒 |
| memory 基线 | `bench_memory`：read/write/copy/triad32/triad64 | memory_bandwidth 字段 |
| 每 kernel small/medium/large | 12 代表 kernel × 3 规模（full）；quick=calibration medium | kernels 12 项 |
| workers 1..available | `worker_candidates(avail)` → {1, 中位, 全部} 去重；budget 每候选设置 | 原始候选 workers 字段 |
| block 候选 | `block_candidates(L2 实测, elt_size)` 几何公比 4 | 原始候选 block 字段 |
| 3 warmup + 7 measure | `bench_kernel(..., warmup=3, samples=7)` | samples=7 |
| median/MAD | `bench_kernel` percentile 统计 | kernels median/mad |
| winner 仅 OK 候选 | `select_winner`（Oracle 失败结构性不胜出） | 单元测试 |
| AVX512 提升<3% 选 AVX2 | `select_with_noise_margin`(margin=0.03) | 单元测试 T-AVX512 |
| worker 收益<3% 可少选但 heavy 不得选 1 | winner_workers==1 且 avail≥2 时强制提升到 avail | profile workers≥2 |
| 保存全部原始候选 | `--events-jsonl` 逐条 `raw candidate` 事件 + `raw_samples_sha256` 绑定 | c01 jsonl |
| 独立 `--verify-profile` 复读 | `astrocs benchmark verify-profile <path>` + `verify profile --profile` | c02 日志 + 负例 |

## 实现文件

- `lib/backend_host/profile_gen_v2.cpp`（新）：v2 profile 生成器 + 独立 scalar Oracle + verify
- `lib/backend_host/profile_gen.h`：新增 v2 API（generate_profile_v2 / verify_profile_v2 / RawCandidate / KernelProfile / ProfileBundle）
- `CMakeLists.txt` / `cli/CMakeLists.txt`：加入 profile_gen_v2.cpp
- `cli/commands.cpp`：benchmark cpu → v2 生成器；verify-profile 复读；events-jsonl 原始候选发射
- `cli/parser.cpp`：`benchmark verify-profile` 命令（位置参数与 --profile 旗标）
- `tests/unit/cpu003_profile_v2_test.cpp`（新）：v2 字段/Oracle 门/winner/AVX512<3%/verify 正负例
- 测试同步（CPU-001 ABI 引入 head 后的遗留 brace 语法修复）：`tests/backend/kernel_bench_main.cpp`、`kernel_oracle_main.cpp`、`bench_harness_main.cpp`、`bench_candidates_main.cpp`
- schema/版本同步：`schemas/hardware_inspect.schema.json`（补 quota_signature）、`tests/backend/test_hardware_inspect.py`（0.10.0-alpha.2）、`tests/backend/test_cpu_profile.py`（机制性断言）、`tests/cli/test_bench_cli.py`（v2 适配 + 9 测试）

## Oracle 独立性与正确性

- `oracle_ref()` 以 double 精度独立计算 12 种 ACS_KOP_* 语义（calibration/noise/psf/drizzle×3/upm×3/rejection/integration/hips），与 `baseline_kernels_impl.inc` 的 f32 实现不同路径。
- 输入由确定性 LCG（seed=20260831）生成；同 seed 同输入可复现。
- 容差 2e-3 预冻结；Oracle 失败 → `oracle_pass=false` → 不计时、不参与 winner。

## 运行结果（本机 2 核 VM）

- quick: 1 kernel（calibration）× 3 provider × 2 workers × 4 blocks = 24 原始候选全 Oracle PASS
- full: 12 kernel 全 Oracle PASS；winner 均为 baseline w=2（本机 AVX2/AVX512 对 calibration 无 ≥3% 收益，符合"提升<3% 选保守"规则）
- `benchmark verify-profile` 复读 PASS；篡改 schema/workers/commit 均 FAIL（exit=8）
- 3 provider DSO（baseline 内置 + avx2/avx512 外部 .so）实际加载、self-test、测量

## 测试结果

- `ctest`: 52/52 PASS（含新增 cpu003_profile_v2）
- `tests/cli/test_bench_cli.py`: 9/9 PASS（v2 适配）
- `tests/cli/test_cli_protocol.py`: 10/10 PASS
- `tests/cli/test_cli_build.py`: 6/6 PASS
- `tests/backend/test_cpu_profile.py`: 7/7 PASS
- `tests/backend/test_isa_variants.py`: 5/5 PASS
- `tests/backend/test_abi_v1.py` / `test_abi_loader.py` / `test_abi_kernels.py`: PASS
- `tests/backend/test_isa_avx2_fma.py` / `test_isa_avx512.py` / `test_isa_avx.py` / `test_isa_bit_manip.py`: PASS
- `tools/quality/check_isa_leak.py`: SELFTEST_PASS + ISA_LEAK_PASS（CLI 无 AVX2/AVX512 泄漏；provider 库含对应指令）

## 遗留说明

- `tests/cli/test_monitor_events.py` 失败属于 MON-001/002 任务范围（V6.1 尚未实现，run 资源事件验收），非 CPU-003 回归。
