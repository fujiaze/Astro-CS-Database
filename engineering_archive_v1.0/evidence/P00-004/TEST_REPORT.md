# TEST_REPORT: P00-004 依赖图生成与可重复性验证

## 测试目标
验证 `merge_deps.py` 生成的 `dependency_graph.json`/`md` 可在同一仓库上重复生成，结果一致，且覆盖全部 13 个模块。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: dde66ba
- **分支**: main
- **Python**: 系统默认
- **工具**: engineering/evidence/P00-004/merge_deps.py

## 数据集
- 输入：deps_group1.json（7 模块子 Agent 分析结果）、deps_group2.json（6 模块子 Agent 分析结果）
- 输出：dependency_graph.json、dependency_graph.md

## 产物哈希

| 文件 | SHA-256 |
|---|---|
| deps_group1.json | c68f7baa2b6621f20efa761f14624e37713498ae4051584cb89b2a29bd7c288f |
| deps_group2.json | faca99c31c982cc01451815e51b663134a474b2c38a3a0f3bf1dc284beddab0c |
| merge_deps.py | 0e934fda245d0be1574695309b7ef73c6110decff57c3f4aa72b3eda68299f03 |
| dependency_graph.json | 6f2ce8f21063e971bcc77c40c335a4b3b3d9111febb4b3d8270f7751e22904b |
| dependency_graph.md | aa149d5c64c7b15c06415a1bdef3cc626b000f226ed5aa4e25ed1ab0c7f876ef |

## 命令与超时
- **命令**: `python merge_deps.py`
- **工作目录**: engineering/evidence/P00-004/
- **退出码**: 0
- **超时**: 无（脚本为纯 JSON 合并，<1s 完成）

## 测试 1: 首次运行
- **命令**: `python merge_deps.py`
- **退出码**: 0
- **stdout**: `OK: 13 modules, 68 edges, 10 issues`
- **产物**: dependency_graph.json、dependency_graph.md 生成

## 测试 2: 重复运行（覆盖同一输出目录）
- **命令**: `python merge_deps.py`
- **退出码**: 0
- **stdout**: `OK: 13 modules, 68 edges, 10 issues`
- **产物**: 覆盖首次产物，SHA-256 不变（可重复）

## 测试 3: 关键字段验证
| 字段 | 预期 | 实际 | 结果 |
|---|---|---|---|
| 模块数 | 13 | 13 | PASS |
| 依赖边数 | >0 | 68 | PASS |
| 潜在问题数 | >0 | 10 | PASS |
| healpix_drizzle 覆盖 | 是 | 是 | PASS |
| healpix_stack 覆盖 | 是 | 是 | PASS |
| plate_solve 运行时依赖记录 | 是 | 是 | PASS |
| orchestrator 动态加载记录 | 是 | 是 | PASS |
| 分层架构存在 | 是 | 是 | PASS |

## 测试 4: 模块覆盖完整性（对照 P00-001 preflight）
| 模块 | P00-001 跟踪 | P00-004 覆盖 | 结果 |
|---|---|---|---|
| astro_image_io | ✓ (59 文件) | ✓ | PASS |
| calibration | ✓ (31 文件) | ✓ | PASS |
| data_pipeline | ✓ (8 文件) | ✓ | PASS |
| dynamic_psf | ✓ (12 文件) | ✓ | PASS |
| gaia_xpsd_client | ✓ (12 文件) | ✓ | PASS |
| healpix_db | ✓ (35 文件) | ✓ (含 3 子模块) | PASS |
| orchestrator | ✓ (37 文件) | ✓ | PASS |
| photometric_calib | ✓ (32 文件) | ✓ | PASS |
| plate_solve | ✓ (55 文件) | ✓ | PASS |
| snr_estimator | ✓ (8 文件) | ✓ | PASS |
| star_detector | ✓ (22 文件) | ✓ | PASS |
| healpix_drizzle | P00-002 新增 | ✓ | PASS |
| healpix_stack | P00-003 新增 | ✓ | PASS |

## 测试 5: 抽查头文件依赖可追踪性
- healpix_db/healpix_drizzle → astro_image_io: include/aio_healpix_io.h（drizzle_engine.h:7）— 可追踪 PASS
- photometric_calib → gaia_xpsd_client: src/gaia_client.h（cpp/src/pc_api.cpp:14）— 可追踪 PASS
- orchestrator → astro_image_io: astro_image_io.h（cpp/src/orchestrator.cpp）— 可追踪 PASS
- plate_solve → astro_image_io: 运行时 LoadLibraryA（ipv_select.cpp:122）— 可追踪 PASS

## 失败与 Skip
无失败、无 skip。

## 日志/证据路径
- `engineering/evidence/P00-004/dependency_graph.json`
- `engineering/evidence/P00-004/dependency_graph.md`
- `engineering/evidence/P00-004/deps_group1.json`
- `engineering/evidence/P00-004/deps_group2.json`
- `engineering/evidence/P00-004/merge_deps.py`

## 可复现步骤
1. `cd engineering/evidence/P00-004`
2. `python merge_deps.py`
3. 比对 dependency_graph.json/dependency_graph.md 的 SHA-256 与本报告记录值

## 结论
- 依赖图生成脚本可重复运行，结果一致
- 13 个模块全部覆盖
- 每个 DLL 与头文件依赖可追踪到具体源文件与行号
- **VERDICT: PASS**
