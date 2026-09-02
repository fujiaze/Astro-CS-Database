# P13-002 任务报告

## 任务信息
- **任务ID**: P13-002
- **阶段**: P13
- **Gate**: G12
- **依赖**: P13-001; P10-005
- **状态**: IN_PROGRESS（部分完成）

## 目标
运行全部 TestData Stage1，对所有 Light 运行完整阶段并输出逐帧状态。

## 执行情况

### 用户方向调整（2026-07-29）
用户指示：不在前面就批量跑 710 帧（严重拖慢进度）。改为：
1. 先修复栈溢出
2. 只用银心+胜利两组数据跑通全流程（Stage1+Stage2+浏览器）
3. 修好浏览器性能
4. 用户验证全流程工作没问题
5. 优化 Stage1 性能（80s/帧太慢）
6. 全部验证好后，再跑最终全流程测试

### 已完成工作

#### 1. 栈溢出修复（3 commit）
- **61d49fa**: healpix_drizzle.dll 栈 8MB + snr_evaluator leaf_max_size 10→32
- **e5aeaac**: 浏览器 CLI 后台调试工具 + deploy.ps1 修复
- **04226a9**: orchestrator.exe 栈 32MB（根因修正）

**根因**: nanoflann divideTree 递归在偏斜 3D 笛卡尔控制点数据上产生极不平衡 KD-tree
- 递归深度从理想 ~11 层膨胀到数百层
- 每帧栈使用 1-2KB（BoundingBox 按值拷贝 + -O3 内联）
- 超过 Windows DLL 默认 1MB 栈限制
- `-Wl,--stack` 对 DLL 无效，DLL 代码使用调用线程栈，必须设置 EXE 栈

**验证**: 3 个失败帧（Victory_Nebula mosaic1 RED/BLUE）全部修复，exit=0

#### 2. 浏览器 CLI 后台调试工具
- 文件: `lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp`
- 功能:
  - `--diag`: DLL 依赖诊断（9 个 DLL 加载状态）
  - `--benchmark`: 文件加载+子叶加载+降采样性能测试
  - `--sim zoom/pan`: 模拟缩放/平移（帧率+内存统计）
- 输出: JSON 报告到 stdout，详细日志到 stderr

**验证结果**:
- .hiss 加载: 3.4ms
- .hcsd 球面模式: 55-63 FPS，子叶加载 0.19-0.42ms/叶
- 内存占用: 8MB

#### 3. 浏览器部署修复
- **根因**: astro_image_io.dll 依赖 libgomp-1.dll（OpenMP）和 liblz4.dll（压缩）未复制到 exe 目录
- **症状**: 双击启动时 STATUS_DLL_NOT_FOUND (exit=-1073741515)
- **修复**: deploy.ps1 添加这两个 DLL 到复制列表
- **验证**: windeployqt + 依赖 DLL 部署后，双击启动正常

#### 4. Stage2 全流程验证
- **银心 5 代表帧**: → galaxy_center_stacked.hcsd (1.2MB, success=true, 0.017s)
- **胜利 20 帧 LUM**: → victory_lum_stacked.hcsd (6890 像素, success=true, 0.063s)
- 浏览器 CLI 性能: 63.4 FPS, 15.8ms/帧

#### 5. Stage1 批量运行
- 已运行 281/385 Victory_Nebula 帧（T4）
- 全部 PASS（has_snr=1, fit_used 1700-1900, sigma 0.06-0.13）
- 性能: 70-80s/帧

## 通过条件检查
1. ❌ 未完成全量 710 帧运行（仅 T4 385 帧，已完成 281 帧）
2. ✅ 无未声明的 fallback/skip
3. ⏳ 证据 4 件套待完成
4. ⏳ 独立复核待完成

## VERDICT: IN_PROGRESS
待用户验证全流程后，优化性能，再跑最终全量测试。
