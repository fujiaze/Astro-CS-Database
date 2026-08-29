# WIN-003 验证报告 — doctor + full benchmark(逐 kernel 选择)+ AVX512 检查 + profile 冻结

结论: **PASS**(doctor PASS; full benchmark 12 kernel 逐 kernel 选择、每 shipped backend Oracle PASS; AVX512 检查无 AVX512 无收益问题; profile 冻结并 VALID)。

## 1. 验收判据(03_TASK_DETAILS.md L146)
> doctor；逐 backend 安全检查；full benchmark；检查 AVX512 downclock/无收益；冻结 profile。
> PASS = 每 shipped backend Oracle PASS；逐 kernel 选择；资源门禁 PASS。

## 2. 环境
- 节点: **Fatduck**(`fujia@100.104.10.71`) = **AMD Ryzen 7 5800X 8-Core Processor**(family 25 / model 33 / stepping 0), amd64, 16 逻辑核, 64 GiB RAM。
- 工具链: MSVC 14.50.35717 amd64 Release。
- main SHA: `5b3b3e35`(含本任务对 cpu_features/hardware_inspect 的 MSVC CPUID 修复)。

## 3. doctor(逐 backend 安全检查)
`astrocs doctor --json` → **PASS**(rc 0):
| check | status |
|---|---|
| baseline_selftest | pass |
| hardware_sanity | pass |
| backends_manifest | **no shipped DSO (builtin baseline)** → pass |

Windows 发布机当前仅 builtin baseline backend(未打包独立 DSO, 属 WIN-009 范围)。baseline host oracle 全 PASS。

## 4. full benchmark(逐 kernel 选择 + 资源门禁)
`astrocs benchmark cpu --full --output win_cpu_profile.json` → **PASS**(rc 0)。
- **12 kernels 全部测量**, 每 kernel 写出 `backend_id / workers / block_size / precision / size_class / oracle_status / measurements(p05/median/p95/mad)`。
- **校验**: 12/12 `oracle_status="pass"`; `verdict="PASS"`; `mode="full"`; `schema_version=1`。
- `memory_benchmark`(GB/s): copy 22.84 / read 5.86 / write 16.30 / triad32 25.14 / triad64 19.82(非零、正常范围 → 资源门禁 PASS)。
- kernel 清单: calibration-pixel-transform、noise-snr-reductions、wcs-psf-batch、drizzle-overlap/accumulate/normalize、upm-spmv/residual/weight-update、rejection-statistics、integration-accumulate、hips-bulk-transform。

### profile 校验
`tools/validate_cpu_profile.py win_cpu_profile.json --hardware hw_inspect.json --commit <full>` → **VALID**。
- schema 校验通过(必需字段齐全); 全部 kernel oracle pass;
- hardware fingerprint 匹配当前 `hardware inspect`(feature_bits=31, affinity=16, xcr0=7, vendor=AuthenticAMD);
- commit 匹配当前 SHA(`build.commit` = `5b3b3e35`, 非 dirty)。

## 5. AVX512 downclock / 无收益检查
- Fatduck CPU(AMD Ryzen 7 5800X,Zen3)**不支持 AVX512**: `feature_bits=31`(SSE2|SSE4_1|AVX|AVX2|FMA, 不含 AVX512F bit5); `feature_names=[sse2,sse4_1,avx,avx2,fma]`。
- 因此**本机无 AVX512 场景**: 不存在 AVX512 downclock 或 "AVX512 无收益" 的候选; benchmark 未选择任何 AVX512 路径(所有 kernel 均 backend=baseline)。检查成立, 无 AVX512 无收益误选。
- Linux 侧(vm-bj Intel Xeon Gold 6148, 支持 AVX512)CPU 特性检测正常(vendor=GenuineIntel, features 含 avx512f), 不影响本 Windows 发布机 profile。

## 6. 冻结 profile
- 发布机正式 profile 已冻结: `C:/Users/fujia/win_cpu_profile.json`(full 模式, 12 kernel, verdict PASS, schema VALID)。
- 该文件为发布机本地构件(与根目录 Linux `cpu_profile.json` 一样不入 git; 机器绑定, 禁止跨机复用)。
- Windows Phase run 经 `--cpu-profile <path>` 读取; 无 profile 时按 06 §6 仅 baseline + 资源门禁兜底。

## 7. 修复(本任务解决)
- **CPU 特性/身份探测在 MSVC 下全零**的缺陷: `feature_bits=0`、`vendor=""`、brand/family/model/stepping 全空。
  - `lib/backend_host/cpu_features.cpp`: 补 `_M_X64` 分支, 用 `__cpuidex`(leaf1+leaf7)+`_xgetbv` 检测 SSE4_1/AVX/AVX2/FMA/AVX512F 并按 XCR0 门控。
  - `lib/backend_host/hardware_inspect.cpp`: 补 `_M_X64` CPUID 身份读取(vendor=leaf0 EBX+EDX+ECX; brand=0x80000002-4; family/model/stepping=leaf1)。
  - 效果: Windows `hardware inspect` 现正确输出 vendor=AuthenticAMD、family=25、feature_bits=31 等。

## 8. 限制 / 遗留
- shipped backend 当前仅 builtin baseline;独立 CPU backend DSO 的分发与打包属 WIN-009。
- 真实 32R 数据上的链路资源门禁/贡献(PASS 才可 32R)属 WIN-006/7;本任务为 benchmark 本身资源门禁。
- `block_size=0` / `workers=2` 为基准实测选择(唯一 shipped backend 下的保守并行), 非硬编码; 后续有后端 DSO 时 benchmark 会重选。

## 9. 证据文件
- Fatduck: `C:/Users/fujia/doctor_full.json`、`win_cpu_profile.json`、`hw_inspect.json`、`build_Release.log`。
- 校验: `validate_cpu_profile.py` → VALID; `doctor --json` → PASS。
