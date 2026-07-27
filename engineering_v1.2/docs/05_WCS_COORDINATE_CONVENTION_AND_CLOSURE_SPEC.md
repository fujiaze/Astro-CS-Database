# WCS 坐标约定与真实星对闭环规范

## 目的

区分 PlateSolve 内部中心坐标、图像数组坐标、FITS 1-based 像素坐标、标准 WCS/SIP 坐标，证明导出的 WCS 可供 Photometric、SNR 和 Drizzle 一致使用。

## 强制测试

1. 使用 PlateSolve 实际匹配对，保存 detector `(x,y)` 与 Gaia `(ra,dec)`；
2. 仅使用写入 PipelineFrame/Header 的标准 WCS/SIP，将 Gaia 投影回像素；
3. 比较预测与 detector 坐标；
4. 计算 median/p90/p99、X/Y 偏差、四象限/边缘分布；
5. 做 pixel→sky→pixel 和 sky→pixel→sky 双向闭环；
6. 对无 SIP 与有 SIP 分开统计。

## 修复原则

Y 轴和 SIP 奇次项符号是待验证假设。必须由诊断确定确切转换，在 PlateSolve WCS 生产端统一修复；不得只在 Photometric 中补偿。

## 门限

- 710 帧原成功集不得产生成功→失败；
- 代表帧标准 WCS 回投 median ≤ 0.75 px、p90 ≤ 1.5 px、p99 ≤ 3 px；
- 无明显全局 Y 镜像、90/180°旋转或象限系统误差；
- 内部 RMS 与标准 WCS 回投残差关系必须可解释。
