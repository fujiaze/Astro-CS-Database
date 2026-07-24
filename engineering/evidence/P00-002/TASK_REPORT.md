# TASK_REPORT: P00-002 恢复并固定 healpix_drizzle 源码

## 任务信息
- **Task ID**: P00-002
- **Phase**: P00
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 开始前状态
- **Commit**: 1ac7425（P00-001 提交后）
- **分支**: main

## 来源记录
- **远端仓库**: https://github.com/fujiaze/Healpix-Drizzle-Cpp.git
- **分支**: main
- **Commit**: ecf8758affaf0caf0eb12faed7d2ed32623886e7
- **日期**: 2026-07-16T12:27:28+08:00
- **最后提交信息**: refactor(drizzle): link aio.dll instead of healpix_io.dll (spec G1 Phase 1)

## 执行步骤
1. 记录独立仓库 commit、远端 URL、源码文件 SHA-256
2. 删除嵌套 .git（解除独立仓库）
3. 清理编译产物（healpix_drizzle.dll、_test_compile.exe、__pycache__/、.pytest_cache/）
4. 修改 `lib/healpix_db/.gitignore`，移除 `healpix_drizzle/` 忽略规则
5. `git add lib/healpix_db/healpix_drizzle` 纳入主仓库
6. 验证源码文件哈希未被修改（3 个抽查全部 MATCH）

## 纳入文件清单（18 个）
| 文件 | 类型 |
|---|---|
| drizzle_engine.cpp/.h | C++ 核心 |
| hp_drizzle_api.cpp/.h | C++ API |
| fits_reader.cpp/.h | FITS 读取 |
| poly_clip.cpp/.h | 多边形裁剪 |
| wcs_sip.cpp/.h | WCS+SIP |
| healpix_drizzle.py | Python 绑定 |
| pipeline_adapter.py | 管线适配 |
| Makefile | 构建 |
| README.md | 文档 |
| memory.md | 模块记忆 |
| .gitignore | 忽略规则 |
| tests/test_drizzle.py | 测试 |
| tests/test_pipeline_adapter.py | 测试 |

## 已清理（未入库）
- healpix_drizzle.dll (1,273,688 bytes)
- _test_compile.exe (504,472 bytes)
- __pycache__/ (6 个 .pyc)
- .pytest_cache/
- .git/ (嵌套仓库)

## 变更文件
- `lib/healpix_db/.gitignore`（移除 healpix_drizzle/ 和 healpix_stack/ 忽略规则）
- `lib/healpix_db/healpix_drizzle/**`（18 个文件新增）
- `engineering/evidence/P00-002/SOURCE_RECORD.md`（新增）
- `engineering/evidence/P00-002/TASK_REPORT.md`（新增）
- `engineering/evidence/P00-002/TEST_REPORT.md`（新增）
- `engineering/evidence/P00-002/EVIDENCE_INDEX.md`（新增）

## 允许范围遵守
- 仅修改了 `lib/healpix_db/healpix_drizzle/**`（删除 .git 和编译产物）、`lib/healpix_db/.gitignore`、`engineering/evidence/P00-002/**`
- 未修改 healpix_drizzle 源码内容（哈希验证 MATCH）
- 未修改其他模块、docs/、构建脚本内容

## 下一任务
P00-003: 恢复并固定 healpix_stack 源码
