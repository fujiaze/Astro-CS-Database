# QA-003 依赖锁 (Dependency Lock) — AstroCS 0.10.0-alpha.2

任务: QA-003 (G7) — 锁定依赖/许可/SBOM/可复现构建。

## 第三方依赖锁定

| 依赖 | 版本 | 来源 | License | 引入方式 | 是否 vendored | 变更/补丁 |
|---|---|---|---|---|---|---|
| nlohmann/json | 3.12.0 | github.com/nlohmann/json | MIT | `third_party/nlohmann/json.hpp` (header-only) | 是 | 无 patch; sha256 a1af42c0... |
| cfitsio | 4.6.4 | HEASARC (NASA GSFC) | NASA HEASARC free-use | `lib/astro_image_io/third_party/cfitsio/` | 是 | 线程 reentrant 构建 (`_REENTRANT`+FFLOCK); sha256 8dc7deee... |
| zlib | 1.3.1 | Debian zlib1g 1:1.3.dfsg+really1.3.1-1+b1 | Zlib | `find_package(ZLIB)` (cfitsio) | 否 (系统包) | 无 |
| libgomp (OpenMP) | 14.2.0 | GCC toolchain | GPL-3.0-with-GCC-exception | `find_package(OpenMP)` → 遗留 omp 模块 `-fopenmp` | 否 | 无 |
| glibc pthread | 随系统 | glibc | LGPL-2.1-or-later | `find_package(Threads REQUIRED)` | 否 | 无 |

## 编译链锁定

- 编译器: GCC 14.2.0 (Debian 14.2.0-19); Clang 19.1.7 (Debian 3+b1); 验证链
- CMake: 3.31.6
- 架构: amd64 (仅支持 amd64, AGENTS.md)
- Source commit: `8003297fe6043620ab66ab0e5effc5b015c9721a`
- 自有 target flags: `-Wall -Wextra -Wpedantic -Wconversion` (QA-001 分层)
- 第三方 cfitsio: `-w` (隔离)
- 遗留 omp 模块 (calibration/aio/drizzle): `-fopenmp` 编译 pragma
- Sanitizer (QA-002): `-fsanitize=address,undefined` (ASan) / `-fsanitize=thread` (TSan)

## Provider build IDs (CPU-001)

`baseline` / `avx2` / `avx512` — 由 `profile_gen_v2` 逐 kernel benchmark 选择
(禁止硬编码, 逐内核实测), provider 可用性经 CPUID/XGETBV 检测并降级链验证。

## 可复现性

- Linux amd64: GCC Release 从 source_commit 重建, 0 warning, ctest 56/56 PASS
- Windows amd64: Fatduck MSVC 重建 (WIN-001 验证)
- Release package (REL-001): 每平台唯一 `astrocs`/`astrocs.exe` 入口,
  含 baseline+AVX2/AVX512 provider、IR、schemas、licenses、SBOM、README、checksums
