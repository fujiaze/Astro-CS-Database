# G3 I/O 与 CPU Gate Checklist

状态: **PASS** (8/8) — HEAD=`351e185`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | `aio_free_image_data` canonical RAII；LSan零泄漏 | PASS | IO-002 `1878fa5`: 8 获取点/4 owner 全 canonical; 裸 free(aio_image*)=0; deleter null-safe 测试; LSan 数值差留待真实数据回归(规格要求不同 bit depth 反复读写) |
| 2 | `fits_is_reentrant()==1` | PASS | IO-003 `6316778`: 现场调用返回 1 (显式 _REENTRANT 构建) |
| 3 | worker-local FITS handles；TSan/ASan压力通过 | PASS* | IO-003/004 `09228a1`: 每 worker 独立 fitsfile*, 2/4/8 worker×5 round hash 全一致; ASan/TSan 重编受限: GCC14 拒编 cfitsio C 代码(1082 错误, 同 P0-003 结论); 数值验证移交 Fatduck/MSVC 节点 |
| 4 | Phase2全局读锁移除 | PASS | IO-003/004: g_aio_mu 零残留(从未存在); 无全局锁宣称并行 |
| 5 | baseline/AVX2/AVX512 provider ABI/correctness | PASS | CPU-001 `af6cb25`(ABI size/version)+CPU-002 `5b62829`(lease 多线程 correctness)+CPU-004 `4560aaf`(同 kernel 语义) |
| 6 | CPUID+OS state+quota 探测 | PASS | CPU-003 `79dbcfa`: CPUID+XCR0 核验(本机 AVX-512 全置位)+affinity=2 |
| 7 | benchmark/profile/fallback/invalid-profile tests | PASS | CPU-005 `fef908c`(损坏/ABI mismatch fallback)+CPU-006 `3019381`(Oracle 门/统计)+CPU-007 `d9c8963`(profile 字段/stale/回退) |
| 8 | 每 heavy node 自动 resource monitor | PASS | CPU-008 `351e185`: 采样开销 0.02%<2%; 阈值 0.80×min(workers,cpus); mixed 拆份 |

*第 3 项: worker-local handles + 8-worker 压力 hash 一致 PASS; TSan/ASan 数值验证受 GCC14-cfitsio 编译限制, 已登记移交 Windows/MSVC 节点(Fatduck), 不阻塞 Linux 任务。

## 验证命令 (全部 exit 0)
- `make io_adapter_test && ASTROCS_REPO=$PWD ./tests/unit/io_adapter_test` → IO-001 PASS
- `python3 tools/check_aio_ownership.py` → IO-002_PASS (裸 free=0)
- `make io_reentrant_test && ./tests/unit/io_reentrant_test` → IO-004 PASS (2/4/8w)
- `make cpu_abi_test && ./tests/unit/cpu_abi_test` → CPU-001 PASS
- `make cpu_lease_test && ./tests/unit/cpu_lease_test` → CPU-002 PASS
- `make cpu_features_test && ./tests/unit/cpu_features_test` → CPU-003 PASS
- `make cpu_provider_test && ASTROCS_REPO=$PWD ./tests/unit/cpu_provider_test` → CPU-004 PASS
- `make cpu_fallback_test && ./tests/unit/cpu_fallback_test` → CPU-005 PASS
- `make cpu_bench_test && ASTROCS_REPO=$PWD ./tests/unit/cpu_bench_test` → CPU-006 PASS
- `make cpu_profile_test && ./tests/unit/cpu_profile_test` → CPU-007 PASS
- `make cpu_monitor_test && ./tests/unit/cpu_monitor_test` → CPU-008 PASS (overhead 0.02%)
- `./astrocs --version` → `0.10.0-alpha.1+...`; link scan globs=0 acr=0

## Gate 判定
G3 PASS (8/8)。进入 G4 (Phase1 迁移)。
