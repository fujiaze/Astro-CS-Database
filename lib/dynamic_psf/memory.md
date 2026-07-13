# dynamic_psf - 模块开发memory

## 模块职责
动态PSF拟合，基于Moffat4模型对图像星点进行7参数LM（Levenberg-Marquardt）求解器拟合，输出PSF模型参数供下游测光、匹配、叠加使用。

## 当前版本
- 版本号：v1.1（含性能修复）
- 最新commit：a3ae0d6
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Dynamic-PSF
- 默认分支：master

## 依赖列表
- C++17
- OpenMP（libgomp，16线程并行）

## 关键决策记录
- **Moffat4 PSF模型**：采用Moffat模型（β参数化），相比高斯模型更能描述天文PSF的翼部延展
- **7参数LM求解器**：拟合参数为(amplitude, x0, y0, sigma_x, sigma_y, beta, background)，LM算法迭代收敛
- **OpenMP 16线程并行**：每星独立拟合，按星点数OpenMP并行，充分利用开发环境16线程CPU

## 进度日志
### 2026-07-12 性能修复（9.26s→0.26s, -97.1%）
- **问题**：单帧36k星点PSF拟合耗时9.26s，全链路45帧需67.5分钟
- **修复措施**：
  1. 日志级别改为WARN（避免DEBUG/INFO大量字符串拼接与IO开销）
  2. 移除双fflush调用（每次写日志强制flush导致syscall风暴）
  3. 添加OpenMP并行（#pragma omp parallel for schedule(dynamic)）
- **结果**：9.26s → 0.26s（-97.1%），16线程并行加速
- 推送至GitHub：commit a3ae0d6

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: d3ec9e2
