# AstroCS Stage1 HISS 交付包

> 交付日期: 2026-07-31
> 任务包: AstroCS_Stage1_HISS_Agent_Package_2026-07-31
> 审计基准 commit: 183558ad6907d1e13a56a01c33c708913d7bbdc3

## 1. 本次交付内容

本交付包基于 Agent 任务包要求, 在用户现有 AstroCS 仓库中就地完成以下工作:

### 1.1 Wiki 规范更新 (Phase 1-2)
- 重写 8 个标准 Wiki 页面, 消除旧契约冲突
- 旧页面标注 SUPERSEDED, 保留作迁移参考
- 详见 `wiki/` 目录

### 1.2 C++ 实现 (Phase 3)
- **HISS 格式定义** (`lib/astro_image_io/include/hiss_format.h`): HissGridSpec/HissTile/HissWriter/HissReader/CodecRegistry 接口
- **HISS Writer** (`lib/astro_image_io/src/hiss_writer.cpp`): XISF 式 Header + attachments, .partial 原子提交
- **HISS Reader** (`lib/astro_image_io/src/hiss_reader.cpp`): 按目录读取, offset/checksum 校验
- **Codec 注册** (`lib/astro_image_io/src/hiss_codec.cpp`): RAW/LZ4/ZSTD 三种 codec
- **通用工具** (`lib/astro_image_io/src/hiss_common.cpp`): validate_support/compute_tile_depth 等
- **最优 Dark 估计** (`lib/calibration/src/dark_optimizer.cpp`): 8×8 分区 + 鲁棒回归 + 5 轮 MAD + 回退
- **Drizzle 引擎增强** (`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp/h`): 自动 NSIDE 计算, sumArea 累加, float64 内部精度
- **校准 API 扩展** (`lib/calibration/include/astro_calibration.h`): Stage1Diagnostics 结构化诊断

### 1.3 C++ 实验 (Phase 4)
- DQ-001 ~ DQ-007 全部未决工程选项实验完成
- 原始数据见 `reports/experiments/raw_results.csv` 和 `raw_results.json`
- 实验汇总见 `reports/experiments/summary.md`
- 决策队列见 `reports/decision_queue.md`
- **重要**: 实验结论仅为推荐, 未冻结为默认值

### 1.4 正确性测试 (Phase 5)
- 21 个测试用例, 覆盖校准/Drizzle/HISS 格式
- 测试程序: `lib/astro_image_io/tests/hiss_correctness_test.cpp`
- 测试结果: 21/21 通过, 退出码 0
- 正确性报告: `reports/correctness_report.md`

### 1.5 性能剖析 (Phase 5)
- 模块性能/内存/并行化分析
- 性能报告: `reports/performance_profile.md`

## 2. 未冻结事项

以下事项需用户与主审助手确认后才能冻结, 详见 `reports/decision_queue.md`:

- DQ-001: signal 子块默认 codec/transform
- DQ-002: support 子块默认 codec
- DQ-003: BITMAP 子块默认 codec
- DQ-004: SPARSE_LIST 子块编码与 codec
- DQ-005: FULL/BITMAP/SPARSE_LIST 切换阈值
- DQ-006: checksum 算法
- DQ-007: 子块对齐

## 3. 已知问题

### 3.1 Writer/Reader SNR 二进制布局不一致
- **严重度**: 中
- **影响**: SNR 控制点无法往返读取 (signal/support 不受影响)
- **根因**: Writer 在 n_points+points 之后追加三个全局 double, Reader 只读取 n_points+points
- **建议修复**: 统一 Writer/Reader 的 SNR 子块二进制布局

### 3.2 未知必需子块未主动拒绝
- **严重度**: 低
- **影响**: 文件含未知必需子块时 Reader 不会拒绝 (规范要求拒绝)
- **根因**: Reader 按 type 查找子块, 未全扫描未知必需子块
- **建议修复**: Reader.open 或 read_tile 时增加全扫描逻辑

## 4. 构建和测试入口

### 4.1 应用变更
参见 `APPLY_IN_PLACE.md` 获取详细的应用指南。

### 4.2 构建
```bash
# 校准模块
cd lib/calibration
./build.ps1

# HISS I/O 模块
cd lib/astro_image_io
./build.ps1

# Drizzle 模块 (含在 healpix_db 构建)
cd lib/healpix_db/healpix_drizzle
# 使用 CMake 或 Makefile
```

### 4.3 运行正确性测试
```bash
cd lib/astro_image_io/tests
g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD \
  -I../include -I../src \
  -I../../calibration/include \
  hiss_correctness_test.cpp \
  ../src/hiss_codec.cpp ../src/hiss_common.cpp \
  ../src/hiss_writer.cpp ../src/hiss_reader.cpp \
  ../../calibration/src/dark_optimizer.cpp ../../calibration/src/calibrator.cpp \
  -llz4 -lzstd -o hiss_correctness_test.exe
./hiss_correctness_test.exe
```
预期输出: 21/21 通过, 退出码 0

## 5. 明确未运行

- **Stage2**: 本任务未实现或修改 Stage2, 保持停止状态
- **710 帧全量回归**: 本任务未自动启动 710 帧或其他全量回归
- **真实天文数据验证**: 测试使用合成数据, 未使用真实天文数据

## 6. 交付结构

```
AstroCS_Stage1_HISS_Delivery/
├─ 00_README.md          (本文件)
├─ APPLY_IN_PLACE.md     (应用指南)
├─ MANIFEST.json         (文件清单)
├─ MANIFEST.sha256       (清单校验)
├─ DELETE_LIST.txt       (删除清单)
├─ git_diff.patch        (基于基准 commit 的 diff)
├─ changed_files/        (新增/修改的源码)
├─ wiki/                 (新增/修改的 Wiki 页面)
├─ tests/                (新增测试)
└─ reports/              (报告)
   ├─ repository_audit.md
   ├─ implementation_summary.md
   ├─ correctness_report.md
   ├─ performance_profile.md
   ├─ decision_queue.md
   └─ experiments/
      ├─ summary.md
      ├─ environment.md
      ├─ raw_results.csv
      └─ raw_results.json
```

## 7. 声明

- 本交付包仅记录实现与测试状态, 不代表用户验收完成
- 已知问题需在后续迭代中修复并重新测试
- 实验结论仅为推荐, 不代表最终冻结决策
- 测试不修改任何数学算法或科学语义
