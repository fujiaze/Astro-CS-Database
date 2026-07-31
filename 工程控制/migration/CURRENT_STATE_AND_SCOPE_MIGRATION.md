# 当前交付状态与范围迁移

## 迁入“已完成基线”

- PlateSolve single internal detection shared export；
- 709/710 A/B；
- 基础 photometric matching 修复；
- 部分真实 HISS 的SNR持久化；
- Stage1/Stage2基础文件通道；
- 浏览器启动与依赖部署。

## 迁入“待验证/待完成”

- T1–T4与Master完整解析；
- HISS正式signal/support/SNR/压缩契约；
- Stage1代表帧科学Gate；
- 全局SNR²加权加性总曲面；
- 正式排异、连续融合和HCSD调试层；
- 银心三片/32帧；
- 资源感知编排器；
- 真实浏览器性能；
- 最终710帧回归。

## 标记为废止或非正式

- 旧任务列表中以跑数数量为主的提前全量任务；
- 每个微任务四件套；
- 混合滤镜Stage2；
- panel1五帧“银心马赛克”；
- 相同HISS副本“非零梯度验证”；
- browser_cli模拟FPS作为GUI性能。

迁移脚本不得删除这些证据，只在新状态中标记 `LEGACY_NON_ACCEPTANCE_EVIDENCE`。
