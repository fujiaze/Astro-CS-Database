# G9 Linux Gate Checklist

状态: **PASS** (6/6) — HEAD=`43de4ac`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | GCC Release clean build | PASS | build/root-cmake Release 构建零 error; astrocs 产物 |
| 2 | Clang Debug/static analysis | PASS | build/clang-debug (clang++ 14) 构建 PASS; omp.h 宏保护 + OpenMP 条件化 (`43de4ac`) |
| 3 | 全模块 synthetic TEST | PASS | GCC: test synthetic all 7 组 PASS; clang: calibration/UPM PASS |
| 4 | 2核 resource/cancel/recovery | PASS | taskset -c 0,1 p2_workers_test PASS; SIGINT cancel 正常; verify manifest 校验 |
| 5 | 少量真实 hash/smoke | PASS | doctor selftest ok; --version 0.10.0-alpha.1+g<commit>; verify 缺失 manifest 明确报错 |
| 6 | 所有外部命令 timeout/log | PASS | cmd_test_synthetic 加 timeout (ASTROCS_TEST_TIMEOUT_S 默认 600s) (`16643bc`) |

## 验证命令 (全部 exit 0)
- `make -C build/root-cmake astrocs` → GCC PASS
- `cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug && make` → clang PASS
- `./build/root-cmake/astrocs test synthetic --group all` → 7 tests PASS
- `taskset -c 0,1 build/root-cmake/tests/unit/p2_workers_test` → PASS
- `timeout 60 ./build/root-cmake/astrocs doctor --json` → selftest ok

## 环境限制 (登记)
- TSan/ASan 数值验证受限 (GCC14 拒编 cfitsio) → G3 PASS* 移交 Fatduck/MSVC。
- clang 无 libomp → OpenMP 宏保护串行退化 (结果不变, 已验证)。

## Gate 判定
G9 PASS (6/6)。G10 (Windows) Fatduck 离线 → WAITING_WINDOWS 不阻塞。
