# P13-002 — EVIDENCE_INDEX

| 字段 | 值 |
| --- | --- |
| 任务 ID | P13-002 |
| 阶段 | P13 |
| Gate | G12 |
| 执行日期 | 2026-07-29 |
| Verdict | IN_PROGRESS（用户调整范围，待全量验证后转 DONE） |

## 1. 证据清单

| # | 类型 | 路径 | 说明 |
| --- | --- | --- | --- |
| 1 | TASK_REPORT | `evidence/P13-002/TASK_REPORT.md` | 任务报告（目标/调整/已修复问题/Stage2 验证/批量进度） |
| 2 | TEST_REPORT | `evidence/P13-002/TEST_REPORT.md` | 测试报告（栈溢出修复 + 浏览器 CLI + Stage2 验证） |
| 3 | REVIEW_REPORT | `evidence/P13-002/REVIEW_REPORT.md` | 独立复核报告 |
| 4 | EVIDENCE_INDEX | `evidence/P13-002/EVIDENCE_INDEX.md` | 本文件 |
| 5 | 栈溢出失败列表 | `evidence/P13-002/reports/stack_overflow_failures.md` | 失败帧清单与根因分析 |
| 6 | 测光报告样本 | `evidence/P13-002/reports/photometry_report.json` | 代表帧测光诊断输出 |
| 7 | 浏览器 CLI 源 | `lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp` | CLI 后台调试工具 |
| 8 | Makefile 栈修复 | `lib/orchestrator/cpp/Makefile` | EXE 栈 32MB（LDFLAGS -Wl,--stack,33554432） |
| 9 | snr_evaluator 修复 | `lib/snr_estimator/cpp/snr_evaluator.cpp` | leaf_max_size 10→32 |
| 10 | 部署脚本修复 | `lib/healpix_db/healpix_browser_qt/app/deploy.ps1` | libgomp-1.dll + liblz4.dll |

## 2. 任务定义参考

- 任务文件：`tasks/P13-002.md`
- 参考 Spec：`docs/08_STAGE1_REAL_DATA_FULL_VALIDATION_SPEC.md`
- 依赖：P13-001（runner）+ P10-005（Light→Master 解析）

## 3. 用户方向调整（2026-07-29）

用户明确指示：**不在前面就批量跑 710 帧**（严重拖慢进度）。改为分阶段推进：

1. 先修复栈溢出 ✅
2. 只用银心+胜利两组数据跑通全流程 ✅（Stage1+Stage2+浏览器）
3. 修好浏览器性能 ✅（CLI 工具 + 部署修复）
4. 用户验证全流程工作没问题 ⏳（待用户审计）
5. 优化 Stage1 性能（80s/帧太慢）⏳
6. 全部验证好后，再跑最终全流程测试 ⏳

## 4. 已完成交付物矩阵

| 交付项 | 任务要求 | 实际交付 | 验证 |
| --- | --- | --- | --- |
| 栈溢出修复 | snr_evaluator 不再栈溢出 | 3 commit（DLL 栈 8MB + leaf_max_size 32 + EXE 栈 32MB） | ✓ 3/3 失败帧 PASS |
| 浏览器 CLI 工具 | 后台调试 DLL 依赖 + 性能 | `browser_cli.exe`（--diag/--benchmark/--sim） | ✓ 9/9 DLL OK |
| 浏览器部署修复 | 双击启动可用 | deploy.ps1 + libgomp-1.dll + liblz4.dll | ✓ 启动正常 |
| Stage2 银心验证 | 银心 HCSD 生成 | `galaxy_center_stacked.hcsd` (1.2MB, success=true) | ✓ |
| Stage2 胜利验证 | 胜利 HCSD 生成 | `victory_lum_stacked.hcsd` (6890 像素, success=true) | ✓ |
| Stage1 批量进度 | 710 帧全量 | 281/385 T4 帧已完成（用户调整范围） | ⏳ 部分 |

## 5. 关键修复汇总

| 问题 | 根因 | 修复 | commit |
| --- | --- | --- | --- |
| snr_evaluator 栈溢出 (0xC00000FD) | nanoflann divideTree 在偏斜 3D 数据上递归数百层，超过 Windows DLL 默认 1MB 栈 | DLL 栈 8MB + leaf_max_size 32 + EXE 栈 32MB | 61d49fa + 04226a9 |
| 浏览器 STATUS_DLL_NOT_FOUND | libgomp-1.dll + liblz4.dll 未部署 | deploy.ps1 添加依赖 | e5aeaac |
| 浏览器无后台调试工具 | 缺少非交互式性能/依赖诊断 | 新增 `browser_cli.cpp` | e5aeaac |
| Windows MAX_PATH=260 | .hiss 输出路径过长（rc=-2） | hash_key 前 16 字符作为目录名 | （runner 内） |

## 6. 性能基线（浏览器 CLI 测量）

| 场景 | 指标 | 值 |
| --- | --- | --- |
| .hiss 加载 | 耗时 | 3.4 ms |
| .hiss nside/n_pix | - | 512 / 3928 |
| .hcsd 加载 | 耗时 | 1.4 ms |
| .hcsd 子叶加载 | avg | 0.42 ms/叶 |
| .hcsd 球面模拟 | FPS | 55.2 FPS（18.1ms/帧） |
| 胜利 LUM 模拟 | FPS | 63.4 FPS（15.8ms/帧） |
| 内存占用 | - | 8 MB |

## 7. Stage1 批量进度

| 数据集 | 设备 | 总帧数 | 已完成 | 通过率 |
| --- | --- | --- | --- | --- |
| Victory_Nebula_T4_Flying_Dutchman | T4 | 228 | 228 | 100% |
| Galaxy_Center_T4 | T4 | 157 | 53 | 100% |
| NGC55_T3_flying_dutchman | T3 | 79 | 0 | - |
| NGC247_T2_flying_dutchman | T2 | 68 | 0 | - |
| NGC1727_T2_flying_dutchman | T2 | 64 | 0 | - |
| NGC83_cluster_T3_Flying_Dutchman | T3 | 72 | 0 | - |
| LDN43_T2_flying_dutchman | T2 | 42 | 0 | - |
| **合计** | | **710** | **281** | **40%** |

**注**：用户调整范围，仅跑银心+胜利两组验证全流程，不全量跑。

## 8. VERDICT

```
VERDICT: IN_PROGRESS
待用户审计通过后，进入性能优化阶段，再跑最终全量测试。
```
