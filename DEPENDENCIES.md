# DEPENDENCIES.md — 工具链版本锁定 (QA-005)

生成: 2026-08-31 (Linux vm-bj)

| 组件 | 版本 | 用途 |
|------|------|------|
| GCC (g++/gcc) | 14.2.0 | Release 生产构建 |
| Clang (clang++) | 18 (Debian 13) | Debug/static analysis |
| CMake | 3.31.6 | 构建系统 (显式 target 图) |
| Python | 3.13 | 工具链/测试脚本 |
| cfitsio | vendored (lib/astro_image_io/third_party, 60 源显式清单) | FITS I/O (第三方) |
| PyYAML | installed | 控制包解析 |

## 构建 flags (Release)
- CMAKE_BUILD_TYPE=Release (GCC)
- -O3 默认; 无未锁定 flag; 无全域 -w
- Debug: -DCMAKE_BUILD_TYPE=Debug (clang++)

## 复现
- build id = VERSION + g<commit> (cli/version_generated.h.in)
- 同 commit 重构建 → 相同 build id
- SBOM: dist/astrocs-alpha/SBOM.json
