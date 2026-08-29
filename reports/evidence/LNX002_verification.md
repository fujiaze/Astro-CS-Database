# LNX-002 验证报告 — inspect/doctor/quick benchmark + profile fallback + ISA 安全

结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L140)
> inspect、doctor、quick benchmark; 测试缺失/stale/corrupt profile、unsupported backend。
> PASS = 安全 fallback baseline 且多线程; profile 决策可解释。

## 2. 执行内容(干净 Release 二进制 `build/lnx_v5_clean_rel/astrocs`)
在 amd64 vm-bj (2 核, Intel Xeon Gold 6148 @2.40GHz) 上运行以下既有 golden 测试套件与 CLI 命令:

| 套件/命令 | 覆盖(06 §5/§6 + 03 L140) | 结果 |
|---|---|---|
| tests.backend.test_cpu_profile (BENCH-004 golden, 7 用例) | profile schema 有效 / stale ISA state / stale affinity+commit / oracle fail→FAIL / AVX512 slower 永不胜 / **no-profile 多线程 avail≥2 不退 1** / 噪声裕量保守 | OK |
| tests.cli.test_bench_cli (BENCH-005 golden, 7 用例) | hardware inspect / doctor JSON checks / quick 模式受限时间完成(单调钟无阻塞) / benchmark 模式 flag 必填 | OK |
| tests.arch.test_backend_arch (5 用例) | ISA/backend 架构合法性 | OK |
| `hardware inspect --json` | architecture=amd64, brand=Intel Xeon Gold 6148, affinity=[0,1], available_cpus=2 | 输出正确 |
| `benchmark cpu --quick` | 生成 cpu_profile.json (mode=quick, schema_version=1, per-kernel backend/block 决策, verdict) | PASS |
| `tools/validate_cpu_profile.py` 对照 | 合法 profile→rc=0 VALID; corrupt(非法 JSON)→rc=1; stale(缺 backend hash)→rc=1 | PASS |

## 3. 关键行为核验(对应 L140 PASS 判据)
- **安全 fallback baseline 且多线程** *(06 §6)*: `test_cpu_profile::test_06` 直接断言
  `avail=2 backend=baseline workers=2 reason=no_valid_profile` 且 `NOT avail=2 ... workers=1`(可用≥2 不得退 1);
  `avail=16 ... workers=16`。无 profile 时只加载 baseline、workers 取有效 affinity、写出
  `reason=no_valid_profile`, **不把"保守"实现为"单线程"**。
- **corrupt/stale profile 失效** *(06 §5 "失效不得静默继续")*: validator 对 corrupt JSON 与非完整
  schema/后端 hash 的 stale profile 均拒绝(rc=1), 合法 profile rc=0; 基准 CLI `--cpu-profile` 走
  `validate_cpu_profile` 路径(见 `cli/main.cpp:345`), 失效拒用。
- **unsupported backend**: `test_backend_arch` + doctor `backends_manifest` 仅认可 manifest/CPU 可用且
  Oracle 通过的 backend, 无 shipped DSO(builtin baseline)时 `verdict=PASS`。
- **profile 决策可解释**: `cpu_profile.json` 含 mode、硬指纹、每 kernel 的 backend_id/block_size 选择、
  时间戳与 verdict; `astrocs doctor --json` 输出逐检查项(status/detail/verdict)。
- **quick 不替代正式证据** *(06 §7 --quick)*: quick 模式有限 kernel/size, 受限时间内完成(单调钟无阻塞),
  不跑长 benchmark。

## 4. 副作用
无源码改动; 本任务仅执行既有 golden 测试与 CLI 命令并记录。`cpu_profile.json`(untracked)为 quick
benchmark 产物, 不提交。

## 5. 限制
- 主机仅 2 核/受限内存; worker/backend 选择遵循 06 合同(禁硬编码), quick 结果仅供安全择优,
  正式性能证据由 `--full`(Windows 发布机)另行生成。
- 无 profile 时 fallback 为 baseline; 本机 baseline 并行足够(avail=2→workers=2)故不触发
  "baseline 并行不足则 FAIL"; 若机器极小导致该 FAIL 属合同预期而非本任务缺陷。
