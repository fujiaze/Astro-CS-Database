# Compiler / platform matrix（当前快照）

本文件记录实际执行过的构建与验收，不把未执行过的平台写成通过。Windows 无工具链，仍为外部阻塞。

| 编译器 | 版本 | 构建目录 | `ctest` | runtime 16 cases | 备注 |
|---|---|---|---|---|---|
| GCC | 14.2.0 (Linux x86-64) | `/tmp/acsd-build` | 19/19 pass | 16/16 pass | 主验收构建；`/tmp/acsd-build-dev` 为 `ACSD_DEV_MODE=ON` |
| Clang | 18.1.8 (Linux x86-64) | `/tmp/acsd-clang` | 20/20 pass | 16/16 pass | 同一源码、同一验证包；`validation/runtime/build.sh` 以 `ACSD_BUILD=/tmp/acsd-clang` 重编 case |

复现：

```bash
# GCC
cmake -S . -B /tmp/acsd-build -G Ninja -DACSD_BUILD_SCIENCE=ON
ninja -C /tmp/acsd-build
ctest --test-dir /tmp/acsd-build
ACSD_BUILD=/tmp/acsd-build ACSD_DEV_BUILD=/tmp/acsd-build-dev bash validation/runtime/build.sh
ACSD_BUILD=/tmp/acsd-build python3 validation/runtime/run_cases.py --no-build

# Clang
cmake -S . -B /tmp/acsd-clang -G Ninja -DACSD_BUILD_SCIENCE=ON \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
ninja -C /tmp/acsd-clang
ctest --test-dir /tmp/acsd-clang
ACSD_BUILD=/tmp/acsd-clang bash validation/runtime/build.sh
ACSD_BUILD=/tmp/acsd-clang python3 validation/runtime/run_cases.py --no-build
```

未验证：Windows/MSVC、macOS、真实 HST/M42 数据、GPU（仅合同，无后端）。这些不得在完成报告中计为通过。
