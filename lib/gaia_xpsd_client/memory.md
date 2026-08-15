# gaia_xpsd_client - 模块开发memory

## 模块职责
Gaia DR3/DR3SP星表C客户端，解析XPSD格式星表文件，提供锥形查询接口与多数据库支持，为plate_solve、photometric_calib等模块提供Gaia参考星数据。

## 当前版本
- 版本号：v1.0
- 最新commit：（稳定运行，未指定具体commit）
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C
- 默认分支：master

## 依赖列表
- C99（纯C实现）
- 无外部库（零依赖）

## 关键决策记录
- **XPSD格式解析**：自行实现XPSD二进制格式解析，避免依赖外部星表访问库
- **多数据库支持**：支持DR3与DR3SP两个数据库切换，通过初始化参数指定数据目录
- **内存缓存（60s TTL）**：锥形查询结果缓存60秒，避免重复查询同一区域的星表数据，提升批量处理性能
- **纯C接口设计**：导出C API（gaia_query_cone等），便于C++与Python（ctypes）双向调用

## 进度日志

### 2026-08-15 V18R3 Code Quality / Gaia Closure
**状态: 全部 Gate 通过（GAIA_POLAR_PRUNE_FALSE_NEGATIVE=0 / CACHE_SEMANTICS=PASS /
ALLOCATION_FAULTS=PASS / FULL_FIRST_PARTY_WARNINGS=PASS / STATIC_ANALYSIS=PASS /
SANITIZER=PASS / TRACKED_DIRTY=0）**
- **极区剪枝（可证明保守）**: 用 AE 投影双-Lipschitz 常数（C45=π/(2√2)≈1.11072，
  cone 跨出极冠时 π/2≈1.5708）建立平面圆盘判据；修复 V18R2
  "center outside cap → 必不相交" 错误（dec=44.8°+r=1° 会跨界漏选）；
  查询中心在冠外且 cone 可能跨 ±45° 时保守不剪枝。属性测试 80270 用例
  false_negative=0；真实 XPSD differential（prune ON/OFF）18 查询
  683,297 星完全一致。
- **查询缓存**: 键改为精确语义（ra/dec/radius/mag_low/mag_high +
  db_type/file_count + version=2），删除量化舍入；同 bucket 不同参数零误命中。
- **内存安全**: cache replace 事务式（先分配成功再释放旧条目）；公共输出
  全路径 checked allocation + NULL/0 + 稳定错误码；collector 零容量自愈；
  spec_collector 双数组事务化。分配故障注入 5 失败点（sc_arr/out_stars/
  cache 三数组）+ cache replace 3 失败点全 PASS（ASan/UBSan 下验证）。
- **trace 并发**: process-global 计数器 → per-query 栈上下文（OpenMP atomic
  只更新本查询）；ASTROCS_GAIA_TRACE=1 并发 64 查询无混合/无竞争。
- **测试钩子**: GAIA_ALLOC_TEST（malloc 包装注入）、GAIA_POLAR_PRUNE_DISABLED
  （differential reference mode）。

### 2026-08-08 Phase1 Final Closure V3 — XPSD 官方解码语义 (重要)
- **发现**: XPSD 光谱字节是每星线性量化, 解码必须用记录内 fluxMin/fluxMul:
  `F(λ) = byte*fluxMul + fluxMin` (W·m⁻²·nm⁻¹)。
  参考 PCL `GaiaDatabaseFile::EncodedStarSPData` (GaiaDatabaseFile.h, 2026-06-21):
  `EncodedStarData(32B) | float fluxMin | float fluxMul | uint8 flux[...]`,
  记录 stride = 40 + 344 = 384 (343 样本, 8-bit, 偶数补齐)。
- **修复**: `GaiaSpectrumStar` 新增 `flux_min`/`flux_mul` 字段
  (偏移 32/36 处 float32), `cone_search_with_spectrum` 与
  `query_spectrum_by_coords` 均输出。
- **旧假设废弃**: `uint8 × 10^(-0.4G)` 不能冻结 (byte 无绝对标度)。
- **验证**: 1000/1050 匹配, 官方解码形状残差 median 0.21%/p95 1.8%
  (旧 byte-only 4.7%/29%), 颜色与 C++ 生产路径全部 Gate 通过。

### 2026-07-12 稳定运行
- 模块进入稳定运行状态，被plate_solve、photometric_calib、integration_test等模块依赖
- 配合Gaia驱动PSF流程优化（详见integration_test记录）：先查Gaia星表→投影到像素→均匀化采样→PSF只拟合采样星，45帧提速7.7x
- 推送至GitHub：commit 128eefd（integration_test记录中提及）

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 旧Python版本仓库Gaia-DR3SP-Client-Pyd已删除
- 文档刷新并重新推送
- 最新commit: 7b9dff8
