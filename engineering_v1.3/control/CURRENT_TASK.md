# 当前任务

## 状态 (2026-07-29 更新)

**P13-002 进行中** — 运行全部 TestData Stage1

### 用户方向调整（2026-07-29）
不在前面就批量跑 710 帧（严重拖慢进度）。改为：
1. 先修复栈溢出 ✅
2. 只用银心+胜利两组数据跑通全流程 ✅（Stage1+Stage2+浏览器）
3. 修好浏览器性能 ✅（CLI 工具 + 部署修复）
4. 用户验证全流程工作没问题
5. 优化 Stage1 性能（80s/帧太慢）
6. 全部验证好后，再跑最终全流程测试

### 已完成
- 栈溢出修复（3 commit: 61d49fa + e5aeaac + 04226a9）
- 浏览器 CLI 后台调试工具 browser_cli.exe
- 浏览器部署修复（libgomp-1.dll + liblz4.dll）
- Stage2 全流程验证（银心 5 帧 + 胜利 20 帧 LUM）
- 281/385 Victory_Nebula .hiss 已生成

### 待用户验证
- 浏览器 GUI 显示效果（双击 healpix_browser_qt.exe）
- .hcsd 文件查看（stage2_victory_lum/victory_lum_stacked.hcsd）

### 已知问题
- Stage1 性能：80s/帧太慢（PLATESOLVE 13s + DRIZZLE 15-24s）
- 栈溢出修复是数据依赖的（32MB 栈对当前数据集足够）
- P13-002 尚未完成全量 710 帧运行（仅 T4 385 帧）

## Gate 状态
- G12 Photometric Diagnostic Gate: P13-002 进行中
