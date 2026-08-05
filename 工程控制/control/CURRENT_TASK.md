# 当前任务

## ACR 状态（2026-08-05 25 号计划完成）

**feature/astrocompute-runtime 分支（不合并 main）**：25 号计划
`docs/ACR_25_SECOND_FIX_IMPLEMENTATION_REVIEW_CORRECTION_PLAN.md`（外部控制包
`AstroCS_ACR_Control_Package(6).zip`，SHA `755278bfd7c227aaea7e894a58f9bf860b73d09b4adb40416b644b7e267b5f98`）。
8 个原子提交已完成（Benchmark 无竞争内核/工作负载等价 → CUDA 容量与分块卷积 →
full qualification 与真实设备元数据 → profile key 与传输成本 → 跨设备 holdout →
RAM/VRAM 预算与内存反压 → compute-sanitizer/CPU ASan 证据 → 单 HEAD Evidence）。
全量 ctest 641/641 通过（CUDA 真实运行），path guard PASS，证据仓库外生成
（`run/evidence/acr_25plan_20260805/`）。已知限制（schema 版本差异、UBSan/TSan
不可用、利用率闭环按用户要求移除）已如实记录。审核包
`AstroCS_Review_SecondFixImplementationReview_20260805.zip` 位于根目录。

## ACR 状态（2026-08-05 聚焦版控制包）

**feature/astrocompute-runtime 分支（不合并 main）**：聚焦版控制包
`docs/ACR_FOCUSED_CONTROL_PACKAGE/`（SHA `56f74f2e97cb4b91675319f1c0767ac76113c964641ca1dd36ceff98805eac14`），
执行入口 `08_CURRENT_EXECUTION_PLAN.md`。旧 20—26 号计划全部失效。
ACR 定位收缩为积分/Drizzle 类重负载像素算法的 CPU+GPU 动态混合分块与
数据驻留优化层。已完成：删除 CPU/GPU 精确利用率控制、定义 RouteMode/
PartitionKind 与 5 个目标合成 OperationId。待办：目标 Operation 合成套件 →
OperationProfile → MixedRoutePlanner → ResidencyManager/内存预算 → 聚焦
测试与单一干净 HEAD Evidence。

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
