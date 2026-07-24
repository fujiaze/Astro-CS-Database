# TEST_REPORT: P01-002 依赖锁定清单验证

## 测试目标
验证 dependencies.lock.json 生成脚本可重复运行、内容完整、与来源数据一致。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: bfc1bb1
- **Python**: 3.x（-B 禁用缓存）

## 测试 1: 生成脚本可重复性
- **命令**: `python -B engineering/evidence/P01-002/generate_lock.py`
- **退出码**: 0
- **stdout**: `OK: toolchain: 16 tools / external_libs: 8 libs / modules: 13 modules / build_order: 13 modules in 3 layers + 1 no_build`
- **结果**: PASS

## 测试 2: 工具链完整性
| 预期 | 实际 | 结果 |
|---|---|---|
| 16 工具 | 16 | PASS |
| 含 GCC/G++/mingw32-make | 含 | PASS |
| 含 Qt6/GSL/Eigen3 | 含 | PASS |
| SHA-256 非空（PowerShell/Make/Eigen3 除外） | 是 | PASS |

## 测试 3: 外部库完整性
| 预期 | 实际 | 结果 |
|---|---|---|
| 8 库 | 8 | PASS |
| 含 GSL/GSL CBLAS | 含 | PASS |
| 含 zstd/lz4/zlib | 含 | PASS |
| 含 OpenMP/Eigen3/Qt6 | 含 | PASS |
| 每库有 used_by | 是 | PASS |

## 测试 4: 模块完整性
| 预期 | 实际 | 结果 |
|---|---|---|
| 13 模块 | 13 | PASS |
| 每模块有 authority 字段 | 是 | PASS |
| 每模块有 output 字段 | 是 | PASS |
| 每模块有 sources 列表 | 是 | PASS |
| makefile_outdated 标注 | 是 | PASS |

## 测试 5: 构建顺序
| 层 | 预期模块 | 实际 | 结果 |
|---|---|---|---|
| 基础层 | 6 | 6 | PASS |
| 中间层 | 4 | 4 | PASS |
| 顶层 | 2 | 2 | PASS |
| 无构建 | 1 | 1 | PASS |

## 测试 6: JSON 可解析性
- **命令**: `python -c "import json; json.load(open('engineering/evidence/P01-002/dependencies.lock.json','r',encoding='utf-8'))"`
- **退出码**: 0
- **结果**: PASS

## 测试 7: 锁定完整性
- lock_integrity 含 3 个来源文件 SHA-256
- environment_baseline_sha256 非空
- module_build_configs_sha256 非空
- dependency_graph_sha256 非空
- **结果**: PASS

## 结论
- 生成脚本可重复运行（需 -B 禁用缓存）
- 16 工具链 + 8 外部库 + 13 模块 + 3 层构建顺序
- JSON 可解析，完整性校验通过
- **VERDICT: PASS**
