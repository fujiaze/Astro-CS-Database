# P11-004 审核裁决

## 裁决

现有证据**不足以支持修改 WCS 生产端**。P11-004 的阻塞来源是验证工具改变了星对对应关系：IPV 内部 RMS 使用 RANSAC + Umeyama 精化后的 inlier 对；诊断工具使用全 Gaia/检测星的 kd-tree 双向最近邻，包含暗星误配、拥挤场歧义、饱和质心偏差和非唯一配对。

因此两组残差回答的是不同问题，不能用同一门限比较。

## 已确认事实

- `to_astropy_wcs(result)` 与写入 FITS Header 后的 `WCS(header)` 在 8/8 帧等价，投影差异约 1e-10 px；
- pixel→sky→pixel 和 sky→pixel→sky 数值闭环正确；
- Gaia 查询调用路径一致；
- callback 导出的 detection 与 PlateSolve 内部检测同源；
- 当前 kd-tree 报告不能区分 WCS 误差、质心偏差、误配和像素中心约定。

## 不采用的方案

- **仅用 IPV RMS 作 Gate**：失去对最终 WCS 序列化/消费路径的独立验证；
- **继续降低 kd-tree 门限直到通过**：仍未固定真实对应关系；
- **依据 0.5 px 均值直接改 CRPIX**：缺少权威星对证据，且会影响 Photometric、SNR、Drizzle；
- **Photometric 局部补偿**：破坏统一 WCS 契约。

## 采用方案

使用“权威对应关系 + 外部 WCS 回投”：

1. PlateSolve 导出最终 inlier 星对；
2. 诊断工具不得重新决定配对；
3. 使用写入 Header/PipelineFrame 的 WCS/SIP 把每个 Gaia inlier 回投；
4. 与 detector 坐标和求解器内部预测分别比较；
5. blind rematch 只作为二级健康检查。

## P11-004 两种合法结论

- `NO_CODE_CHANGE_REQUIRED`：外部回投通过，当前 WCS 生产代码保持不变；
- `WCS_PRODUCTION_FIX_REQUIRED`：权威星对回投失败，且误差形态可复现并指向生产端转换。
